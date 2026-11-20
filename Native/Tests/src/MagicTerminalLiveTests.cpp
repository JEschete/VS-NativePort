#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <span>
#include <stdexcept>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Battle/AbilityState.hpp"
#include "Battle/ActionTable.hpp"
#include "Battle/Actor.hpp"
#include "Boot/SessionLive.hpp"
#include "Content/DiscLayout.h"
#include "Content/ModuleData.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Menu/EquipmentDetailControllersLiveFixture.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Framework/Menus.hpp"
#include "Menu/Magic.hpp"
#include "Menu/MagicCostsLive.hpp"
#include "Menu/MagicMenusLive.hpp"
#include "Menu/MagicTerminalLive.hpp"
#include "Menu/OverlayLoadFixture.hpp"
#include "State/StateFlags.hpp"

using VS::Battle::ActionDefinition;

namespace {
namespace magic = VS::Menu::Magic;
namespace framework = VS::Menu::Framework;
using equipment_detail_tests::ArrayRestore;
using equipment_detail_tests::ValueRestore;
constexpr std::string_view kModule = "MENU/MENU0.PRG";
using SchoolEntry = int (*)(unsigned);
constexpr auto kSchoolAddresses = std::to_array<unsigned>(
    {0x80102B08U, 0x80103464U, 0x801037A8U, 0x80103AECU});
constexpr auto kSchoolEntries = std::to_array<SchoolEntry>({
    &magic::UpdateWarlockMenuLive,
    &magic::UpdateShamanMenuLive,
    &magic::UpdateSorcererMenuLive,
    &magic::UpdateEnchanterMenuLive,
});
constexpr std::array kActions{106, 13, 30, 48};
constexpr std::size_t kTextOffset = 400;
constexpr std::size_t kStringIndices = 80;
constexpr std::size_t kRetainedCursor = 31;
constexpr std::size_t kSavePoint = 3;
constexpr std::size_t kRankSelected = 12;
constexpr std::size_t kRankCount = 13;
constexpr std::size_t kPacketLimit = 128;
constexpr std::size_t kGlyphWords = 4;
constexpr std::size_t kArrowWords = 9;
constexpr std::uint32_t kRankGlyphCommand = 0x66;
constexpr std::uint32_t kRankClut = 0x37F5;
constexpr std::size_t kExpectedGlyphs = 10;

struct MagicLiveFixture : equipment_detail_tests::LiveFixture {
  std::vector<std::byte> arena = std::vector<std::byte>(0x20000);
  VS::Core::Memory::ScopedLiveGameHeap heap{arena};
  ValueRestore<decltype(VS::Battle::characterState)> saved_actor{
      VS::Battle::characterState};
  ValueRestore<VS::Battle::Actor*> saved_ashley{VS::Battle::actors[0]};
  ValueRestore<decltype(VS::State::stateFlags)> saved_flags{
      VS::State::stateFlags};
  ValueRestore<decltype(VS::Input::settings)> saved_settings{
      VS::Input::settings};
  ValueRestore<decltype(VS::Menu::Framework::menuState)> saved_control{
      VS::Menu::Framework::menuState};
  ValueRestore<decltype(VS::Menu::Framework::actionSelection)> saved_action{
      VS::Menu::Framework::actionSelection};
  ValueRestore<decltype(VS::Menu::Framework::shortcutInvoked)> saved_shortcut{
      VS::Menu::Framework::shortcutInvoked};
  ValueRestore<decltype(VS::Menu::Framework::itemsText)> saved_items{
      VS::Menu::Framework::itemsText};
  ValueRestore<decltype(VS::Menu::Framework::displayActionCost)> saved_cost{
      VS::Menu::Framework::displayActionCost};
  ValueRestore<decltype(VS::Menu::Framework::actionRestrictions)>
      saved_restrictions{VS::Menu::Framework::actionRestrictions};
  ArrayRestore<ActionDefinition, std::size(VS::Battle::actionDefinitions)>
      saved_actions{VS::Battle::actionDefinitions.data()};
  ArrayRestore<std::uint16_t, std::size(magic::magicStrings)> saved_magic{
      magic::magicStrings};
  ArrayRestore<std::uint16_t, std::size(magic::teleportationStrings)>
      saved_teleport{magic::teleportationStrings};
  ArrayRestore<std::uint16_t, std::size(VS::Menu::menuStrings)> saved_titles{
      VS::Menu::menuStrings};
  ArrayRestore<std::uint8_t, std::size(magic::warlockSpellIds)> saved_warlock{
      magic::warlockSpellIds};
  ArrayRestore<std::uint8_t, std::size(magic::shamanSpellIds)> saved_shaman{
      magic::shamanSpellIds};
  ArrayRestore<std::uint8_t, std::size(magic::sorcererSpellIds)> saved_sorcerer{
      magic::sorcererSpellIds};
  ArrayRestore<std::uint8_t, std::size(magic::enchanterSpellIds)>
      saved_enchanter{magic::enchanterSpellIds};
  ArrayRestore<std::uint8_t,
               std::size(VS::Menu::Framework::cursorBrightnessAnimation)>
      saved_brightness{VS::Menu::Framework::cursorBrightnessAnimation};
  VS::Battle::Actor actor{};
  VS::Battle::ActorStats stats{};

