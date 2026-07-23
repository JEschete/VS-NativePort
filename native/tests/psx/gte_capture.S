/* GTE golden-vector capture ROM.
 *
 * Runs a table of GTE cases on real COP2 hardware (as emulated by DuckStation)
 * and prints inputs + outputs over BIOS TTY. The host parses the log into
 * native/tests/gte_golden.txt, which turns our C GTE tests into a true
 * differential test.
 *
 * Line format (all 8-digit hex, space separated):
 *   GTE <16 input words> <11 output words>
 * inputs : c0 c1 c2 c3 c4 trx try trz ofx ofy h dqa dqb vxy0 vz0 op
 * outputs: ir0 ir1 ir2 ir3 sxy2 sz3 mac0 mac1 mac2 mac3 flag
 * op     : 0 = RTPS, 1 = MVMVA(1,0,0,3,0)
 */
.set noreorder
.set noat

/* BIOS calls are inlined via macros: routing them through a subroutine
 * corrupts the return path (proven by bisect), so this ROM uses no calls. */
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

    /* Enable COP2 (GTE) in the Status Register: without SR bit30 (CU2) every
     * ctc2/mtc2/rtps raises a Coprocessor Unusable exception. Games get this
     * from the PSYQ runtime; a bare PS-EXE must do it itself. */
    mfc0  $t0, $12
    nop
    lui   $t1, 0x4000
    or    $t0, $t0, $t1
    mtc0  $t0, $12
    nop
    nop
    la    $s0, cases
    la    $t0, ncases
    lw    $s1, 0($t0)
    nop

case_loop:
    blez  $s1, all_done
    nop

    /* ---- load control registers ---- */
    lw $t0,  0($s0)
    nop
    ctc2 $t0, $0
    lw $t0,  4($s0)
    nop
    ctc2 $t0, $1
    lw $t0,  8($s0)
    nop
    ctc2 $t0, $2
    lw $t0, 12($s0)
    nop
    ctc2 $t0, $3
    lw $t0, 16($s0)
    nop
    ctc2 $t0, $4
    lw $t0, 20($s0)
    nop
    ctc2 $t0, $5
    lw $t0, 24($s0)
    nop
    ctc2 $t0, $6
    lw $t0, 28($s0)
    nop
    ctc2 $t0, $7
    lw $t0, 32($s0)
    nop
    ctc2 $t0, $24
    lw $t0, 36($s0)
    nop
    ctc2 $t0, $25
    lw $t0, 40($s0)
    nop
    ctc2 $t0, $26
    lw $t0, 44($s0)
    nop
    ctc2 $t0, $27
    lw $t0, 48($s0)
    nop
    ctc2 $t0, $28
    ctc2 $zero, $31          /* clear FLAG */

    /* ---- load V0 ---- */
    lw $t0, 52($s0)
    nop
    mtc2 $t0, $0
    lw $t0, 56($s0)
    nop
    mtc2 $t0, $1
    nop
    nop
    nop

    /* ---- execute ---- */
    lw   $t0, 60($s0)
    nop
    beq  $t0, $zero, op_rtps
    nop
    .word 0x4A486012         /* mvmva 1,0,0,3,0 */
    b    op_done
    nop
op_rtps:
    .word 0x4A180001         /* rtps */
op_done:
    nop
    nop
    nop
    nop

    /* ---- collect results ---- */
    la $s5, results
    mfc2 $t0, $8
    nop
    sw   $t0,  0($s5)
    mfc2 $t0, $9
    nop
    sw   $t0,  4($s5)
    mfc2 $t0, $10
    nop
    sw   $t0,  8($s5)
    mfc2 $t0, $11
    nop
    sw   $t0, 12($s5)
    mfc2 $t0, $14
    nop
    sw   $t0, 16($s5)
    mfc2 $t0, $19
    nop
    sw   $t0, 20($s5)
    mfc2 $t0, $24
    nop
    sw   $t0, 24($s5)
    mfc2 $t0, $25
    nop
    sw   $t0, 28($s5)
    mfc2 $t0, $26
    nop
    sw   $t0, 32($s5)
    mfc2 $t0, $27
    nop
    sw   $t0, 36($s5)
    cfc2 $t0, $31
    nop
    sw   $t0, 40($s5)

    /* ---- print "GTE " ---- */
    PUTC 0x47
    PUTC 0x54
    PUTC 0x45
    PUTC 0x20

    /* ---- print 16 inputs ---- */
    move $s2, $zero
