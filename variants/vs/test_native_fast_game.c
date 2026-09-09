#include "vs_native_fast_game.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    RAM_SIZE = 0x0800,
    EXTRA_RAM_SIZE = 0x0800,
    INPUT_CASES = 256 * 256,
    STAR_CASES = 256 * 256,
    RANDOM_CASES = 8192,
    MAX_CALLS = 32
};

enum ChildId {
    CHILD_PLAYER = 1,
    CHILD_PROJECTILES,
    CHILD_ACTOR,
    CHILD_SCORE,
    CHILD_PLAYER_BITS,
    CHILD_PLAYER_REL,
    CHILD_RENDER_PLAYER,
    CHILD_BLOCK_FLATTEN,
    CHILD_BLOCK,
    CHILD_MISC,
    CHILD_CANNON,
    CHILD_WHIRLPOOL,
    CHILD_POLE,
    CHILD_TIME,
    CHILD_META_COLOR,
    CHILD_AREA_MUSIC,
    CHILD_FIREFLASH,
    CHILD_FIREFLASH_OFF,
    CHILD_SCENERY
};

typedef struct {
    uint8_t value[18];
} CallRecord;

typedef struct {
    CallRecord records[MAX_CALLS];
    unsigned count;
    unsigned stop_at;
    uint8_t stop_with_unsupported;
    uint32_t seed;
} Probe;

typedef void (*SetupFunction)(
    SmbNeoNativeContext *ctx,
    Probe *probe,
    uint32_t ordinal
);

static uint64_t cases_run;

static uint32_t mix32(uint32_t value) {
    value ^= value >> 16;
    value *= UINT32_C(0x7feb352d);
    value ^= value >> 15;
    value *= UINT32_C(0x846ca68b);
    value ^= value >> 16;
    return value;
}

static uint8_t pattern_byte(uint32_t seed, unsigned index) {
    return (uint8_t)mix32(seed + (uint32_t)index * UINT32_C(0x9e3779b9));
}

static uint8_t reference_nz(SmbNeoNativeContext *ctx, uint8_t value) {
    ctx->status = (uint8_t)(
        (ctx->status & (uint8_t)~(NATIVE_N | NATIVE_Z)) |
        (value & NATIVE_N) |
        (value == 0u ? NATIVE_Z : 0u)
    );
    return value;
}

static void reference_cmp(
    SmbNeoNativeContext *ctx,
    uint8_t lhs,
    uint8_t rhs
) {
    uint8_t result = (uint8_t)(lhs - rhs);
    ctx->status = (uint8_t)(
        (ctx->status & (uint8_t)~(NATIVE_C | NATIVE_N | NATIVE_Z)) |
        (lhs >= rhs ? NATIVE_C : 0u) |
        (result & NATIVE_N) |
        (result == 0u ? NATIVE_Z : 0u)
    );
}

static uint8_t reference_lsr(
    SmbNeoNativeContext *ctx,
    uint8_t value
) {
    ctx->status = (uint8_t)(
        (ctx->status & (uint8_t)~NATIVE_C) |
        ((value & 1u) != 0u ? NATIVE_C : 0u)
    );
    return reference_nz(ctx, (uint8_t)(value >> 1));
}

