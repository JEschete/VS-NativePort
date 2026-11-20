// Hand-maintained (tools/port/owned.csv).
#include "Effects/WeatherPoolLive.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "Core/Log.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Core/RandomLive.hpp"
#include "Effects/WeatherPool.hpp"
#include "Effects/WeatherPoolLiveSupport.hpp"
#include "Field/Background.hpp"
#include "Field/Environment.hpp"
#include "Field/EnvironmentLive.hpp"
#include "Field/EnvironmentLiveState.hpp"
#include "Field/Gimmicks.hpp"
#include "Field/Mpd.hpp"
#include "Field/Room.hpp"
#include "Field/Weather.hpp"

namespace VS::Effects {
namespace {
// D_800F1D98's velocity and wind words.
constexpr std::size_t kVelocityXWord = 0;
constexpr std::size_t kVelocityYWord = 1;
constexpr std::size_t kVelocityZWord = 2;
constexpr std::size_t kElevationWord = 4;
constexpr std::size_t kAzimuthWord = 5;
constexpr std::size_t kScaleWord = 6;

static_assert(sizeof(VS::Field::WeatherParticleRecord) ==
              kWeatherParticleBytes);

std::uint64_t refusedCalls = 0;

void Refuse(std::string_view call, std::string_view reason) {
  if (std::has_single_bit(++refusedCalls)) {
    VS::Core::LogWarn("effects", "weather pool: {} refused: {} ({} refusals)",
                      call, reason, refusedCalls);
  }
}

[[nodiscard]] Field::WeatherParticlesId ParticlesIdOf(
    VS::Field::WeatherParticleRecord* particles) {
  const auto address = reinterpret_cast<std::uintptr_t>(particles);
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  return static_cast<Field::WeatherParticlesId>(address);
}

[[nodiscard]] VS::Field::WeatherParticleRecord* ParticlesOf(
    Field::WeatherParticlesId particles) {
  const auto address = static_cast<std::uintptr_t>(particles);
  return reinterpret_cast<VS::Field::WeatherParticleRecord*>(address);
}

// Runs a spawn over typed copies of the live buffer's particles and stores
// them and the pool globals back.
template <typename Spawn>
void RunSpawn(std::string_view call, Spawn spawn) {
  const std::span<VS::Field::WeatherParticleRecord> buffer =
      weather_live::Buffer();
  std::vector<WeatherParticle> particles = weather_live::LoadParticles(buffer);
  WeatherPoolState state = weather_live::LoadState();
  weather_live::PoolHost host;
  const bool fitted = spawn(host, state, std::span(particles));
  weather_live::StoreState(state);
  weather_live::StoreParticles(buffer, particles);
  if (!fitted) {
    Refuse(call, buffer.empty() ? "no particle buffer"
                                : "more particles than the buffer holds");
  }
}
}  // namespace

namespace weather_live {
std::int32_t PoolHost::Random() { return VS::Core::NextBiosRand(); }
std::int32_t PoolHost::Sine(std::int32_t angle) {
  return Core::Math::Sine(angle);
}
std::int32_t PoolHost::Cosine(std::int32_t angle) {
  return Core::Math::Cosine(angle);
}
Field::WeatherParticlesId PoolHost::AllocateParticles(std::int32_t bytes) {
  return ParticlesIdOf(static_cast<VS::Field::WeatherParticleRecord*>(
      Core::Memory::AllocateLiveFromLowEnd(static_cast<std::size_t>(bytes))));
}
void PoolHost::FreeParticles(Field::WeatherParticlesId particles) {
  Core::Memory::FreeLiveFromLowEnd(ParticlesOf(particles));
}
void PoolHost::SetRoomWeather(std::int32_t kind) {
  Field::LiveSetRoomWeather(kind);
}

WeatherParticle ParticleOf(const VS::Field::WeatherParticleRecord& record) {
  return {.x = record.position.vx,
          .y = record.position.vy,
          .z = record.position.vz,
          .pad = record.position.pad,
          .screen_x = record.screen_x,
          .screen_y = record.screen_y,
          .opaque_c = record.opaque_c,
          .sway_phase = record.sway_phase,
          .fall_shift = record.fall_shift,
          .speed = record.speed};
}

void StoreParticle(VS::Field::WeatherParticleRecord& record,
                   const WeatherParticle& particle) {
  record.position.vx = particle.x;
  record.position.vy = particle.y;
  record.position.vz = particle.z;
  record.position.pad = particle.pad;
  record.screen_x = particle.screen_x;
  record.screen_y = particle.screen_y;
  record.opaque_c = particle.opaque_c;
  record.sway_phase = particle.sway_phase;
  record.fall_shift = particle.fall_shift;
  record.speed = particle.speed;
}

std::vector<WeatherParticle> LoadParticles(
    std::span<const VS::Field::WeatherParticleRecord> records) {
  std::vector<WeatherParticle> particles;
  particles.reserve(records.size());
  for (const VS::Field::WeatherParticleRecord& record : records) {
    particles.push_back(ParticleOf(record));
  }
  return particles;
}

void StoreParticles(std::span<VS::Field::WeatherParticleRecord> records,
                    std::span<const WeatherParticle> particles) {
  const std::size_t count =
      records.size() < particles.size() ? records.size() : particles.size();
  for (std::size_t index = 0; index < count; ++index) {
    StoreParticle(records[index], particles[index]);
  }
}

std::span<VS::Field::WeatherParticleRecord> Buffer() {
  if (VS::Field::weatherParticles == nullptr) return {};
  return {VS::Field::weatherParticles, kWeatherParticleCapacity};
}

bool InBuffer(const VS::Field::WeatherParticleRecord* particle) {
  const std::span<VS::Field::WeatherParticleRecord> buffer = Buffer();
  if (buffer.empty() || particle == nullptr) return false;
  const auto first = reinterpret_cast<std::uintptr_t>(buffer.data());
  const auto address = reinterpret_cast<std::uintptr_t>(particle);
  return address >= first && address - first < buffer.size_bytes() &&
         (address - first) % kWeatherParticleBytes == 0;
}

std::optional<Field::WeatherArea> Area() {
  const VS::Field::MpdEnvironment* const record =
      VS::Field::roomData.environment;
  if (record == nullptr) return std::nullopt;
  return Field::WeatherArea{.origin_x = record->weather_area[0].halves[0],
                            .origin_z = record->weather_area[0].halves[1],
                            .extent_x = record->weather_area[1].halves[0],
                            .extent_z = record->weather_area[1].halves[1]};
}

WeatherPoolState LoadState() {
  return {.particle_count = VS::Field::weatherParticleCount,
          .particle_capacity = VS::Field::weatherParticleCapacity,
          .kind = static_cast<std::uint8_t>(VS::Field::roomWeatherKind),
          .screen_fade = VS::Field::screenWeatherFade};
}

void StoreState(const WeatherPoolState& state) {
  VS::Field::weatherParticleCount = state.particle_count;
  VS::Field::weatherParticleCapacity = state.particle_capacity;
  VS::Field::roomWeatherKind = static_cast<char>(state.kind);
  VS::Field::screenWeatherFade = state.screen_fade;
}
}  // namespace weather_live

// decomp: BATTLE.PRG func_8008E8F8 (146C.c:17445).
void LiveAllocateWeatherParticles() {
  weather_live::PoolHost host;
  VS::Field::weatherParticles = ParticlesOf(AllocateWeatherParticles(
      host, ParticlesIdOf(VS::Field::weatherParticles)));
}

// decomp: BATTLE.PRG func_8008EA90 (146C.c:17500).
int LiveInitializeRoomWeather(int suppress_rain) {
  const VS::Field::MpdEnvironment* const record =
      VS::Field::roomData.environment;
  const std::optional<std::int16_t> kind =
      record != nullptr ? std::optional<std::int16_t>(record->weather_kind)
                        : std::nullopt;
  weather_live::PoolHost host;
  return InitializeRoomWeather(host, kind, suppress_rain) ? 1 : 0;
}

// decomp: BATTLE.PRG func_8008EB04 (146C.c:17525).
void LiveRoomWeatherState(int* weather_kind_output, int* intensity_output) {
  if (weather_kind_output == nullptr || intensity_output == nullptr) {
    Refuse("RoomWeatherState", "missing output");
    return;
  }
  const RoomWeatherReport report =
      RoomWeatherStateOf(static_cast<std::uint8_t>(VS::Field::roomWeatherKind),
                         VS::Field::weatherIntensity);
  *weather_kind_output = report.kind;
  *intensity_output = report.intensity;
}

// decomp: BATTLE.PRG func_8008F29C (146C.c:17723).
void LiveInitializeFallingWeatherParticles(int particle_count,
                                           int weather_kind) {
  const std::optional<Field::WeatherArea> area = weather_live::Area();
  RunSpawn("InitializeFallingWeatherParticles",
           [&](WeatherPoolHost& host, WeatherPoolState& state,
               std::span<WeatherParticle> particles) {
             return InitializeFallingWeatherParticles(
                 host, state, particles, area, particle_count, weather_kind);
           });
}

// decomp: BATTLE.PRG func_8008F30C (146C.c:17737).
void LiveInitializeScreenWeatherParticles(int particle_count,
                                          int weather_kind) {
  RunSpawn("InitializeScreenWeatherParticles",
           [&](WeatherPoolHost& host, WeatherPoolState& state,
               std::span<WeatherParticle> particles) {
             return InitializeScreenWeatherParticles(
                 host, state, particles, particle_count, weather_kind);
           });
}

// decomp: BATTLE.PRG func_80090A1C (146C.c:18200).
void LiveResetFallingWeatherParticle(
    VS::Field::WeatherParticleRecord* weather_particle) {
  if (!weather_live::InBuffer(weather_particle)) {
    Refuse("ResetFallingWeatherParticle",
           "particle outside the particle buffer");
    return;
  }
  weather_live::PoolHost host;
  weather_live::StoreParticle(
      *weather_particle, ResetFallingWeatherParticle(
                             host, weather_live::ParticleOf(*weather_particle),
                             weather_live::Area()));
}

// decomp: BATTLE.PRG func_80090B28 (146C.c:18221).
void LiveRecalculateWeatherVelocity() {
  weather_live::PoolHost host;
  const WeatherVelocity velocity = WeatherVelocityOf(
      host, {.elevation = VS::Field::weatherParameters[kElevationWord],
             .azimuth = VS::Field::weatherParameters[kAzimuthWord],
             .scale = VS::Field::weatherParameters[kScaleWord]});
  VS::Field::weatherParameters[kVelocityXWord] = velocity.x;
  VS::Field::weatherParameters[kVelocityZWord] = velocity.z;
  VS::Field::weatherParameters[kVelocityYWord] = velocity.y;
}
}  // namespace VS::Effects
