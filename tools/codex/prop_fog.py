"""Original area-header fog configuration for ordinary props (read-only, no game data committed).

Reads the raw area header at the pointer stored at member +0x4D0 (same location prop_materials.py / sky_props.py use)
and decodes the fog fields the original AreaLoad / ApplyLevelFog consume. The far u16 (+0x2E) is reported as source
metadata only: the F4 factor helper does not use it (see native_prop_fog.md). No renderer state is touched.

Usage: uv run python tools/codex/prop_fog.py <member hex> [...]   -> one line of fog fields per member
"""
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import prop_materials as pm  # noqa: E402

HEADER_MIN = 0x30
COEFFICIENT = 0xF000
DEFAULT_NEAR = 0x4B0


def _load_blob(blob: bytes, member: int) -> dict:
    pm._need(blob, 0x4D0, 4, "area header pointer")
    ah = struct.unpack_from("<I", blob, 0x4D0)[0]
    pm._need(blob, ah, HEADER_MIN, "area header")
    rgb = tuple(blob[ah + 0x20:ah + 0x23])
    enabled = struct.unpack_from("<h", blob, ah + 0x28)[0]
    src_near, src_far = struct.unpack_from("<HH", blob, ah + 0x2C)
    near = src_near if enabled != 0 else DEFAULT_NEAR
    return {
        "member": member,
        "header_offset": ah,
        "rgb": rgb,
        "fog_enabled": enabled,
        "near": src_near,
        "far": src_far,
        "effective_near": near,
        "coefficient": COEFFICIENT,
        "draw_distance": near * 4 + 0x400,
    }


def load(member: int) -> dict:
    """Fog fields of an area member. Raises ValueError (PropMaterialError) on a malformed source."""
    return _load_blob((pm.MEMBERS / f"{member:03X}.bin").read_bytes(), member)


def main() -> None:
    for a in sys.argv[1:]:
        member = int(a, 16)
        r = load(member)
        print(f"member 0x{member:03X}: header 0x{r['header_offset']:X} rgb {r['rgb']} enabled {r['fog_enabled']} "
              f"near {r['near']} far {r['far']} effective_near {r['effective_near']} "
              f"coefficient 0x{r['coefficient']:X} draw_distance {r['draw_distance']}")


if __name__ == "__main__":
    main()
