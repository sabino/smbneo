from __future__ import annotations

import shutil
import subprocess
import tempfile
import unittest
from unittest.mock import patch
import hashlib
import json
from pathlib import Path

import native_codegen
from check_native_codegen import violations
from native_codegen import (
    NativeSemanticRequirement,
    NativeSemanticRoutine,
    NativeSemanticSlice,
    REQUIRED_SYMBOLS,
    call_graph,
    data_stack_report,
    direct_bodies,
    emit_direct_c,
    emit_direct_header,
    emit_c,
    _emit_simple_op,
    generate,
    parse_symbols,
    saved_value_layouts,
    source_manifest,
    verified_native_semantics,
    verified_native_semantic_slices,
)


class NativeCodegenTests(unittest.TestCase):
    @staticmethod
    def _minimal_direct_source() -> str:
        code = {0x8000: ("RTS", "i", 0)}
        return emit_direct_c(
            code, {"start": 0x8000, "nmi": 0x8000}, {},
            {0x8000: {0x8000}},
            {"tsx_txs": [], "possible_data_underflow": [],
             "ambiguous_depths": [], "unbalanced_exits": [],
             "max_local_values": 0},
        )

    def test_semantic_routine_is_exact_fingerprint_gated(self):
        code = {0x8000: ("RTS", "i", 0)}
        digest = hashlib.sha256(json.dumps(
            [code[0x8000]], separators=(",", ":")
        ).encode()).hexdigest()
        spec = NativeSemanticRoutine(
            (0x8000,), digest, "native_fast_test", "native_fast_test.h"
        )
        with patch.dict(
            native_codegen._NATIVE_SEMANTIC_ROUTINES,
            {0x8000: spec},
            clear=True,
        ):
            verified = verified_native_semantics(
                code, {0x8000: {0x8000}}
            )
            self.assertEqual(verified, {0x8000: spec})
            output = emit_direct_c(
                code, {"start": 0x8000, "nmi": 0x8000}, {},
                {0x8000: {0x8000}},
            )
            self.assertIn('#include "native_fast_test.h"', output)
            self.assertIn("native_fast_test(ctx);", output)
            self.assertIn("#ifdef SMBNEO_NATIVE_FAST_PATHS", output)

            changed = {0x8000: ("BRK", "i", 0)}
            self.assertEqual(
                verified_native_semantics(changed, {0x8000: {0x8000}}), {}
            )
            moved = {0x8000: {0x8000, 0x8001}}
            self.assertEqual(verified_native_semantics(code, moved), {})

    def test_conditional_semantic_routine_gates_all_consumed_bodies(self):
        code = {
            0x8000: ("NOP", "i", 0),
            0x8001: ("RTS", "i", 0),
            0x8010: ("RTS", "i", 0),
        }
        bodies = {
            0x8000: {0x8000, 0x8001},
            0x8010: {0x8010},
        }

        def digest(addresses):
            return hashlib.sha256(json.dumps(
                [code[address] for address in addresses],
                separators=(",", ":"),
            ).encode()).hexdigest()

        dependency = NativeSemanticRequirement(
            0x8010, (0x8010,), digest((0x8010,))
        )
        spec = NativeSemanticRoutine(
            (0x8000, 0x8001),
            digest((0x8000, 0x8001)),
            "native_fast_conditional",
            "native_fast_conditional.h",
            conditional=True,
            requirements=(dependency,),
        )
        with patch.dict(
            native_codegen._NATIVE_SEMANTIC_ROUTINES,
            {0x8000: spec},
            clear=True,
        ):
            self.assertEqual(
                verified_native_semantics(code, bodies), {0x8000: spec}
            )
            output = emit_direct_c(
                code, {"start": 0x8000, "nmi": 0x8000}, {}, bodies
            )
            self.assertIn(
                "if (native_fast_conditional(ctx)) return;", output
            )
            self.assertIn('#include "native_fast_conditional.h"', output)

            changed_dependency = dict(code)
            changed_dependency[0x8010] = ("BRK", "i", 0)
            self.assertEqual(
                verified_native_semantics(changed_dependency, bodies), {}
            )
            widened_dependency = dict(bodies)
            widened_dependency[0x8010] = {0x8010, 0x8011}
            self.assertEqual(
                verified_native_semantics(code, widened_dependency), {}
            )

    def test_actor_semantic_spec_is_conditional_and_dependency_complete(self):
        actor = native_codegen._NATIVE_SEMANTIC_ROUTINES[0xe7da]
        self.assertTrue(actor.conditional)
        self.assertEqual(len(actor.addresses), 385)
        self.assertEqual(
            {requirement.entry for requirement in actor.requirements},
            {0xeb07, 0xeb0f, 0xe512, 0xe518, 0xe51b, 0xe51e,
             0xeb1e, 0xeba7},
        )
        self.assertTrue(
            native_codegen._NATIVE_SEMANTIC_ROUTINES[0xeb07].conditional
        )
        self.assertTrue(
            native_codegen._NATIVE_SEMANTIC_ROUTINES[0xeb0f].conditional
        )

    def test_actor_policy_semantics_are_exact_and_dependency_complete(self):
        routines = native_codegen._NATIVE_SEMANTIC_ROUTINES
        base = routines[0xc9ba]
        oob = routines[0xd5bd]
        self.assertTrue(base.conditional)
        self.assertTrue(oob.conditional)
        self.assertEqual(base.function, "vs_native_fast_actor_proc_base")
        self.assertEqual(oob.function, "vs_native_fast_actor_oob_proc")
        self.assertEqual(len(base.addresses), 67)
        self.assertEqual(len(oob.addresses), 44)
        self.assertEqual(
            {requirement.entry for requirement in base.requirements},
            {0xc8db, 0xbe72, 0xbe7a, 0xbe11, 0xbe1e, 0xbea5,
             0xbebe, 0xbeea},
        )
        self.assertEqual(
            {requirement.entry for requirement in oob.requirements},
            {0xc8db},
        )

    def test_collision_position_semantics_are_dependency_complete(self):
        routines = native_codegen._NATIVE_SEMANTIC_ROUTINES
        expected = {
            0xe1f2: ("vs_native_fast_col_box_proc", set()),
            0xe348: ("vs_native_fast_col_box_buffer", {0xab14}),
            0xf15b: ("vs_native_fast_pos_bits_get_do_x", {0xf1d2}),
            0xf13c: ("vs_native_fast_pos_bits_proc", {0xf15b, 0xf1d2}),
            0xf114: ("vs_native_fast_pos_bits_get_actor",
                     {0xf13c, 0xf15b, 0xf1d2}),
        }
        for entry, (function, dependencies) in expected.items():
            self.assertTrue(routines[entry].conditional)
            self.assertEqual(routines[entry].function, function)
            self.assertEqual(
                {requirement.entry for requirement in
                 routines[entry].requirements},
                dependencies,
            )

    def test_meta_and_game_coordinators_are_native_semantics(self):
        routines = native_codegen._NATIVE_SEMANTIC_ROUTINES
        self.assertTrue(routines[0x8aaf].conditional)
        self.assertEqual(
            routines[0x8aaf].function, "vs_native_fast_meta_render_area"
        )
        self.assertEqual(len(routines[0x8aaf].addresses), 92)
        self.assertFalse(routines[0xad6e].conditional)
        self.assertEqual(
            routines[0xad6e].function, "vs_native_fast_game_proc"
        )
        self.assertEqual(len(routines[0xad6e].addresses), 72)

    def test_interior_semantic_slice_is_exact_and_transactional(self):
        code = {
            0x8000: ("NOP", "i", 0),
            0x8001: ("RTS", "i", 0),
            0x8010: ("RTS", "i", 0),
            0x8020: ("RTS", "i", 0),
        }
        bodies = {
            0x8010: {0x8000, 0x8001, 0x8010},
            0x8020: {0x8020},
        }

        def digest(addresses):
            return hashlib.sha256(json.dumps(
                [code[address] for address in addresses],
                separators=(",", ":"),
            ).encode()).hexdigest()

        requirement = NativeSemanticRequirement(
            0x8020, (0x8020,), digest((0x8020,))
        )
        spec = NativeSemanticSlice(
            owner=0x8010,
            start=0x8000,
            continuation=0x8010,
            addresses=(0x8000, 0x8001),
            digest=digest((0x8000, 0x8001)),
            function="native_fast_slice",
            header="native_fast_slice.h",
            arguments=("ctx", "ctx->fast_video_hooks"),
            requirements=(requirement,),
        )
        with patch.object(
            native_codegen, "_NATIVE_SEMANTIC_SLICES", (spec,)
        ), patch.dict(
            native_codegen._NATIVE_SEMANTIC_ROUTINES, {}, clear=True
        ):
            self.assertEqual(
                verified_native_semantic_slices(code, bodies),
                {(0x8010, 0x8000): spec},
            )
            output = emit_direct_c(
                code, {"start": 0x8010, "nmi": 0x8020}, {}, bodies
            )
            self.assertIn('#include "native_fast_slice.h"', output)
            self.assertIn(
                "if (native_fast_slice(ctx, ctx->fast_video_hooks)) "
                "goto L8010;",
                output,
            )

            changed = dict(code)
            changed[0x8000] = ("CLC", "i", 0)
            self.assertEqual(
                verified_native_semantic_slices(changed, bodies), {}
            )
            widened = dict(bodies)
            widened[0x8010] = bodies[0x8010] | {0x8002}
            changed_with_extra = dict(code)
            changed_with_extra[0x8002] = ("NOP", "i", 0)
            self.assertEqual(
                verified_native_semantic_slices(changed_with_extra, widened),
                {},
            )

    def test_direct_context_carries_per_instance_video_hooks(self):
        header = emit_direct_header()
        self.assertIn("struct VsNativeFastVideoHooks;", header)
        self.assertIn(
            "const struct VsNativeFastVideoHooks *fast_video_hooks;",
            header,
        )

    def test_nmi_semantics_cover_complete_routines_and_exact_slices(self):
        routines = native_codegen._NATIVE_SEMANTIC_ROUTINES
        self.assertEqual(
            {routines[entry].function for entry in (0x8212, 0x826e, 0x8273)},
            {
                "vs_native_fast_nmi_oam_shuffle",
                "vs_native_fast_oam_init",
                "vs_native_fast_oam_init_all",
            },
        )
        nmi_slices = {
            semantic.start: semantic
            for semantic in native_codegen._NATIVE_SEMANTIC_SLICES
            if semantic.owner == 0x810a
        }
        self.assertEqual(set(nmi_slices), {0x817e, 0x81a3, 0x81d9})
        self.assertEqual(
            {semantic.continuation for semantic in nmi_slices.values()},
            {0x81a3, 0x81c0, 0x81de},
        )
        self.assertTrue(all(
            not semantic.conditional for semantic in nmi_slices.values()
        ))

    def test_unconditional_slice_calls_then_continues(self):
        code = {
            0x8000: ("NOP", "i", 0),
            0x8001: ("RTS", "i", 0),
            0x8010: ("RTS", "i", 0),
        }
        digest = hashlib.sha256(json.dumps(
            [code[0x8000], code[0x8001]], separators=(",", ":")
        ).encode()).hexdigest()
        spec = NativeSemanticSlice(
            owner=0x8010,
            start=0x8000,
            continuation=0x8010,
            addresses=(0x8000, 0x8001),
            digest=digest,
            function="native_fast_unconditional",
            header="native_fast_unconditional.h",
            conditional=False,
        )
        with patch.object(
            native_codegen, "_NATIVE_SEMANTIC_SLICES", (spec,)
        ), patch.dict(
            native_codegen._NATIVE_SEMANTIC_ROUTINES, {}, clear=True
        ):
            output = emit_direct_c(
                code,
                {"start": 0x8010, "nmi": 0x8010},
                {},
                {0x8010: {0x8000, 0x8001, 0x8010}},
            )
        self.assertIn("native_fast_unconditional(ctx);", output)
        self.assertIn("goto L8010;", output)
        self.assertNotIn("if (native_fast_unconditional", output)

    def test_hot_primitive_helpers_are_force_inline_portably(self):
        output = self._minimal_direct_source()
        self.assertIn(
            "#define SMBNEO_NATIVE_FORCE_INLINE static inline "
            "__attribute__((always_inline))",
            output,
        )
        self.assertIn(
            "#define SMBNEO_NATIVE_FORCE_INLINE static inline\n",
            output,
        )
        for helper in (
            "native_read", "native_write", "native_zpword", "native_nz",
            "native_cmp", "native_bit", "native_adc", "native_sbc",
            "native_asl", "native_lsr", "native_rol", "native_ror",
        ):
            self.assertRegex(
                output,
                rf"SMBNEO_NATIVE_FORCE_INLINE [^\n]*\b{helper}\(",
            )

    def test_diagnostics_are_compile_time_opt_in_and_boot_safe(self):
        compiler = shutil.which("cc")
        if compiler is None:
            self.skipTest("host C compiler is unavailable")

        generated = self._minimal_direct_source()
        harness = r'''\
#include <assert.h>
#include <stdint.h>
#include "smbneo_native.c"

int main(void) {
    SmbNeoNativeContext ctx = {0};
    ctx.calls = 0x12345678u;
    ctx.last_symbol = 0x5678u;
    smbneo_native_boot(&ctx);
#ifdef SMBNEO_NATIVE_DIAGNOSTICS
    assert(ctx.calls == 1u);
    assert(ctx.last_symbol == 0x8000u);
    smbneo_native_frame(&ctx);
    assert(ctx.calls == 2u);
    assert(ctx.last_symbol == 0x8000u);
#else
    assert(ctx.calls == 0x12345678u);
    assert(ctx.last_symbol == 0x5678u);
    smbneo_native_frame(&ctx);
    assert(ctx.calls == 0x12345678u);
    assert(ctx.last_symbol == 0x5678u);
#endif
    return 0;
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "smbneo_native.h").write_text(
                emit_direct_header(), encoding="utf-8"
            )
            (root / "smbneo_native.c").write_text(generated, encoding="utf-8")
            (root / "test.c").write_text(harness, encoding="utf-8")
            for diagnostics in (False, True):
                executable = root / ("test-diag" if diagnostics else "test-production")
                command = [
                    compiler, "-std=c11", "-O2", "-Wall", "-Wextra",
                    "-Werror", "-Wno-unused-function", "-pedantic",
                ]
                if diagnostics:
                    command.append("-DSMBNEO_NATIVE_DIAGNOSTICS")
                command += [str(root / "test.c"), "-o", str(executable)]
                subprocess.run(command, check=True)
                subprocess.run([str(executable)], check=True)

    def test_native_memory_fast_path_mirrors_ranges_and_bypasses_callbacks(self):
        compiler = shutil.which("cc")
        if compiler is None:
            self.skipTest("host C compiler is unavailable")

        code = {0x8000: ("RTS", "i", 0)}
        generated = emit_direct_c(
            code, {"start": 0x8000, "nmi": 0x8000}, {},
            {0x8000: {0x8000}},
            {"tsx_txs": [], "possible_data_underflow": [],
             "ambiguous_depths": [], "unbalanced_exits": [],
             "max_local_values": 0},
        )
        harness = r'''\
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "smbneo_native.c"

