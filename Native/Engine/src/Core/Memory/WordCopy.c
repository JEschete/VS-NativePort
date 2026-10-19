// Hand-maintained (tools/port/owned.csv).
// The SLUS word copy and clear, written in MIPS assembly in the original
// (main.c:11669, 11699). Both move whole 32-bit words: a size's low two bits
// are ignored, and the clear only reads the low 16 bits of its size.
#include "Core/Memory/WordCopy.h"

#include <stdint.h>
#include <string.h>

#include "Core/Memory.h"
#include "Legacy/Psx.h"

enum {
  kWordRemainderMask = sizeof(uint32_t) - 1,
  kHalfwordBlockElements = 4,
  kHalfwordBlockBytes = kHalfwordBlockElements * sizeof(uint16_t)
};

// decomp: SLUS_010.40 vs_main_memcpy (src/SLUS_010.40/main.c:11669)
void Core_Memory_CopyAlignedWords(void* dest, void* src, u_int sz) {
  memcpy(dest, src, sz & ~(u_int)kWordRemainderMask);
}

// decomp: SLUS_010.40 vs_main_bzero (src/SLUS_010.40/main.c:11699)
void Core_Memory_ZeroAlignedWords(void* dest, int sz) {
  memset(dest, 0, (u_int)sz & (UINT16_MAX & ~(u_int)kWordRemainderMask));
}

// decomp: BATTLE.PRG func_800E4288 (F01758,
// src/BATTLE/BATTLE.PRG/7B5DC.s:344-367). Assembly clears size >> 4
// sixteen-byte blocks, then (size & 12) bytes in words. Unlike vs_main_bzero,
// the size is not truncated to 16 bits. Native extension: nonpositive sizes are
// a no-op instead of entering the assembly's invalid-size loop; any trailing
// sub-word bytes are left untouched.
void Core_Memory_ClearAlignedWords(void* destination, int size) {
  if (size > 0) {
    memset(destination, 0, (unsigned int)size & ~(unsigned)kWordRemainderMask);
  }
}

// decomp: BATTLE.PRG vs_battle_copyAligned (src/BATTLE/BATTLE.PRG/573B8.c:242)
// The assembly copies leading halfwords before entering vs_battle_memcpy.
// Native extension: nonpositive sizes do nothing; incomplete halfwords are
// ignored.
void Core_Memory_CopyHalfwords(void* dst, const void* src, int num_bytes) {
  if (num_bytes <= 0) {
    return;
  }
  unsigned char* destination = dst;
  const unsigned char* source = src;
  const unsigned int bytes =
      (unsigned int)num_bytes & ~(unsigned int)(sizeof(uint16_t) - 1);
  const unsigned int leading = bytes & (kHalfwordBlockBytes - 1);
  for (unsigned int offset = 0; offset < leading; offset += sizeof(uint16_t)) {
    uint16_t halfword;
    memcpy(&halfword, source + offset, sizeof halfword);
    memcpy(destination + offset, &halfword, sizeof halfword);
  }
  Core_Memory_CopyHalfwordBlocks(destination + leading, source + leading,
                                 (int)(bytes - leading));
}

// decomp: BATTLE.PRG vs_battle_memcpy (src/BATTLE/BATTLE.PRG/573B8.c:253)
// All four halfwords are loaded before the block is stored, preserving forward
// overlap. Native extension: nonpositive sizes and incomplete eight-byte blocks
// are ignored.
void Core_Memory_CopyHalfwordBlocks(void* dst, const void* src, int num_bytes) {
  if (num_bytes <= 0) {
    return;
  }
  unsigned char* destination = dst;
  const unsigned char* source = src;
  const unsigned int bytes =
      (unsigned int)num_bytes & ~(unsigned int)(kHalfwordBlockBytes - 1);
  for (unsigned int offset = 0; offset < bytes; offset += kHalfwordBlockBytes) {
    uint16_t block[kHalfwordBlockElements];
    memcpy(block, source + offset, sizeof block);
    memcpy(destination + offset, block, sizeof block);
  }
}
