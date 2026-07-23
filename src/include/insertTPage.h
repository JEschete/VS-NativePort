#pragma once

#ifdef NATIVE_BUILD
/* Native (x86) reimplementation of the MIPS asm below.
 *
 * The original bump-allocates a 12-byte GPU primitive out of the PS1
 * scratchpad at 0x1F800000 and links a tpage command into the ordering table:
 *   scratch[0] = next free primitive pointer
 *   scratch[1] = ordering table base
 * Semantics are preserved exactly; only the addressing is host-side.
 */
#include <psx_scratchpad.h>

static inline void _insertTPage(int after, int arg1)
{
    unsigned long* scratch = _psx_scratchpad();
    unsigned long* prim = (unsigned long*)scratch[0];
    unsigned long* otEntry = (unsigned long*)(scratch[1] + ((unsigned long)after << 2));
    unsigned long head = *otEntry;

    prim[1] = 0xE1000000u | 0x200u | ((unsigned long)arg1 & 0x1FFu);
    prim[2] = 0;
    prim[0] = 0x02000000u | (head & 0x00FFFFFFu);

    *otEntry = ((unsigned long)prim & 0x00FFFFFFu) | (head & 0xFF000000u);
    scratch[0] = (unsigned long)(prim + 3);
}
#else

#define _insertTPage(after, arg1)                                                        \
    __asm__("scratch = $t3;"                                                             \
            "tpageOp = $t4;"                                                             \
            "li         scratch, 0x1F800000;"                                            \
            "sll        $t0, %0, 2;"                                                     \
            "lw         $t4, 4(scratch);"                                                \
            "lw         $t7, 0(scratch);"                                                \
            "addu       $t0, $t4;"                                                       \
            "lw         $t1, 0($t0);"                                                    \
            "li         tpageOp, 0xE1000000;"                                            \
            "and        $t6, %1, 0x1FF;"                                                 \
            "or         tpageOp, 0x200;"                                                 \
            "or         tpageOp, $t6;"                                                   \
            "sw         tpageOp, 4($t7);"                                                \
            "sw         $zero, 8($t7);"                                                  \
            "li         $t5, 0xFFFFFF;"                                                  \
            "li         $t4, 0x2000000;"                                                 \
            "li         $t6, 0xFF000000;"                                                \
            "and        $t2, $t1, $t5;"                                                  \
            "or         $t4, $t2;"                                                       \
            "sw         $t4, 0($t7);"                                                    \
            "and        $t2, $t1, $t6;"                                                  \
            "and        $t4, $t7, $t5;"                                                  \
            "or         $t4, $t2;"                                                       \
            "sw         $t4, 0($t0);"                                                    \
            "addu       $t2, $t7, 0xC;"                                                  \
            "sw         $t2, 0(scratch);"                                                \
            :                                                                            \
            : "r"(after), "r"(arg1)                                                      \
            : "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7")

#endif /* NATIVE_BUILD */
