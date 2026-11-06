#include <algorithm>
#include <array>
#include <cstdint>
#include <deque>
#include <format>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Field/Background.hpp"
#include "Field/Environment.hpp"
#include "Field/EnvironmentLive.hpp"
#include "Legacy/Psx.h"
#include "TestSupport/ChannelWarnings.hpp"

extern "C" {
#include "Field/EnvironmentLiveState.h"
}

namespace {
namespace fx = VS::Field;
using Calls = std::vector<std::string>;
using VS::test_support::ChannelWarnings;

// The id names only kNone; the tests hand out opaque values for the rest.
constexpr fx::WeatherParticlesId ParticlesId(std::uintptr_t id) {
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  return static_cast<fx::WeatherParticlesId>(id);
}

// Catch2's REQUIRE throws, but clang-tidy cannot see that; the explicit throw
// lets bugprone-unchecked-optional-access prove the accesses through it.
template <typename T>
T& Required(std::optional<T>& value) {
  REQUIRE(value.has_value());
  if (!value) {
    throw std::logic_error("REQUIRE did not stop the test");
  }
  return *value;
}

class FakeWeatherHost final : public fx::WeatherHost {
 public:
  [[nodiscard]] std::optional<fx::RoomEnvironment> Environment() override {
    return environment;
  }
  [[nodiscard]] bool AreRoomUpdatesPaused() override { return paused; }
  [[nodiscard]] std::uint16_t RoomAnimationTicks() override { return ticks; }
  [[nodiscard]] std::int32_t Random() override {
    if (randoms.empty()) return 7;
    const std::int32_t value = randoms.front();
    randoms.pop_front();
    return value;
  }
  void RecalculateWeatherVelocity() override {
    calls.emplace_back("RecalculateWeatherVelocity");
  }
  void InitializeFallingParticles(std::int32_t count,
                                  std::int32_t kind) override {
    calls.push_back(
        std::format("InitializeFallingParticles {} {}", count, kind));
  }
  void InitializeScreenParticles(std::int32_t count,
                                 std::int32_t kind) override {
    calls.push_back(
        std::format("InitializeScreenParticles {} {}", count, kind));
  }
  void InitializeRisingEmberParticles(std::int32_t count,
                                      std::int32_t kind) override {
    calls.push_back(
        std::format("InitializeRisingEmberParticles {} {}", count, kind));
  }
  void DrawRainParticles() override { calls.emplace_back("DrawRainParticles"); }
  void DrawSnowParticles() override { calls.emplace_back("DrawSnowParticles"); }
  void DrawScreenWeatherParticles() override {
    calls.emplace_back("DrawScreenWeatherParticles");
  }
  void DrawRisingEmberParticles() override {
    calls.emplace_back("DrawRisingEmberParticles");
  }
  void StopSound(std::int32_t sound) override {
    calls.push_back(std::format("StopSound {}", sound));
  }
  void PlaySoundCentered(std::int32_t sound) override {
    calls.push_back(std::format("PlaySoundCentered {}", sound));
  }
  void FadeSoundVolume(std::int32_t sound, std::int32_t volume,
                       std::int32_t fade_ticks) override {
    calls.push_back(
        std::format("FadeSoundVolume {} {} {}", sound, volume, fade_ticks));
  }
  void FreeWeatherParticles(fx::WeatherParticlesId particles) override {
    calls.push_back(std::format("FreeWeatherParticles {}",
                                static_cast<std::uintptr_t>(particles)));
  }

