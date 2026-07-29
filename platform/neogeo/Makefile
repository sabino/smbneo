ROOT := ../..
BUILD := build
ROMDIR := $(BUILD)/rom
ASSETDIR := $(BUILD)/assets
TITLE_SOURCE := $(ASSETDIR)/smbneogeo-title.c

TARGET := $(BUILD)/smbneogeo.elf
MAP := $(BUILD)/smbneogeo.map
CART_TARGET := $(BUILD)/smbneogeo-cart.elf
CART_MAP := $(BUILD)/smbneogeo-cart.map
WEB_BUILD := $(BUILD)/web
WEB_TARGET := $(WEB_BUILD)/smbneogeo-web.elf
WEB_MAP := $(WEB_BUILD)/smbneogeo-web.map
WEB_VIDEO_OBJECT := $(WEB_BUILD)/video.o
WEB_NATIVE_PROM := $(WEB_BUILD)/smbneo-p1.p1
WEB_PROM := $(WEB_BUILD)/smbneo-web-p1.p1
WEB_MROM := $(WEB_BUILD)/smbneo-m1.m1
WEB_VROM := $(WEB_BUILD)/smbneo-v1.v1
WEB_SITE ?= $(ROOT)/dist/pages
PPU_STATE_TEST := $(BUILD)/test_neogeo_ppu_render_state
HUD_UPDATE_TEST := $(BUILD)/test_neogeo_hud_updates
BACKGROUND_CHAIN_TEST := $(BUILD)/test_neogeo_background_chain
REPLAY_GATE_TEST := $(BUILD)/test_neogeo_replay_gate
REPLAY_TIMING_TEST := $(BUILD)/test_neogeo_replay_timing
REPLAY_CHECKPOINT_TEST := $(BUILD)/test_neogeo_replay_checkpoint
JUMP_ENGINE_TEST := $(BUILD)/test_jump_engine_dispatch
COLLISION_CORE_TEST := $(BUILD)/test_collision_core
BLOCK_BUFFER_COLLISION_TEST := $(BUILD)/test_block_buffer_collision
BOUNDING_BOX_CORE_TEST := $(BUILD)/test_bounding_box_core
ENEMY_BOUNDING_BOX_TEST := $(BUILD)/test_enemy_bounding_box
MOVE_OBJECT_HORIZ_TEST := $(BUILD)/test_move_object_horizontally
RELATIVE_ENEMY_POSITION_TEST := $(BUILD)/test_relative_enemy_position
DRAW_SPRITE_OBJECT_TEST := $(BUILD)/test_draw_sprite_object
MOVE_SPRITES_OFFSCREEN_TEST := $(BUILD)/test_move_sprites_offscreen
SPRITE_SHUFFLER_TEST := $(BUILD)/test_sprite_shuffler
CORE_FAST_PATHS_TEST := $(BUILD)/test_neogeo_core_fast_paths
DRAW_LARGE_PLATFORM_FAST_TEST := $(BUILD)/test_draw_large_platform_fast
SPR_OBJECT_OFFSCREEN_FAST_TEST := $(BUILD)/test_spr_object_offscreen_fast
ENEMY_BG_COLLISION_FAST_TEST := $(BUILD)/test_enemy_bg_collision_fast
ENEMY_COLLISION_FAST_TEST := $(BUILD)/test_enemy_collision_fast
RENDER_AREA_GRAPHICS_FAST_TEST := $(BUILD)/test_render_area_graphics_fast
READ_PORT_BITS_TEST := $(BUILD)/test_read_port_bits
OFFSCREEN_BITS_TEST := $(BUILD)/test_offscreen_bits
TOP_SCORE_CHECK_TEST := $(BUILD)/test_top_score_check
ROM_FALLTHROUGH_TEST := $(BUILD)/test_rom_fallthrough_data
INPUT_POLICY_TEST := $(BUILD)/test_neogeo_input_policy
APU_BRIDGE_TEST := $(BUILD)/test_neogeo_apu_bridge
APU_FRAME_UNITS_TEST := $(BUILD)/test_neogeo_apu_frame_units
AUDIO_PACKET_TEST := $(BUILD)/test_neogeo_audio_packet
AUDIO_CADENCE_TEST := $(BUILD)/test_neogeo_audio_cadence
OAM_VISIBILITY_TEST := $(BUILD)/test_neogeo_oam_visibility
OAM_TILES_TEST := $(BUILD)/test_neogeo_oam_tiles
PALETTE_POLICY_TEST := $(BUILD)/test_neogeo_palette_policy
VBLANK_BUDGET_TEST := $(BUILD)/test_neogeo_vblank_budget
VRAM_BATCH_TEST := $(BUILD)/test_neogeo_vram_batch
VRAM_BATCH_SANITIZE_TEST := $(BUILD)/test_neogeo_vram_batch_sanitize
GAMEROM := smbneo
SMBNEO_NGH := 2026

CC := m68k-neogeo-elf-gcc
HOST_CC ?= cc
OBJCOPY := m68k-neogeo-elf-objcopy
NM := m68k-neogeo-elf-nm
Z80_AS := z80-neogeo-ihx-sdasz80
Z80_LD := z80-neogeo-ihx-sdldz80
PYTHON := python3
MOON ?= moon
PKG_CONFIG := pkg-config
ROMTOOL := romtool.py
Z80_OBJCOPY := z80-neogeo-ihx-sdobjcopy

NG_CFLAGS := $(shell $(PKG_CONFIG) --cflags ngdevkit)
NG_LIBS := $(shell $(PKG_CONFIG) --libs ngdevkit)
NG_SHARE := $(shell $(PKG_CONFIG) --variable=sharedir ngdevkit)
NG_Z80_INCLUDE := $(shell $(PKG_CONFIG) --variable=z80includedir ngdevkit)
NG_Z80_LIB := $(shell $(PKG_CONFIG) --variable=z80libdir ngdevkit)

CPPFLAGS := \
	$(NG_CFLAGS) -I. -I$(ROOT)/codegen/lib \
	-DSMB_NEOGEO_FAST_CORE
ARCH_FLAGS := -m68000 -mlra
OPT_FLAGS ?= -O3
TUNE_FLAGS := \
	-fira-loop-pressure -frename-registers -fweb -fipa-pta
LTO_FLAGS ?= -flto -fwhole-program
WARN_FLAGS := \
	-Wall -Wextra \
	-Wno-unused-function -Wno-unused-label -Wno-unused-parameter \
	-Wno-unused-value -Wno-unused-variable
CFLAGS := \
	$(ARCH_FLAGS) $(OPT_FLAGS) $(TUNE_FLAGS) $(LTO_FLAGS) \
	-std=gnu11 -fgnu89-inline -fomit-frame-pointer \
	-ffunction-sections -fdata-sections \
	-fno-asynchronous-unwind-tables -fno-unwind-tables \
	-MMD -MP $(WARN_FLAGS)
LDFLAGS := \
	$(ARCH_FLAGS) $(OPT_FLAGS) $(TUNE_FLAGS) $(LTO_FLAGS) \
	-Wl,--defsym=rom_eye_catcher_mode=2 \
	-Wl,--defsym=rom_NGH_ID=0x$(SMBNEO_NGH)

ENGINE_SOURCES := \
	video.c ppu_direct.c apu_bridge.c apu_neogeo.c audio_cadence.c \
	core_fast_paths.c
CORE_NAMES := instructions code data cpu
ENGINE_OBJECTS := \
	$(ENGINE_SOURCES:%.c=$(BUILD)/%.o) \
	$(CORE_NAMES:%=$(BUILD)/%.o)
BASE_OBJECTS := $(BUILD)/main.o $(ENGINE_OBJECTS)
ELF_OBJECTS := $(BASE_OBJECTS) $(BUILD)/title_data_stub.o
CART_OBJECTS := $(BASE_OBJECTS) $(BUILD)/smbneogeo-title.o
WEB_OBJECTS := \
	$(BUILD)/main.o $(WEB_VIDEO_OBJECT) \
	$(filter-out $(BUILD)/video.o,$(ENGINE_OBJECTS)) \
	$(BUILD)/title_data_stub.o
DEPENDENCIES = \
	$(BASE_OBJECTS:.o=.d) \
	$(WEB_VIDEO_OBJECT:.o=.d) \
	$(BUILD)/title_data_stub.d \
	$(BUILD)/smbneogeo-title.d \
	$(REPLAY_MAIN_OBJECT:.o=.d) \
	$(REPLAY_GATE_OBJECT:.o=.d) \
	$(if $(filter 1,$(REPLAY_WINDOW_BENCH)),$(REPLAY_VIDEO_OBJECT:.o=.d),)

