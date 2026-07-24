#include "apu.h"
#include "code.h"
#include "constants.h"
#include "cpu.h"
#include "ppu.h"
#include "replay_gate.h"
#include "smb_replay_data.h"
#include "video.h"

#include <stdint.h>
#include <string.h>

#ifndef SMB_NEOGEO_REPLAY_TAIL_FRAMES
#define SMB_NEOGEO_REPLAY_TAIL_FRAMES 1800u
#endif

#ifndef SMB_NEOGEO_REPLAY_PROGRESS_FRAMES
#define SMB_NEOGEO_REPLAY_PROGRESS_FRAMES 1800u
#endif

#define NEOGEO_REPLAY_STATUS_MAGIC UINT32_C(0x534d4252)
#define NEOGEO_REPLAY_STATUS_VERSION UINT32_C(1)
#define NEOGEO_REPLAY_RESULT_INCOMPLETE UINT32_C(0x100)

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
_Static_assert(
    SMB_NEOGEO_REPLAY_PROGRESS_FRAMES > 0 &&
        SMB_NEOGEO_REPLAY_PROGRESS_FRAMES <= UINT16_MAX,
    "replay progress interval must fit in uint16_t"
);

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
} NeogeoReplayStatus;

volatile NeogeoReplayStatus neogeo_replay_status
    __attribute__((aligned(4), used, externally_visible));

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

    update_controller1(controller_state);
    next_frame();
#if !defined(SMB_NEOGEO_REPLAY_FAST)
    ppu_render();
#endif
    snapshot = snapshot_core();
    result = neogeo_replay_gate_update(gate, &snapshot);
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

void neogeo_replay_pass_trap(void)
    __attribute__((noinline, noreturn, used, externally_visible));
void neogeo_replay_fail_trap(void)
    __attribute__((noinline, noreturn, used, externally_visible));
void neogeo_replay_progress_trap(void)
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

static void update_progress_checkpoint(uint16_t *remaining) {
    --*remaining;
    if (*remaining == 0u) {
        neogeo_replay_progress_trap();
        *remaining = SMB_NEOGEO_REPLAY_PROGRESS_FRAMES;
    }
}

int main(void) {
    NeogeoReplayGate gate;
    NeogeoReplaySnapshot snapshot;
    uint32_t frame = 0;
    uint32_t tail_frame = 0;
    uint32_t segment_index = 0;
    uint16_t segment_remaining;
    uint16_t progress_remaining = SMB_NEOGEO_REPLAY_PROGRESS_FRAMES;
    uint8_t controller_state;

    neogeo_video_init();
    cpu_init();
    initialize_power_on_ram();
    apu_init(0);
    ppu_init(0);
    Start();
    neogeo_replay_gate_init(&gate);

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
        NeogeoReplayGateResult result = run_replay_frame(
            &gate,
            controller_state,
            frame,
            0,
            segment_index
        );

        ++frame;
        update_progress_checkpoint(&progress_remaining);
        if (neogeo_replay_gate_failed(&gate) != 0u) {
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
        update_progress_checkpoint(&progress_remaining);
        if (neogeo_replay_gate_failed(&gate) != 0u) {
            neogeo_replay_fail_trap();
        }
        if (result == NEOGEO_REPLAY_GATE_COMPLETE) {
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
    neogeo_replay_fail_trap();
}
