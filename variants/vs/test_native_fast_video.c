#include "vs_native_fast_video.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

enum { MAX_EVENTS = 80 };

typedef struct {
    uint16_t address;
    uint8_t value;
} WriteEvent;

typedef struct {
    WriteEvent events[MAX_EVENTS];
    unsigned event_count;
    unsigned bulk_calls;
    uint16_t address;
    uint16_t address_latch;
    uint8_t address_toggle;
    uint8_t control;
    bool accept_bulk;
    bool metadata_ok;
} VideoSpy;

static void trace_write(void *opaque, uint16_t address, uint8_t value) {
    VideoSpy *spy = (VideoSpy *)opaque;
    assert(spy->event_count < MAX_EVENTS);
    spy->events[spy->event_count].address = address;
    spy->events[spy->event_count].value = value;
    ++spy->event_count;

    if (address == 0x2000u) {
        spy->control = value;
    } else if (address == 0x2006u) {
        if (spy->address_toggle == 0u)
            spy->address_latch = (uint16_t)((uint16_t)(value & 0x3fu) << 8);
        else {
            spy->address_latch = (uint16_t)(spy->address_latch | value);
            spy->address = spy->address_latch;
        }
        spy->address_toggle ^= 1u;
    } else if (address == 0x2007u) {
        spy->address = (uint16_t)((spy->address +
            ((spy->control & 4u) != 0u ? 32u : 1u)) & 0x3fffu);
    }
}

static bool trace_run(void *opaque, uint16_t destination,
                      const uint8_t *source, uint8_t count,
                      uint16_t increment, bool repeat) {
    VideoSpy *spy = (VideoSpy *)opaque;
    ++spy->bulk_calls;
    if (destination != spy->address ||
        increment != ((spy->control & 4u) != 0u ? 32u : 1u) ||
        count == 0u)
        spy->metadata_ok = false;
    if (!spy->accept_bulk)
        return false;
    for (unsigned i = 0; i < count; ++i)
        trace_write(spy, 0x2007u, source[repeat ? 0u : i]);
    return true;
}

static uint8_t reference_nz(SmbNeoNativeContext *ctx, uint8_t value) {
    ctx->status = (uint8_t)((ctx->status &
        (uint8_t)~(NATIVE_N | NATIVE_Z)) |
        (value & NATIVE_N) | (value == 0u ? NATIVE_Z : 0u));
    return value;
}

static uint8_t reference_asl(SmbNeoNativeContext *ctx, uint8_t value) {
    ctx->status = (uint8_t)((ctx->status & (uint8_t)~NATIVE_C) |
        ((value & 0x80u) != 0u ? NATIVE_C : 0u));
    return reference_nz(ctx, (uint8_t)(value << 1));
}

static uint8_t reference_lsr(SmbNeoNativeContext *ctx, uint8_t value) {
    ctx->status = (uint8_t)((ctx->status & (uint8_t)~NATIVE_C) |
        ((value & 1u) != 0u ? NATIVE_C : 0u));
    return reference_nz(ctx, (uint8_t)(value >> 1));
}

static void reference_adc(SmbNeoNativeContext *ctx, uint8_t value) {
    const unsigned sum = (unsigned)ctx->a + value +
        ((ctx->status & NATIVE_C) != 0u ? 1u : 0u);
    const uint8_t result = (uint8_t)sum;
    const uint8_t overflow = (uint8_t)(
        (~(ctx->a ^ value) & (ctx->a ^ result) & 0x80u) != 0u ?
        NATIVE_V : 0u);
    ctx->status = (uint8_t)((ctx->status &
        (uint8_t)~(NATIVE_C | NATIVE_V)) |
        (sum > 255u ? NATIVE_C : 0u) | overflow);
    ctx->a = reference_nz(ctx, result);
}

