"""Render the song of a sound bank (SSsq + SShd tone tables + VB samples) to a stereo WAV file.

A software version of the game's sequencer (notes/23-sequencer.md) and SPU voices (notes/24-tone-tables.md):
  * events from tools/dump_seq.py (tempo changes, controllers 7 volume / 10 pan / 0x40 sustain, pitch bend, programs)
  * program -> layers by key range (layered or key split), root key / fine tune / bend range -> pitch
    (original integer lookup at 0x800B38D0, octave shifts and 12-bit sample phase)
  * the game's volume formula  ((channelVol * programVol * velocityCurve * layerVol) >> 14) * panLaw >> 7  with the pan
    tables read from the EXE (0x800B3D90 / 0x800B4190), channel volume / pan changes applied to sounding notes
  * PlayStation SPU ADSR envelopes (attack / decay / sustain / release, linear and exponential)
  * the driver's 24 voice allocation with voice stealing (SeqAllocVoice, see allocate_voices)
  * per-voice SPU ADPCM predictor history across loops, hardware Gaussian interpolation to 44.1 kHz
  * reverb: the SPU reverb algorithm with the game's register preset 4 and output volume 10 on the layers whose reverb
    flag 0x80 is set (--reverb-mode conv = the earlier plain convolution reverb), modulation LFO on layers with flag 0x20 (replaces the bend while controller 1 is non zero)

Usage: uv run python tools/render_seq.py 01B [--loops 1] [--reverb 1] [--reverb-mode spu|conv] [--out extract/wav/01B.wav] [--max-seconds N]
Output: 44100 Hz stereo 16 bit WAV (default extract/wav/<member>.wav).
"""
import argparse
import glob
import math
import struct
import sys
import wave
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
import dump_seq as sq  # noqa: E402
import dump_sound as ds  # noqa: E402
import dump_tones as dt  # noqa: E402

RATE = 44100
FILTERS = ds.FILTERS


def exe_bytes() -> bytes:
    return Path(glob.glob(str(ds.ROOT / "extract/**/SCUS_944.23"), recursive=True)[0]).read_bytes()


def exe_read(exe: bytes, addr: int, n: int) -> bytes:
    o = addr - 0x80010000 + 0x800
    return exe[o:o + n]


