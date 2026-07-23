ROOT := ../..
BUILD := build
ROMDIR := $(BUILD)/rom
ASSETDIR := $(BUILD)/assets

TARGET := $(BUILD)/smbneogeo.elf
MAP := $(BUILD)/smbneogeo.map
GAMEROM := smbneogeo

CC := m68k-neogeo-elf-gcc
OBJCOPY := m68k-neogeo-elf-objcopy
PYTHON := python3
PKG_CONFIG := pkg-config
ROMTOOL := romtool.py
Z80_OBJCOPY := z80-neogeo-ihx-sdobjcopy

NG_CFLAGS := $(shell $(PKG_CONFIG) --cflags ngdevkit)
NG_LIBS := $(shell $(PKG_CONFIG) --libs ngdevkit)
NG_SHARE := $(shell $(PKG_CONFIG) --variable=sharedir ngdevkit)

CPPFLAGS := $(NG_CFLAGS) -I. -I$(ROOT)/codegen/lib
ARCH_FLAGS := -m68000
OPT_FLAGS ?= -Os
LTO_FLAGS ?= -flto -fwhole-program
WARN_FLAGS := \
	-Wall -Wextra \
	-Wno-unused-function -Wno-unused-label -Wno-unused-parameter \
	-Wno-unused-value -Wno-unused-variable
CFLAGS := \
	$(ARCH_FLAGS) $(OPT_FLAGS) $(LTO_FLAGS) \
	-std=gnu11 -fgnu89-inline -fomit-frame-pointer \
	-ffunction-sections -fdata-sections \
	-fno-asynchronous-unwind-tables -fno-unwind-tables \
	-MMD -MP $(WARN_FLAGS)
LDFLAGS := \
	$(ARCH_FLAGS) $(LTO_FLAGS) \
	-Wl,-Map,$(MAP)

LOCAL_SOURCES := main.c video.c ppu_direct.c apu_null.c
CORE_NAMES := instructions code data cpu
OBJECTS := \
	$(LOCAL_SOURCES:%.c=$(BUILD)/%.o) \
	$(CORE_NAMES:%=$(BUILD)/%.o)
DEPENDENCIES := $(OBJECTS:.o=.d)

PROM := $(ROMDIR)/$(GAMEROM)-p1.p1
CROM1 := $(ROMDIR)/$(GAMEROM)-c1.c1
CROM2 := $(ROMDIR)/$(GAMEROM)-c2.c2
SROM1 := $(ROMDIR)/$(GAMEROM)-s1.s1
MROM1 := $(ROMDIR)/$(GAMEROM)-m1.m1
VROM1 := $(ROMDIR)/$(GAMEROM)-v1.v1
CART := $(ROMDIR)/$(GAMEROM).zip
GNGEO_DATA := $(ROMDIR)/gngeo_data.zip

SMB_ROM ?=

.DEFAULT_GOAL := elf

.PHONY: all elf test verify assets cart run clean

all: verify

elf: $(TARGET)

$(BUILD):
	mkdir -p $@

$(ROMDIR) $(ASSETDIR):
	mkdir -p $@

$(BUILD)/%.o: %.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: $(ROOT)/codegen/lib/%.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJECTS)
	$(CC) -o $@ $(OBJECTS) $(LDFLAGS) $(NG_LIBS)

test:
	$(PYTHON) $(ROOT)/tools/test_gen_neogeo_assets.py

verify: test $(TARGET)
	$(PYTHON) $(ROOT)/tools/check_neogeo_elf.py $(TARGET)

assets: | $(ASSETDIR)
	@test -n "$(SMB_ROM)" || { \
		echo "SMB_ROM is required (a .nes or ZIP containing it)" >&2; \
		exit 2; \
	}
	$(PYTHON) $(ROOT)/tools/gen_neogeo_assets.py \
		--input "$(SMB_ROM)" \
		--output-dir "$(ASSETDIR)"

cart: $(TARGET) | $(ROMDIR) $(ASSETDIR)
	@test -n "$(SMB_ROM)" || { \
		echo "SMB_ROM is required (a .nes or ZIP containing it)" >&2; \
		exit 2; \
	}
	$(PYTHON) $(ROOT)/tools/gen_neogeo_assets.py \
		--input "$(SMB_ROM)" \
		--output-dir "$(ASSETDIR)"
	$(OBJCOPY) -O binary -S -R .text2 \
		--gap-fill 0xff --pad-to 1048576 \
		$(TARGET) $(PROM)
	dd if=$(PROM) of=$(PROM) conv=notrunc,swab status=none
	cp $(ASSETDIR)/$(GAMEROM)-c1.c1 $(CROM1)
	cp $(ASSETDIR)/$(GAMEROM)-c2.c2 $(CROM2)
	cp $(ASSETDIR)/$(GAMEROM)-s1.s1 $(SROM1)
	$(Z80_OBJCOPY) -I ihex -O binary \
		$(NG_SHARE)/nullsound_driver.ihx $(MROM1) \
		--pad-to 131072
	truncate -s 524288 $(VROM1)
	$(ROMTOOL) -b cartridge -f zip \
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
	ngdevkit-gngeo --screen320 --system home \
		-i $(ROMDIR) -d $(GNGEO_DATA) $(GAMEROM)

clean:
	rm -rf $(BUILD)

-include $(DEPENDENCIES)
