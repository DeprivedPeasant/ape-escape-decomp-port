-- Player state logger for PCSX-Redux (Ape Escape USA, SCUS-94423).
-- Load with: pcsx-redux -debugger -interpreter -dofile <this file>
--            (tools\pcsx-redux-playerlog.cmd does it)
--
-- Write breakpoints on g_Player.state, .currentGadget and .health. Each change is logged
-- with the new value, the code that requested it (return address: for state changes that
-- is the caller of PlayerSetState), and the player's context.
-- Output: Lua console + notes/runtime/player-state-log.txt
-- Type  pmark('text')  in the Lua console to drop a labelled marker into the log.

local ffi = require('ffi')
local bit = require('bit')

local ROOT = '<repo>/'
local LOG_PATH = ROOT .. 'notes/runtime/player-state-log.txt'

local PLAYER          = 0x800EC1E0
local P_POS           = PLAYER + 0x24  -- s32[3]
local P_ANIMFRAME     = PLAYER + 0x5C  -- u16
local P_STATE         = PLAYER + 0x5E  -- u16
local P_CONTACT       = PLAYER + 0x50  -- u32
local P_HEALTH        = PLAYER + 0xE8  -- s16
local P_GADGET        = PLAYER + 0xF2  -- s8
local P_GROUNDTYPE    = PLAYER + 0x138 -- u16
local SCR_HELD        = 0x1F800054     -- g_PlayerScratch.held
local G_CURRENT_AREA  = 0x800F4476
local SET_STATE_STORE = 0x80064208     -- PlayerSetState

local stateNames = {}
pcall(function() stateNames = dofile(ROOT .. 'tools/pcsx/player_state_names.lua') or {} end)

local mem = PCSX.getMemPtr()
local scratch = PCSX.getScratchPtr and PCSX.getScratchPtr() or nil
local function off(addr) return bit.band(addr, 0x1FFFFF) end
local function u8(addr) return mem[off(addr)] end
local function s8(addr) local v = mem[off(addr)]; if v >= 0x80 then v = v - 0x100 end; return v end
local function u16(addr) return ffi.cast('uint16_t*', mem + off(addr))[0] end
local function s16(addr) return ffi.cast('int16_t*', mem + off(addr))[0] end
local function u32(addr) return ffi.cast('uint32_t*', mem + off(addr))[0] end
local function s32(addr) return ffi.cast('int32_t*', mem + off(addr))[0] end
local function scratchU32(addr)
    if scratch == nil then return 0 end
    return ffi.cast('uint32_t*', scratch + bit.band(addr, 0x3FF))[0]
end

local logFile = nil
pcall(function() logFile = io.open(LOG_PATH, 'a') end)

local events = 0
local function log(line)
    events = events + 1
    local stamp = os and os.date and os.date('%H:%M:%S') or ''
    local text = string.format('%s #%05d %s', stamp, events, line)
    print(text)
    if logFile then
        logFile:write(text, '\n')
        logFile:flush()
    end
end

-- Value being stored at `target` by the store at PC (sb/sh/sw), masked to `bytes`.
local function storeValue(pc, target, bytes)
    local regs = PCSX.getRegisters()
    local insn = u32(pc)
    local rs = bit.band(bit.rshift(insn, 21), 31)
    local rt = bit.band(bit.rshift(insn, 16), 31)
    local imm = bit.band(insn, 0xFFFF)
    if imm >= 0x8000 then imm = imm - 0x10000 end
    local ea = bit.band(regs.GPR.r[rs] + imm, 0x1FFFFF)
    local lane = bit.band(off(target) - ea, 3)
    local v = bit.rshift(regs.GPR.r[rt], lane * 8)
    if bytes == 1 then return bit.band(v, 0xFF) end
    return bit.band(v, 0xFFFF)
end

local function stateLabel(s)
    local name = stateNames[s]
    if name then return string.format('0x%02X %s', s, name) end
    return string.format('0x%02X', s)
end

local function context()
    return string.format('gadget=%d hp=%d frame=%d held=%04X contact=%08X water=%d area=0x%02X pos=(%d,%d,%d)',
        s8(P_GADGET), s16(P_HEALTH), u16(P_ANIMFRAME), bit.band(scratchU32(SCR_HELD), 0xFFFF),
        u32(P_CONTACT), bit.rshift(u16(P_GROUNDTYPE), 12) == 1 and 1 or 0, u16(G_CURRENT_AREA),
        s32(P_POS), s32(P_POS + 4), s32(P_POS + 8))
end

-- The function that asked for the change. Inside PlayerSetState, $ra may already be
-- clobbered by its internal call, so read the copy its prologue saved at sp+0x1C
-- (stored at 0x80064220, frame size 0x20).
local function requester(pc)
    local regs = PCSX.getRegisters()
    if pc > SET_STATE_STORE + 0x18 and pc < SET_STATE_STORE + 0x24C then
        local savedRa = u32(regs.GPR.r[29] + 0x1C)
        return string.format('caller=%08X', savedRa - 8)
    end
    return string.format('pc=%08X', pc)
end

local nameCount = 0
for _ in pairs(stateNames) do nameCount = nameCount + 1 end
log(string.format('---- player state logger loaded (%d state names, scratchpad %s) ----',
    nameCount, scratch and 'ok' or 'unavailable'))

-- The BIOS shell runs from the same RAM during boot, so stay quiet until the game's
-- own entry point (start, 0x800A3660) has run, and skip start()'s BSS clear.
local GAME_ENTRY, BSS_CLEAR_PC = 0x800A3660, 0x800A3670
local armed = false
playerBps = {}
playerBps[#playerBps + 1] = PCSX.addBreakpoint(GAME_ENTRY, 'Exec', 4, 'player logger arm', function()
    armed = true
    log('game started; logging armed')
    return false -- one-shot: remove this breakpoint
end)
local function ignore(pc)
    return not armed or pc == BSS_CLEAR_PC or pc >= 0xBFC00000
end

playerBps[#playerBps + 1] = PCSX.addBreakpoint(P_STATE, 'Write', 2, 'player state', function()
    local pc = PCSX.getRegisters().pc
    if ignore(pc) then return end
    local old = u16(P_STATE)
    local new = storeValue(pc, P_STATE, 2)
    if new == old then return end
    log(string.format('STATE %-26s -> %-26s %s  %s', stateLabel(old), stateLabel(new), requester(pc), context()))
end)

playerBps[#playerBps + 1] = PCSX.addBreakpoint(P_GADGET, 'Write', 1, 'player gadget', function()
    local pc = PCSX.getRegisters().pc
    if ignore(pc) then return end
    local old = s8(P_GADGET)
    local new = storeValue(pc, P_GADGET, 1)
    if new >= 0x80 then new = new - 0x100 end
    if new == old then return end
    log(string.format('GADGET %d -> %d  pc=%08X  state=%s', old, new, pc, stateLabel(u16(P_STATE))))
end)

playerBps[#playerBps + 1] = PCSX.addBreakpoint(P_HEALTH, 'Write', 2, 'player health', function()
    local pc = PCSX.getRegisters().pc
    if ignore(pc) then return end
    local old = s16(P_HEALTH)
    local new = storeValue(pc, P_HEALTH, 2)
    if new >= 0x8000 then new = new - 0x10000 end
    if new == old then return end
    log(string.format('HEALTH %d -> %d  pc=%08X  state=%s', old, new, pc, stateLabel(u16(P_STATE))))
end)

function pmark(text)
    log('MARK  ' .. tostring(text))
end