  std::optional<fx::RoomEnvironment> environment = fx::RoomEnvironment{};
  bool paused = false;
  std::uint16_t ticks = 0;
  std::deque<std::int32_t> randoms;
  Calls calls;
};

// A rainy room: wind strength 0x100 at 0x200, intensity 5, over a 20x10-cell
// weather area.
void SetUpRainyRoom(FakeWeatherHost& host) {
  fx::RoomEnvironment& room = Required(host.environment);
  room.wind_strength = 0x100;
  room.wind_direction = 0x200;
  room.weather_intensity = 5;
  room.weather_area = {
      .origin_x = 0, .origin_z = 0, .extent_x = 2560, .extent_z = 1280};
}

fx::RoomWeatherState Quiet() {
  return {.ambient_sound_id = fx::kNoAmbientSound, .particle_capacity = 128};
}

// A room whose wind direction drifts every 16 frames inside 0x100..0x300,
// strength every 64 inside 0x10..0x40 and intensity every 32 inside 2..8.
void SetUpDriftingRoom(FakeWeatherHost& host) {
  fx::RoomEnvironment& room = Required(host.environment);
  room.wind_direction = 0x100;
  room.wind_direction_limit = 0x300;
  room.wind_direction_interval = 0;
  room.weather_intensity = 2;
  room.weather_intensity_limit = 8;
  room.weather_intensity_interval = 1;
  room.wind_strength = 0x10;
  room.wind_strength_limit = 0x40;
  room.wind_strength_interval = 2;
}

fx::RoomWeatherState Drifting() {
  fx::RoomWeatherState state = Quiet();
  state.kind = fx::kRainWeather;
  state.variation_enabled = 1;
  state.wind_direction = 0x200;
  state.direction_step = 0x20;
  state.wind_strength = 0x20;
  state.strength_step = 4;
  state.intensity = 5;
  state.intensity_step = 1;
  return state;
}
}  // namespace

TEST_CASE("Room rain starts at the room's wind",
          "[field-camera][field-environment]") {
  FakeWeatherHost host;
  SetUpRainyRoom(host);
  fx::RoomWeatherState state = Quiet();
  fx::SetRoomWeather(host, state, fx::kRainWeather);
  CHECK(state.kind == fx::kRainWeather);
  CHECK(state.direction_step == 1);
  CHECK(state.strength_step == 1);
  CHECK(state.intensity_step == 1);
  CHECK(state.wind_strength == 0x100);
  CHECK(state.wind_direction == 0x200);
  CHECK(state.intensity == 5);
}

TEST_CASE("Room rain starts its particles and sound",
          "[field-camera][field-environment]") {
  FakeWeatherHost host;
  SetUpRainyRoom(host);
  fx::RoomWeatherState state = Quiet();
  fx::SetRoomWeather(host, state, fx::kRainWeather);
  CHECK(state.particle_count == 40);
  CHECK(state.intensity_scale == 52);
  CHECK(state.ambient_sound_id == fx::kRainAmbientSound);
  CHECK(host.calls == Calls{"RecalculateWeatherVelocity",
                            "InitializeFallingParticles 50 1",
                            "PlaySoundCentered 21"});
}

TEST_CASE("Room weather kinds spawn their particles and switch the sound",
          "[field-camera][field-environment]") {
  FakeWeatherHost host;
  SetUpRainyRoom(host);
  fx::RoomWeatherState state = Quiet();
  state.ambient_sound_id = fx::kAmbientSoundHeld;
  fx::SetRoomWeather(host, state, fx::kEmberWeather);
  CHECK(host.calls == Calls{"RecalculateWeatherVelocity",
                            "InitializeRisingEmberParticles 100 4"});
  CHECK(state.ambient_sound_id == fx::kAmbientSoundHeld);

  host.calls.clear();
  state = Quiet();
  fx::SetRoomWeather(host, state, fx::kScreenWeather);
  CHECK(host.calls ==
        Calls{"RecalculateWeatherVelocity", "InitializeScreenParticles 50 3"});
  CHECK(state.ambient_sound_id == fx::kNoAmbientSound);

  host.calls.clear();
  state.ambient_sound_id = fx::kRainAmbientSound;
  fx::SetRoomWeather(host, state, fx::kSnowWeather);
  CHECK(host.calls == Calls{"StopSound 21", "RecalculateWeatherVelocity",
                            "InitializeFallingParticles 50 2", "StopSound 21"});
  CHECK(state.ambient_sound_id == fx::kNoAmbientSound);

  host.calls.clear();
  fx::RoomEnvironment& room = Required(host.environment);
  room.weather_area = {
      .origin_x = 0, .origin_z = 0, .extent_x = 128, .extent_z = 128};
  fx::SetRoomWeather(host, state, fx::kSnowWeather);
  room.weather_area = {
      .origin_x = 0, .origin_z = 0, .extent_x = 8192, .extent_z = 8192};
  fx::SetRoomWeather(host, state, fx::kSnowWeather);
  CHECK(host.calls == Calls{"RecalculateWeatherVelocity",
                            "InitializeFallingParticles 4 2",
                            "RecalculateWeatherVelocity",
                            "InitializeFallingParticles 128 2"});
}