static void record_child(SmbNeoNativeContext *ctx, enum ChildId id) {
    static const uint16_t watched[] = {
        0x0008, 0x0009, 0x000a, 0x000c, 0x000d, 0x00b5,
        0x06fc, 0x06fd, 0x0753, 0x0772, 0x077a, 0x077f,
        0x079f
    };
    Probe *probe = (Probe *)ctx->opaque;
    CallRecord *record;
    unsigned ordinal;
    uint8_t mutation;

    if (probe == NULL || probe->count >= MAX_CALLS) {
        ctx->unsupported = 1u;
        return;
    }
    ordinal = probe->count++;
    record = &probe->records[ordinal];
    record->value[0] = (uint8_t)id;
    record->value[1] = ctx->a;
    record->value[2] = ctx->x;
    record->value[3] = ctx->y;
    record->value[4] = ctx->status;
    for (unsigned index = 0u;
         index < sizeof(watched) / sizeof(watched[0]); ++index)
        record->value[5u + index] = ctx->ram[watched[index]];

    mutation = pattern_byte(
        probe->seed ^ (uint32_t)id * UINT32_C(0x45d9f3b), ordinal
    );
    ctx->a = (uint8_t)(ctx->a + mutation);
    ctx->y = (uint8_t)(ctx->y ^ (uint8_t)(mutation + (uint8_t)id));
    ctx->status = (uint8_t)(ctx->status ^
        (uint8_t)((mutation << 1) | (mutation >> 7)));
    ctx->ram[0x0500u + ((ordinal * 17u + (unsigned)id) & 0xffu)] ^=
        (uint8_t)(mutation | 1u);

    /* Exercise child-clobbered X while keeping the six-actor loop finite. */
    if (id == CHILD_ACTOR)
        ctx->x = (uint8_t)(ctx->ram[0x0008u] ^ 0x40u);
    else if (id == CHILD_SCORE)
        ctx->x = ctx->ram[0x0008u];
    else
        ctx->x = (uint8_t)(ctx->x ^ (mutation & 0x1fu));

    if (probe->stop_at != 0u && probe->count == probe->stop_at) {
        if (probe->stop_with_unsupported != 0u)
            ctx->unsupported = 1u;
        else
            ctx->suspended = 1u;
    }
}

#define DEFINE_CHILD(symbol, id) \
    void symbol(SmbNeoNativeContext *ctx) { record_child(ctx, id); }

DEFINE_CHILD(smbneo_native_fn_player_proc_aef7, CHILD_PLAYER)
DEFINE_CHILD(smbneo_native_fn_proj_proc_b4d1, CHILD_PROJECTILES)
DEFINE_CHILD(smbneo_native_fn_actor_proc_bf60, CHILD_ACTOR)
DEFINE_CHILD(smbneo_native_fn_bg_score_disp_86a4, CHILD_SCORE)
DEFINE_CHILD(smbneo_native_fn_pos_bits_get_player_f0e5, CHILD_PLAYER_BITS)
DEFINE_CHILD(smbneo_native_fn_pos_calc_x_rel_player_f08f, CHILD_PLAYER_REL)
DEFINE_CHILD(smbneo_native_fn_render_player_ee46, CHILD_RENDER_PLAYER)
DEFINE_CHILD(smbneo_native_fn_block_flatten_bde3, CHILD_BLOCK_FLATTEN)
DEFINE_CHILD(smbneo_native_fn_block_proc_bd7f, CHILD_BLOCK)
DEFINE_CHILD(smbneo_native_fn_misc_proc_ba8a, CHILD_MISC)
DEFINE_CHILD(smbneo_native_fn_cannon_proc_b8b0, CHILD_CANNON)
DEFINE_CHILD(smbneo_native_fn_whirlpool_proc_b66c, CHILD_WHIRLPOOL)
DEFINE_CHILD(smbneo_native_fn_pole_proc_b709, CHILD_POLE)
DEFINE_CHILD(smbneo_native_fn_stats_time_proc_b5fc, CHILD_TIME)
DEFINE_CHILD(smbneo_native_fn_meta_color_rotate_8be2, CHILD_META_COLOR)
DEFINE_CHILD(smbneo_native_fn_game_init_area_music_977e, CHILD_AREA_MUSIC)
DEFINE_CHILD(smbneo_native_fn_player_proc_firefl_do_b135, CHILD_FIREFLASH)
DEFINE_CHILD(smbneo_native_fn_player_proc_firefl_do_2_b147,
             CHILD_FIREFLASH_OFF)
DEFINE_CHILD(smbneo_native_fn_game_proc_scenery_scroll_ae1c, CHILD_SCENERY)

#undef DEFINE_CHILD

