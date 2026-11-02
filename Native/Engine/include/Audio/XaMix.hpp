#pragma once

#include <cstdint>
#include <optional>

namespace VS::Audio {
inline constexpr std::uint32_t kMaximumDiscMixVolume = 0xFF;
inline constexpr std::uint32_t kDefaultDiscMixVolume = 0x7F;

// The four CdlATV routes the original vs_sound_setCdVol hands to CdMix.
struct DiscMixMatrix {
  std::uint8_t left_to_left = 0;
  std::uint8_t left_to_right = 0;
  std::uint8_t right_to_left = 0;
  std::uint8_t right_to_right = 0;

  friend bool operator==(const DiscMixMatrix&, const DiscMixMatrix&) = default;
};
}  // namespace VS::Audio

namespace VS::Audio::Control {
// Stereo passes each side straight through; mono feeds both inputs to both
// outputs at the attenuated level. Returns nothing for a volume above 0xFF.
std::optional<DiscMixMatrix> SetDiscMixVolume(std::uint32_t volume, bool mono);
}  // namespace VS::Audio::Control

namespace VS::Audio::Seq {
// The signed CD output level is the high half of the sequencer's 16.16 disc
// volume.
std::int16_t WriteDiscVolume(std::int32_t disc_volume);
}  // namespace VS::Audio::Seq
