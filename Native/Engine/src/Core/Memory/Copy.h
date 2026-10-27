// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_CORE_MEMORY_COPY_H_TYPES
#define VS_CORE_MEMORY_COPY_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_CORE_MEMORY_COPY_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_CORE_MEMORY_COPY_H_DECLS)
#define VS_CORE_MEMORY_COPY_H_DECLS

short Core_Memory_ReadUnalignedShort(u_char* source);
void Core_Memory_ZeroWordsBackward(void* destination, int byte_count);
void Core_Memory_CopyBytesBackward(void* dest, void const* src, int size);
void Core_Memory_CopyWords(void* in_Src, void* in_Dst, u_int in_Size);
void Core_Memory_SwapWords(void* in_A, void* in_B, u_int in_Size);

#endif // VS_CORE_MEMORY_COPY_H_DECLS
