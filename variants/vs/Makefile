CC ?= cc
.DEFAULT_GOAL := test
BUILD ?= build
CFLAGS ?= -O2 -std=c99 -Wall -Wextra -Werror
# Probe the cross SDK only for target work; host tests remain SDK-independent.
ifneq ($(filter neogeo-%,$(MAKECMDGOALS)),)
include neogeo.mk
endif
.PHONY: test reference translate host native-test native-c-tests native-python-tests
$(BUILD)/test_tiles $(BUILD)/test_bus $(BUILD)/test_cpu $(BUILD)/test_machine $(BUILD)/test_program $(BUILD)/test_program_native $(BUILD)/test_native_rules $(BUILD)/test_native_platform $(BUILD)/test_native_fast_motion $(BUILD)/test_native_fast_actor $(BUILD)/test_native_fast_actor_policy $(BUILD)/test_native_fast_actor_loop $(BUILD)/test_native_fast_collision $(BUILD)/test_native_fast_meta $(BUILD)/test_native_fast_game $(BUILD)/test_native_fast_video $(BUILD)/test_native_fast_nmi $(BUILD)/test_native_audio_cadence: test_check.h

test: $(BUILD)/test_bus $(BUILD)/test_cpu $(BUILD)/test_tiles $(BUILD)/test_machine
	$(BUILD)/test_bus
	$(BUILD)/test_cpu
	$(BUILD)/test_tiles
	$(BUILD)/test_machine
	cd ../../tools && python3 -m unittest test_vs_pipeline test_vs_hardware_contract \
		test_vs_assets test_vs_stack_usage test_run_vs_mame \
		test_check_gngeo_driver test_check_vs_package

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

# CPU-free native checks.  These targets deliberately do not depend on a ROM,
# BIOS, cross SDK, or generated translated core, so they are safe on a host.
native-test: native-c-tests native-python-tests

native-c-tests: $(BUILD)/test_native_rules $(BUILD)/test_native_platform $(BUILD)/test_native_fast_motion $(BUILD)/test_native_fast_actor $(BUILD)/test_native_fast_actor_policy $(BUILD)/test_native_fast_actor_loop $(BUILD)/test_native_fast_collision $(BUILD)/test_native_fast_meta $(BUILD)/test_native_fast_game $(BUILD)/test_native_fast_video $(BUILD)/test_native_fast_nmi $(BUILD)/test_native_audio_cadence
	$(BUILD)/test_native_rules
	$(BUILD)/test_native_platform
	$(BUILD)/test_native_fast_motion
	$(BUILD)/test_native_fast_actor
	$(BUILD)/test_native_fast_actor_policy
	$(BUILD)/test_native_fast_actor_loop
	$(BUILD)/test_native_fast_collision
	$(BUILD)/test_native_fast_meta
	$(BUILD)/test_native_fast_game
	$(BUILD)/test_native_fast_video
	$(BUILD)/test_native_fast_nmi
	$(BUILD)/test_native_audio_cadence

native-python-tests:
	cd ../../tools && python3 -m unittest \
		test_native_codegen \
		test_gen_vs_native_profile test_check_neogeo_elf
	python3 ../../tools/check_native_codegen.py native/smbneo_native.c

$(BUILD)/test_native_rules: test_native_rules.c vs_native_rules.c vs_native_rules.h
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I. test_native_rules.c vs_native_rules.c -o $@

$(BUILD)/test_native_platform: test_native_platform.c vs_native_platform.c vs_native_platform.h vs_native_fast_video.c vs_native_fast_video.h native/smbneo_native.h vs_bus.c vs_bus.h
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I. test_native_platform.c vs_native_platform.c vs_native_fast_video.c vs_bus.c -o $@

$(BUILD)/test_native_fast_motion: test_native_fast_motion.c vs_native_fast_motion.c vs_native_fast_motion.h native/smbneo_native.h
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I. test_native_fast_motion.c vs_native_fast_motion.c -o $@

$(BUILD)/test_native_fast_actor: test_native_fast_actor.c vs_native_fast_actor.c vs_native_fast_actor.h native/smbneo_native.c native/smbneo_native.h
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -O0 -I. test_native_fast_actor.c \
		vs_native_fast_actor.c native/smbneo_native.c -o $@

$(BUILD)/test_native_fast_actor_policy: test_native_fast_actor_policy.c vs_native_fast_actor_policy.c vs_native_fast_actor_policy.h vs_native_fast_motion.c vs_native_fast_motion.h native/smbneo_native.c native/smbneo_native.h
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -O0 -I. test_native_fast_actor_policy.c \
		vs_native_fast_actor_policy.c vs_native_fast_motion.c \
		native/smbneo_native.c -o $@

$(BUILD)/test_native_fast_actor_loop: test_native_fast_actor_loop.c vs_native_fast_actor_loop.c vs_native_fast_actor_loop.h native/smbneo_native.c native/smbneo_native.h
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -O1 -I. test_native_fast_actor_loop.c \
		vs_native_fast_actor_loop.c native/smbneo_native.c -o $@