PROM := $(ROMDIR)/$(GAMEROM)-p1.p1
CROM1 := $(ROMDIR)/$(GAMEROM)-c1.c1
CROM2 := $(ROMDIR)/$(GAMEROM)-c2.c2
SROM1 := $(ROMDIR)/$(GAMEROM)-s1.s1
MROM1 := $(ROMDIR)/$(GAMEROM)-m1.m1
VROM1 := $(ROMDIR)/$(GAMEROM)-v1.v1
TRIANGLE_VROM := $(BUILD)/$(GAMEROM)-triangle-v1.v1
COMPAT_CART := $(ROMDIR)/puzzledp.zip
HARDWARE_CART := $(ROMDIR)/$(GAMEROM).zip
NEOSD_CART := $(ROMDIR)/$(GAMEROM).neo
GNGEO_DATA := $(ROMDIR)/gngeo_data.zip

SMB_ROM ?=
REPLAY_FM2 ?=
REPLAY_DATA_HEADER ?=
REPLAY_FAST ?= 0
REPLAY_HARDWARE_PLAYABLE ?= 0
REPLAY_TAIL_FRAMES ?= 1800
REPLAY_BOOTSTRAP_FRAMES ?= 7
REPLAY_AREA_INIT_HOLD_FRAMES ?= 1
REPLAY_STAGE_SETTLE_FRAMES ?= 2
REPLAY_WINDOW_BENCH ?= 0
REPLAY_WINDOWS ?= 3435:3554,25691:25810
REPLAY_WINDOW_WARMUP_FRAMES ?= 240
REPLAY_EVIDENCE_TIMEOUT ?= 7200
REPLAY_EVIDENCE_OVERCLOCK ?= 0
REPLAY_EVIDENCE_DIR ?=
GNGEO ?= ngdevkit-gngeo
GNGEO_PREFIX ?= $(shell \
	executable=$$(command -v "$(GNGEO)" 2>/dev/null); \
	if test -n "$$executable"; then \
		dirname "$$(dirname "$$(readlink -f "$$executable")")"; \
	fi)
GNGEO_SUPPORT_DATA ?= \
	$(GNGEO_PREFIX)/share/ngdevkit-gngeo/gngeo_data.zip
GNGEO_P1 ?= A=K97,B=K115,C=K113,D=K119,START=K49,UP=K82,DOWN=K81,LEFT=K80,RIGHT=K79
GNGEO_P2 ?= START=K50
GNGEO_REALTIME_FLAGS ?= \
	--autoframeskip --no-vsync --sleepidle \
	--68kclock=0 --z80clock=0
MAME ?= mame
MAME_SYSTEM ?= ng_mv1
MAME_BIOS ?= euro
MAME_BIOS_DIR ?=
MAME_BUILD := $(BUILD)/mame
MAME_HASHDIR := $(MAME_BUILD)/hash
MAME_SOFTWARE_LIST := $(MAME_HASHDIR)/neogeo.xml
MAME_CAPTURE_DIR := $(MAME_BUILD)/captures
MAME_CFG_DIR := $(MAME_BUILD)/cfg
MAME_NVRAM_DIR := $(MAME_BUILD)/nvram
MAME_ROMPATH := \
	$(if $(strip $(MAME_BIOS_DIR)),$(MAME_BIOS_DIR);)$(ROMDIR)
MAME_RUN_FLAGS ?= \
	-skip_gameinfo -window -nomaximize -nofilter -keepaspect
CARTRIDGE_TIMESTAMP ?= 202601010000.00

REPLAY_VARIANT := $(if $(filter 1,$(REPLAY_FAST)),fast,rendered)
REPLAYDIR := $(BUILD)/replay-$(REPLAY_VARIANT)
REPLAY_ROMDIR := $(REPLAYDIR)/rom
REPLAY_HEADER := $(REPLAYDIR)/smb_replay_data.h
REPLAY_WINDOW_HEADER := $(REPLAYDIR)/smb_neogeo_replay_windows.h
REPLAY_MAIN_OBJECT := $(REPLAYDIR)/replay_main.o
REPLAY_GATE_OBJECT := $(REPLAYDIR)/replay_gate.o
REPLAY_VIDEO_OBJECT := $(if $(filter 1,$(REPLAY_WINDOW_BENCH)),$(REPLAYDIR)/video.o,$(BUILD)/video.o)
REPLAY_TARGET := $(REPLAYDIR)/smbneogeo-replay.elf
REPLAY_MAP := $(REPLAYDIR)/smbneogeo-replay.map
REPLAY_ENGINE_OBJECTS := \
	$(patsubst $(BUILD)/video.o,$(REPLAY_VIDEO_OBJECT),$(ENGINE_OBJECTS))
REPLAY_OBJECTS := \
	$(REPLAY_MAIN_OBJECT) $(REPLAY_GATE_OBJECT) \
	$(REPLAY_ENGINE_OBJECTS) $(BUILD)/smbneogeo-title.o
REPLAY_PROM := $(REPLAY_ROMDIR)/$(GAMEROM)-p1.p1
REPLAY_CROM1 := $(REPLAY_ROMDIR)/$(GAMEROM)-c1.c1
REPLAY_CROM2 := $(REPLAY_ROMDIR)/$(GAMEROM)-c2.c2
REPLAY_SROM1 := $(REPLAY_ROMDIR)/$(GAMEROM)-s1.s1
REPLAY_MROM1 := $(REPLAY_ROMDIR)/$(GAMEROM)-m1.m1
REPLAY_VROM1 := $(REPLAY_ROMDIR)/$(GAMEROM)-v1.v1
REPLAY_CART := $(REPLAY_ROMDIR)/$(GAMEROM).zip
REPLAY_GNGEO_DATA := $(REPLAY_ROMDIR)/gngeo_data.zip
SOUND_DRIVER_OBJECT := $(BUILD)/sound_driver.rel
SOUND_DRIVER := $(BUILD)/smbneogeo-sound.ihx
SOUND_DRIVER_MAP := $(BUILD)/smbneogeo-sound.map
SOUND_DRIVER_CHECKER := $(ROOT)/tools/check_neogeo_sound_driver.py
SOUND_DRIVER_LIBRARY := $(NG_Z80_LIB)/nullsound.lib
SOUND_DRIVER_HELPERS := $(NG_Z80_INCLUDE)/nullsound/helpers.inc
TRIANGLE_VROM_GENERATOR := $(ROOT)/tools/gen_neogeo_triangle_vrom.py
WEB_PLAYER_BUILDER := $(ROOT)/tools/build_web_player.py
PUZZLEDP_COMPAT_BUILDER := $(ROOT)/tools/puzzledp_compat.py
NEOSD_BUILDER := $(ROOT)/tools/build_neosd.py
WEB_BIOS ?= $(NG_SHARE)/neogeo.zip
MAME_SOFTWARE_GENERATOR := $(ROOT)/tools/gen_mame_neogeo_software.py
GNGEO_DRIVER_CHECKER := $(ROOT)/tools/check_gngeo_driver.py
MAME_CAPTURE_SCRIPT := $(ROOT)/tools/mame/smbneo_capture.lua
TRIANGLE_VROM_SHA256 := \
	c52017058a226a44506a5d94fc1f692b42fe818302761da64d4f7adc5e5928a7
REPLAY_POLICY_FLAG := \
	$(if $(filter 1,$(REPLAY_HARDWARE_PLAYABLE)),--hardware-playable,)
REPLAY_FAST_FLAG := \
	$(if $(filter 1,$(REPLAY_FAST)),-DSMB_NEOGEO_REPLAY_FAST,)
REPLAY_WINDOW_FLAG := \
	$(if $(filter 1,$(REPLAY_WINDOW_BENCH)),-DSMB_NEOGEO_REPLAY_WINDOW_BENCH,)
REPLAY_WINDOW_DEPENDENCY := \
	$(if $(filter 1,$(REPLAY_WINDOW_BENCH)),$(REPLAY_WINDOW_HEADER),)
REPLAY_WINDOW_TOOL := $(ROOT)/tools/neogeo_replay_window.py

.DEFAULT_GOAL := elf

.PHONY: \
	all elf test test-vram-batch-sanitize verify verify-codegen \
	verify-sound-driver assets \
	native-roms cart compat-cart hardware-cart neosd-cart run web \
	mame-list mame-cart mame-run mame-capture \
	replay-cart replay-run replay-rendered-evidence clean FORCE

all: verify

elf: $(TARGET)

$(BUILD):
	mkdir -p $@

$(ROMDIR) $(ASSETDIR) $(WEB_BUILD) $(REPLAYDIR) $(REPLAY_ROMDIR) \
$(MAME_HASHDIR) $(MAME_CAPTURE_DIR) $(MAME_CFG_DIR) $(MAME_NVRAM_DIR):
	mkdir -p $@

