#include "apu.h"
#include "audio_cadence.h"
#include "code.h"
#include "constants.h"
#include "cpu.h"
#include "ppu.h"
#include "replay_checkpoint.h"
#include "replay_gate.h"
#include "replay_timing.h"
#include "smb_replay_data.h"
#include "video.h"

#if defined(SMB_NEOGEO_REPLAY_WINDOW_BENCH)
#include "smb_neogeo_replay_windows.h"
#endif

#include <stdint.h>
#include <string.h>

#ifndef SMB_NEOGEO_REPLAY_TAIL_FRAMES
#define SMB_NEOGEO_REPLAY_TAIL_FRAMES 1800u
#endif

#ifndef SMB_NEOGEO_REPLAY_PROGRESS_FRAMES
#define SMB_NEOGEO_REPLAY_PROGRESS_FRAMES 1800u
#endif

#ifndef SMB_NEOGEO_REPLAY_BOOTSTRAP_FRAMES
#define SMB_NEOGEO_REPLAY_BOOTSTRAP_FRAMES 7u
#endif

#ifndef SMB_NEOGEO_REPLAY_AREA_INIT_HOLD_FRAMES
#define SMB_NEOGEO_REPLAY_AREA_INIT_HOLD_FRAMES 1u
#endif

#ifndef SMB_NEOGEO_REPLAY_STAGE_SETTLE_FRAMES
#define SMB_NEOGEO_REPLAY_STAGE_SETTLE_FRAMES 2u
#endif

#define NEOGEO_REPLAY_STATUS_MAGIC UINT32_C(0x534d4252)
#define NEOGEO_REPLAY_STATUS_VERSION UINT32_C(4)
#define NEOGEO_REPLAY_RESULT_INCOMPLETE UINT32_C(0x100)

#if \
    defined(SMB_NEOGEO_REPLAY_FAST) || \
    (defined(SMB_NEOGEO_REPLAY_WINDOW_BENCH) && \
        defined(SMB_NEOGEO_LOGIC_BENCH))
#define NEOGEO_REPLAY_RENDERING_ENABLED UINT32_C(0)
#else
#define NEOGEO_REPLAY_RENDERING_ENABLED UINT32_C(1)
#endif

#if \
    defined(SMB_NEOGEO_REPLAY_WINDOW_BENCH) && \
    defined(SMB_NEOGEO_REPLAY_FAST)
#error "selective-render replay benchmarking requires rendered replay mode"
#endif

#if SMB_REPLAY_END_FRAME != SMB_REPLAY_FM2_FRAME_COUNT
#error "generated replay frame-count metadata is inconsistent"
#endif

#if \
    SMB_REPLAY_FM2_RAM_INIT_OPTION != 0 && \
    SMB_REPLAY_FM2_RAM_INIT_OPTION != 2
#error "replay gate supports only deterministic FM2 RAM options 0 and 2"
#endif

#if SMB_REPLAY_FM2_INITIAL_COMMAND > 1
#error "replay gate supports only a frame-zero soft reset command"
#endif

_Static_assert(
    SMB_REPLAY_SEGMENT_COUNT > 0,
    "replay must contain at least one input segment"
);
#if defined(SMB_NEOGEO_REPLAY_WINDOW_BENCH)
_Static_assert(
    SMB_NEOGEO_REPLAY_CHECKPOINT_COUNT > 0,
    "window benchmark must contain at least one checkpoint"
);
_Static_assert(
    SMB_NEOGEO_REPLAY_RENDER_RANGE_COUNT > 0,
    "window benchmark must contain at least one render range"
);
#else
_Static_assert(
    SMB_NEOGEO_REPLAY_PROGRESS_FRAMES > 0 &&
        SMB_NEOGEO_REPLAY_PROGRESS_FRAMES <= UINT16_MAX,
    "replay progress interval must fit in uint16_t"
);
#endif
_Static_assert(
    SMB_NEOGEO_REPLAY_BOOTSTRAP_FRAMES <= SMB_REPLAY_END_FRAME,
    "replay bootstrap interval must fit inside the input movie"
);
_Static_assert(
    (int64_t)SMB_NEOGEO_REPLAY_AREA_INIT_HOLD_FRAMES >= 0 &&
        (uint64_t)SMB_NEOGEO_REPLAY_AREA_INIT_HOLD_FRAMES <= UINT8_MAX,
    "area-initialization replay hold must fit in uint8_t"
);
#if defined(SMB_NEOGEO_REPLAY_FAST)
_Static_assert(
    (int64_t)SMB_NEOGEO_REPLAY_STAGE_SETTLE_FRAMES >= 0 &&
        (uint64_t)SMB_NEOGEO_REPLAY_STAGE_SETTLE_FRAMES <= UINT8_MAX,
    "stage screenshot settle interval must fit in uint8_t"
);
#else
_Static_assert(
    SMB_NEOGEO_REPLAY_STAGE_SETTLE_FRAMES == 2,
    "rendered replay evidence requires exactly two settling frames"
);
#endif