/* Instruction-ordered semantic oracle for the supported $914a record body. */
static void reference_command(SmbNeoNativeContext *ctx) {
    uint8_t *ram = ctx->ram;
    const uint16_t source = (uint16_t)(ram[0] | ((uint16_t)ram[1] << 8));
    uint8_t saved;

    trace_write(ctx->opaque, 0x2006u, ctx->a);
    ctx->y = reference_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->a = reference_nz(ctx, ram[source + ctx->y]);
    trace_write(ctx->opaque, 0x2006u, ctx->a);
    ctx->y = reference_nz(ctx, (uint8_t)(ctx->y + 1u));
    ctx->a = reference_nz(ctx, ram[source + ctx->y]);
    ctx->a = reference_asl(ctx, ctx->a);
    saved = ctx->a;

    ctx->a = reference_nz(ctx, ram[0x0778u]);
    ctx->a = reference_nz(ctx, (uint8_t)(ctx->a | 4u));
    if ((ctx->status & NATIVE_C) == 0u)
        ctx->a = reference_nz(ctx, (uint8_t)(ctx->a & 0xfbu));
    trace_write(ctx->opaque, 0x2000u, ctx->a);
    ram[0x0778u] = ctx->a;

    ctx->a = reference_nz(ctx, saved);
    ctx->a = reference_asl(ctx, ctx->a);
    if ((ctx->status & NATIVE_C) != 0u) {
        ctx->a = reference_nz(ctx, (uint8_t)(ctx->a | 2u));
        ctx->y = reference_nz(ctx, (uint8_t)(ctx->y + 1u));
    }
    ctx->a = reference_lsr(ctx, ctx->a);
    ctx->a = reference_lsr(ctx, ctx->a);
    ctx->x = reference_nz(ctx, ctx->a);
    do {
        if ((ctx->status & NATIVE_C) == 0u)
            ctx->y = reference_nz(ctx, (uint8_t)(ctx->y + 1u));
        ctx->a = reference_nz(ctx, ram[source + ctx->y]);
        trace_write(ctx->opaque, 0x2007u, ctx->a);
        ctx->x = reference_nz(ctx, (uint8_t)(ctx->x - 1u));
    } while ((ctx->status & NATIVE_Z) == 0u);

    ctx->status |= NATIVE_C;
    ctx->a = reference_nz(ctx, ctx->y);
    reference_adc(ctx, ram[0]);
    ram[0] = ctx->a;
    ctx->a = reference_nz(ctx, 0u);
    reference_adc(ctx, ram[1]);
    ram[1] = ctx->a;
    ctx->a = reference_nz(ctx, 0x3fu);
    trace_write(ctx->opaque, 0x2006u, ctx->a);
    ctx->a = reference_nz(ctx, 0u);
    trace_write(ctx->opaque, 0x2006u, ctx->a);
    trace_write(ctx->opaque, 0x2006u, ctx->a);
    trace_write(ctx->opaque, 0x2006u, ctx->a);
}

static void prepare_case(SmbNeoNativeContext *ctx, VideoSpy *spy,
                         uint8_t ram[2048], uint8_t command,
                         uint8_t initial_status, unsigned seed) {
    const unsigned count = command & 0x3fu;
    const bool repeat = (command & 0x40u) != 0u;
    const unsigned max_source = repeat ? 0x03fcu : 0x03fdu - count;
    const unsigned source = 0x0300u + seed % (max_source - 0x0300u + 1u);

    memset(ram, 0, 2048u);
    memset(ctx, 0, sizeof(*ctx));
    memset(spy, 0, sizeof(*spy));
    spy->metadata_ok = true;
    ram[0] = (uint8_t)source;
    ram[1] = (uint8_t)(source >> 8);
    ram[source] = (uint8_t)(0x20u | ((seed >> 3) & 0x1fu));
    ram[source + 1u] = (uint8_t)(seed * 37u + command);
    ram[source + 2u] = command;
    for (unsigned i = 0; i < count; ++i)
        ram[source + 3u + (repeat ? 0u : i)] =
            (uint8_t)(seed + i * 73u + command * 11u);
    ram[0x0778u] = (uint8_t)(seed * 29u + 0xa5u);
    ctx->a = ram[source];
    ctx->x = (uint8_t)(seed >> 8);
    ctx->y = 0u;
    ctx->status = initial_status;
    ctx->ram = ram;
    ctx->opaque = spy;
    ctx->write8 = trace_write;
    ctx->calls = 0x12345678u;
    ctx->last_symbol = 0x914au;
}

static void compare_case(uint8_t command, uint8_t initial_status,
                         unsigned seed, int bulk_mode) {
    uint8_t reference_ram[2048];
    uint8_t actual_ram[2048];
    SmbNeoNativeContext reference;
    SmbNeoNativeContext actual;
    VideoSpy reference_spy;
    VideoSpy actual_spy;
    VsNativeFastVideoHooks hooks;

    prepare_case(&reference, &reference_spy, reference_ram, command,
                 initial_status, seed);
    prepare_case(&actual, &actual_spy, actual_ram, command,
                 initial_status, seed);
    reference_command(&reference);

    hooks.opaque = &actual_spy;
    hooks.ppu_data_run = trace_run;
    actual_spy.accept_bulk = bulk_mode > 0;
    assert(vs_native_fast_ppu_displist_command(
        &actual, bulk_mode < 0 ? NULL : &hooks));

    assert(actual.a == reference.a);
    assert(actual.x == reference.x);
    assert(actual.y == reference.y);
    assert(actual.status == reference.status);
    assert(actual.suspended == reference.suspended);
    assert(actual.unsupported == reference.unsupported);
    assert(actual.calls == reference.calls);
    assert(actual.last_symbol == reference.last_symbol);
    assert(memcmp(actual_ram, reference_ram, sizeof(actual_ram)) == 0);
    assert(actual_spy.metadata_ok);
    assert(actual_spy.event_count == reference_spy.event_count);
    assert(memcmp(actual_spy.events, reference_spy.events,
                  actual_spy.event_count * sizeof(actual_spy.events[0])) == 0);
    assert(actual_spy.address == reference_spy.address);
    assert(actual_spy.address_latch == reference_spy.address_latch);
    assert(actual_spy.address_toggle == reference_spy.address_toggle);
    assert(actual_spy.control == reference_spy.control);
    assert(actual_spy.bulk_calls == (bulk_mode < 0 ? 0u : 1u));
}

