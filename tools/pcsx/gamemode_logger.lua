-- g_GameMode logger for PCSX-Redux (Ape Escape USA, SCUS-94423).
-- Load with: pcsx-redux -dofile <this file>   (tools\pcsx-redux-modelog.cmd does it)
--
-- Logs two things, without pausing the emulator:
--   ENTER  - main() is about to call the mode handler (exec bp on the jalr at 0x8005BEB0),
--            with the mode's table entry and the current level/area.
--   SET    - something wrote g_GameMode (write bp), with the writer's PC and the new value.
-- Output goes to the Lua console and to notes/runtime/gamemode-log.txt.

local ffi = require('ffi')
local bit = require('bit')

local LOG_PATH ='<repo>/notes/runtime/gamemode-log.txt'

local G_GAMEMODE      = 0x800F4470 -- u16
local G_CURRENT_LEVEL = 0x800F4474 -- u16
local G_CURRENT_AREA  = 0x800F4476 -- u16
local G_STAGE_EXIT    = 0x800F447C -- u8
local G_MODE_TABLE    = 0x800AFA30 -- 37 x {entry u32, member u32}
local MAIN_DISPATCH   = 0x8005BEB0 -- jalr in main() that calls the mode entry

local mem = PCSX.getMemPtr()
local function off(addr) return bit.band(addr, 0x1FFFFF) end
local function u8(addr) return mem[off(addr)] end
local function u16(addr) return ffi.cast('uint16_t*', mem + off(addr))[0] end
local function u32(addr) return ffi.cast('uint32_t*', mem + off(addr))[0] end

local logFile = nil
pcall(function() logFile = io.open(LOG_PATH, 'a') end)

local events = 0
local function log(line)
    events = events + 1
    local stamp = os and os.date and os.date('%H:%M:%S') or ''
    local text = string.format('%s #%04d %s', stamp, events, line)
    print(text)
    if logFile then
        logFile:write(text, '\n')
        logFile:flush()
    end
end

local function context()
    return string.format('level=0x%02X area=0x%02X exit=0x%02X',
        u16(G_CURRENT_LEVEL), u16(G_CURRENT_AREA), u8(G_STAGE_EXIT))
end

log('---- logger loaded ----')

-- Mode entered: g_GameMode is final by the time main() dispatches.
modeEnterBp = PCSX.addBreakpoint(MAIN_DISPATCH, 'Exec', 4, 'g_GameMode enter', function()
    local mode = u16(G_GAMEMODE)
    local entry, member = 0, 0
    if mode < 37 then
        entry = u32(G_MODE_TABLE + mode * 8)
        member = u32(G_MODE_TABLE + mode * 8 + 4)
    end
    local where = member == 0xFFFFFFFF and 'resident' or string.format('ovl 0x%03X', member)
    log(string.format('ENTER mode=%2d (0x%02X) entry=%08X %-9s %s',
        mode, mode, entry, where, context()))
end)

-- Mode changed: decode the store at PC to get the value being written.
modeWriteBp = PCSX.addBreakpoint(G_GAMEMODE, 'Write', 2, 'g_GameMode write', function(address, width)
    local regs = PCSX.getRegisters()
    local pc = regs.pc
    -- Ignore RAM clears: BIOS (0xBFCxxxxx) and start()'s BSS memset at 0x800A3670.
    if pc >= 0xBFC00000 or pc == 0x800A3670 then return end
    local insn = u32(pc)
    local rt = bit.band(bit.rshift(insn, 16), 31)
    local value = bit.band(regs.GPR.r[rt], 0xFFFF)
    log(string.format('SET   mode=%2d (0x%02X) by pc=%08X  old=%d  %s',
        value, value, pc, u16(G_GAMEMODE), context()))
end)
