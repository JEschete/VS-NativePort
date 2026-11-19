#pragma once
// The sample banks' records: the music bank ids, each instrument's samples
// and envelope, the sound-effect bank ids and the bank upload stream.
#include <cstddef>
#include <cstdint>

#include "Audio/SampleBanks.hpp"
#include "Audio/VoiceOutput.hpp"

namespace VS::Audio {
// decomp: D_80036770_t (src/SLUS_010.40/25AC.c:22)
struct MusicSampleBankIds {
  std::int32_t bank_id;
  std::int32_t alternate_bank_id;
  std::int32_t reserved8;
  std::int32_t reservedc;
};

// decomp: D_80039BC8_t (src/SLUS_010.40/25AC.c:73)
struct LegacyBankStreamState {
  std::int32_t* instrument_destination;  // next g_InstrumentInfo word to fill
  // The next SPU transfer address (D_80039BCC); zero starts a new stream.
  std::int32_t spu_address;
  std::uint32_t spu_bytes_remaining;
  std::uint32_t instrument_bytes_remaining;
};

// decomp: FSoundInstrumentInfo (src/include/sound.h:257)
struct Instrument {
  /* 0x00 */ std::uint32_t sample_start;
  /* 0x04 */ std::uint32_t loop_start;
  /* 0x08 */ std::int16_t fine_tune;
  /* 0x0A */ std::int16_t root_note;
  /* 0x0C */ std::uint16_t adsr_lower;
  /* 0x0E */ std::uint16_t adsr_upper;
};

namespace layout {
inline constexpr std::size_t kMusicSampleBankIdsBytes = 0x10;
static_assert(sizeof(MusicSampleBankIds) == kMusicSampleBankIdsBytes);
static_assert(sizeof(Instrument) == Samples::kInstrumentRecordBytes);
}  // namespace layout
}  // namespace VS::Audio

namespace VS::Audio::Samples {
// decomp: D_80036770 (D_80036770@SLUS_010.40)
extern MusicSampleBankIds musicSampleBankIds;
// decomp: g_InstrumentInfo (g_InstrumentInfo@SLUS_010.40)
extern Instrument instruments[kInstrumentSlotCount];
// decomp: D_800377E0 (D_800377E0@SLUS_010.40)
extern std::int32_t soundEffectBankIds[kSoundEffectBankSlots];
// decomp: D_80037850 (D_80037850@SLUS_010.40)
extern std::int32_t bankChunkHeader[kChunkHeaderWords];
// Its spu_address is the decomp's D_80039BCC view.
// decomp: D_80039BC8 (D_80039BC8@SLUS_010.40)
extern LegacyBankStreamState bankStream;
}  // namespace VS::Audio::Samples
