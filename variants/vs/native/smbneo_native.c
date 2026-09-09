/* Generated direct-C migration backend; no opcode dispatcher. */
#include "smbneo_native.h"
#ifdef SMBNEO_NATIVE_FAST_PATHS
#include "vs_native_fast_actor.h"
#include "vs_native_fast_actor_policy.h"
#include "vs_native_fast_collision.h"
#include "vs_native_fast_game.h"
#include "vs_native_fast_meta.h"
#include "vs_native_fast_motion.h"
#include "vs_native_fast_nmi.h"
#include "vs_native_fast_video.h"
#endif
#include <stddef.h>
#include <string.h>

#ifndef SMBNEO_NATIVE_FORCE_INLINE
#if defined(__GNUC__) || defined(__clang__)
#define SMBNEO_NATIVE_FORCE_INLINE static inline __attribute__((always_inline))
#else
#define SMBNEO_NATIVE_FORCE_INLINE static inline
#endif
#endif

#ifdef SMBNEO_NATIVE_DIAGNOSTICS
#define SMBNEO_NATIVE_MARK(ctx, symbol) do { (ctx)->calls++; (ctx)->last_symbol = (symbol); } while (0)
#define SMBNEO_NATIVE_RESET_DIAGNOSTICS(ctx) do { (ctx)->calls = 0; (ctx)->last_symbol = 0; } while (0)
#else
#define SMBNEO_NATIVE_MARK(ctx, symbol) do { (void)(ctx); (void)(symbol); } while (0)
#define SMBNEO_NATIVE_RESET_DIAGNOSTICS(ctx) do { (void)(ctx); } while (0)
#endif

SMBNEO_NATIVE_FORCE_INLINE uint8_t native_read(SmbNeoNativeContext *ctx, uint16_t address) {
    if (address < 0x2000u) {
        if (ctx->ram == NULL) { ctx->unsupported = 1; return 0; }
        return ctx->ram[address & 0x07ffu];
    }
    if (address >= 0x6000u) {
        if (address < 0x8000u) {
            if (ctx->extra_ram == NULL) { ctx->unsupported = 1; return 0; }
            return ctx->extra_ram[address & 0x07ffu];
        }
        if (ctx->prg == NULL) { ctx->unsupported = 1; return 0; }
        return ctx->prg[address - 0x8000u];
    }
    if (ctx->read8 == NULL) { ctx->unsupported = 1; return 0; }
    return ctx->read8(ctx->opaque, address);
}
SMBNEO_NATIVE_FORCE_INLINE void native_write(SmbNeoNativeContext *ctx, uint16_t address, uint8_t value) {
    if (address < 0x2000u) {
        if (ctx->ram == NULL) { ctx->unsupported = 1; return; }
        ctx->ram[address & 0x07ffu] = value;
        return;
    }
    if (address >= 0x6000u) {
        if (address < 0x8000u) {
            if (ctx->extra_ram == NULL) { ctx->unsupported = 1; return; }
            ctx->extra_ram[address & 0x07ffu] = value;
        }
        /* Program data is immutable; cartridge writes are ignored. */
        return;
    }
    if (ctx->write8 == NULL) { ctx->unsupported = 1; return; }
    ctx->write8(ctx->opaque, address, value);
}
SMBNEO_NATIVE_FORCE_INLINE uint16_t native_zpword(SmbNeoNativeContext *ctx, uint8_t address) { return (uint16_t)(native_read(ctx, address) | ((uint16_t)native_read(ctx, (uint8_t)(address + 1u)) << 8)); }
SMBNEO_NATIVE_FORCE_INLINE uint8_t native_nz(SmbNeoNativeContext *ctx, uint8_t value) { ctx->status = (uint8_t)((ctx->status & (uint8_t)~(NATIVE_N | NATIVE_Z)) | (value & NATIVE_N) | (value == 0 ? NATIVE_Z : 0)); return value; }
SMBNEO_NATIVE_FORCE_INLINE void native_cmp(SmbNeoNativeContext *ctx, uint8_t lhs, uint8_t rhs) { uint8_t result = (uint8_t)(lhs - rhs); ctx->status = (uint8_t)((ctx->status & (uint8_t)~(NATIVE_C | NATIVE_N | NATIVE_Z)) | (lhs >= rhs ? NATIVE_C : 0) | (result & NATIVE_N) | (result == 0 ? NATIVE_Z : 0)); }
SMBNEO_NATIVE_FORCE_INLINE void native_bit(SmbNeoNativeContext *ctx, uint8_t value) { ctx->status = (uint8_t)((ctx->status & (uint8_t)~(NATIVE_Z | NATIVE_V | NATIVE_N)) | ((ctx->a & value) == 0 ? NATIVE_Z : 0) | (value & (NATIVE_V | NATIVE_N))); }
SMBNEO_NATIVE_FORCE_INLINE void native_adc(SmbNeoNativeContext *ctx, uint8_t value) { unsigned sum = ctx->a + value + (ctx->status & NATIVE_C ? 1u : 0u); uint8_t result = (uint8_t)sum; ctx->status = (uint8_t)((ctx->status & (uint8_t)~(NATIVE_C | NATIVE_V)) | (sum > 255u ? NATIVE_C : 0) | ((~(ctx->a ^ value) & (ctx->a ^ result) & 0x80u) ? NATIVE_V : 0)); ctx->a = native_nz(ctx, result); }
SMBNEO_NATIVE_FORCE_INLINE void native_sbc(SmbNeoNativeContext *ctx, uint8_t value) { native_adc(ctx, (uint8_t)~value); }
SMBNEO_NATIVE_FORCE_INLINE uint8_t native_asl(SmbNeoNativeContext *ctx, uint8_t value) { ctx->status = (uint8_t)((ctx->status & (uint8_t)~NATIVE_C) | (value >> 7 ? NATIVE_C : 0)); return native_nz(ctx, (uint8_t)(value << 1)); }
SMBNEO_NATIVE_FORCE_INLINE uint8_t native_lsr(SmbNeoNativeContext *ctx, uint8_t value) { ctx->status = (uint8_t)((ctx->status & (uint8_t)~NATIVE_C) | (value & 1u ? NATIVE_C : 0)); return native_nz(ctx, (uint8_t)(value >> 1)); }
SMBNEO_NATIVE_FORCE_INLINE uint8_t native_rol(SmbNeoNativeContext *ctx, uint8_t value) { uint8_t old = (ctx->status & NATIVE_C) ? 1u : 0u; ctx->status = (uint8_t)((ctx->status & (uint8_t)~NATIVE_C) | (value >> 7 ? NATIVE_C : 0)); return native_nz(ctx, (uint8_t)((value << 1) | old)); }
SMBNEO_NATIVE_FORCE_INLINE uint8_t native_ror(SmbNeoNativeContext *ctx, uint8_t value) { uint8_t old = (ctx->status & NATIVE_C) ? 0x80u : 0u; ctx->status = (uint8_t)((ctx->status & (uint8_t)~NATIVE_C) | (value & 1u ? NATIVE_C : 0)); return native_nz(ctx, (uint8_t)((value >> 1) | old)); }

void smbneo_native_fn_start_8000(SmbNeoNativeContext *ctx);
void smbneo_native_fn_nmi_810a(SmbNeoNativeContext *ctx);
void smbneo_native_fn_irq_81fe(SmbNeoNativeContext *ctx);
void smbneo_native_fn_vs_setbank_low_820c(SmbNeoNativeContext *ctx);
void smbneo_native_fn_nmi_oam_shuffle_8212(SmbNeoNativeContext *ctx);
void smbneo_native_fn_sys_enter_825e(SmbNeoNativeContext *ctx);
void smbneo_native_fn_oam_init_826e(SmbNeoNativeContext *ctx);
void smbneo_native_fn_oam_init_all_8273(SmbNeoNativeContext *ctx);
void smbneo_native_fn_sys_title_8281(SmbNeoNativeContext *ctx);
void smbneo_native_fn_vs_title_init_82af(SmbNeoNativeContext *ctx);
void smbneo_native_fn_title_proc_82c3(SmbNeoNativeContext *ctx);
void smbneo_native_fn_title_course_set_832f(SmbNeoNativeContext *ctx);
void smbneo_native_fn_title_demo_8369(SmbNeoNativeContext *ctx);
void smbneo_native_fn_sys_castle_victory_838e(SmbNeoNativeContext *ctx);
void smbneo_native_fn_victory_enter_83a3(SmbNeoNativeContext *ctx);
void smbneo_native_fn_victory_init_83bf(SmbNeoNativeContext *ctx);
void smbneo_native_fn_victory_proc_83cc(SmbNeoNativeContext *ctx);
void smbneo_native_fn_victory_msg_8405(SmbNeoNativeContext *ctx);
void smbneo_native_fn_victory_time_tally_8474(SmbNeoNativeContext *ctx);
void smbneo_native_fn_victory_84CF_84cf(SmbNeoNativeContext *ctx);
void smbneo_native_fn_victory_8526_8526(SmbNeoNativeContext *ctx);
void smbneo_native_fn_victory_8552_8552(SmbNeoNativeContext *ctx);
void smbneo_native_fn_victory_85E0_85e0(SmbNeoNativeContext *ctx);
void smbneo_native_fn_victory_finish_861e(SmbNeoNativeContext *ctx);
void smbneo_native_fn_victory_864C_864c(SmbNeoNativeContext *ctx);
void smbneo_native_fn_victory_865D_865d(SmbNeoNativeContext *ctx);
void smbneo_native_fn_bg_score_disp_86a4(SmbNeoNativeContext *ctx);
void smbneo_native_fn_bg_proc_8748(SmbNeoNativeContext *ctx);
void smbneo_native_fn_bg_init_876c(SmbNeoNativeContext *ctx);
void smbneo_native_fn_bg_color_init_1_877c(SmbNeoNativeContext *ctx);
void smbneo_native_fn_bg_color_init_2_87a0(SmbNeoNativeContext *ctx);
void smbneo_native_fn_bg_color_init_3_87c4(SmbNeoNativeContext *ctx);
void smbneo_native_fn_bg_color_player_87d2(SmbNeoNativeContext *ctx);
void smbneo_native_fn_bg_color_init_4_8824(SmbNeoNativeContext *ctx);
void smbneo_native_fn_bg_text_draw_header_8833(SmbNeoNativeContext *ctx);
void smbneo_native_fn_bg_text_draw_splash_883b(SmbNeoNativeContext *ctx);
void smbneo_native_fn_bg_text_draw_timeup_8874(SmbNeoNativeContext *ctx);
void smbneo_native_fn_bg_text_draw_intermission_8889(SmbNeoNativeContext *ctx);
void smbneo_native_fn_bg_scenery_88c7(SmbNeoNativeContext *ctx);
void smbneo_native_fn_bg_title_screen_88f0(SmbNeoNativeContext *ctx);
void smbneo_native_fn_bg_vschar1_load_88f7(SmbNeoNativeContext *ctx);
void smbneo_native_fn_bg_title_cursor_8931(SmbNeoNativeContext *ctx);
void smbneo_native_fn_bg_title_score_8945(SmbNeoNativeContext *ctx);
void smbneo_native_fn_bg_text_draw_8a04(SmbNeoNativeContext *ctx);
void smbneo_native_fn_bg_timer_clear_check_8a9e(SmbNeoNativeContext *ctx);
void smbneo_native_fn_bg_timer_reset_8aa6(SmbNeoNativeContext *ctx);
void smbneo_native_fn_meta_render_area_8aaf(SmbNeoNativeContext *ctx);
void smbneo_native_fn_meta_render_attr_8b6b(SmbNeoNativeContext *ctx);
void smbneo_native_fn_meta_color_rotate_8be2(SmbNeoNativeContext *ctx);
void smbneo_native_fn_meta_put_blank_8c4e(SmbNeoNativeContext *ctx);
void smbneo_native_fn_meta_replace_8c62(SmbNeoNativeContext *ctx);
void smbneo_native_fn_meta_destroy_8c6c(SmbNeoNativeContext *ctx);
void smbneo_native_fn_meta_write_8c6e(SmbNeoNativeContext *ctx);
void smbneo_native_fn_meta_step_8c90(SmbNeoNativeContext *ctx);
void smbneo_native_fn_meta_clear_put_8c98(SmbNeoNativeContext *ctx);
void smbneo_native_fn_meta_clear_put_do_8cce(SmbNeoNativeContext *ctx);
void smbneo_native_fn_tbljmp_90bb(SmbNeoNativeContext *ctx);
void smbneo_native_fn_nt_init_90d0(SmbNeoNativeContext *ctx);
void smbneo_native_fn_nt_init_do_90e4(SmbNeoNativeContext *ctx);
void smbneo_native_fn_joypad_read_9113(SmbNeoNativeContext *ctx);
void smbneo_native_fn_joypad_read_do_9125(SmbNeoNativeContext *ctx);
void smbneo_native_fn_ppu_displist_write_9195(SmbNeoNativeContext *ctx);
void smbneo_native_fn_ppu_displist_write_scc_h_v_919e(SmbNeoNativeContext *ctx);
void smbneo_native_fn_ppu_displist_write_ctlr0_91a5(SmbNeoNativeContext *ctx);
void smbneo_native_fn_stats_print_num_91be(SmbNeoNativeContext *ctx);
void smbneo_native_fn_stats_print_num_do_91c9(SmbNeoNativeContext *ctx);
void smbneo_native_fn_stats_calc_9217(SmbNeoNativeContext *ctx);
void smbneo_native_fn_stats_score_hi_check_924f(SmbNeoNativeContext *ctx);
void smbneo_native_fn_stats_score_hi_check_do_9256(SmbNeoNativeContext *ctx);
void smbneo_native_fn_vs_read_dips_9274(SmbNeoNativeContext *ctx);
void smbneo_native_fn_vs_disp_credit_msg_92e1(SmbNeoNativeContext *ctx);
void smbneo_native_fn_sub_9324_9324(SmbNeoNativeContext *ctx);
void smbneo_native_fn_sub_9362_9362(SmbNeoNativeContext *ctx);
void smbneo_native_fn_sub_9389_9389(SmbNeoNativeContext *ctx);
void smbneo_native_fn_sub_9389_do_93a6(SmbNeoNativeContext *ctx);
void smbneo_native_fn_sub_93E0_93e0(SmbNeoNativeContext *ctx);
void smbneo_native_fn_sub_93E7_93e7(SmbNeoNativeContext *ctx);
void smbneo_native_fn_sys_vs_player_select_941e(SmbNeoNativeContext *ctx);
void smbneo_native_fn_vs_player_select_init_0_942c(SmbNeoNativeContext *ctx);
void smbneo_native_fn_vs_player_select_scr_943e(SmbNeoNativeContext *ctx);
void smbneo_native_fn_vs_player_select_init_1_9454(SmbNeoNativeContext *ctx);
void smbneo_native_fn_vs_player_select_do_947a(SmbNeoNativeContext *ctx);
void smbneo_native_fn_vs_title_ppu_init_9502(SmbNeoNativeContext *ctx);
void smbneo_native_fn_vs_title_super_players_953b(SmbNeoNativeContext *ctx);
void smbneo_native_fn_vs_title_tick_9588(SmbNeoNativeContext *ctx);
void smbneo_native_fn_game_init_ram_9643(SmbNeoNativeContext *ctx);
void smbneo_native_fn_game_init_0_9651(SmbNeoNativeContext *ctx);
void smbneo_native_fn_game_init_0_do_965c(SmbNeoNativeContext *ctx);
void smbneo_native_fn_game_init_setup_96e4(SmbNeoNativeContext *ctx);
void smbneo_native_fn_game_init_1_96ff(SmbNeoNativeContext *ctx);
void smbneo_native_fn_game_init_1_do_9702(SmbNeoNativeContext *ctx);
void smbneo_native_fn_game_init_clearram_975d(SmbNeoNativeContext *ctx);
void smbneo_native_fn_game_init_area_music_977e(SmbNeoNativeContext *ctx);
void smbneo_native_fn_player_proc_init_97cb(SmbNeoNativeContext *ctx);
void smbneo_native_fn_player_proc_lifedown_987f(SmbNeoNativeContext *ctx);
void smbneo_native_fn_sys_game_over_98ca(SmbNeoNativeContext *ctx);
void smbneo_native_fn_game_over_98DC_98dc(SmbNeoNativeContext *ctx);
void smbneo_native_fn_game_over_98F2_98f2(SmbNeoNativeContext *ctx);
void smbneo_native_fn_game_over_9902_9902(SmbNeoNativeContext *ctx);
void smbneo_native_fn_sub_9919_9919(SmbNeoNativeContext *ctx);
void smbneo_native_fn_sub_9924_9924(SmbNeoNativeContext *ctx);
void smbneo_native_fn_sub_9945_9945(SmbNeoNativeContext *ctx);
void smbneo_native_fn_sub_995D_995d(SmbNeoNativeContext *ctx);
void smbneo_native_fn_sub_9A20_9a20(SmbNeoNativeContext *ctx);
void smbneo_native_fn_sub_9A32_9a32(SmbNeoNativeContext *ctx);
void smbneo_native_fn_sub_9A4E_9a4e(SmbNeoNativeContext *ctx);
void smbneo_native_fn_sub_9AE3_9ae3(SmbNeoNativeContext *ctx);
void smbneo_native_fn_sub_9AFF_9aff(SmbNeoNativeContext *ctx);
void smbneo_native_fn_sub_9B06_9b06(SmbNeoNativeContext *ctx);
void smbneo_native_fn_sub_9B0C_9b0c(SmbNeoNativeContext *ctx);
void smbneo_native_fn_sub_9B38_9b38(SmbNeoNativeContext *ctx);
void smbneo_native_fn_sub_9D3C_9d3c(SmbNeoNativeContext *ctx);
void smbneo_native_fn_vs_game_over_joypad_get_9d4d(SmbNeoNativeContext *ctx);
void smbneo_native_fn_sub_9D62_9d62(SmbNeoNativeContext *ctx);
void smbneo_native_fn_sub_9DEA_9dea(SmbNeoNativeContext *ctx);
void smbneo_native_fn_sub_9DEA_b_9df0(SmbNeoNativeContext *ctx);
void smbneo_native_fn_sub_9DF7_9df7(SmbNeoNativeContext *ctx);
void smbneo_native_fn_sub_9E29_9e29(SmbNeoNativeContext *ctx);
void smbneo_native_fn_sub_9EDA_0A_9eda(SmbNeoNativeContext *ctx);
void smbneo_native_fn_sub_9EDA_14_9ee3(SmbNeoNativeContext *ctx);
void smbneo_native_fn_sub_9F3B_9f3b(SmbNeoNativeContext *ctx);
void smbneo_native_fn_sub_9FC1_9fc1(SmbNeoNativeContext *ctx);
void smbneo_native_fn_sub_A006_a006(SmbNeoNativeContext *ctx);
void smbneo_native_fn_sub_A01B_a01b(SmbNeoNativeContext *ctx);
void smbneo_native_fn_region_a074_a074(SmbNeoNativeContext *ctx);
void smbneo_native_fn_sub_A075_a075(SmbNeoNativeContext *ctx);
void smbneo_native_fn_sub_A0B2_a0b2(SmbNeoNativeContext *ctx);
void smbneo_native_fn_sub_A0D7_a0d7(SmbNeoNativeContext *ctx);
void smbneo_native_fn_sub_A0D7_b_a0f3(SmbNeoNativeContext *ctx);
void smbneo_native_fn_game_over_init_a10f(SmbNeoNativeContext *ctx);
void smbneo_native_fn_game_over_proc_a122(SmbNeoNativeContext *ctx);
void smbneo_native_fn_game_over_player_switch_a15d(SmbNeoNativeContext *ctx);
void smbneo_native_fn_sub_92AA_a185(SmbNeoNativeContext *ctx);
void smbneo_native_fn_sub_92AA_rts_a18a(SmbNeoNativeContext *ctx);
void smbneo_native_fn_scenery_proc_a18b(SmbNeoNativeContext *ctx);
void smbneo_native_fn_scenery_proc_do_a1a3(SmbNeoNativeContext *ctx);
void smbneo_native_fn_scenery_inc_seam_a1b6(SmbNeoNativeContext *ctx);
void smbneo_native_fn_scenery_proc_main_a2d7(SmbNeoNativeContext *ctx);
void smbneo_native_fn_scenery_do_a3e3(SmbNeoNativeContext *ctx);
void smbneo_native_fn_scenery_offset_base_inc_a487(SmbNeoNativeContext *ctx);
void smbneo_native_fn_scenery_decode_a493(SmbNeoNativeContext *ctx);
void smbneo_native_fn_scenery_decode_rts2_a561(SmbNeoNativeContext *ctx);
void smbneo_native_fn_scenery_set_attr_a5e6(SmbNeoNativeContext *ctx);
void smbneo_native_fn_scenery_lock_warp_a618(SmbNeoNativeContext *ctx);
void smbneo_native_fn_scenery_lock_a633(SmbNeoNativeContext *ctx);
void smbneo_native_fn_scenery_actor_kill_a63c(SmbNeoNativeContext *ctx);
void smbneo_native_fn_scenery_swarm_a651(SmbNeoNativeContext *ctx);
void smbneo_native_fn_scenery_ledge_a666(SmbNeoNativeContext *ctx);
void smbneo_native_fn_scenery_ledge_tree_a672(SmbNeoNativeContext *ctx);
void smbneo_native_fn_scenery_ledge_mushroom_a69e(SmbNeoNativeContext *ctx);
void smbneo_native_fn_scenery_pulley_a6e0(SmbNeoNativeContext *ctx);
void smbneo_native_fn_scenery_castle_a72c(SmbNeoNativeContext *ctx);
void smbneo_native_fn_scenery_pipe_water_a795(SmbNeoNativeContext *ctx);
void smbneo_native_fn_scenery_pipe_intro_a7a8(SmbNeoNativeContext *ctx);
void smbneo_native_fn_scenery_pipe_exit_a7d1(SmbNeoNativeContext *ctx);
void smbneo_native_fn_scenery_pipe_horiz_a7d9(SmbNeoNativeContext *ctx);
void smbneo_native_fn_scenery_pipe_vert_a80b(SmbNeoNativeContext *ctx);
void smbneo_native_fn_scenery_pipe_h_a85f(SmbNeoNativeContext *ctx);
void smbneo_native_fn_scenery_alloc_a870(SmbNeoNativeContext *ctx);
void smbneo_native_fn_scenery_waterhole_a87d(SmbNeoNativeContext *ctx);
void smbneo_native_fn_scenery_block_qhi_a88e(SmbNeoNativeContext *ctx);
void smbneo_native_fn_scenery_block_qlo_a893(SmbNeoNativeContext *ctx);
void smbneo_native_fn_scenery_bridge_hi_a8a1(SmbNeoNativeContext *ctx);
void smbneo_native_fn_scenery_bridge_mid_a8a6(SmbNeoNativeContext *ctx);
void smbneo_native_fn_scenery_bridge_lo_a8ab(SmbNeoNativeContext *ctx);
void smbneo_native_fn_scenery_pole_tip_a8c0(SmbNeoNativeContext *ctx);
void smbneo_native_fn_scenery_pole_a8ca(SmbNeoNativeContext *ctx);
void smbneo_native_fn_scenery_rope_norm_a8fc(SmbNeoNativeContext *ctx);
void smbneo_native_fn_scenery_rope_plat_a903(SmbNeoNativeContext *ctx);
void smbneo_native_fn_scenery_coinrow_a91e(SmbNeoNativeContext *ctx);
void smbneo_native_fn_scenery_castle_bridge_a92d(SmbNeoNativeContext *ctx);
void smbneo_native_fn_scenery_castle_bridge_axe_a935(SmbNeoNativeContext *ctx);
void smbneo_native_fn_scenery_castle_bridge_draw_a93a(SmbNeoNativeContext *ctx);
void smbneo_native_fn_scenery_empty_draw_a945(SmbNeoNativeContext *ctx);
void smbneo_native_fn_scenery_brickrow_a95a(SmbNeoNativeContext *ctx);
void smbneo_native_fn_scenery_floorrow_a96a(SmbNeoNativeContext *ctx);
void smbneo_native_fn_scenery_brickcol_a97c(SmbNeoNativeContext *ctx);
void smbneo_native_fn_scenery_floorcol_a985(SmbNeoNativeContext *ctx);
void smbneo_native_fn_scenery_cannon_a995(SmbNeoNativeContext *ctx);
void smbneo_native_fn_scenery_stairs_a9e3(SmbNeoNativeContext *ctx);
void smbneo_native_fn_scenery_spring_a9ff(SmbNeoNativeContext *ctx);
void smbneo_native_fn_scenery_hidden_one_up_aa2f(SmbNeoNativeContext *ctx);
void smbneo_native_fn_scenery_block_q_aa3c(SmbNeoNativeContext *ctx);
void smbneo_native_fn_scenery_block_coin_aa42(SmbNeoNativeContext *ctx);
void smbneo_native_fn_scenery_block_item_aa47(SmbNeoNativeContext *ctx);
void smbneo_native_fn_scenery_block_id_aa64(SmbNeoNativeContext *ctx);
void smbneo_native_fn_scenery_pit_aa6f(SmbNeoNativeContext *ctx);
void smbneo_native_fn_scenery_render_aaab(SmbNeoNativeContext *ctx);
void smbneo_native_fn_scenery_len_aada(SmbNeoNativeContext *ctx);
void smbneo_native_fn_scenery_len_b_aadd(SmbNeoNativeContext *ctx);
void smbneo_native_fn_scenery_attr_aae9(SmbNeoNativeContext *ctx);
void smbneo_native_fn_scenery_pos_x_get_aafe(SmbNeoNativeContext *ctx);
void smbneo_native_fn_scenery_pos_y_get_ab06(SmbNeoNativeContext *ctx);
void smbneo_native_fn_scenery_get_buffer_ptr_ab14(SmbNeoNativeContext *ctx);
void smbneo_native_fn_course_descriptor_load_ab60(SmbNeoNativeContext *ctx);
void smbneo_native_fn_course_type_get_ab66(SmbNeoNativeContext *ctx);
void smbneo_native_fn_course_descriptor_get_ab70(SmbNeoNativeContext *ctx);
void smbneo_native_fn_course_load_ab7f(SmbNeoNativeContext *ctx);
void smbneo_native_fn_sys_game_ad50(SmbNeoNativeContext *ctx);
void smbneo_native_fn_vs_game_init_ad60(SmbNeoNativeContext *ctx);
void smbneo_native_fn_game_proc_ad6e(SmbNeoNativeContext *ctx);
void smbneo_native_fn_game_proc_scenery_scroll_ae1c(SmbNeoNativeContext *ctx);
void smbneo_native_fn_game_scroll_ae40(SmbNeoNativeContext *ctx);
void smbneo_native_fn_game_scroll_calc_ae71(SmbNeoNativeContext *ctx);
void smbneo_native_fn_game_screen_pos_x_calc_aee5(SmbNeoNativeContext *ctx);
void smbneo_native_fn_player_proc_aef7(SmbNeoNativeContext *ctx);
void smbneo_native_fn_player_proc_enter_af16(SmbNeoNativeContext *ctx);
void smbneo_native_fn_player_proc_auto_af93(SmbNeoNativeContext *ctx);
void smbneo_native_fn_player_proc_ctrl_af96(SmbNeoNativeContext *ctx);
void smbneo_native_fn_player_proc_vine_b074(SmbNeoNativeContext *ctx);
void smbneo_native_fn_player_proc_vine_b_b08a(SmbNeoNativeContext *ctx);
void smbneo_native_fn_player_proc_pipe_v_b092(SmbNeoNativeContext *ctx);
void smbneo_native_fn_game_pos_y_lo_player_add_b0ad(SmbNeoNativeContext *ctx);
void smbneo_native_fn_player_proc_pipe_h_b0b3(SmbNeoNativeContext *ctx);
void smbneo_native_fn_player_proc_pipe_h_do_2_b0c0(SmbNeoNativeContext *ctx);
void smbneo_native_fn_player_proc_pipe_h_enter_b0cc(SmbNeoNativeContext *ctx);
void smbneo_native_fn_player_proc_size_chg_b0e0(SmbNeoNativeContext *ctx);
void smbneo_native_fn_player_proc_hurt_b0f2(SmbNeoNativeContext *ctx);
void smbneo_native_fn_player_proc_death_b116(SmbNeoNativeContext *ctx);
void smbneo_native_fn_player_proc_player_done_b120(SmbNeoNativeContext *ctx);
void smbneo_native_fn_player_proc_firefl_b12a(SmbNeoNativeContext *ctx);
void smbneo_native_fn_player_proc_firefl_do_b135(SmbNeoNativeContext *ctx);
void smbneo_native_fn_player_proc_firefl_do_2_b147(SmbNeoNativeContext *ctx);
void smbneo_native_fn_player_proc_pole_b151(SmbNeoNativeContext *ctx);
void smbneo_native_fn_player_proc_victory_b177(SmbNeoNativeContext *ctx);
void smbneo_native_fn_player_proc_player_move_b1d6(SmbNeoNativeContext *ctx);
void smbneo_native_fn_player_proc_player_ground_b207(SmbNeoNativeContext *ctx);
void smbneo_native_fn_player_proc_player_fall_b21a(SmbNeoNativeContext *ctx);
void smbneo_native_fn_player_proc_player_jump_swim_b223(SmbNeoNativeContext *ctx);
void smbneo_native_fn_player_proc_player_climb_b27c(SmbNeoNativeContext *ctx);
void smbneo_native_fn_player_proc_player_physics_b2fd(SmbNeoNativeContext *ctx);
void smbneo_native_fn_player_proc_player_ani_speed_b43c(SmbNeoNativeContext *ctx);
void smbneo_native_fn_player_proc_player_friction_b479(SmbNeoNativeContext *ctx);
void smbneo_native_fn_proj_proc_b4d1(SmbNeoNativeContext *ctx);
void smbneo_native_fn_proj_proc_do_b536(SmbNeoNativeContext *ctx);
void smbneo_native_fn_bubble_proc_b5a6(SmbNeoNativeContext *ctx);
void smbneo_native_fn_bubble_init_b5b8(SmbNeoNativeContext *ctx);
void smbneo_native_fn_stats_time_proc_b5fc(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_B_1F_proc_b658(SmbNeoNativeContext *ctx);
void smbneo_native_fn_whirlpool_proc_b66c(SmbNeoNativeContext *ctx);
void smbneo_native_fn_pole_proc_b709(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_B_1D_proc_b76e(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_B_1A_init_b7d2(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_B_1A_proc_b7ff(SmbNeoNativeContext *ctx);
void smbneo_native_fn_cannon_proc_b8b0(SmbNeoNativeContext *ctx);
void smbneo_native_fn_cannon_bullet_proc_b927(SmbNeoNativeContext *ctx);
void smbneo_native_fn_misc_init_hammer_b988(SmbNeoNativeContext *ctx);
void smbneo_native_fn_misc_proc_hammer_b9b7(SmbNeoNativeContext *ctx);
void smbneo_native_fn_misc_init_coin_a_ba2c(SmbNeoNativeContext *ctx);
void smbneo_native_fn_misc_init_coin_b_ba45(SmbNeoNativeContext *ctx);
void smbneo_native_fn_misc_init_coin_block_alloc_ba78(SmbNeoNativeContext *ctx);
void smbneo_native_fn_misc_proc_ba8a(SmbNeoNativeContext *ctx);
void smbneo_native_fn_misc_proc_coin_add_baf6(SmbNeoNativeContext *ctx);
void smbneo_native_fn_misc_proc_coin_update_score_bb32(SmbNeoNativeContext *ctx);
void smbneo_native_fn_misc_proc_coin_update_stats_bb3b(SmbNeoNativeContext *ctx);
void smbneo_native_fn_misc_proc_coin_add_score_bb41(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_B_19_init_bb6b(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_B_19_proc_bb90(SmbNeoNativeContext *ctx);
void smbneo_native_fn_block_punch_bbf8(SmbNeoNativeContext *ctx);
void smbneo_native_fn_block_initpos_bc8f(SmbNeoNativeContext *ctx);
void smbneo_native_fn_block_item_bca6(SmbNeoNativeContext *ctx);
void smbneo_native_fn_block_item_end_bcdd(SmbNeoNativeContext *ctx);
void smbneo_native_fn_block_star_bce2(SmbNeoNativeContext *ctx);
void smbneo_native_fn_block_mushroom_oneup_bce7(SmbNeoNativeContext *ctx);
void smbneo_native_fn_block_vine_bcee(SmbNeoNativeContext *ctx);
void smbneo_native_fn_block_is_tile_bd05(SmbNeoNativeContext *ctx);
void smbneo_native_fn_block_break_bd11(SmbNeoNativeContext *ctx);
void smbneo_native_fn_block_top_check_bd2e(SmbNeoNativeContext *ctx);
void smbneo_native_fn_block_bits_init_bd50(SmbNeoNativeContext *ctx);
void smbneo_native_fn_block_proc_bd7f(SmbNeoNativeContext *ctx);
void smbneo_native_fn_block_flatten_bde3(SmbNeoNativeContext *ctx);
void smbneo_native_fn_motion_x_be11(SmbNeoNativeContext *ctx);
void smbneo_native_fn_motion_x_player_be18(SmbNeoNativeContext *ctx);
void smbneo_native_fn_motion_x_do_be1e(SmbNeoNativeContext *ctx);
void smbneo_native_fn_motion_y_be72(SmbNeoNativeContext *ctx);
void smbneo_native_fn_motion_y_state_5_be7a(SmbNeoNativeContext *ctx);
void smbneo_native_fn_motion_fall_fast_be97(SmbNeoNativeContext *ctx);
void smbneo_native_fn_motion_fall_slow_be9b(SmbNeoNativeContext *ctx);
void smbneo_native_fn_motion_fall_mid_bea1(SmbNeoNativeContext *ctx);
void smbneo_native_fn_motion_fall_bea5(SmbNeoNativeContext *ctx);
void smbneo_native_fn_motion_block_fall_slow_beb0(SmbNeoNativeContext *ctx);
void smbneo_native_fn_motion_block_fall_fast_beb5(SmbNeoNativeContext *ctx);
void smbneo_native_fn_motion_gravity_do_bebe(SmbNeoNativeContext *ctx);
void smbneo_native_fn_motion_platform_down_bec5(SmbNeoNativeContext *ctx);
void smbneo_native_fn_motion_platform_up_beca(SmbNeoNativeContext *ctx);
void smbneo_native_fn_motion_gravity_beea(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_proc_bf60(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_area_loop_bfa5(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_init_c160(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_init_do_c1b5(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_A_04_init_c239(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_A_06_init_c23a(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_A_0C_init_c240(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_B_20_init_c250(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_A_00_init_c257(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_A_03_init_c267(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_A_05_init_c26f(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_A_10_init_c280(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_A_07_init_c285(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_init_col_small_c289(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_A_0F_init_c28d(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_init_veloc_y_c2a6(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_A_08_init_c2ae(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_A_0A_init_c2b8(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_A_11_init_c2c8(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_init_lakitu_do_c2cd(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_init_lakitu_spiny_c2e7(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_B_0A_init_c39c(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_B_06_init_c39f(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_init_cheep_fly_c3eb(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_B_18_init_c48c(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_init_dup_c4b8(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_init_bowser_flame_c4e6(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_init_right_side_c51b(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_init_fireworks_c580(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_init_bullet_cheep_c5df(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_A_0D_init_c6ca(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_A_12_init_c6e3(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_init_swarm_null_c6fa(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_B_03_init_c6fb(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_A_0E_init_c714(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_B_0F_init_c722(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_B_14_init_c746(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_B_13_init_c74e(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_B_10_init_c755(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_B_11_init_c782(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_B_12_init_c788(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_B_16_init_c78e(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_B_17_init_c79a(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_plat_pos_c7b4(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_B_36_init_c7c4(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_B_02_proc_c819(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_B_20_proc_c81a(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_proc_enemy_c823(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_proc_enemy_do_c848(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_A_09_proc_c877(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_B_00_proc_c878(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_B_06_proc_c88a(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_B_16_proc_c890(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_B_0F_proc_c8a8(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_proc_plat_do_c8c5(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_erase_c8db(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_A_0C_proc_c8f3(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_A_05_proc_c91b(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_A_00_proc_c9ba(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_A_0E_proc_ca3c(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_A_0F_proc_ca42(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_A_10_proc_ca68(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_floater_koopa_ca88(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_floater_do_ca8a(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_floater_move_caa9(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_A_07_proc_cacc(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_proc_swim_cb22(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_A_08_proc_cb79(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_A_0A_proc_cb8d(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_proc_firebar_do_cc7f(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_firebar_col_draw_ccfe(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_firebar_col_cd4b(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_firebar_get_pos_cdd1(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_A_14_proc_ce22(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_A_11_proc_ce6b(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_proc_lakitu_spiny_ceaf(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_bowser_ending_cf2f(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_B_18_proc_cfa8(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_killall_cfb4(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_proc_bowser_col_d0ff(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_proc_bowser_flame_timer_init_d11c(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_proc_bowser_flame_do_d12e(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_B_01_proc_d1d8(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_B_1C_proc_d21c(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_proc_flag_victory_tbl_done_d235(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_proc_flag_victory_fireworks_rts_d254(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_proc_flag_victory_timer_score_d255(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_proc_flag_victory_timer_score_do_d260(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_proc_flag_victory_raise_d291(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_proc_flag_victory_draw_d2a8(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_proc_flag_victory_delay_d2e5(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_A_0D_proc_d2f3(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_proc_firebar_spin_d353(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_proc_plat_bal_d375(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_init_plat_rope_d484(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_proc_plat_stop_d4f4(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_proc_plat_mov_v_d516(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_proc_plat_move_h_d54a(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_proc_plat_move_player_h_d557(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_proc_plat_drop_d574(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_proc_plat_right_d580(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_proc_plat_lift_d592(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_proc_plat_small_do_d598(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_proc_plat_lift_move_d59e(SmbNeoNativeContext *ctx);
void smbneo_native_fn_actor_oob_proc_d5bd(SmbNeoNativeContext *ctx);
void smbneo_native_fn_col_proj_actor_proc_d630(SmbNeoNativeContext *ctx);
void smbneo_native_fn_col_proj_actor_damage_d695(SmbNeoNativeContext *ctx);
void smbneo_native_fn_col_actor_kill_d6ec(SmbNeoNativeContext *ctx);
void smbneo_native_fn_col_player_hammer_proc_d71b(SmbNeoNativeContext *ctx);
void smbneo_native_fn_col_player_actor_proc_d7aa(SmbNeoNativeContext *ctx);
void smbneo_native_fn_col_player_harm_proc_d883(SmbNeoNativeContext *ctx);
void smbneo_native_fn_col_player_harm_proc_do_d888(SmbNeoNativeContext *ctx);
void smbneo_native_fn_col_player_harm_proc_proc_change_d89f(SmbNeoNativeContext *ctx);
void smbneo_native_fn_col_actor_kick_dir_d95c(SmbNeoNativeContext *ctx);
void smbneo_native_fn_col_score_do_d968(SmbNeoNativeContext *ctx);
void smbneo_native_fn_col_actor_actor_proc_d98a(SmbNeoNativeContext *ctx);
void smbneo_native_fn_col_actor_actor_do_da0b(SmbNeoNativeContext *ctx);
void smbneo_native_fn_col_actor_calc_dir_check_da73(SmbNeoNativeContext *ctx);
void smbneo_native_fn_col_plat_l_proc_da9c(SmbNeoNativeContext *ctx);
void smbneo_native_fn_region_dab6_dab6(SmbNeoNativeContext *ctx);
void smbneo_native_fn_col_plat_s_proc_dad2(SmbNeoNativeContext *ctx);
void smbneo_native_fn_col_plat_l_do_db13(SmbNeoNativeContext *ctx);
void smbneo_native_fn_col_plat_s_player_pos_db70(SmbNeoNativeContext *ctx);
void smbneo_native_fn_col_plat_player_pos_db7a(SmbNeoNativeContext *ctx);
void smbneo_native_fn_col_check_player_pos_y_db9a(SmbNeoNativeContext *ctx);
void smbneo_native_fn_col_actor_box_get_dbab(SmbNeoNativeContext *ctx);
void smbneo_native_fn_col_actor_box_get_do_dbad(SmbNeoNativeContext *ctx);
void smbneo_native_fn_col_bg_proc_dbbd(SmbNeoNativeContext *ctx);
void smbneo_native_fn_col_bg_proc_bg_tile_blank_dd75(SmbNeoNativeContext *ctx);
void smbneo_native_fn_col_bg_proc_vis_de16(SmbNeoNativeContext *ctx);
void smbneo_native_fn_col_bg_proc_spring_de1d(SmbNeoNativeContext *ctx);
void smbneo_native_fn_col_bg_proc_spring_check_de36(SmbNeoNativeContext *ctx);
void smbneo_native_fn_col_bg_proc_pipe_v_do_de41(SmbNeoNativeContext *ctx);
void smbneo_native_fn_col_bg_proc_player_halt_dea1(SmbNeoNativeContext *ctx);
void smbneo_native_fn_col_bg_proc_solid_check_dee5(SmbNeoNativeContext *ctx);
void smbneo_native_fn_col_bg_proc_climb_check_def0(SmbNeoNativeContext *ctx);
void smbneo_native_fn_col_bg_proc_coin_check_def7(SmbNeoNativeContext *ctx);
void smbneo_native_fn_col_bg_proc_get_tile_page_df06(SmbNeoNativeContext *ctx);
void smbneo_native_fn_col_actor_ground_proc_df17(SmbNeoNativeContext *ctx);
void smbneo_native_fn_col_actor_koopa_demote_check_df71(SmbNeoNativeContext *ctx);
void smbneo_native_fn_col_player_actor_stomp_do_df85(SmbNeoNativeContext *ctx);
void smbneo_native_fn_col_actor_check_side_bump_e07a(SmbNeoNativeContext *ctx);
void smbneo_native_fn_col_player_bullet_diff_e099(SmbNeoNativeContext *ctx);
void smbneo_native_fn_col_actor_land_do_e0a5(SmbNeoNativeContext *ctx);
void smbneo_native_fn_col_actor_pos_y_diff_e0b1(SmbNeoNativeContext *ctx);
void smbneo_native_fn_col_actor_bounce_proc_e0b9(SmbNeoNativeContext *ctx);
void smbneo_native_fn_col_actor_block_bump_kill_e0e4(SmbNeoNativeContext *ctx);
void smbneo_native_fn_col_actor_block_col_e104(SmbNeoNativeContext *ctx);
void smbneo_native_fn_col_bg_proc_check_non_solid_e10b(SmbNeoNativeContext *ctx);
void smbneo_native_fn_col_proj_proc_e11e(SmbNeoNativeContext *ctx);
void smbneo_native_fn_col_proj_box_e183(SmbNeoNativeContext *ctx);
void smbneo_native_fn_col_misc_box_e18c(SmbNeoNativeContext *ctx);
void smbneo_native_fn_col_actor_box_e199(SmbNeoNativeContext *ctx);
void smbneo_native_fn_col_plat_s_box_e1a2(SmbNeoNativeContext *ctx);
void smbneo_native_fn_col_plat_l_box_e1c9(SmbNeoNativeContext *ctx);
void smbneo_native_fn_col_box_proc_e1f2(SmbNeoNativeContext *ctx);
void smbneo_native_fn_col_base_player_e27b(SmbNeoNativeContext *ctx);
void smbneo_native_fn_col_base_actor_e27d(SmbNeoNativeContext *ctx);
void smbneo_native_fn_col_box_buffer_check_actor_e2de(SmbNeoNativeContext *ctx);
void smbneo_native_fn_col_box_buffer_check_misc_e2e8(SmbNeoNativeContext *ctx);
void smbneo_native_fn_col_box_buffer_check_proj_e2f2(SmbNeoNativeContext *ctx);
void smbneo_native_fn_col_box_player_step_e33e(SmbNeoNativeContext *ctx);
void smbneo_native_fn_col_box_player_jump_e33f(SmbNeoNativeContext *ctx);
void smbneo_native_fn_col_box_player_back_e344(SmbNeoNativeContext *ctx);
void smbneo_native_fn_col_box_buffer_e348(SmbNeoNativeContext *ctx);
void smbneo_native_fn_render_vine_e392(SmbNeoNativeContext *ctx);
void smbneo_native_fn_render_vine_column_e40b(SmbNeoNativeContext *ctx);
void smbneo_native_fn_render_hammer_e439(SmbNeoNativeContext *ctx);
void smbneo_native_fn_render_flag_enemy_e4a8(SmbNeoNativeContext *ctx);
void smbneo_native_fn_render_clear_six_sprites_e510(SmbNeoNativeContext *ctx);
void smbneo_native_fn_render_set_six_sprites_e512(SmbNeoNativeContext *ctx);
void smbneo_native_fn_render_set_four_sprites_v_e518(SmbNeoNativeContext *ctx);
void smbneo_native_fn_render_set_three_sprites_v_e51b(SmbNeoNativeContext *ctx);
void smbneo_native_fn_render_set_two_sprites_v_e51e(SmbNeoNativeContext *ctx);
void smbneo_native_fn_render_plat_large_e525(SmbNeoNativeContext *ctx);
void smbneo_native_fn_render_coin_score_e5e3(SmbNeoNativeContext *ctx);
void smbneo_native_fn_render_powerup_e62f(SmbNeoNativeContext *ctx);
void smbneo_native_fn_render_actor_e7da(SmbNeoNativeContext *ctx);
void smbneo_native_fn_render_actor_chr_pair_eb07(SmbNeoNativeContext *ctx);
void smbneo_native_fn_render_chr_pair_eb0f(SmbNeoNativeContext *ctx);
void smbneo_native_fn_render_actor_clear_chr_pair_v_eb14(SmbNeoNativeContext *ctx);
void smbneo_native_fn_render_actor_clear_chr_pair_h_eb1e(SmbNeoNativeContext *ctx);
void smbneo_native_fn_render_block_eb2e(SmbNeoNativeContext *ctx);
void smbneo_native_fn_render_offscr_check_top_eba3(SmbNeoNativeContext *ctx);
void smbneo_native_fn_render_offscr_top_clear_eba7(SmbNeoNativeContext *ctx);
void smbneo_native_fn_render_block_bits_ebb0(SmbNeoNativeContext *ctx);
void smbneo_native_fn_render_firebar_ec4a(SmbNeoNativeContext *ctx);
void smbneo_native_fn_render_fireworks_ec74(SmbNeoNativeContext *ctx);
void smbneo_native_fn_render_plat_small_ecc3(SmbNeoNativeContext *ctx);
void smbneo_native_fn_bubble_render_ed3e(SmbNeoNativeContext *ctx);
void smbneo_native_fn_render_player_ee46(SmbNeoNativeContext *ctx);
void smbneo_native_fn_render_player_normal_ee99(SmbNeoNativeContext *ctx);
void smbneo_native_fn_render_player_course_card_ef09(SmbNeoNativeContext *ctx);
void smbneo_native_fn_render_player_tiles_init_ef23(SmbNeoNativeContext *ctx);
void smbneo_native_fn_render_player_tiles_ef41(SmbNeoNativeContext *ctx);
void smbneo_native_fn_render_player_action_ef51(SmbNeoNativeContext *ctx);
void smbneo_native_fn_render_player_fall_efc7(SmbNeoNativeContext *ctx);
void smbneo_native_fn_render_player_get_size_offset_eff6(SmbNeoNativeContext *ctx);
void smbneo_native_fn_render_player_resize_f015(SmbNeoNativeContext *ctx);
void smbneo_native_fn_render_player_tiles_mirror_f04e(SmbNeoNativeContext *ctx);
void smbneo_native_fn_pos_calc_x_rel_player_f08f(SmbNeoNativeContext *ctx);
void smbneo_native_fn_pos_calc_x_rel_bubble_f096(SmbNeoNativeContext *ctx);
void smbneo_native_fn_pos_calc_x_rel_proj_f0a0(SmbNeoNativeContext *ctx);
void smbneo_native_fn_pos_calc_x_rel_misc_f0ad(SmbNeoNativeContext *ctx);
void smbneo_native_fn_pos_calc_x_rel_actor_f0b7(SmbNeoNativeContext *ctx);
void smbneo_native_fn_pos_calc_x_rel_block_f0be(SmbNeoNativeContext *ctx);
void smbneo_native_fn_pos_calc_x_rel_b_f0ca(SmbNeoNativeContext *ctx);
void smbneo_native_fn_pos_calc_x_rel_do_f0d6(SmbNeoNativeContext *ctx);
void smbneo_native_fn_pos_bits_get_player_f0e5(SmbNeoNativeContext *ctx);
void smbneo_native_fn_pos_bits_get_proj_f0ec(SmbNeoNativeContext *ctx);
void smbneo_native_fn_pos_bits_get_bubble_f0f6(SmbNeoNativeContext *ctx);
void smbneo_native_fn_pos_bits_get_misc_f100(SmbNeoNativeContext *ctx);
void smbneo_native_fn_pos_calc_x_offset_get_f10d(SmbNeoNativeContext *ctx);
void smbneo_native_fn_pos_bits_get_actor_f114(SmbNeoNativeContext *ctx);
void smbneo_native_fn_pos_bits_get_block_f11b(SmbNeoNativeContext *ctx);
void smbneo_native_fn_pos_bits_proc_f13c(SmbNeoNativeContext *ctx);
void smbneo_native_fn_pos_bits_get_do_x_f15b(SmbNeoNativeContext *ctx);
void smbneo_native_fn_pos_bits_diff_f1d2(SmbNeoNativeContext *ctx);
void smbneo_native_fn_apu_cycle_f236(SmbNeoNativeContext *ctx);
void smbneo_native_fn_apu_write_base_pulse_1_f289(SmbNeoNativeContext *ctx);
void smbneo_native_fn_apu_write_pulse_1_f290(SmbNeoNativeContext *ctx);
void smbneo_native_fn_apu_write_note_pulse_1_f293(SmbNeoNativeContext *ctx);
void smbneo_native_fn_apu_write_base_pulse_2_f2a7(SmbNeoNativeContext *ctx);
void smbneo_native_fn_apu_write_pulse_2_f2ae(SmbNeoNativeContext *ctx);
void smbneo_native_fn_apu_write_note_pulse_2_f2b1(SmbNeoNativeContext *ctx);
void smbneo_native_fn_apu_write_note_triangle_f2b5(SmbNeoNativeContext *ctx);
void smbneo_native_fn_apu_sfx_pulse_1_play_f323(SmbNeoNativeContext *ctx);
void smbneo_native_fn_apu_sfx_pulse_1_stop_f3af(SmbNeoNativeContext *ctx);
void smbneo_native_fn_apu_sfx_pulse_2_mute_f479(SmbNeoNativeContext *ctx);
void smbneo_native_fn_apu_sfx_pulse_2_play_f484(SmbNeoNativeContext *ctx);
void smbneo_native_fn_apu_sfx_noise_play_f56f(SmbNeoNativeContext *ctx);
void smbneo_native_fn_apu_music_play_f59c(SmbNeoNativeContext *ctx);
void smbneo_native_fn_apu_note_decomp_f803(SmbNeoNativeContext *ctx);
void smbneo_native_fn_apu_note_length_load_f809(SmbNeoNativeContext *ctx);
void smbneo_native_fn_apu_next_state_f816(SmbNeoNativeContext *ctx);
void smbneo_native_fn_apu_next_volume_f832(SmbNeoNativeContext *ctx);

void smbneo_native_fn_start_8000(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x8000u);
    ctx->status |= NATIVE_I;
    ctx->status &= (uint8_t)~NATIVE_D;
    ctx->a = native_nz(ctx, 0x10u);
    native_write(ctx, 0x2000u, ctx->a);
    ctx->x = native_nz(ctx, 0xffu);
    /* reset-only processor state eliminated */
L800a: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x2002u));
    if (!(ctx->status & NATIVE_N)) {
        goto L800a;
    }
L800f: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x2002u));
    if (!(ctx->status & NATIVE_N)) {
        goto L800f;
    }
    ctx->y = native_nz(ctx, 0xfeu);
    ctx->x = native_nz(ctx, 0x05u);
L8018: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x07d7u + ctx->x)));
    native_cmp(ctx, ctx->a, 0x0au);
    if ((ctx->status & NATIVE_C)) {
        goto L8022;
    }
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x - 1u));
    if (!(ctx->status & NATIVE_N)) {
        goto L8018;
    }
L8022: ;
    smbneo_native_fn_game_init_clearram_975d(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_write(ctx, 0x4011u, ctx->a);
    native_write(ctx, 0x0770u, ctx->a);
    ctx->a = native_nz(ctx, 0xa5u);
    native_write(ctx, 0x07a7u, ctx->a);
    ctx->a = native_nz(ctx, 0x0fu);
    native_write(ctx, 0x4015u, ctx->a);
    ctx->a = native_nz(ctx, 0x06u);
    native_write(ctx, 0x2001u, ctx->a);
    smbneo_native_fn_oam_init_826e(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_nt_init_90d0(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_write(ctx, 0x0774u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0774u) + 1u)));
    smbneo_native_fn_vs_setbank_low_820c(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, 0x00u);
    ctx->x = native_nz(ctx, 0x00u);
L804a: ;
    native_write(ctx, (uint16_t)(0x6700u + ctx->x), ctx->a);
    native_write(ctx, (uint16_t)(0x6600u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    if (!(ctx->status & NATIVE_Z)) {
        goto L804a;
    }
    ctx->a = native_nz(ctx, 0x06u);
    native_write(ctx, 0x4016u, ctx->a);
    ctx->a = native_nz(ctx, 0x1eu);
    native_write(ctx, 0x2006u, ctx->a);
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x2006u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x2007u));
    ctx->y = native_nz(ctx, 0x74u);
L8067: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x2007u));
    native_write(ctx, (uint16_t)(0x6600u + ctx->y), ctx->a);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    if (!(ctx->status & NATIVE_Z)) {
        goto L8067;
    }
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, 0x4016u, ctx->a);
    ctx->y = native_nz(ctx, 0x05u);
L8077: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x66bau + ctx->y)));
    native_write(ctx, (uint16_t)(0x07d7u + ctx->y), ctx->a);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    if (!(ctx->status & NATIVE_N)) {
        goto L8077;
    }
    smbneo_native_fn_vs_read_dips_9274(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, 0x0778u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | 0x80u));
    smbneo_native_fn_ppu_displist_write_ctlr0_91a5(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->suspended = 1; return;
    ctx->unsupported = 1; return;
    return;
}

void smbneo_native_fn_nmi_810a(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x810au);
    uint8_t saved_value_0 = 0u;
    ctx->a = native_nz(ctx, native_read(ctx, 0x0778u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x7fu));
    native_write(ctx, 0x0778u, ctx->a);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x7eu));
    native_write(ctx, 0x2000u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0779u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0xe6u));
    ctx->y = native_nz(ctx, native_read(ctx, 0x0774u));
    if (!(ctx->status & NATIVE_Z)) {
        goto L8126;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x0779u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | 0x1eu));
L8126: ;
    native_write(ctx, 0x0779u, ctx->a);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0xe7u));
    native_write(ctx, 0x2001u, ctx->a);
    ctx->x = native_nz(ctx, native_read(ctx, 0x2002u));
    ctx->a = native_nz(ctx, 0x00u);
    smbneo_native_fn_ppu_displist_write_scc_h_v_919e(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_write(ctx, 0x2003u, ctx->a);
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, 0x4014u, ctx->a);
    ctx->x = native_nz(ctx, native_read(ctx, 0x0773u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x808eu + ctx->x)));
    native_write(ctx, 0x00u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x80b5u + ctx->x)));
    native_write(ctx, 0x01u, ctx->a);
    smbneo_native_fn_ppu_displist_write_9195(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->y = native_nz(ctx, 0x00u);
    ctx->x = native_nz(ctx, native_read(ctx, 0x0773u));
    native_cmp(ctx, ctx->x, 0x06u);
    if (!(ctx->status & NATIVE_Z)) {
        goto L8158;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
L8158: ;
    ctx->x = native_nz(ctx, native_read(ctx, (uint16_t)(0x8108u + ctx->y)));
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, (uint16_t)(0x0300u + ctx->x), ctx->a);
    native_write(ctx, (uint16_t)(0x0301u + ctx->x), ctx->a);
    native_write(ctx, 0x0773u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0779u));
    native_write(ctx, 0x2001u, ctx->a);
    smbneo_native_fn_apu_cycle_f236(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_sub_9324_9324(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_sub_9362_9362(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_sub_9389_9389(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_joypad_read_9113(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_stats_score_hi_check_924f(ctx);
    if (ctx->suspended || ctx->unsupported) return;
#ifdef SMBNEO_NATIVE_FAST_PATHS
    vs_native_fast_nmi_timers(ctx);
    goto L81a3;
#endif
    ctx->a = native_nz(ctx, native_read(ctx, 0x0747u));
    if ((ctx->status & NATIVE_Z)) {
        goto L8188;
    }
    native_write(ctx, 0x0747u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0747u) - 1u)));
    if (!(ctx->status & NATIVE_Z)) {
        goto L81a1;
    }
L8188: ;
    ctx->x = native_nz(ctx, 0x14u);
    native_write(ctx, 0x077fu, native_nz(ctx, (uint8_t)(native_read(ctx, 0x077fu) - 1u)));
    if (!(ctx->status & NATIVE_N)) {
        goto L8196;
    }
    ctx->a = native_nz(ctx, 0x14u);
    native_write(ctx, 0x077fu, ctx->a);
    ctx->x = native_nz(ctx, 0x23u);
L8196: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0780u + ctx->x)));
    if ((ctx->status & NATIVE_Z)) {
        goto L819e;
    }
    native_write(ctx, (uint16_t)(0x0780u + ctx->x), native_nz(ctx, (uint8_t)(native_read(ctx, (uint16_t)(0x0780u + ctx->x)) - 1u)));
L819e: ;
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x - 1u));
    if (!(ctx->status & NATIVE_N)) {
        goto L8196;
    }
L81a1: ;
    native_write(ctx, 0x09u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x09u) + 1u)));
#ifdef SMBNEO_NATIVE_FAST_PATHS
L81a3: ;
#endif
#ifdef SMBNEO_NATIVE_FAST_PATHS
    vs_native_fast_nmi_random(ctx);
    goto L81c0;
#endif
    ctx->x = native_nz(ctx, 0x00u);
    ctx->y = native_nz(ctx, 0x07u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x07a7u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x02u));
    native_write(ctx, 0x00u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x07a8u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x02u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a ^ native_read(ctx, 0x00u)));
    ctx->status &= (uint8_t)~NATIVE_C;
    if ((ctx->status & NATIVE_Z)) {
        goto L81b9;
    }
    ctx->status |= NATIVE_C;
L81b9: ;
    native_write(ctx, (uint16_t)(0x07a7u + ctx->x), native_ror(ctx, native_read(ctx, (uint16_t)(0x07a7u + ctx->x))));
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    if (!(ctx->status & NATIVE_Z)) {
        goto L81b9;
    }
#ifdef SMBNEO_NATIVE_FAST_PATHS
L81c0: ;
#endif
    ctx->a = native_nz(ctx, native_read(ctx, 0x0722u));
    if ((ctx->status & NATIVE_Z)) {
        goto L81de;
    }
L81c5: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x2002u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x40u));
    if (!(ctx->status & NATIVE_Z)) {
        goto L81c5;
    }
    smbneo_native_fn_oam_init_all_8273(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_nmi_oam_shuffle_8212(ctx);
    if (ctx->suspended || ctx->unsupported) return;
L81d2: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x2002u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x40u));
    if ((ctx->status & NATIVE_Z)) {
        goto L81d2;
    }
#ifdef SMBNEO_NATIVE_FAST_PATHS
    vs_native_fast_nmi_hblank_delay(ctx);
    goto L81de;
#endif
    ctx->y = native_nz(ctx, 0x14u);
L81db: ;
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    if (!(ctx->status & NATIVE_Z)) {
        goto L81db;
    }
L81de: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x073fu));
    native_write(ctx, 0x2005u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0740u));
    native_write(ctx, 0x2005u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0778u));
    saved_value_0 = ctx->a;
    native_write(ctx, 0x2000u, ctx->a);
    smbneo_native_fn_sys_enter_825e(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, 0x2002u));
    ctx->a = native_nz(ctx, saved_value_0);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | 0x80u));
    native_write(ctx, 0x2000u, ctx->a);
    return;
    return;
}

void smbneo_native_fn_irq_81fe(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x81feu);
    ctx->a = native_nz(ctx, native_read(ctx, 0x4016u));
    if ((ctx->status & NATIVE_N)) {
        goto L8208;
    }
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, 0x4016u, ctx->a);
L8208: ;
    /* processor-created interrupt frame absent in native C */
    /* processor-created interrupt frame absent in native C */
    /* processor-created interrupt frame absent in native C */
    return;
    return;
}

void smbneo_native_fn_vs_setbank_low_820c(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x820cu);
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, 0x4016u, ctx->a);
    return;
    return;
}

void smbneo_native_fn_nmi_oam_shuffle_8212(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x8212u);
#ifdef SMBNEO_NATIVE_FAST_PATHS
    vs_native_fast_nmi_oam_shuffle(ctx);
    return;
#endif
    ctx->y = native_nz(ctx, native_read(ctx, 0x074eu));
    ctx->a = native_nz(ctx, 0x28u);
    native_write(ctx, 0x00u, ctx->a);
    ctx->x = native_nz(ctx, 0x0eu);
L821b: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x06e4u + ctx->x)));
    native_cmp(ctx, ctx->a, native_read(ctx, 0x00u));
    if (!(ctx->status & NATIVE_C)) {
        goto L8231;
    }
    ctx->y = native_nz(ctx, native_read(ctx, 0x06e0u));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, (uint16_t)(0x06e1u + ctx->y)));
    if (!(ctx->status & NATIVE_C)) {
        goto L822e;
    }
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, 0x00u));
L822e: ;
    native_write(ctx, (uint16_t)(0x06e4u + ctx->x), ctx->a);
L8231: ;
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x - 1u));
    if (!(ctx->status & NATIVE_N)) {
        goto L821b;
    }
    ctx->x = native_nz(ctx, native_read(ctx, 0x06e0u));
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    native_cmp(ctx, ctx->x, 0x03u);
    if (!(ctx->status & NATIVE_Z)) {
        goto L823e;
    }
    ctx->x = native_nz(ctx, 0x00u);
L823e: ;
    native_write(ctx, 0x06e0u, ctx->x);
    ctx->x = native_nz(ctx, 0x08u);
    ctx->y = native_nz(ctx, 0x02u);
L8245: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x06e9u + ctx->y)));
    native_write(ctx, (uint16_t)(0x06f1u + ctx->x), ctx->a);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x08u);
    native_write(ctx, (uint16_t)(0x06f2u + ctx->x), ctx->a);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x08u);
    native_write(ctx, (uint16_t)(0x06f3u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x - 1u));
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x - 1u));
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x - 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    if (!(ctx->status & NATIVE_N)) {
        goto L8245;
    }
    return;
    return;
}

void smbneo_native_fn_sys_enter_825e(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x825eu);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0770u));
    uint8_t table_selector_8261 = ctx->a;
    ctx->a = native_asl(ctx, ctx->a);
    ctx->y = native_nz(ctx, ctx->a);
    native_write(ctx, 0x04u, 0x63u);
    native_write(ctx, 0x05u, 0x82u);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    switch (table_selector_8261) {
    case 0: native_write(ctx, 0x06u, 0x81u);
             native_write(ctx, 0x07u, 0x82u);
             ctx->a = native_nz(ctx, 0x82u);
             smbneo_native_fn_sys_title_8281(ctx); return;
    case 1: native_write(ctx, 0x06u, 0x1eu);
             native_write(ctx, 0x07u, 0x94u);
             ctx->a = native_nz(ctx, 0x94u);
             smbneo_native_fn_sys_vs_player_select_941e(ctx); return;
    case 2: native_write(ctx, 0x06u, 0x50u);
             native_write(ctx, 0x07u, 0xadu);
             ctx->a = native_nz(ctx, 0xadu);
             smbneo_native_fn_sys_game_ad50(ctx); return;
    case 3: native_write(ctx, 0x06u, 0x8eu);
             native_write(ctx, 0x07u, 0x83u);
             ctx->a = native_nz(ctx, 0x83u);
             smbneo_native_fn_sys_castle_victory_838e(ctx); return;
    case 4: native_write(ctx, 0x06u, 0xcau);
             native_write(ctx, 0x07u, 0x98u);
             ctx->a = native_nz(ctx, 0x98u);
             smbneo_native_fn_sys_game_over_98ca(ctx); return;
    default: ctx->unsupported = 1; return;
    }
    return;
}

void smbneo_native_fn_oam_init_826e(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x826eu);
#ifdef SMBNEO_NATIVE_FAST_PATHS
    vs_native_fast_oam_init(ctx);
    return;
#endif
    ctx->y = native_nz(ctx, 0x00u);
    /* static tail transfer */
    goto L8275;
L8275: ;
    ctx->a = native_nz(ctx, 0xf8u);
L8277: ;
    native_write(ctx, (uint16_t)(0x0200u + ctx->y), ctx->a);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    if (!(ctx->status & NATIVE_Z)) {
        goto L8277;
    }
    return;
    return;
}

void smbneo_native_fn_oam_init_all_8273(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x8273u);
#ifdef SMBNEO_NATIVE_FAST_PATHS
    vs_native_fast_oam_init_all(ctx);
    return;
#endif
    ctx->y = native_nz(ctx, 0x04u);
    ctx->a = native_nz(ctx, 0xf8u);
L8277: ;
    native_write(ctx, (uint16_t)(0x0200u + ctx->y), ctx->a);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    if (!(ctx->status & NATIVE_Z)) {
        goto L8277;
    }
    return;
    return;
}

void smbneo_native_fn_sys_title_8281(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x8281u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x6610u));
    if ((ctx->status & NATIVE_Z)) {
        goto L8297;
    }
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x0772u, ctx->a);
    native_write(ctx, 0x0722u, ctx->a);
    native_write(ctx, 0x0770u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0770u) + 1u)));
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, 0x0774u, ctx->a);
    return;
L8297: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x0772u));
    uint8_t table_selector_829a = ctx->a;
    ctx->a = native_asl(ctx, ctx->a);
    ctx->y = native_nz(ctx, ctx->a);
    native_write(ctx, 0x04u, 0x9cu);
    native_write(ctx, 0x05u, 0x82u);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    switch (table_selector_829a) {
    case 0: native_write(ctx, 0x06u, 0xafu);
             native_write(ctx, 0x07u, 0x82u);
             ctx->a = native_nz(ctx, 0x82u);
             smbneo_native_fn_vs_title_init_82af(ctx); return;
    case 1: native_write(ctx, 0x06u, 0x51u);
             native_write(ctx, 0x07u, 0x96u);
             ctx->a = native_nz(ctx, 0x96u);
             smbneo_native_fn_game_init_0_9651(ctx); return;
    case 2: native_write(ctx, 0x06u, 0x48u);
             native_write(ctx, 0x07u, 0x87u);
             ctx->a = native_nz(ctx, 0x87u);
             smbneo_native_fn_bg_proc_8748(ctx); return;
    case 3: native_write(ctx, 0x06u, 0xffu);
             native_write(ctx, 0x07u, 0x96u);
             ctx->a = native_nz(ctx, 0x96u);
             smbneo_native_fn_game_init_1_96ff(ctx); return;
    case 4: native_write(ctx, 0x06u, 0xc3u);
             native_write(ctx, 0x07u, 0x82u);
             ctx->a = native_nz(ctx, 0x82u);
             smbneo_native_fn_title_proc_82c3(ctx); return;
    case 5: native_write(ctx, 0x06u, 0x2cu);
             native_write(ctx, 0x07u, 0x94u);
             ctx->a = native_nz(ctx, 0x94u);
             smbneo_native_fn_vs_player_select_init_0_942c(ctx); return;
    case 6: native_write(ctx, 0x06u, 0x02u);
             native_write(ctx, 0x07u, 0x95u);
             ctx->a = native_nz(ctx, 0x95u);
             smbneo_native_fn_vs_title_ppu_init_9502(ctx); return;
    case 7: native_write(ctx, 0x06u, 0x3bu);
             native_write(ctx, 0x07u, 0x95u);
             ctx->a = native_nz(ctx, 0x95u);
             smbneo_native_fn_vs_title_super_players_953b(ctx); return;
    case 8: native_write(ctx, 0x06u, 0x88u);
             native_write(ctx, 0x07u, 0x95u);
             ctx->a = native_nz(ctx, 0x95u);
             smbneo_native_fn_vs_title_tick_9588(ctx); return;
    default: ctx->unsupported = 1; return;
    }
    return;
}

void smbneo_native_fn_vs_title_init_82af(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x82afu);
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, 0x0774u, ctx->a);
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x0722u, ctx->a);
    native_write(ctx, 0x0772u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0772u) + 1u)));
    return;
    return;
}

void smbneo_native_fn_title_proc_82c3(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x82c3u);
    ctx->y = native_nz(ctx, 0x00u);
    ctx->x = native_nz(ctx, native_read(ctx, 0x07a2u));
    if (!(ctx->status & NATIVE_Z)) {
        goto L82d2;
    }
    smbneo_native_fn_title_demo_8369(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    if ((ctx->status & NATIVE_C)) {
        goto L82e3;
    }
    /* static tail transfer */
    goto L82da;
L82d2: ;
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x06fcu, ctx->a);
    native_write(ctx, 0x06fdu, ctx->a);
L82da: ;
    smbneo_native_fn_game_proc_ad6e(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, 0x0eu));
    native_cmp(ctx, ctx->a, 0x06u);
    if (!(ctx->status & NATIVE_Z)) {
        goto L832e;
    }
L82e3: ;
    native_write(ctx, 0x0772u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0772u) + 1u)));
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x0722u, ctx->a);
    native_write(ctx, 0x0774u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0774u) + 1u)));
    return;
L832e: ;
    return;
    return;
}

void smbneo_native_fn_title_course_set_832f(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x832fu);
    native_write(ctx, 0x075fu, ctx->a);
    native_write(ctx, 0x0766u, ctx->a);
    ctx->x = native_nz(ctx, 0x00u);
    native_write(ctx, 0x0760u, ctx->x);
    native_write(ctx, 0x0767u, ctx->x);
    return;
    return;
}

void smbneo_native_fn_title_demo_8369(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x8369u);
    ctx->x = native_nz(ctx, native_read(ctx, 0x0717u));
    ctx->a = native_nz(ctx, native_read(ctx, 0x0718u));
    if (!(ctx->status & NATIVE_Z)) {
        goto L837e;
    }
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    native_write(ctx, 0x0717u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0717u) + 1u)));
    ctx->status |= NATIVE_C;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x8352u + ctx->x)));
    native_write(ctx, 0x0718u, ctx->a);
    if ((ctx->status & NATIVE_Z)) {
        goto L8388;
    }
L837e: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x833du + ctx->x)));
    native_write(ctx, 0x06fcu, ctx->a);
    native_write(ctx, 0x0718u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0718u) - 1u)));
    ctx->status &= (uint8_t)~NATIVE_C;
L8388: ;
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x06fdu, ctx->a);
    return;
    return;
}

void smbneo_native_fn_sys_castle_victory_838e(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x838eu);
    smbneo_native_fn_victory_enter_83a3(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, 0x0772u));
    if ((ctx->status & NATIVE_Z)) {
        goto L839d;
    }
    ctx->x = native_nz(ctx, 0x00u);
    native_write(ctx, 0x08u, ctx->x);
    smbneo_native_fn_actor_proc_bf60(ctx);
    if (ctx->suspended || ctx->unsupported) return;
L839d: ;
    smbneo_native_fn_pos_calc_x_rel_player_f08f(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    /* static tail transfer */
    smbneo_native_fn_render_player_ee46(ctx);
    return;
    return;
}

void smbneo_native_fn_victory_enter_83a3(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x83a3u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0772u));
    uint8_t table_selector_83a6 = ctx->a;
    ctx->a = native_asl(ctx, ctx->a);
    ctx->y = native_nz(ctx, ctx->a);
    native_write(ctx, 0x04u, 0xa8u);
    native_write(ctx, 0x05u, 0x83u);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    switch (table_selector_83a6) {
    case 0: native_write(ctx, 0x06u, 0x2fu);
             native_write(ctx, 0x07u, 0xcfu);
             ctx->a = native_nz(ctx, 0xcfu);
             smbneo_native_fn_actor_bowser_ending_cf2f(ctx); return;
    case 1: native_write(ctx, 0x06u, 0xbfu);
             native_write(ctx, 0x07u, 0x83u);
             ctx->a = native_nz(ctx, 0x83u);
             smbneo_native_fn_victory_init_83bf(ctx); return;
    case 2: native_write(ctx, 0x06u, 0xccu);
             native_write(ctx, 0x07u, 0x83u);
             ctx->a = native_nz(ctx, 0x83u);
             smbneo_native_fn_victory_proc_83cc(ctx); return;
    case 3: native_write(ctx, 0x06u, 0x05u);
             native_write(ctx, 0x07u, 0x84u);
             ctx->a = native_nz(ctx, 0x84u);
             smbneo_native_fn_victory_msg_8405(ctx); return;
    case 4: native_write(ctx, 0x06u, 0x74u);
             native_write(ctx, 0x07u, 0x84u);
             ctx->a = native_nz(ctx, 0x84u);
             smbneo_native_fn_victory_time_tally_8474(ctx); return;
    case 5: native_write(ctx, 0x06u, 0x1eu);
             native_write(ctx, 0x07u, 0x86u);
             ctx->a = native_nz(ctx, 0x86u);
             smbneo_native_fn_victory_finish_861e(ctx); return;
    case 6: native_write(ctx, 0x06u, 0xcfu);
             native_write(ctx, 0x07u, 0x84u);
             ctx->a = native_nz(ctx, 0x84u);
             smbneo_native_fn_victory_84CF_84cf(ctx); return;
    case 7: native_write(ctx, 0x06u, 0x26u);
             native_write(ctx, 0x07u, 0x85u);
             ctx->a = native_nz(ctx, 0x85u);
             smbneo_native_fn_victory_8526_8526(ctx); return;
    case 8: native_write(ctx, 0x06u, 0xe0u);
             native_write(ctx, 0x07u, 0x85u);
             ctx->a = native_nz(ctx, 0x85u);
             smbneo_native_fn_victory_85E0_85e0(ctx); return;
    case 9: native_write(ctx, 0x06u, 0x4cu);
             native_write(ctx, 0x07u, 0x86u);
             ctx->a = native_nz(ctx, 0x86u);
             smbneo_native_fn_victory_864C_864c(ctx); return;
    case 10: native_write(ctx, 0x06u, 0x5du);
             native_write(ctx, 0x07u, 0x86u);
             ctx->a = native_nz(ctx, 0x86u);
             smbneo_native_fn_victory_865D_865d(ctx); return;
    default: ctx->unsupported = 1; return;
    }
    return;
}

void smbneo_native_fn_victory_init_83bf(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x83bfu);
    ctx->x = native_nz(ctx, native_read(ctx, 0x071bu));
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    native_write(ctx, 0x34u, ctx->x);
    ctx->a = native_nz(ctx, 0x08u);
    native_write(ctx, 0xfcu, ctx->a);
    /* static tail transfer */
    goto L894a;
L894a: ;
    native_write(ctx, 0x0772u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0772u) + 1u)));
    return;
    return;
}

void smbneo_native_fn_victory_proc_83cc(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x83ccu);
    ctx->y = native_nz(ctx, 0x00u);
    native_write(ctx, 0x35u, ctx->y);
    ctx->a = native_nz(ctx, native_read(ctx, 0x6du));
    native_cmp(ctx, ctx->a, native_read(ctx, 0x34u));
    if (!(ctx->status & NATIVE_Z)) {
        goto L83dc;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x86u));
    native_cmp(ctx, ctx->a, 0x60u);
    if ((ctx->status & NATIVE_C)) {
        goto L83df;
    }
L83dc: ;
    native_write(ctx, 0x35u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x35u) + 1u)));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
L83df: ;
    ctx->a = native_nz(ctx, ctx->y);
    smbneo_native_fn_player_proc_auto_af93(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, 0x071au));
    native_cmp(ctx, ctx->a, native_read(ctx, 0x34u));
    if ((ctx->status & NATIVE_Z)) {
        goto L8400;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x0768u));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x80u);
    native_write(ctx, 0x0768u, ctx->a);
    ctx->a = native_nz(ctx, 0x01u);
    native_adc(ctx, 0x00u);
    ctx->y = native_nz(ctx, ctx->a);
    smbneo_native_fn_game_scroll_calc_ae71(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_game_proc_scenery_scroll_ae1c(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_write(ctx, 0x35u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x35u) + 1u)));
L8400: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x35u));
    if ((ctx->status & NATIVE_Z)) {
        goto L8465;
    }
    return;
L8465: ;
    native_write(ctx, 0x0772u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0772u) + 1u)));
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x6618u, ctx->a);
    native_write(ctx, 0x6622u, ctx->a);
    native_write(ctx, 0x661fu, ctx->a);
    return;
    return;
}

void smbneo_native_fn_victory_msg_8405(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x8405u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0749u));
    if (!(ctx->status & NATIVE_Z)) {
        goto L844e;
    }
    ctx->y = native_nz(ctx, native_read(ctx, 0x0719u));
    ctx->a = native_nz(ctx, native_read(ctx, 0x075fu));
    native_cmp(ctx, ctx->a, 0x07u);
    if ((ctx->status & NATIVE_Z)) {
        goto L8427;
    }
    native_cmp(ctx, ctx->y, 0x00u);
    if (!(ctx->status & NATIVE_Z)) {
        goto L8421;
    }
    native_write(ctx, 0x0719u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0719u) + 1u)));
    ctx->a = native_nz(ctx, native_read(ctx, 0x0753u));
    if ((ctx->status & NATIVE_Z)) {
        goto L8421;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
L8421: ;
    native_cmp(ctx, ctx->y, 0x03u);
    if (!(ctx->status & NATIVE_C)) {
        goto L8447;
    }
    if ((ctx->status & NATIVE_C)) {
        goto L8460;
    }
L8427: ;
    native_cmp(ctx, ctx->y, 0x02u);
    if ((ctx->status & NATIVE_Z)) {
        goto L842f;
    }
    native_cmp(ctx, ctx->y, 0x06u);
    if (!(ctx->status & NATIVE_Z)) {
        goto L8438;
    }
L842f: ;
    native_write(ctx, 0x0719u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0719u) + 1u)));
    ctx->a = native_nz(ctx, native_read(ctx, 0x0753u));
    if ((ctx->status & NATIVE_Z)) {
        goto L8438;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
L8438: ;
    native_cmp(ctx, ctx->y, 0x0du);
    if ((ctx->status & NATIVE_C)) {
        goto L8460;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    native_cmp(ctx, ctx->y, 0x07u);
    if (!(ctx->status & NATIVE_Z)) {
        goto L8447;
    }
    ctx->a = native_nz(ctx, 0x04u);
    native_write(ctx, 0xfcu, ctx->a);
L8447: ;
    ctx->a = native_nz(ctx, ctx->y);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x0cu);
    native_write(ctx, 0x0773u, ctx->a);
L844e: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x0749u));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x04u);
    native_write(ctx, 0x0749u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0719u));
    native_adc(ctx, 0x00u);
    native_write(ctx, 0x0719u, ctx->a);
    return;
L8460: ;
    ctx->a = native_nz(ctx, 0x0cu);
    native_write(ctx, 0x07a1u, ctx->a);
    native_write(ctx, 0x0772u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0772u) + 1u)));
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x6618u, ctx->a);
    native_write(ctx, 0x6622u, ctx->a);
    native_write(ctx, 0x661fu, ctx->a);
    return;
    return;
}

void smbneo_native_fn_victory_time_tally_8474(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x8474u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x07a1u));
    native_cmp(ctx, ctx->a, 0x0au);
    if ((ctx->status & NATIVE_C)) {
        goto L8491;
    }
    smbneo_native_fn_actor_proc_flag_victory_timer_score_do_d260(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, 0x07f8u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | native_read(ctx, 0x07f9u)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | native_read(ctx, 0x07fau)));
    if (!(ctx->status & NATIVE_Z)) {
        goto L8491;
    }
    ctx->a = native_nz(ctx, 0x30u);
    native_write(ctx, 0x0780u, ctx->a);
    native_write(ctx, 0x0772u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0772u) + 1u)));
L8491: ;
    return;
    return;
}

void smbneo_native_fn_victory_84CF_84cf(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x84cfu);
    ctx->a = native_nz(ctx, native_read(ctx, 0x075fu));
    native_cmp(ctx, ctx->a, 0x07u);
    if ((ctx->status & NATIVE_Z)) {
        goto L84e0;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x0772u));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x02u);
    native_write(ctx, 0x0772u, ctx->a);
    return;
L84e0: ;
    native_write(ctx, 0x6618u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x6618u) + 1u)));
    ctx->a = native_nz(ctx, native_read(ctx, 0x661fu));
    if (!(ctx->status & NATIVE_Z)) {
        goto L84f5;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x6618u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0xffu));
    if (!(ctx->status & NATIVE_Z)) {
        goto L8525;
    }
    native_write(ctx, 0x661fu, native_nz(ctx, (uint8_t)(native_read(ctx, 0x661fu) + 1u)));
    /* static tail transfer */
    goto L84fc;
L84f5: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x6618u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x0fu));
    if (!(ctx->status & NATIVE_Z)) {
        goto L8525;
    }
L84fc: ;
    ctx->x = native_nz(ctx, 0x13u);
L84fe: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x8492u + ctx->x)));
    native_write(ctx, (uint16_t)(0x0301u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x - 1u));
    if (!(ctx->status & NATIVE_N)) {
        goto L84fe;
    }
    ctx->x = native_nz(ctx, 0x0cu);
    ctx->y = native_nz(ctx, native_read(ctx, 0x6622u));
L850c: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x84a6u + ctx->y)));
    native_write(ctx, (uint16_t)(0x0304u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x - 1u));
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x - 1u));
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x - 1u));
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x - 1u));
    if (!(ctx->status & NATIVE_N)) {
        goto L850c;
    }
    native_write(ctx, 0x6622u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x6622u) + 1u)));
    ctx->a = native_nz(ctx, native_read(ctx, 0x6622u));
    native_cmp(ctx, ctx->a, 0x04u);
    if (!(ctx->status & NATIVE_Z)) {
        goto L8525;
    }
    native_write(ctx, 0x0772u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0772u) + 1u)));
L8525: ;
    return;
    return;
}

void smbneo_native_fn_victory_8526_8526(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x8526u);
    ctx->x = native_nz(ctx, 0x08u);
L8528: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x84aau + ctx->x)));
    native_write(ctx, (uint16_t)(0x0301u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x - 1u));
    if (!(ctx->status & NATIVE_N)) {
        goto L8528;
    }
    native_write(ctx, 0x0772u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0772u) + 1u)));
    ctx->x = native_nz(ctx, native_read(ctx, 0x0753u));
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, (uint16_t)(0x6628u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x6622u, ctx->a);
    native_write(ctx, 0x6618u, ctx->a);
    native_write(ctx, 0x661fu, ctx->a);
    ctx->a = native_nz(ctx, 0x06u);
    native_write(ctx, 0x07a1u, ctx->a);
    ctx->a = native_nz(ctx, 0x60u);
    native_write(ctx, 0x662au, ctx->a);
    return;
    return;
}

void smbneo_native_fn_victory_8552_8552(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x8552u);
    uint8_t saved_value_0 = 0u;
    uint8_t saved_value_1 = 0u;
    ctx->a = native_nz(ctx, native_read(ctx, 0x662au));
    if ((ctx->status & NATIVE_Z)) {
        goto L855b;
    }
    native_write(ctx, 0x662au, native_nz(ctx, (uint8_t)(native_read(ctx, 0x662au) - 1u)));
    return;
L855b: ;
    ctx->x = native_nz(ctx, native_read(ctx, 0x6622u));
    native_cmp(ctx, ctx->x, 0x07u);
    if ((ctx->status & NATIVE_Z)) {
        goto L8573;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x6618u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x1fu));
    if (!(ctx->status & NATIVE_Z)) {
        goto L8589;
    }
    native_write(ctx, 0x6622u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x6622u) + 1u)));
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, 0xfeu, ctx->a);
    /* static tail transfer */
    goto L8589;
L8573: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x6618u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x1fu));
    if (!(ctx->status & NATIVE_Z)) {
        goto L8589;
    }
    native_write(ctx, 0x661fu, native_nz(ctx, (uint8_t)(native_read(ctx, 0x661fu) + 1u)));
    ctx->a = native_nz(ctx, native_read(ctx, 0x661fu));
    native_cmp(ctx, ctx->a, 0x0bu);
    if (!(ctx->status & NATIVE_C)) {
        goto L8589;
    }
    ctx->a = native_nz(ctx, 0x04u);
    native_write(ctx, 0x661fu, ctx->a);
L8589: ;
    native_write(ctx, 0x6618u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x6618u) + 1u)));
    ctx->a = native_nz(ctx, native_read(ctx, 0x075fu));
    saved_value_0 = ctx->a;
    ctx->a = native_nz(ctx, native_read(ctx, 0x6622u));
    saved_value_1 = ctx->a;
    ctx->x = native_nz(ctx, ctx->a);
L8595: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x661fu));
    native_cmp(ctx, ctx->a, 0x04u);
    if (!(ctx->status & NATIVE_C)) {
        goto L85a7;
    }
    native_sbc(ctx, 0x04u);
    ctx->y = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x84b3u + ctx->y)));
    native_cmp(ctx, ctx->a, native_read(ctx, (uint16_t)(0x84b9u + ctx->x)));
    if ((ctx->status & NATIVE_Z)) {
        goto L85c6;
    }
L85a7: ;
    ctx->y = native_nz(ctx, native_read(ctx, (uint16_t)(0x84b9u + ctx->x)));
    native_write(ctx, 0x06e5u, ctx->y);
    ctx->a = native_nz(ctx, 0x35u);
    native_write(ctx, 0x16u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x84c0u + ctx->x)));
    native_write(ctx, 0xcfu, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x84c7u + ctx->x)));
    native_write(ctx, 0x03aeu, ctx->a);
    ctx->x = native_nz(ctx, 0x00u);
    native_write(ctx, 0x075fu, ctx->x);
    native_write(ctx, 0x08u, ctx->x);
    smbneo_native_fn_render_actor_e7da(ctx);
    if (ctx->suspended || ctx->unsupported) return;
L85c6: ;
    native_write(ctx, 0x6622u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x6622u) - 1u)));
    ctx->x = native_nz(ctx, native_read(ctx, 0x6622u));
    if (!(ctx->status & NATIVE_Z)) {
        goto L8595;
    }
    ctx->a = native_nz(ctx, saved_value_1);
    native_write(ctx, 0x6622u, ctx->a);
    ctx->a = native_nz(ctx, saved_value_0);
    native_write(ctx, 0x075fu, ctx->a);
    ctx->a = native_nz(ctx, 0x30u);
    native_write(ctx, 0x06e5u, ctx->a);
    ctx->a = native_nz(ctx, 0xb8u);
    native_write(ctx, 0xcfu, ctx->a);
    return;
    return;
}

void smbneo_native_fn_victory_85E0_85e0(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x85e0u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x075fu));
    native_cmp(ctx, ctx->a, 0x07u);
    if (!(ctx->status & NATIVE_Z)) {
        goto L85ea;
    }
    smbneo_native_fn_victory_8552_8552(ctx);
    if (ctx->suspended || ctx->unsupported) return;
L85ea: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x07a1u));
    if (!(ctx->status & NATIVE_Z)) {
        goto L860f;
    }
    ctx->y = native_nz(ctx, native_read(ctx, 0x075fu));
    native_cmp(ctx, ctx->y, 0x07u);
    if ((ctx->status & NATIVE_C)) {
        goto L8610;
    }
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x0760u, ctx->a);
    native_write(ctx, 0x075cu, ctx->a);
    native_write(ctx, 0x0772u, ctx->a);
    native_write(ctx, 0x075fu, native_nz(ctx, (uint8_t)(native_read(ctx, 0x075fu) + 1u)));
    smbneo_native_fn_course_descriptor_load_ab60(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_write(ctx, 0x0757u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0757u) + 1u)));
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, 0x0770u, ctx->a);
L860f: ;
    return;
L8610: ;
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x0747u, ctx->a);
    ctx->a = native_nz(ctx, 0x1cu);
    native_write(ctx, 0x0797u, ctx->a);
    native_write(ctx, 0x0772u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0772u) + 1u)));
    return;
    return;
}

void smbneo_native_fn_victory_finish_861e(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x861eu);
    ctx->a = native_nz(ctx, native_read(ctx, 0x07a1u));
    if (!(ctx->status & NATIVE_Z)) {
        goto L864b;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x075fu));
    native_cmp(ctx, ctx->a, 0x07u);
    if (!(ctx->status & NATIVE_Z)) {
        goto L8648;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x075au));
    if ((ctx->status & NATIVE_N)) {
        goto L8648;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x0780u));
    if (!(ctx->status & NATIVE_Z)) {
        goto L864b;
    }
    ctx->a = native_nz(ctx, 0x30u);
    native_write(ctx, 0x0780u, ctx->a);
    ctx->a = native_nz(ctx, 0x40u);
    native_write(ctx, 0xfeu, ctx->a);
    native_write(ctx, 0x075au, native_nz(ctx, (uint8_t)(native_read(ctx, 0x075au) - 1u)));
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, 0x0135u, ctx->a);
    /* static tail transfer */
    goto Ld279;
L8648: ;
    native_write(ctx, 0x0772u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0772u) + 1u)));
L864b: ;
    return;
Ld279: ;
    ctx->y = native_nz(ctx, 0x0bu);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0753u));
    if ((ctx->status & NATIVE_Z)) {
        goto Ld282;
    }
    ctx->y = native_nz(ctx, 0x11u);
Ld282: ;
    smbneo_native_fn_stats_calc_9217(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, 0x0753u));
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | 0x04u));
    /* static tail transfer */
    smbneo_native_fn_misc_proc_coin_add_score_bb41(ctx);
    return;
    return;
}

void smbneo_native_fn_victory_864C_864c(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x864cu);
    smbneo_native_fn_victory_8552_8552(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, 0x0797u));
    if (!(ctx->status & NATIVE_Z)) {
        goto L867c;
    }
    native_write(ctx, 0x0772u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0772u) + 1u)));
    ctx->a = native_nz(ctx, 0xd8u);
    native_write(ctx, 0x0780u, ctx->a);
    return;
L867c: ;
    return;
    return;
}

void smbneo_native_fn_victory_865D_865d(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x865du);
    smbneo_native_fn_victory_8552_8552(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, 0x0780u));
    if (!(ctx->status & NATIVE_Z)) {
        goto L867c;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x07b1u));
    if (!(ctx->status & NATIVE_Z)) {
        goto L867c;
    }
    native_write(ctx, 0x07fcu, ctx->a);
    ctx->a = native_nz(ctx, 0xffu);
    native_write(ctx, 0x075au, ctx->a);
    ctx->a = native_nz(ctx, 0x04u);
    native_write(ctx, 0x0770u, ctx->a);
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x0772u, ctx->a);
L867c: ;
    return;
    return;
}

void smbneo_native_fn_bg_score_disp_86a4(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x86a4u);
    goto L86a4;
L867c: ;
    return;
L86a4: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0110u + ctx->x)));
    if ((ctx->status & NATIVE_Z)) {
        goto L867c;
    }
    native_cmp(ctx, ctx->a, 0x0cu);
    if (!(ctx->status & NATIVE_C)) {
        goto L86b2;
    }
    ctx->a = native_nz(ctx, 0x0cu);
    native_write(ctx, (uint16_t)(0x0110u + ctx->x), ctx->a);
L86b2: ;
    ctx->y = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x012cu + ctx->x)));
    if (!(ctx->status & NATIVE_Z)) {
        goto L86bc;
    }
    native_write(ctx, (uint16_t)(0x0110u + ctx->x), ctx->a);
    return;
L86bc: ;
    native_write(ctx, (uint16_t)(0x012cu + ctx->x), native_nz(ctx, (uint8_t)(native_read(ctx, (uint16_t)(0x012cu + ctx->x)) - 1u)));
    native_cmp(ctx, ctx->a, 0x2bu);
    if (!(ctx->status & NATIVE_Z)) {
        goto L86e1;
    }
    native_cmp(ctx, ctx->y, 0x0bu);
    if (!(ctx->status & NATIVE_Z)) {
        goto L86ce;
    }
    native_write(ctx, 0x075au, native_nz(ctx, (uint8_t)(native_read(ctx, 0x075au) + 1u)));
    ctx->a = native_nz(ctx, 0x40u);
    native_write(ctx, 0xfeu, ctx->a);
L86ce: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x8697u + ctx->y)));
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->x = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x8697u + ctx->y)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x0fu));
    native_write(ctx, (uint16_t)(0x0134u + ctx->x), ctx->a);
    smbneo_native_fn_misc_proc_coin_update_score_bb32(ctx);
    if (ctx->suspended || ctx->unsupported) return;
L86e1: ;
    ctx->y = native_nz(ctx, native_read(ctx, (uint16_t)(0x06e5u + ctx->x)));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x16u + ctx->x)));
    native_cmp(ctx, ctx->a, 0x12u);
    if ((ctx->status & NATIVE_Z)) {
        goto L870c;
    }
    native_cmp(ctx, ctx->a, 0x0du);
    if ((ctx->status & NATIVE_Z)) {
        goto L870c;
    }
    native_cmp(ctx, ctx->a, 0x05u);
    if ((ctx->status & NATIVE_Z)) {
        goto L8704;
    }
    native_cmp(ctx, ctx->a, 0x0au);
    if ((ctx->status & NATIVE_Z)) {
        goto L870c;
    }
    native_cmp(ctx, ctx->a, 0x0bu);
    if ((ctx->status & NATIVE_Z)) {
        goto L870c;
    }
    native_cmp(ctx, ctx->a, 0x09u);
    if ((ctx->status & NATIVE_C)) {
        goto L8704;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x)));
    native_cmp(ctx, ctx->a, 0x02u);
    if ((ctx->status & NATIVE_C)) {
        goto L870c;
    }
L8704: ;
    ctx->x = native_nz(ctx, native_read(ctx, 0x03eeu));
    ctx->y = native_nz(ctx, native_read(ctx, (uint16_t)(0x06ecu + ctx->x)));
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
L870c: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x011eu + ctx->x)));
    native_cmp(ctx, ctx->a, 0x18u);
    if (!(ctx->status & NATIVE_C)) {
        goto L8718;
    }
    native_sbc(ctx, 0x01u);
    native_write(ctx, (uint16_t)(0x011eu + ctx->x), ctx->a);
L8718: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x011eu + ctx->x)));
    native_sbc(ctx, 0x08u);
    smbneo_native_fn_render_set_two_sprites_v_e51e(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0117u + ctx->x)));
    native_write(ctx, (uint16_t)(0x0203u + ctx->y), ctx->a);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x08u);
    native_write(ctx, (uint16_t)(0x0207u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, (uint16_t)(0x0202u + ctx->y), ctx->a);
    native_write(ctx, (uint16_t)(0x0206u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0110u + ctx->x)));
    ctx->a = native_asl(ctx, ctx->a);
    ctx->x = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x867du + ctx->x)));
    native_write(ctx, (uint16_t)(0x0201u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x867eu + ctx->x)));
    native_write(ctx, (uint16_t)(0x0205u + ctx->y), ctx->a);
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    return;
    return;
}

void smbneo_native_fn_bg_proc_8748(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x8748u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x073cu));
    uint8_t table_selector_874b = ctx->a;
    ctx->a = native_asl(ctx, ctx->a);
    ctx->y = native_nz(ctx, ctx->a);
    native_write(ctx, 0x04u, 0x4du);
    native_write(ctx, 0x05u, 0x87u);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    switch (table_selector_874b) {
    case 0: native_write(ctx, 0x06u, 0x6cu);
             native_write(ctx, 0x07u, 0x87u);
             ctx->a = native_nz(ctx, 0x87u);
             smbneo_native_fn_bg_init_876c(ctx); return;
    case 1: native_write(ctx, 0x06u, 0x7cu);
             native_write(ctx, 0x07u, 0x87u);
             ctx->a = native_nz(ctx, 0x87u);
             smbneo_native_fn_bg_color_init_1_877c(ctx); return;
    case 2: native_write(ctx, 0x06u, 0x33u);
             native_write(ctx, 0x07u, 0x88u);
             ctx->a = native_nz(ctx, 0x88u);
             smbneo_native_fn_bg_text_draw_header_8833(ctx); return;
    case 3: native_write(ctx, 0x06u, 0x3bu);
             native_write(ctx, 0x07u, 0x88u);
             ctx->a = native_nz(ctx, 0x88u);
             smbneo_native_fn_bg_text_draw_splash_883b(ctx); return;
    case 4: native_write(ctx, 0x06u, 0x74u);
             native_write(ctx, 0x07u, 0x88u);
             ctx->a = native_nz(ctx, 0x88u);
             smbneo_native_fn_bg_text_draw_timeup_8874(ctx); return;
    case 5: native_write(ctx, 0x06u, 0x9eu);
             native_write(ctx, 0x07u, 0x8au);
             ctx->a = native_nz(ctx, 0x8au);
             smbneo_native_fn_bg_timer_clear_check_8a9e(ctx); return;
    case 6: native_write(ctx, 0x06u, 0x89u);
             native_write(ctx, 0x07u, 0x88u);
             ctx->a = native_nz(ctx, 0x88u);
             smbneo_native_fn_bg_text_draw_intermission_8889(ctx); return;
    case 7: native_write(ctx, 0x06u, 0x9eu);
             native_write(ctx, 0x07u, 0x8au);
             ctx->a = native_nz(ctx, 0x8au);
             smbneo_native_fn_bg_timer_clear_check_8a9e(ctx); return;
    case 8: native_write(ctx, 0x06u, 0xc7u);
             native_write(ctx, 0x07u, 0x88u);
             ctx->a = native_nz(ctx, 0x88u);
             smbneo_native_fn_bg_scenery_88c7(ctx); return;
    case 9: native_write(ctx, 0x06u, 0xa0u);
             native_write(ctx, 0x07u, 0x87u);
             ctx->a = native_nz(ctx, 0x87u);
             smbneo_native_fn_bg_color_init_2_87a0(ctx); return;
    case 10: native_write(ctx, 0x06u, 0xc4u);
             native_write(ctx, 0x07u, 0x87u);
             ctx->a = native_nz(ctx, 0x87u);
             smbneo_native_fn_bg_color_init_3_87c4(ctx); return;
    case 11: native_write(ctx, 0x06u, 0x24u);
             native_write(ctx, 0x07u, 0x88u);
             ctx->a = native_nz(ctx, 0x88u);
             smbneo_native_fn_bg_color_init_4_8824(ctx); return;
    case 12: native_write(ctx, 0x06u, 0xf0u);
             native_write(ctx, 0x07u, 0x88u);
             ctx->a = native_nz(ctx, 0x88u);
             smbneo_native_fn_bg_title_screen_88f0(ctx); return;
    case 13: native_write(ctx, 0x06u, 0x31u);
             native_write(ctx, 0x07u, 0x89u);
             ctx->a = native_nz(ctx, 0x89u);
             smbneo_native_fn_bg_title_cursor_8931(ctx); return;
    case 14: native_write(ctx, 0x06u, 0x45u);
             native_write(ctx, 0x07u, 0x89u);
             ctx->a = native_nz(ctx, 0x89u);
             smbneo_native_fn_bg_title_score_8945(ctx); return;
    default: ctx->unsupported = 1; return;
    }
    return;
}

void smbneo_native_fn_bg_init_876c(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x876cu);
    smbneo_native_fn_oam_init_826e(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_nt_init_90d0(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, 0x0770u));
    if ((ctx->status & NATIVE_Z)) {
        goto L87a9;
    }
    ctx->x = native_nz(ctx, 0x03u);
    /* static tail transfer */
    goto L87a6;
L87a6: ;
    native_write(ctx, 0x0773u, ctx->x);
L87a9: ;
    /* static tail transfer */
    goto L8941;
L8941: ;
    native_write(ctx, 0x073cu, native_nz(ctx, (uint8_t)(native_read(ctx, 0x073cu) + 1u)));
    return;
    return;
}

void smbneo_native_fn_bg_color_init_1_877c(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x877cu);
    uint8_t saved_value_0 = 0u;
    uint8_t saved_value_1 = 0u;
    ctx->a = native_nz(ctx, native_read(ctx, 0x0744u));
    saved_value_0 = ctx->a;
    ctx->a = native_nz(ctx, native_read(ctx, 0x0756u));
    saved_value_1 = ctx->a;
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x0756u, ctx->a);
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, 0x0744u, ctx->a);
    smbneo_native_fn_bg_color_player_87d2(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, saved_value_1);
    native_write(ctx, 0x0756u, ctx->a);
    ctx->a = native_nz(ctx, saved_value_0);
    native_write(ctx, 0x0744u, ctx->a);
    /* static tail transfer */
    goto L8941;
L8941: ;
    native_write(ctx, 0x073cu, native_nz(ctx, (uint8_t)(native_read(ctx, 0x073cu) + 1u)));
    return;
    return;
}

void smbneo_native_fn_bg_color_init_2_87a0(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x87a0u);
    ctx->y = native_nz(ctx, native_read(ctx, 0x074eu));
    ctx->x = native_nz(ctx, native_read(ctx, (uint16_t)(0x879cu + ctx->y)));
    native_write(ctx, 0x0773u, ctx->x);
    /* static tail transfer */
    goto L8941;
L8941: ;
    native_write(ctx, 0x073cu, native_nz(ctx, (uint8_t)(native_read(ctx, 0x073cu) + 1u)));
    return;
    return;
}

void smbneo_native_fn_bg_color_init_3_87c4(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x87c4u);
    ctx->y = native_nz(ctx, native_read(ctx, 0x0744u));
    if ((ctx->status & NATIVE_Z)) {
        goto L87cf;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x87a8u + ctx->y)));
    native_write(ctx, 0x0773u, ctx->a);
L87cf: ;
    native_write(ctx, 0x073cu, native_nz(ctx, (uint8_t)(native_read(ctx, 0x073cu) + 1u)));
    smbneo_native_fn_bg_color_player_87d2(ctx);
    return;
    return;
}

void smbneo_native_fn_bg_color_player_87d2(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x87d2u);
    ctx->x = native_nz(ctx, native_read(ctx, 0x0300u));
    ctx->y = native_nz(ctx, 0x00u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0753u));
    if ((ctx->status & NATIVE_Z)) {
        goto L87de;
    }
    ctx->y = native_nz(ctx, 0x04u);
L87de: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x0756u));
    native_cmp(ctx, ctx->a, 0x02u);
    if (!(ctx->status & NATIVE_Z)) {
        goto L87e7;
    }
    ctx->y = native_nz(ctx, 0x08u);
L87e7: ;
    ctx->a = native_nz(ctx, 0x03u);
    native_write(ctx, 0x00u, ctx->a);
L87eb: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x87b8u + ctx->y)));
    native_write(ctx, (uint16_t)(0x0304u + ctx->x), ctx->a);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    native_write(ctx, 0x00u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x00u) - 1u)));
    if (!(ctx->status & NATIVE_N)) {
        goto L87eb;
    }
    ctx->x = native_nz(ctx, native_read(ctx, 0x0300u));
    ctx->y = native_nz(ctx, native_read(ctx, 0x0744u));
    if (!(ctx->status & NATIVE_Z)) {
        goto L8802;
    }
    ctx->y = native_nz(ctx, native_read(ctx, 0x074eu));
L8802: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x87b0u + ctx->y)));
    native_write(ctx, (uint16_t)(0x0304u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x3fu);
    native_write(ctx, (uint16_t)(0x0301u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x10u);
    native_write(ctx, (uint16_t)(0x0302u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x04u);
    native_write(ctx, (uint16_t)(0x0303u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, (uint16_t)(0x0308u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, ctx->x);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x07u);
    native_write(ctx, 0x0300u, ctx->a);
    return;
    return;
}

void smbneo_native_fn_bg_color_init_4_8824(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x8824u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0733u));
    native_cmp(ctx, ctx->a, 0x01u);
    if (!(ctx->status & NATIVE_Z)) {
        goto L8830;
    }
    ctx->a = native_nz(ctx, 0x0bu);
    native_write(ctx, 0x0773u, ctx->a);
L8830: ;
    /* static tail transfer */
    goto L8941;
L8941: ;
    native_write(ctx, 0x073cu, native_nz(ctx, (uint8_t)(native_read(ctx, 0x073cu) + 1u)));
    return;
    return;
}

void smbneo_native_fn_bg_text_draw_header_8833(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x8833u);
    ctx->a = native_nz(ctx, 0x00u);
    smbneo_native_fn_bg_text_draw_8a04(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    /* static tail transfer */
    goto L8941;
L8941: ;
    native_write(ctx, 0x073cu, native_nz(ctx, (uint8_t)(native_read(ctx, 0x073cu) + 1u)));
    return;
    return;
}

void smbneo_native_fn_bg_text_draw_splash_883b(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x883bu);
    smbneo_native_fn_misc_proc_coin_update_stats_bb3b(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->x = native_nz(ctx, native_read(ctx, 0x0300u));
    ctx->a = native_nz(ctx, 0x20u);
    native_write(ctx, (uint16_t)(0x0301u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x73u);
    native_write(ctx, (uint16_t)(0x0302u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x03u);
    native_write(ctx, (uint16_t)(0x0303u + ctx->x), ctx->a);
    ctx->y = native_nz(ctx, native_read(ctx, 0x075fu));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->a = native_nz(ctx, ctx->y);
    native_write(ctx, (uint16_t)(0x0304u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x28u);
    native_write(ctx, (uint16_t)(0x0305u + ctx->x), ctx->a);
    ctx->y = native_nz(ctx, native_read(ctx, 0x075cu));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->a = native_nz(ctx, ctx->y);
    native_write(ctx, (uint16_t)(0x0306u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, (uint16_t)(0x0307u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, ctx->x);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x06u);
    native_write(ctx, 0x0300u, ctx->a);
    /* static tail transfer */
    goto L8941;
L8941: ;
    native_write(ctx, 0x073cu, native_nz(ctx, (uint8_t)(native_read(ctx, 0x073cu) + 1u)));
    return;
    return;
}

void smbneo_native_fn_bg_text_draw_timeup_8874(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x8874u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0759u));
    if ((ctx->status & NATIVE_Z)) {
        goto L8883;
    }
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x0759u, ctx->a);
    ctx->a = native_nz(ctx, 0x02u);
    /* static tail transfer */
    goto L88a8;
L8883: ;
    native_write(ctx, 0x073cu, native_nz(ctx, (uint8_t)(native_read(ctx, 0x073cu) + 1u)));
    /* static tail transfer */
    goto L8941;
L88a8: ;
    smbneo_native_fn_bg_text_draw_8a04(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_bg_timer_reset_8aa6(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x0774u, ctx->a);
    return;
L8941: ;
    native_write(ctx, 0x073cu, native_nz(ctx, (uint8_t)(native_read(ctx, 0x073cu) + 1u)));
    return;
    return;
}

void smbneo_native_fn_bg_text_draw_intermission_8889(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x8889u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0770u));
    if ((ctx->status & NATIVE_Z)) {
        goto L88c1;
    }
    native_cmp(ctx, ctx->a, 0x04u);
    if ((ctx->status & NATIVE_Z)) {
        goto L88b4;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x0752u));
    if (!(ctx->status & NATIVE_Z)) {
        goto L88c1;
    }
    ctx->y = native_nz(ctx, native_read(ctx, 0x074eu));
    native_cmp(ctx, ctx->y, 0x03u);
    if ((ctx->status & NATIVE_Z)) {
        goto L88a3;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x0769u));
    if (!(ctx->status & NATIVE_Z)) {
        goto L88c1;
    }
L88a3: ;
    smbneo_native_fn_render_player_course_card_ef09(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, 0x01u);
    smbneo_native_fn_bg_text_draw_8a04(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_bg_timer_reset_8aa6(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x0774u, ctx->a);
    return;
L88b4: ;
    ctx->a = native_nz(ctx, 0x12u);
    native_write(ctx, 0x07a0u, ctx->a);
    ctx->a = native_nz(ctx, 0x03u);
    smbneo_native_fn_bg_text_draw_8a04(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    /* static tail transfer */
    goto L894a;
L88c1: ;
    ctx->a = native_nz(ctx, 0x08u);
    native_write(ctx, 0x073cu, ctx->a);
    return;
L894a: ;
    native_write(ctx, 0x0772u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0772u) + 1u)));
    return;
    return;
}

void smbneo_native_fn_bg_scenery_88c7(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x88c7u);
    native_write(ctx, 0x0774u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0774u) + 1u)));
L88ca: ;
    smbneo_native_fn_scenery_proc_a18b(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, 0x071fu));
    if (!(ctx->status & NATIVE_Z)) {
        goto L88ca;
    }
    native_write(ctx, 0x071eu, native_nz(ctx, (uint8_t)(native_read(ctx, 0x071eu) - 1u)));
    if (!(ctx->status & NATIVE_N)) {
        goto L88da;
    }
    native_write(ctx, 0x073cu, native_nz(ctx, (uint8_t)(native_read(ctx, 0x073cu) + 1u)));
L88da: ;
    ctx->a = native_nz(ctx, 0x06u);
    native_write(ctx, 0x0773u, ctx->a);
    return;
    return;
}

void smbneo_native_fn_bg_title_screen_88f0(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x88f0u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0770u));
    if (!(ctx->status & NATIVE_Z)) {
        goto L894a;
    }
    ctx->y = native_nz(ctx, 0x00u);
    smbneo_native_fn_bg_vschar1_load_88f7(ctx);
    return;
L894a: ;
    native_write(ctx, 0x0772u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0772u) + 1u)));
    return;
    return;
}

void smbneo_native_fn_bg_vschar1_load_88f7(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x88f7u);
    goto L88f7;
L882d: ;
    native_write(ctx, 0x0773u, ctx->a);
    /* static tail transfer */
    goto L8941;
L88f7: ;
    ctx->a = native_nz(ctx, 0x06u);
    native_write(ctx, 0x4016u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x88e0u + ctx->y)));
    native_write(ctx, 0x2006u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x88e8u + ctx->y)));
    native_write(ctx, 0x2006u, ctx->a);
    ctx->a = native_nz(ctx, 0x63u);
    native_write(ctx, 0x01u, ctx->a);
    ctx->y = native_nz(ctx, 0x00u);
    native_write(ctx, 0x00u, ctx->y);
    ctx->a = native_nz(ctx, native_read(ctx, 0x2007u));
L8913: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x2007u));
    native_write(ctx, (uint16_t)(native_zpword(ctx, 0x00u) + ctx->y), ctx->a);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    if (!(ctx->status & NATIVE_Z)) {
        goto L891d;
    }
    native_write(ctx, 0x01u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x01u) + 1u)));
L891d: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x01u));
    native_cmp(ctx, ctx->a, 0x64u);
    if (!(ctx->status & NATIVE_Z)) {
        goto L8913;
    }
    native_cmp(ctx, ctx->y, 0x5au);
    if (!(ctx->status & NATIVE_C)) {
        goto L8913;
    }
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, 0x4016u, ctx->a);
    ctx->a = native_nz(ctx, 0x1cu);
    /* static tail transfer */
    goto L882d;
L8941: ;
    native_write(ctx, 0x073cu, native_nz(ctx, (uint8_t)(native_read(ctx, 0x073cu) + 1u)));
    return;
    return;
}

void smbneo_native_fn_bg_title_cursor_8931(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x8931u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0770u));
    if (!(ctx->status & NATIVE_Z)) {
        goto L894a;
    }
    ctx->x = native_nz(ctx, 0x00u);
L8938: ;
    native_write(ctx, (uint16_t)(0x0300u + ctx->x), ctx->a);
    native_write(ctx, (uint16_t)(0x0400u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x - 1u));
    if (!(ctx->status & NATIVE_Z)) {
        goto L8938;
    }
    native_write(ctx, 0x073cu, native_nz(ctx, (uint8_t)(native_read(ctx, 0x073cu) + 1u)));
    return;
L894a: ;
    native_write(ctx, 0x0772u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0772u) + 1u)));
    return;
    return;
}

void smbneo_native_fn_bg_title_score_8945(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x8945u);
    ctx->a = native_nz(ctx, 0xfau);
    smbneo_native_fn_misc_proc_coin_add_score_bb41(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_write(ctx, 0x0772u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0772u) + 1u)));
    return;
    return;
}

void smbneo_native_fn_bg_text_draw_8a04(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x8a04u);
    uint8_t saved_value_0 = 0u;
    goto L8a04;
L8820: ;
    native_write(ctx, 0x0300u, ctx->a);
    return;
L8a04: ;
    saved_value_0 = ctx->a;
    ctx->a = native_asl(ctx, ctx->a);
    ctx->y = native_nz(ctx, ctx->a);
    native_cmp(ctx, ctx->y, 0x04u);
    if (!(ctx->status & NATIVE_C)) {
        goto L8a17;
    }
    native_cmp(ctx, ctx->y, 0x08u);
    if (!(ctx->status & NATIVE_C)) {
        goto L8a11;
    }
    ctx->y = native_nz(ctx, 0x08u);
L8a11: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x077au));
    if (!(ctx->status & NATIVE_Z)) {
        goto L8a17;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
L8a17: ;
    ctx->x = native_nz(ctx, native_read(ctx, (uint16_t)(0x89fau + ctx->y)));
    ctx->y = native_nz(ctx, 0x00u);
L8a1c: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x894eu + ctx->x)));
    native_cmp(ctx, ctx->a, 0xffu);
    if ((ctx->status & NATIVE_Z)) {
        goto L8a2a;
    }
    native_write(ctx, (uint16_t)(0x0301u + ctx->y), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    if (!(ctx->status & NATIVE_Z)) {
        goto L8a1c;
    }
L8a2a: ;
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, (uint16_t)(0x0301u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, saved_value_0);
    ctx->x = native_nz(ctx, ctx->a);
    native_cmp(ctx, ctx->a, 0x04u);
    if ((ctx->status & NATIVE_C)) {
        goto L8a83;
    }
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x - 1u));
    if (!(ctx->status & NATIVE_Z)) {
        goto L8a5b;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x075au));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x01u);
    native_cmp(ctx, ctx->a, 0x0au);
    if (!(ctx->status & NATIVE_C)) {
        goto L8a49;
    }
    native_sbc(ctx, 0x0au);
    ctx->y = native_nz(ctx, 0x9fu);
    native_write(ctx, 0x0308u, ctx->y);
L8a49: ;
    native_write(ctx, 0x0309u, ctx->a);
    ctx->y = native_nz(ctx, native_read(ctx, 0x075fu));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    native_write(ctx, 0x0314u, ctx->y);
    ctx->y = native_nz(ctx, native_read(ctx, 0x075cu));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    native_write(ctx, 0x0316u, ctx->y);
    return;
L8a5b: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x077au));
    if ((ctx->status & NATIVE_Z)) {
        goto L8a82;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x0753u));
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x - 1u));
    if (!(ctx->status & NATIVE_Z)) {
        goto L8a74;
    }
    ctx->y = native_nz(ctx, native_read(ctx, 0x0770u));
    native_cmp(ctx, ctx->y, 0x04u);
    if ((ctx->status & NATIVE_Z)) {
        goto L8a74;
    }
    ctx->y = native_nz(ctx, native_read(ctx, 0x0761u));
    if ((ctx->status & NATIVE_N)) {
        goto L8a74;
    }
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a ^ 0x01u));
L8a74: ;
    ctx->a = native_lsr(ctx, ctx->a);
    if (!(ctx->status & NATIVE_C)) {
        goto L8a82;
    }
    ctx->y = native_nz(ctx, 0x04u);
L8a79: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x89e9u + ctx->y)));
    native_write(ctx, (uint16_t)(0x0304u + ctx->y), ctx->a);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    if (!(ctx->status & NATIVE_N)) {
        goto L8a79;
    }
L8a82: ;
    return;
L8a83: ;
    native_sbc(ctx, 0x04u);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->x = native_nz(ctx, ctx->a);
    ctx->y = native_nz(ctx, 0x00u);
L8a8a: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x89eeu + ctx->x)));
    native_write(ctx, (uint16_t)(0x031cu + ctx->y), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    native_cmp(ctx, ctx->y, 0x0cu);
    if (!(ctx->status & NATIVE_C)) {
        goto L8a8a;
    }
    ctx->a = native_nz(ctx, 0x2cu);
    /* static tail transfer */
    goto L8820;
    return;
}

void smbneo_native_fn_bg_timer_clear_check_8a9e(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x8a9eu);
    ctx->a = native_nz(ctx, native_read(ctx, 0x07a0u));
    if (!(ctx->status & NATIVE_Z)) {
        goto L8aae;
    }
    smbneo_native_fn_oam_init_826e(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_bg_timer_reset_8aa6(ctx);
    return;
L8aae: ;
    return;
    return;
}

void smbneo_native_fn_bg_timer_reset_8aa6(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x8aa6u);
    ctx->a = native_nz(ctx, 0x07u);
    native_write(ctx, 0x07a0u, ctx->a);
    native_write(ctx, 0x073cu, native_nz(ctx, (uint8_t)(native_read(ctx, 0x073cu) + 1u)));
    return;
    return;
}

void smbneo_native_fn_meta_render_area_8aaf(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x8aafu);
#ifdef SMBNEO_NATIVE_FAST_PATHS
    if (vs_native_fast_meta_render_area(ctx)) return;
#endif
    ctx->a = native_nz(ctx, native_read(ctx, 0x0726u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x01u));
    native_write(ctx, 0x05u, ctx->a);
    ctx->y = native_nz(ctx, native_read(ctx, 0x0340u));
    native_write(ctx, 0x00u, ctx->y);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0721u));
    native_write(ctx, (uint16_t)(0x0342u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0720u));
    native_write(ctx, (uint16_t)(0x0341u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, 0x9au);
    native_write(ctx, (uint16_t)(0x0343u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x04u, ctx->a);
    ctx->x = native_nz(ctx, ctx->a);
L8ad1: ;
    native_write(ctx, 0x01u, ctx->x);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x06a1u + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0xc0u));
    native_write(ctx, 0x03u, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_rol(ctx, ctx->a);
    ctx->a = native_rol(ctx, ctx->a);
    ctx->y = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x8d09u + ctx->y)));
    native_write(ctx, 0x06u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x8d0du + ctx->y)));
    native_write(ctx, 0x07u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x06a1u + ctx->x)));
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    native_write(ctx, 0x02u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x071fu));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x01u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a ^ 0x01u));
    ctx->a = native_asl(ctx, ctx->a);
    native_adc(ctx, native_read(ctx, 0x02u));
    ctx->y = native_nz(ctx, ctx->a);
    ctx->x = native_nz(ctx, native_read(ctx, 0x00u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0x06u) + ctx->y)));
    native_write(ctx, (uint16_t)(0x0344u + ctx->x), ctx->a);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0x06u) + ctx->y)));
    native_write(ctx, (uint16_t)(0x0345u + ctx->x), ctx->a);
    ctx->y = native_nz(ctx, native_read(ctx, 0x04u));
    ctx->a = native_nz(ctx, native_read(ctx, 0x05u));
    if (!(ctx->status & NATIVE_Z)) {
        goto L8b1b;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x01u));
    ctx->a = native_lsr(ctx, ctx->a);
    if ((ctx->status & NATIVE_C)) {
        goto L8b2b;
    }
    native_write(ctx, 0x03u, native_rol(ctx, native_read(ctx, 0x03u)));
    native_write(ctx, 0x03u, native_rol(ctx, native_read(ctx, 0x03u)));
    native_write(ctx, 0x03u, native_rol(ctx, native_read(ctx, 0x03u)));
    /* static tail transfer */
    goto L8b31;
L8b1b: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x01u));
    ctx->a = native_lsr(ctx, ctx->a);
    if ((ctx->status & NATIVE_C)) {
        goto L8b2f;
    }
    native_write(ctx, 0x03u, native_lsr(ctx, native_read(ctx, 0x03u)));
    native_write(ctx, 0x03u, native_lsr(ctx, native_read(ctx, 0x03u)));
    native_write(ctx, 0x03u, native_lsr(ctx, native_read(ctx, 0x03u)));
    native_write(ctx, 0x03u, native_lsr(ctx, native_read(ctx, 0x03u)));
    /* static tail transfer */
    goto L8b31;
L8b2b: ;
    native_write(ctx, 0x03u, native_lsr(ctx, native_read(ctx, 0x03u)));
    native_write(ctx, 0x03u, native_lsr(ctx, native_read(ctx, 0x03u)));
L8b2f: ;
    native_write(ctx, 0x04u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x04u) + 1u)));
L8b31: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x03f9u + ctx->y)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | native_read(ctx, 0x03u)));
    native_write(ctx, (uint16_t)(0x03f9u + ctx->y), ctx->a);
    native_write(ctx, 0x00u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x00u) + 1u)));
    native_write(ctx, 0x00u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x00u) + 1u)));
    ctx->x = native_nz(ctx, native_read(ctx, 0x01u));
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    native_cmp(ctx, ctx->x, 0x0du);
    if (!(ctx->status & NATIVE_C)) {
        goto L8ad1;
    }
    ctx->y = native_nz(ctx, native_read(ctx, 0x00u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, (uint16_t)(0x0341u + ctx->y), ctx->a);
    native_write(ctx, 0x0340u, ctx->y);
    native_write(ctx, 0x0721u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0721u) + 1u)));
    ctx->a = native_nz(ctx, native_read(ctx, 0x0721u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x1fu));
    if (!(ctx->status & NATIVE_Z)) {
        goto L8b68;
    }
    ctx->a = native_nz(ctx, 0x80u);
    native_write(ctx, 0x0721u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0720u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a ^ 0x04u));
    native_write(ctx, 0x0720u, ctx->a);
L8b68: ;
    /* static tail transfer */
    goto L8bbe;
L8bbe: ;
    ctx->a = native_nz(ctx, 0x06u);
    native_write(ctx, 0x0773u, ctx->a);
    return;
    return;
}

void smbneo_native_fn_meta_render_attr_8b6b(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x8b6bu);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0721u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x1fu));
    ctx->status |= NATIVE_C;
    native_sbc(ctx, 0x04u);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x1fu));
    native_write(ctx, 0x01u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0720u));
    if ((ctx->status & NATIVE_C)) {
        goto L8b7e;
    }
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a ^ 0x04u));
L8b7e: ;
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x04u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | 0x23u));
    native_write(ctx, 0x00u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x01u));
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    native_adc(ctx, 0xc0u);
    native_write(ctx, 0x01u, ctx->a);
    ctx->x = native_nz(ctx, 0x00u);
    ctx->y = native_nz(ctx, native_read(ctx, 0x0340u));
L8b91: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x00u));
    native_write(ctx, (uint16_t)(0x0341u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x01u));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x08u);
    native_write(ctx, (uint16_t)(0x0342u + ctx->y), ctx->a);
    native_write(ctx, 0x01u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x03f9u + ctx->x)));
    native_write(ctx, (uint16_t)(0x0344u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, (uint16_t)(0x0343u + ctx->y), ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    native_write(ctx, (uint16_t)(0x03f9u + ctx->x), ctx->a);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    native_cmp(ctx, ctx->x, 0x07u);
    if (!(ctx->status & NATIVE_C)) {
        goto L8b91;
    }
    native_write(ctx, (uint16_t)(0x0341u + ctx->y), ctx->a);
    native_write(ctx, 0x0340u, ctx->y);
    ctx->a = native_nz(ctx, 0x06u);
    native_write(ctx, 0x0773u, ctx->a);
    return;
    return;
}

void smbneo_native_fn_meta_color_rotate_8be2(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x8be2u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x09u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x07u));
    if (!(ctx->status & NATIVE_Z)) {
        goto L8c39;
    }
    ctx->x = native_nz(ctx, native_read(ctx, 0x0300u));
    native_cmp(ctx, ctx->x, 0x21u);
    if ((ctx->status & NATIVE_C)) {
        goto L8c39;
    }
    ctx->y = native_nz(ctx, ctx->a);
L8bf0: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x8bcau + ctx->y)));
    native_write(ctx, (uint16_t)(0x0301u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    native_cmp(ctx, ctx->y, 0x08u);
    if (!(ctx->status & NATIVE_C)) {
        goto L8bf0;
    }
    ctx->x = native_nz(ctx, native_read(ctx, 0x0300u));
    ctx->a = native_nz(ctx, 0x03u);
    native_write(ctx, 0x00u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x074eu));
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->y = native_nz(ctx, ctx->a);
L8c09: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x8bd2u + ctx->y)));
    native_write(ctx, (uint16_t)(0x0304u + ctx->x), ctx->a);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    native_write(ctx, 0x00u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x00u) - 1u)));
    if (!(ctx->status & NATIVE_N)) {
        goto L8c09;
    }
    ctx->x = native_nz(ctx, native_read(ctx, 0x0300u));
    ctx->y = native_nz(ctx, native_read(ctx, 0x06d4u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x8bc4u + ctx->y)));
    native_write(ctx, (uint16_t)(0x0305u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0300u));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x07u);
    native_write(ctx, 0x0300u, ctx->a);
    native_write(ctx, 0x06d4u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x06d4u) + 1u)));
    ctx->a = native_nz(ctx, native_read(ctx, 0x06d4u));
    native_cmp(ctx, ctx->a, 0x06u);
    if (!(ctx->status & NATIVE_C)) {
        goto L8c39;
    }
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x06d4u, ctx->a);
L8c39: ;
    return;
    return;
}

void smbneo_native_fn_meta_put_blank_8c4e(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x8c4eu);
    ctx->y = native_nz(ctx, 0x41u);
    ctx->a = native_nz(ctx, 0x03u);
    ctx->x = native_nz(ctx, native_read(ctx, 0x074eu));
    if (!(ctx->status & NATIVE_Z)) {
        goto L8c59;
    }
    ctx->a = native_nz(ctx, 0x04u);
L8c59: ;
    smbneo_native_fn_meta_clear_put_8c98(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, 0x06u);
    native_write(ctx, 0x0773u, ctx->a);
    return;
    return;
}

void smbneo_native_fn_meta_replace_8c62(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x8c62u);
    smbneo_native_fn_meta_write_8c6e(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_write(ctx, 0x03f0u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x03f0u) + 1u)));
    native_write(ctx, (uint16_t)(0x03ecu + ctx->x), native_nz(ctx, (uint8_t)(native_read(ctx, (uint16_t)(0x03ecu + ctx->x)) - 1u)));
    return;
    return;
}

void smbneo_native_fn_meta_destroy_8c6c(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x8c6cu);
    ctx->a = native_nz(ctx, 0x00u);
    smbneo_native_fn_meta_write_8c6e(ctx);
    return;
    return;
}

void smbneo_native_fn_meta_write_8c6e(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x8c6eu);
    ctx->y = native_nz(ctx, 0x03u);
    native_cmp(ctx, ctx->a, 0x00u);
    if ((ctx->status & NATIVE_Z)) {
        goto L8c88;
    }
    ctx->y = native_nz(ctx, 0x00u);
    native_cmp(ctx, ctx->a, 0x58u);
    if ((ctx->status & NATIVE_Z)) {
        goto L8c88;
    }
    native_cmp(ctx, ctx->a, 0x51u);
    if ((ctx->status & NATIVE_Z)) {
        goto L8c88;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    native_cmp(ctx, ctx->a, 0x5du);
    if ((ctx->status & NATIVE_Z)) {
        goto L8c88;
    }
    native_cmp(ctx, ctx->a, 0x52u);
    if ((ctx->status & NATIVE_Z)) {
        goto L8c88;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
L8c88: ;
    ctx->a = native_nz(ctx, ctx->y);
    ctx->y = native_nz(ctx, native_read(ctx, 0x0300u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    smbneo_native_fn_meta_clear_put_8c98(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_meta_step_8c90(ctx);
    return;
    return;
}

void smbneo_native_fn_meta_step_8c90(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x8c90u);
    goto L8c90;
L8820: ;
    native_write(ctx, 0x0300u, ctx->a);
    return;
L8c90: ;
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    ctx->a = native_nz(ctx, ctx->y);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x0au);
    /* static tail transfer */
    goto L8820;
    return;
}

void smbneo_native_fn_meta_clear_put_8c98(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x8c98u);
    native_write(ctx, 0x00u, ctx->x);
    native_write(ctx, 0x01u, ctx->y);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->x = native_nz(ctx, ctx->a);
    ctx->y = native_nz(ctx, 0x20u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x06u));
    native_cmp(ctx, ctx->a, 0xd0u);
    if (!(ctx->status & NATIVE_C)) {
        goto L8ca9;
    }
    ctx->y = native_nz(ctx, 0x24u);
L8ca9: ;
    native_write(ctx, 0x03u, ctx->y);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x0fu));
    ctx->a = native_asl(ctx, ctx->a);
    native_write(ctx, 0x04u, ctx->a);
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x05u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x02u));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x20u);
    ctx->a = native_asl(ctx, ctx->a);
    native_write(ctx, 0x05u, native_rol(ctx, native_read(ctx, 0x05u)));
    ctx->a = native_asl(ctx, ctx->a);
    native_write(ctx, 0x05u, native_rol(ctx, native_read(ctx, 0x05u)));
    native_adc(ctx, native_read(ctx, 0x04u));
    native_write(ctx, 0x04u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x05u));
    native_adc(ctx, 0x00u);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, 0x03u));
    native_write(ctx, 0x05u, ctx->a);
    ctx->y = native_nz(ctx, native_read(ctx, 0x01u));
    smbneo_native_fn_meta_clear_put_do_8cce(ctx);
    return;
    return;
}

void smbneo_native_fn_meta_clear_put_do_8cce(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x8cceu);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x8c3au + ctx->x)));
    native_write(ctx, (uint16_t)(0x0303u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x8c3bu + ctx->x)));
    native_write(ctx, (uint16_t)(0x0304u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x8c3cu + ctx->x)));
    native_write(ctx, (uint16_t)(0x0308u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x8c3du + ctx->x)));
    native_write(ctx, (uint16_t)(0x0309u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x04u));
    native_write(ctx, (uint16_t)(0x0301u + ctx->y), ctx->a);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x20u);
    native_write(ctx, (uint16_t)(0x0306u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x05u));
    native_write(ctx, (uint16_t)(0x0300u + ctx->y), ctx->a);
    native_write(ctx, (uint16_t)(0x0305u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, (uint16_t)(0x0302u + ctx->y), ctx->a);
    native_write(ctx, (uint16_t)(0x0307u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, (uint16_t)(0x030au + ctx->y), ctx->a);
    ctx->x = native_nz(ctx, native_read(ctx, 0x00u));
    return;
    return;
}

void smbneo_native_fn_tbljmp_90bb(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x90bbu);
    /* All table selections are statically expanded at their call sites. */
    ctx->unsupported = 1;
    return;
}

void smbneo_native_fn_nt_init_90d0(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x90d0u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x2002u));
    ctx->a = native_nz(ctx, native_read(ctx, 0x0778u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | 0x10u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0xf0u));
    smbneo_native_fn_ppu_displist_write_ctlr0_91a5(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, 0x24u);
    smbneo_native_fn_nt_init_do_90e4(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, 0x20u);
    smbneo_native_fn_nt_init_do_90e4(ctx);
    return;
    return;
}

void smbneo_native_fn_nt_init_do_90e4(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x90e4u);
    native_write(ctx, 0x2006u, ctx->a);
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x2006u, ctx->a);
    ctx->x = native_nz(ctx, 0x04u);
    ctx->y = native_nz(ctx, 0xc0u);
    ctx->a = native_nz(ctx, 0x24u);
L90f2: ;
    native_write(ctx, 0x2007u, ctx->a);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    if (!(ctx->status & NATIVE_Z)) {
        goto L90f2;
    }
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x - 1u));
    if (!(ctx->status & NATIVE_Z)) {
        goto L90f2;
    }
    ctx->y = native_nz(ctx, 0x40u);
    ctx->a = native_nz(ctx, ctx->x);
    native_write(ctx, 0x0300u, ctx->a);
    native_write(ctx, 0x0301u, ctx->a);
L9104: ;
    native_write(ctx, 0x2007u, ctx->a);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    if (!(ctx->status & NATIVE_Z)) {
        goto L9104;
    }
    native_write(ctx, 0x073fu, ctx->a);
    native_write(ctx, 0x0740u, ctx->a);
    /* static tail transfer */
    smbneo_native_fn_ppu_displist_write_scc_h_v_919e(ctx);
    return;
    return;
}

void smbneo_native_fn_joypad_read_9113(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x9113u);
    ctx->a = native_nz(ctx, 0x03u);
    native_write(ctx, 0x4016u, ctx->a);
    ctx->a = native_nz(ctx, 0x02u);
    ctx->x = native_nz(ctx, 0x00u);
    native_write(ctx, 0x4016u, ctx->a);
    ctx->a = native_nz(ctx, 0x00u);
    smbneo_native_fn_joypad_read_do_9125(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    smbneo_native_fn_joypad_read_do_9125(ctx);
    return;
    return;
}

void smbneo_native_fn_joypad_read_do_9125(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x9125u);
    uint8_t saved_value_0 = 0u;
    ctx->y = native_nz(ctx, 0x08u);
L9127: ;
    saved_value_0 = ctx->a;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x4016u + ctx->x)));
    native_write(ctx, 0x00u, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_nz(ctx, saved_value_0);
    ctx->a = native_rol(ctx, ctx->a);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    if (!(ctx->status & NATIVE_Z)) {
        goto L9127;
    }
    native_write(ctx, (uint16_t)(0x06fcu + ctx->x), ctx->a);
    saved_value_0 = ctx->a;
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x30u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & native_read(ctx, (uint16_t)(0x074au + ctx->x))));
    if ((ctx->status & NATIVE_Z)) {
        goto L9145;
    }
    ctx->a = native_nz(ctx, saved_value_0);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0xcfu));
    native_write(ctx, (uint16_t)(0x06fcu + ctx->x), ctx->a);
    return;
L9145: ;
    ctx->a = native_nz(ctx, saved_value_0);
    native_write(ctx, (uint16_t)(0x074au + ctx->x), ctx->a);
    return;
    return;
}

void smbneo_native_fn_ppu_displist_write_9195(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x9195u);
    uint8_t saved_value_0 = 0u;
    goto L9195;
L914a: ;
#ifdef SMBNEO_NATIVE_FAST_PATHS
    if (vs_native_fast_ppu_displist_command(ctx, ctx->fast_video_hooks)) goto L9195;
#endif
    native_write(ctx, 0x2006u, ctx->a);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0x00u) + ctx->y)));
    native_write(ctx, 0x2006u, ctx->a);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0x00u) + ctx->y)));
    ctx->a = native_asl(ctx, ctx->a);
    saved_value_0 = ctx->a;
    ctx->a = native_nz(ctx, native_read(ctx, 0x0778u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | 0x04u));
    if ((ctx->status & NATIVE_C)) {
        goto L9161;
    }
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0xfbu));
L9161: ;
    smbneo_native_fn_ppu_displist_write_ctlr0_91a5(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, saved_value_0);
    ctx->a = native_asl(ctx, ctx->a);
    if (!(ctx->status & NATIVE_C)) {
        goto L916b;
    }
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | 0x02u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
L916b: ;
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->x = native_nz(ctx, ctx->a);
L916e: ;
    if ((ctx->status & NATIVE_C)) {
        goto L9171;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
L9171: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0x00u) + ctx->y)));
    native_write(ctx, 0x2007u, ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x - 1u));
    if (!(ctx->status & NATIVE_Z)) {
        goto L916e;
    }
    ctx->status |= NATIVE_C;
    ctx->a = native_nz(ctx, ctx->y);
    native_adc(ctx, native_read(ctx, 0x00u));
    native_write(ctx, 0x00u, ctx->a);
    ctx->a = native_nz(ctx, 0x00u);
    native_adc(ctx, native_read(ctx, 0x01u));
    native_write(ctx, 0x01u, ctx->a);
    ctx->a = native_nz(ctx, 0x3fu);
    native_write(ctx, 0x2006u, ctx->a);
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x2006u, ctx->a);
    native_write(ctx, 0x2006u, ctx->a);
    native_write(ctx, 0x2006u, ctx->a);
L9195: ;
    ctx->x = native_nz(ctx, native_read(ctx, 0x2002u));
    ctx->y = native_nz(ctx, 0x00u);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0x00u) + ctx->y)));
    if (!(ctx->status & NATIVE_Z)) {
        goto L914a;
    }
    smbneo_native_fn_ppu_displist_write_scc_h_v_919e(ctx);
    return;
    return;
}

void smbneo_native_fn_ppu_displist_write_scc_h_v_919e(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x919eu);
    native_write(ctx, 0x2005u, ctx->a);
    native_write(ctx, 0x2005u, ctx->a);
    return;
    return;
}

void smbneo_native_fn_ppu_displist_write_ctlr0_91a5(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x91a5u);
    native_write(ctx, 0x2000u, ctx->a);
    native_write(ctx, 0x0778u, ctx->a);
    return;
    return;
}

void smbneo_native_fn_stats_print_num_91be(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x91beu);
    native_write(ctx, 0x00u, ctx->a);
    smbneo_native_fn_stats_print_num_do_91c9(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, 0x00u));
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    smbneo_native_fn_stats_print_num_do_91c9(ctx);
    return;
    return;
}

void smbneo_native_fn_stats_print_num_do_91c9(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x91c9u);
    uint8_t saved_value_0 = 0u;
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x01u);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x0fu));
    native_cmp(ctx, ctx->a, 0x06u);
    if ((ctx->status & NATIVE_C)) {
        goto L9216;
    }
    saved_value_0 = ctx->a;
    ctx->a = native_asl(ctx, ctx->a);
    ctx->y = native_nz(ctx, ctx->a);
    ctx->x = native_nz(ctx, native_read(ctx, 0x0300u));
    ctx->a = native_nz(ctx, 0x20u);
    native_cmp(ctx, ctx->y, 0x00u);
    if (!(ctx->status & NATIVE_Z)) {
        goto L91e0;
    }
    ctx->a = native_nz(ctx, 0x22u);
L91e0: ;
    native_write(ctx, (uint16_t)(0x0301u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x91acu + ctx->y)));
    native_write(ctx, (uint16_t)(0x0302u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x91adu + ctx->y)));
    native_write(ctx, (uint16_t)(0x0303u + ctx->x), ctx->a);
    native_write(ctx, 0x03u, ctx->a);
    native_write(ctx, 0x02u, ctx->x);
    ctx->a = native_nz(ctx, saved_value_0);
    ctx->x = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x91b8u + ctx->x)));
    ctx->status |= NATIVE_C;
    native_sbc(ctx, native_read(ctx, (uint16_t)(0x91adu + ctx->y)));
    ctx->y = native_nz(ctx, ctx->a);
    ctx->x = native_nz(ctx, native_read(ctx, 0x02u));
L91ff: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x07d7u + ctx->y)));
    native_write(ctx, (uint16_t)(0x0304u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    native_write(ctx, 0x03u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x03u) - 1u)));
    if (!(ctx->status & NATIVE_Z)) {
        goto L91ff;
    }
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, (uint16_t)(0x0304u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    native_write(ctx, 0x0300u, ctx->x);
L9216: ;
    return;
    return;
}

void smbneo_native_fn_stats_calc_9217(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x9217u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0770u));
    native_cmp(ctx, ctx->a, 0x00u);
    if ((ctx->status & NATIVE_Z)) {
        goto L9234;
    }
    ctx->x = native_nz(ctx, 0x05u);
L9220: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0134u + ctx->x)));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, (uint16_t)(0x07d7u + ctx->y)));
    if ((ctx->status & NATIVE_N)) {
        goto L923f;
    }
    native_cmp(ctx, ctx->a, 0x0au);
    if ((ctx->status & NATIVE_C)) {
        goto L9246;
    }
L922d: ;
    native_write(ctx, (uint16_t)(0x07d7u + ctx->y), ctx->a);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x - 1u));
    if (!(ctx->status & NATIVE_N)) {
        goto L9220;
    }
L9234: ;
    ctx->a = native_nz(ctx, 0x00u);
    ctx->x = native_nz(ctx, 0x06u);
L9238: ;
    native_write(ctx, (uint16_t)(0x0133u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x - 1u));
    if (!(ctx->status & NATIVE_N)) {
        goto L9238;
    }
    return;
L923f: ;
    native_write(ctx, (uint16_t)(0x0133u + ctx->x), native_nz(ctx, (uint8_t)(native_read(ctx, (uint16_t)(0x0133u + ctx->x)) - 1u)));
    ctx->a = native_nz(ctx, 0x09u);
    if (!(ctx->status & NATIVE_Z)) {
        goto L922d;
    }
L9246: ;
    ctx->status |= NATIVE_C;
    native_sbc(ctx, 0x0au);
    native_write(ctx, (uint16_t)(0x0133u + ctx->x), native_nz(ctx, (uint8_t)(native_read(ctx, (uint16_t)(0x0133u + ctx->x)) + 1u)));
    /* static tail transfer */
    goto L922d;
    return;
}

void smbneo_native_fn_stats_score_hi_check_924f(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x924fu);
    ctx->x = native_nz(ctx, 0x05u);
    smbneo_native_fn_stats_score_hi_check_do_9256(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->x = native_nz(ctx, 0x0bu);
    smbneo_native_fn_stats_score_hi_check_do_9256(ctx);
    return;
    return;
}

void smbneo_native_fn_stats_score_hi_check_do_9256(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x9256u);
    ctx->y = native_nz(ctx, 0x05u);
    ctx->status |= NATIVE_C;
L9259: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x07ddu + ctx->x)));
    native_sbc(ctx, native_read(ctx, (uint16_t)(0x07d7u + ctx->y)));
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x - 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    if (!(ctx->status & NATIVE_N)) {
        goto L9259;
    }
    if (!(ctx->status & NATIVE_C)) {
        goto L9273;
    }
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
L9267: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x07ddu + ctx->x)));
    native_write(ctx, (uint16_t)(0x07d7u + ctx->y), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    native_cmp(ctx, ctx->y, 0x06u);
    if (!(ctx->status & NATIVE_C)) {
        goto L9267;
    }
L9273: ;
    return;
    return;
}

void smbneo_native_fn_vs_read_dips_9274(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x9274u);
    uint8_t saved_value_0 = 0u;
    smbneo_native_fn_vs_setbank_low_820c(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, 0x4016u));
    native_write(ctx, 0x6600u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x4017u));
    native_write(ctx, 0x6601u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x6600u));
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x03u));
    native_write(ctx, 0x00u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x6601u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0xfcu));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | native_read(ctx, 0x00u)));
    ctx->y = native_nz(ctx, 0x08u);
L9296: ;
    ctx->a = native_asl(ctx, ctx->a);
    native_write(ctx, 0x01u, native_ror(ctx, native_read(ctx, 0x01u)));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    if (!(ctx->status & NATIVE_Z)) {
        goto L9296;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x01u));
    saved_value_0 = ctx->a;
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x01u));
    native_write(ctx, 0x6604u, ctx->a);
    ctx->a = native_nz(ctx, saved_value_0);
    ctx->a = native_lsr(ctx, ctx->a);
    saved_value_0 = ctx->a;
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x01u));
    native_write(ctx, 0x6603u, ctx->a);
    ctx->a = native_nz(ctx, saved_value_0);
    ctx->a = native_lsr(ctx, ctx->a);
    saved_value_0 = ctx->a;
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x03u));
    native_write(ctx, 0x6602u, ctx->a);
    ctx->a = native_nz(ctx, saved_value_0);
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    saved_value_0 = ctx->a;
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x01u));
    native_write(ctx, 0x6605u, ctx->a);
    ctx->a = native_nz(ctx, saved_value_0);
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x07u));
    native_write(ctx, 0x6606u, ctx->a);
    return;
    return;
}

void smbneo_native_fn_vs_disp_credit_msg_92e1(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x92e1u);
    smbneo_native_fn_vs_setbank_low_820c(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->x = native_nz(ctx, 0x0du);
    ctx->a = native_nz(ctx, native_read(ctx, 0x6606u));
    native_cmp(ctx, ctx->a, 0x07u);
    if (!(ctx->status & NATIVE_Z)) {
        goto L92f7;
    }
L92ed: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x92d3u + ctx->x)));
    native_write(ctx, (uint16_t)(0x0300u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x - 1u));
    if (!(ctx->status & NATIVE_N)) {
        goto L92ed;
    }
    return;
L92f7: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x92c5u + ctx->x)));
    native_write(ctx, (uint16_t)(0x0300u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x - 1u));
    if (!(ctx->status & NATIVE_N)) {
        goto L92f7;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x6610u));
L9303: ;
    native_cmp(ctx, ctx->a, 0x0au);
    if ((ctx->status & NATIVE_N)) {
        goto L9310;
    }
    native_write(ctx, 0x030bu, native_nz(ctx, (uint8_t)(native_read(ctx, 0x030bu) + 1u)));
    ctx->status |= NATIVE_C;
    native_sbc(ctx, 0x0au);
    /* static tail transfer */
    goto L9303;
L9310: ;
    native_write(ctx, 0x030cu, ctx->a);
    return;
    return;
}

void smbneo_native_fn_sub_9324_9324(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x9324u);
    smbneo_native_fn_vs_setbank_low_820c(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, 0x6608u));
    if (!(ctx->status & NATIVE_Z)) {
        goto L9342;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x660cu));
    if ((ctx->status & NATIVE_Z)) {
        goto L9341;
    }
    native_write(ctx, 0x660cu, native_nz(ctx, (uint8_t)(native_read(ctx, 0x660cu) - 1u)));
    ctx->a = native_nz(ctx, 0x04u);
    native_write(ctx, 0x6613u, ctx->a);
    native_write(ctx, 0x6608u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x6608u) + 1u)));
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, 0x4020u, ctx->a);
L9341: ;
    return;
L9342: ;
    native_write(ctx, 0x6613u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x6613u) - 1u)));
    if (!(ctx->status & NATIVE_Z)) {
        goto L9341;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x6608u));
    native_cmp(ctx, ctx->a, 0x01u);
    if (!(ctx->status & NATIVE_Z)) {
        goto L935c;
    }
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x4020u, ctx->a);
    ctx->a = native_nz(ctx, 0x06u);
    native_write(ctx, 0x6613u, ctx->a);
    native_write(ctx, 0x6608u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x6608u) + 1u)));
    return;
L935c: ;
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x6608u, ctx->a);
    return;
    return;
}

void smbneo_native_fn_sub_9362_9362(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x9362u);
    smbneo_native_fn_vs_setbank_low_820c(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, 0x6609u));
    if (!(ctx->status & NATIVE_Z)) {
        goto L9377;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x4016u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x04u));
    if ((ctx->status & NATIVE_Z)) {
        goto L9376;
    }
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, 0x6609u, ctx->a);
L9376: ;
    return;
L9377: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x4016u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x04u));
    if (!(ctx->status & NATIVE_Z)) {
        goto L9376;
    }
    ctx->x = native_nz(ctx, 0x02u);
    smbneo_native_fn_sub_93E7_93e7(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x6609u, ctx->a);
    return;
    return;
}

void smbneo_native_fn_sub_9389_9389(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x9389u);
    smbneo_native_fn_vs_setbank_low_820c(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, 0x6606u));
    native_cmp(ctx, ctx->a, 0x07u);
    if (!(ctx->status & NATIVE_Z)) {
        goto L9398;
    }
    ctx->a = native_nz(ctx, 0x20u);
    native_write(ctx, 0x6610u, ctx->a);
L9398: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x4016u));
    native_write(ctx, 0x00u, ctx->a);
    ctx->x = native_nz(ctx, 0x00u);
    smbneo_native_fn_sub_9389_do_93a6(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->x = native_nz(ctx, 0x01u);
    native_write(ctx, 0x00u, native_lsr(ctx, native_read(ctx, 0x00u)));
    smbneo_native_fn_sub_9389_do_93a6(ctx);
    return;
    return;
}

void smbneo_native_fn_sub_9389_do_93a6(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x93a6u);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x660au + ctx->x)));
    if (!(ctx->status & NATIVE_Z)) {
        goto L93bc;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x00u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x20u));
    if ((ctx->status & NATIVE_Z)) {
        goto L93bb;
    }
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, (uint16_t)(0x660au + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x0fu);
    native_write(ctx, (uint16_t)(0x6614u + ctx->x), ctx->a);
L93bb: ;
    return;
L93bc: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x00u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x20u));
    if (!(ctx->status & NATIVE_Z)) {
        goto L93d5;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x660au + ctx->x)));
    native_cmp(ctx, ctx->a, 0xffu);
    if ((ctx->status & NATIVE_Z)) {
        goto L93cf;
    }
    smbneo_native_fn_sub_93E0_93e0(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_write(ctx, 0x660cu, native_nz(ctx, (uint8_t)(native_read(ctx, 0x660cu) + 1u)));
L93cf: ;
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, (uint16_t)(0x660au + ctx->x), ctx->a);
    return;
L93d5: ;
    native_write(ctx, (uint16_t)(0x6614u + ctx->x), native_nz(ctx, (uint8_t)(native_read(ctx, (uint16_t)(0x6614u + ctx->x)) - 1u)));
    if (!(ctx->status & NATIVE_Z)) {
        goto L93df;
    }
    ctx->a = native_nz(ctx, 0xffu);
    native_write(ctx, (uint16_t)(0x660au + ctx->x), ctx->a);
L93df: ;
    return;
    return;
}

void smbneo_native_fn_sub_93E0_93e0(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x93e0u);
    smbneo_native_fn_vs_setbank_low_820c(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, 0xfeu, ctx->a);
    smbneo_native_fn_sub_93E7_93e7(ctx);
    return;
    return;
}

void smbneo_native_fn_sub_93E7_93e7(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x93e7u);
    uint8_t saved_value_0 = 0u;
    ctx->a = native_nz(ctx, native_read(ctx, 0x6606u));
    native_cmp(ctx, ctx->a, 0x07u);
    if ((ctx->status & NATIVE_Z)) {
        goto L941d;
    }
    native_write(ctx, (uint16_t)(0x660du + ctx->x), native_nz(ctx, (uint8_t)(native_read(ctx, (uint16_t)(0x660du + ctx->x)) + 1u)));
    ctx->a = native_nz(ctx, ctx->x);
    saved_value_0 = ctx->a;
    ctx->a = native_nz(ctx, 0x00u);
    ctx->x = native_nz(ctx, ctx->a);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, 0x6606u));
    ctx->y = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, saved_value_0);
    ctx->x = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x9314u + ctx->y)));
    native_cmp(ctx, ctx->a, native_read(ctx, (uint16_t)(0x660du + ctx->x)));
    if ((ctx->status & NATIVE_Z)) {
        goto L9407;
    }
    if (!(ctx->status & NATIVE_N)) {
        goto L941d;
    }
L9407: ;
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, (uint16_t)(0x660du + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x6610u));
    native_cmp(ctx, ctx->a, 0x5cu);
    if (!(ctx->status & NATIVE_N)) {
        goto L941d;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x931cu + ctx->y)));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, 0x6610u));
    native_write(ctx, 0x6610u, ctx->a);
L941d: ;
    return;
    return;
}

void smbneo_native_fn_sys_vs_player_select_941e(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x941eu);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0772u));
    uint8_t table_selector_9421 = ctx->a;
    ctx->a = native_asl(ctx, ctx->a);
    ctx->y = native_nz(ctx, ctx->a);
    native_write(ctx, 0x04u, 0x23u);
    native_write(ctx, 0x05u, 0x94u);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    switch (table_selector_9421) {
    case 0: native_write(ctx, 0x06u, 0x2cu);
             native_write(ctx, 0x07u, 0x94u);
             ctx->a = native_nz(ctx, 0x94u);
             smbneo_native_fn_vs_player_select_init_0_942c(ctx); return;
    case 1: native_write(ctx, 0x06u, 0x3eu);
             native_write(ctx, 0x07u, 0x94u);
             ctx->a = native_nz(ctx, 0x94u);
             smbneo_native_fn_vs_player_select_scr_943e(ctx); return;
    case 2: native_write(ctx, 0x06u, 0x54u);
             native_write(ctx, 0x07u, 0x94u);
             ctx->a = native_nz(ctx, 0x94u);
             smbneo_native_fn_vs_player_select_init_1_9454(ctx); return;
    case 3: native_write(ctx, 0x06u, 0x7au);
             native_write(ctx, 0x07u, 0x94u);
             ctx->a = native_nz(ctx, 0x94u);
             smbneo_native_fn_vs_player_select_do_947a(ctx); return;
    default: ctx->unsupported = 1; return;
    }
    return;
}

void smbneo_native_fn_vs_player_select_init_0_942c(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x942cu);
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x073cu, ctx->a);
    native_write(ctx, 0x0722u, ctx->a);
    smbneo_native_fn_oam_init_826e(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_write(ctx, 0x0774u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0774u) + 1u)));
    native_write(ctx, 0x0772u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0772u) + 1u)));
    return;
    return;
}

void smbneo_native_fn_vs_player_select_scr_943e(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x943eu);
    smbneo_native_fn_nt_init_90d0(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->y = native_nz(ctx, 0x01u);
    smbneo_native_fn_bg_vschar1_load_88f7(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x6627u, ctx->a);
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, 0x6626u, ctx->a);
    native_write(ctx, 0x0772u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0772u) + 1u)));
    return;
    return;
}

void smbneo_native_fn_vs_player_select_init_1_9454(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x9454u);
    native_write(ctx, 0x0772u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0772u) + 1u)));
    ctx->a = native_nz(ctx, native_read(ctx, 0x6610u));
    native_cmp(ctx, ctx->a, 0x01u);
    if ((ctx->status & NATIVE_Z)) {
        goto L9463;
    }
    ctx->a = native_nz(ctx, 0x26u);
    native_write(ctx, 0x0773u, ctx->a);
L9463: ;
    return;
    return;
}

void smbneo_native_fn_vs_player_select_do_947a(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x947au);
    goto L947a;
L82fe: ;
    ctx->a = native_asl(ctx, ctx->a);
    if (!(ctx->status & NATIVE_C)) {
        goto L8307;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x07fdu));
    smbneo_native_fn_title_course_set_832f(ctx);
    if (ctx->suspended || ctx->unsupported) return;
L8307: ;
    smbneo_native_fn_course_descriptor_load_ab60(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_write(ctx, 0x075du, native_nz(ctx, (uint8_t)(native_read(ctx, 0x075du) + 1u)));
    native_write(ctx, 0x0764u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0764u) + 1u)));
    native_write(ctx, 0x0757u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0757u) + 1u)));
    native_write(ctx, 0x0770u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0770u) + 1u)));
    ctx->a = native_nz(ctx, native_read(ctx, 0x07fcu));
    native_write(ctx, 0x076au, ctx->a);
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x0772u, ctx->a);
    native_write(ctx, 0x07a2u, ctx->a);
    ctx->x = native_nz(ctx, 0x17u);
    ctx->a = native_nz(ctx, 0x00u);
L8328: ;
    native_write(ctx, (uint16_t)(0x07ddu + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x - 1u));
    if (!(ctx->status & NATIVE_N)) {
        goto L8328;
    }
    return;
L9457: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x6610u));
    native_cmp(ctx, ctx->a, 0x01u);
    if ((ctx->status & NATIVE_Z)) {
        goto L9463;
    }
    ctx->a = native_nz(ctx, 0x26u);
    native_write(ctx, 0x0773u, ctx->a);
L9463: ;
    return;
L947a: ;
    smbneo_native_fn_vs_setbank_low_820c(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x0774u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x06fcu));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x20u));
    if ((ctx->status & NATIVE_Z)) {
        goto L948e;
    }
    ctx->a = native_nz(ctx, 0x00u);
    /* static tail transfer */
    goto L94a1;
L948e: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x06fdu));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x20u));
    if ((ctx->status & NATIVE_Z)) {
        goto L94bb;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x6610u));
    native_cmp(ctx, ctx->a, 0x01u);
    if ((ctx->status & NATIVE_Z)) {
        goto L94bb;
    }
    native_write(ctx, 0x6610u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x6610u) - 1u)));
    ctx->a = native_nz(ctx, 0x01u);
L94a1: ;
    native_write(ctx, 0x077au, ctx->a);
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x6628u, ctx->a);
    native_write(ctx, 0x6629u, ctx->a);
    native_write(ctx, 0x6610u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x6610u) - 1u)));
    smbneo_native_fn_game_init_ram_9643(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_game_init_setup_96e4(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, 0x06fcu));
    /* static tail transfer */
    goto L82fe;
L94bb: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x09u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x01u));
    if ((ctx->status & NATIVE_Z)) {
        goto L94ff;
    }
    smbneo_native_fn_vs_disp_credit_msg_92e1(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_write(ctx, 0x6626u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x6626u) - 1u)));
    if (!(ctx->status & NATIVE_Z)) {
        goto L94fe;
    }
    ctx->x = native_nz(ctx, native_read(ctx, 0x6627u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x9464u + ctx->x)));
    native_write(ctx, 0x6626u, ctx->a);
    ctx->x = native_nz(ctx, 0x00u);
    ctx->y = native_nz(ctx, native_read(ctx, 0x0300u));
    ctx->a = native_nz(ctx, native_read(ctx, 0x6627u));
    if (!(ctx->status & NATIVE_Z)) {
        goto L94ea;
    }
L94dc: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x9466u + ctx->x)));
    native_write(ctx, (uint16_t)(0x0301u + ctx->y), ctx->a);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    native_cmp(ctx, ctx->x, 0x10u);
    if (!(ctx->status & NATIVE_Z)) {
        goto L94dc;
    }
    if ((ctx->status & NATIVE_Z)) {
        goto L94f6;
    }
L94ea: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x9475u + ctx->x)));
    native_write(ctx, (uint16_t)(0x0301u + ctx->y), ctx->a);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    native_cmp(ctx, ctx->x, 0x05u);
    if (!(ctx->status & NATIVE_Z)) {
        goto L94ea;
    }
L94f6: ;
    ctx->a = native_nz(ctx, 0x01u);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a ^ native_read(ctx, 0x6627u)));
    native_write(ctx, 0x6627u, ctx->a);
L94fe: ;
    return;
L94ff: ;
    /* static tail transfer */
    goto L9457;
    return;
}

void smbneo_native_fn_vs_title_ppu_init_9502(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x9502u);
    smbneo_native_fn_oam_init_826e(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_nt_init_90d0(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, 0xffu);
    native_write(ctx, 0x6616u, ctx->a);
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, 0x0773u, ctx->a);
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x6622u, ctx->a);
    native_write(ctx, 0x0772u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0772u) + 1u)));
    return;
    return;
}

void smbneo_native_fn_vs_title_super_players_953b(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x953bu);
    ctx->a = native_nz(ctx, native_read(ctx, 0x6622u));
    if (!(ctx->status & NATIVE_Z)) {
        goto L9548;
    }
    ctx->a = native_nz(ctx, 0x24u);
    native_write(ctx, 0x0773u, ctx->a);
    /* static tail transfer */
    goto L9551;
L9548: ;
    native_cmp(ctx, ctx->a, 0x01u);
    if (!(ctx->status & NATIVE_Z)) {
        goto L9555;
    }
    ctx->y = native_nz(ctx, 0x07u);
    smbneo_native_fn_bg_vschar1_load_88f7(ctx);
    if (ctx->suspended || ctx->unsupported) return;
L9551: ;
    native_write(ctx, 0x6622u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x6622u) + 1u)));
    return;
L9555: ;
    ctx->x = native_nz(ctx, 0x00u);
    native_write(ctx, 0x04u, ctx->x);
L9559: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x04u));
    native_cmp(ctx, ctx->a, 0x0au);
    if ((ctx->status & NATIVE_Z)) {
        goto L9565;
    }
    smbneo_native_fn_sub_9D62_9d62(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    /* static tail transfer */
    goto L9559;
L9565: ;
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, (uint16_t)(0x6000u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x1eu);
    native_write(ctx, 0x0773u, ctx->a);
    ctx->y = native_nz(ctx, 0x1fu);
L9571: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x951bu + ctx->y)));
    native_write(ctx, (uint16_t)(0x0200u + ctx->y), ctx->a);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    if (!(ctx->status & NATIVE_N)) {
        goto L9571;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x6617u));
    native_cmp(ctx, ctx->a, 0x06u);
    if (!(ctx->status & NATIVE_Z)) {
        goto L9584;
    }
    /* static tail transfer */
    smbneo_native_fn_sub_9DF7_9df7(ctx);
    return;
L9584: ;
    native_write(ctx, 0x0772u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0772u) + 1u)));
    return;
    return;
}

void smbneo_native_fn_vs_title_tick_9588(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x9588u);
    goto L9588;
L82ef: ;
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x0770u, ctx->a);
    native_write(ctx, 0x0772u, ctx->a);
    native_write(ctx, 0x0722u, ctx->a);
    native_write(ctx, 0x0774u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0774u) + 1u)));
    return;
L9588: ;
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x0774u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x09u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x01u));
    if (!(ctx->status & NATIVE_Z)) {
        goto L959b;
    }
    native_write(ctx, 0x6616u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x6616u) - 1u)));
    if (!(ctx->status & NATIVE_Z)) {
        goto L959b;
    }
    /* static tail transfer */
    goto L82ef;
L959b: ;
    return;
    return;
}

void smbneo_native_fn_game_init_ram_9643(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x9643u);
    ctx->y = native_nz(ctx, 0x6fu);
    smbneo_native_fn_game_init_clearram_975d(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->y = native_nz(ctx, 0x1fu);
L964a: ;
    native_write(ctx, (uint16_t)(0x07b0u + ctx->y), ctx->a);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    if (!(ctx->status & NATIVE_N)) {
        goto L964a;
    }
    return;
    return;
}

void smbneo_native_fn_game_init_0_9651(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x9651u);
    smbneo_native_fn_game_init_ram_9643(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, 0x18u);
    native_write(ctx, 0x07a2u, ctx->a);
    smbneo_native_fn_course_descriptor_load_ab60(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_game_init_0_do_965c(ctx);
    return;
    return;
}

void smbneo_native_fn_game_init_0_do_965c(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x965cu);
    ctx->y = native_nz(ctx, 0x4bu);
    smbneo_native_fn_game_init_clearram_975d(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->x = native_nz(ctx, 0x21u);
    ctx->a = native_nz(ctx, 0x00u);
L9665: ;
    native_write(ctx, (uint16_t)(0x0780u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x - 1u));
    if (!(ctx->status & NATIVE_N)) {
        goto L9665;
    }
    native_write(ctx, 0x09u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x8190u));
    native_write(ctx, 0x077fu, ctx->a);
    native_write(ctx, 0x07a7u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x075bu));
    ctx->y = native_nz(ctx, native_read(ctx, 0x0752u));
    if ((ctx->status & NATIVE_Z)) {
        goto L9681;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x0751u));
L9681: ;
    native_write(ctx, 0x071au, ctx->a);
    native_write(ctx, 0x0725u, ctx->a);
    native_write(ctx, 0x0728u, ctx->a);
    smbneo_native_fn_game_screen_pos_x_calc_aee5(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->y = native_nz(ctx, 0x20u);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x01u));
    if ((ctx->status & NATIVE_Z)) {
        goto L9695;
    }
    ctx->y = native_nz(ctx, 0x24u);
L9695: ;
    native_write(ctx, 0x0720u, ctx->y);
    ctx->y = native_nz(ctx, 0x80u);
    native_write(ctx, 0x0721u, ctx->y);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    native_write(ctx, 0x06a0u, ctx->a);
    native_write(ctx, 0x0730u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0730u) - 1u)));
    native_write(ctx, 0x0731u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0731u) - 1u)));
    native_write(ctx, 0x0732u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0732u) - 1u)));
    ctx->a = native_nz(ctx, 0x0bu);
    native_write(ctx, 0x071eu, ctx->a);
    smbneo_native_fn_course_load_ab7f(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, 0x076au));
    if (!(ctx->status & NATIVE_Z)) {
        goto L96ca;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x075fu));
    native_cmp(ctx, ctx->a, 0x03u);
    if (!(ctx->status & NATIVE_C)) {
        goto L96cd;
    }
    if (!(ctx->status & NATIVE_Z)) {
        goto L96ca;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x075cu));
    native_cmp(ctx, ctx->a, 0x01u);
    if (!(ctx->status & NATIVE_C)) {
        goto L96cd;
    }
L96ca: ;
    native_write(ctx, 0x06ccu, native_nz(ctx, (uint8_t)(native_read(ctx, 0x06ccu) + 1u)));
L96cd: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x075bu));
    if ((ctx->status & NATIVE_Z)) {
        goto L96d7;
    }
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, 0x0710u, ctx->a);
L96d7: ;
    ctx->a = native_nz(ctx, 0x80u);
    native_write(ctx, 0xfbu, ctx->a);
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, 0x0774u, ctx->a);
    native_write(ctx, 0x0772u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0772u) + 1u)));
    return;
    return;
}

void smbneo_native_fn_game_init_setup_96e4(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x96e4u);
    smbneo_native_fn_vs_setbank_low_820c(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, 0x0757u, ctx->a);
    native_write(ctx, 0x0754u, ctx->a);
    ctx->a = native_nz(ctx, 0x01u);
    ctx->y = native_nz(ctx, native_read(ctx, 0x6605u));
    if (!(ctx->status & NATIVE_Z)) {
        goto L96f8;
    }
    ctx->a = native_nz(ctx, 0x02u);
L96f8: ;
    native_write(ctx, 0x075au, ctx->a);
    native_write(ctx, 0x0761u, ctx->a);
    return;
    return;
}

void smbneo_native_fn_game_init_1_96ff(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x96ffu);
    smbneo_native_fn_game_init_setup_96e4(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_game_init_1_do_9702(ctx);
    return;
    return;
}

void smbneo_native_fn_game_init_1_do_9702(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x9702u);
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x0774u, ctx->a);
    ctx->y = native_nz(ctx, ctx->a);
L9708: ;
    native_write(ctx, (uint16_t)(0x0300u + ctx->y), ctx->a);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    if (!(ctx->status & NATIVE_Z)) {
        goto L9708;
    }
    native_write(ctx, 0x0759u, ctx->a);
    native_write(ctx, 0x0769u, ctx->a);
    native_write(ctx, 0x0728u, ctx->a);
    ctx->a = native_nz(ctx, 0xffu);
    native_write(ctx, 0x03a0u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x071au));
    native_write(ctx, 0x0778u, native_lsr(ctx, native_read(ctx, 0x0778u)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x01u));
    ctx->a = native_ror(ctx, ctx->a);
    native_write(ctx, 0x0778u, native_rol(ctx, native_read(ctx, 0x0778u)));
    smbneo_native_fn_game_init_area_music_977e(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, 0x38u);
    native_write(ctx, 0x06e3u, ctx->a);
    ctx->a = native_nz(ctx, 0x48u);
    native_write(ctx, 0x06e2u, ctx->a);
    ctx->a = native_nz(ctx, 0x58u);
    native_write(ctx, 0x06e1u, ctx->a);
    ctx->x = native_nz(ctx, 0x0eu);
L973c: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x9630u + ctx->x)));
    native_write(ctx, (uint16_t)(0x06e4u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x - 1u));
    if (!(ctx->status & NATIVE_N)) {
        goto L973c;
    }
    ctx->y = native_nz(ctx, 0x03u);
L9747: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x963fu + ctx->y)));
    native_write(ctx, (uint16_t)(0x0200u + ctx->y), ctx->a);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    if (!(ctx->status & NATIVE_N)) {
        goto L9747;
    }
    smbneo_native_fn_sub_92AA_rts_a18a(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_sub_92AA_a185(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_write(ctx, 0x0722u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0722u) + 1u)));
    native_write(ctx, 0x0772u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0772u) + 1u)));
    return;
    return;
}

void smbneo_native_fn_game_init_clearram_975d(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x975du);
    ctx->x = native_nz(ctx, 0x07u);
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x06u, ctx->a);
L9763: ;
    native_write(ctx, 0x07u, ctx->x);
L9765: ;
    native_cmp(ctx, ctx->x, 0x01u);
    if (!(ctx->status & NATIVE_Z)) {
        goto L976d;
    }
    native_cmp(ctx, ctx->y, 0x60u);
    if ((ctx->status & NATIVE_C)) {
        goto L976f;
    }
L976d: ;
    native_write(ctx, (uint16_t)(native_zpword(ctx, 0x06u) + ctx->y), ctx->a);
L976f: ;
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    native_cmp(ctx, ctx->y, 0xffu);
    if (!(ctx->status & NATIVE_Z)) {
        goto L9765;
    }
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x - 1u));
    if (!(ctx->status & NATIVE_N)) {
        goto L9763;
    }
    return;
    return;
}

void smbneo_native_fn_game_init_area_music_977e(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x977eu);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0770u));
    if ((ctx->status & NATIVE_Z)) {
        goto L97a6;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x0752u));
    native_cmp(ctx, ctx->a, 0x02u);
    if ((ctx->status & NATIVE_Z)) {
        goto L9797;
    }
    ctx->y = native_nz(ctx, 0x05u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0710u));
    native_cmp(ctx, ctx->a, 0x06u);
    if ((ctx->status & NATIVE_Z)) {
        goto L97a1;
    }
    native_cmp(ctx, ctx->a, 0x07u);
    if ((ctx->status & NATIVE_Z)) {
        goto L97a1;
    }
L9797: ;
    ctx->y = native_nz(ctx, native_read(ctx, 0x074eu));
    ctx->a = native_nz(ctx, native_read(ctx, 0x0743u));
    if ((ctx->status & NATIVE_Z)) {
        goto L97a1;
    }
    ctx->y = native_nz(ctx, 0x04u);
L97a1: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x9778u + ctx->y)));
    native_write(ctx, 0xfbu, ctx->a);
L97a6: ;
    return;
    return;
}

void smbneo_native_fn_player_proc_init_97cb(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x97cbu);
    ctx->a = native_nz(ctx, native_read(ctx, 0x071au));
    native_write(ctx, 0x6du, ctx->a);
    ctx->a = native_nz(ctx, 0x28u);
    native_write(ctx, 0x070au, ctx->a);
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, 0x33u, ctx->a);
    native_write(ctx, 0xb5u, ctx->a);
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x1du, ctx->a);
    native_write(ctx, 0x0490u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0490u) - 1u)));
    ctx->y = native_nz(ctx, 0x00u);
    native_write(ctx, 0x075bu, ctx->y);
    ctx->a = native_nz(ctx, native_read(ctx, 0x074eu));
    if (!(ctx->status & NATIVE_Z)) {
        goto L97ed;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
L97ed: ;
    native_write(ctx, 0x0704u, ctx->y);
    ctx->x = native_nz(ctx, native_read(ctx, 0x0710u));
    ctx->y = native_nz(ctx, native_read(ctx, 0x0752u));
    if ((ctx->status & NATIVE_Z)) {
        goto L97ff;
    }
    native_cmp(ctx, ctx->y, 0x01u);
    if ((ctx->status & NATIVE_Z)) {
        goto L97ff;
    }
    ctx->x = native_nz(ctx, native_read(ctx, (uint16_t)(0x97a9u + ctx->y)));
L97ff: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x97a7u + ctx->y)));
    native_write(ctx, 0x86u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x97adu + ctx->x)));
    native_write(ctx, 0xceu, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x97b6u + ctx->x)));
    native_write(ctx, 0x03c4u, ctx->a);
    smbneo_native_fn_bg_color_player_87d2(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->y = native_nz(ctx, native_read(ctx, 0x0715u));
    if ((ctx->status & NATIVE_Z)) {
        goto L9849;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x0757u));
    if ((ctx->status & NATIVE_Z)) {
        goto L9849;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x6603u));
    if ((ctx->status & NATIVE_Z)) {
        goto L9830;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x97c4u + ctx->y)));
    native_write(ctx, 0x07f8u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x97c7u + ctx->y)));
    native_write(ctx, 0x07f9u, ctx->a);
    /* static tail transfer */
    goto L983c;
L9830: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x97beu + ctx->y)));
    native_write(ctx, 0x07f8u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x97c1u + ctx->y)));
    native_write(ctx, 0x07f9u, ctx->a);
L983c: ;
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, 0x07fau, ctx->a);
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x0757u, ctx->a);
    native_write(ctx, 0x079fu, ctx->a);
L9849: ;
    ctx->y = native_nz(ctx, native_read(ctx, 0x0758u));
    if ((ctx->status & NATIVE_Z)) {
        goto L9862;
    }
    ctx->a = native_nz(ctx, 0x03u);
    native_write(ctx, 0x1du, ctx->a);
    ctx->x = native_nz(ctx, 0x00u);
    smbneo_native_fn_block_initpos_bc8f(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, 0xf0u);
    native_write(ctx, 0xd7u, ctx->a);
    ctx->x = native_nz(ctx, 0x05u);
    ctx->y = native_nz(ctx, 0x00u);
    smbneo_native_fn_actor_B_1A_init_b7d2(ctx);
    if (ctx->suspended || ctx->unsupported) return;
L9862: ;
    ctx->y = native_nz(ctx, native_read(ctx, 0x074eu));
    if (!(ctx->status & NATIVE_Z)) {
        goto L986a;
    }
    smbneo_native_fn_bubble_init_b5b8(ctx);
    if (ctx->suspended || ctx->unsupported) return;
L986a: ;
    ctx->a = native_nz(ctx, 0x07u);
    native_write(ctx, 0x0eu, ctx->a);
    return;
    return;
}

void smbneo_native_fn_player_proc_lifedown_987f(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x987fu);
    native_write(ctx, 0x0774u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0774u) + 1u)));
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x0722u, ctx->a);
    ctx->a = native_nz(ctx, 0x80u);
    native_write(ctx, 0xfcu, ctx->a);
    native_write(ctx, 0x075au, native_nz(ctx, (uint8_t)(native_read(ctx, 0x075au) - 1u)));
    if (!(ctx->status & NATIVE_N)) {
        goto L989b;
    }
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x0772u, ctx->a);
    ctx->a = native_nz(ctx, 0x04u);
    native_write(ctx, 0x0770u, ctx->a);
    return;
L989b: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x075fu));
    ctx->a = native_asl(ctx, ctx->a);
    ctx->x = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x075cu));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x02u));
    if ((ctx->status & NATIVE_Z)) {
        goto L98a8;
    }
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
L98a8: ;
    ctx->y = native_nz(ctx, native_read(ctx, (uint16_t)(0x986fu + ctx->x)));
    ctx->a = native_nz(ctx, native_read(ctx, 0x075cu));
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_nz(ctx, ctx->y);
    if ((ctx->status & NATIVE_C)) {
        goto L98b6;
    }
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
L98b6: ;
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x0fu));
    native_cmp(ctx, ctx->a, native_read(ctx, 0x071au));
    if ((ctx->status & NATIVE_Z)) {
        goto L98c1;
    }
    if (!(ctx->status & NATIVE_C)) {
        goto L98c1;
    }
    ctx->a = native_nz(ctx, 0x00u);
L98c1: ;
    native_write(ctx, 0x075bu, ctx->a);
    smbneo_native_fn_game_over_player_switch_a15d(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    /* static tail transfer */
    goto La13f;
La13f: ;
    smbneo_native_fn_course_descriptor_load_ab60(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, 0x0754u, ctx->a);
    native_write(ctx, 0x0757u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0757u) + 1u)));
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x0747u, ctx->a);
    native_write(ctx, 0x0756u, ctx->a);
    native_write(ctx, 0x0eu, ctx->a);
    native_write(ctx, 0x0772u, ctx->a);
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, 0x0770u, ctx->a);
    return;
    return;
}

void smbneo_native_fn_sys_game_over_98ca(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x98cau);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0772u));
    uint8_t table_selector_98cd = ctx->a;
    ctx->a = native_asl(ctx, ctx->a);
    ctx->y = native_nz(ctx, ctx->a);
    native_write(ctx, 0x04u, 0xcfu);
    native_write(ctx, 0x05u, 0x98u);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    switch (table_selector_98cd) {
    case 0: native_write(ctx, 0x06u, 0x0fu);
             native_write(ctx, 0x07u, 0xa1u);
             ctx->a = native_nz(ctx, 0xa1u);
             smbneo_native_fn_game_over_init_a10f(ctx); return;
    case 1: native_write(ctx, 0x06u, 0x48u);
             native_write(ctx, 0x07u, 0x87u);
             ctx->a = native_nz(ctx, 0x87u);
             smbneo_native_fn_bg_proc_8748(ctx); return;
    case 2: native_write(ctx, 0x06u, 0x02u);
             native_write(ctx, 0x07u, 0x99u);
             ctx->a = native_nz(ctx, 0x99u);
             smbneo_native_fn_game_over_9902_9902(ctx); return;
    case 3: native_write(ctx, 0x06u, 0xdcu);
             native_write(ctx, 0x07u, 0x98u);
             ctx->a = native_nz(ctx, 0x98u);
             smbneo_native_fn_game_over_98DC_98dc(ctx); return;
    case 4: native_write(ctx, 0x06u, 0xf2u);
             native_write(ctx, 0x07u, 0x98u);
             ctx->a = native_nz(ctx, 0x98u);
             smbneo_native_fn_game_over_98F2_98f2(ctx); return;
    case 5: native_write(ctx, 0x06u, 0x22u);
             native_write(ctx, 0x07u, 0xa1u);
             ctx->a = native_nz(ctx, 0xa1u);
             smbneo_native_fn_game_over_proc_a122(ctx); return;
    default: ctx->unsupported = 1; return;
    }
    return;
}

void smbneo_native_fn_game_over_98DC_98dc(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x98dcu);
    ctx->a = native_nz(ctx, native_read(ctx, 0x6617u));
    uint8_t table_selector_98df = ctx->a;
    ctx->a = native_asl(ctx, ctx->a);
    ctx->y = native_nz(ctx, ctx->a);
    native_write(ctx, 0x04u, 0xe1u);
    native_write(ctx, 0x05u, 0x98u);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    switch (table_selector_98df) {
    case 0: native_write(ctx, 0x06u, 0x19u);
             native_write(ctx, 0x07u, 0x99u);
             ctx->a = native_nz(ctx, 0x99u);
             smbneo_native_fn_sub_9919_9919(ctx); return;
    case 1: native_write(ctx, 0x06u, 0x5du);
             native_write(ctx, 0x07u, 0x99u);
             ctx->a = native_nz(ctx, 0x99u);
             smbneo_native_fn_sub_995D_995d(ctx); return;
    case 2: native_write(ctx, 0x06u, 0x20u);
             native_write(ctx, 0x07u, 0x9au);
             ctx->a = native_nz(ctx, 0x9au);
             smbneo_native_fn_sub_9A20_9a20(ctx); return;
    case 3: native_write(ctx, 0x06u, 0x4eu);
             native_write(ctx, 0x07u, 0x9au);
             ctx->a = native_nz(ctx, 0x9au);
             smbneo_native_fn_sub_9A4E_9a4e(ctx); return;
    case 4: native_write(ctx, 0x06u, 0x38u);
             native_write(ctx, 0x07u, 0x9bu);
             ctx->a = native_nz(ctx, 0x9bu);
             smbneo_native_fn_sub_9B38_9b38(ctx); return;
    case 5: native_write(ctx, 0x06u, 0x20u);
             native_write(ctx, 0x07u, 0x9au);
             ctx->a = native_nz(ctx, 0x9au);
             smbneo_native_fn_sub_9A20_9a20(ctx); return;
    case 6: native_write(ctx, 0x06u, 0x3bu);
             native_write(ctx, 0x07u, 0x95u);
             ctx->a = native_nz(ctx, 0x95u);
             smbneo_native_fn_vs_title_super_players_953b(ctx); return;
    case 7: native_write(ctx, 0x06u, 0x29u);
             native_write(ctx, 0x07u, 0x9eu);
             ctx->a = native_nz(ctx, 0x9eu);
             smbneo_native_fn_sub_9E29_9e29(ctx); return;
    default: ctx->unsupported = 1; return;
    }
    return;
}

void smbneo_native_fn_game_over_98F2_98f2(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x98f2u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x6617u));
    uint8_t table_selector_98f5 = ctx->a;
    ctx->a = native_asl(ctx, ctx->a);
    ctx->y = native_nz(ctx, ctx->a);
    native_write(ctx, 0x04u, 0xf7u);
    native_write(ctx, 0x05u, 0x98u);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    switch (table_selector_98f5) {
    case 0: native_write(ctx, 0x06u, 0x19u);
             native_write(ctx, 0x07u, 0x99u);
             ctx->a = native_nz(ctx, 0x99u);
             smbneo_native_fn_sub_9919_9919(ctx); return;
    case 1: native_write(ctx, 0x06u, 0x20u);
             native_write(ctx, 0x07u, 0x9au);
             ctx->a = native_nz(ctx, 0x9au);
             smbneo_native_fn_sub_9A20_9a20(ctx); return;
    case 2: native_write(ctx, 0x06u, 0x3bu);
             native_write(ctx, 0x07u, 0x9fu);
             ctx->a = native_nz(ctx, 0x9fu);
             smbneo_native_fn_sub_9F3B_9f3b(ctx); return;
    case 3: native_write(ctx, 0x06u, 0xc1u);
             native_write(ctx, 0x07u, 0x9fu);
             ctx->a = native_nz(ctx, 0x9fu);
             smbneo_native_fn_sub_9FC1_9fc1(ctx); return;
    case 4: native_write(ctx, 0x06u, 0x1bu);
             native_write(ctx, 0x07u, 0xa0u);
             ctx->a = native_nz(ctx, 0xa0u);
             smbneo_native_fn_sub_A01B_a01b(ctx); return;
    default: ctx->unsupported = 1; return;
    }
    return;
}

void smbneo_native_fn_game_over_9902_9902(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x9902u);
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x0774u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x07a0u));
    if (!(ctx->status & NATIVE_Z)) {
        goto L9918;
    }
    ctx->a = native_nz(ctx, 0x80u);
    native_write(ctx, 0xfcu, ctx->a);
    native_write(ctx, 0x0772u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0772u) + 1u)));
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x6617u, ctx->a);
L9918: ;
    return;
    return;
}

void smbneo_native_fn_sub_9919_9919(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x9919u);
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, 0x0774u, ctx->a);
    native_write(ctx, 0x6617u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x6617u) + 1u)));
    return;
    return;
}

void smbneo_native_fn_sub_9924_9924(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x9924u);
    ctx->x = native_nz(ctx, native_read(ctx, 0x0753u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x9922u + ctx->x)));
    native_write(ctx, 0x04u, ctx->a);
    ctx->a = native_nz(ctx, 0x07u);
    native_write(ctx, 0x05u, ctx->a);
    return;
    return;
}

void smbneo_native_fn_sub_9945_9945(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x9945u);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x9931u + ctx->y)));
    native_write(ctx, 0x00u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x993bu + ctx->y)));
    native_write(ctx, 0x06u, ctx->a);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x1du);
    native_write(ctx, 0x02u, ctx->a);
    ctx->a = native_nz(ctx, 0x66u);
    native_write(ctx, 0x01u, ctx->a);
    native_write(ctx, 0x03u, ctx->a);
    native_write(ctx, 0x07u, ctx->a);
    return;
    return;
}

void smbneo_native_fn_sub_995D_995d(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x995du);
    smbneo_native_fn_sub_9924_9924(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x6618u, ctx->a);
    ctx->a = native_nz(ctx, 0x09u);
    native_write(ctx, 0x6622u, ctx->a);
L996a: ;
    ctx->y = native_nz(ctx, native_read(ctx, 0x6618u));
    smbneo_native_fn_sub_9945_9945(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->y = native_nz(ctx, 0x00u);
L9972: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0x04u) + ctx->y)));
    native_cmp(ctx, ctx->a, native_read(ctx, (uint16_t)(native_zpword(ctx, 0x00u) + ctx->y)));
    if (!(ctx->status & NATIVE_C)) {
        goto L997f;
    }
    if (!(ctx->status & NATIVE_Z)) {
        goto L998f;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    native_cmp(ctx, ctx->y, 0x06u);
    if (!(ctx->status & NATIVE_Z)) {
        goto L9972;
    }
L997f: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x6618u));
    native_cmp(ctx, ctx->a, 0x09u);
    if (!(ctx->status & NATIVE_Z)) {
        goto L9989;
    }
    /* static tail transfer */
    goto L9a0c;
L9989: ;
    native_write(ctx, 0x6618u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x6618u) + 1u)));
    /* static tail transfer */
    goto L996a;
L998f: ;
    ctx->y = native_nz(ctx, native_read(ctx, 0x6622u));
    native_cmp(ctx, ctx->y, native_read(ctx, 0x6618u));
    if ((ctx->status & NATIVE_Z)) {
        goto L99d5;
    }
    if (!(ctx->status & NATIVE_C)) {
        goto L99d5;
    }
    smbneo_native_fn_sub_9945_9945(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x9931u + ctx->y)));
    native_write(ctx, 0x04u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x993bu + ctx->y)));
    native_write(ctx, 0xedu, ctx->a);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x1du);
    native_write(ctx, 0xebu, ctx->a);
    ctx->a = native_nz(ctx, 0x66u);
    native_write(ctx, 0x05u, ctx->a);
    native_write(ctx, 0xecu, ctx->a);
    native_write(ctx, 0xeeu, ctx->a);
    native_write(ctx, 0x6622u, ctx->y);
    ctx->y = native_nz(ctx, 0x06u);
L99b9: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0x04u) + ctx->y)));
    native_write(ctx, (uint16_t)(native_zpword(ctx, 0x00u) + ctx->y), ctx->a);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    if (!(ctx->status & NATIVE_N)) {
        goto L99b9;
    }
    ctx->y = native_nz(ctx, 0x02u);
L99c2: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0xebu) + ctx->y)));
    native_write(ctx, (uint16_t)(native_zpword(ctx, 0x02u) + ctx->y), ctx->a);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    if (!(ctx->status & NATIVE_N)) {
        goto L99c2;
    }
    ctx->y = native_nz(ctx, 0x01u);
L99cb: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0xedu) + ctx->y)));
    native_write(ctx, (uint16_t)(native_zpword(ctx, 0x06u) + ctx->y), ctx->a);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    if (!(ctx->status & NATIVE_N)) {
        goto L99cb;
    }
    /* static tail transfer */
    goto L998f;
L99d5: ;
    ctx->y = native_nz(ctx, native_read(ctx, 0x6618u));
    smbneo_native_fn_sub_9945_9945(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_sub_9924_9924(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->y = native_nz(ctx, 0x06u);
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, (uint16_t)(native_zpword(ctx, 0x00u) + ctx->y), ctx->a);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
L99e5: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0x04u) + ctx->y)));
    native_write(ctx, (uint16_t)(native_zpword(ctx, 0x00u) + ctx->y), ctx->a);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    if (!(ctx->status & NATIVE_N)) {
        goto L99e5;
    }
    ctx->y = native_nz(ctx, 0x02u);
L99ee: ;
    ctx->a = native_nz(ctx, 0x24u);
    native_write(ctx, (uint16_t)(native_zpword(ctx, 0x02u) + ctx->y), ctx->a);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    if (!(ctx->status & NATIVE_N)) {
        goto L99ee;
    }
    ctx->y = native_nz(ctx, 0x00u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x075fu));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x01u);
    native_write(ctx, (uint16_t)(native_zpword(ctx, 0x06u) + ctx->y), ctx->a);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->a = native_nz(ctx, native_read(ctx, 0x075cu));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x01u);
    native_write(ctx, (uint16_t)(native_zpword(ctx, 0x06u) + ctx->y), ctx->a);
    smbneo_native_fn_sub_9919_9919(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    return;
L9a0c: ;
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x6617u, ctx->a);
    ctx->x = native_nz(ctx, native_read(ctx, 0x0753u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x6628u + ctx->x)));
    if ((ctx->status & NATIVE_Z)) {
        goto L9a1c;
    }
    native_write(ctx, 0x0772u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0772u) + 1u)));
L9a1c: ;
    native_write(ctx, 0x0772u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0772u) + 1u)));
    return;
    return;
}

void smbneo_native_fn_sub_9A20_9a20(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x9a20u);
    smbneo_native_fn_nt_init_90d0(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_oam_init_826e(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_write(ctx, 0x6617u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x6617u) + 1u)));
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x6622u, ctx->a);
    return;
    return;
}

void smbneo_native_fn_sub_9A32_9a32(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x9a32u);
    ctx->y = native_nz(ctx, 0x00u);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0x06u) + ctx->y)));
    native_write(ctx, (uint16_t)(0x0301u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    ctx->a = native_nz(ctx, 0x20u);
    native_write(ctx, (uint16_t)(0x0301u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    ctx->a = native_nz(ctx, 0x28u);
    native_write(ctx, (uint16_t)(0x0301u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0x06u) + ctx->y)));
    native_write(ctx, (uint16_t)(0x0301u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    return;
    return;
}

void smbneo_native_fn_sub_9A4E_9a4e(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x9a4eu);
    ctx->a = native_nz(ctx, native_read(ctx, 0x6622u));
    if (!(ctx->status & NATIVE_Z)) {
        goto L9a5c;
    }
    ctx->a = native_nz(ctx, 0x24u);
    native_write(ctx, 0x0773u, ctx->a);
    native_write(ctx, 0x6622u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x6622u) + 1u)));
    return;
L9a5c: ;
    native_cmp(ctx, ctx->a, 0x01u);
    if (!(ctx->status & NATIVE_Z)) {
        goto L9a65;
    }
    ctx->y = native_nz(ctx, 0x04u);
    /* static tail transfer */
    goto L9a74;
L9a65: ;
    native_cmp(ctx, ctx->a, 0x02u);
    if (!(ctx->status & NATIVE_Z)) {
        goto L9a6e;
    }
    ctx->y = native_nz(ctx, 0x02u);
    /* static tail transfer */
    goto L9a74;
L9a6e: ;
    native_cmp(ctx, ctx->a, 0x03u);
    if (!(ctx->status & NATIVE_Z)) {
        goto L9a7b;
    }
    ctx->y = native_nz(ctx, 0x03u);
L9a74: ;
    smbneo_native_fn_bg_vschar1_load_88f7(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_write(ctx, 0x6622u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x6622u) + 1u)));
    return;
L9a7b: ;
    ctx->y = native_nz(ctx, native_read(ctx, 0x6618u));
    smbneo_native_fn_sub_9945_9945(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->x = native_nz(ctx, 0x00u);
L9a83: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x9a2fu + ctx->x)));
    native_write(ctx, (uint16_t)(0x0301u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    native_cmp(ctx, ctx->x, 0x03u);
    if (!(ctx->status & NATIVE_Z)) {
        goto L9a83;
    }
    smbneo_native_fn_sub_9A32_9a32(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_sub_9AFF_9aff(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->y = native_nz(ctx, 0x00u);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0x00u) + ctx->y)));
    if (!(ctx->status & NATIVE_Z)) {
        goto L9a9c;
    }
    ctx->a = native_nz(ctx, 0x24u);
L9a9c: ;
    native_write(ctx, (uint16_t)(0x0301u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
L9aa1: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0x00u) + ctx->y)));
    native_write(ctx, (uint16_t)(0x0301u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    native_cmp(ctx, ctx->y, 0x07u);
    if (!(ctx->status & NATIVE_Z)) {
        goto L9aa1;
    }
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, (uint16_t)(0x0301u + ctx->x), ctx->a);
    native_write(ctx, 0x6622u, ctx->a);
    native_write(ctx, 0x0774u, ctx->a);
    native_write(ctx, 0x661au, ctx->a);
    native_write(ctx, 0x6620u, ctx->a);
    native_write(ctx, 0x6621u, ctx->a);
    native_write(ctx, 0x6623u, ctx->a);
    native_write(ctx, 0x6624u, ctx->a);
    native_write(ctx, 0x6617u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x6617u) + 1u)));
    ctx->y = native_nz(ctx, 0x02u);
    native_write(ctx, 0x6619u, ctx->y);
L9ace: ;
    ctx->a = native_nz(ctx, 0x24u);
    native_write(ctx, (uint16_t)(0x661cu + ctx->y), ctx->a);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    if (!(ctx->status & NATIVE_N)) {
        goto L9ace;
    }
    ctx->a = native_nz(ctx, 0x10u);
    native_write(ctx, 0x661fu, ctx->a);
    ctx->a = native_nz(ctx, 0x40u);
    native_write(ctx, 0xfbu, ctx->a);
    native_write(ctx, 0x661bu, ctx->a);
    return;
    return;
}

void smbneo_native_fn_sub_9AE3_9ae3(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x9ae3u);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    native_cmp(ctx, ctx->y, 0x0au);
    if (!(ctx->status & NATIVE_Z)) {
        goto L9af7;
    }
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, (uint16_t)(0x0301u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, (uint16_t)(0x0301u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    /* static tail transfer */
    smbneo_native_fn_sub_9AFF_9aff(ctx);
    return;
L9af7: ;
    smbneo_native_fn_sub_9B0C_9b0c(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, ctx->y);
    native_write(ctx, (uint16_t)(0x0301u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    smbneo_native_fn_sub_9AFF_9aff(ctx);
    return;
    return;
}

void smbneo_native_fn_sub_9AFF_9aff(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x9affu);
    ctx->a = native_nz(ctx, 0xafu);
    native_write(ctx, (uint16_t)(0x0301u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    return;
    return;
}

void smbneo_native_fn_sub_9B06_9b06(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x9b06u);
    ctx->a = native_nz(ctx, 0x24u);
    native_write(ctx, (uint16_t)(0x0301u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    smbneo_native_fn_sub_9B0C_9b0c(ctx);
    return;
    return;
}

void smbneo_native_fn_sub_9B0C_9b0c(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x9b0cu);
    ctx->a = native_nz(ctx, 0x24u);
    native_write(ctx, (uint16_t)(0x0301u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    return;
    return;
}

void smbneo_native_fn_sub_9B38_9b38(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x9b38u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x6619u));
    if (!(ctx->status & NATIVE_Z)) {
        goto L9b4b;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x661au));
    if (!(ctx->status & NATIVE_Z)) {
        goto L9b4b;
    }
    smbneo_native_fn_sub_9919_9919(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_sub_9D3C_9d3c(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    /* static tail transfer */
    goto L9cb0;
L9b4b: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x661bu));
    if ((ctx->status & NATIVE_Z)) {
        goto L9b56;
    }
    native_write(ctx, 0x661bu, native_nz(ctx, (uint8_t)(native_read(ctx, 0x661bu) - 1u)));
    /* static tail transfer */
    goto L9b6e;
L9b56: ;
    ctx->a = native_nz(ctx, 0x40u);
    native_write(ctx, 0x661bu, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x661au));
    if (!(ctx->status & NATIVE_Z)) {
        goto L9b6b;
    }
    ctx->a = native_nz(ctx, 0x09u);
    native_write(ctx, 0x661au, ctx->a);
    native_write(ctx, 0x6619u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x6619u) - 1u)));
    /* static tail transfer */
    goto L9b6e;
L9b6b: ;
    native_write(ctx, 0x661au, native_nz(ctx, (uint8_t)(native_read(ctx, 0x661au) - 1u)));
L9b6e: ;
    smbneo_native_fn_vs_game_over_joypad_get_9d4d(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x0fu));
    if (!(ctx->status & NATIVE_Z)) {
        goto L9b7b;
    }
    native_write(ctx, 0x6625u, ctx->a);
    /* static tail transfer */
    goto L9c31;
L9b7b: ;
    native_write(ctx, 0x6625u, ctx->a);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & native_read(ctx, 0x6624u)));
    if ((ctx->status & NATIVE_Z)) {
        goto L9b91;
    }
    native_write(ctx, 0x6625u, ctx->a);
    native_write(ctx, 0x661fu, native_nz(ctx, (uint8_t)(native_read(ctx, 0x661fu) - 1u)));
    ctx->a = native_nz(ctx, native_read(ctx, 0x661fu));
    if ((ctx->status & NATIVE_Z)) {
        goto L9b91;
    }
    /* static tail transfer */
    goto L9c31;
L9b91: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x6625u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x01u));
    if (!(ctx->status & NATIVE_Z)) {
        goto L9bb1;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x6625u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x02u));
    if (!(ctx->status & NATIVE_Z)) {
        goto L9bf3;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x6625u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x08u));
    native_write(ctx, 0x6625u, ctx->a);
    if (!(ctx->status & NATIVE_Z)) {
        goto L9c10;
    }
    ctx->a = native_nz(ctx, 0x04u);
    native_write(ctx, 0x6625u, ctx->a);
    /* static tail transfer */
    goto L9bce;
L9bb1: ;
    native_write(ctx, 0x6625u, ctx->a);
    native_write(ctx, 0x6620u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x6620u) + 1u)));
    ctx->a = native_nz(ctx, native_read(ctx, 0x6620u));
    native_cmp(ctx, ctx->a, 0x0bu);
    if ((ctx->status & NATIVE_Z)) {
        goto L9bc9;
    }
    native_cmp(ctx, ctx->a, 0x0au);
    if (!(ctx->status & NATIVE_Z)) {
        goto L9bf0;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x6621u));
    native_cmp(ctx, ctx->a, 0x02u);
    if (!(ctx->status & NATIVE_Z)) {
        goto L9bf0;
    }
L9bc9: ;
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x6620u, ctx->a);
L9bce: ;
    native_write(ctx, 0x6621u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x6621u) + 1u)));
    ctx->a = native_nz(ctx, native_read(ctx, 0x6621u));
    native_cmp(ctx, ctx->a, 0x03u);
    if ((ctx->status & NATIVE_Z)) {
        goto L9beb;
    }
    native_cmp(ctx, ctx->a, 0x02u);
    if (!(ctx->status & NATIVE_Z)) {
        goto L9bf0;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x6620u));
    native_cmp(ctx, ctx->a, 0x0au);
    if (!(ctx->status & NATIVE_Z)) {
        goto L9bf0;
    }
    ctx->a = native_nz(ctx, 0x09u);
    native_write(ctx, 0x6620u, ctx->a);
    /* static tail transfer */
    goto L9bf0;
L9beb: ;
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x6621u, ctx->a);
L9bf0: ;
    /* static tail transfer */
    goto L9c2c;
L9bf3: ;
    native_write(ctx, 0x6625u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x6620u));
    if ((ctx->status & NATIVE_Z)) {
        goto L9c01;
    }
    native_write(ctx, 0x6620u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x6620u) - 1u)));
    /* static tail transfer */
    goto L9c2c;
L9c01: ;
    ctx->a = native_nz(ctx, 0x0au);
    native_write(ctx, 0x6620u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x6621u));
    if (!(ctx->status & NATIVE_Z)) {
        goto L9c10;
    }
    ctx->a = native_nz(ctx, 0x09u);
    native_write(ctx, 0x6620u, ctx->a);
L9c10: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x6621u));
    if ((ctx->status & NATIVE_Z)) {
        goto L9c1b;
    }
    native_write(ctx, 0x6621u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x6621u) - 1u)));
    /* static tail transfer */
    goto L9c2c;
L9c1b: ;
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, 0x6621u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x6620u));
    native_cmp(ctx, ctx->a, 0x0au);
    if (!(ctx->status & NATIVE_Z)) {
        goto L9c2c;
    }
    ctx->a = native_nz(ctx, 0x09u);
    native_write(ctx, 0x6620u, ctx->a);
L9c2c: ;
    ctx->a = native_nz(ctx, 0x10u);
    native_write(ctx, 0x661fu, ctx->a);
L9c31: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x6625u));
    native_write(ctx, 0x6624u, ctx->a);
    smbneo_native_fn_vs_game_over_joypad_get_9d4d(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x80u));
    if (!(ctx->status & NATIVE_Z)) {
        goto L9c44;
    }
    native_write(ctx, 0x6623u, ctx->a);
    /* static tail transfer */
    goto L9cb0;
L9c44: ;
    native_cmp(ctx, ctx->a, native_read(ctx, 0x6623u));
    if ((ctx->status & NATIVE_Z)) {
        goto L9cb0;
    }
    native_write(ctx, 0x6623u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x6621u));
    native_cmp(ctx, ctx->a, 0x02u);
    if (!(ctx->status & NATIVE_Z)) {
        goto L9c78;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x6620u));
    native_cmp(ctx, ctx->a, 0x09u);
    if (!(ctx->status & NATIVE_Z)) {
        goto L9c61;
    }
    smbneo_native_fn_sub_9919_9919(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_sub_9D3C_9d3c(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    return;
L9c61: ;
    native_cmp(ctx, ctx->a, 0x08u);
    if (!(ctx->status & NATIVE_Z)) {
        goto L9c78;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x6622u));
    if ((ctx->status & NATIVE_Z)) {
        goto L9c6d;
    }
    native_write(ctx, 0x6622u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x6622u) - 1u)));
L9c6d: ;
    ctx->x = native_nz(ctx, native_read(ctx, 0x6622u));
    ctx->a = native_nz(ctx, 0x24u);
    native_write(ctx, (uint16_t)(0x661cu + ctx->x), ctx->a);
    /* static tail transfer */
    goto L9cb0;
L9c78: ;
    ctx->x = native_nz(ctx, native_read(ctx, 0x6622u));
    native_cmp(ctx, ctx->x, 0x03u);
    if ((ctx->status & NATIVE_Z)) {
        goto L9cb0;
    }
    ctx->y = native_nz(ctx, native_read(ctx, 0x6621u));
    native_cmp(ctx, ctx->y, 0x02u);
    if (!(ctx->status & NATIVE_Z)) {
        goto L9c8f;
    }
    ctx->y = native_nz(ctx, native_read(ctx, 0x6620u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x9b2du + ctx->y)));
    /* static tail transfer */
    goto L9c99;
L9c8f: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x6620u));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, (uint16_t)(0x9b2bu + ctx->y)));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x0au);
L9c99: ;
    native_write(ctx, (uint16_t)(0x661cu + ctx->x), ctx->a);
    native_write(ctx, 0x6622u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x6622u) + 1u)));
    ctx->a = native_nz(ctx, native_read(ctx, 0x6622u));
    native_cmp(ctx, ctx->a, 0x03u);
    if (!(ctx->status & NATIVE_Z)) {
        goto L9cb0;
    }
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, 0x6621u, ctx->a);
    ctx->a = native_nz(ctx, 0x09u);
    native_write(ctx, 0x6620u, ctx->a);
L9cb0: ;
    ctx->x = native_nz(ctx, native_read(ctx, 0x6621u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x9b13u + ctx->x)));
    native_write(ctx, 0x0200u, ctx->a);
    native_write(ctx, 0x0204u, ctx->a);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x08u);
    native_write(ctx, 0x0208u, ctx->a);
    native_write(ctx, 0x020cu, ctx->a);
    ctx->a = native_nz(ctx, 0x32u);
    native_write(ctx, 0x0201u, ctx->a);
    ctx->a = native_nz(ctx, 0x41u);
    native_write(ctx, 0x0205u, ctx->a);
    ctx->a = native_nz(ctx, 0x42u);
    native_write(ctx, 0x0209u, ctx->a);
    ctx->a = native_nz(ctx, 0x43u);
    native_write(ctx, 0x020du, ctx->a);
    ctx->x = native_nz(ctx, native_read(ctx, 0x6620u));
    ctx->a = native_nz(ctx, native_read(ctx, 0x6621u));
    native_cmp(ctx, ctx->a, 0x02u);
    if (!(ctx->status & NATIVE_Z)) {
        goto L9ce9;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x9b21u + ctx->x)));
    /* static tail transfer */
    goto L9cec;
L9ce9: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x9b16u + ctx->x)));
L9cec: ;
    native_write(ctx, 0x0203u, ctx->a);
    native_write(ctx, 0x020bu, ctx->a);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x08u);
    native_write(ctx, 0x0207u, ctx->a);
    native_write(ctx, 0x020fu, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0753u));
    native_write(ctx, 0x0202u, ctx->a);
    native_write(ctx, 0x0206u, ctx->a);
    native_write(ctx, 0x020au, ctx->a);
    native_write(ctx, 0x020eu, ctx->a);
    ctx->x = native_nz(ctx, 0x02u);
L9d0c: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x9b35u + ctx->x)));
    native_write(ctx, (uint16_t)(0x0301u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x661cu + ctx->x)));
    native_write(ctx, (uint16_t)(0x0304u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x - 1u));
    if (!(ctx->status & NATIVE_N)) {
        goto L9d0c;
    }
    ctx->a = native_nz(ctx, 0x23u);
    native_write(ctx, 0x0307u, ctx->a);
    ctx->a = native_nz(ctx, 0x05u);
    native_write(ctx, 0x0308u, ctx->a);
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, 0x0309u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x6619u));
    native_write(ctx, 0x030au, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x661au));
    native_write(ctx, 0x030bu, ctx->a);
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x030cu, ctx->a);
    return;
    return;
}

void smbneo_native_fn_sub_9D3C_9d3c(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x9d3cu);
    ctx->y = native_nz(ctx, native_read(ctx, 0x6618u));
    smbneo_native_fn_sub_9945_9945(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->y = native_nz(ctx, 0x02u);
L9d44: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x661cu + ctx->y)));
    native_write(ctx, (uint16_t)(native_zpword(ctx, 0x02u) + ctx->y), ctx->a);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    if (!(ctx->status & NATIVE_N)) {
        goto L9d44;
    }
    return;
    return;
}

void smbneo_native_fn_vs_game_over_joypad_get_9d4d(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x9d4du);
    ctx->a = native_nz(ctx, native_read(ctx, 0x077au));
    if ((ctx->status & NATIVE_Z)) {
        goto L9d5b;
    }
    ctx->x = native_nz(ctx, native_read(ctx, 0x0753u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x06fcu + ctx->x)));
    /* static tail transfer */
    goto L9d61;
L9d5b: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x06fcu));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | native_read(ctx, 0x06fdu)));
L9d61: ;
    return;
    return;
}

void smbneo_native_fn_sub_9D62_9d62(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x9d62u);
    ctx->y = native_nz(ctx, native_read(ctx, 0x04u));
    smbneo_native_fn_sub_9945_9945(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x9e15u + ctx->y)));
    native_write(ctx, (uint16_t)(0x6000u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x9e1fu + ctx->y)));
    native_write(ctx, (uint16_t)(0x6000u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    ctx->a = native_nz(ctx, 0x15u);
    native_write(ctx, (uint16_t)(0x6000u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->a = native_nz(ctx, ctx->y);
    native_cmp(ctx, ctx->a, 0x0au);
    if (!(ctx->status & NATIVE_Z)) {
        goto L9d90;
    }
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, (uint16_t)(0x6000u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, (uint16_t)(0x6000u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    /* static tail transfer */
    goto L9d98;
L9d90: ;
    smbneo_native_fn_sub_9DEA_b_9df0(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, ctx->y);
    native_write(ctx, (uint16_t)(0x6000u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
L9d98: ;
    ctx->a = native_nz(ctx, 0xafu);
    native_write(ctx, (uint16_t)(0x6000u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    smbneo_native_fn_sub_9DEA_b_9df0(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->y = native_nz(ctx, 0x00u);
L9da3: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0x02u) + ctx->y)));
    native_write(ctx, (uint16_t)(0x6000u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    native_cmp(ctx, ctx->y, 0x03u);
    if (!(ctx->status & NATIVE_Z)) {
        goto L9da3;
    }
    smbneo_native_fn_sub_9DEA_9dea(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->y = native_nz(ctx, 0x00u);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0x06u) + ctx->y)));
    native_write(ctx, (uint16_t)(0x6000u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    ctx->a = native_nz(ctx, 0x20u);
    native_write(ctx, (uint16_t)(0x6000u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    ctx->a = native_nz(ctx, 0x28u);
    native_write(ctx, (uint16_t)(0x6000u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0x06u) + ctx->y)));
    native_write(ctx, (uint16_t)(0x6000u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    smbneo_native_fn_sub_9DEA_b_9df0(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->y = native_nz(ctx, 0x00u);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0x00u) + ctx->y)));
    if (!(ctx->status & NATIVE_Z)) {
        goto L9dd7;
    }
    ctx->a = native_nz(ctx, 0x24u);
L9dd7: ;
    native_write(ctx, (uint16_t)(0x6000u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
L9ddc: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0x00u) + ctx->y)));
    native_write(ctx, (uint16_t)(0x6000u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    native_cmp(ctx, ctx->y, 0x07u);
    if (!(ctx->status & NATIVE_Z)) {
        goto L9ddc;
    }
    native_write(ctx, 0x04u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x04u) + 1u)));
    return;
    return;
}

void smbneo_native_fn_sub_9DEA_9dea(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x9deau);
    ctx->a = native_nz(ctx, 0x24u);
    native_write(ctx, (uint16_t)(0x6000u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    smbneo_native_fn_sub_9DEA_b_9df0(ctx);
    return;
    return;
}

void smbneo_native_fn_sub_9DEA_b_9df0(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x9df0u);
    ctx->a = native_nz(ctx, 0x24u);
    native_write(ctx, (uint16_t)(0x6000u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    return;
    return;
}

void smbneo_native_fn_sub_9DF7_9df7(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x9df7u);
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x0774u, ctx->a);
    native_write(ctx, 0x6622u, ctx->a);
    ctx->a = native_nz(ctx, 0x14u);
    native_write(ctx, 0x661fu, ctx->a);
    native_write(ctx, 0x6617u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x6617u) + 1u)));
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, 0x6619u, ctx->a);
    ctx->a = native_nz(ctx, 0x40u);
    native_write(ctx, 0x661au, ctx->a);
    return;
    return;
}

void smbneo_native_fn_sub_9E29_9e29(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x9e29u);
    goto L9e29;
L9a0c: ;
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x6617u, ctx->a);
    ctx->x = native_nz(ctx, native_read(ctx, 0x0753u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x6628u + ctx->x)));
    if ((ctx->status & NATIVE_Z)) {
        goto L9a1c;
    }
    native_write(ctx, 0x0772u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0772u) + 1u)));
L9a1c: ;
    native_write(ctx, 0x0772u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0772u) + 1u)));
    return;
L9e29: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x6619u));
    if (!(ctx->status & NATIVE_Z)) {
        goto L9e45;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x661au));
    if ((ctx->status & NATIVE_Z)) {
        goto L9e3e;
    }
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x80u));
    if (!(ctx->status & NATIVE_Z)) {
        goto L9e45;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x06fcu));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x20u));
    if ((ctx->status & NATIVE_Z)) {
        goto L9e45;
    }
L9e3e: ;
    ctx->a = native_nz(ctx, 0x80u);
    native_write(ctx, 0xfbu, ctx->a);
    /* static tail transfer */
    goto L9a0c;
L9e45: ;
    ctx->y = native_nz(ctx, native_read(ctx, 0x6618u));
    smbneo_native_fn_sub_9945_9945(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, 0x661au));
    ctx->status |= NATIVE_C;
    native_sbc(ctx, 0x01u);
    native_write(ctx, 0x661au, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x6619u));
    native_sbc(ctx, 0x00u);
    native_write(ctx, 0x6619u, ctx->a);
    ctx->x = native_nz(ctx, 0x00u);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x9e15u + ctx->y)));
    native_write(ctx, (uint16_t)(0x0301u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x9e1fu + ctx->y)));
    native_write(ctx, (uint16_t)(0x0301u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    ctx->a = native_nz(ctx, native_read(ctx, 0x6622u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x01u));
    if ((ctx->status & NATIVE_Z)) {
        goto L9e8e;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x661fu));
    if (!(ctx->status & NATIVE_Z)) {
        goto L9e7c;
    }
    smbneo_native_fn_sub_9EDA_14_9ee3(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    return;
L9e7c: ;
    ctx->y = native_nz(ctx, 0x00u);
L9e7e: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x9e12u + ctx->y)));
    native_write(ctx, (uint16_t)(0x0301u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    native_cmp(ctx, ctx->y, 0x03u);
    if (!(ctx->status & NATIVE_Z)) {
        goto L9e7e;
    }
    native_write(ctx, 0x661fu, native_nz(ctx, (uint8_t)(native_read(ctx, 0x661fu) - 1u)));
    return;
L9e8e: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x661fu));
    if (!(ctx->status & NATIVE_Z)) {
        goto L9e97;
    }
    smbneo_native_fn_sub_9EDA_0A_9eda(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    return;
L9e97: ;
    ctx->a = native_nz(ctx, 0x15u);
    native_write(ctx, (uint16_t)(0x0301u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    smbneo_native_fn_sub_9AE3_9ae3(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_sub_9B0C_9b0c(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->y = native_nz(ctx, 0x00u);
L9ea5: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0x02u) + ctx->y)));
    native_write(ctx, (uint16_t)(0x0301u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    native_cmp(ctx, ctx->y, 0x03u);
    if (!(ctx->status & NATIVE_Z)) {
        goto L9ea5;
    }
    smbneo_native_fn_sub_9B06_9b06(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_sub_9A32_9a32(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_sub_9B0C_9b0c(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->y = native_nz(ctx, 0x00u);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0x00u) + ctx->y)));
    if (!(ctx->status & NATIVE_Z)) {
        goto L9ec1;
    }
    ctx->a = native_nz(ctx, 0x24u);
L9ec1: ;
    native_write(ctx, (uint16_t)(0x0301u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
L9ec6: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0x00u) + ctx->y)));
    native_write(ctx, (uint16_t)(0x0301u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    native_cmp(ctx, ctx->y, 0x07u);
    if (!(ctx->status & NATIVE_Z)) {
        goto L9ec6;
    }
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, (uint16_t)(0x0301u + ctx->x), ctx->a);
    native_write(ctx, 0x661fu, native_nz(ctx, (uint8_t)(native_read(ctx, 0x661fu) - 1u)));
    return;
    return;
}

void smbneo_native_fn_sub_9EDA_0A_9eda(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x9edau);
    ctx->a = native_nz(ctx, 0x0au);
    native_write(ctx, 0x661fu, ctx->a);
    native_write(ctx, 0x6622u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x6622u) + 1u)));
    return;
    return;
}

void smbneo_native_fn_sub_9EDA_14_9ee3(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x9ee3u);
    ctx->a = native_nz(ctx, 0x14u);
    native_write(ctx, 0x661fu, ctx->a);
    native_write(ctx, 0x6622u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x6622u) + 1u)));
    return;
    return;
}

void smbneo_native_fn_sub_9F3B_9f3b(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x9f3bu);
    goto L9f3b;
L9f24: ;
    ctx->x = native_nz(ctx, 0x08u);
L9f26: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x9f1bu + ctx->x)));
    native_write(ctx, (uint16_t)(0x0301u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x - 1u));
    if (!(ctx->status & NATIVE_N)) {
        goto L9f26;
    }
    native_write(ctx, 0x6622u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x6622u) + 1u)));
    return;
L9f3b: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x6622u));
    if (!(ctx->status & NATIVE_Z)) {
        goto L9f49;
    }
    ctx->a = native_nz(ctx, 0x24u);
    native_write(ctx, 0x0773u, ctx->a);
    native_write(ctx, 0x6622u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x6622u) + 1u)));
    return;
L9f49: ;
    native_cmp(ctx, ctx->a, 0x01u);
    if (!(ctx->status & NATIVE_Z)) {
        goto L9f5c;
    }
    ctx->x = native_nz(ctx, 0x07u);
L9f4f: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x9f13u + ctx->x)));
    native_write(ctx, (uint16_t)(0x0301u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x - 1u));
    if (!(ctx->status & NATIVE_N)) {
        goto L9f4f;
    }
    native_write(ctx, 0x6622u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x6622u) + 1u)));
    return;
L9f5c: ;
    native_cmp(ctx, ctx->a, 0x02u);
    if (!(ctx->status & NATIVE_Z)) {
        goto L9f63;
    }
    /* static tail transfer */
    goto L9f24;
L9f63: ;
    native_cmp(ctx, ctx->a, 0x03u);
    if (!(ctx->status & NATIVE_Z)) {
        goto L9f70;
    }
    ctx->y = native_nz(ctx, 0x05u);
    smbneo_native_fn_bg_vschar1_load_88f7(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_write(ctx, 0x6622u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x6622u) + 1u)));
    return;
L9f70: ;
    smbneo_native_fn_sub_9DF7_9df7(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x661au, ctx->a);
    native_write(ctx, 0x6618u, ctx->a);
    ctx->a = native_nz(ctx, 0x80u);
    native_write(ctx, 0x00u, ctx->a);
    ctx->y = native_nz(ctx, 0x07u);
    ctx->x = native_nz(ctx, 0x1fu);
L9f83: ;
    ctx->a = native_nz(ctx, 0x80u);
    native_write(ctx, 0x01u, ctx->a);
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, 0x02u, ctx->a);
L9f8b: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x01u));
    native_write(ctx, (uint16_t)(0x0200u + ctx->x), ctx->a);
    ctx->status |= NATIVE_C;
    native_sbc(ctx, 0x08u);
    native_write(ctx, 0x01u, ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x - 1u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x9f33u + ctx->y)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x80u));
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | native_read(ctx, 0x0753u)));
    native_write(ctx, (uint16_t)(0x0200u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x - 1u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x9f33u + ctx->y)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x7fu));
    native_write(ctx, (uint16_t)(0x0200u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x - 1u));
    ctx->a = native_nz(ctx, native_read(ctx, 0x00u));
    native_write(ctx, (uint16_t)(0x0200u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x - 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    native_write(ctx, 0x02u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x02u) - 1u)));
    if (!(ctx->status & NATIVE_Z)) {
        goto L9f8b;
    }
    ctx->status |= NATIVE_C;
    native_sbc(ctx, 0x08u);
    native_write(ctx, 0x00u, ctx->a);
    native_cmp(ctx, ctx->a, 0x60u);
    if (!(ctx->status & NATIVE_Z)) {
        goto L9f83;
    }
    return;
    return;
}

void smbneo_native_fn_sub_9FC1_9fc1(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0x9fc1u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x6610u));
    if ((ctx->status & NATIVE_Z)) {
        goto L9fe2;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x6618u));
    if (!(ctx->status & NATIVE_Z)) {
        goto L9fd4;
    }
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, 0x0774u, ctx->a);
    native_write(ctx, 0x6618u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x6618u) + 1u)));
    return;
L9fd4: ;
    ctx->y = native_nz(ctx, 0x06u);
    smbneo_native_fn_bg_vschar1_load_88f7(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_sub_9DF7_9df7(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x661au, ctx->a);
    return;
L9fe2: ;
    smbneo_native_fn_sub_A075_a075(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->x = native_nz(ctx, 0x00u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x6622u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x01u));
    if (!(ctx->status & NATIVE_Z)) {
        goto La002;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x661fu));
    if ((ctx->status & NATIVE_Z)) {
        goto L9ffc;
    }
    smbneo_native_fn_sub_A0D7_a0d7(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_write(ctx, 0x661fu, native_nz(ctx, (uint8_t)(native_read(ctx, 0x661fu) - 1u)));
    /* static tail transfer */
    smbneo_native_fn_sub_A0B2_a0b2(ctx);
    return;
L9ffc: ;
    smbneo_native_fn_sub_9EDA_0A_9eda(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    /* static tail transfer */
    smbneo_native_fn_sub_A0B2_a0b2(ctx);
    return;
La002: ;
    smbneo_native_fn_sub_A006_a006(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    return;
    return;
}

void smbneo_native_fn_sub_A006_a006(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xa006u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x661fu));
    if ((ctx->status & NATIVE_Z)) {
        goto La014;
    }
    smbneo_native_fn_sub_A0D7_b_a0f3(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_write(ctx, 0x661fu, native_nz(ctx, (uint8_t)(native_read(ctx, 0x661fu) - 1u)));
    /* static tail transfer */
    goto La017;
La014: ;
    smbneo_native_fn_sub_9EDA_14_9ee3(ctx);
    if (ctx->suspended || ctx->unsupported) return;
La017: ;
    smbneo_native_fn_sub_A0B2_a0b2(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    return;
    return;
}

void smbneo_native_fn_sub_A01B_a01b(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xa01bu);
    ctx->a = native_nz(ctx, native_read(ctx, 0x06fcu));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x20u));
    if ((ctx->status & NATIVE_Z)) {
        goto La04e;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x6604u));
    if ((ctx->status & NATIVE_Z)) {
        goto La02b;
    }
    ctx->a = native_nz(ctx, 0x02u);
    if (!(ctx->status & NATIVE_Z)) {
        goto La02d;
    }
La02b: ;
    ctx->a = native_nz(ctx, 0x03u);
La02d: ;
    native_write(ctx, 0x075au, ctx->a);
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x6617u, ctx->a);
    native_write(ctx, 0x075cu, ctx->a);
    native_write(ctx, 0x0760u, ctx->a);
    native_write(ctx, 0x0772u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0772u) + 1u)));
    smbneo_native_fn_sub_9924_9924(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->y = native_nz(ctx, 0x05u);
La043: ;
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, (uint16_t)(native_zpword(ctx, 0x04u) + ctx->y), ctx->a);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    if (!(ctx->status & NATIVE_N)) {
        goto La043;
    }
    native_write(ctx, 0x6610u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x6610u) - 1u)));
    return;
La04e: ;
    smbneo_native_fn_sub_A075_a075(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_vs_disp_credit_msg_92e1(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->x = native_nz(ctx, 0x0cu);
    ctx->a = native_nz(ctx, native_read(ctx, 0x6622u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x01u));
    if (!(ctx->status & NATIVE_Z)) {
        goto La071;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x661fu));
    if ((ctx->status & NATIVE_Z)) {
        goto La06b;
    }
    smbneo_native_fn_sub_A0D7_a0d7(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_write(ctx, 0x661fu, native_nz(ctx, (uint8_t)(native_read(ctx, 0x661fu) - 1u)));
    /* static tail transfer */
    smbneo_native_fn_sub_A0B2_a0b2(ctx);
    return;
La06b: ;
    smbneo_native_fn_sub_9EDA_0A_9eda(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    /* static tail transfer */
    smbneo_native_fn_sub_A0B2_a0b2(ctx);
    return;
La071: ;
    /* static tail transfer */
    smbneo_native_fn_sub_A006_a006(ctx);
    return;
    return;
}

void smbneo_native_fn_region_a074_a074(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xa074u);
    return;
    return;
}

void smbneo_native_fn_sub_A075_a075(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xa075u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x6619u));
    if (!(ctx->status & NATIVE_Z)) {
        goto La08e;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x661au));
    if (!(ctx->status & NATIVE_Z)) {
        goto La08e;
    }
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x6617u, ctx->a);
    native_write(ctx, 0x07fcu, ctx->a);
    native_write(ctx, 0x076au, ctx->a);
    native_write(ctx, 0x0772u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0772u) + 1u)));
    return;
La08e: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x661bu));
    if ((ctx->status & NATIVE_Z)) {
        goto La099;
    }
    native_write(ctx, 0x661bu, native_nz(ctx, (uint8_t)(native_read(ctx, 0x661bu) - 1u)));
    /* static tail transfer */
    goto La0b1;
La099: ;
    ctx->a = native_nz(ctx, 0x48u);
    native_write(ctx, 0x661bu, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x661au));
    if (!(ctx->status & NATIVE_Z)) {
        goto La0ae;
    }
    ctx->a = native_nz(ctx, 0x09u);
    native_write(ctx, 0x661au, ctx->a);
    native_write(ctx, 0x6619u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x6619u) - 1u)));
    /* static tail transfer */
    goto La0b1;
La0ae: ;
    native_write(ctx, 0x661au, native_nz(ctx, (uint8_t)(native_read(ctx, 0x661au) - 1u)));
La0b1: ;
    return;
    return;
}

void smbneo_native_fn_sub_A0B2_a0b2(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xa0b2u);
    ctx->y = native_nz(ctx, 0x00u);
La0b4: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x9f08u + ctx->y)));
    native_write(ctx, (uint16_t)(0x0301u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    native_cmp(ctx, ctx->y, 0x03u);
    if (!(ctx->status & NATIVE_Z)) {
        goto La0b4;
    }
    ctx->y = native_nz(ctx, 0x00u);
La0c2: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x6619u + ctx->y)));
    native_write(ctx, (uint16_t)(0x0301u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    native_cmp(ctx, ctx->y, 0x02u);
    if (!(ctx->status & NATIVE_Z)) {
        goto La0c2;
    }
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, (uint16_t)(0x0301u + ctx->x), ctx->a);
    native_write(ctx, 0x0300u, ctx->x);
    return;
    return;
}

void smbneo_native_fn_sub_A0D7_a0d7(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xa0d7u);
    ctx->y = native_nz(ctx, 0x00u);
La0d9: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x6617u));
    native_cmp(ctx, ctx->a, 0x03u);
    if (!(ctx->status & NATIVE_Z)) {
        goto La0e6;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x9eecu + ctx->y)));
    /* static tail transfer */
    goto La0e9;
La0e6: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x9efau + ctx->y)));
La0e9: ;
    native_write(ctx, (uint16_t)(0x0301u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    native_cmp(ctx, ctx->y, 0x0eu);
    if (!(ctx->status & NATIVE_Z)) {
        goto La0d9;
    }
    return;
    return;
}

void smbneo_native_fn_sub_A0D7_b_a0f3(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xa0f3u);
    ctx->y = native_nz(ctx, 0x00u);
La0f5: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x6617u));
    native_cmp(ctx, ctx->a, 0x03u);
    if (!(ctx->status & NATIVE_Z)) {
        goto La102;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x9f0bu + ctx->y)));
    /* static tail transfer */
    goto La105;
La102: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x9f0fu + ctx->y)));
La105: ;
    native_write(ctx, (uint16_t)(0x0301u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    native_cmp(ctx, ctx->y, 0x04u);
    if (!(ctx->status & NATIVE_Z)) {
        goto La0f5;
    }
    return;
    return;
}

void smbneo_native_fn_game_over_init_a10f(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xa10fu);
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x073cu, ctx->a);
    native_write(ctx, 0x0722u, ctx->a);
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, 0xfcu, ctx->a);
    native_write(ctx, 0x0774u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0774u) + 1u)));
    native_write(ctx, 0x0772u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0772u) + 1u)));
    return;
    return;
}

void smbneo_native_fn_game_over_proc_a122(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xa122u);
    smbneo_native_fn_game_over_player_switch_a15d(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    if (!(ctx->status & NATIVE_C)) {
        goto La13f;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x075au));
    if (!(ctx->status & NATIVE_N)) {
        goto La13f;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x075fu));
    native_write(ctx, 0x07fdu, ctx->a);
    ctx->a = native_nz(ctx, 0x00u);
    ctx->a = native_asl(ctx, ctx->a);
    native_write(ctx, 0x0772u, ctx->a);
    native_write(ctx, 0x07a0u, ctx->a);
    native_write(ctx, 0x0770u, ctx->a);
    return;
La13f: ;
    smbneo_native_fn_course_descriptor_load_ab60(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, 0x0754u, ctx->a);
    native_write(ctx, 0x0757u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0757u) + 1u)));
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x0747u, ctx->a);
    native_write(ctx, 0x0756u, ctx->a);
    native_write(ctx, 0x0eu, ctx->a);
    native_write(ctx, 0x0772u, ctx->a);
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, 0x0770u, ctx->a);
    return;
    return;
}

void smbneo_native_fn_game_over_player_switch_a15d(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xa15du);
    uint8_t saved_value_0 = 0u;
    ctx->status |= NATIVE_C;
    ctx->a = native_nz(ctx, native_read(ctx, 0x077au));
    if ((ctx->status & NATIVE_Z)) {
        goto La184;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x0761u));
    if ((ctx->status & NATIVE_N)) {
        goto La184;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x0753u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a ^ 0x01u));
    native_write(ctx, 0x0753u, ctx->a);
    ctx->x = native_nz(ctx, 0x06u);
La172: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x075au + ctx->x)));
    saved_value_0 = ctx->a;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0761u + ctx->x)));
    native_write(ctx, (uint16_t)(0x075au + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, saved_value_0);
    native_write(ctx, (uint16_t)(0x0761u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x - 1u));
    if (!(ctx->status & NATIVE_N)) {
        goto La172;
    }
    ctx->status &= (uint8_t)~NATIVE_C;
La184: ;
    return;
    return;
}

void smbneo_native_fn_sub_92AA_a185(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xa185u);
    ctx->a = native_nz(ctx, 0xffu);
    native_write(ctx, 0x06c9u, ctx->a);
    smbneo_native_fn_sub_92AA_rts_a18a(ctx);
    return;
    return;
}

void smbneo_native_fn_sub_92AA_rts_a18a(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xa18au);
    return;
    return;
}

void smbneo_native_fn_scenery_proc_a18b(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xa18bu);
    ctx->y = native_nz(ctx, native_read(ctx, 0x071fu));
    if (!(ctx->status & NATIVE_Z)) {
        goto La195;
    }
    ctx->y = native_nz(ctx, 0x08u);
    native_write(ctx, 0x071fu, ctx->y);
La195: ;
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    ctx->a = native_nz(ctx, ctx->y);
    smbneo_native_fn_scenery_proc_do_a1a3(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_write(ctx, 0x071fu, native_nz(ctx, (uint8_t)(native_read(ctx, 0x071fu) - 1u)));
    if (!(ctx->status & NATIVE_Z)) {
        goto La1a2;
    }
    smbneo_native_fn_meta_render_attr_8b6b(ctx);
    if (ctx->suspended || ctx->unsupported) return;
La1a2: ;
    return;
    return;
}

void smbneo_native_fn_scenery_proc_do_a1a3(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xa1a3u);
    uint8_t table_selector_a1a3 = ctx->a;
    ctx->a = native_asl(ctx, ctx->a);
    ctx->y = native_nz(ctx, ctx->a);
    native_write(ctx, 0x04u, 0xa5u);
    native_write(ctx, 0x05u, 0xa1u);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    switch (table_selector_a1a3) {
    case 0: native_write(ctx, 0x06u, 0xb6u);
             native_write(ctx, 0x07u, 0xa1u);
             ctx->a = native_nz(ctx, 0xa1u);
             smbneo_native_fn_scenery_inc_seam_a1b6(ctx); return;
    case 1: native_write(ctx, 0x06u, 0xafu);
             native_write(ctx, 0x07u, 0x8au);
             ctx->a = native_nz(ctx, 0x8au);
             smbneo_native_fn_meta_render_area_8aaf(ctx); return;
    case 2: native_write(ctx, 0x06u, 0xafu);
             native_write(ctx, 0x07u, 0x8au);
             ctx->a = native_nz(ctx, 0x8au);
             smbneo_native_fn_meta_render_area_8aaf(ctx); return;
    case 3: native_write(ctx, 0x06u, 0xd7u);
             native_write(ctx, 0x07u, 0xa2u);
             ctx->a = native_nz(ctx, 0xa2u);
             smbneo_native_fn_scenery_proc_main_a2d7(ctx); return;
    case 4: native_write(ctx, 0x06u, 0xb6u);
             native_write(ctx, 0x07u, 0xa1u);
             ctx->a = native_nz(ctx, 0xa1u);
             smbneo_native_fn_scenery_inc_seam_a1b6(ctx); return;
    case 5: native_write(ctx, 0x06u, 0xafu);
             native_write(ctx, 0x07u, 0x8au);
             ctx->a = native_nz(ctx, 0x8au);
             smbneo_native_fn_meta_render_area_8aaf(ctx); return;
    case 6: native_write(ctx, 0x06u, 0xafu);
             native_write(ctx, 0x07u, 0x8au);
             ctx->a = native_nz(ctx, 0x8au);
             smbneo_native_fn_meta_render_area_8aaf(ctx); return;
    case 7: native_write(ctx, 0x06u, 0xd7u);
             native_write(ctx, 0x07u, 0xa2u);
             ctx->a = native_nz(ctx, 0xa2u);
             smbneo_native_fn_scenery_proc_main_a2d7(ctx); return;
    default: ctx->unsupported = 1; return;
    }
    return;
}

void smbneo_native_fn_scenery_inc_seam_a1b6(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xa1b6u);
    native_write(ctx, 0x0726u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0726u) + 1u)));
    ctx->a = native_nz(ctx, native_read(ctx, 0x0726u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x0fu));
    if (!(ctx->status & NATIVE_Z)) {
        goto La1c6;
    }
    native_write(ctx, 0x0726u, ctx->a);
    native_write(ctx, 0x0725u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0725u) + 1u)));
La1c6: ;
    native_write(ctx, 0x06a0u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x06a0u) + 1u)));
    ctx->a = native_nz(ctx, native_read(ctx, 0x06a0u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x1fu));
    native_write(ctx, 0x06a0u, ctx->a);
    return;
    return;
}

void smbneo_native_fn_scenery_proc_main_a2d7(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xa2d7u);
    uint8_t saved_value_0 = 0u;
    ctx->a = native_nz(ctx, native_read(ctx, 0x0728u));
    if ((ctx->status & NATIVE_Z)) {
        goto La2df;
    }
    smbneo_native_fn_scenery_do_a3e3(ctx);
    if (ctx->suspended || ctx->unsupported) return;
La2df: ;
    ctx->x = native_nz(ctx, 0x0cu);
    ctx->a = native_nz(ctx, 0x00u);
La2e3: ;
    native_write(ctx, (uint16_t)(0x06a1u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x - 1u));
    if (!(ctx->status & NATIVE_N)) {
        goto La2e3;
    }
    ctx->y = native_nz(ctx, native_read(ctx, 0x0742u));
    if ((ctx->status & NATIVE_Z)) {
        goto La330;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x0725u));
La2f1: ;
    native_cmp(ctx, ctx->a, 0x03u);
    if ((ctx->status & NATIVE_N)) {
        goto La2fa;
    }
    ctx->status |= NATIVE_C;
    native_sbc(ctx, 0x03u);
    if (!(ctx->status & NATIVE_N)) {
        goto La2f1;
    }
La2fa: ;
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    native_adc(ctx, native_read(ctx, (uint16_t)(0xa1d1u + ctx->y)));
    native_adc(ctx, native_read(ctx, 0x0726u));
    ctx->x = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xa1d5u + ctx->x)));
    if ((ctx->status & NATIVE_Z)) {
        goto La330;
    }
    saved_value_0 = ctx->a;
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x0fu));
    ctx->status |= NATIVE_C;
    native_sbc(ctx, 0x01u);
    native_write(ctx, 0x00u, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    native_adc(ctx, native_read(ctx, 0x00u));
    ctx->x = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, saved_value_0);
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->y = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, 0x03u);
    native_write(ctx, 0x00u, ctx->a);
La320: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xa265u + ctx->x)));
    native_write(ctx, (uint16_t)(0x06a1u + ctx->y), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    native_cmp(ctx, ctx->y, 0x0bu);
    if ((ctx->status & NATIVE_Z)) {
        goto La330;
    }
    native_write(ctx, 0x00u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x00u) - 1u)));
    if (!(ctx->status & NATIVE_Z)) {
        goto La320;
    }
La330: ;
    ctx->x = native_nz(ctx, native_read(ctx, 0x0741u));
    if ((ctx->status & NATIVE_Z)) {
        goto La348;
    }
    ctx->y = native_nz(ctx, native_read(ctx, (uint16_t)(0xa288u + ctx->x)));
    ctx->x = native_nz(ctx, 0x00u);
La33a: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xa28cu + ctx->y)));
    if ((ctx->status & NATIVE_Z)) {
        goto La342;
    }
    native_write(ctx, (uint16_t)(0x06a1u + ctx->x), ctx->a);
La342: ;
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    native_cmp(ctx, ctx->x, 0x0du);
    if (!(ctx->status & NATIVE_Z)) {
        goto La33a;
    }
La348: ;
    ctx->y = native_nz(ctx, native_read(ctx, 0x074eu));
    if (!(ctx->status & NATIVE_Z)) {
        goto La359;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x075fu));
    native_cmp(ctx, ctx->a, 0x07u);
    if (!(ctx->status & NATIVE_Z)) {
        goto La359;
    }
    ctx->a = native_nz(ctx, 0x62u);
    /* static tail transfer */
    goto La363;
La359: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xa2b3u + ctx->y)));
    ctx->y = native_nz(ctx, native_read(ctx, 0x0743u));
    if ((ctx->status & NATIVE_Z)) {
        goto La363;
    }
    ctx->a = native_nz(ctx, 0x88u);
La363: ;
    native_write(ctx, 0x07u, ctx->a);
    ctx->x = native_nz(ctx, 0x00u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0727u));
    ctx->a = native_asl(ctx, ctx->a);
    ctx->y = native_nz(ctx, ctx->a);
La36c: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xa2b7u + ctx->y)));
    native_write(ctx, 0x00u, ctx->a);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    native_write(ctx, 0x01u, ctx->y);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0743u));
    if ((ctx->status & NATIVE_Z)) {
        goto La383;
    }
    native_cmp(ctx, ctx->x, 0x00u);
    if ((ctx->status & NATIVE_Z)) {
        goto La383;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x00u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x08u));
    native_write(ctx, 0x00u, ctx->a);
La383: ;
    ctx->y = native_nz(ctx, 0x00u);
La385: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xc5cdu + ctx->y)));
    native_bit(ctx, native_read(ctx, 0x00u));
    if ((ctx->status & NATIVE_Z)) {
        goto La391;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x07u));
    native_write(ctx, (uint16_t)(0x06a1u + ctx->x), ctx->a);
La391: ;
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    native_cmp(ctx, ctx->x, 0x0du);
    if ((ctx->status & NATIVE_Z)) {
        goto La3ae;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x074eu));
    native_cmp(ctx, ctx->a, 0x02u);
    if (!(ctx->status & NATIVE_Z)) {
        goto La3a5;
    }
    native_cmp(ctx, ctx->x, 0x0bu);
    if (!(ctx->status & NATIVE_Z)) {
        goto La3a5;
    }
    ctx->a = native_nz(ctx, 0x54u);
    native_write(ctx, 0x07u, ctx->a);
La3a5: ;
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    native_cmp(ctx, ctx->y, 0x08u);
    if (!(ctx->status & NATIVE_Z)) {
        goto La385;
    }
    ctx->y = native_nz(ctx, native_read(ctx, 0x01u));
    if (!(ctx->status & NATIVE_Z)) {
        goto La36c;
    }
La3ae: ;
    smbneo_native_fn_scenery_do_a3e3(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, 0x06a0u));
    smbneo_native_fn_scenery_get_buffer_ptr_ab14(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->x = native_nz(ctx, 0x00u);
    ctx->y = native_nz(ctx, 0x00u);
La3bb: ;
    native_write(ctx, 0x00u, ctx->y);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x06a1u + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0xc0u));
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_rol(ctx, ctx->a);
    ctx->a = native_rol(ctx, ctx->a);
    ctx->y = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x06a1u + ctx->x)));
    native_cmp(ctx, ctx->a, native_read(ctx, (uint16_t)(0xa3dfu + ctx->y)));
    if ((ctx->status & NATIVE_C)) {
        goto La3d0;
    }
    ctx->a = native_nz(ctx, 0x00u);
La3d0: ;
    ctx->y = native_nz(ctx, native_read(ctx, 0x00u));
    native_write(ctx, (uint16_t)(native_zpword(ctx, 0x06u) + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, ctx->y);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x10u);
    ctx->y = native_nz(ctx, ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    native_cmp(ctx, ctx->x, 0x0du);
    if (!(ctx->status & NATIVE_C)) {
        goto La3bb;
    }
    return;
    return;
}

void smbneo_native_fn_scenery_do_a3e3(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xa3e3u);
    smbneo_native_fn_vs_setbank_low_820c(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, 0x4016u, ctx->a);
La3eb: ;
    ctx->x = native_nz(ctx, 0x02u);
La3ed: ;
    native_write(ctx, 0x08u, ctx->x);
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x0729u, ctx->a);
    ctx->y = native_nz(ctx, native_read(ctx, 0x072cu));
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, 0x4016u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0xe7u) + ctx->y)));
    native_cmp(ctx, ctx->a, 0xfdu);
    if ((ctx->status & NATIVE_Z)) {
        goto La457;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0730u + ctx->x)));
    if (!(ctx->status & NATIVE_N)) {
        goto La457;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0xe7u) + ctx->y)));
    ctx->a = native_asl(ctx, ctx->a);
    if (!(ctx->status & NATIVE_C)) {
        goto La418;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x072bu));
    if (!(ctx->status & NATIVE_Z)) {
        goto La418;
    }
    native_write(ctx, 0x072bu, native_nz(ctx, (uint8_t)(native_read(ctx, 0x072bu) + 1u)));
    native_write(ctx, 0x072au, native_nz(ctx, (uint8_t)(native_read(ctx, 0x072au) + 1u)));
La418: ;
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, 0x4016u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0xe7u) + ctx->y)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x0fu));
    native_cmp(ctx, ctx->a, 0x0du);
    if (!(ctx->status & NATIVE_Z)) {
        goto La446;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0xe7u) + ctx->y)));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x40u));
    if (!(ctx->status & NATIVE_Z)) {
        goto La44f;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x072bu));
    if (!(ctx->status & NATIVE_Z)) {
        goto La44f;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, 0x4016u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0xe7u) + ctx->y)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x1fu));
    native_write(ctx, 0x072au, ctx->a);
    native_write(ctx, 0x072bu, native_nz(ctx, (uint8_t)(native_read(ctx, 0x072bu) + 1u)));
    /* static tail transfer */
    goto La460;
La446: ;
    native_cmp(ctx, ctx->a, 0x0eu);
    if (!(ctx->status & NATIVE_Z)) {
        goto La44f;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x0728u));
    if (!(ctx->status & NATIVE_Z)) {
        goto La457;
    }
La44f: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x072au));
    native_cmp(ctx, ctx->a, native_read(ctx, 0x0725u));
    if (!(ctx->status & NATIVE_C)) {
        goto La45d;
    }
La457: ;
    smbneo_native_fn_scenery_decode_a493(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    /* static tail transfer */
    goto La463;
La45d: ;
    native_write(ctx, 0x0729u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0729u) + 1u)));
La460: ;
    smbneo_native_fn_scenery_offset_base_inc_a487(ctx);
    if (ctx->suspended || ctx->unsupported) return;
La463: ;
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0730u + ctx->x)));
    if ((ctx->status & NATIVE_N)) {
        goto La46d;
    }
    native_write(ctx, (uint16_t)(0x0730u + ctx->x), native_nz(ctx, (uint8_t)(native_read(ctx, (uint16_t)(0x0730u + ctx->x)) - 1u)));
La46d: ;
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x - 1u));
    if ((ctx->status & NATIVE_N)) {
        goto La473;
    }
    /* static tail transfer */
    goto La3ed;
La473: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x0729u));
    if ((ctx->status & NATIVE_Z)) {
        goto La47b;
    }
    /* static tail transfer */
    goto La3eb;
La47b: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x0728u));
    if ((ctx->status & NATIVE_Z)) {
        goto La483;
    }
    /* static tail transfer */
    goto La3eb;
La483: ;
    smbneo_native_fn_vs_setbank_low_820c(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    return;
    return;
}

void smbneo_native_fn_scenery_offset_base_inc_a487(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xa487u);
    native_write(ctx, 0x072cu, native_nz(ctx, (uint8_t)(native_read(ctx, 0x072cu) + 1u)));
    native_write(ctx, 0x072cu, native_nz(ctx, (uint8_t)(native_read(ctx, 0x072cu) + 1u)));
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x072bu, ctx->a);
    return;
    return;
}

void smbneo_native_fn_scenery_decode_a493(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xa493u);
    goto La493;
La483: ;
    smbneo_native_fn_vs_setbank_low_820c(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    return;
La493: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0730u + ctx->x)));
    if ((ctx->status & NATIVE_N)) {
        goto La49b;
    }
    ctx->y = native_nz(ctx, native_read(ctx, (uint16_t)(0x072du + ctx->x)));
La49b: ;
    ctx->x = native_nz(ctx, 0x10u);
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, 0x4016u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0xe7u) + ctx->y)));
    native_cmp(ctx, ctx->a, 0xfdu);
    if ((ctx->status & NATIVE_Z)) {
        goto La483;
    }
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x0fu));
    native_cmp(ctx, ctx->a, 0x0fu);
    if ((ctx->status & NATIVE_Z)) {
        goto La4b6;
    }
    ctx->x = native_nz(ctx, 0x08u);
    native_cmp(ctx, ctx->a, 0x0cu);
    if ((ctx->status & NATIVE_Z)) {
        goto La4b6;
    }
    ctx->x = native_nz(ctx, 0x00u);
La4b6: ;
    native_write(ctx, 0x07u, ctx->x);
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    native_cmp(ctx, ctx->a, 0x0eu);
    if (!(ctx->status & NATIVE_Z)) {
        goto La4c6;
    }
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x07u, ctx->a);
    ctx->a = native_nz(ctx, 0x2eu);
    if (!(ctx->status & NATIVE_Z)) {
        goto La52d;
    }
La4c6: ;
    native_cmp(ctx, ctx->a, 0x0du);
    if (!(ctx->status & NATIVE_Z)) {
        goto La4ea;
    }
    ctx->a = native_nz(ctx, 0x22u);
    native_write(ctx, 0x07u, ctx->a);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, 0x4016u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0xe7u) + ctx->y)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x40u));
    if ((ctx->status & NATIVE_Z)) {
        goto La551;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0xe7u) + ctx->y)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x7fu));
    native_cmp(ctx, ctx->a, 0x4bu);
    if (!(ctx->status & NATIVE_Z)) {
        goto La4e5;
    }
    native_write(ctx, 0x0745u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0745u) + 1u)));
La4e5: ;
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x3fu));
    /* static tail transfer */
    goto La52d;
La4ea: ;
    native_cmp(ctx, ctx->a, 0x0cu);
    if ((ctx->status & NATIVE_C)) {
        goto La51f;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, 0x4016u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0xe7u) + ctx->y)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x70u));
    if (!(ctx->status & NATIVE_Z)) {
        goto La505;
    }
    ctx->a = native_nz(ctx, 0x16u);
    native_write(ctx, 0x07u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0xe7u) + ctx->y)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x0fu));
    /* static tail transfer */
    goto La52d;
La505: ;
    native_write(ctx, 0x00u, ctx->a);
    native_cmp(ctx, ctx->a, 0x70u);
    if (!(ctx->status & NATIVE_Z)) {
        goto La51a;
    }
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, 0x4016u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0xe7u) + ctx->y)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x08u));
    if ((ctx->status & NATIVE_Z)) {
        goto La51a;
    }
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x00u, ctx->a);
La51a: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x00u));
    /* static tail transfer */
    goto La529;
La51f: ;
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, 0x4016u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0xe7u) + ctx->y)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x70u));
La529: ;
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
La52d: ;
    native_write(ctx, 0x00u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0730u + ctx->x)));
    if (!(ctx->status & NATIVE_N)) {
        goto La580;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x072au));
    native_cmp(ctx, ctx->a, native_read(ctx, 0x0725u));
    if ((ctx->status & NATIVE_Z)) {
        goto La552;
    }
    ctx->y = native_nz(ctx, native_read(ctx, 0x072cu));
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, 0x4016u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0xe7u) + ctx->y)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x0fu));
    native_cmp(ctx, ctx->a, 0x0eu);
    if (!(ctx->status & NATIVE_Z)) {
        goto La551;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x0728u));
    if (!(ctx->status & NATIVE_Z)) {
        goto La577;
    }
La551: ;
    return;
La552: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x0728u));
    if ((ctx->status & NATIVE_Z)) {
        goto La562;
    }
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x0728u, ctx->a);
    native_write(ctx, 0x0729u, ctx->a);
    native_write(ctx, 0x08u, ctx->a);
    smbneo_native_fn_scenery_decode_rts2_a561(ctx);
    return;
La562: ;
    ctx->y = native_nz(ctx, native_read(ctx, 0x072cu));
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, 0x4016u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0xe7u) + ctx->y)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0xf0u));
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    native_cmp(ctx, ctx->a, native_read(ctx, 0x0726u));
    if (!(ctx->status & NATIVE_Z)) {
        goto La551;
    }
La577: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x072cu));
    native_write(ctx, (uint16_t)(0x072du + ctx->x), ctx->a);
    smbneo_native_fn_scenery_offset_base_inc_a487(ctx);
    if (ctx->suspended || ctx->unsupported) return;
La580: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x00u));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, 0x07u));
    uint8_t table_selector_a585 = ctx->a;
    ctx->a = native_asl(ctx, ctx->a);
    ctx->y = native_nz(ctx, ctx->a);
    native_write(ctx, 0x04u, 0x87u);
    native_write(ctx, 0x05u, 0xa5u);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    switch (table_selector_a585) {
    case 0: native_write(ctx, 0x06u, 0x0bu);
             native_write(ctx, 0x07u, 0xa8u);
             ctx->a = native_nz(ctx, 0xa8u);
             smbneo_native_fn_scenery_pipe_vert_a80b(ctx); return;
    case 1: native_write(ctx, 0x06u, 0x66u);
             native_write(ctx, 0x07u, 0xa6u);
             ctx->a = native_nz(ctx, 0xa6u);
             smbneo_native_fn_scenery_ledge_a666(ctx); return;
    case 2: native_write(ctx, 0x06u, 0x5au);
             native_write(ctx, 0x07u, 0xa9u);
             ctx->a = native_nz(ctx, 0xa9u);
             smbneo_native_fn_scenery_brickrow_a95a(ctx); return;
    case 3: native_write(ctx, 0x06u, 0x6au);
             native_write(ctx, 0x07u, 0xa9u);
             ctx->a = native_nz(ctx, 0xa9u);
             smbneo_native_fn_scenery_floorrow_a96a(ctx); return;
    case 4: native_write(ctx, 0x06u, 0x1eu);
             native_write(ctx, 0x07u, 0xa9u);
             ctx->a = native_nz(ctx, 0xa9u);
             smbneo_native_fn_scenery_coinrow_a91e(ctx); return;
    case 5: native_write(ctx, 0x06u, 0x7cu);
             native_write(ctx, 0x07u, 0xa9u);
             ctx->a = native_nz(ctx, 0xa9u);
             smbneo_native_fn_scenery_brickcol_a97c(ctx); return;
    case 6: native_write(ctx, 0x06u, 0x85u);
             native_write(ctx, 0x07u, 0xa9u);
             ctx->a = native_nz(ctx, 0xa9u);
             smbneo_native_fn_scenery_floorcol_a985(ctx); return;
    case 7: native_write(ctx, 0x06u, 0x0bu);
             native_write(ctx, 0x07u, 0xa8u);
             ctx->a = native_nz(ctx, 0xa8u);
             smbneo_native_fn_scenery_pipe_vert_a80b(ctx); return;
    case 8: native_write(ctx, 0x06u, 0x6fu);
             native_write(ctx, 0x07u, 0xaau);
             ctx->a = native_nz(ctx, 0xaau);
             smbneo_native_fn_scenery_pit_aa6f(ctx); return;
    case 9: native_write(ctx, 0x06u, 0xe0u);
             native_write(ctx, 0x07u, 0xa6u);
             ctx->a = native_nz(ctx, 0xa6u);
             smbneo_native_fn_scenery_pulley_a6e0(ctx); return;
    case 10: native_write(ctx, 0x06u, 0xa1u);
             native_write(ctx, 0x07u, 0xa8u);
             ctx->a = native_nz(ctx, 0xa8u);
             smbneo_native_fn_scenery_bridge_hi_a8a1(ctx); return;
    case 11: native_write(ctx, 0x06u, 0xa6u);
             native_write(ctx, 0x07u, 0xa8u);
             ctx->a = native_nz(ctx, 0xa8u);
             smbneo_native_fn_scenery_bridge_mid_a8a6(ctx); return;
    case 12: native_write(ctx, 0x06u, 0xabu);
             native_write(ctx, 0x07u, 0xa8u);
             ctx->a = native_nz(ctx, 0xa8u);
             smbneo_native_fn_scenery_bridge_lo_a8ab(ctx); return;
    case 13: native_write(ctx, 0x06u, 0x7du);
             native_write(ctx, 0x07u, 0xa8u);
             ctx->a = native_nz(ctx, 0xa8u);
             smbneo_native_fn_scenery_waterhole_a87d(ctx); return;
    case 14: native_write(ctx, 0x06u, 0x8eu);
             native_write(ctx, 0x07u, 0xa8u);
             ctx->a = native_nz(ctx, 0xa8u);
             smbneo_native_fn_scenery_block_qhi_a88e(ctx); return;
    case 15: native_write(ctx, 0x06u, 0x93u);
             native_write(ctx, 0x07u, 0xa8u);
             ctx->a = native_nz(ctx, 0xa8u);
             smbneo_native_fn_scenery_block_qlo_a893(ctx); return;
    case 16: native_write(ctx, 0x06u, 0xfcu);
             native_write(ctx, 0x07u, 0xa8u);
             ctx->a = native_nz(ctx, 0xa8u);
             smbneo_native_fn_scenery_rope_norm_a8fc(ctx); return;
    case 17: native_write(ctx, 0x06u, 0x03u);
             native_write(ctx, 0x07u, 0xa9u);
             ctx->a = native_nz(ctx, 0xa9u);
             smbneo_native_fn_scenery_rope_plat_a903(ctx); return;
    case 18: native_write(ctx, 0x06u, 0x2cu);
             native_write(ctx, 0x07u, 0xa7u);
             ctx->a = native_nz(ctx, 0xa7u);
             smbneo_native_fn_scenery_castle_a72c(ctx); return;
    case 19: native_write(ctx, 0x06u, 0xe3u);
             native_write(ctx, 0x07u, 0xa9u);
             ctx->a = native_nz(ctx, 0xa9u);
             smbneo_native_fn_scenery_stairs_a9e3(ctx); return;
    case 20: native_write(ctx, 0x06u, 0xd1u);
             native_write(ctx, 0x07u, 0xa7u);
             ctx->a = native_nz(ctx, 0xa7u);
             smbneo_native_fn_scenery_pipe_exit_a7d1(ctx); return;
    case 21: native_write(ctx, 0x06u, 0xc0u);
             native_write(ctx, 0x07u, 0xa8u);
             ctx->a = native_nz(ctx, 0xa8u);
             smbneo_native_fn_scenery_pole_tip_a8c0(ctx); return;
    case 22: native_write(ctx, 0x06u, 0x3cu);
             native_write(ctx, 0x07u, 0xaau);
             ctx->a = native_nz(ctx, 0xaau);
             smbneo_native_fn_scenery_block_q_aa3c(ctx); return;
    case 23: native_write(ctx, 0x06u, 0x3cu);
             native_write(ctx, 0x07u, 0xaau);
             ctx->a = native_nz(ctx, 0xaau);
             smbneo_native_fn_scenery_block_q_aa3c(ctx); return;
    case 24: native_write(ctx, 0x06u, 0x3cu);
             native_write(ctx, 0x07u, 0xaau);
             ctx->a = native_nz(ctx, 0xaau);
             smbneo_native_fn_scenery_block_q_aa3c(ctx); return;
    case 25: native_write(ctx, 0x06u, 0x2fu);
             native_write(ctx, 0x07u, 0xaau);
             ctx->a = native_nz(ctx, 0xaau);
             smbneo_native_fn_scenery_hidden_one_up_aa2f(ctx); return;
    case 26: native_write(ctx, 0x06u, 0x47u);
             native_write(ctx, 0x07u, 0xaau);
             ctx->a = native_nz(ctx, 0xaau);
             smbneo_native_fn_scenery_block_item_aa47(ctx); return;
    case 27: native_write(ctx, 0x06u, 0x47u);
             native_write(ctx, 0x07u, 0xaau);
             ctx->a = native_nz(ctx, 0xaau);
             smbneo_native_fn_scenery_block_item_aa47(ctx); return;
    case 28: native_write(ctx, 0x06u, 0x47u);
             native_write(ctx, 0x07u, 0xaau);
             ctx->a = native_nz(ctx, 0xaau);
             smbneo_native_fn_scenery_block_item_aa47(ctx); return;
    case 29: native_write(ctx, 0x06u, 0x42u);
             native_write(ctx, 0x07u, 0xaau);
             ctx->a = native_nz(ctx, 0xaau);
             smbneo_native_fn_scenery_block_coin_aa42(ctx); return;
    case 30: native_write(ctx, 0x06u, 0x47u);
             native_write(ctx, 0x07u, 0xaau);
             ctx->a = native_nz(ctx, 0xaau);
             smbneo_native_fn_scenery_block_item_aa47(ctx); return;
    case 31: native_write(ctx, 0x06u, 0x95u);
             native_write(ctx, 0x07u, 0xa7u);
             ctx->a = native_nz(ctx, 0xa7u);
             smbneo_native_fn_scenery_pipe_water_a795(ctx); return;
    case 32: native_write(ctx, 0x06u, 0x45u);
             native_write(ctx, 0x07u, 0xa9u);
             ctx->a = native_nz(ctx, 0xa9u);
             smbneo_native_fn_scenery_empty_draw_a945(ctx); return;
    case 33: native_write(ctx, 0x06u, 0xffu);
             native_write(ctx, 0x07u, 0xa9u);
             ctx->a = native_nz(ctx, 0xa9u);
             smbneo_native_fn_scenery_spring_a9ff(ctx); return;
    case 34: native_write(ctx, 0x06u, 0xa8u);
             native_write(ctx, 0x07u, 0xa7u);
             ctx->a = native_nz(ctx, 0xa7u);
             smbneo_native_fn_scenery_pipe_intro_a7a8(ctx); return;
    case 35: native_write(ctx, 0x06u, 0xcau);
             native_write(ctx, 0x07u, 0xa8u);
             ctx->a = native_nz(ctx, 0xa8u);
             smbneo_native_fn_scenery_pole_a8ca(ctx); return;
    case 36: native_write(ctx, 0x06u, 0x35u);
             native_write(ctx, 0x07u, 0xa9u);
             ctx->a = native_nz(ctx, 0xa9u);
             smbneo_native_fn_scenery_castle_bridge_axe_a935(ctx); return;
    case 37: native_write(ctx, 0x06u, 0x3au);
             native_write(ctx, 0x07u, 0xa9u);
             ctx->a = native_nz(ctx, 0xa9u);
             smbneo_native_fn_scenery_castle_bridge_draw_a93a(ctx); return;
    case 38: native_write(ctx, 0x06u, 0x2du);
             native_write(ctx, 0x07u, 0xa9u);
             ctx->a = native_nz(ctx, 0xa9u);
             smbneo_native_fn_scenery_castle_bridge_a92d(ctx); return;
    case 39: native_write(ctx, 0x06u, 0x18u);
             native_write(ctx, 0x07u, 0xa6u);
             ctx->a = native_nz(ctx, 0xa6u);
             smbneo_native_fn_scenery_lock_warp_a618(ctx); return;
    case 40: native_write(ctx, 0x06u, 0x33u);
             native_write(ctx, 0x07u, 0xa6u);
             ctx->a = native_nz(ctx, 0xa6u);
             smbneo_native_fn_scenery_lock_a633(ctx); return;
    case 41: native_write(ctx, 0x06u, 0x33u);
             native_write(ctx, 0x07u, 0xa6u);
             ctx->a = native_nz(ctx, 0xa6u);
             smbneo_native_fn_scenery_lock_a633(ctx); return;
    case 42: native_write(ctx, 0x06u, 0x51u);
             native_write(ctx, 0x07u, 0xa6u);
             ctx->a = native_nz(ctx, 0xa6u);
             smbneo_native_fn_scenery_swarm_a651(ctx); return;
    case 43: native_write(ctx, 0x06u, 0x51u);
             native_write(ctx, 0x07u, 0xa6u);
             ctx->a = native_nz(ctx, 0xa6u);
             smbneo_native_fn_scenery_swarm_a651(ctx); return;
    case 44: native_write(ctx, 0x06u, 0x51u);
             native_write(ctx, 0x07u, 0xa6u);
             ctx->a = native_nz(ctx, 0xa6u);
             smbneo_native_fn_scenery_swarm_a651(ctx); return;
    case 45: native_write(ctx, 0x06u, 0x61u);
             native_write(ctx, 0x07u, 0xa5u);
             ctx->a = native_nz(ctx, 0xa5u);
             smbneo_native_fn_scenery_decode_rts2_a561(ctx); return;
    case 46: native_write(ctx, 0x06u, 0xe6u);
             native_write(ctx, 0x07u, 0xa5u);
             ctx->a = native_nz(ctx, 0xa5u);
             smbneo_native_fn_scenery_set_attr_a5e6(ctx); return;
    default: ctx->unsupported = 1; return;
    }
    return;
}

void smbneo_native_fn_scenery_decode_rts2_a561(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xa561u);
    return;
    return;
}

void smbneo_native_fn_scenery_set_attr_a5e6(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xa5e6u);
    uint8_t saved_value_0 = 0u;
    ctx->y = native_nz(ctx, native_read(ctx, (uint16_t)(0x072du + ctx->x)));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, 0x4016u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0xe7u) + ctx->y)));
    saved_value_0 = ctx->a;
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x40u));
    if (!(ctx->status & NATIVE_Z)) {
        goto La608;
    }
    ctx->a = native_nz(ctx, saved_value_0);
    saved_value_0 = ctx->a;
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x0fu));
    native_write(ctx, 0x0727u, ctx->a);
    ctx->a = native_nz(ctx, saved_value_0);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x30u));
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    native_write(ctx, 0x0742u, ctx->a);
    return;
La608: ;
    ctx->a = native_nz(ctx, saved_value_0);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x07u));
    native_cmp(ctx, ctx->a, 0x04u);
    if (!(ctx->status & NATIVE_C)) {
        goto La614;
    }
    native_write(ctx, 0x0744u, ctx->a);
    ctx->a = native_nz(ctx, 0x00u);
La614: ;
    native_write(ctx, 0x0741u, ctx->a);
    return;
    return;
}

void smbneo_native_fn_scenery_lock_warp_a618(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xa618u);
    ctx->x = native_nz(ctx, 0x04u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x075fu));
    if ((ctx->status & NATIVE_Z)) {
        goto La627;
    }
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    ctx->y = native_nz(ctx, native_read(ctx, 0x074eu));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    if (!(ctx->status & NATIVE_Z)) {
        goto La627;
    }
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
La627: ;
    ctx->a = native_nz(ctx, ctx->x);
    native_write(ctx, 0x06d6u, ctx->a);
    smbneo_native_fn_bg_text_draw_8a04(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, 0x0du);
    smbneo_native_fn_scenery_actor_kill_a63c(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_scenery_lock_a633(ctx);
    return;
    return;
}

void smbneo_native_fn_scenery_lock_a633(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xa633u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0723u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a ^ 0x01u));
    native_write(ctx, 0x0723u, ctx->a);
    return;
    return;
}

void smbneo_native_fn_scenery_actor_kill_a63c(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xa63cu);
    native_write(ctx, 0x00u, ctx->a);
    ctx->a = native_nz(ctx, 0x00u);
    ctx->x = native_nz(ctx, 0x04u);
La642: ;
    ctx->y = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x16u + ctx->x)));
    native_cmp(ctx, ctx->y, native_read(ctx, 0x00u));
    if (!(ctx->status & NATIVE_Z)) {
        goto La64a;
    }
    native_write(ctx, (uint16_t)(uint8_t)(0x0fu + ctx->x), ctx->a);
La64a: ;
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x - 1u));
    if (!(ctx->status & NATIVE_N)) {
        goto La642;
    }
    return;
    return;
}

void smbneo_native_fn_scenery_swarm_a651(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xa651u);
    ctx->x = native_nz(ctx, native_read(ctx, 0x00u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xa646u + ctx->x)));
    ctx->y = native_nz(ctx, 0x05u);
La658: ;
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    if ((ctx->status & NATIVE_N)) {
        goto La662;
    }
    native_cmp(ctx, ctx->a, native_read(ctx, (uint16_t)(0x0016u + ctx->y)));
    if (!(ctx->status & NATIVE_Z)) {
        goto La658;
    }
    ctx->a = native_nz(ctx, 0x00u);
La662: ;
    native_write(ctx, 0x06cdu, ctx->a);
    return;
    return;
}

void smbneo_native_fn_scenery_ledge_a666(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xa666u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0733u));
    uint8_t table_selector_a669 = ctx->a;
    ctx->a = native_asl(ctx, ctx->a);
    ctx->y = native_nz(ctx, ctx->a);
    native_write(ctx, 0x04u, 0x6bu);
    native_write(ctx, 0x05u, 0xa6u);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    switch (table_selector_a669) {
    case 0: native_write(ctx, 0x06u, 0x72u);
             native_write(ctx, 0x07u, 0xa6u);
             ctx->a = native_nz(ctx, 0xa6u);
             smbneo_native_fn_scenery_ledge_tree_a672(ctx); return;
    case 1: native_write(ctx, 0x06u, 0x9eu);
             native_write(ctx, 0x07u, 0xa6u);
             ctx->a = native_nz(ctx, 0xa6u);
             smbneo_native_fn_scenery_ledge_mushroom_a69e(ctx); return;
    case 2: native_write(ctx, 0x06u, 0x95u);
             native_write(ctx, 0x07u, 0xa9u);
             ctx->a = native_nz(ctx, 0xa9u);
             smbneo_native_fn_scenery_cannon_a995(ctx); return;
    default: ctx->unsupported = 1; return;
    }
    return;
}

void smbneo_native_fn_scenery_ledge_tree_a672(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xa672u);
    smbneo_native_fn_scenery_attr_aae9(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0730u + ctx->x)));
    if ((ctx->status & NATIVE_Z)) {
        goto La699;
    }
    if (!(ctx->status & NATIVE_N)) {
        goto La68d;
    }
    ctx->a = native_nz(ctx, ctx->y);
    native_write(ctx, (uint16_t)(0x0730u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0725u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | native_read(ctx, 0x0726u)));
    if ((ctx->status & NATIVE_Z)) {
        goto La68d;
    }
    ctx->a = native_nz(ctx, 0x16u);
    /* static tail transfer */
    goto La6d6;
La68d: ;
    ctx->x = native_nz(ctx, native_read(ctx, 0x07u));
    ctx->a = native_nz(ctx, 0x17u);
    native_write(ctx, (uint16_t)(0x06a1u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x4cu);
    /* static tail transfer */
    goto La6d0;
La699: ;
    ctx->a = native_nz(ctx, 0x18u);
    /* static tail transfer */
    goto La6d6;
La6d0: ;
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    ctx->y = native_nz(ctx, 0x0fu);
    /* static tail transfer */
    smbneo_native_fn_scenery_render_aaab(ctx);
    return;
La6d6: ;
    ctx->x = native_nz(ctx, native_read(ctx, 0x07u));
    ctx->y = native_nz(ctx, 0x00u);
    /* static tail transfer */
    smbneo_native_fn_scenery_render_aaab(ctx);
    return;
    return;
}

void smbneo_native_fn_scenery_ledge_mushroom_a69e(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xa69eu);
    smbneo_native_fn_scenery_len_aada(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_write(ctx, 0x06u, ctx->y);
    if (!(ctx->status & NATIVE_C)) {
        goto La6b1;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0730u + ctx->x)));
    ctx->a = native_lsr(ctx, ctx->a);
    native_write(ctx, (uint16_t)(0x0736u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x19u);
    /* static tail transfer */
    goto La6d6;
La6b1: ;
    ctx->a = native_nz(ctx, 0x1bu);
    ctx->y = native_nz(ctx, native_read(ctx, (uint16_t)(0x0730u + ctx->x)));
    if ((ctx->status & NATIVE_Z)) {
        goto La6d6;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0736u + ctx->x)));
    native_write(ctx, 0x06u, ctx->a);
    ctx->x = native_nz(ctx, native_read(ctx, 0x07u));
    ctx->a = native_nz(ctx, 0x1au);
    native_write(ctx, (uint16_t)(0x06a1u + ctx->x), ctx->a);
    native_cmp(ctx, ctx->y, native_read(ctx, 0x06u));
    if (!(ctx->status & NATIVE_Z)) {
        goto La6f4;
    }
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    ctx->a = native_nz(ctx, 0x4fu);
    native_write(ctx, (uint16_t)(0x06a1u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x50u);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    ctx->y = native_nz(ctx, 0x0fu);
    /* static tail transfer */
    smbneo_native_fn_scenery_render_aaab(ctx);
    return;
La6d6: ;
    ctx->x = native_nz(ctx, native_read(ctx, 0x07u));
    ctx->y = native_nz(ctx, 0x00u);
    /* static tail transfer */
    smbneo_native_fn_scenery_render_aaab(ctx);
    return;
La6f4: ;
    return;
    return;
}

void smbneo_native_fn_scenery_pulley_a6e0(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xa6e0u);
    smbneo_native_fn_scenery_len_aada(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->y = native_nz(ctx, 0x00u);
    if ((ctx->status & NATIVE_C)) {
        goto La6ee;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0730u + ctx->x)));
    if (!(ctx->status & NATIVE_Z)) {
        goto La6ee;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
La6ee: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xa6ddu + ctx->y)));
    native_write(ctx, 0x06a1u, ctx->a);
    return;
    return;
}

void smbneo_native_fn_scenery_castle_a72c(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xa72cu);
    uint8_t saved_value_0 = 0u;
    smbneo_native_fn_scenery_attr_aae9(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_write(ctx, 0x07u, ctx->y);
    ctx->y = native_nz(ctx, 0x04u);
    smbneo_native_fn_scenery_len_b_aadd(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, ctx->x);
    saved_value_0 = ctx->a;
    ctx->y = native_nz(ctx, native_read(ctx, (uint16_t)(0x0730u + ctx->x)));
    ctx->x = native_nz(ctx, native_read(ctx, 0x07u));
    ctx->a = native_nz(ctx, 0x0bu);
    native_write(ctx, 0x06u, ctx->a);
La741: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xa6f5u + ctx->y)));
    native_write(ctx, (uint16_t)(0x06a1u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    ctx->a = native_nz(ctx, native_read(ctx, 0x06u));
    if ((ctx->status & NATIVE_Z)) {
        goto La753;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    native_write(ctx, 0x06u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x06u) - 1u)));
La753: ;
    native_cmp(ctx, ctx->x, 0x0bu);
    if (!(ctx->status & NATIVE_Z)) {
        goto La741;
    }
    ctx->a = native_nz(ctx, saved_value_0);
    ctx->x = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0725u));
    if ((ctx->status & NATIVE_Z)) {
        goto La794;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0730u + ctx->x)));
    native_cmp(ctx, ctx->a, 0x01u);
    if ((ctx->status & NATIVE_Z)) {
        goto La78f;
    }
    ctx->y = native_nz(ctx, native_read(ctx, 0x07u));
    if (!(ctx->status & NATIVE_Z)) {
        goto La76d;
    }
    native_cmp(ctx, ctx->a, 0x03u);
    if ((ctx->status & NATIVE_Z)) {
        goto La78f;
    }
La76d: ;
    native_cmp(ctx, ctx->a, 0x02u);
    if (!(ctx->status & NATIVE_Z)) {
        goto La794;
    }
    smbneo_native_fn_scenery_pos_x_get_aafe(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    saved_value_0 = ctx->a;
    smbneo_native_fn_scenery_alloc_a870(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, saved_value_0);
    native_write(ctx, (uint16_t)(uint8_t)(0x87u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0725u));
    native_write(ctx, (uint16_t)(uint8_t)(0x6eu + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, (uint16_t)(uint8_t)(0xb6u + ctx->x), ctx->a);
    native_write(ctx, (uint16_t)(uint8_t)(0x0fu + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x90u);
    native_write(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x31u);
    native_write(ctx, (uint16_t)(uint8_t)(0x16u + ctx->x), ctx->a);
    return;
La78f: ;
    ctx->y = native_nz(ctx, 0x52u);
    native_write(ctx, 0x06abu, ctx->y);
La794: ;
    return;
    return;
}

void smbneo_native_fn_scenery_pipe_water_a795(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xa795u);
    smbneo_native_fn_scenery_attr_aae9(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->y = native_nz(ctx, native_read(ctx, (uint16_t)(0x0730u + ctx->x)));
    ctx->x = native_nz(ctx, native_read(ctx, 0x07u));
    ctx->a = native_nz(ctx, 0x6bu);
    native_write(ctx, (uint16_t)(0x06a1u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x6cu);
    native_write(ctx, (uint16_t)(0x06a2u + ctx->x), ctx->a);
    return;
    return;
}

void smbneo_native_fn_scenery_pipe_intro_a7a8(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xa7a8u);
    ctx->y = native_nz(ctx, 0x03u);
    smbneo_native_fn_scenery_len_b_aadd(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->y = native_nz(ctx, 0x0au);
    smbneo_native_fn_scenery_pipe_horiz_a7d9(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    if ((ctx->status & NATIVE_C)) {
        goto La7c4;
    }
    ctx->x = native_nz(ctx, 0x06u);
La7b6: ;
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, (uint16_t)(0x06a1u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x - 1u));
    if (!(ctx->status & NATIVE_N)) {
        goto La7b6;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xa803u + ctx->y)));
    native_write(ctx, 0x06a8u, ctx->a);
La7c4: ;
    return;
    return;
}

void smbneo_native_fn_scenery_pipe_exit_a7d1(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xa7d1u);
    ctx->y = native_nz(ctx, 0x03u);
    smbneo_native_fn_scenery_len_b_aadd(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_scenery_attr_aae9(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_scenery_pipe_horiz_a7d9(ctx);
    return;
    return;
}

void smbneo_native_fn_scenery_pipe_horiz_a7d9(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xa7d9u);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    native_write(ctx, 0x05u, ctx->y);
    ctx->y = native_nz(ctx, native_read(ctx, (uint16_t)(0x0730u + ctx->x)));
    native_write(ctx, 0x06u, ctx->y);
    ctx->x = native_nz(ctx, native_read(ctx, 0x05u));
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xa7c5u + ctx->y)));
    native_cmp(ctx, ctx->a, 0x00u);
    if ((ctx->status & NATIVE_Z)) {
        goto La7f4;
    }
    ctx->x = native_nz(ctx, 0x00u);
    ctx->y = native_nz(ctx, native_read(ctx, 0x05u));
    smbneo_native_fn_scenery_render_aaab(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->status &= (uint8_t)~NATIVE_C;
La7f4: ;
    ctx->y = native_nz(ctx, native_read(ctx, 0x06u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xa7c9u + ctx->y)));
    native_write(ctx, (uint16_t)(0x06a1u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xa7cdu + ctx->y)));
    native_write(ctx, (uint16_t)(0x06a2u + ctx->x), ctx->a);
    return;
    return;
}

void smbneo_native_fn_scenery_pipe_vert_a80b(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xa80bu);
    uint8_t saved_value_0 = 0u;
    smbneo_native_fn_scenery_pipe_h_a85f(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, 0x00u));
    if ((ctx->status & NATIVE_Z)) {
        goto La816;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
La816: ;
    ctx->a = native_nz(ctx, ctx->y);
    saved_value_0 = ctx->a;
    ctx->a = native_nz(ctx, native_read(ctx, 0x0760u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | native_read(ctx, 0x075fu)));
    if ((ctx->status & NATIVE_Z)) {
        goto La84b;
    }
    ctx->y = native_nz(ctx, native_read(ctx, (uint16_t)(0x0730u + ctx->x)));
    if ((ctx->status & NATIVE_Z)) {
        goto La84b;
    }
    smbneo_native_fn_scenery_alloc_a870(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    if ((ctx->status & NATIVE_C)) {
        goto La84b;
    }
    smbneo_native_fn_scenery_pos_x_get_aafe(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x08u);
    native_write(ctx, (uint16_t)(uint8_t)(0x87u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0725u));
    native_adc(ctx, 0x00u);
    native_write(ctx, (uint16_t)(uint8_t)(0x6eu + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, (uint16_t)(uint8_t)(0xb6u + ctx->x), ctx->a);
    native_write(ctx, (uint16_t)(uint8_t)(0x0fu + ctx->x), ctx->a);
    smbneo_native_fn_scenery_pos_y_get_ab06(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_write(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x0du);
    native_write(ctx, (uint16_t)(uint8_t)(0x16u + ctx->x), ctx->a);
    smbneo_native_fn_actor_A_0D_init_c6ca(ctx);
    if (ctx->suspended || ctx->unsupported) return;
La84b: ;
    ctx->a = native_nz(ctx, saved_value_0);
    ctx->y = native_nz(ctx, ctx->a);
    ctx->x = native_nz(ctx, native_read(ctx, 0x07u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xa803u + ctx->y)));
    native_write(ctx, (uint16_t)(0x06a1u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xa805u + ctx->y)));
    ctx->y = native_nz(ctx, native_read(ctx, 0x06u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    /* static tail transfer */
    smbneo_native_fn_scenery_render_aaab(ctx);
    return;
    return;
}

void smbneo_native_fn_scenery_pipe_h_a85f(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xa85fu);
    ctx->y = native_nz(ctx, 0x01u);
    smbneo_native_fn_scenery_len_b_aadd(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_scenery_attr_aae9(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, ctx->y);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x07u));
    native_write(ctx, 0x06u, ctx->a);
    ctx->y = native_nz(ctx, native_read(ctx, (uint16_t)(0x0730u + ctx->x)));
    return;
    return;
}

void smbneo_native_fn_scenery_alloc_a870(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xa870u);
    ctx->x = native_nz(ctx, 0x00u);
La872: ;
    ctx->status &= (uint8_t)~NATIVE_C;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x0fu + ctx->x)));
    if ((ctx->status & NATIVE_Z)) {
        goto La87c;
    }
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    native_cmp(ctx, ctx->x, 0x05u);
    if (!(ctx->status & NATIVE_Z)) {
        goto La872;
    }
La87c: ;
    return;
    return;
}

void smbneo_native_fn_scenery_waterhole_a87d(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xa87du);
    smbneo_native_fn_scenery_len_aada(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, 0x86u);
    native_write(ctx, 0x06abu, ctx->a);
    ctx->x = native_nz(ctx, 0x0bu);
    ctx->y = native_nz(ctx, 0x01u);
    ctx->a = native_nz(ctx, 0x87u);
    /* static tail transfer */
    smbneo_native_fn_scenery_render_aaab(ctx);
    return;
    return;
}

void smbneo_native_fn_scenery_block_qhi_a88e(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xa88eu);
    uint8_t saved_value_0 = 0u;
    ctx->a = native_nz(ctx, 0x03u);
    /* static tail transfer */
    goto La895;
La895: ;
    saved_value_0 = ctx->a;
    smbneo_native_fn_scenery_len_aada(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, saved_value_0);
    ctx->x = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, 0xc0u);
    native_write(ctx, (uint16_t)(0x06a1u + ctx->x), ctx->a);
    return;
    return;
}

void smbneo_native_fn_scenery_block_qlo_a893(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xa893u);
    uint8_t saved_value_0 = 0u;
    ctx->a = native_nz(ctx, 0x07u);
    saved_value_0 = ctx->a;
    smbneo_native_fn_scenery_len_aada(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, saved_value_0);
    ctx->x = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, 0xc0u);
    native_write(ctx, (uint16_t)(0x06a1u + ctx->x), ctx->a);
    return;
    return;
}

void smbneo_native_fn_scenery_bridge_hi_a8a1(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xa8a1u);
    uint8_t saved_value_0 = 0u;
    ctx->a = native_nz(ctx, 0x06u);
    /* static tail transfer */
    goto La8ad;
La8ad: ;
    saved_value_0 = ctx->a;
    smbneo_native_fn_scenery_len_aada(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, saved_value_0);
    ctx->x = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, 0x0bu);
    native_write(ctx, (uint16_t)(0x06a1u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    ctx->y = native_nz(ctx, 0x00u);
    ctx->a = native_nz(ctx, 0x63u);
    /* static tail transfer */
    smbneo_native_fn_scenery_render_aaab(ctx);
    return;
    return;
}

void smbneo_native_fn_scenery_bridge_mid_a8a6(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xa8a6u);
    uint8_t saved_value_0 = 0u;
    ctx->a = native_nz(ctx, 0x07u);
    /* static tail transfer */
    goto La8ad;
La8ad: ;
    saved_value_0 = ctx->a;
    smbneo_native_fn_scenery_len_aada(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, saved_value_0);
    ctx->x = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, 0x0bu);
    native_write(ctx, (uint16_t)(0x06a1u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    ctx->y = native_nz(ctx, 0x00u);
    ctx->a = native_nz(ctx, 0x63u);
    /* static tail transfer */
    smbneo_native_fn_scenery_render_aaab(ctx);
    return;
    return;
}

void smbneo_native_fn_scenery_bridge_lo_a8ab(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xa8abu);
    uint8_t saved_value_0 = 0u;
    ctx->a = native_nz(ctx, 0x09u);
    saved_value_0 = ctx->a;
    smbneo_native_fn_scenery_len_aada(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, saved_value_0);
    ctx->x = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, 0x0bu);
    native_write(ctx, (uint16_t)(0x06a1u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    ctx->y = native_nz(ctx, 0x00u);
    ctx->a = native_nz(ctx, 0x63u);
    /* static tail transfer */
    smbneo_native_fn_scenery_render_aaab(ctx);
    return;
    return;
}

void smbneo_native_fn_scenery_pole_tip_a8c0(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xa8c0u);
    smbneo_native_fn_scenery_attr_aae9(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->x = native_nz(ctx, 0x02u);
    ctx->a = native_nz(ctx, 0x6du);
    /* static tail transfer */
    smbneo_native_fn_scenery_render_aaab(ctx);
    return;
    return;
}

void smbneo_native_fn_scenery_pole_a8ca(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xa8cau);
    ctx->a = native_nz(ctx, 0x24u);
    native_write(ctx, 0x06a1u, ctx->a);
    ctx->x = native_nz(ctx, 0x01u);
    ctx->y = native_nz(ctx, 0x08u);
    ctx->a = native_nz(ctx, 0x25u);
    smbneo_native_fn_scenery_render_aaab(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, 0x61u);
    native_write(ctx, 0x06abu, ctx->a);
    smbneo_native_fn_scenery_pos_x_get_aafe(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->status |= NATIVE_C;
    native_sbc(ctx, 0x08u);
    native_write(ctx, 0x8cu, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0725u));
    native_sbc(ctx, 0x00u);
    native_write(ctx, 0x73u, ctx->a);
    ctx->a = native_nz(ctx, 0x30u);
    native_write(ctx, 0xd4u, ctx->a);
    ctx->a = native_nz(ctx, 0xb0u);
    native_write(ctx, 0x010du, ctx->a);
    ctx->a = native_nz(ctx, 0x30u);
    native_write(ctx, 0x1bu, ctx->a);
    native_write(ctx, 0x14u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x14u) + 1u)));
    return;
    return;
}

void smbneo_native_fn_scenery_rope_norm_a8fc(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xa8fcu);
    ctx->x = native_nz(ctx, 0x00u);
    ctx->y = native_nz(ctx, 0x0fu);
    /* static tail transfer */
    goto La915;
La915: ;
    ctx->a = native_nz(ctx, 0x40u);
    /* static tail transfer */
    smbneo_native_fn_scenery_render_aaab(ctx);
    return;
    return;
}

void smbneo_native_fn_scenery_rope_plat_a903(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xa903u);
    uint8_t saved_value_0 = 0u;
    ctx->a = native_nz(ctx, ctx->x);
    saved_value_0 = ctx->a;
    ctx->x = native_nz(ctx, 0x01u);
    ctx->y = native_nz(ctx, 0x0fu);
    ctx->a = native_nz(ctx, 0x44u);
    smbneo_native_fn_scenery_render_aaab(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, saved_value_0);
    ctx->x = native_nz(ctx, ctx->a);
    smbneo_native_fn_scenery_attr_aae9(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->x = native_nz(ctx, 0x01u);
    ctx->a = native_nz(ctx, 0x40u);
    /* static tail transfer */
    smbneo_native_fn_scenery_render_aaab(ctx);
    return;
    return;
}

void smbneo_native_fn_scenery_coinrow_a91e(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xa91eu);
    uint8_t saved_value_0 = 0u;
    ctx->y = native_nz(ctx, native_read(ctx, 0x074eu));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xa91au + ctx->y)));
    /* static tail transfer */
    goto La970;
La970: ;
    saved_value_0 = ctx->a;
    smbneo_native_fn_scenery_len_aada(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->x = native_nz(ctx, native_read(ctx, 0x07u));
    ctx->y = native_nz(ctx, 0x00u);
    ctx->a = native_nz(ctx, saved_value_0);
    /* static tail transfer */
    smbneo_native_fn_scenery_render_aaab(ctx);
    return;
    return;
}

void smbneo_native_fn_scenery_castle_bridge_a92d(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xa92du);
    ctx->y = native_nz(ctx, 0x0cu);
    smbneo_native_fn_scenery_len_b_aadd(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    /* static tail transfer */
    smbneo_native_fn_scenery_castle_bridge_draw_a93a(ctx);
    return;
    return;
}

void smbneo_native_fn_scenery_castle_bridge_axe_a935(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xa935u);
    ctx->a = native_nz(ctx, 0x08u);
    native_write(ctx, 0x0773u, ctx->a);
    smbneo_native_fn_scenery_castle_bridge_draw_a93a(ctx);
    return;
    return;
}

void smbneo_native_fn_scenery_castle_bridge_draw_a93a(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xa93au);
    ctx->y = native_nz(ctx, native_read(ctx, 0x00u));
    ctx->x = native_nz(ctx, native_read(ctx, (uint16_t)(0xa925u + ctx->y)));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xa928u + ctx->y)));
    /* static tail transfer */
    goto La94c;
La94c: ;
    ctx->y = native_nz(ctx, 0x00u);
    /* static tail transfer */
    smbneo_native_fn_scenery_render_aaab(ctx);
    return;
    return;
}

void smbneo_native_fn_scenery_empty_draw_a945(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xa945u);
    smbneo_native_fn_scenery_attr_aae9(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->x = native_nz(ctx, native_read(ctx, 0x07u));
    ctx->a = native_nz(ctx, 0xc4u);
    ctx->y = native_nz(ctx, 0x00u);
    /* static tail transfer */
    smbneo_native_fn_scenery_render_aaab(ctx);
    return;
    return;
}

void smbneo_native_fn_scenery_brickrow_a95a(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xa95au);
    uint8_t saved_value_0 = 0u;
    ctx->y = native_nz(ctx, native_read(ctx, 0x074eu));
    ctx->a = native_nz(ctx, native_read(ctx, 0x0743u));
    if ((ctx->status & NATIVE_Z)) {
        goto La964;
    }
    ctx->y = native_nz(ctx, 0x04u);
La964: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xa955u + ctx->y)));
    /* static tail transfer */
    goto La970;
La970: ;
    saved_value_0 = ctx->a;
    smbneo_native_fn_scenery_len_aada(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->x = native_nz(ctx, native_read(ctx, 0x07u));
    ctx->y = native_nz(ctx, 0x00u);
    ctx->a = native_nz(ctx, saved_value_0);
    /* static tail transfer */
    smbneo_native_fn_scenery_render_aaab(ctx);
    return;
    return;
}

void smbneo_native_fn_scenery_floorrow_a96a(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xa96au);
    uint8_t saved_value_0 = 0u;
    ctx->y = native_nz(ctx, native_read(ctx, 0x074eu));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xa951u + ctx->y)));
    saved_value_0 = ctx->a;
    smbneo_native_fn_scenery_len_aada(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->x = native_nz(ctx, native_read(ctx, 0x07u));
    ctx->y = native_nz(ctx, 0x00u);
    ctx->a = native_nz(ctx, saved_value_0);
    /* static tail transfer */
    smbneo_native_fn_scenery_render_aaab(ctx);
    return;
    return;
}

void smbneo_native_fn_scenery_brickcol_a97c(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xa97cu);
    uint8_t saved_value_0 = 0u;
    ctx->y = native_nz(ctx, native_read(ctx, 0x074eu));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xa955u + ctx->y)));
    /* static tail transfer */
    goto La98b;
La98b: ;
    saved_value_0 = ctx->a;
    smbneo_native_fn_scenery_attr_aae9(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, saved_value_0);
    ctx->x = native_nz(ctx, native_read(ctx, 0x07u));
    /* static tail transfer */
    smbneo_native_fn_scenery_render_aaab(ctx);
    return;
    return;
}

void smbneo_native_fn_scenery_floorcol_a985(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xa985u);
    uint8_t saved_value_0 = 0u;
    ctx->y = native_nz(ctx, native_read(ctx, 0x074eu));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xa951u + ctx->y)));
    saved_value_0 = ctx->a;
    smbneo_native_fn_scenery_attr_aae9(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, saved_value_0);
    ctx->x = native_nz(ctx, native_read(ctx, 0x07u));
    /* static tail transfer */
    smbneo_native_fn_scenery_render_aaab(ctx);
    return;
    return;
}

void smbneo_native_fn_scenery_cannon_a995(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xa995u);
    smbneo_native_fn_scenery_attr_aae9(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->x = native_nz(ctx, native_read(ctx, 0x07u));
    ctx->a = native_nz(ctx, 0x64u);
    native_write(ctx, (uint16_t)(0x06a1u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    if ((ctx->status & NATIVE_N)) {
        goto La9b1;
    }
    ctx->a = native_nz(ctx, 0x65u);
    native_write(ctx, (uint16_t)(0x06a1u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    if ((ctx->status & NATIVE_N)) {
        goto La9b1;
    }
    ctx->a = native_nz(ctx, 0x66u);
    smbneo_native_fn_scenery_render_aaab(ctx);
    if (ctx->suspended || ctx->unsupported) return;
La9b1: ;
    ctx->x = native_nz(ctx, native_read(ctx, 0x046au));
    smbneo_native_fn_scenery_pos_y_get_ab06(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_write(ctx, (uint16_t)(0x0477u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0725u));
    native_write(ctx, (uint16_t)(0x046bu + ctx->x), ctx->a);
    smbneo_native_fn_scenery_pos_x_get_aafe(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_write(ctx, (uint16_t)(0x0471u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    native_cmp(ctx, ctx->x, 0x06u);
    if (!(ctx->status & NATIVE_C)) {
        goto La9cd;
    }
    ctx->x = native_nz(ctx, 0x00u);
La9cd: ;
    native_write(ctx, 0x046au, ctx->x);
    return;
    return;
}

void smbneo_native_fn_scenery_stairs_a9e3(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xa9e3u);
    smbneo_native_fn_scenery_len_aada(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    if (!(ctx->status & NATIVE_C)) {
        goto La9ed;
    }
    ctx->a = native_nz(ctx, 0x09u);
    native_write(ctx, 0x0734u, ctx->a);
La9ed: ;
    native_write(ctx, 0x0734u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0734u) - 1u)));
    ctx->y = native_nz(ctx, native_read(ctx, 0x0734u));
    ctx->x = native_nz(ctx, native_read(ctx, (uint16_t)(0xa9dau + ctx->y)));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xa9d1u + ctx->y)));
    ctx->y = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, 0x61u);
    /* static tail transfer */
    smbneo_native_fn_scenery_render_aaab(ctx);
    return;
    return;
}

void smbneo_native_fn_scenery_spring_a9ff(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xa9ffu);
    smbneo_native_fn_scenery_attr_aae9(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_scenery_alloc_a870(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    if ((ctx->status & NATIVE_C)) {
        goto Laa2e;
    }
    smbneo_native_fn_scenery_pos_x_get_aafe(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_write(ctx, (uint16_t)(uint8_t)(0x87u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0725u));
    native_write(ctx, (uint16_t)(uint8_t)(0x6eu + ctx->x), ctx->a);
    smbneo_native_fn_scenery_pos_y_get_ab06(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_write(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x), ctx->a);
    native_write(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x32u);
    native_write(ctx, (uint16_t)(uint8_t)(0x16u + ctx->x), ctx->a);
    ctx->y = native_nz(ctx, 0x01u);
    native_write(ctx, (uint16_t)(uint8_t)(0xb6u + ctx->x), ctx->y);
    native_write(ctx, (uint16_t)(uint8_t)(0x0fu + ctx->x), native_nz(ctx, (uint8_t)(native_read(ctx, (uint16_t)(uint8_t)(0x0fu + ctx->x)) + 1u)));
    ctx->x = native_nz(ctx, native_read(ctx, 0x07u));
    ctx->a = native_nz(ctx, 0x67u);
    native_write(ctx, (uint16_t)(0x06a1u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x68u);
    native_write(ctx, (uint16_t)(0x06a2u + ctx->x), ctx->a);
Laa2e: ;
    return;
    return;
}

void smbneo_native_fn_scenery_hidden_one_up_aa2f(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xaa2fu);
    ctx->a = native_nz(ctx, native_read(ctx, 0x075du));
    if ((ctx->status & NATIVE_Z)) {
        goto Laa6a;
    }
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x075du, ctx->a);
    /* static tail transfer */
    smbneo_native_fn_scenery_block_item_aa47(ctx);
    return;
Laa6a: ;
    return;
    return;
}

void smbneo_native_fn_scenery_block_q_aa3c(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xaa3cu);
    uint8_t saved_value_0 = 0u;
    goto Laa3c;
La974: ;
    ctx->x = native_nz(ctx, native_read(ctx, 0x07u));
    ctx->y = native_nz(ctx, 0x00u);
    ctx->a = native_nz(ctx, saved_value_0);
    /* static tail transfer */
    smbneo_native_fn_scenery_render_aaab(ctx);
    return;
Laa3c: ;
    smbneo_native_fn_scenery_block_id_aa64(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    /* static tail transfer */
    goto Laa5a;
Laa5a: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xbcf7u + ctx->y)));
    saved_value_0 = ctx->a;
    smbneo_native_fn_scenery_attr_aae9(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    /* static tail transfer */
    goto La974;
    return;
}

void smbneo_native_fn_scenery_block_coin_aa42(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xaa42u);
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x06bcu, ctx->a);
    smbneo_native_fn_scenery_block_item_aa47(ctx);
    return;
    return;
}

void smbneo_native_fn_scenery_block_item_aa47(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xaa47u);
    uint8_t saved_value_0 = 0u;
    goto Laa47;
La974: ;
    ctx->x = native_nz(ctx, native_read(ctx, 0x07u));
    ctx->y = native_nz(ctx, 0x00u);
    ctx->a = native_nz(ctx, saved_value_0);
    /* static tail transfer */
    smbneo_native_fn_scenery_render_aaab(ctx);
    return;
Laa47: ;
    smbneo_native_fn_scenery_block_id_aa64(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_write(ctx, 0x07u, ctx->y);
    ctx->a = native_nz(ctx, 0x00u);
    ctx->y = native_nz(ctx, native_read(ctx, 0x074eu));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    if ((ctx->status & NATIVE_Z)) {
        goto Laa56;
    }
    ctx->a = native_nz(ctx, 0x05u);
Laa56: ;
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, 0x07u));
    ctx->y = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xbcf7u + ctx->y)));
    saved_value_0 = ctx->a;
    smbneo_native_fn_scenery_attr_aae9(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    /* static tail transfer */
    goto La974;
    return;
}

void smbneo_native_fn_scenery_block_id_aa64(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xaa64u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x00u));
    ctx->status |= NATIVE_C;
    native_sbc(ctx, 0x00u);
    ctx->y = native_nz(ctx, ctx->a);
    return;
    return;
}

void smbneo_native_fn_scenery_pit_aa6f(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xaa6fu);
    smbneo_native_fn_scenery_len_aada(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    if (!(ctx->status & NATIVE_C)) {
        goto Laaa1;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x074eu));
    if (!(ctx->status & NATIVE_Z)) {
        goto Laaa1;
    }
    ctx->x = native_nz(ctx, native_read(ctx, 0x046au));
    smbneo_native_fn_scenery_pos_x_get_aafe(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->status |= NATIVE_C;
    native_sbc(ctx, 0x10u);
    native_write(ctx, (uint16_t)(0x0471u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0725u));
    native_sbc(ctx, 0x00u);
    native_write(ctx, (uint16_t)(0x046bu + ctx->x), ctx->a);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->a = native_nz(ctx, ctx->y);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    native_write(ctx, (uint16_t)(0x0477u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    native_cmp(ctx, ctx->x, 0x05u);
    if (!(ctx->status & NATIVE_C)) {
        goto Laa9e;
    }
    ctx->x = native_nz(ctx, 0x00u);
Laa9e: ;
    native_write(ctx, 0x046au, ctx->x);
Laaa1: ;
    ctx->x = native_nz(ctx, native_read(ctx, 0x074eu));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xaa6bu + ctx->x)));
    ctx->x = native_nz(ctx, 0x08u);
    ctx->y = native_nz(ctx, 0x0fu);
    smbneo_native_fn_scenery_render_aaab(ctx);
    return;
    return;
}

void smbneo_native_fn_scenery_render_aaab(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xaaabu);
Laaab: ;
    native_write(ctx, 0x0735u, ctx->y);
    ctx->y = native_nz(ctx, native_read(ctx, (uint16_t)(0x06a1u + ctx->x)));
    if ((ctx->status & NATIVE_Z)) {
        goto Laacb;
    }
    native_cmp(ctx, ctx->y, 0x17u);
    if ((ctx->status & NATIVE_Z)) {
        goto Laace;
    }
    native_cmp(ctx, ctx->y, 0x1au);
    if ((ctx->status & NATIVE_Z)) {
        goto Laace;
    }
    native_cmp(ctx, ctx->y, 0xc0u);
    if ((ctx->status & NATIVE_Z)) {
        goto Laacb;
    }
    native_cmp(ctx, ctx->y, 0xc0u);
    if ((ctx->status & NATIVE_C)) {
        goto Laace;
    }
    native_cmp(ctx, ctx->y, 0x54u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Laacb;
    }
    native_cmp(ctx, ctx->a, 0x50u);
    if ((ctx->status & NATIVE_Z)) {
        goto Laace;
    }
Laacb: ;
    native_write(ctx, (uint16_t)(0x06a1u + ctx->x), ctx->a);
Laace: ;
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    native_cmp(ctx, ctx->x, 0x0du);
    if ((ctx->status & NATIVE_C)) {
        goto Laad9;
    }
    ctx->y = native_nz(ctx, native_read(ctx, 0x0735u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    if (!(ctx->status & NATIVE_N)) {
        goto Laaab;
    }
Laad9: ;
    return;
    return;
}

void smbneo_native_fn_scenery_len_aada(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xaadau);
    smbneo_native_fn_scenery_attr_aae9(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_scenery_len_b_aadd(ctx);
    return;
    return;
}

void smbneo_native_fn_scenery_len_b_aadd(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xaaddu);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0730u + ctx->x)));
    ctx->status &= (uint8_t)~NATIVE_C;
    if (!(ctx->status & NATIVE_N)) {
        goto Laae8;
    }
    ctx->a = native_nz(ctx, ctx->y);
    native_write(ctx, (uint16_t)(0x0730u + ctx->x), ctx->a);
    ctx->status |= NATIVE_C;
Laae8: ;
    return;
    return;
}

void smbneo_native_fn_scenery_attr_aae9(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xaae9u);
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, 0x4016u, ctx->a);
    ctx->y = native_nz(ctx, native_read(ctx, (uint16_t)(0x072du + ctx->x)));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0xe7u) + ctx->y)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x0fu));
    native_write(ctx, 0x07u, ctx->a);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0xe7u) + ctx->y)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x0fu));
    ctx->y = native_nz(ctx, ctx->a);
    return;
    return;
}

void smbneo_native_fn_scenery_pos_x_get_aafe(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xaafeu);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0726u));
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    return;
    return;
}

void smbneo_native_fn_scenery_pos_y_get_ab06(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xab06u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x07u));
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x20u);
    return;
    return;
}

void smbneo_native_fn_scenery_get_buffer_ptr_ab14(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xab14u);
    uint8_t saved_value_0 = 0u;
    saved_value_0 = ctx->a;
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->y = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xab12u + ctx->y)));
    native_write(ctx, 0x07u, ctx->a);
    ctx->a = native_nz(ctx, saved_value_0);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x0fu));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, (uint16_t)(0xab10u + ctx->y)));
    native_write(ctx, 0x06u, ctx->a);
    return;
    return;
}

void smbneo_native_fn_course_descriptor_load_ab60(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xab60u);
    smbneo_native_fn_course_descriptor_get_ab70(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_write(ctx, 0x0750u, ctx->a);
    smbneo_native_fn_course_type_get_ab66(ctx);
    return;
    return;
}

void smbneo_native_fn_course_type_get_ab66(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xab66u);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x60u));
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_rol(ctx, ctx->a);
    ctx->a = native_rol(ctx, ctx->a);
    ctx->a = native_rol(ctx, ctx->a);
    native_write(ctx, 0x074eu, ctx->a);
    return;
    return;
}

void smbneo_native_fn_course_descriptor_get_ab70(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xab70u);
    ctx->y = native_nz(ctx, native_read(ctx, 0x075fu));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xac77u + ctx->y)));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, 0x0760u));
    ctx->y = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xac7fu + ctx->y)));
    return;
    return;
}

void smbneo_native_fn_course_load_ab7f(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xab7fu);
    uint8_t saved_value_0 = 0u;
    smbneo_native_fn_vs_setbank_low_820c(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, 0x06u);
    native_write(ctx, 0x4016u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x2002u));
    ctx->a = native_nz(ctx, native_read(ctx, 0x00u));
    native_write(ctx, 0x2006u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x00u));
    native_write(ctx, 0x2006u, ctx->a);
    ctx->y = native_nz(ctx, 0x00u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x2007u));
Lab99: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x2007u));
    native_write(ctx, (uint16_t)(0x6000u + ctx->y), ctx->a);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    native_cmp(ctx, ctx->y, 0x20u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lab99;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x0750u));
    smbneo_native_fn_course_type_get_ab66(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->y = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0750u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x1fu));
    native_write(ctx, 0x074fu, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xaca3u + ctx->y)));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, 0x074fu));
    ctx->a = native_asl(ctx, ctx->a);
    ctx->y = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x2002u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xaca8u + ctx->y)));
    native_write(ctx, 0x2006u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xaca7u + ctx->y)));
    native_write(ctx, 0x2006u, ctx->a);
    ctx->y = native_nz(ctx, 0x00u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x2007u));
Labd0: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x2007u));
    native_write(ctx, (uint16_t)(0x6100u + ctx->y), ctx->a);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    native_cmp(ctx, ctx->a, 0xffu);
    if (!(ctx->status & NATIVE_Z)) {
        goto Labd0;
    }
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0xe9u, ctx->a);
    ctx->a = native_nz(ctx, 0x61u);
    native_write(ctx, 0xeau, ctx->a);
    ctx->y = native_nz(ctx, native_read(ctx, 0x074eu));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xacf5u + ctx->y)));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, 0x074fu));
    ctx->a = native_asl(ctx, ctx->a);
    ctx->y = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x2002u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xacfau + ctx->y)));
    native_write(ctx, 0x2006u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xacf9u + ctx->y)));
    native_write(ctx, 0x2006u, ctx->a);
    ctx->y = native_nz(ctx, 0x00u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x2007u));
Lac03: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x2007u));
    native_write(ctx, (uint16_t)(0x6200u + ctx->y), ctx->a);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    native_cmp(ctx, ctx->a, 0xfdu);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lac03;
    }
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0xe7u, ctx->a);
    ctx->a = native_nz(ctx, 0x62u);
    native_write(ctx, 0xe8u, ctx->a);
    ctx->y = native_nz(ctx, 0x00u);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0xe7u) + ctx->y)));
    saved_value_0 = ctx->a;
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x07u));
    native_cmp(ctx, ctx->a, 0x04u);
    if (!(ctx->status & NATIVE_C)) {
        goto Lac26;
    }
    native_write(ctx, 0x0744u, ctx->a);
    ctx->a = native_nz(ctx, 0x00u);
Lac26: ;
    native_write(ctx, 0x0741u, ctx->a);
    ctx->a = native_nz(ctx, saved_value_0);
    saved_value_0 = ctx->a;
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x38u));
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    native_write(ctx, 0x0710u, ctx->a);
    ctx->a = native_nz(ctx, saved_value_0);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0xc0u));
    ctx->status &= (uint8_t)~NATIVE_C;
    ctx->a = native_rol(ctx, ctx->a);
    ctx->a = native_rol(ctx, ctx->a);
    ctx->a = native_rol(ctx, ctx->a);
    native_write(ctx, 0x0715u, ctx->a);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0xe7u) + ctx->y)));
    saved_value_0 = ctx->a;
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x0fu));
    native_write(ctx, 0x0727u, ctx->a);
    ctx->a = native_nz(ctx, saved_value_0);
    saved_value_0 = ctx->a;
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x30u));
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    native_write(ctx, 0x0742u, ctx->a);
    ctx->a = native_nz(ctx, saved_value_0);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0xc0u));
    ctx->status &= (uint8_t)~NATIVE_C;
    ctx->a = native_rol(ctx, ctx->a);
    ctx->a = native_rol(ctx, ctx->a);
    ctx->a = native_rol(ctx, ctx->a);
    native_cmp(ctx, ctx->a, 0x03u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lac61;
    }
    native_write(ctx, 0x0743u, ctx->a);
    ctx->a = native_nz(ctx, 0x00u);
Lac61: ;
    native_write(ctx, 0x0733u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0xe7u));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x02u);
    native_write(ctx, 0xe7u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0xe8u));
    native_adc(ctx, 0x00u);
    native_write(ctx, 0xe8u, ctx->a);
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, 0x4016u, ctx->a);
    return;
    return;
}

void smbneo_native_fn_sys_game_ad50(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xad50u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0772u));
    uint8_t table_selector_ad53 = ctx->a;
    ctx->a = native_asl(ctx, ctx->a);
    ctx->y = native_nz(ctx, ctx->a);
    native_write(ctx, 0x04u, 0x55u);
    native_write(ctx, 0x05u, 0xadu);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    switch (table_selector_ad53) {
    case 0: native_write(ctx, 0x06u, 0x60u);
             native_write(ctx, 0x07u, 0xadu);
             ctx->a = native_nz(ctx, 0xadu);
             smbneo_native_fn_vs_game_init_ad60(ctx); return;
    case 1: native_write(ctx, 0x06u, 0x5cu);
             native_write(ctx, 0x07u, 0x96u);
             ctx->a = native_nz(ctx, 0x96u);
             smbneo_native_fn_game_init_0_do_965c(ctx); return;
    case 2: native_write(ctx, 0x06u, 0x48u);
             native_write(ctx, 0x07u, 0x87u);
             ctx->a = native_nz(ctx, 0x87u);
             smbneo_native_fn_bg_proc_8748(ctx); return;
    case 3: native_write(ctx, 0x06u, 0x02u);
             native_write(ctx, 0x07u, 0x97u);
             ctx->a = native_nz(ctx, 0x97u);
             smbneo_native_fn_game_init_1_do_9702(ctx); return;
    case 4: native_write(ctx, 0x06u, 0x6eu);
             native_write(ctx, 0x07u, 0xadu);
             ctx->a = native_nz(ctx, 0xadu);
             smbneo_native_fn_game_proc_ad6e(ctx); return;
    default: ctx->unsupported = 1; return;
    }
    return;
}

void smbneo_native_fn_vs_game_init_ad60(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xad60u);
    native_write(ctx, 0x0772u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0772u) + 1u)));
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, 0x0774u, ctx->a);
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x0722u, ctx->a);
    return;
    return;
}

void smbneo_native_fn_game_proc_ad6e(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xad6eu);
#ifdef SMBNEO_NATIVE_FAST_PATHS
    vs_native_fast_game_proc(ctx);
    return;
#endif
    ctx->a = native_nz(ctx, native_read(ctx, 0x077au));
    if ((ctx->status & NATIVE_Z)) {
        goto Lad7c;
    }
    ctx->x = native_nz(ctx, native_read(ctx, 0x0753u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x06fcu + ctx->x)));
    /* static tail transfer */
    goto Lad82;
Lad7c: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x06fcu));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | native_read(ctx, 0x06fdu)));
Lad82: ;
    native_write(ctx, 0x06fcu, ctx->a);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x01u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lad91;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x06fcu));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0xfdu));
    native_write(ctx, 0x06fcu, ctx->a);
Lad91: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x06fcu));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x04u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lada0;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x06fcu));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0xf7u));
    native_write(ctx, 0x06fcu, ctx->a);
Lada0: ;
    smbneo_native_fn_player_proc_aef7(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, 0x0772u));
    native_cmp(ctx, ctx->a, 0x03u);
    if ((ctx->status & NATIVE_C)) {
        goto Ladab;
    }
    return;
Ladab: ;
    smbneo_native_fn_proj_proc_b4d1(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->x = native_nz(ctx, 0x00u);
Ladb0: ;
    native_write(ctx, 0x08u, ctx->x);
    smbneo_native_fn_actor_proc_bf60(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_bg_score_disp_86a4(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    native_cmp(ctx, ctx->x, 0x06u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Ladb0;
    }
    smbneo_native_fn_pos_bits_get_player_f0e5(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_pos_calc_x_rel_player_f08f(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_render_player_ee46(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_block_flatten_bde3(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->x = native_nz(ctx, 0x01u);
    native_write(ctx, 0x08u, ctx->x);
    smbneo_native_fn_block_proc_bd7f(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x - 1u));
    native_write(ctx, 0x08u, ctx->x);
    smbneo_native_fn_block_proc_bd7f(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_misc_proc_ba8a(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_cannon_proc_b8b0(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_whirlpool_proc_b66c(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_pole_proc_b709(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_stats_time_proc_b5fc(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_meta_color_rotate_8be2(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, 0xb5u));
    native_cmp(ctx, ctx->a, 0x02u);
    if (!(ctx->status & NATIVE_N)) {
        goto Ladff;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x079fu));
    if ((ctx->status & NATIVE_Z)) {
        goto Lae11;
    }
    native_cmp(ctx, ctx->a, 0x04u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Ladff;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x077fu));
    if (!(ctx->status & NATIVE_Z)) {
        goto Ladff;
    }
    smbneo_native_fn_game_init_area_music_977e(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Ladff: ;
    ctx->y = native_nz(ctx, native_read(ctx, 0x079fu));
    ctx->a = native_nz(ctx, native_read(ctx, 0x09u));
    native_cmp(ctx, ctx->y, 0x08u);
    if ((ctx->status & NATIVE_C)) {
        goto Lae0a;
    }
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
Lae0a: ;
    ctx->a = native_lsr(ctx, ctx->a);
    smbneo_native_fn_player_proc_firefl_do_b135(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    /* static tail transfer */
    goto Lae14;
Lae11: ;
    smbneo_native_fn_player_proc_firefl_do_2_b147(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Lae14: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x0au));
    native_write(ctx, 0x0du, ctx->a);
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x0cu, ctx->a);
    smbneo_native_fn_game_proc_scenery_scroll_ae1c(ctx);
    return;
    return;
}

void smbneo_native_fn_game_proc_scenery_scroll_ae1c(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xae1cu);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0773u));
    native_cmp(ctx, ctx->a, 0x06u);
    if ((ctx->status & NATIVE_Z)) {
        goto Lae3f;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x071fu));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lae3c;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x073du));
    native_cmp(ctx, ctx->a, 0x20u);
    if ((ctx->status & NATIVE_N)) {
        goto Lae3f;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x073du));
    native_sbc(ctx, 0x20u);
    native_write(ctx, 0x073du, ctx->a);
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x0340u, ctx->a);
Lae3c: ;
    smbneo_native_fn_scenery_proc_a18b(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Lae3f: ;
    return;
    return;
}

void smbneo_native_fn_game_scroll_ae40(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xae40u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x06ffu));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, 0x03a1u));
    native_write(ctx, 0x06ffu, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0723u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Laea8;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x0755u));
    native_cmp(ctx, ctx->a, 0x50u);
    if (!(ctx->status & NATIVE_C)) {
        goto Laea8;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x0785u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Laea8;
    }
    ctx->y = native_nz(ctx, native_read(ctx, 0x06ffu));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    if ((ctx->status & NATIVE_N)) {
        goto Laea8;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    native_cmp(ctx, ctx->y, 0x02u);
    if (!(ctx->status & NATIVE_C)) {
        goto Lae67;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
Lae67: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x0755u));
    native_cmp(ctx, ctx->a, 0x70u);
    if (!(ctx->status & NATIVE_C)) {
        smbneo_native_fn_game_scroll_calc_ae71(ctx);
        return;
    }
    ctx->y = native_nz(ctx, native_read(ctx, 0x06ffu));
    smbneo_native_fn_game_scroll_calc_ae71(ctx);
    return;
Laea8: ;
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x0775u, ctx->a);
    ctx->x = native_nz(ctx, 0x00u);
    smbneo_native_fn_pos_bits_get_do_x_f15b(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_write(ctx, 0x00u, ctx->a);
    ctx->y = native_nz(ctx, 0x00u);
    ctx->a = native_asl(ctx, ctx->a);
    if ((ctx->status & NATIVE_C)) {
        goto Laec0;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->a = native_nz(ctx, native_read(ctx, 0x00u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x20u));
    if ((ctx->status & NATIVE_Z)) {
        goto Laedb;
    }
Laec0: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x071cu + ctx->y)));
    ctx->status |= NATIVE_C;
    native_sbc(ctx, native_read(ctx, (uint16_t)(0xaee1u + ctx->y)));
    native_write(ctx, 0x86u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x071au + ctx->y)));
    native_sbc(ctx, 0x00u);
    native_write(ctx, 0x6du, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0cu));
    native_cmp(ctx, ctx->a, native_read(ctx, (uint16_t)(0xaee3u + ctx->y)));
    if ((ctx->status & NATIVE_Z)) {
        goto Laedb;
    }
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x57u, ctx->a);
Laedb: ;
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x03a1u, ctx->a);
    return;
    return;
}

void smbneo_native_fn_game_scroll_calc_ae71(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xae71u);
    ctx->a = native_nz(ctx, ctx->y);
    native_write(ctx, 0x0775u, ctx->a);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, 0x073du));
    native_write(ctx, 0x073du, ctx->a);
    ctx->a = native_nz(ctx, ctx->y);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, 0x071cu));
    native_write(ctx, 0x071cu, ctx->a);
    native_write(ctx, 0x073fu, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x071au));
    native_adc(ctx, 0x00u);
    native_write(ctx, 0x071au, ctx->a);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x01u));
    native_write(ctx, 0x00u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0778u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0xfeu));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | native_read(ctx, 0x00u)));
    native_write(ctx, 0x0778u, ctx->a);
    smbneo_native_fn_game_screen_pos_x_calc_aee5(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, 0x08u);
    native_write(ctx, 0x0795u, ctx->a);
    /* static tail transfer */
    goto Laead;
Laead: ;
    ctx->x = native_nz(ctx, 0x00u);
    smbneo_native_fn_pos_bits_get_do_x_f15b(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_write(ctx, 0x00u, ctx->a);
    ctx->y = native_nz(ctx, 0x00u);
    ctx->a = native_asl(ctx, ctx->a);
    if ((ctx->status & NATIVE_C)) {
        goto Laec0;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->a = native_nz(ctx, native_read(ctx, 0x00u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x20u));
    if ((ctx->status & NATIVE_Z)) {
        goto Laedb;
    }
Laec0: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x071cu + ctx->y)));
    ctx->status |= NATIVE_C;
    native_sbc(ctx, native_read(ctx, (uint16_t)(0xaee1u + ctx->y)));
    native_write(ctx, 0x86u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x071au + ctx->y)));
    native_sbc(ctx, 0x00u);
    native_write(ctx, 0x6du, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0cu));
    native_cmp(ctx, ctx->a, native_read(ctx, (uint16_t)(0xaee3u + ctx->y)));
    if ((ctx->status & NATIVE_Z)) {
        goto Laedb;
    }
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x57u, ctx->a);
Laedb: ;
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x03a1u, ctx->a);
    return;
    return;
}

void smbneo_native_fn_game_screen_pos_x_calc_aee5(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xaee5u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x071cu));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0xffu);
    native_write(ctx, 0x071du, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x071au));
    native_adc(ctx, 0x00u);
    native_write(ctx, 0x071bu, ctx->a);
    return;
    return;
}

void smbneo_native_fn_player_proc_aef7(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xaef7u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0eu));
    uint8_t table_selector_aef9 = ctx->a;
    ctx->a = native_asl(ctx, ctx->a);
    ctx->y = native_nz(ctx, ctx->a);
    native_write(ctx, 0x04u, 0xfbu);
    native_write(ctx, 0x05u, 0xaeu);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    switch (table_selector_aef9) {
    case 0: native_write(ctx, 0x06u, 0xcbu);
             native_write(ctx, 0x07u, 0x97u);
             ctx->a = native_nz(ctx, 0x97u);
             smbneo_native_fn_player_proc_init_97cb(ctx); return;
    case 1: native_write(ctx, 0x06u, 0x74u);
             native_write(ctx, 0x07u, 0xb0u);
             ctx->a = native_nz(ctx, 0xb0u);
             smbneo_native_fn_player_proc_vine_b074(ctx); return;
    case 2: native_write(ctx, 0x06u, 0xb3u);
             native_write(ctx, 0x07u, 0xb0u);
             ctx->a = native_nz(ctx, 0xb0u);
             smbneo_native_fn_player_proc_pipe_h_b0b3(ctx); return;
    case 3: native_write(ctx, 0x06u, 0x92u);
             native_write(ctx, 0x07u, 0xb0u);
             ctx->a = native_nz(ctx, 0xb0u);
             smbneo_native_fn_player_proc_pipe_v_b092(ctx); return;
    case 4: native_write(ctx, 0x06u, 0x51u);
             native_write(ctx, 0x07u, 0xb1u);
             ctx->a = native_nz(ctx, 0xb1u);
             smbneo_native_fn_player_proc_pole_b151(ctx); return;
    case 5: native_write(ctx, 0x06u, 0x77u);
             native_write(ctx, 0x07u, 0xb1u);
             ctx->a = native_nz(ctx, 0xb1u);
             smbneo_native_fn_player_proc_victory_b177(ctx); return;
    case 6: native_write(ctx, 0x06u, 0x7fu);
             native_write(ctx, 0x07u, 0x98u);
             ctx->a = native_nz(ctx, 0x98u);
             smbneo_native_fn_player_proc_lifedown_987f(ctx); return;
    case 7: native_write(ctx, 0x06u, 0x16u);
             native_write(ctx, 0x07u, 0xafu);
             ctx->a = native_nz(ctx, 0xafu);
             smbneo_native_fn_player_proc_enter_af16(ctx); return;
    case 8: native_write(ctx, 0x06u, 0x96u);
             native_write(ctx, 0x07u, 0xafu);
             ctx->a = native_nz(ctx, 0xafu);
             smbneo_native_fn_player_proc_ctrl_af96(ctx); return;
    case 9: native_write(ctx, 0x06u, 0xe0u);
             native_write(ctx, 0x07u, 0xb0u);
             ctx->a = native_nz(ctx, 0xb0u);
             smbneo_native_fn_player_proc_size_chg_b0e0(ctx); return;
    case 10: native_write(ctx, 0x06u, 0xf2u);
             native_write(ctx, 0x07u, 0xb0u);
             ctx->a = native_nz(ctx, 0xb0u);
             smbneo_native_fn_player_proc_hurt_b0f2(ctx); return;
    case 11: native_write(ctx, 0x06u, 0x16u);
             native_write(ctx, 0x07u, 0xb1u);
             ctx->a = native_nz(ctx, 0xb1u);
             smbneo_native_fn_player_proc_death_b116(ctx); return;
    case 12: native_write(ctx, 0x06u, 0x2au);
             native_write(ctx, 0x07u, 0xb1u);
             ctx->a = native_nz(ctx, 0xb1u);
             smbneo_native_fn_player_proc_firefl_b12a(ctx); return;
    default: ctx->unsupported = 1; return;
    }
    return;
}

void smbneo_native_fn_player_proc_enter_af16(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xaf16u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0752u));
    native_cmp(ctx, ctx->a, 0x02u);
    if ((ctx->status & NATIVE_Z)) {
        goto Laf48;
    }
    ctx->a = native_nz(ctx, 0x00u);
    ctx->y = native_nz(ctx, native_read(ctx, 0xceu));
    native_cmp(ctx, ctx->y, 0x30u);
    if (!(ctx->status & NATIVE_C)) {
        smbneo_native_fn_player_proc_auto_af93(ctx);
        return;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x0710u));
    native_cmp(ctx, ctx->a, 0x06u);
    if ((ctx->status & NATIVE_Z)) {
        goto Laf30;
    }
    native_cmp(ctx, ctx->a, 0x07u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Laf80;
    }
Laf30: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x03c4u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Laf3a;
    }
    ctx->a = native_nz(ctx, 0x01u);
    /* static tail transfer */
    smbneo_native_fn_player_proc_auto_af93(ctx);
    return;
Laf3a: ;
    smbneo_native_fn_player_proc_pipe_h_enter_b0cc(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_write(ctx, 0x06deu, native_nz(ctx, (uint8_t)(native_read(ctx, 0x06deu) - 1u)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Laf92;
    }
    native_write(ctx, 0x0769u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0769u) + 1u)));
    /* static tail transfer */
    goto Lb1c2;
Laf48: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x0758u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Laf59;
    }
    ctx->a = native_nz(ctx, 0xffu);
    smbneo_native_fn_game_pos_y_lo_player_add_b0ad(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, 0xceu));
    native_cmp(ctx, ctx->a, 0x91u);
    if (!(ctx->status & NATIVE_C)) {
        goto Laf80;
    }
    return;
Laf59: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x0399u));
    native_cmp(ctx, ctx->a, 0x60u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Laf92;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0xceu));
    native_cmp(ctx, ctx->a, 0x99u);
    ctx->y = native_nz(ctx, 0x00u);
    ctx->a = native_nz(ctx, 0x01u);
    if (!(ctx->status & NATIVE_C)) {
        goto Laf74;
    }
    ctx->a = native_nz(ctx, 0x03u);
    native_write(ctx, 0x1du, ctx->a);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->a = native_nz(ctx, 0x08u);
    native_write(ctx, 0x05b4u, ctx->a);
Laf74: ;
    native_write(ctx, 0x0716u, ctx->y);
    smbneo_native_fn_player_proc_auto_af93(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, 0x86u));
    native_cmp(ctx, ctx->a, 0x48u);
    if (!(ctx->status & NATIVE_C)) {
        goto Laf92;
    }
Laf80: ;
    ctx->a = native_nz(ctx, 0x08u);
    native_write(ctx, 0x0eu, ctx->a);
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, 0x33u, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    native_write(ctx, 0x0752u, ctx->a);
    native_write(ctx, 0x0716u, ctx->a);
    native_write(ctx, 0x0758u, ctx->a);
Laf92: ;
    return;
Lb1c2: ;
    native_write(ctx, 0x0760u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0760u) + 1u)));
    smbneo_native_fn_course_descriptor_load_ab60(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_write(ctx, 0x0757u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0757u) + 1u)));
    smbneo_native_fn_player_proc_pipe_h_do_2_b0c0(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_write(ctx, 0x075bu, ctx->a);
    ctx->a = native_nz(ctx, 0x80u);
    native_write(ctx, 0xfcu, ctx->a);
    return;
    return;
}

void smbneo_native_fn_player_proc_auto_af93(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xaf93u);
    native_write(ctx, 0x06fcu, ctx->a);
    smbneo_native_fn_player_proc_ctrl_af96(ctx);
    return;
    return;
}

void smbneo_native_fn_player_proc_ctrl_af96(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xaf96u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0eu));
    native_cmp(ctx, ctx->a, 0x0bu);
    if ((ctx->status & NATIVE_Z)) {
        goto Lafd8;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x074eu));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lafb1;
    }
    ctx->y = native_nz(ctx, native_read(ctx, 0xb5u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lafac;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0xceu));
    native_cmp(ctx, ctx->a, 0xd0u);
    if (!(ctx->status & NATIVE_C)) {
        goto Lafb1;
    }
Lafac: ;
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x06fcu, ctx->a);
Lafb1: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x06fcu));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0xc0u));
    native_write(ctx, 0x0au, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x06fcu));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x03u));
    native_write(ctx, 0x0cu, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x06fcu));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x0cu));
    native_write(ctx, 0x0bu, ctx->a);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x04u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lafd8;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x1du));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lafd8;
    }
    ctx->y = native_nz(ctx, native_read(ctx, 0x0cu));
    if ((ctx->status & NATIVE_Z)) {
        goto Lafd8;
    }
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x0cu, ctx->a);
    native_write(ctx, 0x0bu, ctx->a);
Lafd8: ;
    smbneo_native_fn_player_proc_player_move_b1d6(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->y = native_nz(ctx, 0x01u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0754u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lafeb;
    }
    ctx->y = native_nz(ctx, 0x00u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0714u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lafeb;
    }
    ctx->y = native_nz(ctx, 0x02u);
Lafeb: ;
    native_write(ctx, 0x0499u, ctx->y);
    ctx->a = native_nz(ctx, 0x01u);
    ctx->y = native_nz(ctx, native_read(ctx, 0x57u));
    if ((ctx->status & NATIVE_Z)) {
        goto Laff9;
    }
    if (!(ctx->status & NATIVE_N)) {
        goto Laff7;
    }
    ctx->a = native_asl(ctx, ctx->a);
Laff7: ;
    native_write(ctx, 0x45u, ctx->a);
Laff9: ;
    smbneo_native_fn_game_scroll_ae40(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_pos_bits_get_player_f0e5(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_pos_calc_x_rel_player_f08f(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->x = native_nz(ctx, 0x00u);
    smbneo_native_fn_col_box_proc_e1f2(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_col_bg_proc_dbbd(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, 0xceu));
    native_cmp(ctx, ctx->a, 0x40u);
    if (!(ctx->status & NATIVE_C)) {
        goto Lb026;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x0eu));
    native_cmp(ctx, ctx->a, 0x05u);
    if ((ctx->status & NATIVE_Z)) {
        goto Lb026;
    }
    native_cmp(ctx, ctx->a, 0x07u);
    if ((ctx->status & NATIVE_Z)) {
        goto Lb026;
    }
    native_cmp(ctx, ctx->a, 0x04u);
    if (!(ctx->status & NATIVE_C)) {
        goto Lb026;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x03c4u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0xdfu));
    native_write(ctx, 0x03c4u, ctx->a);
Lb026: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0xb5u));
    native_cmp(ctx, ctx->a, 0x02u);
    if ((ctx->status & NATIVE_N)) {
        goto Lb067;
    }
    ctx->x = native_nz(ctx, 0x01u);
    native_write(ctx, 0x0723u, ctx->x);
    ctx->y = native_nz(ctx, 0x04u);
    native_write(ctx, 0x07u, ctx->y);
    ctx->x = native_nz(ctx, 0x00u);
    ctx->y = native_nz(ctx, native_read(ctx, 0x0759u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb041;
    }
    ctx->y = native_nz(ctx, native_read(ctx, 0x0743u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb057;
    }
Lb041: ;
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    ctx->y = native_nz(ctx, native_read(ctx, 0x0eu));
    native_cmp(ctx, ctx->y, 0x0bu);
    if ((ctx->status & NATIVE_Z)) {
        goto Lb057;
    }
    ctx->y = native_nz(ctx, native_read(ctx, 0x0712u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb053;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    native_write(ctx, 0xfcu, ctx->y);
    native_write(ctx, 0x0712u, ctx->y);
Lb053: ;
    ctx->y = native_nz(ctx, 0x06u);
    native_write(ctx, 0x07u, ctx->y);
Lb057: ;
    native_cmp(ctx, ctx->a, native_read(ctx, 0x07u));
    if ((ctx->status & NATIVE_N)) {
        goto Lb067;
    }
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x - 1u));
    if ((ctx->status & NATIVE_N)) {
        goto Lb068;
    }
    ctx->y = native_nz(ctx, native_read(ctx, 0x07b1u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb067;
    }
    ctx->a = native_nz(ctx, 0x06u);
    native_write(ctx, 0x0eu, ctx->a);
Lb067: ;
    return;
Lb068: ;
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x0758u, ctx->a);
    smbneo_native_fn_player_proc_vine_b_b08a(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_write(ctx, 0x0752u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0752u) + 1u)));
    return;
    return;
}

void smbneo_native_fn_player_proc_vine_b074(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xb074u);
    ctx->a = native_nz(ctx, native_read(ctx, 0xb5u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb07e;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0xceu));
    native_cmp(ctx, ctx->a, 0xe4u);
    if (!(ctx->status & NATIVE_C)) {
        smbneo_native_fn_player_proc_vine_b_b08a(ctx);
        return;
    }
Lb07e: ;
    ctx->a = native_nz(ctx, 0x08u);
    native_write(ctx, 0x0758u, ctx->a);
    ctx->y = native_nz(ctx, 0x03u);
    native_write(ctx, 0x1du, ctx->y);
    /* static tail transfer */
    smbneo_native_fn_player_proc_auto_af93(ctx);
    return;
    return;
}

void smbneo_native_fn_player_proc_vine_b_b08a(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xb08au);
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, 0x0752u, ctx->a);
    /* static tail transfer */
    smbneo_native_fn_player_proc_pipe_h_do_2_b0c0(ctx);
    return;
    return;
}

void smbneo_native_fn_player_proc_pipe_v_b092(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xb092u);
    ctx->a = native_nz(ctx, 0x01u);
    smbneo_native_fn_game_pos_y_lo_player_add_b0ad(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_game_scroll_ae40(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->y = native_nz(ctx, 0x00u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x06d6u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb0b8;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->a = native_nz(ctx, native_read(ctx, 0x074eu));
    native_cmp(ctx, ctx->a, 0x03u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb0b8;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    /* static tail transfer */
    goto Lb0b8;
Lb0b8: ;
    native_write(ctx, 0x06deu, native_nz(ctx, (uint8_t)(native_read(ctx, 0x06deu) - 1u)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb0cb;
    }
    native_write(ctx, 0x0752u, ctx->y);
    smbneo_native_fn_player_proc_pipe_h_do_2_b0c0(ctx);
    return;
Lb0cb: ;
    return;
    return;
}

void smbneo_native_fn_game_pos_y_lo_player_add_b0ad(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xb0adu);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, 0xceu));
    native_write(ctx, 0xceu, ctx->a);
    return;
    return;
}

void smbneo_native_fn_player_proc_pipe_h_b0b3(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xb0b3u);
    smbneo_native_fn_player_proc_pipe_h_enter_b0cc(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->y = native_nz(ctx, 0x02u);
    native_write(ctx, 0x06deu, native_nz(ctx, (uint8_t)(native_read(ctx, 0x06deu) - 1u)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb0cb;
    }
    native_write(ctx, 0x0752u, ctx->y);
    smbneo_native_fn_player_proc_pipe_h_do_2_b0c0(ctx);
    return;
Lb0cb: ;
    return;
    return;
}

void smbneo_native_fn_player_proc_pipe_h_do_2_b0c0(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xb0c0u);
    native_write(ctx, 0x0774u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0774u) + 1u)));
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x0772u, ctx->a);
    native_write(ctx, 0x0722u, ctx->a);
    return;
    return;
}

void smbneo_native_fn_player_proc_pipe_h_enter_b0cc(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xb0ccu);
    ctx->a = native_nz(ctx, 0x08u);
    native_write(ctx, 0x57u, ctx->a);
    ctx->y = native_nz(ctx, 0x01u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x86u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x0fu));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb0db;
    }
    native_write(ctx, 0x57u, ctx->a);
    ctx->y = native_nz(ctx, ctx->a);
Lb0db: ;
    ctx->a = native_nz(ctx, ctx->y);
    smbneo_native_fn_player_proc_auto_af93(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    return;
    return;
}

void smbneo_native_fn_player_proc_size_chg_b0e0(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xb0e0u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0747u));
    native_cmp(ctx, ctx->a, 0xf8u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb0ea;
    }
    /* static tail transfer */
    goto Lb102;
Lb0ea: ;
    native_cmp(ctx, ctx->a, 0xc4u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb0f1;
    }
    smbneo_native_fn_player_proc_player_done_b120(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Lb0f1: ;
    return;
Lb102: ;
    ctx->y = native_nz(ctx, native_read(ctx, 0x070bu));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb115;
    }
    native_write(ctx, 0x070du, ctx->y);
    native_write(ctx, 0x070bu, native_nz(ctx, (uint8_t)(native_read(ctx, 0x070bu) + 1u)));
    ctx->a = native_nz(ctx, native_read(ctx, 0x0754u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a ^ 0x01u));
    native_write(ctx, 0x0754u, ctx->a);
Lb115: ;
    return;
    return;
}

void smbneo_native_fn_player_proc_hurt_b0f2(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xb0f2u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0747u));
    native_cmp(ctx, ctx->a, 0xf0u);
    if ((ctx->status & NATIVE_C)) {
        goto Lb100;
    }
    native_cmp(ctx, ctx->a, 0xc8u);
    if ((ctx->status & NATIVE_Z)) {
        smbneo_native_fn_player_proc_player_done_b120(ctx);
        return;
    }
    /* static tail transfer */
    smbneo_native_fn_player_proc_ctrl_af96(ctx);
    return;
Lb100: ;
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb115;
    }
    ctx->y = native_nz(ctx, native_read(ctx, 0x070bu));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb115;
    }
    native_write(ctx, 0x070du, ctx->y);
    native_write(ctx, 0x070bu, native_nz(ctx, (uint8_t)(native_read(ctx, 0x070bu) + 1u)));
    ctx->a = native_nz(ctx, native_read(ctx, 0x0754u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a ^ 0x01u));
    native_write(ctx, 0x0754u, ctx->a);
Lb115: ;
    return;
    return;
}

void smbneo_native_fn_player_proc_death_b116(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xb116u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0747u));
    native_cmp(ctx, ctx->a, 0xf0u);
    if ((ctx->status & NATIVE_C)) {
        goto Lb150;
    }
    /* static tail transfer */
    smbneo_native_fn_player_proc_ctrl_af96(ctx);
    return;
Lb150: ;
    return;
    return;
}

void smbneo_native_fn_player_proc_player_done_b120(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xb120u);
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x0747u, ctx->a);
    ctx->a = native_nz(ctx, 0x08u);
    native_write(ctx, 0x0eu, ctx->a);
    return;
    return;
}

void smbneo_native_fn_player_proc_firefl_b12a(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xb12au);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0747u));
    native_cmp(ctx, ctx->a, 0xc0u);
    if ((ctx->status & NATIVE_Z)) {
        goto Lb144;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x09u));
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    smbneo_native_fn_player_proc_firefl_do_b135(ctx);
    return;
Lb144: ;
    smbneo_native_fn_player_proc_player_done_b120(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_player_proc_firefl_do_2_b147(ctx);
    return;
    return;
}

void smbneo_native_fn_player_proc_firefl_do_b135(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xb135u);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x03u));
    native_write(ctx, 0x00u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x03c4u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0xfcu));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | native_read(ctx, 0x00u)));
    native_write(ctx, 0x03c4u, ctx->a);
    return;
    return;
}

void smbneo_native_fn_player_proc_firefl_do_2_b147(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xb147u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x03c4u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0xfcu));
    native_write(ctx, 0x03c4u, ctx->a);
    return;
    return;
}

void smbneo_native_fn_player_proc_pole_b151(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xb151u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x1bu));
    native_cmp(ctx, ctx->a, 0x30u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb16c;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x0713u));
    native_write(ctx, 0xffu, ctx->a);
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x0713u, ctx->a);
    ctx->y = native_nz(ctx, native_read(ctx, 0xceu));
    native_cmp(ctx, ctx->y, 0x9eu);
    if ((ctx->status & NATIVE_C)) {
        goto Lb169;
    }
    ctx->a = native_nz(ctx, 0x04u);
Lb169: ;
    /* static tail transfer */
    smbneo_native_fn_player_proc_auto_af93(ctx);
    return;
Lb16c: ;
    native_write(ctx, 0x0eu, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0eu) + 1u)));
    return;
    return;
}

void smbneo_native_fn_player_proc_victory_b177(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xb177u);
    ctx->a = native_nz(ctx, 0x01u);
    smbneo_native_fn_player_proc_auto_af93(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, 0xceu));
    native_cmp(ctx, ctx->a, 0xaeu);
    if (!(ctx->status & NATIVE_C)) {
        goto Lb190;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x0723u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lb190;
    }
    ctx->a = native_nz(ctx, 0x20u);
    native_write(ctx, 0xfcu, ctx->a);
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x0723u, ctx->a);
Lb190: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x0490u));
    ctx->a = native_lsr(ctx, ctx->a);
    if ((ctx->status & NATIVE_C)) {
        goto Lb1a3;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x0746u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb19e;
    }
    native_write(ctx, 0x0746u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0746u) + 1u)));
Lb19e: ;
    ctx->a = native_nz(ctx, 0x20u);
    native_write(ctx, 0x03c4u, ctx->a);
Lb1a3: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x0746u));
    native_cmp(ctx, ctx->a, 0x05u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb1d5;
    }
    native_write(ctx, 0x075cu, native_nz(ctx, (uint8_t)(native_read(ctx, 0x075cu) + 1u)));
    ctx->a = native_nz(ctx, native_read(ctx, 0x075cu));
    native_cmp(ctx, ctx->a, 0x03u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb1c2;
    }
    ctx->y = native_nz(ctx, native_read(ctx, 0x075fu));
    ctx->a = native_nz(ctx, native_read(ctx, 0x0748u));
    native_cmp(ctx, ctx->a, native_read(ctx, (uint16_t)(0xb16fu + ctx->y)));
    if (!(ctx->status & NATIVE_C)) {
        goto Lb1c2;
    }
    native_write(ctx, 0x075du, native_nz(ctx, (uint8_t)(native_read(ctx, 0x075du) + 1u)));
Lb1c2: ;
    native_write(ctx, 0x0760u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0760u) + 1u)));
    smbneo_native_fn_course_descriptor_load_ab60(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_write(ctx, 0x0757u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0757u) + 1u)));
    smbneo_native_fn_player_proc_pipe_h_do_2_b0c0(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_write(ctx, 0x075bu, ctx->a);
    ctx->a = native_nz(ctx, 0x80u);
    native_write(ctx, 0xfcu, ctx->a);
Lb1d5: ;
    return;
    return;
}

void smbneo_native_fn_player_proc_player_move_b1d6(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xb1d6u);
    ctx->a = native_nz(ctx, 0x00u);
    ctx->y = native_nz(ctx, native_read(ctx, 0x0754u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb1e5;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x1du));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb1e8;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x0bu));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x04u));
Lb1e5: ;
    native_write(ctx, 0x0714u, ctx->a);
Lb1e8: ;
    smbneo_native_fn_player_proc_player_physics_b2fd(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, 0x070bu));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb206;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x1du));
    native_cmp(ctx, ctx->a, 0x03u);
    if ((ctx->status & NATIVE_Z)) {
        goto Lb1fb;
    }
    ctx->y = native_nz(ctx, 0x18u);
    native_write(ctx, 0x0789u, ctx->y);
Lb1fb: ;
    uint8_t table_selector_b1fb = ctx->a;
    ctx->a = native_asl(ctx, ctx->a);
    ctx->y = native_nz(ctx, ctx->a);
    native_write(ctx, 0x04u, 0xfdu);
    native_write(ctx, 0x05u, 0xb1u);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    switch (table_selector_b1fb) {
    case 0: native_write(ctx, 0x06u, 0x07u);
             native_write(ctx, 0x07u, 0xb2u);
             ctx->a = native_nz(ctx, 0xb2u);
             smbneo_native_fn_player_proc_player_ground_b207(ctx); return;
    case 1: native_write(ctx, 0x06u, 0x23u);
             native_write(ctx, 0x07u, 0xb2u);
             ctx->a = native_nz(ctx, 0xb2u);
             smbneo_native_fn_player_proc_player_jump_swim_b223(ctx); return;
    case 2: native_write(ctx, 0x06u, 0x1au);
             native_write(ctx, 0x07u, 0xb2u);
             ctx->a = native_nz(ctx, 0xb2u);
             smbneo_native_fn_player_proc_player_fall_b21a(ctx); return;
    case 3: native_write(ctx, 0x06u, 0x7cu);
             native_write(ctx, 0x07u, 0xb2u);
             ctx->a = native_nz(ctx, 0xb2u);
             smbneo_native_fn_player_proc_player_climb_b27c(ctx); return;
    default: ctx->unsupported = 1; return;
    }
Lb206: ;
    return;
    return;
}

void smbneo_native_fn_player_proc_player_ground_b207(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xb207u);
    smbneo_native_fn_player_proc_player_ani_speed_b43c(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, 0x0cu));
    if ((ctx->status & NATIVE_Z)) {
        goto Lb210;
    }
    native_write(ctx, 0x33u, ctx->a);
Lb210: ;
    smbneo_native_fn_player_proc_player_friction_b479(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_motion_x_player_be18(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_write(ctx, 0x06ffu, ctx->a);
    return;
    return;
}

void smbneo_native_fn_player_proc_player_fall_b21a(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xb21au);
    ctx->a = native_nz(ctx, native_read(ctx, 0x070au));
    native_write(ctx, 0x0709u, ctx->a);
    /* static tail transfer */
    goto Lb259;
Lb259: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x0cu));
    if ((ctx->status & NATIVE_Z)) {
        goto Lb260;
    }
    smbneo_native_fn_player_proc_player_friction_b479(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Lb260: ;
    smbneo_native_fn_motion_x_player_be18(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_write(ctx, 0x06ffu, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0eu));
    native_cmp(ctx, ctx->a, 0x0bu);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb271;
    }
    ctx->a = native_nz(ctx, 0x28u);
    native_write(ctx, 0x0709u, ctx->a);
Lb271: ;
    /* static tail transfer */
    goto Lbe5c;
Lbe5b: ;
    return;
Lbe5c: ;
    ctx->x = native_nz(ctx, 0x00u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0747u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lbe68;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x070eu));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lbe5b;
    }
Lbe68: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x0709u));
    native_write(ctx, 0x00u, ctx->a);
    ctx->a = native_nz(ctx, 0x04u);
    /* static tail transfer */
    smbneo_native_fn_motion_gravity_do_bebe(ctx);
    return;
    return;
}

void smbneo_native_fn_player_proc_player_jump_swim_b223(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xb223u);
    ctx->y = native_nz(ctx, native_read(ctx, 0x9fu));
    if (!(ctx->status & NATIVE_N)) {
        goto Lb23a;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x0au));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x80u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & native_read(ctx, 0x0du)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb240;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x0708u));
    ctx->status |= NATIVE_C;
    native_sbc(ctx, native_read(ctx, 0xceu));
    native_cmp(ctx, ctx->a, native_read(ctx, 0x0706u));
    if (!(ctx->status & NATIVE_C)) {
        goto Lb240;
    }
Lb23a: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x070au));
    native_write(ctx, 0x0709u, ctx->a);
Lb240: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x0704u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lb259;
    }
    smbneo_native_fn_player_proc_player_ani_speed_b43c(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, 0xceu));
    native_cmp(ctx, ctx->a, 0x14u);
    if ((ctx->status & NATIVE_C)) {
        goto Lb253;
    }
    ctx->a = native_nz(ctx, 0x18u);
    native_write(ctx, 0x0709u, ctx->a);
Lb253: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x0cu));
    if ((ctx->status & NATIVE_Z)) {
        goto Lb259;
    }
    native_write(ctx, 0x33u, ctx->a);
Lb259: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x0cu));
    if ((ctx->status & NATIVE_Z)) {
        goto Lb260;
    }
    smbneo_native_fn_player_proc_player_friction_b479(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Lb260: ;
    smbneo_native_fn_motion_x_player_be18(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_write(ctx, 0x06ffu, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0eu));
    native_cmp(ctx, ctx->a, 0x0bu);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb271;
    }
    ctx->a = native_nz(ctx, 0x28u);
    native_write(ctx, 0x0709u, ctx->a);
Lb271: ;
    /* static tail transfer */
    goto Lbe5c;
Lbe5b: ;
    return;
Lbe5c: ;
    ctx->x = native_nz(ctx, 0x00u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0747u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lbe68;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x070eu));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lbe5b;
    }
Lbe68: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x0709u));
    native_write(ctx, 0x00u, ctx->a);
    ctx->a = native_nz(ctx, 0x04u);
    /* static tail transfer */
    smbneo_native_fn_motion_gravity_do_bebe(ctx);
    return;
    return;
}

void smbneo_native_fn_player_proc_player_climb_b27c(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xb27cu);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0416u));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, 0x0433u));
    native_write(ctx, 0x0416u, ctx->a);
    ctx->y = native_nz(ctx, 0x00u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x9fu));
    if (!(ctx->status & NATIVE_N)) {
        goto Lb28d;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
Lb28d: ;
    native_write(ctx, 0x00u, ctx->y);
    native_adc(ctx, native_read(ctx, 0xceu));
    native_write(ctx, 0xceu, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0xb5u));
    native_adc(ctx, native_read(ctx, 0x00u));
    native_write(ctx, 0xb5u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0cu));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & native_read(ctx, 0x0490u)));
    if ((ctx->status & NATIVE_Z)) {
        goto Lb2cd;
    }
    ctx->y = native_nz(ctx, native_read(ctx, 0x0789u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb2cc;
    }
    ctx->y = native_nz(ctx, 0x18u);
    native_write(ctx, 0x0789u, ctx->y);
    ctx->x = native_nz(ctx, 0x00u);
    ctx->y = native_nz(ctx, native_read(ctx, 0x33u));
    ctx->a = native_lsr(ctx, ctx->a);
    if ((ctx->status & NATIVE_C)) {
        goto Lb2b3;
    }
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
Lb2b3: ;
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lb2b7;
    }
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
Lb2b7: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x86u));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, (uint16_t)(0xb274u + ctx->x)));
    native_write(ctx, 0x86u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x6du));
    native_adc(ctx, native_read(ctx, (uint16_t)(0xb278u + ctx->x)));
    native_write(ctx, 0x6du, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0cu));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a ^ 0x03u));
    native_write(ctx, 0x33u, ctx->a);
Lb2cc: ;
    return;
Lb2cd: ;
    native_write(ctx, 0x0789u, ctx->a);
    return;
    return;
}

void smbneo_native_fn_player_proc_player_physics_b2fd(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xb2fdu);
    ctx->a = native_nz(ctx, native_read(ctx, 0x1du));
    native_cmp(ctx, ctx->a, 0x03u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb326;
    }
    ctx->y = native_nz(ctx, 0x00u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0bu));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & native_read(ctx, 0x0490u)));
    if ((ctx->status & NATIVE_Z)) {
        goto Lb312;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x08u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb312;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
Lb312: ;
    ctx->x = native_nz(ctx, native_read(ctx, (uint16_t)(0xb2fau + ctx->y)));
    native_write(ctx, 0x0433u, ctx->x);
    ctx->a = native_nz(ctx, 0x08u);
    ctx->x = native_nz(ctx, native_read(ctx, (uint16_t)(0xb2f7u + ctx->y)));
    native_write(ctx, 0x9fu, ctx->x);
    if ((ctx->status & NATIVE_N)) {
        goto Lb322;
    }
    ctx->a = native_lsr(ctx, ctx->a);
Lb322: ;
    native_write(ctx, 0x070cu, ctx->a);
    return;
Lb326: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x070eu));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb335;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x0au));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x80u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lb335;
    }
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & native_read(ctx, 0x0du)));
    if ((ctx->status & NATIVE_Z)) {
        goto Lb338;
    }
Lb335: ;
    /* static tail transfer */
    goto Lb3c9;
Lb338: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x1du));
    if ((ctx->status & NATIVE_Z)) {
        goto Lb34d;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x0704u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lb335;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x0782u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb34d;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x9fu));
    if (!(ctx->status & NATIVE_N)) {
        goto Lb34d;
    }
    /* static tail transfer */
    goto Lb3c9;
Lb34d: ;
    ctx->a = native_nz(ctx, 0x20u);
    native_write(ctx, 0x0782u, ctx->a);
    ctx->y = native_nz(ctx, 0x00u);
    native_write(ctx, 0x0416u, ctx->y);
    native_write(ctx, 0x0433u, ctx->y);
    ctx->a = native_nz(ctx, native_read(ctx, 0xb5u));
    native_write(ctx, 0x0707u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0xceu));
    native_write(ctx, 0x0708u, ctx->a);
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, 0x1du, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0700u));
    native_cmp(ctx, ctx->a, 0x09u);
    if (!(ctx->status & NATIVE_C)) {
        goto Lb37f;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    native_cmp(ctx, ctx->a, 0x10u);
    if (!(ctx->status & NATIVE_C)) {
        goto Lb37f;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    native_cmp(ctx, ctx->a, 0x19u);
    if (!(ctx->status & NATIVE_C)) {
        goto Lb37f;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    native_cmp(ctx, ctx->a, 0x1cu);
    if (!(ctx->status & NATIVE_C)) {
        goto Lb37f;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
Lb37f: ;
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, 0x0706u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0704u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lb391;
    }
    ctx->y = native_nz(ctx, 0x05u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x047du));
    if ((ctx->status & NATIVE_Z)) {
        goto Lb391;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
Lb391: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xb2d1u + ctx->y)));
    native_write(ctx, 0x0709u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xb2d8u + ctx->y)));
    native_write(ctx, 0x070au, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xb2e6u + ctx->y)));
    native_write(ctx, 0x0433u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xb2dfu + ctx->y)));
    native_write(ctx, 0x9fu, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0704u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lb3be;
    }
    ctx->a = native_nz(ctx, 0x04u);
    native_write(ctx, 0xffu, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0xceu));
    native_cmp(ctx, ctx->a, 0x14u);
    if ((ctx->status & NATIVE_C)) {
        goto Lb3c9;
    }
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x9fu, ctx->a);
    /* static tail transfer */
    goto Lb3c9;
Lb3be: ;
    ctx->a = native_nz(ctx, 0x01u);
    ctx->y = native_nz(ctx, native_read(ctx, 0x0754u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lb3c7;
    }
    ctx->a = native_nz(ctx, 0x80u);
Lb3c7: ;
    native_write(ctx, 0xffu, ctx->a);
Lb3c9: ;
    ctx->y = native_nz(ctx, 0x00u);
    native_write(ctx, 0x00u, ctx->y);
    ctx->a = native_nz(ctx, native_read(ctx, 0x1du));
    if ((ctx->status & NATIVE_Z)) {
        goto Lb3da;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x0700u));
    native_cmp(ctx, ctx->a, 0x19u);
    if ((ctx->status & NATIVE_C)) {
        goto Lb40b;
    }
    if (!(ctx->status & NATIVE_C)) {
        goto Lb3f2;
    }
Lb3da: ;
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->a = native_nz(ctx, native_read(ctx, 0x074eu));
    if ((ctx->status & NATIVE_Z)) {
        goto Lb3f2;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    ctx->a = native_nz(ctx, native_read(ctx, 0x0cu));
    native_cmp(ctx, ctx->a, native_read(ctx, 0x45u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb3f2;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x0au));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x40u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb406;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x0783u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb40b;
    }
Lb3f2: ;
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    native_write(ctx, 0x00u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x00u) + 1u)));
    ctx->a = native_nz(ctx, native_read(ctx, 0x0703u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb401;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x0700u));
    native_cmp(ctx, ctx->a, 0x21u);
    if (!(ctx->status & NATIVE_C)) {
        goto Lb40b;
    }
Lb401: ;
    native_write(ctx, 0x00u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x00u) + 1u)));
    /* static tail transfer */
    goto Lb40b;
Lb406: ;
    ctx->a = native_nz(ctx, 0x0au);
    native_write(ctx, 0x0783u, ctx->a);
Lb40b: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xb2edu + ctx->y)));
    native_write(ctx, 0x0450u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0eu));
    native_cmp(ctx, ctx->a, 0x07u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb419;
    }
    ctx->y = native_nz(ctx, 0x03u);
Lb419: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xb2f0u + ctx->y)));
    native_write(ctx, 0x0456u, ctx->a);
    ctx->y = native_nz(ctx, native_read(ctx, 0x00u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xb2f4u + ctx->y)));
    native_write(ctx, 0x0702u, ctx->a);
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x0701u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x33u));
    native_cmp(ctx, ctx->a, native_read(ctx, 0x45u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lb438;
    }
    native_write(ctx, 0x0702u, native_asl(ctx, native_read(ctx, 0x0702u)));
    native_write(ctx, 0x0701u, native_rol(ctx, native_read(ctx, 0x0701u)));
Lb438: ;
    return;
    return;
}

void smbneo_native_fn_player_proc_player_ani_speed_b43c(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xb43cu);
    ctx->y = native_nz(ctx, 0x00u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0700u));
    native_cmp(ctx, ctx->a, 0x1cu);
    if ((ctx->status & NATIVE_C)) {
        goto Lb45a;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    native_cmp(ctx, ctx->a, 0x0eu);
    if ((ctx->status & NATIVE_C)) {
        goto Lb44b;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
Lb44b: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x06fcu));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x7fu));
    if ((ctx->status & NATIVE_Z)) {
        goto Lb472;
    }
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x03u));
    native_cmp(ctx, ctx->a, native_read(ctx, 0x45u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb460;
    }
    ctx->a = native_nz(ctx, 0x00u);
Lb45a: ;
    native_write(ctx, 0x0703u, ctx->a);
    /* static tail transfer */
    goto Lb472;
Lb460: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x0700u));
    native_cmp(ctx, ctx->a, 0x0bu);
    if ((ctx->status & NATIVE_C)) {
        goto Lb472;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x33u));
    native_write(ctx, 0x45u, ctx->a);
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x57u, ctx->a);
    native_write(ctx, 0x0705u, ctx->a);
Lb472: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xb439u + ctx->y)));
    native_write(ctx, 0x070cu, ctx->a);
    return;
    return;
}

void smbneo_native_fn_player_proc_player_friction_b479(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xb479u);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & native_read(ctx, 0x0490u)));
    native_cmp(ctx, ctx->a, 0x00u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb488;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x57u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lb4cd;
    }
    if (!(ctx->status & NATIVE_N)) {
        goto Lb4a9;
    }
    if ((ctx->status & NATIVE_N)) {
        goto Lb48b;
    }
Lb488: ;
    ctx->a = native_lsr(ctx, ctx->a);
    if (!(ctx->status & NATIVE_C)) {
        goto Lb4a9;
    }
Lb48b: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x0705u));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, 0x0702u));
    native_write(ctx, 0x0705u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x57u));
    native_adc(ctx, native_read(ctx, 0x0701u));
    native_write(ctx, 0x57u, ctx->a);
    native_cmp(ctx, ctx->a, native_read(ctx, 0x0456u));
    if ((ctx->status & NATIVE_N)) {
        goto Lb4c4;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x0456u));
    native_write(ctx, 0x57u, ctx->a);
    /* static tail transfer */
    goto Lb4cd;
Lb4a9: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x0705u));
    ctx->status |= NATIVE_C;
    native_sbc(ctx, native_read(ctx, 0x0702u));
    native_write(ctx, 0x0705u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x57u));
    native_sbc(ctx, native_read(ctx, 0x0701u));
    native_write(ctx, 0x57u, ctx->a);
    native_cmp(ctx, ctx->a, native_read(ctx, 0x0450u));
    if (!(ctx->status & NATIVE_N)) {
        goto Lb4c4;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x0450u));
    native_write(ctx, 0x57u, ctx->a);
Lb4c4: ;
    native_cmp(ctx, ctx->a, 0x00u);
    if (!(ctx->status & NATIVE_N)) {
        goto Lb4cd;
    }
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a ^ 0xffu));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x01u);
Lb4cd: ;
    native_write(ctx, 0x0700u, ctx->a);
    return;
    return;
}

void smbneo_native_fn_proj_proc_b4d1(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xb4d1u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0756u));
    native_cmp(ctx, ctx->a, 0x02u);
    if (!(ctx->status & NATIVE_C)) {
        goto Lb51b;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x0au));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x40u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lb511;
    }
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & native_read(ctx, 0x0du)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb511;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x06ceu));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x01u));
    ctx->x = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x24u + ctx->x)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb511;
    }
    ctx->y = native_nz(ctx, native_read(ctx, 0xb5u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb511;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x0714u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb511;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x1du));
    native_cmp(ctx, ctx->a, 0x03u);
    if ((ctx->status & NATIVE_Z)) {
        goto Lb511;
    }
    ctx->a = native_nz(ctx, 0x20u);
    native_write(ctx, 0xffu, ctx->a);
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, (uint16_t)(uint8_t)(0x24u + ctx->x), ctx->a);
    ctx->y = native_nz(ctx, native_read(ctx, 0x070cu));
    native_write(ctx, 0x0711u, ctx->y);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    native_write(ctx, 0x0781u, ctx->y);
    native_write(ctx, 0x06ceu, native_nz(ctx, (uint8_t)(native_read(ctx, 0x06ceu) + 1u)));
Lb511: ;
    ctx->x = native_nz(ctx, 0x00u);
    smbneo_native_fn_proj_proc_do_b536(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->x = native_nz(ctx, 0x01u);
    smbneo_native_fn_proj_proc_do_b536(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Lb51b: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x074eu));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb533;
    }
    ctx->x = native_nz(ctx, 0x02u);
Lb522: ;
    native_write(ctx, 0x08u, ctx->x);
    smbneo_native_fn_bubble_proc_b5a6(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_pos_calc_x_rel_bubble_f096(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_pos_bits_get_bubble_f0f6(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_bubble_render_ed3e(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x - 1u));
    if (!(ctx->status & NATIVE_N)) {
        goto Lb522;
    }
Lb533: ;
    return;
    return;
}

void smbneo_native_fn_proj_proc_do_b536(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xb536u);
    native_write(ctx, 0x08u, ctx->x);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x24u + ctx->x)));
    ctx->a = native_asl(ctx, ctx->a);
    if ((ctx->status & NATIVE_C)) {
        goto Lb5a0;
    }
    ctx->y = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x24u + ctx->x)));
    if ((ctx->status & NATIVE_Z)) {
        goto Lb59f;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lb56b;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x86u));
    native_adc(ctx, 0x04u);
    native_write(ctx, (uint16_t)(uint8_t)(0x8du + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x6du));
    native_adc(ctx, 0x00u);
    native_write(ctx, (uint16_t)(uint8_t)(0x74u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0xceu));
    native_write(ctx, (uint16_t)(uint8_t)(0xd5u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, (uint16_t)(uint8_t)(0xbcu + ctx->x), ctx->a);
    ctx->y = native_nz(ctx, native_read(ctx, 0x33u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xb534u + ctx->y)));
    native_write(ctx, (uint16_t)(uint8_t)(0x5eu + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x04u);
    native_write(ctx, (uint16_t)(uint8_t)(0xa6u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x07u);
    native_write(ctx, (uint16_t)(0x04a0u + ctx->x), ctx->a);
    native_write(ctx, (uint16_t)(uint8_t)(0x24u + ctx->x), native_nz(ctx, (uint8_t)(native_read(ctx, (uint16_t)(uint8_t)(0x24u + ctx->x)) - 1u)));
Lb56b: ;
    ctx->a = native_nz(ctx, ctx->x);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x07u);
    ctx->x = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, 0x50u);
    native_write(ctx, 0x00u, ctx->a);
    ctx->a = native_nz(ctx, 0x03u);
    native_write(ctx, 0x02u, ctx->a);
    ctx->a = native_nz(ctx, 0x00u);
    smbneo_native_fn_motion_gravity_beea(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_motion_x_do_be1e(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    smbneo_native_fn_pos_calc_x_rel_proj_f0a0(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_pos_bits_get_proj_f0ec(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_col_proj_box_e183(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_col_proj_proc_e11e(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, 0x03d2u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0xccu));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb59b;
    }
    smbneo_native_fn_col_proj_actor_proc_d630(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    /* static tail transfer */
    goto Lec3b;
Lb59b: ;
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, (uint16_t)(uint8_t)(0x24u + ctx->x), ctx->a);
Lb59f: ;
    return;
Lb5a0: ;
    smbneo_native_fn_pos_calc_x_rel_proj_f0a0(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    /* static tail transfer */
    goto Lec66;
Lec3b: ;
    ctx->y = native_nz(ctx, native_read(ctx, (uint16_t)(0x06f1u + ctx->x)));
    ctx->a = native_nz(ctx, native_read(ctx, 0x03bau));
    native_write(ctx, (uint16_t)(0x0200u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x03afu));
    native_write(ctx, (uint16_t)(0x0203u + ctx->y), ctx->a);
    smbneo_native_fn_render_firebar_ec4a(ctx);
    return;
Lec66: ;
    ctx->y = native_nz(ctx, native_read(ctx, (uint16_t)(0x06ecu + ctx->x)));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x24u + ctx->x)));
    native_write(ctx, (uint16_t)(uint8_t)(0x24u + ctx->x), native_nz(ctx, (uint8_t)(native_read(ctx, (uint16_t)(uint8_t)(0x24u + ctx->x)) + 1u)));
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x07u));
    native_cmp(ctx, ctx->a, 0x03u);
    if ((ctx->status & NATIVE_C)) {
        goto Lecbe;
    }
    smbneo_native_fn_render_fireworks_ec74(ctx);
    return;
Lecbe: ;
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, (uint16_t)(uint8_t)(0x24u + ctx->x), ctx->a);
    return;
    return;
}

void smbneo_native_fn_bubble_proc_b5a6(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xb5a6u);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x07a8u + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x01u));
    native_write(ctx, 0x07u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xe4u + ctx->x)));
    native_cmp(ctx, ctx->a, 0xf8u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb5df;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x0792u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb5f7;
    }
    smbneo_native_fn_bubble_init_b5b8(ctx);
    return;
Lb5df: ;
    ctx->y = native_nz(ctx, native_read(ctx, 0x07u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x042cu + ctx->x)));
    ctx->status |= NATIVE_C;
    native_sbc(ctx, native_read(ctx, (uint16_t)(0xb5f8u + ctx->y)));
    native_write(ctx, (uint16_t)(0x042cu + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xe4u + ctx->x)));
    native_sbc(ctx, 0x00u);
    native_cmp(ctx, ctx->a, 0x20u);
    if ((ctx->status & NATIVE_C)) {
        goto Lb5f5;
    }
    ctx->a = native_nz(ctx, 0xf8u);
Lb5f5: ;
    native_write(ctx, (uint16_t)(uint8_t)(0xe4u + ctx->x), ctx->a);
Lb5f7: ;
    return;
    return;
}

void smbneo_native_fn_bubble_init_b5b8(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xb5b8u);
    ctx->y = native_nz(ctx, 0x00u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x33u));
    ctx->a = native_lsr(ctx, ctx->a);
    if (!(ctx->status & NATIVE_C)) {
        goto Lb5c1;
    }
    ctx->y = native_nz(ctx, 0x08u);
Lb5c1: ;
    ctx->a = native_nz(ctx, ctx->y);
    native_adc(ctx, native_read(ctx, 0x86u));
    native_write(ctx, (uint16_t)(uint8_t)(0x9cu + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x6du));
    native_adc(ctx, 0x00u);
    native_write(ctx, (uint16_t)(uint8_t)(0x83u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0xceu));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x08u);
    native_write(ctx, (uint16_t)(uint8_t)(0xe4u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, (uint16_t)(uint8_t)(0xcbu + ctx->x), ctx->a);
    ctx->y = native_nz(ctx, native_read(ctx, 0x07u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xb5fau + ctx->y)));
    native_write(ctx, 0x0792u, ctx->a);
    ctx->y = native_nz(ctx, native_read(ctx, 0x07u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x042cu + ctx->x)));
    ctx->status |= NATIVE_C;
    native_sbc(ctx, native_read(ctx, (uint16_t)(0xb5f8u + ctx->y)));
    native_write(ctx, (uint16_t)(0x042cu + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xe4u + ctx->x)));
    native_sbc(ctx, 0x00u);
    native_cmp(ctx, ctx->a, 0x20u);
    if ((ctx->status & NATIVE_C)) {
        goto Lb5f5;
    }
    ctx->a = native_nz(ctx, 0xf8u);
Lb5f5: ;
    native_write(ctx, (uint16_t)(uint8_t)(0xe4u + ctx->x), ctx->a);
    return;
    return;
}

void smbneo_native_fn_stats_time_proc_b5fc(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xb5fcu);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0770u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lb657;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x0eu));
    native_cmp(ctx, ctx->a, 0x08u);
    if (!(ctx->status & NATIVE_C)) {
        goto Lb657;
    }
    native_cmp(ctx, ctx->a, 0x0bu);
    if ((ctx->status & NATIVE_Z)) {
        goto Lb657;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0xb5u));
    native_cmp(ctx, ctx->a, 0x02u);
    if ((ctx->status & NATIVE_C)) {
        goto Lb657;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x0787u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb657;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x07f8u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | native_read(ctx, 0x07f9u)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | native_read(ctx, 0x07fau)));
    if ((ctx->status & NATIVE_Z)) {
        goto Lb64e;
    }
    ctx->y = native_nz(ctx, native_read(ctx, 0x07f8u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb633;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x07f9u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | native_read(ctx, 0x07fau)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb633;
    }
    ctx->a = native_nz(ctx, 0x40u);
    native_write(ctx, 0xfcu, ctx->a);
Lb633: ;
    ctx->a = native_nz(ctx, 0x11u);
    ctx->y = native_nz(ctx, native_read(ctx, 0x6603u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb63c;
    }
    ctx->a = native_nz(ctx, 0x14u);
Lb63c: ;
    native_write(ctx, 0x0787u, ctx->a);
    ctx->y = native_nz(ctx, 0x23u);
    ctx->a = native_nz(ctx, 0xffu);
    native_write(ctx, 0x0139u, ctx->a);
    smbneo_native_fn_stats_calc_9217(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, 0xa4u);
    /* static tail transfer */
    smbneo_native_fn_stats_print_num_91be(ctx);
    return;
Lb64e: ;
    native_write(ctx, 0x0756u, ctx->a);
    smbneo_native_fn_col_player_harm_proc_do_d888(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_write(ctx, 0x0759u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0759u) + 1u)));
Lb657: ;
    return;
    return;
}

void smbneo_native_fn_actor_B_1F_proc_b658(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xb658u);
    goto Lb658;
Lb657: ;
    return;
Lb658: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x0723u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lb657;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0xceu));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & native_read(ctx, 0xb5u)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb657;
    }
    native_write(ctx, 0x0723u, ctx->a);
    native_write(ctx, 0x06d6u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x06d6u) + 1u)));
    /* static tail transfer */
    smbneo_native_fn_actor_erase_c8db(ctx);
    return;
    return;
}

void smbneo_native_fn_whirlpool_proc_b66c(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xb66cu);
    ctx->a = native_nz(ctx, native_read(ctx, 0x074eu));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb6a8;
    }
    native_write(ctx, 0x047du, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0747u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb6a8;
    }
    ctx->y = native_nz(ctx, 0x04u);
Lb67b: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0471u + ctx->y)));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, (uint16_t)(0x0477u + ctx->y)));
    native_write(ctx, 0x02u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x046bu + ctx->y)));
    if ((ctx->status & NATIVE_Z)) {
        goto Lb6a5;
    }
    native_adc(ctx, 0x00u);
    native_write(ctx, 0x01u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x86u));
    ctx->status |= NATIVE_C;
    native_sbc(ctx, native_read(ctx, (uint16_t)(0x0471u + ctx->y)));
    ctx->a = native_nz(ctx, native_read(ctx, 0x6du));
    native_sbc(ctx, native_read(ctx, (uint16_t)(0x046bu + ctx->y)));
    if ((ctx->status & NATIVE_N)) {
        goto Lb6a5;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x02u));
    ctx->status |= NATIVE_C;
    native_sbc(ctx, native_read(ctx, 0x86u));
    ctx->a = native_nz(ctx, native_read(ctx, 0x01u));
    native_sbc(ctx, native_read(ctx, 0x6du));
    if (!(ctx->status & NATIVE_N)) {
        goto Lb6a9;
    }
Lb6a5: ;
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    if (!(ctx->status & NATIVE_N)) {
        goto Lb67b;
    }
Lb6a8: ;
    return;
Lb6a9: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0477u + ctx->y)));
    ctx->a = native_lsr(ctx, ctx->a);
    native_write(ctx, 0x00u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0471u + ctx->y)));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, 0x00u));
    native_write(ctx, 0x01u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x046bu + ctx->y)));
    native_adc(ctx, 0x00u);
    native_write(ctx, 0x00u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x09u));
    ctx->a = native_lsr(ctx, ctx->a);
    if (!(ctx->status & NATIVE_C)) {
        goto Lb6ef;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x01u));
    ctx->status |= NATIVE_C;
    native_sbc(ctx, native_read(ctx, 0x86u));
    ctx->a = native_nz(ctx, native_read(ctx, 0x00u));
    native_sbc(ctx, native_read(ctx, 0x6du));
    if (!(ctx->status & NATIVE_N)) {
        goto Lb6dc;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x86u));
    ctx->status |= NATIVE_C;
    native_sbc(ctx, 0x01u);
    native_write(ctx, 0x86u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x6du));
    native_sbc(ctx, 0x00u);
    /* static tail transfer */
    goto Lb6ed;
Lb6dc: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x0490u));
    ctx->a = native_lsr(ctx, ctx->a);
    if (!(ctx->status & NATIVE_C)) {
        goto Lb6ef;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x86u));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x01u);
    native_write(ctx, 0x86u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x6du));
    native_adc(ctx, 0x00u);
Lb6ed: ;
    native_write(ctx, 0x6du, ctx->a);
Lb6ef: ;
    ctx->a = native_nz(ctx, 0x10u);
    native_write(ctx, 0x00u, ctx->a);
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, 0x047du, ctx->a);
    native_write(ctx, 0x02u, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->x = native_nz(ctx, ctx->a);
    /* static tail transfer */
    smbneo_native_fn_motion_gravity_beea(ctx);
    return;
    return;
}

void smbneo_native_fn_pole_proc_b709(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xb709u);
    ctx->x = native_nz(ctx, 0x05u);
    native_write(ctx, 0x08u, ctx->x);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x16u + ctx->x)));
    native_cmp(ctx, ctx->a, 0x30u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb769;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x0eu));
    native_cmp(ctx, ctx->a, 0x04u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb74a;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x1du));
    native_cmp(ctx, ctx->a, 0x03u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb74a;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x)));
    native_cmp(ctx, ctx->a, 0xaau);
    if ((ctx->status & NATIVE_C)) {
        goto Lb74d;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0xceu));
    native_cmp(ctx, ctx->a, 0xa2u);
    if ((ctx->status & NATIVE_C)) {
        goto Lb74d;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0417u + ctx->x)));
    native_adc(ctx, 0xffu);
    native_write(ctx, (uint16_t)(0x0417u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x)));
    native_adc(ctx, 0x01u);
    native_write(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x010eu));
    ctx->status |= NATIVE_C;
    native_sbc(ctx, 0xffu);
    native_write(ctx, 0x010eu, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x010du));
    native_sbc(ctx, 0x01u);
    native_write(ctx, 0x010du, ctx->a);
Lb74a: ;
    /* static tail transfer */
    goto Lb760;
Lb74d: ;
    ctx->y = native_nz(ctx, native_read(ctx, 0x010fu));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xb6ffu + ctx->y)));
    ctx->x = native_nz(ctx, native_read(ctx, (uint16_t)(0xb704u + ctx->y)));
    native_write(ctx, (uint16_t)(0x0134u + ctx->x), ctx->a);
    smbneo_native_fn_misc_proc_coin_update_score_bb32(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, 0x05u);
    native_write(ctx, 0x0eu, ctx->a);
Lb760: ;
    smbneo_native_fn_pos_bits_get_actor_f114(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_pos_calc_x_rel_actor_f0b7(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_render_flag_enemy_e4a8(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Lb769: ;
    return;
    return;
}

void smbneo_native_fn_actor_B_1D_proc_b76e(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xb76eu);
    smbneo_native_fn_pos_bits_get_actor_f114(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, 0x0747u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb7b6;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x070eu));
    if ((ctx->status & NATIVE_Z)) {
        goto Lb7b6;
    }
    ctx->y = native_nz(ctx, ctx->a);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    ctx->a = native_nz(ctx, ctx->y);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x02u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb789;
    }
    native_write(ctx, 0xceu, native_nz(ctx, (uint8_t)(native_read(ctx, 0xceu) + 1u)));
    native_write(ctx, 0xceu, native_nz(ctx, (uint8_t)(native_read(ctx, 0xceu) + 1u)));
    /* static tail transfer */
    goto Lb78d;
Lb789: ;
    native_write(ctx, 0xceu, native_nz(ctx, (uint8_t)(native_read(ctx, 0xceu) - 1u)));
    native_write(ctx, 0xceu, native_nz(ctx, (uint8_t)(native_read(ctx, 0xceu) - 1u)));
Lb78d: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x)));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, (uint16_t)(0xb76au + ctx->y)));
    native_write(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x), ctx->a);
    native_cmp(ctx, ctx->y, 0x01u);
    if (!(ctx->status & NATIVE_C)) {
        goto Lb7a8;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x0au));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x80u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lb7a8;
    }
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & native_read(ctx, 0x0du)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb7a8;
    }
    ctx->a = native_nz(ctx, 0xf4u);
    native_write(ctx, 0x06dbu, ctx->a);
Lb7a8: ;
    native_cmp(ctx, ctx->y, 0x03u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb7b6;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x06dbu));
    native_write(ctx, 0x9fu, ctx->a);
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x070eu, ctx->a);
Lb7b6: ;
    smbneo_native_fn_pos_calc_x_rel_actor_f0b7(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_render_actor_e7da(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_actor_oob_proc_d5bd(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, 0x070eu));
    if ((ctx->status & NATIVE_Z)) {
        goto Lb7d1;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x0786u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb7d1;
    }
    ctx->a = native_nz(ctx, 0x04u);
    native_write(ctx, 0x0786u, ctx->a);
    native_write(ctx, 0x070eu, native_nz(ctx, (uint8_t)(native_read(ctx, 0x070eu) + 1u)));
Lb7d1: ;
    return;
    return;
}

void smbneo_native_fn_actor_B_1A_init_b7d2(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xb7d2u);
    ctx->a = native_nz(ctx, 0x2fu);
    native_write(ctx, (uint16_t)(uint8_t)(0x16u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, (uint16_t)(uint8_t)(0x0fu + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0076u + ctx->y)));
    native_write(ctx, (uint16_t)(uint8_t)(0x6eu + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x008fu + ctx->y)));
    native_write(ctx, (uint16_t)(uint8_t)(0x87u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x00d7u + ctx->y)));
    native_write(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x), ctx->a);
    ctx->y = native_nz(ctx, native_read(ctx, 0x0398u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb7f1;
    }
    native_write(ctx, 0x039du, ctx->a);
Lb7f1: ;
    ctx->a = native_nz(ctx, ctx->x);
    native_write(ctx, (uint16_t)(0x039au + ctx->y), ctx->a);
    native_write(ctx, 0x0398u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0398u) + 1u)));
    ctx->a = native_nz(ctx, 0x04u);
    native_write(ctx, 0xfeu, ctx->a);
    return;
    return;
}

void smbneo_native_fn_actor_B_1A_proc_b7ff(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xb7ffu);
    native_cmp(ctx, ctx->x, 0x05u);
    if ((ctx->status & NATIVE_Z)) {
        goto Lb804;
    }
    return;
Lb804: ;
    ctx->y = native_nz(ctx, native_read(ctx, 0x0398u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    ctx->a = native_nz(ctx, native_read(ctx, 0x0399u));
    native_cmp(ctx, ctx->a, native_read(ctx, (uint16_t)(0xb7fdu + ctx->y)));
    if ((ctx->status & NATIVE_Z)) {
        goto Lb81f;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x09u));
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    if (!(ctx->status & NATIVE_C)) {
        goto Lb81f;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0xd4u));
    native_sbc(ctx, 0x01u);
    native_write(ctx, 0xd4u, ctx->a);
    native_write(ctx, 0x0399u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0399u) + 1u)));
Lb81f: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x0399u));
    native_cmp(ctx, ctx->a, 0x08u);
    if (!(ctx->status & NATIVE_C)) {
        goto Lb86c;
    }
    smbneo_native_fn_pos_calc_x_rel_actor_f0b7(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_pos_bits_get_actor_f114(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->y = native_nz(ctx, 0x00u);
Lb82e: ;
    smbneo_native_fn_render_vine_e392(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    native_cmp(ctx, ctx->y, native_read(ctx, 0x0398u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb82e;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x03d1u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x0cu));
    if ((ctx->status & NATIVE_Z)) {
        goto Lb84e;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
Lb83f: ;
    ctx->x = native_nz(ctx, native_read(ctx, (uint16_t)(0x039au + ctx->y)));
    smbneo_native_fn_actor_erase_c8db(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    if (!(ctx->status & NATIVE_N)) {
        goto Lb83f;
    }
    native_write(ctx, 0x0398u, ctx->a);
    native_write(ctx, 0x0399u, ctx->a);
Lb84e: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x0399u));
    native_cmp(ctx, ctx->a, 0x20u);
    if (!(ctx->status & NATIVE_C)) {
        goto Lb86c;
    }
    ctx->x = native_nz(ctx, 0x06u);
    ctx->a = native_nz(ctx, 0x01u);
    ctx->y = native_nz(ctx, 0x1bu);
    smbneo_native_fn_col_box_buffer_e348(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->y = native_nz(ctx, native_read(ctx, 0x02u));
    native_cmp(ctx, ctx->y, 0xd0u);
    if ((ctx->status & NATIVE_C)) {
        goto Lb86c;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0x06u) + ctx->y)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb86c;
    }
    ctx->a = native_nz(ctx, 0x26u);
    native_write(ctx, (uint16_t)(native_zpword(ctx, 0x06u) + ctx->y), ctx->a);
Lb86c: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x8cu));
    ctx->status |= NATIVE_C;
    native_sbc(ctx, native_read(ctx, 0x071cu));
    ctx->y = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x73u));
    native_sbc(ctx, native_read(ctx, 0x071au));
    if ((ctx->status & NATIVE_N)) {
        goto Lb87e;
    }
    native_cmp(ctx, ctx->y, 0x09u);
    if ((ctx->status & NATIVE_C)) {
        goto Lb8ab;
    }
Lb87e: ;
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x14u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x73u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x01u));
    ctx->y = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xab10u + ctx->y)));
    native_write(ctx, 0x06u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xab12u + ctx->y)));
    native_write(ctx, 0x07u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x8cu));
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
Lb898: ;
    ctx->y = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0x06u) + ctx->y)));
    native_cmp(ctx, ctx->a, 0x26u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb8a3;
    }
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, (uint16_t)(native_zpword(ctx, 0x06u) + ctx->y), ctx->a);
Lb8a3: ;
    ctx->a = native_nz(ctx, ctx->y);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x10u);
    native_cmp(ctx, ctx->a, 0xd0u);
    if (!(ctx->status & NATIVE_C)) {
        goto Lb898;
    }
Lb8ab: ;
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    return;
    return;
}

void smbneo_native_fn_cannon_proc_b8b0(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xb8b0u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x074eu));
    if ((ctx->status & NATIVE_Z)) {
        goto Lb924;
    }
    ctx->x = native_nz(ctx, 0x02u);
Lb8b7: ;
    native_write(ctx, 0x08u, ctx->x);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x0fu + ctx->x)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb90e;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x07a8u + ctx->x)));
    ctx->y = native_nz(ctx, native_read(ctx, 0x06ccu));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & native_read(ctx, (uint16_t)(0xb8aeu + ctx->y))));
    native_cmp(ctx, ctx->a, 0x06u);
    if ((ctx->status & NATIVE_C)) {
        goto Lb90e;
    }
    ctx->y = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x046bu + ctx->y)));
    if ((ctx->status & NATIVE_Z)) {
        goto Lb90e;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x047du + ctx->y)));
    if ((ctx->status & NATIVE_Z)) {
        goto Lb8dd;
    }
    native_sbc(ctx, 0x00u);
    native_write(ctx, (uint16_t)(0x047du + ctx->y), ctx->a);
    /* static tail transfer */
    goto Lb90e;
Lb8dd: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x0747u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb90e;
    }
    ctx->a = native_nz(ctx, 0x0eu);
    native_write(ctx, (uint16_t)(0x047du + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x046bu + ctx->y)));
    native_write(ctx, (uint16_t)(uint8_t)(0x6eu + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0471u + ctx->y)));
    native_write(ctx, (uint16_t)(uint8_t)(0x87u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0477u + ctx->y)));
    ctx->status |= NATIVE_C;
    native_sbc(ctx, 0x08u);
    native_write(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, (uint16_t)(uint8_t)(0xb6u + ctx->x), ctx->a);
    native_write(ctx, (uint16_t)(uint8_t)(0x0fu + ctx->x), ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    native_write(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x09u);
    native_write(ctx, (uint16_t)(0x049au + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x33u);
    native_write(ctx, (uint16_t)(uint8_t)(0x16u + ctx->x), ctx->a);
    /* static tail transfer */
    goto Lb921;
Lb90e: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x16u + ctx->x)));
    native_cmp(ctx, ctx->a, 0x33u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb921;
    }
    smbneo_native_fn_actor_oob_proc_d5bd(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x0fu + ctx->x)));
    if ((ctx->status & NATIVE_Z)) {
        goto Lb921;
    }
    smbneo_native_fn_pos_bits_get_actor_f114(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_cannon_bullet_proc_b927(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Lb921: ;
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x - 1u));
    if (!(ctx->status & NATIVE_N)) {
        goto Lb8b7;
    }
Lb924: ;
    return;
    return;
}

void smbneo_native_fn_cannon_bullet_proc_b927(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xb927u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0747u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb96a;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb95e;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x03d1u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x0cu));
    native_cmp(ctx, ctx->a, 0x0cu);
    if ((ctx->status & NATIVE_Z)) {
        goto Lb979;
    }
    ctx->y = native_nz(ctx, 0x01u);
    smbneo_native_fn_col_player_bullet_diff_e099(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    if ((ctx->status & NATIVE_N)) {
        goto Lb941;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
Lb941: ;
    native_write(ctx, (uint16_t)(uint8_t)(0x46u + ctx->x), ctx->y);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xb925u + ctx->y)));
    native_write(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x00u));
    native_adc(ctx, 0x28u);
    native_cmp(ctx, ctx->a, 0x50u);
    if (!(ctx->status & NATIVE_C)) {
        goto Lb979;
    }
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x0au);
    native_write(ctx, (uint16_t)(0x078au + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x08u);
    native_write(ctx, 0xfeu, ctx->a);
Lb95e: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x20u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lb967;
    }
    smbneo_native_fn_motion_y_be72(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Lb967: ;
    smbneo_native_fn_motion_x_be11(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Lb96a: ;
    smbneo_native_fn_pos_bits_get_actor_f114(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_pos_calc_x_rel_actor_f0b7(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_col_actor_box_e199(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_col_player_actor_proc_d7aa(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    /* static tail transfer */
    smbneo_native_fn_render_actor_e7da(ctx);
    return;
Lb979: ;
    smbneo_native_fn_actor_erase_c8db(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    return;
    return;
}

void smbneo_native_fn_misc_init_hammer_b988(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xb988u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x07a8u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x07u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb994;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x07a8u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x08u));
Lb994: ;
    ctx->y = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x002au + ctx->y)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb9b3;
    }
    ctx->x = native_nz(ctx, native_read(ctx, (uint16_t)(0xb97du + ctx->y)));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x0fu + ctx->x)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lb9b3;
    }
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    ctx->a = native_nz(ctx, ctx->x);
    native_write(ctx, (uint16_t)(0x06aeu + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, 0x90u);
    native_write(ctx, (uint16_t)(0x002au + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, 0x07u);
    native_write(ctx, (uint16_t)(0x04a2u + ctx->y), ctx->a);
    ctx->status |= NATIVE_C;
    return;
Lb9b3: ;
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    ctx->status &= (uint8_t)~NATIVE_C;
    return;
    return;
}

void smbneo_native_fn_misc_proc_hammer_b9b7(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xb9b7u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0747u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lba1f;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x2au + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x7fu));
    ctx->y = native_nz(ctx, native_read(ctx, (uint16_t)(0x06aeu + ctx->x)));
    native_cmp(ctx, ctx->a, 0x02u);
    if ((ctx->status & NATIVE_Z)) {
        goto Lb9e7;
    }
    if ((ctx->status & NATIVE_C)) {
        goto Lb9fd;
    }
    ctx->a = native_nz(ctx, ctx->x);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x0du);
    ctx->x = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, 0x10u);
    native_write(ctx, 0x00u, ctx->a);
    ctx->a = native_nz(ctx, 0x0fu);
    native_write(ctx, 0x01u, ctx->a);
    ctx->a = native_nz(ctx, 0x04u);
    native_write(ctx, 0x02u, ctx->a);
    ctx->a = native_nz(ctx, 0x00u);
    smbneo_native_fn_motion_gravity_beea(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_motion_x_do_be1e(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    /* static tail transfer */
    goto Lba1c;
Lb9e7: ;
    ctx->a = native_nz(ctx, 0xfeu);
    native_write(ctx, (uint16_t)(uint8_t)(0xacu + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x001eu + ctx->y)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0xf7u));
    native_write(ctx, (uint16_t)(0x001eu + ctx->y), ctx->a);
    ctx->x = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x46u + ctx->y)));
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x - 1u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xb986u + ctx->x)));
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    native_write(ctx, (uint16_t)(uint8_t)(0x64u + ctx->x), ctx->a);
Lb9fd: ;
    native_write(ctx, (uint16_t)(uint8_t)(0x2au + ctx->x), native_nz(ctx, (uint8_t)(native_read(ctx, (uint16_t)(uint8_t)(0x2au + ctx->x)) - 1u)));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0087u + ctx->y)));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x02u);
    native_write(ctx, (uint16_t)(uint8_t)(0x93u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x006eu + ctx->y)));
    native_adc(ctx, 0x00u);
    native_write(ctx, (uint16_t)(uint8_t)(0x7au + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x00cfu + ctx->y)));
    ctx->status |= NATIVE_C;
    native_sbc(ctx, 0x0au);
    native_write(ctx, (uint16_t)(uint8_t)(0xdbu + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, (uint16_t)(uint8_t)(0xc2u + ctx->x), ctx->a);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lba1f;
    }
Lba1c: ;
    smbneo_native_fn_col_player_hammer_proc_d71b(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Lba1f: ;
    smbneo_native_fn_pos_bits_get_misc_f100(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_pos_calc_x_rel_misc_f0ad(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_col_misc_box_e18c(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_render_hammer_e439(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    return;
    return;
}

void smbneo_native_fn_misc_init_coin_a_ba2c(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xba2cu);
    smbneo_native_fn_misc_init_coin_block_alloc_ba78(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x76u + ctx->x)));
    native_write(ctx, (uint16_t)(0x007au + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x8fu + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | 0x05u));
    native_write(ctx, (uint16_t)(0x0093u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xd7u + ctx->x)));
    native_sbc(ctx, 0x10u);
    native_write(ctx, (uint16_t)(0x00dbu + ctx->y), ctx->a);
    /* static tail transfer */
    goto Lba60;
Lba60: ;
    ctx->a = native_nz(ctx, 0xfbu);
    native_write(ctx, (uint16_t)(0x00acu + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, (uint16_t)(0x00c2u + ctx->y), ctx->a);
    native_write(ctx, (uint16_t)(0x002au + ctx->y), ctx->a);
    native_write(ctx, 0xfeu, ctx->a);
    native_write(ctx, 0x08u, ctx->x);
    smbneo_native_fn_misc_proc_coin_add_baf6(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_write(ctx, 0x0748u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0748u) + 1u)));
    return;
    return;
}

void smbneo_native_fn_misc_init_coin_b_ba45(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xba45u);
    smbneo_native_fn_misc_init_coin_block_alloc_ba78(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x03eau + ctx->x)));
    native_write(ctx, (uint16_t)(0x007au + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x06u));
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | 0x05u));
    native_write(ctx, (uint16_t)(0x0093u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x02u));
    native_adc(ctx, 0x20u);
    native_write(ctx, (uint16_t)(0x00dbu + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, 0xfbu);
    native_write(ctx, (uint16_t)(0x00acu + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, (uint16_t)(0x00c2u + ctx->y), ctx->a);
    native_write(ctx, (uint16_t)(0x002au + ctx->y), ctx->a);
    native_write(ctx, 0xfeu, ctx->a);
    native_write(ctx, 0x08u, ctx->x);
    smbneo_native_fn_misc_proc_coin_add_baf6(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_write(ctx, 0x0748u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0748u) + 1u)));
    return;
    return;
}

void smbneo_native_fn_misc_init_coin_block_alloc_ba78(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xba78u);
    ctx->y = native_nz(ctx, 0x08u);
Lba7a: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x002au + ctx->y)));
    if ((ctx->status & NATIVE_Z)) {
        goto Lba86;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    native_cmp(ctx, ctx->y, 0x05u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lba7a;
    }
    ctx->y = native_nz(ctx, 0x08u);
Lba86: ;
    native_write(ctx, 0x06b7u, ctx->y);
    return;
    return;
}

void smbneo_native_fn_misc_proc_ba8a(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xba8au);
    ctx->x = native_nz(ctx, 0x08u);
Lba8c: ;
    native_write(ctx, 0x08u, ctx->x);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x2au + ctx->x)));
    if ((ctx->status & NATIVE_Z)) {
        goto Lbae8;
    }
    ctx->a = native_asl(ctx, ctx->a);
    if (!(ctx->status & NATIVE_C)) {
        goto Lba9b;
    }
    smbneo_native_fn_misc_proc_hammer_b9b7(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    /* static tail transfer */
    goto Lbae8;
Lba9b: ;
    ctx->y = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x2au + ctx->x)));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lbabd;
    }
    native_write(ctx, (uint16_t)(uint8_t)(0x2au + ctx->x), native_nz(ctx, (uint8_t)(native_read(ctx, (uint16_t)(uint8_t)(0x2au + ctx->x)) + 1u)));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x93u + ctx->x)));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, 0x0775u));
    native_write(ctx, (uint16_t)(uint8_t)(0x93u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x7au + ctx->x)));
    native_adc(ctx, 0x00u);
    native_write(ctx, (uint16_t)(uint8_t)(0x7au + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x2au + ctx->x)));
    native_cmp(ctx, ctx->a, 0x30u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lbadc;
    }
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, (uint16_t)(uint8_t)(0x2au + ctx->x), ctx->a);
    /* static tail transfer */
    goto Lbae8;
Lbabd: ;
    ctx->a = native_nz(ctx, ctx->x);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x0du);
    ctx->x = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, 0x50u);
    native_write(ctx, 0x00u, ctx->a);
    ctx->a = native_nz(ctx, 0x06u);
    native_write(ctx, 0x02u, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    native_write(ctx, 0x01u, ctx->a);
    ctx->a = native_nz(ctx, 0x00u);
    smbneo_native_fn_motion_gravity_beea(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xacu + ctx->x)));
    native_cmp(ctx, ctx->a, 0x05u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lbadc;
    }
    native_write(ctx, (uint16_t)(uint8_t)(0x2au + ctx->x), native_nz(ctx, (uint8_t)(native_read(ctx, (uint16_t)(uint8_t)(0x2au + ctx->x)) + 1u)));
Lbadc: ;
    smbneo_native_fn_pos_calc_x_rel_misc_f0ad(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_pos_bits_get_misc_f100(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_col_misc_box_e18c(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_render_coin_score_e5e3(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Lbae8: ;
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x - 1u));
    if (!(ctx->status & NATIVE_N)) {
        goto Lba8c;
    }
    return;
    return;
}

void smbneo_native_fn_misc_proc_coin_add_baf6(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xbaf6u);
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, 0x0139u, ctx->a);
    ctx->x = native_nz(ctx, native_read(ctx, 0x0753u));
    ctx->y = native_nz(ctx, native_read(ctx, (uint16_t)(0xbaecu + ctx->x)));
    smbneo_native_fn_stats_calc_9217(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_write(ctx, 0x075eu, native_nz(ctx, (uint8_t)(native_read(ctx, 0x075eu) + 1u)));
    ctx->a = native_nz(ctx, native_read(ctx, 0x075eu));
    ctx->x = native_nz(ctx, native_read(ctx, 0x6602u));
    native_cmp(ctx, ctx->a, native_read(ctx, (uint16_t)(0xbaf2u + ctx->x)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lbb2d;
    }
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x075eu, ctx->a);
    ctx->x = native_nz(ctx, native_read(ctx, 0x0753u));
    ctx->y = native_nz(ctx, native_read(ctx, (uint16_t)(0xbaecu + ctx->x)));
    ctx->x = native_nz(ctx, 0x03u);
Lbb1f: ;
    native_write(ctx, (uint16_t)(0x07d7u + ctx->y), ctx->a);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x - 1u));
    if (!(ctx->status & NATIVE_N)) {
        goto Lbb1f;
    }
    native_write(ctx, 0x075au, native_nz(ctx, (uint8_t)(native_read(ctx, 0x075au) + 1u)));
    ctx->a = native_nz(ctx, 0x40u);
    native_write(ctx, 0xfeu, ctx->a);
Lbb2d: ;
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, 0x0138u, ctx->a);
    smbneo_native_fn_misc_proc_coin_update_score_bb32(ctx);
    return;
    return;
}

void smbneo_native_fn_misc_proc_coin_update_score_bb32(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xbb32u);
    ctx->x = native_nz(ctx, native_read(ctx, 0x0753u));
    ctx->y = native_nz(ctx, native_read(ctx, (uint16_t)(0xbaeeu + ctx->x)));
    smbneo_native_fn_stats_calc_9217(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_misc_proc_coin_update_stats_bb3b(ctx);
    return;
    return;
}

void smbneo_native_fn_misc_proc_coin_update_stats_bb3b(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xbb3bu);
    ctx->y = native_nz(ctx, native_read(ctx, 0x0753u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xbaf0u + ctx->y)));
    smbneo_native_fn_misc_proc_coin_add_score_bb41(ctx);
    return;
    return;
}

void smbneo_native_fn_misc_proc_coin_add_score_bb41(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xbb41u);
    smbneo_native_fn_stats_print_num_91be(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->y = native_nz(ctx, native_read(ctx, 0x0300u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x02fbu + ctx->y)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lbb51;
    }
    ctx->a = native_nz(ctx, 0x24u);
    native_write(ctx, (uint16_t)(0x02fbu + ctx->y), ctx->a);
Lbb51: ;
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    return;
    return;
}

void smbneo_native_fn_actor_B_19_init_bb6b(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xbb6bu);
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, 0x23u, ctx->a);
    native_write(ctx, 0x14u, ctx->a);
    ctx->a = native_nz(ctx, 0x03u);
    native_write(ctx, 0x049fu, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x39u));
    native_cmp(ctx, ctx->a, 0x02u);
    if ((ctx->status & NATIVE_C)) {
        goto Lbb86;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x0756u));
    native_cmp(ctx, ctx->a, 0x02u);
    if (!(ctx->status & NATIVE_C)) {
        goto Lbb84;
    }
    ctx->a = native_lsr(ctx, ctx->a);
Lbb84: ;
    native_write(ctx, 0x39u, ctx->a);
Lbb86: ;
    ctx->a = native_nz(ctx, 0x20u);
    native_write(ctx, 0x03cau, ctx->a);
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, 0xfeu, ctx->a);
    return;
    return;
}

void smbneo_native_fn_actor_B_19_proc_bb90(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xbb90u);
    ctx->x = native_nz(ctx, 0x05u);
    native_write(ctx, 0x08u, ctx->x);
    ctx->a = native_nz(ctx, native_read(ctx, 0x23u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lbbf5;
    }
    ctx->a = native_asl(ctx, ctx->a);
    if (!(ctx->status & NATIVE_C)) {
        goto Lbbbe;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x0747u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lbbe3;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x39u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lbbb5;
    }
    native_cmp(ctx, ctx->a, 0x03u);
    if ((ctx->status & NATIVE_Z)) {
        goto Lbbb5;
    }
    native_cmp(ctx, ctx->a, 0x02u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lbbe3;
    }
    smbneo_native_fn_actor_A_0E_proc_ca3c(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_col_actor_bounce_proc_e0b9(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    /* static tail transfer */
    goto Lbbe3;
Lbbb5: ;
    smbneo_native_fn_actor_A_00_proc_c9ba(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_col_actor_ground_proc_df17(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    /* static tail transfer */
    goto Lbbe3;
Lbbbe: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x09u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x03u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lbbdd;
    }
    native_write(ctx, 0xd4u, native_nz(ctx, (uint8_t)(native_read(ctx, 0xd4u) - 1u)));
    ctx->a = native_nz(ctx, native_read(ctx, 0x23u));
    native_write(ctx, 0x23u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x23u) + 1u)));
    native_cmp(ctx, ctx->a, 0x11u);
    if (!(ctx->status & NATIVE_C)) {
        goto Lbbdd;
    }
    ctx->a = native_nz(ctx, 0x10u);
    native_write(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x80u);
    native_write(ctx, 0x23u, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    native_write(ctx, 0x03cau, ctx->a);
    ctx->a = native_rol(ctx, ctx->a);
    native_write(ctx, (uint16_t)(uint8_t)(0x46u + ctx->x), ctx->a);
Lbbdd: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x23u));
    native_cmp(ctx, ctx->a, 0x06u);
    if (!(ctx->status & NATIVE_C)) {
        goto Lbbf5;
    }
Lbbe3: ;
    smbneo_native_fn_pos_calc_x_rel_actor_f0b7(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_pos_bits_get_actor_f114(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_col_actor_box_e199(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_render_powerup_e62f(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_col_player_actor_proc_d7aa(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_actor_oob_proc_d5bd(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Lbbf5: ;
    return;
    return;
}

void smbneo_native_fn_block_punch_bbf8(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xbbf8u);
    uint8_t saved_value_0 = 0u;
    saved_value_0 = ctx->a;
    ctx->a = native_nz(ctx, 0x11u);
    ctx->x = native_nz(ctx, native_read(ctx, 0x03eeu));
    ctx->y = native_nz(ctx, native_read(ctx, 0x0754u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lbc05;
    }
    ctx->a = native_nz(ctx, 0x12u);
Lbc05: ;
    native_write(ctx, (uint16_t)(uint8_t)(0x26u + ctx->x), ctx->a);
    smbneo_native_fn_meta_destroy_8c6c(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->x = native_nz(ctx, native_read(ctx, 0x03eeu));
    ctx->a = native_nz(ctx, native_read(ctx, 0x02u));
    native_write(ctx, (uint16_t)(0x03e4u + ctx->x), ctx->a);
    ctx->y = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x06u));
    native_write(ctx, (uint16_t)(0x03e6u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0x06u) + ctx->y)));
    smbneo_native_fn_block_is_tile_bd05(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_write(ctx, 0x00u, ctx->a);
    ctx->y = native_nz(ctx, native_read(ctx, 0x0754u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lbc25;
    }
    ctx->a = native_nz(ctx, ctx->y);
Lbc25: ;
    if (!(ctx->status & NATIVE_C)) {
        goto Lbc4c;
    }
    ctx->y = native_nz(ctx, 0x11u);
    native_write(ctx, (uint16_t)(uint8_t)(0x26u + ctx->x), ctx->y);
    ctx->a = native_nz(ctx, 0xc4u);
    ctx->y = native_nz(ctx, native_read(ctx, 0x00u));
    native_cmp(ctx, ctx->y, 0x58u);
    if ((ctx->status & NATIVE_Z)) {
        goto Lbc37;
    }
    native_cmp(ctx, ctx->y, 0x5du);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lbc4c;
    }
Lbc37: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x06bcu));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lbc44;
    }
    ctx->a = native_nz(ctx, 0x0bu);
    native_write(ctx, 0x079du, ctx->a);
    native_write(ctx, 0x06bcu, native_nz(ctx, (uint8_t)(native_read(ctx, 0x06bcu) + 1u)));
Lbc44: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x079du));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lbc4b;
    }
    ctx->y = native_nz(ctx, 0xc4u);
Lbc4b: ;
    ctx->a = native_nz(ctx, ctx->y);
Lbc4c: ;
    native_write(ctx, (uint16_t)(0x03e8u + ctx->x), ctx->a);
    smbneo_native_fn_block_initpos_bc8f(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->y = native_nz(ctx, native_read(ctx, 0x02u));
    ctx->a = native_nz(ctx, 0x23u);
    native_write(ctx, (uint16_t)(native_zpword(ctx, 0x06u) + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, 0x10u);
    native_write(ctx, 0x0784u, ctx->a);
    ctx->a = native_nz(ctx, saved_value_0);
    native_write(ctx, 0x05u, ctx->a);
    ctx->y = native_nz(ctx, 0x00u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0714u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lbc6c;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x0754u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lbc6d;
    }
Lbc6c: ;
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
Lbc6d: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0xceu));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, (uint16_t)(0xbbf6u + ctx->y)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0xf0u));
    native_write(ctx, (uint16_t)(uint8_t)(0xd7u + ctx->x), ctx->a);
    ctx->y = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x26u + ctx->x)));
    native_cmp(ctx, ctx->y, 0x11u);
    if ((ctx->status & NATIVE_Z)) {
        goto Lbc83;
    }
    smbneo_native_fn_block_break_bd11(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    /* static tail transfer */
    goto Lbc86;
Lbc83: ;
    smbneo_native_fn_block_item_bca6(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Lbc86: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x03eeu));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a ^ 0x01u));
    native_write(ctx, 0x03eeu, ctx->a);
    return;
    return;
}

void smbneo_native_fn_block_initpos_bc8f(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xbc8fu);
    ctx->a = native_nz(ctx, native_read(ctx, 0x86u));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x08u);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0xf0u));
    native_write(ctx, (uint16_t)(uint8_t)(0x8fu + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x6du));
    native_adc(ctx, 0x00u);
    native_write(ctx, (uint16_t)(uint8_t)(0x76u + ctx->x), ctx->a);
    native_write(ctx, (uint16_t)(0x03eau + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0xb5u));
    native_write(ctx, (uint16_t)(uint8_t)(0xbeu + ctx->x), ctx->a);
    return;
    return;
}

void smbneo_native_fn_block_item_bca6(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xbca6u);
    smbneo_native_fn_block_top_check_bd2e(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, 0xffu, ctx->a);
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, (uint16_t)(uint8_t)(0x60u + ctx->x), ctx->a);
    native_write(ctx, (uint16_t)(0x043cu + ctx->x), ctx->a);
    native_write(ctx, 0x9fu, ctx->a);
    ctx->a = native_nz(ctx, 0xfeu);
    native_write(ctx, (uint16_t)(uint8_t)(0xa8u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x05u));
    smbneo_native_fn_block_is_tile_bd05(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    if (!(ctx->status & NATIVE_C)) {
        goto Lbcf6;
    }
    ctx->a = native_nz(ctx, ctx->y);
    native_cmp(ctx, ctx->a, 0x09u);
    if (!(ctx->status & NATIVE_C)) {
        goto Lbcc8;
    }
    native_sbc(ctx, 0x05u);
Lbcc8: ;
    uint8_t table_selector_bcc8 = ctx->a;
    ctx->a = native_asl(ctx, ctx->a);
    ctx->y = native_nz(ctx, ctx->a);
    native_write(ctx, 0x04u, 0xcau);
    native_write(ctx, 0x05u, 0xbcu);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    switch (table_selector_bcc8) {
    case 0: native_write(ctx, 0x06u, 0xddu);
             native_write(ctx, 0x07u, 0xbcu);
             ctx->a = native_nz(ctx, 0xbcu);
             smbneo_native_fn_block_item_end_bcdd(ctx); return;
    case 1: native_write(ctx, 0x06u, 0x2cu);
             native_write(ctx, 0x07u, 0xbau);
             ctx->a = native_nz(ctx, 0xbau);
             smbneo_native_fn_misc_init_coin_a_ba2c(ctx); return;
    case 2: native_write(ctx, 0x06u, 0x2cu);
             native_write(ctx, 0x07u, 0xbau);
             ctx->a = native_nz(ctx, 0xbau);
             smbneo_native_fn_misc_init_coin_a_ba2c(ctx); return;
    case 3: native_write(ctx, 0x06u, 0xe7u);
             native_write(ctx, 0x07u, 0xbcu);
             ctx->a = native_nz(ctx, 0xbcu);
             smbneo_native_fn_block_mushroom_oneup_bce7(ctx); return;
    case 4: native_write(ctx, 0x06u, 0xddu);
             native_write(ctx, 0x07u, 0xbcu);
             ctx->a = native_nz(ctx, 0xbcu);
             smbneo_native_fn_block_item_end_bcdd(ctx); return;
    case 5: native_write(ctx, 0x06u, 0xeeu);
             native_write(ctx, 0x07u, 0xbcu);
             ctx->a = native_nz(ctx, 0xbcu);
             smbneo_native_fn_block_vine_bcee(ctx); return;
    case 6: native_write(ctx, 0x06u, 0xe2u);
             native_write(ctx, 0x07u, 0xbcu);
             ctx->a = native_nz(ctx, 0xbcu);
             smbneo_native_fn_block_star_bce2(ctx); return;
    case 7: native_write(ctx, 0x06u, 0x2cu);
             native_write(ctx, 0x07u, 0xbau);
             ctx->a = native_nz(ctx, 0xbau);
             smbneo_native_fn_misc_init_coin_a_ba2c(ctx); return;
    case 8: native_write(ctx, 0x06u, 0xe7u);
             native_write(ctx, 0x07u, 0xbcu);
             ctx->a = native_nz(ctx, 0xbcu);
             smbneo_native_fn_block_mushroom_oneup_bce7(ctx); return;
    default: ctx->unsupported = 1; return;
    }
Lbcf6: ;
    return;
    return;
}

void smbneo_native_fn_block_item_end_bcdd(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xbcddu);
    goto Lbcdd;
Lbb54: ;
    ctx->a = native_nz(ctx, 0x2eu);
    native_write(ctx, 0x1bu, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x76u + ctx->x)));
    native_write(ctx, 0x73u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x8fu + ctx->x)));
    native_write(ctx, 0x8cu, ctx->a);
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, 0xbbu, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xd7u + ctx->x)));
    ctx->status |= NATIVE_C;
    native_sbc(ctx, 0x08u);
    native_write(ctx, 0xd4u, ctx->a);
    smbneo_native_fn_actor_B_19_init_bb6b(ctx);
    return;
Lbcdd: ;
    ctx->a = native_nz(ctx, 0x00u);
    /* static tail transfer */
    goto Lbce9;
Lbce9: ;
    native_write(ctx, 0x39u, ctx->a);
    /* static tail transfer */
    goto Lbb54;
    return;
}

void smbneo_native_fn_block_star_bce2(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xbce2u);
    goto Lbce2;
Lbb54: ;
    ctx->a = native_nz(ctx, 0x2eu);
    native_write(ctx, 0x1bu, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x76u + ctx->x)));
    native_write(ctx, 0x73u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x8fu + ctx->x)));
    native_write(ctx, 0x8cu, ctx->a);
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, 0xbbu, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xd7u + ctx->x)));
    ctx->status |= NATIVE_C;
    native_sbc(ctx, 0x08u);
    native_write(ctx, 0xd4u, ctx->a);
    smbneo_native_fn_actor_B_19_init_bb6b(ctx);
    return;
Lbce2: ;
    ctx->a = native_nz(ctx, 0x02u);
    /* static tail transfer */
    goto Lbce9;
Lbce9: ;
    native_write(ctx, 0x39u, ctx->a);
    /* static tail transfer */
    goto Lbb54;
    return;
}

void smbneo_native_fn_block_mushroom_oneup_bce7(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xbce7u);
    goto Lbce7;
Lbb54: ;
    ctx->a = native_nz(ctx, 0x2eu);
    native_write(ctx, 0x1bu, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x76u + ctx->x)));
    native_write(ctx, 0x73u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x8fu + ctx->x)));
    native_write(ctx, 0x8cu, ctx->a);
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, 0xbbu, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xd7u + ctx->x)));
    ctx->status |= NATIVE_C;
    native_sbc(ctx, 0x08u);
    native_write(ctx, 0xd4u, ctx->a);
    smbneo_native_fn_actor_B_19_init_bb6b(ctx);
    return;
Lbce7: ;
    ctx->a = native_nz(ctx, 0x03u);
    native_write(ctx, 0x39u, ctx->a);
    /* static tail transfer */
    goto Lbb54;
    return;
}

void smbneo_native_fn_block_vine_bcee(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xbceeu);
    ctx->x = native_nz(ctx, 0x05u);
    ctx->y = native_nz(ctx, native_read(ctx, 0x03eeu));
    smbneo_native_fn_actor_B_1A_init_b7d2(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    return;
    return;
}

void smbneo_native_fn_block_is_tile_bd05(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xbd05u);
    ctx->y = native_nz(ctx, 0x0du);
Lbd07: ;
    native_cmp(ctx, ctx->a, native_read(ctx, (uint16_t)(0xbcf7u + ctx->y)));
    if ((ctx->status & NATIVE_Z)) {
        goto Lbd10;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    if (!(ctx->status & NATIVE_N)) {
        goto Lbd07;
    }
    ctx->status &= (uint8_t)~NATIVE_C;
Lbd10: ;
    return;
    return;
}

void smbneo_native_fn_block_break_bd11(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xbd11u);
    smbneo_native_fn_block_top_check_bd2e(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, (uint16_t)(0x03ecu + ctx->x), ctx->a);
    native_write(ctx, 0xfdu, ctx->a);
    smbneo_native_fn_block_bits_init_bd50(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, 0xfeu);
    native_write(ctx, 0x9fu, ctx->a);
    ctx->a = native_nz(ctx, 0x05u);
    native_write(ctx, 0x0139u, ctx->a);
    smbneo_native_fn_misc_proc_coin_update_score_bb32(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->x = native_nz(ctx, native_read(ctx, 0x03eeu));
    return;
    return;
}

void smbneo_native_fn_block_top_check_bd2e(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xbd2eu);
    ctx->x = native_nz(ctx, native_read(ctx, 0x03eeu));
    ctx->y = native_nz(ctx, native_read(ctx, 0x02u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lbd4f;
    }
    ctx->a = native_nz(ctx, ctx->y);
    ctx->status |= NATIVE_C;
    native_sbc(ctx, 0x10u);
    native_write(ctx, 0x02u, ctx->a);
    ctx->y = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0x06u) + ctx->y)));
    native_cmp(ctx, ctx->a, 0xc2u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lbd4f;
    }
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, (uint16_t)(native_zpword(ctx, 0x06u) + ctx->y), ctx->a);
    smbneo_native_fn_meta_put_blank_8c4e(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->x = native_nz(ctx, native_read(ctx, 0x03eeu));
    smbneo_native_fn_misc_init_coin_b_ba45(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Lbd4f: ;
    return;
    return;
}

void smbneo_native_fn_block_bits_init_bd50(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xbd50u);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x8fu + ctx->x)));
    native_write(ctx, (uint16_t)(0x03f1u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0xf0u);
    native_write(ctx, (uint16_t)(uint8_t)(0x60u + ctx->x), ctx->a);
    native_write(ctx, (uint16_t)(uint8_t)(0x62u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0xfau);
    native_write(ctx, (uint16_t)(uint8_t)(0xa8u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0xfcu);
    native_write(ctx, (uint16_t)(uint8_t)(0xaau + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, (uint16_t)(0x043cu + ctx->x), ctx->a);
    native_write(ctx, (uint16_t)(0x043eu + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x76u + ctx->x)));
    native_write(ctx, (uint16_t)(uint8_t)(0x78u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x8fu + ctx->x)));
    native_write(ctx, (uint16_t)(uint8_t)(0x91u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xd7u + ctx->x)));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x08u);
    native_write(ctx, (uint16_t)(uint8_t)(0xd9u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0xfau);
    native_write(ctx, (uint16_t)(uint8_t)(0xa8u + ctx->x), ctx->a);
    return;
    return;
}

void smbneo_native_fn_block_proc_bd7f(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xbd7fu);
    uint8_t saved_value_0 = 0u;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x26u + ctx->x)));
    if ((ctx->status & NATIVE_Z)) {
        goto Lbde0;
    }
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x0fu));
    saved_value_0 = ctx->a;
    ctx->y = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, ctx->x);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x09u);
    ctx->x = native_nz(ctx, ctx->a);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lbdc2;
    }
    smbneo_native_fn_motion_block_fall_fast_beb5(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_motion_x_do_be1e(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, ctx->x);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x02u);
    ctx->x = native_nz(ctx, ctx->a);
    smbneo_native_fn_motion_block_fall_fast_beb5(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_motion_x_do_be1e(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    smbneo_native_fn_pos_calc_x_rel_block_f0be(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_pos_bits_get_block_f11b(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_render_block_bits_ebb0(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, saved_value_0);
    ctx->y = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xbeu + ctx->x)));
    if ((ctx->status & NATIVE_Z)) {
        goto Lbde0;
    }
    saved_value_0 = ctx->a;
    ctx->a = native_nz(ctx, 0xf0u);
    native_cmp(ctx, ctx->a, native_read(ctx, (uint16_t)(uint8_t)(0xd9u + ctx->x)));
    if ((ctx->status & NATIVE_C)) {
        goto Lbdb9;
    }
    native_write(ctx, (uint16_t)(uint8_t)(0xd9u + ctx->x), ctx->a);
Lbdb9: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xd7u + ctx->x)));
    native_cmp(ctx, ctx->a, 0xf0u);
    ctx->a = native_nz(ctx, saved_value_0);
    if (!(ctx->status & NATIVE_C)) {
        goto Lbde0;
    }
    if ((ctx->status & NATIVE_C)) {
        goto Lbdde;
    }
Lbdc2: ;
    smbneo_native_fn_motion_block_fall_fast_beb5(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    smbneo_native_fn_pos_calc_x_rel_block_f0be(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_pos_bits_get_block_f11b(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_render_block_eb2e(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xd7u + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x0fu));
    native_cmp(ctx, ctx->a, 0x05u);
    ctx->a = native_nz(ctx, saved_value_0);
    if ((ctx->status & NATIVE_C)) {
        goto Lbde0;
    }
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, (uint16_t)(0x03ecu + ctx->x), ctx->a);
Lbdde: ;
    ctx->a = native_nz(ctx, 0x00u);
Lbde0: ;
    native_write(ctx, (uint16_t)(uint8_t)(0x26u + ctx->x), ctx->a);
    return;
    return;
}

void smbneo_native_fn_block_flatten_bde3(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xbde3u);
    ctx->x = native_nz(ctx, 0x01u);
Lbde5: ;
    native_write(ctx, 0x08u, ctx->x);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0301u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lbe0d;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x03ecu + ctx->x)));
    if ((ctx->status & NATIVE_Z)) {
        goto Lbe0d;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x03e6u + ctx->x)));
    native_write(ctx, 0x06u, ctx->a);
    ctx->a = native_nz(ctx, 0x05u);
    native_write(ctx, 0x07u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x03e4u + ctx->x)));
    native_write(ctx, 0x02u, ctx->a);
    ctx->y = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x03e8u + ctx->x)));
    native_write(ctx, (uint16_t)(native_zpword(ctx, 0x06u) + ctx->y), ctx->a);
    smbneo_native_fn_meta_replace_8c62(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, (uint16_t)(0x03ecu + ctx->x), ctx->a);
Lbe0d: ;
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x - 1u));
    if (!(ctx->status & NATIVE_N)) {
        goto Lbde5;
    }
    return;
    return;
}

void smbneo_native_fn_motion_x_be11(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xbe11u);
#ifdef SMBNEO_NATIVE_FAST_PATHS
    vs_native_fast_motion_x(ctx);
    return;
#endif
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    smbneo_native_fn_motion_x_do_be1e(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    return;
    return;
}

void smbneo_native_fn_motion_x_player_be18(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xbe18u);
#ifdef SMBNEO_NATIVE_FAST_PATHS
    vs_native_fast_motion_x_player(ctx);
    return;
#endif
    ctx->a = native_nz(ctx, native_read(ctx, 0x070eu));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lbe5b;
    }
    ctx->x = native_nz(ctx, ctx->a);
    smbneo_native_fn_motion_x_do_be1e(ctx);
    return;
Lbe5b: ;
    return;
    return;
}

void smbneo_native_fn_motion_x_do_be1e(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xbe1eu);
#ifdef SMBNEO_NATIVE_FAST_PATHS
    vs_native_fast_motion_x_do(ctx);
    return;
#endif
    uint8_t saved_value_0 = 0u;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x57u + ctx->x)));
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    native_write(ctx, 0x01u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x57u + ctx->x)));
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    native_cmp(ctx, ctx->a, 0x08u);
    if (!(ctx->status & NATIVE_C)) {
        goto Lbe32;
    }
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | 0xf0u));
Lbe32: ;
    native_write(ctx, 0x00u, ctx->a);
    ctx->y = native_nz(ctx, 0x00u);
    native_cmp(ctx, ctx->a, 0x00u);
    if (!(ctx->status & NATIVE_N)) {
        goto Lbe3b;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
Lbe3b: ;
    native_write(ctx, 0x02u, ctx->y);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0400u + ctx->x)));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, 0x01u));
    native_write(ctx, (uint16_t)(0x0400u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x00u);
    ctx->a = native_rol(ctx, ctx->a);
    saved_value_0 = ctx->a;
    ctx->a = native_ror(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x86u + ctx->x)));
    native_adc(ctx, native_read(ctx, 0x00u));
    native_write(ctx, (uint16_t)(uint8_t)(0x86u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x6du + ctx->x)));
    native_adc(ctx, native_read(ctx, 0x02u));
    native_write(ctx, (uint16_t)(uint8_t)(0x6du + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, saved_value_0);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, 0x00u));
    return;
    return;
}

void smbneo_native_fn_motion_y_be72(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xbe72u);
    ctx->y = native_nz(ctx, 0x3du);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x)));
    native_cmp(ctx, ctx->a, 0x05u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lbe7c;
    }
    smbneo_native_fn_motion_y_state_5_be7a(ctx);
    return;
Lbe7c: ;
    /* static tail transfer */
    goto Lbea3;
Lbea3: ;
    ctx->a = native_nz(ctx, 0x03u);
    smbneo_native_fn_motion_fall_bea5(ctx);
    return;
    return;
}

void smbneo_native_fn_motion_y_state_5_be7a(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xbe7au);
    ctx->y = native_nz(ctx, 0x20u);
    /* static tail transfer */
    goto Lbea3;
Lbea3: ;
    ctx->a = native_nz(ctx, 0x03u);
    smbneo_native_fn_motion_fall_bea5(ctx);
    return;
    return;
}

void smbneo_native_fn_motion_fall_fast_be97(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xbe97u);
#ifdef SMBNEO_NATIVE_FAST_PATHS
    vs_native_fast_motion_fall_fast(ctx);
    return;
#endif
    ctx->y = native_nz(ctx, 0x7fu);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lbe9d;
    }
    smbneo_native_fn_motion_fall_slow_be9b(ctx);
    return;
Lbe9d: ;
    ctx->a = native_nz(ctx, 0x02u);
    if (!(ctx->status & NATIVE_Z)) {
        smbneo_native_fn_motion_fall_bea5(ctx);
        return;
    }
    smbneo_native_fn_motion_fall_mid_bea1(ctx);
    return;
    return;
}

void smbneo_native_fn_motion_fall_slow_be9b(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xbe9bu);
#ifdef SMBNEO_NATIVE_FAST_PATHS
    vs_native_fast_motion_fall_slow(ctx);
    return;
#endif
    ctx->y = native_nz(ctx, 0x0fu);
    ctx->a = native_nz(ctx, 0x02u);
    if (!(ctx->status & NATIVE_Z)) {
        smbneo_native_fn_motion_fall_bea5(ctx);
        return;
    }
    smbneo_native_fn_motion_fall_mid_bea1(ctx);
    return;
    return;
}

void smbneo_native_fn_motion_fall_mid_bea1(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xbea1u);
#ifdef SMBNEO_NATIVE_FAST_PATHS
    vs_native_fast_motion_fall_mid(ctx);
    return;
#endif
    ctx->y = native_nz(ctx, 0x1cu);
    ctx->a = native_nz(ctx, 0x03u);
    smbneo_native_fn_motion_fall_bea5(ctx);
    return;
    return;
}

void smbneo_native_fn_motion_fall_bea5(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xbea5u);
#ifdef SMBNEO_NATIVE_FAST_PATHS
    vs_native_fast_motion_fall(ctx);
    return;
#endif
    native_write(ctx, 0x00u, ctx->y);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    smbneo_native_fn_motion_gravity_do_bebe(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    return;
    return;
}

void smbneo_native_fn_motion_block_fall_slow_beb0(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xbeb0u);
    ctx->y = native_nz(ctx, 0x00u);
    /* static tail transfer */
    goto Lbeb7;
Lbeb7: ;
    ctx->a = native_nz(ctx, 0x50u);
    native_write(ctx, 0x00u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xbeaeu + ctx->y)));
    smbneo_native_fn_motion_gravity_do_bebe(ctx);
    return;
    return;
}

void smbneo_native_fn_motion_block_fall_fast_beb5(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xbeb5u);
    ctx->y = native_nz(ctx, 0x01u);
    ctx->a = native_nz(ctx, 0x50u);
    native_write(ctx, 0x00u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xbeaeu + ctx->y)));
    smbneo_native_fn_motion_gravity_do_bebe(ctx);
    return;
    return;
}

void smbneo_native_fn_motion_gravity_do_bebe(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xbebeu);
#ifdef SMBNEO_NATIVE_FAST_PATHS
    vs_native_fast_motion_gravity_do(ctx);
    return;
#endif
    native_write(ctx, 0x02u, ctx->a);
    ctx->a = native_nz(ctx, 0x00u);
    /* static tail transfer */
    smbneo_native_fn_motion_gravity_beea(ctx);
    return;
    return;
}

void smbneo_native_fn_motion_platform_down_bec5(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xbec5u);
    uint8_t saved_value_0 = 0u;
    ctx->a = native_nz(ctx, 0x00u);
    /* static tail transfer */
    goto Lbecc;
Lbecc: ;
    saved_value_0 = ctx->a;
    ctx->y = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x16u + ctx->x)));
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    ctx->a = native_nz(ctx, 0x05u);
    native_cmp(ctx, ctx->y, 0x29u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lbed8;
    }
    ctx->a = native_nz(ctx, 0x09u);
Lbed8: ;
    native_write(ctx, 0x00u, ctx->a);
    ctx->a = native_nz(ctx, 0x0au);
    native_write(ctx, 0x01u, ctx->a);
    ctx->a = native_nz(ctx, 0x03u);
    native_write(ctx, 0x02u, ctx->a);
    ctx->a = native_nz(ctx, saved_value_0);
    ctx->y = native_nz(ctx, ctx->a);
    smbneo_native_fn_motion_gravity_beea(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    return;
    return;
}

void smbneo_native_fn_motion_platform_up_beca(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xbecau);
    uint8_t saved_value_0 = 0u;
    ctx->a = native_nz(ctx, 0x01u);
    saved_value_0 = ctx->a;
    ctx->y = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x16u + ctx->x)));
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    ctx->a = native_nz(ctx, 0x05u);
    native_cmp(ctx, ctx->y, 0x29u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lbed8;
    }
    ctx->a = native_nz(ctx, 0x09u);
Lbed8: ;
    native_write(ctx, 0x00u, ctx->a);
    ctx->a = native_nz(ctx, 0x0au);
    native_write(ctx, 0x01u, ctx->a);
    ctx->a = native_nz(ctx, 0x03u);
    native_write(ctx, 0x02u, ctx->a);
    ctx->a = native_nz(ctx, saved_value_0);
    ctx->y = native_nz(ctx, ctx->a);
    smbneo_native_fn_motion_gravity_beea(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    return;
    return;
}

void smbneo_native_fn_motion_gravity_beea(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xbeeau);
#ifdef SMBNEO_NATIVE_FAST_PATHS
    vs_native_fast_motion_gravity(ctx);
    return;
#endif
    uint8_t saved_value_0 = 0u;
    saved_value_0 = ctx->a;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0416u + ctx->x)));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, (uint16_t)(0x0433u + ctx->x)));
    native_write(ctx, (uint16_t)(0x0416u + ctx->x), ctx->a);
    ctx->y = native_nz(ctx, 0x00u);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x9fu + ctx->x)));
    if (!(ctx->status & NATIVE_N)) {
        goto Lbefc;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
Lbefc: ;
    native_write(ctx, 0x07u, ctx->y);
    native_adc(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xceu + ctx->x)));
    native_write(ctx, (uint16_t)(uint8_t)(0xceu + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xb5u + ctx->x)));
    native_adc(ctx, native_read(ctx, 0x07u));
    native_write(ctx, (uint16_t)(uint8_t)(0xb5u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0433u + ctx->x)));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, 0x00u));
    native_write(ctx, (uint16_t)(0x0433u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x9fu + ctx->x)));
    native_adc(ctx, 0x00u);
    native_write(ctx, (uint16_t)(uint8_t)(0x9fu + ctx->x), ctx->a);
    native_cmp(ctx, ctx->a, native_read(ctx, 0x02u));
    if ((ctx->status & NATIVE_N)) {
        goto Lbf2b;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0433u + ctx->x)));
    native_cmp(ctx, ctx->a, 0x80u);
    if (!(ctx->status & NATIVE_C)) {
        goto Lbf2b;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x02u));
    native_write(ctx, (uint16_t)(uint8_t)(0x9fu + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, (uint16_t)(0x0433u + ctx->x), ctx->a);
Lbf2b: ;
    ctx->a = native_nz(ctx, saved_value_0);
    if ((ctx->status & NATIVE_Z)) {
        goto Lbf59;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x02u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a ^ 0xffu));
    ctx->y = native_nz(ctx, ctx->a);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    native_write(ctx, 0x07u, ctx->y);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0433u + ctx->x)));
    ctx->status |= NATIVE_C;
    native_sbc(ctx, native_read(ctx, 0x01u));
    native_write(ctx, (uint16_t)(0x0433u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x9fu + ctx->x)));
    native_sbc(ctx, 0x00u);
    native_write(ctx, (uint16_t)(uint8_t)(0x9fu + ctx->x), ctx->a);
    native_cmp(ctx, ctx->a, native_read(ctx, 0x07u));
    if (!(ctx->status & NATIVE_N)) {
        goto Lbf59;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0433u + ctx->x)));
    native_cmp(ctx, ctx->a, 0x80u);
    if ((ctx->status & NATIVE_C)) {
        goto Lbf59;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x07u));
    native_write(ctx, (uint16_t)(uint8_t)(0x9fu + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0xffu);
    native_write(ctx, (uint16_t)(0x0433u + ctx->x), ctx->a);
Lbf59: ;
    return;
    return;
}

void smbneo_native_fn_actor_proc_bf60(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xbf60u);
    uint8_t saved_value_0 = 0u;
    uint8_t saved_value_1 = 0u;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x0fu + ctx->x)));
    saved_value_0 = ctx->a;
    ctx->a = native_asl(ctx, ctx->a);
    if ((ctx->status & NATIVE_C)) {
        goto Lbf78;
    }
    ctx->a = native_nz(ctx, saved_value_0);
    if ((ctx->status & NATIVE_Z)) {
        goto Lbf6c;
    }
    /* static tail transfer */
    goto Lc7c5;
Lbf6c: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x071fu));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x07u));
    native_cmp(ctx, ctx->a, 0x07u);
    if ((ctx->status & NATIVE_Z)) {
        goto Lbf83;
    }
    /* static tail transfer */
    goto Lbfea;
Lbf78: ;
    ctx->a = native_nz(ctx, saved_value_0);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x0fu));
    ctx->y = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x000fu + ctx->y)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lbf83;
    }
    native_write(ctx, (uint16_t)(uint8_t)(0x0fu + ctx->x), ctx->a);
Lbf83: ;
    return;
Lbfea: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x0745u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lc04d;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x0726u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lc04d;
    }
    ctx->y = native_nz(ctx, 0x0bu);
Lbff6: ;
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    if ((ctx->status & NATIVE_N)) {
        goto Lc04d;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x075fu));
    native_cmp(ctx, ctx->a, native_read(ctx, (uint16_t)(0xbf84u + ctx->y)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lbff6;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x0725u));
    native_cmp(ctx, ctx->a, native_read(ctx, (uint16_t)(0xbf8fu + ctx->y)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lbff6;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0xceu));
    native_cmp(ctx, ctx->a, native_read(ctx, (uint16_t)(0xbf9au + ctx->y)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lc033;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x1du));
    native_cmp(ctx, ctx->a, 0x00u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lc033;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x075fu));
    native_cmp(ctx, ctx->a, 0x06u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lc040;
    }
    native_write(ctx, 0x06d9u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x06d9u) + 1u)));
Lc020: ;
    native_write(ctx, 0x06dau, native_nz(ctx, (uint8_t)(native_read(ctx, 0x06dau) + 1u)));
    ctx->a = native_nz(ctx, native_read(ctx, 0x06dau));
    native_cmp(ctx, ctx->a, 0x03u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lc048;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x06d9u));
    native_cmp(ctx, ctx->a, 0x03u);
    if ((ctx->status & NATIVE_Z)) {
        goto Lc040;
    }
    if (!(ctx->status & NATIVE_Z)) {
        goto Lc03a;
    }
Lc033: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x075fu));
    native_cmp(ctx, ctx->a, 0x06u);
    if ((ctx->status & NATIVE_Z)) {
        goto Lc020;
    }
Lc03a: ;
    smbneo_native_fn_actor_area_loop_bfa5(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_actor_killall_cfb4(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Lc040: ;
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x06dau, ctx->a);
    native_write(ctx, 0x06d9u, ctx->a);
Lc048: ;
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x0745u, ctx->a);
Lc04d: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x06cdu));
    if ((ctx->status & NATIVE_Z)) {
        goto Lc062;
    }
    native_write(ctx, (uint16_t)(uint8_t)(0x16u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, (uint16_t)(uint8_t)(0x0fu + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x), ctx->a);
    native_write(ctx, 0x06cdu, ctx->a);
    /* static tail transfer */
    smbneo_native_fn_actor_init_c160(ctx);
    return;
Lc062: ;
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, 0x4016u, ctx->a);
    ctx->y = native_nz(ctx, native_read(ctx, 0x0739u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0xe9u) + ctx->y)));
    native_cmp(ctx, ctx->a, 0xffu);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lc073;
    }
    /* static tail transfer */
    goto Lc150;
Lc073: ;
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x0fu));
    native_cmp(ctx, ctx->a, 0x0eu);
    if ((ctx->status & NATIVE_Z)) {
        goto Lc08c;
    }
    native_cmp(ctx, ctx->x, 0x05u);
    if (!(ctx->status & NATIVE_C)) {
        goto Lc08c;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, 0x4016u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0xe9u) + ctx->y)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x3fu));
    native_cmp(ctx, ctx->a, 0x2eu);
    if ((ctx->status & NATIVE_Z)) {
        goto Lc08c;
    }
    return;
Lc08c: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x071du));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x30u);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0xf0u));
    native_write(ctx, 0x07u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x071bu));
    native_adc(ctx, 0x00u);
    native_write(ctx, 0x06u, ctx->a);
    ctx->y = native_nz(ctx, native_read(ctx, 0x0739u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, 0x4016u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0xe9u) + ctx->y)));
    ctx->a = native_asl(ctx, ctx->a);
    if (!(ctx->status & NATIVE_C)) {
        goto Lc0b6;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x073bu));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lc0b6;
    }
    native_write(ctx, 0x073bu, native_nz(ctx, (uint8_t)(native_read(ctx, 0x073bu) + 1u)));
    native_write(ctx, 0x073au, native_nz(ctx, (uint8_t)(native_read(ctx, 0x073au) + 1u)));
Lc0b6: ;
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, 0x4016u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0xe9u) + ctx->y)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x0fu));
    native_cmp(ctx, ctx->a, 0x0fu);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lc0e2;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x073bu));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lc0e2;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, 0x4016u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0xe9u) + ctx->y)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x3fu));
    native_write(ctx, 0x073au, ctx->a);
    native_write(ctx, 0x0739u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0739u) + 1u)));
    native_write(ctx, 0x0739u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0739u) + 1u)));
    native_write(ctx, 0x073bu, native_nz(ctx, (uint8_t)(native_read(ctx, 0x073bu) + 1u)));
    /* static tail transfer */
    goto Lbfea;
Lc0e2: ;
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, 0x4016u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x073au));
    native_write(ctx, (uint16_t)(uint8_t)(0x6eu + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, 0x4016u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0xe9u) + ctx->y)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0xf0u));
    native_write(ctx, (uint16_t)(uint8_t)(0x87u + ctx->x), ctx->a);
    native_cmp(ctx, ctx->a, native_read(ctx, 0x071du));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x6eu + ctx->x)));
    native_sbc(ctx, native_read(ctx, 0x071bu));
    if ((ctx->status & NATIVE_C)) {
        goto Lc111;
    }
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, 0x4016u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0xe9u) + ctx->y)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x0fu));
    native_cmp(ctx, ctx->a, 0x0eu);
    if ((ctx->status & NATIVE_Z)) {
        goto Lc16b;
    }
    /* static tail transfer */
    goto Lc194;
Lc111: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x07u));
    native_cmp(ctx, ctx->a, native_read(ctx, (uint16_t)(uint8_t)(0x87u + ctx->x)));
    ctx->a = native_nz(ctx, native_read(ctx, 0x06u));
    native_sbc(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x6eu + ctx->x)));
    if (!(ctx->status & NATIVE_C)) {
        goto Lc150;
    }
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, (uint16_t)(uint8_t)(0xb6u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, 0x4016u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0xe9u) + ctx->y)));
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    native_write(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x), ctx->a);
    native_cmp(ctx, ctx->a, 0xe0u);
    if ((ctx->status & NATIVE_Z)) {
        goto Lc16b;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, 0x4016u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0xe9u) + ctx->y)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x3fu));
    native_cmp(ctx, ctx->a, 0x37u);
    if (!(ctx->status & NATIVE_C)) {
        goto Lc142;
    }
    native_cmp(ctx, ctx->a, 0x3fu);
    if (!(ctx->status & NATIVE_C)) {
        goto Lc168;
    }
Lc142: ;
    native_write(ctx, (uint16_t)(uint8_t)(0x16u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, (uint16_t)(uint8_t)(0x0fu + ctx->x), ctx->a);
    smbneo_native_fn_actor_init_c160(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x0fu + ctx->x)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lc1a7;
    }
    return;
Lc150: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x06cbu));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lc15e;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x0398u));
    native_cmp(ctx, ctx->a, 0x01u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lc167;
    }
    ctx->a = native_nz(ctx, 0x2fu);
Lc15e: ;
    native_write(ctx, (uint16_t)(uint8_t)(0x16u + ctx->x), ctx->a);
    smbneo_native_fn_actor_init_c160(ctx);
    return;
Lc167: ;
    return;
Lc168: ;
    /* static tail transfer */
    goto Lc65e;
Lc16b: ;
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, 0x4016u, ctx->a);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0xe9u) + ctx->y)));
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    native_cmp(ctx, ctx->a, native_read(ctx, 0x075fu));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lc191;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, 0x4016u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0xe9u) + ctx->y)));
    native_write(ctx, 0x0750u, ctx->a);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0xe9u) + ctx->y)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x1fu));
    native_write(ctx, 0x0751u, ctx->a);
Lc191: ;
    /* static tail transfer */
    goto Lc1a4;
Lc194: ;
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, 0x4016u, ctx->a);
    ctx->y = native_nz(ctx, native_read(ctx, 0x0739u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0xe9u) + ctx->y)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x0fu));
    native_cmp(ctx, ctx->a, 0x0eu);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lc1a7;
    }
Lc1a4: ;
    native_write(ctx, 0x0739u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0739u) + 1u)));
Lc1a7: ;
    native_write(ctx, 0x0739u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0739u) + 1u)));
    native_write(ctx, 0x0739u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0739u) + 1u)));
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x073bu, ctx->a);
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    return;
Lc65e: ;
    ctx->y = native_nz(ctx, 0x00u);
    ctx->status |= NATIVE_C;
    native_sbc(ctx, 0x37u);
    saved_value_0 = ctx->a;
    native_cmp(ctx, ctx->a, 0x04u);
    if ((ctx->status & NATIVE_C)) {
        goto Lc673;
    }
    saved_value_1 = ctx->a;
    ctx->y = native_nz(ctx, 0x06u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x076au));
    if ((ctx->status & NATIVE_Z)) {
        goto Lc672;
    }
    ctx->y = native_nz(ctx, 0x02u);
Lc672: ;
    ctx->a = native_nz(ctx, saved_value_1);
Lc673: ;
    native_write(ctx, 0x01u, ctx->y);
    ctx->y = native_nz(ctx, 0xb0u);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x02u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lc67d;
    }
    ctx->y = native_nz(ctx, 0x70u);
Lc67d: ;
    native_write(ctx, 0x00u, ctx->y);
    ctx->a = native_nz(ctx, native_read(ctx, 0x071bu));
    native_write(ctx, 0x02u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x071du));
    native_write(ctx, 0x03u, ctx->a);
    ctx->y = native_nz(ctx, 0x02u);
    ctx->a = native_nz(ctx, saved_value_0);
    ctx->a = native_lsr(ctx, ctx->a);
    if (!(ctx->status & NATIVE_C)) {
        goto Lc690;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
Lc690: ;
    native_write(ctx, 0x06d3u, ctx->y);
Lc693: ;
    ctx->x = native_nz(ctx, 0xffu);
Lc695: ;
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    native_cmp(ctx, ctx->x, 0x05u);
    if ((ctx->status & NATIVE_C)) {
        goto Lc6c7;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x0fu + ctx->x)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lc695;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x01u));
    native_write(ctx, (uint16_t)(uint8_t)(0x16u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x02u));
    native_write(ctx, (uint16_t)(uint8_t)(0x6eu + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x03u));
    native_write(ctx, (uint16_t)(uint8_t)(0x87u + ctx->x), ctx->a);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x18u);
    native_write(ctx, 0x03u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x02u));
    native_adc(ctx, 0x00u);
    native_write(ctx, 0x02u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x00u));
    native_write(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, (uint16_t)(uint8_t)(0xb6u + ctx->x), ctx->a);
    native_write(ctx, (uint16_t)(uint8_t)(0x0fu + ctx->x), ctx->a);
    smbneo_native_fn_actor_init_do_c1b5(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_write(ctx, 0x06d3u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x06d3u) - 1u)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lc693;
    }
Lc6c7: ;
    /* static tail transfer */
    goto Lc1a7;
Lc7c5: ;
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    ctx->a = native_nz(ctx, 0x00u);
    ctx->y = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x16u + ctx->x)));
    native_cmp(ctx, ctx->y, 0x15u);
    if (!(ctx->status & NATIVE_C)) {
        goto Lc7d2;
    }
    ctx->a = native_nz(ctx, ctx->y);
    native_sbc(ctx, 0x14u);
Lc7d2: ;
    uint8_t table_selector_c7d2 = ctx->a;
    ctx->a = native_asl(ctx, ctx->a);
    ctx->y = native_nz(ctx, ctx->a);
    native_write(ctx, 0x04u, 0xd4u);
    native_write(ctx, 0x05u, 0xc7u);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    switch (table_selector_c7d2) {
    case 0: native_write(ctx, 0x06u, 0x23u);
             native_write(ctx, 0x07u, 0xc8u);
             ctx->a = native_nz(ctx, 0xc8u);
             smbneo_native_fn_actor_proc_enemy_c823(ctx); return;
    case 1: native_write(ctx, 0x06u, 0x78u);
             native_write(ctx, 0x07u, 0xc8u);
             ctx->a = native_nz(ctx, 0xc8u);
             smbneo_native_fn_actor_B_00_proc_c878(ctx); return;
    case 2: native_write(ctx, 0x06u, 0xd8u);
             native_write(ctx, 0x07u, 0xd1u);
             ctx->a = native_nz(ctx, 0xd1u);
             smbneo_native_fn_actor_B_01_proc_d1d8(ctx); return;
    case 3: native_write(ctx, 0x06u, 0x19u);
             native_write(ctx, 0x07u, 0xc8u);
             ctx->a = native_nz(ctx, 0xc8u);
             smbneo_native_fn_actor_B_02_proc_c819(ctx); return;
    case 4: native_write(ctx, 0x06u, 0x19u);
             native_write(ctx, 0x07u, 0xc8u);
             ctx->a = native_nz(ctx, 0xc8u);
             smbneo_native_fn_actor_B_02_proc_c819(ctx); return;
    case 5: native_write(ctx, 0x06u, 0x19u);
             native_write(ctx, 0x07u, 0xc8u);
             ctx->a = native_nz(ctx, 0xc8u);
             smbneo_native_fn_actor_B_02_proc_c819(ctx); return;
    case 6: native_write(ctx, 0x06u, 0x19u);
             native_write(ctx, 0x07u, 0xc8u);
             ctx->a = native_nz(ctx, 0xc8u);
             smbneo_native_fn_actor_B_02_proc_c819(ctx); return;
    case 7: native_write(ctx, 0x06u, 0x8au);
             native_write(ctx, 0x07u, 0xc8u);
             ctx->a = native_nz(ctx, 0xc8u);
             smbneo_native_fn_actor_B_06_proc_c88a(ctx); return;
    case 8: native_write(ctx, 0x06u, 0x8au);
             native_write(ctx, 0x07u, 0xc8u);
             ctx->a = native_nz(ctx, 0xc8u);
             smbneo_native_fn_actor_B_06_proc_c88a(ctx); return;
    case 9: native_write(ctx, 0x06u, 0x8au);
             native_write(ctx, 0x07u, 0xc8u);
             ctx->a = native_nz(ctx, 0xc8u);
             smbneo_native_fn_actor_B_06_proc_c88a(ctx); return;
    case 10: native_write(ctx, 0x06u, 0x8au);
             native_write(ctx, 0x07u, 0xc8u);
             ctx->a = native_nz(ctx, 0xc8u);
             smbneo_native_fn_actor_B_06_proc_c88a(ctx); return;
    case 11: native_write(ctx, 0x06u, 0x8au);
             native_write(ctx, 0x07u, 0xc8u);
             ctx->a = native_nz(ctx, 0xc8u);
             smbneo_native_fn_actor_B_06_proc_c88a(ctx); return;
    case 12: native_write(ctx, 0x06u, 0x8au);
             native_write(ctx, 0x07u, 0xc8u);
             ctx->a = native_nz(ctx, 0xc8u);
             smbneo_native_fn_actor_B_06_proc_c88a(ctx); return;
    case 13: native_write(ctx, 0x06u, 0x8au);
             native_write(ctx, 0x07u, 0xc8u);
             ctx->a = native_nz(ctx, 0xc8u);
             smbneo_native_fn_actor_B_06_proc_c88a(ctx); return;
    case 14: native_write(ctx, 0x06u, 0x8au);
             native_write(ctx, 0x07u, 0xc8u);
             ctx->a = native_nz(ctx, 0xc8u);
             smbneo_native_fn_actor_B_06_proc_c88a(ctx); return;
    case 15: native_write(ctx, 0x06u, 0x19u);
             native_write(ctx, 0x07u, 0xc8u);
             ctx->a = native_nz(ctx, 0xc8u);
             smbneo_native_fn_actor_B_02_proc_c819(ctx); return;
    case 16: native_write(ctx, 0x06u, 0xa8u);
             native_write(ctx, 0x07u, 0xc8u);
             ctx->a = native_nz(ctx, 0xc8u);
             smbneo_native_fn_actor_B_0F_proc_c8a8(ctx); return;
    case 17: native_write(ctx, 0x06u, 0xa8u);
             native_write(ctx, 0x07u, 0xc8u);
             ctx->a = native_nz(ctx, 0xc8u);
             smbneo_native_fn_actor_B_0F_proc_c8a8(ctx); return;
    case 18: native_write(ctx, 0x06u, 0xa8u);
             native_write(ctx, 0x07u, 0xc8u);
             ctx->a = native_nz(ctx, 0xc8u);
             smbneo_native_fn_actor_B_0F_proc_c8a8(ctx); return;
    case 19: native_write(ctx, 0x06u, 0xa8u);
             native_write(ctx, 0x07u, 0xc8u);
             ctx->a = native_nz(ctx, 0xc8u);
             smbneo_native_fn_actor_B_0F_proc_c8a8(ctx); return;
    case 20: native_write(ctx, 0x06u, 0xa8u);
             native_write(ctx, 0x07u, 0xc8u);
             ctx->a = native_nz(ctx, 0xc8u);
             smbneo_native_fn_actor_B_0F_proc_c8a8(ctx); return;
    case 21: native_write(ctx, 0x06u, 0xa8u);
             native_write(ctx, 0x07u, 0xc8u);
             ctx->a = native_nz(ctx, 0xc8u);
             smbneo_native_fn_actor_B_0F_proc_c8a8(ctx); return;
    case 22: native_write(ctx, 0x06u, 0xa8u);
             native_write(ctx, 0x07u, 0xc8u);
             ctx->a = native_nz(ctx, 0xc8u);
             smbneo_native_fn_actor_B_0F_proc_c8a8(ctx); return;
    case 23: native_write(ctx, 0x06u, 0x90u);
             native_write(ctx, 0x07u, 0xc8u);
             ctx->a = native_nz(ctx, 0xc8u);
             smbneo_native_fn_actor_B_16_proc_c890(ctx); return;
    case 24: native_write(ctx, 0x06u, 0x90u);
             native_write(ctx, 0x07u, 0xc8u);
             ctx->a = native_nz(ctx, 0xc8u);
             smbneo_native_fn_actor_B_16_proc_c890(ctx); return;
    case 25: native_write(ctx, 0x06u, 0xa8u);
             native_write(ctx, 0x07u, 0xcfu);
             ctx->a = native_nz(ctx, 0xcfu);
             smbneo_native_fn_actor_B_18_proc_cfa8(ctx); return;
    case 26: native_write(ctx, 0x06u, 0x90u);
             native_write(ctx, 0x07u, 0xbbu);
             ctx->a = native_nz(ctx, 0xbbu);
             smbneo_native_fn_actor_B_19_proc_bb90(ctx); return;
    case 27: native_write(ctx, 0x06u, 0xffu);
             native_write(ctx, 0x07u, 0xb7u);
             ctx->a = native_nz(ctx, 0xb7u);
             smbneo_native_fn_actor_B_1A_proc_b7ff(ctx); return;
    case 28: native_write(ctx, 0x06u, 0x19u);
             native_write(ctx, 0x07u, 0xc8u);
             ctx->a = native_nz(ctx, 0xc8u);
             smbneo_native_fn_actor_B_02_proc_c819(ctx); return;
    case 29: native_write(ctx, 0x06u, 0x1cu);
             native_write(ctx, 0x07u, 0xd2u);
             ctx->a = native_nz(ctx, 0xd2u);
             smbneo_native_fn_actor_B_1C_proc_d21c(ctx); return;
    case 30: native_write(ctx, 0x06u, 0x6eu);
             native_write(ctx, 0x07u, 0xb7u);
             ctx->a = native_nz(ctx, 0xb7u);
             smbneo_native_fn_actor_B_1D_proc_b76e(ctx); return;
    case 31: native_write(ctx, 0x06u, 0x19u);
             native_write(ctx, 0x07u, 0xc8u);
             ctx->a = native_nz(ctx, 0xc8u);
             smbneo_native_fn_actor_B_02_proc_c819(ctx); return;
    case 32: native_write(ctx, 0x06u, 0x58u);
             native_write(ctx, 0x07u, 0xb6u);
             ctx->a = native_nz(ctx, 0xb6u);
             smbneo_native_fn_actor_B_1F_proc_b658(ctx); return;
    case 33: native_write(ctx, 0x06u, 0x1au);
             native_write(ctx, 0x07u, 0xc8u);
             ctx->a = native_nz(ctx, 0xc8u);
             smbneo_native_fn_actor_B_20_proc_c81a(ctx); return;
    default: ctx->unsupported = 1; return;
    }
    return;
}

void smbneo_native_fn_actor_area_loop_bfa5(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xbfa5u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x6du));
    ctx->status |= NATIVE_C;
    native_sbc(ctx, 0x04u);
    native_write(ctx, 0x6du, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0725u));
    ctx->status |= NATIVE_C;
    native_sbc(ctx, 0x04u);
    native_write(ctx, 0x0725u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x071au));
    ctx->status |= NATIVE_C;
    native_sbc(ctx, 0x04u);
    native_write(ctx, 0x071au, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x071bu));
    ctx->status |= NATIVE_C;
    native_sbc(ctx, 0x04u);
    native_write(ctx, 0x071bu, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x072au));
    ctx->status |= NATIVE_C;
    native_sbc(ctx, 0x04u);
    native_write(ctx, 0x072au, ctx->a);
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x073bu, ctx->a);
    native_write(ctx, 0x072bu, ctx->a);
    native_write(ctx, 0x0739u, ctx->a);
    native_write(ctx, 0x073au, ctx->a);
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, 0x4016u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x6000u + ctx->y)));
    native_write(ctx, 0x072cu, ctx->a);
    return;
    return;
}

void smbneo_native_fn_actor_init_c160(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xc160u);
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x), ctx->a);
    smbneo_native_fn_actor_init_do_c1b5(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    return;
    return;
}

void smbneo_native_fn_actor_init_do_c1b5(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xc1b5u);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x16u + ctx->x)));
    native_cmp(ctx, ctx->a, 0x15u);
    if ((ctx->status & NATIVE_C)) {
        goto Lc1c8;
    }
    ctx->y = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x)));
    native_adc(ctx, 0x08u);
    native_write(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, (uint16_t)(0x03d8u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, ctx->y);
Lc1c8: ;
    uint8_t table_selector_c1c8 = ctx->a;
    ctx->a = native_asl(ctx, ctx->a);
    ctx->y = native_nz(ctx, ctx->a);
    native_write(ctx, 0x04u, 0xcau);
    native_write(ctx, 0x05u, 0xc1u);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    switch (table_selector_c1c8) {
    case 0: native_write(ctx, 0x06u, 0x57u);
             native_write(ctx, 0x07u, 0xc2u);
             ctx->a = native_nz(ctx, 0xc2u);
             smbneo_native_fn_actor_A_00_init_c257(ctx); return;
    case 1: native_write(ctx, 0x06u, 0x57u);
             native_write(ctx, 0x07u, 0xc2u);
             ctx->a = native_nz(ctx, 0xc2u);
             smbneo_native_fn_actor_A_00_init_c257(ctx); return;
    case 2: native_write(ctx, 0x06u, 0x57u);
             native_write(ctx, 0x07u, 0xc2u);
             ctx->a = native_nz(ctx, 0xc2u);
             smbneo_native_fn_actor_A_00_init_c257(ctx); return;
    case 3: native_write(ctx, 0x06u, 0x67u);
             native_write(ctx, 0x07u, 0xc2u);
             ctx->a = native_nz(ctx, 0xc2u);
             smbneo_native_fn_actor_A_03_init_c267(ctx); return;
    case 4: native_write(ctx, 0x06u, 0x39u);
             native_write(ctx, 0x07u, 0xc2u);
             ctx->a = native_nz(ctx, 0xc2u);
             smbneo_native_fn_actor_A_04_init_c239(ctx); return;
    case 5: native_write(ctx, 0x06u, 0x6fu);
             native_write(ctx, 0x07u, 0xc2u);
             ctx->a = native_nz(ctx, 0xc2u);
             smbneo_native_fn_actor_A_05_init_c26f(ctx); return;
    case 6: native_write(ctx, 0x06u, 0x3au);
             native_write(ctx, 0x07u, 0xc2u);
             ctx->a = native_nz(ctx, 0xc2u);
             smbneo_native_fn_actor_A_06_init_c23a(ctx); return;
    case 7: native_write(ctx, 0x06u, 0x85u);
             native_write(ctx, 0x07u, 0xc2u);
             ctx->a = native_nz(ctx, 0xc2u);
             smbneo_native_fn_actor_A_07_init_c285(ctx); return;
    case 8: native_write(ctx, 0x06u, 0xaeu);
             native_write(ctx, 0x07u, 0xc2u);
             ctx->a = native_nz(ctx, 0xc2u);
             smbneo_native_fn_actor_A_08_init_c2ae(ctx); return;
    case 9: native_write(ctx, 0x06u, 0x39u);
             native_write(ctx, 0x07u, 0xc2u);
             ctx->a = native_nz(ctx, 0xc2u);
             smbneo_native_fn_actor_A_04_init_c239(ctx); return;
    case 10: native_write(ctx, 0x06u, 0xb8u);
             native_write(ctx, 0x07u, 0xc2u);
             ctx->a = native_nz(ctx, 0xc2u);
             smbneo_native_fn_actor_A_0A_init_c2b8(ctx); return;
    case 11: native_write(ctx, 0x06u, 0xb8u);
             native_write(ctx, 0x07u, 0xc2u);
             ctx->a = native_nz(ctx, 0xc2u);
             smbneo_native_fn_actor_A_0A_init_c2b8(ctx); return;
    case 12: native_write(ctx, 0x06u, 0x40u);
             native_write(ctx, 0x07u, 0xc2u);
             ctx->a = native_nz(ctx, 0xc2u);
             smbneo_native_fn_actor_A_0C_init_c240(ctx); return;
    case 13: native_write(ctx, 0x06u, 0xcau);
             native_write(ctx, 0x07u, 0xc6u);
             ctx->a = native_nz(ctx, 0xc6u);
             smbneo_native_fn_actor_A_0D_init_c6ca(ctx); return;
    case 14: native_write(ctx, 0x06u, 0x14u);
             native_write(ctx, 0x07u, 0xc7u);
             ctx->a = native_nz(ctx, 0xc7u);
             smbneo_native_fn_actor_A_0E_init_c714(ctx); return;
    case 15: native_write(ctx, 0x06u, 0x8du);
             native_write(ctx, 0x07u, 0xc2u);
             ctx->a = native_nz(ctx, 0xc2u);
             smbneo_native_fn_actor_A_0F_init_c28d(ctx); return;
    case 16: native_write(ctx, 0x06u, 0x80u);
             native_write(ctx, 0x07u, 0xc2u);
             ctx->a = native_nz(ctx, 0xc2u);
             smbneo_native_fn_actor_A_10_init_c280(ctx); return;
    case 17: native_write(ctx, 0x06u, 0xc8u);
             native_write(ctx, 0x07u, 0xc2u);
             ctx->a = native_nz(ctx, 0xc2u);
             smbneo_native_fn_actor_A_11_init_c2c8(ctx); return;
    case 18: native_write(ctx, 0x06u, 0xe3u);
             native_write(ctx, 0x07u, 0xc6u);
             ctx->a = native_nz(ctx, 0xc6u);
             smbneo_native_fn_actor_A_12_init_c6e3(ctx); return;
    case 19: native_write(ctx, 0x06u, 0x39u);
             native_write(ctx, 0x07u, 0xc2u);
             ctx->a = native_nz(ctx, 0xc2u);
             smbneo_native_fn_actor_A_04_init_c239(ctx); return;
    case 20: native_write(ctx, 0x06u, 0xe3u);
             native_write(ctx, 0x07u, 0xc6u);
             ctx->a = native_nz(ctx, 0xc6u);
             smbneo_native_fn_actor_A_12_init_c6e3(ctx); return;
    case 21: native_write(ctx, 0x06u, 0xe3u);
             native_write(ctx, 0x07u, 0xc6u);
             ctx->a = native_nz(ctx, 0xc6u);
             smbneo_native_fn_actor_A_12_init_c6e3(ctx); return;
    case 22: native_write(ctx, 0x06u, 0xe3u);
             native_write(ctx, 0x07u, 0xc6u);
             ctx->a = native_nz(ctx, 0xc6u);
             smbneo_native_fn_actor_A_12_init_c6e3(ctx); return;
    case 23: native_write(ctx, 0x06u, 0xe3u);
             native_write(ctx, 0x07u, 0xc6u);
             ctx->a = native_nz(ctx, 0xc6u);
             smbneo_native_fn_actor_A_12_init_c6e3(ctx); return;
    case 24: native_write(ctx, 0x06u, 0xfbu);
             native_write(ctx, 0x07u, 0xc6u);
             ctx->a = native_nz(ctx, 0xc6u);
             smbneo_native_fn_actor_B_03_init_c6fb(ctx); return;
    case 25: native_write(ctx, 0x06u, 0x39u);
             native_write(ctx, 0x07u, 0xc2u);
             ctx->a = native_nz(ctx, 0xc2u);
             smbneo_native_fn_actor_A_04_init_c239(ctx); return;
    case 26: native_write(ctx, 0x06u, 0x39u);
             native_write(ctx, 0x07u, 0xc2u);
             ctx->a = native_nz(ctx, 0xc2u);
             smbneo_native_fn_actor_A_04_init_c239(ctx); return;
    case 27: native_write(ctx, 0x06u, 0x9fu);
             native_write(ctx, 0x07u, 0xc3u);
             ctx->a = native_nz(ctx, 0xc3u);
             smbneo_native_fn_actor_B_06_init_c39f(ctx); return;
    case 28: native_write(ctx, 0x06u, 0x9fu);
             native_write(ctx, 0x07u, 0xc3u);
             ctx->a = native_nz(ctx, 0xc3u);
             smbneo_native_fn_actor_B_06_init_c39f(ctx); return;
    case 29: native_write(ctx, 0x06u, 0x9fu);
             native_write(ctx, 0x07u, 0xc3u);
             ctx->a = native_nz(ctx, 0xc3u);
             smbneo_native_fn_actor_B_06_init_c39f(ctx); return;
    case 30: native_write(ctx, 0x06u, 0x9fu);
             native_write(ctx, 0x07u, 0xc3u);
             ctx->a = native_nz(ctx, 0xc3u);
             smbneo_native_fn_actor_B_06_init_c39f(ctx); return;
    case 31: native_write(ctx, 0x06u, 0x9cu);
             native_write(ctx, 0x07u, 0xc3u);
             ctx->a = native_nz(ctx, 0xc3u);
             smbneo_native_fn_actor_B_0A_init_c39c(ctx); return;
    case 32: native_write(ctx, 0x06u, 0x39u);
             native_write(ctx, 0x07u, 0xc2u);
             ctx->a = native_nz(ctx, 0xc2u);
             smbneo_native_fn_actor_A_04_init_c239(ctx); return;
    case 33: native_write(ctx, 0x06u, 0x39u);
             native_write(ctx, 0x07u, 0xc2u);
             ctx->a = native_nz(ctx, 0xc2u);
             smbneo_native_fn_actor_A_04_init_c239(ctx); return;
    case 34: native_write(ctx, 0x06u, 0x39u);
             native_write(ctx, 0x07u, 0xc2u);
             ctx->a = native_nz(ctx, 0xc2u);
             smbneo_native_fn_actor_A_04_init_c239(ctx); return;
    case 35: native_write(ctx, 0x06u, 0x39u);
             native_write(ctx, 0x07u, 0xc2u);
             ctx->a = native_nz(ctx, 0xc2u);
             smbneo_native_fn_actor_A_04_init_c239(ctx); return;
    case 36: native_write(ctx, 0x06u, 0x22u);
             native_write(ctx, 0x07u, 0xc7u);
             ctx->a = native_nz(ctx, 0xc7u);
             smbneo_native_fn_actor_B_0F_init_c722(ctx); return;
    case 37: native_write(ctx, 0x06u, 0x55u);
             native_write(ctx, 0x07u, 0xc7u);
             ctx->a = native_nz(ctx, 0xc7u);
             smbneo_native_fn_actor_B_10_init_c755(ctx); return;
    case 38: native_write(ctx, 0x06u, 0x82u);
             native_write(ctx, 0x07u, 0xc7u);
             ctx->a = native_nz(ctx, 0xc7u);
             smbneo_native_fn_actor_B_11_init_c782(ctx); return;
    case 39: native_write(ctx, 0x06u, 0x88u);
             native_write(ctx, 0x07u, 0xc7u);
             ctx->a = native_nz(ctx, 0xc7u);
             smbneo_native_fn_actor_B_12_init_c788(ctx); return;
    case 40: native_write(ctx, 0x06u, 0x4eu);
             native_write(ctx, 0x07u, 0xc7u);
             ctx->a = native_nz(ctx, 0xc7u);
             smbneo_native_fn_actor_B_13_init_c74e(ctx); return;
    case 41: native_write(ctx, 0x06u, 0x46u);
             native_write(ctx, 0x07u, 0xc7u);
             ctx->a = native_nz(ctx, 0xc7u);
             smbneo_native_fn_actor_B_14_init_c746(ctx); return;
    case 42: native_write(ctx, 0x06u, 0x4eu);
             native_write(ctx, 0x07u, 0xc7u);
             ctx->a = native_nz(ctx, 0xc7u);
             smbneo_native_fn_actor_B_13_init_c74e(ctx); return;
    case 43: native_write(ctx, 0x06u, 0x8eu);
             native_write(ctx, 0x07u, 0xc7u);
             ctx->a = native_nz(ctx, 0xc7u);
             smbneo_native_fn_actor_B_16_init_c78e(ctx); return;
    case 44: native_write(ctx, 0x06u, 0x9au);
             native_write(ctx, 0x07u, 0xc7u);
             ctx->a = native_nz(ctx, 0xc7u);
             smbneo_native_fn_actor_B_17_init_c79a(ctx); return;
    case 45: native_write(ctx, 0x06u, 0x8cu);
             native_write(ctx, 0x07u, 0xc4u);
             ctx->a = native_nz(ctx, 0xc4u);
             smbneo_native_fn_actor_B_18_init_c48c(ctx); return;
    case 46: native_write(ctx, 0x06u, 0x6bu);
             native_write(ctx, 0x07u, 0xbbu);
             ctx->a = native_nz(ctx, 0xbbu);
             smbneo_native_fn_actor_B_19_init_bb6b(ctx); return;
    case 47: native_write(ctx, 0x06u, 0xd2u);
             native_write(ctx, 0x07u, 0xb7u);
             ctx->a = native_nz(ctx, 0xb7u);
             smbneo_native_fn_actor_B_1A_init_b7d2(ctx); return;
    case 48: native_write(ctx, 0x06u, 0x39u);
             native_write(ctx, 0x07u, 0xc2u);
             ctx->a = native_nz(ctx, 0xc2u);
             smbneo_native_fn_actor_A_04_init_c239(ctx); return;
    case 49: native_write(ctx, 0x06u, 0x39u);
             native_write(ctx, 0x07u, 0xc2u);
             ctx->a = native_nz(ctx, 0xc2u);
             smbneo_native_fn_actor_A_04_init_c239(ctx); return;
    case 50: native_write(ctx, 0x06u, 0x39u);
             native_write(ctx, 0x07u, 0xc2u);
             ctx->a = native_nz(ctx, 0xc2u);
             smbneo_native_fn_actor_A_04_init_c239(ctx); return;
    case 51: native_write(ctx, 0x06u, 0x39u);
             native_write(ctx, 0x07u, 0xc2u);
             ctx->a = native_nz(ctx, 0xc2u);
             smbneo_native_fn_actor_A_04_init_c239(ctx); return;
    case 52: native_write(ctx, 0x06u, 0x39u);
             native_write(ctx, 0x07u, 0xc2u);
             ctx->a = native_nz(ctx, 0xc2u);
             smbneo_native_fn_actor_A_04_init_c239(ctx); return;
    case 53: native_write(ctx, 0x06u, 0x50u);
             native_write(ctx, 0x07u, 0xc2u);
             ctx->a = native_nz(ctx, 0xc2u);
             smbneo_native_fn_actor_B_20_init_c250(ctx); return;
    case 54: native_write(ctx, 0x06u, 0xc4u);
             native_write(ctx, 0x07u, 0xc7u);
             ctx->a = native_nz(ctx, 0xc7u);
             smbneo_native_fn_actor_B_36_init_c7c4(ctx); return;
    default: ctx->unsupported = 1; return;
    }
    return;
}

void smbneo_native_fn_actor_A_04_init_c239(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xc239u);
    return;
    return;
}

void smbneo_native_fn_actor_A_06_init_c23a(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xc23au);
    smbneo_native_fn_actor_A_00_init_c257(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    /* static tail transfer */
    smbneo_native_fn_actor_init_col_small_c289(ctx);
    return;
    return;
}

void smbneo_native_fn_actor_A_0C_init_c240(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xc240u);
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, (uint16_t)(uint8_t)(0xb6u + ctx->x), ctx->a);
    native_write(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x), ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    native_write(ctx, (uint16_t)(0x0796u + ctx->x), ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    native_write(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x), ctx->a);
    /* static tail transfer */
    smbneo_native_fn_actor_init_col_small_c289(ctx);
    return;
    return;
}

void smbneo_native_fn_actor_B_20_init_c250(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xc250u);
    ctx->a = native_nz(ctx, 0xb8u);
    native_write(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x), ctx->a);
    return;
    return;
}

void smbneo_native_fn_actor_A_00_init_c257(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xc257u);
    ctx->y = native_nz(ctx, 0x01u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x076au));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lc25f;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
Lc25f: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xc255u + ctx->y)));
    native_write(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x), ctx->a);
    /* static tail transfer */
    goto Lc29d;
Lc29d: ;
    ctx->a = native_nz(ctx, 0x03u);
    native_write(ctx, (uint16_t)(0x049au + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, (uint16_t)(uint8_t)(0x46u + ctx->x), ctx->a);
    smbneo_native_fn_actor_init_veloc_y_c2a6(ctx);
    return;
    return;
}

void smbneo_native_fn_actor_A_03_init_c267(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xc267u);
    smbneo_native_fn_actor_A_00_init_c257(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x), ctx->a);
    return;
    return;
}

void smbneo_native_fn_actor_A_05_init_c26f(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xc26fu);
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, (uint16_t)(0x03a2u + ctx->x), ctx->a);
    native_write(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0xffu);
    native_write(ctx, (uint16_t)(0x0796u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x0bu);
    /* static tail transfer */
    goto Lc29f;
Lc29f: ;
    native_write(ctx, (uint16_t)(0x049au + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, (uint16_t)(uint8_t)(0x46u + ctx->x), ctx->a);
    smbneo_native_fn_actor_init_veloc_y_c2a6(ctx);
    return;
    return;
}

void smbneo_native_fn_actor_A_10_init_c280(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xc280u);
    goto Lc280;
Lc262: ;
    native_write(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x), ctx->a);
    /* static tail transfer */
    goto Lc29d;
Lc280: ;
    ctx->a = native_nz(ctx, 0x00u);
    /* static tail transfer */
    goto Lc262;
Lc29d: ;
    ctx->a = native_nz(ctx, 0x03u);
    native_write(ctx, (uint16_t)(0x049au + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, (uint16_t)(uint8_t)(0x46u + ctx->x), ctx->a);
    smbneo_native_fn_actor_init_veloc_y_c2a6(ctx);
    return;
    return;
}

void smbneo_native_fn_actor_A_07_init_c285(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xc285u);
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x), ctx->a);
    smbneo_native_fn_actor_init_col_small_c289(ctx);
    return;
    return;
}

void smbneo_native_fn_actor_init_col_small_c289(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xc289u);
    ctx->a = native_nz(ctx, 0x09u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lc29f;
    }
    smbneo_native_fn_actor_A_0F_init_c28d(ctx);
    return;
Lc29f: ;
    native_write(ctx, (uint16_t)(0x049au + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, (uint16_t)(uint8_t)(0x46u + ctx->x), ctx->a);
    smbneo_native_fn_actor_init_veloc_y_c2a6(ctx);
    return;
    return;
}

void smbneo_native_fn_actor_A_0F_init_c28d(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xc28du);
    ctx->y = native_nz(ctx, 0x30u);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x)));
    native_write(ctx, (uint16_t)(0x0401u + ctx->x), ctx->a);
    if (!(ctx->status & NATIVE_N)) {
        goto Lc298;
    }
    ctx->y = native_nz(ctx, 0xe0u);
Lc298: ;
    ctx->a = native_nz(ctx, ctx->y);
    native_adc(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x)));
    native_write(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x03u);
    native_write(ctx, (uint16_t)(0x049au + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, (uint16_t)(uint8_t)(0x46u + ctx->x), ctx->a);
    smbneo_native_fn_actor_init_veloc_y_c2a6(ctx);
    return;
    return;
}

void smbneo_native_fn_actor_init_veloc_y_c2a6(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xc2a6u);
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, (uint16_t)(uint8_t)(0xa0u + ctx->x), ctx->a);
    native_write(ctx, (uint16_t)(0x0434u + ctx->x), ctx->a);
    return;
    return;
}

void smbneo_native_fn_actor_A_08_init_c2ae(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xc2aeu);
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, (uint16_t)(uint8_t)(0x46u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x09u);
    native_write(ctx, (uint16_t)(0x049au + ctx->x), ctx->a);
    return;
    return;
}

void smbneo_native_fn_actor_A_0A_init_c2b8(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xc2b8u);
    smbneo_native_fn_actor_init_col_small_c289(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x07a7u + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x10u));
    native_write(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x)));
    native_write(ctx, (uint16_t)(0x0434u + ctx->x), ctx->a);
    return;
    return;
}

void smbneo_native_fn_actor_A_11_init_c2c8(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xc2c8u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x06cbu));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lc2d8;
    }
    smbneo_native_fn_actor_init_lakitu_do_c2cd(ctx);
    return;
Lc2d8: ;
    /* static tail transfer */
    smbneo_native_fn_actor_erase_c8db(ctx);
    return;
    return;
}

void smbneo_native_fn_actor_init_lakitu_do_c2cd(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xc2cdu);
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x06d1u, ctx->a);
    smbneo_native_fn_actor_A_10_init_c280(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    /* static tail transfer */
    goto Lc71c;
Lc71c: ;
    ctx->a = native_nz(ctx, 0x03u);
    native_write(ctx, (uint16_t)(0x049au + ctx->x), ctx->a);
    return;
    return;
}

void smbneo_native_fn_actor_init_lakitu_spiny_c2e7(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xc2e7u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x078fu));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lc328;
    }
    native_cmp(ctx, ctx->x, 0x05u);
    if ((ctx->status & NATIVE_C)) {
        goto Lc328;
    }
    ctx->a = native_nz(ctx, 0x80u);
    native_write(ctx, 0x078fu, ctx->a);
    ctx->y = native_nz(ctx, 0x04u);
Lc2f7: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0016u + ctx->y)));
    native_cmp(ctx, ctx->a, 0x11u);
    if ((ctx->status & NATIVE_Z)) {
        goto Lc329;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    if (!(ctx->status & NATIVE_N)) {
        goto Lc2f7;
    }
    native_write(ctx, 0x06d1u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x06d1u) + 1u)));
    ctx->a = native_nz(ctx, native_read(ctx, 0x06d1u));
    native_cmp(ctx, ctx->a, 0x07u);
    if (!(ctx->status & NATIVE_C)) {
        goto Lc328;
    }
    ctx->x = native_nz(ctx, 0x04u);
Lc30d: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x0fu + ctx->x)));
    if ((ctx->status & NATIVE_Z)) {
        goto Lc316;
    }
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x - 1u));
    if (!(ctx->status & NATIVE_N)) {
        goto Lc30d;
    }
    if ((ctx->status & NATIVE_N)) {
        goto Lc326;
    }
Lc316: ;
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x11u);
    native_write(ctx, (uint16_t)(uint8_t)(0x16u + ctx->x), ctx->a);
    smbneo_native_fn_actor_init_lakitu_do_c2cd(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, 0x20u);
    smbneo_native_fn_actor_init_right_side_c51b(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Lc326: ;
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
Lc328: ;
    return;
Lc329: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0xceu));
    native_cmp(ctx, ctx->a, 0x2cu);
    if (!(ctx->status & NATIVE_C)) {
        goto Lc328;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x001eu + ctx->y)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lc328;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x006eu + ctx->y)));
    native_write(ctx, (uint16_t)(uint8_t)(0x6eu + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0087u + ctx->y)));
    native_write(ctx, (uint16_t)(uint8_t)(0x87u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, (uint16_t)(uint8_t)(0xb6u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x00cfu + ctx->y)));
    ctx->status |= NATIVE_C;
    native_sbc(ctx, 0x08u);
    native_write(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x07a7u + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x03u));
    ctx->y = native_nz(ctx, ctx->a);
    ctx->x = native_nz(ctx, 0x02u);
Lc352: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xc2dbu + ctx->y)));
    native_write(ctx, (uint16_t)(uint8_t)(0x01u + ctx->x), ctx->a);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x - 1u));
    if (!(ctx->status & NATIVE_N)) {
        goto Lc352;
    }
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    smbneo_native_fn_actor_proc_lakitu_spiny_ceaf(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->y = native_nz(ctx, native_read(ctx, 0x57u));
    native_cmp(ctx, ctx->y, 0x08u);
    if ((ctx->status & NATIVE_C)) {
        goto Lc377;
    }
    ctx->y = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x07a8u + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x03u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lc376;
    }
    ctx->a = native_nz(ctx, ctx->y);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a ^ 0xffu));
    ctx->y = native_nz(ctx, ctx->a);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
Lc376: ;
    ctx->a = native_nz(ctx, ctx->y);
Lc377: ;
    smbneo_native_fn_actor_init_col_small_c289(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->y = native_nz(ctx, 0x02u);
    native_write(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x), ctx->a);
    native_cmp(ctx, ctx->a, 0x00u);
    if ((ctx->status & NATIVE_N)) {
        goto Lc383;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
Lc383: ;
    native_write(ctx, (uint16_t)(uint8_t)(0x46u + ctx->x), ctx->y);
    ctx->a = native_nz(ctx, 0xfdu);
    native_write(ctx, (uint16_t)(uint8_t)(0xa0u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, (uint16_t)(uint8_t)(0x0fu + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x05u);
    native_write(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x), ctx->a);
    return;
    return;
}

void smbneo_native_fn_actor_B_0A_init_c39c(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xc39cu);
    smbneo_native_fn_actor_init_dup_c4b8(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_actor_B_06_init_c39f(ctx);
    return;
    return;
}

void smbneo_native_fn_actor_B_06_init_c39f(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xc39fu);
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x16u + ctx->x)));
    ctx->status |= NATIVE_C;
    native_sbc(ctx, 0x1bu);
    ctx->y = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xc392u + ctx->y)));
    native_write(ctx, (uint16_t)(0x0388u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xc397u + ctx->y)));
    native_write(ctx, (uint16_t)(uint8_t)(0x34u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x)));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x04u);
    native_write(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x87u + ctx->x)));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x04u);
    native_write(ctx, (uint16_t)(uint8_t)(0x87u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x6eu + ctx->x)));
    native_adc(ctx, 0x00u);
    native_write(ctx, (uint16_t)(uint8_t)(0x6eu + ctx->x), ctx->a);
    /* static tail transfer */
    goto Lc71c;
Lc71c: ;
    ctx->a = native_nz(ctx, 0x03u);
    native_write(ctx, (uint16_t)(0x049au + ctx->x), ctx->a);
    return;
    return;
}

void smbneo_native_fn_actor_init_cheep_fly_c3eb(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xc3ebu);
    uint8_t saved_value_0 = 0u;
    goto Lc3eb;
Lc391: ;
    return;
Lc3eb: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x078fu));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lc391;
    }
    smbneo_native_fn_actor_init_col_small_c289(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x07a8u + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x03u));
    ctx->y = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xc3e7u + ctx->y)));
    native_write(ctx, 0x078fu, ctx->a);
    ctx->y = native_nz(ctx, 0x03u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x06ccu));
    if ((ctx->status & NATIVE_Z)) {
        goto Lc407;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
Lc407: ;
    native_write(ctx, 0x00u, ctx->y);
    native_cmp(ctx, ctx->x, native_read(ctx, 0x00u));
    if ((ctx->status & NATIVE_C)) {
        goto Lc391;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x07a7u + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x03u));
    native_write(ctx, 0x00u, ctx->a);
    native_write(ctx, 0x01u, ctx->a);
    ctx->a = native_nz(ctx, 0xfbu);
    native_write(ctx, (uint16_t)(uint8_t)(0xa0u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x00u);
    ctx->y = native_nz(ctx, native_read(ctx, 0x57u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lc427;
    }
    ctx->a = native_nz(ctx, 0x04u);
    native_cmp(ctx, ctx->y, 0x19u);
    if (!(ctx->status & NATIVE_C)) {
        goto Lc427;
    }
    ctx->a = native_asl(ctx, ctx->a);
Lc427: ;
    saved_value_0 = ctx->a;
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, 0x00u));
    native_write(ctx, 0x00u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x07a8u + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x03u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lc43b;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x07a9u + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x0fu));
    native_write(ctx, 0x00u, ctx->a);
Lc43b: ;
    ctx->a = native_nz(ctx, saved_value_0);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, 0x01u));
    ctx->y = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xc3dbu + ctx->y)));
    native_write(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, (uint16_t)(uint8_t)(0x46u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x57u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lc45f;
    }
    ctx->y = native_nz(ctx, native_read(ctx, 0x00u));
    ctx->a = native_nz(ctx, ctx->y);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x02u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lc45f;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a ^ 0xffu));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x01u);
    native_write(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x), ctx->a);
    native_write(ctx, (uint16_t)(uint8_t)(0x46u + ctx->x), native_nz(ctx, (uint8_t)(native_read(ctx, (uint16_t)(uint8_t)(0x46u + ctx->x)) + 1u)));
Lc45f: ;
    ctx->a = native_nz(ctx, ctx->y);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x02u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lc473;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x86u));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, (uint16_t)(0xc3cbu + ctx->y)));
    native_write(ctx, (uint16_t)(uint8_t)(0x87u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x6du));
    native_adc(ctx, 0x00u);
    /* static tail transfer */
    goto Lc47f;
Lc473: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x86u));
    ctx->status |= NATIVE_C;
    native_sbc(ctx, native_read(ctx, (uint16_t)(0xc3cbu + ctx->y)));
    native_write(ctx, (uint16_t)(uint8_t)(0x87u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x6du));
    native_sbc(ctx, 0x00u);
Lc47f: ;
    native_write(ctx, (uint16_t)(uint8_t)(0x6eu + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, (uint16_t)(uint8_t)(0x0fu + ctx->x), ctx->a);
    native_write(ctx, (uint16_t)(uint8_t)(0xb6u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0xf8u);
    native_write(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x), ctx->a);
    return;
    return;
}

void smbneo_native_fn_actor_B_18_init_c48c(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xc48cu);
    smbneo_native_fn_actor_init_dup_c4b8(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_write(ctx, 0x0368u, ctx->x);
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x0363u, ctx->a);
    native_write(ctx, 0x0369u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x87u + ctx->x)));
    native_write(ctx, 0x0366u, ctx->a);
    ctx->a = native_nz(ctx, 0xdfu);
    native_write(ctx, 0x0790u, ctx->a);
    native_write(ctx, (uint16_t)(uint8_t)(0x46u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x20u);
    native_write(ctx, 0x0364u, ctx->a);
    native_write(ctx, (uint16_t)(0x078au + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x05u);
    native_write(ctx, 0x0483u, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    native_write(ctx, 0x0365u, ctx->a);
    return;
    return;
}

void smbneo_native_fn_actor_init_dup_c4b8(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xc4b8u);
    ctx->y = native_nz(ctx, 0xffu);
Lc4ba: ;
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x000fu + ctx->y)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lc4ba;
    }
    native_write(ctx, 0x06cfu, ctx->y);
    ctx->a = native_nz(ctx, ctx->x);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | 0x80u));
    native_write(ctx, (uint16_t)(0x000fu + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x6eu + ctx->x)));
    native_write(ctx, (uint16_t)(0x006eu + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x87u + ctx->x)));
    native_write(ctx, (uint16_t)(0x0087u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, (uint16_t)(uint8_t)(0x0fu + ctx->x), ctx->a);
    native_write(ctx, (uint16_t)(0x00b6u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x)));
    native_write(ctx, (uint16_t)(0x00cfu + ctx->y), ctx->a);
    return;
    return;
}

void smbneo_native_fn_actor_init_bowser_flame_c4e6(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xc4e6u);
    goto Lc4e6;
Lc4df: ;
    return;
Lc4e6: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x078fu));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lc4df;
    }
    native_write(ctx, (uint16_t)(0x0434u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0xfdu));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | 0x02u));
    native_write(ctx, 0xfdu, ctx->a);
    ctx->y = native_nz(ctx, native_read(ctx, 0x0368u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0016u + ctx->y)));
    native_cmp(ctx, ctx->a, 0x2du);
    if ((ctx->status & NATIVE_Z)) {
        goto Lc52f;
    }
    smbneo_native_fn_actor_proc_bowser_flame_timer_init_d11c(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x20u);
    ctx->y = native_nz(ctx, native_read(ctx, 0x06ccu));
    if ((ctx->status & NATIVE_Z)) {
        goto Lc50c;
    }
    ctx->status |= NATIVE_C;
    native_sbc(ctx, 0x10u);
Lc50c: ;
    native_write(ctx, 0x078fu, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x07a7u + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x03u));
    native_write(ctx, (uint16_t)(0x0417u + ctx->x), ctx->a);
    ctx->y = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xc4e0u + ctx->y)));
    smbneo_native_fn_actor_init_right_side_c51b(ctx);
    return;
Lc52f: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0087u + ctx->y)));
    ctx->status |= NATIVE_C;
    native_sbc(ctx, 0x0eu);
    native_write(ctx, (uint16_t)(uint8_t)(0x87u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x006eu + ctx->y)));
    native_write(ctx, (uint16_t)(uint8_t)(0x6eu + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x00cfu + ctx->y)));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x08u);
    native_write(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x07a7u + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x03u));
    native_write(ctx, (uint16_t)(0x0417u + ctx->x), ctx->a);
    ctx->y = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xc4e0u + ctx->y)));
    ctx->y = native_nz(ctx, 0x00u);
    native_cmp(ctx, ctx->a, native_read(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x)));
    if (!(ctx->status & NATIVE_C)) {
        goto Lc557;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
Lc557: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xc4e4u + ctx->y)));
    native_write(ctx, (uint16_t)(0x0434u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x06cbu, ctx->a);
    ctx->a = native_nz(ctx, 0x08u);
    native_write(ctx, (uint16_t)(0x049au + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, (uint16_t)(uint8_t)(0xb6u + ctx->x), ctx->a);
    native_write(ctx, (uint16_t)(uint8_t)(0x0fu + ctx->x), ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    native_write(ctx, (uint16_t)(0x0401u + ctx->x), ctx->a);
    native_write(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x), ctx->a);
    return;
    return;
}

void smbneo_native_fn_actor_init_right_side_c51b(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xc51bu);
    native_write(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x071du));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x20u);
    native_write(ctx, (uint16_t)(uint8_t)(0x87u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x071bu));
    native_adc(ctx, 0x00u);
    native_write(ctx, (uint16_t)(uint8_t)(0x6eu + ctx->x), ctx->a);
    /* static tail transfer */
    goto Lc562;
Lc562: ;
    ctx->a = native_nz(ctx, 0x08u);
    native_write(ctx, (uint16_t)(0x049au + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, (uint16_t)(uint8_t)(0xb6u + ctx->x), ctx->a);
    native_write(ctx, (uint16_t)(uint8_t)(0x0fu + ctx->x), ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    native_write(ctx, (uint16_t)(0x0401u + ctx->x), ctx->a);
    native_write(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x), ctx->a);
    return;
    return;
}

void smbneo_native_fn_actor_init_fireworks_c580(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xc580u);
    uint8_t saved_value_0 = 0u;
    ctx->a = native_nz(ctx, native_read(ctx, 0x078fu));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lc5cc;
    }
    ctx->a = native_nz(ctx, 0x20u);
    native_write(ctx, 0x078fu, ctx->a);
    native_write(ctx, 0x06d7u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x06d7u) - 1u)));
    ctx->y = native_nz(ctx, 0x06u);
Lc58f: ;
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0016u + ctx->y)));
    native_cmp(ctx, ctx->a, 0x31u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lc58f;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0087u + ctx->y)));
    ctx->status |= NATIVE_C;
    native_sbc(ctx, 0x30u);
    saved_value_0 = ctx->a;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x006eu + ctx->y)));
    native_sbc(ctx, 0x00u);
    native_write(ctx, 0x00u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x06d7u));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, (uint16_t)(0x001eu + ctx->y)));
    ctx->y = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, saved_value_0);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, (uint16_t)(0xc574u + ctx->y)));
    native_write(ctx, (uint16_t)(uint8_t)(0x87u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x00u));
    native_adc(ctx, 0x00u);
    native_write(ctx, (uint16_t)(uint8_t)(0x6eu + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xc57au + ctx->y)));
    native_write(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, (uint16_t)(uint8_t)(0xb6u + ctx->x), ctx->a);
    native_write(ctx, (uint16_t)(uint8_t)(0x0fu + ctx->x), ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    native_write(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x08u);
    native_write(ctx, (uint16_t)(uint8_t)(0xa0u + ctx->x), ctx->a);
Lc5cc: ;
    return;
    return;
}

void smbneo_native_fn_actor_init_bullet_cheep_c5df(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xc5dfu);
    uint8_t saved_value_0 = 0u;
    uint8_t saved_value_1 = 0u;
    goto Lc5df;
Lc1a7: ;
    native_write(ctx, 0x0739u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0739u) + 1u)));
    native_write(ctx, 0x0739u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0739u) + 1u)));
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x073bu, ctx->a);
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    return;
Lc5df: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x078fu));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lc653;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x074eu));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lc640;
    }
    native_cmp(ctx, ctx->x, 0x03u);
    if ((ctx->status & NATIVE_C)) {
        goto Lc653;
    }
    ctx->y = native_nz(ctx, 0x00u);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x07a7u + ctx->x)));
    native_cmp(ctx, ctx->a, 0xaau);
    if (!(ctx->status & NATIVE_C)) {
        goto Lc5f7;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
Lc5f7: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x075fu));
    native_cmp(ctx, ctx->a, 0x01u);
    if ((ctx->status & NATIVE_Z)) {
        goto Lc5ff;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
Lc5ff: ;
    ctx->a = native_nz(ctx, ctx->y);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x01u));
    ctx->y = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xc5ddu + ctx->y)));
Lc606: ;
    native_write(ctx, (uint16_t)(uint8_t)(0x16u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x06ddu));
    native_cmp(ctx, ctx->a, 0xffu);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lc614;
    }
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x06ddu, ctx->a);
Lc614: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x07a7u + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x07u));
Lc619: ;
    ctx->y = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xc5cdu + ctx->y)));
    native_bit(ctx, native_read(ctx, 0x06ddu));
    if ((ctx->status & NATIVE_Z)) {
        goto Lc629;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->a = native_nz(ctx, ctx->y);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x07u));
    /* static tail transfer */
    goto Lc619;
Lc629: ;
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | native_read(ctx, 0x06ddu)));
    native_write(ctx, 0x06ddu, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xc5d5u + ctx->y)));
    smbneo_native_fn_actor_init_right_side_c51b(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_write(ctx, (uint16_t)(0x0417u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x20u);
    native_write(ctx, 0x078fu, ctx->a);
    /* static tail transfer */
    smbneo_native_fn_actor_init_do_c1b5(ctx);
    return;
Lc640: ;
    ctx->y = native_nz(ctx, 0xffu);
Lc642: ;
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    native_cmp(ctx, ctx->y, 0x05u);
    if ((ctx->status & NATIVE_C)) {
        goto Lc654;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x000fu + ctx->y)));
    if ((ctx->status & NATIVE_Z)) {
        goto Lc642;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0016u + ctx->y)));
    native_cmp(ctx, ctx->a, 0x08u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lc642;
    }
Lc653: ;
    return;
Lc654: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0xfeu));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | 0x08u));
    native_write(ctx, 0xfeu, ctx->a);
    ctx->a = native_nz(ctx, 0x08u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lc606;
    }
    ctx->y = native_nz(ctx, 0x00u);
    ctx->status |= NATIVE_C;
    native_sbc(ctx, 0x37u);
    saved_value_0 = ctx->a;
    native_cmp(ctx, ctx->a, 0x04u);
    if ((ctx->status & NATIVE_C)) {
        goto Lc673;
    }
    saved_value_1 = ctx->a;
    ctx->y = native_nz(ctx, 0x06u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x076au));
    if ((ctx->status & NATIVE_Z)) {
        goto Lc672;
    }
    ctx->y = native_nz(ctx, 0x02u);
Lc672: ;
    ctx->a = native_nz(ctx, saved_value_1);
Lc673: ;
    native_write(ctx, 0x01u, ctx->y);
    ctx->y = native_nz(ctx, 0xb0u);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x02u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lc67d;
    }
    ctx->y = native_nz(ctx, 0x70u);
Lc67d: ;
    native_write(ctx, 0x00u, ctx->y);
    ctx->a = native_nz(ctx, native_read(ctx, 0x071bu));
    native_write(ctx, 0x02u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x071du));
    native_write(ctx, 0x03u, ctx->a);
    ctx->y = native_nz(ctx, 0x02u);
    ctx->a = native_nz(ctx, saved_value_0);
    ctx->a = native_lsr(ctx, ctx->a);
    if (!(ctx->status & NATIVE_C)) {
        goto Lc690;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
Lc690: ;
    native_write(ctx, 0x06d3u, ctx->y);
Lc693: ;
    ctx->x = native_nz(ctx, 0xffu);
Lc695: ;
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    native_cmp(ctx, ctx->x, 0x05u);
    if ((ctx->status & NATIVE_C)) {
        goto Lc6c7;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x0fu + ctx->x)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lc695;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x01u));
    native_write(ctx, (uint16_t)(uint8_t)(0x16u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x02u));
    native_write(ctx, (uint16_t)(uint8_t)(0x6eu + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x03u));
    native_write(ctx, (uint16_t)(uint8_t)(0x87u + ctx->x), ctx->a);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x18u);
    native_write(ctx, 0x03u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x02u));
    native_adc(ctx, 0x00u);
    native_write(ctx, 0x02u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x00u));
    native_write(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, (uint16_t)(uint8_t)(0xb6u + ctx->x), ctx->a);
    native_write(ctx, (uint16_t)(uint8_t)(0x0fu + ctx->x), ctx->a);
    smbneo_native_fn_actor_init_do_c1b5(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_write(ctx, 0x06d3u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x06d3u) - 1u)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lc693;
    }
Lc6c7: ;
    /* static tail transfer */
    goto Lc1a7;
    return;
}

void smbneo_native_fn_actor_A_0D_init_c6ca(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xc6cau);
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x), ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    native_write(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x), ctx->a);
    native_write(ctx, (uint16_t)(uint8_t)(0xa0u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x)));
    native_write(ctx, (uint16_t)(0x0434u + ctx->x), ctx->a);
    ctx->status |= NATIVE_C;
    native_sbc(ctx, 0x18u);
    native_write(ctx, (uint16_t)(0x0417u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x09u);
    /* static tail transfer */
    goto Lc71e;
Lc71e: ;
    native_write(ctx, (uint16_t)(0x049au + ctx->x), ctx->a);
    return;
    return;
}

void smbneo_native_fn_actor_A_12_init_c6e3(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xc6e3u);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x16u + ctx->x)));
    native_write(ctx, 0x06cbu, ctx->a);
    ctx->status |= NATIVE_C;
    native_sbc(ctx, 0x12u);
    uint8_t table_selector_c6eb = ctx->a;
    ctx->a = native_asl(ctx, ctx->a);
    ctx->y = native_nz(ctx, ctx->a);
    native_write(ctx, 0x04u, 0xedu);
    native_write(ctx, 0x05u, 0xc6u);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    switch (table_selector_c6eb) {
    case 0: native_write(ctx, 0x06u, 0xe7u);
             native_write(ctx, 0x07u, 0xc2u);
             ctx->a = native_nz(ctx, 0xc2u);
             smbneo_native_fn_actor_init_lakitu_spiny_c2e7(ctx); return;
    case 1: native_write(ctx, 0x06u, 0xfau);
             native_write(ctx, 0x07u, 0xc6u);
             ctx->a = native_nz(ctx, 0xc6u);
             smbneo_native_fn_actor_init_swarm_null_c6fa(ctx); return;
    case 2: native_write(ctx, 0x06u, 0xebu);
             native_write(ctx, 0x07u, 0xc3u);
             ctx->a = native_nz(ctx, 0xc3u);
             smbneo_native_fn_actor_init_cheep_fly_c3eb(ctx); return;
    case 3: native_write(ctx, 0x06u, 0xe6u);
             native_write(ctx, 0x07u, 0xc4u);
             ctx->a = native_nz(ctx, 0xc4u);
             smbneo_native_fn_actor_init_bowser_flame_c4e6(ctx); return;
    case 4: native_write(ctx, 0x06u, 0x80u);
             native_write(ctx, 0x07u, 0xc5u);
             ctx->a = native_nz(ctx, 0xc5u);
             smbneo_native_fn_actor_init_fireworks_c580(ctx); return;
    case 5: native_write(ctx, 0x06u, 0xdfu);
             native_write(ctx, 0x07u, 0xc5u);
             ctx->a = native_nz(ctx, 0xc5u);
             smbneo_native_fn_actor_init_bullet_cheep_c5df(ctx); return;
    default: ctx->unsupported = 1; return;
    }
    return;
}

void smbneo_native_fn_actor_init_swarm_null_c6fa(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xc6fau);
    return;
    return;
}

void smbneo_native_fn_actor_B_03_init_c6fb(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xc6fbu);
    ctx->y = native_nz(ctx, 0x05u);
Lc6fd: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0016u + ctx->y)));
    native_cmp(ctx, ctx->a, 0x11u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lc709;
    }
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, (uint16_t)(0x001eu + ctx->y), ctx->a);
Lc709: ;
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    if (!(ctx->status & NATIVE_N)) {
        goto Lc6fd;
    }
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x06cbu, ctx->a);
    native_write(ctx, (uint16_t)(uint8_t)(0x0fu + ctx->x), ctx->a);
    return;
    return;
}

void smbneo_native_fn_actor_A_0E_init_c714(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xc714u);
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, (uint16_t)(uint8_t)(0x46u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0xf8u);
    native_write(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x03u);
    native_write(ctx, (uint16_t)(0x049au + ctx->x), ctx->a);
    return;
    return;
}

void smbneo_native_fn_actor_B_0F_init_c722(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xc722u);
    native_write(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x), native_nz(ctx, (uint8_t)(native_read(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x)) - 1u)));
    native_write(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x), native_nz(ctx, (uint8_t)(native_read(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x)) - 1u)));
    ctx->y = native_nz(ctx, native_read(ctx, 0x06ccu));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lc730;
    }
    ctx->y = native_nz(ctx, 0x02u);
    smbneo_native_fn_actor_plat_pos_c7b4(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Lc730: ;
    ctx->y = native_nz(ctx, 0xffu);
    ctx->a = native_nz(ctx, native_read(ctx, 0x03a0u));
    native_write(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x), ctx->a);
    if (!(ctx->status & NATIVE_N)) {
        goto Lc73b;
    }
    ctx->a = native_nz(ctx, ctx->x);
    ctx->y = native_nz(ctx, ctx->a);
Lc73b: ;
    native_write(ctx, 0x03a0u, ctx->y);
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, (uint16_t)(uint8_t)(0x46u + ctx->x), ctx->a);
    ctx->y = native_nz(ctx, ctx->a);
    smbneo_native_fn_actor_plat_pos_c7b4(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_actor_B_14_init_c746(ctx);
    return;
    return;
}

void smbneo_native_fn_actor_B_14_init_c746(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xc746u);
    ctx->a = native_nz(ctx, 0xffu);
    native_write(ctx, (uint16_t)(0x03a2u + ctx->x), ctx->a);
    /* static tail transfer */
    goto Lc76b;
Lc76b: ;
    smbneo_native_fn_actor_init_veloc_y_c2a6(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, 0x05u);
    ctx->y = native_nz(ctx, native_read(ctx, 0x074eu));
    native_cmp(ctx, ctx->y, 0x03u);
    if ((ctx->status & NATIVE_Z)) {
        goto Lc77e;
    }
    ctx->y = native_nz(ctx, native_read(ctx, 0x06ccu));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lc77e;
    }
    ctx->a = native_nz(ctx, 0x06u);
Lc77e: ;
    native_write(ctx, (uint16_t)(0x049au + ctx->x), ctx->a);
    return;
    return;
}

void smbneo_native_fn_actor_B_13_init_c74e(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xc74eu);
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x), ctx->a);
    /* static tail transfer */
    goto Lc76b;
Lc76b: ;
    smbneo_native_fn_actor_init_veloc_y_c2a6(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, 0x05u);
    ctx->y = native_nz(ctx, native_read(ctx, 0x074eu));
    native_cmp(ctx, ctx->y, 0x03u);
    if ((ctx->status & NATIVE_Z)) {
        goto Lc77e;
    }
    ctx->y = native_nz(ctx, native_read(ctx, 0x06ccu));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lc77e;
    }
    ctx->a = native_nz(ctx, 0x06u);
Lc77e: ;
    native_write(ctx, (uint16_t)(0x049au + ctx->x), ctx->a);
    return;
    return;
}

void smbneo_native_fn_actor_B_10_init_c755(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xc755u);
    ctx->y = native_nz(ctx, 0x40u);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x)));
    if (!(ctx->status & NATIVE_N)) {
        goto Lc762;
    }
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a ^ 0xffu));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x01u);
    ctx->y = native_nz(ctx, 0xc0u);
Lc762: ;
    native_write(ctx, (uint16_t)(0x0401u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, ctx->y);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x)));
    native_write(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x), ctx->a);
    smbneo_native_fn_actor_init_veloc_y_c2a6(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, 0x05u);
    ctx->y = native_nz(ctx, native_read(ctx, 0x074eu));
    native_cmp(ctx, ctx->y, 0x03u);
    if ((ctx->status & NATIVE_Z)) {
        goto Lc77e;
    }
    ctx->y = native_nz(ctx, native_read(ctx, 0x06ccu));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lc77e;
    }
    ctx->a = native_nz(ctx, 0x06u);
Lc77e: ;
    native_write(ctx, (uint16_t)(0x049au + ctx->x), ctx->a);
    return;
    return;
}

void smbneo_native_fn_actor_B_11_init_c782(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xc782u);
    goto Lc782;
Lc76e: ;
    ctx->a = native_nz(ctx, 0x05u);
    ctx->y = native_nz(ctx, native_read(ctx, 0x074eu));
    native_cmp(ctx, ctx->y, 0x03u);
    if ((ctx->status & NATIVE_Z)) {
        goto Lc77e;
    }
    ctx->y = native_nz(ctx, native_read(ctx, 0x06ccu));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lc77e;
    }
    ctx->a = native_nz(ctx, 0x06u);
Lc77e: ;
    native_write(ctx, (uint16_t)(0x049au + ctx->x), ctx->a);
    return;
Lc782: ;
    smbneo_native_fn_actor_B_16_init_c78e(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    /* static tail transfer */
    goto Lc78b;
Lc78b: ;
    /* static tail transfer */
    goto Lc76e;
    return;
}

void smbneo_native_fn_actor_B_12_init_c788(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xc788u);
    goto Lc788;
Lc76e: ;
    ctx->a = native_nz(ctx, 0x05u);
    ctx->y = native_nz(ctx, native_read(ctx, 0x074eu));
    native_cmp(ctx, ctx->y, 0x03u);
    if ((ctx->status & NATIVE_Z)) {
        goto Lc77e;
    }
    ctx->y = native_nz(ctx, native_read(ctx, 0x06ccu));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lc77e;
    }
    ctx->a = native_nz(ctx, 0x06u);
Lc77e: ;
    native_write(ctx, (uint16_t)(0x049au + ctx->x), ctx->a);
    return;
Lc788: ;
    smbneo_native_fn_actor_B_17_init_c79a(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    /* static tail transfer */
    goto Lc76e;
    return;
}

void smbneo_native_fn_actor_B_16_init_c78e(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xc78eu);
    ctx->a = native_nz(ctx, 0x10u);
    native_write(ctx, (uint16_t)(0x0434u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0xffu);
    native_write(ctx, (uint16_t)(uint8_t)(0xa0u + ctx->x), ctx->a);
    /* static tail transfer */
    goto Lc7a3;
Lc7a3: ;
    ctx->y = native_nz(ctx, 0x01u);
    smbneo_native_fn_actor_plat_pos_c7b4(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, 0x04u);
    native_write(ctx, (uint16_t)(0x049au + ctx->x), ctx->a);
    return;
    return;
}

void smbneo_native_fn_actor_B_17_init_c79a(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xc79au);
    ctx->a = native_nz(ctx, 0xf0u);
    native_write(ctx, (uint16_t)(0x0434u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, (uint16_t)(uint8_t)(0xa0u + ctx->x), ctx->a);
    ctx->y = native_nz(ctx, 0x01u);
    smbneo_native_fn_actor_plat_pos_c7b4(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, 0x04u);
    native_write(ctx, (uint16_t)(0x049au + ctx->x), ctx->a);
    return;
    return;
}

void smbneo_native_fn_actor_plat_pos_c7b4(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xc7b4u);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x87u + ctx->x)));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, (uint16_t)(0xc7aeu + ctx->y)));
    native_write(ctx, (uint16_t)(uint8_t)(0x87u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x6eu + ctx->x)));
    native_adc(ctx, native_read(ctx, (uint16_t)(0xc7b1u + ctx->y)));
    native_write(ctx, (uint16_t)(uint8_t)(0x6eu + ctx->x), ctx->a);
    return;
    return;
}

void smbneo_native_fn_actor_B_36_init_c7c4(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xc7c4u);
    return;
    return;
}

void smbneo_native_fn_actor_B_02_proc_c819(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xc819u);
    return;
    return;
}

void smbneo_native_fn_actor_B_20_proc_c81a(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xc81au);
    smbneo_native_fn_pos_bits_get_actor_f114(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_pos_calc_x_rel_actor_f0b7(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    /* static tail transfer */
    smbneo_native_fn_render_actor_e7da(ctx);
    return;
    return;
}

void smbneo_native_fn_actor_proc_enemy_c823(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xc823u);
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, (uint16_t)(0x03c5u + ctx->x), ctx->a);
    smbneo_native_fn_pos_bits_get_actor_f114(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_pos_calc_x_rel_actor_f0b7(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_render_actor_e7da(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_col_actor_box_e199(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_col_actor_ground_proc_df17(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_col_actor_actor_proc_d98a(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_col_player_actor_proc_d7aa(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->y = native_nz(ctx, native_read(ctx, 0x0747u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lc845;
    }
    smbneo_native_fn_actor_proc_enemy_do_c848(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Lc845: ;
    /* static tail transfer */
    smbneo_native_fn_actor_oob_proc_d5bd(ctx);
    return;
    return;
}

void smbneo_native_fn_actor_proc_enemy_do_c848(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xc848u);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x16u + ctx->x)));
    uint8_t table_selector_c84a = ctx->a;
    ctx->a = native_asl(ctx, ctx->a);
    ctx->y = native_nz(ctx, ctx->a);
    native_write(ctx, 0x04u, 0x4cu);
    native_write(ctx, 0x05u, 0xc8u);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    switch (table_selector_c84a) {
    case 0: native_write(ctx, 0x06u, 0xbau);
             native_write(ctx, 0x07u, 0xc9u);
             ctx->a = native_nz(ctx, 0xc9u);
             smbneo_native_fn_actor_A_00_proc_c9ba(ctx); return;
    case 1: native_write(ctx, 0x06u, 0xbau);
             native_write(ctx, 0x07u, 0xc9u);
             ctx->a = native_nz(ctx, 0xc9u);
             smbneo_native_fn_actor_A_00_proc_c9ba(ctx); return;
    case 2: native_write(ctx, 0x06u, 0xbau);
             native_write(ctx, 0x07u, 0xc9u);
             ctx->a = native_nz(ctx, 0xc9u);
             smbneo_native_fn_actor_A_00_proc_c9ba(ctx); return;
    case 3: native_write(ctx, 0x06u, 0xbau);
             native_write(ctx, 0x07u, 0xc9u);
             ctx->a = native_nz(ctx, 0xc9u);
             smbneo_native_fn_actor_A_00_proc_c9ba(ctx); return;
    case 4: native_write(ctx, 0x06u, 0xbau);
             native_write(ctx, 0x07u, 0xc9u);
             ctx->a = native_nz(ctx, 0xc9u);
             smbneo_native_fn_actor_A_00_proc_c9ba(ctx); return;
    case 5: native_write(ctx, 0x06u, 0x1bu);
             native_write(ctx, 0x07u, 0xc9u);
             ctx->a = native_nz(ctx, 0xc9u);
             smbneo_native_fn_actor_A_05_proc_c91b(ctx); return;
    case 6: native_write(ctx, 0x06u, 0xbau);
             native_write(ctx, 0x07u, 0xc9u);
             ctx->a = native_nz(ctx, 0xc9u);
             smbneo_native_fn_actor_A_00_proc_c9ba(ctx); return;
    case 7: native_write(ctx, 0x06u, 0xccu);
             native_write(ctx, 0x07u, 0xcau);
             ctx->a = native_nz(ctx, 0xcau);
             smbneo_native_fn_actor_A_07_proc_cacc(ctx); return;
    case 8: native_write(ctx, 0x06u, 0x79u);
             native_write(ctx, 0x07u, 0xcbu);
             ctx->a = native_nz(ctx, 0xcbu);
             smbneo_native_fn_actor_A_08_proc_cb79(ctx); return;
    case 9: native_write(ctx, 0x06u, 0x77u);
             native_write(ctx, 0x07u, 0xc8u);
             ctx->a = native_nz(ctx, 0xc8u);
             smbneo_native_fn_actor_A_09_proc_c877(ctx); return;
    case 10: native_write(ctx, 0x06u, 0x8du);
             native_write(ctx, 0x07u, 0xcbu);
             ctx->a = native_nz(ctx, 0xcbu);
             smbneo_native_fn_actor_A_0A_proc_cb8d(ctx); return;
    case 11: native_write(ctx, 0x06u, 0x8du);
             native_write(ctx, 0x07u, 0xcbu);
             ctx->a = native_nz(ctx, 0xcbu);
             smbneo_native_fn_actor_A_0A_proc_cb8d(ctx); return;
    case 12: native_write(ctx, 0x06u, 0xf3u);
             native_write(ctx, 0x07u, 0xc8u);
             ctx->a = native_nz(ctx, 0xc8u);
             smbneo_native_fn_actor_A_0C_proc_c8f3(ctx); return;
    case 13: native_write(ctx, 0x06u, 0xf3u);
             native_write(ctx, 0x07u, 0xd2u);
             ctx->a = native_nz(ctx, 0xd2u);
             smbneo_native_fn_actor_A_0D_proc_d2f3(ctx); return;
    case 14: native_write(ctx, 0x06u, 0x3cu);
             native_write(ctx, 0x07u, 0xcau);
             ctx->a = native_nz(ctx, 0xcau);
             smbneo_native_fn_actor_A_0E_proc_ca3c(ctx); return;
    case 15: native_write(ctx, 0x06u, 0x42u);
             native_write(ctx, 0x07u, 0xcau);
             ctx->a = native_nz(ctx, 0xcau);
             smbneo_native_fn_actor_A_0F_proc_ca42(ctx); return;
    case 16: native_write(ctx, 0x06u, 0x68u);
             native_write(ctx, 0x07u, 0xcau);
             ctx->a = native_nz(ctx, 0xcau);
             smbneo_native_fn_actor_A_10_proc_ca68(ctx); return;
    case 17: native_write(ctx, 0x06u, 0x6bu);
             native_write(ctx, 0x07u, 0xceu);
             ctx->a = native_nz(ctx, 0xceu);
             smbneo_native_fn_actor_A_11_proc_ce6b(ctx); return;
    case 18: native_write(ctx, 0x06u, 0xbau);
             native_write(ctx, 0x07u, 0xc9u);
             ctx->a = native_nz(ctx, 0xc9u);
             smbneo_native_fn_actor_A_00_proc_c9ba(ctx); return;
    case 19: native_write(ctx, 0x06u, 0x77u);
             native_write(ctx, 0x07u, 0xc8u);
             ctx->a = native_nz(ctx, 0xc8u);
             smbneo_native_fn_actor_A_09_proc_c877(ctx); return;
    case 20: native_write(ctx, 0x06u, 0x22u);
             native_write(ctx, 0x07u, 0xceu);
             ctx->a = native_nz(ctx, 0xceu);
             smbneo_native_fn_actor_A_14_proc_ce22(ctx); return;
    default: ctx->unsupported = 1; return;
    }
    return;
}

void smbneo_native_fn_actor_A_09_proc_c877(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xc877u);
    return;
    return;
}

void smbneo_native_fn_actor_B_00_proc_c878(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xc878u);
    smbneo_native_fn_actor_proc_bowser_flame_do_d12e(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_pos_bits_get_actor_f114(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_pos_calc_x_rel_actor_f0b7(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_col_actor_box_e199(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_col_player_actor_proc_d7aa(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    /* static tail transfer */
    smbneo_native_fn_actor_oob_proc_d5bd(ctx);
    return;
    return;
}

void smbneo_native_fn_actor_B_06_proc_c88a(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xc88au);
    smbneo_native_fn_actor_proc_firebar_do_cc7f(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    /* static tail transfer */
    smbneo_native_fn_actor_oob_proc_d5bd(ctx);
    return;
    return;
}

void smbneo_native_fn_actor_B_16_proc_c890(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xc890u);
    smbneo_native_fn_pos_bits_get_actor_f114(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_pos_calc_x_rel_actor_f0b7(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_col_plat_s_box_e1a2(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_col_plat_s_proc_dad2(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_pos_calc_x_rel_actor_f0b7(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_render_plat_small_ecc3(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_actor_proc_plat_small_do_d598(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    /* static tail transfer */
    smbneo_native_fn_actor_oob_proc_d5bd(ctx);
    return;
    return;
}

void smbneo_native_fn_actor_B_0F_proc_c8a8(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xc8a8u);
    smbneo_native_fn_pos_bits_get_actor_f114(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_pos_calc_x_rel_actor_f0b7(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_col_plat_l_box_e1c9(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_col_plat_l_proc_da9c(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, 0x0747u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lc8bc;
    }
    smbneo_native_fn_actor_proc_plat_do_c8c5(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Lc8bc: ;
    smbneo_native_fn_pos_calc_x_rel_actor_f0b7(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_render_plat_large_e525(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    /* static tail transfer */
    smbneo_native_fn_actor_oob_proc_d5bd(ctx);
    return;
    return;
}

void smbneo_native_fn_actor_proc_plat_do_c8c5(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xc8c5u);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x16u + ctx->x)));
    ctx->status |= NATIVE_C;
    native_sbc(ctx, 0x24u);
    uint8_t table_selector_c8ca = ctx->a;
    ctx->a = native_asl(ctx, ctx->a);
    ctx->y = native_nz(ctx, ctx->a);
    native_write(ctx, 0x04u, 0xccu);
    native_write(ctx, 0x05u, 0xc8u);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    switch (table_selector_c8ca) {
    case 0: native_write(ctx, 0x06u, 0x75u);
             native_write(ctx, 0x07u, 0xd3u);
             ctx->a = native_nz(ctx, 0xd3u);
             smbneo_native_fn_actor_proc_plat_bal_d375(ctx); return;
    case 1: native_write(ctx, 0x06u, 0x16u);
             native_write(ctx, 0x07u, 0xd5u);
             ctx->a = native_nz(ctx, 0xd5u);
             smbneo_native_fn_actor_proc_plat_mov_v_d516(ctx); return;
    case 2: native_write(ctx, 0x06u, 0x92u);
             native_write(ctx, 0x07u, 0xd5u);
             ctx->a = native_nz(ctx, 0xd5u);
             smbneo_native_fn_actor_proc_plat_lift_d592(ctx); return;
    case 3: native_write(ctx, 0x06u, 0x92u);
             native_write(ctx, 0x07u, 0xd5u);
             ctx->a = native_nz(ctx, 0xd5u);
             smbneo_native_fn_actor_proc_plat_lift_d592(ctx); return;
    case 4: native_write(ctx, 0x06u, 0x4au);
             native_write(ctx, 0x07u, 0xd5u);
             ctx->a = native_nz(ctx, 0xd5u);
             smbneo_native_fn_actor_proc_plat_move_h_d54a(ctx); return;
    case 5: native_write(ctx, 0x06u, 0x74u);
             native_write(ctx, 0x07u, 0xd5u);
             ctx->a = native_nz(ctx, 0xd5u);
             smbneo_native_fn_actor_proc_plat_drop_d574(ctx); return;
    case 6: native_write(ctx, 0x06u, 0x80u);
             native_write(ctx, 0x07u, 0xd5u);
             ctx->a = native_nz(ctx, 0xd5u);
             smbneo_native_fn_actor_proc_plat_right_d580(ctx); return;
    default: ctx->unsupported = 1; return;
    }
    return;
}

void smbneo_native_fn_actor_erase_c8db(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xc8dbu);
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, (uint16_t)(uint8_t)(0x0fu + ctx->x), ctx->a);
    native_write(ctx, (uint16_t)(uint8_t)(0x16u + ctx->x), ctx->a);
    native_write(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x), ctx->a);
    native_write(ctx, (uint16_t)(0x0110u + ctx->x), ctx->a);
    native_write(ctx, (uint16_t)(0x0796u + ctx->x), ctx->a);
    native_write(ctx, (uint16_t)(0x0125u + ctx->x), ctx->a);
    native_write(ctx, (uint16_t)(0x03c5u + ctx->x), ctx->a);
    native_write(ctx, (uint16_t)(0x078au + ctx->x), ctx->a);
    return;
    return;
}

void smbneo_native_fn_actor_A_0C_proc_c8f3(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xc8f3u);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0796u + ctx->x)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lc90e;
    }
    smbneo_native_fn_actor_A_0C_init_c240(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x07a8u + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | 0x80u));
    native_write(ctx, (uint16_t)(0x0434u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x0fu));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | 0x06u));
    native_write(ctx, (uint16_t)(0x0796u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0xf9u);
    native_write(ctx, (uint16_t)(uint8_t)(0xa0u + ctx->x), ctx->a);
Lc90e: ;
    /* static tail transfer */
    smbneo_native_fn_motion_fall_mid_bea1(ctx);
    return;
    return;
}

void smbneo_native_fn_actor_A_05_proc_c91b(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xc91bu);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x20u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lc924;
    }
    /* static tail transfer */
    goto Lca28;
Lc924: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x3cu + ctx->x)));
    if ((ctx->status & NATIVE_Z)) {
        goto Lc955;
    }
    native_write(ctx, (uint16_t)(uint8_t)(0x3cu + ctx->x), native_nz(ctx, (uint8_t)(native_read(ctx, (uint16_t)(uint8_t)(0x3cu + ctx->x)) - 1u)));
    ctx->a = native_nz(ctx, native_read(ctx, 0x03d1u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x0cu));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lc99b;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x03a2u + ctx->x)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lc94d;
    }
    ctx->y = native_nz(ctx, native_read(ctx, 0x06ccu));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xc911u + ctx->y)));
    native_write(ctx, (uint16_t)(0x03a2u + ctx->x), ctx->a);
    smbneo_native_fn_misc_init_hammer_b988(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    if (!(ctx->status & NATIVE_C)) {
        goto Lc94d;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | 0x08u));
    native_write(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x), ctx->a);
    /* static tail transfer */
    goto Lc99b;
Lc94d: ;
    native_write(ctx, (uint16_t)(0x03a2u + ctx->x), native_nz(ctx, (uint8_t)(native_read(ctx, (uint16_t)(0x03a2u + ctx->x)) - 1u)));
    /* static tail transfer */
    goto Lc99b;
Lc955: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x07u));
    native_cmp(ctx, ctx->a, 0x01u);
    if ((ctx->status & NATIVE_Z)) {
        goto Lc99b;
    }
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x00u, ctx->a);
    ctx->y = native_nz(ctx, 0xfau);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x)));
    if ((ctx->status & NATIVE_N)) {
        goto Lc97a;
    }
    ctx->y = native_nz(ctx, 0xfdu);
    native_cmp(ctx, ctx->a, 0x70u);
    native_write(ctx, 0x00u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x00u) + 1u)));
    if (!(ctx->status & NATIVE_C)) {
        goto Lc97a;
    }
    native_write(ctx, 0x00u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x00u) - 1u)));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x07a8u + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x01u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lc97a;
    }
    ctx->y = native_nz(ctx, 0xfau);
Lc97a: ;
    native_write(ctx, (uint16_t)(uint8_t)(0xa0u + ctx->x), ctx->y);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | 0x01u));
    native_write(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x00u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & native_read(ctx, (uint16_t)(0x07a9u + ctx->x))));
    ctx->y = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x06ccu));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lc98e;
    }
    ctx->y = native_nz(ctx, ctx->a);
Lc98e: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xc953u + ctx->y)));
    native_write(ctx, (uint16_t)(0x078au + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x07a8u + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | 0xc0u));
    native_write(ctx, (uint16_t)(uint8_t)(0x3cu + ctx->x), ctx->a);
Lc99b: ;
    ctx->y = native_nz(ctx, 0xfcu);
    ctx->a = native_nz(ctx, native_read(ctx, 0x09u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x40u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lc9a5;
    }
    ctx->y = native_nz(ctx, 0x04u);
Lc9a5: ;
    native_write(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x), ctx->y);
    ctx->y = native_nz(ctx, 0x01u);
    smbneo_native_fn_col_player_bullet_diff_e099(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    if ((ctx->status & NATIVE_N)) {
        goto Lc9b8;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0796u + ctx->x)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lc9b8;
    }
    ctx->a = native_nz(ctx, 0xf8u);
    native_write(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x), ctx->a);
Lc9b8: ;
    native_write(ctx, (uint16_t)(uint8_t)(0x46u + ctx->x), ctx->y);
    smbneo_native_fn_actor_A_00_proc_c9ba(ctx);
    return;
Lca28: ;
    smbneo_native_fn_motion_y_be72(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    /* static tail transfer */
    smbneo_native_fn_motion_x_be11(ctx);
    return;
    return;
}

void smbneo_native_fn_actor_A_00_proc_c9ba(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xc9bau);
#ifdef SMBNEO_NATIVE_FAST_PATHS
    if (vs_native_fast_actor_proc_base(ctx)) return;
#endif
    uint8_t saved_value_0 = 0u;
    ctx->y = native_nz(ctx, 0x00u);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x40u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lc9db;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x)));
    ctx->a = native_asl(ctx, ctx->a);
    if ((ctx->status & NATIVE_C)) {
        goto Lc9f7;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x20u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lca28;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x07u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lc9f7;
    }
    native_cmp(ctx, ctx->a, 0x05u);
    if ((ctx->status & NATIVE_Z)) {
        goto Lc9db;
    }
    native_cmp(ctx, ctx->a, 0x03u);
    if ((ctx->status & NATIVE_C)) {
        goto Lca0b;
    }
Lc9db: ;
    smbneo_native_fn_motion_y_be72(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->y = native_nz(ctx, 0x00u);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x)));
    native_cmp(ctx, ctx->a, 0x02u);
    if ((ctx->status & NATIVE_Z)) {
        goto Lc9f2;
    }
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x40u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lc9f7;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x16u + ctx->x)));
    native_cmp(ctx, ctx->a, 0x2eu);
    if ((ctx->status & NATIVE_Z)) {
        goto Lc9f7;
    }
    if (!(ctx->status & NATIVE_Z)) {
        goto Lc9f5;
    }
Lc9f2: ;
    /* static tail transfer */
    smbneo_native_fn_motion_x_be11(ctx);
    return;
Lc9f5: ;
    ctx->y = native_nz(ctx, 0x01u);
Lc9f7: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x)));
    saved_value_0 = ctx->a;
    if (!(ctx->status & NATIVE_N)) {
        goto Lc9fe;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
Lc9fe: ;
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, (uint16_t)(0xc913u + ctx->y)));
    native_write(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x), ctx->a);
    smbneo_native_fn_motion_x_be11(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, saved_value_0);
    native_write(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x), ctx->a);
    return;
Lca0b: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0796u + ctx->x)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lca2e;
    }
    native_write(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x09u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x01u));
    ctx->y = native_nz(ctx, ctx->a);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    native_write(ctx, (uint16_t)(uint8_t)(0x46u + ctx->x), ctx->y);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    ctx->a = native_nz(ctx, native_read(ctx, 0x076au));
    if ((ctx->status & NATIVE_Z)) {
        goto Lca22;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
Lca22: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xc917u + ctx->y)));
    native_write(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x), ctx->a);
    return;
Lca28: ;
    smbneo_native_fn_motion_y_be72(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    /* static tail transfer */
    smbneo_native_fn_motion_x_be11(ctx);
    return;
Lca2e: ;
    native_cmp(ctx, ctx->a, 0x0eu);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lca3b;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x16u + ctx->x)));
    native_cmp(ctx, ctx->a, 0x06u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lca3b;
    }
    smbneo_native_fn_actor_erase_c8db(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Lca3b: ;
    return;
    return;
}

void smbneo_native_fn_actor_A_0E_proc_ca3c(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xca3cu);
    smbneo_native_fn_motion_fall_mid_bea1(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    /* static tail transfer */
    smbneo_native_fn_motion_x_be11(ctx);
    return;
    return;
}

void smbneo_native_fn_actor_A_0F_proc_ca42(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xca42u);
    goto Lca42;
Lbe7f: ;
    ctx->y = native_nz(ctx, 0x00u);
    /* static tail transfer */
    goto Lbe86;
Lbe84: ;
    ctx->y = native_nz(ctx, 0x01u);
Lbe86: ;
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    ctx->a = native_nz(ctx, 0x03u);
    native_write(ctx, 0x00u, ctx->a);
    ctx->a = native_nz(ctx, 0x06u);
    native_write(ctx, 0x01u, ctx->a);
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, 0x02u, ctx->a);
    ctx->a = native_nz(ctx, ctx->y);
    /* static tail transfer */
    goto Lbee4;
Lbee4: ;
    smbneo_native_fn_motion_gravity_beea(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    return;
Lca42: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xa0u + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | native_read(ctx, (uint16_t)(0x0434u + ctx->x))));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lca5c;
    }
    native_write(ctx, (uint16_t)(0x0417u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x)));
    native_cmp(ctx, ctx->a, native_read(ctx, (uint16_t)(0x0401u + ctx->x)));
    if ((ctx->status & NATIVE_C)) {
        goto Lca5c;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x09u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x07u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lca5b;
    }
    native_write(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x), native_nz(ctx, (uint8_t)(native_read(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x)) + 1u)));
Lca5b: ;
    return;
Lca5c: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x)));
    native_cmp(ctx, ctx->a, native_read(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x)));
    if (!(ctx->status & NATIVE_C)) {
        goto Lca65;
    }
    /* static tail transfer */
    goto Lbe84;
Lca65: ;
    /* static tail transfer */
    goto Lbe7f;
    return;
}

void smbneo_native_fn_actor_A_10_proc_ca68(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xca68u);
    smbneo_native_fn_actor_floater_koopa_ca88(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_actor_floater_move_caa9(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->y = native_nz(ctx, 0x01u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x09u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x03u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lca87;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x09u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x40u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lca7e;
    }
    ctx->y = native_nz(ctx, 0xffu);
Lca7e: ;
    native_write(ctx, 0x00u, ctx->y);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x)));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, 0x00u));
    native_write(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x), ctx->a);
Lca87: ;
    return;
    return;
}

void smbneo_native_fn_actor_floater_koopa_ca88(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xca88u);
    ctx->a = native_nz(ctx, 0x13u);
    smbneo_native_fn_actor_floater_do_ca8a(ctx);
    return;
    return;
}

void smbneo_native_fn_actor_floater_do_ca8a(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xca8au);
    native_write(ctx, 0x01u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x09u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x03u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lca9f;
    }
    ctx->y = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x)));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xa0u + ctx->x)));
    ctx->a = native_lsr(ctx, ctx->a);
    if ((ctx->status & NATIVE_C)) {
        goto Lcaa3;
    }
    native_cmp(ctx, ctx->y, native_read(ctx, 0x01u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lcaa0;
    }
    native_write(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x), native_nz(ctx, (uint8_t)(native_read(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x)) + 1u)));
Lca9f: ;
    return;
Lcaa0: ;
    native_write(ctx, (uint16_t)(uint8_t)(0xa0u + ctx->x), native_nz(ctx, (uint8_t)(native_read(ctx, (uint16_t)(uint8_t)(0xa0u + ctx->x)) + 1u)));
    return;
Lcaa3: ;
    ctx->a = native_nz(ctx, ctx->y);
    if ((ctx->status & NATIVE_Z)) {
        goto Lcaa0;
    }
    native_write(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x), native_nz(ctx, (uint8_t)(native_read(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x)) - 1u)));
    return;
    return;
}

void smbneo_native_fn_actor_floater_move_caa9(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xcaa9u);
    uint8_t saved_value_0 = 0u;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x)));
    saved_value_0 = ctx->a;
    ctx->y = native_nz(ctx, 0x01u);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xa0u + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x02u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lcabf;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a ^ 0xffu));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x01u);
    native_write(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x), ctx->a);
    ctx->y = native_nz(ctx, 0x02u);
Lcabf: ;
    native_write(ctx, (uint16_t)(uint8_t)(0x46u + ctx->x), ctx->y);
    smbneo_native_fn_motion_x_be11(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_write(ctx, 0x00u, ctx->a);
    ctx->a = native_nz(ctx, saved_value_0);
    native_write(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x), ctx->a);
    return;
    return;
}

void smbneo_native_fn_actor_A_07_proc_cacc(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xcaccu);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x20u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lcb1f;
    }
    ctx->y = native_nz(ctx, native_read(ctx, 0x06ccu));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x07a8u + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & native_read(ctx, (uint16_t)(0xcacau + ctx->y))));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lcaef;
    }
    ctx->a = native_nz(ctx, ctx->x);
    ctx->a = native_lsr(ctx, ctx->a);
    if (!(ctx->status & NATIVE_C)) {
        goto Lcae5;
    }
    ctx->y = native_nz(ctx, native_read(ctx, 0x45u));
    if ((ctx->status & NATIVE_C)) {
        goto Lcaed;
    }
Lcae5: ;
    ctx->y = native_nz(ctx, 0x02u);
    smbneo_native_fn_col_player_bullet_diff_e099(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    if (!(ctx->status & NATIVE_N)) {
        goto Lcaed;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
Lcaed: ;
    native_write(ctx, (uint16_t)(uint8_t)(0x46u + ctx->x), ctx->y);
Lcaef: ;
    smbneo_native_fn_actor_proc_swim_cb22(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x)));
    ctx->status |= NATIVE_C;
    native_sbc(ctx, native_read(ctx, (uint16_t)(0x0434u + ctx->x)));
    native_cmp(ctx, ctx->a, 0x20u);
    if (!(ctx->status & NATIVE_C)) {
        goto Lcafe;
    }
    native_write(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x), ctx->a);
Lcafe: ;
    ctx->y = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x46u + ctx->x)));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lcb11;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x87u + ctx->x)));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x)));
    native_write(ctx, (uint16_t)(uint8_t)(0x87u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x6eu + ctx->x)));
    native_adc(ctx, 0x00u);
    native_write(ctx, (uint16_t)(uint8_t)(0x6eu + ctx->x), ctx->a);
    return;
Lcb11: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x87u + ctx->x)));
    ctx->status |= NATIVE_C;
    native_sbc(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x)));
    native_write(ctx, (uint16_t)(uint8_t)(0x87u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x6eu + ctx->x)));
    native_sbc(ctx, 0x00u);
    native_write(ctx, (uint16_t)(uint8_t)(0x6eu + ctx->x), ctx->a);
    return;
Lcb1f: ;
    /* static tail transfer */
    smbneo_native_fn_motion_fall_slow_be9b(ctx);
    return;
    return;
}

void smbneo_native_fn_actor_proc_swim_cb22(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xcb22u);
    uint8_t saved_value_0 = 0u;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xa0u + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x02u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lcb5f;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x09u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x07u));
    saved_value_0 = ctx->a;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xa0u + ctx->x)));
    ctx->a = native_lsr(ctx, ctx->a);
    if ((ctx->status & NATIVE_C)) {
        goto Lcb47;
    }
    ctx->a = native_nz(ctx, saved_value_0);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lcb46;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0434u + ctx->x)));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x01u);
    native_write(ctx, (uint16_t)(0x0434u + ctx->x), ctx->a);
    native_write(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x), ctx->a);
    native_cmp(ctx, ctx->a, 0x02u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lcb46;
    }
    native_write(ctx, (uint16_t)(uint8_t)(0xa0u + ctx->x), native_nz(ctx, (uint8_t)(native_read(ctx, (uint16_t)(uint8_t)(0xa0u + ctx->x)) + 1u)));
Lcb46: ;
    return;
Lcb47: ;
    ctx->a = native_nz(ctx, saved_value_0);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lcb5e;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0434u + ctx->x)));
    ctx->status |= NATIVE_C;
    native_sbc(ctx, 0x01u);
    native_write(ctx, (uint16_t)(0x0434u + ctx->x), ctx->a);
    native_write(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x), ctx->a);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lcb5e;
    }
    native_write(ctx, (uint16_t)(uint8_t)(0xa0u + ctx->x), native_nz(ctx, (uint8_t)(native_read(ctx, (uint16_t)(uint8_t)(0xa0u + ctx->x)) + 1u)));
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, (uint16_t)(0x0796u + ctx->x), ctx->a);
Lcb5e: ;
    return;
Lcb5f: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0796u + ctx->x)));
    if ((ctx->status & NATIVE_Z)) {
        goto Lcb6c;
    }
Lcb64: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x09u));
    ctx->a = native_lsr(ctx, ctx->a);
    if ((ctx->status & NATIVE_C)) {
        goto Lcb6b;
    }
    native_write(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x), native_nz(ctx, (uint8_t)(native_read(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x)) + 1u)));
Lcb6b: ;
    return;
Lcb6c: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x)));
    native_adc(ctx, 0x10u);
    native_cmp(ctx, ctx->a, native_read(ctx, 0xceu));
    if (!(ctx->status & NATIVE_C)) {
        goto Lcb64;
    }
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, (uint16_t)(uint8_t)(0xa0u + ctx->x), ctx->a);
    return;
    return;
}

void smbneo_native_fn_actor_A_08_proc_cb79(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xcb79u);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x20u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lcb82;
    }
    /* static tail transfer */
    smbneo_native_fn_motion_fall_mid_bea1(ctx);
    return;
Lcb82: ;
    ctx->a = native_nz(ctx, 0xe8u);
    native_write(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x), ctx->a);
    /* static tail transfer */
    smbneo_native_fn_motion_x_be11(ctx);
    return;
    return;
}

void smbneo_native_fn_actor_A_0A_proc_cb8d(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xcb8du);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x20u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lcb96;
    }
    /* static tail transfer */
    smbneo_native_fn_motion_fall_slow_be9b(ctx);
    return;
Lcb96: ;
    native_write(ctx, 0x03u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x16u + ctx->x)));
    ctx->status |= NATIVE_C;
    native_sbc(ctx, 0x0au);
    ctx->y = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xcb89u + ctx->y)));
    native_write(ctx, 0x02u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0401u + ctx->x)));
    ctx->status |= NATIVE_C;
    native_sbc(ctx, native_read(ctx, 0x02u));
    native_write(ctx, (uint16_t)(0x0401u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x87u + ctx->x)));
    native_sbc(ctx, 0x00u);
    native_write(ctx, (uint16_t)(uint8_t)(0x87u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x6eu + ctx->x)));
    native_sbc(ctx, 0x00u);
    native_write(ctx, (uint16_t)(uint8_t)(0x6eu + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x20u);
    native_write(ctx, 0x02u, ctx->a);
    native_cmp(ctx, ctx->x, 0x02u);
    if (!(ctx->status & NATIVE_C)) {
        goto Lcc09;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x)));
    native_cmp(ctx, ctx->a, 0x10u);
    if (!(ctx->status & NATIVE_C)) {
        goto Lcbdc;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0417u + ctx->x)));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, 0x02u));
    native_write(ctx, (uint16_t)(0x0417u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x)));
    native_adc(ctx, native_read(ctx, 0x03u));
    native_write(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xb6u + ctx->x)));
    native_adc(ctx, 0x00u);
    /* static tail transfer */
    goto Lcbef;
Lcbdc: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0417u + ctx->x)));
    ctx->status |= NATIVE_C;
    native_sbc(ctx, native_read(ctx, 0x02u));
    native_write(ctx, (uint16_t)(0x0417u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x)));
    native_sbc(ctx, native_read(ctx, 0x03u));
    native_write(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xb6u + ctx->x)));
    native_sbc(ctx, 0x00u);
Lcbef: ;
    native_write(ctx, (uint16_t)(uint8_t)(0xb6u + ctx->x), ctx->a);
    ctx->y = native_nz(ctx, 0x00u);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x)));
    ctx->status |= NATIVE_C;
    native_sbc(ctx, native_read(ctx, (uint16_t)(0x0434u + ctx->x)));
    if (!(ctx->status & NATIVE_N)) {
        goto Lcc02;
    }
    ctx->y = native_nz(ctx, 0x10u);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a ^ 0xffu));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x01u);
Lcc02: ;
    native_cmp(ctx, ctx->a, 0x0fu);
    if (!(ctx->status & NATIVE_C)) {
        goto Lcc09;
    }
    ctx->a = native_nz(ctx, ctx->y);
    native_write(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x), ctx->a);
Lcc09: ;
    return;
    return;
}

void smbneo_native_fn_actor_proc_firebar_do_cc7f(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xcc7fu);
    smbneo_native_fn_pos_bits_get_actor_f114(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, 0x03d1u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x08u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lccfd;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x0747u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lcc98;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0388u + ctx->x)));
    smbneo_native_fn_actor_proc_firebar_spin_d353(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x1fu));
    native_write(ctx, (uint16_t)(uint8_t)(0xa0u + ctx->x), ctx->a);
Lcc98: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xa0u + ctx->x)));
    ctx->y = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x16u + ctx->x)));
    native_cmp(ctx, ctx->y, 0x1fu);
    if (!(ctx->status & NATIVE_C)) {
        goto Lccad;
    }
    native_cmp(ctx, ctx->a, 0x08u);
    if ((ctx->status & NATIVE_Z)) {
        goto Lcca8;
    }
    native_cmp(ctx, ctx->a, 0x18u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lccad;
    }
Lcca8: ;
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x01u);
    native_write(ctx, (uint16_t)(uint8_t)(0xa0u + ctx->x), ctx->a);
Lccad: ;
    native_write(ctx, 0xefu, ctx->a);
    smbneo_native_fn_pos_calc_x_rel_actor_f0b7(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_actor_firebar_get_pos_cdd1(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->y = native_nz(ctx, native_read(ctx, (uint16_t)(0x06e5u + ctx->x)));
    ctx->a = native_nz(ctx, native_read(ctx, 0x03b9u));
    native_write(ctx, (uint16_t)(0x0200u + ctx->y), ctx->a);
    native_write(ctx, 0x07u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x03aeu));
    native_write(ctx, (uint16_t)(0x0203u + ctx->y), ctx->a);
    native_write(ctx, 0x06u, ctx->a);
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, 0x00u, ctx->a);
    smbneo_native_fn_actor_firebar_col_cd4b(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->y = native_nz(ctx, 0x05u);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x16u + ctx->x)));
    native_cmp(ctx, ctx->a, 0x1fu);
    if (!(ctx->status & NATIVE_C)) {
        goto Lccd9;
    }
    ctx->y = native_nz(ctx, 0x0bu);
Lccd9: ;
    native_write(ctx, 0xedu, ctx->y);
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x00u, ctx->a);
Lccdf: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0xefu));
    smbneo_native_fn_actor_firebar_get_pos_cdd1(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_actor_firebar_col_draw_ccfe(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, 0x00u));
    native_cmp(ctx, ctx->a, 0x04u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lccf5;
    }
    ctx->y = native_nz(ctx, native_read(ctx, 0x06cfu));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x06e5u + ctx->y)));
    native_write(ctx, 0x06u, ctx->a);
Lccf5: ;
    native_write(ctx, 0x00u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x00u) + 1u)));
    ctx->a = native_nz(ctx, native_read(ctx, 0x00u));
    native_cmp(ctx, ctx->a, native_read(ctx, 0xedu));
    if (!(ctx->status & NATIVE_C)) {
        goto Lccdf;
    }
Lccfd: ;
    return;
    return;
}

void smbneo_native_fn_actor_firebar_col_draw_ccfe(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xccfeu);
    ctx->a = native_nz(ctx, native_read(ctx, 0x03u));
    native_write(ctx, 0x05u, ctx->a);
    ctx->y = native_nz(ctx, native_read(ctx, 0x06u));
    ctx->a = native_nz(ctx, native_read(ctx, 0x01u));
    native_write(ctx, 0x05u, native_lsr(ctx, native_read(ctx, 0x05u)));
    if ((ctx->status & NATIVE_C)) {
        goto Lcd0e;
    }
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a ^ 0xffu));
    native_adc(ctx, 0x01u);
Lcd0e: ;
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, 0x03aeu));
    native_write(ctx, (uint16_t)(0x0203u + ctx->y), ctx->a);
    native_write(ctx, 0x06u, ctx->a);
    native_cmp(ctx, ctx->a, native_read(ctx, 0x03aeu));
    if ((ctx->status & NATIVE_C)) {
        goto Lcd25;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x03aeu));
    ctx->status |= NATIVE_C;
    native_sbc(ctx, native_read(ctx, 0x06u));
    /* static tail transfer */
    goto Lcd29;
Lcd25: ;
    ctx->status |= NATIVE_C;
    native_sbc(ctx, native_read(ctx, 0x03aeu));
Lcd29: ;
    native_cmp(ctx, ctx->a, 0x59u);
    if (!(ctx->status & NATIVE_C)) {
        goto Lcd31;
    }
    ctx->a = native_nz(ctx, 0xf8u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lcd46;
    }
Lcd31: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x03b9u));
    native_cmp(ctx, ctx->a, 0xf8u);
    if ((ctx->status & NATIVE_Z)) {
        goto Lcd46;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x02u));
    native_write(ctx, 0x05u, native_lsr(ctx, native_read(ctx, 0x05u)));
    if ((ctx->status & NATIVE_C)) {
        goto Lcd42;
    }
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a ^ 0xffu));
    native_adc(ctx, 0x01u);
Lcd42: ;
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, 0x03b9u));
Lcd46: ;
    native_write(ctx, (uint16_t)(0x0200u + ctx->y), ctx->a);
    native_write(ctx, 0x07u, ctx->a);
    smbneo_native_fn_actor_firebar_col_cd4b(ctx);
    return;
    return;
}

void smbneo_native_fn_actor_firebar_col_cd4b(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xcd4bu);
    uint8_t saved_value_0 = 0u;
    uint8_t saved_value_1 = 0u;
    smbneo_native_fn_render_firebar_ec4a(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, ctx->y);
    saved_value_0 = ctx->a;
    ctx->a = native_nz(ctx, native_read(ctx, 0x079fu));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | native_read(ctx, 0x0747u)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lcdc8;
    }
    native_write(ctx, 0x05u, ctx->a);
    ctx->y = native_nz(ctx, native_read(ctx, 0xb5u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lcdc8;
    }
    ctx->y = native_nz(ctx, native_read(ctx, 0xceu));
    ctx->a = native_nz(ctx, native_read(ctx, 0x0754u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lcd6b;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x0714u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lcd74;
    }
Lcd6b: ;
    native_write(ctx, 0x05u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x05u) + 1u)));
    native_write(ctx, 0x05u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x05u) + 1u)));
    ctx->a = native_nz(ctx, ctx->y);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x18u);
    ctx->y = native_nz(ctx, ctx->a);
Lcd74: ;
    ctx->a = native_nz(ctx, ctx->y);
Lcd75: ;
    ctx->status |= NATIVE_C;
    native_sbc(ctx, native_read(ctx, 0x07u));
    if (!(ctx->status & NATIVE_N)) {
        goto Lcd7f;
    }
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a ^ 0xffu));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x01u);
Lcd7f: ;
    native_cmp(ctx, ctx->a, 0x08u);
    if ((ctx->status & NATIVE_C)) {
        goto Lcd9f;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x06u));
    native_cmp(ctx, ctx->a, 0xf0u);
    if ((ctx->status & NATIVE_C)) {
        goto Lcd9f;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x0207u));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x04u);
    native_write(ctx, 0x04u, ctx->a);
    ctx->status |= NATIVE_C;
    native_sbc(ctx, native_read(ctx, 0x06u));
    if (!(ctx->status & NATIVE_N)) {
        goto Lcd9b;
    }
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a ^ 0xffu));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x01u);
Lcd9b: ;
    native_cmp(ctx, ctx->a, 0x08u);
    if (!(ctx->status & NATIVE_C)) {
        goto Lcdb2;
    }
Lcd9f: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x05u));
    native_cmp(ctx, ctx->a, 0x02u);
    if ((ctx->status & NATIVE_Z)) {
        goto Lcdc8;
    }
    ctx->y = native_nz(ctx, native_read(ctx, 0x05u));
    ctx->a = native_nz(ctx, native_read(ctx, 0xceu));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, (uint16_t)(0xcc7du + ctx->y)));
    native_write(ctx, 0x05u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x05u) + 1u)));
    /* static tail transfer */
    goto Lcd75;
Lcdb2: ;
    ctx->x = native_nz(ctx, 0x01u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x04u));
    native_cmp(ctx, ctx->a, native_read(ctx, 0x06u));
    if ((ctx->status & NATIVE_C)) {
        goto Lcdbb;
    }
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
Lcdbb: ;
    native_write(ctx, 0x46u, ctx->x);
    ctx->x = native_nz(ctx, 0x00u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x00u));
    saved_value_1 = ctx->a;
    smbneo_native_fn_col_player_harm_proc_d883(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, saved_value_1);
    native_write(ctx, 0x00u, ctx->a);
Lcdc8: ;
    ctx->a = native_nz(ctx, saved_value_0);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x04u);
    native_write(ctx, 0x06u, ctx->a);
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    return;
    return;
}

void smbneo_native_fn_actor_firebar_get_pos_cdd1(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xcdd1u);
    uint8_t saved_value_0 = 0u;
    saved_value_0 = ctx->a;
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x0fu));
    native_cmp(ctx, ctx->a, 0x09u);
    if (!(ctx->status & NATIVE_C)) {
        goto Lcddd;
    }
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a ^ 0x0fu));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x01u);
Lcddd: ;
    native_write(ctx, 0x01u, ctx->a);
    ctx->y = native_nz(ctx, native_read(ctx, 0x00u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xcc71u + ctx->y)));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, 0x01u));
    ctx->y = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xcc0au + ctx->y)));
    native_write(ctx, 0x01u, ctx->a);
    ctx->a = native_nz(ctx, saved_value_0);
    saved_value_0 = ctx->a;
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x08u);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x0fu));
    native_cmp(ctx, ctx->a, 0x09u);
    if (!(ctx->status & NATIVE_C)) {
        goto Lcdfd;
    }
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a ^ 0x0fu));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x01u);
Lcdfd: ;
    native_write(ctx, 0x02u, ctx->a);
    ctx->y = native_nz(ctx, native_read(ctx, 0x00u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xcc71u + ctx->y)));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, 0x02u));
    ctx->y = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xcc0au + ctx->y)));
    native_write(ctx, 0x02u, ctx->a);
    ctx->a = native_nz(ctx, saved_value_0);
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->y = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xcc6du + ctx->y)));
    native_write(ctx, 0x03u, ctx->a);
    return;
    return;
}

void smbneo_native_fn_actor_A_14_proc_ce22(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xce22u);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x20u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lce30;
    }
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, (uint16_t)(0x03c5u + ctx->x), ctx->a);
    /* static tail transfer */
    smbneo_native_fn_motion_fall_mid_bea1(ctx);
    return;
Lce30: ;
    smbneo_native_fn_motion_x_be11(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->y = native_nz(ctx, 0x0du);
    ctx->a = native_nz(ctx, 0x05u);
    smbneo_native_fn_motion_fall_bea5(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0434u + ctx->x)));
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->y = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x)));
    ctx->status |= NATIVE_C;
    native_sbc(ctx, native_read(ctx, (uint16_t)(0xce18u + ctx->y)));
    if (!(ctx->status & NATIVE_N)) {
        goto Lce4f;
    }
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a ^ 0xffu));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x01u);
Lce4f: ;
    native_cmp(ctx, ctx->a, 0x08u);
    if ((ctx->status & NATIVE_C)) {
        goto Lce61;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0434u + ctx->x)));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x10u);
    native_write(ctx, (uint16_t)(0x0434u + ctx->x), ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->y = native_nz(ctx, ctx->a);
Lce61: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xce1du + ctx->y)));
    native_write(ctx, (uint16_t)(0x03c5u + ctx->x), ctx->a);
    return;
    return;
}

void smbneo_native_fn_actor_A_11_proc_ce6b(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xce6bu);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x20u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lce74;
    }
    /* static tail transfer */
    smbneo_native_fn_motion_y_be72(ctx);
    return;
Lce74: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x)));
    if ((ctx->status & NATIVE_Z)) {
        goto Lce83;
    }
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, (uint16_t)(uint8_t)(0xa0u + ctx->x), ctx->a);
    native_write(ctx, 0x06cbu, ctx->a);
    ctx->a = native_nz(ctx, 0x10u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lce96;
    }
Lce83: ;
    ctx->a = native_nz(ctx, 0x12u);
    native_write(ctx, 0x06cbu, ctx->a);
    ctx->y = native_nz(ctx, 0x02u);
Lce8a: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xce68u + ctx->y)));
    native_write(ctx, (uint16_t)(0x0001u + ctx->y), ctx->a);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    if (!(ctx->status & NATIVE_N)) {
        goto Lce8a;
    }
    smbneo_native_fn_actor_proc_lakitu_spiny_ceaf(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Lce96: ;
    native_write(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x), ctx->a);
    ctx->y = native_nz(ctx, 0x01u);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xa0u + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x01u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lceaa;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a ^ 0xffu));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x01u);
    native_write(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x), ctx->a);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
Lceaa: ;
    native_write(ctx, (uint16_t)(uint8_t)(0x46u + ctx->x), ctx->y);
    /* static tail transfer */
    smbneo_native_fn_motion_x_be11(ctx);
    return;
    return;
}

void smbneo_native_fn_actor_proc_lakitu_spiny_ceaf(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xceafu);
    ctx->y = native_nz(ctx, 0x00u);
    smbneo_native_fn_col_player_bullet_diff_e099(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    if (!(ctx->status & NATIVE_N)) {
        goto Lcec0;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->a = native_nz(ctx, native_read(ctx, 0x00u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a ^ 0xffu));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x01u);
    native_write(ctx, 0x00u, ctx->a);
Lcec0: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x00u));
    native_cmp(ctx, ctx->a, 0x3cu);
    if (!(ctx->status & NATIVE_C)) {
        goto Lcee2;
    }
    ctx->a = native_nz(ctx, 0x3cu);
    native_write(ctx, 0x00u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x16u + ctx->x)));
    native_cmp(ctx, ctx->a, 0x11u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lcee2;
    }
    ctx->a = native_nz(ctx, ctx->y);
    native_cmp(ctx, ctx->a, native_read(ctx, (uint16_t)(uint8_t)(0xa0u + ctx->x)));
    if ((ctx->status & NATIVE_Z)) {
        goto Lcee2;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xa0u + ctx->x)));
    if ((ctx->status & NATIVE_Z)) {
        goto Lcedf;
    }
    native_write(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x), native_nz(ctx, (uint8_t)(native_read(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x)) - 1u)));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lcf1f;
    }
Lcedf: ;
    ctx->a = native_nz(ctx, ctx->y);
    native_write(ctx, (uint16_t)(uint8_t)(0xa0u + ctx->x), ctx->a);
Lcee2: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x00u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x3cu));
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    native_write(ctx, 0x00u, ctx->a);
    ctx->y = native_nz(ctx, 0x00u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x57u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lcf14;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x0775u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lcf14;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->a = native_nz(ctx, native_read(ctx, 0x57u));
    native_cmp(ctx, ctx->a, 0x19u);
    if (!(ctx->status & NATIVE_C)) {
        goto Lcf04;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x0775u));
    native_cmp(ctx, ctx->a, 0x02u);
    if (!(ctx->status & NATIVE_C)) {
        goto Lcf04;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
Lcf04: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x16u + ctx->x)));
    native_cmp(ctx, ctx->a, 0x12u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lcf0e;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x57u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lcf14;
    }
Lcf0e: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xa0u + ctx->x)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lcf14;
    }
    ctx->y = native_nz(ctx, 0x00u);
Lcf14: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0001u + ctx->y)));
    ctx->y = native_nz(ctx, native_read(ctx, 0x00u));
Lcf19: ;
    ctx->status |= NATIVE_C;
    native_sbc(ctx, 0x01u);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    if (!(ctx->status & NATIVE_N)) {
        goto Lcf19;
    }
Lcf1f: ;
    return;
    return;
}

void smbneo_native_fn_actor_bowser_ending_cf2f(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xcf2fu);
    uint8_t saved_value_0 = 0u;
    ctx->x = native_nz(ctx, native_read(ctx, 0x0368u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x16u + ctx->x)));
    native_cmp(ctx, ctx->a, 0x2du);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lcf48;
    }
    native_write(ctx, 0x08u, ctx->x);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x)));
    if ((ctx->status & NATIVE_Z)) {
        goto Lcf58;
    }
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x40u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lcf48;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x)));
    native_cmp(ctx, ctx->a, 0xe0u);
    if (!(ctx->status & NATIVE_C)) {
        goto Lcf52;
    }
Lcf48: ;
    ctx->a = native_nz(ctx, 0x80u);
    native_write(ctx, 0xfcu, ctx->a);
    native_write(ctx, 0x0772u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0772u) + 1u)));
    /* static tail transfer */
    smbneo_native_fn_actor_killall_cfb4(ctx);
    return;
Lcf52: ;
    smbneo_native_fn_motion_fall_slow_be9b(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    /* static tail transfer */
    goto Ld0be;
Lcf58: ;
    native_write(ctx, 0x0364u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0364u) - 1u)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lcfa1;
    }
    ctx->a = native_nz(ctx, 0x04u);
    native_write(ctx, 0x0364u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0363u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a ^ 0x01u));
    native_write(ctx, 0x0363u, ctx->a);
    ctx->a = native_nz(ctx, 0x22u);
    native_write(ctx, 0x05u, ctx->a);
    ctx->y = native_nz(ctx, native_read(ctx, 0x0369u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xcf20u + ctx->y)));
    native_write(ctx, 0x04u, ctx->a);
    ctx->y = native_nz(ctx, native_read(ctx, 0x0300u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->x = native_nz(ctx, 0x0cu);
    smbneo_native_fn_meta_clear_put_do_8cce(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    smbneo_native_fn_meta_step_8c90(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, 0x08u);
    native_write(ctx, 0xfeu, ctx->a);
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, 0xfdu, ctx->a);
    native_write(ctx, 0x0369u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0369u) + 1u)));
    ctx->a = native_nz(ctx, native_read(ctx, 0x0369u));
    native_cmp(ctx, ctx->a, 0x0fu);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lcfa1;
    }
    smbneo_native_fn_actor_init_veloc_y_c2a6(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, 0x40u);
    native_write(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x80u);
    native_write(ctx, 0xfeu, ctx->a);
Lcfa1: ;
    /* static tail transfer */
    goto Ld0be;
Ld0be: ;
    smbneo_native_fn_actor_proc_bowser_col_d0ff(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->y = native_nz(ctx, 0x10u);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x46u + ctx->x)));
    ctx->a = native_lsr(ctx, ctx->a);
    if (!(ctx->status & NATIVE_C)) {
        goto Ld0ca;
    }
    ctx->y = native_nz(ctx, 0xf0u);
Ld0ca: ;
    ctx->a = native_nz(ctx, ctx->y);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x87u + ctx->x)));
    ctx->y = native_nz(ctx, native_read(ctx, 0x06cfu));
    native_write(ctx, (uint16_t)(0x0087u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x)));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x08u);
    native_write(ctx, (uint16_t)(0x00cfu + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x)));
    native_write(ctx, (uint16_t)(0x001eu + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x46u + ctx->x)));
    native_write(ctx, (uint16_t)(0x0046u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x08u));
    saved_value_0 = ctx->a;
    ctx->x = native_nz(ctx, native_read(ctx, 0x06cfu));
    native_write(ctx, 0x08u, ctx->x);
    ctx->a = native_nz(ctx, 0x2du);
    native_write(ctx, (uint16_t)(uint8_t)(0x16u + ctx->x), ctx->a);
    smbneo_native_fn_actor_proc_bowser_col_d0ff(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, saved_value_0);
    native_write(ctx, 0x08u, ctx->a);
    ctx->x = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x036au, ctx->a);
    return;
    return;
}

void smbneo_native_fn_actor_B_18_proc_cfa8(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xcfa8u);
    uint8_t saved_value_0 = 0u;
    goto Lcfa8;
Lcf52: ;
    smbneo_native_fn_motion_fall_slow_be9b(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    /* static tail transfer */
    goto Ld0be;
Lcfa8: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x20u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lcfc2;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x)));
    native_cmp(ctx, ctx->a, 0xe0u);
    if (!(ctx->status & NATIVE_C)) {
        goto Lcf52;
    }
    smbneo_native_fn_actor_killall_cfb4(ctx);
    return;
Lcfc2: ;
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x06cbu, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0747u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lcfcf;
    }
    /* static tail transfer */
    goto Ld07c;
Lcfcf: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x0363u));
    if (!(ctx->status & NATIVE_N)) {
        goto Lcfd7;
    }
    /* static tail transfer */
    goto Ld052;
Lcfd7: ;
    native_write(ctx, 0x0364u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0364u) - 1u)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lcfe9;
    }
    ctx->a = native_nz(ctx, 0x20u);
    native_write(ctx, 0x0364u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0363u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a ^ 0x01u));
    native_write(ctx, 0x0363u, ctx->a);
Lcfe9: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x09u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x0fu));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lcff3;
    }
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, (uint16_t)(uint8_t)(0x46u + ctx->x), ctx->a);
Lcff3: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x078au + ctx->x)));
    if ((ctx->status & NATIVE_Z)) {
        goto Ld014;
    }
    smbneo_native_fn_col_player_bullet_diff_e099(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    if (!(ctx->status & NATIVE_N)) {
        goto Ld014;
    }
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, (uint16_t)(uint8_t)(0x46u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, 0x0365u, ctx->a);
    ctx->a = native_nz(ctx, 0x20u);
    native_write(ctx, (uint16_t)(0x078au + ctx->x), ctx->a);
    native_write(ctx, 0x0790u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x87u + ctx->x)));
    native_cmp(ctx, ctx->a, 0xc8u);
    if ((ctx->status & NATIVE_C)) {
        goto Ld052;
    }
Ld014: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x09u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x03u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Ld052;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x87u + ctx->x)));
    native_cmp(ctx, ctx->a, native_read(ctx, 0x0366u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Ld02d;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x07a7u + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x03u));
    ctx->y = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xcfa4u + ctx->y)));
    native_write(ctx, 0x06dcu, ctx->a);
Ld02d: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x87u + ctx->x)));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, 0x0365u));
    native_write(ctx, (uint16_t)(uint8_t)(0x87u + ctx->x), ctx->a);
    ctx->y = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x46u + ctx->x)));
    native_cmp(ctx, ctx->y, 0x01u);
    if ((ctx->status & NATIVE_Z)) {
        goto Ld052;
    }
    ctx->y = native_nz(ctx, 0xffu);
    ctx->status |= NATIVE_C;
    native_sbc(ctx, native_read(ctx, 0x0366u));
    if (!(ctx->status & NATIVE_N)) {
        goto Ld04a;
    }
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a ^ 0xffu));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x01u);
    ctx->y = native_nz(ctx, 0x01u);
Ld04a: ;
    native_cmp(ctx, ctx->a, native_read(ctx, 0x06dcu));
    if (!(ctx->status & NATIVE_C)) {
        goto Ld052;
    }
    native_write(ctx, 0x0365u, ctx->y);
Ld052: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x078au + ctx->x)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Ld07f;
    }
    smbneo_native_fn_motion_fall_slow_be9b(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, 0x075fu));
    native_cmp(ctx, ctx->a, 0x05u);
    if (!(ctx->status & NATIVE_C)) {
        goto Ld06a;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x09u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x03u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Ld06a;
    }
    smbneo_native_fn_misc_init_hammer_b988(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Ld06a: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x)));
    native_cmp(ctx, ctx->a, 0x80u);
    if (!(ctx->status & NATIVE_C)) {
        goto Ld08c;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x07a7u + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x03u));
    ctx->y = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xcfa4u + ctx->y)));
    native_write(ctx, (uint16_t)(0x078au + ctx->x), ctx->a);
Ld07c: ;
    /* static tail transfer */
    goto Ld08c;
Ld07f: ;
    native_cmp(ctx, ctx->a, 0x01u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Ld08c;
    }
    native_write(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x), native_nz(ctx, (uint8_t)(native_read(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x)) - 1u)));
    smbneo_native_fn_actor_init_veloc_y_c2a6(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, 0xfeu);
    native_write(ctx, (uint16_t)(uint8_t)(0xa0u + ctx->x), ctx->a);
Ld08c: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x075fu));
    native_cmp(ctx, ctx->a, 0x07u);
    if ((ctx->status & NATIVE_Z)) {
        goto Ld097;
    }
    native_cmp(ctx, ctx->a, 0x05u);
    if ((ctx->status & NATIVE_C)) {
        goto Ld0be;
    }
Ld097: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x0790u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Ld0be;
    }
    ctx->a = native_nz(ctx, 0x20u);
    native_write(ctx, 0x0790u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0363u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a ^ 0x80u));
    native_write(ctx, 0x0363u, ctx->a);
    if ((ctx->status & NATIVE_N)) {
        goto Ld08c;
    }
    smbneo_native_fn_actor_proc_bowser_flame_timer_init_d11c(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->y = native_nz(ctx, native_read(ctx, 0x06ccu));
    if ((ctx->status & NATIVE_Z)) {
        goto Ld0b6;
    }
    ctx->status |= NATIVE_C;
    native_sbc(ctx, 0x10u);
Ld0b6: ;
    native_write(ctx, 0x0790u, ctx->a);
    ctx->a = native_nz(ctx, 0x15u);
    native_write(ctx, 0x06cbu, ctx->a);
Ld0be: ;
    smbneo_native_fn_actor_proc_bowser_col_d0ff(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->y = native_nz(ctx, 0x10u);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x46u + ctx->x)));
    ctx->a = native_lsr(ctx, ctx->a);
    if (!(ctx->status & NATIVE_C)) {
        goto Ld0ca;
    }
    ctx->y = native_nz(ctx, 0xf0u);
Ld0ca: ;
    ctx->a = native_nz(ctx, ctx->y);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x87u + ctx->x)));
    ctx->y = native_nz(ctx, native_read(ctx, 0x06cfu));
    native_write(ctx, (uint16_t)(0x0087u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x)));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x08u);
    native_write(ctx, (uint16_t)(0x00cfu + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x)));
    native_write(ctx, (uint16_t)(0x001eu + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x46u + ctx->x)));
    native_write(ctx, (uint16_t)(0x0046u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x08u));
    saved_value_0 = ctx->a;
    ctx->x = native_nz(ctx, native_read(ctx, 0x06cfu));
    native_write(ctx, 0x08u, ctx->x);
    ctx->a = native_nz(ctx, 0x2du);
    native_write(ctx, (uint16_t)(uint8_t)(0x16u + ctx->x), ctx->a);
    smbneo_native_fn_actor_proc_bowser_col_d0ff(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, saved_value_0);
    native_write(ctx, 0x08u, ctx->a);
    ctx->x = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x036au, ctx->a);
    return;
    return;
}

void smbneo_native_fn_actor_killall_cfb4(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xcfb4u);
    ctx->x = native_nz(ctx, 0x04u);
Lcfb6: ;
    smbneo_native_fn_actor_erase_c8db(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x - 1u));
    if (!(ctx->status & NATIVE_N)) {
        goto Lcfb6;
    }
    native_write(ctx, 0x06cbu, ctx->a);
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    return;
    return;
}

void smbneo_native_fn_actor_proc_bowser_col_d0ff(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xd0ffu);
    goto Ld0ff;
Ld0fe: ;
    return;
Ld0ff: ;
    native_write(ctx, 0x036au, native_nz(ctx, (uint8_t)(native_read(ctx, 0x036au) + 1u)));
    smbneo_native_fn_actor_B_20_proc_c81a(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Ld0fe;
    }
    ctx->a = native_nz(ctx, 0x0au);
    native_write(ctx, (uint16_t)(0x049au + ctx->x), ctx->a);
    smbneo_native_fn_col_actor_box_e199(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    /* static tail transfer */
    smbneo_native_fn_col_player_actor_proc_d7aa(ctx);
    return;
    return;
}

void smbneo_native_fn_actor_proc_bowser_flame_timer_init_d11c(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xd11cu);
    ctx->y = native_nz(ctx, native_read(ctx, 0x0367u));
    native_write(ctx, 0x0367u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0367u) + 1u)));
    ctx->a = native_nz(ctx, native_read(ctx, 0x0367u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x07u));
    native_write(ctx, 0x0367u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xd114u + ctx->y)));
    return;
    return;
}

void smbneo_native_fn_actor_proc_bowser_flame_do_d12e(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xd12eu);
    uint8_t saved_value_0 = 0u;
    goto Ld12e;
Ld12d: ;
    return;
Ld12e: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x0747u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Ld163;
    }
    ctx->a = native_nz(ctx, 0x40u);
    ctx->y = native_nz(ctx, native_read(ctx, 0x06ccu));
    if ((ctx->status & NATIVE_Z)) {
        goto Ld13c;
    }
    ctx->a = native_nz(ctx, 0x60u);
Ld13c: ;
    native_write(ctx, 0x00u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0401u + ctx->x)));
    ctx->status |= NATIVE_C;
    native_sbc(ctx, native_read(ctx, 0x00u));
    native_write(ctx, (uint16_t)(0x0401u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x87u + ctx->x)));
    native_sbc(ctx, 0x01u);
    native_write(ctx, (uint16_t)(uint8_t)(0x87u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x6eu + ctx->x)));
    native_sbc(ctx, 0x00u);
    native_write(ctx, (uint16_t)(uint8_t)(0x6eu + ctx->x), ctx->a);
    ctx->y = native_nz(ctx, native_read(ctx, (uint16_t)(0x0417u + ctx->x)));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x)));
    native_cmp(ctx, ctx->a, native_read(ctx, (uint16_t)(0xc4e0u + ctx->y)));
    if ((ctx->status & NATIVE_Z)) {
        goto Ld163;
    }
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, (uint16_t)(0x0434u + ctx->x)));
    native_write(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x), ctx->a);
Ld163: ;
    smbneo_native_fn_pos_calc_x_rel_actor_f0b7(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Ld12d;
    }
    ctx->a = native_nz(ctx, 0x51u);
    native_write(ctx, 0x00u, ctx->a);
    ctx->y = native_nz(ctx, 0x02u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x09u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x02u));
    if ((ctx->status & NATIVE_Z)) {
        goto Ld178;
    }
    ctx->y = native_nz(ctx, 0x82u);
Ld178: ;
    native_write(ctx, 0x01u, ctx->y);
    ctx->y = native_nz(ctx, native_read(ctx, (uint16_t)(0x06e5u + ctx->x)));
    ctx->x = native_nz(ctx, 0x00u);
Ld17f: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x03b9u));
    native_write(ctx, (uint16_t)(0x0200u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x00u));
    native_write(ctx, (uint16_t)(0x0201u + ctx->y), ctx->a);
    native_write(ctx, 0x00u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x00u) + 1u)));
    ctx->a = native_nz(ctx, native_read(ctx, 0x01u));
    native_write(ctx, (uint16_t)(0x0202u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x03aeu));
    native_write(ctx, (uint16_t)(0x0203u + ctx->y), ctx->a);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x08u);
    native_write(ctx, 0x03aeu, ctx->a);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    native_cmp(ctx, ctx->x, 0x03u);
    if (!(ctx->status & NATIVE_C)) {
        goto Ld17f;
    }
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    smbneo_native_fn_pos_bits_get_actor_f114(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->y = native_nz(ctx, native_read(ctx, (uint16_t)(0x06e5u + ctx->x)));
    ctx->a = native_nz(ctx, native_read(ctx, 0x03d1u));
    ctx->a = native_lsr(ctx, ctx->a);
    saved_value_0 = ctx->a;
    if (!(ctx->status & NATIVE_C)) {
        goto Ld1ba;
    }
    ctx->a = native_nz(ctx, 0xf8u);
    native_write(ctx, (uint16_t)(0x020cu + ctx->y), ctx->a);
Ld1ba: ;
    ctx->a = native_nz(ctx, saved_value_0);
    ctx->a = native_lsr(ctx, ctx->a);
    saved_value_0 = ctx->a;
    if (!(ctx->status & NATIVE_C)) {
        goto Ld1c4;
    }
    ctx->a = native_nz(ctx, 0xf8u);
    native_write(ctx, (uint16_t)(0x0208u + ctx->y), ctx->a);
Ld1c4: ;
    ctx->a = native_nz(ctx, saved_value_0);
    ctx->a = native_lsr(ctx, ctx->a);
    saved_value_0 = ctx->a;
    if (!(ctx->status & NATIVE_C)) {
        goto Ld1ce;
    }
    ctx->a = native_nz(ctx, 0xf8u);
    native_write(ctx, (uint16_t)(0x0204u + ctx->y), ctx->a);
Ld1ce: ;
    ctx->a = native_nz(ctx, saved_value_0);
    ctx->a = native_lsr(ctx, ctx->a);
    if (!(ctx->status & NATIVE_C)) {
        goto Ld1d7;
    }
    ctx->a = native_nz(ctx, 0xf8u);
    native_write(ctx, (uint16_t)(0x0200u + ctx->y), ctx->a);
Ld1d7: ;
    return;
    return;
}

void smbneo_native_fn_actor_B_01_proc_d1d8(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xd1d8u);
    native_write(ctx, (uint16_t)(uint8_t)(0xa0u + ctx->x), native_nz(ctx, (uint8_t)(native_read(ctx, (uint16_t)(uint8_t)(0xa0u + ctx->x)) - 1u)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Ld1e8;
    }
    ctx->a = native_nz(ctx, 0x08u);
    native_write(ctx, (uint16_t)(uint8_t)(0xa0u + ctx->x), ctx->a);
    native_write(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x), native_nz(ctx, (uint8_t)(native_read(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x)) + 1u)));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x)));
    native_cmp(ctx, ctx->a, 0x03u);
    if ((ctx->status & NATIVE_C)) {
        goto Ld200;
    }
Ld1e8: ;
    smbneo_native_fn_pos_calc_x_rel_actor_f0b7(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, 0x03b9u));
    native_write(ctx, 0x03bau, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x03aeu));
    native_write(ctx, 0x03afu, ctx->a);
    ctx->y = native_nz(ctx, native_read(ctx, (uint16_t)(0x06e5u + ctx->x)));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x)));
    smbneo_native_fn_render_fireworks_ec74(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    return;
Ld200: ;
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, (uint16_t)(uint8_t)(0x0fu + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x08u);
    native_write(ctx, 0xfeu, ctx->a);
    ctx->a = native_nz(ctx, 0x05u);
    native_write(ctx, 0x0138u, ctx->a);
    /* static tail transfer */
    goto Ld279;
Ld279: ;
    ctx->y = native_nz(ctx, 0x0bu);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0753u));
    if ((ctx->status & NATIVE_Z)) {
        goto Ld282;
    }
    ctx->y = native_nz(ctx, 0x11u);
Ld282: ;
    smbneo_native_fn_stats_calc_9217(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, 0x0753u));
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | 0x04u));
    /* static tail transfer */
    smbneo_native_fn_misc_proc_coin_add_score_bb41(ctx);
    return;
    return;
}

void smbneo_native_fn_actor_B_1C_proc_d21c(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xd21cu);
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x06cbu, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0746u));
    native_cmp(ctx, ctx->a, 0x05u);
    if ((ctx->status & NATIVE_C)) {
        smbneo_native_fn_actor_proc_flag_victory_fireworks_rts_d254(ctx);
        return;
    }
    uint8_t table_selector_d228 = ctx->a;
    ctx->a = native_asl(ctx, ctx->a);
    ctx->y = native_nz(ctx, ctx->a);
    native_write(ctx, 0x04u, 0x2au);
    native_write(ctx, 0x05u, 0xd2u);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    switch (table_selector_d228) {
    case 0: native_write(ctx, 0x06u, 0x54u);
             native_write(ctx, 0x07u, 0xd2u);
             ctx->a = native_nz(ctx, 0xd2u);
             smbneo_native_fn_actor_proc_flag_victory_fireworks_rts_d254(ctx); return;
    case 1: native_write(ctx, 0x06u, 0x35u);
             native_write(ctx, 0x07u, 0xd2u);
             ctx->a = native_nz(ctx, 0xd2u);
             smbneo_native_fn_actor_proc_flag_victory_tbl_done_d235(ctx); return;
    case 2: native_write(ctx, 0x06u, 0x55u);
             native_write(ctx, 0x07u, 0xd2u);
             ctx->a = native_nz(ctx, 0xd2u);
             smbneo_native_fn_actor_proc_flag_victory_timer_score_d255(ctx); return;
    case 3: native_write(ctx, 0x06u, 0x91u);
             native_write(ctx, 0x07u, 0xd2u);
             ctx->a = native_nz(ctx, 0xd2u);
             smbneo_native_fn_actor_proc_flag_victory_raise_d291(ctx); return;
    case 4: native_write(ctx, 0x06u, 0xe5u);
             native_write(ctx, 0x07u, 0xd2u);
             ctx->a = native_nz(ctx, 0xd2u);
             smbneo_native_fn_actor_proc_flag_victory_delay_d2e5(ctx); return;
    default: ctx->unsupported = 1; return;
    }
    return;
}

void smbneo_native_fn_actor_proc_flag_victory_tbl_done_d235(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xd235u);
    ctx->y = native_nz(ctx, 0x05u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x07fau));
    native_cmp(ctx, ctx->a, 0x01u);
    if ((ctx->status & NATIVE_Z)) {
        goto Ld24c;
    }
    ctx->y = native_nz(ctx, 0x03u);
    native_cmp(ctx, ctx->a, 0x03u);
    if ((ctx->status & NATIVE_Z)) {
        goto Ld24c;
    }
    ctx->y = native_nz(ctx, 0x00u);
    native_cmp(ctx, ctx->a, 0x06u);
    if ((ctx->status & NATIVE_Z)) {
        goto Ld24c;
    }
    ctx->a = native_nz(ctx, 0xffu);
Ld24c: ;
    native_write(ctx, 0x06d7u, ctx->a);
    native_write(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x), ctx->y);
    native_write(ctx, 0x0746u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0746u) + 1u)));
    smbneo_native_fn_actor_proc_flag_victory_fireworks_rts_d254(ctx);
    return;
    return;
}

void smbneo_native_fn_actor_proc_flag_victory_fireworks_rts_d254(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xd254u);
    return;
    return;
}

void smbneo_native_fn_actor_proc_flag_victory_timer_score_d255(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xd255u);
    goto Ld255;
Ld251: ;
    native_write(ctx, 0x0746u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0746u) + 1u)));
    smbneo_native_fn_actor_proc_flag_victory_fireworks_rts_d254(ctx);
    return;
Ld255: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x07f8u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | native_read(ctx, 0x07f9u)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | native_read(ctx, 0x07fau)));
    if ((ctx->status & NATIVE_Z)) {
        goto Ld251;
    }
    smbneo_native_fn_actor_proc_flag_victory_timer_score_do_d260(ctx);
    return;
    return;
}

void smbneo_native_fn_actor_proc_flag_victory_timer_score_do_d260(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xd260u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x09u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x04u));
    if ((ctx->status & NATIVE_Z)) {
        goto Ld26a;
    }
    ctx->a = native_nz(ctx, 0x10u);
    native_write(ctx, 0xfeu, ctx->a);
Ld26a: ;
    ctx->y = native_nz(ctx, 0x23u);
    ctx->a = native_nz(ctx, 0xffu);
    native_write(ctx, 0x0139u, ctx->a);
    smbneo_native_fn_stats_calc_9217(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, 0x05u);
    native_write(ctx, 0x0139u, ctx->a);
    ctx->y = native_nz(ctx, 0x0bu);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0753u));
    if ((ctx->status & NATIVE_Z)) {
        goto Ld282;
    }
    ctx->y = native_nz(ctx, 0x11u);
Ld282: ;
    smbneo_native_fn_stats_calc_9217(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, 0x0753u));
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | 0x04u));
    /* static tail transfer */
    smbneo_native_fn_misc_proc_coin_add_score_bb41(ctx);
    return;
    return;
}

void smbneo_native_fn_actor_proc_flag_victory_raise_d291(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xd291u);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x)));
    native_cmp(ctx, ctx->a, 0x72u);
    if (!(ctx->status & NATIVE_C)) {
        goto Ld29c;
    }
    native_write(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x), native_nz(ctx, (uint8_t)(native_read(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x)) - 1u)));
    /* static tail transfer */
    smbneo_native_fn_actor_proc_flag_victory_draw_d2a8(ctx);
    return;
Ld29c: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x06d7u));
    if ((ctx->status & NATIVE_Z)) {
        goto Ld2d9;
    }
    if ((ctx->status & NATIVE_N)) {
        goto Ld2d9;
    }
    ctx->a = native_nz(ctx, 0x16u);
    native_write(ctx, 0x06cbu, ctx->a);
    smbneo_native_fn_actor_proc_flag_victory_draw_d2a8(ctx);
    return;
Ld2d9: ;
    smbneo_native_fn_actor_proc_flag_victory_draw_d2a8(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, 0x06u);
    native_write(ctx, (uint16_t)(0x0796u + ctx->x), ctx->a);
    native_write(ctx, 0x0746u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0746u) + 1u)));
    return;
    return;
}

void smbneo_native_fn_actor_proc_flag_victory_draw_d2a8(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xd2a8u);
    smbneo_native_fn_pos_calc_x_rel_actor_f0b7(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->y = native_nz(ctx, native_read(ctx, (uint16_t)(0x06e5u + ctx->x)));
    ctx->x = native_nz(ctx, 0x03u);
Ld2b0: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x03b9u));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, (uint16_t)(0xd210u + ctx->x)));
    native_write(ctx, (uint16_t)(0x0200u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xd218u + ctx->x)));
    native_write(ctx, (uint16_t)(0x0201u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, 0x22u);
    native_write(ctx, (uint16_t)(0x0202u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x03aeu));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, (uint16_t)(0xd214u + ctx->x)));
    native_write(ctx, (uint16_t)(0x0203u + ctx->y), ctx->a);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x - 1u));
    if (!(ctx->status & NATIVE_N)) {
        goto Ld2b0;
    }
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    return;
    return;
}

void smbneo_native_fn_actor_proc_flag_victory_delay_d2e5(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xd2e5u);
    goto Ld2e5;
Ld2e1: ;
    native_write(ctx, 0x0746u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0746u) + 1u)));
    return;
Ld2e5: ;
    smbneo_native_fn_actor_proc_flag_victory_draw_d2a8(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0796u + ctx->x)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Ld2f2;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x07b1u));
    if ((ctx->status & NATIVE_Z)) {
        goto Ld2e1;
    }
Ld2f2: ;
    return;
    return;
}

void smbneo_native_fn_actor_A_0D_proc_d2f3(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xd2f3u);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Ld34d;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x078au + ctx->x)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Ld34d;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xa0u + ctx->x)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Ld323;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x)));
    if ((ctx->status & NATIVE_N)) {
        goto Ld318;
    }
    smbneo_native_fn_col_player_bullet_diff_e099(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    if (!(ctx->status & NATIVE_N)) {
        goto Ld312;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x00u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a ^ 0xffu));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x01u);
    native_write(ctx, 0x00u, ctx->a);
Ld312: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x00u));
    native_cmp(ctx, ctx->a, 0x21u);
    if (!(ctx->status & NATIVE_C)) {
        goto Ld34d;
    }
Ld318: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a ^ 0xffu));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x01u);
    native_write(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x), ctx->a);
    native_write(ctx, (uint16_t)(uint8_t)(0xa0u + ctx->x), native_nz(ctx, (uint8_t)(native_read(ctx, (uint16_t)(uint8_t)(0xa0u + ctx->x)) + 1u)));
Ld323: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0434u + ctx->x)));
    ctx->y = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x)));
    if (!(ctx->status & NATIVE_N)) {
        goto Ld32d;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0417u + ctx->x)));
Ld32d: ;
    native_write(ctx, 0x00u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x09u));
    ctx->a = native_lsr(ctx, ctx->a);
    if (!(ctx->status & NATIVE_C)) {
        goto Ld34d;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x0747u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Ld34d;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x)));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x)));
    native_write(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x), ctx->a);
    native_cmp(ctx, ctx->a, native_read(ctx, 0x00u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Ld34d;
    }
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, (uint16_t)(uint8_t)(0xa0u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x40u);
    native_write(ctx, (uint16_t)(0x078au + ctx->x), ctx->a);
Ld34d: ;
    ctx->a = native_nz(ctx, 0x20u);
    native_write(ctx, (uint16_t)(0x03c5u + ctx->x), ctx->a);
    return;
    return;
}

void smbneo_native_fn_actor_proc_firebar_spin_d353(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xd353u);
    native_write(ctx, 0x07u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x34u + ctx->x)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Ld367;
    }
    ctx->y = native_nz(ctx, 0x18u);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x)));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, 0x07u));
    native_write(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xa0u + ctx->x)));
    native_adc(ctx, 0x00u);
    return;
Ld367: ;
    ctx->y = native_nz(ctx, 0x08u);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x)));
    ctx->status |= NATIVE_C;
    native_sbc(ctx, native_read(ctx, 0x07u));
    native_write(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xa0u + ctx->x)));
    native_sbc(ctx, 0x00u);
    return;
    return;
}

void smbneo_native_fn_actor_proc_plat_bal_d375(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xd375u);
    uint8_t saved_value_0 = 0u;
    uint8_t saved_value_1 = 0u;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xb6u + ctx->x)));
    native_cmp(ctx, ctx->a, 0x03u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Ld37e;
    }
    /* static tail transfer */
    smbneo_native_fn_actor_erase_c8db(ctx);
    return;
Ld37e: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x)));
    if (!(ctx->status & NATIVE_N)) {
        goto Ld383;
    }
    return;
Ld383: ;
    ctx->y = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x03a2u + ctx->x)));
    native_write(ctx, 0x00u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x46u + ctx->x)));
    if ((ctx->status & NATIVE_Z)) {
        goto Ld390;
    }
    /* static tail transfer */
    goto Ld4fe;
Ld390: ;
    ctx->a = native_nz(ctx, 0x2du);
    native_cmp(ctx, ctx->a, native_read(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x)));
    if (!(ctx->status & NATIVE_C)) {
        goto Ld3a5;
    }
    native_cmp(ctx, ctx->y, native_read(ctx, 0x00u));
    if ((ctx->status & NATIVE_Z)) {
        goto Ld3a2;
    }
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x02u);
    native_write(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x), ctx->a);
    /* static tail transfer */
    smbneo_native_fn_actor_proc_plat_stop_d4f4(ctx);
    return;
Ld3a2: ;
    /* static tail transfer */
    goto Ld4db;
Ld3a5: ;
    native_cmp(ctx, ctx->a, native_read(ctx, (uint16_t)(0x00cfu + ctx->y)));
    if (!(ctx->status & NATIVE_C)) {
        goto Ld3b7;
    }
    native_cmp(ctx, ctx->x, native_read(ctx, 0x00u));
    if ((ctx->status & NATIVE_Z)) {
        goto Ld3a2;
    }
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x02u);
    native_write(ctx, (uint16_t)(0x00cfu + ctx->y), ctx->a);
    /* static tail transfer */
    smbneo_native_fn_actor_proc_plat_stop_d4f4(ctx);
    return;
Ld3b7: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x)));
    saved_value_0 = ctx->a;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x03a2u + ctx->x)));
    if (!(ctx->status & NATIVE_N)) {
        goto Ld3d7;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0434u + ctx->x)));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x05u);
    native_write(ctx, 0x00u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xa0u + ctx->x)));
    native_adc(ctx, 0x00u);
    if ((ctx->status & NATIVE_N)) {
        goto Ld3e7;
    }
    if (!(ctx->status & NATIVE_Z)) {
        goto Ld3db;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x00u));
    native_cmp(ctx, ctx->a, 0x0bu);
    if (!(ctx->status & NATIVE_C)) {
        goto Ld3e1;
    }
    if ((ctx->status & NATIVE_C)) {
        goto Ld3db;
    }
Ld3d7: ;
    native_cmp(ctx, ctx->a, native_read(ctx, 0x08u));
    if ((ctx->status & NATIVE_Z)) {
        goto Ld3e7;
    }
Ld3db: ;
    smbneo_native_fn_motion_platform_up_beca(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    /* static tail transfer */
    goto Ld3ea;
Ld3e1: ;
    smbneo_native_fn_actor_proc_plat_stop_d4f4(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    /* static tail transfer */
    goto Ld3ea;
Ld3e7: ;
    smbneo_native_fn_motion_platform_down_bec5(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Ld3ea: ;
    ctx->y = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x)));
    ctx->a = native_nz(ctx, saved_value_0);
    ctx->status |= NATIVE_C;
    native_sbc(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x)));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, (uint16_t)(0x00cfu + ctx->y)));
    native_write(ctx, (uint16_t)(0x00cfu + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x03a2u + ctx->x)));
    if ((ctx->status & NATIVE_N)) {
        goto Ld400;
    }
    ctx->x = native_nz(ctx, ctx->a);
    smbneo_native_fn_col_plat_player_pos_db7a(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Ld400: ;
    ctx->y = native_nz(ctx, native_read(ctx, 0x08u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x00a0u + ctx->y)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | native_read(ctx, (uint16_t)(0x0434u + ctx->y))));
    if ((ctx->status & NATIVE_Z)) {
        goto Ld481;
    }
    ctx->x = native_nz(ctx, native_read(ctx, 0x0300u));
    native_cmp(ctx, ctx->x, 0x20u);
    if ((ctx->status & NATIVE_C)) {
        goto Ld481;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x00a0u + ctx->y)));
    saved_value_0 = ctx->a;
    saved_value_1 = ctx->a;
    smbneo_native_fn_actor_init_plat_rope_d484(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, 0x01u));
    native_write(ctx, (uint16_t)(0x0301u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x00u));
    native_write(ctx, (uint16_t)(0x0302u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, (uint16_t)(0x0303u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x00a0u + ctx->y)));
    if ((ctx->status & NATIVE_N)) {
        goto Ld43a;
    }
    ctx->a = native_nz(ctx, 0xa2u);
    native_write(ctx, (uint16_t)(0x0304u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0xa3u);
    native_write(ctx, (uint16_t)(0x0305u + ctx->x), ctx->a);
    /* static tail transfer */
    goto Ld442;
Ld43a: ;
    ctx->a = native_nz(ctx, 0x24u);
    native_write(ctx, (uint16_t)(0x0304u + ctx->x), ctx->a);
    native_write(ctx, (uint16_t)(0x0305u + ctx->x), ctx->a);
Ld442: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x001eu + ctx->y)));
    ctx->y = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, saved_value_1);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a ^ 0xffu));
    smbneo_native_fn_actor_init_plat_rope_d484(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, 0x01u));
    native_write(ctx, (uint16_t)(0x0306u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x00u));
    native_write(ctx, (uint16_t)(0x0307u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, (uint16_t)(0x0308u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, saved_value_0);
    if (!(ctx->status & NATIVE_N)) {
        goto Ld46b;
    }
    ctx->a = native_nz(ctx, 0xa2u);
    native_write(ctx, (uint16_t)(0x0309u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0xa3u);
    native_write(ctx, (uint16_t)(0x030au + ctx->x), ctx->a);
    /* static tail transfer */
    goto Ld473;
Ld46b: ;
    ctx->a = native_nz(ctx, 0x24u);
    native_write(ctx, (uint16_t)(0x0309u + ctx->x), ctx->a);
    native_write(ctx, (uint16_t)(0x030au + ctx->x), ctx->a);
Ld473: ;
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, (uint16_t)(0x030bu + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0300u));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x0au);
    native_write(ctx, 0x0300u, ctx->a);
Ld481: ;
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    return;
Ld4db: ;
    ctx->a = native_nz(ctx, ctx->y);
    ctx->x = native_nz(ctx, ctx->a);
    smbneo_native_fn_pos_bits_get_actor_f114(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, 0x06u);
    smbneo_native_fn_col_score_do_d968(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, 0x03adu));
    native_write(ctx, (uint16_t)(0x0117u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0xceu));
    native_write(ctx, (uint16_t)(0x011eu + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, (uint16_t)(uint8_t)(0x46u + ctx->x), ctx->a);
    smbneo_native_fn_actor_proc_plat_stop_d4f4(ctx);
    return;
Ld4fe: ;
    ctx->a = native_nz(ctx, ctx->y);
    saved_value_0 = ctx->a;
    smbneo_native_fn_motion_y_state_5_be7a(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, saved_value_0);
    ctx->x = native_nz(ctx, ctx->a);
    smbneo_native_fn_motion_y_state_5_be7a(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x03a2u + ctx->x)));
    if ((ctx->status & NATIVE_N)) {
        goto Ld513;
    }
    ctx->x = native_nz(ctx, ctx->a);
    smbneo_native_fn_col_plat_player_pos_db7a(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Ld513: ;
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    return;
    return;
}

void smbneo_native_fn_actor_init_plat_rope_d484(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xd484u);
    uint8_t saved_value_0 = 0u;
    uint8_t saved_value_1 = 0u;
    saved_value_0 = ctx->a;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0087u + ctx->y)));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x08u);
    ctx->x = native_nz(ctx, native_read(ctx, 0x06ccu));
    if (!(ctx->status & NATIVE_Z)) {
        goto Ld493;
    }
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x10u);
Ld493: ;
    saved_value_1 = ctx->a;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x006eu + ctx->y)));
    native_adc(ctx, 0x00u);
    native_write(ctx, 0x02u, ctx->a);
    ctx->a = native_nz(ctx, saved_value_1);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0xf0u));
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    native_write(ctx, 0x00u, ctx->a);
    ctx->x = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->y)));
    ctx->a = native_nz(ctx, saved_value_0);
    if (!(ctx->status & NATIVE_N)) {
        goto Ld4ad;
    }
    ctx->a = native_nz(ctx, ctx->x);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x08u);
    ctx->x = native_nz(ctx, ctx->a);
Ld4ad: ;
    ctx->a = native_nz(ctx, ctx->x);
    ctx->x = native_nz(ctx, native_read(ctx, 0x0300u));
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_rol(ctx, ctx->a);
    saved_value_0 = ctx->a;
    ctx->a = native_rol(ctx, ctx->a);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x03u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | 0x20u));
    native_write(ctx, 0x01u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x02u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x01u));
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | native_read(ctx, 0x01u)));
    native_write(ctx, 0x01u, ctx->a);
    ctx->a = native_nz(ctx, saved_value_0);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0xe0u));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, 0x00u));
    native_write(ctx, 0x00u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x00cfu + ctx->y)));
    native_cmp(ctx, ctx->a, 0xe8u);
    if (!(ctx->status & NATIVE_C)) {
        goto Ld4da;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x00u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0xbfu));
    native_write(ctx, 0x00u, ctx->a);
Ld4da: ;
    return;
    return;
}

void smbneo_native_fn_actor_proc_plat_stop_d4f4(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xd4f4u);
    smbneo_native_fn_actor_init_veloc_y_c2a6(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_write(ctx, (uint16_t)(0x00a0u + ctx->y), ctx->a);
    native_write(ctx, (uint16_t)(0x0434u + ctx->y), ctx->a);
    return;
    return;
}

void smbneo_native_fn_actor_proc_plat_mov_v_d516(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xd516u);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xa0u + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | native_read(ctx, (uint16_t)(0x0434u + ctx->x))));
    if (!(ctx->status & NATIVE_Z)) {
        goto Ld532;
    }
    native_write(ctx, (uint16_t)(0x0417u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x)));
    native_cmp(ctx, ctx->a, native_read(ctx, (uint16_t)(0x0401u + ctx->x)));
    if ((ctx->status & NATIVE_C)) {
        goto Ld532;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x09u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x07u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Ld52f;
    }
    native_write(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x), native_nz(ctx, (uint8_t)(native_read(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x)) + 1u)));
Ld52f: ;
    /* static tail transfer */
    goto Ld541;
Ld532: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x)));
    native_cmp(ctx, ctx->a, native_read(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x)));
    if (!(ctx->status & NATIVE_C)) {
        goto Ld53e;
    }
    smbneo_native_fn_motion_platform_up_beca(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    /* static tail transfer */
    goto Ld541;
Ld53e: ;
    smbneo_native_fn_motion_platform_down_bec5(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Ld541: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x03a2u + ctx->x)));
    if ((ctx->status & NATIVE_N)) {
        goto Ld549;
    }
    smbneo_native_fn_col_plat_player_pos_db7a(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Ld549: ;
    return;
    return;
}

void smbneo_native_fn_actor_proc_plat_move_h_d54a(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xd54au);
    ctx->a = native_nz(ctx, 0x0eu);
    smbneo_native_fn_actor_floater_do_ca8a(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_actor_floater_move_caa9(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x03a2u + ctx->x)));
    if ((ctx->status & NATIVE_N)) {
        goto Ld573;
    }
    smbneo_native_fn_actor_proc_plat_move_player_h_d557(ctx);
    return;
Ld573: ;
    return;
    return;
}

void smbneo_native_fn_actor_proc_plat_move_player_h_d557(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xd557u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x86u));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, 0x00u));
    native_write(ctx, 0x86u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x6du));
    ctx->y = native_nz(ctx, native_read(ctx, 0x00u));
    if ((ctx->status & NATIVE_N)) {
        goto Ld569;
    }
    native_adc(ctx, 0x00u);
    /* static tail transfer */
    goto Ld56b;
Ld569: ;
    native_sbc(ctx, 0x00u);
Ld56b: ;
    native_write(ctx, 0x6du, ctx->a);
    native_write(ctx, 0x03a1u, ctx->y);
    smbneo_native_fn_col_plat_player_pos_db7a(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    return;
    return;
}

void smbneo_native_fn_actor_proc_plat_drop_d574(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xd574u);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x03a2u + ctx->x)));
    if ((ctx->status & NATIVE_N)) {
        goto Ld57f;
    }
    smbneo_native_fn_motion_fall_fast_be97(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_col_plat_player_pos_db7a(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Ld57f: ;
    return;
    return;
}

void smbneo_native_fn_actor_proc_plat_right_d580(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xd580u);
    smbneo_native_fn_motion_x_be11(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_write(ctx, 0x00u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x03a2u + ctx->x)));
    if ((ctx->status & NATIVE_N)) {
        goto Ld591;
    }
    ctx->a = native_nz(ctx, 0x10u);
    native_write(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x), ctx->a);
    smbneo_native_fn_actor_proc_plat_move_player_h_d557(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Ld591: ;
    return;
    return;
}

void smbneo_native_fn_actor_proc_plat_lift_d592(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xd592u);
    goto Ld592;
Ld541: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x03a2u + ctx->x)));
    if ((ctx->status & NATIVE_N)) {
        goto Ld549;
    }
    smbneo_native_fn_col_plat_player_pos_db7a(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Ld549: ;
    return;
Ld592: ;
    smbneo_native_fn_actor_proc_plat_lift_move_d59e(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    /* static tail transfer */
    goto Ld541;
    return;
}

void smbneo_native_fn_actor_proc_plat_small_do_d598(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xd598u);
    smbneo_native_fn_actor_proc_plat_lift_move_d59e(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    /* static tail transfer */
    goto Ld5b4;
Ld5b4: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x03a2u + ctx->x)));
    if ((ctx->status & NATIVE_Z)) {
        goto Ld5bc;
    }
    smbneo_native_fn_col_plat_s_player_pos_db70(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Ld5bc: ;
    return;
    return;
}

void smbneo_native_fn_actor_proc_plat_lift_move_d59e(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xd59eu);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0747u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Ld5bc;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0417u + ctx->x)));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, (uint16_t)(0x0434u + ctx->x)));
    native_write(ctx, (uint16_t)(0x0417u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x)));
    native_adc(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xa0u + ctx->x)));
    native_write(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x), ctx->a);
    return;
Ld5bc: ;
    return;
    return;
}

void smbneo_native_fn_actor_oob_proc_d5bd(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xd5bdu);
#ifdef SMBNEO_NATIVE_FAST_PATHS
    if (vs_native_fast_actor_oob_proc(ctx)) return;
#endif
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x16u + ctx->x)));
    native_cmp(ctx, ctx->a, 0x14u);
    if ((ctx->status & NATIVE_Z)) {
        goto Ld618;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x071cu));
    ctx->y = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x16u + ctx->x)));
    native_cmp(ctx, ctx->y, 0x05u);
    if ((ctx->status & NATIVE_Z)) {
        goto Ld5d0;
    }
    native_cmp(ctx, ctx->y, 0x0du);
    if (!(ctx->status & NATIVE_Z)) {
        goto Ld5d2;
    }
Ld5d0: ;
    native_adc(ctx, 0x38u);
Ld5d2: ;
    native_sbc(ctx, 0x48u);
    native_write(ctx, 0x01u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x071au));
    native_sbc(ctx, 0x00u);
    native_write(ctx, 0x00u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x071du));
    native_adc(ctx, 0x48u);
    native_write(ctx, 0x03u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x071bu));
    native_adc(ctx, 0x00u);
    native_write(ctx, 0x02u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x87u + ctx->x)));
    native_cmp(ctx, ctx->a, native_read(ctx, 0x01u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x6eu + ctx->x)));
    native_sbc(ctx, native_read(ctx, 0x00u));
    if ((ctx->status & NATIVE_N)) {
        goto Ld615;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x87u + ctx->x)));
    native_cmp(ctx, ctx->a, native_read(ctx, 0x03u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x6eu + ctx->x)));
    native_sbc(ctx, native_read(ctx, 0x02u));
    if ((ctx->status & NATIVE_N)) {
        goto Ld618;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x)));
    native_cmp(ctx, ctx->a, 0x05u);
    if ((ctx->status & NATIVE_Z)) {
        goto Ld618;
    }
    native_cmp(ctx, ctx->y, 0x0du);
    if ((ctx->status & NATIVE_Z)) {
        goto Ld618;
    }
    native_cmp(ctx, ctx->y, 0x30u);
    if ((ctx->status & NATIVE_Z)) {
        goto Ld618;
    }
    native_cmp(ctx, ctx->y, 0x31u);
    if ((ctx->status & NATIVE_Z)) {
        goto Ld618;
    }
    native_cmp(ctx, ctx->y, 0x32u);
    if ((ctx->status & NATIVE_Z)) {
        goto Ld618;
    }
Ld615: ;
    smbneo_native_fn_actor_erase_c8db(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Ld618: ;
    return;
    return;
}

void smbneo_native_fn_col_proj_actor_proc_d630(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xd630u);
    uint8_t saved_value_0 = 0u;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x24u + ctx->x)));
    if ((ctx->status & NATIVE_Z)) {
        goto Ld68a;
    }
    ctx->a = native_asl(ctx, ctx->a);
    if ((ctx->status & NATIVE_C)) {
        goto Ld68a;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x09u));
    ctx->a = native_lsr(ctx, ctx->a);
    if ((ctx->status & NATIVE_C)) {
        goto Ld68a;
    }
    ctx->a = native_nz(ctx, ctx->x);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x1cu);
    ctx->y = native_nz(ctx, ctx->a);
    ctx->x = native_nz(ctx, 0x04u);
Ld645: ;
    native_write(ctx, 0x01u, ctx->x);
    ctx->a = native_nz(ctx, ctx->y);
    saved_value_0 = ctx->a;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x20u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Ld683;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x0fu + ctx->x)));
    if ((ctx->status & NATIVE_Z)) {
        goto Ld683;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x16u + ctx->x)));
    native_cmp(ctx, ctx->a, 0x24u);
    if (!(ctx->status & NATIVE_C)) {
        goto Ld65d;
    }
    native_cmp(ctx, ctx->a, 0x2bu);
    if (!(ctx->status & NATIVE_C)) {
        goto Ld683;
    }
Ld65d: ;
    native_cmp(ctx, ctx->a, 0x06u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Ld667;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x)));
    native_cmp(ctx, ctx->a, 0x02u);
    if ((ctx->status & NATIVE_C)) {
        goto Ld683;
    }
Ld667: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x03d8u + ctx->x)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Ld683;
    }
    ctx->a = native_nz(ctx, ctx->x);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x04u);
    ctx->x = native_nz(ctx, ctx->a);
    smbneo_native_fn_col_base_actor_e27d(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    if (!(ctx->status & NATIVE_C)) {
        goto Ld683;
    }
    ctx->a = native_nz(ctx, 0x80u);
    native_write(ctx, (uint16_t)(uint8_t)(0x24u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, native_read(ctx, 0x01u));
    smbneo_native_fn_col_proj_actor_damage_d695(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Ld683: ;
    ctx->a = native_nz(ctx, saved_value_0);
    ctx->y = native_nz(ctx, ctx->a);
    ctx->x = native_nz(ctx, native_read(ctx, 0x01u));
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x - 1u));
    if (!(ctx->status & NATIVE_N)) {
        goto Ld645;
    }
Ld68a: ;
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    return;
    return;
}

void smbneo_native_fn_col_proj_actor_damage_d695(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xd695u);
    smbneo_native_fn_pos_calc_x_rel_actor_f0b7(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->x = native_nz(ctx, native_read(ctx, 0x01u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x0fu + ctx->x)));
    if (!(ctx->status & NATIVE_N)) {
        goto Ld6a9;
    }
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x0fu));
    ctx->x = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x16u + ctx->x)));
    native_cmp(ctx, ctx->a, 0x2du);
    if ((ctx->status & NATIVE_Z)) {
        goto Ld6b3;
    }
    ctx->x = native_nz(ctx, native_read(ctx, 0x01u));
Ld6a9: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x16u + ctx->x)));
    native_cmp(ctx, ctx->a, 0x02u);
    if ((ctx->status & NATIVE_Z)) {
        goto Ld71a;
    }
    native_cmp(ctx, ctx->a, 0x2du);
    if (!(ctx->status & NATIVE_Z)) {
        goto Ld6e0;
    }
Ld6b3: ;
    native_write(ctx, 0x0483u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0483u) - 1u)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Ld71a;
    }
    smbneo_native_fn_actor_init_veloc_y_c2a6(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_write(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x), ctx->a);
    native_write(ctx, 0x06cbu, ctx->a);
    ctx->a = native_nz(ctx, 0xfeu);
    native_write(ctx, (uint16_t)(uint8_t)(0xa0u + ctx->x), ctx->a);
    ctx->y = native_nz(ctx, native_read(ctx, 0x075fu));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xd68du + ctx->y)));
    native_write(ctx, (uint16_t)(uint8_t)(0x16u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x20u);
    native_cmp(ctx, ctx->y, 0x03u);
    if ((ctx->status & NATIVE_C)) {
        goto Ld6d4;
    }
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | 0x03u));
Ld6d4: ;
    native_write(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x80u);
    native_write(ctx, 0xfeu, ctx->a);
    ctx->x = native_nz(ctx, native_read(ctx, 0x01u));
    ctx->a = native_nz(ctx, 0x09u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Ld713;
    }
Ld6e0: ;
    native_cmp(ctx, ctx->a, 0x08u);
    if ((ctx->status & NATIVE_Z)) {
        goto Ld71a;
    }
    native_cmp(ctx, ctx->a, 0x0cu);
    if ((ctx->status & NATIVE_Z)) {
        goto Ld71a;
    }
    native_cmp(ctx, ctx->a, 0x15u);
    if ((ctx->status & NATIVE_C)) {
        goto Ld71a;
    }
    smbneo_native_fn_col_actor_kill_d6ec(ctx);
    return;
Ld713: ;
    smbneo_native_fn_col_score_do_d968(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, 0x08u);
    native_write(ctx, 0xffu, ctx->a);
Ld71a: ;
    return;
    return;
}

void smbneo_native_fn_col_actor_kill_d6ec(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xd6ecu);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x16u + ctx->x)));
    native_cmp(ctx, ctx->a, 0x0du);
    if (!(ctx->status & NATIVE_Z)) {
        goto Ld6f8;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x)));
    native_adc(ctx, 0x18u);
    native_write(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x), ctx->a);
Ld6f8: ;
    smbneo_native_fn_col_actor_koopa_demote_check_df71(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x1fu));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | 0x20u));
    native_write(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x02u);
    ctx->y = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x16u + ctx->x)));
    native_cmp(ctx, ctx->y, 0x05u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Ld70d;
    }
    ctx->a = native_nz(ctx, 0x06u);
Ld70d: ;
    native_cmp(ctx, ctx->y, 0x06u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Ld713;
    }
    ctx->a = native_nz(ctx, 0x01u);
Ld713: ;
    smbneo_native_fn_col_score_do_d968(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, 0x08u);
    native_write(ctx, 0xffu, ctx->a);
    return;
    return;
}

void smbneo_native_fn_col_player_hammer_proc_d71b(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xd71bu);
    ctx->a = native_nz(ctx, native_read(ctx, 0x09u));
    ctx->a = native_lsr(ctx, ctx->a);
    if (!(ctx->status & NATIVE_C)) {
        goto Ld756;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x0747u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | native_read(ctx, 0x03d6u)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Ld756;
    }
    ctx->a = native_nz(ctx, ctx->x);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x24u);
    ctx->y = native_nz(ctx, ctx->a);
    smbneo_native_fn_col_base_player_e27b(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    if (!(ctx->status & NATIVE_C)) {
        goto Ld751;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x06beu + ctx->x)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Ld756;
    }
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, (uint16_t)(0x06beu + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x64u + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a ^ 0xffu));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x01u);
    native_write(ctx, (uint16_t)(uint8_t)(0x64u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x079fu));
    if (!(ctx->status & NATIVE_Z)) {
        goto Ld756;
    }
    /* static tail transfer */
    smbneo_native_fn_col_player_harm_proc_d883(ctx);
    return;
Ld751: ;
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, (uint16_t)(0x06beu + ctx->x), ctx->a);
Ld756: ;
    return;
    return;
}

void smbneo_native_fn_col_player_actor_proc_d7aa(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xd7aau);
    uint8_t saved_value_0 = 0u;
    goto Ld7aa;
Ld757: ;
    smbneo_native_fn_actor_erase_c8db(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, 0x06u);
    smbneo_native_fn_col_score_do_d968(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, 0x20u);
    native_write(ctx, 0xfeu, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x39u));
    native_cmp(ctx, ctx->a, 0x02u);
    if (!(ctx->status & NATIVE_C)) {
        goto Ld777;
    }
    native_cmp(ctx, ctx->a, 0x03u);
    if ((ctx->status & NATIVE_Z)) {
        goto Ld791;
    }
    ctx->a = native_nz(ctx, 0x23u);
    native_write(ctx, 0x079fu, ctx->a);
    ctx->a = native_nz(ctx, 0x10u);
    native_write(ctx, 0xfbu, ctx->a);
    return;
Ld777: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x0756u));
    if ((ctx->status & NATIVE_Z)) {
        goto Ld797;
    }
    native_cmp(ctx, ctx->a, 0x01u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Ld7a3;
    }
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, 0x0756u, ctx->a);
    smbneo_native_fn_bg_color_player_87d2(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    ctx->a = native_nz(ctx, 0x0cu);
    /* static tail transfer */
    goto Ld79e;
Ld791: ;
    ctx->a = native_nz(ctx, 0x0bu);
    native_write(ctx, (uint16_t)(0x0110u + ctx->x), ctx->a);
    return;
Ld797: ;
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, 0x0756u, ctx->a);
    ctx->a = native_nz(ctx, 0x09u);
Ld79e: ;
    ctx->y = native_nz(ctx, 0x00u);
    smbneo_native_fn_col_player_harm_proc_proc_change_d89f(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Ld7a3: ;
    return;
Ld7aa: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x09u));
    ctx->a = native_lsr(ctx, ctx->a);
    if ((ctx->status & NATIVE_C)) {
        goto Ld7a3;
    }
    smbneo_native_fn_col_check_player_pos_y_db9a(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    if ((ctx->status & NATIVE_C)) {
        goto Ld7d7;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x03d8u + ctx->x)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Ld7d7;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x0eu));
    native_cmp(ctx, ctx->a, 0x08u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Ld7d7;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x20u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Ld7d7;
    }
    smbneo_native_fn_col_actor_box_get_dbab(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_col_base_player_e27b(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    if ((ctx->status & NATIVE_C)) {
        goto Ld7d8;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0491u + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0xfeu));
    native_write(ctx, (uint16_t)(0x0491u + ctx->x), ctx->a);
Ld7d7: ;
    return;
Ld7d8: ;
    ctx->y = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x16u + ctx->x)));
    native_cmp(ctx, ctx->y, 0x2eu);
    if (!(ctx->status & NATIVE_Z)) {
        goto Ld7e1;
    }
    /* static tail transfer */
    goto Ld757;
Ld7e1: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x079fu));
    if ((ctx->status & NATIVE_Z)) {
        goto Ld7ec;
    }
    /* static tail transfer */
    smbneo_native_fn_col_actor_kill_d6ec(ctx);
    return;
Ld7ec: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0491u + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x01u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | native_read(ctx, (uint16_t)(0x03d8u + ctx->x))));
    if (!(ctx->status & NATIVE_Z)) {
        goto Ld84f;
    }
    ctx->a = native_nz(ctx, 0x01u);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | native_read(ctx, (uint16_t)(0x0491u + ctx->x))));
    native_write(ctx, (uint16_t)(0x0491u + ctx->x), ctx->a);
    native_cmp(ctx, ctx->y, 0x12u);
    if ((ctx->status & NATIVE_Z)) {
        goto Ld850;
    }
    native_cmp(ctx, ctx->y, 0x0du);
    if ((ctx->status & NATIVE_Z)) {
        smbneo_native_fn_col_player_harm_proc_d883(ctx);
        return;
    }
    native_cmp(ctx, ctx->y, 0x0cu);
    if ((ctx->status & NATIVE_Z)) {
        smbneo_native_fn_col_player_harm_proc_d883(ctx);
        return;
    }
    native_cmp(ctx, ctx->y, 0x33u);
    if ((ctx->status & NATIVE_Z)) {
        goto Ld850;
    }
    native_cmp(ctx, ctx->y, 0x15u);
    if ((ctx->status & NATIVE_C)) {
        smbneo_native_fn_col_player_harm_proc_d883(ctx);
        return;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x074eu));
    if ((ctx->status & NATIVE_Z)) {
        smbneo_native_fn_col_player_harm_proc_d883(ctx);
        return;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x)));
    ctx->a = native_asl(ctx, ctx->a);
    if ((ctx->status & NATIVE_C)) {
        goto Ld850;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x07u));
    native_cmp(ctx, ctx->a, 0x02u);
    if (!(ctx->status & NATIVE_C)) {
        goto Ld850;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x16u + ctx->x)));
    native_cmp(ctx, ctx->a, 0x06u);
    if ((ctx->status & NATIVE_Z)) {
        goto Ld84f;
    }
    ctx->a = native_nz(ctx, 0x08u);
    native_write(ctx, 0xffu, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | 0x80u));
    native_write(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x), ctx->a);
    smbneo_native_fn_col_actor_kick_dir_d95c(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xd7a6u + ctx->y)));
    native_write(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x03u);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, 0x0484u));
    ctx->y = native_nz(ctx, native_read(ctx, (uint16_t)(0x0796u + ctx->x)));
    native_cmp(ctx, ctx->y, 0x03u);
    if ((ctx->status & NATIVE_C)) {
        goto Ld84c;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xd7e9u + ctx->y)));
Ld84c: ;
    smbneo_native_fn_col_score_do_d968(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Ld84f: ;
    return;
Ld850: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x9fu));
    if ((ctx->status & NATIVE_N)) {
        goto Ld856;
    }
    if (!(ctx->status & NATIVE_Z)) {
        goto Ld8c0;
    }
Ld856: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x16u + ctx->x)));
    native_cmp(ctx, ctx->a, 0x07u);
    if (!(ctx->status & NATIVE_C)) {
        goto Ld865;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0xceu));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x0cu);
    native_cmp(ctx, ctx->a, native_read(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x)));
    if (!(ctx->status & NATIVE_C)) {
        goto Ld8c0;
    }
Ld865: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x0791u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Ld8c0;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x079eu));
    if (!(ctx->status & NATIVE_Z)) {
        goto Ld8ac;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x03adu));
    native_cmp(ctx, ctx->a, native_read(ctx, 0x03aeu));
    if (!(ctx->status & NATIVE_C)) {
        goto Ld87a;
    }
    /* static tail transfer */
    goto Ld94d;
Ld87a: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x46u + ctx->x)));
    native_cmp(ctx, ctx->a, 0x01u);
    if (!(ctx->status & NATIVE_Z)) {
        smbneo_native_fn_col_player_harm_proc_d883(ctx);
        return;
    }
    /* static tail transfer */
    goto Ld956;
Ld8ac: ;
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    return;
Ld8c0: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x16u + ctx->x)));
    native_cmp(ctx, ctx->a, 0x12u);
    if ((ctx->status & NATIVE_Z)) {
        smbneo_native_fn_col_player_harm_proc_d883(ctx);
        return;
    }
    ctx->a = native_nz(ctx, 0x04u);
    native_write(ctx, 0xffu, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x16u + ctx->x)));
    ctx->y = native_nz(ctx, 0x00u);
    native_cmp(ctx, ctx->a, 0x14u);
    if ((ctx->status & NATIVE_Z)) {
        goto Ld8ed;
    }
    native_cmp(ctx, ctx->a, 0x08u);
    if ((ctx->status & NATIVE_Z)) {
        goto Ld8ed;
    }
    native_cmp(ctx, ctx->a, 0x33u);
    if ((ctx->status & NATIVE_Z)) {
        goto Ld8ed;
    }
    native_cmp(ctx, ctx->a, 0x0cu);
    if ((ctx->status & NATIVE_Z)) {
        goto Ld8ed;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    native_cmp(ctx, ctx->a, 0x05u);
    if ((ctx->status & NATIVE_Z)) {
        goto Ld8ed;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    native_cmp(ctx, ctx->a, 0x11u);
    if ((ctx->status & NATIVE_Z)) {
        goto Ld8ed;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    native_cmp(ctx, ctx->a, 0x07u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Ld90a;
    }
Ld8ed: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xd8bcu + ctx->y)));
    smbneo_native_fn_col_score_do_d968(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x46u + ctx->x)));
    saved_value_0 = ctx->a;
    smbneo_native_fn_col_player_actor_stomp_do_df85(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, saved_value_0);
    native_write(ctx, (uint16_t)(uint8_t)(0x46u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x20u);
    native_write(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x), ctx->a);
    smbneo_native_fn_actor_init_veloc_y_c2a6(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_write(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0xfdu);
    native_write(ctx, 0x9fu, ctx->a);
    return;
Ld90a: ;
    native_cmp(ctx, ctx->a, 0x09u);
    if (!(ctx->status & NATIVE_C)) {
        goto Ld92b;
    }
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x01u));
    native_write(ctx, (uint16_t)(uint8_t)(0x16u + ctx->x), ctx->a);
    ctx->y = native_nz(ctx, 0x00u);
    native_write(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x), ctx->y);
    ctx->a = native_nz(ctx, 0x03u);
    smbneo_native_fn_col_score_do_d968(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_actor_init_veloc_y_c2a6(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_col_actor_kick_dir_d95c(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xd7a8u + ctx->y)));
    native_write(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x), ctx->a);
    /* static tail transfer */
    goto Ld948;
Ld92b: ;
    ctx->a = native_nz(ctx, 0x04u);
    native_write(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x), ctx->a);
    native_write(ctx, 0x0484u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0484u) + 1u)));
    ctx->a = native_nz(ctx, native_read(ctx, 0x0484u));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, 0x0791u));
    smbneo_native_fn_col_score_do_d968(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_write(ctx, 0x0791u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0791u) + 1u)));
    ctx->y = native_nz(ctx, native_read(ctx, 0x076au));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xd929u + ctx->y)));
    native_write(ctx, (uint16_t)(0x0796u + ctx->x), ctx->a);
Ld948: ;
    ctx->a = native_nz(ctx, 0xfcu);
    native_write(ctx, 0x9fu, ctx->a);
    return;
Ld94d: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x46u + ctx->x)));
    native_cmp(ctx, ctx->a, 0x01u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Ld956;
    }
    /* static tail transfer */
    smbneo_native_fn_col_player_harm_proc_d883(ctx);
    return;
Ld956: ;
    smbneo_native_fn_col_actor_calc_dir_check_da73(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    /* static tail transfer */
    smbneo_native_fn_col_player_harm_proc_d883(ctx);
    return;
    return;
}

void smbneo_native_fn_col_player_harm_proc_d883(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xd883u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x079eu));
    if (!(ctx->status & NATIVE_Z)) {
        goto Ld8ac;
    }
    smbneo_native_fn_col_player_harm_proc_do_d888(ctx);
    return;
Ld8ac: ;
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    return;
    return;
}

void smbneo_native_fn_col_player_harm_proc_do_d888(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xd888u);
    ctx->x = native_nz(ctx, native_read(ctx, 0x0756u));
    if ((ctx->status & NATIVE_Z)) {
        goto Ld8af;
    }
    native_write(ctx, 0x0756u, ctx->a);
    ctx->a = native_nz(ctx, 0x08u);
    native_write(ctx, 0x079eu, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    native_write(ctx, 0xffu, ctx->a);
    smbneo_native_fn_bg_color_player_87d2(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, 0x0au);
Ld89d: ;
    ctx->y = native_nz(ctx, 0x01u);
    smbneo_native_fn_col_player_harm_proc_proc_change_d89f(ctx);
    return;
Ld8af: ;
    native_write(ctx, 0x57u, ctx->x);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    native_write(ctx, 0xfcu, ctx->x);
    ctx->a = native_nz(ctx, 0xfcu);
    native_write(ctx, 0x9fu, ctx->a);
    ctx->a = native_nz(ctx, 0x0bu);
    if (!(ctx->status & NATIVE_Z)) {
        goto Ld89d;
    }
    ctx->unsupported = 1; return;
    return;
}

void smbneo_native_fn_col_player_harm_proc_proc_change_d89f(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xd89fu);
    native_write(ctx, 0x0eu, ctx->a);
    native_write(ctx, 0x1du, ctx->y);
    ctx->y = native_nz(ctx, 0xffu);
    native_write(ctx, 0x0747u, ctx->y);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    native_write(ctx, 0x0775u, ctx->y);
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    return;
    return;
}

void smbneo_native_fn_col_actor_kick_dir_d95c(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xd95cu);
    ctx->y = native_nz(ctx, 0x01u);
    smbneo_native_fn_col_player_bullet_diff_e099(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    if (!(ctx->status & NATIVE_N)) {
        goto Ld964;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
Ld964: ;
    native_write(ctx, (uint16_t)(uint8_t)(0x46u + ctx->x), ctx->y);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    return;
    return;
}

void smbneo_native_fn_col_score_do_d968(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xd968u);
    native_write(ctx, (uint16_t)(0x0110u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x30u);
    native_write(ctx, (uint16_t)(0x012cu + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x)));
    native_write(ctx, (uint16_t)(0x011eu + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x03aeu));
    native_write(ctx, (uint16_t)(0x0117u + ctx->x), ctx->a);
    return;
    return;
}

void smbneo_native_fn_col_actor_actor_proc_d98a(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xd98au);
    uint8_t saved_value_0 = 0u;
    goto Ld98a;
Ld97b: ;
    return;
Ld98a: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x09u));
    ctx->a = native_lsr(ctx, ctx->a);
    if (!(ctx->status & NATIVE_C)) {
        goto Ld97b;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x074eu));
    if ((ctx->status & NATIVE_Z)) {
        goto Ld97b;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x16u + ctx->x)));
    native_cmp(ctx, ctx->a, 0x15u);
    if ((ctx->status & NATIVE_C)) {
        goto Lda08;
    }
    native_cmp(ctx, ctx->a, 0x11u);
    if ((ctx->status & NATIVE_Z)) {
        goto Lda08;
    }
    native_cmp(ctx, ctx->a, 0x0du);
    if ((ctx->status & NATIVE_Z)) {
        goto Lda08;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x03d8u + ctx->x)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lda08;
    }
    smbneo_native_fn_col_actor_box_get_dbab(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x - 1u));
    if ((ctx->status & NATIVE_N)) {
        goto Lda08;
    }
Ld9ad: ;
    native_write(ctx, 0x01u, ctx->x);
    ctx->a = native_nz(ctx, ctx->y);
    saved_value_0 = ctx->a;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x0fu + ctx->x)));
    if ((ctx->status & NATIVE_Z)) {
        goto Lda01;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x16u + ctx->x)));
    native_cmp(ctx, ctx->a, 0x15u);
    if ((ctx->status & NATIVE_C)) {
        goto Lda01;
    }
    native_cmp(ctx, ctx->a, 0x11u);
    if ((ctx->status & NATIVE_Z)) {
        goto Lda01;
    }
    native_cmp(ctx, ctx->a, 0x0du);
    if ((ctx->status & NATIVE_Z)) {
        goto Lda01;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x03d8u + ctx->x)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lda01;
    }
    ctx->a = native_nz(ctx, ctx->x);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x04u);
    ctx->x = native_nz(ctx, ctx->a);
    smbneo_native_fn_col_base_actor_e27d(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    ctx->y = native_nz(ctx, native_read(ctx, 0x01u));
    if (!(ctx->status & NATIVE_C)) {
        goto Ld9f8;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | native_read(ctx, (uint16_t)(0x001eu + ctx->y))));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x80u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Ld9f2;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0491u + ctx->y)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & native_read(ctx, (uint16_t)(0xd97cu + ctx->x))));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lda01;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0491u + ctx->y)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | native_read(ctx, (uint16_t)(0xd97cu + ctx->x))));
    native_write(ctx, (uint16_t)(0x0491u + ctx->y), ctx->a);
Ld9f2: ;
    smbneo_native_fn_col_actor_actor_do_da0b(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    /* static tail transfer */
    goto Lda01;
Ld9f8: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0491u + ctx->y)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & native_read(ctx, (uint16_t)(0xd983u + ctx->x))));
    native_write(ctx, (uint16_t)(0x0491u + ctx->y), ctx->a);
Lda01: ;
    ctx->a = native_nz(ctx, saved_value_0);
    ctx->y = native_nz(ctx, ctx->a);
    ctx->x = native_nz(ctx, native_read(ctx, 0x01u));
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x - 1u));
    if (!(ctx->status & NATIVE_N)) {
        goto Ld9ad;
    }
Lda08: ;
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    return;
    return;
}

void smbneo_native_fn_col_actor_actor_do_da0b(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xda0bu);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x001eu + ctx->y)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | native_read(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x))));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x20u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lda47;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x)));
    native_cmp(ctx, ctx->a, 0x06u);
    if (!(ctx->status & NATIVE_C)) {
        goto Lda48;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x16u + ctx->x)));
    native_cmp(ctx, ctx->a, 0x05u);
    if ((ctx->status & NATIVE_Z)) {
        goto Lda47;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x001eu + ctx->y)));
    ctx->a = native_asl(ctx, ctx->a);
    if (!(ctx->status & NATIVE_C)) {
        goto Lda30;
    }
    ctx->a = native_nz(ctx, 0x06u);
    smbneo_native_fn_col_score_do_d968(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_col_actor_kill_d6ec(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->y = native_nz(ctx, native_read(ctx, 0x01u));
Lda30: ;
    ctx->a = native_nz(ctx, ctx->y);
    ctx->x = native_nz(ctx, ctx->a);
    smbneo_native_fn_col_actor_kill_d6ec(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0125u + ctx->x)));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x04u);
    ctx->x = native_nz(ctx, native_read(ctx, 0x01u));
    smbneo_native_fn_col_score_do_d968(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    native_write(ctx, (uint16_t)(0x0125u + ctx->x), native_nz(ctx, (uint8_t)(native_read(ctx, (uint16_t)(0x0125u + ctx->x)) + 1u)));
Lda47: ;
    return;
Lda48: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x001eu + ctx->y)));
    native_cmp(ctx, ctx->a, 0x06u);
    if (!(ctx->status & NATIVE_C)) {
        goto Lda6c;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0016u + ctx->y)));
    native_cmp(ctx, ctx->a, 0x05u);
    if ((ctx->status & NATIVE_Z)) {
        goto Lda47;
    }
    smbneo_native_fn_col_actor_kill_d6ec(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->y = native_nz(ctx, native_read(ctx, 0x01u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0125u + ctx->y)));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x04u);
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    smbneo_native_fn_col_score_do_d968(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->x = native_nz(ctx, native_read(ctx, 0x01u));
    native_write(ctx, (uint16_t)(0x0125u + ctx->x), native_nz(ctx, (uint8_t)(native_read(ctx, (uint16_t)(0x0125u + ctx->x)) + 1u)));
    return;
Lda6c: ;
    ctx->a = native_nz(ctx, ctx->y);
    ctx->x = native_nz(ctx, ctx->a);
    smbneo_native_fn_col_actor_calc_dir_check_da73(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    smbneo_native_fn_col_actor_calc_dir_check_da73(ctx);
    return;
    return;
}

void smbneo_native_fn_col_actor_calc_dir_check_da73(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xda73u);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x16u + ctx->x)));
    native_cmp(ctx, ctx->a, 0x0du);
    if ((ctx->status & NATIVE_Z)) {
        goto Lda9b;
    }
    native_cmp(ctx, ctx->a, 0x11u);
    if ((ctx->status & NATIVE_Z)) {
        goto Lda9b;
    }
    native_cmp(ctx, ctx->a, 0x05u);
    if ((ctx->status & NATIVE_Z)) {
        goto Lda9b;
    }
    native_cmp(ctx, ctx->a, 0x12u);
    if ((ctx->status & NATIVE_Z)) {
        goto Lda8d;
    }
    native_cmp(ctx, ctx->a, 0x0eu);
    if ((ctx->status & NATIVE_Z)) {
        goto Lda8d;
    }
    native_cmp(ctx, ctx->a, 0x07u);
    if ((ctx->status & NATIVE_C)) {
        goto Lda9b;
    }
Lda8d: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a ^ 0xffu));
    ctx->y = native_nz(ctx, ctx->a);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    native_write(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x), ctx->y);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x46u + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a ^ 0x03u));
    native_write(ctx, (uint16_t)(uint8_t)(0x46u + ctx->x), ctx->a);
Lda9b: ;
    return;
    return;
}

void smbneo_native_fn_col_plat_l_proc_da9c(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xda9cu);
    ctx->a = native_nz(ctx, 0xffu);
    native_write(ctx, (uint16_t)(0x03a2u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0747u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Ldacf;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x)));
    if ((ctx->status & NATIVE_N)) {
        goto Ldacf;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x16u + ctx->x)));
    native_cmp(ctx, ctx->a, 0x24u);
    if (!(ctx->status & NATIVE_Z)) {
        smbneo_native_fn_region_dab6_dab6(ctx);
        return;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x)));
    ctx->x = native_nz(ctx, ctx->a);
    smbneo_native_fn_region_dab6_dab6(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_region_dab6_dab6(ctx);
    return;
Ldacf: ;
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    return;
    return;
}

void smbneo_native_fn_region_dab6_dab6(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xdab6u);
    uint8_t saved_value_0 = 0u;
    smbneo_native_fn_col_check_player_pos_y_db9a(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    if ((ctx->status & NATIVE_C)) {
        goto Ldacf;
    }
    ctx->a = native_nz(ctx, ctx->x);
    smbneo_native_fn_col_actor_box_get_do_dbad(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x)));
    native_write(ctx, 0x00u, ctx->a);
    ctx->a = native_nz(ctx, ctx->x);
    saved_value_0 = ctx->a;
    smbneo_native_fn_col_base_player_e27b(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, saved_value_0);
    ctx->x = native_nz(ctx, ctx->a);
    if (!(ctx->status & NATIVE_C)) {
        goto Ldacf;
    }
    smbneo_native_fn_col_plat_l_do_db13(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Ldacf: ;
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    return;
    return;
}

void smbneo_native_fn_col_plat_s_proc_dad2(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xdad2u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0747u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Ldb0e;
    }
    native_write(ctx, (uint16_t)(0x03a2u + ctx->x), ctx->a);
    smbneo_native_fn_col_check_player_pos_y_db9a(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    if ((ctx->status & NATIVE_C)) {
        goto Ldb0e;
    }
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, 0x00u, ctx->a);
Ldae3: ;
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    smbneo_native_fn_col_actor_box_get_dbab(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x02u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Ldb0e;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x04adu + ctx->y)));
    native_cmp(ctx, ctx->a, 0x20u);
    if (!(ctx->status & NATIVE_C)) {
        goto Ldaf8;
    }
    smbneo_native_fn_col_base_player_e27b(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    if ((ctx->status & NATIVE_C)) {
        goto Ldb11;
    }
Ldaf8: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x04adu + ctx->y)));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x80u);
    native_write(ctx, (uint16_t)(0x04adu + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x04afu + ctx->y)));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x80u);
    native_write(ctx, (uint16_t)(0x04afu + ctx->y), ctx->a);
    native_write(ctx, 0x00u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x00u) - 1u)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Ldae3;
    }
Ldb0e: ;
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    return;
Ldb11: ;
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    smbneo_native_fn_col_plat_l_do_db13(ctx);
    return;
    return;
}

void smbneo_native_fn_col_plat_l_do_db13(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xdb13u);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x04afu + ctx->y)));
    ctx->status |= NATIVE_C;
    native_sbc(ctx, native_read(ctx, 0x04adu));
    native_cmp(ctx, ctx->a, 0x04u);
    if ((ctx->status & NATIVE_C)) {
        goto Ldb26;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x9fu));
    if (!(ctx->status & NATIVE_N)) {
        goto Ldb26;
    }
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, 0x9fu, ctx->a);
Ldb26: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x04afu));
    ctx->status |= NATIVE_C;
    native_sbc(ctx, native_read(ctx, (uint16_t)(0x04adu + ctx->y)));
    native_cmp(ctx, ctx->a, 0x06u);
    if ((ctx->status & NATIVE_C)) {
        goto Ldb4c;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x9fu));
    if ((ctx->status & NATIVE_N)) {
        goto Ldb4c;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x00u));
    ctx->y = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x16u + ctx->x)));
    native_cmp(ctx, ctx->y, 0x2bu);
    if ((ctx->status & NATIVE_Z)) {
        goto Ldb42;
    }
    native_cmp(ctx, ctx->y, 0x2cu);
    if ((ctx->status & NATIVE_Z)) {
        goto Ldb42;
    }
    ctx->a = native_nz(ctx, ctx->x);
Ldb42: ;
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    native_write(ctx, (uint16_t)(0x03a2u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x1du, ctx->a);
    return;
Ldb4c: ;
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, 0x00u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x04aeu));
    ctx->status |= NATIVE_C;
    native_sbc(ctx, native_read(ctx, (uint16_t)(0x04acu + ctx->y)));
    native_cmp(ctx, ctx->a, 0x08u);
    if (!(ctx->status & NATIVE_C)) {
        goto Ldb68;
    }
    native_write(ctx, 0x00u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x00u) + 1u)));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x04aeu + ctx->y)));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_sbc(ctx, native_read(ctx, 0x04acu));
    native_cmp(ctx, ctx->a, 0x09u);
    if ((ctx->status & NATIVE_C)) {
        goto Ldb6b;
    }
Ldb68: ;
    smbneo_native_fn_col_bg_proc_player_halt_dea1(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Ldb6b: ;
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    return;
    return;
}

void smbneo_native_fn_col_plat_s_player_pos_db70(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xdb70u);
    ctx->y = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x)));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, (uint16_t)(0xdb6du + ctx->y)));
    /* static tail transfer */
    goto Ldb7c;
Ldb7c: ;
    ctx->y = native_nz(ctx, native_read(ctx, 0x0eu));
    native_cmp(ctx, ctx->y, 0x0bu);
    if ((ctx->status & NATIVE_Z)) {
        goto Ldb99;
    }
    ctx->y = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xb6u + ctx->x)));
    native_cmp(ctx, ctx->y, 0x01u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Ldb99;
    }
    ctx->status |= NATIVE_C;
    native_sbc(ctx, 0x20u);
    native_write(ctx, 0xceu, ctx->a);
    ctx->a = native_nz(ctx, ctx->y);
    native_sbc(ctx, 0x00u);
    native_write(ctx, 0xb5u, ctx->a);
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x9fu, ctx->a);
    native_write(ctx, 0x0433u, ctx->a);
Ldb99: ;
    return;
    return;
}

void smbneo_native_fn_col_plat_player_pos_db7a(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xdb7au);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x)));
    ctx->y = native_nz(ctx, native_read(ctx, 0x0eu));
    native_cmp(ctx, ctx->y, 0x0bu);
    if ((ctx->status & NATIVE_Z)) {
        goto Ldb99;
    }
    ctx->y = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xb6u + ctx->x)));
    native_cmp(ctx, ctx->y, 0x01u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Ldb99;
    }
    ctx->status |= NATIVE_C;
    native_sbc(ctx, 0x20u);
    native_write(ctx, 0xceu, ctx->a);
    ctx->a = native_nz(ctx, ctx->y);
    native_sbc(ctx, 0x00u);
    native_write(ctx, 0xb5u, ctx->a);
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x9fu, ctx->a);
    native_write(ctx, 0x0433u, ctx->a);
Ldb99: ;
    return;
    return;
}

void smbneo_native_fn_col_check_player_pos_y_db9a(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xdb9au);
    ctx->a = native_nz(ctx, native_read(ctx, 0x03d0u));
    native_cmp(ctx, ctx->a, 0xf0u);
    if ((ctx->status & NATIVE_C)) {
        goto Ldbaa;
    }
    ctx->y = native_nz(ctx, native_read(ctx, 0xb5u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Ldbaa;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0xceu));
    native_cmp(ctx, ctx->a, 0xd0u);
Ldbaa: ;
    return;
    return;
}

void smbneo_native_fn_col_actor_box_get_dbab(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xdbabu);
    ctx->a = native_nz(ctx, native_read(ctx, 0x08u));
    smbneo_native_fn_col_actor_box_get_do_dbad(ctx);
    return;
    return;
}

void smbneo_native_fn_col_actor_box_get_do_dbad(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xdbadu);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x04u);
    ctx->y = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x03d1u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x0fu));
    native_cmp(ctx, ctx->a, 0x0fu);
    return;
    return;
}

void smbneo_native_fn_col_bg_proc_dbbd(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xdbbdu);
    uint8_t saved_value_0 = 0u;
    ctx->a = native_nz(ctx, native_read(ctx, 0x0716u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Ldbf0;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x0eu));
    native_cmp(ctx, ctx->a, 0x0bu);
    if ((ctx->status & NATIVE_Z)) {
        goto Ldbf0;
    }
    native_cmp(ctx, ctx->a, 0x04u);
    if (!(ctx->status & NATIVE_C)) {
        goto Ldbf0;
    }
    ctx->a = native_nz(ctx, 0x01u);
    ctx->y = native_nz(ctx, native_read(ctx, 0x0704u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Ldbdd;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x1du));
    if ((ctx->status & NATIVE_Z)) {
        goto Ldbdb;
    }
    native_cmp(ctx, ctx->a, 0x03u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Ldbdf;
    }
Ldbdb: ;
    ctx->a = native_nz(ctx, 0x02u);
Ldbdd: ;
    native_write(ctx, 0x1du, ctx->a);
Ldbdf: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0xb5u));
    native_cmp(ctx, ctx->a, 0x01u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Ldbf0;
    }
    ctx->a = native_nz(ctx, 0xffu);
    native_write(ctx, 0x0490u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0xceu));
    native_cmp(ctx, ctx->a, 0xcfu);
    if (!(ctx->status & NATIVE_C)) {
        goto Ldbf1;
    }
Ldbf0: ;
    return;
Ldbf1: ;
    ctx->y = native_nz(ctx, 0x02u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0714u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Ldc04;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x0754u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Ldc04;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    ctx->a = native_nz(ctx, native_read(ctx, 0x0704u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Ldc04;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
Ldc04: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xe303u + ctx->y)));
    native_write(ctx, 0xebu, ctx->a);
    ctx->y = native_nz(ctx, ctx->a);
    ctx->x = native_nz(ctx, native_read(ctx, 0x0754u));
    ctx->a = native_nz(ctx, native_read(ctx, 0x0714u));
    if ((ctx->status & NATIVE_Z)) {
        goto Ldc13;
    }
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
Ldc13: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0xceu));
    native_cmp(ctx, ctx->a, native_read(ctx, (uint16_t)(0xdbbbu + ctx->x)));
    if (!(ctx->status & NATIVE_C)) {
        goto Ldc4f;
    }
    smbneo_native_fn_col_box_player_jump_e33f(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    if ((ctx->status & NATIVE_Z)) {
        goto Ldc4f;
    }
    smbneo_native_fn_col_bg_proc_coin_check_def7(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    if ((ctx->status & NATIVE_C)) {
        goto Ldc73;
    }
    ctx->y = native_nz(ctx, native_read(ctx, 0x9fu));
    if (!(ctx->status & NATIVE_N)) {
        goto Ldc4f;
    }
    ctx->y = native_nz(ctx, native_read(ctx, 0x04u));
    native_cmp(ctx, ctx->y, 0x04u);
    if (!(ctx->status & NATIVE_C)) {
        goto Ldc4f;
    }
    smbneo_native_fn_col_bg_proc_solid_check_dee5(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    if ((ctx->status & NATIVE_C)) {
        goto Ldc43;
    }
    ctx->y = native_nz(ctx, native_read(ctx, 0x074eu));
    if ((ctx->status & NATIVE_Z)) {
        goto Ldc4b;
    }
    ctx->y = native_nz(ctx, native_read(ctx, 0x0784u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Ldc4b;
    }
    smbneo_native_fn_block_punch_bbf8(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    /* static tail transfer */
    goto Ldc4f;
Ldc43: ;
    native_cmp(ctx, ctx->a, 0x26u);
    if ((ctx->status & NATIVE_Z)) {
        goto Ldc4b;
    }
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, 0xffu, ctx->a);
Ldc4b: ;
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, 0x9fu, ctx->a);
Ldc4f: ;
    ctx->y = native_nz(ctx, native_read(ctx, 0xebu));
    ctx->a = native_nz(ctx, native_read(ctx, 0xceu));
    native_cmp(ctx, ctx->a, 0xcfu);
    if ((ctx->status & NATIVE_C)) {
        goto Ldcb7;
    }
    smbneo_native_fn_col_box_player_step_e33e(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_col_bg_proc_coin_check_def7(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    if ((ctx->status & NATIVE_C)) {
        goto Ldc73;
    }
    saved_value_0 = ctx->a;
    smbneo_native_fn_col_box_player_step_e33e(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_write(ctx, 0x00u, ctx->a);
    ctx->a = native_nz(ctx, saved_value_0);
    native_write(ctx, 0x01u, ctx->a);
    if (!(ctx->status & NATIVE_Z)) {
        goto Ldc76;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x00u));
    if ((ctx->status & NATIVE_Z)) {
        goto Ldcb7;
    }
    smbneo_native_fn_col_bg_proc_coin_check_def7(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    if (!(ctx->status & NATIVE_C)) {
        goto Ldc76;
    }
Ldc73: ;
    /* static tail transfer */
    goto Ldd5e;
Ldc76: ;
    smbneo_native_fn_col_bg_proc_climb_check_def0(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    if ((ctx->status & NATIVE_C)) {
        goto Ldcb7;
    }
    ctx->y = native_nz(ctx, native_read(ctx, 0x9fu));
    if ((ctx->status & NATIVE_N)) {
        goto Ldcb7;
    }
    native_cmp(ctx, ctx->a, 0xc5u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Ldc86;
    }
    /* static tail transfer */
    goto Ldd67;
Ldc86: ;
    smbneo_native_fn_col_bg_proc_vis_de16(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    if ((ctx->status & NATIVE_Z)) {
        goto Ldcb7;
    }
    ctx->y = native_nz(ctx, native_read(ctx, 0x070eu));
    if (!(ctx->status & NATIVE_Z)) {
        goto Ldcb3;
    }
    ctx->y = native_nz(ctx, native_read(ctx, 0x04u));
    native_cmp(ctx, ctx->y, 0x05u);
    if (!(ctx->status & NATIVE_C)) {
        goto Ldc9d;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x45u));
    native_write(ctx, 0x00u, ctx->a);
    /* static tail transfer */
    smbneo_native_fn_col_bg_proc_player_halt_dea1(ctx);
    return;
Ldc9d: ;
    smbneo_native_fn_col_bg_proc_spring_de1d(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, 0xf0u);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & native_read(ctx, 0xceu)));
    native_write(ctx, 0xceu, ctx->a);
    smbneo_native_fn_col_bg_proc_pipe_v_do_de41(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x9fu, ctx->a);
    native_write(ctx, 0x0433u, ctx->a);
    native_write(ctx, 0x0484u, ctx->a);
Ldcb3: ;
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x1du, ctx->a);
Ldcb7: ;
    ctx->y = native_nz(ctx, native_read(ctx, 0xebu));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, 0x00u, ctx->a);
Ldcbf: ;
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    native_write(ctx, 0xebu, ctx->y);
    ctx->a = native_nz(ctx, native_read(ctx, 0xceu));
    native_cmp(ctx, ctx->a, 0x20u);
    if (!(ctx->status & NATIVE_C)) {
        goto Ldcde;
    }
    native_cmp(ctx, ctx->a, 0xe4u);
    if ((ctx->status & NATIVE_C)) {
        goto Ldcf4;
    }
    smbneo_native_fn_col_box_player_back_e344(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    if ((ctx->status & NATIVE_Z)) {
        goto Ldcde;
    }
    native_cmp(ctx, ctx->a, 0x1cu);
    if ((ctx->status & NATIVE_Z)) {
        goto Ldcde;
    }
    native_cmp(ctx, ctx->a, 0x6bu);
    if ((ctx->status & NATIVE_Z)) {
        goto Ldcde;
    }
    smbneo_native_fn_col_bg_proc_climb_check_def0(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    if (!(ctx->status & NATIVE_C)) {
        goto Ldcf5;
    }
Ldcde: ;
    ctx->y = native_nz(ctx, native_read(ctx, 0xebu));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->a = native_nz(ctx, native_read(ctx, 0xceu));
    native_cmp(ctx, ctx->a, 0x08u);
    if (!(ctx->status & NATIVE_C)) {
        goto Ldcf4;
    }
    native_cmp(ctx, ctx->a, 0xd0u);
    if ((ctx->status & NATIVE_C)) {
        goto Ldcf4;
    }
    smbneo_native_fn_col_box_player_back_e344(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    if (!(ctx->status & NATIVE_Z)) {
        goto Ldcf5;
    }
    native_write(ctx, 0x00u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x00u) - 1u)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Ldcbf;
    }
Ldcf4: ;
    return;
Ldcf5: ;
    smbneo_native_fn_col_bg_proc_vis_de16(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    if ((ctx->status & NATIVE_Z)) {
        goto Ldd5b;
    }
    smbneo_native_fn_col_bg_proc_climb_check_def0(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    if (!(ctx->status & NATIVE_C)) {
        goto Ldd02;
    }
    /* static tail transfer */
    goto Ldd87;
Ldd02: ;
    smbneo_native_fn_col_bg_proc_coin_check_def7(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    if ((ctx->status & NATIVE_C)) {
        goto Ldd5e;
    }
    smbneo_native_fn_col_bg_proc_spring_check_de36(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    if (!(ctx->status & NATIVE_C)) {
        goto Ldd14;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x070eu));
    if (!(ctx->status & NATIVE_Z)) {
        goto Ldd5b;
    }
    /* static tail transfer */
    goto Ldd58;
Ldd14: ;
    ctx->y = native_nz(ctx, native_read(ctx, 0x1du));
    native_cmp(ctx, ctx->y, 0x00u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Ldd58;
    }
    ctx->y = native_nz(ctx, native_read(ctx, 0x33u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Ldd58;
    }
    native_cmp(ctx, ctx->a, 0x6cu);
    if ((ctx->status & NATIVE_Z)) {
        goto Ldd27;
    }
    native_cmp(ctx, ctx->a, 0x1fu);
    if (!(ctx->status & NATIVE_Z)) {
        goto Ldd58;
    }
Ldd27: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x03c4u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Ldd30;
    }
    ctx->y = native_nz(ctx, 0x10u);
    native_write(ctx, 0xffu, ctx->y);
Ldd30: ;
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | 0x20u));
    native_write(ctx, 0x03c4u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x86u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x0fu));
    if ((ctx->status & NATIVE_Z)) {
        goto Ldd49;
    }
    ctx->y = native_nz(ctx, 0x00u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x071au));
    if ((ctx->status & NATIVE_Z)) {
        goto Ldd43;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
Ldd43: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xdd5cu + ctx->y)));
    native_write(ctx, 0x06deu, ctx->a);
Ldd49: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x0eu));
    native_cmp(ctx, ctx->a, 0x07u);
    if ((ctx->status & NATIVE_Z)) {
        goto Ldd5b;
    }
    native_cmp(ctx, ctx->a, 0x08u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Ldd5b;
    }
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, 0x0eu, ctx->a);
    return;
Ldd58: ;
    smbneo_native_fn_col_bg_proc_player_halt_dea1(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Ldd5b: ;
    return;
Ldd5e: ;
    smbneo_native_fn_col_bg_proc_bg_tile_blank_dd75(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_write(ctx, 0x0748u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0748u) + 1u)));
    /* static tail transfer */
    smbneo_native_fn_misc_proc_coin_add_baf6(ctx);
    return;
Ldd67: ;
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x0772u, ctx->a);
    ctx->a = native_nz(ctx, 0x03u);
    native_write(ctx, 0x0770u, ctx->a);
    ctx->a = native_nz(ctx, 0x18u);
    native_write(ctx, 0x57u, ctx->a);
    smbneo_native_fn_col_bg_proc_bg_tile_blank_dd75(ctx);
    return;
Ldd87: ;
    ctx->y = native_nz(ctx, native_read(ctx, 0x04u));
    native_cmp(ctx, ctx->y, 0x06u);
    if (!(ctx->status & NATIVE_C)) {
        goto Ldd91;
    }
    native_cmp(ctx, ctx->y, 0x0au);
    if (!(ctx->status & NATIVE_C)) {
        goto Ldd92;
    }
Ldd91: ;
    return;
Ldd92: ;
    native_cmp(ctx, ctx->a, 0x24u);
    if ((ctx->status & NATIVE_Z)) {
        goto Ldd9a;
    }
    native_cmp(ctx, ctx->a, 0x25u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lddd3;
    }
Ldd9a: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x0eu));
    native_cmp(ctx, ctx->a, 0x05u);
    if ((ctx->status & NATIVE_Z)) {
        goto Ldde1;
    }
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, 0x33u, ctx->a);
    native_write(ctx, 0x0723u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0723u) + 1u)));
    ctx->a = native_nz(ctx, native_read(ctx, 0x0eu));
    native_cmp(ctx, ctx->a, 0x04u);
    if ((ctx->status & NATIVE_Z)) {
        goto Lddcc;
    }
    ctx->a = native_nz(ctx, 0x33u);
    smbneo_native_fn_scenery_actor_kill_a63c(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, 0x80u);
    native_write(ctx, 0xfcu, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    native_write(ctx, 0x0713u, ctx->a);
    ctx->x = native_nz(ctx, 0x04u);
    ctx->a = native_nz(ctx, native_read(ctx, 0xceu));
    native_write(ctx, 0x070fu, ctx->a);
Lddc1: ;
    native_cmp(ctx, ctx->a, native_read(ctx, (uint16_t)(0xdd82u + ctx->x)));
    if ((ctx->status & NATIVE_C)) {
        goto Lddc9;
    }
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x - 1u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lddc1;
    }
Lddc9: ;
    native_write(ctx, 0x010fu, ctx->x);
Lddcc: ;
    ctx->a = native_nz(ctx, 0x04u);
    native_write(ctx, 0x0eu, ctx->a);
    /* static tail transfer */
    goto Ldde1;
Lddd3: ;
    native_cmp(ctx, ctx->a, 0x26u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Ldde1;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0xceu));
    native_cmp(ctx, ctx->a, 0x20u);
    if ((ctx->status & NATIVE_C)) {
        goto Ldde1;
    }
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, 0x0eu, ctx->a);
Ldde1: ;
    ctx->a = native_nz(ctx, 0x03u);
    native_write(ctx, 0x1du, ctx->a);
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x57u, ctx->a);
    native_write(ctx, 0x0705u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x8cu));
    ctx->status |= NATIVE_C;
    native_sbc(ctx, native_read(ctx, 0x071cu));
    native_cmp(ctx, ctx->a, 0x0au);
    if ((ctx->status & NATIVE_C)) {
        goto Lddfa;
    }
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, 0x33u, ctx->a);
Lddfa: ;
    ctx->y = native_nz(ctx, native_read(ctx, 0x33u));
    ctx->a = native_nz(ctx, native_read(ctx, 0x06u));
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, (uint16_t)(0xdd7du + ctx->y)));
    native_write(ctx, 0x86u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x06u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lde15;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x071bu));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, (uint16_t)(0xdd7fu + ctx->y)));
    native_write(ctx, 0x6du, ctx->a);
Lde15: ;
    return;
    return;
}

void smbneo_native_fn_col_bg_proc_bg_tile_blank_dd75(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xdd75u);
    ctx->y = native_nz(ctx, native_read(ctx, 0x02u));
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, (uint16_t)(native_zpword(ctx, 0x06u) + ctx->y), ctx->a);
    /* static tail transfer */
    smbneo_native_fn_meta_put_blank_8c4e(ctx);
    return;
    return;
}

void smbneo_native_fn_col_bg_proc_vis_de16(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xde16u);
    native_cmp(ctx, ctx->a, 0x5fu);
    if ((ctx->status & NATIVE_Z)) {
        goto Lde1c;
    }
    native_cmp(ctx, ctx->a, 0x60u);
Lde1c: ;
    return;
    return;
}

void smbneo_native_fn_col_bg_proc_spring_de1d(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xde1du);
    smbneo_native_fn_col_bg_proc_spring_check_de36(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    if (!(ctx->status & NATIVE_C)) {
        goto Lde35;
    }
    ctx->a = native_nz(ctx, 0x70u);
    native_write(ctx, 0x0709u, ctx->a);
    ctx->a = native_nz(ctx, 0xf9u);
    native_write(ctx, 0x06dbu, ctx->a);
    ctx->a = native_nz(ctx, 0x03u);
    native_write(ctx, 0x0786u, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    native_write(ctx, 0x070eu, ctx->a);
Lde35: ;
    return;
    return;
}

void smbneo_native_fn_col_bg_proc_spring_check_de36(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xde36u);
    native_cmp(ctx, ctx->a, 0x67u);
    if ((ctx->status & NATIVE_Z)) {
        goto Lde3f;
    }
    native_cmp(ctx, ctx->a, 0x68u);
    ctx->status &= (uint8_t)~NATIVE_C;
    if (!(ctx->status & NATIVE_Z)) {
        goto Lde40;
    }
Lde3f: ;
    ctx->status |= NATIVE_C;
Lde40: ;
    return;
    return;
}

void smbneo_native_fn_col_bg_proc_pipe_v_do_de41(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xde41u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0bu));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x04u));
    if ((ctx->status & NATIVE_Z)) {
        goto Ldea0;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x00u));
    native_cmp(ctx, ctx->a, 0x11u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Ldea0;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x01u));
    native_cmp(ctx, ctx->a, 0x10u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Ldea0;
    }
    ctx->a = native_nz(ctx, 0x30u);
    native_write(ctx, 0x06deu, ctx->a);
    ctx->a = native_nz(ctx, 0x03u);
    native_write(ctx, 0x0eu, ctx->a);
    ctx->a = native_nz(ctx, 0x10u);
    native_write(ctx, 0xffu, ctx->a);
    ctx->a = native_nz(ctx, 0x20u);
    native_write(ctx, 0x03c4u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x06d6u));
    if ((ctx->status & NATIVE_Z)) {
        goto Ldea0;
    }
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x03u));
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->x = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x86u));
    native_cmp(ctx, ctx->a, 0x60u);
    if (!(ctx->status & NATIVE_C)) {
        goto Lde7b;
    }
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    native_cmp(ctx, ctx->a, 0xa0u);
    if (!(ctx->status & NATIVE_C)) {
        goto Lde7b;
    }
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
Lde7b: ;
    ctx->y = native_nz(ctx, native_read(ctx, (uint16_t)(0x89eeu + ctx->x)));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    native_write(ctx, 0x075fu, ctx->y);
    ctx->x = native_nz(ctx, native_read(ctx, (uint16_t)(0xac77u + ctx->y)));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xac7fu + ctx->x)));
    native_write(ctx, 0x0750u, ctx->a);
    ctx->a = native_nz(ctx, 0x80u);
    native_write(ctx, 0xfcu, ctx->a);
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x0751u, ctx->a);
    native_write(ctx, 0x0760u, ctx->a);
    native_write(ctx, 0x075cu, ctx->a);
    native_write(ctx, 0x0752u, ctx->a);
    native_write(ctx, 0x0757u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0757u) + 1u)));
Ldea0: ;
    return;
    return;
}

void smbneo_native_fn_col_bg_proc_player_halt_dea1(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xdea1u);
    ctx->a = native_nz(ctx, 0x00u);
    ctx->y = native_nz(ctx, native_read(ctx, 0x57u));
    ctx->x = native_nz(ctx, native_read(ctx, 0x00u));
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x - 1u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Ldeb4;
    }
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    native_cmp(ctx, ctx->y, 0x00u);
    if ((ctx->status & NATIVE_N)) {
        goto Lded7;
    }
    ctx->a = native_nz(ctx, 0xffu);
    /* static tail transfer */
    goto Ldebc;
Ldeb4: ;
    ctx->x = native_nz(ctx, 0x02u);
    native_cmp(ctx, ctx->y, 0x01u);
    if (!(ctx->status & NATIVE_N)) {
        goto Lded7;
    }
    ctx->a = native_nz(ctx, 0x01u);
Ldebc: ;
    ctx->y = native_nz(ctx, 0x10u);
    native_write(ctx, 0x0785u, ctx->y);
    ctx->y = native_nz(ctx, 0x00u);
    native_write(ctx, 0x57u, ctx->y);
    native_cmp(ctx, ctx->a, 0x00u);
    if (!(ctx->status & NATIVE_N)) {
        goto Ldeca;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
Ldeca: ;
    native_write(ctx, 0x00u, ctx->y);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, 0x86u));
    native_write(ctx, 0x86u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x6du));
    native_adc(ctx, native_read(ctx, 0x00u));
    native_write(ctx, 0x6du, ctx->a);
Lded7: ;
    ctx->a = native_nz(ctx, ctx->x);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a ^ 0xffu));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & native_read(ctx, 0x0490u)));
    native_write(ctx, 0x0490u, ctx->a);
    return;
    return;
}

void smbneo_native_fn_col_bg_proc_solid_check_dee5(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xdee5u);
    smbneo_native_fn_col_bg_proc_get_tile_page_df06(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_cmp(ctx, ctx->a, native_read(ctx, (uint16_t)(0xdee1u + ctx->x)));
    return;
    return;
}

void smbneo_native_fn_col_bg_proc_climb_check_def0(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xdef0u);
    smbneo_native_fn_col_bg_proc_get_tile_page_df06(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_cmp(ctx, ctx->a, native_read(ctx, (uint16_t)(0xdeecu + ctx->x)));
    return;
    return;
}

void smbneo_native_fn_col_bg_proc_coin_check_def7(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xdef7u);
    native_cmp(ctx, ctx->a, 0xc2u);
    if ((ctx->status & NATIVE_Z)) {
        goto Ldf01;
    }
    native_cmp(ctx, ctx->a, 0xc3u);
    if ((ctx->status & NATIVE_Z)) {
        goto Ldf01;
    }
    ctx->status &= (uint8_t)~NATIVE_C;
    return;
Ldf01: ;
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, 0xfeu, ctx->a);
    return;
    return;
}

void smbneo_native_fn_col_bg_proc_get_tile_page_df06(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xdf06u);
    ctx->y = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0xc0u));
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_rol(ctx, ctx->a);
    ctx->a = native_rol(ctx, ctx->a);
    ctx->x = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, ctx->y);
    return;
    return;
}

void smbneo_native_fn_col_actor_ground_proc_df17(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xdf17u);
    goto Ldf17;
Ldf0e: ;
    return;
Ldf17: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x20u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Ldf0e;
    }
    smbneo_native_fn_col_actor_pos_y_diff_e0b1(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    if (!(ctx->status & NATIVE_C)) {
        goto Ldf0e;
    }
    ctx->y = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x16u + ctx->x)));
    native_cmp(ctx, ctx->y, 0x12u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Ldf2e;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x)));
    native_cmp(ctx, ctx->a, 0x25u);
    if (!(ctx->status & NATIVE_C)) {
        goto Ldf0e;
    }
Ldf2e: ;
    native_cmp(ctx, ctx->y, 0x0eu);
    if (!(ctx->status & NATIVE_Z)) {
        goto Ldf35;
    }
    /* static tail transfer */
    smbneo_native_fn_col_actor_bounce_proc_e0b9(ctx);
    return;
Ldf35: ;
    native_cmp(ctx, ctx->y, 0x05u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Ldf3c;
    }
    /* static tail transfer */
    goto Le0db;
Ldf3c: ;
    native_cmp(ctx, ctx->y, 0x12u);
    if ((ctx->status & NATIVE_Z)) {
        goto Ldf48;
    }
    native_cmp(ctx, ctx->y, 0x2eu);
    if ((ctx->status & NATIVE_Z)) {
        goto Ldf48;
    }
    native_cmp(ctx, ctx->y, 0x07u);
    if ((ctx->status & NATIVE_C)) {
        goto Ldfbc;
    }
Ldf48: ;
    smbneo_native_fn_col_actor_block_col_e104(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    if (!(ctx->status & NATIVE_Z)) {
        goto Ldf50;
    }
Ldf4d: ;
    /* static tail transfer */
    goto Le038;
Ldf50: ;
    smbneo_native_fn_col_bg_proc_check_non_solid_e10b(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    if ((ctx->status & NATIVE_Z)) {
        goto Ldf4d;
    }
    native_cmp(ctx, ctx->a, 0x23u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Ldfbd;
    }
    ctx->y = native_nz(ctx, native_read(ctx, 0x02u));
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, (uint16_t)(native_zpword(ctx, 0x06u) + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x16u + ctx->x)));
    native_cmp(ctx, ctx->a, 0x15u);
    if ((ctx->status & NATIVE_C)) {
        smbneo_native_fn_col_actor_koopa_demote_check_df71(ctx);
        return;
    }
    native_cmp(ctx, ctx->a, 0x06u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Ldf6c;
    }
    smbneo_native_fn_col_actor_block_bump_kill_e0e4(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Ldf6c: ;
    ctx->a = native_nz(ctx, 0x01u);
    smbneo_native_fn_col_score_do_d968(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_col_actor_koopa_demote_check_df71(ctx);
    return;
Ldfbc: ;
    return;
Ldfbd: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x04u));
    ctx->status |= NATIVE_C;
    native_sbc(ctx, 0x08u);
    native_cmp(ctx, ctx->a, 0x05u);
    if ((ctx->status & NATIVE_C)) {
        goto Le038;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x40u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Le023;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x)));
    ctx->a = native_asl(ctx, ctx->a);
    if (!(ctx->status & NATIVE_C)) {
        goto Ldfd4;
    }
Ldfd1: ;
    /* static tail transfer */
    goto Le054;
Ldfd4: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x)));
    if ((ctx->status & NATIVE_Z)) {
        goto Ldfd1;
    }
    native_cmp(ctx, ctx->a, 0x05u);
    if ((ctx->status & NATIVE_Z)) {
        goto Ldffb;
    }
    native_cmp(ctx, ctx->a, 0x03u);
    if ((ctx->status & NATIVE_C)) {
        goto Ldffa;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x)));
    native_cmp(ctx, ctx->a, 0x02u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Ldffb;
    }
    ctx->a = native_nz(ctx, 0x10u);
    ctx->y = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x16u + ctx->x)));
    native_cmp(ctx, ctx->y, 0x12u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Ldff0;
    }
    ctx->a = native_nz(ctx, 0x00u);
Ldff0: ;
    native_write(ctx, (uint16_t)(0x0796u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x03u);
    native_write(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x), ctx->a);
    smbneo_native_fn_col_actor_land_do_e0a5(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Ldffa: ;
    return;
Ldffb: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x16u + ctx->x)));
    native_cmp(ctx, ctx->a, 0x06u);
    if ((ctx->status & NATIVE_Z)) {
        goto Le023;
    }
    native_cmp(ctx, ctx->a, 0x12u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Le013;
    }
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, (uint16_t)(uint8_t)(0x46u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x08u);
    native_write(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x09u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x07u));
    if ((ctx->status & NATIVE_Z)) {
        goto Le023;
    }
Le013: ;
    ctx->y = native_nz(ctx, 0x01u);
    smbneo_native_fn_col_player_bullet_diff_e099(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    if (!(ctx->status & NATIVE_N)) {
        goto Le01b;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
Le01b: ;
    ctx->a = native_nz(ctx, ctx->y);
    native_cmp(ctx, ctx->a, native_read(ctx, (uint16_t)(uint8_t)(0x46u + ctx->x)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Le023;
    }
    smbneo_native_fn_col_actor_check_side_bump_e07a(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Le023: ;
    smbneo_native_fn_col_actor_land_do_e0a5(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x80u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Le031;
    }
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x), ctx->a);
    return;
Le031: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0xbfu));
    native_write(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x), ctx->a);
    return;
Le038: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x16u + ctx->x)));
    native_cmp(ctx, ctx->a, 0x03u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Le042;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x)));
    if ((ctx->status & NATIVE_Z)) {
        smbneo_native_fn_col_actor_check_side_bump_e07a(ctx);
        return;
    }
Le042: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x)));
    ctx->y = native_nz(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    if (!(ctx->status & NATIVE_C)) {
        goto Le04f;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | 0x40u));
    /* static tail transfer */
    goto Le052;
Le04f: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xdf0fu + ctx->y)));
Le052: ;
    native_write(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x), ctx->a);
Le054: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x)));
    native_cmp(ctx, ctx->a, 0x20u);
    if (!(ctx->status & NATIVE_C)) {
        goto Le079;
    }
    ctx->y = native_nz(ctx, 0x16u);
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, 0xebu, ctx->a);
Le060: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0xebu));
    native_cmp(ctx, ctx->a, native_read(ctx, (uint16_t)(uint8_t)(0x46u + ctx->x)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Le072;
    }
    ctx->a = native_nz(ctx, 0x01u);
    smbneo_native_fn_col_box_buffer_check_actor_e2de(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    if ((ctx->status & NATIVE_Z)) {
        goto Le072;
    }
    smbneo_native_fn_col_bg_proc_check_non_solid_e10b(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    if (!(ctx->status & NATIVE_Z)) {
        smbneo_native_fn_col_actor_check_side_bump_e07a(ctx);
        return;
    }
Le072: ;
    native_write(ctx, 0xebu, native_nz(ctx, (uint8_t)(native_read(ctx, 0xebu) - 1u)));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    native_cmp(ctx, ctx->y, 0x18u);
    if (!(ctx->status & NATIVE_C)) {
        goto Le060;
    }
Le079: ;
    return;
Le0db: ;
    smbneo_native_fn_col_actor_block_col_e104(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    if ((ctx->status & NATIVE_Z)) {
        goto Le0fd;
    }
    native_cmp(ctx, ctx->a, 0x23u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Le0ec;
    }
    smbneo_native_fn_col_actor_block_bump_kill_e0e4(ctx);
    return;
Le0ec: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x078au + ctx->x)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Le0fd;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x88u));
    native_write(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x), ctx->a);
    smbneo_native_fn_col_actor_land_do_e0a5(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    /* static tail transfer */
    goto Le054;
Le0fd: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | 0x01u));
    native_write(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x), ctx->a);
    return;
    return;
}

void smbneo_native_fn_col_actor_koopa_demote_check_df71(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xdf71u);
    native_cmp(ctx, ctx->a, 0x09u);
    if (!(ctx->status & NATIVE_C)) {
        smbneo_native_fn_col_player_actor_stomp_do_df85(ctx);
        return;
    }
    native_cmp(ctx, ctx->a, 0x11u);
    if ((ctx->status & NATIVE_C)) {
        smbneo_native_fn_col_player_actor_stomp_do_df85(ctx);
        return;
    }
    native_cmp(ctx, ctx->a, 0x0au);
    if (!(ctx->status & NATIVE_C)) {
        goto Ldf81;
    }
    native_cmp(ctx, ctx->a, 0x0du);
    if (!(ctx->status & NATIVE_C)) {
        smbneo_native_fn_col_player_actor_stomp_do_df85(ctx);
        return;
    }
Ldf81: ;
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x01u));
    native_write(ctx, (uint16_t)(uint8_t)(0x16u + ctx->x), ctx->a);
    smbneo_native_fn_col_player_actor_stomp_do_df85(ctx);
    return;
    return;
}

void smbneo_native_fn_col_player_actor_stomp_do_df85(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xdf85u);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0xf0u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | 0x02u));
    native_write(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x), ctx->a);
    native_write(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x), native_nz(ctx, (uint8_t)(native_read(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x)) - 1u)));
    native_write(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x), native_nz(ctx, (uint8_t)(native_read(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x)) - 1u)));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x16u + ctx->x)));
    native_cmp(ctx, ctx->a, 0x07u);
    if ((ctx->status & NATIVE_Z)) {
        goto Ldf9e;
    }
    ctx->a = native_nz(ctx, 0xfdu);
    ctx->y = native_nz(ctx, native_read(ctx, 0x074eu));
    if (!(ctx->status & NATIVE_Z)) {
        goto Ldfa0;
    }
Ldf9e: ;
    ctx->a = native_nz(ctx, 0xffu);
Ldfa0: ;
    native_write(ctx, (uint16_t)(uint8_t)(0xa0u + ctx->x), ctx->a);
    ctx->y = native_nz(ctx, 0x01u);
    smbneo_native_fn_col_player_bullet_diff_e099(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    if (!(ctx->status & NATIVE_N)) {
        goto Ldfaa;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
Ldfaa: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x16u + ctx->x)));
    native_cmp(ctx, ctx->a, 0x33u);
    if ((ctx->status & NATIVE_Z)) {
        goto Ldfb6;
    }
    native_cmp(ctx, ctx->a, 0x08u);
    if ((ctx->status & NATIVE_Z)) {
        goto Ldfb6;
    }
    native_write(ctx, (uint16_t)(uint8_t)(0x46u + ctx->x), ctx->y);
Ldfb6: ;
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xdf15u + ctx->y)));
    native_write(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x), ctx->a);
    return;
    return;
}

void smbneo_native_fn_col_actor_check_side_bump_e07a(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xe07au);
    goto Le07a;
Lc97a: ;
    native_write(ctx, (uint16_t)(uint8_t)(0xa0u + ctx->x), ctx->y);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | 0x01u));
    native_write(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x00u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & native_read(ctx, (uint16_t)(0x07a9u + ctx->x))));
    ctx->y = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x06ccu));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lc98e;
    }
    ctx->y = native_nz(ctx, ctx->a);
Lc98e: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xc953u + ctx->y)));
    native_write(ctx, (uint16_t)(0x078au + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x07a8u + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | 0xc0u));
    native_write(ctx, (uint16_t)(uint8_t)(0x3cu + ctx->x), ctx->a);
    ctx->y = native_nz(ctx, 0xfcu);
    ctx->a = native_nz(ctx, native_read(ctx, 0x09u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x40u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lc9a5;
    }
    ctx->y = native_nz(ctx, 0x04u);
Lc9a5: ;
    native_write(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x), ctx->y);
    ctx->y = native_nz(ctx, 0x01u);
    smbneo_native_fn_col_player_bullet_diff_e099(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    if ((ctx->status & NATIVE_N)) {
        goto Lc9b8;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0796u + ctx->x)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lc9b8;
    }
    ctx->a = native_nz(ctx, 0xf8u);
    native_write(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x), ctx->a);
Lc9b8: ;
    native_write(ctx, (uint16_t)(uint8_t)(0x46u + ctx->x), ctx->y);
    smbneo_native_fn_actor_A_00_proc_c9ba(ctx);
    return;
Lda8d: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a ^ 0xffu));
    ctx->y = native_nz(ctx, ctx->a);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    native_write(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x), ctx->y);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x46u + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a ^ 0x03u));
    native_write(ctx, (uint16_t)(uint8_t)(0x46u + ctx->x), ctx->a);
    return;
Le07a: ;
    native_cmp(ctx, ctx->x, 0x05u);
    if ((ctx->status & NATIVE_Z)) {
        goto Le087;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x)));
    ctx->a = native_asl(ctx, ctx->a);
    if (!(ctx->status & NATIVE_C)) {
        goto Le087;
    }
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, 0xffu, ctx->a);
Le087: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x16u + ctx->x)));
    native_cmp(ctx, ctx->a, 0x05u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Le096;
    }
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x00u, ctx->a);
    ctx->y = native_nz(ctx, 0xfau);
    /* static tail transfer */
    goto Lc97a;
Le096: ;
    /* static tail transfer */
    goto Lda8d;
    return;
}

void smbneo_native_fn_col_player_bullet_diff_e099(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xe099u);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x87u + ctx->x)));
    ctx->status |= NATIVE_C;
    native_sbc(ctx, native_read(ctx, 0x86u));
    native_write(ctx, 0x00u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x6eu + ctx->x)));
    native_sbc(ctx, native_read(ctx, 0x6du));
    return;
    return;
}

void smbneo_native_fn_col_actor_land_do_e0a5(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xe0a5u);
    smbneo_native_fn_actor_init_veloc_y_c2a6(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0xf0u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | 0x08u));
    native_write(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x), ctx->a);
    return;
    return;
}

void smbneo_native_fn_col_actor_pos_y_diff_e0b1(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xe0b1u);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x)));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x3eu);
    native_cmp(ctx, ctx->a, 0x44u);
    return;
    return;
}

void smbneo_native_fn_col_actor_bounce_proc_e0b9(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xe0b9u);
    goto Le0b9;
Le054: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x)));
    native_cmp(ctx, ctx->a, 0x20u);
    if (!(ctx->status & NATIVE_C)) {
        goto Le079;
    }
    ctx->y = native_nz(ctx, 0x16u);
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, 0xebu, ctx->a);
Le060: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0xebu));
    native_cmp(ctx, ctx->a, native_read(ctx, (uint16_t)(uint8_t)(0x46u + ctx->x)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Le072;
    }
    ctx->a = native_nz(ctx, 0x01u);
    smbneo_native_fn_col_box_buffer_check_actor_e2de(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    if ((ctx->status & NATIVE_Z)) {
        goto Le072;
    }
    smbneo_native_fn_col_bg_proc_check_non_solid_e10b(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    if (!(ctx->status & NATIVE_Z)) {
        smbneo_native_fn_col_actor_check_side_bump_e07a(ctx);
        return;
    }
Le072: ;
    native_write(ctx, 0xebu, native_nz(ctx, (uint8_t)(native_read(ctx, 0xebu) - 1u)));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    native_cmp(ctx, ctx->y, 0x18u);
    if (!(ctx->status & NATIVE_C)) {
        goto Le060;
    }
Le079: ;
    return;
Le0b9: ;
    smbneo_native_fn_col_actor_pos_y_diff_e0b1(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    if (!(ctx->status & NATIVE_C)) {
        goto Le0d8;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xa0u + ctx->x)));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x02u);
    native_cmp(ctx, ctx->a, 0x03u);
    if (!(ctx->status & NATIVE_C)) {
        goto Le0d8;
    }
    smbneo_native_fn_col_actor_block_col_e104(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    if ((ctx->status & NATIVE_Z)) {
        goto Le0d8;
    }
    smbneo_native_fn_col_bg_proc_check_non_solid_e10b(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    if ((ctx->status & NATIVE_Z)) {
        goto Le0d8;
    }
    smbneo_native_fn_col_actor_land_do_e0a5(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, 0xfdu);
    native_write(ctx, (uint16_t)(uint8_t)(0xa0u + ctx->x), ctx->a);
Le0d8: ;
    /* static tail transfer */
    goto Le054;
    return;
}

void smbneo_native_fn_col_actor_block_bump_kill_e0e4(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xe0e4u);
    smbneo_native_fn_col_actor_kill_d6ec(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, 0xfcu);
    native_write(ctx, (uint16_t)(uint8_t)(0xa0u + ctx->x), ctx->a);
    return;
    return;
}

void smbneo_native_fn_col_actor_block_col_e104(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xe104u);
    ctx->a = native_nz(ctx, 0x00u);
    ctx->y = native_nz(ctx, 0x15u);
    /* static tail transfer */
    smbneo_native_fn_col_box_buffer_check_actor_e2de(ctx);
    return;
    return;
}

void smbneo_native_fn_col_bg_proc_check_non_solid_e10b(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xe10bu);
    native_cmp(ctx, ctx->a, 0x26u);
    if ((ctx->status & NATIVE_Z)) {
        goto Le11d;
    }
    native_cmp(ctx, ctx->a, 0xc2u);
    if ((ctx->status & NATIVE_Z)) {
        goto Le11d;
    }
    native_cmp(ctx, ctx->a, 0xc3u);
    if ((ctx->status & NATIVE_Z)) {
        goto Le11d;
    }
    native_cmp(ctx, ctx->a, 0x5fu);
    if ((ctx->status & NATIVE_Z)) {
        goto Le11d;
    }
    native_cmp(ctx, ctx->a, 0x60u);
Le11d: ;
    return;
    return;
}

void smbneo_native_fn_col_proj_proc_e11e(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xe11eu);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xd5u + ctx->x)));
    native_cmp(ctx, ctx->a, 0x18u);
    if (!(ctx->status & NATIVE_C)) {
        goto Le145;
    }
    smbneo_native_fn_col_box_buffer_check_proj_e2f2(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    if ((ctx->status & NATIVE_Z)) {
        goto Le145;
    }
    smbneo_native_fn_col_bg_proc_check_non_solid_e10b(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    if ((ctx->status & NATIVE_Z)) {
        goto Le145;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xa6u + ctx->x)));
    if ((ctx->status & NATIVE_N)) {
        goto Le14a;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x3au + ctx->x)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Le14a;
    }
    ctx->a = native_nz(ctx, 0xfdu);
    native_write(ctx, (uint16_t)(uint8_t)(0xa6u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, (uint16_t)(uint8_t)(0x3au + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xd5u + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0xf8u));
    native_write(ctx, (uint16_t)(uint8_t)(0xd5u + ctx->x), ctx->a);
    return;
Le145: ;
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, (uint16_t)(uint8_t)(0x3au + ctx->x), ctx->a);
    return;
Le14a: ;
    ctx->a = native_nz(ctx, 0x80u);
    native_write(ctx, (uint16_t)(uint8_t)(0x24u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, 0xffu, ctx->a);
    return;
    return;
}

void smbneo_native_fn_col_proj_box_e183(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xe183u);
    ctx->a = native_nz(ctx, ctx->x);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x07u);
    ctx->x = native_nz(ctx, ctx->a);
    ctx->y = native_nz(ctx, 0x02u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Le193;
    }
    smbneo_native_fn_col_misc_box_e18c(ctx);
    return;
Le193: ;
    smbneo_native_fn_col_box_proc_e1f2(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    /* static tail transfer */
    goto Le234;
Le234: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x071cu));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x80u);
    native_write(ctx, 0x02u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x071au));
    native_adc(ctx, 0x00u);
    native_write(ctx, 0x01u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x86u + ctx->x)));
    native_cmp(ctx, ctx->a, native_read(ctx, 0x02u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x6du + ctx->x)));
    native_sbc(ctx, native_read(ctx, 0x01u));
    if (!(ctx->status & NATIVE_C)) {
        goto Le262;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x04aeu + ctx->y)));
    if ((ctx->status & NATIVE_N)) {
        goto Le25f;
    }
    ctx->a = native_nz(ctx, 0xffu);
    ctx->x = native_nz(ctx, native_read(ctx, (uint16_t)(0x04acu + ctx->y)));
    if ((ctx->status & NATIVE_N)) {
        goto Le25c;
    }
    native_write(ctx, (uint16_t)(0x04acu + ctx->y), ctx->a);
Le25c: ;
    native_write(ctx, (uint16_t)(0x04aeu + ctx->y), ctx->a);
Le25f: ;
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    return;
Le262: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x04acu + ctx->y)));
    if (!(ctx->status & NATIVE_N)) {
        goto Le278;
    }
    native_cmp(ctx, ctx->a, 0xa0u);
    if (!(ctx->status & NATIVE_C)) {
        goto Le278;
    }
    ctx->a = native_nz(ctx, 0x00u);
    ctx->x = native_nz(ctx, native_read(ctx, (uint16_t)(0x04aeu + ctx->y)));
    if (!(ctx->status & NATIVE_N)) {
        goto Le275;
    }
    native_write(ctx, (uint16_t)(0x04aeu + ctx->y), ctx->a);
Le275: ;
    native_write(ctx, (uint16_t)(0x04acu + ctx->y), ctx->a);
Le278: ;
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    return;
    return;
}

void smbneo_native_fn_col_misc_box_e18c(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xe18cu);
    ctx->a = native_nz(ctx, ctx->x);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x09u);
    ctx->x = native_nz(ctx, ctx->a);
    ctx->y = native_nz(ctx, 0x06u);
    smbneo_native_fn_col_box_proc_e1f2(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    /* static tail transfer */
    goto Le234;
Le234: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x071cu));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x80u);
    native_write(ctx, 0x02u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x071au));
    native_adc(ctx, 0x00u);
    native_write(ctx, 0x01u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x86u + ctx->x)));
    native_cmp(ctx, ctx->a, native_read(ctx, 0x02u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x6du + ctx->x)));
    native_sbc(ctx, native_read(ctx, 0x01u));
    if (!(ctx->status & NATIVE_C)) {
        goto Le262;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x04aeu + ctx->y)));
    if ((ctx->status & NATIVE_N)) {
        goto Le25f;
    }
    ctx->a = native_nz(ctx, 0xffu);
    ctx->x = native_nz(ctx, native_read(ctx, (uint16_t)(0x04acu + ctx->y)));
    if ((ctx->status & NATIVE_N)) {
        goto Le25c;
    }
    native_write(ctx, (uint16_t)(0x04acu + ctx->y), ctx->a);
Le25c: ;
    native_write(ctx, (uint16_t)(0x04aeu + ctx->y), ctx->a);
Le25f: ;
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    return;
Le262: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x04acu + ctx->y)));
    if (!(ctx->status & NATIVE_N)) {
        goto Le278;
    }
    native_cmp(ctx, ctx->a, 0xa0u);
    if (!(ctx->status & NATIVE_C)) {
        goto Le278;
    }
    ctx->a = native_nz(ctx, 0x00u);
    ctx->x = native_nz(ctx, native_read(ctx, (uint16_t)(0x04aeu + ctx->y)));
    if (!(ctx->status & NATIVE_N)) {
        goto Le275;
    }
    native_write(ctx, (uint16_t)(0x04aeu + ctx->y), ctx->a);
Le275: ;
    native_write(ctx, (uint16_t)(0x04acu + ctx->y), ctx->a);
Le278: ;
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    return;
    return;
}

void smbneo_native_fn_col_actor_box_e199(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xe199u);
    ctx->y = native_nz(ctx, 0x48u);
    native_write(ctx, 0x00u, ctx->y);
    ctx->y = native_nz(ctx, 0x44u);
    /* static tail transfer */
    goto Le1a8;
Le1a8: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x87u + ctx->x)));
    ctx->status |= NATIVE_C;
    native_sbc(ctx, native_read(ctx, 0x071cu));
    native_write(ctx, 0x01u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x6eu + ctx->x)));
    native_sbc(ctx, native_read(ctx, 0x071au));
    if ((ctx->status & NATIVE_N)) {
        goto Le1bd;
    }
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | native_read(ctx, 0x01u)));
    if ((ctx->status & NATIVE_Z)) {
        goto Le1bd;
    }
    ctx->y = native_nz(ctx, native_read(ctx, 0x00u));
Le1bd: ;
    ctx->a = native_nz(ctx, ctx->y);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & native_read(ctx, 0x03d1u)));
    native_write(ctx, (uint16_t)(0x03d8u + ctx->x), ctx->a);
    if (!(ctx->status & NATIVE_Z)) {
        goto Le1df;
    }
    /* static tail transfer */
    goto Le1d2;
Le1d2: ;
    ctx->a = native_nz(ctx, ctx->x);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x01u);
    ctx->x = native_nz(ctx, ctx->a);
    ctx->y = native_nz(ctx, 0x01u);
    smbneo_native_fn_col_box_proc_e1f2(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    /* static tail transfer */
    goto Le234;
Le1df: ;
    ctx->a = native_nz(ctx, ctx->x);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->y = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, 0xffu);
    native_write(ctx, (uint16_t)(0x04b0u + ctx->y), ctx->a);
    native_write(ctx, (uint16_t)(0x04b1u + ctx->y), ctx->a);
    native_write(ctx, (uint16_t)(0x04b2u + ctx->y), ctx->a);
    native_write(ctx, (uint16_t)(0x04b3u + ctx->y), ctx->a);
    return;
Le234: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x071cu));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x80u);
    native_write(ctx, 0x02u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x071au));
    native_adc(ctx, 0x00u);
    native_write(ctx, 0x01u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x86u + ctx->x)));
    native_cmp(ctx, ctx->a, native_read(ctx, 0x02u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x6du + ctx->x)));
    native_sbc(ctx, native_read(ctx, 0x01u));
    if (!(ctx->status & NATIVE_C)) {
        goto Le262;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x04aeu + ctx->y)));
    if ((ctx->status & NATIVE_N)) {
        goto Le25f;
    }
    ctx->a = native_nz(ctx, 0xffu);
    ctx->x = native_nz(ctx, native_read(ctx, (uint16_t)(0x04acu + ctx->y)));
    if ((ctx->status & NATIVE_N)) {
        goto Le25c;
    }
    native_write(ctx, (uint16_t)(0x04acu + ctx->y), ctx->a);
Le25c: ;
    native_write(ctx, (uint16_t)(0x04aeu + ctx->y), ctx->a);
Le25f: ;
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    return;
Le262: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x04acu + ctx->y)));
    if (!(ctx->status & NATIVE_N)) {
        goto Le278;
    }
    native_cmp(ctx, ctx->a, 0xa0u);
    if (!(ctx->status & NATIVE_C)) {
        goto Le278;
    }
    ctx->a = native_nz(ctx, 0x00u);
    ctx->x = native_nz(ctx, native_read(ctx, (uint16_t)(0x04aeu + ctx->y)));
    if (!(ctx->status & NATIVE_N)) {
        goto Le275;
    }
    native_write(ctx, (uint16_t)(0x04aeu + ctx->y), ctx->a);
Le275: ;
    native_write(ctx, (uint16_t)(0x04acu + ctx->y), ctx->a);
Le278: ;
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    return;
    return;
}

void smbneo_native_fn_col_plat_s_box_e1a2(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xe1a2u);
    ctx->y = native_nz(ctx, 0x08u);
    native_write(ctx, 0x00u, ctx->y);
    ctx->y = native_nz(ctx, 0x04u);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x87u + ctx->x)));
    ctx->status |= NATIVE_C;
    native_sbc(ctx, native_read(ctx, 0x071cu));
    native_write(ctx, 0x01u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x6eu + ctx->x)));
    native_sbc(ctx, native_read(ctx, 0x071au));
    if ((ctx->status & NATIVE_N)) {
        goto Le1bd;
    }
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | native_read(ctx, 0x01u)));
    if ((ctx->status & NATIVE_Z)) {
        goto Le1bd;
    }
    ctx->y = native_nz(ctx, native_read(ctx, 0x00u));
Le1bd: ;
    ctx->a = native_nz(ctx, ctx->y);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & native_read(ctx, 0x03d1u)));
    native_write(ctx, (uint16_t)(0x03d8u + ctx->x), ctx->a);
    if (!(ctx->status & NATIVE_Z)) {
        goto Le1df;
    }
    /* static tail transfer */
    goto Le1d2;
Le1d2: ;
    ctx->a = native_nz(ctx, ctx->x);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x01u);
    ctx->x = native_nz(ctx, ctx->a);
    ctx->y = native_nz(ctx, 0x01u);
    smbneo_native_fn_col_box_proc_e1f2(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    /* static tail transfer */
    goto Le234;
Le1df: ;
    ctx->a = native_nz(ctx, ctx->x);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->y = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, 0xffu);
    native_write(ctx, (uint16_t)(0x04b0u + ctx->y), ctx->a);
    native_write(ctx, (uint16_t)(0x04b1u + ctx->y), ctx->a);
    native_write(ctx, (uint16_t)(0x04b2u + ctx->y), ctx->a);
    native_write(ctx, (uint16_t)(0x04b3u + ctx->y), ctx->a);
    return;
Le234: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x071cu));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x80u);
    native_write(ctx, 0x02u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x071au));
    native_adc(ctx, 0x00u);
    native_write(ctx, 0x01u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x86u + ctx->x)));
    native_cmp(ctx, ctx->a, native_read(ctx, 0x02u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x6du + ctx->x)));
    native_sbc(ctx, native_read(ctx, 0x01u));
    if (!(ctx->status & NATIVE_C)) {
        goto Le262;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x04aeu + ctx->y)));
    if ((ctx->status & NATIVE_N)) {
        goto Le25f;
    }
    ctx->a = native_nz(ctx, 0xffu);
    ctx->x = native_nz(ctx, native_read(ctx, (uint16_t)(0x04acu + ctx->y)));
    if ((ctx->status & NATIVE_N)) {
        goto Le25c;
    }
    native_write(ctx, (uint16_t)(0x04acu + ctx->y), ctx->a);
Le25c: ;
    native_write(ctx, (uint16_t)(0x04aeu + ctx->y), ctx->a);
Le25f: ;
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    return;
Le262: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x04acu + ctx->y)));
    if (!(ctx->status & NATIVE_N)) {
        goto Le278;
    }
    native_cmp(ctx, ctx->a, 0xa0u);
    if (!(ctx->status & NATIVE_C)) {
        goto Le278;
    }
    ctx->a = native_nz(ctx, 0x00u);
    ctx->x = native_nz(ctx, native_read(ctx, (uint16_t)(0x04aeu + ctx->y)));
    if (!(ctx->status & NATIVE_N)) {
        goto Le275;
    }
    native_write(ctx, (uint16_t)(0x04aeu + ctx->y), ctx->a);
Le275: ;
    native_write(ctx, (uint16_t)(0x04acu + ctx->y), ctx->a);
Le278: ;
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    return;
    return;
}

void smbneo_native_fn_col_plat_l_box_e1c9(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xe1c9u);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    smbneo_native_fn_pos_bits_get_do_x_f15b(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x - 1u));
    native_cmp(ctx, ctx->a, 0xfeu);
    if ((ctx->status & NATIVE_C)) {
        goto Le1df;
    }
    ctx->a = native_nz(ctx, ctx->x);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x01u);
    ctx->x = native_nz(ctx, ctx->a);
    ctx->y = native_nz(ctx, 0x01u);
    smbneo_native_fn_col_box_proc_e1f2(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    /* static tail transfer */
    goto Le234;
Le1df: ;
    ctx->a = native_nz(ctx, ctx->x);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->y = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, 0xffu);
    native_write(ctx, (uint16_t)(0x04b0u + ctx->y), ctx->a);
    native_write(ctx, (uint16_t)(0x04b1u + ctx->y), ctx->a);
    native_write(ctx, (uint16_t)(0x04b2u + ctx->y), ctx->a);
    native_write(ctx, (uint16_t)(0x04b3u + ctx->y), ctx->a);
    return;
Le234: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x071cu));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x80u);
    native_write(ctx, 0x02u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x071au));
    native_adc(ctx, 0x00u);
    native_write(ctx, 0x01u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x86u + ctx->x)));
    native_cmp(ctx, ctx->a, native_read(ctx, 0x02u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x6du + ctx->x)));
    native_sbc(ctx, native_read(ctx, 0x01u));
    if (!(ctx->status & NATIVE_C)) {
        goto Le262;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x04aeu + ctx->y)));
    if ((ctx->status & NATIVE_N)) {
        goto Le25f;
    }
    ctx->a = native_nz(ctx, 0xffu);
    ctx->x = native_nz(ctx, native_read(ctx, (uint16_t)(0x04acu + ctx->y)));
    if ((ctx->status & NATIVE_N)) {
        goto Le25c;
    }
    native_write(ctx, (uint16_t)(0x04acu + ctx->y), ctx->a);
Le25c: ;
    native_write(ctx, (uint16_t)(0x04aeu + ctx->y), ctx->a);
Le25f: ;
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    return;
Le262: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x04acu + ctx->y)));
    if (!(ctx->status & NATIVE_N)) {
        goto Le278;
    }
    native_cmp(ctx, ctx->a, 0xa0u);
    if (!(ctx->status & NATIVE_C)) {
        goto Le278;
    }
    ctx->a = native_nz(ctx, 0x00u);
    ctx->x = native_nz(ctx, native_read(ctx, (uint16_t)(0x04aeu + ctx->y)));
    if (!(ctx->status & NATIVE_N)) {
        goto Le275;
    }
    native_write(ctx, (uint16_t)(0x04aeu + ctx->y), ctx->a);
Le275: ;
    native_write(ctx, (uint16_t)(0x04acu + ctx->y), ctx->a);
Le278: ;
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    return;
    return;
}

void smbneo_native_fn_col_box_proc_e1f2(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xe1f2u);
#ifdef SMBNEO_NATIVE_FAST_PATHS
    if (vs_native_fast_col_box_proc(ctx)) return;
#endif
    uint8_t saved_value_0 = 0u;
    native_write(ctx, 0x00u, ctx->x);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x03b8u + ctx->y)));
    native_write(ctx, 0x02u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x03adu + ctx->y)));
    native_write(ctx, 0x01u, ctx->a);
    ctx->a = native_nz(ctx, ctx->x);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    saved_value_0 = ctx->a;
    ctx->y = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0499u + ctx->x)));
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->x = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x01u));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, (uint16_t)(0xe153u + ctx->x)));
    native_write(ctx, (uint16_t)(0x04acu + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x01u));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, (uint16_t)(0xe155u + ctx->x)));
    native_write(ctx, (uint16_t)(0x04aeu + ctx->y), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->a = native_nz(ctx, native_read(ctx, 0x02u));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, (uint16_t)(0xe153u + ctx->x)));
    native_write(ctx, (uint16_t)(0x04acu + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x02u));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, (uint16_t)(0xe155u + ctx->x)));
    native_write(ctx, (uint16_t)(0x04aeu + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, saved_value_0);
    ctx->y = native_nz(ctx, ctx->a);
    ctx->x = native_nz(ctx, native_read(ctx, 0x00u));
    return;
    return;
}

void smbneo_native_fn_col_base_player_e27b(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xe27bu);
    ctx->x = native_nz(ctx, 0x00u);
    smbneo_native_fn_col_base_actor_e27d(ctx);
    return;
    return;
}

void smbneo_native_fn_col_base_actor_e27d(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xe27du);
    native_write(ctx, 0x06u, ctx->y);
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, 0x07u, ctx->a);
Le283: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x04acu + ctx->y)));
    native_cmp(ctx, ctx->a, native_read(ctx, (uint16_t)(0x04acu + ctx->x)));
    if ((ctx->status & NATIVE_C)) {
        goto Le2b5;
    }
    native_cmp(ctx, ctx->a, native_read(ctx, (uint16_t)(0x04aeu + ctx->x)));
    if (!(ctx->status & NATIVE_C)) {
        goto Le2a2;
    }
    if ((ctx->status & NATIVE_Z)) {
        goto Le2d4;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x04aeu + ctx->y)));
    native_cmp(ctx, ctx->a, native_read(ctx, (uint16_t)(0x04acu + ctx->y)));
    if (!(ctx->status & NATIVE_C)) {
        goto Le2d4;
    }
    native_cmp(ctx, ctx->a, native_read(ctx, (uint16_t)(0x04acu + ctx->x)));
    if ((ctx->status & NATIVE_C)) {
        goto Le2d4;
    }
    ctx->y = native_nz(ctx, native_read(ctx, 0x06u));
    return;
Le2a2: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x04aeu + ctx->x)));
    native_cmp(ctx, ctx->a, native_read(ctx, (uint16_t)(0x04acu + ctx->x)));
    if (!(ctx->status & NATIVE_C)) {
        goto Le2d4;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x04aeu + ctx->y)));
    native_cmp(ctx, ctx->a, native_read(ctx, (uint16_t)(0x04acu + ctx->x)));
    if ((ctx->status & NATIVE_C)) {
        goto Le2d4;
    }
    ctx->y = native_nz(ctx, native_read(ctx, 0x06u));
    return;
Le2b5: ;
    native_cmp(ctx, ctx->a, native_read(ctx, (uint16_t)(0x04acu + ctx->x)));
    if ((ctx->status & NATIVE_Z)) {
        goto Le2d4;
    }
    native_cmp(ctx, ctx->a, native_read(ctx, (uint16_t)(0x04aeu + ctx->x)));
    if (!(ctx->status & NATIVE_C)) {
        goto Le2d4;
    }
    if ((ctx->status & NATIVE_Z)) {
        goto Le2d4;
    }
    native_cmp(ctx, ctx->a, native_read(ctx, (uint16_t)(0x04aeu + ctx->y)));
    if (!(ctx->status & NATIVE_C)) {
        goto Le2d0;
    }
    if ((ctx->status & NATIVE_Z)) {
        goto Le2d0;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x04aeu + ctx->y)));
    native_cmp(ctx, ctx->a, native_read(ctx, (uint16_t)(0x04acu + ctx->x)));
    if ((ctx->status & NATIVE_C)) {
        goto Le2d4;
    }
Le2d0: ;
    ctx->status &= (uint8_t)~NATIVE_C;
    ctx->y = native_nz(ctx, native_read(ctx, 0x06u));
    return;
Le2d4: ;
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    native_write(ctx, 0x07u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x07u) - 1u)));
    if (!(ctx->status & NATIVE_N)) {
        goto Le283;
    }
    ctx->status |= NATIVE_C;
    ctx->y = native_nz(ctx, native_read(ctx, 0x06u));
    return;
    return;
}

void smbneo_native_fn_col_box_buffer_check_actor_e2de(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xe2deu);
    uint8_t saved_value_0 = 0u;
    saved_value_0 = ctx->a;
    ctx->a = native_nz(ctx, ctx->x);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x01u);
    ctx->x = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, saved_value_0);
    /* static tail transfer */
    goto Le2fb;
Le2fb: ;
    smbneo_native_fn_col_box_buffer_e348(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    native_cmp(ctx, ctx->a, 0x00u);
    return;
    return;
}

void smbneo_native_fn_col_box_buffer_check_misc_e2e8(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xe2e8u);
    ctx->a = native_nz(ctx, ctx->x);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x0du);
    ctx->x = native_nz(ctx, ctx->a);
    ctx->y = native_nz(ctx, 0x1bu);
    /* static tail transfer */
    goto Le2f9;
Le2f9: ;
    ctx->a = native_nz(ctx, 0x00u);
    smbneo_native_fn_col_box_buffer_e348(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    native_cmp(ctx, ctx->a, 0x00u);
    return;
    return;
}

void smbneo_native_fn_col_box_buffer_check_proj_e2f2(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xe2f2u);
    ctx->y = native_nz(ctx, 0x1au);
    ctx->a = native_nz(ctx, ctx->x);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x07u);
    ctx->x = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, 0x00u);
    smbneo_native_fn_col_box_buffer_e348(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    native_cmp(ctx, ctx->a, 0x00u);
    return;
    return;
}

void smbneo_native_fn_col_box_player_step_e33e(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xe33eu);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    smbneo_native_fn_col_box_player_jump_e33f(ctx);
    return;
    return;
}

void smbneo_native_fn_col_box_player_jump_e33f(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xe33fu);
    ctx->a = native_nz(ctx, 0x00u);
    /* static tail transfer */
    goto Le346;
Le346: ;
    ctx->x = native_nz(ctx, 0x00u);
    smbneo_native_fn_col_box_buffer_e348(ctx);
    return;
    return;
}

void smbneo_native_fn_col_box_player_back_e344(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xe344u);
    ctx->a = native_nz(ctx, 0x01u);
    ctx->x = native_nz(ctx, 0x00u);
    smbneo_native_fn_col_box_buffer_e348(ctx);
    return;
    return;
}

void smbneo_native_fn_col_box_buffer_e348(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xe348u);
#ifdef SMBNEO_NATIVE_FAST_PATHS
    if (vs_native_fast_col_box_buffer(ctx)) return;
#endif
    uint8_t saved_value_0 = 0u;
    saved_value_0 = ctx->a;
    native_write(ctx, 0x04u, ctx->y);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xe306u + ctx->y)));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x86u + ctx->x)));
    native_write(ctx, 0x05u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x6du + ctx->x)));
    native_adc(ctx, 0x00u);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x01u));
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | native_read(ctx, 0x05u)));
    ctx->a = native_ror(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    smbneo_native_fn_scenery_get_buffer_ptr_ab14(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->y = native_nz(ctx, native_read(ctx, 0x04u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xceu + ctx->x)));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, (uint16_t)(0xe322u + ctx->y)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0xf0u));
    ctx->status |= NATIVE_C;
    native_sbc(ctx, 0x20u);
    native_write(ctx, 0x02u, ctx->a);
    ctx->y = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0x06u) + ctx->y)));
    native_write(ctx, 0x03u, ctx->a);
    ctx->y = native_nz(ctx, native_read(ctx, 0x04u));
    ctx->a = native_nz(ctx, saved_value_0);
    if (!(ctx->status & NATIVE_Z)) {
        goto Le381;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xceu + ctx->x)));
    /* static tail transfer */
    goto Le383;
Le381: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x86u + ctx->x)));
Le383: ;
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x0fu));
    native_write(ctx, 0x04u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x03u));
    return;
    return;
}

void smbneo_native_fn_render_vine_e392(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xe392u);
    native_write(ctx, 0x00u, ctx->y);
    ctx->a = native_nz(ctx, native_read(ctx, 0x03b9u));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, (uint16_t)(0xe390u + ctx->y)));
    ctx->x = native_nz(ctx, native_read(ctx, (uint16_t)(0x039au + ctx->y)));
    ctx->y = native_nz(ctx, native_read(ctx, (uint16_t)(0x06e5u + ctx->x)));
    native_write(ctx, 0x02u, ctx->y);
    smbneo_native_fn_render_vine_column_e40b(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, 0x03aeu));
    native_write(ctx, (uint16_t)(0x0203u + ctx->y), ctx->a);
    native_write(ctx, (uint16_t)(0x020bu + ctx->y), ctx->a);
    native_write(ctx, (uint16_t)(0x0213u + ctx->y), ctx->a);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x06u);
    native_write(ctx, (uint16_t)(0x0207u + ctx->y), ctx->a);
    native_write(ctx, (uint16_t)(0x020fu + ctx->y), ctx->a);
    native_write(ctx, (uint16_t)(0x0217u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, 0x21u);
    native_write(ctx, (uint16_t)(0x0202u + ctx->y), ctx->a);
    native_write(ctx, (uint16_t)(0x020au + ctx->y), ctx->a);
    native_write(ctx, (uint16_t)(0x0212u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | 0x40u));
    native_write(ctx, (uint16_t)(0x0206u + ctx->y), ctx->a);
    native_write(ctx, (uint16_t)(0x020eu + ctx->y), ctx->a);
    native_write(ctx, (uint16_t)(0x0216u + ctx->y), ctx->a);
    ctx->x = native_nz(ctx, 0x05u);
Le3d6: ;
    ctx->a = native_nz(ctx, 0xe1u);
    native_write(ctx, (uint16_t)(0x0201u + ctx->y), ctx->a);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x - 1u));
    if (!(ctx->status & NATIVE_N)) {
        goto Le3d6;
    }
    ctx->y = native_nz(ctx, native_read(ctx, 0x02u));
    ctx->a = native_nz(ctx, native_read(ctx, 0x00u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Le3ed;
    }
    ctx->a = native_nz(ctx, 0xe0u);
    native_write(ctx, (uint16_t)(0x0201u + ctx->y), ctx->a);
Le3ed: ;
    ctx->x = native_nz(ctx, 0x00u);
Le3ef: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x039du));
    ctx->status |= NATIVE_C;
    native_sbc(ctx, native_read(ctx, (uint16_t)(0x0200u + ctx->y)));
    native_cmp(ctx, ctx->a, 0x64u);
    if (!(ctx->status & NATIVE_C)) {
        goto Le3ff;
    }
    ctx->a = native_nz(ctx, 0xf8u);
    native_write(ctx, (uint16_t)(0x0200u + ctx->y), ctx->a);
Le3ff: ;
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    native_cmp(ctx, ctx->x, 0x06u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Le3ef;
    }
    ctx->y = native_nz(ctx, native_read(ctx, 0x00u));
    return;
    return;
}

void smbneo_native_fn_render_vine_column_e40b(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xe40bu);
    ctx->x = native_nz(ctx, 0x06u);
Le40d: ;
    native_write(ctx, (uint16_t)(0x0200u + ctx->y), ctx->a);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x08u);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x - 1u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Le40d;
    }
    ctx->y = native_nz(ctx, native_read(ctx, 0x02u));
    return;
    return;
}

void smbneo_native_fn_render_hammer_e439(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xe439u);
    ctx->y = native_nz(ctx, native_read(ctx, (uint16_t)(0x06f3u + ctx->x)));
    ctx->a = native_nz(ctx, native_read(ctx, 0x0747u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Le449;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x2au + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x7fu));
    native_cmp(ctx, ctx->a, 0x01u);
    if ((ctx->status & NATIVE_Z)) {
        goto Le44d;
    }
Le449: ;
    ctx->x = native_nz(ctx, 0x00u);
    if ((ctx->status & NATIVE_Z)) {
        goto Le454;
    }
Le44d: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x09u));
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x03u));
    ctx->x = native_nz(ctx, ctx->a);
Le454: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x03beu));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, (uint16_t)(0xe421u + ctx->x)));
    native_write(ctx, (uint16_t)(0x0200u + ctx->y), ctx->a);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, (uint16_t)(0xe429u + ctx->x)));
    native_write(ctx, (uint16_t)(0x0204u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x03b3u));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, (uint16_t)(0xe41du + ctx->x)));
    native_write(ctx, (uint16_t)(0x0203u + ctx->y), ctx->a);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, (uint16_t)(0xe425u + ctx->x)));
    native_write(ctx, (uint16_t)(0x0207u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xe42du + ctx->x)));
    native_write(ctx, (uint16_t)(0x0201u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xe431u + ctx->x)));
    native_write(ctx, (uint16_t)(0x0205u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xe435u + ctx->x)));
    native_write(ctx, (uint16_t)(0x0202u + ctx->y), ctx->a);
    native_write(ctx, (uint16_t)(0x0206u + ctx->y), ctx->a);
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    ctx->a = native_nz(ctx, native_read(ctx, 0x03d6u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0xfcu));
    if ((ctx->status & NATIVE_Z)) {
        goto Le49d;
    }
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, (uint16_t)(uint8_t)(0x2au + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, 0xf8u);
    smbneo_native_fn_render_set_two_sprites_v_e51e(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Le49d: ;
    return;
    return;
}

void smbneo_native_fn_render_flag_enemy_e4a8(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xe4a8u);
    ctx->y = native_nz(ctx, native_read(ctx, (uint16_t)(0x06e5u + ctx->x)));
    ctx->a = native_nz(ctx, native_read(ctx, 0x03aeu));
    native_write(ctx, (uint16_t)(0x0203u + ctx->y), ctx->a);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x08u);
    native_write(ctx, (uint16_t)(0x0207u + ctx->y), ctx->a);
    native_write(ctx, (uint16_t)(0x020bu + ctx->y), ctx->a);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x0cu);
    native_write(ctx, 0x05u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x)));
    smbneo_native_fn_render_set_two_sprites_v_e51e(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_adc(ctx, 0x08u);
    native_write(ctx, (uint16_t)(0x0208u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x010du));
    native_write(ctx, 0x02u, ctx->a);
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, 0x03u, ctx->a);
    native_write(ctx, 0x04u, ctx->a);
    native_write(ctx, (uint16_t)(0x0202u + ctx->y), ctx->a);
    native_write(ctx, (uint16_t)(0x0206u + ctx->y), ctx->a);
    native_write(ctx, (uint16_t)(0x020au + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, 0x7eu);
    native_write(ctx, (uint16_t)(0x0201u + ctx->y), ctx->a);
    native_write(ctx, (uint16_t)(0x0209u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, 0x7fu);
    native_write(ctx, (uint16_t)(0x0205u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x070fu));
    if ((ctx->status & NATIVE_Z)) {
        goto Le504;
    }
    ctx->a = native_nz(ctx, ctx->y);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x0cu);
    ctx->y = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x010fu));
    ctx->a = native_asl(ctx, ctx->a);
    ctx->x = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xe49eu + ctx->x)));
    native_write(ctx, 0x00u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xe49fu + ctx->x)));
    smbneo_native_fn_render_chr_pair_eb0f(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Le504: ;
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    ctx->y = native_nz(ctx, native_read(ctx, (uint16_t)(0x06e5u + ctx->x)));
    ctx->a = native_nz(ctx, native_read(ctx, 0x03d1u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x0eu));
    if ((ctx->status & NATIVE_Z)) {
        goto Le524;
    }
    smbneo_native_fn_render_clear_six_sprites_e510(ctx);
    return;
Le524: ;
    return;
    return;
}

void smbneo_native_fn_render_clear_six_sprites_e510(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xe510u);
    ctx->a = native_nz(ctx, 0xf8u);
    smbneo_native_fn_render_set_six_sprites_e512(ctx);
    return;
    return;
}

void smbneo_native_fn_render_set_six_sprites_e512(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xe512u);
    native_write(ctx, (uint16_t)(0x0214u + ctx->y), ctx->a);
    native_write(ctx, (uint16_t)(0x0210u + ctx->y), ctx->a);
    smbneo_native_fn_render_set_four_sprites_v_e518(ctx);
    return;
    return;
}

void smbneo_native_fn_render_set_four_sprites_v_e518(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xe518u);
    native_write(ctx, (uint16_t)(0x020cu + ctx->y), ctx->a);
    smbneo_native_fn_render_set_three_sprites_v_e51b(ctx);
    return;
    return;
}

void smbneo_native_fn_render_set_three_sprites_v_e51b(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xe51bu);
    native_write(ctx, (uint16_t)(0x0208u + ctx->y), ctx->a);
    smbneo_native_fn_render_set_two_sprites_v_e51e(ctx);
    return;
    return;
}

void smbneo_native_fn_render_set_two_sprites_v_e51e(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xe51eu);
    native_write(ctx, (uint16_t)(0x0204u + ctx->y), ctx->a);
    native_write(ctx, (uint16_t)(0x0200u + ctx->y), ctx->a);
    return;
    return;
}

void smbneo_native_fn_render_plat_large_e525(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xe525u);
    uint8_t saved_value_0 = 0u;
    ctx->y = native_nz(ctx, native_read(ctx, (uint16_t)(0x06e5u + ctx->x)));
    native_write(ctx, 0x02u, ctx->y);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->a = native_nz(ctx, native_read(ctx, 0x03aeu));
    smbneo_native_fn_render_vine_column_e40b(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x)));
    smbneo_native_fn_render_set_four_sprites_v_e518(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->y = native_nz(ctx, native_read(ctx, 0x074eu));
    native_cmp(ctx, ctx->y, 0x03u);
    if ((ctx->status & NATIVE_Z)) {
        goto Le546;
    }
    ctx->y = native_nz(ctx, native_read(ctx, 0x06ccu));
    if ((ctx->status & NATIVE_Z)) {
        goto Le548;
    }
Le546: ;
    ctx->a = native_nz(ctx, 0xf8u);
Le548: ;
    ctx->y = native_nz(ctx, native_read(ctx, (uint16_t)(0x06e5u + ctx->x)));
    native_write(ctx, (uint16_t)(0x0210u + ctx->y), ctx->a);
    native_write(ctx, (uint16_t)(0x0214u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, 0x5bu);
    ctx->x = native_nz(ctx, native_read(ctx, 0x0743u));
    if ((ctx->status & NATIVE_Z)) {
        goto Le55a;
    }
    ctx->a = native_nz(ctx, 0x75u);
Le55a: ;
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    smbneo_native_fn_render_set_six_sprites_e512(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, 0x02u);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    smbneo_native_fn_render_set_six_sprites_e512(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    smbneo_native_fn_pos_bits_get_do_x_f15b(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x - 1u));
    ctx->y = native_nz(ctx, native_read(ctx, (uint16_t)(0x06e5u + ctx->x)));
    ctx->a = native_asl(ctx, ctx->a);
    saved_value_0 = ctx->a;
    if (!(ctx->status & NATIVE_C)) {
        goto Le577;
    }
    ctx->a = native_nz(ctx, 0xf8u);
    native_write(ctx, (uint16_t)(0x0200u + ctx->y), ctx->a);
Le577: ;
    ctx->a = native_nz(ctx, saved_value_0);
    ctx->a = native_asl(ctx, ctx->a);
    saved_value_0 = ctx->a;
    if (!(ctx->status & NATIVE_C)) {
        goto Le581;
    }
    ctx->a = native_nz(ctx, 0xf8u);
    native_write(ctx, (uint16_t)(0x0204u + ctx->y), ctx->a);
Le581: ;
    ctx->a = native_nz(ctx, saved_value_0);
    ctx->a = native_asl(ctx, ctx->a);
    saved_value_0 = ctx->a;
    if (!(ctx->status & NATIVE_C)) {
        goto Le58b;
    }
    ctx->a = native_nz(ctx, 0xf8u);
    native_write(ctx, (uint16_t)(0x0208u + ctx->y), ctx->a);
Le58b: ;
    ctx->a = native_nz(ctx, saved_value_0);
    ctx->a = native_asl(ctx, ctx->a);
    saved_value_0 = ctx->a;
    if (!(ctx->status & NATIVE_C)) {
        goto Le595;
    }
    ctx->a = native_nz(ctx, 0xf8u);
    native_write(ctx, (uint16_t)(0x020cu + ctx->y), ctx->a);
Le595: ;
    ctx->a = native_nz(ctx, saved_value_0);
    ctx->a = native_asl(ctx, ctx->a);
    saved_value_0 = ctx->a;
    if (!(ctx->status & NATIVE_C)) {
        goto Le59f;
    }
    ctx->a = native_nz(ctx, 0xf8u);
    native_write(ctx, (uint16_t)(0x0210u + ctx->y), ctx->a);
Le59f: ;
    ctx->a = native_nz(ctx, saved_value_0);
    ctx->a = native_asl(ctx, ctx->a);
    if (!(ctx->status & NATIVE_C)) {
        goto Le5a8;
    }
    ctx->a = native_nz(ctx, 0xf8u);
    native_write(ctx, (uint16_t)(0x0214u + ctx->y), ctx->a);
Le5a8: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x03d1u));
    ctx->a = native_asl(ctx, ctx->a);
    if (!(ctx->status & NATIVE_C)) {
        goto Le5b1;
    }
    smbneo_native_fn_render_clear_six_sprites_e510(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Le5b1: ;
    return;
    return;
}

void smbneo_native_fn_render_coin_score_e5e3(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xe5e3u);
    goto Le5e3;
Le5b2: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x09u));
    ctx->a = native_lsr(ctx, ctx->a);
    if ((ctx->status & NATIVE_C)) {
        goto Le5b9;
    }
    native_write(ctx, (uint16_t)(uint8_t)(0xdbu + ctx->x), native_nz(ctx, (uint8_t)(native_read(ctx, (uint16_t)(uint8_t)(0xdbu + ctx->x)) - 1u)));
Le5b9: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xdbu + ctx->x)));
    smbneo_native_fn_render_set_two_sprites_v_e51e(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, 0x03b3u));
    native_write(ctx, (uint16_t)(0x0203u + ctx->y), ctx->a);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x08u);
    native_write(ctx, (uint16_t)(0x0207u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, (uint16_t)(0x0202u + ctx->y), ctx->a);
    native_write(ctx, (uint16_t)(0x0206u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, 0xf7u);
    native_write(ctx, (uint16_t)(0x0201u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, 0xfbu);
    native_write(ctx, (uint16_t)(0x0205u + ctx->y), ctx->a);
    /* static tail transfer */
    goto Le61a;
Le5e3: ;
    ctx->y = native_nz(ctx, native_read(ctx, (uint16_t)(0x06f3u + ctx->x)));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x2au + ctx->x)));
    native_cmp(ctx, ctx->a, 0x02u);
    if ((ctx->status & NATIVE_C)) {
        goto Le5b2;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xdbu + ctx->x)));
    native_write(ctx, (uint16_t)(0x0200u + ctx->y), ctx->a);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x08u);
    native_write(ctx, (uint16_t)(0x0204u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x03b3u));
    native_write(ctx, (uint16_t)(0x0203u + ctx->y), ctx->a);
    native_write(ctx, (uint16_t)(0x0207u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x09u));
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x03u));
    ctx->x = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xe5dfu + ctx->x)));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    smbneo_native_fn_render_set_two_sprites_v_e51e(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, (uint16_t)(0x0202u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, 0x82u);
    native_write(ctx, (uint16_t)(0x0206u + ctx->y), ctx->a);
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
Le61a: ;
    return;
    return;
}

void smbneo_native_fn_render_powerup_e62f(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xe62fu);
    uint8_t saved_value_0 = 0u;
    ctx->y = native_nz(ctx, native_read(ctx, 0x06eau));
    ctx->a = native_nz(ctx, native_read(ctx, 0x03b9u));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x08u);
    native_write(ctx, 0x02u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x03aeu));
    native_write(ctx, 0x05u, ctx->a);
    ctx->x = native_nz(ctx, native_read(ctx, 0x39u));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xe62bu + ctx->x)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | native_read(ctx, 0x03cau)));
    native_write(ctx, 0x04u, ctx->a);
    ctx->a = native_nz(ctx, ctx->x);
    saved_value_0 = ctx->a;
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->x = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, 0x07u, ctx->a);
    native_write(ctx, 0x03u, ctx->a);
Le654: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xe61bu + ctx->x)));
    native_write(ctx, 0x00u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xe61cu + ctx->x)));
    smbneo_native_fn_render_chr_pair_eb0f(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_write(ctx, 0x07u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x07u) - 1u)));
    if (!(ctx->status & NATIVE_N)) {
        goto Le654;
    }
    ctx->y = native_nz(ctx, native_read(ctx, 0x06eau));
    ctx->a = native_nz(ctx, saved_value_0);
    if ((ctx->status & NATIVE_Z)) {
        goto Le698;
    }
    native_cmp(ctx, ctx->a, 0x03u);
    if ((ctx->status & NATIVE_Z)) {
        goto Le698;
    }
    native_write(ctx, 0x00u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x09u));
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x03u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | native_read(ctx, 0x03cau)));
    native_write(ctx, (uint16_t)(0x0202u + ctx->y), ctx->a);
    native_write(ctx, (uint16_t)(0x0206u + ctx->y), ctx->a);
    ctx->x = native_nz(ctx, native_read(ctx, 0x00u));
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x - 1u));
    if ((ctx->status & NATIVE_Z)) {
        goto Le688;
    }
    native_write(ctx, (uint16_t)(0x020au + ctx->y), ctx->a);
    native_write(ctx, (uint16_t)(0x020eu + ctx->y), ctx->a);
Le688: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0206u + ctx->y)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | 0x40u));
    native_write(ctx, (uint16_t)(0x0206u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x020eu + ctx->y)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | 0x40u));
    native_write(ctx, (uint16_t)(0x020eu + ctx->y), ctx->a);
Le698: ;
    /* static tail transfer */
    goto Leac1;
Leac1: ;
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    ctx->a = native_nz(ctx, native_read(ctx, 0x03d1u));
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    saved_value_0 = ctx->a;
    if (!(ctx->status & NATIVE_C)) {
        goto Lead1;
    }
    ctx->a = native_nz(ctx, 0x04u);
    smbneo_native_fn_render_actor_clear_chr_pair_h_eb1e(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Lead1: ;
    ctx->a = native_nz(ctx, saved_value_0);
    ctx->a = native_lsr(ctx, ctx->a);
    saved_value_0 = ctx->a;
    if (!(ctx->status & NATIVE_C)) {
        goto Leadb;
    }
    ctx->a = native_nz(ctx, 0x00u);
    smbneo_native_fn_render_actor_clear_chr_pair_h_eb1e(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Leadb: ;
    ctx->a = native_nz(ctx, saved_value_0);
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    saved_value_0 = ctx->a;
    if (!(ctx->status & NATIVE_C)) {
        goto Leae6;
    }
    ctx->a = native_nz(ctx, 0x10u);
    smbneo_native_fn_render_actor_clear_chr_pair_v_eb14(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Leae6: ;
    ctx->a = native_nz(ctx, saved_value_0);
    ctx->a = native_lsr(ctx, ctx->a);
    saved_value_0 = ctx->a;
    if (!(ctx->status & NATIVE_C)) {
        goto Leaf0;
    }
    ctx->a = native_nz(ctx, 0x08u);
    smbneo_native_fn_render_actor_clear_chr_pair_v_eb14(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Leaf0: ;
    ctx->a = native_nz(ctx, saved_value_0);
    ctx->a = native_lsr(ctx, ctx->a);
    if (!(ctx->status & NATIVE_C)) {
        goto Leb06;
    }
    smbneo_native_fn_render_actor_clear_chr_pair_v_eb14(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x16u + ctx->x)));
    native_cmp(ctx, ctx->a, 0x0cu);
    if ((ctx->status & NATIVE_Z)) {
        goto Leb06;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xb6u + ctx->x)));
    native_cmp(ctx, ctx->a, 0x02u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Leb06;
    }
    smbneo_native_fn_actor_erase_c8db(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Leb06: ;
    return;
    return;
}

void smbneo_native_fn_render_actor_e7da(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xe7dau);
#ifdef SMBNEO_NATIVE_FAST_PATHS
    if (vs_native_fast_render_actor(ctx)) return;
#endif
    uint8_t saved_value_0 = 0u;
    uint8_t saved_value_1 = 0u;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x)));
    native_write(ctx, 0x02u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x03aeu));
    native_write(ctx, 0x05u, ctx->a);
    ctx->y = native_nz(ctx, native_read(ctx, (uint16_t)(0x06e5u + ctx->x)));
    native_write(ctx, 0xebu, ctx->y);
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x0109u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x46u + ctx->x)));
    native_write(ctx, 0x03u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x03c5u + ctx->x)));
    native_write(ctx, 0x04u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x16u + ctx->x)));
    native_cmp(ctx, ctx->a, 0x0du);
    if (!(ctx->status & NATIVE_Z)) {
        goto Le806;
    }
    ctx->y = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x58u + ctx->x)));
    if ((ctx->status & NATIVE_N)) {
        goto Le806;
    }
    ctx->y = native_nz(ctx, native_read(ctx, (uint16_t)(0x078au + ctx->x)));
    if ((ctx->status & NATIVE_Z)) {
        goto Le806;
    }
    return;
Le806: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x)));
    native_write(ctx, 0xedu, ctx->a);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x1fu));
    ctx->y = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x16u + ctx->x)));
    native_cmp(ctx, ctx->a, 0x35u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Le81b;
    }
    ctx->y = native_nz(ctx, 0x00u);
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, 0x03u, ctx->a);
    ctx->a = native_nz(ctx, 0x15u);
Le81b: ;
    native_cmp(ctx, ctx->a, 0x33u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Le832;
    }
    native_write(ctx, 0x02u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x02u) - 1u)));
    ctx->a = native_nz(ctx, 0x03u);
    ctx->y = native_nz(ctx, native_read(ctx, (uint16_t)(0x078au + ctx->x)));
    if ((ctx->status & NATIVE_Z)) {
        goto Le82a;
    }
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | 0x20u));
Le82a: ;
    native_write(ctx, 0x04u, ctx->a);
    ctx->y = native_nz(ctx, 0x00u);
    native_write(ctx, 0xedu, ctx->y);
    ctx->a = native_nz(ctx, 0x08u);
Le832: ;
    native_cmp(ctx, ctx->a, 0x32u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Le83e;
    }
    ctx->y = native_nz(ctx, 0x03u);
    ctx->x = native_nz(ctx, native_read(ctx, 0x070eu));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xe7d5u + ctx->x)));
Le83e: ;
    native_write(ctx, 0xefu, ctx->a);
    native_write(ctx, 0xecu, ctx->y);
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    native_cmp(ctx, ctx->a, 0x0cu);
    if (!(ctx->status & NATIVE_Z)) {
        goto Le84f;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xa0u + ctx->x)));
    if ((ctx->status & NATIVE_N)) {
        goto Le84f;
    }
    native_write(ctx, 0x0109u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x0109u) + 1u)));
Le84f: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x036au));
    if ((ctx->status & NATIVE_Z)) {
        goto Le85d;
    }
    ctx->y = native_nz(ctx, 0x16u);
    native_cmp(ctx, ctx->a, 0x01u);
    if ((ctx->status & NATIVE_Z)) {
        goto Le85b;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
Le85b: ;
    native_write(ctx, 0xefu, ctx->y);
Le85d: ;
    ctx->y = native_nz(ctx, native_read(ctx, 0xefu));
    native_cmp(ctx, ctx->y, 0x06u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Le880;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x1eu + ctx->x)));
    native_cmp(ctx, ctx->a, 0x02u);
    if (!(ctx->status & NATIVE_C)) {
        goto Le86d;
    }
    ctx->x = native_nz(ctx, 0x04u);
    native_write(ctx, 0xecu, ctx->x);
Le86d: ;
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x20u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | native_read(ctx, 0x0747u)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Le880;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x09u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x08u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Le880;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x03u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a ^ 0x03u));
    native_write(ctx, 0x03u, ctx->a);
Le880: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xe7b8u + ctx->y)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | native_read(ctx, 0x04u)));
    native_write(ctx, 0x04u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xe79du + ctx->y)));
    ctx->x = native_nz(ctx, ctx->a);
    ctx->y = native_nz(ctx, native_read(ctx, 0xecu));
    ctx->a = native_nz(ctx, native_read(ctx, 0x036au));
    if ((ctx->status & NATIVE_Z)) {
        goto Le8c2;
    }
    native_cmp(ctx, ctx->a, 0x01u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Le8a9;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x0363u));
    if (!(ctx->status & NATIVE_N)) {
        goto Le89d;
    }
    ctx->x = native_nz(ctx, 0xdeu);
Le89d: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0xedu));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x20u));
    if ((ctx->status & NATIVE_Z)) {
        goto Le8a6;
    }
Le8a3: ;
    native_write(ctx, 0x0109u, ctx->x);
Le8a6: ;
    /* static tail transfer */
    goto Le9a8;
Le8a9: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x0363u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x01u));
    if ((ctx->status & NATIVE_Z)) {
        goto Le8b2;
    }
    ctx->x = native_nz(ctx, 0xe4u);
Le8b2: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0xedu));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x20u));
    if ((ctx->status & NATIVE_Z)) {
        goto Le8a6;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x02u));
    ctx->status |= NATIVE_C;
    native_sbc(ctx, 0x10u);
    native_write(ctx, 0x02u, ctx->a);
    /* static tail transfer */
    goto Le8a3;
Le8c2: ;
    native_cmp(ctx, ctx->x, 0x24u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Le8d7;
    }
    native_cmp(ctx, ctx->y, 0x05u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Le8d4;
    }
    ctx->x = native_nz(ctx, 0x30u);
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, 0x03u, ctx->a);
    ctx->a = native_nz(ctx, 0x05u);
    native_write(ctx, 0xecu, ctx->a);
Le8d4: ;
    /* static tail transfer */
    goto Le927;
Le8d7: ;
    native_cmp(ctx, ctx->x, 0x90u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Le8ed;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0xedu));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x20u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Le8ea;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x078fu));
    native_cmp(ctx, ctx->a, 0x10u);
    if ((ctx->status & NATIVE_C)) {
        goto Le8ea;
    }
    ctx->x = native_nz(ctx, 0x96u);
Le8ea: ;
    /* static tail transfer */
    goto Le994;
Le8ed: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0xefu));
    native_cmp(ctx, ctx->a, 0x04u);
    if ((ctx->status & NATIVE_C)) {
        goto Le903;
    }
    native_cmp(ctx, ctx->y, 0x02u);
    if (!(ctx->status & NATIVE_C)) {
        goto Le903;
    }
    ctx->x = native_nz(ctx, 0x5au);
    ctx->y = native_nz(ctx, native_read(ctx, 0xefu));
    native_cmp(ctx, ctx->y, 0x02u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Le903;
    }
    ctx->x = native_nz(ctx, 0x7eu);
    native_write(ctx, 0x02u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x02u) + 1u)));
Le903: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0xecu));
    native_cmp(ctx, ctx->a, 0x04u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Le927;
    }
    ctx->x = native_nz(ctx, 0x72u);
    native_write(ctx, 0x02u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x02u) + 1u)));
    ctx->y = native_nz(ctx, native_read(ctx, 0xefu));
    native_cmp(ctx, ctx->y, 0x02u);
    if ((ctx->status & NATIVE_Z)) {
        goto Le917;
    }
    ctx->x = native_nz(ctx, 0x66u);
    native_write(ctx, 0x02u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x02u) + 1u)));
Le917: ;
    native_cmp(ctx, ctx->y, 0x06u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Le927;
    }
    ctx->x = native_nz(ctx, 0x54u);
    ctx->a = native_nz(ctx, native_read(ctx, 0xedu));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x20u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Le927;
    }
    ctx->x = native_nz(ctx, 0x8au);
    native_write(ctx, 0x02u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x02u) - 1u)));
Le927: ;
    ctx->y = native_nz(ctx, native_read(ctx, 0x08u));
    ctx->a = native_nz(ctx, native_read(ctx, 0xefu));
    native_cmp(ctx, ctx->a, 0x05u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Le93b;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0xedu));
    if ((ctx->status & NATIVE_Z)) {
        goto Le957;
    }
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x08u));
    if ((ctx->status & NATIVE_Z)) {
        goto Le994;
    }
    ctx->x = native_nz(ctx, 0xb4u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Le957;
    }
Le93b: ;
    native_cmp(ctx, ctx->x, 0x48u);
    if ((ctx->status & NATIVE_Z)) {
        goto Le957;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0796u + ctx->y)));
    native_cmp(ctx, ctx->a, 0x05u);
    if ((ctx->status & NATIVE_C)) {
        goto Le994;
    }
    native_cmp(ctx, ctx->x, 0x3cu);
    if (!(ctx->status & NATIVE_Z)) {
        goto Le957;
    }
    native_cmp(ctx, ctx->a, 0x01u);
    if ((ctx->status & NATIVE_Z)) {
        goto Le994;
    }
    native_write(ctx, 0x02u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x02u) + 1u)));
    native_write(ctx, 0x02u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x02u) + 1u)));
    native_write(ctx, 0x02u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x02u) + 1u)));
    /* static tail transfer */
    goto Le986;
Le957: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0xefu));
    native_cmp(ctx, ctx->a, 0x06u);
    if ((ctx->status & NATIVE_Z)) {
        goto Le994;
    }
    native_cmp(ctx, ctx->a, 0x08u);
    if ((ctx->status & NATIVE_Z)) {
        goto Le994;
    }
    native_cmp(ctx, ctx->a, 0x0cu);
    if ((ctx->status & NATIVE_Z)) {
        goto Le994;
    }
    native_cmp(ctx, ctx->a, 0x18u);
    if ((ctx->status & NATIVE_C)) {
        goto Le994;
    }
    ctx->y = native_nz(ctx, 0x00u);
    native_cmp(ctx, ctx->a, 0x15u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Le97f;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->a = native_nz(ctx, native_read(ctx, 0x075fu));
    native_cmp(ctx, ctx->a, 0x07u);
    if ((ctx->status & NATIVE_C)) {
        goto Le994;
    }
    ctx->x = native_nz(ctx, 0xa2u);
    ctx->a = native_nz(ctx, 0x03u);
    native_write(ctx, 0xecu, ctx->a);
    if (!(ctx->status & NATIVE_Z)) {
        goto Le994;
    }
Le97f: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x09u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & native_read(ctx, (uint16_t)(0xe7d3u + ctx->y))));
    if (!(ctx->status & NATIVE_Z)) {
        goto Le994;
    }
Le986: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0xedu));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0xa0u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | native_read(ctx, 0x0747u)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Le994;
    }
    ctx->a = native_nz(ctx, ctx->x);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x06u);
    ctx->x = native_nz(ctx, ctx->a);
Le994: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0xedu));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x20u));
    if ((ctx->status & NATIVE_Z)) {
        goto Le9a8;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0xefu));
    native_cmp(ctx, ctx->a, 0x04u);
    if (!(ctx->status & NATIVE_C)) {
        goto Le9a8;
    }
    ctx->y = native_nz(ctx, 0x01u);
    native_write(ctx, 0x0109u, ctx->y);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    native_write(ctx, 0xecu, ctx->y);
Le9a8: ;
    ctx->y = native_nz(ctx, native_read(ctx, 0xebu));
    smbneo_native_fn_render_actor_chr_pair_eb07(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_render_actor_chr_pair_eb07(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_render_actor_chr_pair_eb07(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    ctx->y = native_nz(ctx, native_read(ctx, (uint16_t)(0x06e5u + ctx->x)));
    ctx->a = native_nz(ctx, native_read(ctx, 0xefu));
    native_cmp(ctx, ctx->a, 0x08u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Le9c1;
    }
Le9be: ;
    /* static tail transfer */
    goto Leac1;
Le9c1: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x0109u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lea03;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0202u + ctx->y)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | 0x80u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    smbneo_native_fn_render_set_six_sprites_e512(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    ctx->a = native_nz(ctx, ctx->y);
    ctx->x = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0xefu));
    native_cmp(ctx, ctx->a, 0x05u);
    if ((ctx->status & NATIVE_Z)) {
        goto Le9e7;
    }
    native_cmp(ctx, ctx->a, 0x11u);
    if ((ctx->status & NATIVE_Z)) {
        goto Le9e7;
    }
    native_cmp(ctx, ctx->a, 0x15u);
    if ((ctx->status & NATIVE_C)) {
        goto Le9e7;
    }
    ctx->a = native_nz(ctx, ctx->x);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x08u);
    ctx->x = native_nz(ctx, ctx->a);
Le9e7: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0201u + ctx->x)));
    saved_value_0 = ctx->a;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0205u + ctx->x)));
    saved_value_1 = ctx->a;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0211u + ctx->y)));
    native_write(ctx, (uint16_t)(0x0201u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0215u + ctx->y)));
    native_write(ctx, (uint16_t)(0x0205u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, saved_value_1);
    native_write(ctx, (uint16_t)(0x0215u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, saved_value_0);
    native_write(ctx, (uint16_t)(0x0211u + ctx->y), ctx->a);
Lea03: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x036au));
    if (!(ctx->status & NATIVE_Z)) {
        goto Le9be;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0xefu));
    ctx->x = native_nz(ctx, native_read(ctx, 0xecu));
    native_cmp(ctx, ctx->a, 0x05u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lea13;
    }
    /* static tail transfer */
    goto Leac1;
Lea13: ;
    native_cmp(ctx, ctx->a, 0x07u);
    if ((ctx->status & NATIVE_Z)) {
        goto Lea34;
    }
    native_cmp(ctx, ctx->a, 0x0du);
    if ((ctx->status & NATIVE_Z)) {
        goto Lea34;
    }
    native_cmp(ctx, ctx->a, 0x0cu);
    if ((ctx->status & NATIVE_Z)) {
        goto Lea34;
    }
    native_cmp(ctx, ctx->a, 0x12u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lea27;
    }
    native_cmp(ctx, ctx->x, 0x05u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lea6f;
    }
Lea27: ;
    native_cmp(ctx, ctx->a, 0x15u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lea30;
    }
    ctx->a = native_nz(ctx, 0x42u);
    native_write(ctx, (uint16_t)(0x0216u + ctx->y), ctx->a);
Lea30: ;
    native_cmp(ctx, ctx->x, 0x02u);
    if (!(ctx->status & NATIVE_C)) {
        goto Lea6f;
    }
Lea34: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x036au));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lea6f;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0202u + ctx->y)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0xa3u));
    native_write(ctx, (uint16_t)(0x0202u + ctx->y), ctx->a);
    native_write(ctx, (uint16_t)(0x020au + ctx->y), ctx->a);
    native_write(ctx, (uint16_t)(0x0212u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | 0x40u));
    native_cmp(ctx, ctx->x, 0x05u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lea4f;
    }
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | 0x80u));
Lea4f: ;
    native_write(ctx, (uint16_t)(0x0206u + ctx->y), ctx->a);
    native_write(ctx, (uint16_t)(0x020eu + ctx->y), ctx->a);
    native_write(ctx, (uint16_t)(0x0216u + ctx->y), ctx->a);
    native_cmp(ctx, ctx->x, 0x04u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lea6f;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x020au + ctx->y)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | 0x80u));
    native_write(ctx, (uint16_t)(0x020au + ctx->y), ctx->a);
    native_write(ctx, (uint16_t)(0x0212u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | 0x40u));
    native_write(ctx, (uint16_t)(0x020eu + ctx->y), ctx->a);
    native_write(ctx, (uint16_t)(0x0216u + ctx->y), ctx->a);
Lea6f: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0xefu));
    native_cmp(ctx, ctx->a, 0x11u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Leaab;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x0109u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lea9b;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0212u + ctx->y)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x81u));
    native_write(ctx, (uint16_t)(0x0212u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0216u + ctx->y)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | 0x41u));
    native_write(ctx, (uint16_t)(0x0216u + ctx->y), ctx->a);
    ctx->x = native_nz(ctx, native_read(ctx, 0x078fu));
    native_cmp(ctx, ctx->x, 0x10u);
    if ((ctx->status & NATIVE_C)) {
        goto Leac1;
    }
    native_write(ctx, (uint16_t)(0x020eu + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x81u));
    native_write(ctx, (uint16_t)(0x020au + ctx->y), ctx->a);
    if (!(ctx->status & NATIVE_C)) {
        goto Leac1;
    }
Lea9b: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0202u + ctx->y)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x81u));
    native_write(ctx, (uint16_t)(0x0202u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0206u + ctx->y)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | 0x41u));
    native_write(ctx, (uint16_t)(0x0206u + ctx->y), ctx->a);
Leaab: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0xefu));
    native_cmp(ctx, ctx->a, 0x18u);
    if (!(ctx->status & NATIVE_C)) {
        goto Leac1;
    }
    ctx->a = native_nz(ctx, 0x82u);
    native_write(ctx, (uint16_t)(0x020au + ctx->y), ctx->a);
    native_write(ctx, (uint16_t)(0x0212u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | 0x40u));
    native_write(ctx, (uint16_t)(0x020eu + ctx->y), ctx->a);
    native_write(ctx, (uint16_t)(0x0216u + ctx->y), ctx->a);
Leac1: ;
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    ctx->a = native_nz(ctx, native_read(ctx, 0x03d1u));
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    saved_value_0 = ctx->a;
    if (!(ctx->status & NATIVE_C)) {
        goto Lead1;
    }
    ctx->a = native_nz(ctx, 0x04u);
    smbneo_native_fn_render_actor_clear_chr_pair_h_eb1e(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Lead1: ;
    ctx->a = native_nz(ctx, saved_value_0);
    ctx->a = native_lsr(ctx, ctx->a);
    saved_value_0 = ctx->a;
    if (!(ctx->status & NATIVE_C)) {
        goto Leadb;
    }
    ctx->a = native_nz(ctx, 0x00u);
    smbneo_native_fn_render_actor_clear_chr_pair_h_eb1e(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Leadb: ;
    ctx->a = native_nz(ctx, saved_value_0);
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    saved_value_0 = ctx->a;
    if (!(ctx->status & NATIVE_C)) {
        goto Leae6;
    }
    ctx->a = native_nz(ctx, 0x10u);
    smbneo_native_fn_render_actor_clear_chr_pair_v_eb14(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Leae6: ;
    ctx->a = native_nz(ctx, saved_value_0);
    ctx->a = native_lsr(ctx, ctx->a);
    saved_value_0 = ctx->a;
    if (!(ctx->status & NATIVE_C)) {
        goto Leaf0;
    }
    ctx->a = native_nz(ctx, 0x08u);
    smbneo_native_fn_render_actor_clear_chr_pair_v_eb14(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Leaf0: ;
    ctx->a = native_nz(ctx, saved_value_0);
    ctx->a = native_lsr(ctx, ctx->a);
    if (!(ctx->status & NATIVE_C)) {
        goto Leb06;
    }
    smbneo_native_fn_render_actor_clear_chr_pair_v_eb14(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x16u + ctx->x)));
    native_cmp(ctx, ctx->a, 0x0cu);
    if ((ctx->status & NATIVE_Z)) {
        goto Leb06;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xb6u + ctx->x)));
    native_cmp(ctx, ctx->a, 0x02u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Leb06;
    }
    smbneo_native_fn_actor_erase_c8db(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Leb06: ;
    return;
    return;
}

void smbneo_native_fn_render_actor_chr_pair_eb07(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xeb07u);
#ifdef SMBNEO_NATIVE_FAST_PATHS
    if (vs_native_fast_render_actor_chr_pair(ctx)) return;
#endif
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xe69bu + ctx->x)));
    native_write(ctx, 0x00u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xe69cu + ctx->x)));
    smbneo_native_fn_render_chr_pair_eb0f(ctx);
    return;
    return;
}

void smbneo_native_fn_render_chr_pair_eb0f(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xeb0fu);
#ifdef SMBNEO_NATIVE_FAST_PATHS
    if (vs_native_fast_render_chr_pair(ctx)) return;
#endif
    native_write(ctx, 0x01u, ctx->a);
    /* static tail transfer */
    goto Lf1e7;
Lf1e7: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x03u));
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x00u));
    if (!(ctx->status & NATIVE_C)) {
        goto Lf1fb;
    }
    native_write(ctx, (uint16_t)(0x0205u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x01u));
    native_write(ctx, (uint16_t)(0x0201u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, 0x40u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf205;
    }
Lf1fb: ;
    native_write(ctx, (uint16_t)(0x0201u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x01u));
    native_write(ctx, (uint16_t)(0x0205u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, 0x00u);
Lf205: ;
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | native_read(ctx, 0x04u)));
    native_write(ctx, (uint16_t)(0x0202u + ctx->y), ctx->a);
    native_write(ctx, (uint16_t)(0x0206u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x02u));
    native_write(ctx, (uint16_t)(0x0200u + ctx->y), ctx->a);
    native_write(ctx, (uint16_t)(0x0204u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x05u));
    native_write(ctx, (uint16_t)(0x0203u + ctx->y), ctx->a);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x08u);
    native_write(ctx, (uint16_t)(0x0207u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x02u));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x08u);
    native_write(ctx, 0x02u, ctx->a);
    ctx->a = native_nz(ctx, ctx->y);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x08u);
    ctx->y = native_nz(ctx, ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    return;
    return;
}

void smbneo_native_fn_render_actor_clear_chr_pair_v_eb14(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xeb14u);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, (uint16_t)(0x06e5u + ctx->x)));
    ctx->y = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, 0xf8u);
    /* static tail transfer */
    smbneo_native_fn_render_set_two_sprites_v_e51e(ctx);
    return;
    return;
}

void smbneo_native_fn_render_actor_clear_chr_pair_h_eb1e(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xeb1eu);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, (uint16_t)(0x06e5u + ctx->x)));
    ctx->y = native_nz(ctx, ctx->a);
    smbneo_native_fn_render_offscr_top_clear_eba7(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_write(ctx, (uint16_t)(0x0210u + ctx->y), ctx->a);
    return;
    return;
}

void smbneo_native_fn_render_block_eb2e(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xeb2eu);
    uint8_t saved_value_0 = 0u;
    ctx->a = native_nz(ctx, native_read(ctx, 0x03bcu));
    native_write(ctx, 0x02u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x03b1u));
    native_write(ctx, 0x05u, ctx->a);
    ctx->a = native_nz(ctx, 0x03u);
    native_write(ctx, 0x04u, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    native_write(ctx, 0x03u, ctx->a);
    ctx->y = native_nz(ctx, native_read(ctx, (uint16_t)(0x06ecu + ctx->x)));
    ctx->x = native_nz(ctx, 0x00u);
Leb44: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xeb2au + ctx->x)));
    native_write(ctx, 0x00u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xeb2bu + ctx->x)));
    smbneo_native_fn_render_chr_pair_eb0f(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_cmp(ctx, ctx->x, 0x04u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Leb44;
    }
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    ctx->y = native_nz(ctx, native_read(ctx, (uint16_t)(0x06ecu + ctx->x)));
    ctx->a = native_nz(ctx, native_read(ctx, 0x074eu));
    native_cmp(ctx, ctx->a, 0x01u);
    if ((ctx->status & NATIVE_Z)) {
        goto Leb67;
    }
    ctx->a = native_nz(ctx, 0x86u);
    native_write(ctx, (uint16_t)(0x0201u + ctx->y), ctx->a);
    native_write(ctx, (uint16_t)(0x0205u + ctx->y), ctx->a);
Leb67: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x03e8u + ctx->x)));
    native_cmp(ctx, ctx->a, 0xc4u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Leb92;
    }
    ctx->a = native_nz(ctx, 0x87u);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    smbneo_native_fn_render_set_four_sprites_v_e518(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    ctx->a = native_nz(ctx, 0x03u);
    ctx->x = native_nz(ctx, native_read(ctx, 0x074eu));
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x - 1u));
    if ((ctx->status & NATIVE_Z)) {
        goto Leb7e;
    }
    ctx->a = native_lsr(ctx, ctx->a);
Leb7e: ;
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    native_write(ctx, (uint16_t)(0x0202u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | 0x40u));
    native_write(ctx, (uint16_t)(0x0206u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | 0x80u));
    native_write(ctx, (uint16_t)(0x020eu + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x83u));
    native_write(ctx, (uint16_t)(0x020au + ctx->y), ctx->a);
Leb92: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x03d4u));
    saved_value_0 = ctx->a;
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x04u));
    if ((ctx->status & NATIVE_Z)) {
        goto Leba2;
    }
    ctx->a = native_nz(ctx, 0xf8u);
    native_write(ctx, (uint16_t)(0x0204u + ctx->y), ctx->a);
    native_write(ctx, (uint16_t)(0x020cu + ctx->y), ctx->a);
Leba2: ;
    ctx->a = native_nz(ctx, saved_value_0);
    smbneo_native_fn_render_offscr_check_top_eba3(ctx);
    return;
    return;
}

void smbneo_native_fn_render_offscr_check_top_eba3(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xeba3u);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x08u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lebaf;
    }
    smbneo_native_fn_render_offscr_top_clear_eba7(ctx);
    return;
Lebaf: ;
    return;
    return;
}

void smbneo_native_fn_render_offscr_top_clear_eba7(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xeba7u);
    ctx->a = native_nz(ctx, 0xf8u);
    native_write(ctx, (uint16_t)(0x0200u + ctx->y), ctx->a);
    native_write(ctx, (uint16_t)(0x0208u + ctx->y), ctx->a);
    return;
    return;
}

void smbneo_native_fn_render_block_bits_ebb0(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xebb0u);
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, 0x00u, ctx->a);
    ctx->a = native_nz(ctx, 0x75u);
    ctx->y = native_nz(ctx, native_read(ctx, 0x0eu));
    native_cmp(ctx, ctx->y, 0x05u);
    if ((ctx->status & NATIVE_Z)) {
        goto Lebc2;
    }
    ctx->a = native_nz(ctx, 0x03u);
    native_write(ctx, 0x00u, ctx->a);
    ctx->a = native_nz(ctx, 0x84u);
Lebc2: ;
    ctx->y = native_nz(ctx, native_read(ctx, (uint16_t)(0x06ecu + ctx->x)));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    smbneo_native_fn_render_set_four_sprites_v_e518(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, 0x09u));
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0xc0u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | native_read(ctx, 0x00u)));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    smbneo_native_fn_render_set_four_sprites_v_e518(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    ctx->a = native_nz(ctx, native_read(ctx, 0x03bcu));
    smbneo_native_fn_render_set_two_sprites_v_e51e(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, 0x03b1u));
    native_write(ctx, (uint16_t)(0x0203u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x03f1u + ctx->x)));
    ctx->status |= NATIVE_C;
    native_sbc(ctx, native_read(ctx, 0x071cu));
    native_write(ctx, 0x00u, ctx->a);
    ctx->status |= NATIVE_C;
    native_sbc(ctx, native_read(ctx, 0x03b1u));
    native_adc(ctx, native_read(ctx, 0x00u));
    native_adc(ctx, 0x06u);
    native_write(ctx, (uint16_t)(0x0207u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x03bdu));
    native_write(ctx, (uint16_t)(0x0208u + ctx->y), ctx->a);
    native_write(ctx, (uint16_t)(0x020cu + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x03b2u));
    native_write(ctx, (uint16_t)(0x020bu + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x00u));
    ctx->status |= NATIVE_C;
    native_sbc(ctx, native_read(ctx, 0x03b2u));
    native_adc(ctx, native_read(ctx, 0x00u));
    native_adc(ctx, 0x06u);
    native_write(ctx, (uint16_t)(0x020fu + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x03d4u));
    smbneo_native_fn_render_offscr_check_top_eba3(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, 0x03d4u));
    ctx->a = native_asl(ctx, ctx->a);
    if (!(ctx->status & NATIVE_C)) {
        goto Lec26;
    }
    ctx->a = native_nz(ctx, 0xf8u);
    smbneo_native_fn_render_set_two_sprites_v_e51e(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Lec26: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x00u));
    if (!(ctx->status & NATIVE_N)) {
        goto Lec3a;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0203u + ctx->y)));
    native_cmp(ctx, ctx->a, native_read(ctx, (uint16_t)(0x0207u + ctx->y)));
    if (!(ctx->status & NATIVE_C)) {
        goto Lec3a;
    }
    ctx->a = native_nz(ctx, 0xf8u);
    native_write(ctx, (uint16_t)(0x0204u + ctx->y), ctx->a);
    native_write(ctx, (uint16_t)(0x020cu + ctx->y), ctx->a);
Lec3a: ;
    return;
    return;
}

void smbneo_native_fn_render_firebar_ec4a(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xec4au);
    uint8_t saved_value_0 = 0u;
    ctx->a = native_nz(ctx, native_read(ctx, 0x09u));
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    saved_value_0 = ctx->a;
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x01u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a ^ 0x64u));
    native_write(ctx, (uint16_t)(0x0201u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, saved_value_0);
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_nz(ctx, 0x02u);
    if (!(ctx->status & NATIVE_C)) {
        goto Lec5f;
    }
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | 0xc0u));
Lec5f: ;
    native_write(ctx, (uint16_t)(0x0202u + ctx->y), ctx->a);
    return;
    return;
}

void smbneo_native_fn_render_fireworks_ec74(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xec74u);
    ctx->x = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xec63u + ctx->x)));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    smbneo_native_fn_render_set_four_sprites_v_e518(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    ctx->a = native_nz(ctx, native_read(ctx, 0x03bau));
    ctx->status |= NATIVE_C;
    native_sbc(ctx, 0x04u);
    native_write(ctx, (uint16_t)(0x0200u + ctx->y), ctx->a);
    native_write(ctx, (uint16_t)(0x0208u + ctx->y), ctx->a);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x08u);
    native_write(ctx, (uint16_t)(0x0204u + ctx->y), ctx->a);
    native_write(ctx, (uint16_t)(0x020cu + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x03afu));
    ctx->status |= NATIVE_C;
    native_sbc(ctx, 0x04u);
    native_write(ctx, (uint16_t)(0x0203u + ctx->y), ctx->a);
    native_write(ctx, (uint16_t)(0x0207u + ctx->y), ctx->a);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x08u);
    native_write(ctx, (uint16_t)(0x020bu + ctx->y), ctx->a);
    native_write(ctx, (uint16_t)(0x020fu + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, (uint16_t)(0x0202u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, 0x82u);
    native_write(ctx, (uint16_t)(0x0206u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, 0x42u);
    native_write(ctx, (uint16_t)(0x020au + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, 0xc2u);
    native_write(ctx, (uint16_t)(0x020eu + ctx->y), ctx->a);
    return;
    return;
}

void smbneo_native_fn_render_plat_small_ecc3(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xecc3u);
    uint8_t saved_value_0 = 0u;
    ctx->y = native_nz(ctx, native_read(ctx, (uint16_t)(0x06e5u + ctx->x)));
    ctx->a = native_nz(ctx, 0x5bu);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    smbneo_native_fn_render_set_six_sprites_e512(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->a = native_nz(ctx, 0x02u);
    smbneo_native_fn_render_set_six_sprites_e512(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    ctx->a = native_nz(ctx, native_read(ctx, 0x03aeu));
    native_write(ctx, (uint16_t)(0x0203u + ctx->y), ctx->a);
    native_write(ctx, (uint16_t)(0x020fu + ctx->y), ctx->a);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x08u);
    native_write(ctx, (uint16_t)(0x0207u + ctx->y), ctx->a);
    native_write(ctx, (uint16_t)(0x0213u + ctx->y), ctx->a);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x08u);
    native_write(ctx, (uint16_t)(0x020bu + ctx->y), ctx->a);
    native_write(ctx, (uint16_t)(0x0217u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xcfu + ctx->x)));
    ctx->x = native_nz(ctx, ctx->a);
    saved_value_0 = ctx->a;
    native_cmp(ctx, ctx->x, 0x20u);
    if ((ctx->status & NATIVE_C)) {
        goto Lecf9;
    }
    ctx->a = native_nz(ctx, 0xf8u);
Lecf9: ;
    smbneo_native_fn_render_set_three_sprites_v_e51b(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, saved_value_0);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x80u);
    ctx->x = native_nz(ctx, ctx->a);
    native_cmp(ctx, ctx->x, 0x20u);
    if ((ctx->status & NATIVE_C)) {
        goto Led07;
    }
    ctx->a = native_nz(ctx, 0xf8u);
Led07: ;
    native_write(ctx, (uint16_t)(0x020cu + ctx->y), ctx->a);
    native_write(ctx, (uint16_t)(0x0210u + ctx->y), ctx->a);
    native_write(ctx, (uint16_t)(0x0214u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x03d1u));
    saved_value_0 = ctx->a;
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x08u));
    if ((ctx->status & NATIVE_Z)) {
        goto Led20;
    }
    ctx->a = native_nz(ctx, 0xf8u);
    native_write(ctx, (uint16_t)(0x0200u + ctx->y), ctx->a);
    native_write(ctx, (uint16_t)(0x020cu + ctx->y), ctx->a);
Led20: ;
    ctx->a = native_nz(ctx, saved_value_0);
    saved_value_0 = ctx->a;
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x04u));
    if ((ctx->status & NATIVE_Z)) {
        goto Led2e;
    }
    ctx->a = native_nz(ctx, 0xf8u);
    native_write(ctx, (uint16_t)(0x0204u + ctx->y), ctx->a);
    native_write(ctx, (uint16_t)(0x0210u + ctx->y), ctx->a);
Led2e: ;
    ctx->a = native_nz(ctx, saved_value_0);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x02u));
    if ((ctx->status & NATIVE_Z)) {
        goto Led3b;
    }
    ctx->a = native_nz(ctx, 0xf8u);
    native_write(ctx, (uint16_t)(0x0208u + ctx->y), ctx->a);
    native_write(ctx, (uint16_t)(0x0214u + ctx->y), ctx->a);
Led3b: ;
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    return;
    return;
}

void smbneo_native_fn_bubble_render_ed3e(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xed3eu);
    ctx->y = native_nz(ctx, native_read(ctx, 0xb5u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Led63;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x03d3u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x08u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Led63;
    }
    ctx->y = native_nz(ctx, native_read(ctx, (uint16_t)(0x06eeu + ctx->x)));
    ctx->a = native_nz(ctx, native_read(ctx, 0x03b0u));
    native_write(ctx, (uint16_t)(0x0203u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x03bbu));
    native_write(ctx, (uint16_t)(0x0200u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, 0x74u);
    native_write(ctx, (uint16_t)(0x0201u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, 0x02u);
    native_write(ctx, (uint16_t)(0x0202u + ctx->y), ctx->a);
Led63: ;
    return;
    return;
}

void smbneo_native_fn_render_player_ee46(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xee46u);
    ctx->a = native_nz(ctx, 0xffu);
    ctx->a = native_nz(ctx, native_read(ctx, 0x079eu));
    if ((ctx->status & NATIVE_Z)) {
        goto Lee58;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x09u));
    ctx->a = native_lsr(ctx, ctx->a);
    if ((ctx->status & NATIVE_C)) {
        goto Lee98;
    }
Lee58: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x0eu));
    native_cmp(ctx, ctx->a, 0x0bu);
    if ((ctx->status & NATIVE_Z)) {
        goto Leea5;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x070bu));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lee9f;
    }
    ctx->y = native_nz(ctx, native_read(ctx, 0x0704u));
    if ((ctx->status & NATIVE_Z)) {
        smbneo_native_fn_render_player_normal_ee99(ctx);
        return;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x1du));
    native_cmp(ctx, ctx->a, 0x00u);
    if ((ctx->status & NATIVE_Z)) {
        smbneo_native_fn_render_player_normal_ee99(ctx);
        return;
    }
    smbneo_native_fn_render_player_normal_ee99(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, 0x09u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x04u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lee98;
    }
    ctx->x = native_nz(ctx, ctx->a);
    ctx->y = native_nz(ctx, native_read(ctx, 0x06e4u));
    ctx->a = native_nz(ctx, native_read(ctx, 0x33u));
    ctx->a = native_lsr(ctx, ctx->a);
    if ((ctx->status & NATIVE_C)) {
        goto Lee84;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
Lee84: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x0754u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lee92;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0219u + ctx->y)));
    native_cmp(ctx, ctx->a, native_read(ctx, 0xee12u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lee98;
    }
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
Lee92: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xee44u + ctx->x)));
    native_write(ctx, (uint16_t)(0x0219u + ctx->y), ctx->a);
Lee98: ;
    return;
Lee9f: ;
    smbneo_native_fn_render_player_resize_f015(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    /* static tail transfer */
    goto Leeaa;
Leea5: ;
    ctx->y = native_nz(ctx, 0x0eu);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xed64u + ctx->y)));
Leeaa: ;
    native_write(ctx, 0x06d5u, ctx->a);
    ctx->a = native_nz(ctx, 0x04u);
    smbneo_native_fn_render_player_tiles_init_ef23(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_render_player_tiles_mirror_f04e(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, 0x0711u));
    if ((ctx->status & NATIVE_Z)) {
        goto Leedf;
    }
    ctx->y = native_nz(ctx, 0x00u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0781u));
    native_cmp(ctx, ctx->a, native_read(ctx, 0x0711u));
    native_write(ctx, 0x0711u, ctx->y);
    if ((ctx->status & NATIVE_C)) {
        goto Leedf;
    }
    native_write(ctx, 0x0711u, ctx->a);
    ctx->y = native_nz(ctx, 0x07u);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xed64u + ctx->y)));
    native_write(ctx, 0x06d5u, ctx->a);
    ctx->y = native_nz(ctx, 0x04u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x57u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | native_read(ctx, 0x0cu)));
    if ((ctx->status & NATIVE_Z)) {
        goto Leedb;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
Leedb: ;
    ctx->a = native_nz(ctx, ctx->y);
    smbneo_native_fn_render_player_tiles_init_ef23(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Leedf: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x03d0u));
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    native_write(ctx, 0x00u, ctx->a);
    ctx->x = native_nz(ctx, 0x03u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x06e4u));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x18u);
    ctx->y = native_nz(ctx, ctx->a);
Leef1: ;
    ctx->a = native_nz(ctx, 0xf8u);
    native_write(ctx, 0x00u, native_lsr(ctx, native_read(ctx, 0x00u)));
    if (!(ctx->status & NATIVE_C)) {
        goto Leefa;
    }
    smbneo_native_fn_render_set_two_sprites_v_e51e(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Leefa: ;
    ctx->a = native_nz(ctx, ctx->y);
    ctx->status |= NATIVE_C;
    native_sbc(ctx, 0x08u);
    ctx->y = native_nz(ctx, ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x - 1u));
    if (!(ctx->status & NATIVE_N)) {
        goto Leef1;
    }
    return;
    return;
}

void smbneo_native_fn_render_player_normal_ee99(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xee99u);
    smbneo_native_fn_render_player_action_ef51(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    /* static tail transfer */
    goto Leeaa;
Leeaa: ;
    native_write(ctx, 0x06d5u, ctx->a);
    ctx->a = native_nz(ctx, 0x04u);
    smbneo_native_fn_render_player_tiles_init_ef23(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_render_player_tiles_mirror_f04e(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, 0x0711u));
    if ((ctx->status & NATIVE_Z)) {
        goto Leedf;
    }
    ctx->y = native_nz(ctx, 0x00u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0781u));
    native_cmp(ctx, ctx->a, native_read(ctx, 0x0711u));
    native_write(ctx, 0x0711u, ctx->y);
    if ((ctx->status & NATIVE_C)) {
        goto Leedf;
    }
    native_write(ctx, 0x0711u, ctx->a);
    ctx->y = native_nz(ctx, 0x07u);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xed64u + ctx->y)));
    native_write(ctx, 0x06d5u, ctx->a);
    ctx->y = native_nz(ctx, 0x04u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x57u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | native_read(ctx, 0x0cu)));
    if ((ctx->status & NATIVE_Z)) {
        goto Leedb;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
Leedb: ;
    ctx->a = native_nz(ctx, ctx->y);
    smbneo_native_fn_render_player_tiles_init_ef23(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Leedf: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x03d0u));
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    native_write(ctx, 0x00u, ctx->a);
    ctx->x = native_nz(ctx, 0x03u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x06e4u));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x18u);
    ctx->y = native_nz(ctx, ctx->a);
Leef1: ;
    ctx->a = native_nz(ctx, 0xf8u);
    native_write(ctx, 0x00u, native_lsr(ctx, native_read(ctx, 0x00u)));
    if (!(ctx->status & NATIVE_C)) {
        goto Leefa;
    }
    smbneo_native_fn_render_set_two_sprites_v_e51e(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Leefa: ;
    ctx->a = native_nz(ctx, ctx->y);
    ctx->status |= NATIVE_C;
    native_sbc(ctx, 0x08u);
    ctx->y = native_nz(ctx, ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x - 1u));
    if (!(ctx->status & NATIVE_N)) {
        goto Leef1;
    }
    return;
    return;
}

void smbneo_native_fn_render_player_course_card_ef09(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xef09u);
    ctx->x = native_nz(ctx, 0x05u);
Lef0b: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xef03u + ctx->x)));
    native_write(ctx, (uint16_t)(uint8_t)(0x02u + ctx->x), ctx->a);
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x - 1u));
    if (!(ctx->status & NATIVE_N)) {
        goto Lef0b;
    }
    ctx->x = native_nz(ctx, 0xb8u);
    ctx->y = native_nz(ctx, 0x04u);
    smbneo_native_fn_render_player_tiles_ef41(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, 0x0226u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | 0x40u));
    native_write(ctx, 0x0222u, ctx->a);
    return;
    return;
}

void smbneo_native_fn_render_player_tiles_init_ef23(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xef23u);
    native_write(ctx, 0x07u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x03adu));
    native_write(ctx, 0x0755u, ctx->a);
    native_write(ctx, 0x05u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x03b8u));
    native_write(ctx, 0x02u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x33u));
    native_write(ctx, 0x03u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x03c4u));
    native_write(ctx, 0x04u, ctx->a);
    ctx->x = native_nz(ctx, native_read(ctx, 0x06d5u));
    ctx->y = native_nz(ctx, native_read(ctx, 0x06e4u));
    smbneo_native_fn_render_player_tiles_ef41(ctx);
    return;
    return;
}

void smbneo_native_fn_render_player_tiles_ef41(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xef41u);
Lef41: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xed74u + ctx->x)));
    native_write(ctx, 0x00u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xed75u + ctx->x)));
    smbneo_native_fn_render_chr_pair_eb0f(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_write(ctx, 0x07u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x07u) - 1u)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lef41;
    }
    return;
    return;
}

void smbneo_native_fn_render_player_action_ef51(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xef51u);
    uint8_t saved_value_0 = 0u;
    ctx->a = native_nz(ctx, native_read(ctx, 0x1du));
    native_cmp(ctx, ctx->a, 0x03u);
    if ((ctx->status & NATIVE_Z)) {
        goto Lefa9;
    }
    native_cmp(ctx, ctx->a, 0x02u);
    if ((ctx->status & NATIVE_Z)) {
        goto Lef99;
    }
    native_cmp(ctx, ctx->a, 0x01u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lef70;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x0704u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lefb5;
    }
    ctx->y = native_nz(ctx, 0x06u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0714u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lef8d;
    }
    ctx->y = native_nz(ctx, 0x00u);
    /* static tail transfer */
    goto Lef8d;
Lef70: ;
    ctx->y = native_nz(ctx, 0x06u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0714u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lef8d;
    }
    ctx->y = native_nz(ctx, 0x02u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x57u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | native_read(ctx, 0x0cu)));
    if ((ctx->status & NATIVE_Z)) {
        goto Lef8d;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x0700u));
    native_cmp(ctx, ctx->a, 0x09u);
    if (!(ctx->status & NATIVE_C)) {
        goto Lefa1;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x45u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & native_read(ctx, 0x33u)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lefa1;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
Lef8d: ;
    smbneo_native_fn_render_player_get_size_offset_eff6(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x070du, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xed64u + ctx->y)));
    return;
Lef99: ;
    ctx->y = native_nz(ctx, 0x04u);
    smbneo_native_fn_render_player_get_size_offset_eff6(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    /* static tail transfer */
    smbneo_native_fn_render_player_fall_efc7(ctx);
    return;
Lefa1: ;
    ctx->y = native_nz(ctx, 0x04u);
    smbneo_native_fn_render_player_get_size_offset_eff6(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    /* static tail transfer */
    goto Lefcd;
Lefa9: ;
    ctx->y = native_nz(ctx, 0x05u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x9fu));
    if ((ctx->status & NATIVE_Z)) {
        goto Lef8d;
    }
    smbneo_native_fn_render_player_get_size_offset_eff6(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    /* static tail transfer */
    goto Lefd2;
Lefb5: ;
    ctx->y = native_nz(ctx, 0x01u);
    smbneo_native_fn_render_player_get_size_offset_eff6(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, native_read(ctx, 0x0782u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | native_read(ctx, 0x070du)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lefcd;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x0au));
    ctx->a = native_asl(ctx, ctx->a);
    if ((ctx->status & NATIVE_C)) {
        goto Lefcd;
    }
    smbneo_native_fn_render_player_fall_efc7(ctx);
    return;
Lefcd: ;
    ctx->a = native_nz(ctx, 0x03u);
    /* static tail transfer */
    goto Lefd4;
Lefd2: ;
    ctx->a = native_nz(ctx, 0x02u);
Lefd4: ;
    native_write(ctx, 0x00u, ctx->a);
    smbneo_native_fn_render_player_fall_efc7(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    saved_value_0 = ctx->a;
    ctx->a = native_nz(ctx, native_read(ctx, 0x0781u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Leff4;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x070cu));
    native_write(ctx, 0x0781u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x070du));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x01u);
    native_cmp(ctx, ctx->a, native_read(ctx, 0x00u));
    if (!(ctx->status & NATIVE_C)) {
        goto Leff1;
    }
    ctx->a = native_nz(ctx, 0x00u);
Leff1: ;
    native_write(ctx, 0x070du, ctx->a);
Leff4: ;
    ctx->a = native_nz(ctx, saved_value_0);
    return;
    return;
}

void smbneo_native_fn_render_player_fall_efc7(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xefc7u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x070du));
    /* static tail transfer */
    goto Lf035;
Lf035: ;
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    native_adc(ctx, native_read(ctx, (uint16_t)(0xed64u + ctx->y)));
    return;
    return;
}

void smbneo_native_fn_render_player_get_size_offset_eff6(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xeff6u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0754u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lf000;
    }
    ctx->a = native_nz(ctx, ctx->y);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x08u);
    ctx->y = native_nz(ctx, ctx->a);
Lf000: ;
    return;
    return;
}

void smbneo_native_fn_render_player_resize_f015(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xf015u);
    ctx->y = native_nz(ctx, native_read(ctx, 0x070du));
    ctx->a = native_nz(ctx, native_read(ctx, 0x09u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x03u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf02b;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    native_cmp(ctx, ctx->y, 0x0au);
    if (!(ctx->status & NATIVE_C)) {
        goto Lf028;
    }
    ctx->y = native_nz(ctx, 0x00u);
    native_write(ctx, 0x070bu, ctx->y);
Lf028: ;
    native_write(ctx, 0x070du, ctx->y);
Lf02b: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x0754u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf03c;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xf001u + ctx->y)));
    ctx->y = native_nz(ctx, 0x0fu);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    native_adc(ctx, native_read(ctx, (uint16_t)(0xed64u + ctx->y)));
    return;
Lf03c: ;
    ctx->a = native_nz(ctx, ctx->y);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, 0x0au);
    ctx->x = native_nz(ctx, ctx->a);
    ctx->y = native_nz(ctx, 0x09u);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xf001u + ctx->x)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf04a;
    }
    ctx->y = native_nz(ctx, 0x01u);
Lf04a: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xed64u + ctx->y)));
    return;
    return;
}

void smbneo_native_fn_render_player_tiles_mirror_f04e(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xf04eu);
    ctx->y = native_nz(ctx, native_read(ctx, 0x06e4u));
    ctx->a = native_nz(ctx, native_read(ctx, 0x0eu));
    native_cmp(ctx, ctx->a, 0x0bu);
    if ((ctx->status & NATIVE_Z)) {
        goto Lf06a;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x06d5u));
    native_cmp(ctx, ctx->a, 0x50u);
    if ((ctx->status & NATIVE_Z)) {
        goto Lf07c;
    }
    native_cmp(ctx, ctx->a, 0xb8u);
    if ((ctx->status & NATIVE_Z)) {
        goto Lf07c;
    }
    native_cmp(ctx, ctx->a, 0xc0u);
    if ((ctx->status & NATIVE_Z)) {
        goto Lf07c;
    }
    native_cmp(ctx, ctx->a, 0xc8u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf08e;
    }
Lf06a: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0212u + ctx->y)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x3fu));
    native_write(ctx, (uint16_t)(0x0212u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x0216u + ctx->y)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x3fu));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | 0x40u));
    native_write(ctx, (uint16_t)(0x0216u + ctx->y), ctx->a);
Lf07c: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x021au + ctx->y)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x3fu));
    native_write(ctx, (uint16_t)(0x021au + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x021eu + ctx->y)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x3fu));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | 0x40u));
    native_write(ctx, (uint16_t)(0x021eu + ctx->y), ctx->a);
Lf08e: ;
    return;
    return;
}

void smbneo_native_fn_pos_calc_x_rel_player_f08f(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xf08fu);
    ctx->x = native_nz(ctx, 0x00u);
    ctx->y = native_nz(ctx, 0x00u);
    /* static tail transfer */
    goto Lf0a7;
Lf0a7: ;
    smbneo_native_fn_pos_calc_x_rel_do_f0d6(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    return;
    return;
}

void smbneo_native_fn_pos_calc_x_rel_bubble_f096(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xf096u);
    ctx->y = native_nz(ctx, 0x01u);
    smbneo_native_fn_pos_calc_x_offset_get_f10d(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->y = native_nz(ctx, 0x03u);
    /* static tail transfer */
    goto Lf0a7;
Lf0a7: ;
    smbneo_native_fn_pos_calc_x_rel_do_f0d6(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    return;
    return;
}

void smbneo_native_fn_pos_calc_x_rel_proj_f0a0(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xf0a0u);
    ctx->y = native_nz(ctx, 0x00u);
    smbneo_native_fn_pos_calc_x_offset_get_f10d(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->y = native_nz(ctx, 0x02u);
    smbneo_native_fn_pos_calc_x_rel_do_f0d6(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    return;
    return;
}

void smbneo_native_fn_pos_calc_x_rel_misc_f0ad(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xf0adu);
    goto Lf0ad;
Lf0a7: ;
    smbneo_native_fn_pos_calc_x_rel_do_f0d6(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    return;
Lf0ad: ;
    ctx->y = native_nz(ctx, 0x02u);
    smbneo_native_fn_pos_calc_x_offset_get_f10d(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->y = native_nz(ctx, 0x06u);
    /* static tail transfer */
    goto Lf0a7;
    return;
}

void smbneo_native_fn_pos_calc_x_rel_actor_f0b7(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xf0b7u);
    ctx->a = native_nz(ctx, 0x01u);
    ctx->y = native_nz(ctx, 0x01u);
    /* static tail transfer */
    smbneo_native_fn_pos_calc_x_rel_b_f0ca(ctx);
    return;
    return;
}

void smbneo_native_fn_pos_calc_x_rel_block_f0be(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xf0beu);
    ctx->a = native_nz(ctx, 0x09u);
    ctx->y = native_nz(ctx, 0x04u);
    smbneo_native_fn_pos_calc_x_rel_b_f0ca(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x + 1u));
    ctx->a = native_nz(ctx, 0x09u);
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    smbneo_native_fn_pos_calc_x_rel_b_f0ca(ctx);
    return;
    return;
}

void smbneo_native_fn_pos_calc_x_rel_b_f0ca(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xf0cau);
    native_write(ctx, 0x00u, ctx->x);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, 0x00u));
    ctx->x = native_nz(ctx, ctx->a);
    smbneo_native_fn_pos_calc_x_rel_do_f0d6(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    return;
    return;
}

void smbneo_native_fn_pos_calc_x_rel_do_f0d6(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xf0d6u);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xceu + ctx->x)));
    native_write(ctx, (uint16_t)(0x03b8u + ctx->y), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x86u + ctx->x)));
    ctx->status |= NATIVE_C;
    native_sbc(ctx, native_read(ctx, 0x071cu));
    native_write(ctx, (uint16_t)(0x03adu + ctx->y), ctx->a);
    return;
    return;
}

void smbneo_native_fn_pos_bits_get_player_f0e5(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xf0e5u);
    uint8_t saved_value_0 = 0u;
    ctx->x = native_nz(ctx, 0x00u);
    ctx->y = native_nz(ctx, 0x00u);
    /* static tail transfer */
    goto Lf125;
Lf125: ;
    ctx->a = native_nz(ctx, ctx->y);
    saved_value_0 = ctx->a;
    smbneo_native_fn_pos_bits_proc_f13c(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | native_read(ctx, 0x00u)));
    native_write(ctx, 0x00u, ctx->a);
    ctx->a = native_nz(ctx, saved_value_0);
    ctx->y = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x00u));
    native_write(ctx, (uint16_t)(0x03d0u + ctx->y), ctx->a);
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    return;
    return;
}

void smbneo_native_fn_pos_bits_get_proj_f0ec(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xf0ecu);
    uint8_t saved_value_0 = 0u;
    ctx->y = native_nz(ctx, 0x00u);
    smbneo_native_fn_pos_calc_x_offset_get_f10d(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->y = native_nz(ctx, 0x02u);
    /* static tail transfer */
    goto Lf125;
Lf125: ;
    ctx->a = native_nz(ctx, ctx->y);
    saved_value_0 = ctx->a;
    smbneo_native_fn_pos_bits_proc_f13c(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | native_read(ctx, 0x00u)));
    native_write(ctx, 0x00u, ctx->a);
    ctx->a = native_nz(ctx, saved_value_0);
    ctx->y = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x00u));
    native_write(ctx, (uint16_t)(0x03d0u + ctx->y), ctx->a);
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    return;
    return;
}

void smbneo_native_fn_pos_bits_get_bubble_f0f6(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xf0f6u);
    uint8_t saved_value_0 = 0u;
    ctx->y = native_nz(ctx, 0x01u);
    smbneo_native_fn_pos_calc_x_offset_get_f10d(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->y = native_nz(ctx, 0x03u);
    /* static tail transfer */
    goto Lf125;
Lf125: ;
    ctx->a = native_nz(ctx, ctx->y);
    saved_value_0 = ctx->a;
    smbneo_native_fn_pos_bits_proc_f13c(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | native_read(ctx, 0x00u)));
    native_write(ctx, 0x00u, ctx->a);
    ctx->a = native_nz(ctx, saved_value_0);
    ctx->y = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x00u));
    native_write(ctx, (uint16_t)(0x03d0u + ctx->y), ctx->a);
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    return;
    return;
}

void smbneo_native_fn_pos_bits_get_misc_f100(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xf100u);
    uint8_t saved_value_0 = 0u;
    ctx->y = native_nz(ctx, 0x02u);
    smbneo_native_fn_pos_calc_x_offset_get_f10d(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->y = native_nz(ctx, 0x06u);
    /* static tail transfer */
    goto Lf125;
Lf125: ;
    ctx->a = native_nz(ctx, ctx->y);
    saved_value_0 = ctx->a;
    smbneo_native_fn_pos_bits_proc_f13c(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | native_read(ctx, 0x00u)));
    native_write(ctx, 0x00u, ctx->a);
    ctx->a = native_nz(ctx, saved_value_0);
    ctx->y = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x00u));
    native_write(ctx, (uint16_t)(0x03d0u + ctx->y), ctx->a);
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    return;
    return;
}

void smbneo_native_fn_pos_calc_x_offset_get_f10d(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xf10du);
    ctx->a = native_nz(ctx, ctx->x);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, (uint16_t)(0xf10au + ctx->y)));
    ctx->x = native_nz(ctx, ctx->a);
    return;
    return;
}

void smbneo_native_fn_pos_bits_get_actor_f114(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xf114u);
#ifdef SMBNEO_NATIVE_FAST_PATHS
    if (vs_native_fast_pos_bits_get_actor(ctx)) return;
#endif
    uint8_t saved_value_0 = 0u;
    ctx->a = native_nz(ctx, 0x01u);
    ctx->y = native_nz(ctx, 0x01u);
    /* static tail transfer */
    goto Lf11f;
Lf11f: ;
    native_write(ctx, 0x00u, ctx->x);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, 0x00u));
    ctx->x = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, ctx->y);
    saved_value_0 = ctx->a;
    smbneo_native_fn_pos_bits_proc_f13c(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | native_read(ctx, 0x00u)));
    native_write(ctx, 0x00u, ctx->a);
    ctx->a = native_nz(ctx, saved_value_0);
    ctx->y = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x00u));
    native_write(ctx, (uint16_t)(0x03d0u + ctx->y), ctx->a);
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    return;
    return;
}

void smbneo_native_fn_pos_bits_get_block_f11b(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xf11bu);
    uint8_t saved_value_0 = 0u;
    ctx->a = native_nz(ctx, 0x09u);
    ctx->y = native_nz(ctx, 0x04u);
    native_write(ctx, 0x00u, ctx->x);
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, 0x00u));
    ctx->x = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, ctx->y);
    saved_value_0 = ctx->a;
    smbneo_native_fn_pos_bits_proc_f13c(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_asl(ctx, ctx->a);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | native_read(ctx, 0x00u)));
    native_write(ctx, 0x00u, ctx->a);
    ctx->a = native_nz(ctx, saved_value_0);
    ctx->y = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x00u));
    native_write(ctx, (uint16_t)(0x03d0u + ctx->y), ctx->a);
    ctx->x = native_nz(ctx, native_read(ctx, 0x08u));
    return;
    return;
}

void smbneo_native_fn_pos_bits_proc_f13c(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xf13cu);
#ifdef SMBNEO_NATIVE_FAST_PATHS
    if (vs_native_fast_pos_bits_proc(ctx)) return;
#endif
    smbneo_native_fn_pos_bits_get_do_x_f15b(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    native_write(ctx, 0x00u, ctx->a);
    /* static tail transfer */
    goto Lf19e;
Lf19e: ;
    native_write(ctx, 0x04u, ctx->x);
    ctx->y = native_nz(ctx, 0x01u);
Lf1a2: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xf19cu + ctx->y)));
    ctx->status |= NATIVE_C;
    native_sbc(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xceu + ctx->x)));
    native_write(ctx, 0x07u, ctx->a);
    ctx->a = native_nz(ctx, 0x01u);
    native_sbc(ctx, native_read(ctx, (uint16_t)(uint8_t)(0xb5u + ctx->x)));
    ctx->x = native_nz(ctx, native_read(ctx, (uint16_t)(0xf199u + ctx->y)));
    native_cmp(ctx, ctx->a, 0x00u);
    if ((ctx->status & NATIVE_N)) {
        goto Lf1c5;
    }
    ctx->x = native_nz(ctx, native_read(ctx, (uint16_t)(0xf19au + ctx->y)));
    native_cmp(ctx, ctx->a, 0x01u);
    if (!(ctx->status & NATIVE_N)) {
        goto Lf1c5;
    }
    ctx->a = native_nz(ctx, 0x20u);
    native_write(ctx, 0x06u, ctx->a);
    ctx->a = native_nz(ctx, 0x04u);
    smbneo_native_fn_pos_bits_diff_f1d2(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Lf1c5: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xf190u + ctx->x)));
    ctx->x = native_nz(ctx, native_read(ctx, 0x04u));
    native_cmp(ctx, ctx->a, 0x00u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf1d1;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    if (!(ctx->status & NATIVE_N)) {
        goto Lf1a2;
    }
Lf1d1: ;
    return;
    return;
}

void smbneo_native_fn_pos_bits_get_do_x_f15b(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xf15bu);
#ifdef SMBNEO_NATIVE_FAST_PATHS
    if (vs_native_fast_pos_bits_get_do_x(ctx)) return;
#endif
    native_write(ctx, 0x04u, ctx->x);
    ctx->y = native_nz(ctx, 0x01u);
Lf15f: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x071cu + ctx->y)));
    ctx->status |= NATIVE_C;
    native_sbc(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x86u + ctx->x)));
    native_write(ctx, 0x07u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0x071au + ctx->y)));
    native_sbc(ctx, native_read(ctx, (uint16_t)(uint8_t)(0x6du + ctx->x)));
    ctx->x = native_nz(ctx, native_read(ctx, (uint16_t)(0xf158u + ctx->y)));
    native_cmp(ctx, ctx->a, 0x00u);
    if ((ctx->status & NATIVE_N)) {
        goto Lf183;
    }
    ctx->x = native_nz(ctx, native_read(ctx, (uint16_t)(0xf159u + ctx->y)));
    native_cmp(ctx, ctx->a, 0x01u);
    if (!(ctx->status & NATIVE_N)) {
        goto Lf183;
    }
    ctx->a = native_nz(ctx, 0x38u);
    native_write(ctx, 0x06u, ctx->a);
    ctx->a = native_nz(ctx, 0x08u);
    smbneo_native_fn_pos_bits_diff_f1d2(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Lf183: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xf148u + ctx->x)));
    ctx->x = native_nz(ctx, native_read(ctx, 0x04u));
    native_cmp(ctx, ctx->a, 0x00u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf18f;
    }
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y - 1u));
    if (!(ctx->status & NATIVE_N)) {
        goto Lf15f;
    }
Lf18f: ;
    return;
    return;
}

void smbneo_native_fn_pos_bits_diff_f1d2(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xf1d2u);
    native_write(ctx, 0x05u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, 0x07u));
    native_cmp(ctx, ctx->a, native_read(ctx, 0x06u));
    if ((ctx->status & NATIVE_C)) {
        goto Lf1e6;
    }
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x07u));
    native_cmp(ctx, ctx->y, 0x01u);
    if ((ctx->status & NATIVE_C)) {
        goto Lf1e5;
    }
    native_adc(ctx, native_read(ctx, 0x05u));
Lf1e5: ;
    ctx->x = native_nz(ctx, ctx->a);
Lf1e6: ;
    return;
    return;
}

void smbneo_native_fn_apu_cycle_f236(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xf236u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x0770u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf24b;
    }
    native_write(ctx, 0x4015u, ctx->a);
    native_write(ctx, 0xf1u, ctx->a);
    native_write(ctx, 0xf2u, ctx->a);
    native_write(ctx, 0xf3u, ctx->a);
    native_write(ctx, 0x07b1u, ctx->a);
    native_write(ctx, 0xf4u, ctx->a);
    if ((ctx->status & NATIVE_Z)) {
        goto Lf261;
    }
Lf24b: ;
    ctx->a = native_nz(ctx, 0xffu);
    native_write(ctx, 0x4017u, ctx->a);
    ctx->a = native_nz(ctx, 0x0fu);
    native_write(ctx, 0x4015u, ctx->a);
    smbneo_native_fn_apu_sfx_pulse_1_play_f323(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_apu_sfx_pulse_2_play_f484(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_apu_sfx_noise_play_f56f(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_apu_music_play_f59c(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Lf261: ;
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0xfbu, ctx->a);
    native_write(ctx, 0xfcu, ctx->a);
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0xffu, ctx->a);
    native_write(ctx, 0xfeu, ctx->a);
    native_write(ctx, 0xfdu, ctx->a);
    ctx->y = native_nz(ctx, native_read(ctx, 0x07c0u));
    ctx->a = native_nz(ctx, native_read(ctx, 0xf4u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x03u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lf27f;
    }
    native_write(ctx, 0x07c0u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x07c0u) + 1u)));
    native_cmp(ctx, ctx->y, 0x30u);
    if (!(ctx->status & NATIVE_C)) {
        goto Lf285;
    }
Lf27f: ;
    ctx->a = native_nz(ctx, ctx->y);
    if ((ctx->status & NATIVE_Z)) {
        goto Lf285;
    }
    native_write(ctx, 0x07c0u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x07c0u) - 1u)));
Lf285: ;
    native_write(ctx, 0x4011u, ctx->y);
    return;
    return;
}

void smbneo_native_fn_apu_write_base_pulse_1_f289(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xf289u);
    native_write(ctx, 0x4001u, ctx->y);
    native_write(ctx, 0x4000u, ctx->x);
    return;
    return;
}

void smbneo_native_fn_apu_write_pulse_1_f290(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xf290u);
    smbneo_native_fn_apu_write_base_pulse_1_f289(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_apu_write_note_pulse_1_f293(ctx);
    return;
    return;
}

void smbneo_native_fn_apu_write_note_pulse_1_f293(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xf293u);
    ctx->x = native_nz(ctx, 0x00u);
    ctx->y = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xff01u + ctx->y)));
    if ((ctx->status & NATIVE_Z)) {
        goto Lf2a6;
    }
    native_write(ctx, (uint16_t)(0x4002u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xff00u + ctx->y)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | 0x08u));
    native_write(ctx, (uint16_t)(0x4003u + ctx->x), ctx->a);
Lf2a6: ;
    return;
    return;
}

void smbneo_native_fn_apu_write_base_pulse_2_f2a7(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xf2a7u);
    native_write(ctx, 0x4004u, ctx->x);
    native_write(ctx, 0x4005u, ctx->y);
    return;
    return;
}

void smbneo_native_fn_apu_write_pulse_2_f2ae(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xf2aeu);
    smbneo_native_fn_apu_write_base_pulse_2_f2a7(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_apu_write_note_pulse_2_f2b1(ctx);
    return;
    return;
}

void smbneo_native_fn_apu_write_note_pulse_2_f2b1(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xf2b1u);
    goto Lf2b1;
Lf295: ;
    ctx->y = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xff01u + ctx->y)));
    if ((ctx->status & NATIVE_Z)) {
        goto Lf2a6;
    }
    native_write(ctx, (uint16_t)(0x4002u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xff00u + ctx->y)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | 0x08u));
    native_write(ctx, (uint16_t)(0x4003u + ctx->x), ctx->a);
Lf2a6: ;
    return;
Lf2b1: ;
    ctx->x = native_nz(ctx, 0x04u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf295;
    }
    smbneo_native_fn_apu_write_note_triangle_f2b5(ctx);
    return;
    return;
}

void smbneo_native_fn_apu_write_note_triangle_f2b5(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xf2b5u);
    goto Lf2b5;
Lf295: ;
    ctx->y = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xff01u + ctx->y)));
    if ((ctx->status & NATIVE_Z)) {
        goto Lf2a6;
    }
    native_write(ctx, (uint16_t)(0x4002u + ctx->x), ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xff00u + ctx->y)));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | 0x08u));
    native_write(ctx, (uint16_t)(0x4003u + ctx->x), ctx->a);
Lf2a6: ;
    return;
Lf2b5: ;
    ctx->x = native_nz(ctx, 0x08u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf295;
    }
    ctx->unsupported = 1; return;
    return;
}

void smbneo_native_fn_apu_sfx_pulse_1_play_f323(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xf323u);
    goto Lf323;
Lf2c7: ;
    ctx->a = native_nz(ctx, 0x40u);
    native_write(ctx, 0x07bbu, ctx->a);
    ctx->a = native_nz(ctx, 0x62u);
    smbneo_native_fn_apu_write_note_pulse_1_f293(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->x = native_nz(ctx, 0x99u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf2fa;
    }
Lf2d5: ;
    ctx->a = native_nz(ctx, 0x26u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf2db;
    }
Lf2d9: ;
    ctx->a = native_nz(ctx, 0x18u);
Lf2db: ;
    ctx->x = native_nz(ctx, 0x82u);
    ctx->y = native_nz(ctx, 0xa7u);
    smbneo_native_fn_apu_write_pulse_1_f290(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->a = native_nz(ctx, 0x28u);
    native_write(ctx, 0x07bbu, ctx->a);
Lf2e7: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x07bbu));
    native_cmp(ctx, ctx->a, 0x25u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf2f4;
    }
    ctx->x = native_nz(ctx, 0x5fu);
    ctx->y = native_nz(ctx, 0xf6u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf2fc;
    }
Lf2f4: ;
    native_cmp(ctx, ctx->a, 0x20u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf321;
    }
    ctx->x = native_nz(ctx, 0x48u);
Lf2fa: ;
    ctx->y = native_nz(ctx, 0xbcu);
Lf2fc: ;
    smbneo_native_fn_apu_write_base_pulse_1_f289(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf321;
    }
Lf301: ;
    ctx->a = native_nz(ctx, 0x05u);
    ctx->y = native_nz(ctx, 0x99u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf30b;
    }
Lf307: ;
    ctx->a = native_nz(ctx, 0x0au);
    ctx->y = native_nz(ctx, 0x93u);
Lf30b: ;
    ctx->x = native_nz(ctx, 0x9eu);
    native_write(ctx, 0x07bbu, ctx->a);
    ctx->a = native_nz(ctx, 0x0cu);
    smbneo_native_fn_apu_write_pulse_1_f290(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Lf315: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x07bbu));
    native_cmp(ctx, ctx->a, 0x06u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf321;
    }
    ctx->a = native_nz(ctx, 0xbbu);
    native_write(ctx, 0x4001u, ctx->a);
Lf321: ;
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf383;
    }
Lf323: ;
    ctx->y = native_nz(ctx, native_read(ctx, 0xffu));
    if ((ctx->status & NATIVE_Z)) {
        goto Lf347;
    }
    native_write(ctx, 0xf1u, ctx->y);
    if ((ctx->status & NATIVE_N)) {
        goto Lf2d5;
    }
    native_write(ctx, 0xffu, native_lsr(ctx, native_read(ctx, 0xffu)));
    if ((ctx->status & NATIVE_C)) {
        goto Lf2d9;
    }
    native_write(ctx, 0xffu, native_lsr(ctx, native_read(ctx, 0xffu)));
    if ((ctx->status & NATIVE_C)) {
        goto Lf307;
    }
    native_write(ctx, 0xffu, native_lsr(ctx, native_read(ctx, 0xffu)));
    if ((ctx->status & NATIVE_C)) {
        goto Lf363;
    }
    native_write(ctx, 0xffu, native_lsr(ctx, native_read(ctx, 0xffu)));
    if ((ctx->status & NATIVE_C)) {
        goto Lf385;
    }
    native_write(ctx, 0xffu, native_lsr(ctx, native_read(ctx, 0xffu)));
    if ((ctx->status & NATIVE_C)) {
        goto Lf3be;
    }
    native_write(ctx, 0xffu, native_lsr(ctx, native_read(ctx, 0xffu)));
    if ((ctx->status & NATIVE_C)) {
        goto Lf301;
    }
    native_write(ctx, 0xffu, native_lsr(ctx, native_read(ctx, 0xffu)));
    if ((ctx->status & NATIVE_C)) {
        goto Lf2c7;
    }
Lf347: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0xf1u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lf362;
    }
    if ((ctx->status & NATIVE_N)) {
        goto Lf2e7;
    }
    ctx->a = native_lsr(ctx, ctx->a);
    if ((ctx->status & NATIVE_C)) {
        goto Lf2e7;
    }
    ctx->a = native_lsr(ctx, ctx->a);
    if ((ctx->status & NATIVE_C)) {
        goto Lf315;
    }
    ctx->a = native_lsr(ctx, ctx->a);
    if ((ctx->status & NATIVE_C)) {
        goto Lf371;
    }
    ctx->a = native_lsr(ctx, ctx->a);
    if ((ctx->status & NATIVE_C)) {
        goto Lf395;
    }
    ctx->a = native_lsr(ctx, ctx->a);
    if ((ctx->status & NATIVE_C)) {
        goto Lf3c3;
    }
    ctx->a = native_lsr(ctx, ctx->a);
    if ((ctx->status & NATIVE_C)) {
        goto Lf315;
    }
    ctx->a = native_lsr(ctx, ctx->a);
    if ((ctx->status & NATIVE_C)) {
        goto Lf3aa;
    }
Lf362: ;
    return;
Lf363: ;
    ctx->a = native_nz(ctx, 0x0eu);
    native_write(ctx, 0x07bbu, ctx->a);
    ctx->y = native_nz(ctx, 0x9cu);
    ctx->x = native_nz(ctx, 0x9eu);
    ctx->a = native_nz(ctx, 0x26u);
    smbneo_native_fn_apu_write_pulse_1_f290(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Lf371: ;
    ctx->y = native_nz(ctx, native_read(ctx, 0x07bbu));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xf2b8u + ctx->y)));
    native_write(ctx, 0x4000u, ctx->a);
    native_cmp(ctx, ctx->y, 0x06u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf383;
    }
    ctx->a = native_nz(ctx, 0x9eu);
    native_write(ctx, 0x4002u, ctx->a);
Lf383: ;
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf3aa;
    }
Lf385: ;
    ctx->a = native_nz(ctx, 0x0eu);
    ctx->y = native_nz(ctx, 0xcbu);
    ctx->x = native_nz(ctx, 0x9fu);
    native_write(ctx, 0x07bbu, ctx->a);
    ctx->a = native_nz(ctx, 0x28u);
    smbneo_native_fn_apu_write_pulse_1_f290(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf3aa;
    }
Lf395: ;
    ctx->y = native_nz(ctx, native_read(ctx, 0x07bbu));
    native_cmp(ctx, ctx->y, 0x08u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf3a5;
    }
    ctx->a = native_nz(ctx, 0xa0u);
    native_write(ctx, 0x4002u, ctx->a);
    ctx->a = native_nz(ctx, 0x9fu);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf3a7;
    }
Lf3a5: ;
    ctx->a = native_nz(ctx, 0x90u);
Lf3a7: ;
    native_write(ctx, 0x4000u, ctx->a);
Lf3aa: ;
    native_write(ctx, 0x07bbu, native_nz(ctx, (uint8_t)(native_read(ctx, 0x07bbu) - 1u)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf3bd;
    }
    smbneo_native_fn_apu_sfx_pulse_1_stop_f3af(ctx);
    return;
Lf3bd: ;
    return;
Lf3be: ;
    ctx->a = native_nz(ctx, 0x2fu);
    native_write(ctx, 0x07bbu, ctx->a);
Lf3c3: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x07bbu));
    ctx->a = native_lsr(ctx, ctx->a);
    if ((ctx->status & NATIVE_C)) {
        goto Lf3d9;
    }
    ctx->a = native_lsr(ctx, ctx->a);
    if ((ctx->status & NATIVE_C)) {
        goto Lf3d9;
    }
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x02u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lf3d9;
    }
    ctx->y = native_nz(ctx, 0x91u);
    ctx->x = native_nz(ctx, 0x9au);
    ctx->a = native_nz(ctx, 0x44u);
    smbneo_native_fn_apu_write_pulse_1_f290(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Lf3d9: ;
    /* static tail transfer */
    goto Lf3aa;
    return;
}

void smbneo_native_fn_apu_sfx_pulse_1_stop_f3af(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xf3afu);
    ctx->x = native_nz(ctx, 0x00u);
    native_write(ctx, 0xf1u, ctx->x);
    ctx->x = native_nz(ctx, 0x0eu);
    native_write(ctx, 0x4015u, ctx->x);
    ctx->x = native_nz(ctx, 0x0fu);
    native_write(ctx, 0x4015u, ctx->x);
    return;
    return;
}

void smbneo_native_fn_apu_sfx_pulse_2_mute_f479(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xf479u);
    ctx->x = native_nz(ctx, 0x0du);
    native_write(ctx, 0x4015u, ctx->x);
    ctx->x = native_nz(ctx, 0x0fu);
    native_write(ctx, 0x4015u, ctx->x);
    return;
    return;
}

void smbneo_native_fn_apu_sfx_pulse_2_play_f484(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xf484u);
    goto Lf484;
Lf420: ;
    ctx->a = native_nz(ctx, 0x35u);
    ctx->x = native_nz(ctx, 0x8du);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf42a;
    }
Lf426: ;
    ctx->a = native_nz(ctx, 0x06u);
    ctx->x = native_nz(ctx, 0x98u);
Lf42a: ;
    native_write(ctx, 0x07bdu, ctx->a);
    ctx->y = native_nz(ctx, 0x7fu);
    ctx->a = native_nz(ctx, 0x42u);
    smbneo_native_fn_apu_write_pulse_2_f2ae(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Lf434: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x07bdu));
    native_cmp(ctx, ctx->a, 0x30u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf440;
    }
    ctx->a = native_nz(ctx, 0x54u);
    native_write(ctx, 0x4006u, ctx->a);
Lf440: ;
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf470;
    }
Lf442: ;
    ctx->a = native_nz(ctx, 0x20u);
    native_write(ctx, 0x07bdu, ctx->a);
    ctx->y = native_nz(ctx, 0x94u);
    ctx->a = native_nz(ctx, 0x5eu);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf458;
    }
Lf44d: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x07bdu));
    native_cmp(ctx, ctx->a, 0x18u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf470;
    }
    ctx->y = native_nz(ctx, 0x93u);
    ctx->a = native_nz(ctx, 0x18u);
Lf458: ;
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf4d9;
    }
Lf45a: ;
    ctx->a = native_nz(ctx, 0x36u);
    native_write(ctx, 0x07bdu, ctx->a);
Lf45f: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x07bdu));
    ctx->a = native_lsr(ctx, ctx->a);
    if ((ctx->status & NATIVE_C)) {
        goto Lf470;
    }
    ctx->y = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xf3e1u + ctx->y)));
    ctx->x = native_nz(ctx, 0x5du);
    ctx->y = native_nz(ctx, 0x7fu);
Lf46d: ;
    smbneo_native_fn_apu_write_pulse_2_f2ae(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Lf470: ;
    native_write(ctx, 0x07bdu, native_nz(ctx, (uint8_t)(native_read(ctx, 0x07bdu) - 1u)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf483;
    }
Lf475: ;
    ctx->x = native_nz(ctx, 0x00u);
    native_write(ctx, 0xf2u, ctx->x);
    smbneo_native_fn_apu_sfx_pulse_2_mute_f479(ctx);
    return;
Lf483: ;
    return;
Lf484: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0xf2u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x40u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf4ef;
    }
    ctx->y = native_nz(ctx, native_read(ctx, 0xfeu));
    if ((ctx->status & NATIVE_Z)) {
        goto Lf4ae;
    }
    native_write(ctx, 0xf2u, ctx->y);
    if ((ctx->status & NATIVE_N)) {
        goto Lf4d0;
    }
    native_write(ctx, 0xfeu, native_lsr(ctx, native_read(ctx, 0xfeu)));
    if ((ctx->status & NATIVE_C)) {
        goto Lf420;
    }
    native_write(ctx, 0xfeu, native_lsr(ctx, native_read(ctx, 0xfeu)));
    if ((ctx->status & NATIVE_C)) {
        goto Lf504;
    }
    native_write(ctx, 0xfeu, native_lsr(ctx, native_read(ctx, 0xfeu)));
    if ((ctx->status & NATIVE_C)) {
        goto Lf508;
    }
    native_write(ctx, 0xfeu, native_lsr(ctx, native_read(ctx, 0xfeu)));
    if ((ctx->status & NATIVE_C)) {
        goto Lf442;
    }
    native_write(ctx, 0xfeu, native_lsr(ctx, native_read(ctx, 0xfeu)));
    if ((ctx->status & NATIVE_C)) {
        goto Lf426;
    }
    native_write(ctx, 0xfeu, native_lsr(ctx, native_read(ctx, 0xfeu)));
    if ((ctx->status & NATIVE_C)) {
        goto Lf45a;
    }
    native_write(ctx, 0xfeu, native_lsr(ctx, native_read(ctx, 0xfeu)));
    if ((ctx->status & NATIVE_C)) {
        goto Lf4ea;
    }
Lf4ae: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0xf2u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lf4c9;
    }
    if ((ctx->status & NATIVE_N)) {
        goto Lf4db;
    }
    ctx->a = native_lsr(ctx, ctx->a);
    if ((ctx->status & NATIVE_C)) {
        goto Lf4ca;
    }
    ctx->a = native_lsr(ctx, ctx->a);
    if ((ctx->status & NATIVE_C)) {
        goto Lf517;
    }
    ctx->a = native_lsr(ctx, ctx->a);
    if ((ctx->status & NATIVE_C)) {
        goto Lf517;
    }
    ctx->a = native_lsr(ctx, ctx->a);
    if ((ctx->status & NATIVE_C)) {
        goto Lf44d;
    }
    ctx->a = native_lsr(ctx, ctx->a);
    if ((ctx->status & NATIVE_C)) {
        goto Lf4ca;
    }
    ctx->a = native_lsr(ctx, ctx->a);
    if ((ctx->status & NATIVE_C)) {
        goto Lf45f;
    }
    ctx->a = native_lsr(ctx, ctx->a);
    if ((ctx->status & NATIVE_C)) {
        goto Lf4ef;
    }
Lf4c9: ;
    return;
Lf4ca: ;
    /* static tail transfer */
    goto Lf434;
Lf4cd: ;
    /* static tail transfer */
    goto Lf470;
Lf4d0: ;
    ctx->a = native_nz(ctx, 0x38u);
    native_write(ctx, 0x07bdu, ctx->a);
    ctx->y = native_nz(ctx, 0xc4u);
    ctx->a = native_nz(ctx, 0x18u);
Lf4d9: ;
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf4e6;
    }
Lf4db: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x07bdu));
    native_cmp(ctx, ctx->a, 0x08u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf470;
    }
    ctx->y = native_nz(ctx, 0xa4u);
    ctx->a = native_nz(ctx, 0x5au);
Lf4e6: ;
    ctx->x = native_nz(ctx, 0x9fu);
Lf4e8: ;
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf46d;
    }
Lf4ea: ;
    ctx->a = native_nz(ctx, 0x30u);
    native_write(ctx, 0x07bdu, ctx->a);
Lf4ef: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x07bdu));
    ctx->x = native_nz(ctx, 0x03u);
Lf4f4: ;
    ctx->a = native_lsr(ctx, ctx->a);
    if ((ctx->status & NATIVE_C)) {
        goto Lf4cd;
    }
    ctx->x = native_nz(ctx, (uint8_t)(ctx->x - 1u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf4f4;
    }
    ctx->y = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xf3dbu + ctx->y)));
    ctx->x = native_nz(ctx, 0x82u);
    ctx->y = native_nz(ctx, 0x7fu);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf4e8;
    }
Lf504: ;
    ctx->a = native_nz(ctx, 0x10u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf50a;
    }
Lf508: ;
    ctx->a = native_nz(ctx, 0x20u);
Lf50a: ;
    native_write(ctx, 0x07bdu, ctx->a);
    ctx->a = native_nz(ctx, 0x7fu);
    native_write(ctx, 0x4005u, ctx->a);
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0x07beu, ctx->a);
Lf517: ;
    native_write(ctx, 0x07beu, native_nz(ctx, (uint8_t)(native_read(ctx, 0x07beu) + 1u)));
    ctx->a = native_nz(ctx, native_read(ctx, 0x07beu));
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->y = native_nz(ctx, ctx->a);
    native_cmp(ctx, ctx->y, native_read(ctx, 0x07bdu));
    if ((ctx->status & NATIVE_Z)) {
        goto Lf530;
    }
    ctx->a = native_nz(ctx, 0x9du);
    native_write(ctx, 0x4004u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xf400u + ctx->y)));
    smbneo_native_fn_apu_write_note_pulse_2_f2b1(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    return;
Lf530: ;
    /* static tail transfer */
    goto Lf475;
    return;
}

void smbneo_native_fn_apu_sfx_noise_play_f56f(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xf56fu);
    goto Lf56f;
Lf543: ;
    ctx->a = native_nz(ctx, 0x20u);
    native_write(ctx, 0x07bfu, ctx->a);
Lf548: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x07bfu));
    ctx->a = native_lsr(ctx, ctx->a);
    if (!(ctx->status & NATIVE_C)) {
        goto Lf560;
    }
    ctx->y = native_nz(ctx, ctx->a);
    ctx->x = native_nz(ctx, native_read(ctx, (uint16_t)(0xf533u + ctx->y)));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xffe6u + ctx->y)));
Lf555: ;
    native_write(ctx, 0x400cu, ctx->a);
    native_write(ctx, 0x400eu, ctx->x);
    ctx->a = native_nz(ctx, 0x18u);
    native_write(ctx, 0x400fu, ctx->a);
Lf560: ;
    native_write(ctx, 0x07bfu, native_nz(ctx, (uint8_t)(native_read(ctx, 0x07bfu) - 1u)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf56e;
    }
    ctx->a = native_nz(ctx, 0xf0u);
    native_write(ctx, 0x400cu, ctx->a);
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0xf3u, ctx->a);
Lf56e: ;
    return;
Lf56f: ;
    ctx->y = native_nz(ctx, native_read(ctx, 0xfdu));
    if ((ctx->status & NATIVE_Z)) {
        goto Lf57d;
    }
    native_write(ctx, 0xf3u, ctx->y);
    native_write(ctx, 0xfdu, native_lsr(ctx, native_read(ctx, 0xfdu)));
    if ((ctx->status & NATIVE_C)) {
        goto Lf543;
    }
    native_write(ctx, 0xfdu, native_lsr(ctx, native_read(ctx, 0xfdu)));
    if ((ctx->status & NATIVE_C)) {
        goto Lf588;
    }
Lf57d: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0xf3u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lf587;
    }
    ctx->a = native_lsr(ctx, ctx->a);
    if ((ctx->status & NATIVE_C)) {
        goto Lf548;
    }
    ctx->a = native_lsr(ctx, ctx->a);
    if ((ctx->status & NATIVE_C)) {
        goto Lf58d;
    }
Lf587: ;
    return;
Lf588: ;
    ctx->a = native_nz(ctx, 0x40u);
    native_write(ctx, 0x07bfu, ctx->a);
Lf58d: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x07bfu));
    ctx->a = native_lsr(ctx, ctx->a);
    ctx->y = native_nz(ctx, ctx->a);
    ctx->x = native_nz(ctx, 0x0fu);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xffc5u + ctx->y)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf555;
    }
    /* static tail transfer */
    goto Lf678;
Lf5e2: ;
    native_write(ctx, 0x07ccu, native_nz(ctx, (uint8_t)(native_read(ctx, 0x07ccu) + 1u)));
    ctx->y = native_nz(ctx, native_read(ctx, 0x07ccu));
    native_cmp(ctx, ctx->y, 0x37u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf633;
    }
    /* static tail transfer */
    goto Lf69f;
Lf5f8: ;
    native_write(ctx, 0x07c7u, ctx->y);
    ctx->y = native_nz(ctx, 0x36u);
Lf5fd: ;
    native_write(ctx, 0x07c9u, ctx->y);
Lf600: ;
    ctx->y = native_nz(ctx, 0x00u);
    native_write(ctx, 0x07b1u, ctx->y);
    native_write(ctx, 0xf4u, ctx->a);
    native_cmp(ctx, ctx->a, 0x01u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf619;
    }
    native_write(ctx, 0x07c7u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x07c7u) + 1u)));
    ctx->y = native_nz(ctx, native_read(ctx, 0x07c7u));
    native_cmp(ctx, ctx->y, 0x32u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf633;
    }
    ctx->y = native_nz(ctx, 0x11u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf5f8;
    }
Lf619: ;
    native_cmp(ctx, ctx->a, 0x40u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf62b;
    }
    native_write(ctx, 0x07c9u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x07c9u) + 1u)));
    ctx->y = native_nz(ctx, native_read(ctx, 0x07c9u));
    native_cmp(ctx, ctx->y, 0x3bu);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf633;
    }
    ctx->y = native_nz(ctx, 0x36u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf5fd;
    }
Lf62b: ;
    ctx->y = native_nz(ctx, 0x08u);
    native_write(ctx, 0xf7u, ctx->y);
Lf62f: ;
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->a = native_lsr(ctx, ctx->a);
    if (!(ctx->status & NATIVE_C)) {
        goto Lf62f;
    }
Lf633: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xf857u + ctx->y)));
    ctx->y = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xf858u + ctx->y)));
    native_write(ctx, 0xf0u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xf859u + ctx->y)));
    native_write(ctx, 0xf5u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xf85au + ctx->y)));
    native_write(ctx, 0xf6u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xf85bu + ctx->y)));
    native_write(ctx, 0xf9u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xf85cu + ctx->y)));
    native_write(ctx, 0xf8u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xf85du + ctx->y)));
    native_write(ctx, 0x07b0u, ctx->a);
    native_write(ctx, 0x07c1u, ctx->a);
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, 0x07b4u, ctx->a);
    native_write(ctx, 0x07b6u, ctx->a);
    native_write(ctx, 0x07b9u, ctx->a);
    native_write(ctx, 0x07bau, ctx->a);
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0xf7u, ctx->a);
    native_write(ctx, 0x07cau, ctx->a);
    ctx->a = native_nz(ctx, 0x0bu);
    native_write(ctx, 0x4015u, ctx->a);
    ctx->a = native_nz(ctx, 0x0fu);
    native_write(ctx, 0x4015u, ctx->a);
Lf678: ;
    native_write(ctx, 0x07b4u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x07b4u) - 1u)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf6dc;
    }
    ctx->y = native_nz(ctx, native_read(ctx, 0xf7u));
    native_write(ctx, 0xf7u, native_nz(ctx, (uint8_t)(native_read(ctx, 0xf7u) + 1u)));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0xf5u) + ctx->y)));
    if ((ctx->status & NATIVE_Z)) {
        goto Lf689;
    }
    if (!(ctx->status & NATIVE_N)) {
        goto Lf6c4;
    }
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf6b8;
    }
Lf689: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x07b1u));
    native_cmp(ctx, ctx->a, 0x40u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf695;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x07c5u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf6b2;
    }
Lf695: ;
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x04u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf6b5;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0xf4u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x5fu));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf6b2;
    }
Lf69f: ;
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0xf4u, ctx->a);
    native_write(ctx, 0x07b1u, ctx->a);
    native_write(ctx, 0x4008u, ctx->a);
    ctx->a = native_nz(ctx, 0x90u);
    native_write(ctx, 0x4000u, ctx->a);
    native_write(ctx, 0x4004u, ctx->a);
    return;
Lf6b2: ;
    /* static tail transfer */
    goto Lf600;
Lf6b5: ;
    /* static tail transfer */
    goto Lf5e2;
Lf6b8: ;
    smbneo_native_fn_apu_note_length_load_f809(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_write(ctx, 0x07b3u, ctx->a);
    ctx->y = native_nz(ctx, native_read(ctx, 0xf7u));
    native_write(ctx, 0xf7u, native_nz(ctx, (uint8_t)(native_read(ctx, 0xf7u) + 1u)));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0xf5u) + ctx->y)));
Lf6c4: ;
    ctx->x = native_nz(ctx, native_read(ctx, 0xf2u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf6d6;
    }
    smbneo_native_fn_apu_write_note_pulse_2_f2b1(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    if ((ctx->status & NATIVE_Z)) {
        goto Lf6d0;
    }
    smbneo_native_fn_apu_next_state_f816(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Lf6d0: ;
    native_write(ctx, 0x07b5u, ctx->a);
    smbneo_native_fn_apu_write_base_pulse_2_f2a7(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Lf6d6: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x07b3u));
    native_write(ctx, 0x07b4u, ctx->a);
Lf6dc: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0xf2u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf6fa;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x07b1u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x81u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf6fa;
    }
    ctx->y = native_nz(ctx, native_read(ctx, 0x07b5u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lf6ef;
    }
    native_write(ctx, 0x07b5u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x07b5u) - 1u)));
Lf6ef: ;
    smbneo_native_fn_apu_next_volume_f832(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_write(ctx, 0x4004u, ctx->a);
    ctx->x = native_nz(ctx, 0x7fu);
    native_write(ctx, 0x4005u, ctx->x);
Lf6fa: ;
    ctx->y = native_nz(ctx, native_read(ctx, 0xf8u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lf758;
    }
    native_write(ctx, 0x07b6u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x07b6u) - 1u)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf735;
    }
Lf703: ;
    ctx->y = native_nz(ctx, native_read(ctx, 0xf8u));
    native_write(ctx, 0xf8u, native_nz(ctx, (uint8_t)(native_read(ctx, 0xf8u) + 1u)));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0xf5u) + ctx->y)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf71a;
    }
    ctx->a = native_nz(ctx, 0x83u);
    native_write(ctx, 0x4000u, ctx->a);
    ctx->a = native_nz(ctx, 0x94u);
    native_write(ctx, 0x4001u, ctx->a);
    native_write(ctx, 0x07cau, ctx->a);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf703;
    }
Lf71a: ;
    smbneo_native_fn_apu_note_decomp_f803(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_write(ctx, 0x07b6u, ctx->a);
    ctx->y = native_nz(ctx, native_read(ctx, 0xf1u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf758;
    }
    ctx->a = native_nz(ctx, ctx->x);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x3eu));
    smbneo_native_fn_apu_write_note_pulse_1_f293(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    if ((ctx->status & NATIVE_Z)) {
        goto Lf72f;
    }
    smbneo_native_fn_apu_next_state_f816(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Lf72f: ;
    native_write(ctx, 0x07b7u, ctx->a);
    smbneo_native_fn_apu_write_base_pulse_1_f289(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Lf735: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0xf1u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf758;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x07b1u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x81u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf74e;
    }
    ctx->y = native_nz(ctx, native_read(ctx, 0x07b7u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lf748;
    }
    native_write(ctx, 0x07b7u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x07b7u) - 1u)));
Lf748: ;
    smbneo_native_fn_apu_next_volume_f832(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_write(ctx, 0x4000u, ctx->a);
Lf74e: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x07cau));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf755;
    }
    ctx->a = native_nz(ctx, 0x7fu);
Lf755: ;
    native_write(ctx, 0x4001u, ctx->a);
Lf758: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0xf9u));
    native_write(ctx, 0x07b9u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x07b9u) - 1u)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf7ab;
    }
    ctx->y = native_nz(ctx, native_read(ctx, 0xf9u));
    native_write(ctx, 0xf9u, native_nz(ctx, (uint8_t)(native_read(ctx, 0xf9u) + 1u)));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0xf5u) + ctx->y)));
    if ((ctx->status & NATIVE_Z)) {
        goto Lf7a8;
    }
    if (!(ctx->status & NATIVE_N)) {
        goto Lf77c;
    }
    smbneo_native_fn_apu_note_length_load_f809(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_write(ctx, 0x07b8u, ctx->a);
    ctx->a = native_nz(ctx, 0x1fu);
    native_write(ctx, 0x4008u, ctx->a);
    ctx->y = native_nz(ctx, native_read(ctx, 0xf9u));
    native_write(ctx, 0xf9u, native_nz(ctx, (uint8_t)(native_read(ctx, 0xf9u) + 1u)));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0xf5u) + ctx->y)));
    if ((ctx->status & NATIVE_Z)) {
        goto Lf7a8;
    }
Lf77c: ;
    smbneo_native_fn_apu_write_note_triangle_f2b5(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->x = native_nz(ctx, native_read(ctx, 0x07b8u));
    native_write(ctx, 0x07b9u, ctx->x);
    ctx->a = native_nz(ctx, native_read(ctx, 0x07b1u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x7eu));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf792;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0xf4u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x0au));
    if ((ctx->status & NATIVE_Z)) {
        goto Lf7ab;
    }
Lf792: ;
    ctx->a = native_nz(ctx, ctx->x);
    native_cmp(ctx, ctx->a, 0x12u);
    if ((ctx->status & NATIVE_C)) {
        goto Lf7a6;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x07b1u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x08u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lf7a2;
    }
    ctx->a = native_nz(ctx, 0x0fu);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf7a8;
    }
Lf7a2: ;
    ctx->a = native_nz(ctx, 0x1fu);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf7a8;
    }
Lf7a6: ;
    ctx->a = native_nz(ctx, 0xffu);
Lf7a8: ;
    native_write(ctx, 0x4008u, ctx->a);
Lf7ab: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0xf4u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x73u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lf802;
    }
    native_write(ctx, 0x07bau, native_nz(ctx, (uint8_t)(native_read(ctx, 0x07bau) - 1u)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf802;
    }
Lf7b6: ;
    ctx->y = native_nz(ctx, native_read(ctx, 0x07b0u));
    native_write(ctx, 0x07b0u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x07b0u) + 1u)));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0xf5u) + ctx->y)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf7c8;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x07c1u));
    native_write(ctx, 0x07b0u, ctx->a);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf7b6;
    }
Lf7c8: ;
    smbneo_native_fn_apu_note_decomp_f803(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_write(ctx, 0x07bau, ctx->a);
    ctx->a = native_nz(ctx, ctx->x);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x3eu));
    if ((ctx->status & NATIVE_Z)) {
        goto Lf7f7;
    }
    native_cmp(ctx, ctx->a, 0x30u);
    if ((ctx->status & NATIVE_Z)) {
        goto Lf7ef;
    }
    native_cmp(ctx, ctx->a, 0x20u);
    if ((ctx->status & NATIVE_Z)) {
        goto Lf7e7;
    }
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x10u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lf7f7;
    }
    ctx->a = native_nz(ctx, 0x1cu);
    ctx->x = native_nz(ctx, 0x03u);
    ctx->y = native_nz(ctx, 0x18u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf7f9;
    }
Lf7e7: ;
    ctx->a = native_nz(ctx, 0x1cu);
    ctx->x = native_nz(ctx, 0x0cu);
    ctx->y = native_nz(ctx, 0x18u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf7f9;
    }
Lf7ef: ;
    ctx->a = native_nz(ctx, 0x1cu);
    ctx->x = native_nz(ctx, 0x03u);
    ctx->y = native_nz(ctx, 0x58u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf7f9;
    }
Lf7f7: ;
    ctx->a = native_nz(ctx, 0x10u);
Lf7f9: ;
    native_write(ctx, 0x400cu, ctx->a);
    native_write(ctx, 0x400eu, ctx->x);
    native_write(ctx, 0x400fu, ctx->y);
Lf802: ;
    return;
    return;
}

void smbneo_native_fn_apu_music_play_f59c(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xf59cu);
    goto Lf59c;
Lf599: ;
    /* static tail transfer */
    goto Lf678;
Lf59c: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0xfcu));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf5ac;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0xfbu));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf5ef;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x07b1u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a | native_read(ctx, 0xf4u)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf599;
    }
    return;
Lf5ac: ;
    ctx->y = native_nz(ctx, 0x31u);
    native_write(ctx, 0x07ccu, ctx->y);
    native_write(ctx, 0x07b1u, ctx->a);
    native_cmp(ctx, ctx->a, 0x01u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf5c7;
    }
    smbneo_native_fn_apu_sfx_pulse_1_stop_f3af(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    smbneo_native_fn_apu_sfx_pulse_2_mute_f479(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->y = native_nz(ctx, 0x00u);
    native_write(ctx, 0x07c4u, ctx->y);
    native_write(ctx, 0xf4u, ctx->y);
    if ((ctx->status & NATIVE_Z)) {
        goto Lf62f;
    }
Lf5c7: ;
    ctx->x = native_nz(ctx, native_read(ctx, 0xf4u));
    native_write(ctx, 0x07c5u, ctx->x);
    ctx->y = native_nz(ctx, 0x00u);
    native_write(ctx, 0x07c4u, ctx->y);
    native_write(ctx, 0xf4u, ctx->y);
    native_cmp(ctx, ctx->a, 0x40u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf5de;
    }
    ctx->x = native_nz(ctx, 0x08u);
    native_write(ctx, 0x07c4u, ctx->x);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf62f;
    }
Lf5de: ;
    native_cmp(ctx, ctx->a, 0x04u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf62f;
    }
Lf5e2: ;
    native_write(ctx, 0x07ccu, native_nz(ctx, (uint8_t)(native_read(ctx, 0x07ccu) + 1u)));
    ctx->y = native_nz(ctx, native_read(ctx, 0x07ccu));
    native_cmp(ctx, ctx->y, 0x37u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf633;
    }
    /* static tail transfer */
    goto Lf69f;
Lf5ef: ;
    native_cmp(ctx, ctx->a, 0x04u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf5f6;
    }
    smbneo_native_fn_apu_sfx_pulse_1_stop_f3af(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Lf5f6: ;
    ctx->y = native_nz(ctx, 0x10u);
Lf5f8: ;
    native_write(ctx, 0x07c7u, ctx->y);
    ctx->y = native_nz(ctx, 0x36u);
Lf5fd: ;
    native_write(ctx, 0x07c9u, ctx->y);
Lf600: ;
    ctx->y = native_nz(ctx, 0x00u);
    native_write(ctx, 0x07b1u, ctx->y);
    native_write(ctx, 0xf4u, ctx->a);
    native_cmp(ctx, ctx->a, 0x01u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf619;
    }
    native_write(ctx, 0x07c7u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x07c7u) + 1u)));
    ctx->y = native_nz(ctx, native_read(ctx, 0x07c7u));
    native_cmp(ctx, ctx->y, 0x32u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf633;
    }
    ctx->y = native_nz(ctx, 0x11u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf5f8;
    }
Lf619: ;
    native_cmp(ctx, ctx->a, 0x40u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf62b;
    }
    native_write(ctx, 0x07c9u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x07c9u) + 1u)));
    ctx->y = native_nz(ctx, native_read(ctx, 0x07c9u));
    native_cmp(ctx, ctx->y, 0x3bu);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf633;
    }
    ctx->y = native_nz(ctx, 0x36u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf5fd;
    }
Lf62b: ;
    ctx->y = native_nz(ctx, 0x08u);
    native_write(ctx, 0xf7u, ctx->y);
Lf62f: ;
    ctx->y = native_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->a = native_lsr(ctx, ctx->a);
    if (!(ctx->status & NATIVE_C)) {
        goto Lf62f;
    }
Lf633: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xf857u + ctx->y)));
    ctx->y = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xf858u + ctx->y)));
    native_write(ctx, 0xf0u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xf859u + ctx->y)));
    native_write(ctx, 0xf5u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xf85au + ctx->y)));
    native_write(ctx, 0xf6u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xf85bu + ctx->y)));
    native_write(ctx, 0xf9u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xf85cu + ctx->y)));
    native_write(ctx, 0xf8u, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xf85du + ctx->y)));
    native_write(ctx, 0x07b0u, ctx->a);
    native_write(ctx, 0x07c1u, ctx->a);
    ctx->a = native_nz(ctx, 0x01u);
    native_write(ctx, 0x07b4u, ctx->a);
    native_write(ctx, 0x07b6u, ctx->a);
    native_write(ctx, 0x07b9u, ctx->a);
    native_write(ctx, 0x07bau, ctx->a);
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0xf7u, ctx->a);
    native_write(ctx, 0x07cau, ctx->a);
    ctx->a = native_nz(ctx, 0x0bu);
    native_write(ctx, 0x4015u, ctx->a);
    ctx->a = native_nz(ctx, 0x0fu);
    native_write(ctx, 0x4015u, ctx->a);
Lf678: ;
    native_write(ctx, 0x07b4u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x07b4u) - 1u)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf6dc;
    }
    ctx->y = native_nz(ctx, native_read(ctx, 0xf7u));
    native_write(ctx, 0xf7u, native_nz(ctx, (uint8_t)(native_read(ctx, 0xf7u) + 1u)));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0xf5u) + ctx->y)));
    if ((ctx->status & NATIVE_Z)) {
        goto Lf689;
    }
    if (!(ctx->status & NATIVE_N)) {
        goto Lf6c4;
    }
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf6b8;
    }
Lf689: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x07b1u));
    native_cmp(ctx, ctx->a, 0x40u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf695;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x07c5u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf6b2;
    }
Lf695: ;
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x04u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf6b5;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0xf4u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x5fu));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf6b2;
    }
Lf69f: ;
    ctx->a = native_nz(ctx, 0x00u);
    native_write(ctx, 0xf4u, ctx->a);
    native_write(ctx, 0x07b1u, ctx->a);
    native_write(ctx, 0x4008u, ctx->a);
    ctx->a = native_nz(ctx, 0x90u);
    native_write(ctx, 0x4000u, ctx->a);
    native_write(ctx, 0x4004u, ctx->a);
    return;
Lf6b2: ;
    /* static tail transfer */
    goto Lf600;
Lf6b5: ;
    /* static tail transfer */
    goto Lf5e2;
Lf6b8: ;
    smbneo_native_fn_apu_note_length_load_f809(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_write(ctx, 0x07b3u, ctx->a);
    ctx->y = native_nz(ctx, native_read(ctx, 0xf7u));
    native_write(ctx, 0xf7u, native_nz(ctx, (uint8_t)(native_read(ctx, 0xf7u) + 1u)));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0xf5u) + ctx->y)));
Lf6c4: ;
    ctx->x = native_nz(ctx, native_read(ctx, 0xf2u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf6d6;
    }
    smbneo_native_fn_apu_write_note_pulse_2_f2b1(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    if ((ctx->status & NATIVE_Z)) {
        goto Lf6d0;
    }
    smbneo_native_fn_apu_next_state_f816(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Lf6d0: ;
    native_write(ctx, 0x07b5u, ctx->a);
    smbneo_native_fn_apu_write_base_pulse_2_f2a7(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Lf6d6: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x07b3u));
    native_write(ctx, 0x07b4u, ctx->a);
Lf6dc: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0xf2u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf6fa;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x07b1u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x81u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf6fa;
    }
    ctx->y = native_nz(ctx, native_read(ctx, 0x07b5u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lf6ef;
    }
    native_write(ctx, 0x07b5u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x07b5u) - 1u)));
Lf6ef: ;
    smbneo_native_fn_apu_next_volume_f832(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_write(ctx, 0x4004u, ctx->a);
    ctx->x = native_nz(ctx, 0x7fu);
    native_write(ctx, 0x4005u, ctx->x);
Lf6fa: ;
    ctx->y = native_nz(ctx, native_read(ctx, 0xf8u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lf758;
    }
    native_write(ctx, 0x07b6u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x07b6u) - 1u)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf735;
    }
Lf703: ;
    ctx->y = native_nz(ctx, native_read(ctx, 0xf8u));
    native_write(ctx, 0xf8u, native_nz(ctx, (uint8_t)(native_read(ctx, 0xf8u) + 1u)));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0xf5u) + ctx->y)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf71a;
    }
    ctx->a = native_nz(ctx, 0x83u);
    native_write(ctx, 0x4000u, ctx->a);
    ctx->a = native_nz(ctx, 0x94u);
    native_write(ctx, 0x4001u, ctx->a);
    native_write(ctx, 0x07cau, ctx->a);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf703;
    }
Lf71a: ;
    smbneo_native_fn_apu_note_decomp_f803(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_write(ctx, 0x07b6u, ctx->a);
    ctx->y = native_nz(ctx, native_read(ctx, 0xf1u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf758;
    }
    ctx->a = native_nz(ctx, ctx->x);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x3eu));
    smbneo_native_fn_apu_write_note_pulse_1_f293(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    if ((ctx->status & NATIVE_Z)) {
        goto Lf72f;
    }
    smbneo_native_fn_apu_next_state_f816(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Lf72f: ;
    native_write(ctx, 0x07b7u, ctx->a);
    smbneo_native_fn_apu_write_base_pulse_1_f289(ctx);
    if (ctx->suspended || ctx->unsupported) return;
Lf735: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0xf1u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf758;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x07b1u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x81u));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf74e;
    }
    ctx->y = native_nz(ctx, native_read(ctx, 0x07b7u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lf748;
    }
    native_write(ctx, 0x07b7u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x07b7u) - 1u)));
Lf748: ;
    smbneo_native_fn_apu_next_volume_f832(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_write(ctx, 0x4000u, ctx->a);
Lf74e: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0x07cau));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf755;
    }
    ctx->a = native_nz(ctx, 0x7fu);
Lf755: ;
    native_write(ctx, 0x4001u, ctx->a);
Lf758: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0xf9u));
    native_write(ctx, 0x07b9u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x07b9u) - 1u)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf7ab;
    }
    ctx->y = native_nz(ctx, native_read(ctx, 0xf9u));
    native_write(ctx, 0xf9u, native_nz(ctx, (uint8_t)(native_read(ctx, 0xf9u) + 1u)));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0xf5u) + ctx->y)));
    if ((ctx->status & NATIVE_Z)) {
        goto Lf7a8;
    }
    if (!(ctx->status & NATIVE_N)) {
        goto Lf77c;
    }
    smbneo_native_fn_apu_note_length_load_f809(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_write(ctx, 0x07b8u, ctx->a);
    ctx->a = native_nz(ctx, 0x1fu);
    native_write(ctx, 0x4008u, ctx->a);
    ctx->y = native_nz(ctx, native_read(ctx, 0xf9u));
    native_write(ctx, 0xf9u, native_nz(ctx, (uint8_t)(native_read(ctx, 0xf9u) + 1u)));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0xf5u) + ctx->y)));
    if ((ctx->status & NATIVE_Z)) {
        goto Lf7a8;
    }
Lf77c: ;
    smbneo_native_fn_apu_write_note_triangle_f2b5(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    ctx->x = native_nz(ctx, native_read(ctx, 0x07b8u));
    native_write(ctx, 0x07b9u, ctx->x);
    ctx->a = native_nz(ctx, native_read(ctx, 0x07b1u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x7eu));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf792;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0xf4u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x0au));
    if ((ctx->status & NATIVE_Z)) {
        goto Lf7ab;
    }
Lf792: ;
    ctx->a = native_nz(ctx, ctx->x);
    native_cmp(ctx, ctx->a, 0x12u);
    if ((ctx->status & NATIVE_C)) {
        goto Lf7a6;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x07b1u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x08u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lf7a2;
    }
    ctx->a = native_nz(ctx, 0x0fu);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf7a8;
    }
Lf7a2: ;
    ctx->a = native_nz(ctx, 0x1fu);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf7a8;
    }
Lf7a6: ;
    ctx->a = native_nz(ctx, 0xffu);
Lf7a8: ;
    native_write(ctx, 0x4008u, ctx->a);
Lf7ab: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0xf4u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x73u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lf802;
    }
    native_write(ctx, 0x07bau, native_nz(ctx, (uint8_t)(native_read(ctx, 0x07bau) - 1u)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf802;
    }
Lf7b6: ;
    ctx->y = native_nz(ctx, native_read(ctx, 0x07b0u));
    native_write(ctx, 0x07b0u, native_nz(ctx, (uint8_t)(native_read(ctx, 0x07b0u) + 1u)));
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(native_zpword(ctx, 0xf5u) + ctx->y)));
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf7c8;
    }
    ctx->a = native_nz(ctx, native_read(ctx, 0x07c1u));
    native_write(ctx, 0x07b0u, ctx->a);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf7b6;
    }
Lf7c8: ;
    smbneo_native_fn_apu_note_decomp_f803(ctx);
    if (ctx->suspended || ctx->unsupported) return;
    native_write(ctx, 0x07bau, ctx->a);
    ctx->a = native_nz(ctx, ctx->x);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x3eu));
    if ((ctx->status & NATIVE_Z)) {
        goto Lf7f7;
    }
    native_cmp(ctx, ctx->a, 0x30u);
    if ((ctx->status & NATIVE_Z)) {
        goto Lf7ef;
    }
    native_cmp(ctx, ctx->a, 0x20u);
    if ((ctx->status & NATIVE_Z)) {
        goto Lf7e7;
    }
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x10u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lf7f7;
    }
    ctx->a = native_nz(ctx, 0x1cu);
    ctx->x = native_nz(ctx, 0x03u);
    ctx->y = native_nz(ctx, 0x18u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf7f9;
    }
Lf7e7: ;
    ctx->a = native_nz(ctx, 0x1cu);
    ctx->x = native_nz(ctx, 0x0cu);
    ctx->y = native_nz(ctx, 0x18u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf7f9;
    }
Lf7ef: ;
    ctx->a = native_nz(ctx, 0x1cu);
    ctx->x = native_nz(ctx, 0x03u);
    ctx->y = native_nz(ctx, 0x58u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf7f9;
    }
Lf7f7: ;
    ctx->a = native_nz(ctx, 0x10u);
Lf7f9: ;
    native_write(ctx, 0x400cu, ctx->a);
    native_write(ctx, 0x400eu, ctx->x);
    native_write(ctx, 0x400fu, ctx->y);
Lf802: ;
    return;
    return;
}

void smbneo_native_fn_apu_note_decomp_f803(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xf803u);
    ctx->x = native_nz(ctx, ctx->a);
    ctx->a = native_ror(ctx, ctx->a);
    ctx->a = native_nz(ctx, ctx->x);
    ctx->a = native_rol(ctx, ctx->a);
    ctx->a = native_rol(ctx, ctx->a);
    ctx->a = native_rol(ctx, ctx->a);
    smbneo_native_fn_apu_note_length_load_f809(ctx);
    return;
    return;
}

void smbneo_native_fn_apu_note_length_load_f809(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xf809u);
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x07u));
    ctx->status &= (uint8_t)~NATIVE_C;
    native_adc(ctx, native_read(ctx, 0xf0u));
    native_adc(ctx, native_read(ctx, 0x07c4u));
    ctx->y = native_nz(ctx, ctx->a);
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xff66u + ctx->y)));
    return;
    return;
}

void smbneo_native_fn_apu_next_state_f816(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xf816u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x07b1u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x08u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lf821;
    }
    ctx->a = native_nz(ctx, 0x04u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf82d;
    }
Lf821: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0xf4u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x7du));
    if ((ctx->status & NATIVE_Z)) {
        goto Lf82b;
    }
    ctx->a = native_nz(ctx, 0x08u);
    if (!(ctx->status & NATIVE_Z)) {
        goto Lf82d;
    }
Lf82b: ;
    ctx->a = native_nz(ctx, 0x28u);
Lf82d: ;
    ctx->x = native_nz(ctx, 0x82u);
    ctx->y = native_nz(ctx, 0x7fu);
    return;
    return;
}

void smbneo_native_fn_apu_next_volume_f832(SmbNeoNativeContext *ctx) {
    SMBNEO_NATIVE_MARK(ctx, 0xf832u);
    ctx->a = native_nz(ctx, native_read(ctx, 0x07b1u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x08u));
    if ((ctx->status & NATIVE_Z)) {
        goto Lf83d;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xf84bu + ctx->y)));
    return;
Lf83d: ;
    ctx->a = native_nz(ctx, native_read(ctx, 0xf4u));
    ctx->a = native_nz(ctx, (uint8_t)(ctx->a & 0x7du));
    if ((ctx->status & NATIVE_Z)) {
        goto Lf847;
    }
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xf84fu + ctx->y)));
    return;
Lf847: ;
    ctx->a = native_nz(ctx, native_read(ctx, (uint16_t)(0xff9eu + ctx->y)));
    return;
    return;
}

#if defined(__GNUC__) && !defined(__clang__)
#define SMBNEO_NATIVE_LINK_API __attribute__((noinline, used, externally_visible))
#elif defined(__clang__)
#define SMBNEO_NATIVE_LINK_API __attribute__((noinline, used))
#else
#define SMBNEO_NATIVE_LINK_API
#endif
SMBNEO_NATIVE_LINK_API void smbneo_native_boot(SmbNeoNativeContext *ctx) { ctx->a = 0; ctx->x = 0; ctx->y = 0; ctx->status = NATIVE_U | NATIVE_I; ctx->suspended = 0; ctx->unsupported = 0; SMBNEO_NATIVE_RESET_DIAGNOSTICS(ctx);
    smbneo_native_fn_start_8000(ctx);
}
SMBNEO_NATIVE_LINK_API void smbneo_native_frame(SmbNeoNativeContext *ctx) { uint8_t interrupted_status = ctx->status;
    smbneo_native_fn_nmi_810a(ctx);
    ctx->status = (uint8_t)((interrupted_status | NATIVE_U) & (uint8_t)~NATIVE_B);
}
SMBNEO_NATIVE_LINK_API void smbneo_native_title_tick(SmbNeoNativeContext *ctx) {
    smbneo_native_fn_sys_title_8281(ctx);
}
SMBNEO_NATIVE_LINK_API void smbneo_native_game_tick(SmbNeoNativeContext *ctx) {
    smbneo_native_fn_sys_game_ad50(ctx);
}
#undef SMBNEO_NATIVE_LINK_API

/* Saved-value analysis is compile-time only. */
/* max_local_values=2 */