#include <cstdint>
#include <deque>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Effects/WeatherPool.hpp"
#include "Effects/WeatherRender.hpp"
#include "Field/Background.hpp"

namespace {
namespace fx = VS::Effects;
using VS::Field::WeatherArea;
using Calls = std::vector<std::string>;
using Words = std::vector<std::uint32_t>;

template <typename Value>
Value Pop(std::deque<Value>& values) {
  if (values.empty()) return Value{};
  Value value = values.front();
  values.pop_front();
  return value;
}

class FakePoolHost final : public fx::WeatherPoolHost {
 public:
  explicit FakePoolHost(Calls& calls) : calls_(calls) {}

  [[nodiscard]] std::int32_t Random() override {
    calls_.emplace_back("Random");
    return Pop(randoms);
  }
  [[nodiscard]] std::int32_t Sine(std::int32_t angle) override {
    calls_.push_back(std::format("Sine {}", angle));
    return Pop(trig);
  }
  [[nodiscard]] std::int32_t Cosine(std::int32_t angle) override {
    calls_.push_back(std::format("Cosine {}", angle));
    return Pop(trig);
  }
  [[nodiscard]] VS::Field::WeatherParticlesId AllocateParticles(
      std::int32_t /*bytes*/) override {
    return VS::Field::WeatherParticlesId::kNone;
  }
  void FreeParticles(VS::Field::WeatherParticlesId /*particles*/) override {}
  void SetRoomWeather(std::int32_t /*kind*/) override {}

  std::deque<std::int32_t> randoms;
  std::deque<std::int32_t> trig;

 private:
  Calls& calls_;
};

struct Submitted {
  std::int32_t slot = 0;
  Words words;

  friend bool operator==(const Submitted&, const Submitted&) = default;
};

class FakeRenderHost final : public fx::WeatherRenderHost {
 public:
  explicit FakeRenderHost(Calls& calls) : calls_(calls) {}

  void BeginProjection() override { calls_.emplace_back("Begin"); }
  [[nodiscard]] fx::WeatherProjection Project(
      const fx::WeatherPoint& point) override {
    calls_.push_back(
        std::format("Project {} {} {}", point.x, point.y, point.z));
    return Pop(projections);
  }
  [[nodiscard]] fx::WeatherQuadProjection ProjectQuad(
      const std::array<fx::WeatherPoint, fx::kWeatherQuadCorners>& corners)
      override {
    std::string call = "Quad";
    for (const fx::WeatherPoint& corner : corners) {
      call += std::format(" {} {} {}", corner.x, corner.y, corner.z);
    }
    calls_.push_back(std::move(call));
    return Pop(quads);
  }
  [[nodiscard]] fx::WeatherTransform Transform(
      const fx::WeatherPoint& point) override {
    calls_.push_back(
        std::format("Transform {} {} {}", point.x, point.y, point.z));
    return Pop(transforms);
  }
  [[nodiscard]] std::optional<std::uint8_t> CollisionSurface(
      std::int32_t tile_x, std::int32_t tile_z) override {
    calls_.push_back(std::format("Surface {} {}", tile_x, tile_z));
    return surface;
  }
  [[nodiscard]] std::int32_t FloorHeight(std::int16_t x,
                                         std::int16_t z) override {
    calls_.push_back(std::format("Floor {} {}", x, z));
    return floor;
  }
  void Submit(std::int32_t slot, const fx::WeatherPacket& packet) override {
    calls_.push_back(std::format("Submit {}", slot));
    submitted.push_back({.slot = slot,
                         .words = Words(packet.words.begin(),
                                        packet.words.begin() + packet.size)});
  }

  std::deque<fx::WeatherProjection> projections;
  std::deque<fx::WeatherQuadProjection> quads;
  std::deque<fx::WeatherTransform> transforms;
  std::optional<std::uint8_t> surface;
  std::int32_t floor = 0;
  std::vector<Submitted> submitted;

 private:
  Calls& calls_;
};

struct Hosts {
  Calls calls;
  FakePoolHost pool{calls};
  FakeRenderHost render{calls};
};

constexpr fx::WeatherProjection Projected(std::int16_t x, std::int16_t y,
                                          std::int32_t depth) {
  return {.screen = {.x = x, .y = y}, .depth = depth};
}

constexpr fx::WeatherFrame Running(std::int32_t tick_speed,
                                   fx::WeatherVelocity velocity = {}) {
  return {.paused = false, .tick_speed = tick_speed, .velocity = velocity};
}

constexpr fx::WeatherFrame kPaused{.paused = true, .tick_speed = 4};
Words RainPage() { return {0xE1000260, 0}; }
}  // namespace

