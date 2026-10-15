@echo off
cd /d "%~dp0..\.."
python tools/codex/build_native_viewer.py --native-player --run
if errorlevel 1 pause
