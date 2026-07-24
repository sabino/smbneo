#define _POSIX_C_SOURCE 200809L

/*
 * Host-native deterministic state transcript for the translated game core.
 *
 * smb_trace_replay_data.h is generated from a local FM2 by rec_tool.py.
 * This executable deliberately skips rendering and audio. Game logic, PPU
 * register writes, OAM DMA, and controller reads still run through the same
 * translated core used by the cartridge.
 */

#include "apu.h"
#include "code.h"
#include "constants.h"
#include "cpu.h"
#include "ppu.h"
#include "smb_trace_replay_data.h"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define TRACE_SCHEMA "smb-core-state-trace-v1"
#define FNV1A_OFFSET UINT32_C(2166136261)
#define FNV1A_PRIME UINT32_C(16777619)

#if SMB_TRACE_END_FRAME != SMB_TRACE_FM2_FRAME_COUNT
#error "generated replay frame-count metadata is inconsistent"
#endif

#if \
    SMB_TRACE_FM2_RAM_INIT_OPTION != 0 && \
    SMB_TRACE_FM2_RAM_INIT_OPTION != 2
#error "state trace supports only deterministic FM2 RAM options 0 and 2"
#endif

#if SMB_TRACE_FM2_INITIAL_COMMAND > 1
#error "state trace supports only a frame-zero soft reset command"
#endif

static uint32_t fnv1a_update(
    uint32_t hash,
    const uint8_t *data,
    size_t size
) {
    size_t index;

    for (index = 0; index < size; ++index) {
        hash ^= data[index];
        hash *= FNV1A_PRIME;
    }
    return hash;
}

static uint32_t fnv1a(const uint8_t *data, size_t size) {
    return fnv1a_update(FNV1A_OFFSET, data, size);
}

/*
 * The generated C core models the 6502 stack pointer but intentionally does
 * not mirror C calls into NES stack page $0100-$01ff. Excluding that page
 * produces a state domain both the translated core and an instruction-level
 * emulator model with the same meaning:
 *
 *   zero page $0000-$00ff, then game/OAM/work RAM $0200-$07ff.
 */
static uint32_t semantic_ram_hash(void) {
    uint32_t hash = fnv1a_update(FNV1A_OFFSET, ram, 0x100u);

    return fnv1a_update(hash, ram + 0x200u, RAM_SIZE - 0x200u);
}

static void initialize_power_on_ram(void) {
    uint16_t address;

#if SMB_TRACE_FM2_RAM_INIT_OPTION == 0
    for (address = 0; address < RAM_SIZE; ++address) {
        ram[address] = (address & 7u) < 4u ? 0u : 0xffu;
    }
#else
    memset(ram, 0, RAM_SIZE);
#endif
}

static uint32_t parse_frame_limit(const char *value) {
    char *end = NULL;
    unsigned long parsed;

    errno = 0;
    parsed = strtoul(value, &end, 10);
    if (
        errno != 0 ||
        end == value ||
        *end != '\0' ||
        parsed > SMB_TRACE_END_FRAME
    ) {
        fprintf(
            stderr,
            "frame limit must be between 0 and %" PRIu32 "\n",
            (uint32_t)SMB_TRACE_END_FRAME
        );
        exit(2);
    }
    return (uint32_t)parsed;
}

static void print_header(
    uint32_t frame_limit,
    uint32_t input_frame_offset,
    uint32_t scheduled_holds
) {
    puts("# schema=" TRACE_SCHEMA);
    puts("# source=translated-core");
    puts("# frame_semantics=post_input_nmi");
    puts("# command_1_semantics=fresh_core_reset_before_frame_0");
    puts("# semantic_ram=$0000-$00ff,$0200-$07ff");
    puts("# hash=fnv1a32");
    puts("# lagged_semantics=scheduled_no_nmi_hold");
    printf("# frames=%" PRIu32 "\n", frame_limit);
    printf("# input_frame_offset=%" PRIu32 "\n", input_frame_offset);
    printf("# scheduled_holds=%" PRIu32 "\n", scheduled_holds);
    printf(
        "# fm2_source_sha256=%s\n",
        SMB_TRACE_FM2_SOURCE_SHA256
    );
    printf(
        "# fm2_ram_init_option=%" PRIu32 "\n",
        (uint32_t)SMB_TRACE_FM2_RAM_INIT_OPTION
    );
    printf(
        "# fm2_initial_command=%" PRIu32 "\n",
        (uint32_t)SMB_TRACE_FM2_INITIAL_COMMAND
    );
    puts(
        "frame,input,semantic_hash,full_ram_hash,zero_page_hash,"
        "stack_hash,oam_hash,work_hash,oper_mode,oper_mode_task,"
        "world,level,engine_subroutine,player_state,player_page,"
        "player_x,player_y,screen_page,screen_x,world_end_timer"
        ",lagged,lag_count"
    );
}

