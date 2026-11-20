#include <array>
#include <cstddef>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include "Actors/ActorRecord.hpp"
#include "Actors/SoundTransitionBinding.hpp"
#include "Audio/SurfaceSounds.hpp"
#include "Legacy/Psx.h"
#include "State/Timers.hpp"

namespace {
std::array<u_char, 4> SoundValues(const VS::Actors::ModelEffectSlot& state) {
  return {state.kind, state.level, state.flags, state.parameter};
}

struct SoundTransitionFixture {
  int saved_tick_speed = VS::State::tickSpeed;
  VS::Actors::ActorRecord actor{};

  ~SoundTransitionFixture() { VS::State::tickSpeed = saved_tick_speed; }

  void Tick(int speed) {
    VS::State::tickSpeed = speed;
    VS::Actors::UpdateActorSoundTransition(actor);
  }

  void CheckCurrent(VS::Actors::ModelEffectSlot expected) const {
    CHECK(SoundValues(actor.sound_effect) == SoundValues(expected));
  }
};

struct TransitionCase {
  VS::Actors::ModelEffectSlot current;
  VS::Actors::ModelEffectSlot target;
  int speed;
  VS::Actors::ModelEffectSlot expected;
};

// func_800AEEC4: unsigned bytes, signed division and timer clamp, 44F14.c:886.
constexpr std::array<TransitionCase, 11> kCases{{
    {{0, 10, 250, 4}, {1, 250, 10, 4}, 2, {0, 10, 250, 4}},
    {{2, 10, 250, 4}, {1, 250, 10, 4}, 2, {2, 10, 250, 4}},
    {{1, 10, 250, 0}, {1, 250, 10, 4}, 2, {1, 10, 250, 0}},
    {{1, 10, 250, 4}, {1, 250, 10, 4}, 2, {1, 70, 190, 3}},
    {{1, 0, 255, 5}, {1, 255, 0, 5}, 4, {1, 102, 153, 3}},
    {{1, 0, 255, 5}, {1, 255, 0, 5}, 1024, {1, 255, 0, 0}},
    {{1, 128, 1, 3}, {1, 127, 2, 3}, 2, {1, 128, 1, 2}},
    {{1, 10, 250, 4}, {1, 250, 10, 4}, 1, {1, 10, 250, 4}},
    {{1, 8, 20, 3}, {1, 20, 8, 3}, 3, {1, 12, 16, 2}},
    {{1, 10, 250, 4}, {1, 250, 10, 4}, 0, {1, 10, 250, 4}},
    {{1, 0, 255, 5}, {1, 0, 255, 5}, 2, {1, 0, 255, 4}},
}};
}  // namespace

TEST_CASE("Actor sound transitions preserve original unsigned-byte arithmetic",
          "[actors][audio][sound-transition]") {
  const auto index =
      GENERATE(Catch::Generators::range(std::size_t{0}, kCases.size()));
  CAPTURE(index);
  const auto& sample = kCases[index];
  SoundTransitionFixture fixture;
  fixture.actor.sound_effect = sample.current;
  fixture.actor.sound_effect_target = sample.target;
  fixture.actor.instance.position.vx = 1234;

  fixture.Tick(sample.speed);

  fixture.CheckCurrent(sample.expected);
  CHECK(SoundValues(fixture.actor.sound_effect_target) ==
        SoundValues(sample.target));
  CHECK(fixture.actor.instance.position.vx == 1234);
}

TEST_CASE("Actor sound transitions finish, retarget and reset independently",
          "[actors][audio][sound-transition]") {
  SoundTransitionFixture fixture;
  fixture.actor.sound_effect = {1, 128, 1, 3};
  fixture.actor.sound_effect_target = {1, 127, 2, 3};
  fixture.Tick(2);
  fixture.Tick(2);
  fixture.Tick(2);
  fixture.CheckCurrent({1, 127, 2, 0});

  fixture.actor.sound_effect.parameter = 2;
  fixture.actor.sound_effect_target = {1, 1, 254, 2};
  fixture.Tick(2);
  fixture.CheckCurrent({1, 64, 128, 1});
  fixture.Tick(2);
  fixture.CheckCurrent({1, 1, 254, 0});

  fixture.actor = {};
  fixture.Tick(2);
  fixture.CheckCurrent({0, 0, 0, 0});
}

TEST_CASE("Actor sound transitions retain the full unsigned timer range",
          "[actors][audio][sound-transition]") {
  SoundTransitionFixture fixture;
  fixture.actor.sound_effect = {1, 0, 255, 255};
  fixture.actor.sound_effect_target = {1, 255, 0, 255};
  for (int frame = 1; frame <= 255; ++frame) {
    fixture.Tick(2);
    const auto pan = static_cast<u_char>(frame);
    const auto remaining = static_cast<u_char>(255 - frame);
    fixture.CheckCurrent({1, pan, remaining, remaining});
  }
}
