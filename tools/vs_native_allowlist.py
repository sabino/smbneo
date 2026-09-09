"""Deterministic hot VS routines suitable for native C lowering.

This is deliberately an address list, not a runtime profile threshold.  The
addresses are 6502 code addresses from the verified VS debug database.  The
translator should union this set with independently verified semantic fast
paths and their targets.
"""

from typing import Final


# System scheduling, input, and frame-level orchestration.
SYSTEM_ROUTINES: Final = frozenset({
    0x820C, 0x8212, 0x825E, 0x8273, 0x9113, 0x919E, 0x9389,
    0x91A5, 0x91C9, 0x9217, 0x92E1, 0xAD50, 0xAD6E,
    0x977E, 0xAE1C, 0xAE40, 0xAF96,
})

# Player movement and state transitions.
PLAYER_ROUTINES: Final = frozenset({
    0xAEF7,  # player_proc (source label)
    0xAEE5, 0xB116, 0xB135, 0xB147, 0xB1D6, 0xB207, 0xB21A,
    0xB223, 0xB27C, 0xB5FC, 0xB709, 0xBEEA,
})

# Actor scheduling, projectiles, and high-frequency object behavior.
ACTOR_ROUTINES: Final = frozenset({
    0xB4D1, 0xB66C, 0xB8B0, 0xBA8A, 0xBF60, 0xC823, 0xC848,
    0xC160, 0xC1B5, 0xC23A, 0xC257, 0xC8DB, 0xC9BA,
})

# Collision and box tests dominate crowded scenes; keep their shared helpers.
COLLISION_ROUTINES: Final = frozenset({
    0xD7AA, 0xD98A, 0xDB9A, 0xDBAB, 0xDBBD, 0xDE16, 0xDE1D,
    0xDE36, 0xDE41, 0xDEA1, 0xDEF0, 0xDEF7, 0xDF06, 0xDF17, 0xE199,
    0xE1F2, 0xE27B, 0xE27D, 0xE33E, 0xE33F, 0xE344, 0xE348,
})

# Renderer, PPU display-list, and position helpers.
RENDER_ROUTINES: Final = frozenset({
    0x86A4, 0x8B6B, 0x8BE2, 0x9195, 0xE51E, 0xE7DA, 0xEB1E, 0xEBA7,
    0xEE46, 0xEE99, 0xEF23, 0xEF51, 0xEFC7, 0xEFF6, 0xF04E,
    0xF08F, 0xF0D6, 0xF0E5, 0xF114, 0xF13C,
})

# Course/scenery decoding and score/status updates.
WORLD_ROUTINES: Final = frozenset({
    0x93A6, 0x9256, 0x924F, 0x9324, 0x9362, 0xA18B, 0xA1A3,
    0xA1B6, 0xA2D7, 0xA3E3, 0xA487, 0xA493, 0xA80B, 0xA85F,
    0xA95A, 0xAADA, 0xAADD, 0xAAE9, 0xAB14, 0xBDE3, 0xBD7F,
})

# APU scheduling is part of the per-tick hot path and keeps audio cadence out
# of the page dispatcher.  Sound data tables themselves are not entries.
AUDIO_ROUTINES: Final = frozenset({
    0xF236, 0xF289, 0xF290, 0xF293, 0xF2A7, 0xF2B1, 0xF2B5,
    0xF323, 0xF3AF, 0xF479, 0xF484, 0xF56F, 0xF59C, 0xF803,
    0xF809, 0xF816, 0xF832,
})


NATIVE_ROUTINE_ALLOWLIST: Final[frozenset[int]] = frozenset().union(
    SYSTEM_ROUTINES, PLAYER_ROUTINES, ACTOR_ROUTINES, COLLISION_ROUTINES,
    RENDER_ROUTINES, WORLD_ROUTINES, AUDIO_ROUTINES,
)

