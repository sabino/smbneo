-- Local owned-ROM integration: screenshots and inputs, no ROM bytes in Git.
local frame = 0
local captured = {}
local previous_tick = 0
local real_coin = os.getenv("VS_REAL_COIN") == "1"
local coin_field
local end_tick = tonumber(os.getenv("VS_END_FRAME") or "600")
local function field(port, name)
  local input_port = manager.machine.ioport.ports[port]
  if input_port and input_port.fields[name] then return input_port.fields[name] end
  -- AES controller slots and the MVS edge connector have different paths.
  local match
  local alias = name == "1 Player Start" and "P1 Start" or name
  for _, candidate in pairs(manager.machine.ioport.ports) do
    for label, input in pairs(candidate.fields) do
      if label == name or label == alias then assert(not match, "ambiguous input: " .. name); match = input end
    end
  end
  if not match then
    for path, candidate in pairs(manager.machine.ioport.ports) do
      for label, _ in pairs(candidate.fields) do print("VS input: " .. path .. " / " .. label) end
    end
    manager.machine:exit()
    error("missing input: " .. name)
  end
  return match
end
local function step()
  frame = frame + 1
  local frameaddr = tonumber(os.getenv("VS_DEBUG_FRAMES") or "0")
  local stageaddr = tonumber(os.getenv("VS_DEBUG_STAGE") or "0")
  local stopframe = tonumber(os.getenv("VS_STOP_GAMEFRAME") or "0")
  if stopframe ~= 0 then
    local space = manager.machine.devices[":maincpu"].spaces["program"]
    if space:read_u32(frameaddr) == stopframe and space:read_u16(stageaddr) == 4 then
      manager.machine.screens[":screen"]:snapshot("vs-neo-gameframe.png")
      local address = tonumber(os.getenv("VS_MACHINE_ADDRESS") or "0")
      local path = os.getenv("VS_DUMP_PATH")
      if address ~= 0 and path then
        local f = assert(io.open(path, "wb"))
        for i = 0, 4095 do f:write(string.char(space:read_u8(address + i))) end
        f:close()
      end
      print("VS validation passed: reference frame")
      manager.machine:exit()
    end
    return
  end
  local start = field(":edge:joy:START", "1 Player Start")
  local run = field(":edge:joy:JOY1", "P1 C")
  local tick = frameaddr ~= 0 and manager.machine.devices[":maincpu"].spaces["program"]:read_u32(frameaddr) or frame
  assert(tick >= previous_tick, "VS game frame counter reset during coin/start")
  previous_tick = tick
  if real_coin and not coin_field then
    for _, port in pairs(manager.machine.ioport.ports) do
      for name, input in pairs(port.fields) do
        if name == "Coin 1" then coin_field = input end
      end
    end
    assert(coin_field, "MVS Coin 1 input not found")
  end
  -- Start+run is the cartridge's AES-friendly insert-coin shortcut.
  if tick >= 240 and tick < 244 then
    if real_coin then coin_field:set_value(1)
    else start:set_value(1); run:set_value(1) end
  elseif tick >= 360 and tick < 364 then start:set_value(1); run:clear_value()
  else start:clear_value(); run:clear_value() end
  if real_coin and not (tick >= 240 and tick < 244) then coin_field:clear_value() end
  if end_tick > 600 and tick >= 600 then
    field(":edge:joy:JOY1", "P1 Right"):set_value(1)
    run:set_value(1)
    local jump = field(":edge:joy:JOY1", "P1 A")
    if tick % 60 < 25 then jump:set_value(1) else jump:clear_value() end
  end
  if (tick == 180 or tick == 300 or tick == 600 or tick == 900 or tick == end_tick) and not captured[tick] then
    captured[tick] = true
    local cpu = manager.machine.devices[":maincpu"]
    local address = tonumber(os.getenv("VS_MACHINE_ADDRESS") or "0")
    assert(address ~= 0, "missing machine address")
    if tick == 300 then
      local credit = cpu.spaces["program"]:read_u8(address + 2048 + 0x610)
      print(string.format("VS credits=%d statusA=%02x statusB=%02x bios_mvs=%d", credit,
        cpu.spaces["program"]:read_u8(0x320001), cpu.spaces["program"]:read_u8(0x380000),
        cpu.spaces["program"]:read_u8(0x10fd82)))
      assert(credit == 1, "coin did not credit VS")
    end
    if tick == 600 then
      assert(cpu.spaces["program"]:read_u8(address + 2048 + 0x610) == 0, "start did not consume credit")
      local mode = cpu.spaces["program"]:read_u8(address + 0x770)
      if mode ~= 2 then manager.machine.screens[":screen"]:snapshot("vs-failure-mode.png") end
      assert(mode == 2, "VS did not enter gameplay, mode=" .. mode)
    end
    for _, name in ipairs({"stage", "pc", "fault", "ctrl", "frames", "steps", "logic_vblanks", "render_vblanks"}) do
      local address = tonumber(os.getenv("VS_DEBUG_" .. string.upper(name)) or "0")
      if address ~= 0 then
        local value = (name == "frames" or name == "steps") and cpu.spaces["program"]:read_u32(address)
                      or cpu.spaces["program"]:read_u16(address)
        print(string.format("VS frame=%d %s=%x", frame, name, value))
      end
    end
    manager.machine.screens[":screen"]:snapshot(string.format("vs-neo-tick-%04d.png", tick))
  end
  if tick >= end_tick then print("VS validation passed: coin/start/gameplay"); manager.machine:exit() end
end
emu.register_frame_done(function()
  local ok, err = xpcall(step, debug.traceback)
  if not ok then print("VS validation failed: " .. err); manager.machine:exit() end
end, "vs_neo_local_validation")