  MagicLiveFixture() {
    framework::ResetLiveMainMenuStatics();
    magic::ResetMagicMenusLiveState();
    magic::ResetTeleportMenuLiveState();
    VS::Battle::characterState = &actor;
    VS::Battle::actors[0] = &actor;
    actor.stats = &stats;
    stats.current_mp = 500;
    VS::State::stateFlags = {};
    VS::Input::settings.cursor_memory = 0;
    VS::Menu::Framework::menuState = {};
    VS::Menu::Framework::actionSelection = {};
    VS::Menu::Framework::shortcutInvoked = 0;
    VS::Menu::Framework::menuMemory = {};
    VS::Menu::Framework::actionRestrictions = 0;
    std::ranges::fill(VS::Battle::actionDefinitions, ActionDefinition{});
    InitializeStrings();
    InitializeSpells();
  }
  ~MagicLiveFixture() {
    CloseList();
    magic::ResetMagicMenusLiveState();
    magic::ResetTeleportMenuLiveState();
    framework::ResetLiveMainMenuStatics();
  }
  void InitializeStrings() {
    for (auto text : {std::span<std::uint16_t>(magic::magicStrings),
                      std::span<std::uint16_t>(magic::teleportationStrings)}) {
      std::ranges::fill(text, std::uint16_t{0});
      std::fill_n(text.begin(), kStringIndices,
                  static_cast<std::uint16_t>(kTextOffset));
      text[kTextOffset] = 0xE701;
    }
    std::ranges::fill(VS::Menu::menuStrings, std::uint16_t{0});
    std::fill_n(VS::Menu::menuStrings, 16, std::uint16_t{64});
    VS::Menu::menuStrings[64] = 0xE701;
  }
  void InitializeSpells() {
    const std::array<std::span<std::uint8_t>, 4> schools{
        magic::warlockSpellIds, magic::shamanSpellIds, magic::sorcererSpellIds,
        magic::enchanterSpellIds};
    for (std::size_t school = 0; school < schools.size(); ++school) {
      std::ranges::fill(schools[school], std::uint8_t{0});
      schools[school][0] = static_cast<std::uint8_t>(kActions[school]);
      auto& action = VS::Battle::actionDefinitions[kActions[school]];
      action.unlocked = 1;
      action.name[0] = 2;
      action.name[1] = static_cast<char>(0xE7);
    }
  }
  void CloseList() {
    if (VS::Menu::Framework::itemsText != nullptr) {
      static_cast<void>(framework::CloseLiveRowsAndRememberCursor());
    }
    framework::ClearLiveMenuExcept(framework::kNoRow);
  }
  void OpenSchool(std::size_t school, unsigned init = 1) {
    REQUIRE(kSchoolEntries[school](init) == 0);
    IdleRows();
    REQUIRE(kSchoolEntries[school](0) == 0);
    REQUIRE(VS::Menu::Framework::subMenuRowCount == 1);
    IdleRows();
  }
  void LeaveSchool(std::size_t school) {
    OpenSchool(school);
    CloseList();
    REQUIRE(kSchoolEntries[school](0) == 0);
    IdleRows();
    REQUIRE(kSchoolEntries[school](0) == kActions[school]);
  }
  void CheckSchoolRow(std::size_t school, int flags) const {
    const auto item = FirstItem();
    CHECK(VS::Menu::Framework::subMenuRowCount == 1);
    CHECK(item.flags == flags);
    CHECK(item.name[0] == (school == 0 ? 1 : 2));
  }
  void LeaveTeleport() {
    VS::State::stateFlags.save_point_states[kSavePoint] = 1;
    REQUIRE(magic::UpdateTeleportMenuLive(1) == 0);
    IdleRows();
    REQUIRE(magic::UpdateTeleportMenuLive(0) == 0);
    REQUIRE(VS::Menu::Framework::subMenuRowCount == 1);
    CloseList();
    REQUIRE(magic::UpdateTeleportMenuLive(0) == 0);
    IdleRows();
    REQUIRE(magic::UpdateTeleportMenuLive(0) > 0);
  }
  void EnterRanks(unsigned init = 1) {
    VS::Battle::actionDefinitions[kActions[0] + 1].unlocked = 1;
    OpenSchool(0, init);
    VS::Menu::Framework::selectedRow = 0;
    static_cast<void>(framework::CloseLiveRowsAndRememberCursor());
    REQUIRE(magic::UpdateWarlockMenuLive(0) == 0);
    IdleRows();
    REQUIRE(magic::UpdateWarlockMenuLive(0) == 0);
    REQUIRE(framework::LiveMenuCursorMemory()[kRankCount] == 2);
    framework::LiveMenuCursorMemory()[kRankSelected] = 0;
  }
  static void ResetThroughBoundary(bool cold) {
    if (cold) {
      const auto entry = +[] { return 0; };
      REQUIRE(VS::Boot::RunLiveSessionLoop(entry, entry) == 0);
    } else {
      menu_overlay_tests::Load load(1, {VS_MENU0_PRG_LBA, VS_MENU0_PRG_SIZE});
      REQUIRE(load.Complete() == 1);
    }
  }
};

SchoolEntry ResolveSchoolEntry(std::size_t school) {
  auto* address = VS::Content::ResolveModuleAddress(
      VS::Content::ModuleSymbols(), kModule, kSchoolAddresses[school]);
  CHECK(address == reinterpret_cast<void*>(kSchoolEntries[school]));
  if (address == nullptr) {
    throw std::runtime_error("missing magic school entry");
  }
  return reinterpret_cast<SchoolEntry>(address);
}

void CheckRankPackets(const MagicLiveFixture& fixture) {
  const void* packet = &fixture.ordering.back();
  std::size_t glyphs = 0;
  std::size_t arrows = 0;
  for (std::size_t visits = 0; packet != nullptr && visits < kPacketLimit;
       ++visits) {
    if (!fixture.Owns(packet)) {
      FAIL("rank drawing linked outside the active frame");
      return;
    }
    const auto* words = static_cast<const u_long*>(packet);
    const auto length = static_cast<const P_TAG*>(packet)->len;
    if (length == kGlyphWords &&
        (words[1] >> VS::Render::kPacketLengthShift) == kRankGlyphCommand) {
      ++glyphs;
      CHECK((words[3] >> framework::kHalfwordShift) == kRankClut);
    }
    if (length == kArrowWords) {
      ++arrows;
    }
    packet = isendprim(packet) ? nullptr : nextPrim(packet);
  }
  CHECK(packet == nullptr);
  CHECK(glyphs == kExpectedGlyphs);
  CHECK(arrows == 1);
}
}  // namespace

