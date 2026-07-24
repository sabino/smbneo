ROOT := ../..
BUILD := build
ROMDIR := $(BUILD)/rom
ASSETDIR := $(BUILD)/assets
TITLE_SOURCE := $(ASSETDIR)/smbneogeo-title.c

TARGET := $(BUILD)/smbneogeo.elf
MAP := $(BUILD)/smbneogeo.map
CART_TARGET := $(BUILD)/smbneogeo-cart.elf
CART_MAP := $(BUILD)/smbneogeo-cart.map
PPU_STATE_TEST := $(BUILD)/test_neogeo_ppu_render_state
REPLAY_GATE_TEST := $(BUILD)/test_neogeo_replay_gate
REPLAY_TIMING_TEST := $(BUILD)/test_neogeo_replay_timing
REPLAY_CHECKPOINT_TEST := $(BUILD)/test_neogeo_replay_checkpoint
JUMP_ENGINE_TEST := $(BUILD)/test_jump_engine_dispatch
ROM_FALLTHROUGH_TEST := $(BUILD)/test_rom_fallthrough_data
INPUT_POLICY_TEST := $(BUILD)/test_neogeo_input_policy
APU_BRIDGE_TEST := $(BUILD)/test_neogeo_apu_bridge
AUDIO_PACKET_TEST := $(BUILD)/test_neogeo_audio_packet
AUDIO_CADENCE_TEST := $(BUILD)/test_neogeo_audio_cadence
GAMEROM := smbneogeo

CC := m68k-neogeo-elf-gcc
HOST_CC ?= cc
OBJCOPY := m68k-neogeo-elf-objcopy
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
	$(ARCH_FLAGS) $(OPT_FLAGS) $(TUNE_FLAGS) $(LTO_FLAGS)

ENGINE_SOURCES := \
	video.c ppu_direct.c apu_bridge.c apu_neogeo.c audio_cadence.c
CORE_NAMES := instructions code data cpu
ENGINE_OBJECTS := \
	$(ENGINE_SOURCES:%.c=$(BUILD)/%.o) \
	$(CORE_NAMES:%=$(BUILD)/%.o)
BASE_OBJECTS := $(BUILD)/main.o $(ENGINE_OBJECTS)
ELF_OBJECTS := $(BASE_OBJECTS) $(BUILD)/title_data_stub.o
CART_OBJECTS := $(BASE_OBJECTS) $(BUILD)/smbneogeo-title.o
DEPENDENCIES = \
	$(BASE_OBJECTS:.o=.d) \
	$(BUILD)/title_data_stub.d \
	$(BUILD)/smbneogeo-title.d \
	$(REPLAY_MAIN_OBJECT:.o=.d) \
	$(REPLAY_GATE_OBJECT:.o=.d)

PROM := $(ROMDIR)/$(GAMEROM)-p1.p1
CROM1 := $(ROMDIR)/$(GAMEROM)-c1.c1
CROM2 := $(ROMDIR)/$(GAMEROM)-c2.c2
SROM1 := $(ROMDIR)/$(GAMEROM)-s1.s1
MROM1 := $(ROMDIR)/$(GAMEROM)-m1.m1
VROM1 := $(ROMDIR)/$(GAMEROM)-v1.v1
TRIANGLE_VROM := $(BUILD)/$(GAMEROM)-triangle-v1.v1
CART := $(ROMDIR)/$(GAMEROM).zip
GNGEO_DATA := $(ROMDIR)/gngeo_data.zip

SMB_ROM ?=
REPLAY_FM2 ?=
REPLAY_FAST ?= 0
REPLAY_HARDWARE_PLAYABLE ?= 0
REPLAY_TAIL_FRAMES ?= 1800
REPLAY_BOOTSTRAP_FRAMES ?= 7
REPLAY_AREA_INIT_HOLD_FRAMES ?= 1
REPLAY_STAGE_SETTLE_FRAMES ?= 2
REPLAY_EVIDENCE_TIMEOUT ?= 7200
REPLAY_EVIDENCE_OVERCLOCK ?= 0
REPLAY_EVIDENCE_DIR ?=
GNGEO ?= ngdevkit-gngeo
GNGEO_P1 ?= A=K97,B=K115,C=K113,D=K119,START=K49,UP=K82,DOWN=K81,LEFT=K80,RIGHT=K79
GNGEO_P2 ?= START=K50
GNGEO_REALTIME_FLAGS ?= \
	--autoframeskip --no-vsync --sleepidle \
	--68kclock=0 --z80clock=0
