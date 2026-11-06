// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Audio/Samples/SampleBanks.h"

// decomp: D_80036770 (D_80036770@SLUS_010.40)
Audio_MusicSampleBankIds Audio_Samples_musicSampleBankIds;

// decomp: g_InstrumentInfo (g_InstrumentInfo@SLUS_010.40)
Audio_Instrument Audio_Samples_instruments[256];

// decomp: D_800377E0 (D_800377E0@SLUS_010.40)
int Audio_Samples_soundEffectBankIds[3];

// decomp: D_80037850 (D_80037850@SLUS_010.40)
int Audio_Samples_bankChunkHeader[16];

// decomp: D_80039BC8 (D_80039BC8@SLUS_010.40)
Audio_BankStreamState Audio_Samples_bankStream;

}  // extern "C"
