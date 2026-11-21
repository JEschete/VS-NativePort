// Hand-maintained (tools/port/owned.csv).
#include "Effects/WeatherPool.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Core/Math/ScalarMath.hpp"

namespace VS::Effects {
namespace {
namespace Math = Core::Math;

// The square a falling particle respawns in outside a room, the height band
// above it, and the screen weather's screen and depth ranges.
constexpr std::int32_t kOpenAreaSpan = 4096;
constexpr std::int32_t kFallHeightBand = 512;
constexpr std::int32_t kFallHeight = 2560;
constexpr std::int32_t kScreenWidth = 320;
constexpr std::int32_t kScreenHeight = 240;
constexpr std::int32_t kScreenDepth = 256;
constexpr std::uint8_t kSpeedMask = 0x7F;
constexpr std::uint8_t kRespawnFallShift = 2;
constexpr std::int16_t kUnprojected = -1;
constexpr std::int32_t kQ12Shift = 12;
constexpr std::int32_t kQ12Rounding = 0xFFF;

[[nodiscard]] constexpr std::int16_t Half(std::int32_t value) noexcept {
  return Math::WrapSigned16(static_cast<std::uint32_t>(value));
}

[[nodiscard]] std::uint8_t RandomSpeed(WeatherPoolHost& host) {
  return static_cast<std::uint8_t>(static_cast<std::uint32_t>(host.Random()) &
                                   kSpeedMask);
}

// A Q12 product shifted down, rounded toward zero.
[[nodiscard]] constexpr std::int32_t Q12(std::int32_t product) noexcept {
  return (product < 0 ? product + kQ12Rounding : product) >> kQ12Shift;
}

// The spawn globals func_8008F29C and func_8008F30C set before their loops.
void SetSpawnState(WeatherPoolState& state, std::int32_t count,
                   std::int32_t kind) {
  state.kind = static_cast<std::uint8_t>(static_cast<std::uint32_t>(kind));
  state.particle_capacity = Half(count);
  state.particle_count = Half(count);
}

// The particles a spawn of `count` fills, at most the storage.
[[nodiscard]] std::size_t SpawnedCount(std::span<WeatherParticle> particles,
                                       std::int32_t count) {
  if (count <= 0) return 0;
  const auto wanted = static_cast<std::size_t>(count);
  return wanted < particles.size() ? wanted : particles.size();
}

[[nodiscard]] bool Fits(std::span<WeatherParticle> particles,
                        std::int32_t count) {
  return count <= 0 || static_cast<std::size_t>(count) <= particles.size();
}
}  // namespace

// decomp: BATTLE.PRG func_8008E8F8 (src/BATTLE/BATTLE.PRG/146C.c:17324).
Field::WeatherParticlesId AllocateWeatherParticles(
    WeatherPoolHost& host, Field::WeatherParticlesId current) {
  if (current != Field::WeatherParticlesId::kNone) {
    host.FreeParticles(current);
  }
  return host.AllocateParticles(kWeatherParticleBufferBytes);
}

// decomp: BATTLE.PRG func_8008EA90 (src/BATTLE/BATTLE.PRG/146C.c:17377).
bool InitializeRoomWeather(WeatherPoolHost& host,
                           std::optional<std::int16_t> room_weather_kind,
                           std::int32_t suppress_rain) {
  if (!room_weather_kind) return false;
  const std::int16_t kind = *room_weather_kind;
  const bool rain_suppressed =
      suppress_rain != 0 && kind == Field::kRainWeather;
  host.SetRoomWeather(rain_suppressed ? Field::kNoWeather : kind);
  return true;
}

// decomp: BATTLE.PRG func_8008EB04 (src/BATTLE/BATTLE.PRG/146C.c:17402).
RoomWeatherReport RoomWeatherStateOf(std::uint8_t kind,
                                     std::int16_t intensity) {
  return {.kind = kind, .intensity = intensity};
}

// decomp: BATTLE.PRG func_80090A1C (src/BATTLE/BATTLE.PRG/146C.c:18075).
WeatherParticle ResetFallingWeatherParticle(
    WeatherPoolHost& host, WeatherParticle particle,
    const std::optional<Field::WeatherArea>& area) {
  if (area) {
    const std::int32_t x = Math::R3000Remainder(host.Random(), area->extent_x);
    particle.x = Half(Math::WrappingAdd(area->origin_x, x));
    const std::int32_t z = Math::R3000Remainder(host.Random(), area->extent_z);
    particle.z = Half(Math::WrappingAdd(area->origin_z, z));
  } else {
    particle.x = Half(host.Random() % kOpenAreaSpan);
    particle.z = Half(host.Random() % kOpenAreaSpan);
  }
  particle.y = Half((host.Random() % kFallHeightBand) - kFallHeight);
  particle.fall_shift = kRespawnFallShift;
  particle.has_landed = 0;
  particle.sway_phase = 0;
  particle.speed = RandomSpeed(host);
  return particle;
}

// decomp: BATTLE.PRG func_8008F29C (src/BATTLE/BATTLE.PRG/146C.c:17600).
bool InitializeFallingWeatherParticles(
    WeatherPoolHost& host, WeatherPoolState& state,
    std::span<WeatherParticle> particles,
    const std::optional<Field::WeatherArea>& area, std::int32_t count,
    std::int32_t kind) {
  SetSpawnState(state, count, kind);
  for (WeatherParticle& particle :
       particles.first(SpawnedCount(particles, count))) {
    particle = ResetFallingWeatherParticle(host, particle, area);
  }
  return Fits(particles, count);
}

// decomp: BATTLE.PRG func_8008F30C (src/BATTLE/BATTLE.PRG/146C.c:17614).
bool InitializeScreenWeatherParticles(WeatherPoolHost& host,
                                      WeatherPoolState& state,
                                      std::span<WeatherParticle> particles,
                                      std::int32_t count, std::int32_t kind) {
  state.screen_fade = 0;
  SetSpawnState(state, count, kind);
  for (WeatherParticle& particle :
       particles.first(SpawnedCount(particles, count))) {
    particle.speed = RandomSpeed(host);
    particle.x = Half(host.Random() % kScreenWidth);
    particle.y = Half(host.Random() % kScreenHeight);
    particle.z = Half(host.Random() % kScreenDepth);
    particle.screen_x = kUnprojected;
    particle.screen_y = kUnprojected;
  }
  return Fits(particles, count);
}

// decomp: BATTLE.PRG func_80090B28 (src/BATTLE/BATTLE.PRG/146C.c:18096).
WeatherVelocity WeatherVelocityOf(WeatherPoolHost& host,
                                  const WeatherWind& wind) {
  const std::int32_t azimuth_sine = host.Sine(wind.azimuth);
  const std::int32_t x = Q12(Math::WrappingMultiply(
      Q12(Math::WrappingMultiply(azimuth_sine, wind.scale)),
      host.Cosine(wind.elevation)));
  const std::int32_t azimuth_cosine =
      Math::WrappingNegate(host.Cosine(wind.azimuth));
  const std::int32_t z = Q12(Math::WrappingMultiply(
      Q12(Math::WrappingMultiply(azimuth_cosine, wind.scale)),
      host.Cosine(wind.elevation)));
  const std::int32_t y =
      Q12(Math::WrappingMultiply(host.Sine(wind.elevation), wind.scale));
  return {.x = Half(x), .y = Half(y), .z = Half(z)};
}
}  // namespace VS::Effects
