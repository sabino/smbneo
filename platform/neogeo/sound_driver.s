;;;
;;; Integer-only NES APU state receiver for the Neo Geo YM2610 SSG.
;;;
;;; The MC68000 sends each target register as two acknowledged commands in
;;; the $06-$7f alphabet. They encode a rotated base-121 quotient/remainder
;;; pair; rotation keeps adjacent command bytes distinct so a previous echo
;;; cannot satisfy the next acknowledgement wait.
;;;

        .include "helpers.inc"

        .area   CODE

cmd_jmptable::
        ;; BIOS-reserved commands $00-$03.
        jp      snd_command_unused
        jp      snd_command_01_prepare_for_rom_switch
        jp      snd_command_unused
        jp      snd_command_03_reset_driver

        ;; $04/$05 reset and verify the post-reset packet state.
        jp      apu_ready_ping
        jp      apu_ready_ping

        ;; $06-$7f are the 122 packet symbols.
        .rept   122
        jp      apu_packet_byte
        .endm

        init_unused_cmd_jmptable

apu_ready_ping:
        xor     a
        ld      (apu_packet_phase), a
        ld      (apu_previous_symbol), a
        ret

apu_packet_byte:
        sub     a, #6
        ld      e, a
        ld      a, (apu_packet_phase)
        or      a
        jr      nz, apu_packet_second

        ;; quotient = (first - previous - 1) mod 122, range 0..59.
        ld      a, (apu_previous_symbol)
        ld      c, a
        ld      a, e
        sub     c
        ret     z
        jr      nc, apu_packet_first_delta
        add     a, #122
apu_packet_first_delta:
        dec     a
        cp      #60
        ret     nc
        ld      (apu_packet_quotient), a
        ld      a, e
        ld      (apu_previous_symbol), a
        ld      a, #1
        ld      (apu_packet_phase), a
        ret

apu_packet_second:
        ;; remainder = (second - first - 1) mod 122, range 0..120.
        ld      d, e
        ld      a, (apu_previous_symbol)
        ld      c, a
        ld      a, e
        sub     c
        jr      z, apu_packet_abort
        jr      nc, apu_packet_second_delta
        add     a, #122
apu_packet_second_delta:
        dec     a
        ld      e, a
        xor     a
        ld      (apu_packet_phase), a
        ld      a, d
        ld      (apu_previous_symbol), a

        ;; BC = quotient * 121 + remainder.
        ld      a, (apu_packet_quotient)
        ld      l, a
        ld      h, #0
        add     hl, hl
        ld      bc, #apu_packet_bases
        add     hl, bc
        ld      c, (hl)
        inc     hl
        ld      b, (hl)
        ld      a, c
        add     a, e
        ld      c, a
        jr      nc, apu_packet_validate
        inc     b
apu_packet_validate:
        ld      a, b
        cp      #0x1c
        ret     nc
        call    ym2610_write_port_a
        ret

apu_packet_abort:
        xor     a
        ld      (apu_packet_phase), a
        ret

apu_packet_bases:
        .dw     0x0000, 0x0079, 0x00f2, 0x016b, 0x01e4, 0x025d, 0x02d6, 0x034f
        .dw     0x03c8, 0x0441, 0x04ba, 0x0533, 0x05ac, 0x0625, 0x069e, 0x0717
        .dw     0x0790, 0x0809, 0x0882, 0x08fb, 0x0974, 0x09ed, 0x0a66, 0x0adf
        .dw     0x0b58, 0x0bd1, 0x0c4a, 0x0cc3, 0x0d3c, 0x0db5, 0x0e2e, 0x0ea7
        .dw     0x0f20, 0x0f99, 0x1012, 0x108b, 0x1104, 0x117d, 0x11f6, 0x126f
        .dw     0x12e8, 0x1361, 0x13da, 0x1453, 0x14cc, 0x1545, 0x15be, 0x1637
        .dw     0x16b0, 0x1729, 0x17a2, 0x181b, 0x1894, 0x190d, 0x1986, 0x19ff
        .dw     0x1a78, 0x1af1, 0x1b6a, 0x1be3

        .area   DATA

apu_packet_phase:
        .blkb   1
apu_packet_quotient:
        .blkb   1
apu_previous_symbol:
        .blkb   1
