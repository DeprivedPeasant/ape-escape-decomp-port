"""Continuous replay of a captured Ape Escape player trace against the native player module.

Seeds the native player ONCE from the first captured frame of a segment, then for every following
frame feeds only the captured per-frame inputs, runs ae_player_update + ae_player_body_step (with the
real area collision data through Codex's read-only host adapter) and compares the native state with the
captured post-collision RAM.  The native state is carried from frame to frame; captured RAM is never
copied back into it inside a segment.

Per-frame world inputs the module cannot compute itself (everything else is native):
  * pad: held, (changed), stick class/magnitude, camera-relative stick direction cam_dir[4],
    stick_cam[2], right-stick gesture, input-lock flag        (captured `input[0..11]`)
  * g_FrameCounter                                            (captured `input[12]`)
  * g_PlayerMode                                              (captured `mode`; only 0 is supported)
  * externals not in the 0x150-byte Player image: damage_enabled (level flag), hurt_blink, dash_boost
    are seeded ONCE from the first frame of a segment and then carried natively
    (`--feed-externals` feeds the captured values every frame instead)
  * the ground query (GetGroundHeightEx + GetSurfaceType at the native position) is computed from the
    real area grid/solids; the captured query is only used as a diagnostic (`--captured-ground` feeds it)
  * collision (CollisionUpdate body pass incl. wall/ceiling probes, ledge ray, collider scan) is native.
`--derive` computes `changed`, stick_class and stick_cam natively (from held history, stick_mag and the
native facing) instead of using the captured values, to test those couplings.

Segments never silently include unsupported frames: a frame is unsupported if g_PlayerMode != 0, the
captured post state is a gadget state (>= 0x80), or the module / body pass / volume callbacks return any
unsupported bit.  An unsupported frame ends the segment, is not compared, and the next supported frame
reseeds from its captured pre-update RAM.
"""
import argparse
import ctypes as C
import json
import os
import shutil
import subprocess
import sys
import tempfile
from collections import Counter, OrderedDict
from pathlib import Path

HERE = Path(__file__).resolve().parent
PLAYER_DIR = HERE.parent


def find_main_root():
    """Main checkout holding the (ignored) extracted game data and Codex's host sources."""
    for p in HERE.parents:
        if (p / 'extract/disc/SCUS_944.23').exists() and (p / 'src/app/native_player_host.c').exists():
            return p
        if p.name == '.wt' and (p.parent / 'extract/disc/SCUS_944.23').exists():
            return p.parent
    raise SystemExit('cannot find main checkout with extract/disc and src/; pass --codex-root')


def build(codex_root, out_dir, gcc='gcc'):
    srcs = [HERE / 'replay_host.c'] + [codex_root / 'src' / n for n in ('app/native_player_host.c', 'collision/native_collision.c', 'collision/native_ground.c')]
    srcs += [PLAYER_DIR / n for n in ('native_player.c', 'native_player_body.c', 'native_player_volume.c', 'native_player_pad.c', 'native_gadget_slots.c')]
    srcs += [codex_root / 'src/core' / n for n in ('ae_ram.c', 'ae_pad.c')]
    out = Path(out_dir) / ('replay_host' + ('.dll' if sys.platform == 'win32' else '.so'))
    cmd = [shutil.which(gcc), '-std=c11', '-O2', '-Wall', '-Wextra', '-shared']
    cmd += [str(s) for s in srcs] + ['-lm', '-o', str(out)]
    if sys.platform != 'win32':
        cmd.append('-fPIC')
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode:
        raise SystemExit(r.stderr)
    return out


def s_int(b, w, sign):
    return int.from_bytes(b, 'little', signed=sign)


class Lib:
    def __init__(self, dll):
        L = C.CDLL(str(dll))
        L.replay_open.restype = C.c_void_p
        L.replay_open.argtypes = [C.c_char_p, C.c_char_p, C.c_char_p, C.c_uint]
        L.replay_close.argtypes = [C.c_void_p]
        L.replay_debug_set_collider_pos.argtypes = [C.c_void_p, C.c_int, C.c_int, C.c_int, C.c_int]
        L.replay_field_name.restype = C.c_char_p
        L.replay_seed.argtypes = [C.c_void_p, C.c_char_p, C.c_int32, C.c_int32, C.c_int32]
        L.replay_frame.argtypes = [C.c_void_p, C.POINTER(C.c_int32), C.c_uint32, C.c_uint32, C.c_uint32, C.c_uint32,
                                   C.POINTER(C.c_int32), C.POINTER(C.c_int64), C.POINTER(C.c_int64), C.POINTER(C.c_uint32)]
        self.L = L
        self.fields = []
        for i in range(L.replay_field_count()):
            self.fields.append((L.replay_field_name(i).decode(), *(L.replay_field_meta(i, k) for k in range(4))))
        self.nelem = sum(f[3] for f in self.fields)


