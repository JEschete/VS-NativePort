// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_AUDIO_LEGACY_H_TYPES
#define VS_AUDIO_LEGACY_H_TYPES
#include "Legacy/Psx.h"


// decomp: src/include/sound.h:545
#define SPU_MALLOC_NUM_BLOCKS 4

#endif // VS_AUDIO_LEGACY_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_AUDIO_LEGACY_H_DECLS)
#define VS_AUDIO_LEGACY_H_DECLS

extern int Audio_legacyUnreferencedTable[795];
extern int Audio_legacyUnreferencedWord;
extern char Audio_legacySpuAllocationTable[SPU_MALLOC_RECSIZ * (SPU_MALLOC_NUM_BLOCKS + 1)];;
extern volatile int Audio_legacySpuTransferBusy;

#endif // VS_AUDIO_LEGACY_H_DECLS
