CC = clang

CFLAGS = -g -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function -Wno-unused-value -Wconversion -Wsign-conversion -Wno-missing-braces
CFLAGS += -I./raylib-quickstart/build/external/raylib-master/src
CFLAGS += -O3 -std=c99
CFLAGS += -ferror-limit=0

# Platform-specific flags
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    RAYLIB_FLAGS = -lGL -lm -lpthread -ldl -lrt -lX11
else ifeq ($(UNAME_S),Darwin) # macOS
    RAYLIB_FLAGS = -framework OpenGL -framework Cocoa -framework IOKit -framework CoreFoundation -lm -lpthread
endif

OBJECTS = raylib-quickstart/bin/Debug/libraylib.a

SOURCES += codegen/lib/instructions.c
SOURCES += codegen/lib/code.c
SOURCES += codegen/lib/data.c
SOURCES += codegen/lib/cpu.c
SOURCES += codegen/lib/ppu.c
SOURCES += codegen/lib/apu.c
SOURCES += codegen/lib/state.c
MAIN = codegen/main.c

.PHONY: codegen build wasm hash test ci clean

codegen:
	moon run src/main

build: clean
	$(CC) $(CFLAGS) -o smb ${MAIN} $(SOURCES) codegen/lib/common.c $(RAYLIB_FLAGS) $(OBJECTS)

wasm: clean
	$(CC) -O3 --target=wasm32 -nostdlib -Wl,--import-memory -Wl,--export-all -Wl,--no-entry -Wl,--allow-undefined -o web/smb.wasm $(SOURCES)

hash: clean
	$(CC) $(CFLAGS) -o hash codegen/hash.c $(SOURCES) codegen/lib/rec.c codegen/lib/common.c

test:
	$(MAKE) -C platform/neogeo PKG_CONFIG=true test

ci: test wasm
	test -s web/smb.wasm

clean:
	rm -f smb hash smb.wasm web/smb.wasm
