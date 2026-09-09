#include "vs_native_fast_actor_loop.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* The generated body is linked into this differential test as the oracle. */
void smbneo_native_fn_actor_proc_bf60(SmbNeoNativeContext *ctx);

enum {
    RAM_SIZE = 0x0800,
    EXTRA_RAM_SIZE = 0x0800,
    PRG_SIZE = 0x8000,
    IO_SIZE = 0x4000,
    TRACE_CAPACITY = 4096
};

typedef struct {
    uint16_t address;
    uint8_t value;
    uint8_t write;
} BusEvent;

typedef struct {
    uint8_t io[IO_SIZE];
    BusEvent trace[TRACE_CAPACITY];
    unsigned trace_count;
    unsigned trace_overflow;
} TestBus;

typedef struct {
    SmbNeoNativeContext ctx;
    uint8_t ram[RAM_SIZE];
    uint8_t extra_ram[EXTRA_RAM_SIZE];
    uint8_t prg[PRG_SIZE];
    TestBus bus;
} Fixture;

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
    return (uint8_t)mix32(
        seed + (uint32_t)index * UINT32_C(0x9e3779b9)
    );
}

static void record_bus(
    TestBus *bus,
    uint16_t address,
    uint8_t value,
    uint8_t write
) {
    if (bus->trace_count < TRACE_CAPACITY) {
        BusEvent *event = &bus->trace[bus->trace_count++];
        event->address = address;
        event->value = value;
        event->write = write;
    } else {
        bus->trace_overflow = 1u;
    }
}

static uint8_t test_read8(void *opaque, uint16_t address) {
    TestBus *bus = (TestBus *)opaque;
    uint8_t value = bus->io[(address - 0x2000u) & (IO_SIZE - 1u)];

    record_bus(bus, address, value, 0u);
    return value;
}

static void test_write8(void *opaque, uint16_t address, uint8_t value) {
    TestBus *bus = (TestBus *)opaque;

    bus->io[(address - 0x2000u) & (IO_SIZE - 1u)] = value;
    record_bus(bus, address, value, 1u);
}

static void prepare_fixture(Fixture *fixture, uint32_t seed) {
    memset(fixture, 0, sizeof(*fixture));
    memset(fixture->ram, pattern_byte(seed, 4u), RAM_SIZE);
    memset(fixture->extra_ram, pattern_byte(seed, 5u), EXTRA_RAM_SIZE);
    memset(fixture->prg, pattern_byte(seed, 6u), PRG_SIZE);
    memset(fixture->bus.io, pattern_byte(seed, 7u), IO_SIZE);

    fixture->ctx.a = pattern_byte(seed, 0u);
    fixture->ctx.x = pattern_byte(seed, 1u);
    fixture->ctx.y = pattern_byte(seed, 2u);
    fixture->ctx.status = pattern_byte(seed, 3u);
    fixture->ctx.ram = fixture->ram;
    fixture->ctx.extra_ram = fixture->extra_ram;
    fixture->ctx.prg = fixture->prg;
    fixture->ctx.opaque = &fixture->bus;
    fixture->ctx.read8 = test_read8;
    fixture->ctx.write8 = test_write8;
    fixture->ctx.calls = mix32(seed);
    fixture->ctx.last_symbol = (uint16_t)mix32(seed ^ UINT32_C(0x55aa));
}

static void clone_fixture(Fixture *to, const Fixture *from) {
    memcpy(to, from, sizeof(*to));
    to->ctx.ram = to->ram;
    to->ctx.extra_ram = to->extra_ram;
    to->ctx.prg = to->prg;
    to->ctx.opaque = &to->bus;
    to->ctx.read8 = test_read8;
    to->ctx.write8 = test_write8;
}

