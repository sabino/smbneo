-- Same normal-clock MVS machine and moving input cadence as the VS runner.
-- Home and VS level rules differ, so this is a backend/cadence comparison,
-- not an assertion that the two games must have identical frame state.
local frame, previous = 0, 0
local captured = {}
local function step()
  frame = frame + 1
  local space = manager.machine.devices[":maincpu"].spaces["program"]
  local game = space:read_u32(assert(tonumber(os.getenv("HOME_GAME_ADDRESS"))))
  local ram = assert(tonumber(os.getenv("HOME_RAM_ADDRESS")))
  assert(game >= previous, "home game counter reset")
  previous = game
  local ports = manager.machine.ioport.ports
  local start = ports[":edge:joy:START"].fields["1 Player Start"]
  if game >= 240 and game < 244 then start:set_value(1) else start:clear_value() end
  if game >= 600 then
    local pad = ports[":edge:joy:JOY1"].fields
    pad["P1 Right"]:set_value(1); pad["P1 C"]:set_value(1)
    if game % 60 < 25 then pad["P1 A"]:set_value(1) else pad["P1 A"]:clear_value() end
  end
  if (game == 600 or game == 900 or game == 1200) and not captured[game] then
    captured[game] = true
    local mode = space:read_u8(ram + 0x770)
    print(string.format("HOME display=%d game=%d mode=%d", frame, game, mode))
    assert(mode == 1, "regular SMB did not enter gameplay")
    manager.machine.screens[":screen"]:snapshot(string.format("home-tick-%04d.png", game))
  end
  if game >= 1200 then print("HOME validation passed"); manager.machine:exit() end
end
emu.register_frame_done(function()
  local ok, err = xpcall(step, debug.traceback)
  if not ok then print("HOME validation failed: " .. err); manager.machine:exit() end
end, "home_core_comparison")
