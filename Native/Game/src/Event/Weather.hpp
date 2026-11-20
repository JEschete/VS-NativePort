#pragma once
// The scripted weather tween, in the layout the event script VM keeps it.
// Event/ScriptEffectCommands.hpp's WeatherTween is the value the tween update
// works on; ScriptEffectCommandsLive.cpp converts between the two.

#include <cstddef>
#include <cstdint>

namespace VS::Event {
// decomp: D_800F4B88_t (src/BATTLE/BATTLE.PRG/4DEA8.c:178)
struct WeatherTweenRecord {
  std::uint8_t reserved00;
  std::uint8_t reserved01;
  std::uint8_t elapsed_frames;
  // Zero while the weather is idle.
  std::uint8_t duration_frames;
  // The current wind direction in 4096ths of a turn, its initial value and the
  // shortest wrapped change to the target.
  std::int16_t wind_direction;
  std::int16_t start_wind_direction;
  std::int16_t delta_wind_direction;
  std::int16_t wind_strength;
  std::int16_t start_wind_strength;
  std::int16_t delta_wind_strength;
  std::int16_t intensity;
  std::int16_t start_intensity;
  std::int16_t delta_intensity;
};
inline constexpr std::size_t kWeatherTweenRecordBytes = 0x16;
static_assert(sizeof(WeatherTweenRecord) == kWeatherTweenRecordBytes);
}  // namespace VS::Event
