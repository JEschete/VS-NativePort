#pragma once
// The vibration preset directory and its one pattern (decomp: D_8004A504,
// main.c:139), read by PlayLiveVibrationPreset.
#include <array>
#include <cstddef>
#include <cstdint>

namespace VS::Input {
inline constexpr std::size_t kVibrationPresetBytes = 24;

extern std::array<std::uint8_t, kVibrationPresetBytes> vibrationPresets;
}  // namespace VS::Input