static void print_frame(
    uint32_t frame_index,
    uint8_t input,
    uint8_t lagged,
    uint32_t lag_count
) {
    printf(
        "%" PRIu32 ",%u,%08" PRIx32 ",%08" PRIx32
        ",%08" PRIx32 ",%08" PRIx32 ",%08" PRIx32
        ",%08" PRIx32
        ",%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",
        frame_index,
        (unsigned int)input,
        semantic_ram_hash(),
        fnv1a(ram, RAM_SIZE),
        fnv1a(ram, 0x100u),
        fnv1a(ram + 0x100u, 0x100u),
        fnv1a(ram + 0x200u, 0x100u),
        fnv1a(ram + 0x300u, RAM_SIZE - 0x300u),
        (unsigned int)ram[OperMode],
        (unsigned int)ram[OperMode_Task],
        (unsigned int)ram[WorldNumber],
        (unsigned int)ram[LevelNumber],
        (unsigned int)ram[GameEngineSubroutine],
        (unsigned int)ram[Player_State],
        (unsigned int)ram[Player_PageLoc],
        (unsigned int)ram[Player_X_Position],
        (unsigned int)ram[Player_Y_Position],
        (unsigned int)ram[ScreenLeft_PageLoc],
        (unsigned int)ram[ScreenLeft_X_Pos],
        (unsigned int)ram[WorldEndTimer],
        (unsigned int)lagged,
        lag_count
    );
}

static uint8_t *load_hold_schedule(
    const char *path,
    uint32_t frame_limit,
    uint32_t *hold_count
) {
    uint8_t *schedule;
    FILE *source;
    uint32_t index;

    *hold_count = 0;
    if (path == NULL) {
        return NULL;
    }
    source = fopen(path, "rb");
    if (source == NULL) {
        fprintf(stderr, "cannot open hold schedule %s\n", path);
        exit(2);
    }
    schedule = calloc(frame_limit == 0u ? 1u : frame_limit, 1u);
    if (schedule == NULL) {
        fputs("cannot allocate hold schedule\n", stderr);
        fclose(source);
        exit(2);
    }
    if (
        frame_limit != 0u &&
        fread(schedule, 1, frame_limit, source) != frame_limit
    ) {
        fputs("hold schedule is shorter than the trace\n", stderr);
        free(schedule);
        fclose(source);
        exit(2);
    }
    if (fgetc(source) != EOF) {
        fputs("hold schedule is longer than the trace\n", stderr);
        free(schedule);
        fclose(source);
        exit(2);
    }
    fclose(source);

    for (index = 0; index < frame_limit; ++index) {
        if (schedule[index] > 1u) {
            fprintf(
                stderr,
                "hold schedule byte %" PRIu32 " is not 0 or 1\n",
                index
            );
            free(schedule);
            exit(2);
        }
        *hold_count += schedule[index];
    }
    return schedule;
}

