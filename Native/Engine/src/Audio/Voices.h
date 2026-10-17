// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_AUDIO_VOICES_H_TYPES
#define VS_AUDIO_VOICES_H_TYPES
#include "Legacy/Psx.h"

typedef struct Audio_LegacyVoiceChannelState Audio_LegacyVoiceChannelState;

// decomp: FSound80094FA0 (src/include/sound.h:304)
typedef struct Audio_LegacyVoiceChannelState {
    /* 0x00 */ int reserved00;
    /* 0x04 */ int reserved04;
    /* 0x08 */ int reserved08;
    /* 0x0C */ int voicesInUseMask;
    /* 0x10 */ int voiceIndex;
    /* 0x14 */ u_int channelFlags;
    /* 0x18 */ int reserved18;
    /* 0x1C */ int reserved1C;
    /* 0x20 */ int reserved20;
    /* 0x24 */ int reserved24;
    /* 0x28 */ int reserved28;
    /* 0x2C */ int reserved2C;
    /* 0x30 */ int reserved30;
    /* 0x34 */ int reserved34;
    /* 0x38 */ int reserved38;
    /* 0x3C */ int reserved3C;
    /* 0x40 */ int volume;
    /* 0x44 */ int reserved44;
    /* 0x48 */ int reserved48;
    /* 0x4C */ char reserved4C;
    /* 0x4D */ char reserved4D;
    /* 0x4E */ char reserved4E;
    /* 0x4F */ char reserved4F;
    /* 0x50 */ int reserved50;
    /* 0x54 */ int reserved54;
    /* 0x58 */ int voiceSampleRate;
    /* 0x5C */ int controlFlags;
} Audio_LegacyVoiceChannelState;

#endif // VS_AUDIO_VOICES_H_TYPES
