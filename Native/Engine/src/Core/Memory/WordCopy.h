#pragma once
// Hand-maintained (tools/port/owned.csv).
#ifndef VS_OWNED_CORE_MEMORY_WORDCOPY_H
#define VS_OWNED_CORE_MEMORY_WORDCOPY_H
#include "Legacy/Psx.h"

#ifdef __cplusplus
extern "C" {
#endif

void Core_Memory_CopyAlignedWords(void* dest, void* src, u_int sz);

void Core_Memory_ZeroAlignedWords(void* dest, int sz);

void Core_Memory_ClearAlignedWords(void* destination, int size);

#ifdef __cplusplus
}
#endif

void Core_Memory_CopyHalfwords(void* dst, const void* src, int num_bytes);

void Core_Memory_CopyHalfwordBlocks(void* dst, const void* src, int num_bytes);

#endif  // VS_OWNED_CORE_MEMORY_WORDCOPY_H