int main(int argc, char **argv) {
    static const uint8_t blank_chr[CHR_ROM_SIZE];
    uint32_t frame_limit;
    uint32_t input_frame_offset = 0;
    uint32_t skipped_frames;
    uint32_t frame_index;
    uint32_t segment_index = 0;
    uint16_t segment_remaining;
    uint8_t controller_state;
    const char *dump_frame_value = getenv("SMB_TRACE_RAM_DUMP_FRAME");
    const char *dump_path = getenv("SMB_TRACE_RAM_DUMP_OUTPUT");
    uint32_t dump_frame = 0;
    FILE *dump_file = NULL;
    const char *hold_schedule_path = getenv("SMB_TRACE_HOLD_SCHEDULE");
    uint8_t *hold_schedule;
    uint32_t scheduled_holds;
    uint32_t lag_count = 0;

    if (argc > 3) {
        fprintf(
            stderr,
            "usage: %s [frame-limit [input-frame-offset]]\n",
            argv[0]
        );
        return 2;
    }
    if (argc >= 3) {
        input_frame_offset = parse_frame_limit(argv[2]);
    }
    frame_limit = SMB_TRACE_END_FRAME - input_frame_offset;
    if (argc >= 2) {
        frame_limit = parse_frame_limit(argv[1]);
    }
    if (frame_limit > SMB_TRACE_END_FRAME - input_frame_offset) {
        fprintf(
            stderr,
            "frame limit plus input offset exceeds %" PRIu32 "\n",
            (uint32_t)SMB_TRACE_END_FRAME
        );
        return 2;
    }
    if (
        (dump_frame_value == NULL) != (dump_path == NULL) ||
        (dump_path != NULL && dump_path[0] == '\0')
    ) {
        fputs(
            "SMB_TRACE_RAM_DUMP_FRAME and SMB_TRACE_RAM_DUMP_OUTPUT "
            "must be set together\n",
            stderr
        );
        return 2;
    }
    if (dump_frame_value != NULL) {
        int dump_fd;

        dump_frame = parse_frame_limit(dump_frame_value);
        if (dump_frame >= frame_limit) {
            fputs("RAM dump frame is outside the emitted trace\n", stderr);
            return 2;
        }
        dump_fd = open(
            dump_path,
            O_WRONLY | O_CREAT | O_EXCL,
            0666
        );
        if (dump_fd < 0) {
            fprintf(
                stderr,
                "cannot exclusively create RAM dump %s: %s\n",
                dump_path,
                strerror(errno)
            );
            return 2;
        }
        dump_file = fdopen(dump_fd, "wb");
        if (dump_file == NULL) {
            fprintf(
                stderr,
                "cannot open RAM dump stream %s: %s\n",
                dump_path,
                strerror(errno)
            );
            close(dump_fd);
            return 2;
        }
    }
    hold_schedule = load_hold_schedule(
        hold_schedule_path,
        frame_limit,
        &scheduled_holds
    );

    setvbuf(stdout, NULL, _IOFBF, 1024u * 1024u);
    cpu_init();
    initialize_power_on_ram();
    apu_init(0);
    ppu_init((uint8_t *)blank_chr);

    /*
     * FM2 command 1 requests reset before frame zero. A new host process is
     * already at power-on state, so Start is the equivalent reset/bootstrap
     * for both command 0 and command 1 recordings.
     */
    Start();

    segment_remaining = smb_trace_durations[0];
    controller_state = smb_trace_states[0];
    skipped_frames = input_frame_offset;
    while (skipped_frames >= segment_remaining) {
        skipped_frames -= segment_remaining;
        ++segment_index;
        if (segment_index >= SMB_TRACE_SEGMENT_COUNT) {
            break;
        }
        segment_remaining = smb_trace_durations[segment_index];
        controller_state = smb_trace_states[segment_index];
    }
    if (segment_index < SMB_TRACE_SEGMENT_COUNT) {
        segment_remaining = (uint16_t)(
            segment_remaining - skipped_frames
        );
    }
    print_header(frame_limit, input_frame_offset, scheduled_holds);

    for (frame_index = 0; frame_index < frame_limit; ++frame_index) {
        uint8_t hold = (
            hold_schedule == NULL ? 0u : hold_schedule[frame_index]
        );

        if (hold == 0u) {
            update_controller1(controller_state);
            next_frame();
        } else {
            ++lag_count;
        }
        if (dump_file != NULL && frame_index == dump_frame) {
            if (
                fwrite(
                    ram + 0x300u,
                    1,
                    RAM_SIZE - 0x300u,
                    dump_file
                ) != RAM_SIZE - 0x300u
            ) {
                fputs("cannot write RAM dump\n", stderr);
                fclose(dump_file);
                return 2;
            }
        }
        print_frame(
            frame_index,
            controller_state,
            hold,
            lag_count
        );

        --segment_remaining;
        if (segment_remaining == 0u) {
            ++segment_index;
            if (segment_index < SMB_TRACE_SEGMENT_COUNT) {
                segment_remaining = smb_trace_durations[segment_index];
                controller_state = smb_trace_states[segment_index];
            }
        }
    }

    puts("# complete=1");
    if (dump_file != NULL && fclose(dump_file) != 0) {
        fputs("cannot close RAM dump\n", stderr);
        return 2;
    }
    free(hold_schedule);
    return 0;
}