static int compare_fixture(
    const Fixture *expected,
    const Fixture *actual,
    const char *suite,
    uint32_t ordinal
) {
#define CHECK_SCALAR(field) \
    do { \
        if (expected->ctx.field != actual->ctx.field) { \
            fprintf(stderr, \
                "%s case %" PRIu32 ": ctx.%s expected %u got %u\n", \
                suite, ordinal, #field, (unsigned)expected->ctx.field, \
                (unsigned)actual->ctx.field); \
            return 0; \
        } \
    } while (0)

    CHECK_SCALAR(a);
    CHECK_SCALAR(x);
    CHECK_SCALAR(y);
    CHECK_SCALAR(status);
    CHECK_SCALAR(suspended);
    CHECK_SCALAR(unsupported);
    CHECK_SCALAR(calls);
    CHECK_SCALAR(last_symbol);
#undef CHECK_SCALAR

    if (memcmp(expected->ram, actual->ram, RAM_SIZE) != 0) {
        for (unsigned i = 0u; i < RAM_SIZE; ++i) {
            if (expected->ram[i] != actual->ram[i]) {
                fprintf(stderr,
                    "%s case %" PRIu32
                    ": RAM[%04x] expected %02x got %02x\n",
                    suite, ordinal, i, expected->ram[i], actual->ram[i]);
                return 0;
            }
        }
    }
    if (memcmp(expected->extra_ram, actual->extra_ram, EXTRA_RAM_SIZE) != 0) {
        fprintf(stderr, "%s case %" PRIu32 ": extra RAM differs\n",
            suite, ordinal);
        return 0;
    }
    if (memcmp(expected->bus.io, actual->bus.io, IO_SIZE) != 0) {
        fprintf(stderr, "%s case %" PRIu32 ": I/O state differs\n",
            suite, ordinal);
        return 0;
    }
    if (expected->bus.trace_count != actual->bus.trace_count ||
        expected->bus.trace_overflow != actual->bus.trace_overflow ||
        memcmp(expected->bus.trace, actual->bus.trace,
            expected->bus.trace_count * sizeof(expected->bus.trace[0])) != 0) {
        fprintf(stderr,
            "%s case %" PRIu32 ": bus trace differs (%u vs %u)\n",
            suite, ordinal, expected->bus.trace_count,
            actual->bus.trace_count);
        return 0;
    }
    return 1;
}

static int run_case(
    const Fixture *initial,
    const char *suite,
    uint32_t ordinal
) {
    Fixture expected;
    Fixture actual;

    clone_fixture(&expected, initial);
    clone_fixture(&actual, initial);
    smbneo_native_fn_actor_proc_bf60(&expected.ctx);
    vs_native_fast_actor_proc(&actual.ctx);
    ++cases_run;
    return compare_fixture(&expected, &actual, suite, ordinal);
}

static void make_quiet(Fixture *fixture) {
    memset(fixture->ram, 0, RAM_SIZE);
    memset(fixture->extra_ram, 0, EXTRA_RAM_SIZE);
    memset(fixture->prg, 0, PRG_SIZE);
    fixture->ram[0x071fu] = 7u;
    fixture->ram[0x0008u] = 0u;
    fixture->ctx.a = 0u;
    fixture->ctx.x = 0u;
    fixture->ctx.y = 0u;
    fixture->ctx.status = 0u;
    fixture->ctx.suspended = 0u;
    fixture->ctx.unsupported = 0u;
    fixture->bus.trace_count = 0u;
    fixture->bus.trace_overflow = 0u;
}

static void set_course_pointer(Fixture *fixture, uint16_t address) {
    fixture->ram[0x00e9u] = (uint8_t)address;
    fixture->ram[0x00eau] = (uint8_t)(address >> 8);
}

static void set_course_byte(
    Fixture *fixture,
    uint16_t address,
    uint8_t value
) {
    if (address < 0x2000u)
        fixture->ram[address & 0x07ffu] = value;
    else if (address < 0x6000u)
        fixture->bus.io[(address - 0x2000u) & (IO_SIZE - 1u)] = value;
    else if (address < 0x8000u)
        fixture->extra_ram[address & 0x07ffu] = value;
    else
        fixture->prg[address - 0x8000u] = value;
}