typedef struct NeogeoReplayStatus {
    uint32_t magic;
    uint32_t version;
    uint32_t result;
    uint32_t frame;
    uint32_t tail_frame;
    uint32_t segment_index;
    uint32_t controller_state;
    uint32_t entered_mask;
    uint32_t completed_mask;
    uint32_t current_stage;
    uint32_t victory_stable_frames;
    uint32_t oper_mode;
    uint32_t oper_mode_task;
    uint32_t world;
    uint32_t level;
    uint32_t world_end_timer;
    uint32_t replay_end_frame;
    uint32_t hardware_playable;
    uint32_t opposite_direction_transitions;
    uint32_t ram_init_option;
    uint32_t bootstrap_frames;
    uint32_t area_init_hold_frames;
    uint32_t area_init_hold_count;
    uint32_t core_frames_advanced;
    uint32_t rendering_enabled;
    uint32_t game_frame_count;
    uint32_t vblank_count;
    uint32_t stage_settle_frames;
    uint32_t render_generation;
    uint32_t presented_generation;
} NeogeoReplayStatus;

volatile NeogeoReplayStatus neogeo_replay_status
    __attribute__((aligned(4), used, externally_visible));

/*
 * ngdevkit clears BSS in one final 32-byte block. Keep replay-only
 * initialized data large enough for startup to restore every over-cleared
 * byte before main executes, independent of the replay mode or LTO symbol
 * ordering.
 */
volatile uint8_t neogeo_replay_startup_guard[32]
    __attribute__((used, externally_visible)) = {1u};

static NeogeoReplayTiming replay_timing;
static uint32_t replay_core_frames_advanced;
#if !defined(SMB_NEOGEO_REPLAY_FAST)
static uint16_t replay_audio_vblank;
#if defined(SMB_NEOGEO_REPLAY_WINDOW_BENCH)
static uint8_t replay_benchmark_rendering;
#endif
#endif

#if defined(SMB_NEOGEO_REPLAY_WINDOW_BENCH)
static uint8_t benchmark_should_render(uint32_t frame) {
    uint32_t index;

    for (
        index = 0;
        index < SMB_NEOGEO_REPLAY_RENDER_RANGE_COUNT;
        ++index
    ) {
        if (
            frame >= smb_neogeo_replay_render_ranges[index][0] &&
            frame <= smb_neogeo_replay_render_ranges[index][1]
        ) {
            return 1u;
        }
    }
    return 0u;
}
#endif

#if !defined(SMB_NEOGEO_REPLAY_FAST)
static void present_rendered_checkpoint(void) {
    uint32_t committed_result = neogeo_replay_status.result;

    neogeo_replay_checkpoint_present(
        neogeo_video_wait_for_present,
        &neogeo_replay_status.vblank_count,
        &neogeo_vblank_count,
        &neogeo_replay_status.render_generation,
        &neogeo_render_generation,
        &neogeo_replay_status.presented_generation,
        &neogeo_presented_generation
    );
    /*
     * The refreshed presentation counters belong to the same frozen status
     * snapshot. Recommit result last before the debugger trap.
     */
    neogeo_replay_status.result = committed_result;
}
#else
#define present_rendered_checkpoint() ((void)0)
#endif

static void initialize_power_on_ram(void) {
    uint16_t address;

#if SMB_REPLAY_FM2_RAM_INIT_OPTION == 0
    /*
     * FCEUX's legacy deterministic power-on pattern repeats four zero bytes
     * followed by four $ff bytes. The game then performs its normal cold-boot
     * clearing, preserving only the locations the original reset path skips.
     */
    for (address = 0; address < RAM_SIZE; ++address) {
        ram[address] = (address & 7u) < 4u ? 0u : 0xffu;
    }
#else
    memset(ram, 0, RAM_SIZE);
#endif
}

static NeogeoReplaySnapshot snapshot_core(void) {
    NeogeoReplaySnapshot snapshot;

    snapshot.oper_mode = ram[OperMode];
    snapshot.oper_mode_task = ram[OperMode_Task];
    snapshot.world = ram[WorldNumber];
    snapshot.level = ram[LevelNumber];
    snapshot.world_end_timer = ram[WorldEndTimer];
    return snapshot;
}