$(BUILD)/test_native_fast_collision: test_native_fast_collision.c vs_native_fast_collision.c vs_native_fast_collision.h native/smbneo_native.c native/smbneo_native.h
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -O0 -I. test_native_fast_collision.c \
		vs_native_fast_collision.c native/smbneo_native.c -o $@

$(BUILD)/test_native_fast_meta: test_native_fast_meta.c vs_native_fast_meta.c vs_native_fast_meta.h native/smbneo_native.c native/smbneo_native.h
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -O0 -I. test_native_fast_meta.c \
		vs_native_fast_meta.c native/smbneo_native.c -o $@

$(BUILD)/test_native_fast_game: test_native_fast_game.c vs_native_fast_game.c vs_native_fast_game.h native/smbneo_native.h
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I. test_native_fast_game.c vs_native_fast_game.c -o $@

$(BUILD)/test_native_fast_video: test_native_fast_video.c vs_native_fast_video.c vs_native_fast_video.h native/smbneo_native.h
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I. test_native_fast_video.c vs_native_fast_video.c -o $@

$(BUILD)/test_native_fast_nmi: test_native_fast_nmi.c vs_native_fast_nmi.c vs_native_fast_nmi.h native/smbneo_native.h
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I. test_native_fast_nmi.c vs_native_fast_nmi.c -o $@

$(BUILD)/test_native_audio_cadence: test_native_audio_cadence.c ../../platform/neogeo/audio_cadence.c ../../platform/neogeo/audio_cadence.h
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I../../platform/neogeo -I../../codegen/lib \
		test_native_audio_cadence.c \
		../../platform/neogeo/audio_cadence.c -o $@

# ROM/reference-dependent generation and differential execution remain
# explicit opt-in.  They are intentionally not prerequisites of native-test.
.PHONY: native-generate native-codegen-check native-reproducible native-differential
native-generate:
	@test -n "$(VS_PRG)" -a -n "$(VS_DEBUG)" -a -n "$(VS_SOURCE)" || \
		(echo "set VS_PRG, VS_DEBUG, and VS_SOURCE for native generation"; exit 2)
	python3 ../../tools/native_codegen.py --prg "$(VS_PRG)" \
		--debug "$(VS_DEBUG)" --source-root "$(VS_SOURCE)" \
		--output-dir "$(BUILD)/native-generated"

native-codegen-check: native-generate
	python3 ../../tools/check_native_codegen.py \
		"$(BUILD)/native-generated/smbneo_native.c"

native-reproducible: native-codegen-check
	cmp native/smbneo_native.h "$(BUILD)/native-generated/smbneo_native.h"
	cmp native/smbneo_native.c "$(BUILD)/native-generated/smbneo_native.c"
	cmp native/manifest.json "$(BUILD)/native-generated/manifest.json"

native-differential:
	@test -n "$(VS_ROM)" -a -n "$(VS_REFERENCE)" || \
		(echo "set VS_ROM and VS_REFERENCE for native differential testing"; exit 2)
	$(MAKE) reference translate VS_ROM="$(VS_ROM)" VS_REFERENCE="$(VS_REFERENCE)"
	$(MAKE) "$(BUILD)/vs_program.reference.o"
	$(MAKE) native-reproducible \
		VS_PRG="$(VS_REFERENCE)/vsmain.vs.bin" \
		VS_DEBUG="$(VS_REFERENCE)/vs.dbg" \
		VS_SOURCE="$(VS_REFERENCE)"
	$(CC) $(CFLAGS) -DSMBNEO_NATIVE_DIAGNOSTICS=1 \
		-DSMBNEO_NATIVE_FAST_PATHS=1 -I. -Inative \
		test_native_differential.c vs_machine.c vs_bus.c \
		vs_native_platform.c vs_native_fast_motion.c vs_native_fast_actor.c \
		vs_native_fast_actor_policy.c vs_native_fast_collision.c \
		vs_native_fast_actor_loop.c vs_native_fast_meta.c vs_native_fast_game.c \
		vs_native_fast_video.c vs_native_fast_nmi.c \
		native/smbneo_native.c \
		"$(BUILD)/vs_program.reference.o" \
		-o "$(BUILD)/test_native_differential"
	"$(BUILD)/test_native_differential" "$(BUILD)/assets/vs-reference.nes"
	$(CC) $(CFLAGS) -DSMBNEO_NATIVE_DIAGNOSTICS=1 \
		-DSMBNEO_NATIVE_FAST_PATHS=1 -I. -Inative \
		test_native_audio_differential.c vs_machine.c vs_bus.c \
		vs_native_platform.c vs_native_fast_motion.c vs_native_fast_actor.c \
		vs_native_fast_actor_policy.c vs_native_fast_collision.c \
		vs_native_fast_actor_loop.c vs_native_fast_meta.c vs_native_fast_game.c \
		vs_native_fast_video.c vs_native_fast_nmi.c \
		native/smbneo_native.c "$(BUILD)/vs_program.reference.o" \
		-o "$(BUILD)/test_native_audio_differential"
	"$(BUILD)/test_native_audio_differential" "$(BUILD)/assets/vs-reference.nes"
