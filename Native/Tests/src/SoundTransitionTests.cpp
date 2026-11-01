#include <span>

#include <catch2/catch_test_macros.hpp>

#include "Actors/SoundTransition.hpp"
#include "Content/ModuleData.h"

namespace {
using VS::Actors::SoundTransition;
using VS::Actors::StepSoundTransition;
}  // namespace

TEST_CASE("A sound glide step is a pure function of its states and tick speed",
          "[actors][audio][sound-transition]") {
  const SoundTransition current{1, 10, 250, 4};
  const SoundTransition target{1, 250, 10, 4};
  CHECK(StepSoundTransition(current, target, 2) ==
        SoundTransition{1, 70, 190, 3});
  CHECK(StepSoundTransition(current, target, 2) ==
        StepSoundTransition(current, target, 2));
  CHECK(StepSoundTransition(current, target, 1024) ==
        SoundTransition{1, 250, 10, 0});
}

TEST_CASE("A sound glide ignores the target's mode and length",
          "[actors][audio][sound-transition]") {
  const SoundTransition current{1, 0, 255, 5};
  CHECK(StepSoundTransition(current, {0, 255, 0, 0}, 4) ==
        StepSoundTransition(current, {1, 255, 0, 5}, 4));
  CHECK(StepSoundTransition({2, 0, 255, 5}, {1, 255, 0, 5}, 4) ==
        SoundTransition{2, 0, 255, 5});
}

TEST_CASE("Promoted sound glide has no original module address",
          "[actors][audio][sound-transition]") {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  // func_800AEEC4 is reached only by direct jal, and its native callers bind
  // the actor record by name.
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800AEEC4u) == nullptr);
}
