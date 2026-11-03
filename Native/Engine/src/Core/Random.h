// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_CORE_RANDOM_H_TYPES
#define VS_CORE_RANDOM_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_CORE_RANDOM_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_CORE_RANDOM_H_DECLS)
#define VS_CORE_RANDOM_H_DECLS

extern u_int Core_interfaceRandomState;
extern int Core_randomShuffleTable[97];
extern int Core_randomShuffleIndex;

#endif // VS_CORE_RANDOM_H_DECLS
