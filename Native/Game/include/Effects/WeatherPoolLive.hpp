#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Effects/WeatherPool.hpp"

namespace VS::Field {
struct WeatherParticleRecord;
}  // namespace VS::Field

// Live binding for the weather particle pool (WeatherPool.hpp) over the copied
// weather globals (Field/EnvironmentLiveState.hpp and
// VS::Field::screenWeatherFade), the room record's weather kind and area,
// rand(), the sine table, the low-end heap and the room weather
// (Field/EnvironmentLive.hpp). The entries keep the original call shapes
// (BATTLE.PRG 146C.c:17445-18221) for the room loading, the room weather, the
// ambience binding, the weather draws and the module pointers. The particle
// buffer is the 0x800-byte block LiveAllocateWeatherParticles makes: a spawn
// larger than it, a missing buffer or output, and a particle outside the buffer
// are refused with a diagnostic.
namespace VS::Effects {
// func_8008E8F8 (146C.c:17445).
void LiveAllocateWeatherParticles();
// func_8008EA90 (146C.c:17500): 1 when the room has a record, else 0.
int LiveInitializeRoomWeather(int suppress_rain);
// func_8008EB04 (146C.c:17525).
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void LiveRoomWeatherState(int* weather_kind_output, int* intensity_output);
// func_8008F29C (146C.c:17723).
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void LiveInitializeFallingWeatherParticles(int particle_count,
                                           int weather_kind);
// func_8008F30C (146C.c:17737).
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void LiveInitializeScreenWeatherParticles(int particle_count, int weather_kind);
// func_80090A1C (146C.c:18200).
void LiveResetFallingWeatherParticle(
    VS::Field::WeatherParticleRecord* weather_particle);
// func_80090B28 (146C.c:18221).
void LiveRecalculateWeatherVelocity();
}  // namespace VS::Effects
