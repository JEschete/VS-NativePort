#pragma once
// Hand-maintained (tools/port/owned.csv).
#ifndef VS_OWNED_CORE_MEMORY_WORDCOPY_H
#define VS_OWNED_CORE_MEMORY_WORDCOPY_H
#include "Legacy/Psx.h"

void Core_Memory_CopyAlignedWords(void* dest, void* src, u_int sz);

void Core_Memory_ZeroAlignedWords(void* dest, int sz);

#endif // VS_OWNED_CORE_MEMORY_WORDCOPY_H
