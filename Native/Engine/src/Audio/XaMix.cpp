// Hand-maintained (tools/port/owned.csv).
#include "Audio/XaMix.hpp"

#include <cstdint>
#include <optional>

namespace VS::Audio::Control {
// decomp: Sound_SetCdMixVolume (src/SLUS_010.40/25AC.c:916; was
// vs_sound_setCdVol); original 80013230.
std::optional<DiscMixMatrix> SetDiscMixVolume(std::uint32_t volume, bool mono) {
  constexpr std::uint32_t kMonoMultiplier = 0xB570;
  constexpr std::uint32_t kMonoShift = 17;
  if (volume > kMaximumDiscMixVolume) return std::nullopt;
  if (mono) {
    const auto level =
        static_cast<std::uint8_t>((volume * kMonoMultiplier) >> kMonoShift);
    return DiscMixMatrix{level, level, level, level};
  }
  const auto level = static_cast<std::uint8_t>(volume);
  return DiscMixMatrix{level, 0, 0, level};
}
}  // namespace VS::Audio::Control

namespace VS::Audio::Seq {
// decomp: Sound_UpdateCdVolume (src/SLUS_010.40/25AC.c:4164; was
// UpdateCdVolume); original 80019134.
std::int16_t WriteDiscVolume(std::int32_t disc_volume) {
  constexpr int kLevelShift = 16;
  return static_cast<std::int16_t>(disc_volume >> kLevelShift);
}
}  // namespace VS::Audio::Seq