# Human-readable names are kept alongside addresses for review and diagnostics.
NATIVE_ROUTINE_NAMES: Final[dict[int, str]] = {
    0x820C: "vs_setbank_low", 0x8212: "nmi_oam_shuffle",
    0x825E: "sys_enter",
    0x8273: "oam_init_all", 0x9113: "joypad_read", 0xAD50: "sys_game",
    0x919E: "ppu_displist_write_scc_h_v", 0x91A5: "ppu_displist_write_ctlr0",
    0x91C9: "stats_print_num_do", 0x9217: "stats_calc",
    0x92E1: "vs_text_free_play_end", 0x9389: "sub_9389",
    0xAD6E: "game_proc", 0xAE1C: "game_proc_scenery_scroll",
    0xAE40: "game_scroll", 0xAF96: "player_proc_ctrl",
    0x977E: "game_init_area_music",
    0xAEF7: "player_proc", 0xAEE5: "game_screen_pos_x_calc",
    0xB116: "player_proc_death",
    0xB135: "player_proc_firefl_do", 0xB147: "player_proc_firefl_do_2",
    0xB1D6: "player_proc_player_move", 0xB207: "player_proc_player_ground",
    0xB21A: "player_proc_player_fall", 0xB223: "player_proc_player_jump_swim",
    0xB27C: "player_proc_player_climb", 0xB5FC: "stats_time_proc",
    0xB709: "pole_proc", 0xBEEA: "motion_gravity",
    0xB4D1: "proj_proc", 0xB66C: "whirlpool_proc",
    0xB8B0: "cannon_proc", 0xBA8A: "misc_proc",
    0xBF60: "actor_proc", 0xC823: "actor_proc_enemy",
    0xC160: "actor_init", 0xC1B5: "actor_init_do",
    0xC23A: "actor_init_goomba", 0xC257: "actor_init_base",
    0xC848: "actor_proc_enemy_do", 0xC8DB: "actor_erase",
    0xC9BA: "actor_A_00_proc",
    0xD7AA: "col_player_actor_proc", 0xD98A: "col_actor_actor_proc",
    0xDB9A: "col_check_player_pos_y", 0xDBAB: "col_actor_box_get",
    0xDBBD: "col_bg_proc",
    0xDE16: "col_bg_proc_vis", 0xDE1D: "col_bg_proc_spring",
    0xDE36: "col_bg_proc_spring_check", 0xDE41: "col_bg_proc_pipe_v_do",
    0xDEA1: "col_bg_proc_player_halt",
    0xDEF0: "col_bg_proc_climb_check", 0xDEF7: "col_bg_proc_coin_check",
    0xDF06: "col_bg_proc_get_tile_page", 0xDF17: "col_actor_ground_proc",
    0xE199: "col_actor_box", 0xE1F2: "col_box_proc",
    0xE27B: "col_base_player", 0xE27D: "col_base_actor",
    0xE33E: "col_box_player_step", 0xE33F: "col_box_player_jump",
    0xE344: "col_box_player_back", 0xE348: "col_box_buffer",
    0x86A4: "bg_score_disp", 0x8B6B: "meta_render_attr",
    0x8BE2: "meta_color_rotate",
    0x9195: "ppu_displist_write", 0xE51E: "render_set_two_sprites_v",
    0xE7DA: "render_actor", 0xEB1E: "render_actor_clear_chr_pair_h",
    0xEBA7: "render_offscr_top_clear",
    0xEE46: "render_player", 0xEE99: "render_player_normal",
    0xEF23: "render_player_tiles_init", 0xEF51: "render_player_action",
    0xEFC7: "render_player_fall", 0xEFF6: "render_player_get_size_offset",
    0xF04E: "render_player_tiles_mirror", 0xF08F: "pos_calc_x_rel_player",
    0xF0D6: "pos_calc_x_rel_do", 0xF0E5: "pos_bits_get_player",
    0xF114: "pos_bits_get_actor", 0xF13C: "pos_bits_proc",
    0x93A6: "sub_9389_do", 0x9256: "stats_score_hi_check_do",
    0x924F: "stats_score_hi_check", 0x9324: "sub_9324",
    0x9362: "sub_9362", 0xA18B: "scenery_proc",
    0xA1A3: "scenery_proc_do", 0xA1B6: "scenery_inc_seam",
    0xA2D7: "scenery_proc_main", 0xA3E3: "scenery_do",
    0xA487: "scenery_offset_base_inc", 0xA493: "scenery_decode",
    0xA80B: "scenery_pipe_vert", 0xA85F: "scenery_pipe_h",
    0xA95A: "scenery_brickrow", 0xAADA: "scenery_len",
    0xAADD: "scenery_len_b", 0xAAE9: "scenery_attr",
    0xAB14: "scenery_get_buffer_ptr",
    0xBDE3: "block_flatten", 0xBD7F: "block_proc",
    0xF236: "apu_cycle", 0xF289: "apu_write_base_pulse_1",
    0xF290: "apu_write_pulse_1", 0xF293: "apu_write_note_pulse_1",
    0xF2A7: "apu_write_base_pulse_2",
    0xF2B1: "apu_write_note_pulse_2", 0xF2B5: "apu_write_note_triangle",
    0xF323: "apu_sfx_pulse_1_play", 0xF3AF: "apu_sfx_pulse_1_stop",
    0xF479: "apu_sfx_pulse_2_mute",
    0xF484: "apu_sfx_pulse_2_play", 0xF56F: "apu_sfx_noise_play",
    0xF59C: "apu_music_play", 0xF803: "apu_note_decomp",
    0xF809: "apu_note_length_load", 0xF816: "apu_next_state",
    0xF832: "apu_next_volume",
}

if set(NATIVE_ROUTINE_NAMES) != set(NATIVE_ROUTINE_ALLOWLIST):
    raise RuntimeError("VS native allowlist names and addresses disagree")