$(BUILD)/%.o: %.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: $(ROOT)/codegen/lib/%.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(WEB_VIDEO_OBJECT): video.c | $(WEB_BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -DSMB_NEOGEO_FBNEO -c $< -o $@

$(SOUND_DRIVER_OBJECT): sound_driver.s $(SOUND_DRIVER_HELPERS) | $(BUILD)
	$(Z80_AS) -g -l -p -u \
		-I$(NG_Z80_INCLUDE)/nullsound -I. -o $@ $<

$(SOUND_DRIVER) $(SOUND_DRIVER_MAP) &: \
		$(SOUND_DRIVER_OBJECT) $(SOUND_DRIVER_LIBRARY)
	$(Z80_LD) -b DATA=0xf800 -i -m -w $(SOUND_DRIVER) \
		$(SOUND_DRIVER_LIBRARY) $(SOUND_DRIVER_OBJECT)

verify-sound-driver: \
		$(SOUND_DRIVER) $(SOUND_DRIVER_MAP) $(SOUND_DRIVER_CHECKER)
	$(PYTHON) $(SOUND_DRIVER_CHECKER) $(SOUND_DRIVER_MAP)

$(TRIANGLE_VROM): $(TRIANGLE_VROM_GENERATOR) | $(BUILD)
	$(PYTHON) $(TRIANGLE_VROM_GENERATOR) $@
	echo "$(TRIANGLE_VROM_SHA256)  $@" | sha256sum -c -

$(BUILD)/smbneogeo-title.o: $(TITLE_SOURCE) title_data.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(REPLAY_HEADER): FORCE | $(REPLAYDIR)
	@test -z "$(REPLAY_FM2)" || test -z "$(REPLAY_DATA_HEADER)" || { \
		echo "set only one of REPLAY_FM2 or REPLAY_DATA_HEADER" >&2; \
		exit 2; \
	}
	@test -n "$(REPLAY_FM2)" || test -n "$(REPLAY_DATA_HEADER)" || { \
		echo "REPLAY_FM2 or REPLAY_DATA_HEADER is required" >&2; \
		exit 2; \
	}
	@if test -n "$(REPLAY_DATA_HEADER)"; then \
		test -f "$(REPLAY_DATA_HEADER)" || { \
			echo "replay data header is missing: $(REPLAY_DATA_HEADER)" >&2; \
			exit 2; \
		}; \
		cp -- "$(REPLAY_DATA_HEADER)" "$@"; \
	else \
		$(PYTHON) $(ROOT)/tools/rec_tool.py emit-fm2-c \
			"$(REPLAY_FM2)" \
			--symbol-prefix smb_replay \
			$(REPLAY_POLICY_FLAG) \
			--output "$@"; \
	fi

$(REPLAY_WINDOW_HEADER): FORCE $(REPLAY_WINDOW_TOOL) | $(REPLAYDIR)
	@test "$(REPLAY_WINDOW_BENCH)" = "1" || { \
		echo "window metadata requires REPLAY_WINDOW_BENCH=1" >&2; \
		exit 2; \
	}
	@test "$(REPLAY_FAST)" = "0" || { \
		echo "replay-window benchmarking requires REPLAY_FAST=0" >&2; \
		exit 2; \
	}
	$(PYTHON) $(REPLAY_WINDOW_TOOL) emit-header \
		--windows "$(REPLAY_WINDOWS)" \
		--warmup "$(REPLAY_WINDOW_WARMUP_FRAMES)" \
		--output "$@"

$(REPLAY_MAIN_OBJECT): \
		replay_main.c replay_checkpoint.h replay_gate.h replay_timing.h \
		$(REPLAY_HEADER) $(REPLAY_WINDOW_DEPENDENCY) | $(REPLAYDIR)
	$(CC) $(CPPFLAGS) -I$(REPLAYDIR) $(CFLAGS) \
		$(REPLAY_FAST_FLAG) $(REPLAY_WINDOW_FLAG) \
		-DSMB_NEOGEO_REPLAY_TAIL_FRAMES=$(REPLAY_TAIL_FRAMES) \
		-DSMB_NEOGEO_REPLAY_BOOTSTRAP_FRAMES=$(REPLAY_BOOTSTRAP_FRAMES) \
		-DSMB_NEOGEO_REPLAY_AREA_INIT_HOLD_FRAMES=$(REPLAY_AREA_INIT_HOLD_FRAMES) \
		-DSMB_NEOGEO_REPLAY_STAGE_SETTLE_FRAMES=$(REPLAY_STAGE_SETTLE_FRAMES) \
		-c $< -o $@

$(REPLAYDIR)/video.o: video.c video.h | $(REPLAYDIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(REPLAY_WINDOW_FLAG) -c $< -o $@

$(REPLAY_GATE_OBJECT): replay_gate.c replay_gate.h | $(REPLAYDIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(TARGET): $(ELF_OBJECTS)
	$(CC) -o $@ $(ELF_OBJECTS) $(LDFLAGS) \
		-Wl,-Map,$(MAP) $(NG_LIBS)

$(CART_TARGET): $(CART_OBJECTS)
	$(CC) -o $@ $(CART_OBJECTS) $(LDFLAGS) \
		-Wl,-Map,$(CART_MAP) $(NG_LIBS)

$(WEB_TARGET): $(WEB_OBJECTS)
	$(CC) -o $@ $(WEB_OBJECTS) $(LDFLAGS) \
		-Wl,-Map,$(WEB_MAP) $(NG_LIBS)

$(REPLAY_TARGET): $(REPLAY_OBJECTS)
	$(CC) -o $@ $(REPLAY_OBJECTS) $(LDFLAGS) \
		-Wl,-Map,$(REPLAY_MAP) $(NG_LIBS)

$(PPU_STATE_TEST): \
		$(ROOT)/tools/test_neogeo_ppu_render_state.c \
		ppu_direct.c ppu_render_state.h title_data_stub.c title_data.h \
		video.h | $(BUILD)
	$(HOST_CC) -std=c11 -Wall -Wextra -Werror \
		-DSMB_NEOGEO_TITLE_TEST_DATA \
		-I. -I$(ROOT)/codegen/lib \
		$(ROOT)/tools/test_neogeo_ppu_render_state.c \
		ppu_direct.c title_data_stub.c -o $@

$(HUD_UPDATE_TEST): \
		$(ROOT)/tools/test_neogeo_hud_updates.c \
		ppu_direct.c ppu_render_state.h hud_update.h vblank_budget.h \
		title_data_stub.c title_data.h video.h | $(BUILD)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror \
		-DSMB_NEOGEO_TITLE_TEST_DATA \
		-I. -I$(ROOT)/codegen/lib \
		$(ROOT)/tools/test_neogeo_hud_updates.c \
		ppu_direct.c title_data_stub.c -o $@

$(BACKGROUND_CHAIN_TEST): \
		$(ROOT)/tools/test_neogeo_background_chain.c \
		background_chain.h | $(BUILD)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror \
		-I. \
		$(ROOT)/tools/test_neogeo_background_chain.c -o $@

$(REPLAY_GATE_TEST): \
		$(ROOT)/tools/test_neogeo_replay_gate.c \
		replay_gate.c replay_gate.h | $(BUILD)
	$(HOST_CC) -std=c11 -Wall -Wextra -Werror \
		-I. \
		$(ROOT)/tools/test_neogeo_replay_gate.c \
		replay_gate.c -o $@

$(REPLAY_TIMING_TEST): \
		$(ROOT)/tools/test_neogeo_replay_timing.c replay_timing.h | $(BUILD)
	$(HOST_CC) -std=c11 -Wall -Wextra -Werror \
		-I. \
		$(ROOT)/tools/test_neogeo_replay_timing.c -o $@

$(REPLAY_CHECKPOINT_TEST): \
		$(ROOT)/tools/test_neogeo_replay_checkpoint.c \
		replay_checkpoint.h | $(BUILD)
	$(HOST_CC) -std=c11 -Wall -Wextra -Werror \
		-I. \
		$(ROOT)/tools/test_neogeo_replay_checkpoint.c -o $@

$(JUMP_ENGINE_TEST): \
		$(ROOT)/tools/test_jump_engine_dispatch.c \
		$(ROOT)/codegen/lib/code.c \
		$(ROOT)/codegen/lib/data.c \
		$(ROOT)/codegen/lib/instructions.c \
		$(ROOT)/codegen/lib/cpu.c \
		$(ROOT)/codegen/lib/ppu.c \
		$(ROOT)/codegen/lib/nes_palette_rgb.inc \
		apu_null.c | $(BUILD)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror \
		-Wno-unused-parameter -Wno-unused-variable \
		-Wno-unused-function \
		-I$(ROOT)/codegen/lib \
		$(ROOT)/tools/test_jump_engine_dispatch.c \
		$(ROOT)/codegen/lib/instructions.c \
		$(ROOT)/codegen/lib/code.c \
		$(ROOT)/codegen/lib/data.c \
		$(ROOT)/codegen/lib/cpu.c \
		$(ROOT)/codegen/lib/ppu.c \
		apu_null.c -o $@

$(COLLISION_CORE_TEST): \
		$(ROOT)/tools/test_collision_core.c \
		$(ROOT)/codegen/lib/code.c \
		$(ROOT)/codegen/lib/data.c \
		$(ROOT)/codegen/lib/instructions.c \
		$(ROOT)/codegen/lib/cpu.c \
		$(ROOT)/codegen/lib/ppu.c \
		$(ROOT)/codegen/lib/nes_palette_rgb.inc \
		apu_null.c | $(BUILD)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror \
		-Wno-unused-parameter -Wno-unused-variable \
		-Wno-unused-function \
		-I$(ROOT)/codegen/lib \
		$(ROOT)/tools/test_collision_core.c \
		$(ROOT)/codegen/lib/instructions.c \
		$(ROOT)/codegen/lib/code.c \
		$(ROOT)/codegen/lib/data.c \
		$(ROOT)/codegen/lib/cpu.c \
		$(ROOT)/codegen/lib/ppu.c \
		apu_null.c -o $@

$(BLOCK_BUFFER_COLLISION_TEST): \
		$(ROOT)/tools/test_block_buffer_collision.c \
		$(ROOT)/codegen/lib/code.c \
		$(ROOT)/codegen/lib/data.c \
		$(ROOT)/codegen/lib/instructions.c \
		$(ROOT)/codegen/lib/cpu.c \
		$(ROOT)/codegen/lib/ppu.c \
		$(ROOT)/codegen/lib/nes_palette_rgb.inc \
		apu_null.c | $(BUILD)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror \
		-Wno-unused-parameter -Wno-unused-variable \
		-Wno-unused-function \
		-I$(ROOT)/codegen/lib \
		$(ROOT)/tools/test_block_buffer_collision.c \
		$(ROOT)/codegen/lib/instructions.c \
		$(ROOT)/codegen/lib/code.c \
		$(ROOT)/codegen/lib/data.c \
		$(ROOT)/codegen/lib/cpu.c \
		$(ROOT)/codegen/lib/ppu.c \
		apu_null.c -o $@

$(BOUNDING_BOX_CORE_TEST): \
		$(ROOT)/tools/test_bounding_box_core.c \
		$(ROOT)/codegen/lib/code.c \
		$(ROOT)/codegen/lib/data.c \
		$(ROOT)/codegen/lib/instructions.c \
		$(ROOT)/codegen/lib/cpu.c \
		$(ROOT)/codegen/lib/ppu.c \
		$(ROOT)/codegen/lib/nes_palette_rgb.inc \
		apu_null.c | $(BUILD)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror \
		-Wno-unused-parameter -Wno-unused-variable \
		-Wno-unused-function \
		-I$(ROOT)/codegen/lib \
		$(ROOT)/tools/test_bounding_box_core.c \
		$(ROOT)/codegen/lib/instructions.c \
		$(ROOT)/codegen/lib/code.c \
		$(ROOT)/codegen/lib/data.c \
		$(ROOT)/codegen/lib/cpu.c \
		$(ROOT)/codegen/lib/ppu.c \
		apu_null.c -o $@

$(ENEMY_BOUNDING_BOX_TEST): \
		$(ROOT)/tools/test_enemy_bounding_box.c \
		$(ROOT)/codegen/lib/code.c \
		$(ROOT)/codegen/lib/data.c \
		$(ROOT)/codegen/lib/instructions.c \
		$(ROOT)/codegen/lib/cpu.c \
		$(ROOT)/codegen/lib/ppu.c \
		$(ROOT)/codegen/lib/nes_palette_rgb.inc \
		apu_null.c | $(BUILD)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror \
		-Wno-unused-parameter -Wno-unused-variable \
		-Wno-unused-function \
		-I$(ROOT)/codegen/lib \
		$(ROOT)/tools/test_enemy_bounding_box.c \
		$(ROOT)/codegen/lib/instructions.c \
		$(ROOT)/codegen/lib/code.c \
		$(ROOT)/codegen/lib/data.c \
		$(ROOT)/codegen/lib/cpu.c \
		$(ROOT)/codegen/lib/ppu.c \
		apu_null.c -o $@

$(MOVE_OBJECT_HORIZ_TEST): \
		$(ROOT)/tools/test_move_object_horizontally.c \
		$(ROOT)/codegen/lib/code.c \
		$(ROOT)/codegen/lib/data.c \
		$(ROOT)/codegen/lib/instructions.c \
		$(ROOT)/codegen/lib/cpu.c \
		$(ROOT)/codegen/lib/ppu.c \
		$(ROOT)/codegen/lib/nes_palette_rgb.inc \
		apu_null.c | $(BUILD)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror \
		-Wno-unused-parameter -Wno-unused-variable \
		-Wno-unused-function \
		-I$(ROOT)/codegen/lib \
		$(ROOT)/tools/test_move_object_horizontally.c \
		$(ROOT)/codegen/lib/instructions.c \
		$(ROOT)/codegen/lib/code.c \
		$(ROOT)/codegen/lib/data.c \
		$(ROOT)/codegen/lib/cpu.c \
		$(ROOT)/codegen/lib/ppu.c \
		apu_null.c -o $@

$(RELATIVE_ENEMY_POSITION_TEST): \
		$(ROOT)/tools/test_relative_enemy_position.c \
		$(ROOT)/codegen/lib/code.c \
		$(ROOT)/codegen/lib/data.c \
		$(ROOT)/codegen/lib/instructions.c \
		$(ROOT)/codegen/lib/cpu.c \
		$(ROOT)/codegen/lib/ppu.c \
		$(ROOT)/codegen/lib/nes_palette_rgb.inc \
		apu_null.c | $(BUILD)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror \
		-Wno-unused-parameter -Wno-unused-variable \
		-Wno-unused-function \
		-I$(ROOT)/codegen/lib \
		$(ROOT)/tools/test_relative_enemy_position.c \
		$(ROOT)/codegen/lib/instructions.c \
		$(ROOT)/codegen/lib/code.c \
		$(ROOT)/codegen/lib/data.c \
		$(ROOT)/codegen/lib/cpu.c \
		$(ROOT)/codegen/lib/ppu.c \
		apu_null.c -o $@

$(DRAW_SPRITE_OBJECT_TEST): \
		$(ROOT)/tools/test_draw_sprite_object.c \
		$(ROOT)/codegen/lib/code.c \
		$(ROOT)/codegen/lib/data.c \
		$(ROOT)/codegen/lib/instructions.c \
		$(ROOT)/codegen/lib/cpu.c \
		$(ROOT)/codegen/lib/ppu.c \
		$(ROOT)/codegen/lib/nes_palette_rgb.inc \
		apu_null.c | $(BUILD)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror \
		-Wno-unused-parameter -Wno-unused-variable \
		-Wno-unused-function \
		-I$(ROOT)/codegen/lib \
		$(ROOT)/tools/test_draw_sprite_object.c \
		$(ROOT)/codegen/lib/instructions.c \
		$(ROOT)/codegen/lib/code.c \
		$(ROOT)/codegen/lib/data.c \
		$(ROOT)/codegen/lib/cpu.c \
		$(ROOT)/codegen/lib/ppu.c \
		apu_null.c -o $@

$(MOVE_SPRITES_OFFSCREEN_TEST): \
		$(ROOT)/tools/test_move_sprites_offscreen.c \
		$(ROOT)/codegen/lib/code.c \
		$(ROOT)/codegen/lib/data.c \
		$(ROOT)/codegen/lib/instructions.c \
		$(ROOT)/codegen/lib/cpu.c \
		$(ROOT)/codegen/lib/ppu.c \
		$(ROOT)/codegen/lib/nes_palette_rgb.inc \
		apu_null.c | $(BUILD)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror \
		-Wno-unused-parameter -Wno-unused-variable \
		-Wno-unused-function \
		-I$(ROOT)/codegen/lib \
		$(ROOT)/tools/test_move_sprites_offscreen.c \
		$(ROOT)/codegen/lib/instructions.c \
		$(ROOT)/codegen/lib/code.c \
		$(ROOT)/codegen/lib/data.c \
		$(ROOT)/codegen/lib/cpu.c \
		$(ROOT)/codegen/lib/ppu.c \
		apu_null.c -o $@

$(SPRITE_SHUFFLER_TEST): \
		$(ROOT)/tools/test_sprite_shuffler.c \
		$(ROOT)/codegen/lib/code.c \
		$(ROOT)/codegen/lib/data.c \
		$(ROOT)/codegen/lib/instructions.c \
		$(ROOT)/codegen/lib/cpu.c \
		$(ROOT)/codegen/lib/ppu.c \
		$(ROOT)/codegen/lib/nes_palette_rgb.inc \
		apu_null.c | $(BUILD)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror \
		-Wno-unused-parameter -Wno-unused-variable \
		-Wno-unused-function \
		-I$(ROOT)/codegen/lib \
		$(ROOT)/tools/test_sprite_shuffler.c \
		$(ROOT)/codegen/lib/instructions.c \
		$(ROOT)/codegen/lib/code.c \
		$(ROOT)/codegen/lib/data.c \
		$(ROOT)/codegen/lib/cpu.c \
		$(ROOT)/codegen/lib/ppu.c \
		apu_null.c -o $@

$(CORE_FAST_PATHS_TEST): \
		$(ROOT)/tools/test_neogeo_core_fast_paths.c \
		core_fast_paths.c core_fast_paths.h \
		$(ROOT)/codegen/lib/code.c \
		$(ROOT)/codegen/lib/data.c \
		$(ROOT)/codegen/lib/instructions.c \
		$(ROOT)/codegen/lib/cpu.c \
		$(ROOT)/codegen/lib/ppu.c \
		$(ROOT)/codegen/lib/nes_palette_rgb.inc \
		apu_null.c | $(BUILD)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror \
		-Wno-unused-parameter -Wno-unused-variable \
		-Wno-unused-function \
		-I. -I$(ROOT)/codegen/lib \
		$(ROOT)/tools/test_neogeo_core_fast_paths.c \
		core_fast_paths.c \
		$(ROOT)/codegen/lib/instructions.c \
		$(ROOT)/codegen/lib/code.c \
		$(ROOT)/codegen/lib/data.c \
		$(ROOT)/codegen/lib/cpu.c \
		$(ROOT)/codegen/lib/ppu.c \
		apu_null.c -o $@

$(DRAW_LARGE_PLATFORM_FAST_TEST): \
		$(ROOT)/tools/test_draw_large_platform_fast.c \
		core_fast_paths.c core_fast_paths.h \
		$(ROOT)/codegen/lib/code.c \
		$(ROOT)/codegen/lib/data.c \
		$(ROOT)/codegen/lib/instructions.c \
		$(ROOT)/codegen/lib/cpu.c \
		$(ROOT)/codegen/lib/ppu.c \
		$(ROOT)/codegen/lib/nes_palette_rgb.inc \
		apu_null.c | $(BUILD)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror \
		-Wno-unused-parameter -Wno-unused-variable \
		-Wno-unused-function \
		-I. -I$(ROOT)/codegen/lib \
		$(ROOT)/tools/test_draw_large_platform_fast.c \
		core_fast_paths.c \
		$(ROOT)/codegen/lib/instructions.c \
		$(ROOT)/codegen/lib/code.c \
		$(ROOT)/codegen/lib/data.c \
		$(ROOT)/codegen/lib/cpu.c \
		$(ROOT)/codegen/lib/ppu.c \
		apu_null.c -o $@

$(SPR_OBJECT_OFFSCREEN_FAST_TEST): \
		$(ROOT)/tools/test_spr_object_offscreen_fast.c \
		core_fast_paths.c core_fast_paths.h \
		$(ROOT)/codegen/lib/code.c \
		$(ROOT)/codegen/lib/data.c \
		$(ROOT)/codegen/lib/instructions.c \
		$(ROOT)/codegen/lib/cpu.c \
		$(ROOT)/codegen/lib/ppu.c \
		$(ROOT)/codegen/lib/nes_palette_rgb.inc \
		apu_null.c | $(BUILD)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror \
		-Wno-unused-parameter -Wno-unused-variable \
		-Wno-unused-function \
		-I. -I$(ROOT)/codegen/lib \
		$(ROOT)/tools/test_spr_object_offscreen_fast.c \
		core_fast_paths.c \
		$(ROOT)/codegen/lib/instructions.c \
		$(ROOT)/codegen/lib/code.c \
		$(ROOT)/codegen/lib/data.c \
		$(ROOT)/codegen/lib/cpu.c \
		$(ROOT)/codegen/lib/ppu.c \
		apu_null.c -o $@

$(ENEMY_BG_COLLISION_FAST_TEST): \
		$(ROOT)/tools/test_enemy_bg_collision_fast.c \
		core_fast_paths.c core_fast_paths.h \
		$(ROOT)/codegen/lib/code.c \
		$(ROOT)/codegen/lib/data.c \
		$(ROOT)/codegen/lib/instructions.c \
		$(ROOT)/codegen/lib/cpu.c \
		$(ROOT)/codegen/lib/ppu.c \
		$(ROOT)/codegen/lib/nes_palette_rgb.inc \
		apu_null.c | $(BUILD)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror \
		-Wno-unused-parameter -Wno-unused-variable \
		-Wno-unused-function \
		-I. -I$(ROOT)/codegen/lib \
		$(ROOT)/tools/test_enemy_bg_collision_fast.c \
		core_fast_paths.c \
		$(ROOT)/codegen/lib/instructions.c \
		$(ROOT)/codegen/lib/code.c \
		$(ROOT)/codegen/lib/data.c \
		$(ROOT)/codegen/lib/cpu.c \
		$(ROOT)/codegen/lib/ppu.c \
		apu_null.c -o $@

$(ENEMY_COLLISION_FAST_TEST): \
		$(ROOT)/tools/test_enemy_collision_fast.c \
		$(ROOT)/tools/enemy_collision_captured_states.h \
		core_fast_paths.c core_fast_paths.h \
		$(ROOT)/codegen/lib/code.c \
		$(ROOT)/codegen/lib/data.c \
		$(ROOT)/codegen/lib/instructions.c \
		$(ROOT)/codegen/lib/cpu.c \
		$(ROOT)/codegen/lib/ppu.c \
		$(ROOT)/codegen/lib/nes_palette_rgb.inc \
		apu_null.c | $(BUILD)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror \
		-Wno-unused-parameter -Wno-unused-variable \
		-Wno-unused-function \
		-I. -I$(ROOT)/codegen/lib -I$(ROOT)/tools \
		$(ROOT)/tools/test_enemy_collision_fast.c \
		core_fast_paths.c \
		$(ROOT)/codegen/lib/instructions.c \
		$(ROOT)/codegen/lib/code.c \
		$(ROOT)/codegen/lib/data.c \
		$(ROOT)/codegen/lib/cpu.c \
		$(ROOT)/codegen/lib/ppu.c \
		apu_null.c -o $@

$(RENDER_AREA_GRAPHICS_FAST_TEST): \
		$(ROOT)/tools/test_render_area_graphics_fast.c \
		core_fast_paths.c core_fast_paths.h \
		$(ROOT)/codegen/lib/code.c \
		$(ROOT)/codegen/lib/data.c \
		$(ROOT)/codegen/lib/instructions.c \
		$(ROOT)/codegen/lib/cpu.c \
		$(ROOT)/codegen/lib/ppu.c \
		$(ROOT)/codegen/lib/nes_palette_rgb.inc \
		apu_null.c | $(BUILD)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror \
		-Wno-unused-parameter -Wno-unused-variable \
		-Wno-unused-function \
		-I. -I$(ROOT)/codegen/lib \
		$(ROOT)/tools/test_render_area_graphics_fast.c \
		core_fast_paths.c \
		$(ROOT)/codegen/lib/instructions.c \
		$(ROOT)/codegen/lib/code.c \
		$(ROOT)/codegen/lib/data.c \
		$(ROOT)/codegen/lib/cpu.c \
		$(ROOT)/codegen/lib/ppu.c \
		apu_null.c -o $@

$(READ_PORT_BITS_TEST): \
		$(ROOT)/tools/test_read_port_bits.c \
		$(ROOT)/codegen/lib/code.c \
		$(ROOT)/codegen/lib/data.c \
		$(ROOT)/codegen/lib/instructions.c \
		$(ROOT)/codegen/lib/cpu.c \
		$(ROOT)/codegen/lib/ppu.c \
		$(ROOT)/codegen/lib/nes_palette_rgb.inc \
		apu_null.c | $(BUILD)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror \
		-Wno-unused-parameter -Wno-unused-variable \
		-Wno-unused-function \
		-I$(ROOT)/codegen/lib \
		$(ROOT)/tools/test_read_port_bits.c \
		$(ROOT)/codegen/lib/instructions.c \
		$(ROOT)/codegen/lib/code.c \
		$(ROOT)/codegen/lib/data.c \
		$(ROOT)/codegen/lib/cpu.c \
		$(ROOT)/codegen/lib/ppu.c \
		apu_null.c -o $@

$(OFFSCREEN_BITS_TEST): \
		$(ROOT)/tools/test_offscreen_bits.c \
		$(ROOT)/codegen/lib/code.c \
		$(ROOT)/codegen/lib/data.c \
		$(ROOT)/codegen/lib/instructions.c \
		$(ROOT)/codegen/lib/cpu.c \
		$(ROOT)/codegen/lib/ppu.c \
		$(ROOT)/codegen/lib/nes_palette_rgb.inc \
		apu_null.c | $(BUILD)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror \
		-Wno-unused-parameter -Wno-unused-variable \
		-Wno-unused-function \
		-I$(ROOT)/codegen/lib \
		$(ROOT)/tools/test_offscreen_bits.c \
		$(ROOT)/codegen/lib/instructions.c \
		$(ROOT)/codegen/lib/code.c \
		$(ROOT)/codegen/lib/data.c \
		$(ROOT)/codegen/lib/cpu.c \
		$(ROOT)/codegen/lib/ppu.c \
		apu_null.c -o $@

$(TOP_SCORE_CHECK_TEST): \
		$(ROOT)/tools/test_top_score_check.c \
		$(ROOT)/codegen/lib/code.c \
		$(ROOT)/codegen/lib/data.c \
		$(ROOT)/codegen/lib/instructions.c \
		$(ROOT)/codegen/lib/cpu.c \
		$(ROOT)/codegen/lib/ppu.c \
		$(ROOT)/codegen/lib/nes_palette_rgb.inc \
		apu_null.c | $(BUILD)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror \
		-Wno-unused-parameter -Wno-unused-variable \
		-Wno-unused-function \
		-I$(ROOT)/codegen/lib \
		$(ROOT)/tools/test_top_score_check.c \
		$(ROOT)/codegen/lib/instructions.c \
		$(ROOT)/codegen/lib/code.c \
		$(ROOT)/codegen/lib/data.c \
		$(ROOT)/codegen/lib/cpu.c \
		$(ROOT)/codegen/lib/ppu.c \
		apu_null.c -o $@

$(ROM_FALLTHROUGH_TEST): \
		$(ROOT)/tools/test_rom_fallthrough_data.c \
		$(ROOT)/codegen/lib/data.c \
		$(ROOT)/codegen/lib/data.h | $(BUILD)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror \
		-I$(ROOT)/codegen/lib \
		$(ROOT)/tools/test_rom_fallthrough_data.c \
		$(ROOT)/codegen/lib/data.c -o $@

$(INPUT_POLICY_TEST): \
		$(ROOT)/tools/test_neogeo_input_policy.c input_policy.h | $(BUILD)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror \
		-I. \
		$(ROOT)/tools/test_neogeo_input_policy.c -o $@

$(APU_BRIDGE_TEST): \
		$(ROOT)/tools/test_neogeo_apu_bridge.c \
		apu_bridge.c apu_bridge.h apu_frame_units.h | $(BUILD)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror \
		-I. \
		$(ROOT)/tools/test_neogeo_apu_bridge.c \
		apu_bridge.c -o $@

$(APU_FRAME_UNITS_TEST): \
		$(ROOT)/tools/test_neogeo_apu_frame_units.c \
		apu_frame_units.h | $(BUILD)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror \
		-I. \
		$(ROOT)/tools/test_neogeo_apu_frame_units.c -o $@

$(AUDIO_PACKET_TEST): \
		$(ROOT)/tools/test_neogeo_audio_packet.c audio_packet.h | $(BUILD)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror \
		-I. \
		$(ROOT)/tools/test_neogeo_audio_packet.c -o $@

$(AUDIO_CADENCE_TEST): \
		$(ROOT)/tools/test_neogeo_audio_cadence.c \
		$(ROOT)/codegen/lib/apu.h \
		audio_cadence.c audio_cadence.h video.h | $(BUILD)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror \
		-I. -I$(ROOT)/codegen/lib \
		$(ROOT)/tools/test_neogeo_audio_cadence.c \
		audio_cadence.c -o $@

$(OAM_VISIBILITY_TEST): \
		$(ROOT)/tools/test_neogeo_oam_visibility.c \
		oam_visibility.h | $(BUILD)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror \
		-I. \
		$(ROOT)/tools/test_neogeo_oam_visibility.c -o $@

$(OAM_TILES_TEST): \
		$(ROOT)/tools/test_neogeo_oam_tiles.c oam_tiles.h | $(BUILD)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror \
		-I. \
		$(ROOT)/tools/test_neogeo_oam_tiles.c -o $@

$(PALETTE_POLICY_TEST): \
		$(ROOT)/tools/test_neogeo_palette_policy.c \
		palette_policy.h | $(BUILD)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror \
		-I. \
		$(ROOT)/tools/test_neogeo_palette_policy.c -o $@

$(VBLANK_BUDGET_TEST): \
		$(ROOT)/tools/test_neogeo_vblank_budget.c \
		vblank_budget.h | $(BUILD)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror \
		-I. \
		$(ROOT)/tools/test_neogeo_vblank_budget.c -o $@

VRAM_BATCH_TEST_SOURCES := \
	$(ROOT)/tools/test_neogeo_vram_batch.c \
	$(ROOT)/codegen/lib/instructions.c \
	$(ROOT)/codegen/lib/code.c \
	$(ROOT)/codegen/lib/data.c \
	$(ROOT)/codegen/lib/cpu.c \
	ppu_direct.c title_data_stub.c apu_null.c

$(VRAM_BATCH_TEST): \
		$(VRAM_BATCH_TEST_SOURCES) ppu_render_state.h title_data.h video.h \
		$(ROOT)/codegen/lib/ppu.h | $(BUILD)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror \
		-Wno-unused-parameter -Wno-unused-variable \
		-Wno-unused-function \
		-DSMB_NEOGEO_VRAM_BATCH_TEST \
		-DSMB_NEOGEO_TITLE_TEST_DATA \
		-I. -I$(ROOT)/codegen/lib \
		$(VRAM_BATCH_TEST_SOURCES) -o $@

$(VRAM_BATCH_SANITIZE_TEST): \
		$(VRAM_BATCH_TEST_SOURCES) ppu_render_state.h title_data.h video.h \
		$(ROOT)/codegen/lib/ppu.h | $(BUILD)
	$(HOST_CC) -std=c11 -O1 -g -Wall -Wextra -Werror \
		-Wno-unused-parameter -Wno-unused-variable \
		-Wno-unused-function -fno-omit-frame-pointer \
		-fsanitize=address,undefined \
		-DSMB_NEOGEO_VRAM_BATCH_TEST \
		-DSMB_NEOGEO_TITLE_TEST_DATA \
		-I. -I$(ROOT)/codegen/lib \
		$(VRAM_BATCH_TEST_SOURCES) -o $@

test-vram-batch-sanitize: $(VRAM_BATCH_SANITIZE_TEST)
	$(VRAM_BATCH_SANITIZE_TEST)

test: \
		$(PPU_STATE_TEST) $(HUD_UPDATE_TEST) $(BACKGROUND_CHAIN_TEST) \
		$(REPLAY_GATE_TEST) \
		$(REPLAY_TIMING_TEST) $(REPLAY_CHECKPOINT_TEST) \
		$(JUMP_ENGINE_TEST) $(COLLISION_CORE_TEST) $(BOUNDING_BOX_CORE_TEST) \
		$(ENEMY_BOUNDING_BOX_TEST) \
		$(BLOCK_BUFFER_COLLISION_TEST) $(MOVE_OBJECT_HORIZ_TEST) \
		$(RELATIVE_ENEMY_POSITION_TEST) \
		$(DRAW_SPRITE_OBJECT_TEST) $(MOVE_SPRITES_OFFSCREEN_TEST) \
		$(SPRITE_SHUFFLER_TEST) $(CORE_FAST_PATHS_TEST) \
		$(DRAW_LARGE_PLATFORM_FAST_TEST) \
		$(SPR_OBJECT_OFFSCREEN_FAST_TEST) \
		$(ENEMY_BG_COLLISION_FAST_TEST) \
		$(ENEMY_COLLISION_FAST_TEST) \
		$(RENDER_AREA_GRAPHICS_FAST_TEST) \
		$(READ_PORT_BITS_TEST) $(OFFSCREEN_BITS_TEST) \
		$(TOP_SCORE_CHECK_TEST) \
		$(ROM_FALLTHROUGH_TEST) $(INPUT_POLICY_TEST) \
		$(APU_BRIDGE_TEST) $(APU_FRAME_UNITS_TEST) $(AUDIO_PACKET_TEST) \
		$(AUDIO_CADENCE_TEST) $(OAM_VISIBILITY_TEST) $(OAM_TILES_TEST) \
		$(PALETTE_POLICY_TEST) $(VBLANK_BUDGET_TEST) $(VRAM_BATCH_TEST)
	$(PYTHON) -m unittest discover \
		-s $(ROOT)/tools -p 'test_*.py'
	$(PPU_STATE_TEST)
	$(HUD_UPDATE_TEST)
	$(BACKGROUND_CHAIN_TEST)
	$(REPLAY_GATE_TEST)
	$(REPLAY_TIMING_TEST)
	$(REPLAY_CHECKPOINT_TEST)
	$(JUMP_ENGINE_TEST)
	$(COLLISION_CORE_TEST)
	$(BLOCK_BUFFER_COLLISION_TEST)
	$(BOUNDING_BOX_CORE_TEST)
	$(ENEMY_BOUNDING_BOX_TEST)
	$(MOVE_OBJECT_HORIZ_TEST)
	$(RELATIVE_ENEMY_POSITION_TEST)
	$(DRAW_SPRITE_OBJECT_TEST)
	$(MOVE_SPRITES_OFFSCREEN_TEST)
	$(SPRITE_SHUFFLER_TEST)
	$(CORE_FAST_PATHS_TEST)
	$(DRAW_LARGE_PLATFORM_FAST_TEST)
	$(SPR_OBJECT_OFFSCREEN_FAST_TEST)
	$(ENEMY_BG_COLLISION_FAST_TEST)
	$(ENEMY_COLLISION_FAST_TEST)
	$(RENDER_AREA_GRAPHICS_FAST_TEST)
	$(READ_PORT_BITS_TEST)
	$(OFFSCREEN_BITS_TEST)
	$(TOP_SCORE_CHECK_TEST)
	$(ROM_FALLTHROUGH_TEST)
	$(INPUT_POLICY_TEST)
	$(APU_BRIDGE_TEST)
	$(APU_FRAME_UNITS_TEST)
	$(AUDIO_PACKET_TEST)
	$(AUDIO_CADENCE_TEST)
	$(OAM_VISIBILITY_TEST)
	$(OAM_TILES_TEST)
	$(PALETTE_POLICY_TEST)
	$(VBLANK_BUDGET_TEST)
	$(VRAM_BATCH_TEST)

verify-codegen:
	$(PYTHON) $(ROOT)/tools/check_codegen_consistency.py \
		--moon "$(MOON)"

verify: verify-codegen test $(TARGET) verify-sound-driver
	$(PYTHON) $(ROOT)/tools/check_neogeo_elf.py $(TARGET)

assets: $(TITLE_SOURCE)

$(TITLE_SOURCE): FORCE | $(ASSETDIR)
	@test -n "$(SMB_ROM)" || { \
		echo "SMB_ROM is required (a .nes or ZIP containing it)" >&2; \
		exit 2; \
	}
	$(PYTHON) $(ROOT)/tools/gen_neogeo_assets.py \
		--input "$(SMB_ROM)" \
		--output-dir "$(ASSETDIR)"

FORCE:

native-roms: \
		assets $(CART_TARGET) verify-sound-driver $(TRIANGLE_VROM) \
		| $(ROMDIR) $(ASSETDIR)
	@test -n "$(SMB_ROM)" || { \
		echo "SMB_ROM is required (a .nes or ZIP containing it)" >&2; \
		exit 2; \
	}
	$(PYTHON) $(ROOT)/tools/check_neogeo_elf.py $(CART_TARGET)
	$(OBJCOPY) -O binary -S -R .text2 \
		--gap-fill 0xff --pad-to 1048576 \
		$(CART_TARGET) $(PROM)
	dd if=$(PROM) of=$(PROM) conv=notrunc,swab status=none
	cp $(ASSETDIR)/$(GAMEROM)-c1.c1 $(CROM1)
	cp $(ASSETDIR)/$(GAMEROM)-c2.c2 $(CROM2)
	cp $(ASSETDIR)/$(GAMEROM)-s1.s1 $(SROM1)
	$(Z80_OBJCOPY) -I ihex -O binary \
		$(SOUND_DRIVER) $(MROM1) \
		--pad-to 131072
	cp $(TRIANGLE_VROM) $(VROM1)
	echo "$(TRIANGLE_VROM_SHA256)  $(VROM1)" | sha256sum -c -
	chmod 0644 $(PROM) $(CROM1) $(CROM2) $(SROM1) $(MROM1) $(VROM1)
	TZ=UTC touch -t $(CARTRIDGE_TIMESTAMP) \
		$(PROM) $(CROM1) $(CROM2) $(SROM1) $(MROM1) $(VROM1)

cart: hardware-cart neosd-cart

compat-cart: native-roms $(PUZZLEDP_COMPAT_BUILDER)
	$(PYTHON) $(PUZZLEDP_COMPAT_BUILDER) \
		--rom-dir $(ROMDIR) \
		--output $(COMPAT_CART)
	@echo "Optional fixed-database cartridge: $(COMPAT_CART)"

hardware-cart: native-roms
	@test -f "$(GNGEO_SUPPORT_DATA)" || { \
		echo "GnGeo support data is missing: $(GNGEO_SUPPORT_DATA)" >&2; \
		echo "override GNGEO_SUPPORT_DATA with its installed path" >&2; \
		exit 2; \
	}
	TZ=UTC $(ROMTOOL) -b cartridge -f zip \
		-p $(PROM) -c $(CROM1) $(CROM2) \
		-v $(VROM1) -s $(SROM1) -m $(MROM1) \
		-n $(GAMEROM) -l "Super Mario Bros. Neo" \
		-y 2026 --publisher "Community port" \
		-o $(HARDWARE_CART)
	$(ROMTOOL) -b hash -f gngeo \
		-p $(PROM) -c $(CROM1) $(CROM2) \
		-v $(VROM1) -s $(SROM1) -m $(MROM1) \
		-n $(GAMEROM) -l "Super Mario Bros. Neo" \
		-x gngeo.data=$(GNGEO_SUPPORT_DATA) \
		-o $(GNGEO_DATA)
	$(PYTHON) $(GNGEO_DRIVER_CHECKER) \
		--data $(GNGEO_DATA) --rom-dir $(ROMDIR) \
		--shortname $(GAMEROM) --title "Super Mario Bros. Neo"
	cp $(NG_SHARE)/aes.zip $(ROMDIR)/
	cp $(NG_SHARE)/neogeo.zip $(ROMDIR)/
	@echo "Canonical hardware/MAME/GnGeo cartridge: $(HARDWARE_CART)"

neosd-cart: native-roms $(NEOSD_BUILDER)
	$(ROMTOOL) -b cartridge -f neo \
		-p $(PROM) -c $(CROM1) $(CROM2) \
		-v $(VROM1) -s $(SROM1) -m $(MROM1) \
		-n $(GAMEROM) -l "Super Mario Bros. Neo" \
		-y 2026 --publisher "Community port" \
		-x neo.genre=Platformer neo.ngh=$(SMBNEO_NGH) \
		-o $(NEOSD_CART)
	$(PYTHON) $(NEOSD_BUILDER) \
		--rom-dir $(ROMDIR) --validate $(NEOSD_CART)
	@echo "NeoSD/NeoSD Pro image: $(NEOSD_CART)"

run: hardware-cart
	$(GNGEO) -b soft --screen320 --scale 3 --no-resize --sound \
		$(GNGEO_REALTIME_FLAGS) \
		--system home --p1control "$(GNGEO_P1)" \
		--p2control "$(GNGEO_P2)" \
		-i $(ROMDIR) -d $(GNGEO_DATA) $(GAMEROM)

mame-list: hardware-cart | $(MAME_HASHDIR)
	$(PYTHON) $(MAME_SOFTWARE_GENERATOR) \
		--rom-dir $(ROMDIR) \
		--output $(MAME_SOFTWARE_LIST)

mame-cart: mame-list
	@echo "Canonical MAME software list: $(MAME_SOFTWARE_LIST)"
	@echo "Canonical MAME cartridge: $(HARDWARE_CART)"

mame-run: mame-cart | $(MAME_CFG_DIR) $(MAME_NVRAM_DIR)
	@command -v "$(MAME)" >/dev/null 2>&1 || { \
		echo "MAME executable not found: $(MAME)" >&2; \
		exit 2; \
	}
	$(MAME) $(MAME_SYSTEM) $(GAMEROM) \
		-hashpath "$(MAME_HASHDIR)" \
		-rompath "$(MAME_ROMPATH)" \
		-bios "$(MAME_BIOS)" \
		-cfg_directory "$(MAME_CFG_DIR)" \
		-nvram_directory "$(MAME_NVRAM_DIR)" \
		$(MAME_RUN_FLAGS)

mame-capture: \
		mame-cart \
		| $(MAME_CAPTURE_DIR) $(MAME_CFG_DIR) $(MAME_NVRAM_DIR)
	@command -v "$(MAME)" >/dev/null 2>&1 || { \
		echo "MAME executable not found: $(MAME)" >&2; \
		exit 2; \
	}
	$(MAME) $(MAME_SYSTEM) $(GAMEROM) \
		-hashpath "$(MAME_HASHDIR)" \
		-rompath "$(MAME_ROMPATH)" \
		-bios "$(MAME_BIOS)" \
		-cfg_directory "$(MAME_CFG_DIR)" \
		-nvram_directory "$(MAME_NVRAM_DIR)" \
		-snapshot_directory "$(MAME_CAPTURE_DIR)" \
		-snapview native \
		-video none -sound none -nothrottle -seconds_to_run 25 \
		-autoboot_script "$(MAME_CAPTURE_SCRIPT)"
	@echo "MAME captures: $(MAME_CAPTURE_DIR)"

$(WEB_NATIVE_PROM): $(TARGET) | $(WEB_BUILD)
	$(OBJCOPY) -O binary -S -R .text2 \
		--gap-fill 0xff --pad-to 1048576 \
		$(TARGET) $@
	dd if=$@ of=$@ conv=notrunc,swab status=none
	chmod 0644 $@

$(WEB_PROM): $(WEB_TARGET) | $(WEB_BUILD)
	$(OBJCOPY) -O binary -S -R .text2 \
		--gap-fill 0xff --pad-to 1048576 \
		$(WEB_TARGET) $@
	dd if=$@ of=$@ conv=notrunc,swab status=none
	chmod 0644 $@

$(WEB_MROM): $(SOUND_DRIVER) | $(WEB_BUILD)
	$(Z80_OBJCOPY) -I ihex -O binary \
		$(SOUND_DRIVER) $@ \
		--pad-to 131072
	chmod 0644 $@

$(WEB_VROM): $(TRIANGLE_VROM) | $(WEB_BUILD)
	cp $(TRIANGLE_VROM) $@
	chmod 0644 $@

web: $(WEB_NATIVE_PROM) $(WEB_PROM) $(WEB_MROM) $(WEB_VROM) verify-sound-driver
	@test -f "$(WEB_BIOS)" || { \
		echo "ngdevkit NullBIOS is missing: $(WEB_BIOS)" >&2; \
		exit 2; \
	}
	$(PYTHON) $(ROOT)/tools/check_neogeo_elf.py $(WEB_TARGET)
	$(PYTHON) $(WEB_PLAYER_BUILDER) \
		--native-elf $(TARGET) \
		--native-prom $(WEB_NATIVE_PROM) \
		--web-elf $(WEB_TARGET) \
		--web-prom $(WEB_PROM) \
		--mrom $(WEB_MROM) \
		--vrom $(WEB_VROM) \
		--bios "$(WEB_BIOS)" \
		--nm $(NM) \
		--templates $(ROOT)/web \
		--title-image $(ROOT)/docs/screenshots/title-screen.png \
		--output $(WEB_SITE)

replay-cart: \
		assets $(REPLAY_TARGET) verify-sound-driver $(TRIANGLE_VROM) \
		| $(REPLAY_ROMDIR)
	@test -n "$(SMB_ROM)" || { \
		echo "SMB_ROM is required (a .nes or ZIP containing it)" >&2; \
		exit 2; \
	}
	@test -n "$(REPLAY_FM2)" || { \
		test -n "$(REPLAY_DATA_HEADER)" || { \
			echo "REPLAY_FM2 or REPLAY_DATA_HEADER is required" >&2; \
			exit 2; \
		}; \
	}
	@test -z "$(REPLAY_FM2)" || test -z "$(REPLAY_DATA_HEADER)" || { \
		echo "set only one of REPLAY_FM2 or REPLAY_DATA_HEADER" >&2; \
		exit 2; \
	}
	@test -f "$(GNGEO_SUPPORT_DATA)" || { \
		echo "GnGeo support data is missing: $(GNGEO_SUPPORT_DATA)" >&2; \
		echo "override GNGEO_SUPPORT_DATA with its installed path" >&2; \
		exit 2; \
	}
	$(PYTHON) $(ROOT)/tools/check_neogeo_elf.py $(REPLAY_TARGET)
	$(OBJCOPY) -O binary -S -R .text2 \
		--gap-fill 0xff --pad-to 1048576 \
		$(REPLAY_TARGET) $(REPLAY_PROM)
	dd if=$(REPLAY_PROM) of=$(REPLAY_PROM) conv=notrunc,swab status=none
	cp $(ASSETDIR)/$(GAMEROM)-c1.c1 $(REPLAY_CROM1)
	cp $(ASSETDIR)/$(GAMEROM)-c2.c2 $(REPLAY_CROM2)
	cp $(ASSETDIR)/$(GAMEROM)-s1.s1 $(REPLAY_SROM1)
	$(Z80_OBJCOPY) -I ihex -O binary \
		$(SOUND_DRIVER) $(REPLAY_MROM1) \
		--pad-to 131072
	cp $(TRIANGLE_VROM) $(REPLAY_VROM1)
	echo "$(TRIANGLE_VROM_SHA256)  $(REPLAY_VROM1)" | sha256sum -c -
	chmod 0644 \
		$(REPLAY_PROM) $(REPLAY_CROM1) $(REPLAY_CROM2) \
		$(REPLAY_SROM1) $(REPLAY_MROM1) $(REPLAY_VROM1)
	TZ=UTC touch -t $(CARTRIDGE_TIMESTAMP) \
		$(REPLAY_PROM) $(REPLAY_CROM1) $(REPLAY_CROM2) \
		$(REPLAY_SROM1) $(REPLAY_MROM1) $(REPLAY_VROM1)
	TZ=UTC $(ROMTOOL) -b cartridge -f zip \
		-p $(REPLAY_PROM) -c $(REPLAY_CROM1) $(REPLAY_CROM2) \
		-v $(REPLAY_VROM1) -s $(REPLAY_SROM1) -m $(REPLAY_MROM1) \
		-n $(GAMEROM) -l "SMBNeo TAS replay gate" \
		-y 2026 --publisher "Community port" \
		-o $(REPLAY_CART)
	$(ROMTOOL) -b hash -f gngeo \
		-p $(REPLAY_PROM) -c $(REPLAY_CROM1) $(REPLAY_CROM2) \
		-v $(REPLAY_VROM1) -s $(REPLAY_SROM1) -m $(REPLAY_MROM1) \
		-n $(GAMEROM) -l "SMBNeo TAS replay gate" \
		-x gngeo.data=$(GNGEO_SUPPORT_DATA) \
		-o $(REPLAY_GNGEO_DATA)
	cp $(NG_SHARE)/aes.zip $(REPLAY_ROMDIR)/
	cp $(NG_SHARE)/neogeo.zip $(REPLAY_ROMDIR)/
	@echo "Built $(REPLAY_CART)"

replay-run: replay-cart
	$(GNGEO) -b soft --screen320 --scale 3 --no-resize --sound \
		$(GNGEO_REALTIME_FLAGS) \
		--system home \
		-i $(REPLAY_ROMDIR) -d $(REPLAY_GNGEO_DATA) $(GAMEROM)

replay-rendered-evidence: replay-cart
	@test "$(REPLAY_FAST)" = "0" || { \
		echo "replay-rendered-evidence requires REPLAY_FAST=0" >&2; \
		exit 2; \
	}
	@test "$(REPLAY_HARDWARE_PLAYABLE)" = "1" || { \
		echo "replay-rendered-evidence requires REPLAY_HARDWARE_PLAYABLE=1" >&2; \
		exit 2; \
	}
	@test "$(REPLAY_STAGE_SETTLE_FRAMES)" = "2" || { \
		echo "replay-rendered-evidence requires REPLAY_STAGE_SETTLE_FRAMES=2" >&2; \
		exit 2; \
	}
	$(PYTHON) $(ROOT)/tools/run_neogeo_replay_gate.py \
		--rendered-evidence \
		--68k-overclock "$(REPLAY_EVIDENCE_OVERCLOCK)" \
		--timeout "$(REPLAY_EVIDENCE_TIMEOUT)" $(if $(strip $(REPLAY_EVIDENCE_DIR)),--evidence-dir "$(REPLAY_EVIDENCE_DIR)",)

clean:
	rm -rf $(BUILD)

-include $(DEPENDENCIES)
