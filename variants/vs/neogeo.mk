# Deliberately separate from the default home cartridge pipeline.
NG_CC := m68k-neogeo-elf-gcc
GNGEO_SUPPORT_DATA ?= $(shell pkg-config --variable=sharedir ngdevkit)/../ngdevkit-gngeo/gngeo_data.zip
NG_FLAGS := -m68000 -Os -std=gnu11 -ffunction-sections -fdata-sections \
 -fno-asynchronous-unwind-tables -fno-unwind-tables -DSMB_VS \
 -I. -I../../platform/neogeo -I../../codegen/lib $(shell pkg-config --cflags ngdevkit)
NG_OBJECTS := $(addprefix $(BUILD)/ng-,neogeo_main.o vs_bus.o vs_machine.o video.o ppu_direct.o apu_bridge.o apu_neogeo.o)
NG_OBJECTS += $(BUILD)/vs_program.m68k.o $(BUILD)/ng-vs_data.o

.PHONY: neogeo-assets neogeo-elf
neogeo-assets:
	python3 ../../tools/gen_vs_assets.py --input "$(VS_ROM)" --output-dir "$(BUILD)/assets"

neogeo-elf: $(BUILD)/vssmbneo.elf

.PHONY: neogeo-cart
neogeo-cart: neogeo-elf
	python3 ../../tools/check_neogeo_elf.py $(BUILD)/vssmbneo.elf --variant vs
	python3 ../../tools/check_vs_stack_usage.py $(BUILD)/vs_program.m68k.su --max-frame 96
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

$(BUILD)/ng-vs_data.o: $(BUILD)/assets/vs_data.c
	$(NG_CC) $(NG_FLAGS) -c $< -o $@

$(BUILD)/vssmbneo.elf: $(NG_OBJECTS)
	$(NG_CC) -m68000 -Wl,--gc-sections -Wl,-u,rom_game_default -Wl,-u,rom_callback_VBlank \
	 -Wl,--defsym=rom_eye_catcher_mode=2 \
	 -Wl,--defsym=rom_NGH_ID=0x2027 -Wl,-Map,$(BUILD)/vssmbneo.map \
	 -o $@ $(NG_OBJECTS) $(shell pkg-config --libs ngdevkit)

-include $(NG_OBJECTS:.o=.d)