TEST_CASE("Room weather outside a room only stops the ambient sound",
          "[field-camera][field-environment]") {
  FakeWeatherHost host;
  host.environment.reset();
  fx::RoomWeatherState state = Quiet();
  state.ambient_sound_id = fx::kRainAmbientSound;
  fx::SetRoomWeather(host, state, fx::kRainWeather);
  CHECK(host.calls == Calls{"StopSound 21"});
  CHECK(state.ambient_sound_id == fx::kNoAmbientSound);
  CHECK(state.kind == fx::kNoWeather);
  host.calls.clear();
  state.ambient_sound_id = fx::kAmbientSoundHeld;
  fx::SetRoomWeather(host, state, fx::kRainWeather);
  CHECK(host.calls.empty());
  CHECK(state.ambient_sound_id == fx::kAmbientSoundHeld);
}

TEST_CASE("Weather intensity sets the particles, volume and scale",
          "[field-camera][field-environment]") {
  FakeWeatherHost host;
  fx::RoomWeatherState state{.ambient_sound_id = fx::kRainAmbientSound,
                             .particle_capacity = 100,
                             .intensity = 10};
  fx::ApplyWeatherIntensity(host, state);
  CHECK(state.particle_count == 80);
  CHECK(state.intensity_scale == 105);
  CHECK(host.calls ==
        Calls{"PlaySoundCentered 21", "FadeSoundVolume 21 110 8"});

  host.calls.clear();
  state.intensity = 0;
  fx::ApplyWeatherIntensity(host, state);
  CHECK(state.particle_count == 4);
  CHECK(state.intensity_scale == 21);
  CHECK(host.calls == Calls{"StopSound 21"});
}

TEST_CASE("Weather intensity particles stop at the capacity",
          "[field-camera][field-environment]") {
  FakeWeatherHost host;
  fx::RoomWeatherState state{.ambient_sound_id = fx::kRainAmbientSound,
                             .particle_capacity = 64,
                             .intensity = 15};
  fx::ApplyWeatherIntensity(host, state);
  CHECK(state.particle_count == 64);
  CHECK(state.intensity_scale == 157);
  CHECK(host.calls ==
        Calls{"PlaySoundCentered 21", "FadeSoundVolume 21 127 8"});
}

TEST_CASE("Weather intensity setter keeps the low halfword",
          "[field-camera][field-environment]") {
  FakeWeatherHost host;
  fx::RoomWeatherState state{.ambient_sound_id = fx::kNoAmbientSound,
                             .particle_capacity = 64,
                             .intensity = 15};
  fx::SetWeatherIntensity(host, state, 0x10006);
  CHECK(state.intensity == 6);
  CHECK(state.particle_count == 48);
  CHECK(host.calls == Calls{"RecalculateWeatherVelocity"});
}

TEST_CASE("Weather intensity outside the volume table keeps the sound",
          "[field-camera][field-environment]") {
  ChannelWarnings warnings{"field.environment"};
  FakeWeatherHost host;
  fx::RoomWeatherState state{.ambient_sound_id = fx::kRainAmbientSound,
                             .particle_capacity = 128,
                             .intensity = 16};
  fx::ApplyWeatherIntensity(host, state);
  CHECK(state.particle_count == 128);
  CHECK(state.intensity_scale == 157);
  state.intensity = -1;
  fx::ApplyWeatherIntensity(host, state);
  CHECK(state.particle_count == 4);
  CHECK(state.intensity_scale == 21);
  CHECK(host.calls.empty());
  CHECK(warnings.messages ==
        Calls{"ApplyWeatherIntensity: weather intensity 16 is outside the "
              "volume table; the ambient sound is left unchanged",
              "ApplyWeatherIntensity: weather intensity -1 is outside the "
              "volume table; the ambient sound is left unchanged"});
}