static int test_child_actor_exhaustive(void) {
    Fixture fixture;
    uint32_t ordinal = 0u;

    for (unsigned x = 0u; x < 6u; ++x) {
        for (unsigned mode = 0x80u; mode < 0x100u; ++mode) {
            prepare_fixture(&fixture, UINT32_C(0x10000000) ^ ordinal);
            make_quiet(&fixture);
            fixture.ctx.x = (uint8_t)x;
            fixture.ctx.status = (uint8_t)mode;
            fixture.ram[(uint8_t)(0x0fu + x)] = (uint8_t)mode;
            fixture.ram[0x000fu + (mode & 0x0fu)] =
                (uint8_t)((x ^ mode) & 3u);
            if (!run_case(&fixture, "child-mode", ordinal++))
                return 0;
        }
    }
    return 1;
}

static int test_free_disabled_exhaustive(void) {
    Fixture fixture;
    uint32_t ordinal = 0u;

    for (unsigned x = 0u; x < 6u; ++x) {
        for (unsigned status = 0u; status < 256u; ++status) {
            prepare_fixture(&fixture, UINT32_C(0x20000000) ^ ordinal);
            make_quiet(&fixture);
            fixture.ctx.x = (uint8_t)x;
            fixture.ctx.status = (uint8_t)status;
            fixture.ram[(uint8_t)(0x0fu + x)] = 0u;
            fixture.ram[0x071fu] = (uint8_t)(7u | (status & 0xf8u));
            if (!run_case(&fixture, "free-disabled", ordinal++))
                return 0;
        }
    }
    return 1;
}

static int test_actor_init_check_exhaustive(void) {
    Fixture fixture;
    uint32_t ordinal = 0u;
    static const uint16_t pointers[] = {
        0x0100u, 0x2000u, 0x6000u, 0x8000u, 0xffffu
    };

    for (unsigned pointer_index = 0u;
         pointer_index < sizeof(pointers) / sizeof(pointers[0]);
         ++pointer_index) {
        for (unsigned x = 0u; x < 8u; ++x) {
            for (unsigned status = 0u; status < 256u; ++status) {
                uint16_t pointer = pointers[pointer_index];

                prepare_fixture(&fixture, UINT32_C(0x30000000) ^ ordinal);
                make_quiet(&fixture);
                fixture.ctx.x = (uint8_t)x;
                fixture.ctx.status = (uint8_t)status;
                fixture.ram[0x071fu] = 0u;
                fixture.ram[(uint8_t)(0x0fu + x)] = 0u;
                fixture.ram[0x0739u] = (uint8_t)(status * 13u);
                fixture.ram[0x0398u] = (uint8_t)(status == 255u ? 1u : 0u);
                fixture.ram[0x06cbu] = 0u;
                set_course_pointer(&fixture, pointer);
                set_course_byte(
                    &fixture,
                    (uint16_t)(pointer + fixture.ram[0x0739u]),
                    0xffu
                );
                if (!run_case(&fixture, "init-check", ordinal++))
                    return 0;
            }
        }
    }
    return 1;
}

static int test_active_dispatch(void) {
    Fixture fixture;
    uint32_t ordinal = 0u;

    /* All valid actor IDs plus both invalid table edges. */
    for (unsigned id = 0u; id <= 0xffu; ++id) {
        for (unsigned slot = 0u; slot < 6u; ++slot) {
            prepare_fixture(&fixture, UINT32_C(0x40000000) ^ ordinal);
            make_quiet(&fixture);
            fixture.ctx.x = (uint8_t)slot;
            fixture.ctx.status = (uint8_t)(id ^ slot);
            fixture.ram[0x0008u] = (uint8_t)slot;
            fixture.ram[0x000fu + slot] = 1u;
            fixture.ram[0x0016u + slot] = (uint8_t)id;
            fixture.ram[0x001eu + slot] = 0u;
            fixture.ram[0x00b6u + slot] = 1u;
            fixture.ram[0x00cfu + slot] = 0x80u;
            if (!run_case(&fixture, "active-dispatch", ordinal++))
                return 0;
        }
    }
    return 1;
}

