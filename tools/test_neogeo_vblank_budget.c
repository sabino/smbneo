#include "vblank_budget.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

int main(void) {
    uint32_t palette_words;
    uint32_t hud_entries;
    uint32_t scb_operations;

    assert(NEO_VBLANK_PALETTE_MAX_WORDS == 51u);
    assert(NEO_VBLANK_HUD_MAX_ENTRIES <= 32u);
    assert(NEO_VBLANK_SHARED_HUD_MAX_ENTRIES <= 8u);
    assert(NEO_VBLANK_SHARED_PALETTE_MAX_WORDS <= 16u);
    assert(NEO_VBLANK_SCB_SWAP_MAX_OPERATIONS <= 132u);
    assert(NEO_VBLANK_PHASE_SAFETY_MARGIN_CYCLES >= 1024u);

    assert(neogeo_vblank_palette_chunk(0u) == 0u);
    assert(neogeo_vblank_palette_chunk(51u) == 51u);
    assert(neogeo_vblank_palette_chunk(UINT16_MAX) == 51u);
    assert(neogeo_vblank_hud_chunk(32u) == 32u);
    assert(neogeo_vblank_hud_chunk(UINT16_MAX) == 32u);
    assert(neogeo_vblank_scb_swap_chunk(132u) == 132u);
    assert(neogeo_vblank_scb_swap_chunk(UINT16_MAX) == 132u);

    /* Only a small modeled live delta may share the atomic SCB3 phase. */
    assert(
        neogeo_vblank_choose_commit_phase(0u, 0u, 0u) ==
        NEO_VBLANK_COMMIT_NONE
    );
    assert(
        neogeo_vblank_choose_commit_phase(0u, 1u, 0u) ==
        NEO_VBLANK_COMMIT_LIVE_UPDATES
    );
    assert(
        neogeo_vblank_choose_commit_phase(1u, 0u, 0u) ==
        NEO_VBLANK_COMMIT_SCB_SWAP
    );
    assert(
        neogeo_vblank_choose_commit_phase(1u, 1u, 1u) ==
        NEO_VBLANK_COMMIT_SHARED_LIVE_AND_SCB
    );
    assert(
        neogeo_vblank_choose_commit_phase(1u, 0u, 1u) ==
        NEO_VBLANK_COMMIT_SHARED_LIVE_AND_SCB
    );
    assert(
        neogeo_vblank_choose_commit_phase(1u, 0u, 8u) ==
        NEO_VBLANK_COMMIT_SHARED_LIVE_AND_SCB
    );
    assert(
        neogeo_vblank_choose_commit_phase(1u, 0u, 9u) ==
        NEO_VBLANK_COMMIT_SCB_SWAP
    );
    assert(
        neogeo_vblank_choose_commit_phase(1u, 1u, 0u) ==
        NEO_VBLANK_COMMIT_SHARED_LIVE_AND_SCB
    );
    assert(
        neogeo_vblank_choose_commit_phase(1u, 16u, 0u) ==
        NEO_VBLANK_COMMIT_SHARED_LIVE_AND_SCB
    );
    assert(
        neogeo_vblank_choose_commit_phase(1u, 17u, 0u) ==
        NEO_VBLANK_COMMIT_SCB_SWAP
    );
    assert(
        neogeo_vblank_choose_commit_phase(1u, 2u, 1u) ==
        NEO_VBLANK_COMMIT_SHARED_LIVE_AND_SCB
    );
    assert(
        neogeo_vblank_choose_commit_phase(1u, 16u, 9u) ==
        NEO_VBLANK_COMMIT_SCB_SWAP
    );
    assert(
        neogeo_vblank_choose_commit_phase(0u, 0u, 1u) ==
        NEO_VBLANK_COMMIT_LIVE_UPDATES
    );

    assert(
        neogeo_vblank_live_phase_cycles(
            NEO_VBLANK_PALETTE_MAX_WORDS,
            NEO_VBLANK_HUD_MAX_ENTRIES
        ) + NEO_VBLANK_PHASE_SAFETY_MARGIN_CYCLES <=
            NEO_VBLANK_PHASE_BUDGET_CYCLES
    );
    assert(
        neogeo_vblank_scb_phase_cycles(
            NEO_VBLANK_SCB_SWAP_MAX_OPERATIONS
        ) + NEO_VBLANK_PHASE_SAFETY_MARGIN_CYCLES <=
            NEO_VBLANK_PHASE_BUDGET_CYCLES
    );
    assert(
        neogeo_vblank_shared_live_phase_cycles(
            0u,
            NEO_VBLANK_SHARED_HUD_MAX_ENTRIES,
            NEO_VBLANK_SCB_SWAP_MAX_OPERATIONS
        ) + NEO_VBLANK_PHASE_SAFETY_MARGIN_CYCLES <=
            NEO_VBLANK_PHASE_BUDGET_CYCLES
    );
    assert(neogeo_vblank_shared_live_fits(2u, 1u) != 0u);
    assert(neogeo_vblank_shared_live_fits(16u, 0u) != 0u);
    assert(neogeo_vblank_shared_live_fits(16u, 1u) == 0u);

    /* Clamping keeps every possible caller-supplied count within budget. */
    for (palette_words = 0; palette_words <= UINT16_MAX; ++palette_words) {
        for (hud_entries = 0; hud_entries <= 32u; ++hud_entries) {
            assert(
                neogeo_vblank_live_phase_cycles(
                    (uint16_t)palette_words,
                    (uint16_t)hud_entries
                ) + NEO_VBLANK_PHASE_SAFETY_MARGIN_CYCLES <=
                    NEO_VBLANK_PHASE_BUDGET_CYCLES
            );
        }
    }
    for (scb_operations = 0; scb_operations <= UINT16_MAX; ++scb_operations) {
        assert(
            neogeo_vblank_scb_phase_cycles((uint16_t)scb_operations) +
                NEO_VBLANK_PHASE_SAFETY_MARGIN_CYCLES <=
                NEO_VBLANK_PHASE_BUDGET_CYCLES
        );
    }
    for (palette_words = 0; palette_words <= 51u; ++palette_words) {
        for (hud_entries = 0; hud_entries <= 32u; ++hud_entries) {
            NeoVblankCommitPhase phase = neogeo_vblank_choose_commit_phase(
                1u,
                (uint16_t)palette_words,
                (uint16_t)hud_entries
            );

            if (palette_words == 0u && hud_entries == 0u) {
                assert(phase == NEO_VBLANK_COMMIT_SCB_SWAP);
            } else if (
                neogeo_vblank_shared_live_fits(
                    (uint16_t)palette_words,
                    (uint16_t)hud_entries
                ) != 0u
            ) {
                assert(phase == NEO_VBLANK_COMMIT_SHARED_LIVE_AND_SCB);
                assert(
                    neogeo_vblank_shared_live_phase_cycles(
                        (uint16_t)palette_words,
                        (uint16_t)hud_entries,
                        NEO_VBLANK_SCB_SWAP_MAX_OPERATIONS
                    ) + NEO_VBLANK_PHASE_SAFETY_MARGIN_CYCLES <=
                        NEO_VBLANK_PHASE_BUDGET_CYCLES
                );
            } else {
                assert(phase == NEO_VBLANK_COMMIT_SCB_SWAP);
            }
        }
    }

    puts("Neo Geo VBlank-budget tests: OK");
    return 0;
}
