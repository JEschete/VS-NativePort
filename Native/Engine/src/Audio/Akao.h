// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_AUDIO_AKAO_H_TYPES
#define VS_AUDIO_AKAO_H_TYPES
#include "Legacy/Psx.h"

typedef struct Audio_AkaoProgramFile Audio_AkaoProgramFile;
typedef struct Audio_AkaoTimestamp Audio_AkaoTimestamp;

// decomp: AkaoTimeStamp (src/SLUS_010.40/25AC.c:33)
typedef struct Audio_AkaoTimestamp {
    char yearBcd;
    char monthBcd;
    char dayBcd;
    char hoursBcd;
    char minutesBcd;
    char secondsBcd;
} Audio_AkaoTimestamp;

// decomp: FAkaoFileBlob (src/include/sound.h:197)
typedef struct Audio_AkaoProgramFile {
    /* 0x000 */ int magic; // AKAO
    /* 0x004 */ char reserved04[0xC]; // Padding? Version? Counts? Music Akao blobs have a
                                   // different flag in here...
    /* 0x010 */ u_short programOffsets[0x100]
                                      [2]; // Offsets into bytecode - indexed by Sfx ID
    /* 0x410 */ u_short metadataTableA[0x100]; // Some per sfx table
    /* 0x610 */ u_short metadataTableB[0x100]; // Some per sfx table
    /* 0x810 */ char programData[1]; // Sfx bytecode
} Audio_AkaoProgramFile;

#endif // VS_AUDIO_AKAO_H_TYPES
