#pragma once
// The weather particle record and the screen weather fade. Weather.cpp
// defines the fade; VS::Field::weatherParticles (EnvironmentLiveState.hpp)
// holds the particle buffer.

#include <cstddef>
#include <cstdint>

#include "Core/Gte.hpp"

namespace VS::Field {
inline constexpr std::size_t kWeatherParticleRecordBytes = 0x10;

// One particle as the low-heap buffer holds it; the weather pool
// (Effects/WeatherPool.hpp) works on typed copies.
// decomp: D_800F1DD4_t (src/BATTLE/BATTLE.PRG/146C.h:13)
struct WeatherParticleRecord {
  VS::Core::Gte::SVECTOR position;
  std::int16_t screen_x;
  std::int16_t screen_y;
  std::uint8_t has_landed;
  std::uint8_t sway_phase;
  std::uint8_t fall_shift;
  std::uint8_t speed;
};
static_assert(sizeof(WeatherParticleRecord) == kWeatherParticleRecordBytes);

// The screen weather fade.
// decomp: D_800F1DC7 (D_800F1DC7@BATTLE/BATTLE.PRG,
// src/BATTLE/BATTLE.PRG/146C.c:367)
extern std::uint8_t screenWeatherFade;
}  // namespace VS::Field
