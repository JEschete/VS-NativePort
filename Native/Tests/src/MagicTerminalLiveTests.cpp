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

#include "Battle/AbilityState.h"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Boot/SessionLive.hpp"
#include "Content/DiscLayout.h"
#include "Content/ModuleData.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Menu/EquipmentDetailControllersLiveFixture.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Framework/Menus.h"
#include "Menu/Magic.h"
#include "Menu/MagicCostsLive.hpp"
#include "Menu/MagicMenusLive.hpp"
#include "Menu/MagicTerminalLive.hpp"
#include "Menu/OverlayLoadFixture.hpp"

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
  ValueRestore<decltype(Battle_characterState)> saved_actor{
      Battle_characterState};
  ValueRestore<Battle_Actor*> saved_ashley{Battle_actors[0]};
  ValueRestore<decltype(Audio_Seq_unusedStateFlags)> saved_flags{
      Audio_Seq_unusedStateFlags};
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
  ArrayRestore<Battle_ActionDefinition, std::size(Battle_actionDefinitions)>
      saved_actions{Battle_actionDefinitions};
  ArrayRestore<u_short, std::size(Menu_Magic_magicStrings)> saved_magic{
      Menu_Magic_magicStrings};
  ArrayRestore<u_short, std::size(Menu_Magic_teleportationStrings)>
      saved_teleport{Menu_Magic_teleportationStrings};
  ArrayRestore<u_short, std::size(Menu_menuStrings)> saved_titles{
      Menu_menuStrings};
  ArrayRestore<char, std::size(Menu_Magic_warlockSpellIds)> saved_warlock{
      Menu_Magic_warlockSpellIds};
  ArrayRestore<char, std::size(Menu_Magic_shamanSpellIds)> saved_shaman{
      Menu_Magic_shamanSpellIds};
  ArrayRestore<char, std::size(Menu_Magic_sorcererSpellIds)> saved_sorcerer{
      Menu_Magic_sorcererSpellIds};
  ArrayRestore<char, std::size(Menu_Magic_enchanterSpellIds)> saved_enchanter{
      Menu_Magic_enchanterSpellIds};
  ArrayRestore<char, std::size(Menu_Framework_cursorBrightnessAnimation)>
      saved_brightness{Menu_Framework_cursorBrightnessAnimation};
  Battle_Actor actor{};
  Battle_ActorStats stats{};

  MagicLiveFixture() {
    framework::ResetLiveMainMenuStatics();
    magic::ResetMagicMenusLiveState();
    magic::ResetTeleportMenuLiveState();
    Battle_characterState = &actor;
    Battle_actors[0] = &actor;
    actor.stats = &stats;
    stats.currentMp = 500;
    Audio_Seq_unusedStateFlags = {};
    VS::Input::settings.cursorMemory = 0;
    VS::Menu::Framework::menuState = {};
    VS::Menu::Framework::actionSelection = {};
    VS::Menu::Framework::shortcutInvoked = 0;
    VS::Menu::Framework::menuMemory = {};
    VS::Menu::Framework::actionRestrictions = 0;
    std::ranges::fill(Battle_actionDefinitions, Battle_ActionDefinition{});
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
    for (auto text : {std::span<u_short>(Menu_Magic_magicStrings),
                      std::span<u_short>(Menu_Magic_teleportationStrings)}) {
      std::ranges::fill(text, u_short{0});
      std::fill_n(text.begin(), kStringIndices,
                  static_cast<u_short>(kTextOffset));
      text[kTextOffset] = 0xE701;
    }
    std::ranges::fill(Menu_menuStrings, u_short{0});
    std::fill_n(Menu_menuStrings, 16, u_short{64});
    Menu_menuStrings[64] = 0xE701;
  }
  void InitializeSpells() {
    const std::array<std::span<char>, 4> schools{
        Menu_Magic_warlockSpellIds, Menu_Magic_shamanSpellIds,
        Menu_Magic_sorcererSpellIds, Menu_Magic_enchanterSpellIds};
    for (std::size_t school = 0; school < schools.size(); ++school) {
      std::ranges::fill(schools[school], char{0});
      schools[school][0] = static_cast<char>(kActions[school]);
      auto& action = Battle_actionDefinitions[kActions[school]];
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
    Audio_Seq_unusedStateFlags.savePointStates[kSavePoint] = 1;
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
    Battle_actionDefinitions[kActions[0] + 1].unlocked = 1;
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
  CHECK(VS::Input::settings.cursorMemory == 0);
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
  CHECK(Battle_actionDefinitions[kActions[school]].unlocked == 1);
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
  CHECK(Audio_Seq_unusedStateFlags.savePointStates[kSavePoint] == 1);
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
       step < std::size(Menu_Framework_cursorBrightnessAnimation); ++step) {
    Menu_Framework_cursorBrightnessAnimation[step] =
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
