// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_CORE_MEMORY_HEAP_H_TYPES
#define VS_CORE_MEMORY_HEAP_H_TYPES
#include "Legacy/Psx.h"

typedef struct Core_HeapBlockHeader Core_HeapBlockHeader;

// decomp: vs_main_HeapHeader (src/SLUS_010.40/main.h:142)
typedef struct Core_HeapBlockHeader {
    struct Core_HeapBlockHeader* prev;
    struct Core_HeapBlockHeader* next;
    u_int blockSize;
    int reservedC;
} Core_HeapBlockHeader;

#endif // VS_CORE_MEMORY_HEAP_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_CORE_MEMORY_HEAP_H_DECLS)
#define VS_CORE_MEMORY_HEAP_H_DECLS

void Core_Memory_FreeFromHighEnd(void* block);
void Core_Memory_FreeFromLowEnd(void* block);
void* Core_Memory_AllocateFromHighEnd(u_int size);
void* Core_Memory_AllocateFromLowEnd(u_int size);
void Core_Memory_InitializeHeap(Core_HeapBlockHeader* node, u_int byte_count);

#endif // VS_CORE_MEMORY_HEAP_H_DECLS