#define REFERENCE_CALL(child) \
    do { \
        child(ctx); \
        if (ctx->suspended != 0u || ctx->unsupported != 0u) \
            return; \
    } while (0)

/* Instruction-shaped oracle for the complete $AD6E..$AE1A owner body. */
static void reference_game_proc(SmbNeoNativeContext *ctx) {
    uint8_t *ram = ctx->ram;

    ctx->a = reference_nz(ctx, ram[0x077au]);
    if ((ctx->status & NATIVE_Z) == 0u) {
        ctx->x = reference_nz(ctx, ram[0x0753u]);
        ctx->a = reference_nz(ctx, ram[0x06fcu + ctx->x]);
    } else {
        ctx->a = reference_nz(ctx, ram[0x06fcu]);
        ctx->a = reference_nz(ctx, (uint8_t)(ctx->a | ram[0x06fdu]));
    }
    ram[0x06fcu] = ctx->a;
    ctx->a = reference_nz(ctx, (uint8_t)(ctx->a & 1u));
    if ((ctx->status & NATIVE_Z) == 0u) {
        ctx->a = reference_nz(ctx, ram[0x06fcu]);
        ctx->a = reference_nz(ctx, (uint8_t)(ctx->a & 0xfdu));
        ram[0x06fcu] = ctx->a;
    }
    ctx->a = reference_nz(ctx, ram[0x06fcu]);
    ctx->a = reference_nz(ctx, (uint8_t)(ctx->a & 4u));
    if ((ctx->status & NATIVE_Z) == 0u) {
        ctx->a = reference_nz(ctx, ram[0x06fcu]);
        ctx->a = reference_nz(ctx, (uint8_t)(ctx->a & 0xf7u));
        ram[0x06fcu] = ctx->a;
    }

    REFERENCE_CALL(smbneo_native_fn_player_proc_aef7);
    ctx->a = reference_nz(ctx, ram[0x0772u]);
    reference_cmp(ctx, ctx->a, 3u);
    if ((ctx->status & NATIVE_C) == 0u)
        return;

    REFERENCE_CALL(smbneo_native_fn_proj_proc_b4d1);
    ctx->x = reference_nz(ctx, 0u);
    for (;;) {
        ram[0x0008u] = ctx->x;
        REFERENCE_CALL(smbneo_native_fn_actor_proc_bf60);
        REFERENCE_CALL(smbneo_native_fn_bg_score_disp_86a4);
        ctx->x = reference_nz(ctx, (uint8_t)(ctx->x + 1u));
        reference_cmp(ctx, ctx->x, 6u);
        if ((ctx->status & NATIVE_Z) != 0u)
            break;
    }
    REFERENCE_CALL(smbneo_native_fn_pos_bits_get_player_f0e5);
    REFERENCE_CALL(smbneo_native_fn_pos_calc_x_rel_player_f08f);
    REFERENCE_CALL(smbneo_native_fn_render_player_ee46);
    REFERENCE_CALL(smbneo_native_fn_block_flatten_bde3);
    ctx->x = reference_nz(ctx, 1u);
    ram[0x0008u] = ctx->x;
    REFERENCE_CALL(smbneo_native_fn_block_proc_bd7f);
    ctx->x = reference_nz(ctx, (uint8_t)(ctx->x - 1u));
    ram[0x0008u] = ctx->x;
    REFERENCE_CALL(smbneo_native_fn_block_proc_bd7f);
    REFERENCE_CALL(smbneo_native_fn_misc_proc_ba8a);
    REFERENCE_CALL(smbneo_native_fn_cannon_proc_b8b0);
    REFERENCE_CALL(smbneo_native_fn_whirlpool_proc_b66c);
    REFERENCE_CALL(smbneo_native_fn_pole_proc_b709);
    REFERENCE_CALL(smbneo_native_fn_stats_time_proc_b5fc);
    REFERENCE_CALL(smbneo_native_fn_meta_color_rotate_8be2);

    ctx->a = reference_nz(ctx, ram[0x00b5u]);
    reference_cmp(ctx, ctx->a, 2u);
    if ((ctx->status & NATIVE_N) != 0u) {
        ctx->a = reference_nz(ctx, ram[0x079fu]);
        if ((ctx->status & NATIVE_Z) != 0u) {
            REFERENCE_CALL(smbneo_native_fn_player_proc_firefl_do_2_b147);
            goto finish;
        }
        reference_cmp(ctx, ctx->a, 4u);
        if ((ctx->status & NATIVE_Z) != 0u) {
            ctx->a = reference_nz(ctx, ram[0x077fu]);
            if ((ctx->status & NATIVE_Z) != 0u)
                REFERENCE_CALL(smbneo_native_fn_game_init_area_music_977e);
        }
    }

    ctx->y = reference_nz(ctx, ram[0x079fu]);
    ctx->a = reference_nz(ctx, ram[0x0009u]);
    reference_cmp(ctx, ctx->y, 8u);
    if ((ctx->status & NATIVE_C) == 0u) {
        ctx->a = reference_lsr(ctx, ctx->a);
        ctx->a = reference_lsr(ctx, ctx->a);
    }
    ctx->a = reference_lsr(ctx, ctx->a);
    REFERENCE_CALL(smbneo_native_fn_player_proc_firefl_do_b135);

finish:
    ctx->a = reference_nz(ctx, ram[0x000au]);
    ram[0x000du] = ctx->a;
    ctx->a = reference_nz(ctx, 0u);
    ram[0x000cu] = ctx->a;
    smbneo_native_fn_game_proc_scenery_scroll_ae1c(ctx);
}

