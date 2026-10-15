-- Monkey AI state logger for PCSX-Redux (Ape Escape USA, SCUS-94423).
-- Load with: pcsx-redux -debugger -interpreter -dofile <this file>
--            (tools\pcsx-redux-monkeylog.cmd does it)
--
-- Write breakpoints on each monkey slot's aiState (+0x78) and activity (+0x7A) bytes.
-- Every change is logged with the PC that wrote it (the state code that made the
-- transition), the new state's handler from the jump table, and context.
-- Output: Lua console + notes/runtime/monkey-ai-log.txt

local ffi = require('ffi')
local bit = require('bit')

local LOG_PATH = '<repo>/notes/runtime/monkey-ai-log.txt'

local G_MONKEYS       = 0x800E5500 -- Monkey[8], 0x250 each
local MONKEY_SIZE     = 0x250
local OFF_POS         = 0x24       -- s32[3]
local OFF_AISTATE     = 0x78
local OFF_ACTIVITY    = 0x7A
local OFF_ID          = 0x7C
local OFF_DIST_PLAYER = 0x210
local AI_JUMP_TABLE   = 0x800A4AB0 -- u32[0x43]
local G_CURRENT_AREA  = 0x800F4476
local G_GAMEMODE      = 0x800F4470
local G_GADGET        = 0x800EC2D2
local G_MONKEY_COUNT  = 0x800AEB68

local mem = PCSX.getMemPtr()
local function off(addr) return bit.band(addr, 0x1FFFFF) end
local function u8(addr) return mem[off(addr)] end
local function u16(addr) return ffi.cast('uint16_t*', mem + off(addr))[0] end
local function u32(addr) return ffi.cast('uint32_t*', mem + off(addr))[0] end
local function s32(addr) return ffi.cast('int32_t*', mem + off(addr))[0] end

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

-- Byte value being stored at `target` by the store instruction at PC (sb/sh/sw):
-- effective address = rs + simm16; pick the byte lane that lands on `target`.
local function storeValue(pc, target)
    local regs = PCSX.getRegisters()
    local insn = u32(pc)
    local rs = bit.band(bit.rshift(insn, 21), 31)
    local rt = bit.band(bit.rshift(insn, 16), 31)
    local imm = bit.band(insn, 0xFFFF)
    if imm >= 0x8000 then imm = imm - 0x10000 end
    local ea = bit.band(regs.GPR.r[rs] + imm, 0x1FFFFF)
    local lane = bit.band(off(target) - ea, 3)
    return bit.band(bit.rshift(regs.GPR.r[rt], lane * 8), 0xFF)
end

local function handlerFor(state)
    if state < 0x43 then return u32(AI_JUMP_TABLE + state * 4) end
    return 0
end

local lastArea = -1
local function areaContext()
    local area = u16(G_CURRENT_AREA)
    if area ~= lastArea then
        lastArea = area
        log(string.format('AREA  area=0x%02X mode=%d monkeys=%d gadget=%d',
            area, u16(G_GAMEMODE), u16(G_MONKEY_COUNT), u8(G_GADGET)))
    end
end

local function monkeyLine(slot, base)
    return string.format('slot=%d id=%3d act=%d dist=%5d pos=(%d,%d,%d)',
        slot, u8(base + OFF_ID), u8(base + OFF_ACTIVITY), s32(base + OFF_DIST_PLAYER),
        s32(base + OFF_POS), s32(base + OFF_POS + 4), s32(base + OFF_POS + 8))
end

log('---- monkey AI logger loaded ----')

monkeyBps = {}
for slot = 0, 7 do
    local base = G_MONKEYS + slot * MONKEY_SIZE

    monkeyBps[#monkeyBps + 1] = PCSX.addBreakpoint(base + OFF_AISTATE, 'Write', 1,
        'monkey ' .. slot .. ' aiState', function()
            local pc = PCSX.getRegisters().pc
            local old = u8(base + OFF_AISTATE)
            local new = storeValue(pc, base + OFF_AISTATE)
            if new == old then return end
            areaContext()
            log(string.format('STATE 0x%02X -> 0x%02X (handler %08X) by pc=%08X  gadget=%d  %s',
                old, new, handlerFor(new), pc, u8(G_GADGET), monkeyLine(slot, base)))
        end)

    monkeyBps[#monkeyBps + 1] = PCSX.addBreakpoint(base + OFF_ACTIVITY, 'Write', 1,
        'monkey ' .. slot .. ' activity', function()
            local pc = PCSX.getRegisters().pc
            local old = u8(base + OFF_ACTIVITY)
            local new = storeValue(pc, base + OFF_ACTIVITY)
            if new == old then return end
            areaContext()
            log(string.format('ACT   %d -> %d by pc=%08X  state=0x%02X  %s',
                old, new, pc, u8(base + OFF_AISTATE), monkeyLine(slot, base)))
        end)
end

-- Manual markers: type  mark('caught one')  in the Lua console to annotate the log.
function mark(text)
    log('MARK  ' .. tostring(text))
end
