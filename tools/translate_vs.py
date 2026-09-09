#!/usr/bin/env python3
"""Statically translate ca65-marked VS instructions into portable C.

Requires the verified user's PRG and the matching reference's linker debug map.
Data spans are never guessed to be code. Generated game source stays private in
the ignored build directory. Unsupported opcodes and unmapped transfers fail
closed. There is no runtime ROM opcode fetch/decode loop in the generated code.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re

# Official RP2A03 instructions, organized by opcode. '-' is unsupported.
ROWS = (
"BRK:i ORA:ix - - - ORA:z ASL:z - PHP:i ORA:n ASL:a - - ORA:w ASL:w -",
"BPL:r ORA:iy - - - ORA:zx ASL:zx - CLC:i ORA:wy - - - ORA:wx ASL:wx -",
"JSR:w AND:ix - - BIT:z AND:z ROL:z - PLP:i AND:n ROL:a - BIT:w AND:w ROL:w -",
"BMI:r AND:iy - - - AND:zx ROL:zx - SEC:i AND:wy - - - AND:wx ROL:wx -",
"RTI:i EOR:ix - - - EOR:z LSR:z - PHA:i EOR:n LSR:a - JMP:w EOR:w LSR:w -",
"BVC:r EOR:iy - - - EOR:zx LSR:zx - CLI:i EOR:wy - - - EOR:wx LSR:wx -",
"RTS:i ADC:ix - - - ADC:z ROR:z - PLA:i ADC:n ROR:a - JMP:ind ADC:w ROR:w -",
"BVS:r ADC:iy - - - ADC:zx ROR:zx - SEI:i ADC:wy - - - ADC:wx ROR:wx -",
"- STA:ix - - STY:z STA:z STX:z - DEY:i - TXA:i - STY:w STA:w STX:w -",
"BCC:r STA:iy - - STY:zx STA:zx STX:zy - TYA:i STA:wy TXS:i - - STA:wx - -",
"LDY:n LDA:ix LDX:n - LDY:z LDA:z LDX:z - TAY:i LDA:n TAX:i - LDY:w LDA:w LDX:w -",
"BCS:r LDA:iy - - LDY:zx LDA:zx LDX:zy - CLV:i LDA:wy TSX:i - LDY:wx LDA:wx LDX:wy -",
"CPY:n CMP:ix - - CPY:z CMP:z DEC:z - INY:i CMP:n DEX:i - CPY:w CMP:w DEC:w -",
"BNE:r CMP:iy - - - CMP:zx DEC:zx - CLD:i CMP:wy - - - CMP:wx DEC:wx -",
"CPX:n SBC:ix - - CPX:z SBC:z INC:z - INX:i SBC:n NOP:i - CPX:w SBC:w INC:w -",
"BEQ:r SBC:iy - - - SBC:zx INC:zx - SED:i SBC:wy - - - SBC:wx INC:wx -",
)
OPS = [entry for row in ROWS for entry in row.split()]
LENGTH = dict(i=1, a=1, n=2, z=2, zx=2, zy=2, ix=2, iy=2, r=2,
              w=3, wx=3, wy=3, ind=3)


def instructions(prg: bytes, debug: str, source_root: Path) -> dict[int, tuple[str, str, int]]:
    if len(prg) != 32768:
        raise ValueError("VS PRG must be exactly 32768 bytes")
    segments = {}
    spans = []
    files = {}
    source_lines = []
    for line in debug.splitlines():
        kind, _, rest = line.partition('\t')
        if kind not in ('seg', 'span', 'file', 'line'):
            continue
        fields = dict(re.findall(r'(\w+)=("[^"]*"|[^,]+)', rest))
        if kind == 'seg':
            segments[fields['id']] = fields
        elif kind == 'span':
            spans.append(fields)
        elif kind == 'file':
            path = (source_root / json.loads(fields['name'])).resolve()
            if not path.is_relative_to(source_root.resolve()):
                raise ValueError('Reference source path escapes checkout')
            # ca65 also records incbin assets; instruction tokens are ASCII.
            files[fields['id']] = path.read_text(encoding='latin1').splitlines()
        elif 'span' in fields:
            source_lines.append(fields)
    marked = {}
    mnemonics = {entry.split(':')[0].lower() for entry in OPS if entry != '-'}
    for line in source_lines:
        text = files[line['file']][int(line['line']) - 1].split(';')[0].strip()
        text = re.sub(r'^(?:[A-Za-z_][\w]*)?:\s*', '', text)
        word = text.split()[0].lower() if text else ''
        if word in mnemonics:
            for span_id in line['span'].split('+'):
                marked[span_id] = word.upper()
    result = {}
    for span in spans:
        seg = segments[span['seg']]
        # Aggregate scopes have larger spans. Typed spans are explicit data.
        size = int(span['size'], 0)
        if seg.get('name') != '"CODE"' or 'type' in span or span['id'] not in marked:
            continue
        address = int(seg['start'], 0) + int(span['start'], 0)
        offset = address - 0x8000
        if not 0 <= offset <= len(prg) - size:
            raise ValueError(f"Invalid code span at {address:04x}")
        entry = OPS[prg[offset]]
        if entry == '-':
            raise ValueError(f"Unsupported opcode at {address:04x}")
        op, mode = entry.split(':')
        if LENGTH[mode] != size or marked[span['id']] != op:
            raise ValueError(f"Code/debug mismatch at {address:04x}")
        operand = int.from_bytes(prg[offset + 1:offset + size], 'little')
        result[address] = op, mode, operand
    if not result:
        raise ValueError("No CODE instruction spans")
    end = 0
    for address, (_, mode, _) in sorted(result.items()):
        if address < end:
            raise ValueError(f"Overlapping instruction spans at {address:04x}")
        end = address + LENGTH[mode]
    return result


def live_flags(code):
    """Backward flag liveness; every page exit/call/unknown transfer is a barrier.

    Never infer targets from ROM data or assume anything about a callee. Keep
    the full status at dispatcher boundaries and PHP/PLP/RTI. This deliberately
    leaves interprocedural opportunities alone until independently verified.
    """
    nz, carry, overflow, all_flags = 0x82, 1, 0x40, 0xff
    branches = dict(BPL=0x80, BMI=0x80, BVC=overflow, BVS=overflow,
                    BCC=carry, BCS=carry, BNE=2, BEQ=2)
    reads, writes, successors = {}, {}, {}
    for a, (op, mode, value) in code.items():
        nxt = a + LENGTH[mode]
        read, write = 0, 0
        edges = [nxt]
        if op in ('LDA', 'LDX', 'LDY', 'AND', 'ORA', 'EOR', 'INC', 'DEC',
                  'INX', 'INY', 'DEX', 'DEY', 'TAX', 'TAY', 'TXA', 'TYA', 'TSX', 'PLA'):
            write = nz
        elif op in ('ADC', 'SBC'):
            read, write = carry, nz | carry | overflow
        elif op in ('CMP', 'CPX', 'CPY'): write = nz | carry
        elif op == 'BIT': write = nz | overflow
        elif op in ('ASL', 'LSR', 'ROL', 'ROR'):
            read, write = (carry if op in ('ROL', 'ROR') else 0), nz | carry
        elif op in branches:
            read = branches[op]
            edges.append((nxt + (value if value < 128 else value - 256)) & 0xffff)
        elif op in ('CLC', 'SEC', 'CLI', 'SEI', 'CLD', 'SED', 'CLV'):
            write = dict(C=carry, I=4, D=8, V=overflow)[op[-1]]
        elif op == 'JMP' and mode == 'w':
            edges = [value]
            if value == a: read = all_flags  # idle returns to frame/NMI caller
        elif op in ('JSR', 'RTS', 'RTI', 'JMP', 'PHP', 'PLP'):
            read = all_flags  # Status stack, callee and dynamic targets opaque.
        elif op not in ('STA', 'STX', 'STY', 'PHA', 'TXS', 'NOP'):
            read = all_flags
        reads[a], writes[a] = read, write
        successors[a] = edges
    before = {a: reads[a] for a in code}
    after = dict.fromkeys(code, 0)
    changed = True
    while changed:
        changed = False
        for a in reversed(sorted(code)):
            out = 0
            for edge in successors[a]:
                out |= before[edge] if edge in code and edge >> 8 == a >> 8 else all_flags
            incoming = reads[a] | (out & ~writes[a])
            if before[a] != incoming: changed = True
            before[a], after[a] = incoming, out
    return after


def emit_instruction(address: int, op: str, mode: str, operand: int, live: int = 0xff) -> list[str]:
    nxt = address + LENGTH[mode]
    addr = {
        'z': f'0x{operand:02x}', 'w': f'0x{operand:04x}',
        'zx': f'(uint8_t)(0x{operand:02x} + c->x)',
        'zy': f'(uint8_t)(0x{operand:02x} + c->y)',
        'wx': f'(uint16_t)(0x{operand:04x} + c->x)',
        'wy': f'(uint16_t)(0x{operand:04x} + c->y)',
        'ix': f'vs_zpword(c, (uint8_t)(0x{operand:02x} + c->x))',
        'iy': f'(uint16_t)(vs_zpword(c, 0x{operand:02x}) + c->y)',
    }.get(mode)
    direct = None
    if mode in ('z', 'zx', 'zy'):
        direct = 'c->bus->ram[addr]'
    elif mode in ('w', 'wx', 'wy'):
        upper = operand + (255 if mode != 'w' else 0)
        if upper < 0x2000: direct = 'c->bus->ram[addr & 0x7ff]'
        elif 0x6000 <= operand and upper < 0x8000: direct = 'c->bus->extra_ram[addr & 0x7ff]'
        elif 0x8000 <= operand and upper <= 0xffff: direct = 'c->bus->prg[addr - 0x8000]'
    read = f'0x{operand:02x}' if mode == 'n' else ('c->a' if mode == 'a' else direct or 'vs_rd(c, addr)')
    def write(value):
        # ROM writes remain ignored through the bus, never assigned to const.
        if direct and '->prg' not in direct:
            return f'{direct} = {value};'
        return f'vs_wr(c, addr, {value});'
    lines = [f'L{address:04x}:', '#if !defined(SMB_VS)',
             f'if (!c->fuel) {{ c->pc = 0x{address:04x}; return; }}',
             '--c->fuel; ++c->instructions;', '#endif']
    if addr is not None and op not in ('JSR', 'JMP'):
        lines.append(f'addr = {addr};')
    body = ''
    if op in ('LDA', 'LDX', 'LDY'):
        body = f'c->{op[-1].lower()} = vs_nz(c, {read});'
    elif op in ('STA', 'STX', 'STY'):
        body = write(f'c->{op[-1].lower()}')
    elif op in ('AND', 'ORA', 'EOR'):
        sym = dict(AND='&', ORA='|', EOR='^')[op]
        body = f'c->a = vs_nz(c, c->a {sym} {read});'
    elif op in ('ADC', 'SBC'):
        body = f'vs_adc(c, {read});' if op == 'ADC' else f'vs_adc(c, (uint8_t)~({read}));'
    elif op in ('CMP', 'CPX', 'CPY'):
        reg = dict(CMP='a', CPX='x', CPY='y')[op]
        body = f'vs_cmp(c, c->{reg}, {read});'
    elif op == 'BIT':
        body = f'v = {read}; c->p = (c->p & ~(VS_N | VS_V | VS_Z)) | (v & 0xc0) | ((c->a & v) ? 0 : VS_Z);'
    elif op in ('INC', 'DEC', 'ASL', 'LSR', 'ROL', 'ROR'):
        body = f'v = {read}; '
        if op in ('INC', 'DEC'):
            body += f'v {"++" if op == "INC" else "--"}; '
        else:
            body += 'carry = c->p & VS_C; '
            left = op in ('ASL', 'ROL')
            body += f'c->p = (c->p & ~VS_C) | {"(v >> 7)" if left else "(v & 1)"}; '
            value = dict(ASL='v << 1', LSR='v >> 1', ROL='(v << 1) | carry', ROR='(v >> 1) | (carry << 7)')[op]
            body += f'v = (uint8_t)({value}); '
        body += '(void)vs_nz(c, v); ' + ('c->a = v;' if mode == 'a' else write('v'))
    elif op in ('INX', 'INY', 'DEX', 'DEY'):
        reg = op[-1].lower(); sign = '+' if op.startswith('IN') else '-'
        body = f'c->{reg} = vs_nz(c, (uint8_t)(c->{reg} {sign} 1));'
    elif op in ('TAX', 'TAY', 'TXA', 'TYA', 'TSX', 'TXS'):
        source, dest = op[1:].lower()
        body = f'c->{dest} = ' + (f'c->{source};' if dest == 's' else f'vs_nz(c, c->{source});')
    elif op in ('CLC', 'SEC', 'CLI', 'SEI', 'CLD', 'SED', 'CLV'):
        body = f'c->p {"|=" if op.startswith("SE") else "&= ~"} VS_{op[-1]};'
    elif op == 'PHA': body = 'vs_push(c, c->a);'
    elif op == 'PHP': body = 'vs_push(c, c->p | VS_B | VS_U);'
    elif op == 'PLA': body = 'c->a = vs_nz(c, vs_pop(c));'
    elif op == 'PLP': body = 'c->p = (vs_pop(c) | VS_U) & ~VS_B;'
    elif mode == 'r':
        target = (nxt + (operand if operand < 128 else operand - 256)) & 0xffff
        flag, yes = dict(BPL=('N', False), BMI=('N', True), BVC=('V', False), BVS=('V', True),
                         BCC=('C', False), BCS=('C', True), BNE=('Z', False), BEQ=('Z', True))[op]
        if target <= address:
            lines += ['#if defined(SMB_VS)',
                      f'if (!c->fuel) {{ c->pc = 0x{address:04x}; return; }} --c->fuel;', '#endif']
        body = f'if ({"" if yes else "!"}(c->p & VS_{flag})) goto L{target:04x};'
    elif op == 'JSR':
        lines.append(f'vs_push(c, 0x{(nxt - 1) >> 8:02x}); vs_push(c, 0x{(nxt - 1) & 255:02x}); goto L{operand:04x};')
        return lines
    elif op == 'JMP':
        if mode == 'w' and operand < address:
            lines += ['#if defined(SMB_VS)',
                      f'if (!c->fuel) {{ c->pc = 0x{address:04x}; return; }} --c->fuel;', '#endif']
        if mode == 'ind':
            lines.append(f'c->pc = vs_rd(c, 0x{operand:04x}) | ((uint16_t)vs_rd(c, 0x{(operand & 0xff00) | ((operand + 1) & 255):04x}) << 8); goto dispatch;')
        elif operand == address:
            lines.append(f'c->pc = 0x{address:04x}; c->idle = 1; return;')
        else:
            lines.append(f'goto L{operand:04x};')
        return lines
    elif op in ('RTS', 'RTI'):
        if op == 'RTI': lines.append('c->p = (vs_pop(c) | VS_U) & ~VS_B;')
        lines.append('addr = vs_pop(c); addr |= (uint16_t)vs_pop(c) << 8;')
        lines.append('c->pc = (uint16_t)(addr + 1); goto dispatch;' if op == 'RTS' else
                     'c->pc = addr; c->in_nmi = 0; c->yielded = 1; return;')
        return lines
    elif op == 'NOP': body = ';'
    else:
        raise ValueError(f"Unsupported translation {op} at {address:04x}")
    optimized = body
    # Replace only supported pure flag helpers. Memory/I/O accesses, arithmetic
    # results, stack operations and timing boundaries remain unchanged.
    def specialize(text, helper):
        start = text.find(helper + '(')
        if start < 0: return text
        opening = start + len(helper)
        depth = 1
        end = opening + 1
        while depth:
            depth += (text[end] == '(') - (text[end] == ')')
            end += 1
        return (text[:start] + helper + '_live' + text[opening:end - 1]
                + f', 0x{live:02x})' + text[end:])
    if live & 0x82 != 0x82:
        optimized = specialize(optimized, 'vs_nz')
    if op in ('ADC', 'SBC') and live & 0xc3 != 0xc3:
        optimized = specialize(optimized, 'vs_adc')
    if op in ('CMP', 'CPX', 'CPY') and live & 0x83 != 0x83:
        optimized = specialize(optimized, 'vs_cmp')
    if optimized != body:
        lines += ['#if defined(SMB_VS)', optimized, '#else', body, '#endif']
    else:
        lines.append(body)
    lines.append(f'goto L{nxt:04x};')
    return lines


def semantic_fast_paths(code, named_entries):
    """Recognize complete reviewed routines, never just a name/address."""
    result = {}
    for a, (op, mode, fill) in code.items():
        if (op, mode) != ('LDA', 'n'): continue
        store = code.get(a + 2)
        if not store or store[:2] != ('STA', 'wy') or not 0x200 <= store[2] <= 0x1f00: continue
        if any(code.get(a + i) != ('INY', 'i', 0) for i in range(5, 9)): continue
        if code.get(a + 9) != ('BNE', 'r', 247) or code.get(a + 11) != ('RTS', 'i', 0): continue
        result[a] = (f'if (vs_fast_stride_fill(c, 0x{store[2]:04x}, 0x{fill:02x}))', a + 11)
    start = named_entries.get('render_chr_pair_do')
    if start is not None:
        a, body = start, []
        while a in code and len(body) < 64:
            ins = code[a]; body.append(ins); a += LENGTH[ins[1]]
            if ins[0] == 'RTS': break
        # Fingerprint of the full decoded operation sequence, including RAM
        # operands and branches, from the verified SM4-4 E reference.
        digest = hashlib.sha256(json.dumps(body, separators=(',', ':')).encode()).hexdigest()
        if digest == '52b0d0b34ee0ccfe79674ddafbef6eabc9d9a516134a5950be611f1c5639e870':
            result[start] = ('vs_fast_render_pair(c); vs_fast_return(c); return;', None)
    for name, count, expected, action in (
        ('cannon_proc', 52,
         'b6344a6f877b29a26979467a5808b9a8dc078f9ba70a3693d18202ab7d1cf1db',
         'if (vs_fast_cannon_proc(c)) return;'),
        ('player_proc_player_physics', 142,
         '9e32d4b02d44599dc63bad5150a179f2366b828bdbad366b6be7e497c995d12a',
         'vs_fast_player_physics(c); return;'),
        ('player_proc_player_ani_speed', 28,
         'b994f9a50e624f87e33499283a701934597cd9fbdd4357ab636e05efb00cdf55',
         'vs_fast_player_animation_speed(c); return;'),
        ('player_proc_player_friction', 39,
         '8dbd4d5757d0b3846ed0f6d2a0e450067052d4aea332050f867c0aa1bd3456e7',
         'vs_fast_player_friction(c); return;'),
        ('render_actor_chr_pair', 5,
         '50b64dc2ef458cce7f02753ba8150611b881c140ee646312b8806032d43dfe1c',
         'vs_fast_render_actor_pair(c); vs_fast_return(c); return;'),
        ('render_chr_pair', 2,
         '42ce39eac366bd5e84e69d7e0fe0491622899df41078eae9a5e93b62823efca6',
         'vs_fast_render_pair_right(c); vs_fast_return(c); return;'),
        ('misc_proc', 49,
         '84d7bfbe10bb06430f68d1f08e5fbef38716eaee6b226c7d0fc51d90151717f7',
         'if (vs_fast_misc_proc_inactive(c)) return;'),
        ('motion_x', 4,
         '7b4d79c3d245d53c9d89b7aaebd8272a7d81380e708388070d15c0f430828655',
         'vs_fast_motion_x(c); return;'),
        ('motion_x_player', 41,
         '6f2ca95dbca2e023a85736f4096ecfec0c6d724d6ac8b23f38aa91be49be6df9',
         'vs_fast_motion_x_player(c); return;'),
        ('motion_gravity', 55,
         'dcaec7c8f29ecd6b849d0b69bdab6ccc891fa1d14d39032f552dbcf4ea1b48fc',
         'if (vs_fast_motion_gravity(c)) return;'),
        ('render_player_tiles', 7,
         '1bea09ccbed6d3ed6687a55fb92881a7592319611d70d401d93e63b6a0d92510',
         'if (vs_fast_render_player_tiles(c)) return;'),
        ('actor_oob_proc', 44,
         'edb1d0eecc751178f3eb45e32cedb4be1b79cd8733094490c3d4cd20f1c7ca72',
         'if (vs_fast_actor_oob_proc(c)) return;'),
        ('render_actor_clear_offscr', 40,
         '8a6525a058dc95579702069936dac95d66856453b6133834f732060c4b43158b',
         'if (vs_fast_render_actor_clear_offscr(c)) return;'),
        ('col_player_actor_proc', 3,
         'a74d1fd16eb604fc52b00f3bfe16f0dede2f7b450275ae2231d0dd4f00435cab',
         'if (vs_fast_col_player_actor_odd(c)) return;'),
        ('col_actor_pos_y_diff', 5,
         'd4e4325349dc2b68fab71f410db180db57f8f7c58eab2fabea6562f50d39db71',
         'vs_fast_col_actor_pos_y_diff(c); return;'),
        ('col_actor_check_side_do', 19,
         'f4e68e8d3e1bc746bff781601e78a20b4781a50d244f7b66386392f978c16295',
         'if (vs_fast_col_actor_check_side_do(c)) return;'),
        ('col_bg_proc_check_non_solid', 10,
         '7c15458a530e1dbc7d2a9295cae2ba815913100c25e93391674e768bc154ba29',
         'vs_fast_col_bg_non_solid(c); return;'),
    ):
        start = named_entries.get(name)
        if start is None:
            continue
        body, a = [], start
        for _ in range(count):
            ins = code.get(a)
            if ins is None:
                break
            body.append(ins)
            a += LENGTH[ins[1]]
        digest = hashlib.sha256(json.dumps(body, separators=(',', ':')).encode()).hexdigest()
        if len(body) == count and digest == expected:
            result[start] = (action, None)

    # These wrappers are only safe when every member of their shared helper
    # chain retains the reviewed instruction shape.
    chained_routines = (
        (
            'motion_y_player',
            (
                ('motion_y_player', 9,
                 'cae4af9e9f053f70a939482a928c9d30daac42307c6830513c48d3aafca000da'),
                ('motion_x_do', 38,
                 '34446f0bd877b97ab0ccbe68e61ab85adc38b6707f78e6b4340d9eb1ac5cbe13'),
                ('motion_gravity_do', 3,
                 'a836b7b438091d196f832f57eaf14241838acead87727398c88563c478750dd3'),
                ('motion_gravity', 55,
                 'dcaec7c8f29ecd6b849d0b69bdab6ccc891fa1d14d39032f552dbcf4ea1b48fc'),
            ),
            'if (vs_fast_motion_y_player(c)) return;',
        ),
        (
            'motion_y',
            (
                ('motion_y', 6,
                 '49b23d23552cc657310c21bd3bfd24299d5c1281077c6b6951ee22fda620ca67'),
                ('motion_fall_fast', 12,
                 'df899df6d95b1681a9ae1be093acc2e2b9132391350acccda2a351194e1040ef'),
                ('motion_gravity_do', 3,
                 'a836b7b438091d196f832f57eaf14241838acead87727398c88563c478750dd3'),
                ('motion_gravity', 55,
                 'dcaec7c8f29ecd6b849d0b69bdab6ccc891fa1d14d39032f552dbcf4ea1b48fc'),
            ),
            'if (vs_fast_motion_y(c)) return;',
        ),
        (
            'game_scroll',
            (
                ('game_scroll', 72,
                 '7655251e95aee817e406a608668b2db353147b8060d125a836156d1154e783c0'),
                ('game_screen_pos_x_calc', 8,
                 '08cc79bed70c85d37b7bd39215c6196418aa78fce1f9bbbc4cf33625677d4239'),
                ('pos_bits_diff', 13,
                 '2512abfcfa3e43f35588715357b4d81257c8f8158569ecc085b53aa91a7ab91a'),
                ('pos_bits_get_do_x', 25,
                 '42d792d7740f7fc14606f2c861a1cf04f903e6898ef520d53089ca41e267d758'),
            ),
            'if (vs_fast_game_scroll(c, 0u)) return;',
        ),
        (
            'ppu_displist_write_do',
            (
                ('ppu_displist_write_do', 40,
                 'df68b2f9c2ffaf8b0b2c615ea5964721e93c53d4696c2feb8124bb7ddc221b47'),
                ('ppu_displist_write_ctlr0', 3,
                 'e263115cf8e9b468233d0d2f2a5d4b2885e73dd2addd4a16cf245fd16a2b9c8a'),
            ),
            'if (vs_fast_ppu_displist_command(c)) return;',
        ),
        (
            'pos_calc_x_rel_player',
            (
                ('pos_calc_x_rel_player', 3,
                 '48ef85d45f53c47f236c12a72c198f7b45ee6a4a9d8b56e1bc0e456e2a865872'),
                ('pos_calc_x_rel_a', 3,
                 '6d8cdfa49ce2f64d8cfec10f743f1e768e5bea79924909a325b986f3029150b8'),
                ('pos_calc_x_rel_do', 7,
                 'ba797b0764e36b6dcb49d0b26197f6ab1e4916deb15d9fe6c6a213d285e36710'),
            ),
            'vs_fast_pos_calc_x_rel_player(c); return;',
        ),
        (
            'pos_calc_x_rel_actor',
            (
                ('pos_calc_x_rel_actor', 3,
                 '8abb7e5fc19c1b423422d01fd64f1cd4fee47a9d8b43a9bca19625dcb1724182'),
                ('pos_calc_x_rel_b', 7,
                 '10ae2fd438dd38958e4120a4da89cf747d099ef164fc542922afde31e4ef2016'),
                ('pos_calc_x_rel_do', 7,
                 'ba797b0764e36b6dcb49d0b26197f6ab1e4916deb15d9fe6c6a213d285e36710'),
            ),
            'vs_fast_pos_calc_x_rel_actor(c); return;',
        ),
        (
            'col_box_buffer_check_actor',
            (
                ('col_box_buffer_check_actor', 7,
                 '9c40d677c266f838dd19bea39b87e35ca17220743d213304f273a854e9cbb08c'),
                ('col_box_buffer_check_do', 4,
                 'c449624b4e5ff2faf686c0d12cde0b38ae31d3cc036ffbb4ed11dfc20a468be6'),
                ('col_box_buffer', 37,
                 '0a8c68a9f05633c3cbb20d2f43e4254fab8738ec42efdce557885abc255552df'),
            ),
            'vs_fast_col_box_buffer_check_actor(c); return;',
        ),
        (
            'col_actor_block_col',
            (
                ('col_actor_block_col', 3,
                 '322caa004f0b8b4a9984df99da2320bcbc7b4060c6e392419b2a0b98545daf23'),
                ('col_box_buffer_check_actor', 7,
                 '9c40d677c266f838dd19bea39b87e35ca17220743d213304f273a854e9cbb08c'),
                ('col_box_buffer_check_do', 4,
                 'c449624b4e5ff2faf686c0d12cde0b38ae31d3cc036ffbb4ed11dfc20a468be6'),
                ('col_box_buffer', 37,
                 '0a8c68a9f05633c3cbb20d2f43e4254fab8738ec42efdce557885abc255552df'),
            ),
            'vs_fast_col_actor_block_col(c); return;',
        ),
        (
            'render_plat_large',
            (
                ('render_plat_large', 72,
                 '57d52485b1ee5c872b61b9cbb80133cc22d04746632bd87c4973db3255e21975'),
                ('render_vine_column', 12,
                 'a19c0fee0e85c0a58be7de00eb39cfe4d5c73e6393649167a8404b3f697aced9'),
                ('render_set_four_sprites_v', 5,
                 'fb9bcdc481d1346663fb0dbb8e477d46f1b71863ab89338bbc4d6b3d9adbb804'),
                ('render_set_six_sprites', 7,
                 '44025c05744f68abbe80d93b60907f9a737ed3eada58ef087ff8f6ca27d997a3'),
                ('render_clear_six_sprites', 8,
                 '6e47809fb2f87775de7555a629f9e0804947f18c7313d612272676c1db32b40c'),
                ('pos_bits_get_do_x', 25,
                 '42d792d7740f7fc14606f2c861a1cf04f903e6898ef520d53089ca41e267d758'),
            ),
            'if (vs_fast_render_plat_large(c)) return;',
        ),
        (
            'render_player_do',
            (
                ('render_player_do', 44,
                 '35b255aa055084373bb684d58d4e2a49e32a51b7556f494fc168974950f0d648'),
                ('render_player_tiles_init', 19,
                 'b83e996d90865db5bc7fbcef3e6db3cc8cbfb35b17c38e8394b0743f0a8177fd'),
                ('render_player_tiles_mirror', 28,
                 '0bd7b65f99476ae17761bcb0c6a3a52219811ea445daacec96bd60805a350746'),
                ('render_set_two_sprites_v', 3,
                 '29bb026601033d3bf9b080ebc4672eb8d8b617b08b1c798e2d32e058ab427d95'),
                ('render_chr_pair', 2,
                 '42ce39eac366bd5e84e69d7e0fe0491622899df41078eae9a5e93b62823efca6'),
                ('render_chr_pair_do', 36,
                 '52b0d0b34ee0ccfe79674ddafbef6eabc9d9a516134a5950be611f1c5639e870'),
            ),
            'if (vs_fast_render_player_do(c)) return;',
        ),
    )
    for entry_name, members, action in chained_routines:
        valid = True
        for name, count, expected in members:
            start = named_entries.get(name)
            if start is None:
                valid = False
                break
            body, cursor = [], start
            for _ in range(count):
                ins = code.get(cursor)
                if ins is None:
                    break
                body.append(ins)
                cursor += LENGTH[ins[1]]
            digest = hashlib.sha256(
                json.dumps(body, separators=(',', ':')).encode()
            ).hexdigest()
            if len(body) != count or digest != expected:
                valid = False
                break
        if valid:
            result[named_entries[entry_name]] = (action, None)

    # The alternate entry is inside the same fully fingerprinted scroll body.
    scroll = named_entries.get('game_scroll')
    if scroll in result and named_entries.get('game_scroll_calc') == 0xae71:
        result[0xae71] = ('if (vs_fast_game_scroll(c, 1u)) return;', None)

    # All fall presets share the fully checked actor-gravity chain above.
    if named_entries.get('motion_y') in result:
        for name, address, force, cap in (('motion_fall_fast', 0xbe97, 0x7f, 2),
                                         ('motion_fall_slow', 0xbe9b, 0x0f, 2),
                                         ('motion_fall_mid', 0xbea1, 0x1c, 3)):
            entry = named_entries.get(name)
            if entry == address:
                result[entry] = (
                    f'if (vs_fast_motion_fall(c, 0x{force:02x}u, {cap}u)) return;', None
                )

    actor_loop = named_entries.get('actor_loop')
    actor_init_check = named_entries.get('actor_init_check')
    actor_group = named_entries.get('actor_group')
    if actor_loop is not None and actor_init_check is not None and actor_group is not None:
        loop_body = [
            code[a] for a in sorted(code) if actor_loop <= a < actor_init_check
        ]
        init_body = [
            code[a] for a in sorted(code) if actor_init_check <= a < actor_group
        ]
        loop_digest = hashlib.sha256(
            json.dumps(loop_body, separators=(',', ':')).encode()
        ).hexdigest()
        init_digest = hashlib.sha256(
            json.dumps(init_body, separators=(',', ':')).encode()
        ).hexdigest()
        if (
            len(loop_body) == 161 and
            loop_digest == 'f89ce3364798c2ad7d40ecbe50a74829bd9ba09572257ea80421407a9e53b6fa' and
            len(init_body) == 11 and
            init_digest == '956ce2485f6ed51dad652d29b7d505e867d8418f872ecd6ce3ded7a3643f9a3d'
        ):
            result[actor_loop] = ('if (vs_fast_actor_loop_no_spawn(c)) return;', None)

    actor_proc = named_entries.get('actor_proc')
    if actor_proc is not None and actor_loop is not None:
        proc_body = [
            code[a] for a in sorted(code) if actor_proc <= a < actor_loop
        ]
        proc_digest = hashlib.sha256(
            json.dumps(proc_body, separators=(',', ':')).encode()
        ).hexdigest()
        if (
            len(proc_body) == 49 and
            proc_digest == '38ffffb6683f222c964723cf3f588b78c5fa60cb69e32969a849f8f7ba323114'
        ):
            result[actor_proc] = ('if (vs_fast_actor_proc_dispatch(c)) return;', None)

    actor_base = named_entries.get('actor_proc_base')
    actor_bounce = named_entries.get('actor_proc_bounce')
    if actor_base is not None and actor_bounce is not None:
        base_body = [
            code[a] for a in sorted(code) if actor_base <= a < actor_bounce
        ]
        base_digest = hashlib.sha256(
            json.dumps(base_body, separators=(',', ':')).encode()
        ).hexdigest()
        if (
            len(base_body) == 67 and
            base_digest == '321140d5c489194c532d65fb73d0d3d8b6353a6962837a501768514e5efb5a64'
        ):
            # The complete state policy additionally invokes shared gravity,
            # horizontal motion and actor cleanup. Validate the whole chain.
            extra = (
                ('actor_erase', 10,
                 'da46d438d54338de15c5f5cb266ee00ff3936e1987923469ec8b34508054a6ea'),
                ('motion_x', 4,
                 '7b4d79c3d245d53c9d89b7aaebd8272a7d81380e708388070d15c0f430828655'),
                ('motion_x_do', 38,
                 '34446f0bd877b97ab0ccbe68e61ab85adc38b6707f78e6b4340d9eb1ac5cbe13'),
            )
            valid = named_entries.get('motion_y') in result
            for name, count, expected in extra:
                cursor = named_entries.get(name)
                body = []
                for _ in range(count):
                    ins = code.get(cursor)
                    if ins is None:
                        break
                    body.append(ins)
                    cursor += LENGTH[ins[1]]
                digest = hashlib.sha256(json.dumps(body, separators=(',', ':')).encode()).hexdigest()
                valid = valid and len(body) == count and digest == expected
            if valid:
                result[actor_base] = ('if (vs_fast_actor_proc_base(c)) return;', None)

    # The no-overlap actor scanner depends on the complete caller plus both
    # geometry leaves.  Requiring all three fingerprints keeps the optimized
    # decision table tied to the verified linked VS implementation.
    actor_collision = named_entries.get('col_actor_actor_proc')
    actor_collision_do = named_entries.get('col_actor_actor_do')
    actor_box_get = named_entries.get('col_actor_box_get')
    col_bg_proc = named_entries.get('col_bg_proc')
    col_base_actor = named_entries.get('col_base_actor')
    col_buffer_actor = named_entries.get('col_box_buffer_check_actor')
    actor_collision_members = (
        (actor_collision, actor_collision_do, 63,
         '150e1e51e196898c579b2619d0cde4ea06a7b4a7cd63bbad1c01246567068a8c'),
        (actor_box_get, col_bg_proc, 10,
         'c13a05f11d20bcfdce2eb1a1bb5c811d4b3dec440a49475630055f56614189f0'),
        (col_base_actor, col_buffer_actor, 45,
         'e1047aa37492ed1e159cd80c9b5203d2a5248c6a6c4e0a119f18c70027456495'),
    )
    if all(start is not None and end is not None for start, end, _, _ in actor_collision_members):
        valid = True
        for start, end, count, expected in actor_collision_members:
            body = [code[a] for a in sorted(code) if start <= a < end]
            digest = hashlib.sha256(
                json.dumps(body, separators=(',', ':')).encode()
            ).hexdigest()
            if len(body) != count or digest != expected:
                valid = False
                break
        if valid:
            result[actor_collision] = ('if (vs_fast_col_actor_actor_exit(c)) return;', None)

    actor_ground = named_entries.get('col_actor_ground_proc')
    actor_bounce = named_entries.get('col_actor_bounce_proc')
    actor_block = named_entries.get('col_actor_block_col')
    actor_non_solid = named_entries.get('col_bg_proc_check_non_solid')
    projectile_collision = named_entries.get('col_proj_proc')
    ground_members = (
        (actor_ground, actor_bounce, 207,
         '07c3229d5b14de7d3b865330d95d4052d3af85c8fbb3bb3e6edc3d2e2f54f32a'),
        (actor_block, actor_non_solid, 3,
         '322caa004f0b8b4a9984df99da2320bcbc7b4060c6e392419b2a0b98545daf23'),
        (actor_non_solid, projectile_collision, 10,
         '7c15458a530e1dbc7d2a9295cae2ba815913100c25e93391674e768bc154ba29'),
    )
    if all(start is not None and end is not None for start, end, _, _ in ground_members):
        valid = True
        for start, end, count, expected in ground_members:
            body = [code[a] for a in sorted(code) if start <= a < end]
            digest = hashlib.sha256(
                json.dumps(body, separators=(',', ':')).encode()
            ).hexdigest()
            if len(body) != count or digest != expected:
                valid = False
                break
        if valid:
            result[actor_ground] = ('if (vs_fast_col_actor_ground(c)) return;', None)

    render_tiles = named_entries.get('render_actor_tiles')
    render_pair = named_entries.get('render_actor_chr_pair')
    if render_tiles is not None and render_pair is not None:
        tile_body = []
        cursor = render_tiles
        for _ in range(4):
            tile_body.append(code.get(cursor))
            if tile_body[-1] is None:
                break
            cursor += LENGTH[tile_body[-1][1]]
        pair_body = []
        pair_cursor = render_pair
        for _ in range(5):
            pair_body.append(code.get(pair_cursor))
            if pair_body[-1] is None:
                break
            pair_cursor += LENGTH[pair_body[-1][1]]
        tile_digest = hashlib.sha256(
            json.dumps(tile_body, separators=(',', ':')).encode()
        ).hexdigest()
        pair_digest = hashlib.sha256(
            json.dumps(pair_body, separators=(',', ':')).encode()
        ).hexdigest()
        if (
            tile_digest == 'fa163b1ef14fdffe172e3f9fb12ea738706390bac638bf9e3f20ea61fb9b12bd' and
            pair_digest == '50b64dc2ef458cce7f02753ba8150611b881c140ee646312b8806032d43dfe1c'
        ):
            result[render_tiles] = ('vs_fast_render_actor_tiles(c);', cursor)

    render_actor = named_entries.get('render_actor')
    render_cleanup = named_entries.get('render_actor_clear_offscr')
    if render_actor is not None and render_cleanup is not None:
        actor_body = [
            code[a] for a in sorted(code) if render_actor <= a < render_cleanup
        ]
        actor_digest = hashlib.sha256(
            json.dumps(actor_body, separators=(',', ':')).encode()
        ).hexdigest()
        # The flipped-actor path also calls this shared six-store leaf.
        six_sprite_stores = all(
            code.get(0xe512 + i * 3) == ('STA', 'wy', 0x0214 - i * 4)
            for i in range(6)
        ) and code.get(0xe524) == ('RTS', 'i', 0)
        if (
            len(actor_body) == 345 and
            actor_digest == 'a8835dda978c39bd3d12f7a64ada1ec0d88e63c43380f643047cb90866412b9b' and
            render_tiles in result and render_cleanup in result and
            render_pair in result and six_sprite_stores and
            named_entries.get('render_chr_pair_do') in result
        ):
            result[render_actor] = ('if (vs_fast_render_actor(c)) return;', None)

    # Rendering one ordinary area column traverses 13 metatiles and crosses a
    # generated C page boundary on every row.  Fingerprint the complete source
    # routine before replacing it with the equivalent batched operation.
    meta_render_area = named_entries.get('meta_render_area')
    meta_render_attr = named_entries.get('meta_render_attr')
    if meta_render_area is not None and meta_render_attr is not None:
        meta_body = [
            code[a] for a in sorted(code)
            if meta_render_area <= a < meta_render_attr
        ]
        meta_digest = hashlib.sha256(
            json.dumps(meta_body, separators=(',', ':')).encode()
        ).hexdigest()
        if (
            len(meta_body) == 89 and
            meta_digest == '803b5506021a203ea34a36c3a9dd6943f469fc1291d630dfc4baa62d850b843b' and
            # The routine tail-jumps to the shared command terminator.
            code.get(0x8bbe) == ('LDA', 'n', 6) and
            code.get(0x8bc0) == ('STA', 'w', 0x0773) and
            code.get(0x8bc3) == ('RTS', 'i', 0)
        ):
            result[meta_render_area] = ('if (vs_fast_meta_render_area(c)) return;', None)

    start = named_entries.get('tbljmp')
    if start is not None:
        a, body = start, []
        while a in code and len(body) < 32:
            ins = code[a]; body.append(ins); a += LENGTH[ins[1]]
            if ins == ('JMP', 'ind', 6): break
        digest = hashlib.sha256(json.dumps(body, separators=(',', ':')).encode()).hexdigest()
        if digest == 'd66523143c86f5df4b330afe7f2cad7b449de9b252846e819b446eeef547e74d':
            result[start] = ('vs_fast_table_jump(c); return;', None)
    start = named_entries.get('nmi_oam_shuffle')
    if start is not None:
        a, body = start, []
        while a in code and len(body) < 64:
            ins = code[a]; body.append(ins); a += LENGTH[ins[1]]
            if ins[0] == 'RTS': break
        digest = hashlib.sha256(json.dumps(body, separators=(',', ':')).encode()).hexdigest()
        if digest == 'f94b014c05ed7c56da8bd0e38168cbbb2267af4778a0cd545e8f0cf69918866f':
            result[start] = ('vs_fast_nmi_oam_shuffle(c);', a - 1)
    routines = {
        'joypad_read_do': (21, 'b04474690ca931f11f562b0092a6e7dfefea6fc28acb12fd9fc2203543eda46b',
                           'vs_fast_joypad_read_do(c);'),
        'col_box_buffer': (37, '0a8c68a9f05633c3cbb20d2f43e4254fab8738ec42efdce557885abc255552df',
                           'vs_fast_col_box_buffer(c);'),
        'stats_score_hi_check_do': (17, '49349a425f4879e1c462b7d9c9598719f0066c297b1995694a30bbae9524ab8d',
                                    'vs_fast_stats_score_hi_check_do(c);'),
        'motion_x_do': (38, '34446f0bd877b97ab0ccbe68e61ab85adc38b6707f78e6b4340d9eb1ac5cbe13',
                        'vs_fast_motion_x_do(c);'),
        'col_box_proc': (36, 'e1f5a4cb13dd3a8e91f18a8ea7b43dabb77ebe02132f8b8e763ffdc244f2b938',
                         'vs_fast_col_box_proc(c);'),
        'pos_bits_get_do_x': (25, '42d792d7740f7fc14606f2c861a1cf04f903e6898ef520d53089ca41e267d758',
                              'vs_fast_pos_bits_get_do_x(c);'),
        'pos_bits_get_do_y': (25, 'dca26da57092fcb2e7e2d1a5687e2d7ab576ca20a340538248597abfa13b86fa',
                              'vs_fast_pos_bits_get_do_y(c);'),
        'pos_bits_get': (15, '02adfccc13fb79abbbff6190b3a113cc343c2a2c84846bed26e0faf325da9ff0',
                         'vs_fast_pos_bits_get(c);'),
    }
    for name, (count, expected, action) in routines.items():
        start = named_entries.get(name)
        if start is None:
            continue
        a, body = start, []
        for _ in range(count):
            ins = code.get(a)
            if ins is None:
                break
            body.append(ins)
            a += LENGTH[ins[1]]
        digest = hashlib.sha256(json.dumps(body, separators=(',', ':')).encode()).hexdigest()
        if len(body) == count and digest == expected:
            result[start] = (action, a - LENGTH[body[-1][1]])
    start = named_entries.get('nmi_timers_tick')
    if start is not None:
        body, a = [], start
        for _ in range(12):
            ins = code.get(a)
            if ins is None:
                break
            body.append(ins); a += LENGTH[ins[1]]
        digest = hashlib.sha256(json.dumps(body, separators=(',', ':')).encode()).hexdigest()
        if (len(body) == 12 and
                digest == 'acfad2351823e51ee4732d1d72ca322c01b140c2d5b7d82e54a572f1079928ff'):
            result[start] = ('vs_fast_nmi_timers(c);', a)
    start = named_entries.get('nmi_rand')
    if start is not None:
        body, a = [], start
        for _ in range(15):
            ins = code.get(a)
            if ins is None:
                break
            body.append(ins); a += LENGTH[ins[1]]
        digest = hashlib.sha256(json.dumps(body, separators=(',', ':')).encode()).hexdigest()
        if (len(body) == 15 and
                digest == 'de79b7d38db5f79f664910f7d57b1a70d490bc66fd75617ed5abe9c9d9fbe393'):
            result[start] = ('vs_fast_nmi_rand(c);', a)
        # The following fixed delay aligns NES writes after sprite-zero. Neo Geo
        # presents the split independently, so only its final CPU state matters.
        full, cursor = [], start
        while cursor in code and len(full) < 64:
            ins = code[cursor]; full.append(ins); cursor += LENGTH[ins[1]]
            if ins[0] == 'RTI':
                break
        full_digest = hashlib.sha256(json.dumps(full, separators=(',', ':')).encode()).hexdigest()
        if full_digest == '7dc715fb66ddba8a2663df5bf520421853052567c741087aeb8aa124bc057401':
            result[0x81d9] = ('vs_fast_nmi_hblank_delay(c);', 0x81de)
    return result


def translate(prg: bytes, debug: str, source_root: Path, profile: bool = False) -> tuple[str, int]:
    code = instructions(prg, debug, source_root)
    live = live_flags(code)
    # Native dispatch only needs actual source labels, cross-page transfers and
    # return addresses. The host keeps every instruction resumable for tracing.
    native_entries = set()
    named_entries = {}
    for line in debug.splitlines():
        if line.startswith('sym\t') and ',type=lab' in line:
            match = re.search(r'\bval=(0x[0-9a-fA-F]+|\d+)', line)
            if match and int(match[1], 0) in code:
                native_entries.add(int(match[1], 0))
                name = re.search(r'\bname=("[^"]*")', line)
                if name: named_entries[json.loads(name[1])] = int(match[1], 0)
    fast_paths = semantic_fast_paths(code, named_entries)
    native_entries.update(target for _, target in fast_paths.values() if target is not None)
    first_in_page = {}
    for a in sorted(code): first_in_page.setdefault(a >> 8, a)
    native_entries.update(first_in_page.values())
    for a, (op, mode, operand) in code.items():
        target = None
        if mode == 'r':
            target = (a + 2 + (operand if operand < 128 else operand - 256)) & 0xffff
        elif op in ('JMP', 'JSR') and mode == 'w':
            target = operand
        if target is not None and target not in code:
            raise ValueError(f'Unmapped direct transfer {a:04x} -> {target:04x}')
        if target is not None: native_entries.add(target)
        nxt = a + LENGTH[mode]
        if nxt in code and (op == 'JSR' or a >> 8 != nxt >> 8): native_entries.add(nxt)
    lines = ['/* Generated privately from verified VS input. Do not distribute game data. */',
             '#include "vs_cpu.h"', '#include "vs_fast_paths.h"']
    if profile: lines.append('uint32_t vs_profile[32768];')
    pages = sorted({a >> 8 for a in code})
    for page in pages:
        page_code = {a: ins for a, ins in code.items() if a >> 8 == page}
        # CPU register state is disjoint from bus RAM/PPU storage. Express that
        # contract without copying the state on every page entry and return.
        lines += [f'static void page_{page:02x}(VsCpu *restrict c) {{',
                  'uint16_t addr = 0; uint8_t v = 0, carry = 0;',
                  '(void)addr; (void)v; (void)carry;',
                  '#if defined(SMB_VS)', 'if (!c->fuel) { return; }', '--c->fuel;', '#endif', 'goto dispatch;',
                  'dispatch:', f'if ((c->pc >> 8) != 0x{page:02x}) return;',
                  'switch (c->pc) {']
        for a in sorted(page_code):
            if a not in native_entries: lines.append('#if !defined(SMB_VS)')
            lines.append(f'case 0x{a:04x}: goto L{a:04x};')
            if a not in native_entries: lines.append('#endif')
        lines += ['default: c->fault = 1; return;', '}']
        body = []
        for a, (op, mode, operand) in sorted(page_code.items()):
            emitted = emit_instruction(a, op, mode, operand, live[a])
            if a in fast_paths:
                action, target = fast_paths[a]
                suffix = '' if target is None else f' goto L{target:04x};'
                emitted[1:1] = ['#if defined(SMB_VS) && !defined(VS_REFERENCE_KERNELS)',
                                f'{action}{suffix}', '#endif']
            body.extend(emitted)
        def transfer(match):
            target = int(match[1], 16)
            if target in page_code:
                return match[0]
            # Crossing a page returns to the C function table. Any invalid
            # dynamic/fallthrough destination is rejected there, never decoded.
            return f'{{ c->pc = 0x{target:04x}; return; }}'
        lines += [re.sub(r'goto L([0-9a-f]{4});', transfer, line) for line in body]
        lines += ['}']
    lines += ['typedef void (*VsPage)(VsCpu *);', 'static const VsPage pages[128] = {']
    lines += [f'[{page - 128}] = page_{page:02x},' for page in pages]
    lines += ['};', 'void vs_program_run(VsCpu *c, unsigned budget) {',
              'c->fuel = budget; c->yielded = 0;',
              'while (c->fuel && !c->fault && !c->idle && !c->yielded) {',
              'if (c->pc < 0x8000 || !pages[(c->pc >> 8) - 128]) { c->fault = 1; return; }',
              'pages[(c->pc >> 8) - 128](c);', '}', '}']
    if profile:
        lines = [re.sub(r'^L([0-9a-f]{4}):$',
                        lambda m: m[0] + f' ++vs_profile[0x{int(m[1], 16) - 0x8000:04x}];', line)
                 for line in lines]
    return '\n'.join(lines) + '\n', len(code)


def main() -> None:
    from vs_rom import load_vs_rom
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--input', type=Path, required=True)
    p.add_argument('--debug', type=Path, required=True)
    p.add_argument('--reference-prg', type=Path, required=True)
    p.add_argument('--source-root', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--profile', action='store_true', help='host-only instruction counters; use a separate output')
    args = p.parse_args()
    rom = load_vs_rom(args.input)
    if args.reference_prg.read_bytes() != rom.prg:
        raise ValueError('Linked reference PRG differs from verified user input')
    debug = args.debug.read_text()
    source, count = translate(rom.prg, debug, args.source_root, args.profile)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if not args.output.exists() or args.output.read_text() != source:
        args.output.write_text(source)
    args.output.with_suffix('.json').write_text(json.dumps({
        'instruction_count': count, 'prg_sha256': hashlib.sha256(rom.prg).hexdigest(),
        'debug_sha256': hashlib.sha256(debug.encode()).hexdigest(),
        'c_sha256': hashlib.sha256(source.encode()).hexdigest()}, indent=2) + '\n')
    print(f'Translated {count} VS instructions to static C')


if __name__ == '__main__':
    main()