TEST_CASE("Rain draws a streak back along the velocity after its ticks",
          "[weather-render]") {
  Hosts hosts;
  std::vector<fx::WeatherParticle> particles{
      {.x = 100, .y = -500, .z = 200, .fall_shift = 0}};
  hosts.render.projections = {Projected(50, 60, 300), Projected(40, 45, 7)};

  CHECK(fx::DrawRainParticles(hosts.pool, hosts.render,
                              Running(5, {.x = 3, .y = 10, .z = -4}), particles,
                              1, std::nullopt));
  CHECK(hosts.calls == Calls{"Begin", "Project 106 -480 192",
                             "Project 103 -490 196", "Submit 300",
                             "Submit 300"});
  REQUIRE(hosts.render.submitted.size() == 2);
  CHECK(hosts.render.submitted[0].words ==
        Words{0x52806060, 0x003C0032, 0x00642020, 0x002D0028});
  CHECK(hosts.render.submitted[1].words == RainPage());
  CHECK(particles[0].fall_shift == 0);
}

TEST_CASE("Rain shifts the fall by the low five bits of the fall shift",
          "[weather-render]") {
  Hosts hosts;
  std::vector<fx::WeatherParticle> particles{
      {.x = 100, .y = -500, .z = 200, .fall_shift = 33}};
  hosts.render.projections = {Projected(1000, 60, 300)};
  hosts.pool.randoms = {5, 6, 7, 8};

  CHECK(fx::DrawRainParticles(hosts.pool, hosts.render,
                              Running(2, {.x = 3, .y = 10, .z = -4}), particles,
                              1, std::nullopt));
  CHECK(hosts.calls[1] == "Project 100 -480 200");
  // Off the guard band: respawned over the open area.
  CHECK(particles[0] ==
        fx::WeatherParticle{
            .x = 5, .y = -2553, .z = 6, .fall_shift = 2, .speed = 8});
  CHECK(hosts.render.submitted.empty());
}

// Two drops paused: the first in range but too near to draw and above the
// floor, the second (odd) below it.
struct RainLandingRun {
  explicit RainLandingRun(std::uint8_t surface) {
    hosts.render.projections = {Projected(10, 10, 2), Projected(50, 60, 100),
                                Projected(0, 0, 0)};
    hosts.render.floor = 0x4123;
    hosts.render.surface = surface;
    hosts.pool.randoms = {1, 2, 3, 4};
    fitted = fx::DrawRainParticles(hosts.pool, hosts.render, kPaused, particles,
                                   2, std::nullopt);
  }

  Hosts hosts;
  std::vector<fx::WeatherParticle> particles{{.y = -5, .fall_shift = 2},
                                             {.x = -300, .y = 40, .z = 700}};
  bool fitted = false;
};

TEST_CASE("Rain splashes an odd drop at the floor's 15-bit height",
          "[weather-render]") {
  const RainLandingRun run(3);
  CHECK(run.fitted);
  CHECK(run.particles[1].opaque_c == 1);
  CHECK(run.particles[1].y == 0x4123 - 0x8000);
  CHECK(run.hosts.calls.back() == "Floor -300 700");
  CHECK(run.hosts.calls[run.hosts.calls.size() - 2] == "Surface -2 5");
  CHECK(run.particles[0].fall_shift == 2);
  CHECK(run.particles[0].y == -5);
}

TEST_CASE("Rain respawns a drop landing on the no-floor surface",
          "[weather-render]") {
  const RainLandingRun run(31);
  CHECK(run.fitted);
  CHECK(run.particles[1].opaque_c == 0);
  CHECK(run.particles[1].y == 3 - 2560);
  CHECK(run.hosts.calls.back() == "Random");
  CHECK(run.particles[0].y == -5);
}

