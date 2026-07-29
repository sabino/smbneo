#include "background_chain.h"

#include <stdint.h>
#include <stdio.h>

enum {
    BACKGROUND_STRIPS = 33,
    NO_STRIP = 0xff,
};

static int failures;

static uint8_t expected_sticky(uint8_t origin, uint8_t strip) {
    return (uint8_t)(strip != 0u && strip != origin);
}

static void check_plan(uint8_t valid, uint8_t previous, uint8_t next) {
    NeoGeoBackgroundChainPlan plan = neogeo_background_chain_plan(
        valid,
        previous,
        next
    );
    uint8_t state[BACKGROUND_STRIPS];
    uint8_t writes = 0u;
    uint8_t strip;

    for (strip = 0u; strip < BACKGROUND_STRIPS; ++strip) {
        state[strip] = valid != 0u
            ? expected_sticky(previous, strip)
            : (uint8_t)(0xa5u ^ strip);
    }

    if (plan.full_rebuild != 0u) {
        for (strip = 0u; strip < BACKGROUND_STRIPS; ++strip) {
            state[strip] = expected_sticky(next, strip);
            ++writes;
        }
    } else {
        if (plan.clear_root != NO_STRIP) {
            if (plan.clear_root == 0u || plan.clear_root >= BACKGROUND_STRIPS) {
                ++failures;
            } else {
                state[plan.clear_root] = 0u;
                ++writes;
            }
        }
        if (plan.set_sticky != NO_STRIP) {
            if (plan.set_sticky == 0u ||
                plan.set_sticky >= BACKGROUND_STRIPS) {
                ++failures;
            } else {
                state[plan.set_sticky] = 1u;
                ++writes;
            }
        }
    }

    for (strip = 0u; strip < BACKGROUND_STRIPS; ++strip) {
        if (state[strip] != expected_sticky(next, strip)) {
            fprintf(
                stderr,
                "state mismatch valid=%u previous=%u next=%u strip=%u\n",
                valid,
                previous,
                next,
                strip
            );
            ++failures;
        }
    }

    if (valid == 0u) {
        if (plan.full_rebuild == 0u || writes != BACKGROUND_STRIPS) {
            ++failures;
        }
    } else {
        uint8_t expected_writes = (uint8_t)(
            (previous != next && next != 0u ? 1u : 0u) +
            (previous != next && previous != 0u ? 1u : 0u)
        );

        if (plan.full_rebuild != 0u || writes != expected_writes) {
            ++failures;
        }
    }
}

int main(void) {
    uint8_t valid;
    uint8_t previous;
    uint8_t next;

    for (valid = 0u; valid <= 1u; ++valid) {
        for (previous = 0u; previous < BACKGROUND_STRIPS; ++previous) {
            for (next = 0u; next < BACKGROUND_STRIPS; ++next) {
                check_plan(valid, previous, next);
            }
        }
    }

    if (failures != 0) {
        fprintf(stderr, "%d background-chain failures\n", failures);
        return 1;
    }
    puts("background-chain tests passed");
    return 0;
}
