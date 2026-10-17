// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_CORE_MEMORY_H_TYPES
#define VS_CORE_MEMORY_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_CORE_MEMORY_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_CORE_MEMORY_H_DECLS)
#define VS_CORE_MEMORY_H_DECLS
#include "Core/Memory/Heap.h"

extern Core_HeapBlockHeader Core_legacyHeapList;
extern Core_HeapBlockHeader Core_legacySecondaryHeapList;

#endif // VS_CORE_MEMORY_H_DECLS