// A splash on its fifth frame, drawn at depth `depth`.
struct RainSplashRun {
  RainSplashRun(const fx::WeatherFrame& frame, std::uint8_t sway_phase,
                std::int32_t depth) {
    particles[0].sway_phase = sway_phase;
    hosts.render.quads = {
        {.screens = {{{1, 2}, {3, 4}, {5, 6}, {7, 8}}}, .depth = depth}};
    hosts.pool.randoms = {1, 2, 3, 4};
    fitted = fx::DrawRainParticles(hosts.pool, hosts.render, frame, particles,
                                   1, std::nullopt);
  }

  Hosts hosts;
  std::vector<fx::WeatherParticle> particles{{.y = 10, .opaque_c = 1}};
  bool fitted = false;
};

TEST_CASE("Rain splash frames advance by half the tick speed",
          "[weather-render]") {
  const RainSplashRun run(Running(4), 5, 100);
  CHECK(run.fitted);
  CHECK(run.hosts.calls[1] == "Quad -24 10 -24 24 10 -24 -24 10 24 24 10 24");
  REQUIRE(run.hosts.render.submitted.size() == 1);
  CHECK(run.hosts.render.submitted[0] ==
        Submitted{.slot = 36,
                  .words = {0x2E808080, 0x00020001, 0x38BFC020, 0x00040003,
                            0x0071C030, 0x00060005, 0x0000D020, 0x00080007,
                            0x0000D030}});
  CHECK(run.particles[0].sway_phase == 7);
  CHECK(run.particles[0].opaque_c == 1);
}

TEST_CASE("Rain splash past its frames respawns after drawing",
          "[weather-render]") {
  const RainSplashRun run(kPaused, 9, 100);
  CHECK(run.hosts.render.submitted.size() == 1);
  CHECK(run.particles[0].opaque_c == 0);
  CHECK(run.particles[0].sway_phase == 0);
}

TEST_CASE("Rain splash at the near plane respawns undrawn",
          "[weather-render]") {
  const RainSplashRun run(kPaused, 5, 64);
  CHECK(run.hosts.render.submitted.empty());
  CHECK(run.particles[0].opaque_c == 0);
}

TEST_CASE("Ember respawn draws four randoms over the weather area",
          "[weather-render]") {
  Hosts hosts;
  hosts.pool.randoms = {123, 77, -700, 0x1F};
  const fx::WeatherParticle marked{.pad = 1,
                                   .screen_x = 2,
                                   .screen_y = 3,
                                   .opaque_c = 4,
                                   .sway_phase = 5,
                                   .fall_shift = 6,
                                   .speed = 7};
  // A zero extent leaves the random value, as the R3000's DIV does.
  CHECK(fx::ResetRisingEmberParticle(hosts.pool, marked,
                                     WeatherArea{.origin_x = 100,
                                                 .origin_z = 200,
                                                 .extent_x = 50,
                                                 .extent_z = 0}) ==
        fx::WeatherParticle{.x = 123,
                            .y = 324,
                            .z = 277,
                            .pad = 1,
                            .screen_x = 2,
                            .screen_y = 3,
                            .opaque_c = 0,
                            .sway_phase = 5,
                            .fall_shift = 6,
                            .speed = 23});
  CHECK(hosts.calls.size() == 4);
}

TEST_CASE("Ember spawn sets the pool and refuses more than the storage",
          "[weather-render]") {
  Hosts hosts;
  fx::WeatherPoolState state{};
  std::vector<fx::WeatherParticle> particles(2);
  CHECK_FALSE(fx::InitializeRisingEmberParticles(
      hosts.pool, state, particles, WeatherArea{.extent_x = 10, .extent_z = 10},
      3, 0x105));
  CHECK(state == fx::WeatherPoolState{
                     .particle_count = 3, .particle_capacity = 3, .kind = 5});
  CHECK(hosts.calls.size() == 8);
  CHECK(particles[1].speed == 8);
}

TEST_CASE("Embers rise and draw a flame to the point 192 below",
          "[weather-render]") {
  Hosts hosts;
  const WeatherArea area{.extent_x = 10, .extent_z = 10};
  std::vector<fx::WeatherParticle> particles{
      {.x = 1, .y = 100, .z = 2, .speed = 10}, {.opaque_c = 1}};
  hosts.render.projections = {Projected(100, 100, 200),
                              Projected(132, 164, 5000)};

  CHECK(fx::DrawRisingEmberParticles(hosts.pool, hosts.render, Running(3),
                                     particles, 2, area));
  CHECK(hosts.calls == Calls{"Begin", "Project 1 70 2", "Project 1 262 2",
                             "Submit 5000", "Submit 5000"});
  CHECK(particles[0].y == 70);
  REQUIRE(hosts.render.submitted.size() == 2);
  CHECK(hosts.render.submitted[0].words ==
        Words{0x3A200820, 0x00650067, 0x004040C0, 0x00640064, 0x00000000,
              0x00A40084, 0x00200820, 0x00670063});
  CHECK(hosts.render.submitted[1].words == Words{0xE1000220, 0});
}