TEST_CASE("Magic module entries create only populated restricted school rows",
          "[magic-terminal-live]") {
  MagicLiveFixture fixture;
  const std::size_t school = GENERATE(0U, 1U, 2U, 3U);
  const auto refusals = magic::RefusedMagicMenuCount();
  VS::Menu::Framework::actionRestrictions = 7;
  const auto entry = ResolveSchoolEntry(school);
  REQUIRE(entry(2) == 0);
  fixture.IdleRows();
  REQUIRE(entry(0) == 0);
  fixture.CheckSchoolRow(school, 1);
  CHECK(VS::Input::settings.cursor_memory == 0);
  CHECK(magic::RefusedMagicMenuCount() == refusals);
}

TEST_CASE("Magic outer module ABI publishes categories through its caller byte",
          "[magic-terminal-live][F02600]") {
  MagicLiveFixture fixture;
  const auto refusals = magic::RefusedMagicMenuCount();
  auto* address = VS::Content::ResolveModuleAddress(
      VS::Content::ModuleSymbols(), kModule, 0x801042C8);
  CHECK(address == reinterpret_cast<void*>(&magic::UpdateLive));
  if (address == nullptr) {
    FAIL("missing magic terminal entry");
    return;
  }
  std::uint8_t state = 3;
  REQUIRE(reinterpret_cast<int (*)(std::uint8_t*)>(address)(&state) == 0);
  CHECK(state == 5);
  CHECK(VS::Menu::Framework::subMenuRowCount == 4);
  CHECK(fixture.FirstItem().name[0] == 1);
  CHECK(magic::RefusedMagicMenuCount() == refusals);
}