static void publish_status(
    const NeogeoReplayGate *gate,
    const NeogeoReplaySnapshot *snapshot,
    uint32_t result,
    uint32_t frame,
    uint32_t tail_frame,
    uint32_t segment_index,
    uint8_t controller_state
) {
    /*
     * The debugger stops the CPU before reading this mailbox. Result is
     * written last so a non-running value never describes older fields.
     */
    neogeo_replay_status.magic = NEOGEO_REPLAY_STATUS_MAGIC;
    neogeo_replay_status.version = NEOGEO_REPLAY_STATUS_VERSION;
    neogeo_replay_status.frame = frame;
    neogeo_replay_status.tail_frame = tail_frame;
    neogeo_replay_status.segment_index = segment_index;
    neogeo_replay_status.controller_state = controller_state;
    neogeo_replay_status.entered_mask = gate->entered_mask;
    neogeo_replay_status.completed_mask = gate->completed_mask;
    neogeo_replay_status.current_stage = gate->current_stage;
    neogeo_replay_status.victory_stable_frames =
        gate->victory_stable_frames;
    neogeo_replay_status.oper_mode = snapshot->oper_mode;
    neogeo_replay_status.oper_mode_task = snapshot->oper_mode_task;
    neogeo_replay_status.world = snapshot->world;
    neogeo_replay_status.level = snapshot->level;
    neogeo_replay_status.world_end_timer = snapshot->world_end_timer;
    neogeo_replay_status.replay_end_frame = SMB_REPLAY_END_FRAME;
    neogeo_replay_status.hardware_playable = SMB_REPLAY_HARDWARE_PLAYABLE;
    neogeo_replay_status.opposite_direction_transitions =
        SMB_REPLAY_OPPOSITE_DIRECTION_TRANSITIONS;
    neogeo_replay_status.ram_init_option =
        SMB_REPLAY_FM2_RAM_INIT_OPTION;
    neogeo_replay_status.bootstrap_frames =
        SMB_NEOGEO_REPLAY_BOOTSTRAP_FRAMES;
    neogeo_replay_status.area_init_hold_frames =
        SMB_NEOGEO_REPLAY_AREA_INIT_HOLD_FRAMES;
    neogeo_replay_status.area_init_hold_count =
        replay_timing.area_init_hold_count;
    neogeo_replay_status.core_frames_advanced =
        replay_core_frames_advanced;
    neogeo_replay_status.rendering_enabled =
        NEOGEO_REPLAY_RENDERING_ENABLED;
    neogeo_replay_status.game_frame_count =
        neogeo_game_frame_count;
    neogeo_replay_status.vblank_count =
        neogeo_vblank_count;
    neogeo_replay_status.stage_settle_frames =
        SMB_NEOGEO_REPLAY_STAGE_SETTLE_FRAMES;
    neogeo_replay_status.render_generation =
        neogeo_render_generation;
    neogeo_replay_status.presented_generation =
        neogeo_presented_generation;
    neogeo_replay_status.result = result;
}

static NeogeoReplayGateResult run_replay_frame(
    NeogeoReplayGate *gate,
    uint8_t controller_state,
    uint32_t frame,
    uint32_t tail_frame,
    uint32_t segment_index
) {
    NeogeoReplaySnapshot snapshot;
    NeogeoReplayGateResult result;
#if !defined(SMB_NEOGEO_REPLAY_FAST)
#if defined(SMB_NEOGEO_REPLAY_WINDOW_BENCH)
    uint8_t render_frame = benchmark_should_render(frame);
#endif
    uint16_t game_frame_vblank;

#if defined(SMB_NEOGEO_REPLAY_WINDOW_BENCH)
    if (render_frame != 0u) {
        if (replay_benchmark_rendering == 0u) {
            neogeo_video_benchmark_invalidate();
            /*
             * The fast-forwarded prefix deliberately skips native bridge
             * work. Reset it before the warmup so stale audio transport state
             * cannot reach a measured frame.
             */
            apu_init(0);
            replay_audio_vblank = neogeo_video_current_vblank();
            replay_benchmark_rendering = 1u;
        }
        replay_audio_vblank = neogeo_audio_prepare_game_frame(
            replay_audio_vblank,
            &game_frame_vblank
        );
    } else {
        replay_benchmark_rendering = 0u;
    }
#else
    replay_audio_vblank = neogeo_audio_prepare_game_frame(
        replay_audio_vblank,
        &game_frame_vblank
    );
#endif
#endif

    update_controller1(controller_state);
    next_frame();
    ++replay_core_frames_advanced;
#if !defined(SMB_NEOGEO_REPLAY_FAST)
#if defined(SMB_NEOGEO_REPLAY_WINDOW_BENCH)
    if (render_frame != 0u) {
        apu_step_frame();
        replay_audio_vblank = game_frame_vblank;
        ppu_render();
    }
#else
    apu_step_frame();
    replay_audio_vblank = game_frame_vblank;
    ppu_render();
#endif
#endif
    snapshot = snapshot_core();
    result = neogeo_replay_gate_update(gate, &snapshot);
#if \
    defined(SMB_NEOGEO_REPLAY_WINDOW_BENCH) && \
    !defined(SMB_NEOGEO_REPLAY_FAST)
    if (render_frame == 0u) {
        /* Keep distant fast-forward prefixes free of mailbox-store overhead. */
        return result;
    }
#endif
    publish_status(
        gate,
        &snapshot,
        (uint32_t)result,
        frame,
        tail_frame,
        segment_index,
        controller_state
    );
    return result;
}

