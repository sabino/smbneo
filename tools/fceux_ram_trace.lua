-- FCEUX 2.2.1 reference-side state transcript.
--
-- Required environment:
--   SMB_TRACE_OUTPUT  destination CSV path
--   SMB_TRACE_FRAMES  positive number of FM2 input frames to execute
--
-- The FM2 must already be playing when this script is loaded. Row 0 is
-- sampled after the first emu.frameadvance(): FCEUX has applied FM2 record 0,
-- including any frame-zero command and that record's controller input.

local U32 = 4294967296
local FNV_OFFSET = 2166136261
local FNV_LOW = 403 -- 0x01000193 == 2^24 + 0x193

local output_path = os.getenv("SMB_TRACE_OUTPUT")
local frame_limit = tonumber(os.getenv("SMB_TRACE_FRAMES") or "")
local emulator_label = os.getenv("SMB_TRACE_EMULATOR_LABEL")
local emulator_sha256 = os.getenv("SMB_TRACE_EMULATOR_SHA256")

if output_path == nil or output_path == "" then
    error("SMB_TRACE_OUTPUT is required")
end
if frame_limit == nil or frame_limit < 0 or frame_limit % 1 ~= 0 then
    error("SMB_TRACE_FRAMES must be a non-negative integer")
end
if (
    emulator_label == nil or emulator_label == "" or
    string.find(emulator_label, "[\r\n]") ~= nil
) then
    error("SMB_TRACE_EMULATOR_LABEL is required and must be one line")
end
if (
    emulator_sha256 == nil or #emulator_sha256 ~= 64 or
    string.find(emulator_sha256, "[^0-9a-fA-F]") ~= nil
) then
    error("SMB_TRACE_EMULATOR_SHA256 must be a 64-digit hex digest")
end

if emu ~= nil and emu.speedmode ~= nil then
    emu.speedmode("maximum")
elseif FCEU ~= nil and FCEU.speedmode ~= nil then
    FCEU.speedmode("maximum")
else
    error("this FCEUX build does not expose a maximum-speed Lua mode")
end

local output, open_error = io.open(output_path, "wx")
if output == nil then
    error(
        "cannot exclusively create trace output: " ..
        tostring(open_error)
    )
end

local function unsigned32(value)
    value = value % U32
    if value < 0 then
        value = value + U32
    end
    return value
end

-- Multiplication is split so every intermediate remains exactly representable
-- by Lua 5.1's double-precision number type.
local function fnv_byte(hash, value)
    local mixed = bit.bxor(hash, value)
    mixed = unsigned32(mixed)
    local high = unsigned32(bit.lshift(mixed, 24))
    return unsigned32(mixed * FNV_LOW + high)
end

local function fnv_range(hash, first_address, end_address)
    for address = first_address, end_address - 1 do
        hash = fnv_byte(hash, memory.readbyte(address))
    end
    return hash
end

local function hash_range(first_address, end_address)
    return fnv_range(FNV_OFFSET, first_address, end_address)
end

local function semantic_hash()
    local hash = fnv_range(FNV_OFFSET, 0x000, 0x100)
    return fnv_range(hash, 0x200, 0x800)
end

local function controller_state()
    local buttons = joypad.get(1)
    local value = 0

    if buttons.A then value = value + 0x01 end
    if buttons.B then value = value + 0x02 end
    if buttons.select then value = value + 0x04 end
    if buttons.start then value = value + 0x08 end
    if buttons.up then value = value + 0x10 end
    if buttons.down then value = value + 0x20 end
    if buttons.left then value = value + 0x40 end
    if buttons.right then value = value + 0x80 end
    return value
end

output:write("# schema=smb-core-state-trace-v1\n")
output:write("# source=" .. emulator_label .. "\n")
output:write("# emulator_sha256=" .. string.lower(emulator_sha256) .. "\n")
output:write("# frame_semantics=post_input_nmi\n")
output:write("# frame_zero_semantics=fm2_command_and_input_applied\n")
output:write("# semantic_ram=$0000-$00ff,$0200-$07ff\n")
output:write("# hash=fnv1a32\n")
output:write("# lagged_semantics=fceux_emu_lagged\n")
output:write(string.format("# frames=%d\n", frame_limit))
output:write(
    "frame,input,semantic_hash,full_ram_hash,zero_page_hash," ..
    "stack_hash,oam_hash,work_hash,oper_mode,oper_mode_task," ..
    "world,level,engine_subroutine,player_state,player_page," ..
    "player_x,player_y,screen_page,screen_x,world_end_timer," ..
    "lagged,lag_count\n"
)

for frame = 0, frame_limit - 1 do
    emu.frameadvance()
    output:write(string.format(
        "%d,%d,%08x,%08x,%08x,%08x,%08x,%08x," ..
        "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
        frame,
        controller_state(),
        semantic_hash(),
        hash_range(0x000, 0x800),
        hash_range(0x000, 0x100),
        hash_range(0x100, 0x200),
        hash_range(0x200, 0x300),
        hash_range(0x300, 0x800),
        memory.readbyte(0x770),
        memory.readbyte(0x772),
        memory.readbyte(0x75f),
        memory.readbyte(0x75c),
        memory.readbyte(0x00e),
        memory.readbyte(0x01d),
        memory.readbyte(0x06d),
        memory.readbyte(0x086),
        memory.readbyte(0x0ce),
        memory.readbyte(0x71a),
        memory.readbyte(0x71c),
        memory.readbyte(0x7a1),
        emu.lagged() and 1 or 0,
        emu.lagcount()
    ))
    if frame % 120 == 119 then
        output:flush()
    end
end

output:write("# complete=1\n")
output:flush()
output:close()
emu.pause()
