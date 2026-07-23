/* DPCS golden-vector capture ROM.
 * Line format:  DPC <5 inputs> <8 outputs>
 *   in : rfc gfc bfc rgbc ir0
 *   out: rgb2 mac1 mac2 mac3 ir1 ir2 ir3 flag
 * Every load and mfc2 is followed by an explicit nop (see docs/PSX_NOTES.md).
 */
.set noreorder
.set noat

.macro PUTC ch
    li    $a0, \ch
    li    $t1, 0x3C
    li    $t2, 0xA0
    jalr  $t2
    nop
.endm

.macro PUTHEX reg
    move  $s3, \reg
    li    $s4, 28
1:
    srlv  $t0, $s3, $s4
    andi  $t0, $t0, 0xF
    sltiu $t1, $t0, 10
    beq   $t1, $zero, 2f
    nop
    addiu $a0, $t0, 0x30
    b     3f
    nop
2:
    addiu $a0, $t0, 0x37
3:
    li    $t1, 0x3C
    li    $t2, 0xA0
    jalr  $t2
    nop
    addiu $s4, $s4, -4
    bgez  $s4, 1b
    nop
.endm

.text
.globl _start
_start:
    lui   $sp, 0x801F
    ori   $sp, $sp, 0xFF00

    mfc0  $t0, $12
    nop
    lui   $t1, 0x4000
    or    $t0, $t0, $t1
    mtc0  $t0, $12
    nop
    nop

    la    $s0, cases
    la    $t0, ncases
    nop
    lw    $s1, 0($t0)
    nop

case_loop:
    blez  $s1, all_done
    nop

    lw $t0, 0($s0)
    nop
    ctc2 $t0, $21
    lw $t0, 4($s0)
    nop
    ctc2 $t0, $22
    lw $t0, 8($s0)
    nop
    ctc2 $t0, $23
    ctc2 $zero, $31

    lw $t0, 12($s0)
    nop
    mtc2 $t0, $6            /* RGBC */
    lw $t0, 16($s0)
    nop
    mtc2 $t0, $8            /* IR0 */
    nop
    nop
    nop

    .word 0x4A780010        /* dpcs */
    nop
    nop
    nop
    nop

    la $s5, results
    nop
    mfc2 $t0, $22
    nop
    sw   $t0, 0($s5)
    mfc2 $t0, $25
    nop
    sw   $t0, 4($s5)
    mfc2 $t0, $26
    nop
    sw   $t0, 8($s5)
    mfc2 $t0, $27
    nop
    sw   $t0, 12($s5)
    mfc2 $t0, $9
    nop
    sw   $t0, 16($s5)
    mfc2 $t0, $10
    nop
    sw   $t0, 20($s5)
    mfc2 $t0, $11
    nop
    sw   $t0, 24($s5)
    cfc2 $t0, $31
    nop
    sw   $t0, 28($s5)

    PUTC 0x44
    PUTC 0x50
    PUTC 0x43
    PUTC 0x20

    move $s2, $zero
in_loop:
    sll  $t0, $s2, 2
    addu $t0, $s0, $t0
    lw   $t3, 0($t0)
    nop
    PUTHEX $t3
    PUTC 0x20
    addiu $s2, $s2, 1
    slti $t0, $s2, 5
    bne  $t0, $zero, in_loop
    nop

    move $s2, $zero
out_loop:
    la   $t1, results
    nop
    sll  $t0, $s2, 2
    addu $t0, $t1, $t0
    lw   $t3, 0($t0)
    nop
    PUTHEX $t3
    PUTC 0x20
    addiu $s2, $s2, 1
    slti $t0, $s2, 8
    bne  $t0, $zero, out_loop
    nop

    PUTC 0x0A

    addiu $s0, $s0, 20
    addiu $s1, $s1, -1
    b    case_loop
    nop

all_done:
    PUTC 0x44
    PUTC 0x4F
    PUTC 0x4E
    PUTC 0x45
    PUTC 0x0A
hang:
    b     hang
    nop

.section .data
.align 4
results:
    .space 32
ncases:
    .word 12
.align 4
cases:
/*      rfc         gfc         bfc         rgbc        ir0 */
.word 0x00000000, 0x00000000, 0x00000000, 0x00808080, 0x00000000
.word 0x00000000, 0x00000000, 0x00000000, 0x00808080, 0x00001000
.word 0x00000800, 0x00000800, 0x00000800, 0x00808080, 0x00000800
.word 0x00000FF0, 0x00000000, 0x00000000, 0x00404040, 0x00000800
.word 0x00000000, 0x00000FF0, 0x00000000, 0x00404040, 0x00000400
.word 0x00000000, 0x00000000, 0x00000FF0, 0x00404040, 0x00000C00
.word 0x00000800, 0x00000400, 0x00000200, 0x00FFFFFF, 0x00001000
.word 0x00000000, 0x00000000, 0x00000000, 0x00FFFFFF, 0x00001000
.word 0x0000FFFF, 0x0000FFFF, 0x0000FFFF, 0x00000000, 0x00001000
.word 0xFFFFF800, 0xFFFFF800, 0xFFFFF800, 0x00808080, 0x00000800
.word 0x00007FFF, 0x00007FFF, 0x00007FFF, 0x00FF8000, 0x00007FFF
.word 0x00000100, 0x00000200, 0x00000300, 0x12345678, 0x00000123
