# ROM-free VS cartridge templates for the browser player.  This makefile
# deliberately reuses the production native object graph while replacing only
# the user-owned VS data object.  Invoke it with a dedicated BUILD directory.

BUILD ?= build/web-template
FBNEO ?= 0
OBJCOPY := m68k-neogeo-elf-objcopy

include neogeo.mk

NG_TEMPLATE_DATA_OBJECT := $(BUILD)/ng-vs_data_stub.o
NG_TEMPLATE_VIDEO_OBJECT := $(BUILD)/ng-video-web.o
NG_TEMPLATE_OBJECTS := \
	$(filter-out $(BUILD)/ng-vs_data.o,$(NG_OBJECTS)) \
	$(NG_TEMPLATE_DATA_OBJECT)

ifeq ($(FBNEO),1)
NG_TEMPLATE_OBJECTS := \
	$(filter-out $(BUILD)/ng-video.o,$(NG_TEMPLATE_OBJECTS)) \
	$(NG_TEMPLATE_VIDEO_OBJECT)
NG_TEMPLATE_PROM := $(BUILD)/vssmbneo-web-p1.p1
else
NG_TEMPLATE_PROM := $(BUILD)/vssmbneo-p1.p1
endif

NG_TEMPLATE_ELF := $(BUILD)/vssmbneo-template.elf
NG_TEMPLATE_MAP := $(BUILD)/vssmbneo-template.map

.PHONY: template
template: $(NG_TEMPLATE_ELF) $(NG_TEMPLATE_PROM)

$(NG_TEMPLATE_DATA_OBJECT): vs_data_stub.c
	mkdir -p $(BUILD)
	$(NG_CC) $(NG_FLAGS) -MMD -MP -c $< -o $@

$(NG_TEMPLATE_VIDEO_OBJECT): ../../platform/neogeo/video.c
	mkdir -p $(BUILD)
	$(NG_CC) $(filter-out -Os,$(NG_FLAGS)) -O3 -mlra \
		-fomit-frame-pointer -fira-loop-pressure -frename-registers \
		-fweb -fipa-pta -DSMB_NEOGEO_FBNEO -MMD -MP -c $< -o $@

$(NG_TEMPLATE_ELF): $(NG_TEMPLATE_OBJECTS)
	$(NG_CC) -m68000 $(NG_LTO_FLAGS) -Wl,--gc-sections \
		-Wl,-u,rom_game_default -Wl,-u,rom_callback_VBlank \
		-Wl,-u,vs_chr -Wl,-u,vs_prg -Wl,-u,vs_palette_neogeo \
		-Wl,--defsym=rom_eye_catcher_mode=2 \
		-Wl,--defsym=rom_NGH_ID=0x2027 -Wl,-Map,$(NG_TEMPLATE_MAP) \
		-o $@ $(NG_TEMPLATE_OBJECTS) $(shell pkg-config --libs ngdevkit)

$(NG_TEMPLATE_PROM): $(NG_TEMPLATE_ELF)
	$(OBJCOPY) -O binary -S -R .text2 --gap-fill 0xff --pad-to 1048576 \
		$< $@
	dd if=$@ of=$@ conv=notrunc,swab status=none
	chmod 0644 $@

-include $(NG_TEMPLATE_DATA_OBJECT:.o=.d) \
	$(if $(filter 1,$(FBNEO)),$(NG_TEMPLATE_VIDEO_OBJECT:.o=.d),)