CARTRIDGE_TIMESTAMP ?= 202601010000.00

REPLAY_VARIANT := $(if $(filter 1,$(REPLAY_FAST)),fast,rendered)
REPLAYDIR := $(BUILD)/replay-$(REPLAY_VARIANT)
REPLAY_ROMDIR := $(REPLAYDIR)/rom
REPLAY_HEADER := $(REPLAYDIR)/smb_replay_data.h
REPLAY_MAIN_OBJECT := $(REPLAYDIR)/replay_main.o
REPLAY_GATE_OBJECT := $(REPLAYDIR)/replay_gate.o
REPLAY_TARGET := $(REPLAYDIR)/smbneogeo-replay.elf
REPLAY_MAP := $(REPLAYDIR)/smbneogeo-replay.map
REPLAY_OBJECTS := \
	$(REPLAY_MAIN_OBJECT) $(REPLAY_GATE_OBJECT) \
	$(ENGINE_OBJECTS) $(BUILD)/smbneogeo-title.o
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
TRIANGLE_VROM_SHA256 := \
	c52017058a226a44506a5d94fc1f692b42fe818302761da64d4f7adc5e5928a7
REPLAY_POLICY_FLAG := \
	$(if $(filter 1,$(REPLAY_HARDWARE_PLAYABLE)),--hardware-playable,)
REPLAY_FAST_FLAG := \
	$(if $(filter 1,$(REPLAY_FAST)),-DSMB_NEOGEO_REPLAY_FAST,)

.DEFAULT_GOAL := elf

.PHONY: \
	all elf test verify verify-codegen verify-sound-driver assets cart run \
	replay-cart replay-run replay-rendered-evidence clean FORCE

all: verify

elf: $(TARGET)

$(BUILD):
	mkdir -p $@

$(ROMDIR) $(ASSETDIR) $(REPLAYDIR) $(REPLAY_ROMDIR):
	mkdir -p $@

$(BUILD)/%.o: %.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: $(ROOT)/codegen/lib/%.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

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
	@test -n "$(REPLAY_FM2)" || { \
		echo "REPLAY_FM2 is required (a local text .fm2 movie)" >&2; \
		exit 2; \
	}
	$(PYTHON) $(ROOT)/tools/rec_tool.py emit-fm2-c \
		"$(REPLAY_FM2)" \
		--symbol-prefix smb_replay \
		$(REPLAY_POLICY_FLAG) \
		--output "$@"

$(REPLAY_MAIN_OBJECT): \
		replay_main.c replay_checkpoint.h replay_gate.h replay_timing.h \
		$(REPLAY_HEADER) | $(REPLAYDIR)
	$(CC) $(CPPFLAGS) -I$(REPLAYDIR) $(CFLAGS) \
		$(REPLAY_FAST_FLAG) \
		-DSMB_NEOGEO_REPLAY_TAIL_FRAMES=$(REPLAY_TAIL_FRAMES) \
		-DSMB_NEOGEO_REPLAY_BOOTSTRAP_FRAMES=$(REPLAY_BOOTSTRAP_FRAMES) \
		-DSMB_NEOGEO_REPLAY_AREA_INIT_HOLD_FRAMES=$(REPLAY_AREA_INIT_HOLD_FRAMES) \
		-DSMB_NEOGEO_REPLAY_STAGE_SETTLE_FRAMES=$(REPLAY_STAGE_SETTLE_FRAMES) \
		-c $< -o $@

