"""Differential check for readable overlay member 184 ports."""
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/mips"))
import overlay_small_harness as shared

MEMBER = "184"
FUNCTIONS = (
    "ovl184_entry_80136000",
    "Ovl184_LoadListLoop",
    "Ovl184_DemoLoop",
    "Ovl184_DrawDemoPrompt",
)


if __name__ == "__main__":
    if set(FUNCTIONS) != set(shared.CONFIGS[MEMBER]["functions"]):
        raise SystemExit(f"{__file__}: function table does not match overlay {MEMBER}")
    raise SystemExit(shared.main(MEMBER))
