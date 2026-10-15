"""Differential check for readable overlay member 185 ports."""
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/mips"))
import overlay_small_harness as shared

MEMBER = "185"
FUNCTIONS = (
    "ovl185_entry_80146000",
    "Ovl185_Run",
    "Ovl185_Update",
    "Ovl185_LoadScene",
)


if __name__ == "__main__":
    if set(FUNCTIONS) != set(shared.CONFIGS[MEMBER]["functions"]):
        raise SystemExit(f"{__file__}: function table does not match overlay {MEMBER}")
    raise SystemExit(shared.main(MEMBER))
