# Deliberately separate from the default home cartridge pipeline.
NG_CC := m68k-neogeo-elf-gcc
NG_LTO_FLAGS ?= -flto
GNGEO_SUPPORT_DATA ?= $(shell pkg-config --variable=sharedir ngdevkit)/../ngdevkit-gngeo/gngeo_data.zip
NG_FLAGS := -m68000 -Os -std=gnu11 -ffunction-sections -fdata-sections \
 -fno-asynchronous-unwind-tables -fno-unwind-tables -DSMB_VS \
 $(NG_LTO_FLAGS) -I. -Inative -I../../platform/neogeo -I../../codegen/lib \
 $(shell pkg-config --cflags ngdevkit)
NG_COMMON_OBJECTS := $(addprefix $(BUILD)/ng-,vs_bus.o video.o ppu_direct.o apu_bridge.o apu_neogeo.o audio_cadence.o)
NG_OBJECTS := $(NG_COMMON_OBJECTS) $(BUILD)/ng-neogeo_native_main.o \
 $(BUILD)/ng-vs_native_platform.o $(BUILD)/ng-vs_native_fast_motion.o \
 $(BUILD)/ng-vs_native_fast_actor.o $(BUILD)/ng-vs_native_fast_actor_policy.o \
 $(BUILD)/ng-vs_native_fast_collision.o $(BUILD)/ng-vs_native_fast_meta.o \
 $(BUILD)/ng-vs_native_fast_game.o \
 $(BUILD)/ng-vs_native_fast_video.o \
 $(BUILD)/ng-vs_native_fast_nmi.o \
 $(BUILD)/ng-smbneo_native.o \
 $(BUILD)/ng-vs_data.o
NG_LEGACY_OBJECTS := $(NG_COMMON_OBJECTS) $(BUILD)/ng-neogeo_main.o \
 $(BUILD)/ng-vs_machine.o $(BUILD)/vs_program.m68k.o $(BUILD)/ng-vs_data.o

.PHONY: neogeo-assets neogeo-elf neogeo-legacy-elf
neogeo-assets:
	@test -n "$(VS_ROM)" || (echo "set VS_ROM to your suprmrio.zip"; exit 2)
	python3 ../../tools/gen_vs_assets.py --input "$(VS_ROM)" --output-dir "$(BUILD)/assets"

neogeo-elf: $(BUILD)/vssmbneo.elf

# The translated executable remains available only as a development oracle.
# It is never packaged by the normal cartridge/release target.
neogeo-legacy-elf: $(BUILD)/vssmbneo-legacy.elf

.PHONY: neogeo-cart
neogeo-cart: neogeo-assets neogeo-elf
	python3 ../../tools/check_neogeo_elf.py $(BUILD)/vssmbneo.elf --variant vs-native
	$(MAKE) -C ../../platform/neogeo verify-sound-driver build/smbneo-triangle-v1.v1
	python3 ../../tools/package_vs.py --build "$(BUILD)" \
	 --sound ../../platform/neogeo/build/smbneogeo-sound.ihx \
	 --samples ../../platform/neogeo/build/smbneo-triangle-v1.v1 \
	 --gngeo-data "$(GNGEO_SUPPORT_DATA)"

$(BUILD)/ng-%.o: %.c
	mkdir -p $(BUILD)
	$(NG_CC) $(NG_FLAGS) -MMD -MP -c $< -o $@

$(BUILD)/ng-%.o: ../../platform/neogeo/%.c
	mkdir -p $(BUILD)
	$(NG_CC) $(NG_FLAGS) -MMD -MP -c $< -o $@

# Keep the reviewed renderer's optimization profile; core size tuning must not
# quietly alter the assumptions behind its bounded live VRAM commit loops.
$(BUILD)/ng-video.o: ../../platform/neogeo/video.c
	$(NG_CC) $(filter-out -Os,$(NG_FLAGS)) -O3 -mlra -fomit-frame-pointer \
	 -fira-loop-pressure -frename-registers -fweb -fipa-pta -MMD -MP -c $< -o $@

$(BUILD)/vs_program.m68k.o: $(BUILD)/vs_program.c vs_cpu.h vs_bus.h vs_fast_paths.h
	$(NG_CC) $(filter-out -Os,$(NG_FLAGS)) -O3 -mlra -fomit-frame-pointer -fno-jump-tables \
	 -fira-loop-pressure -frename-registers -fweb -fipa-pta -fstack-usage -c $< -o $@

$(BUILD)/ng-vs_data.o: neogeo-assets
	$(NG_CC) $(NG_FLAGS) -c $(BUILD)/assets/vs_data.c -o $@

$(BUILD)/ng-smbneo_native.o: native/smbneo_native.c native/smbneo_native.h \
 vs_native_fast_motion.h vs_native_fast_actor.h vs_native_fast_video.h \
 vs_native_fast_actor_policy.h vs_native_fast_collision.h \
 vs_native_fast_meta.h vs_native_fast_game.h vs_native_fast_nmi.h
	mkdir -p $(BUILD)
	$(NG_CC) $(filter-out -Os,$(NG_FLAGS)) -O2 -mlra -fomit-frame-pointer \
	 -fira-loop-pressure -frename-registers -fweb -fipa-pta \
	 -DSMBNEO_NATIVE_FAST_PATHS=1 -fstack-usage -MMD -MP -c $< -o $@

$(BUILD)/vssmbneo.elf: $(NG_OBJECTS)
	$(NG_CC) -m68000 $(NG_LTO_FLAGS) -Wl,--gc-sections -Wl,-u,rom_game_default -Wl,-u,rom_callback_VBlank \
	 -Wl,-u,vs_chr -Wl,-u,vs_prg \
	 -Wl,--defsym=rom_eye_catcher_mode=2 \
	 -Wl,--defsym=rom_NGH_ID=0x2027 -Wl,-Map,$(BUILD)/vssmbneo.map \
	 -o $@ $(NG_OBJECTS) $(shell pkg-config --libs ngdevkit)

$(BUILD)/vssmbneo-legacy.elf: $(NG_LEGACY_OBJECTS)
	$(NG_CC) -m68000 $(NG_LTO_FLAGS) -Wl,--gc-sections -Wl,-u,rom_game_default -Wl,-u,rom_callback_VBlank \
	 -Wl,--defsym=rom_eye_catcher_mode=2 \
	 -Wl,--defsym=rom_NGH_ID=0x2027 -Wl,-Map,$(BUILD)/vssmbneo-legacy.map \
	 -o $@ $(NG_LEGACY_OBJECTS) $(shell pkg-config --libs ngdevkit)

-include $(NG_OBJECTS:.o=.d) $(NG_LEGACY_OBJECTS:.o=.d)