#undef REFERENCE_CALL

static void prepare_context(
    SmbNeoNativeContext *ctx,
    Probe *probe,
    uint8_t *ram,
    uint8_t *extra_ram,
    uint32_t seed
) {
    memset(ctx, 0, sizeof(*ctx));
    memset(probe, 0, sizeof(*probe));
    memset(ram, pattern_byte(seed, 0u), RAM_SIZE);
    memset(extra_ram, pattern_byte(seed, 1u), EXTRA_RAM_SIZE);
    ctx->a = pattern_byte(seed, 2u);
    ctx->x = pattern_byte(seed, 3u);
    ctx->y = pattern_byte(seed, 4u);
    ctx->status = pattern_byte(seed, 5u);
    ctx->ram = ram;
    ctx->extra_ram = extra_ram;
    ctx->opaque = probe;
    ctx->calls = mix32(seed ^ UINT32_C(0xa5a55a5a));
    ctx->last_symbol = (uint16_t)mix32(seed ^ UINT32_C(0x31415926));
    probe->seed = seed;
}

static void setup_merge(
    SmbNeoNativeContext *ctx,
    Probe *probe,
    uint32_t ordinal
) {
    (void)probe;
    ctx->ram[0x077au] = 0u;
    ctx->ram[0x0772u] = 0u;
    ctx->ram[0x06fcu] = (uint8_t)ordinal;
    ctx->ram[0x06fdu] = (uint8_t)(ordinal >> 8);
}

static void setup_selected(
    SmbNeoNativeContext *ctx,
    Probe *probe,
    uint32_t ordinal
) {
    uint8_t player_id = (uint8_t)(ordinal >> 8);

    (void)probe;
    ctx->ram[0x06fcu + player_id] = (uint8_t)ordinal;
    ctx->ram[0x0753u] = player_id;
    ctx->ram[0x077au] = 1u;
    ctx->ram[0x0772u] = 0u;
}

