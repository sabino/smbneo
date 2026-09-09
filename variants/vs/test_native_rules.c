#include "vs_native_rules.h"
#include <assert.h>
#include <stdio.h>

static void pulse(VsNativeRulesState *s, uint8_t mask) {
    vs_native_rules_frame(s, 0, 0, mask);
    vs_native_rules_frame(s, 0, 0, 0);
}

static void test_coinage(void) {
    static const uint8_t expected_after_three[8] = {3, 9, 1, 15, 1, 12, 6, 32};
    for (unsigned d = 0; d < 8; ++d) {
        VsNativeRules rules = vs_native_rules_for_dips((uint8_t)d,
                                                       VS_NATIVE_REGION_NTSC);
        VsNativeRulesState state;
        vs_native_rules_init(&state, &rules);
        assert(rules.coinage == d);
        if (d == 7) {
            assert(rules.free_play && state.credits == 32);
            pulse(&state, VS_NATIVE_RULES_COIN_1);
        } else {
            for (unsigned i = 0; i < 3; ++i) pulse(&state, VS_NATIVE_RULES_COIN_1);
        }
        assert(state.credits == expected_after_three[d]);
    }
}

static void test_dips_and_timing(void) {
    VsNativeRules r = vs_native_rules_for_dips(0x10, VS_NATIVE_REGION_NTSC);
    assert(r.starting_lives_display == 3 && r.starting_lives == 2);
    assert(r.continue_lives_display == 4 && r.continue_lives == 3);
    assert(r.bonus_life_coins == 200 && r.timer_reload == 20);
    assert(r.nmi_timer_period_frames == 20 && r.game_timer_control == 24);
    assert(r.title_demo_ticks == 24 && r.title_select_ticks == 16);
    r = vs_native_rules_for_dips(0xc8, VS_NATIVE_REGION_PAL);
    assert(r.starting_lives_display == 2 && r.continue_lives_display == 3);
    assert(r.bonus_life_coins == 100 && r.timer_reload == 17);
    assert(r.nmi_timer_period_frames == 17 && r.game_timer_control == 20);
}

static void test_all_dips(void) {
    /* MAME suprmrio DSW0 values, cross-checked with the reference's
     * vs_read_dips field layout and game_init_setup/sub_A01B branches. */
    static const uint8_t coin_required[8] = {1, 1, 3, 1, 2, 1, 1, 0};
    static const uint8_t credit_award[8] = {1, 3, 1, 5, 1, 4, 2, 0};
    static const uint8_t is_free_play[8] = {0, 0, 0, 0, 0, 0, 0, 1};
    static const uint8_t bonus_coins[4] = {100, 200, 150, 250};
    for (unsigned d = 0; d < 256; ++d) {
        VsNativeRules n = vs_native_rules_for_dips((uint8_t)d,
                                                   VS_NATIVE_REGION_NTSC);
        VsNativeRules p = vs_native_rules_for_dips((uint8_t)d,
                                                   VS_NATIVE_REGION_PAL);
        unsigned coin = d & 7u;
        unsigned bonus = d & 0x30u;
        unsigned bonus_index = bonus == 0x10u ? 1u :
                               bonus == 0x20u ? 2u :
                               bonus == 0x30u ? 3u : 0u;
        assert(n.dip_switches == d && n.coinage == coin);
        assert(n.coins_required == coin_required[coin]);
        assert(n.credits_awarded == credit_award[coin]);
        assert(n.free_play == is_free_play[coin]);
        assert(n.starting_lives_display == ((d & 8u) ? 2u : 3u));
        assert(n.starting_lives == n.starting_lives_display - 1u);
        assert(n.continue_lives_display == ((d & 0x80u) ? 3u : 4u));
        assert(n.continue_lives == n.continue_lives_display - 1u);
        assert(n.bonus_life_coins == bonus_coins[bonus_index]);
        assert(n.timer_reload == ((d & 0x40u) ? 17u : 20u));
        assert(n.nmi_timer_period_frames == 20u && n.game_timer_control == 24u);
        assert(p.nmi_timer_period_frames == 17u && p.game_timer_control == 20u);
        assert(p.timer_reload == n.timer_reload);
    }
}

