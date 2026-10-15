@echo off
rem Boot Ape Escape in PCSX-Redux with the g_GameMode logger loaded.
rem -debugger and -interpreter are required for Lua breakpoints to fire.
rem Note: -debugger is saved to PCSX-Redux's config; pass -no-debugger next time to turn it off.
if not exist "%~dp0..\notes\runtime" mkdir "%~dp0..\notes\runtime"
start "" "%~dp0pcsx-redux\pcsx-redux.exe" -bios "%~dp0..\bios\scph1001.bin" -iso "%~dp0..\disc\SCUS-94423.cue" -debugger -interpreter -dofile "%~dp0pcsx\gamemode_logger.lua" -run
