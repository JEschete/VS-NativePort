// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_AUDIO_STREAM_H_TYPES
#define VS_AUDIO_STREAM_H_TYPES
#include "Legacy/Psx.h"

typedef struct Audio_AkaoStreamHeader Audio_AkaoStreamHeader;
typedef struct Audio_CutsceneStreamData Audio_CutsceneStreamData;

// decomp: FAkaoHeader (src/include/sound.h:559)
typedef struct Audio_AkaoStreamHeader {
    /* 0x00 */ u_int magic;
    /* 0x04 */ int reserved04; // -> StreamState.field8_0x20
    /* 0x08 */ char reserved08[0x8];
    /* 0x10 */ u_int totalPages;
    /* 0x14 */ int reserved14;
    /* 0x18 */ int reserved18; // -> StreamState.field2_0x8
    /* 0x1C */ int sampleRate;
    /* 0x20 */ u_int currentPage;
    /* 0x24 */ char reserved24[0x4];
    /* 0x28 */ int reserved28; // outgoing voice reference for volume handoff
} Audio_AkaoStreamHeader;
_Static_assert(sizeof(Audio_AkaoStreamHeader) == 0x2C, "Audio_AkaoStreamHeader must keep its PS1 layout");

// decomp: FSoundCutsceneStreamData (src/include/sound.h:566)
typedef struct Audio_CutsceneStreamData {
    /* 0x00 */ char reserved00[0x80]; // Probably some pointers to Chunks
    /* 0x80 */ Audio_AkaoStreamHeader streamHeader;
    /* 0xAC */ char reservedAC[0x24];
    /* 0xD0 */ char audioData[1];
} Audio_CutsceneStreamData;
_Static_assert(sizeof(Audio_CutsceneStreamData) == 0xD4, "Audio_CutsceneStreamData must keep its PS1 layout");

#endif // VS_AUDIO_STREAM_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_AUDIO_STREAM_H_DECLS)
#define VS_AUDIO_STREAM_H_DECLS

int Audio_BeginXaStream(int sector, int byteCount);
int Audio_RequestXaStreamStop(void);
void Audio_StopCutsceneStream(void);
int Audio_PlayCutsceneStreamSlot(int slot_id, int pan, int volume);
void Audio_PlayCutsceneStreamSlotCentered(int slot_id);
int Audio_FreeCutsceneStreamSlot(int slot_id);
void Audio_SetXaVolumeMaximum(void);

#endif // VS_AUDIO_STREAM_H_DECLS
