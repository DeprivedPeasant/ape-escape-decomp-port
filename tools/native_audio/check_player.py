"""Build src/audio/sp_player.c as a DLL and compare its output with tools/render_seq.py.

Usage: uv run python tools/native_audio/check_player.py [01B 0AD ...] [--all] [--wav]
       uv run python tools/native_audio/check_player.py --sfx [--bank 01B]   effects: C mixer vs tools/render_sfx.py
For every bank: render_seq.py --npy (the Python reference, unnormalised) and the C player with the same options;
prints the relative RMS difference, the peak difference and the voice statistics. --wav also writes the C render to
extract/native_audio/<member>.wav.
"""
import ctypes
import shutil
import subprocess
import sys
import wave
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
sys.path.insert(0, str(HERE.parent))
import dump_sound as ds  # noqa: E402
import render_seq as rs  # noqa: E402

OUT = ROOT / "extract/native_audio"


def build(name: str = "sp_player") -> ctypes.CDLL:
    OUT.mkdir(parents=True, exist_ok=True)
    dll = OUT / f"{name}.dll"
    subprocess.run([shutil.which("gcc"), "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror", "-shared", str(ROOT / "src/audio/sp_player.c"),
                    "-o", str(dll), "-lm", "-static-libgcc"], check=True)
    lib = ctypes.CDLL(str(dll))
    lib.sp_create.restype = ctypes.c_void_p
    lib.sp_create.argtypes = [ctypes.c_char_p, ctypes.c_size_t, ctypes.c_char_p, ctypes.c_size_t, ctypes.c_int, ctypes.c_int,
                              ctypes.c_int, ctypes.c_int]
    lib.sp_render.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_float), ctypes.c_int]
    lib.sp_render.restype = ctypes.c_int
    lib.sp_destroy.argtypes = [ctypes.c_void_p]
    lib.sp_stats.argtypes = [ctypes.c_void_p] + [ctypes.POINTER(ctypes.c_int)] * 4
    lib.sp_sfx_create.restype = ctypes.c_void_p
    lib.sp_sfx_create.argtypes = [ctypes.c_char_p, ctypes.c_size_t]
    lib.sp_sfx_bank.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_char_p, ctypes.c_size_t, ctypes.c_char_p, ctypes.c_size_t]
    lib.sp_sfx_play_id.argtypes = [ctypes.c_void_p, ctypes.c_int]
    lib.sp_sfx_auto_off.argtypes = [ctypes.c_void_p, ctypes.c_double]
    lib.sp_sfx_render.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_float), ctypes.c_int]
    lib.sp_sfx_render.restype = ctypes.c_int
    lib.sp_sfx_destroy.argtypes = [ctypes.c_void_p]
    return lib


def check_sfx(lib, stage_bank: str | None) -> None:
    """Every effect id (slot 0, plus slot 2 with --bank): the C effect mixer against render_sfx.render."""
    import render_sfx as rx
    exe = rs.exe_bytes()
    keep = []  # the C side keeps pointers to the bank bytes
    worst = 0.0
    n_ok = 0
    n_total = 0
    for i, (slot, prog, note) in enumerate(rx.sfx_table(exe)):
        if slot == 2 and not stage_bank:
            continue
        h, vb = rx.bank(slot, stage_bank)
        ref, _desc = rx.render(h, vb, prog, note, exe, 4.0)
        if ref is None:
            continue
        n_total += 1
        s = lib.sp_sfx_create(exe, len(exe))
        keep += [h, vb]
        lib.sp_sfx_bank(s, slot, h, len(h), vb, len(vb))
        lib.sp_sfx_auto_off(s, 4.0)
        lib.sp_sfx_play_id(s, i)
        n = len(ref) + rs.RATE
        buf = (ctypes.c_float * (2 * n))()
        lib.sp_sfx_render(s, buf, n)
        lib.sp_sfx_destroy(s)
        c = np.frombuffer(buf, dtype=np.float32).reshape(-1, 2).astype(np.float64)
        d = np.abs(c[: len(ref)] - ref).max() / (np.abs(ref).max() or 1)
        rest = float(np.abs(c[len(ref):]).max())
        worst = max(worst, d)
        n_ok += d < 1e-5 and rest < 1e-3
        if d >= 1e-5 or rest >= 1e-3:
            print(f"id {i}: relative peak diff {d:.2e}, after the reference end {rest:.1e}")
    print(f"{n_ok} effects match (worst relative peak difference {worst:.2e})")
    if n_ok != n_total:
        raise AssertionError("sound-effect regression")


