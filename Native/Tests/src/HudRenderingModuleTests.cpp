#include <cstdint>
#include <span>
#include <type_traits>

#include <catch2/catch_test_macros.hpp>

#include "Battle/HudRenderingLive.hpp"
#include "Battle/HudRenderingLiveFixture.hpp"
#include "Battle/HudShortcutLiveFixture.hpp"
#include "Content/ModuleData.h"

namespace {
namespace b = VS::Battle;
namespace f = hud_rendering_fixture;

template <typename Function>
Function HudRenderingEntry(unsigned address, Function expected) {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  void* const entry =
      VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG", address);
  if (entry == nullptr || entry != reinterpret_cast<void*>(expected)) {
    FAIL("HUD render module entry has the wrong registered native signature");
    return nullptr;
  }
  return reinterpret_cast<Function>(entry);
}

static_assert(std::is_same_v<decltype(&b::DrawLiveHudStatGauge),
                             void (*)(int, int, int)>);
static_assert(std::is_same_v<decltype(&b::DrawLiveSlidingStatusIcon),
                             void (*)(int, int, int)>);
static_assert(
    std::is_same_v<decltype(&b::LiveShortcutIconBrightness), int (*)(int)>);
static_assert(std::is_same_v<decltype(&b::DrawLiveShortcutButtonMarker),
                             void (*)(int, int)>);
}  // namespace

TEST_CASE("F01354 production module dispatch renders the risk gauge",
          "[hud-rendering-module][F01354]") {
  const auto draw = HudRenderingEntry(0x800C9B38, &b::DrawLiveHudStatGauge);
  if (draw == nullptr) return;
  f::LiveFrame frame;
  draw(2, 0, 100);
  CHECK(frame.UsedWords() == 26);
  CHECK((frame.primitives[0] >> 24) == 3);
}

TEST_CASE("F01355 production module dispatch renders both status strips",
          "[hud-rendering-module][F01355]") {
  const auto draw =
      HudRenderingEntry(0x800C9CB4, &b::DrawLiveSlidingStatusIcon);
  if (draw == nullptr) return;
  f::LiveFrame frame;
  draw(16, 0, 0);
  CHECK(frame.UsedWords() == 12);
  CHECK(frame.primitives[10] == 0x37FF3068);
}

TEST_CASE("F01358 production module consumes the packed brightness result",
          "[hud-rendering-module][F01358]") {
  const auto brightness =
      HudRenderingEntry(0x800C9EB8, &b::LiveShortcutIconBrightness);
  if (brightness == nullptr) return;
  f::LiveShortcut state;
  state.stats.currentMp = 0;
  CHECK(brightness(4) == 0x180);
  CHECK(brightness(0) == 0);
}

TEST_CASE("F01360 production module dispatch preserves marker argument order",
          "[hud-rendering-module][F01360]") {
  const auto draw =
      HudRenderingEntry(0x800C9F98, &b::DrawLiveShortcutButtonMarker);
  if (draw == nullptr) return;
  f::LiveFrame frame;
  draw(-1, 0x140);
  CHECK(frame.UsedWords() == 6);
  CHECK(frame.primitives[2] == 0x66404040);
  CHECK(frame.primitives[3] == 0xFFFFFFFF);
}
