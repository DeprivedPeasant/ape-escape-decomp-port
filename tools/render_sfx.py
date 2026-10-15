"""Render the game's sound effects (PlaySfx ids) to WAV files.

PlaySfx(id) (0x800877F8) reads the 8 byte record id of the table at 0x800B2D08: s16 bank slot, s16 program, s16 note,
s16 last voice. Slot 0 = the common effect bank (SShd member 0x00B, samples in member 0x00A), slot 2 = the effect bank
of the loaded stage (the second SShd / VB pair of the stage's sound bank member, e.g. 01B); slot 1 would be member
0x01E (samples 0x01F) but no table entry uses it. (program, note) selects a tiny event sequence of the SFX chunk
(notes/24): `A0 note velocity program` starts a voice (velocity 0 = key off), `B0` controllers with extended
parameters, `FF 2F` ends it. Effect tracks always run at 60 BPM / 480 ticks per beat (SeqReadDelta), so a tick is
1/480 s.

Extended controllers (from SeqCmd_Ctl07_Volume / SeqCmd_Ctl41 and the per frame voice update), parameters
[time, value, program, note] apply to the voice playing (program, note) of the same track:
  * 0x07 volume ramp: from the voice volume to `value` over time * 4 frames (linear, FUN_800A0B0C)
  * 0x0A pan ramp: same shape for the pan
  * 0x41 pitch glide: value bit 7 clear = up value / 10 semitones, set = down (256 - value) / 10 semitones, linear over
    time * 4 frames (the pitch is clamped to notes 12..243)
Controllers 1 / 2 / 0x60 occur in a few sequences and are ignored here.

Usage:
  uv run python tools/render_sfx.py list                 table of all ids with their sequences
  uv run python tools/render_sfx.py 12                   render id 12 (slot 0) -> extract/sfx/012.wav
  uv run python tools/render_sfx.py 190 --bank 01B       slot 2 ids need the stage sound bank
  uv run python tools/render_sfx.py all [--bank 01B]     render every id (slot 2 ids only with --bank)
  uv run python tools/render_sfx.py note                 write notes/29-sound-effects.md (ids, sequences, callers)
"""
import argparse
import struct
import sys
import wave
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
import dump_sound as ds  # noqa: E402
import dump_tones as dt  # noqa: E402
import render_seq as rs  # noqa: E402

RATE = rs.RATE
TABLE, COUNT = 0x800B2D08, 198  # ids 0..197 (the table ends with a zero record and other data)
TICK = 1.0 / 480  # seconds per tick of an effect track (60 BPM, 480 PPQ)
FRAME = 1.0 / 60


def sfx_table(exe: bytes) -> list[tuple[int, int, int]]:
    return [struct.unpack_from("<3h", rs.exe_read(exe, TABLE + 8 * i, 6)) for i in range(COUNT)]


def bank(slot: int, member: str | None) -> tuple[bytes, bytes]:
    """(SShd chunk, VB sample block) of a bank slot."""
    if slot == 0:
        return (ds.MEMBERS / "00B.bin").read_bytes(), (ds.MEMBERS / "00A.bin").read_bytes()
    if slot == 1:
        return (ds.MEMBERS / "01E.bin").read_bytes(), (ds.MEMBERS / "01F.bin").read_bytes()
    if not member:
        raise SystemExit("slot 2 effects need --bank <stage sound bank member>, e.g. --bank 01B")
    b = (ds.MEMBERS / f"{member.upper()}.bin").read_bytes()
    c = ds.parse(b)
    for k, (so, ss) in enumerate(c["shd"]):
        if struct.unpack_from("<I", b, so + 0x20)[0] != 0xFFFFFFFF:
            vo, vs = c["vb"][min(k, len(c["vb"]) - 1)]
            return b[so:so + ss], b[vo:vo + vs]
    raise SystemExit(f"member {member} has no effect bank")


def ramp_at(t: np.ndarray, start: float, ramps: list[tuple[float, float, float]]) -> np.ndarray:
    """Value over time t (seconds from key on): piecewise linear ramps (t0, duration, target) applied in order."""
    v = np.full(len(t), float(start))
    cur = float(start)
    for t0, dur, target in ramps:
        if dur <= 0:
            v[t >= t0] = target
        else:
            k = np.clip((t - t0) / dur, 0.0, 1.0)
            sel = t >= t0
            v[sel] = cur + (target - cur) * k[sel]
        cur = target
    return v


