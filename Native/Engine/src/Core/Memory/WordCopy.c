// Hand-maintained (tools/port/owned.csv).
// The game's word copy and clear, written in MIPS assembly in the original (main.c:11669, 11699). Both move whole
// 32-bit words: a size's low two bits are ignored, and the clear only reads the low 16 bits of its size.
#include "Core/Memory/WordCopy.h"
#include "Legacy/Psx.h"
#include "Core/Memory.h"

#include <string.h>

// decomp: SLUS_010.40 vs_main_memcpy (src/SLUS_010.40/main.c:11669)
void Core_Memory_CopyAlignedWords(void* dest, void* src, u_int sz)
{
    memcpy(dest, src, sz & ~3u);
}

// decomp: SLUS_010.40 vs_main_bzero (src/SLUS_010.40/main.c:11699)
void Core_Memory_ZeroAlignedWords(void* dest, int sz)
{
    memset(dest, 0, (u_int)sz & 0xFFFCu);
}
