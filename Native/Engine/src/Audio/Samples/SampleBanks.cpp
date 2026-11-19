// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Audio/Samples/SampleBanks.hpp"

// decomp: D_80036770 (D_80036770@SLUS_010.40)
VS::Audio::MusicSampleBankIds VS::Audio::Samples::musicSampleBankIds;

// decomp: g_InstrumentInfo (g_InstrumentInfo@SLUS_010.40)
VS::Audio::Instrument VS::Audio::Samples::instruments[VS::Audio::Samples::kInstrumentSlotCount];

// decomp: D_800377E0 (D_800377E0@SLUS_010.40)
int VS::Audio::Samples::soundEffectBankIds[VS::Audio::Samples::kSoundEffectBankSlots];

// decomp: D_80037850 (D_80037850@SLUS_010.40)
int VS::Audio::Samples::bankChunkHeader[VS::Audio::Samples::kChunkHeaderWords];

// decomp: D_80039BC8 (D_80039BC8@SLUS_010.40)
VS::Audio::LegacyBankStreamState VS::Audio::Samples::bankStream;

