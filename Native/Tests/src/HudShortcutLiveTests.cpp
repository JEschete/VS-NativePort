#include <algorithm>
#include <cstdint>
#include <iterator>

#include <catch2/catch_test_macros.hpp>

#include "Battle/HudRenderingLive.hpp"
#include "Battle/HudShortcutLiveFixture.hpp"

namespace {
namespace b = VS::Battle;
namespace f = hud_rendering_fixture;
}  // namespace

TEST_CASE("F01358 Live permits unused null spell rows but hides their classes",
          "[hud-rendering-live][F01358]") {
  f::LiveShortcut state;
  CHECK(b::LiveShortcutIconBrightness(0) == 0);
  state.stats.currentMp = 0;
  CHECK(b::LiveShortcutIconBrightness(4) == 0x180);
  CHECK(Battle_spellClassSpellIds[1] == nullptr);
  CHECK(Battle_spellClassCounts[1] == 0);
}

TEST_CASE(
    "F01358 Live accepts an aliased last spell and leaves later rows unused",
    "[hud-rendering-live][F01358]") {
  f::LiveShortcut state;
  auto* last = std::end(Menu_Magic_warlockSpellIds) - 1;
  *last = static_cast<char>(255);
  f::LiveShortcut::SpellClass(1, last, 2);
  Battle_actionDefinitions[255].unlocked = 1;
  CHECK(b::LiveShortcutIconBrightness(0) == 0x180);
  state.stats.currentMp = 0;
  CHECK(b::LiveShortcutIconBrightness(0) == 0x140);
  CHECK(Battle_spellClassSpellIds[0] == nullptr);
  CHECK(Battle_spellClassCounts[1] == 2);
}

TEST_CASE(
    "F01358 Live distinguishes chain and defense and ignores padding entries",
    "[hud-rendering-live][F01358]") {
  f::LiveShortcut state;
  std::ranges::fill(Battle_chainAbilityActionIds, char{100});
  std::ranges::fill(Battle_defenseAbilityActionIds, char{101});
  Battle_actionDefinitions[202].unlocked = 1;
  Battle_chainAbilityActionIds[14] = static_cast<char>(202);
  Battle_defenseAbilityActionIds[13] = static_cast<char>(202);
  state.stats.currentMp = 0;
  CHECK(b::LiveShortcutIconBrightness(6) == 0);
  CHECK(b::LiveShortcutIconBrightness(5) == 0x180);
  Battle_chainAbilityActionIds[13] = static_cast<char>(202);
  CHECK(b::LiveShortcutIconBrightness(6) == 0x180);
  CHECK(Battle_chainAbilityActionIds[14] == static_cast<char>(202));
}

TEST_CASE("F01358 Live resamples the current weapon class on repeated queries",
          "[hud-rendering-live][F01358]") {
  f::LiveShortcut state;
  Battle_actionDefinitions[184].unlocked = 1;
  state.stats.currentHp = 0;
  CHECK(b::LiveShortcutIconBrightness(7) == 0x140);
  state.actor.equippedWeaponCategory = 2;
  state.stats.currentHp = 1;
  CHECK(b::LiveShortcutIconBrightness(7) == 0);
  Battle_actionDefinitions[188].unlocked = 1;
  CHECK(b::LiveShortcutIconBrightness(7) == 0x180);
}

TEST_CASE(
    "F01358 Live defaults still sample restrictions but not unused unlocks",
    "[hud-rendering-live][F01358]") {
  f::LiveShortcut state;
  Battle_spellClassCounts[0] = 255;
  state.stats.currentHp = 0;
  CHECK(b::LiveShortcutIconBrightness(-1) == 0x180);
  CHECK(b::LiveShortcutIconBrightness(8) == 0x180);
  CHECK(Battle_spellClassSpellIds[0] == nullptr);
  CHECK(Battle_spellClassCounts[0] == 255);
}
