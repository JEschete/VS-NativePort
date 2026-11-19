#pragma once
// The bound sound-effect program tables, and the header each program starts
// with.
#include <cstddef>
#include <cstdint>

namespace VS::Audio {
// decomp: vs_main_sfxContext (src/SLUS_010.40/25AC.h:9)
struct SfxProgramHeader {
  std::int32_t reserved0;
  std::int32_t reserved4;
  std::uint8_t reserved8;
  std::uint8_t flags;
};

namespace layout {
inline constexpr std::size_t kSfxProgramHeaderBytes = 0xC;
static_assert(sizeof(SfxProgramHeader) == kSfxProgramHeaderBytes);
}  // namespace layout
}  // namespace VS::Audio

namespace VS::Audio::Playback {
// decomp: g_Sound_SfxProgramOffsets (g_Sound_SfxProgramOffsets@SLUS_010.40)
extern std::uint32_t* soundEffectProgramOffsets;
// decomp: g_Sound_SfxMetadataTable (g_Sound_SfxMetadataTable@SLUS_010.40)
extern std::uint16_t* soundEffectBankSelectors;
// decomp: g_Sound_SfxProgramData (g_Sound_SfxProgramData@SLUS_010.40)
extern std::uint8_t* soundEffectProgramData;
}  // namespace VS::Audio::Playback