static void test_edges_start_and_continue(void) {
    VsNativeRulesState s;
    VsNativeRules r = vs_native_rules_for_dips(0, VS_NATIVE_REGION_NTSC);
    vs_native_rules_init(&s, &r);
    vs_native_rules_frame(&s, 0, 0, VS_NATIVE_RULES_COIN_1 | VS_NATIVE_RULES_COIN_2);
    vs_native_rules_frame(&s, 0, 0, VS_NATIVE_RULES_COIN_1 | VS_NATIVE_RULES_COIN_2);
    assert(s.coin_total[0] == 1 && s.coin_total[1] == 1 && s.credits == 2);
    vs_native_rules_frame(&s, 0x04, 0, 0);
    assert(s.mode == VS_NATIVE_RULES_GAME && s.credits == 1);
    vs_native_rules_frame(&s, 0x04, 0, 0);
    assert(s.credits == 1);
    vs_native_rules_enter_game_over(&s);
    assert(vs_native_rules_continue(&s)); /* one credit buys a continue */
    assert(s.mode == VS_NATIVE_RULES_CONTINUE && s.credits == 0);
    assert(s.lives[0] == r.continue_lives);
}

static void test_overlapping_edges(void) {
    VsNativeRules r = vs_native_rules_for_dips(0, VS_NATIVE_REGION_NTSC);
    VsNativeRulesState s;
    vs_native_rules_init(&s, &r);

    /* Both physical coin slots on the same edge each count once. */
    vs_native_rules_frame(&s, 0x04, 0x04,
                          VS_NATIVE_RULES_COIN_1 | VS_NATIVE_RULES_COIN_2);
    assert(s.coin_total[0] == 1 && s.coin_total[1] == 1);
    assert(s.mode == VS_NATIVE_RULES_GAME && s.players == 2 && s.credits == 0);

    /* A held P1 start cannot suppress a later P2 edge. */
    vs_native_rules_init(&s, &r);
    vs_native_rules_frame(&s, 0x04, 0, 0);
    vs_native_rules_frame(&s, 0x04, 0,
                          VS_NATIVE_RULES_COIN_1 | VS_NATIVE_RULES_COIN_2);
    assert(s.mode == VS_NATIVE_RULES_CREDITS && s.credits == 2);
    vs_native_rules_frame(&s, 0x04, 0x04, 0);
    assert(s.mode == VS_NATIVE_RULES_GAME && s.players == 2 && s.credits == 0);

    /* Conversely, a held P2 start cannot suppress a new P1 edge. */
    vs_native_rules_init(&s, &r);
    vs_native_rules_frame(&s, 0, 0x04, 0);
    vs_native_rules_frame(&s, 0, 0x04, VS_NATIVE_RULES_COIN_1);
    assert(s.mode == VS_NATIVE_RULES_CREDITS && s.credits == 1);
    vs_native_rules_frame(&s, 0x04, 0x04, 0);
    assert(s.mode == VS_NATIVE_RULES_GAME && s.players == 1 && s.credits == 0);

    /* Holding either coin line and service does not repeat events. */
    vs_native_rules_init(&s, &r);
    vs_native_rules_frame(&s, 0, 0,
                          VS_NATIVE_RULES_COIN_1 | VS_NATIVE_RULES_SERVICE);
    vs_native_rules_frame(&s, 0, 0,
                          VS_NATIVE_RULES_COIN_1 | VS_NATIVE_RULES_SERVICE);
    assert(s.coin_total[0] == 1 && s.credits == 2);
}

static void test_title_attract(void) {
    VsNativeRulesState s;
    VsNativeRules r = vs_native_rules_for_dips(0, VS_NATIVE_REGION_NTSC);
    vs_native_rules_init(&s, &r);
    for (unsigned i = 0; i < 23; ++i) vs_native_rules_frame(&s, 0, 0, 0);
    assert(s.mode == VS_NATIVE_RULES_TITLE);
    vs_native_rules_frame(&s, 0, 0, 0);
    assert(s.mode == VS_NATIVE_RULES_ATTRACT);
    pulse(&s, VS_NATIVE_RULES_COIN_1);
    assert(s.mode == VS_NATIVE_RULES_CREDITS && s.credits == 1);
}

int main(void) {
    test_coinage();
    test_dips_and_timing();
    test_all_dips();
    test_edges_start_and_continue();
    test_overlapping_edges();
    test_title_attract();
    puts("native VS rules: coinage, DIPs, timing, edges, start/continue, title/attract passed");
    return 0;
}
