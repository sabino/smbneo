.DEFAULT_GOAL := test

.PHONY: \
	all ci test verify codegen cart run replay-cart replay-run \
	replay-rendered-evidence clean

all: test

ci: test

# ROM-free host regression suite. PKG_CONFIG=true prevents the cross-toolchain
# probe from being required for tests that compile and run on the host.
test:
	$(MAKE) -C platform/neogeo PKG_CONFIG=true test

# Full target verification requires MoonBit and the ngdevkit cross-toolchain.
verify:
	$(MAKE) -C platform/neogeo verify

codegen:
	moon run src/main

# Command-line variables such as SMB_ROM, REPLAY_FM2, and GNGEO are forwarded
# automatically by recursive Make.
cart run replay-cart replay-run replay-rendered-evidence clean:
	$(MAKE) -C platform/neogeo $@
