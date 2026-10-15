@echo off
setlocal
set "AE_ROOT=%~dp0..\.."
set "AE_VIEWER=%AE_ROOT%\extract\codex\native\native_viewer.exe"
if exist "%AE_ROOT%\extract\codex\native\native_viewer_latest.txt" for /f "usebackq delims=" %%V in ("%AE_ROOT%\extract\codex\native\native_viewer_latest.txt") do set "AE_VIEWER=%AE_ROOT%\extract\codex\native\%%V"
if not exist "%AE_VIEWER%" goto missing
if not exist "%AE_ROOT%\extract\codex\native\area_01.aev" goto missing
if not exist "%AE_ROOT%\extract\codex\native\008_0000BC.aea" goto missing
if not exist "%AE_ROOT%\extract\members\05B.bin" goto missing
if not exist "%AE_ROOT%\extract\disc\SCUS_944.23" goto missing
start "" "%AE_VIEWER%" "%AE_ROOT%\extract\codex\native\area_01.aev" --actor "%AE_ROOT%\extract\codex\native\008_0000BC.aea" --ground "%AE_ROOT%\extract\members\05B.bin" --collision-exe "%AE_ROOT%\extract\disc\SCUS_944.23" --player --focus-actor
exit /b 0
:missing
echo Native demo assets are missing. From the repository root, run:
echo uv run --offline python tools/codex/build_native_viewer.py --player --run
pause
exit /b 1