static NeogeoReplayGateResult run_replay_input_frame(
    NeogeoReplayGate *gate,
    uint8_t controller_state,
    uint32_t frame,
    uint32_t segment_index
) {
    NeogeoReplaySnapshot snapshot;

    /*
     * The translated Start() routine executes synchronously, but an FM2
     * power-on/reset spends its first video frames inside that routine before
     * the first NMI. Preserve those source-frame inputs without advancing the
     * game core, then begin one NMI per movie record at the measured boundary.
     */
    if (
        !neogeo_replay_timing_should_advance(
            &replay_timing,
            frame,
            ram[OperMode],
            ram[OperMode_Task],
            SMB_NEOGEO_REPLAY_BOOTSTRAP_FRAMES,
            SMB_NEOGEO_REPLAY_AREA_INIT_HOLD_FRAMES
        )
    ) {
        update_controller1(controller_state);
        snapshot = snapshot_core();
        publish_status(
            gate,
            &snapshot,
            NEOGEO_REPLAY_GATE_RUNNING,
            frame,
            0,
            segment_index,
            controller_state
        );
        return NEOGEO_REPLAY_GATE_RUNNING;
    }

    return run_replay_frame(
        gate,
        controller_state,
        frame,
        0,
        segment_index
    );
}

void neogeo_replay_pass_trap(void)
    __attribute__((noinline, noreturn, used, externally_visible));
void neogeo_replay_fail_trap(void)
    __attribute__((noinline, noreturn, used, externally_visible));
void neogeo_replay_progress_trap(void)
    __attribute__((noinline, used, externally_visible));
void neogeo_replay_stage_trap(void)
    __attribute__((noinline, used, externally_visible));
void neogeo_replay_transition_trap(void)
    __attribute__((noinline, used, externally_visible));

void neogeo_replay_pass_trap(void) {
    for (;;) {
        __asm__ volatile ("nop");
    }
}

void neogeo_replay_fail_trap(void) {
    for (;;) {
        __asm__ volatile ("nop");
    }
}

void neogeo_replay_progress_trap(void) {
    __asm__ volatile ("nop");
}

void neogeo_replay_stage_trap(void) {
    __asm__ volatile ("nop");
}

void neogeo_replay_transition_trap(void) {
    __asm__ volatile ("nop");
}

#if defined(SMB_NEOGEO_REPLAY_WINDOW_BENCH)
static void update_progress_checkpoint(
    uint32_t *remaining,
    uint32_t *interval_index
) {
    --*remaining;
    if (*remaining == 0u) {
        neogeo_replay_progress_trap();
        ++*interval_index;
        if (*interval_index < SMB_NEOGEO_REPLAY_CHECKPOINT_COUNT) {
            *remaining =
                smb_neogeo_replay_progress_intervals[*interval_index];
        } else {
            *remaining = UINT32_MAX;
        }
    }
}
#else
static void update_progress_checkpoint(uint16_t *remaining) {
    --*remaining;
    if (*remaining == 0u) {
        neogeo_replay_progress_trap();
        *remaining = SMB_NEOGEO_REPLAY_PROGRESS_FRAMES;
    }
}
#endif

#if !defined(SMB_NEOGEO_REPLAY_FAST)
static void update_stage_checkpoint(
    NeogeoReplayCheckpoint *checkpoint,
    uint32_t entered_mask
) {
    uint8_t stage_changed = (uint8_t)(
        entered_mask != checkpoint->observed_entered_mask
    );

    if (
        neogeo_replay_checkpoint_stage_ready(
            checkpoint,
            entered_mask,
            SMB_NEOGEO_REPLAY_STAGE_SETTLE_FRAMES
        )
    ) {
        present_rendered_checkpoint();
        neogeo_replay_stage_trap();
    }
    if (stage_changed != 0u) {
        present_rendered_checkpoint();
        neogeo_replay_transition_trap();
    }
}
#endif

