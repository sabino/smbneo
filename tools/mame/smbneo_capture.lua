local frame = 0
local capture_frames = {
  [60] = true,
  [120] = true,
  [300] = true,
  [360] = true,
  [420] = true,
  [480] = true,
  [600] = true,
  [720] = true,
  [900] = true,
  [1080] = true,
}

local function field(port, name)
  return manager.machine.ioport.ports[port].fields[name]
end

emu.register_frame_done(
  function()
    frame = frame + 1

    local start = field(":edge:joy:START", "1 Player Start")
    if frame == 500 then
      start:set_value(1)
    elseif frame == 506 then
      start:clear_value()
    end

    local right = field(":edge:joy:JOY1", "P1 Right")
    if frame == 650 then
      right:set_value(1)
    elseif frame == 1050 then
      right:clear_value()
    end

    local jump = field(":edge:joy:JOY1", "P1 A")
    if frame == 730 or frame == 900 then
      jump:set_value(1)
    elseif frame == 760 or frame == 930 then
      jump:clear_value()
    end

    if capture_frames[frame] then
      manager.machine.screens[":screen"]:snapshot(
        string.format("frame-%04d.png", frame)
      )
    end

    if frame >= 1080 then
      manager.machine:exit()
    end
  end,
  "smbneo_hardware_capture"
)
