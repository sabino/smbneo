#ifndef SMBNEO_VS_NATIVE_RULES_H
#define SMBNEO_VS_NATIVE_RULES_H

/*
 * Small, CPU-independent model of the Vs. Super Mario Bros. cabinet rules.
 *
 * The tables below are derived from the private reference source, not copied
 * from it: src/lib/vs.s (sub_93E7_data_0/1, sub_9389, sub_9362),
 * src/title/init.s (title_init_1), src/game/init.s (game_init_setup),
 * src/game/vs_game_over.s (sub_A01B), src/game/timer.s, and include/const.i.
 * The MAME DIP names/encodings are cross-checked against vsnes.cpp's
 * INPUT_PORTS_START(suprmrio).  This header contains only compact rule data.
 */
#include <stdint.h>

#define VS_NATIVE_RULES_COIN_1 UINT8_C(0x20)
#define VS_NATIVE_RULES_COIN_2 UINT8_C(0x40)
#define VS_NATIVE_RULES_SERVICE UINT8_C(0x04)
#define VS_NATIVE_RULES_CREDIT_LIMIT UINT8_C(0x5c)
#define VS_NATIVE_RULES_FREE_PLAY_CREDIT UINT8_C(0x20)

typedef enum {
    VS_NATIVE_REGION_NTSC = 0,
    VS_NATIVE_REGION_PAL = 1
} VsNativeRegion;

typedef enum {
    VS_NATIVE_RULES_TITLE = 0,
    VS_NATIVE_RULES_CREDITS = 1,
    VS_NATIVE_RULES_ATTRACT = 2,
    VS_NATIVE_RULES_GAME = 3,
    VS_NATIVE_RULES_GAME_OVER = 4,
    VS_NATIVE_RULES_CONTINUE = 5
} VsNativeRulesMode;

typedef struct {
    uint8_t dip_switches;
    uint8_t coinage;
    uint8_t coins_required;
    uint8_t credits_awarded;
    uint8_t free_play;

    /* These are the stored values used by player_lives (displayed lives - 1). */
    uint8_t starting_lives;
    uint8_t starting_lives_display;
    uint8_t continue_lives;
    uint8_t continue_lives_display;
    uint16_t bonus_life_coins;

    /* VS timer.s has a fast/slow reload distinct from const.i's generic
     * GAME_TIMER_CTL.  Keep both values explicit for native callers. */
    uint8_t timer_reload;
    uint8_t timer_reload_fast;
    uint8_t timer_reload_slow;
    uint8_t nmi_timer_period_frames;
    uint8_t game_timer_control;

    uint8_t title_demo_ticks;
    uint8_t title_select_ticks;
} VsNativeRules;

typedef struct {
    VsNativeRules rules;
    uint8_t mode;
    uint8_t credits;
    uint8_t coin_progress[2];
    uint8_t previous_coin_mask;
    /* Start edges are per player.  A held P1 start must not mask a new P2
     * start on the following frame (the cabinet has two independent inputs). */
    uint8_t previous_start_p1;
    uint8_t previous_start_p2;
    uint8_t players;
    uint8_t lives[2];
    uint16_t coin_total[2];
    uint8_t title_timer;
    uint8_t select_timer;
    uint8_t timer_divider;
} VsNativeRulesState;

/* dips is the decoded MAME/VS DSW0 byte, not the raw active-low port value. */
VsNativeRules vs_native_rules_for_dips(uint8_t dips, VsNativeRegion region);

void vs_native_rules_init(VsNativeRulesState *state,
                          const VsNativeRules *rules);

/* One semantic frame. Inputs use normalized active-high bits. Coin inputs are
 * edge-triggered per slot; service is an independent one-shot credit. */
void vs_native_rules_frame(VsNativeRulesState *state, uint8_t p1,
                           uint8_t p2, uint8_t coin_service);

uint8_t vs_native_rules_can_start(const VsNativeRulesState *state,
                                  uint8_t players);
uint8_t vs_native_rules_start(VsNativeRulesState *state, uint8_t players);

void vs_native_rules_enter_game_over(VsNativeRulesState *state);
uint8_t vs_native_rules_continue(VsNativeRulesState *state);

#endif
