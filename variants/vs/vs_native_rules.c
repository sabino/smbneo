#include "vs_native_rules.h"
#include <string.h>

/* MAME's displayed DSW0 values are intentionally non-monotonic.  The
 * reference's compact tables are indexed by the decoded VS coinage selector;
 * this table maps the public DSW value to the equivalent pair.  Source:
 * vs-reference/src/lib/vs.s:sub_93E7_data_0/1 and vsnes.cpp:1984-2009. */
typedef struct {
    uint8_t coins;
    uint8_t awards;
    uint8_t free_play;
} Coinage;

static const Coinage coinage_table[8] = {
    {1, 1, 0}, /* 0x00: 1C/1C */
    {1, 3, 0}, /* 0x01: 1C/3C */
    {3, 1, 0}, /* 0x02: 3C/1C */
    {1, 5, 0}, /* 0x03: 1C/5C */
    {2, 1, 0}, /* 0x04: 2C/1C */
    {1, 4, 0}, /* 0x05: 1C/4C */
    {1, 2, 0}, /* 0x06: 1C/2C */
    {0, 0, 1}  /* 0x07: Free Play */
};

static uint8_t clamp_credits(unsigned value) {
    return value > VS_NATIVE_RULES_CREDIT_LIMIT
        ? VS_NATIVE_RULES_CREDIT_LIMIT : (uint8_t)value;
}

VsNativeRules vs_native_rules_for_dips(uint8_t dips, VsNativeRegion region) {
    VsNativeRules rules;
    const Coinage coinage = coinage_table[dips & 7u];
    const uint8_t lives_bit = (uint8_t)((dips >> 3) & 1u);
    /* MAME's displayed bonus order is 00=100, 20=150, 10=200, 30=250. */
    const uint8_t bonus = (uint8_t)((dips & 0x30u) == 0x20u ? 1u :
                                    (dips & 0x30u) == 0x10u ? 2u :
                                    (dips & 0x30u) == 0x30u ? 3u : 0u);
    const uint8_t timer_fast = (uint8_t)((dips >> 6) & 1u);
    const uint8_t continue_three = (uint8_t)((dips >> 7) & 1u);

    memset(&rules, 0, sizeof(rules));
    rules.dip_switches = dips;
    rules.coinage = (uint8_t)(dips & 7u);
    rules.coins_required = coinage.coins;
    rules.credits_awarded = coinage.awards;
    rules.free_play = coinage.free_play;

    /* Source stores lives one below the cabinet display count. */
    rules.starting_lives_display = lives_bit ? 2u : 3u;
    rules.starting_lives = (uint8_t)(rules.starting_lives_display - 1u);
    rules.continue_lives_display = continue_three ? 3u : 4u;
    rules.continue_lives = (uint8_t)(rules.continue_lives_display - 1u);
    rules.bonus_life_coins = (uint16_t)(100u + bonus * 50u);

    /* Source: include/const.i:TIMERS_ALL_FRAME_COUNT/GAME_TIMER_CTL and
     * src/game/timer.s VS branch (reload 17/20 for fast/slow). */
    rules.nmi_timer_period_frames = region == VS_NATIVE_REGION_PAL ? 17u : 20u;
    rules.game_timer_control = region == VS_NATIVE_REGION_PAL ? 20u : 24u;
    rules.timer_reload_fast = 17u;
    rules.timer_reload_slow = 20u;
    rules.timer_reload = timer_fast ? rules.timer_reload_fast
                                    : rules.timer_reload_slow;
    rules.title_demo_ticks = 0x18u;   /* TITLE_DEMO_TIMER */
    rules.title_select_ticks = 0x10u; /* TITLE_SELECT_TIMER */
    return rules;
}

void vs_native_rules_init(VsNativeRulesState *state,
                          const VsNativeRules *rules) {
    memset(state, 0, sizeof(*state));
    state->rules = *rules;
    state->mode = VS_NATIVE_RULES_TITLE;
    state->players = 1u;
    state->lives[0] = rules->starting_lives;
    state->lives[1] = rules->starting_lives;
    state->title_timer = rules->title_demo_ticks;
    state->select_timer = rules->title_select_ticks;
    if (rules->free_play) state->credits = VS_NATIVE_RULES_FREE_PLAY_CREDIT;
}