TEST_CASE("MENU0 completion and cold sessions restart every school owner",
          "[magic-terminal-live][menu-overlay-reload]") {
  MagicLiveFixture fixture;
  const std::size_t school = GENERATE(0U, 1U, 2U, 3U);
  const bool cold = GENERATE(false, true);
  fixture.LeaveSchool(school);
  {
    menu_overlay_tests::Load other(3, {VS_MENU2_PRG_LBA, VS_MENU2_PRG_SIZE});
    REQUIRE(other.Complete() == 1);
  }
  CHECK(kSchoolEntries[school](0) == kActions[school]);
  framework::LiveMenuCursorMemory()[kRetainedCursor] = 3;
  fixture.ResetThroughBoundary(cold);
  REQUIRE(kSchoolEntries[school](0) == 0);
  fixture.CheckSchoolRow(school, 0);
  CHECK(framework::LiveMenuCursorMemory()[kRetainedCursor] == 3);
  CHECK(VS::Battle::actionDefinitions[kActions[school]].unlocked == 1);
}

TEST_CASE("MENU0 completion and cold sessions also restore teleport ownership",
          "[magic-terminal-live][menu-overlay-reload]") {
  MagicLiveFixture fixture;
  const bool cold = GENERATE(false, true);
  fixture.LeaveTeleport();
  fixture.ResetThroughBoundary(cold);
  REQUIRE(magic::UpdateTeleportMenuLive(0) == 0);
  CHECK(VS::Menu::Framework::subMenuRowCount == 1);
  CHECK(VS::Core::Memory::LivePayloadCapacity(VS::Menu::Framework::itemsText)
            .has_value());
  CHECK(VS::State::stateFlags.save_point_states[kSavePoint] == 1);
}

TEST_CASE("Warlock rank drawing preserves native packet chains and glyph style",
          "[magic-terminal-live][F02604]") {
  MagicLiveFixture fixture;
  const auto refusals = magic::RefusedMagicMenuCount();
  fixture.EnterRanks();
  fixture.ResetFrame();
  REQUIRE(magic::UpdateWarlockMenuLive(0) == 0);
  CheckRankPackets(fixture);
  CHECK(magic::RefusedMagicMenuCount() == refusals);
}

TEST_CASE("MENU0 reload and cold entry restart visible rank blinking",
          "[magic-terminal-live][magic-reload-blink]") {
  MagicLiveFixture fixture;
  const bool cold = GENERATE(false, true);
  for (std::size_t step = 0;
       step < std::size(VS::Menu::Framework::cursorBrightnessAnimation);
       ++step) {
    VS::Menu::Framework::cursorBrightnessAnimation[step] =
        static_cast<char>(step + 1);
  }
  fixture.EnterRanks();
  fixture.ResetFrame();
  REQUIRE(magic::UpdateWarlockMenuLive(0) == 0);
  const auto initial = fixture.Sprites();
  REQUIRE_FALSE(initial.empty());
  fixture.ResetFrame();
  REQUIRE(magic::UpdateWarlockMenuLive(0) == 0);
  CHECK(fixture.Sprites() != initial);
  fixture.ResetThroughBoundary(cold);
  fixture.EnterRanks(0);
  fixture.ResetFrame();
  REQUIRE(magic::UpdateWarlockMenuLive(0) == 0);
  CHECK(fixture.Sprites() == initial);
}