def load_trace(path):
    frames, marks = [], []
    for line in Path(path).read_text().splitlines():
        r = json.loads(line)
        if r.get('kind') == 'frame':
            frames.append(r)
        elif r.get('kind') == 'mark':
            marks.append((r['frame'], r['label']))
    return frames, marks


def label_for(marks, frame):
    lab = 'unmarked'
    for f, l in marks:
        if f <= frame:
            lab = l
    return lab


def expected_values(lib, post):
    vals = []
    for name, off, w, cnt, sign in lib.fields:
        for k in range(cnt):
            vals.append((name, k, s_int(post[off + k * w: off + (k + 1) * w], w, sign)))
    return vals


def run(args, lib, frames, marks, area_paths, policy):
    """policy: 'free' (carry native state through divergences) or 'resync' (a divergent frame ends the segment)."""
    L = lib.L
    derive = (1 if args.derive else 0) | (2 if args.derive else 0) | (4 if args.derive else 0) | (8 if args.feed_externals else 0)
    covered = bytearray(0x150)
    for name, off, w, cnt, sign in lib.fields:
        for i in range(off, off + w * cnt):
            covered[i] = 1
    segs = []                 # finished segment records
    seg = None
    excluded = []             # (frame, reason)
    unsup_bits = Counter()
    first_div = OrderedDict() # field[elem] -> detail
    field_frames = Counter()  # field -> number of divergent frames
    diag = dict(ground_checked=0, ground_core_mismatch=0, ground_water_y_only_mismatch=0, ground_first=None,
                externals_checked=0, hurt_blink_mismatch=0, dash_boost_mismatch=0, externals_first=None)
    last_ext = None           # native (hurt_blink, dash_boost) after the previous in-sync frame
    uncovered_changes = Counter()
    handle = None
    cur_area = None
    prev_held = 0
    compared = matched = 0
    per_label = OrderedDict()
    nb = lib.nelem
    out = (C.c_int64 * (nb + 4))(); aux = (C.c_int64 * 8)(); bits = (C.c_uint32 * 4)()

    def close_seg(end_reason):
        nonlocal seg
        if seg is not None:
            seg['end_reason'] = end_reason
            segs.append(seg)
            seg = None

    for r in frames:
        fr, area = r['frame'], r['area']
        row = per_label.setdefault(label_for(marks, fr), dict(frames=0, compared=0, matched=0, mismatched=0, excluded=0))
        row['frames'] += 1
        pre, post = bytes.fromhex(r['pre']), bytes.fromhex(r['post_collision'])
        assert len(pre) == len(post) == 0x150
        for i in range(0x150):
            if not covered[i] and pre[i] != post[i]:
                uncovered_changes[i & ~1] += 1
        reason = None
        if r['mode'] != 0:
            reason = f"g_PlayerMode={r['mode']}"
        elif int.from_bytes(post[0x5e:0x60], 'little') >= 0x80:
            reason = 'captured post state is a gadget state (>=0x80)'
        if reason is None and (int.from_bytes(pre[0x5e:0x60], 'little') >= 0x80):
            reason = 'captured pre state is a gadget state (>=0x80)'
        if area != cur_area:
            close_seg('area change')
            if handle:
                L.replay_close(handle)
            handle = L.replay_open(str(area_paths[area]).encode(), str(args.exe).encode(), str(args.member).encode(), area)
            if not handle:
                raise SystemExit(f'cannot open area {area}')
            cur_area = area
            for (oa, oi, ox, oy, oz) in args.collider_override:
                if oa == area and not L.replay_debug_set_collider_pos(handle, oi, ox, oy, oz):
                    raise SystemExit(f'bad --collider-override index {oi}')
        if reason:
            close_seg('unsupported frame: ' + reason)
            excluded.append((fr, reason)); row['excluded'] += 1
            continue
        inp = r['input']
        if seg is None:
            L.replay_seed(handle, pre, inp[13], inp[14], inp[15])
            seg = dict(start=fr, frames=0, matched_prefix=None, first_mismatch=None, mismatched_frames=0, label=label_for(marks, fr),
                       in_sync=True)
            prev_held = (inp[0] ^ inp[1]) & 0xFFFFFFFF
            last_ext = None
        arr = (C.c_int32 * 16)(*[(v if v < 2**31 else v - 2**32) for v in inp])
        gro = None
        if args.captured_ground:
            gro = (C.c_int32 * 6)(*r['ground'])
        L.replay_frame(handle, arr, r['mode'], inp[12] & 0xFFFFFFFF, derive, prev_held, gro, out, aux, bits)
        prev_held = inp[0] & 0xFFFFFFFF
        b = [int(x) for x in bits]
        if any(b):
            for k, name in enumerate(('player_update', 'body_step', 'volume_status', 'host_walls_failed')):
                if b[k]:
                    unsup_bits[(name, b[k])] += 1
            reason = 'unsupported bits ' + ','.join(f'{n}=0x{b[k]:X}' for k, n in enumerate(('update', 'body', 'volume', 'walls')) if b[k])
            close_seg('unsupported frame: ' + reason)
            excluded.append((fr, reason)); row['excluded'] += 1
            continue
        # diagnostics only valid while the native state equals the captured one at frame start
        if seg['in_sync']:
            diag['ground_checked'] += 1
            cg = r['ground']
            ng = [int(aux[i]) for i in range(6)]
            ng[3] &= 0xFFFF; ng[4] &= 0xFFFF
            core_same = [cg[0], cg[1], cg[2], cg[3] & 0xFFFF, cg[4] & 0xFFFF] == ng[:5]
            if not core_same or cg[5] != ng[5]:
                diag['ground_core_mismatch' if not core_same else 'ground_water_y_only_mismatch'] += 1
                diag['ground_first'] = diag['ground_first'] or dict(frame=fr, captured=cg, native=ng)
            if last_ext is not None and not args.feed_externals:
                diag['externals_checked'] += 1
                if last_ext[0] != inp[14]:
                    diag['hurt_blink_mismatch'] += 1
                if last_ext[1] != inp[15]:
                    diag['dash_boost_mismatch'] += 1
                if last_ext[0] != inp[14] or last_ext[1] != inp[15]:
                    diag['externals_first'] = diag['externals_first'] or dict(frame=fr, native=last_ext, captured=inp[14:16])
        last_ext = (int(aux[6]), int(aux[7]))
        exp = expected_values(lib, post)
        diffs = []
        for (name, k, real), nat in zip(exp, out):
            if real != int(nat):
                diffs.append((name, k, real, int(nat)))
        seg['frames'] += 1
        compared += 1; row['compared'] += 1
        if args.dump and args.dump[0] <= fr <= args.dump[1] and policy == args.dump_policy:
            def v3(b, o):
                return [int.from_bytes(b[o + 4 * i:o + 4 * i + 4], 'little', signed=True) for i in range(3)]
            pu = bytes.fromhex(r['post_update'])
            print(f'[dump {policy}] frame {fr} pre pos/vel {v3(pre, 0x24)} {v3(pre, 0x30)} | captured post_update pos/vel/contact '
                  f'{v3(pu, 0x24)} {v3(pu, 0x30)} 0x{int.from_bytes(pu[0x50:0x54], "little"):X} | captured post_collision '
                  f'{v3(post, 0x24)} {v3(post, 0x30)} 0x{int.from_bytes(post[0x50:0x54], "little"):X} | ground {r["ground"]} aux {[int(x) for x in aux]}')
            for d in diffs:
                print('    diff', d)
        if diffs:
            row['mismatched'] += 1
            seg['mismatched_frames'] += 1
            if seg['first_mismatch'] is None:
                seg['first_mismatch'] = dict(frame=fr, fields=sorted({d[0] for d in diffs}),
                                             detail=[dict(field=d[0], element=d[1], original=d[2], native=d[3]) for d in diffs[:12]])
                seg['matched_prefix'] = seg['frames'] - 1
            for name in {d[0] for d in diffs}:
                field_frames[name] += 1
            for name, k, real, nat in diffs:
                key = f'{name}[{k}]'
                if key not in first_div:
                    first_div[key] = dict(frame=fr, segment_start=seg['start'], frames_since_seed=seg['frames'], original=real, native=nat,
                                          scenario=label_for(marks, fr), onset_in_sync=seg['in_sync'])
            seg['in_sync'] = False
            if policy == 'resync':
                close_seg('divergence (resync)')
        else:
            matched += 1; row['matched'] += 1
    close_seg('end of trace')
    if handle:
        L.replay_close(handle)
    return dict(policy=policy, compared=compared, matched=matched, mismatched=compared - matched, excluded=len(excluded),
                excluded_by_reason=dict(Counter(e[1] for e in excluded)), excluded_frames_first=excluded[:5],
                unsupported_bit_counts={f'{k[0]}=0x{k[1]:X}': v for k, v in unsup_bits.items()},
                segments=segs, first_divergence_per_field=first_div, divergent_frames_per_field=dict(field_frames),
                per_scenario=per_label, diagnostics=diag,
                uncovered_ram_bytes_changed_pre_to_post_collision={f'0x{k:X}': v for k, v in sorted(uncovered_changes.items())})


