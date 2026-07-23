/* Minimal PS-EXE: print a marker over BIOS TTY, then spin.
 * Verifies the toolchain, the PS-EXE header and DuckStation TTY capture
 * before any GTE work depends on them. */
.set noreorder
.set noat
.text
.globl _start
_start:
    lui   $sp, 0x801F
    ori   $sp, $sp, 0xFF00
    la    $s0, msg
1:
    lbu   $a0, 0($s0)
    beq   $a0, $zero, 2f
    nop
    li    $t1, 0x3C          /* A(3Ch) = std_out_putchar */
    li    $t2, 0xA0
    jalr  $t2
    nop
    b     1b
    addiu $s0, $s0, 1
2:
    b     2b
    nop

.section .rodata
msg:
    .asciz "PSEXE-TTY-OK\n"
