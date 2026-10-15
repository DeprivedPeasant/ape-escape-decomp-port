"""Differential check for readable overlay member 183 ports."""
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/mips"))
import overlay_small_harness as shared

MEMBER = "183"
FUNCTIONS = (
    "ovl183_entry_80136000",
    "SoundTest_Update",
    "SoundTest_DrawMainMenu",
    "SoundTest_DrawCommonTest",
    "SoundTest_DrawSightTest",
)


if __name__ == "__main__":
    if set(FUNCTIONS) != set(shared.CONFIGS[MEMBER]["functions"]):
        raise SystemExit(f"{__file__}: function table does not match overlay {MEMBER}")
    raise SystemExit(shared.main(MEMBER))
