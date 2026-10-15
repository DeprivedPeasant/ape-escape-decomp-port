@echo off
rem Boot Ape Escape in PCSX-Redux with all loggers (game mode, monkey AI, player state).
rem -debugger and -interpreter are required for Lua breakpoints to fire.
if not exist "%~dp0..\notes\runtime" mkdir "%~dp0..\notes\runtime"
start "" "%~dp0pcsx-redux\pcsx-redux.exe" -bios "%~dp0..\bios\scph1001.bin" -iso "%~dp0..\disc\SCUS-94423.cue" -debugger -interpreter -dofile "%~dp0pcsx\all_loggers.lua" -run