def music_pitch(table, root, note, fine, bend, rng):
    """SeqPitchFromNote: signed arithmetic shift for bend, unsigned octave shift."""
    delta = note - root
    dist = abs(delta)
    index = 208 + fine + ((rng * (np.asarray(bend, dtype=np.int64) - 64)) >> 2)
    index = index + (12 - dist % 12 if delta < 0 else dist % 12) * 16
    if np.any(index < 0) or np.any(index >= len(table)):
        raise ValueError('pitch table index outside EXE data')
    pitch = table[index]
    if delta < 0:
        pitch = pitch >> ((dist // 12 + 1) & 31)
    else:
        pitch = pitch << ((dist // 12) & 31)
    return np.minimum(pitch & 0xFFFF, 0x4000)


def music_mod_pitch(table, root, note, fine, bend, rng, wave, depth):
    """Original normal-music bend decomposition followed by small additive LFO."""
    offset = rng * (np.asarray(bend, dtype=np.int64) - 64)
    semitones = np.sign(offset) * (np.abs(offset) // 64)
    quarter = np.sign(offset) * (np.abs(offset) // 4)
    fine = ((fine + quarter - semitones * 16 + 128) & 255) - 128
    root = (120 + root - note) & 255
    note = (120 + semitones) & 255
    delta = note - root
    distance = np.abs(delta)
    mod = np.asarray(wave, dtype=np.int64) * depth // 255 - ((depth + 1) >> 1) + 64
    index = 208 + fine + ((mod - 64) >> 2)
    index += np.where(delta < 0, 12 - distance % 12, distance % 12) * 16
    if np.any(index < 0) or np.any(index >= len(table)):
        raise ValueError('modulated pitch outside EXE table')
    pitch = np.where(delta < 0, table[index] >> ((distance // 12 + 1) & 31),
                     table[index] << ((distance // 12) & 31)) & 0xFFFF
    return np.minimum(pitch, 0x4000)


# ---------------------------------------------------------------- ADPCM samples

class Sample:
    __slots__ = ("pcm", "loop")

    def __init__(self, pcm: np.ndarray, loop: int | None):
        self.pcm = pcm
        self.loop = loop  # sample index the voice jumps back to at the end (None = one shot)


def decode_sample(vb: bytes, start: int) -> Sample:
    """Decode SPU ADPCM from `start` until the block with the end flag (bit 0); bit 1 = repeat, bit 2 = loop start."""
    out: list[int] = []
    s1 = s2 = 0
    loop_start = 0  # a voice starts with the loop address at its start address
    repeat = False
    i = start
    while i + 16 <= len(vb):
        shift, filt = vb[i] & 15, min(vb[i] >> 4, 4)
        flags = vb[i + 1]
        if flags & 4:
            loop_start = len(out)
        k1, k2 = FILTERS[filt]
        for byte in vb[i + 2:i + 16]:
            for nib in (byte & 15, byte >> 4):
                v = (nib - 16 if nib > 7 else nib) << 12 >> shift
                v += (s1 * k1 + s2 * k2 + 32) >> 6
                v = max(-32768, min(32767, v))
                out.append(v)
                s2, s1 = s1, v
        i += 16
        if flags & 1:
            repeat = bool(flags & 2)
            break
    pcm = np.asarray(out, dtype=np.float64) / 32768.0
    if len(pcm) < 8:
        pcm = np.zeros(8)
    return Sample(pcm, loop_start if repeat else None)


class MusicSample:
    """Lazy sequential ADPCM decode, including repeat history; shared deterministic cache."""
    def __init__(self, vb, start):
        initial = decode_sample(vb, start)
        self.pcm, self.loop = initial.pcm, initial.loop
        self.vb, self.address, self.repeat = vb, start, start
        self.s1 = self.s2 = 0
        self.decoded = []

    def extend(self, length):
        while len(self.decoded) < length:
            if self.address + 16 > len(self.vb):
                self.decoded.extend([0] * (length - len(self.decoded)))
                break
            b = self.vb[self.address:self.address + 16]
            shift, filt = b[0] & 15, min(b[0] >> 4, 4)
            if b[1] & 4:
                self.repeat = self.address
            k1, k2 = FILTERS[filt]
            for byte in b[2:]:
                for nib in (byte & 15, byte >> 4):
                    value = ((nib - 16 if nib > 7 else nib) * 4096) >> shift
                    value += (self.s1 * k1 + self.s2 * k2 + 32) >> 6
                    value = max(-32768, min(32767, value))
                    self.decoded.append(value)
                    self.s2, self.s1 = self.s1, value
            self.address = (self.repeat if b[1] & 2 else len(self.vb)) if b[1] & 1 else self.address + 16


def music_resample(sample, pos):
    """Documented SPU four recent samples, integer ROM products, 8-bit fraction."""
    import re
    header = (Path(__file__).parent / '../src/audio/spu_gauss.h').read_text()
    table = np.asarray([int(v) for v in re.findall(r'-?\d+', header.split('= {')[1].split('}')[0])], dtype=np.int64)
    assert len(table) == 512
    index = np.asarray(pos, dtype=np.int64)
    sample.extend(int(index.max()) + 1)
    decoded = np.asarray(sample.decoded, dtype=np.int64)
    fraction = ((np.asarray((pos - index) * 4096, dtype=np.int64)) >> 4) & 255
    result = np.zeros(len(index), dtype=np.int64)
    for offset, coefficient in zip((-3, -2, -1, 0), (255 - fraction, 511 - fraction, 256 + fraction, fraction)):
        source = index + offset
        values = np.where(source >= 0, decoded[np.maximum(source, 0)], 0)
        result += (values * table[coefficient]) >> 15
    return np.clip(result, -32768, 32767) / 32768.0


# ---------------------------------------------------------------- ADSR envelope

def _rate(shift: int, step_field: int, increase: bool):
    """(per sample change of the level, cycles per update) following the SPU rate rules (level 0..0x7FFF)."""
    step = 7 - step_field if increase else -8 + step_field
    return step << max(0, 11 - shift), 1 << max(0, shift - 11)


def envelope(adsr1: int, adsr2: int, n_on: int, release_cap: int = RATE * 4, *, hardware_hold: bool = False) -> tuple[np.ndarray, int]:
    """Return (levels 0..1 for the key-on part with n_on samples, release array until silence)."""
    a_exp = bool(adsr1 & 0x8000)
    a_shift, a_step = (adsr1 >> 10) & 31, (adsr1 >> 8) & 3
    d_shift = (adsr1 >> 4) & 15
    sus = ((adsr1 & 15) + 1) * 0x800
    s_exp, s_dec = bool(adsr2 & 0x8000), bool(adsr2 & 0x4000)
    s_shift, s_step = (adsr2 >> 8) & 31, (adsr2 >> 6) & 3
    r_exp, r_shift = bool(adsr2 & 0x20), adsr2 & 31

    parts: list[np.ndarray] = []
    level = 0.0
    total = 0

    def add(a: np.ndarray) -> None:
        nonlocal level, total
        if len(a):
            parts.append(a)
            total += len(a)
            level = float(a[-1])

    # attack
    inc, cyc = _rate(a_shift, a_step, True)
    slope = inc / cyc
    if a_exp:
        n1 = int(math.ceil(0x6000 / slope)) if slope > 0 else n_on
        add(np.minimum(np.arange(1, n1 + 1) * slope, 0x6000))
        n2 = int(math.ceil((0x7FFF - 0x6000) / (slope / 4)))
        add(np.minimum(0x6000 + np.arange(1, n2 + 1) * (slope / 4), 0x7FFF))
    else:
        n1 = int(math.ceil(0x7FFF / slope)) if slope > 0 else n_on
        add(np.minimum(np.arange(1, n1 + 1) * slope, 0x7FFF))
    if total < n_on:
        level = 0x7FFF
        # decay (always exponential decrease): level *= (1 - 2^-(shift+1)) per sample
        f = 1.0 - 2.0 ** -(d_shift + 1)
        if f > 0 and level > sus:
            n = min(int(math.ceil(math.log(sus / level) / math.log(f))) if f < 1 else n_on, n_on - total)
            n = max(n, 0)
            add(level * f ** np.arange(1, n + 1))
    if total < n_on:
        # sustain
        inc, cyc = _rate(s_shift, s_step, not s_dec)
        if hardware_hold and (adsr2 & 0x1FC0) == 0x1FC0:
            inc = 0
        n = n_on - total
        if not s_dec:
            add(np.minimum(level + np.arange(1, n + 1) * (inc / cyc), 0x7FFF))  # (exponential increase treated as linear)
        else:
            if s_exp:
                f = 1.0 + (inc / cyc) / 32768.0 if inc else 1.0
                add(level * f ** np.arange(1, n + 1))
            else:
                add(np.maximum(level + np.arange(1, n + 1) * (inc / cyc), 0))
    on = np.concatenate(parts)[:n_on] / 0x7FFF if parts else np.zeros(n_on)
    if len(on) < n_on:
        on = np.concatenate([on, np.full(n_on - len(on), on[-1] if len(on) else 0.0)])
    # release from the level reached at key off
    lev = on[-1] * 0x7FFF if n_on else 0.0
    inc, cyc = _rate(r_shift, 0, False)
    if lev <= 1:
        return on, np.zeros(0)
    if r_exp:
        f = 1.0 - 2.0 ** -(r_shift + 1)
        n = min(release_cap, int(math.ceil(math.log(1.0 / lev) / math.log(f))) if 0 < f < 1 else release_cap)
        rel = lev * f ** np.arange(1, max(n, 1) + 1)
    else:
        slope = -inc / cyc  # positive decrease per sample
        n = min(release_cap, int(math.ceil(lev / slope)) if slope > 0 else release_cap)
        rel = np.maximum(lev - np.arange(1, max(n, 1) + 1) * slope, 0)
    return on, rel / 0x7FFF


# ---------------------------------------------------------------- song timeline

class Timeline:
    """Piecewise constant value over time (sample index), default value before the first change."""

    def __init__(self, default: int):
        self.t = [0]
        self.v = [default]

    def set(self, t: int, v: int) -> None:
        if t == self.t[-1]:
            self.v[-1] = v
        else:
            self.t.append(t)
            self.v.append(v)

    def finish(self) -> None:
        self.t = np.asarray(self.t)
        self.v = np.asarray(self.v)

    def at(self, t0: int, n: int) -> np.ndarray | int:
        """Values for samples t0..t0+n-1 (an int when constant)."""
        i0 = int(np.searchsorted(self.t, t0, side="right")) - 1
        i1 = int(np.searchsorted(self.t, t0 + n - 1, side="right")) - 1
        if i0 == i1:
            return int(self.v[i0])
        idx = np.searchsorted(self.t, np.arange(t0, t0 + n), side="right") - 1
        return self.v[idx]


def unroll(ev, loops: int):
    """Repeat the section between controller 0x63 = 20 (loop start) and 30 (loop end) `loops` times."""
    s = e = None
    for i, (_p, st, data, _d) in enumerate(ev):
        if st >> 4 == 0xB and data[0] == 0x63:
            if data[1] == 20 and s is None:
                s = i
            elif data[1] == 30:
                e = i
    if s is None or e is None or loops == 1:
        return ev
    return ev[:s] + ev[s:e + 1] * loops + ev[e + 1:] if loops > 1 else ev[:e + 1]


def build(q: bytes, loops: int):
    """Return (notes, timelines, total samples). notes: (on, off, channel, note, velocity) in samples."""
    _, res, tempo, _ = struct.unpack_from("<4H", q, 0)
    ev, _why = sq.decode(q)
    ev = unroll(ev, loops)
    bpm = float(tempo)
    t = 0.0  # seconds
    chan = {c: {"vol": Timeline(100), "pan": Timeline(0x40), "bend": Timeline(0x40), "prog": Timeline(q[0x10 + c * 16 + 2]), "mod": Timeline(0)}
            for c in range(16)}
    sustain = {c: False for c in range(16)}
    held = {c: [] for c in range(16)}  # notes whose release waits for the pedal
    active: dict[tuple[int, int], list] = {}
    notes: list[list] = []
    sec_per_tick = lambda: 60.0 / (bpm * res)
    for _pos, st, data, d in ev:
        ts = int(round(t * RATE))
        c, kind = st & 15, st >> 4
        if kind == 9:
            n = [ts, None, c, data[0], data[1]]
            notes.append(n)
            active.setdefault((c, data[0]), []).append(n)
        elif kind == 8:
            lst = active.get((c, data[0]))
            if lst:
                n = lst.pop(0)
                if sustain[c]:
                    held[c].append(n)
                else:
                    n[1] = ts
        elif kind == 0xB:
            ctl, v = data[0], data[1]
            if ctl == 7:
                chan[c]["vol"].set(ts, 100 * v >> 7)
            elif ctl == 1:
                chan[c]["mod"].set(ts, v)
            elif ctl == 10:
                chan[c]["pan"].set(ts, v)
            elif ctl == 0x40:
                on = v >= 64
                if sustain[c] and not on:
                    for n in held[c]:
                        n[1] = ts
                    held[c] = []
                sustain[c] = on
        elif kind == 0xC:
            chan[c]["prog"].set(ts, data[0])
        elif kind == 0xE:
            chan[c]["bend"].set(ts, data[0])
        elif st == 0xFF and data[0] == 0x51:
            bpm = float(struct.unpack_from("<H", data, 1)[0])
        t += d * sec_per_tick()
    total = int(round(t * RATE))
    for n in notes:
        if n[1] is None:
            n[1] = total
    for c in chan.values():
        for tl in c.values():
            tl.finish()
    return notes, chan, total


# ---------------------------------------------------------------- voices and mixing

def lfo_wave(h: bytes, index: int) -> np.ndarray:
    """LFO curve `index` (0x40 bytes, 0x3C used by the 0xF0 / 4 phase steps) from the table at SShd +0x18."""
    base = struct.unpack_from("<I", h, 0x18)[0]
    off = struct.unpack_from("<H", h, base + 2 + index * 2)[0]
    return np.frombuffer(h[base + off:base + off + 0x40], dtype=np.uint8).astype(np.float64)


def cubic(pcm: np.ndarray, pos: np.ndarray, loop: int | None) -> np.ndarray:
    """4 point Catmull-Rom interpolation of pcm at the (float) positions, wrapping into the loop or ending silent."""
    n = len(pcm)
    i = np.floor(pos).astype(np.int64)
    f = pos - i
    idx = np.stack([i - 1, i, i + 1, i + 2])
    if loop is None:
        pad = np.concatenate([[0.0], pcm, np.zeros(3)])
        y = pad[np.clip(idx + 1, 0, n + 3)]
    else:
        span = n - loop
        idx = np.where(idx >= n, loop + (idx - loop) % span, idx)
        y = pcm[np.clip(idx, 0, n - 1)]
    y0, y1, y2, y3 = y
    return y1 + 0.5 * f * (y2 - y0 + f * (2 * y0 - 5 * y1 + 4 * y2 - y3 + f * (3 * (y1 - y2) + y3 - y0)))


REVERB_TABLE = 0x800B41D0  # SpuSetReverbPreset: 0x21 halfwords per preset (work area start, then the 32 SPU reverb registers 0x1F801DC0..DFE)
REVERB_NAMES = ("dAPF1 dAPF2 vIIR vCOMB1 vCOMB2 vCOMB3 vCOMB4 vWALL vAPF1 vAPF2 mLSAME mRSAME mLCOMB1 mRCOMB1 mLCOMB2 mRCOMB2 dLSAME dRSAME "
                "mLDIFF mRDIFF mLCOMB3 mRCOMB3 mLCOMB4 mRCOMB4 dLDIFF dRDIFF mLAPF1 mRAPF1 mLAPF2 mRAPF2 vLIN vRIN").split()


def spu_reverb(wet: np.ndarray, exe: bytes, preset: int, out_vol: int) -> np.ndarray:
    """The PlayStation SPU reverb (the documented hardware algorithm) with the game's preset registers.

    The game calls SpuSetReverbPreset(4) and SpuSetReverbVolume(10, 10) when a stage loads (reverb output volume
    10 << 8 per side). Runs at 22.05 kHz on the summed voices whose reverb flag is set. Vectorised in blocks shorter
    than the smallest write -> read delay of the buffer; the one-sample IIR recursion is solved with lfilter. Checked
    against the per-sample version (_spu_reverb_steps) for all 9 presets: equal to 1e-17, except that lfilter does not
    saturate the IIR state, which differs only when the buffer clips (preset 9 with a very loud input).
    """
    from scipy.signal import lfilter
    raw = struct.unpack("<33H", exe_read(exe, REVERB_TABLE + (preset - 1) * 0x42, 0x42))
    r = dict(zip(REVERB_NAMES, raw[1:]))
    sv = {k: (v - 0x10000 if v & 0x8000 else v) / 32768.0 for k, v in r.items() if k[0] == "v"}
    off = {k: v * 4 for k, v in r.items() if k[0] in "dm"}  # registers count 8 byte units = 4 samples
    size = (0x80000 - raw[0] * 8) // 2  # work area in samples (mBASE .. end of SPU RAM)
    # 44.1 -> 22.05 kHz, causal (step k uses samples 2k-2 and 2k-1) so that a streaming player gives the same result
    w = np.concatenate([np.zeros((2, 2)), wet])
    x = w[: len(wet) // 2 * 2].reshape(-1, 2, 2).mean(1)
    n_total = len(x)
    lin, rin = x[:, 0] * sv["vLIN"], x[:, 1] * sv["vRIN"]
    writes = [off[k] for k in ("mLSAME", "mRSAME", "mLDIFF", "mRDIFF", "mLAPF1", "mRAPF1", "mLAPF2", "mRAPF2")]
    reads = [off[k] for k in ("dLSAME", "dRSAME", "dLDIFF", "dRDIFF", "mLCOMB1", "mLCOMB2", "mLCOMB3", "mLCOMB4",
                              "mRCOMB1", "mRCOMB2", "mRCOMB3", "mRCOMB4")]
    reads += [off["mLAPF1"] - off["dAPF1"], off["mRAPF1"] - off["dAPF1"], off["mLAPF2"] - off["dAPF2"], off["mRAPF2"] - off["dAPF2"]]
    delays = [(w - rd) % size for w in writes for rd in reads]  # read-after-write distance; must be >= the block
    # the IIR reads the previous sample of its own cell (solved by lfilter); other writes into that cell limit the block
    delays += [(w - (off[k] - 1)) % size for k in ("mLSAME", "mRSAME", "mLDIFF", "mRDIFF") for w in writes if w != off[k]]
    delays += [(w1 - w2) % size for w1 in writes for w2 in writes if w1 != w2]  # two writes to one cell keep the step order
    if 0 in delays:  # same-step read after write (e.g. preset 1 maps all DIFF addresses to 0): exact per-sample order
        return _spu_reverb_steps(x, raw, out_vol, wet)
    blk = max(1, min(delays + [4096]))
    buf = np.zeros(size)
    out = np.zeros((n_total, 2))
    iir = sv["vIIR"]
    for n0 in range(0, n_total, blk):
        n = np.arange(n0, min(n0 + blk, n_total))
        at = lambda o: (n + o) % size  # noqa: E731
        rd = lambda k: buf[at(off[k])]  # noqa: E731
        sl = slice(n0, n0 + len(n))
        # all reads of the block first (cells written later in the same block must still give their old value)
        taps = {k: rd(k) for k in ("dLSAME", "dRSAME", "dLDIFF", "dRDIFF")}
        combs = {s: sum(sv[f"vCOMB{i}"] * rd(f"m{s}COMB{i}") for i in range(1, 5)) for s in "LR"}
        olds = {(s, apf): buf[at(off[f"m{s}{apf}"] - off[f"d{apf}"])] for s in "LR" for apf in ("APF1", "APF2")}
        prevs = {dst: buf[(n0 + off[dst] - 1) % size] for dst in ("mLSAME", "mRSAME", "mLDIFF", "mRDIFF")}
        for dst, src_in, tap in (("mLSAME", lin, "dLSAME"), ("mRSAME", rin, "dRSAME"), ("mLDIFF", lin, "dRDIFF"), ("mRDIFF", rin, "dLDIFF")):
            xin = src_in[sl] + taps[tap] * sv["vWALL"]
            y, _ = lfilter([iir], [1.0, -(1.0 - iir)], xin, zi=[(1.0 - iir) * prevs[dst]])
            buf[at(off[dst])] = np.clip(y, -1.0, 1.0)
        for side, col in (("L", 0), ("R", 1)):
            o = combs[side]
            for apf in ("APF1", "APF2"):
                old = olds[(side, apf)]
                o = o - sv[f"v{apf}"] * old
                buf[at(off[f"m{side}{apf}"])] = np.clip(o, -1.0, 1.0)
                o = o * sv[f"v{apf}"] + old
            out[sl, col] = o
    return _reverb_out(out, out_vol, wet)


def _spu_reverb_steps(x: np.ndarray, raw: tuple, out_vol: int, wet: np.ndarray) -> np.ndarray:
    """Per-sample SPU reverb (slow reference, hardware step order); x = 22.05 kHz input."""
    r = dict(zip(REVERB_NAMES, raw[1:]))
    v = {k: (val - 0x10000 if val & 0x8000 else val) / 32768.0 for k, val in r.items() if k[0] == "v"}
    o = {k: val * 4 for k, val in r.items() if k[0] in "dm"}
    size = (0x80000 - raw[0] * 8) // 2
    buf = [0.0] * size
    out = np.zeros((len(x), 2))
    for n in range(len(x)):
        lin, rin = x[n, 0] * v["vLIN"], x[n, 1] * v["vRIN"]
        for dst, inp, tap in (("mLSAME", lin, "dLSAME"), ("mRSAME", rin, "dRSAME"), ("mLDIFF", lin, "dRDIFF"), ("mRDIFF", rin, "dLDIFF")):
            prev = buf[(n + o[dst] - 1) % size]
            buf[(n + o[dst]) % size] = max(-1.0, min(1.0, (inp + buf[(n + o[tap]) % size] * v["vWALL"] - prev) * v["vIIR"] + prev))
        for side, col in (("L", 0), ("R", 1)):
            a = sum(v[f"vCOMB{i}"] * buf[(n + o[f"m{side}COMB{i}"]) % size] for i in range(1, 5))
            for apf in ("APF1", "APF2"):
                old = buf[(n + o[f"m{side}{apf}"] - o[f"d{apf}"]) % size]
                a -= v[f"v{apf}"] * old
                buf[(n + o[f"m{side}{apf}"]) % size] = max(-1.0, min(1.0, a))
                a = a * v[f"v{apf}"] + old
            out[n, col] = a
    return _reverb_out(out, out_vol, wet)


def _reverb_out(out: np.ndarray, out_vol: int, wet: np.ndarray) -> np.ndarray:
    out = out * ((out_vol << 8) / 32768.0)
    up = np.empty((2 * len(out), 2))  # 22.05 -> 44.1 kHz: odd samples = step k, even = mean of steps k-1 and k
    up[1::2] = out
    up[0::2] = 0.5 * (np.vstack([np.zeros((1, 2)), out[:-1]]) + out)
    res = np.zeros_like(wet)
    res[: len(up)] = up
    return res


def allocate_voices(jobs: list[dict], stats: dict) -> None:
    """Emulate SeqAllocVoice (0x8009DAB4) for the song's 24 voices; sets job["end"] (cut when stolen) / job["dropped"].

    A voice is free once its envelope is below 0x10 (the driver's per-frame check, voice +0x08 = 1). Allocation: round
    robin from the last voice (DAT_800F75CF) to the next free one; otherwise the stealable voice (+0x08 = 1: from key on,
    or from key off for layers with flag 0x01) with the lowest age (+0x0A); otherwise the first voice with age < 24.
    Ages: a new voice gets the number of active voices; freeing / stealing one decrements the larger ages.
    """
    slots: list[dict | None] = [None] * 24
    ages = [0] * 24
    cursor, count = 0, 0
    pending = []  # (end, slot) of active voices, for freeing in time order

    def release(k: int) -> None:
        nonlocal count
        for i in range(24):
            if slots[i] is not None and ages[i] > ages[k]:
                ages[i] -= 1
        slots[k] = None
        count = max(count - 1, 0)

    import heapq
    for j in jobs:
        t = j["on"]
        while pending and pending[0][0] <= t:
            end, k, jid = heapq.heappop(pending)
            if slots[k] is not None and id(slots[k]) == jid:
                release(k)
        pick = None
        for _ in range(24):
            cursor = (cursor + 1) % 24
            if slots[cursor] is None:
                pick = cursor
                break
        if pick is None:
            cand = [i for i in range(24) if slots[i]["steal_t"] <= t]
            if cand:
                pick = min(cand, key=lambda i: ages[i])
            else:
                pick = next((i for i in range(24) if ages[i] < 24), None)
            if pick is None:
                j["dropped"] = True
                stats["dropped"] += 1
                continue
            old = slots[pick]
            old["end"] = min(old["end"], t)
            stats["stolen"] += 1
            release(pick)
            cursor = pick
        slots[pick] = j
        ages[pick] = count
        count = min(count + 1, 24)
        heapq.heappush(pending, (j["end"], pick, id(j)))


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("member", help="bank member id, e.g. 01B")
    ap.add_argument("--loops", type=int, default=1, help="how many times the loop section plays (0 = stop at the loop end)")
    ap.add_argument("--reverb", type=float, default=1.0, help="reverb level multiplier (0 = off)")
    ap.add_argument("--reverb-mode", choices=("spu", "conv"), default="spu",
                    help="spu = the SPU reverb algorithm with the game's preset registers (default); conv = the old plain convolution reverb")
    ap.add_argument("--reverb-preset", type=int, default=4, help="SPU reverb preset 1..9 (the game uses 4)")
    ap.add_argument("--reverb-vol", type=int, default=10, help="reverb output volume 0..127 (the game uses 10)")
    ap.add_argument("--out", default=None)
    ap.add_argument("--no-mod", action="store_true", help="disable the modulation LFO")
    ap.add_argument("--no-voice-limit", action="store_true", help="do not emulate the 24 SPU voices (no voice stealing)")
    ap.add_argument("--max-seconds", type=float, default=0)
    ap.add_argument("--tail-seconds", type=float, default=3.0, help="release/reverb export tail; validation uses 5 seconds")
    ap.add_argument("--npy", default=None, help="also save the unnormalised float mix (numpy .npy) for comparisons")
    a = ap.parse_args()

    b = (ds.MEMBERS / f"{a.member.upper()}.bin").read_bytes()
    c = ds.parse(b)
    so, ss = c["shd"][0]
    h = b[so:so + ss]
    progs = dt.parse_programs(h)
    curve_off = struct.unpack_from("<I", h, 0x14)[0]
    curve = np.frombuffer(h[curve_off:curve_off + 130], dtype=np.uint8)
    vo, vs = c["vb"][0]
    vb = b[vo:vo + vs]
    qo, qs = c["ssq"]
    q = b[qo:qo + qs]
    exe = exe_bytes()
    pitch_tab = np.frombuffer(exe_read(exe, 0x800B38D0, 1216), dtype="<u2").astype(np.int64)
    pan_tab = np.frombuffer(exe_read(exe, 0x800B3D90, 0x400), dtype=np.uint8).astype(np.int64)
    law = np.frombuffer(exe_read(exe, 0x800B4190, 0x82), dtype=np.uint8).astype(np.int64)

    notes, chan, total = build(q, a.loops)
    if a.max_seconds:
        total = min(total, int(a.max_seconds * RATE))
    if not 0 <= a.tail_seconds <= 30:
        ap.error("tail-seconds must be 0..30")
    tail = int(RATE * a.tail_seconds)
    mix = np.zeros((total + tail, 2))
    wet = np.zeros((total + tail, 2))
    samples: dict[int, Sample] = {}
    stats = {"voices": 0, "silent": 0, "mod": 0, "stolen": 0, "dropped": 0}

    # pass 1: one job per sounding layer, with the time at which the SPU would free its voice (envelope < 0x10)
    jobs = []
    for on_t, off_t, ch, note, vel in notes:
        if on_t >= total:
            continue
        prog = progs.get(int(chan[ch]["prog"].at(on_t, 1)))
        if prog is None:
            stats["silent"] += 1
            continue
        hdr, layers = prog["hdr"], prog["layers"]
        if hdr[0] == 0xFF:  # key indexed kit: one layer per key from header[6]
            hit = [layers[note - hdr[6]]] if 0 <= note - hdr[6] < len(layers) else []
        else:
            hit = [L for L in layers if L[0] <= note <= L[1]]
        hit = [L for L in hit if L != dt.EMPTY]
        if not hit:
            stats["silent"] += 1
            continue
        if hdr[0] != 0xFF and not hdr[0] & 0x80:
            hit = hit[:1]
        n_on = max(int(min(off_t, total + tail) - on_t), 1)
        for L in hit:
            addr = struct.unpack_from("<H", L, 4)[0] * 8
            smp = samples.get(addr)
            if smp is None:
                smp = samples[addr] = MusicSample(vb, addr)
            adsr1, adsr2 = struct.unpack_from("<HH", L, 6)
            env_on, env_rel = envelope(adsr1, adsr2, n_on, hardware_hold=True)
            env = np.concatenate([env_on, env_rel])
            n = min(len(env), mix.shape[0] - on_t)
            if n <= 0:
                continue
            loud = np.nonzero(env[:n] >= 0x10 / 0x7FFF)[0]
            end = on_t + (int(loud[-1]) + 1 if len(loud) else 1)
            if smp.loop is None:  # a one shot sample mutes the voice at its end (pitch at key on)
                rng0 = hdr[4] if L[15] & 0x10 else L[13]
                pitch0 = int(music_pitch(pitch_tab, L[2], note, struct.unpack_from("b", L, 3)[0],
                                         int(np.asarray(chan[ch]["bend"].at(on_t, 1)).flat[0]), rng0))
                end = min(end, on_t + int(len(smp.pcm) / (max(pitch0, 1) / 4096.0)) + 1)
            # stealable (+0x08 = 1) from key on, or from key off for layers with flag 0x01
            jobs.append({"on": on_t, "off": on_t + n_on, "ch": ch, "note": note, "vel": vel, "hdr": hdr, "L": L, "smp": smp,
                         "env": env[:n], "end": end, "steal_t": on_t if not L[15] & 1 else on_t + n_on})
    if not a.no_voice_limit:
        allocate_voices(jobs, stats)

    # pass 2: render
    for j in jobs:
        on_t, ch, note, vel, hdr, L, smp = j["on"], j["ch"], j["note"], j["vel"], j["hdr"], j["L"], j["smp"]
        if j.get("dropped"):
            continue
        env = j["env"]
        if not a.no_voice_limit:
            env = env[: max(j["end"] - on_t, 1)]
        n = len(env)
        end = on_t + n
        fine = struct.unpack_from("b", L, 3)[0]
        rng = hdr[4] if L[15] & 0x10 else L[13]
        bend = chan[ch]["bend"].at(on_t, n)
        pitch = music_pitch(pitch_tab, L[2], note, fine, bend, rng)
        if L[15] & 0x20 and not a.no_mod:  # modulation (LFO) layers: the LFO replaces the bend value while controller 1 is non zero
            depth = np.asarray(chan[ch]["mod"].at(on_t, n))
            if np.any(depth):
                stats["mod"] += 1
                speed = q[0x10 + ch * 16 + 0xC]
                frames = n // (RATE // 60) + 2
                ph, phases = 0, []
                for _ in range(frames):
                    phases.append(ph)
                    ph += speed
                    if ph > 0xEF:
                        ph = speed >> 1
                curve_l = lfo_wave(h, hdr[5] if L[15] & 0x40 else L[14])
                lv = curve_l[np.minimum(np.asarray(phases)[np.arange(n) // (RATE // 60)] >> 2, len(curve_l) - 1)]
                mod_pitch = music_mod_pitch(pitch_tab, L[2], note, fine, bend, L[13], lv, depth)
                pitch = np.where(depth != 0, mod_pitch, pitch)
        step = pitch / 4096.0
        pos = np.cumsum(step) - step if isinstance(step, np.ndarray) and step.ndim else np.arange(n) * float(step)
        sig = music_resample(smp, pos) * env
        if smp.loop is None:  # one shot: silent after the sample ends
            sig[pos >= len(smp.pcm)] = 0.0
        # volume / pan exactly as the game computes them (integer maths, channel changes applied while sounding)
        chvol = np.asarray(chan[ch]["vol"].at(on_t, n))
        chpan = np.asarray(chan[ch]["pan"].at(on_t, n))
        p1 = pan_tab[(L[12] >> 2) + (hdr[2] >> 2) * 32]
        p2 = pan_tab[(p1 >> 2) + (chpan >> 2) * 32]
        idx = (p2 >> 1) & 0x7E
        base = (chvol * hdr[1] * int(curve[vel + 2]) * L[11]) >> 14
        gl, gr = (base * law[idx] >> 7) / 16384.0, (base * law[idx + 1] >> 7) / 16384.0
        mix[on_t:end, 0] += sig * gl
        mix[on_t:end, 1] += sig * gr
        if L[15] & 0x80:
            wet[on_t:end, 0] += sig * gl
            wet[on_t:end, 1] += sig * gr
        stats["voices"] += 1

    if a.reverb > 0 and a.reverb_mode == "spu" and np.abs(wet).max() > 0:
        mix += a.reverb * spu_reverb(wet, exe, a.reverb_preset, a.reverb_vol)
    elif a.reverb > 0 and np.abs(wet).max() > 0:
        rng_ = np.random.default_rng(1)
        n_ir = int(RATE * 1.4)
        ir = rng_.standard_normal((n_ir, 2)) * np.exp(-np.arange(n_ir) / (RATE * 0.35))[:, None]
        ir[: int(RATE * 0.02)] *= np.linspace(0, 1, int(RATE * 0.02))[:, None]
        ir /= np.abs(ir).sum(0).max() / 4
        size = 1 << int(math.ceil(math.log2(len(wet) + n_ir)))
        for k in range(2):
            mix[:, k] += a.reverb * np.fft.irfft(np.fft.rfft(wet[:, k], size) * np.fft.rfft(ir[:, k], size), size)[: len(wet)]

    if a.npy:
        np.save(a.npy, mix)
    peak = float(np.abs(mix).max()) or 1.0
    mix *= 0.89 / peak
    out = Path(a.out) if a.out else ds.ROOT / "extract/wav" / f"{a.member.upper()}.wav"
    out.parent.mkdir(parents=True, exist_ok=True)
    # trim trailing silence
    last = np.where(np.abs(mix).max(1) > 1e-4)[0]
    mix = mix[: (last[-1] + 1 if len(last) else 1)]
    with wave.open(str(out), "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes((np.clip(mix, -1, 1) * 32767).astype("<i2").tobytes())
    print(f"{out}: {len(mix) / RATE:.1f} s, {len(notes)} notes, {stats['voices']} voices, {stats['silent']} silent notes, {stats['mod']} modulated, {stats['stolen']} stolen / {stats['dropped']} dropped voices, "
          f"gain x{0.89 / peak:.2f}")


if __name__ == "__main__":
    main()