def render_c(lib, member: str, loops: int = 1, flags: int = 0) -> tuple[np.ndarray, list[int]]:
    bank = (ds.MEMBERS / f"{member}.bin").read_bytes()
    exe = rs.exe_bytes()
    p = lib.sp_create(bank, len(bank), exe, len(exe), loops, flags, 4, 10)
    if not p:
        raise SystemExit(f"{member}: not a sound bank")
    chunks, quiet = [], 0
    block = rs.RATE
    buf = (ctypes.c_float * (2 * block))()
    while quiet < 3:  # stop after 3 silent seconds
        n = lib.sp_render(p, buf, block)
        chunks.append(np.frombuffer(buf, dtype=np.float32).reshape(-1, 2).astype(np.float64).copy())
        quiet = quiet + 1 if n == 0 else 0
    st = [ctypes.c_int() for _ in range(4)]
    lib.sp_stats(p, *[ctypes.byref(x) for x in st])
    lib.sp_destroy(p)
    return np.concatenate(chunks), [x.value for x in st]


def main() -> None:
    args = [a for a in sys.argv[1:] if not a.startswith("--") and (sys.argv.index(a) == 0 or sys.argv[sys.argv.index(a) - 1] != "--bank")]
    if "--all" in sys.argv:
        args = sorted(p.stem for p in (ROOT / "extract/wav").glob("*.wav"))
    lib = build("sp_player_sfx" if "--sfx" in sys.argv else "sp_player")
    if "--sfx" in sys.argv:
        bank_arg = sys.argv[sys.argv.index("--bank") + 1] if "--bank" in sys.argv else None
        check_sfx(lib, bank_arg)
        return
    for m in args or ["01B"]:
        ref_path = OUT / f"{m}_ref.npy"
        subprocess.run([sys.executable, str(HERE.parent / "render_seq.py"), m, "--tail-seconds", "5", "--npy", str(ref_path), "--out", str(OUT / f"{m}_ref.wav")],
                       check=True, capture_output=True)
        ref = np.load(ref_path)
        c, st = render_c(lib, m)
        n = min(len(ref), len(c))
        d = c[:n] - ref[:n]
        tail_ref = float(np.abs(ref[n:]).max()) if len(ref) > n else 0.0
        tail_c = float(np.abs(c[n:]).max()) if len(c) > n else 0.0
        rms = float(np.sqrt((d ** 2).mean()) / (np.sqrt((ref[:n] ** 2).mean()) or 1))
        print(f"{m}: {n / rs.RATE:6.1f} s  rel rms diff {rms:.2e}  peak diff {np.abs(d).max():.2e} (ref peak {np.abs(ref).max():.2f})"
              f"  tails ref {tail_ref:.1e} c {tail_c:.1e}  voices {st[0]} stolen {st[1]} dropped {st[2]} silent {st[3]}", flush=True)
        if rms > 5e-4 or np.abs(d).max() > max(float(np.abs(ref).max()), 1e-9) * 2e-3 or max(tail_ref, tail_c) > 1e-3:
            raise AssertionError(f"{m}: C/Python music comparison failed")
        if "--wav" in sys.argv:
            peak = float(np.abs(c).max()) or 1.0
            with wave.open(str(OUT / f"{m}.wav"), "wb") as w:
                w.setnchannels(2)
                w.setsampwidth(2)
                w.setframerate(rs.RATE)
                w.writeframes((np.clip(c * 0.89 / peak, -1, 1) * 32767).astype("<i2").tobytes())


if __name__ == "__main__":
    main()