static void setup_full(
    SmbNeoNativeContext *ctx,
    Probe *probe,
    uint32_t ordinal
) {
    (void)probe;
    ctx->ram[0x077au] = (uint8_t)(ordinal & 1u);
    ctx->ram[0x0753u] = (uint8_t)((ordinal >> 1) & 1u);
    ctx->ram[0x06fcu] = pattern_byte(ordinal, 0x6fcu);
    ctx->ram[0x06fdu] = pattern_byte(ordinal, 0x6fdu);
    ctx->ram[0x0772u] = (uint8_t)(3u + (ordinal % 253u));
    ctx->ram[0x00b5u] = pattern_byte(ordinal, 0xb5u);
    ctx->ram[0x079fu] = pattern_byte(ordinal, 0x79fu);
    ctx->ram[0x077fu] = pattern_byte(ordinal, 0x77fu);
    ctx->ram[0x0009u] = pattern_byte(ordinal, 9u);
    ctx->ram[0x000au] = pattern_byte(ordinal, 10u);
}

static void setup_star(
    SmbNeoNativeContext *ctx,
    Probe *probe,
    uint32_t ordinal
) {
    setup_full(ctx, probe, ordinal);
    ctx->ram[0x00b5u] = (uint8_t)ordinal;
    ctx->ram[0x079fu] = (uint8_t)(ordinal >> 8);
    ctx->ram[0x077fu] = (uint8_t)(ordinal ^ (ordinal >> 8));
}

static void setup_threshold(
    SmbNeoNativeContext *ctx,
    Probe *probe,
    uint32_t ordinal
) {
    setup_full(ctx, probe, ordinal);
    ctx->ram[0x0772u] = (uint8_t)ordinal;
}

static void setup_early(
    SmbNeoNativeContext *ctx,
    Probe *probe,
    uint32_t ordinal
) {
    setup_full(ctx, probe, ordinal);
    ctx->ram[0x00b5u] = 0u;
    ctx->ram[0x079fu] = 4u;
    ctx->ram[0x077fu] = 0u;
}

static int probes_equal(const Probe *reference, const Probe *actual) {
    if (reference->count != actual->count ||
            reference->stop_at != actual->stop_at ||
            reference->stop_with_unsupported !=
                actual->stop_with_unsupported ||
            reference->seed != actual->seed)
        return 0;
    return memcmp(
        reference->records, actual->records,
        reference->count * sizeof(reference->records[0])
    ) == 0;
}

static int compare_case(
    const char *suite,
    uint32_t ordinal,
    SmbNeoNativeContext *reference,
    SmbNeoNativeContext *actual,
    Probe *reference_probe,
    Probe *actual_probe,
    uint8_t *reference_ram,
    uint8_t *actual_ram,
    uint8_t *reference_extra,
    uint8_t *actual_extra
) {
    reference_game_proc(reference);
    vs_native_fast_game_proc(actual);
    ++cases_run;

#define CHECK_SCALAR(field) \
    do { if (reference->field != actual->field) { \
        fprintf(stderr, "%s case=%" PRIu32 " " #field \
                " expected=%" PRIu32 " actual=%" PRIu32 "\n", \
                suite, ordinal, (uint32_t)reference->field, \
                (uint32_t)actual->field); \
        return 1; \
    } } while (0)
    CHECK_SCALAR(a);
    CHECK_SCALAR(x);
    CHECK_SCALAR(y);
    CHECK_SCALAR(status);
    CHECK_SCALAR(suspended);
    CHECK_SCALAR(unsupported);
    CHECK_SCALAR(calls);
    CHECK_SCALAR(last_symbol);
#undef CHECK_SCALAR

    if (!probes_equal(reference_probe, actual_probe)) {
        fprintf(stderr, "%s case=%" PRIu32 " child trace differs "
                "expected-count=%u actual-count=%u\n", suite, ordinal,
                reference_probe->count, actual_probe->count);
        return 1;
    }
    if (memcmp(reference_ram, actual_ram, RAM_SIZE) != 0) {
        for (unsigned address = 0u; address < RAM_SIZE; ++address) {
            if (reference_ram[address] != actual_ram[address]) {
                fprintf(stderr, "%s case=%" PRIu32 " RAM[%04x] "
                        "expected=%02x actual=%02x\n", suite, ordinal,
                        address, reference_ram[address], actual_ram[address]);
                break;
            }
        }
        return 1;
    }
    if (memcmp(reference_extra, actual_extra, EXTRA_RAM_SIZE) != 0) {
        fprintf(stderr, "%s case=%" PRIu32 " modified extra RAM\n",
                suite, ordinal);
        return 1;
    }
    return 0;
}