static void add_credit(VsNativeRulesState *state, uint8_t amount) {
    state->credits = clamp_credits((unsigned)state->credits + amount);
    if (state->mode == VS_NATIVE_RULES_TITLE ||
        state->mode == VS_NATIVE_RULES_ATTRACT)
        state->mode = VS_NATIVE_RULES_CREDITS;
}

static void accept_coin(VsNativeRulesState *state, unsigned slot) {
    if (state->coin_total[slot] != UINT16_MAX)
        ++state->coin_total[slot];
    if (state->rules.free_play) return;
    if (state->coin_progress[slot] != UINT8_MAX)
        ++state->coin_progress[slot];
    if (state->coin_progress[slot] >= state->rules.coins_required) {
        state->coin_progress[slot] = 0;
        add_credit(state, state->rules.credits_awarded);
    }
}

void vs_native_rules_frame(VsNativeRulesState *state, uint8_t p1,
                           uint8_t p2, uint8_t coin_service) {
    const uint8_t coins = (uint8_t)(coin_service &
        (VS_NATIVE_RULES_COIN_1 | VS_NATIVE_RULES_COIN_2));
    const uint8_t coin_edges = (uint8_t)(coins &
        (uint8_t)~state->previous_coin_mask);
    const uint8_t p1_start = (uint8_t)((p1 & 0x04u) != 0u &&
                                       state->previous_start_p1 == 0u);
    const uint8_t p2_start = (uint8_t)((p2 & 0x04u) != 0u &&
                                       state->previous_start_p2 == 0u);

    if (coin_edges & VS_NATIVE_RULES_COIN_1) accept_coin(state, 0u);
    if (coin_edges & VS_NATIVE_RULES_COIN_2) accept_coin(state, 1u);
    if ((coin_service & VS_NATIVE_RULES_SERVICE) != 0u &&
        (state->previous_coin_mask & VS_NATIVE_RULES_SERVICE) == 0u)
        add_credit(state, 1u);

    state->previous_coin_mask = (uint8_t)(coins |
        (coin_service & VS_NATIVE_RULES_SERVICE));
    state->previous_start_p1 = (uint8_t)((p1 & 0x04u) != 0u);
    state->previous_start_p2 = (uint8_t)((p2 & 0x04u) != 0u);

    if (state->mode == VS_NATIVE_RULES_TITLE && state->title_timer != 0u) {
        --state->title_timer;
        if (state->title_timer == 0u) state->mode = VS_NATIVE_RULES_ATTRACT;
    }
    if (p2_start)
        (void)vs_native_rules_start(state, 2u);
    else if (p1_start)
        (void)vs_native_rules_start(state, 1u);
}

uint8_t vs_native_rules_can_start(const VsNativeRulesState *state,
                                  uint8_t players) {
    if (players < 1u || players > 2u) return 0u;
    if (state->rules.free_play) return 1u;
    return state->credits >= (players == 2u ? 2u : 1u);
}

uint8_t vs_native_rules_start(VsNativeRulesState *state, uint8_t players) {
    const uint8_t cost = players == 2u ? 2u : 1u;
    if (players < 1u || players > 2u ||
        (!state->rules.free_play && state->credits < cost)) return 0u;
    if (!state->rules.free_play) state->credits = (uint8_t)(state->credits - cost);
    state->players = players;
    state->lives[0] = state->rules.starting_lives;
    state->lives[1] = state->rules.starting_lives;
    state->mode = VS_NATIVE_RULES_GAME;
    state->title_timer = state->rules.title_demo_ticks;
    return 1u;
}

void vs_native_rules_enter_game_over(VsNativeRulesState *state) {
    state->mode = VS_NATIVE_RULES_GAME_OVER;
}

uint8_t vs_native_rules_continue(VsNativeRulesState *state) {
    if (state->mode != VS_NATIVE_RULES_GAME_OVER ||
        (!state->rules.free_play && state->credits == 0u)) return 0u;
    if (!state->rules.free_play) --state->credits;
    state->lives[0] = state->rules.continue_lives;
    state->lives[1] = state->rules.continue_lives;
    state->mode = VS_NATIVE_RULES_CONTINUE;
    return 1u;
}
