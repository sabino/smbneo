CC ?= cc
PYTHON ?= python3
BUILD ?= build
OUT ?= $(BUILD)/generated
CFLAGS ?= -std=c99 -Wall -Wextra -Werror

.PHONY: test generate check host

test:
	PYTHONPATH=../../../tools $(PYTHON) -m unittest discover -s ../../../tools -p 'test_native_codegen.py' -v

generate:
	@test -n "$(VS_PRG)" -a -n "$(VS_DEBUG)" -a -n "$(VS_SOURCE)" || \
		(echo "set VS_PRG, VS_DEBUG, and VS_SOURCE for a local generation"; exit 2)
	$(PYTHON) ../../../tools/native_codegen.py --prg "$(VS_PRG)" \
		--debug "$(VS_DEBUG)" --source-root "$(VS_SOURCE)" --output-dir "$(OUT)"

check: generate
	$(PYTHON) ../../../tools/check_native_codegen.py "$(OUT)/smbneo_native.c"

host: check
	$(CC) $(CFLAGS) -DSMBNEO_NATIVE_DIAGNOSTICS=1 -I"$(OUT)" \
		"$(OUT)/smbneo_native.c" host_test.c -o "$(OUT)/native_host_test"
	"$(OUT)/native_host_test"
