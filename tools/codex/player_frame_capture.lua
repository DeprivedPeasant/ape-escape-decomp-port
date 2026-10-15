-- PCSX-Redux original-game per-frame evidence; separate from Claude's logger.
-- Load after the game boots; call aeplayer_trace_start(), perform movements,
-- use aeplayer_trace_mark('walk/run/jump...'), then aeplayer_trace_stop().
-- Output is ignored game-state evidence, never commit it.
local ffi = require('ffi')
local bit = require('bit')
local ROOT = '<repo>/'
local mem = PCSX.getMemPtr()
local scratch = PCSX.getScratchPtr and PCSX.getScratchPtr() or nil
assert(scratch, 'Scratchpad access required; capture cannot use invented zero values')
local function off(a) return bit.band(a,0x1fffff) end
local function u8(a) return tonumber(mem[off(a)]) end
local function u16(a) return tonumber(ffi.cast('uint16_t*',mem+off(a))[0]) end
local function s16(a) return tonumber(ffi.cast('int16_t*',mem+off(a))[0]) end
local function u32(a) return tonumber(ffi.cast('uint32_t*',mem+off(a))[0]) end
local function sc16(a) return tonumber(ffi.cast('uint16_t*',scratch+bit.band(a,0x3ff))[0]) end
local function hex(a,n)
 local t={}; for i=0,n-1 do t[#t+1]=string.format('%02x',u8(a+i)) end
 return table.concat(t)
end
local function json(x)
 if type(x)=='number' then return string.format('%.0f',x) end
 if type(x)=='string' then return '"'..x:gsub('\\','\\\\'):gsub('"','\\"'):gsub('\n','\\n')..'"' end
 local t={}
 if x[1]~=nil then for i=1,#x do t[#t+1]=json(x[i]) end;return '['..table.concat(t,',')..']' end
 for k,v in pairs(x) do t[#t+1]=json(k)..':'..json(v) end
 return '{'..table.concat(t,',')..'}'
end
local log, pending, enabled = nil,nil,false
local function emit(x) if log then log:write(json(x),'\n');log:flush() end end
local function player() return hex(0x800ec1e0,0x150) end
function aeplayer_trace_start(path)
 assert(not enabled,'Capture already running')
 log=assert(io.open(path or ROOT..'extract/codex/cloud-player-review/player_trace.jsonl','w'))
 enabled=true;pending=nil
 emit({kind='metadata',schema=1,game='SCUS-94423',phases='PlayerUpdate entry, ground-query return, PlayerUpdate return, CollisionUpdate return'})
 print('Player frame capture running. Mark each movement, then call aeplayer_trace_stop().')
end
function aeplayer_trace_mark(text) emit({kind='mark',label=tostring(text),frame=u32(0x800ec350)}) end
function aeplayer_trace_stop()
 enabled=false;pending=nil;if log then log:close();log=nil end
 print('Player capture stopped. The ignored JSONL is ready for local comparison.')
end
aeplayerFrameBreakpoints={}
local function hook(pc,label,fn)
 aeplayerFrameBreakpoints[#aeplayerFrameBreakpoints+1]=PCSX.addBreakpoint(pc,'Exec',4,label,function()
  if enabled then fn() end
 end)
end
hook(0x80060780,'player capture entry',function()
 local level=u16(0x800f4474)
 pending={kind='frame',frame=u32(0x800ec350),area=u16(0x800f4476),mode=u16(0x800f44a0),pre=player(),
  raw_left={u8(0x800e52d0+6),u8(0x800e52d0+7)},
  input={u32(0x800e5478),u32(0x800e5480),u16(0x800e5498),u16(0x800e54a0),
   s16(0x800e54b4),s16(0x800e54b6),s16(0x800e54b8),s16(0x800e54ba),
   s16(0x800e54d0),s16(0x800e54d2),u16(0x800e549a),
   u16(0x800f44a2)==0xb and 1 or 0,u32(0x800ec350),
   (level==0x1f or level==0x20) and 0 or 1,s16(0x800f4dce),s16(0x800f5126)}}
end)
hook(0x80060d08,'player capture ground return',function()
 if not pending then return end
 local v=bit.band(tonumber(PCSX.getRegisters().GPR.r[2]),0xffff);if v>=0x8000 then v=v-0x10000 end
 pending.ground={v,s16(0x800ec1e0+0x104),s16(0x800ec1e0+0x106),sc16(0x1f8003f8),0,0}
end)
hook(0x8005c134,'player capture update return',function()
 if not pending then return end
 pending.post_update=player()
 if pending.ground then
  pending.ground[5]=u16(0x800ec1e0+0x138);pending.ground[6]=s16(0x800ec1e0+0x13c)
 end
end)
hook(0x8005c274,'player capture collision return',function()
 if not pending then return end
 pending.post_collision=player()
 if pending.post_update and pending.ground then emit(pending)
 else emit({kind='incomplete',frame=pending.frame,reason='Missing update/ground phase; excluded from comparison'}) end
 pending=nil
end)
