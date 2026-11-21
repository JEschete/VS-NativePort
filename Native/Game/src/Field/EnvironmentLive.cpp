// Hand-maintained (tools/port/owned.csv).
#include "Field/EnvironmentLive.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <type_traits>

#include "Audio/SfxLive.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Core/RandomLive.hpp"
#include "Effects/WeatherPoolLive.hpp"
#include "Effects/WeatherRenderLive.hpp"
#include "Field/Background.hpp"
#include "Field/BackgroundLive.hpp"
#include "Field/EnvironmentLiveState.hpp"
#include "Field/Gimmicks.hpp"
#include "Field/RoomRenderingLive.hpp"

// decomp: D_800F1D98 (D_800F1D98@BATTLE/BATTLE.PRG)
int16_t VS::Field::weatherParameters[VS::Field::kWeatherParameterWords];
// decomp: D_800F1DA4 (D_800F1DA4@BATTLE/BATTLE.PRG)
int16_t VS::Field::weatherIntensityScale;
// decomp: D_800F1DBC (D_800F1DBC@BATTLE/BATTLE.PRG)
int16_t VS::Field::weatherAmbientSoundId;
// decomp: D_800F1DC0 (D_800F1DC0@BATTLE/BATTLE.PRG)
int16_t VS::Field::weatherParticleCount;
// decomp: D_800F1DC2 (D_800F1DC2@BATTLE/BATTLE.PRG)
int16_t VS::Field::weatherParticleCapacity;
// decomp: D_800F1DC4 (D_800F1DC4@BATTLE/BATTLE.PRG)
int16_t VS::Field::weatherIntensity;
// decomp: D_800F1DCA (D_800F1DCA@BATTLE/BATTLE.PRG)
char VS::Field::roomWeatherKind;
// decomp: D_800F1DCB (D_800F1DCB@BATTLE/BATTLE.PRG)
char VS::Field::isWeatherVariationEnabled;
// decomp: D_800F1DD4 (D_800F1DD4@BATTLE/BATTLE.PRG)
VS::Field::WeatherParticleRecord* VS::Field::weatherParticles;

namespace {
// The original's particle record (146C.h) and D_800F1D98's word indices:
// the wind strength and direction, the intensity scale (D_800F1DA4) and the
// direction, intensity and strength drift steps.
constexpr std::size_t kWeatherParticleBytes = 0x10;
constexpr std::size_t kWindStrengthWord = 4;
constexpr std::size_t kWindDirectionWord = 5;
constexpr std::size_t kIntensityScaleWord = 6;
constexpr std::size_t kDirectionStepWord = 7;
constexpr std::size_t kIntensityStepWord = 8;
constexpr std::size_t kStrengthStepWord = 9;
}  // namespace

static_assert(sizeof(VS::Field::WeatherParticleRecord) ==
              kWeatherParticleBytes);

