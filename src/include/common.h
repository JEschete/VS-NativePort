#pragma once
#include <stddef.h>

#define INCLUDE_CMN(SECTION, FOLDER, NAME)                                               \
    __asm__(".pushsection ." #SECTION ";"                                                \
            ".set push;"                                                                 \
            ".set noat;"                                                                 \
            ".set noreorder;"                                                            \
            ".include \"" FOLDER "/" #NAME ".s\";"                                       \
            ".set pop;"                                                                  \
            ".popsection")

#define BIOS_STUB(name, table, id)                                                       \
    __asm__(".set push;"                                                                 \
            ".set noat;"                                                                 \
            ".set noreorder;"                                                            \
            "glabel " #name ";"                                                          \
            "li  $t2, " #table ";"                                                       \
            "jr  $t2;"                                                                   \
            "li  $t1, " #id ";"                                                          \
            "endlabel " #name ";"                                                        \
            ".set pop;");

#define SYSCALL(id)                                                                      \
    __asm__("li $a0, " #id ";"                                                           \
            "syscall 0;");

#if !defined(PERMUTER) && !defined(OBJDIFF) && !defined(NATIVE_BUILD)
#define INCLUDE_ASM(FOLDER, NAME) INCLUDE_CMN(text, FOLDER, NAME)
#define INCLUDE_RODATA(FOLDER, NAME) INCLUDE_CMN(rodata, FOLDER, NAME)
#else
#define INCLUDE_ASM(FOLDER, NAME)
#define INCLUDE_RODATA(FOLDER, NAME)
#endif // PERMUTER && OBJDIFF

/* The native (x86) target cannot emit MIPS. NATIVE_BUILD stubs out the macros
 * that inject assembly; the affected functions become undefined symbols and are
 * picked up by the generated stub layer until the platform layer implements
 * them for real. The PSX build is unaffected. */
#ifdef NATIVE_BUILD
#undef BIOS_STUB
#define BIOS_STUB(name, table, id)
#undef SYSCALL
#define SYSCALL(id)
#endif

#if defined(PERMUTER)
#define __attribute__(x)
#define __asm__(...)
#endif // PERMUTER