$(REPLAY_GATE_OBJECT): replay_gate.c replay_gate.h | $(REPLAYDIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(TARGET): $(ELF_OBJECTS)
	$(CC) -o $@ $(ELF_OBJECTS) $(LDFLAGS) \
		-Wl,-Map,$(MAP) $(NG_LIBS)

$(CART_TARGET): $(CART_OBJECTS)
	$(CC) -o $@ $(CART_OBJECTS) $(LDFLAGS) \
		-Wl,-Map,$(CART_MAP) $(NG_LIBS)

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
		apu_bridge.c apu_bridge.h | $(BUILD)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror \
		-I. \
		$(ROOT)/tools/test_neogeo_apu_bridge.c \
		apu_bridge.c -o $@

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

test: \
		$(PPU_STATE_TEST) $(REPLAY_GATE_TEST) \
		$(REPLAY_TIMING_TEST) $(REPLAY_CHECKPOINT_TEST) \
		$(JUMP_ENGINE_TEST) \
		$(ROM_FALLTHROUGH_TEST) $(INPUT_POLICY_TEST) \
		$(APU_BRIDGE_TEST) $(AUDIO_PACKET_TEST) \
		$(AUDIO_CADENCE_TEST)
	$(PYTHON) -m unittest discover \
		-s $(ROOT)/tools -p 'test_*.py'
	$(PPU_STATE_TEST)
	$(REPLAY_GATE_TEST)
	$(REPLAY_TIMING_TEST)
	$(REPLAY_CHECKPOINT_TEST)
	$(JUMP_ENGINE_TEST)
	$(ROM_FALLTHROUGH_TEST)
	$(INPUT_POLICY_TEST)
	$(APU_BRIDGE_TEST)
	$(AUDIO_PACKET_TEST)
	$(AUDIO_CADENCE_TEST)

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

cart: \
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
	TZ=UTC $(ROMTOOL) -b cartridge -f zip \
		-p $(PROM) -c $(CROM1) $(CROM2) \
		-v $(VROM1) -s $(SROM1) -m $(MROM1) \
		-n $(GAMEROM) -l "Super Mario Bros. Neo Geo proof of concept" \
		-y 2026 --publisher "Community port" \
		-o $(CART)
	$(ROMTOOL) -b hash -f gngeo \
		-p $(PROM) -c $(CROM1) $(CROM2) \
		-v $(VROM1) -s $(SROM1) -m $(MROM1) \
		-n $(GAMEROM) -l "Super Mario Bros. Neo Geo proof of concept" \
		-x gngeo.data=/usr/share/ngdevkit-gngeo/gngeo_data.zip \
		-o $(GNGEO_DATA)
	cp $(NG_SHARE)/aes.zip $(ROMDIR)/
	cp $(NG_SHARE)/neogeo.zip $(ROMDIR)/
	@echo "Built $(CART)"

run: cart
	$(GNGEO) -b soft --screen320 --scale 3 --no-resize --sound \
		$(GNGEO_REALTIME_FLAGS) \
		--system home --p1control "$(GNGEO_P1)" \
		--p2control "$(GNGEO_P2)" \
		-i $(ROMDIR) -d $(GNGEO_DATA) $(GAMEROM)

replay-cart: \
		assets $(REPLAY_TARGET) verify-sound-driver $(TRIANGLE_VROM) \
		| $(REPLAY_ROMDIR)
	@test -n "$(SMB_ROM)" || { \
		echo "SMB_ROM is required (a .nes or ZIP containing it)" >&2; \
		exit 2; \
	}
	@test -n "$(REPLAY_FM2)" || { \
		echo "REPLAY_FM2 is required (a local text .fm2 movie)" >&2; \
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
		-n $(GAMEROM) -l "Super Mario Bros. Neo Geo TAS replay gate" \
		-y 2026 --publisher "Community port" \
		-o $(REPLAY_CART)
	$(ROMTOOL) -b hash -f gngeo \
		-p $(REPLAY_PROM) -c $(REPLAY_CROM1) $(REPLAY_CROM2) \
		-v $(REPLAY_VROM1) -s $(REPLAY_SROM1) -m $(REPLAY_MROM1) \
		-n $(GAMEROM) -l "Super Mario Bros. Neo Geo TAS replay gate" \
		-x gngeo.data=/usr/share/ngdevkit-gngeo/gngeo_data.zip \
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
