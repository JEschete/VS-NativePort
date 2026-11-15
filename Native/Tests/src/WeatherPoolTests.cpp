#include <cstdint>
#include <deque>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Effects/WeatherPool.hpp"
#include "Field/Background.hpp"
#include "Field/Environment.hpp"

namespace {
namespace fx = VS::Effects;
using VS::Field::WeatherArea;
using VS::Field::WeatherParticlesId;
using Calls = std::vector<std::string>;

constexpr WeatherParticlesId ParticlesId(std::uintptr_t id) {
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  return static_cast<WeatherParticlesId>(id);
}

class FakeWeatherPoolHost final : public fx::WeatherPoolHost {
 public:
  [[nodiscard]] std::int32_t Random() override {
    ++random_calls;
    if (randoms.empty()) return 0;
    const std::int32_t value = randoms.front();
    randoms.pop_front();
    return value;
  }
  [[nodiscard]] std::int32_t Sine(std::int32_t angle) override {
    calls.push_back(std::format("Sine {}", angle));
    return Next();
  }
  [[nodiscard]] std::int32_t Cosine(std::int32_t angle) override {
    calls.push_back(std::format("Cosine {}", angle));
    return Next();
  }
  [[nodiscard]] WeatherParticlesId AllocateParticles(
      std::int32_t bytes) override {
    calls.push_back(std::format("Allocate {:#x}", bytes));
    return allocation;
  }
  void FreeParticles(WeatherParticlesId particles) override {
    calls.push_back(
        std::format("Free {}", static_cast<std::uintptr_t>(particles)));
  }
  void SetRoomWeather(std::int32_t kind) override {
    calls.push_back(std::format("SetRoomWeather {}", kind));
  }

  std::deque<std::int32_t> randoms;
  std::deque<std::int32_t> trig;
  int random_calls = 0;
  WeatherParticlesId allocation = ParticlesId(0x40);
  Calls calls;

 private:
  std::int32_t Next() {
    if (trig.empty()) return 0;
    const std::int32_t value = trig.front();
    trig.pop_front();
    return value;
  }
};

// A particle whose every field holds a marker the rules must keep or replace:
// x, y, z, pad, screen_x, screen_y, then the four bytes.
constexpr fx::WeatherParticle kMarked{11, 12, 13, 14, 15, 16, 17, 18, 19, 20};
}  // namespace

TEST_CASE("Weather pool allocation frees the old buffer first",
          "[weather-pool]") {
  FakeWeatherPoolHost host;
  CHECK(fx::AllocateWeatherParticles(host, ParticlesId(0x20)) ==
        ParticlesId(0x40));
  CHECK(host.calls == Calls{"Free 32", "Allocate 0x800"});

  host.calls.clear();
  host.allocation = WeatherParticlesId::kNone;
  CHECK(fx::AllocateWeatherParticles(host, WeatherParticlesId::kNone) ==
        WeatherParticlesId::kNone);
  CHECK(host.calls == Calls{"Allocate 0x800"});
}

TEST_CASE("Room weather start suppresses only rain", "[weather-pool]") {
  FakeWeatherPoolHost host;
  CHECK_FALSE(fx::InitializeRoomWeather(host, std::nullopt, 1));
  CHECK(host.calls.empty());

  CHECK(fx::InitializeRoomWeather(host, std::int16_t{1}, 1));
  CHECK(fx::InitializeRoomWeather(host, std::int16_t{1}, 0));
  CHECK(fx::InitializeRoomWeather(host, std::int16_t{2}, -5));
  CHECK(fx::InitializeRoomWeather(host, std::int16_t{-1}, 1));
  CHECK(host.calls == Calls{"SetRoomWeather 0", "SetRoomWeather 1",
                            "SetRoomWeather 2", "SetRoomWeather -1"});
}

TEST_CASE("Room weather report reads the kind unsigned", "[weather-pool]") {
  CHECK(fx::RoomWeatherStateOf(0xFF, -1) ==
        fx::RoomWeatherReport{.kind = 255, .intensity = -1});
  CHECK(fx::RoomWeatherStateOf(2, 15) ==
        fx::RoomWeatherReport{.kind = 2, .intensity = 15});
}

TEST_CASE("Falling particle respawns over the weather area", "[weather-pool]") {
  FakeWeatherPoolHost host;
  host.randoms = {1000, -7, 600, 0x1FF};
  const WeatherArea area{
      .origin_x = 0x7FF0, .origin_z = -100, .extent_x = 0x180, .extent_z = 5};
  const fx::WeatherParticle particle =
      fx::ResetFallingWeatherParticle(host, kMarked, area);
  // 0x7FF0 + 1000 % 0x180 wraps past the halfword.
  CHECK(particle.x == static_cast<std::int16_t>(0x7FF0 + (1000 % 0x180)));
  CHECK(particle.z == -100 + (-7 % 5));
  CHECK(particle.y == (600 % 512) - 2560);
  CHECK(particle.speed == 0x7F);
  CHECK(particle.fall_shift == 2);
  CHECK(particle.opaque_c == 0);
  CHECK(particle.sway_phase == 0);
  CHECK(particle.pad == 14);
  CHECK((particle.screen_x == 15 && particle.screen_y == 16));
}

TEST_CASE("Falling particle respawn keeps the R3000 zero extent remainder",
          "[weather-pool]") {
  FakeWeatherPoolHost host;
  host.randoms = {1234, 4321, -515, 0x80};
  const WeatherArea area{
      .origin_x = 10, .origin_z = 20, .extent_x = 0, .extent_z = -1};
  const fx::WeatherParticle particle =
      fx::ResetFallingWeatherParticle(host, kMarked, area);
  CHECK(particle.x == 10 + 1234);
  CHECK(particle.z == 20);
  CHECK(particle.y == (-515 % 512) - 2560);
  CHECK(particle.speed == 0);
  CHECK(host.random_calls == 4);
}

