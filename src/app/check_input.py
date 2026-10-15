"""Build and run the native_input unit test (plain gcc, no SDL, no game data)."""
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent


def main():
    gcc = shutil.which("gcc")
    if not gcc:
        print("gcc not found")
        return 1
    with tempfile.TemporaryDirectory() as tmp:
        exe = Path(tmp) / "test_input.exe"
        subprocess.run([gcc, "-std=c11", "-Wall", "-Wextra", "-Werror", "-O1", str(HERE / "native_input.c"),
                        str(HERE / "test_input.c"), "-lm", "-o", str(exe)], check=True)
        return subprocess.run([str(exe)]).returncode


if __name__ == "__main__":
    sys.exit(main())
