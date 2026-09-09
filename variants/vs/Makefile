CC ?= cc
.DEFAULT_GOAL := test
BUILD ?= build
CFLAGS ?= -O2 -std=c99 -Wall -Wextra -Werror
# Probe the cross SDK only for target work; host tests remain SDK-independent.
ifneq ($(filter neogeo-%,$(MAKECMDGOALS)),)
include neogeo.mk
endif
.PHONY: test reference translate host
$(BUILD)/test_tiles $(BUILD)/test_bus $(BUILD)/test_cpu $(BUILD)/test_machine $(BUILD)/test_program $(BUILD)/test_program_native: test_check.h

test: $(BUILD)/test_bus $(BUILD)/test_cpu $(BUILD)/test_tiles $(BUILD)/test_machine
	$(BUILD)/test_bus
	$(BUILD)/test_cpu
	$(BUILD)/test_tiles
	$(BUILD)/test_machine
	cd ../../tools && python3 -m unittest test_vs_pipeline test_vs_hardware_contract test_vs_assets test_vs_stack_usage

$(BUILD)/test_tiles: test_tiles.c ../../platform/neogeo/oam_tiles.h ../../platform/neogeo/hud_update.h
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -DSMB_VS -I../../platform/neogeo test_tiles.c -o $@

$(BUILD)/test_bus: test_bus.c vs_bus.c vs_bus.h
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) test_bus.c vs_bus.c -o $@

$(BUILD)/test_cpu: test_cpu.c vs_cpu.h vs_bus.c vs_bus.h
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) test_cpu.c vs_bus.c -o $@

$(BUILD)/test_machine: test_machine.c vs_machine.c vs_machine.h vs_cpu.h vs_bus.c vs_bus.h
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -DSMB_VS test_machine.c vs_machine.c vs_bus.c -o $@

.PHONY: program-test
program-test: $(BUILD)/test_program
	$(BUILD)/test_program $(BUILD)/assets/vs-reference.nes

$(BUILD)/test_program: test_program.c vs_machine.c vs_machine.h vs_cpu.h vs_bus.c $(BUILD)/vs_program.o
	$(CC) $(CFLAGS) -I. test_program.c vs_machine.c vs_bus.c $(BUILD)/vs_program.o -o $@

.PHONY: program-test-native
program-test-native: $(BUILD)/test_program_native
	$(BUILD)/test_program_native $(BUILD)/assets/vs-reference.nes

.PHONY: program-compare
program-compare: $(BUILD)/test_program $(BUILD)/test_program_native
	python3 ../../tools/check_vs_trace.py $(BUILD)/test_program $(BUILD)/test_program_native $(BUILD)/assets/vs-reference.nes

.PHONY: kernel-compare
kernel-compare: $(BUILD)/test_fast_program
	$(BUILD)/test_fast_program $(BUILD)/assets/vs-reference.nes

$(BUILD)/vs_program.reference.o: $(BUILD)/vs_program.o
	objcopy --redefine-sym vs_program_run=vs_program_reference $< $@

$(BUILD)/test_fast_program: test_fast_program.c test_check.h vs_cpu.h vs_bus.h vs_bus.c $(BUILD)/vs_program.reference.o $(BUILD)/vs_program.native.o
	$(CC) $(CFLAGS) -I. test_fast_program.c vs_bus.c $(BUILD)/vs_program.reference.o $(BUILD)/vs_program.native.o -o $@

$(BUILD)/vs_program.native.o: $(BUILD)/vs_program.c vs_cpu.h vs_bus.h vs_fast_paths.h
	$(CC) $(CFLAGS) -DSMB_VS -Wno-unused-label -I. -c $< -o $@

$(BUILD)/test_program_native: test_program.c vs_machine.c vs_machine.h vs_cpu.h vs_bus.c $(BUILD)/vs_program.native.o
	$(CC) $(CFLAGS) -DSMB_VS -I. test_program.c vs_machine.c vs_bus.c $(BUILD)/vs_program.native.o -o $@

reference:
	python3 ../../tools/prepare_vs_reference.py --input "$(VS_ROM)" --output-dir "$(BUILD)/assets"

translate:
	python3 ../../tools/translate_vs.py --input "$(VS_ROM)" --debug "$(VS_REFERENCE)/vs.dbg" --reference-prg "$(VS_REFERENCE)/vsmain.vs.bin" --source-root "$(VS_REFERENCE)" --output "$(BUILD)/vs_program.c"

host: $(BUILD)/vs_host

$(BUILD)/vs_program.o: $(BUILD)/vs_program.c vs_cpu.h vs_bus.h vs_fast_paths.h
	$(CC) $(CFLAGS) -I. -c $< -o $@

$(BUILD)/vs_host: host.c vs_machine.c vs_machine.h vs_bus.c vs_bus.h vs_cpu.h $(BUILD)/vs_program.o
	$(CC) $(CFLAGS) -DVS_HOST_SDL $$(pkg-config --cflags sdl2) -I. host.c vs_machine.c vs_bus.c $(BUILD)/vs_program.o $$(pkg-config --libs sdl2) -o $@
