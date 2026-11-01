#include <span>

#include <catch2/catch_test_macros.hpp>

#include "Content/ModuleData.h"
#include "Core/FrameTiming.hpp"

namespace {
constexpr int kCounterModulus = 65536;
constexpr int kMaximumReplayStep = 262;
}  // namespace

TEST_CASE("A stepped frame counter wraps at sixteen bits like the root counter",
          "[core][clock][frame-budget]") {
  VS::Core::StepFrameCounterTicks(kMaximumReplayStep);
  VS::Core::ResetFrameTimingBaseline();
  int previous = 0;
  int wraps = 0;
  for (int read = 0; read < 600; ++read) {
    const int ticks = VS::Core::ElapsedFrameCounterTicks();
    CHECK(ticks >= 0);
    CHECK(ticks < kCounterModulus);
    CHECK(ticks == (previous + kMaximumReplayStep) % kCounterModulus);
    if (ticks < previous) ++wraps;
    previous = ticks;
  }
  CHECK(wraps == 2);
  VS::Core::StepFrameCounterTicks(0);
  VS::Core::ResetFrameTimingBaseline();
}

TEST_CASE("Promoted frame timing has no original module address",
          "[core][clock][redirect-binding]") {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  // func_800E45B4 is reached only by direct jal, so callers use the typed API
  // by name and the BATTLE.PRG address no longer resolves.
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800E45B4u) == nullptr);
}