static int test_course_stream_models(void) {
    Fixture fixture;
    uint32_t ordinal = 0u;
    static const uint16_t pointers[] = {0x0100u, 0x2000u, 0x6000u, 0x9000u};

    for (unsigned pointer_index = 0u;
         pointer_index < sizeof(pointers) / sizeof(pointers[0]);
         ++pointer_index) {
        uint16_t pointer = pointers[pointer_index];

        /* Slot-pressure early return: every first/second-byte boundary. */
        for (unsigned first = 0u; first < 256u; ++first) {
            prepare_fixture(&fixture, UINT32_C(0x50000000) ^ ordinal);
            make_quiet(&fixture);
            fixture.ctx.x = 5u;
            fixture.ram[0x071fu] = 0u;
            fixture.ram[0x000fu + 5u] = 0u;
            fixture.ram[0x0739u] = 0u;
            set_course_pointer(&fixture, pointer);
            set_course_byte(&fixture, pointer, (uint8_t)first);
            set_course_byte(&fixture, (uint16_t)(pointer + 1u), 0x01u);
            set_course_byte(&fixture, (uint16_t)(pointer + 2u), 0xffu);
            if (!run_case(&fixture, "course-pressure", ordinal++))
                return 0;
        }

        /* Page-control record followed by end-of-stream. */
        for (unsigned page = 0u; page < 64u; ++page) {
            prepare_fixture(&fixture, UINT32_C(0x51000000) ^ ordinal);
            make_quiet(&fixture);
            fixture.ctx.x = (uint8_t)(page % 6u);
            fixture.ram[0x071fu] = 0u;
            fixture.ram[0x000fu + fixture.ctx.x] = 0u;
            set_course_pointer(&fixture, pointer);
            set_course_byte(&fixture, pointer, 0x0fu);
            set_course_byte(&fixture, (uint16_t)(pointer + 1u), (uint8_t)page);
            set_course_byte(&fixture, (uint16_t)(pointer + 2u), 0xffu);
            if (!run_case(&fixture, "course-page", ordinal++))
                return 0;
        }

        /* Ordinary, group, and type-E records exercise every spawn class. */
        for (unsigned id = 0u; id < 64u; ++id) {
            prepare_fixture(&fixture, UINT32_C(0x52000000) ^ ordinal);
            make_quiet(&fixture);
            fixture.ctx.x = (uint8_t)(id % 6u);
            fixture.ram[0x071fu] = 0u;
            fixture.ram[0x0008u] = fixture.ctx.x;
            fixture.ram[0x000fu + fixture.ctx.x] = 0u;
            fixture.ram[0x071bu] = 1u;
            fixture.ram[0x071du] = 0u;
            fixture.ram[0x073au] = 1u;
            set_course_pointer(&fixture, pointer);
            set_course_byte(&fixture, pointer, 0x50u);
            set_course_byte(&fixture, (uint16_t)(pointer + 1u), (uint8_t)id);
            set_course_byte(&fixture, (uint16_t)(pointer + 2u), 0xffu);
            if (!run_case(&fixture, "course-spawn", ordinal++))
                return 0;

            prepare_fixture(&fixture, UINT32_C(0x53000000) ^ ordinal);
            make_quiet(&fixture);
            fixture.ctx.x = (uint8_t)(id % 6u);
            fixture.ram[0x071fu] = 0u;
            fixture.ram[0x0008u] = fixture.ctx.x;
            fixture.ram[0x000fu + fixture.ctx.x] = 0u;
            fixture.ram[0x071bu] = 1u;
            fixture.ram[0x071du] = 0u;
            fixture.ram[0x073au] = 1u;
            set_course_pointer(&fixture, pointer);
            set_course_byte(&fixture, pointer, 0x5eu);
            set_course_byte(&fixture, (uint16_t)(pointer + 1u), (uint8_t)id);
            set_course_byte(&fixture, (uint16_t)(pointer + 2u), 0x20u);
            set_course_byte(&fixture, (uint16_t)(pointer + 3u), 0xffu);
            if (!run_case(&fixture, "course-special", ordinal++))
                return 0;
        }

        /* Page-advance bit and existing-page state around every actor ID. */
        for (unsigned id = 0u; id < 64u; ++id) {
            for (unsigned page_on = 0u; page_on < 2u; ++page_on) {
                prepare_fixture(&fixture, UINT32_C(0x54000000) ^ ordinal);
                make_quiet(&fixture);
                fixture.ctx.x = (uint8_t)(id % 6u);
                fixture.ctx.status = (uint8_t)(id * 5u + page_on);
                fixture.ram[0x071fu] = 0u;
                fixture.ram[0x0008u] = fixture.ctx.x;
                fixture.ram[0x000fu + fixture.ctx.x] = 0u;
                fixture.ram[0x071bu] = 1u;
                fixture.ram[0x071du] = 0u;
                fixture.ram[0x073au] = 1u;
                fixture.ram[0x073bu] = (uint8_t)page_on;
                set_course_pointer(&fixture, pointer);
                set_course_byte(&fixture, pointer, 0x50u);
                set_course_byte(&fixture, (uint16_t)(pointer + 1u),
                    (uint8_t)(0x80u | id));
                set_course_byte(&fixture, (uint16_t)(pointer + 2u), 0xffu);
                if (!run_case(&fixture, "course-page-bit", ordinal++))
                    return 0;
            }
        }
    }
    return 1;
}

