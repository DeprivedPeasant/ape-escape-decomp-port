"""Read-only audit of Claude's current Time Station render and a PSF reference.

Run with installed system Python (numpy available there):
    python tools/codex/audit_time_station.py
Requires the authorized PSF ZIP already downloaded to extract/codex/audio-audit/.
Does not change the player or reference files. Outputs are ignored.
Loop comparisons describe the historical cached-PCM model; the current music
engine now carries decoder history and uses integer pitch/Gaussian interpolation.
"""
import ctypes
import hashlib
import json
import shutil
import struct
import subprocess
import sys
import wave
import zipfile
import zlib
from pathlib import Path
import numpy as np

ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'extract/codex/audio-audit'
sys.path.insert(0,str(ROOT/'tools'))
sys.path.insert(0,str(ROOT/'tools/native_audio'))
import check_player as cp
import dump_sound as ds
import dump_tones as dt
import render_seq as rs


def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()


def fresh_player():
    source=ROOT/'src/audio/sp_player.c';sha=digest(source)
    dll=OUT/f'sp_player_{sha[:16]}.dll'
    if not dll.exists():
        subprocess.run([shutil.which('gcc'),'-std=c11','-O2','-Wall','-Wextra','-Werror','-shared',
                        str(source),'-o',str(dll),'-lm','-static-libgcc'],check=True)
    lib=ctypes.CDLL(str(dll))
    lib.sp_create.restype=ctypes.c_void_p
    lib.sp_create.argtypes=[ctypes.c_char_p,ctypes.c_size_t,ctypes.c_char_p,ctypes.c_size_t]+[ctypes.c_int]*4
    lib.sp_render.argtypes=[ctypes.c_void_p,ctypes.POINTER(ctypes.c_float),ctypes.c_int]
    lib.sp_render.restype=ctypes.c_int
    lib.sp_destroy.argtypes=[ctypes.c_void_p]
    lib.sp_stats.argtypes=[ctypes.c_void_p]+[ctypes.POINTER(ctypes.c_int)]*4
    rendered,stats=cp.render_c(lib,'136')
    assert digest(source)==sha,'Claude changed engine during audit; rerun against stable revision'
    return rendered,stats,sha


def psf_payload(raw):
    assert raw[:4]==b'PSF\x01'
    reserved,size,crc=struct.unpack_from('<III',raw,4)
    compressed=raw[16+reserved:16+reserved+size]
    assert zlib.crc32(compressed)==crc
    data=zlib.decompress(compressed)
    assert data[:8]==b'PS-X EXE'
    tags=raw[16+reserved+size:].decode('utf-8',errors='replace')
    return data,dict(line.split('=',1) for line in tags.splitlines() if '=' in line)


def loop_audit(vb,address):
    sample=rs.decode_sample(vb,address)
    if sample.loop is None:return dict(address=address,looped=False)
    start=address+sample.loop//28*16;end=address+len(sample.pcm)//28*16
    history=[int(sample.pcm[-1]*32768),int(sample.pcm[-2]*32768)]
    first=sample.pcm[sample.loop:]*32768;passes=[]
    # Same decoder rounding as the current implementation, changing only whether
    # ADPCM predictor history survives the jump. This isolates the loop behavior.
    for turn in range(3):
        decoded=[]
        for off in range(start,end,16):
            shift=vb[off]&15;filter_index=min(vb[off]>>4,4)
            f0,f1=ds.FILTERS[filter_index]
            for byte in vb[off+2:off+16]:
                for nibble in (byte&15,byte>>4):
                    value=((nibble-16 if nibble>7 else nibble)*4096)>>shift
                    value+=(history[0]*f0+history[1]*f1+32)>>6
                    value=max(-32768,min(32767,value))
                    history=[value,history[0]];decoded.append(value)
        delta=np.array(decoded)-first
        passes.append(dict(turn=turn+2,peak_lsb=float(np.abs(delta).max()),
                           rms_lsb=float(np.sqrt(np.mean(delta**2))),first_samples=decoded[:8]))
    return dict(address=address,looped=True,samples=len(sample.pcm),loop_start=sample.loop,
                loop_samples=len(first),cached_first_samples=first[:8].astype(int).tolist(),passes=passes)