TEST_CASE("Falling particle respawns in the open square outside a room",
          "[weather-pool]") {
  FakeWeatherPoolHost host;
  host.randoms = {5000, -4097, 0x7FFF, 0xFF};
  const fx::WeatherParticle particle =
      fx::ResetFallingWeatherParticle(host, kMarked, std::nullopt);
  CHECK(particle.x == 5000 % 4096);
  CHECK(particle.z == -1);
  CHECK(particle.y == 511 - 2560);
  CHECK(particle.speed == 0x7F);
  CHECK(host.random_calls == 4);
}

TEST_CASE("Falling spawn sets the pool and respawns the count",
          "[weather-pool]") {
  FakeWeatherPoolHost host;
  std::vector<fx::WeatherParticle> particles(4, kMarked);
  fx::WeatherPoolState state{.screen_fade = 9};
  CHECK(fx::InitializeFallingWeatherParticles(host, state, particles,
                                              std::nullopt, 3, 0x101));
  CHECK(state == fx::WeatherPoolState{.particle_count = 3,
                                      .particle_capacity = 3,
                                      .kind = 1,
                                      .screen_fade = 9});
  CHECK(host.random_calls == 12);
  CHECK(particles[2].fall_shift == 2);
  CHECK(particles[3] == kMarked);
}

TEST_CASE("Falling spawn refuses more particles than the storage holds",
          "[weather-pool]") {
  FakeWeatherPoolHost host;
  std::vector<fx::WeatherParticle> particles(2, kMarked);
  fx::WeatherPoolState state;
  CHECK_FALSE(fx::InitializeFallingWeatherParticles(host, state, particles,
                                                    std::nullopt, 0x10003, 2));
  CHECK(state.particle_count == 3);
  CHECK(state.particle_capacity == 3);
  CHECK(host.random_calls == 8);

  host.random_calls = 0;
  CHECK(fx::InitializeFallingWeatherParticles(host, state, {}, std::nullopt, -4,
                                              1));
  CHECK(state.particle_count == -4);
  CHECK(host.random_calls == 0);
}

TEST_CASE("Screen spawn places particles on the screen unprojected",
          "[weather-pool]") {
  FakeWeatherPoolHost host;
  host.randoms = {0x1FF, 650, -241, 0x1FF};
  std::vector<fx::WeatherParticle> particles(2, kMarked);
  fx::WeatherPoolState state{.screen_fade = 0x60};
  CHECK(fx::InitializeScreenWeatherParticles(host, state, particles, 1, 3));
  CHECK(state == fx::WeatherPoolState{.particle_count = 1,
                                      .particle_capacity = 1,
                                      .kind = 3,
                                      .screen_fade = 0});
  const fx::WeatherParticle expected{.x = 650 % 320,
                                     .y = -1,
                                     .z = 0xFF,
                                     .pad = 14,
                                     .screen_x = -1,
                                     .screen_y = -1,
                                     .opaque_c = 17,
                                     .sway_phase = 18,
                                     .fall_shift = 19,
                                     .speed = 0x7F};
  CHECK(particles[0] == expected);
  CHECK(particles[1] == kMarked);
  CHECK_FALSE(
      fx::InitializeScreenWeatherParticles(host, state, particles, 3, 3));
}

TEST_CASE("Weather velocity rounds each Q12 product toward zero",
          "[weather-pool]") {
  FakeWeatherPoolHost host;
  // sin(azimuth), cos(elevation), cos(azimuth), cos(elevation),
  // sin(elevation).
  host.trig = {-4096, 2047, 1, 4096, -3};
  const fx::WeatherVelocity velocity = fx::WeatherVelocityOf(
      host, {.elevation = 0x123, .azimuth = -0x200, .scale = 100});
  CHECK(host.calls == Calls{"Sine -512", "Cosine 291", "Cosine -512",
                            "Cosine 291", "Sine 291"});
  // (-4096 * 100) >> 12 = -100; (-100 * 2047 + 0xFFF) >> 12 = -49.
  CHECK(velocity.x == -49);
  // (-1 * 100 + 0xFFF) >> 12 = 0.
  CHECK(velocity.z == 0);
  // (-3 * 100 + 0xFFF) >> 12 = 0.
  CHECK(velocity.y == 0);
}

TEST_CASE("Weather velocity wraps its products like the R3000",
          "[weather-pool]") {
  FakeWeatherPoolHost host;
  host.trig = {0x7FFFFFFF, 3, -0x40000, 4096, 0x10000};
  const fx::WeatherVelocity velocity = fx::WeatherVelocityOf(
      host, {.elevation = 0, .azimuth = 0, .scale = 0x7FFF});
  const auto q12 = [](std::int64_t product) {
    const auto word = static_cast<std::int32_t>(
        static_cast<std::uint32_t>(static_cast<std::uint64_t>(product)));
    return (word < 0 ? word + 0xFFF : word) >> 12;
  };
  CHECK(velocity.x ==
        static_cast<std::int16_t>(q12(q12(0x7FFFFFFFLL * 0x7FFF) * 3LL)));
  CHECK(velocity.z ==
        static_cast<std::int16_t>(q12(q12(0x40000LL * 0x7FFF) * 4096LL)));
  CHECK(velocity.y == static_cast<std::int16_t>(q12(0x10000LL * 0x7FFF)));
}