static int test_swarm_and_loop_models(void) {
    Fixture fixture;
    uint32_t ordinal = 0u;
    static const uint8_t queue_ids[] = {0x14u, 0x17u, 0x18u};

    for (unsigned queue_index = 0u;
         queue_index < sizeof(queue_ids) / sizeof(queue_ids[0]);
         ++queue_index) {
        for (unsigned x = 0u; x < 6u; ++x) {
            prepare_fixture(&fixture, UINT32_C(0x60000000) ^ ordinal);
            make_quiet(&fixture);
            fixture.ctx.x = (uint8_t)x;
            fixture.ram[0x071fu] = 0u;
            fixture.ram[0x000fu + x] = 0u;
            fixture.ram[0x06cdu] = queue_ids[queue_index];
            if (!run_case(&fixture, "swarm", ordinal++))
                return 0;
        }
    }
    /* Model every descriptor index and success/failure counter boundary. */
    for (unsigned descriptor = 0u; descriptor < 11u; ++descriptor) {
        for (unsigned counters = 0u; counters < 16u; ++counters) {
            prepare_fixture(&fixture, UINT32_C(0x61000000) ^ ordinal);
            make_quiet(&fixture);
            fixture.ctx.x = (uint8_t)(descriptor % 6u);
            fixture.ram[0x071fu] = 0u;
            fixture.ram[0x000fu + fixture.ctx.x] = 0u;
            fixture.ram[0x0745u] = 1u;
            fixture.ram[0x0726u] = 0u;
            fixture.ram[0x075fu] = fixture.prg[0x3f84u + descriptor];
            fixture.ram[0x0725u] = fixture.prg[0x3f8fu + descriptor];
            fixture.ram[0x00ceu] = fixture.prg[0x3f9au + descriptor];
            fixture.ram[0x001du] = (uint8_t)((counters & 8u) != 0u);
            fixture.ram[0x06d9u] = (uint8_t)(counters & 3u);
            fixture.ram[0x06dau] = (uint8_t)((counters >> 2) & 3u);
            fixture.ram[0x0739u] = 0u;
            set_course_pointer(&fixture, 0x9000u);
            set_course_byte(&fixture, 0x9000u, 0xffu);
            if (!run_case(&fixture, "course-loop", ordinal++))
                return 0;
        }
    }
    return 1;
}

int main(void) {
#define RUN_SUITE(name) \
    do { \
        puts("running " #name); \
        fflush(stdout); \
        if (!name()) \
            return 1; \
    } while (0)

    RUN_SUITE(test_child_actor_exhaustive);
    RUN_SUITE(test_free_disabled_exhaustive);
    RUN_SUITE(test_actor_init_check_exhaustive);
    RUN_SUITE(test_active_dispatch);
    RUN_SUITE(test_course_stream_models);
    RUN_SUITE(test_swarm_and_loop_models);
#undef RUN_SUITE

    printf("native actor-loop differential: %" PRIu64 " exact cases\n",
        cases_run);
    return 0;
}