typedef struct {
    unsigned reads, writes;
    uint16_t last_address;
    uint8_t last_value;
} Probe;

static uint8_t probe_read(void *opaque, uint16_t address) {
    Probe *probe = (Probe *)opaque;
    probe->reads++;
    probe->last_address = address;
    return (uint8_t)(address ^ 0xa5u);
}

static void probe_write(void *opaque, uint16_t address, uint8_t value) {
    Probe *probe = (Probe *)opaque;
    probe->writes++;
    probe->last_address = address;
    probe->last_value = value;
}

int main(void) {
    uint8_t ram[0x800], extra_ram[0x800], prg[0x8000];
    Probe probe = {0};
    SmbNeoNativeContext ctx = {0};
    ctx.ram = ram;
    ctx.extra_ram = extra_ram;
    ctx.prg = prg;
    ctx.opaque = &probe;
    ctx.read8 = probe_read;
    ctx.write8 = probe_write;

    memset(ram, 0, sizeof(ram));
    memset(extra_ram, 0, sizeof(extra_ram));
    memset(prg, 0, sizeof(prg));
    ram[0x000] = 0x10; ram[0x7ff] = 0x11;
    extra_ram[0x000] = 0x20; extra_ram[0x7ff] = 0x21;
    prg[0x0000] = 0x30; prg[0x7fff] = 0x31;

    assert(native_read(&ctx, 0x0000) == 0x10);
    assert(native_read(&ctx, 0x0800) == 0x10);
    assert(native_read(&ctx, 0x17ff) == 0x11);
    assert(native_read(&ctx, 0x6000) == 0x20);
    assert(native_read(&ctx, 0x6800) == 0x20);
    assert(native_read(&ctx, 0x7fff) == 0x21);
    assert(native_read(&ctx, 0x8000) == 0x30);
    assert(native_read(&ctx, 0xffff) == 0x31);
    assert(probe.reads == 0);

    native_write(&ctx, 0x1fff, 0x41);
    native_write(&ctx, 0x7800, 0x42);
    native_write(&ctx, 0x8000, 0x43);
    assert(ram[0x7ff] == 0x41);
    assert(extra_ram[0x000] == 0x42);
    assert(prg[0x0000] == 0x30);
    assert(probe.writes == 0);

    assert(native_read(&ctx, 0x2000) == (uint8_t)(0x2000 ^ 0xa5));
    assert(native_read(&ctx, 0x5fff) == (uint8_t)(0x5fff ^ 0xa5));
    native_write(&ctx, 0x2000, 0x51);
    native_write(&ctx, 0x5fff, 0x52);
    assert(probe.reads == 2);
    assert(probe.writes == 2);
    assert(probe.last_address == 0x5fff);
    assert(probe.last_value == 0x52);

    ctx.unsupported = 0; ctx.ram = 0;
    assert(native_read(&ctx, 0x0000) == 0 && ctx.unsupported);
    ctx.unsupported = 0; ctx.extra_ram = 0;
    native_write(&ctx, 0x6000, 1); assert(ctx.unsupported);
    ctx.unsupported = 0; ctx.prg = 0;
    assert(native_read(&ctx, 0x8000) == 0 && ctx.unsupported);
    ctx.unsupported = 0;
    native_write(&ctx, 0xffff, 1); assert(!ctx.unsupported);
    ctx.unsupported = 0; ctx.read8 = 0;
    assert(native_read(&ctx, 0x4000) == 0 && ctx.unsupported);
    ctx.unsupported = 0; ctx.write8 = 0;
    native_write(&ctx, 0x4000, 1); assert(ctx.unsupported);
    return 0;
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "smbneo_native.h").write_text(
                emit_direct_header(), encoding="utf-8"
            )
            (root / "smbneo_native.c").write_text(generated, encoding="utf-8")
            (root / "test.c").write_text(harness, encoding="utf-8")
            executable = root / "test"
            subprocess.run(
                [compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
                 "-Wno-unused-function", "-pedantic", str(root / "test.c"),
                 "-o", str(executable)],
                check=True,
            )
            subprocess.run([str(executable)], check=True)

    def test_debug_symbols_are_resolved_without_paths(self):
        debug = '\n'.join(
            f'sym\tid={i},name="{name}",addrsize=absolute,val=0x{0x8000 + i:04x},type=lab'
            for i, name in enumerate(REQUIRED_SYMBOLS)
        )
        symbols = parse_symbols(debug)
        self.assertEqual(symbols["start"], 0x8000)
        self.assertEqual(len(symbols), len(REQUIRED_SYMBOLS))

    def test_call_graph_is_direct_and_named(self):
        code = {
            0x8000: ("JSR", "w", 0x8010),
            0x8003: ("RTS", "i", 0),
            0x8010: ("RTS", "i", 0),
        }
        graph = call_graph(code, {"start": 0x8000, "sys_title": 0x8010}, ("start",))
        self.assertEqual(graph, {"start": ["sys_title"]})
        self.assertNotIn("runtime PC", emit_c({"start": 0x8000}, {}))

    def test_register_transfers_preserve_6502_direction_and_flags(self):
        expected = {
            "TAX": "ctx->x = native_nz(ctx, ctx->a);",
            "TAY": "ctx->y = native_nz(ctx, ctx->a);",
            "TXA": "ctx->a = native_nz(ctx, ctx->x);",
            "TYA": "ctx->a = native_nz(ctx, ctx->y);",
        }
        for op, statement in expected.items():
            self.assertEqual(_emit_simple_op(0x8000, op, "i", 0), [statement])

    def test_policy_rejects_translated_runtime_constructs(self):
        self.assertEqual(violations("void f(void) { return; }"), [])
        self.assertEqual(violations("goto L1234;"), [])
        self.assertEqual(violations("vs_native_fast_motion_x(ctx);"), [])
        self.assertTrue(violations("VsCpu *c; c->pc = 0x8000; vs_push(c, 1);"))

    def test_tbljmp_is_lowered_to_ordered_direct_switch_with_side_effects(self):
        code = {
            0x8000: ("JSR", "w", 0x9000),
            0x8003: ("RTS", "i", 0),
            0x8010: ("RTS", "i", 0),
        }
        symbols = {"start": 0x8000, "nmi": 0x8010, "tbljmp": 0x9000}
        bodies, _ = direct_bodies(code, {0x8000, 0x8010})
        output = emit_direct_c(
            code, symbols, {0x8000: [0x8010]}, bodies,
            {"tsx_txs": [], "possible_data_underflow": []},
        )
        self.assertIn("switch (table_selector_8000)", output)
        self.assertIn("native_write(ctx, 0x04u, 0x02u)", output)
        self.assertIn("native_write(ctx, 0x06u, 0x10u)", output)
        self.assertNotIn("fn_tbljmp_9000(ctx);", output)
        self.assertEqual(violations(output), [])

    def test_function_with_lower_predecessor_starts_at_its_entry(self):
        code = {
            0x8000: ("RTS", "i", 0),
            0x8010: ("JMP", "w", 0x8000),
        }
        output = emit_direct_c(
            code, {"start": 0x8010, "nmi": 0x8010}, {},
            {0x8010: {0x8000, 0x8010}},
            {"tsx_txs": [], "possible_data_underflow": []},
        )
        function = output[output.index("void smbneo_native_fn_nmi_8010"):]
        self.assertLess(function.index("goto L8010;"), function.index("    return;"))
        self.assertIn("L8010: ;", function)

    def test_frame_restores_interrupted_status_after_native_nmi(self):
        code = {0x8000: ("RTI", "i", 0)}
        output = emit_direct_c(
            code, {"start": 0x8000, "nmi": 0x8000}, {},
            {0x8000: {0x8000}},
            {"tsx_txs": [], "possible_data_underflow": []},
        )
        self.assertIn("uint8_t interrupted_status = ctx->status", output)
        self.assertIn(
            "(interrupted_status | NATIVE_U) & (uint8_t)~NATIVE_B",
            output,
        )

    def test_data_stack_report_tracks_cfg_and_separates_call_frames(self):
        code = {
            0x8000: ("PHA", "i", 0),
            0x8001: ("PLA", "i", 0),
            0x8002: ("RTS", "i", 0),
            0x8010: ("TXS", "i", 0),
            0x8011: ("RTS", "i", 0),
        }
        report = data_stack_report(code, {0x8000: {0x8000, 0x8001, 0x8002}, 0x8010: {0x8010, 0x8011}})
        self.assertEqual(report["possible_data_underflow"], [])
        self.assertEqual(report["tsx_txs"], ["0x8010"])

    def test_balanced_processor_saves_become_scalar_c_locals(self):
        code = {
            0x8000: ("PHA", "i", 0),
            0x8001: ("PHP", "i", 0),
            0x8002: ("PLA", "i", 0),
            0x8003: ("PLP", "i", 0),
            0x8004: ("RTS", "i", 0),
        }
        bodies = {0x8000: set(code)}
        layouts, report = saved_value_layouts(code, bodies)
        self.assertEqual(report["max_local_values"], 2)
        self.assertEqual(report["possible_data_underflow"], [])
        self.assertEqual(report["unbalanced_exits"], [])
        output = emit_direct_c(
            code, {"start": 0x8000, "nmi": 0x8000}, {}, bodies,
            report, layouts,
        )
        self.assertIn("uint8_t saved_value_0 = 0u;", output)
        self.assertIn("uint8_t saved_value_1 = 0u;", output)
        self.assertIn("saved_value_0 = ctx->a;", output)
        self.assertIn(
            "saved_value_1 = (uint8_t)(ctx->status | NATIVE_B | NATIVE_U);",
            output,
        )
        self.assertIn("ctx->a = native_nz(ctx, saved_value_1);", output)
        self.assertIn(
            "ctx->status = (uint8_t)((saved_value_0 | NATIVE_U) & (uint8_t)~NATIVE_B);",
            output,
        )
        forbidden = ("data_stack", "data_sp", "data_underflow", "native_push", "native_pop")
        for token in forbidden:
            self.assertNotIn(token, emit_direct_header())
            self.assertNotIn(token, output)

    def test_complementary_branches_do_not_invent_stack_path(self):
        code = {
            0x8000: ("PHA", "i", 0),
            0x8001: ("BEQ", "r", 0x05),       # retained-depth path -> 8008
            0x8003: ("PLA", "i", 0),
            0x8004: ("BCC", "r", 0x05),       # -> 800b
            0x8006: ("BCS", "r", 0x04),       # -> 800c; no fallthrough
            0x8008: ("PLA", "i", 0),
            0x8009: ("RTS", "i", 0),
            0x800b: ("RTS", "i", 0),
            0x800c: ("RTS", "i", 0),
        }
        layouts, report = saved_value_layouts(code, {0x8000: set(code)})
        self.assertEqual(report["possible_data_underflow"], [])
        self.assertEqual(report["ambiguous_depths"], [])
        self.assertEqual(layouts[0x8000].depth_at[0x8008], 1)

    def test_unbalanced_processor_save_is_a_generation_error(self):
        code = {0x8000: ("PLA", "i", 0), 0x8001: ("RTS", "i", 0)}
        with self.assertRaisesRegex(ValueError, "possible_data_underflow"):
            emit_direct_c(
                code, {"start": 0x8000, "nmi": 0x8000}, {},
                {0x8000: set(code)},
            )

    def test_terminal_vs_irq_frame_discards_need_no_runtime_storage(self):
        code = {
            0x8000: ("RTS", "i", 0),
            0x8100: ("PLA", "i", 0),
            0x8101: ("PLA", "i", 0),
            0x8102: ("PLA", "i", 0),
            0x8103: ("RTS", "i", 0),
        }
        bodies = {0x8000: {0x8000}, 0x8100: {0x8100, 0x8101, 0x8102, 0x8103}}
        layouts, report = saved_value_layouts(
            code, bodies, hardware_frame_entries={0x8100}
        )
        self.assertEqual(
            report["hardware_frame_discards"],
            ["0x8100", "0x8101", "0x8102"],
        )
        self.assertEqual(report["possible_data_underflow"], [])
        output = emit_direct_c(
            code, {"start": 0x8000, "nmi": 0x8000, "irq": 0x8100}, {},
            bodies, report, layouts,
        )
        self.assertEqual(
            output.count("processor-created interrupt frame absent in native C"),
            3,
        )
        self.assertNotIn("native_pop", output)

    def test_source_manifest_requires_vs_configuration(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "src").mkdir()
            (root / "src" / "main.s").write_text("nop\n", encoding="latin1")
            (root / "Makefile").write_text(
                "ASFLAGS_VS = -DVS -DVS_2C04_0004 -DVS_SMB -DSMBV1\n",
                encoding="latin1",
            )
            manifest = source_manifest(root)
            self.assertEqual(manifest["module_count"], 1)
            self.assertEqual(manifest["defines"], ["VS", "VS_2C04_0004", "VS_SMB", "SMBV1"])

    def test_synthetic_generation_is_rom_free_and_buildable_metadata(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "source"
            source.mkdir()
            (source / "src").mkdir()
            (source / "src" / "main.s").write_text("\n".join(["nop"] * 7) + "\n", encoding="latin1")
            (source / "Makefile").write_text(
                "ASFLAGS_VS = -DVS -DVS_2C04_0004 -DVS_SMB -DSMBV1\n",
                encoding="latin1",
            )
            addresses = [0x8000 + i for i in range(len(REQUIRED_SYMBOLS))]
            debug_lines = [
                'seg\tid=0,name="CODE",start=0x8000,size=0x8000',
                'file\tid=0,name="src/main.s"',
            ]
            for i, (name, address) in enumerate(zip(REQUIRED_SYMBOLS, addresses)):
                debug_lines += [
                    f"span\tid={i},seg=0,start=0x{i:x},size=1",
                    f"line\tfile=0,line={i + 1},span={i}",
                    f'sym\tid={i},name="{name}",addrsize=absolute,val=0x{address:04x},type=lab',
                ]
            (source / "src" / "main.s").write_text("\n".join(["nop"] * len(addresses)) + "\n", encoding="latin1")
            debug = root / "vs.dbg"
            debug.write_text("\n".join(debug_lines) + "\n", encoding="latin1")
            prg = root / "vsmain.vs.bin"
            prg.write_bytes(bytes([0xEA]) * 0x8000)
            _, manifest = generate(prg, debug, source)
            self.assertEqual(manifest["prg_size"], 0x8000)
            self.assertNotIn("prg", manifest["runtime_contract"]["rom_data"])
            self.assertEqual(violations(manifest["c_source"]), [])


if __name__ == "__main__":
    unittest.main()