TEST_CASE("Room weather drifts the wind direction on its interval",
          "[field-camera][field-environment]") {
  FakeWeatherHost host;
  SetUpDriftingRoom(host);
  fx::RoomWeatherState state = Drifting();
  fx::UpdateRoomWeather(host, state);
  CHECK(state.wind_direction == 0x220);
  CHECK(state.wind_strength == 0x20);
  CHECK(state.intensity == 5);
  CHECK(host.calls == Calls{"RecalculateWeatherVelocity", "DrawRainParticles"});

  state = Drifting();
  host.randoms = {2};
  fx::UpdateRoomWeather(host, state);
  CHECK(state.wind_direction == 0x1E0);
  CHECK(state.direction_step == -0x20);

  state = Drifting();
  state.wind_direction = 0x2F0;
  fx::UpdateRoomWeather(host, state);
  CHECK(state.wind_direction == 0x300);
}

TEST_CASE("Room weather drifts the strength and intensity on their intervals",
          "[field-camera][field-environment]") {
  FakeWeatherHost host;
  SetUpDriftingRoom(host);
  fx::RoomWeatherState state = Drifting();
  host.ticks = 34;
  fx::UpdateRoomWeather(host, state);
  CHECK(state.wind_direction == 0x200);
  CHECK(state.wind_strength == 0x24);
  CHECK(state.intensity == 5);

  state = Drifting();
  host.ticks = 4;
  fx::UpdateRoomWeather(host, state);
  CHECK(state.wind_strength == 0x20);
  CHECK(state.intensity == 6);
  CHECK(state.particle_count == 48);
}

TEST_CASE("Room weather holds still while paused or unvaried",
          "[field-camera][field-environment]") {
  FakeWeatherHost host;
  SetUpDriftingRoom(host);
  fx::RoomWeatherState state = Drifting();
  host.paused = true;
  fx::UpdateRoomWeather(host, state);
  host.paused = false;
  state.variation_enabled = 0;
  fx::UpdateRoomWeather(host, state);
  fx::RoomWeatherState unvaried = Drifting();
  unvaried.variation_enabled = 0;
  CHECK(state == unvaried);
  CHECK(state.wind_direction == 0x200);
  CHECK(host.calls == Calls{"DrawRainParticles", "DrawRainParticles"});

  host.calls.clear();
  state.kind = fx::kNoWeather;
  fx::UpdateRoomWeather(host, state);
  state.kind = fx::kRainWeather;
  host.environment.reset();
  fx::UpdateRoomWeather(host, state);
  CHECK(host.calls.empty());

  host.environment = fx::RoomEnvironment{};
  host.paused = true;
  for (const std::uint8_t kind : {fx::kSnowWeather, fx::kScreenWeather,
                                  fx::kEmberWeather, std::uint8_t{5}}) {
    state.kind = kind;
    fx::UpdateRoomWeather(host, state);
  }
  CHECK(host.calls == Calls{"DrawSnowParticles", "DrawScreenWeatherParticles",
                            "DrawRisingEmberParticles"});
}

TEST_CASE("Room weather refuses a drift interval outside the mask table",
          "[field-camera][field-environment]") {
  ChannelWarnings warnings{"field.environment"};
  FakeWeatherHost host;
  SetUpDriftingRoom(host);
  Required(host.environment).wind_direction_interval = 4;
  fx::RoomWeatherState state = Drifting();
  fx::UpdateRoomWeather(host, state);
  CHECK(state.wind_direction == 0x200);
  CHECK(host.calls == Calls{"DrawRainParticles"});
  REQUIRE(warnings.messages.size() == 1);
  CHECK(warnings.messages[0] ==
        "UpdateRoomWeather: wind direction interval 4 is outside the mask "
        "table; the wind direction does not drift this frame");
}

