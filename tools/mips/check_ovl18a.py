"""Differential check for readable overlay member 18A ports."""
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/mips"))
import overlay_small_harness as shared

MEMBER = "18A"
FUNCTIONS = (
    "StageClearUi_LookAt_18A",
    "Ovl18A_StringTableIndex",
    "ovl_18A_entry_80136208",
    "Ovl18A_WaitInputTimeout",
    "Ovl18A_SceneInit",
    "Ovl18A_UpdateSpin",
    "Ovl18A_AdvanceAnim",
    "Ovl18A_StartClip",
    "Ovl18A_PlayXa",
    "Ovl18A_PickScene",
)


if __name__ == "__main__":
    if set(FUNCTIONS) != set(shared.CONFIGS[MEMBER]["functions"]):
        raise SystemExit(f"{__file__}: function table does not match overlay {MEMBER}")
    raise SystemExit(shared.main(MEMBER))