TEST_CASE("Embers above the ceiling respawn and near embers are not drawn",
          "[weather-render]") {
  Hosts hosts;
  const WeatherArea area{.extent_x = 10, .extent_z = 10};
  std::vector<fx::WeatherParticle> particles{{.y = -1000, .speed = 10}};
  hosts.pool.randoms = {3, 4, 5, 6};
  hosts.render.projections = {Projected(100, 100, 64)};

  CHECK(fx::DrawRisingEmberParticles(hosts.pool, hosts.render, Running(3),
                                     particles, 1, area));
  CHECK(particles[0].y == 5 + 512);
  CHECK(particles[0].speed == 14);
  CHECK(hosts.render.submitted.empty());
}

TEST_CASE("Snow drifts with a random speed and a sway", "[weather-render]") {
  Hosts hosts;
  std::vector<fx::WeatherParticle> particles{{.y = -100, .speed = 20}};
  hosts.render.projections = {Projected(0, 0, 2)};
  const fx::WeatherVelocity velocity{.x = 16, .y = 8, .z = -24};

  SECTION("a still flake drifts at a quarter of the sway") {
    hosts.pool.randoms = {9, 0x81};
    hosts.pool.trig = {1024, 2048};
    CHECK(fx::DrawSnowParticles(hosts.pool, hosts.render, Running(2, velocity),
                                particles, 1, std::nullopt));
    CHECK(hosts.calls == Calls{"Begin", "Random", "Sine 704", "Cosine 704",
                               "Random", "Project 3 -99 -5"});
    CHECK(particles[0].sway_phase == 0);
    CHECK(particles[0].speed == 22);
  }
  SECTION("a swaying flake ends its sway after 32 ticks") {
    particles[0].sway_phase = 32;
    hosts.pool.randoms = {7};
    hosts.pool.trig = {512, 0, 1024};
    CHECK(fx::DrawSnowParticles(hosts.pool, hosts.render, Running(2, velocity),
                                particles, 1, std::nullopt));
    CHECK(hosts.calls == Calls{"Begin", "Random", "Sine 640", "Cosine 640",
                               "Sine 2048", "Project 3 -101 -3"});
    CHECK(particles[0].sway_phase == 0);
  }
}

TEST_CASE("Snow draws a flake pair and settles odd flakes",
          "[weather-render]") {
  Hosts hosts;
  std::vector<fx::WeatherParticle> particles{
      {.y = -10, .fall_shift = 2}, {.x = 5, .y = 30, .z = 6, .fall_shift = 2}};
  hosts.render.projections = {Projected(0, 0, 2048), Projected(100, 100, 50)};
  hosts.pool.randoms = {1, 2, 3, 4};
  hosts.pool.trig = {640, 1280};
  hosts.render.floor = 0x8010;

  CHECK(fx::DrawSnowParticles(hosts.pool, hosts.render, kPaused, particles, 2,
                              std::nullopt));
  // The first flake is beyond the far plane and respawns.
  CHECK(particles[0].y == 3 - 2560);
  REQUIRE(hosts.render.submitted.size() == 3);
  CHECK(hosts.render.submitted[0].words ==
        Words{0x62FFFFFF, 0x00640064, 0x00020002});
  CHECK(hosts.render.submitted[1].words ==
        Words{0x62FFFFFF, 0x0050006E, 0x00010001});
  CHECK(hosts.render.submitted[2].words == Words{0xE1000271, 0});
  CHECK(particles[1].opaque_c == 1);
  CHECK(particles[1].y == 0x10);
  CHECK(hosts.calls.back() == "Floor 5 6");
}