TEST_CASE("Weather ambient sound switches",
          "[field-camera][field-environment]") {
  FakeWeatherHost host;
  fx::RoomWeatherState state{.ambient_sound_id = fx::kRainAmbientSound,
                             .intensity = 10};
  CHECK(fx::SetWeatherAmbientSound(host, state, 30) == 21);
  CHECK(state.ambient_sound_id == 30);
  CHECK(host.calls ==
        Calls{"PlaySoundCentered 30", "FadeSoundVolume 30 110 8"});

  host.calls.clear();
  CHECK(fx::SetWeatherAmbientSound(host, state, -1) == 30);
  CHECK(state.ambient_sound_id == -1);
  CHECK(host.calls.empty());
  state.intensity = 0;
  CHECK(fx::SetWeatherAmbientSound(host, state, 30) == -1);
  CHECK(host.calls == Calls{"StopSound 30"});
}

TEST_CASE("Weather ambient sound ducks and comes back",
          "[field-camera][field-environment]") {
  FakeWeatherHost host;
  fx::RoomWeatherState state{.ambient_sound_id = fx::kRainAmbientSound,
                             .intensity = 10};
  fx::DuckWeatherAmbientSound(host, state, 1);
  fx::DuckWeatherAmbientSound(host, state, 0);
  CHECK(host.calls ==
        Calls{"FadeSoundVolume 21 27 8", "FadeSoundVolume 21 110 8"});
  host.calls.clear();
  state.ambient_sound_id = fx::kNoAmbientSound;
  fx::DuckWeatherAmbientSound(host, state, 1);
  CHECK(host.calls.empty());
}

TEST_CASE("Weather setters keep halfwords and free the particles",
          "[field-camera][field-environment]") {
  FakeWeatherHost host;
  fx::RoomWeatherState state{.particle_count = 40,
                             .particle_capacity = 128,
                             .particles = ParticlesId(9)};
  fx::ReleaseWeatherParticles(host, state);
  CHECK(state.particle_count == 0);
  CHECK(state.particle_capacity == 0);
  CHECK(state.particles == fx::WeatherParticlesId::kNone);
  fx::ReleaseWeatherParticles(host, state);
  CHECK(host.calls == Calls{"FreeWeatherParticles 9"});

  host.calls.clear();
  fx::SetWeatherWindAngles(host, state, 0x10123, -5);
  CHECK(state.wind_direction == 0x123);
  CHECK(state.wind_strength == -5);
  CHECK(host.calls == Calls{"RecalculateWeatherVelocity"});
  fx::SetWeatherVariationEnabled(state, 0x102);
  CHECK(state.variation_enabled == 2);
}

TEST_CASE("Live weather intensity reaches the velocity's scale word",
          "[field-camera][field-environment]") {
  std::array<std::int16_t, 10> saved_words{};
  std::copy_n(Field_weatherParameters, saved_words.size(), saved_words.begin());
  const std::int16_t saved_scale = Field_weatherIntensityScale;
  const std::int16_t saved_sound = Field_weatherAmbientSoundId;
  const std::int16_t saved_intensity = Field_weatherIntensity;
  const std::int16_t saved_capacity = Field_weatherParticleCapacity;
  const std::int16_t saved_count = Field_weatherParticleCount;

  Field_weatherAmbientSoundId = fx::kNoAmbientSound;
  Field_weatherParticleCapacity = 128;
  Field_weatherParameters[4] = 0;
  Field_weatherParameters[5] = 0x400;
  VS::Field::LiveSetWeatherIntensity(5);
  const std::int16_t scale = Field_weatherIntensityScale;
  const std::int16_t mirror = Field_weatherParameters[6];
  const std::int16_t velocity_x = Field_weatherParameters[0];
  const std::int16_t count = Field_weatherParticleCount;

  std::copy(saved_words.begin(), saved_words.end(), Field_weatherParameters);
  Field_weatherIntensityScale = saved_scale;
  Field_weatherAmbientSoundId = saved_sound;
  Field_weatherIntensity = saved_intensity;
  Field_weatherParticleCapacity = saved_capacity;
  Field_weatherParticleCount = saved_count;

  CHECK(scale == 52);
  CHECK(mirror == 52);
  CHECK(velocity_x == 52);
  CHECK(count == 40);
}