namespace VS::Field {
namespace {
namespace Memory = Core::Memory;

// The field sound group the ambient sound plays in.
constexpr int kFieldSoundGroup = 0x200;

// The particle id names only kNone; the live buffer's address is made here.
[[nodiscard]] WeatherParticlesId ParticlesIdOf(
    WeatherParticleRecord* particles) {
  const auto address = reinterpret_cast<std::uintptr_t>(particles);
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  return static_cast<WeatherParticlesId>(address);
}

[[nodiscard]] WeatherParticleRecord* ParticlesOf(WeatherParticlesId particles) {
  const auto address = static_cast<std::uintptr_t>(particles);
  return reinterpret_cast<WeatherParticleRecord*>(address);
}

// Runs a rule over the loaded globals and stores them back before returning
// its result.
template <typename Rule>
auto RunLive(Rule rule) {
  RoomWeatherState state = LoadLiveRoomWeatherState();
  LiveWeatherHost host(state);
  if constexpr (std::is_void_v<decltype(rule(host, state))>) {
    rule(host, state);
    StoreLiveRoomWeatherState(state);
  } else {
    auto result = rule(host, state);
    StoreLiveRoomWeatherState(state);
    return result;
  }
}
}  // namespace

RoomWeatherState LoadLiveRoomWeatherState() {
  return {
      .wind_strength = weatherParameters[kWindStrengthWord],
      .wind_direction = weatherParameters[kWindDirectionWord],
      .direction_step = weatherParameters[kDirectionStepWord],
      .intensity_step = weatherParameters[kIntensityStepWord],
      .strength_step = weatherParameters[kStrengthStepWord],
      .intensity_scale = weatherIntensityScale,
      .ambient_sound_id = weatherAmbientSoundId,
      .particle_count = weatherParticleCount,
      .particle_capacity = weatherParticleCapacity,
      .intensity = weatherIntensity,
      .kind = static_cast<std::uint8_t>(roomWeatherKind),
      .variation_enabled = static_cast<std::uint8_t>(isWeatherVariationEnabled),
      .particles = ParticlesIdOf(weatherParticles)};
}

void StoreLiveRoomWeatherState(const RoomWeatherState& state) {
  weatherParameters[kWindStrengthWord] = state.wind_strength;
  weatherParameters[kWindDirectionWord] = state.wind_direction;
  weatherParameters[kDirectionStepWord] = state.direction_step;
  weatherParameters[kIntensityStepWord] = state.intensity_step;
  weatherParameters[kStrengthStepWord] = state.strength_step;
  weatherIntensityScale = state.intensity_scale;
  weatherParameters[kIntensityScaleWord] = state.intensity_scale;
  weatherAmbientSoundId = state.ambient_sound_id;
  weatherParticleCount = state.particle_count;
  weatherParticleCapacity = state.particle_capacity;
  weatherIntensity = state.intensity;
  roomWeatherKind = static_cast<char>(state.kind);
  isWeatherVariationEnabled = static_cast<char>(state.variation_enabled);
  weatherParticles = ParticlesOf(state.particles);
}

template <typename Call>
void LiveWeatherHost::Escape(Call call) {
  StoreLiveRoomWeatherState(state_);
  call();
  state_ = LoadLiveRoomWeatherState();
}

std::optional<RoomEnvironment> LiveWeatherHost::Environment() {
  return LiveRoomEnvironment();
}

bool LiveWeatherHost::AreRoomUpdatesPaused() {
  return LiveRoomRenderState().updates_paused;
}

std::uint16_t LiveWeatherHost::RoomAnimationTicks() {
  return VS::Field::roomAnimationTicks;
}

std::int32_t LiveWeatherHost::Random() { return VS::Core::NextBiosRand(); }

void LiveWeatherHost::RecalculateWeatherVelocity() {
  Escape([] { Effects::LiveRecalculateWeatherVelocity(); });
}

void LiveWeatherHost::InitializeFallingParticles(std::int32_t count,
                                                 std::int32_t kind) {
  Escape([count, kind] {
    Effects::LiveInitializeFallingWeatherParticles(count, kind);
  });
}

void LiveWeatherHost::InitializeScreenParticles(std::int32_t count,
                                                std::int32_t kind) {
  Escape([count, kind] {
    Effects::LiveInitializeScreenWeatherParticles(count, kind);
  });
}

void LiveWeatherHost::InitializeRisingEmberParticles(std::int32_t count,
                                                     std::int32_t kind) {
  Escape([count, kind] {
    Effects::LiveInitializeRisingEmberParticles(count, kind);
  });
}

void LiveWeatherHost::DrawRainParticles() {
  Escape([] { Effects::LiveDrawRainParticles(); });
}

void LiveWeatherHost::DrawSnowParticles() {
  Escape([] { Effects::LiveDrawSnowParticles(); });
}

void LiveWeatherHost::DrawScreenWeatherParticles() {
  Escape([] { Effects::LiveDrawScreenWeatherParticles(); });
}

void LiveWeatherHost::DrawRisingEmberParticles() {
  Escape([] { Effects::LiveDrawRisingEmberParticles(); });
}

void LiveWeatherHost::StopSound(std::int32_t sound) {
  Audio::LiveStopSfx(kFieldSoundGroup, sound);
}

void LiveWeatherHost::PlaySoundCentered(std::int32_t sound) {
  Audio::LivePlaySfxCentered(kFieldSoundGroup, sound);
}

void LiveWeatherHost::FadeSoundVolume(std::int32_t sound, std::int32_t volume,
                                      std::int32_t fade_ticks) {
  Audio::LiveFadeSfxVolume(kFieldSoundGroup, sound, volume, fade_ticks);
}

void LiveWeatherHost::FreeWeatherParticles(WeatherParticlesId particles) {
  Memory::FreeLiveFromLowEnd(ParticlesOf(particles));
}

// decomp: BATTLE.PRG func_80090C2C (src/BATTLE/BATTLE.PRG/2842C.c:266)
void LiveSetRoomWeather(int kind) {
  RunLive([kind](WeatherHost& host, RoomWeatherState& state) {
    SetRoomWeather(host, state, kind);
  });
}

// decomp: BATTLE.PRG func_80090EEC (src/BATTLE/BATTLE.PRG/2842C.c:329)
void LiveUpdateRoomWeather() {
  RunLive([](WeatherHost& host, RoomWeatherState& state) {
    UpdateRoomWeather(host, state);
  });
}

// decomp: BATTLE.PRG func_80091314 (src/BATTLE/BATTLE.PRG/2842C.c:414)
void LiveSetWeatherVariationEnabled(int enabled) {
  RunLive([enabled](WeatherHost& /*host*/, RoomWeatherState& state) {
    SetWeatherVariationEnabled(state, enabled);
  });
}

// decomp: BATTLE.PRG func_80091320 (src/BATTLE/BATTLE.PRG/2842C.c:416)
void LiveSetWeatherIntensity(int intensity) {
  RunLive([intensity](WeatherHost& host, RoomWeatherState& state) {
    SetWeatherIntensity(host, state, intensity);
  });
}

// decomp: BATTLE.PRG func_8009134C (src/BATTLE/BATTLE.PRG/2842C.c:423)
void LiveSetWeatherWindAngles(int azimuth, int elevation) {
  RunLive([azimuth, elevation](WeatherHost& host, RoomWeatherState& state) {
    SetWeatherWindAngles(host, state, azimuth, elevation);
  });
}

// decomp: BATTLE.PRG func_80091378 (src/BATTLE/BATTLE.PRG/2842C.c:430)
void LiveReleaseWeatherParticles() {
  RunLive([](WeatherHost& host, RoomWeatherState& state) {
    ReleaseWeatherParticles(host, state);
  });
}

// decomp: BATTLE.PRG func_800913BC (src/BATTLE/BATTLE.PRG/2842C.c:454)
int LiveSetWeatherAmbientSound(int sound) {
  return RunLive([sound](WeatherHost& host, RoomWeatherState& state) {
    return SetWeatherAmbientSound(host, state, sound);
  });
}

// decomp: BATTLE.PRG func_80091468 (src/BATTLE/BATTLE.PRG/2842C.c:458)
void LiveSetRoomBackgroundOffset(int offset_x, int offset_y) {
  RoomBackgroundState state = LoadLiveRoomBackgroundState();
  SetRoomBackgroundOffset(state, offset_x, offset_y);
  StoreLiveRoomBackgroundState(state);
}

// decomp: BATTLE.PRG func_800918E8 (src/BATTLE/BATTLE.PRG/2842C.c:525)
void LiveDuckWeatherAmbientSound(int ducked) {
  RunLive([ducked](WeatherHost& host, const RoomWeatherState& state) {
    DuckWeatherAmbientSound(host, state, ducked);
  });
}
}  // namespace VS::Field
