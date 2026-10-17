// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_AUDIO_SEQ_AKAOFORMAT_H_TYPES
#define VS_AUDIO_SEQ_AKAOFORMAT_H_TYPES
#include "Legacy/Psx.h"

typedef struct Audio_AkaoFileHeader Audio_AkaoFileHeader;
typedef struct Audio_AkaoSequenceFile Audio_AkaoSequenceFile;
typedef struct Audio_KeymapEntry Audio_KeymapEntry;

#ifndef VS_TYPES_ONLY
#define VS_TYPES_ONLY
#define VS_AUDIO_SEQ_AKAOFORMAT_H_SET_TYPES_ONLY
#endif
#include "Audio/Akao.h"
#ifdef VS_AUDIO_SEQ_AKAOFORMAT_H_SET_TYPES_ONLY
#undef VS_TYPES_ONLY
#undef VS_AUDIO_SEQ_AKAOFORMAT_H_SET_TYPES_ONLY
#endif

// decomp: AkaoSeqHeader (src/SLUS_010.40/25AC.c:41)
typedef struct Audio_AkaoFileHeader {
    char magic[4];
    u_short sequenceId;
    u_short length;
    u_short reverbMode;
    Audio_AkaoTimestamp timestamp;
} Audio_AkaoFileHeader;

// decomp: FAkaoSequence (src/include/sound.h:444)
typedef struct Audio_AkaoSequenceFile {
    /* 0x00 */ u_int reserved00;
    /* 0x04 */ u_int reserved04;
    /* 0x08 */ u_int reserved08;
    /* 0x0C */ u_int reserved0C;

    /* 0x10 */ u_int reserved10;
    /* 0x14 */ u_int sampleBankId;
    /* 0x18 */ u_int reserved18;
    /* 0x1C */ u_int instrumentDataOffset;

    /* 0x20 */ u_int channelEnableMask;
    /* 0x24 */ u_int keyedMask;
    /* 0x28 */ u_int allocatedVoiceMask;
    /* 0x2C */ u_int reserved2C;

    /* 0x30 */ int patchRegionOffset; // field-relative
    /* 0x34 */ int keymapRegionOffset; // field-relative
    /* 0x38 */ u_int resumeKeyOnMask;
    /* 0x3C */ u_int pushReleaseMask;
    /* 0x40 */ char payload[1]; // starts at 0x40 (variable length)
} Audio_AkaoSequenceFile;

// decomp: FSoundKeymapEntry8 (src/include/sound.h:421)
typedef struct Audio_KeymapEntry {
    signed char instrumentIndex;
    char note;
    char attackRate;
    char sustainRate;
    char sustainMode; /* values 3/5/7 map to 0x4000/0x8000/0xC000 */
    char releaseRate;
    char volumeScale;
    char panAndReverb; /* low 7 bits pan, high bit reverb-enable */
} Audio_KeymapEntry;

#endif // VS_AUDIO_SEQ_AKAOFORMAT_H_TYPES
