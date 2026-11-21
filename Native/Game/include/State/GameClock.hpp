#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <bit>
#include <cstdint>

#include "State/Clock.hpp"

// The play time clock the save files and menus show (decomp:
// vs_main_gametime, SLUS_010.40 main.h:173 vs_Gametime_t). It stops at 100
// hours.
namespace VS::State {
inline constexpr int kMaxPlayTimeHours = 100;

using PlayTime = ClockTime;

// The save files and the clear records store the play time as one word, with
// frames in the low byte, then seconds, minutes and hours (the original's
// vs_Gametime_t.all).
static_assert(sizeof(PlayTime) == sizeof(std::uint32_t));
static_assert(std::endian::native == std::endian::little,
              "the packed play time keeps frames in the low byte");
[[nodiscard]] constexpr std::uint32_t PackPlayTime(
    const PlayTime& time) noexcept {
  return std::bit_cast<std::uint32_t>(time);
}
[[nodiscard]] constexpr PlayTime UnpackPlayTime(std::uint32_t packed) noexcept {
  return std::bit_cast<PlayTime>(packed);
}

// decomp: SLUS_010.40 vs_main_gametimeUpdate (src/SLUS_010.40/main.c:1050), its
// clock part: advances by `tick_speed` frames, carrying whole seconds, minutes
// and hours, unless the clock has reached kMaxPlayTimeHours.
void AdvancePlayTime(PlayTime& time, int tick_speed) noexcept;
}  // namespace VS::State