static void test_exhaustive_commands_flags_and_bulk(void) {
    for (unsigned command = 0; command < 256u; ++command) {
        if ((command & 0x3fu) == 0u)
            continue;
        for (unsigned status = 0; status < 256u; ++status) {
            const unsigned seed = command * 257u + status * 4051u;
            compare_case((uint8_t)command, (uint8_t)status, seed, -1);
            compare_case((uint8_t)command, (uint8_t)status, seed ^ 0x55aau, 1);
        }
        /* A declined callback is side-effect-free and selects scalar writes. */
        compare_case((uint8_t)command, (uint8_t)(command * 19u),
                     command * 313u, 0);
    }
}

static void assert_rejected_unchanged(SmbNeoNativeContext *ctx,
                                      VideoSpy *spy,
                                      const VsNativeFastVideoHooks *hooks) {
    uint8_t before_ram[2048];
    SmbNeoNativeContext before = *ctx;
    memcpy(before_ram, ctx->ram, sizeof(before_ram));
    assert(!vs_native_fast_ppu_displist_command(ctx, hooks));
    assert(memcmp(ctx, &before, sizeof(before)) == 0);
    assert(memcmp(ctx->ram, before_ram, sizeof(before_ram)) == 0);
    assert(spy->event_count == 0u && spy->bulk_calls == 0u);
}

static void test_all_rejections_are_transactional(void) {
    uint8_t ram[2048];
    SmbNeoNativeContext ctx;
    VideoSpy spy;
    VsNativeFastVideoHooks hooks = {&spy, trace_run};
    uint8_t *saved_ram;
    SmbNeoNativeWrite8 saved_write;

    prepare_case(&ctx, &spy, ram, 0x81u, 0x55u, 7u);
    assert(!vs_native_fast_ppu_displist_command(NULL, &hooks));

    saved_ram = ctx.ram;
    ctx.ram = NULL;
    assert(!vs_native_fast_ppu_displist_command(&ctx, &hooks));
    assert(spy.event_count == 0u && spy.bulk_calls == 0u);
    ctx.ram = saved_ram;

    saved_write = ctx.write8;
    ctx.write8 = NULL;
    assert_rejected_unchanged(&ctx, &spy, &hooks);
    ctx.write8 = saved_write;

    ctx.y = 1u;
    assert_rejected_unchanged(&ctx, &spy, &hooks);
    ctx.y = 0u;
    ctx.suspended = 1u;
    assert_rejected_unchanged(&ctx, &spy, &hooks);
    ctx.suspended = 0u;
    ctx.unsupported = 1u;
    assert_rejected_unchanged(&ctx, &spy, &hooks);
    ctx.unsupported = 0u;

    ram[0] = 0xffu; ram[1] = 0x02u;
    assert_rejected_unchanged(&ctx, &spy, &hooks);
    ram[0] = 0x00u; ram[1] = 0x0bu; /* CPU mirror is intentionally unsafe. */
    assert_rejected_unchanged(&ctx, &spy, &hooks);
    ram[0] = 0xfdu; ram[1] = 0x03u;
    assert_rejected_unchanged(&ctx, &spy, &hooks);

    prepare_case(&ctx, &spy, ram, 0x81u, 0x55u, 7u);
    ram[ram[0] | ((unsigned)ram[1] << 8)] ^= 1u;
    assert_rejected_unchanged(&ctx, &spy, &hooks);

    prepare_case(&ctx, &spy, ram, 0x81u, 0x55u, 7u);
    {
        const unsigned source = ram[0] | ((unsigned)ram[1] << 8);
        ram[source] = 0u;
        ctx.a = 0u; /* Display-list terminator belongs to the generic path. */
    }
    assert_rejected_unchanged(&ctx, &spy, &hooks);

    prepare_case(&ctx, &spy, ram, 0x81u, 0x55u, 7u);
    ram[(ram[0] | ((unsigned)ram[1] << 8)) + 2u] = 0x80u;
    assert_rejected_unchanged(&ctx, &spy, &hooks);

    prepare_case(&ctx, &spy, ram, 0x02u, 0x55u, 7u);
    ram[0] = 0xfcu; ram[1] = 0x03u;
    ram[0x03fcu] = ctx.a;
    ram[0x03feu] = 0x02u;
    assert_rejected_unchanged(&ctx, &spy, &hooks);
}

int main(void) {
    test_exhaustive_commands_flags_and_bulk();
    test_all_rejections_are_transactional();
    puts("native VS display-list fast path: exhaustive commands, flags, "
         "bulk/scalar ordering and transactional fallbacks passed");
    return 0;
}