in_loop:
    sll  $t0, $s2, 2
    addu $t0, $s0, $t0
    lw   $t3, 0($t0)
    nop
    PUTHEX $t3
    PUTC 0x20
    addiu $s2, $s2, 1
    slti $t0, $s2, 16
    bne  $t0, $zero, in_loop
    nop

    /* ---- print 11 outputs ---- */
    move $s2, $zero
out_loop:
    la   $t1, results
    sll  $t0, $s2, 2
    addu $t0, $t1, $t0
    lw   $t3, 0($t0)
    nop
    PUTHEX $t3
    PUTC 0x20
    addiu $s2, $s2, 1
    slti $t0, $s2, 11
    bne  $t0, $zero, out_loop
    nop

    PUTC 0x0A

    addiu $s0, $s0, 64
    addiu $s1, $s1, -1
    b    case_loop
    nop

all_done:
    PUTC 0x47
    PUTC 0x54
    PUTC 0x45
    PUTC 0x2D
    PUTC 0x44
    PUTC 0x4F
    PUTC 0x4E
    PUTC 0x45
    PUTC 0x0A
hang:
    b     hang
    nop


.section .rodata
donemsg:
    .asciz "GTE-DONE\n"

.section .data
.align 4
results:
    .space 44

ncases:
    .word 16

.align 4
cases:
/* identity rotation = 4096 (1.0 in 1.3.12) */
/*  c0         c1         c2         c3         c4        trx  try  trz  ofx        ofy        h      dqa    dqb        vxy0       vz0        op */
.word 0x00001000,0x00000000,0x00001000,0x00000000,0x00001000, 0,0,0, 0x00000000,0x00000000,0x00000190,0x00000000,0x00000000,0x00C80064,0x0000012C,0
.word 0x00001000,0x00000000,0x00001000,0x00000000,0x00001000, 10,20,30, 0x00000000,0x00000000,0x00000190,0x00000000,0x00000000,0x00C80064,0x0000012C,0
.word 0x00001000,0x00000000,0x00001000,0x00000000,0x00001000, 0,0,0, 0x00000000,0x00000000,0x00000190,0x00000000,0x00000000,0xFF38FF9C,0x0000FED4,0
.word 0x00001000,0x00000000,0x00001000,0x00000000,0x00001000, 0,0,0, 0x00000000,0x00000000,0x000003E8,0x00000000,0x00000000,0x00000000,0x00000000,0
.word 0x00001000,0x00000000,0x00001000,0x00000000,0x00001000, 0x7FFFFF,0x7FFFFF,0x7FFFFF, 0,0,0x00000190,0,0,0x7FFF7FFF,0x00007FFF,0
.word 0x00001000,0x00000000,0x00001000,0x00000000,0x00001000, 0,0,0, 0,0,0x00001000,0,0,0x00000000,0x0000000A,0
.word 0xF4B00B50,0x0B500000,0x00000B50,0x00000000,0x00001000, 0,0,0, 0,0,0x00000190,0,0,0x00C80064,0x0000012C,0
.word 0x00001000,0x00000000,0x00001000,0x00000000,0x00001000, 0,0,0, 0x01400000,0x00F00000,0x00000190,0,0,0x00C80064,0x0000012C,0
.word 0x00001000,0x00000000,0x00001000,0x00000000,0x00001000, 0,0,0, 0,0,0x00000190,0x0000FF9C,0x0C000000,0x00C80064,0x0000012C,0
.word 0x00001000,0x00000000,0x00001000,0x00000000,0x00001000, 0,0,0, 0,0,0x00000001,0,0,0x00010001,0x00000001,0
.word 0xF4B00B50,0x0B500000,0x00000B50,0x00000000,0x00001000, 100,200,300, 0x01400000,0x00F00000,0x00000200,0x00000064,0x01000000,0xFF38FF9C,0x000001F4,0
.word 0x00001000,0x00000000,0x00001000,0x00000000,0x00001000, 0,0,0, 0,0,0x00000190,0,0,0x00C80064,0x0000012C,1
.word 0xF4B00B50,0x0B500000,0x00000B50,0x00000000,0x00001000, 0,0,0, 0,0,0x00000190,0,0,0x00C80064,0x0000012C,1
.word 0x00001000,0x00000000,0x00001000,0x00000000,0x00001000, 0,0,0, 0,0,0x00000190,0,0,0x7FFF7FFF,0x00007FFF,1
.word 0x80008000,0x80008000,0x80008000,0x80008000,0x00008000, 0,0,0, 0,0,0x00000190,0,0,0x7FFF7FFF,0x00007FFF,1
.word 0x00001000,0x00000000,0x00001000,0x00000000,0x00001000, -1,-1,-1, 0,0,0x00000190,0,0,0xFF38FF9C,0x0000FED4,1
