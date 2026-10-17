// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Core/Memory/Copy.h"

// decomp: BATTLE.PRG vs_battle_getShort (src/BATTLE/BATTLE.PRG/573B8.c:146)
short Core_Memory_ReadUnalignedShort(u_char* arg0)
{
    if (!((long)arg0 & 1)) {
        return *(short*)arg0;
    }
    return arg0[0] | (arg0[1] << 8);
}

// decomp: BATTLE.PRG vs_battle_copyAligned (src/BATTLE/BATTLE.PRG/573B8.c:242) [inline-asm: not yet ported]
void Core_Memory_CopyHalfwords(void* dst, const void* src, int numBytes)
{
    VS_UNPORTED("Core::Memory::CopyHalfwords");
}

// decomp: BATTLE.PRG vs_battle_memcpy (src/BATTLE/BATTLE.PRG/573B8.c:253) [inline-asm: not yet ported]
void Core_Memory_CopyHalfwordBlocks(void* dst, const void* src, int numBytes)
{
    VS_UNPORTED("Core::Memory::CopyHalfwordBlocks");
}

// decomp: BATTLE.PRG vs_battle_rMemzero (src/BATTLE/BATTLE.PRG/5BF94.c:4476)
void Core_Memory_ZeroWordsBackward(void* arg0, int arg1)
{
    int* var_v0;

    var_v0 = arg0 + arg1;
    do {
        *(int*)--var_v0 = 0;
    } while (arg0 != var_v0);
}

// decomp: BATTLE.PRG vs_battle_rMemcpy (src/BATTLE/BATTLE.PRG/5BF94.c:4486)
void Core_Memory_CopyBytesBackward(void* dest, void const* src, int size)
{
    do {
        --size;
        ((char*)dest)[size] = ((char const*)src)[size];
    } while (size != 0);
}

// decomp: BATTLE.PRG func_800E4288 (src/BATTLE/BATTLE.PRG/7B5DC.s:0) [asm-file: not yet ported]
void Core_Memory_ClearAlignedWords(void*, int)
{
    VS_UNPORTED("Core::Memory::ClearAlignedWords");
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

