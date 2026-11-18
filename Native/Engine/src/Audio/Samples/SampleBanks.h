// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"

struct Audio_BankStreamState;
struct Audio_Instrument;
struct Audio_MusicSampleBankIds;

// decomp: D_80039BCC (D_80039BCC@SLUS_010.40): a view into another global
#define Audio_Samples_bankStreamDestination (*(int*)(&Audio_Samples_bankStream.spuAddress))

// decomp: D_80036770_t (src/SLUS_010.40/25AC.c:22)
struct Audio_MusicSampleBankIds {
    int bankId;
    int alternateBankId;
    int reserved8;
    int reservedC;
};
static_assert(sizeof(Audio_MusicSampleBankIds) == 0x10, "Audio_MusicSampleBankIds must keep its PS1 layout");

// decomp: D_80039BC8_t (src/SLUS_010.40/25AC.c:73)
struct Audio_BankStreamState {
    int* instrumentDestination; // next g_InstrumentInfo word to fill
    int spuAddress; // next SPU transfer address; 0 = start a new stream
    u_int spuBytesRemaining;
    u_int instrumentBytesRemaining;
};

// decomp: FSoundInstrumentInfo (src/include/sound.h:257)
struct Audio_Instrument {
    /* 0x00 */ u_int sampleStart;
    /* 0x04 */ u_int loopStart;
    /* 0x08 */ short fineTune;
    /* 0x0A */ short rootNote;
    /* 0x0C */ u_short adsrLower;
    /* 0x0E */ u_short adsrUpper;
};
static_assert(sizeof(Audio_Instrument) == 0x10, "Audio_Instrument must keep its PS1 layout");

extern Audio_MusicSampleBankIds Audio_Samples_musicSampleBankIds;
extern Audio_Instrument Audio_Samples_instruments[256];
extern int Audio_Samples_soundEffectBankIds[3];
extern int Audio_Samples_bankChunkHeader[16];
extern Audio_BankStreamState Audio_Samples_bankStream;
