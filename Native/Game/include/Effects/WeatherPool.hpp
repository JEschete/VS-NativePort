#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Field/Background.hpp"
#include "Field/Environment.hpp"

// The weather particle pool (BATTLE.PRG 146C.c:17445-17737 and 18200-18221):
// the room's 0x800-byte particle buffer, the room weather's start from the
// room record, the kind and intensity the ambience reads, the falling and
// screen weather's particle spawns, a falling particle's respawn over the
// room's weather area and the particle velocity from the wind. The rules work
// on typed particles and state and reach the heap, the random generator, the
// sine table and the room weather only through WeatherPoolHost.
// WeatherPoolLive.hpp binds them to the copied globals.
namespace VS::Effects {
// D_800F1DD4_t (146C.c:184): one weather particle,
// VS::Field::WeatherParticleRecord.
struct WeatherParticle {
  std::int16_t x = 0;
  std::int16_t y = 0;
  std::int16_t z = 0;
  std::int16_t pad = 0;
  std::int16_t screen_x = 0;
  std::int16_t screen_y = 0;
  std::uint8_t opaque_c = 0;    // unkC
  std::uint8_t sway_phase = 0;  // unkD
  std::uint8_t fall_shift = 0;  // unkE
  std::uint8_t speed = 0;       // unkF

  friend bool operator==(const WeatherParticle&,
                         const WeatherParticle&) = default;
};

// The particle buffer's size and the 0x10-byte particles it holds.
inline constexpr std::int32_t kWeatherParticleBufferBytes = 0x800;
inline constexpr std::size_t kWeatherParticleBytes = 0x10;
inline constexpr std::size_t kWeatherParticleCapacity = 0x800 / 0x10;

// The pool globals the spawns set: D_800F1DC0, D_800F1DC2, D_800F1DCA and the
// screen weather's fade, D_800F1DC7.
struct WeatherPoolState {
  std::int16_t particle_count = 0;
  std::int16_t particle_capacity = 0;
  std::uint8_t kind = Field::kNoWeather;
  std::uint8_t screen_fade = 0;

  friend bool operator==(const WeatherPoolState&,
                         const WeatherPoolState&) = default;
};

// D_800F1D98[0..2]: the particle velocity.
struct WeatherVelocity {
  std::int16_t x = 0;
  std::int16_t y = 0;
  std::int16_t z = 0;

  friend bool operator==(const WeatherVelocity&,
                         const WeatherVelocity&) = default;
};

// D_800F1D98[4..6]: the wind's elevation and azimuth (the room weather's
// strength and direction words) and the intensity scale.
struct WeatherWind {
  std::int16_t elevation = 0;
  std::int16_t azimuth = 0;
  std::int16_t scale = 0;
};

// func_8008EB04's outputs: the weather kind and intensity.
struct RoomWeatherReport {
  std::int32_t kind = 0;
  std::int32_t intensity = 0;

  friend bool operator==(const RoomWeatherReport&,
                         const RoomWeatherReport&) = default;
};

class WeatherPoolHost {
 public:
  WeatherPoolHost() = default;
  WeatherPoolHost(const WeatherPoolHost&) = delete;
  WeatherPoolHost& operator=(const WeatherPoolHost&) = delete;
  WeatherPoolHost(WeatherPoolHost&&) = delete;
  WeatherPoolHost& operator=(WeatherPoolHost&&) = delete;
  virtual ~WeatherPoolHost() = default;

  // rand().
  [[nodiscard]] virtual std::int32_t Random() = 0;
  // rsin and rcos.
  [[nodiscard]] virtual std::int32_t Sine(std::int32_t angle) = 0;
  [[nodiscard]] virtual std::int32_t Cosine(std::int32_t angle) = 0;
  // vs_main_allocHeap and vs_main_freeHeap over the particle buffer; the
  // allocation is kNone when the heap has no room.
  [[nodiscard]] virtual Field::WeatherParticlesId AllocateParticles(
      std::int32_t bytes) = 0;
  virtual void FreeParticles(Field::WeatherParticlesId particles) = 0;
  // func_80090C2C.
  virtual void SetRoomWeather(std::int32_t kind) = 0;
};

// func_8008E8F8: frees the current buffer, if any, and returns a new
// 0x800-byte one (kNone when the heap has no room).
[[nodiscard]] Field::WeatherParticlesId AllocateWeatherParticles(
    WeatherPoolHost& host, Field::WeatherParticlesId current);

// func_8008EA90: starts the room record's weather, except that a suppressed
// start turns rain into none. False, with no call, outside a room;
// `room_weather_kind` is the record's signed halfword.
[[nodiscard]] bool InitializeRoomWeather(
    WeatherPoolHost& host, std::optional<std::int16_t> room_weather_kind,
    std::int32_t suppress_rain);

// func_8008EB04: the kind byte, unsigned, and the intensity halfword.
[[nodiscard]] RoomWeatherReport RoomWeatherStateOf(std::uint8_t kind,
                                                   std::int16_t intensity);

// func_80090A1C: respawns a falling particle at a random point of the room's
// weather area (a 4096-unit square from the origin outside a room), at a
// height of -2560 to -2049, with a random speed. The original's remainder by a
// zero extent leaves the random value, as the R3000's DIV does.
[[nodiscard]] WeatherParticle ResetFallingWeatherParticle(
    WeatherPoolHost& host, WeatherParticle particle,
    const std::optional<Field::WeatherArea>& area);

// func_8008F29C: sets the kind and both counts (the halfwords and byte the
// original stores) and respawns `count` falling particles in order. False
// (refused) when the particles do not fit `particles`; the state is set and
// only the particles that fit are respawned.
[[nodiscard]] bool InitializeFallingWeatherParticles(
    WeatherPoolHost& host, WeatherPoolState& state,
    std::span<WeatherParticle> particles,
    const std::optional<Field::WeatherArea>& area, std::int32_t count,
    std::int32_t kind);

// func_8008F30C: as the falling spawn, with the screen fade cleared and each
// particle placed at a random screen point and depth, unprojected.
[[nodiscard]] bool InitializeScreenWeatherParticles(
    WeatherPoolHost& host, WeatherPoolState& state,
    std::span<WeatherParticle> particles, std::int32_t count,
    std::int32_t kind);

// func_80090B28: the particle velocity, `scale` along the wind's azimuth and
// elevation, each Q12 product rounded toward zero.
[[nodiscard]] WeatherVelocity WeatherVelocityOf(WeatherPoolHost& host,
                                                const WeatherWind& wind);
}  // namespace VS::Effects