def render(h: bytes, vb: bytes, prog: int, note: int, exe: bytes, max_seconds: float = 4.0):
    """Mix of one effect (stereo float array), plus a description of its events."""
    seqs = dt.sfx_sequences(h)
    if (prog, note) not in seqs:
        return None, "no sequence"
    ev, err = dt.sfx_events(h, seqs[(prog, note)])
    if err:
        return None, err
    progs = dt.sfx_programs(h)
    curve_off = struct.unpack_from("<I", h, 0x14)[0]
    curve = h[curve_off:curve_off + 130]
    pan_tab = np.frombuffer(rs.exe_read(exe, 0x800B3D90, 0x400), dtype=np.uint8).astype(np.int64)
    law = np.frombuffer(rs.exe_read(exe, 0x800B4190, 0x82), dtype=np.uint8).astype(np.int64)
    voices: list[dict] = []
    t = 0.0
    desc = []
    for st, data, d in ev:
        if st == 0xA0:
            n, vel, p = data
            match = [v for v in voices if v["key"] == (p, n) and v["off"] is None]
            if vel == 0:
                for v in match:
                    v["off"] = t
                desc.append(f"{t:.2f}s off {n}")
            else:
                pr = progs.get(p)
                idx = n - pr["hdr"][6] if pr else -1
                if pr and 0 <= idx < len(pr["layers"]) and pr["layers"][idx] != dt.EMPTY:
                    voices.append({"key": (p, n), "on": t, "off": None, "vel": vel, "hdr": pr["hdr"], "L": pr["layers"][idx],
                                   "vol": [], "pan": [], "pitch": []})
                desc.append(f"{t:.2f}s note {n} vel {vel} prog {p}")
        elif st == 0xB0:
            ctl = data[0]
            if ctl in (0x07, 0x0A, 0x41) and len(data) >= 5:
                tm, val, p, n = data[1], data[2], data[3], data[4]
                for v in voices:
                    if v["key"] == (p, n) and v["off"] is None:
                        dur = tm * 4 * FRAME
                        if ctl == 0x07:
                            v["vol"].append((t - v["on"], dur, val))
                        elif ctl == 0x0A:
                            v["pan"].append((t - v["on"], dur, val))
                        else:
                            v["pitch"].append((t - v["on"], dur, val / 10.0 if val < 0x80 else -(256 - val) / 10.0))
                desc.append(f"{t:.2f}s ctl {ctl:02X} {data[1:].hex()}")
            else:
                desc.append(f"{t:.2f}s ctl {ctl:02X} {data[1:].hex()} (ignored)")
        t += d * TICK
    end_t = t
    total = int((max(end_t, max([v["on"] for v in voices], default=0)) + max_seconds) * RATE)
    mix = np.zeros((total + RATE * 4, 2))
    samples: dict[int, rs.Sample] = {}
    for v in voices:
        L, hdr = v["L"], v["hdr"]
        addr = struct.unpack_from("<H", L, 4)[0] * 8
        smp = samples.get(addr) or samples.setdefault(addr, rs.decode_sample(vb, addr))
        on = int(v["on"] * RATE)
        off_t = v["off"] if v["off"] is not None else (v["on"] + max_seconds if smp.loop is not None else None)
        n_on = int(((off_t - v["on"]) if off_t is not None else max_seconds) * RATE)
        n_on = max(n_on, 1)
        adsr1, adsr2 = struct.unpack_from("<HH", L, 6)
        env_on, env_rel = rs.envelope(adsr1, adsr2, n_on)
        env = np.concatenate([env_on, env_rel])[: mix.shape[0] - on]
        n = len(env)
        tt = np.arange(n) / RATE
        # pitch: the glides add semitones (clamped to notes 12..243 like the driver)
        if v["pitch"]:
            acc, ramps = 0.0, []
            for t0, dur, dst in v["pitch"]:
                acc += dst
                ramps.append((t0, dur, acc))
            shift = ramp_at(tt, 0.0, ramps)
        else:
            shift = 0.0
        key = np.clip(v["key"][1] + shift, 12, 243)
        semis = key - L[2] + struct.unpack_from("b", L, 3)[0] / 16.0
        step = np.minimum(2.0 ** (np.asarray(semis, dtype=float) / 12.0), 4.0)
        pos = np.cumsum(step) - step if np.ndim(step) else np.arange(n) * float(step)
        sig = rs.cubic(smp.pcm, pos, smp.loop) * env
        if smp.loop is None:
            sig[pos >= len(smp.pcm)] = 0.0
        vol = ramp_at(tt, v["vel"], v["vol"]) if v["vol"] else np.full(n, float(v["vel"]))
        panv = ramp_at(tt, 0x40, v["pan"]).astype(np.int64) if v["pan"] else np.full(n, 0x40, dtype=np.int64)
        p1 = pan_tab[(L[12] >> 2) + (hdr[2] >> 2) * 32]
        p2 = pan_tab[(p1 >> 2) + (np.clip(panv, 0, 127) >> 2) * 32]
        idx = (p2 >> 1) & 0x7E
        velc = np.array([curve[min(int(x), 127) + 2] for x in vol])
        base = (127 * hdr[1] * velc * L[11]) >> 14
        mix[on:on + n, 0] += sig * (base * law[idx] >> 7) / 16384.0
        mix[on:on + n, 1] += sig * (base * law[idx + 1] >> 7) / 16384.0
    last = np.where(np.abs(mix).max(1) > 1e-4)[0]
    return mix[: (last[-1] + 1 if len(last) else 1)], "; ".join(desc)


