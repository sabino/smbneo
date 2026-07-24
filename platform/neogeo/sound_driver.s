;;;
;;; Integer-only NES APU state receiver for the Neo Geo YM2610 SSG.
;;;
;;; The MC68000 sends each target register as three acknowledged commands:
;;;   $1r selects SSG register r
;;;   $2h stores the value's high nibble
;;;   $3l stores its low nibble and commits the write
;;;

        .include "helpers.inc"

        .area   CODE

cmd_jmptable::
        ;; BIOS-reserved commands $00-$03.
        jp      snd_command_unused
        jp      snd_command_01_prepare_for_rom_switch
        jp      snd_command_unused
        jp      snd_command_03_reset_driver

        ;; $04/$05 are alternating post-reset transport-ready pings.
        jp      apu_ready_ping
        jp      apu_ready_ping

        ;; $06-$0f are deliberately unused.
        .rept   10
        jp      snd_command_unused
        .endm

        ;; $10-$1f: YM2610 SSG register selector.
        .rept   16
        jp      apu_select_register
        .endm

        ;; $20-$2f: high data nibble.
        .rept   16
        jp      apu_store_high_nibble
        .endm

        ;; $30-$3f: low data nibble and atomic YM2610 write.
        .rept   16
        jp      apu_commit_low_nibble
        .endm

        init_unused_cmd_jmptable

apu_ready_ping:
        ret

apu_select_register:
        and     a, #0x0f
        ld      (apu_pending_register), a
        ret

apu_store_high_nibble:
        and     a, #0x0f
        rlca
        rlca
        rlca
        rlca
        ld      (apu_pending_value), a
        ret

apu_commit_low_nibble:
        and     a, #0x0f
        ld      c, a
        ld      a, (apu_pending_value)
        or      a, c
        ld      c, a
        ld      a, (apu_pending_register)
        ld      b, a
        call    ym2610_write_port_a
        ret

        .area   DATA

apu_pending_register:
        .blkb   1
apu_pending_value:
        .blkb   1
