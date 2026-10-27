// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Core/Memory/Copy.h"

// decomp: BATTLE.PRG vs_battle_getShort (src/BATTLE/BATTLE.PRG/573B8.c:146)
short Core_Memory_ReadUnalignedShort(u_char* source)
{
    if (!((long)source & 1)) {
        return *(short*)source;
    }
    return source[0] | (source[1] << 8);
}

// decomp: BATTLE.PRG vs_battle_rMemzero (src/BATTLE/BATTLE.PRG/5BF94.c:4476)
void Core_Memory_ZeroWordsBackward(void* destination, int byte_count)
{
    int* word_cursor;

    word_cursor = destination + byte_count;
    do {
        *(int*)--word_cursor = 0;
    } while (destination != word_cursor);
}

// decomp: BATTLE.PRG vs_battle_rMemcpy (src/BATTLE/BATTLE.PRG/5BF94.c:4486)
void Core_Memory_CopyBytesBackward(void* dest, void const* src, int size)
{
    do {
        --size;
        ((char*)dest)[size] = ((char const*)src)[size];
    } while (size != 0);
}

// decomp: SLUS_010.40 Sound_memcpy32 (src/SLUS_010.40/25AC.c:3965)
void Core_Memory_CopyWords(void* in_Src, void* in_Dst, u_int in_Size)
{
    int w0, w1, w2, w3;

    in_Size >>= 2;

    while ((in_Size >> 2) != 0) {
        w0 = ((int*)in_Src)[0];
        w1 = ((int*)in_Src)[1];
        w2 = ((int*)in_Src)[2];
        w3 = ((int*)in_Src)[3];
        ((int*)in_Dst)[0] = w0;
        ((int*)in_Dst)[1] = w1;
        ((int*)in_Dst)[2] = w2;
        ((int*)in_Dst)[3] = w3;
        in_Src += 0x10;
        in_Dst += 0x10;
        in_Size -= 4;
    }

    while (in_Size != 0) {
        *(int*)in_Dst = *(int*)in_Src;
        in_Dst += 4;
        in_Src += 4;
        in_Size--;
    }
}

// decomp: SLUS_010.40 Sound_memswp32 (src/SLUS_010.40/25AC.c:3993)
void Core_Memory_SwapWords(void* in_A, void* in_B, u_int in_Size)
{
    int a0, a1, b0, b1;

    in_Size >>= 2;

    while ((in_Size >> 1) != 0) {
        a0 = ((int*)in_A)[0];
        b0 = ((int*)in_B)[0];
        a1 = ((int*)in_A)[1];
        b1 = ((int*)in_B)[1];

        ((int*)in_B)[0] = a0;
        ((int*)in_B)[1] = a1;
        ((int*)in_A)[0] = b0;
        ((int*)in_A)[1] = b1;

        in_A += 8;
        in_B += 8;

        in_Size -= 2;
    }

    if (in_Size != 0) {
        a0 = *(int*)in_A;
        b0 = *(int*)in_B;
        *(int*)in_B = a0;
        *(int*)in_A = b0;
    }
}