static int run_case(
    const char *suite,
    uint32_t ordinal,
    SetupFunction setup,
    unsigned stop_at,
    uint8_t unsupported
) {
    uint8_t reference_ram[RAM_SIZE];
    uint8_t actual_ram[RAM_SIZE];
    uint8_t reference_extra[EXTRA_RAM_SIZE];
    uint8_t actual_extra[EXTRA_RAM_SIZE];
    SmbNeoNativeContext reference;
    SmbNeoNativeContext actual;
    Probe reference_probe;
    Probe actual_probe;
    uint32_t seed = mix32(ordinal ^ UINT32_C(0xd1b54a35));

    prepare_context(
        &reference, &reference_probe, reference_ram, reference_extra, seed
    );
    setup(&reference, &reference_probe, ordinal);
    reference_probe.stop_at = stop_at;
    reference_probe.stop_with_unsupported = unsupported;

    actual = reference;
    actual_probe = reference_probe;
    memcpy(actual_ram, reference_ram, sizeof(actual_ram));
    memcpy(actual_extra, reference_extra, sizeof(actual_extra));
    actual.ram = actual_ram;
    actual.extra_ram = actual_extra;
    actual.opaque = &actual_probe;

    if (compare_case(
        suite, ordinal, &reference, &actual,
        &reference_probe, &actual_probe,
        reference_ram, actual_ram, reference_extra, actual_extra
    ) != 0)
        return 1;
    if (stop_at != 0u && reference_probe.count != stop_at) {
        fprintf(stderr, "%s case=%" PRIu32 " requested stop=%u "
                "but reached=%u\n", suite, ordinal, stop_at,
                reference_probe.count);
        return 1;
    }
    return 0;
}

static int run_suite(
    const char *name,
    uint32_t count,
    SetupFunction setup
) {
    for (uint32_t ordinal = 0u; ordinal < count; ++ordinal) {
        if (run_case(name, ordinal, setup, 0u, 0u) != 0)
            return 1;
    }
    return 0;
}

static int run_early_exits(void) {
    /* The music-enabled path has 29 child boundaries, including scenery. */
    for (unsigned unsupported = 0u; unsupported < 2u; ++unsupported) {
        for (unsigned stop = 1u; stop <= 29u; ++stop) {
            uint32_t ordinal = (uint32_t)(stop + unsupported * 29u);
            if (run_case(
                    "early-exit", ordinal, setup_early, stop,
                    (uint8_t)unsupported
                ) != 0)
                return 1;
        }
    }
    return 0;
}

static int check_missing_ram(void) {
    SmbNeoNativeContext ctx;

    memset(&ctx, 0, sizeof(ctx));
    vs_native_fast_game_proc(&ctx);
    if (ctx.unsupported != 1u) {
        fputs("game coordinator did not reject missing RAM\n", stderr);
        return 1;
    }
    return 0;
}

int main(void) {
    if (run_suite("merged-input", INPUT_CASES, setup_merge) != 0 ||
        run_suite("selected-input", INPUT_CASES, setup_selected) != 0 ||
        run_suite("star-fire", STAR_CASES, setup_star) != 0 ||
        run_suite("random-full", RANDOM_CASES, setup_full) != 0 ||
        run_suite("mode-threshold", 256u, setup_threshold) != 0 ||
        run_early_exits() != 0 ||
        check_missing_ram() != 0)
        return 1;

    printf("VS native game coordinator: %" PRIu64
           " exact model cases with ordered child-boundary traces passed\n",
           cases_run);
    return 0;
}