def write_wav(path: Path, mix: np.ndarray) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    peak = float(np.abs(mix).max()) or 1.0
    mix = mix * (0.89 / peak) if peak > 0.89 else mix
    with wave.open(str(path), "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes((np.clip(mix, -1, 1) * 32767).astype("<i2").tobytes())


# ---------------------------------------------------------------- call sites (which code plays which id)
# function address -> argument register holding the effect id (a0 = 4, a1 = 5)
SFX_FUNCS = {0x800877F8: ("PlaySfx", 4), 0x80087A20: ("PlaySfxAt", 4), 0x80055404: ("ObjPlaySfx", 5),
             0x800878E0: ("SoundLoopStart", 5), 0x800882C4: ("SoundLoopStartEx", 5), 0x800884B8: ("SoundLoopStartTracked", 5)}


def _code_images():
    """(block name, load address, bytes) of the main EXE code and every code overlay of tools/overlay_map.json."""
    import json
    exe = rs.exe_bytes()
    yield "CODE", 0x80010000, exe[0x800:]
    for o in json.loads((ds.ROOT / "tools/overlay_map.json").read_text())["overlays"]:
        m = int(o["index"], 16)
        p = ds.MEMBERS / f"{m:03X}.bin"
        if p.exists():
            yield f"ovl_{m:03X}", int(o["load_addr"], 16), p.read_bytes()


def _functions():
    import csv
    out: dict[str, list[tuple[int, str]]] = {}
    for r in csv.DictReader(open(ds.ROOT / "notes/inventory/functions.csv")):
        out.setdefault(r["block"], []).append((int(r["entry"].split("::")[-1], 16), r["name"]))
    for v in out.values():
        v.sort()
    return out


def call_sites() -> dict[int, list[tuple[str, str]]]:
    """{effect id: [(function name, entry point)]} for calls with a constant id (li reg, imm within 8 instructions
    before the jal or in its delay slot, no other jal / jr in between)."""
    import bisect
    funcs = _functions()
    res: dict[int, list[tuple[str, str]]] = {}
    for blk, base, img in _code_images():
        words = np.frombuffer(img[: len(img) // 4 * 4], dtype="<u4")
        targets = {0x0C000000 | ((a >> 2) & 0x3FFFFFF): (a, reg) for a, (_n, reg) in SFX_FUNCS.items()}
        for i in np.nonzero(np.isin(words, list(targets)))[0]:
            _fa, reg = targets[int(words[i])]
            val = None
            for j in [i + 1] + list(range(i - 1, max(i - 9, -1), -1)):
                w = int(words[j])
                op, rs_, rt = w >> 26, (w >> 21) & 31, (w >> 16) & 31
                if j < i and (op in (2, 3) or (op == 0 and (w & 0x3F) in (8, 9))):
                    break
                if rt == reg and rs_ == 0 and op in (9, 0xD):  # addiu / ori reg, zero, imm
                    val = w & 0xFFFF
                    break
                if rt == reg and op in (8, 9, 0xD, 0xF, 0x20, 0x21, 0x23, 0x24, 0x25) or (op == 0 and (w >> 11) & 31 == reg):
                    break  # the register is computed: not a constant
            if val is None:
                continue
            addr = base + int(i) * 4
            fl = funcs.get(blk, [])
            k = bisect.bisect_right(fl, (addr, "￿")) - 1
            name = fl[k][1] if k >= 0 else "?"
            res.setdefault(val, []).append((name, f"{blk}:{addr:08X}"))
    return res


def write_note(exe: bytes, banks: list[str]) -> None:
    """notes/29-sound-effects.md: every id with its sequence, length and the functions that play it."""
    tab = sfx_table(exe)
    sites = call_sites()
    lines = ["# Sound effects (generated by tools/render_sfx.py note)", "",
             "PlaySfx / PlaySfxAt / ObjPlaySfx / SoundLoopStart* id -> table 0x800B2D08 (bank slot, program, note) -> SFX "
             "sequence (notes/24). `python tools/render_sfx.py <id>` renders one (slot 2 = per stage bank, `--bank`). "
             "Callers: functions that pass the id as a constant (EXE and code overlays; ids computed at run time are not "
             "listed). Lengths of looping effects (no key off) are the 4 s render limit.", ""]
    lines += ["| Id | Slot / prog / note | Length | Events | Called from |", "|---|---|---|---|---|"]
    for i, (slot, prog, note) in enumerate(tab):
        who = sites.get(i, [])
        names = sorted({n for n, _ in who})
        callers = ", ".join(names[:6]) + (f" (+{len(names) - 6})" if len(names) > 6 else "")
        if slot == 2:
            lines.append(f"| {i} | 2 / {prog} / {note} | stage | per stage bank | {callers} |")
            continue
        h, vb = bank(slot, None)
        mix, desc = render(h, vb, prog, note, exe)
        ln = f"{len(mix) / RATE:.2f} s" if mix is not None else "-"
        lines.append(f"| {i} | {slot} / {prog} / {note} | {ln} | {desc.replace('|', '/')} | {callers} |")
    lines += ["", "## Stage effect banks (slot 2)", "",
              "Number of (program 0, note) effects in the second SShd of each stage sound bank; ids 172 + note.", "",
              "| Bank | Effects | Lengths (s) |", "|---|---|---|"]
    for m in banks:
        try:
            h, vb = bank(2, m)
        except SystemExit:
            lines.append(f"| {m} | 0 (no effect bank) | |")
            continue
        notes_ = sorted(n for (p, n) in dt.sfx_sequences(h) if p == 0)
        lens = []
        for n in notes_:
            mix, _d = render(h, vb, 0, n, exe)
            lens.append(f"{len(mix) / RATE:.1f}" if mix is not None else "-")
        lines.append(f"| {m} | {len(notes_)} | {' '.join(lens)} |")
    lines += ["", "## Sound bank of each area", "",
              "Every area's load list (notes/12) has one record with postprocess 2 that loads a sound bank member to "
              "0x80146000; `SoundBankUpload(2)` uploads its samples and starts the song (`BgmStart`). The second SShd / VB "
              "of that member is the area's effect bank (slot 2).", "",
              "| Areas | Bank | Song length (s) |", "|---|---|---|"]
    import dump_areas as da
    by_bank: dict[int, list[int]] = {}
    for area, ptr in da.area_chains():
        rec = ptr
        for _ in range(16):
            m, post = struct.unpack_from("<Hh", da.EXE, rec - 0x80010000 + 0x800)
            if post < 0:
                break
            if post == 2 and da.exe_u32(rec + 4) == 0x80146000:
                by_bank.setdefault(m, []).append(area)
            rec += 8
    for m, areas in sorted(by_bank.items()):
        b = (ds.MEMBERS / f"{m:03X}.bin").read_bytes()
        c = ds.parse(b)
        ln = "-"
        if c and c["ssq"]:
            notes, _ch, total = rs.build(b[c["ssq"][0]:c["ssq"][0] + c["ssq"][1]], 1)
            ln = f"{total / RATE:.0f}"
        lines.append(f"| {', '.join(f'0x{a:02X}' for a in areas)} | {m:03X} | {ln} |")
    out = ds.ROOT / "notes/29-sound-effects.md"
    out.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"{sum(len(v) for v in sites.values())} call sites for {len(sites)} ids -> {out}")


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("what", help="effect id, 'all' or 'list'")
    ap.add_argument("--bank", default=None, help="stage sound bank member for slot 2 effects, e.g. 01B")
    ap.add_argument("--max-seconds", type=float, default=4.0, help="how long a looping voice without key off sounds")
    ap.add_argument("--out", default=None)
    a = ap.parse_args()
    exe = rs.exe_bytes()
    tab = sfx_table(exe)
    if a.what == "note":
        banks = sorted(p.stem for p in (ds.ROOT / "extract/wav").glob("*.wav"))
        write_note(exe, banks)
        return
    if a.what == "list":
        for i, (slot, prog, note) in enumerate(tab):
            if slot == 2 and not a.bank:
                print(f"{i:3d} slot 2 prog {prog} note {note}")
                continue
            h, vb = bank(slot, a.bank)
            mix, desc = render(h, vb, prog, note, exe, a.max_seconds)
            print(f"{i:3d} slot {slot} prog {prog} note {note:3d} {len(mix) / RATE if mix is not None else 0:5.2f} s  {desc}")
        return
    ids = range(COUNT) if a.what == "all" else [int(a.what, 0)]
    for i in ids:
        slot, prog, note = tab[i]
        if slot == 2 and not a.bank:
            if a.what != "all":
                bank(slot, None)
            continue
        h, vb = bank(slot, a.bank)
        mix, desc = render(h, vb, prog, note, exe, a.max_seconds)
        if mix is None:
            print(f"{i}: {desc}")
            continue
        out = Path(a.out) if a.out and a.what != "all" else ds.ROOT / "extract/sfx" / (f"{i:03d}.wav" if slot != 2 else f"{i:03d}_{a.bank.upper()}.wav")
        write_wav(out, mix)
        print(f"{out}: {len(mix) / RATE:.2f} s  {desc}")


if __name__ == "__main__":
    main()