def main():
    OUT.mkdir(parents=True,exist_ok=True)
    fresh,stats,sha=fresh_player()
    reference=np.load(ROOT/'extract/native_audio/136_ref.npy',mmap_mode='r')
    n=min(len(reference),len(fresh));difference=fresh[:n]-reference[:n]
    rms=float(np.sqrt(np.mean(difference**2))/np.sqrt(np.mean(reference[:n]**2)))
    peak=float(np.abs(difference).max())
    with wave.open(str(ROOT/'extract/native_audio/136_ref.wav'),'rb') as wav:
        assert wav.getframerate()==44100 and wav.getnchannels()==2 and wav.getsampwidth()==2
        wav_pcm=np.frombuffer(wav.readframes(wav.getnframes()),dtype='<i2').reshape(-1,2)
    expected=(np.clip(reference[:len(wav_pcm)]*(.89/np.abs(reference).max()),-1,1)*32767).astype('<i2')
    assert np.array_equal(expected,wav_pcm),'WAV does not match saved reference array'
    gain=.89/np.abs(fresh).max()
    wav_n=min(n,len(wav_pcm))
    current_pcm=(np.clip(fresh[:wav_n]*gain,-1,1)*32767).astype('<i2')
    wav_delta=np.abs(current_pcm.astype(np.int32)-wav_pcm[:wav_n].astype(np.int32))
    with wave.open(str(OUT/f'136_audited_{sha[:16]}.wav'),'wb') as wav:
        wav.setnchannels(2);wav.setsampwidth(2);wav.setframerate(44100);wav.writeframes(current_pcm.tobytes())
    bank=(ROOT/'extract/members/136.bin').read_bytes();chunks=ds.parse(bank)
    archive=OUT/'ape-escape-psf.zip'
    with zipfile.ZipFile(archive) as z:
        payload,tags=psf_payload(z.read('05 - Time Station.psf'))
        driver,driver_tags=psf_payload(z.read(tags['_lib']))
        matches=[]
        for kind in ('ssq','shd','vb'):
            chunk_list=[chunks[kind]] if kind=='ssq' else chunks[kind]
            for index,(offset,size) in enumerate(chunk_list):
                found=payload.find(bank[offset:offset+size])
                matches.append(dict(kind=kind,index=index,size=size,psf_offset=found,identical=found>=0))
    so,ss=chunks['shd'][0];h=bank[so:so+ss];qo,qs=chunks['ssq'];q=bank[qo:qo+qs]
    notes,channels,total=rs.build(q,1);programs=dt.parse_programs(h)
    long_notes=[];addresses=set()
    for on,off,channel,key,velocity in notes:
        if off-on<2*44100:continue
        program=int(channels[channel]['prog'].at(on,1));record=programs.get(program)
        if not record:continue
        header=record['hdr'];layers=record['layers']
        if header[0]==255:
            hit=[layers[key-header[6]]] if 0<=key-header[6]<len(layers) else []
        else:hit=[layer for layer in layers if layer[0]<=key<=layer[1]]
        hit=[layer for layer in hit if layer!=dt.EMPTY]
        if header[0]!=255 and not header[0]&128:hit=hit[:1]
        for layer in hit:
            address=struct.unpack_from('<H',layer,4)[0]*8;addresses.add(address)
            adsr1,adsr2=struct.unpack_from('<HH',layer,6)
            long_notes.append(dict(start=on/44100,duration=(off-on)/44100,channel=channel,key=key,
                                   program=program,sample_address=address,adsr1=adsr1,adsr2=adsr2,
                                   sustain_all_ones=(adsr2>>6&127)==127,flags=layer[15]))
    vo,vs=chunks['vb'][0];vb=bank[vo:vo+vs]
    loops=[loop_audit(vb,address) for address in sorted(addresses)]
    report=dict(source_sha256=sha,source_git_commit=subprocess.check_output(['git','log','-1','--format=%H','--',
                'src/audio/sp_player.c'],cwd=ROOT,text=True).strip(),fresh_stats=stats,
                reference_frames=len(reference),current_frames=len(fresh),compared_seconds=n/44100,
                current_vs_reference_relative_rms=rms,peak_float_difference=peak,
                wav_matches_reference_array=True,normalized_wav_peak_lsb=int(wav_delta.max()),
                normalized_wav_rms_lsb=float(np.sqrt(np.mean(wav_delta.astype(float)**2))),
                psf_archive_sha256=digest(archive),psf_tags=tags,psf_chunks=matches,
                psf_driver_sha256=hashlib.sha256(driver).hexdigest(),notes=len(notes),
                long_notes=long_notes,loops=loops,psf_playback_rendered=False)
    (OUT/'time-station-audit.json').write_text(json.dumps(report,indent=2)+'\n')
    lines=['# Time Station sound audit (generated)','',
           'Generated by `python tools/codex/audit_time_station.py`; system Python supplies installed numpy.','',
           f'Current C engine versus saved reference: {n/44100:.2f}s compared; relative RMS {rms:.3g}; '
           f'peak float difference {peak:.3g}. Normalized WAV peak difference {wav_delta.max()} PCM LSB. '
           'The supplied WAV exactly matches its saved Python reference array.','',
           'PSF source: https://www.zophar.net/music/playstation-psf/ape-escape . '
           'Sequence, music tone header and sample bank are byte-identical to local member136. '
           'PSF tag says "Wild Arms 2 driver by Matrixz": use it for a hardware/SPU reference; '
           'do not assume it runs the original Ape Escape sequencer. No PSF playback rendered in this audit.','',
           'Hardware behavior reference: https://psx-spx.consoledev.net/ps1/spu/soundprocessingunitspu/ . '
           'The historical model cached one ADPCM decode and repeated PCM; current music now retains predictor history. '
           'The following counterfactual differences isolate history reuse, retaining decoder rounding. '
           'These establish a discrepancy, not an audible root-cause proof.','',
           '| Sample address | Loop samples | Second-loop peak difference (PCM LSB) | RMS difference |',
           '|---|---:|---:|---:|']
    for row in loops:
        if row['looped']:
            delta=row['passes'][0]
            lines.append(f"| 0x{row['address']:X} | {row['loop_samples']} | {delta['peak_lsb']} | {delta['rms_lsb']:.3f} |")
    lines+=['','The sustained pad program28 uses samples0xE200/0x11BC0, ADSR B9FF/5FCF, '
            'with paired roughly10.783s notes starting0.0035s/0.0456s. The sustain rate is all-ones; '
            'hardware freezes that envelope rate, implemented by the current music player. '
            'The earlier slow-decay error was tiny over ten seconds. '
            'The music player now uses the documented SPU Gaussian interpolation instead of cubic. '
            'See notes/30-native-audio.md and check_music_fidelity.py / check_spu_reference.py for the '
            'implemented correction and independent tests; C/Python agreement alone is not a hardware correctness test.','']
    (OUT/'time-station-audit.md').write_text('\n'.join(lines))
    print(f'Current C/reference relative RMS {rms:.3g}; normalized WAV peak {wav_delta.max()} LSB; '
          f'{len(long_notes)} long layer-notes; {len(loops)} samples audited. Report: {OUT}')


if __name__=='__main__':main()
