-- Load the game-mode, monkey AI and player state loggers in one PCSX-Redux session.
-- Each writes its own log in notes/runtime/. mark('text') drops a marker into the
-- monkey and player logs at once.
local ROOT = '<repo>/tools/pcsx/'
dofile(ROOT .. 'gamemode_logger.lua')
dofile(ROOT .. 'monkey_ai_logger.lua')
local monkeyMark = mark
dofile(ROOT .. 'player_state_logger.lua')

function mark(text)
    monkeyMark(text)
    pmark(text)
end