TEST_CASE("Settled snow fades out over its frames", "[weather-render]") {
  Hosts hosts;
  std::vector<fx::WeatherParticle> particles{
      {.y = 10, .opaque_c = 1, .sway_phase = 3}};
  hosts.render.projections = {Projected(20, 30, 40)};
  hosts.pool.randoms = {1, 2, 3, 4};

  CHECK(fx::DrawSnowParticles(hosts.pool, hosts.render, Running(10), particles,
                              1, std::nullopt));
  REQUIRE(hosts.render.submitted.size() == 2);
  CHECK(hosts.render.submitted[0] ==
        Submitted{.slot = 40, .words = {0x62505050, 0x001E0014, 0x00020002}});
  CHECK(hosts.render.submitted[1].words == RainPage());
  // Three plus five frames is past the fourth: respawned.
  CHECK(particles[0].opaque_c == 0);
  CHECK(particles[0].y == 3 - 2560);
}

struct ScreenTrailRun {
  Hosts hosts;
  fx::WeatherPoolState state{.particle_count = 1, .screen_fade = 4};
  std::vector<fx::WeatherParticle> particles{
      {.x = 200, .y = 60, .z = -768, .screen_x = 150, .screen_y = 100}};
  bool fitted = fx::DrawScreenWeatherParticles(
      hosts.pool, hosts.render, Running(1), {.x = 1, .y = 2, .z = 3}, state,
      particles);
};

TEST_CASE("Screen weather divides like the R3000 and wraps its fade",
          "[weather-render]") {
  const ScreenTrailRun run;
  CHECK(run.fitted);
  CHECK(run.hosts.calls[1] == "Transform 1 2 3");
  // The fade steps down past zero, as the byte does.
  CHECK(run.state.screen_fade == 252);
  CHECK(run.particles[0].screen_x == 159);
  CHECK(run.particles[0].screen_y == 121);
}

TEST_CASE("Screen weather draws a fading trail of tiles", "[weather-render]") {
  const ScreenTrailRun run;
  const std::vector<Submitted>& submitted = run.hosts.render.submitted;
  REQUIRE(submitted.size() == 4);
  CHECK(submitted[0] ==
        Submitted{.slot = 4, .words = {0x62232323, 0x0079009F, 0x00010001}});
  CHECK(submitted[1].words == Words{0x62111111, 0x00640096, 0x00010001});
  CHECK(submitted[2].words == Words{0x62080808, 0x004F008D, 0x00010001});
  CHECK(submitted[3] == Submitted{.slot = 4, .words = {0xE1000231, 0}});
}

TEST_CASE("Screen weather drifts by the transformed velocity and wraps",
          "[weather-render]") {
  Hosts hosts;
  fx::WeatherPoolState state{.particle_count = 1};
  std::vector<fx::WeatherParticle> particles{{.x = 318, .y = 1, .z = 300}};
  hosts.render.transforms = {{.x = 8, .y = 16, .z = -8},
                             {.x = 40, .y = 0, .z = 8}};
  hosts.pool.randoms = {7, 1};

  CHECK(fx::DrawScreenWeatherParticles(
      hosts.pool, hosts.render, Running(2, {.x = 4, .y = 5, .z = 6}),
      {.x = 1, .y = 2, .z = 3}, state, particles));
  CHECK(hosts.calls[2] == "Transform 4 5 6");
  CHECK(particles[0].x == 2);
  CHECK(particles[0].y == 239);
  CHECK(particles[0].z == (302 & 0xFF));
}

TEST_CASE("Screen weather fade eases in while paused", "[weather-render]") {
  Hosts hosts;
  std::vector<fx::WeatherParticle> particles;
  fx::WeatherPoolState state{.screen_fade = 90};
  CHECK(fx::DrawScreenWeatherParticles(hosts.pool, hosts.render, kPaused, {},
                                       state, particles));
  CHECK(state.screen_fade == 98);
  CHECK(fx::DrawScreenWeatherParticles(hosts.pool, hosts.render, kPaused, {},
                                       state, particles));
  CHECK(state.screen_fade == 98);
}

TEST_CASE("Weather draws refuse counts beyond the storage",
          "[weather-render]") {
  Hosts hosts;
  std::vector<fx::WeatherParticle> particles{{.y = -10, .fall_shift = 2}};
  hosts.render.projections = {Projected(0, 0, 2)};
  CHECK_FALSE(fx::DrawRainParticles(hosts.pool, hosts.render, kPaused,
                                    particles, 2, std::nullopt));
  CHECK(hosts.calls == Calls{"Begin", "Project 0 -10 0"});
}