int main(void) {
    NeogeoReplayGate gate;
    NeogeoReplaySnapshot snapshot;
    uint32_t frame = 0;
    uint32_t tail_frame = 0;
    uint32_t segment_index = 0;
    uint16_t segment_remaining;
#if defined(SMB_NEOGEO_REPLAY_WINDOW_BENCH)
    uint32_t progress_interval_index = 0;
    uint32_t progress_remaining =
        smb_neogeo_replay_progress_intervals[0];
#else
    uint16_t progress_remaining = SMB_NEOGEO_REPLAY_PROGRESS_FRAMES;
#endif
#if !defined(SMB_NEOGEO_REPLAY_FAST)
    NeogeoReplayCheckpoint stage_checkpoint;
#endif
    uint8_t controller_state;

    neogeo_video_init();
    cpu_init();
    initialize_power_on_ram();
    apu_init(0);
    ppu_init(0);
    Start();
#if !defined(SMB_NEOGEO_REPLAY_FAST)
    replay_audio_vblank = neogeo_video_current_vblank();
#endif
    neogeo_replay_gate_init(&gate);
#if !defined(SMB_NEOGEO_REPLAY_FAST)
    neogeo_replay_checkpoint_init(
        &stage_checkpoint,
        gate.entered_mask
    );
#endif

    segment_remaining = smb_replay_durations[0];
    controller_state = smb_replay_states[0];
    snapshot = snapshot_core();
    publish_status(
        &gate,
        &snapshot,
        NEOGEO_REPLAY_GATE_RUNNING,
        0,
        0,
        0,
        controller_state
    );

    while (frame < SMB_REPLAY_END_FRAME) {
        NeogeoReplayGateResult result = run_replay_input_frame(
            &gate,
            controller_state,
            frame,
            segment_index
        );

        ++frame;
#if defined(SMB_NEOGEO_REPLAY_WINDOW_BENCH)
        update_progress_checkpoint(
            &progress_remaining,
            &progress_interval_index
        );
#else
        update_progress_checkpoint(&progress_remaining);
#endif
#if !defined(SMB_NEOGEO_REPLAY_FAST)
        update_stage_checkpoint(
            &stage_checkpoint,
            gate.entered_mask
        );
#endif
        if (neogeo_replay_gate_failed(&gate) != 0u) {
            present_rendered_checkpoint();
            neogeo_replay_fail_trap();
        }

        --segment_remaining;
        if (segment_remaining == 0u) {
            ++segment_index;
            if (segment_index < SMB_REPLAY_SEGMENT_COUNT) {
                segment_remaining =
                    smb_replay_durations[segment_index];
                controller_state =
                    smb_replay_states[segment_index];
            }
        }

        if (result == NEOGEO_REPLAY_GATE_COMPLETE) {
            present_rendered_checkpoint();
            neogeo_replay_pass_trap();
        }
    }

    controller_state = 0;
    while (
        tail_frame < SMB_NEOGEO_REPLAY_TAIL_FRAMES &&
        neogeo_replay_gate_passed(&gate) == 0u
    ) {
        NeogeoReplayGateResult result = run_replay_frame(
            &gate,
            controller_state,
            frame,
            tail_frame,
            segment_index
        );

        ++frame;
        ++tail_frame;
#if defined(SMB_NEOGEO_REPLAY_WINDOW_BENCH)
        update_progress_checkpoint(
            &progress_remaining,
            &progress_interval_index
        );
#else
        update_progress_checkpoint(&progress_remaining);
#endif
#if !defined(SMB_NEOGEO_REPLAY_FAST)
        update_stage_checkpoint(
            &stage_checkpoint,
            gate.entered_mask
        );
#endif
        if (neogeo_replay_gate_failed(&gate) != 0u) {
            present_rendered_checkpoint();
            neogeo_replay_fail_trap();
        }
        if (result == NEOGEO_REPLAY_GATE_COMPLETE) {
            present_rendered_checkpoint();
            neogeo_replay_pass_trap();
        }
    }

    snapshot = snapshot_core();
    publish_status(
        &gate,
        &snapshot,
        NEOGEO_REPLAY_RESULT_INCOMPLETE,
        frame,
        tail_frame,
        segment_index,
        controller_state
    );
    present_rendered_checkpoint();
    neogeo_replay_fail_trap();
}