def main():
    root = None
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('trace', type=Path)
    ap.add_argument('--codex-root', type=Path, help='main checkout containing tools/codex (read-only)')
    ap.add_argument('--exe', type=Path)
    ap.add_argument('--member', type=Path, help='archive member 0x008 (player data)')
    ap.add_argument('--members-dir', type=Path)
    ap.add_argument('--policy', choices=['free', 'resync', 'both'], default='both')
    ap.add_argument('--derive', action='store_true', help='derive changed / stick_class / stick_cam natively')
    ap.add_argument('--feed-externals', action='store_true', help='feed captured damage_enabled/hurt_blink/dash_boost each frame')
    ap.add_argument('--captured-ground', action='store_true', help='feed the captured ground query instead of the native one')
    ap.add_argument('--report', type=Path)
    ap.add_argument('--dump', type=lambda s: tuple(int(x) for x in s.split(':')), help='FIRST:LAST frames to print in detail')
    ap.add_argument('--dump-policy', default='resync', choices=['free', 'resync'])
    ap.add_argument('--collider-override', action='append', default=[], metavar='AREA:INDEX:X,Y,Z',
                    type=lambda s: (int(s.split(':')[0]), int(s.split(':')[1]), *[int(v) for v in s.split(':')[2].split(',')]),
                    help='HYPOTHESIS TEST ONLY: replace the centre of one static collider (the capture has no live collider table)')
    ap.add_argument('--gcc', default='gcc')
    ap.add_argument('--max-segments', type=int, default=40, help='segments listed in the text summary')
    args = ap.parse_args()
    root = args.codex_root or find_main_root()
    args.exe = args.exe or root / 'extract/disc/SCUS_944.23'
    args.member = args.member or root / 'extract/members/008.bin'
    members = args.members_dir or root / 'extract/members'
    sys.path.insert(0, str(root / 'tools'))
    import dump_areas as da
    area_members = da.area_members()
    frames, marks = load_trace(args.trace)
    if not frames:
        raise SystemExit('empty trace')
    area_paths = {a: members / f'{area_members[a]:03X}.bin' for a in {r['area'] for r in frames}}
    with tempfile.TemporaryDirectory(ignore_cleanup_errors=True) as td:
        lib = Lib(build(root, td, args.gcc))
        results = {}
        for pol in (['free', 'resync'] if args.policy == 'both' else [args.policy]):
            results[pol] = run(args, lib, frames, marks, area_paths, pol)
        # the ctypes handle holds the DLL until process exit; temp dir cleanup may fail on Windows
        del lib
    # continuity of the capture itself
    cont_bad = sum(1 for a, b in zip(frames, frames[1:]) if a['post_collision'] != b['pre'] or b['frame'] != a['frame'] + 1)
    report = dict(trace=str(args.trace), frames=len(frames), capture_discontinuities_post_collision_vs_next_pre=cont_bad,
                  options=dict(derive=args.derive, feed_externals=args.feed_externals, captured_ground=args.captured_ground, collider_override=args.collider_override),
                  results=results)
    for pol, res in results.items():
        print(f'== policy {pol}: compared {res["compared"]}, matched {res["matched"]}, mismatched {res["mismatched"]}, excluded {res["excluded"]}')
        print('   excluded by reason:', res['excluded_by_reason'])
        segs = res['segments']
        full = [s for s in segs if s['mismatched_frames'] == 0]
        print(f'   segments: {len(segs)}; fully matched: {len(full)}; longest fully matched: '
              f'{max((s["frames"] for s in full), default=0)} frames')
        for s in sorted(segs, key=lambda s: -s['frames'])[:args.max_segments]:
            fm = s['first_mismatch']
            print(f'   seg {s["start"]}+{s["frames"]} ({s["label"]}) matched_prefix={s["matched_prefix"] if fm else s["frames"]} '
                  f'bad={s["mismatched_frames"]} first={fm["frame"] if fm else None}:{fm["fields"] if fm else ""} end={s["end_reason"]}')
        print('   first divergence per field:')
        for k, d in res['first_divergence_per_field'].items():
            print(f'     {k}: {d}')
        print('   diagnostics:', json.dumps(res['diagnostics']))
    if cont_bad:
        print('WARNING: capture is not continuous', cont_bad)
    if args.report:
        args.report.write_text(json.dumps(report, indent=2, default=str) + '\n')
    bad = any(r['mismatched'] for r in results.values())
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
