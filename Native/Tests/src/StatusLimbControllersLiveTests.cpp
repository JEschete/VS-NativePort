#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <stdexcept>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Battle/Actor.hpp"
#include "Battle/Damage.hpp"
#include "Boot/SessionLive.hpp"
#include "Content/DiscLayout.h"
#include "Menu/EquipmentDetailControllersLiveFixture.hpp"
#include "Menu/OverlayLoadFixture.hpp"
#include "Menu/Status/ActorStatus.hpp"
#include "Menu/Status/LimbControllersLive.hpp"

namespace {
namespace status = VS::Menu::Status;
namespace fw = VS::Menu::Framework;
using equipment_detail_tests::ArrayRestore;
using equipment_detail_tests::ValueRestore;

template <typename Entry>
Entry ModuleEntry(unsigned address, Entry expected) {
  auto* resolved = VS::Content::ResolveModuleAddress(
      VS::Content::ModuleSymbols(), "MENU/MENU4.PRG", address);
  CHECK(resolved == reinterpret_cast<void*>(expected));
  if (resolved == nullptr) {
    throw std::runtime_error("missing status limb module entry");
  }
  return reinterpret_cast<Entry>(resolved);
}

struct LiveLimbFixture {
  ValueRestore<VS::Battle::Actor*> saved_actor{VS::Battle::actors[0]};
  ValueRestore<VS::Battle::Actor*> saved_second_actor{VS::Battle::actors[1]};
  ValueRestore<std::int32_t> saved_selected{status::selectedActor};
  ValueRestore<std::uint8_t> saved_element{status::selectedElement};
  ValueRestore<std::int32_t> saved_x{status::screenXPos};
  ValueRestore<std::int32_t> saved_y{status::screenYPos};
  ValueRestore<char> saved_cursor{VS::Menu::Framework::cursorColor};
  ValueRestore<char> saved_subtype{VS::Menu::Framework::equipmentSubtype};
  ValueRestore<char*> saved_buffer{VS::Menu::Framework::stringBuf};
  ValueRestore<VS::Menu::Framework::MenuMemory> saved_memory{
      VS::Menu::Framework::menuMemory};
  ValueRestore<VS::Menu::StringContext> saved_context{
      VS::Battle::stringContext};
  ArrayRestore<int16_t, fw::kEquipmentStatCount> saved_stats{
      VS::Menu::Framework::equipmentStats};
  ArrayRestore<int16_t, fw::kPrimaryStatCount> saved_primary{
      VS::Menu::Framework::strIntAgi};
  ArrayRestore<int16_t, fw::kComparisonStatCount> saved_deltas{
      VS::Menu::Framework::comparisonStatDeltas};
  ArrayRestore<int16_t, fw::kComparisonStatCount> saved_totals{
      VS::Menu::Framework::comparisonStatTotals};
  ArrayRestore<int16_t, fw::kComparisonStatCount> saved_accessory{
      VS::Menu::Framework::comparisonAccessoryStats};
  ArrayRestore<VS::Menu::TextBoxState, std::size(VS::Menu::textBoxes)>
      saved_boxes{VS::Menu::textBoxes};
  ArrayRestore<std::uint16_t, std::size(status::statusStrings)>
      saved_status_text{status::statusStrings};
  ArrayRestore<u_short, std::size(VS::Battle::statusStrings)> saved_battle_text{
      VS::Battle::statusStrings.data()};
  VS::Battle::ActorStats stats{};
  VS::Battle::Actor actor{};
  std::array<char, fw::kStringBufferBytes> output{};

  LiveLimbFixture() {
    status::ResetLiveLimbControllersModuleData();
    actor.stats = &stats;
    VS::Battle::actors[0] = &actor;
    VS::Battle::actors[1] = &actor;
    status::selectedActor = 1;
    VS::Menu::Framework::stringBuf = output.data();
    output.fill(static_cast<char>(0xAA));
    stats.statuses =
        std::bit_cast<decltype(stats.statuses)>(std::uint32_t{0x60});
    for (auto& limb : stats.limbs) {
      limb.name_index = 1;
      limb.hp = limb.max_hp = 100;
    }
    BuildText();
  }
  ~LiveLimbFixture() { status::ResetLiveLimbControllersModuleData(); }

  void PrimeReloadState() {
    status::selectedElement = 6;
    REQUIRE(status::LiveNavigateStatusEffects(&stats, 1) == 0);
    CHECK(status::LiveStatusEffectNavigationControllerState().selected == 1);
    CHECK(
        status::LiveStatusEffectNavigationControllerState().cursor_animation !=
        0);
    status::selectedActor = 2;
    REQUIRE(status::LiveUpdateLimbPanel(-1) == 0);
    status::selectedElement = 11;
    VS::Menu::Framework::menuMemory.limb_selection_and_dimming = 0x85;
  }
  static void CheckResetState() {
    const auto& navigation =
        status::LiveStatusEffectNavigationControllerState();
    CHECK(navigation.count == 0);
    CHECK(navigation.selected == 0);
    CHECK(navigation.cursor_animation == 0);
    CHECK_FALSE(navigation.target_slot.has_value());
    CHECK(std::ranges::all_of(navigation.active,
                              [](auto effect) { return effect == 0; }));
    CHECK(status::LiveLimbPanelControllerState().phase == 0);
    CHECK(status::LiveLimbPanelControllerState().actor_slot == 0);
    CHECK(status::LiveLimbPanelControllerState().animation_steps ==
          std::array<std::int32_t, 6>{});
    CHECK(status::selectedElement == 0);
  }
  void CheckBorrowedOwners(char* workspace, std::uint64_t heap_refusals) {
    CHECK(VS::Menu::Framework::stringBuf == workspace);
    CHECK(VS::Core::Memory::LivePayloadCapacity(workspace).has_value());
    CHECK(VS::Core::Memory::RefusedLiveHeapRequestCount() == heap_refusals);
    CHECK(VS::Battle::actors[1] == &actor);
    CHECK(actor.stats == &stats);
    CHECK(status::statusStrings[74] == 0xE741);
    CHECK(VS::Menu::Framework::menuMemory.limb_selection_and_dimming == 0x85);
  }
  static void ResetThroughBoundary(bool cold) {
    {
      menu_overlay_tests::Load other(3, {VS_MENU2_PRG_LBA, VS_MENU2_PRG_SIZE});
      REQUIRE(other.Complete() == 1);
    }
    CHECK(status::LiveStatusEffectNavigationControllerState().count == 2);
    CHECK(status::LiveLimbPanelControllerState().phase == 1);
    if (cold) {
      const auto entry = +[] { return 0; };
      REQUIRE(VS::Boot::RunLiveSessionLoop(entry, entry) == 0);
    } else {
      menu_overlay_tests::Load load(5, {VS_MENU4_PRG_LBA, VS_MENU4_PRG_SIZE});
      REQUIRE(load.Poll() == 0);
      CHECK(status::LiveStatusEffectNavigationControllerState().count == 2);
      REQUIRE(load.Complete() == 1);
    }
  }

  static void BuildText() {
    std::ranges::fill(status::statusStrings, u_short{0});
    std::ranges::fill(VS::Battle::statusStrings, u_short{0});
    VS::Battle::statusStrings[1] = 64;
    VS::Battle::statusStrings[64] = 0xE710;
    for (std::size_t index = 0; index < status::kLimbConditionCount; ++index) {
      status::statusStrings[13 + index] = 70;
    }
    status::statusStrings[70] = 0xE730;
    status::statusStrings[18] = 72;
    status::statusStrings[19] = 74;
    status::statusStrings[72] = 0xE740;
    status::statusStrings[74] = 0xE741;
    status::statusStrings[253] = 0x00FF;
    status::statusStrings[254] = 0xFF8F;
    status::statusStrings[255] = 0xE701;
  }
};
}  // namespace

TEST_CASE(
    "F02665 owner hook resets file backed navigation without releasing peers",
    "[status-limbs][status-limbs-live][F02665]") {
  LiveLimbFixture fixture;
  const auto navigate =
      ModuleEntry(0x80104134, &status::LiveNavigateStatusEffects);
  CHECK(navigate(&fixture.stats, 0) == 0);
  CHECK(status::LiveStatusEffectNavigationControllerState().count == 2);
  status::selectedElement = 11;
  VS::Menu::Framework::menuMemory.limb_selection_and_dimming = 0x85;
  status::ResetLiveLimbControllersModuleData();
  const auto& state = status::LiveStatusEffectNavigationControllerState();
  CHECK(state.count == 0);
  CHECK_FALSE(state.target_slot.has_value());
  CHECK(std::ranges::all_of(state.active,
                            [](auto effect) { return effect == 0; }));
  CHECK(status::selectedElement == 0);
  CHECK(VS::Menu::Framework::stringBuf == fixture.output.data());
  CHECK(status::statusStrings[74] == 0xE741);
  CHECK(VS::Menu::Framework::menuMemory.limb_selection_and_dimming == 0x85);
}

TEST_CASE("F02668 owner hook clears steps and phase before reentry",
          "[status-limbs][status-limbs-live][F02668]") {
  LiveLimbFixture fixture;
  status::selectedActor = 2;
  const auto panel = ModuleEntry(0x801045B8, &status::LiveUpdateLimbPanel);
  CHECK(panel(-1) == 0);
  CHECK(status::LiveUpdateLimbPanel(-3) == 1);
  CHECK(status::LiveLimbPanelControllerState().animation_steps.back() == 17);
  CHECK(status::LiveUpdateLimbPanel(-2) == 0);
  status::ResetLiveLimbControllersModuleData();
  CHECK(status::LiveUpdateLimbPanel(-3) == 0);
  CHECK(status::LiveLimbPanelControllerState().animation_steps ==
        std::array<std::int32_t, 6>{});
  CHECK(status::LiveUpdateLimbPanel(-1) == 0);
  CHECK(status::LiveLimbPanelControllerState().animation_steps.front() == 12);
}

TEST_CASE("F02668 owner hook forgets actor identity without freeing its owner",
          "[status-limbs][status-limbs-live][F02668]") {
  LiveLimbFixture fixture;
  status::selectedActor = 2;
  CHECK(status::LiveUpdateLimbPanel(-1) == 0);
  CHECK(status::LiveLimbPanelControllerState().actor_slot == 1);
  status::ResetLiveLimbControllersModuleData();
  CHECK(status::LiveLimbPanelControllerState().actor_slot == 0);
  CHECK(VS::Battle::actors[1] == &fixture.actor);
  CHECK(fixture.actor.stats == &fixture.stats);
}

TEST_CASE("F02668 Live rejects the confirmed unnamed entering actor",
          "[status-limbs][status-limbs-live][F02668]") {
  LiveLimbFixture fixture;
  CHECK(status::LiveUpdateLimbPanel(-1) == 0);
  fixture.stats.limbs[0].name_index = 0;
  const auto refused = status::RefusedLiveLimbControllerCount();
  CHECK(status::LiveUpdateLimbPanel(0) == 0);
  CHECK(status::RefusedLiveLimbControllerCount() == refused + 1);
  CHECK(status::LiveLimbPanelControllerState().phase == 1);
  CHECK(status::LiveLimbPanelControllerState().animation_steps.front() == 12);
}

TEST_CASE(
    "F02680 Live publishes bounded text and original table pointer relations",
    "[status-limbs][status-limbs-live][F02680]") {
  LiveLimbFixture fixture;
  status::selectedElement = 2;
  const auto refused = status::RefusedLiveLimbControllerCount();
  ModuleEntry(0x80106308, &status::LiveShowSelectedLimbCondition)();
  CHECK(status::RefusedLiveLimbControllerCount() == refused);
  CHECK(static_cast<std::uint8_t>(fixture.output[0]) == 0x10);
  CHECK(static_cast<std::uint8_t>(fixture.output[1]) == 0x8F);
  CHECK(static_cast<std::uint8_t>(fixture.output[2]) == 0x30);
  CHECK(static_cast<std::uint8_t>(fixture.output[3]) == 0xE7);
  CHECK(static_cast<std::uint8_t>(fixture.output[4]) == 0xAA);
  CHECK(VS::Battle::stringContext.strings[0] ==
        reinterpret_cast<char*>(&VS::Battle::statusStrings[64]));
  CHECK(VS::Battle::stringContext.strings[1] ==
        reinterpret_cast<char*>(&status::statusStrings[70]));
  CHECK(VS::Menu::Framework::menuMemory.limb_selection_and_dimming == 2);
}

TEST_CASE("F02681 Live writes the selected limb not the first limb",
          "[status-limbs][status-limbs-live][F02681]") {
  LiveLimbFixture fixture;
  status::selectedElement = 4;
  fixture.stats.limbs[4].affinities[0] = -32768;
  fixture.stats.limbs[4].types[3] = -123;
  std::ranges::fill(VS::Menu::Framework::equipmentStats, std::int16_t{444});
  const auto refused = status::RefusedLiveLimbControllerCount();
  ModuleEntry(0x801063F8, &status::LivePopulateSelectedLimbDefense)();
  CHECK(status::RefusedLiveLimbControllerCount() == refused);
  CHECK(VS::Menu::Framework::equipmentStats[15] == 444);
  CHECK(VS::Menu::Framework::equipmentStats[16] == -32768);
  CHECK(VS::Menu::Framework::equipmentStats[24] == -32768);
  CHECK(VS::Menu::Framework::equipmentStats[35] == -123);
  CHECK(VS::Menu::Framework::equipmentStats[36] == 444);
  CHECK(VS::Menu::Framework::menuMemory.limb_selection_and_dimming == 0x84);
}

TEST_CASE(
    "F02680 Live refuses a missing MAINMENU workspace without changing peers",
    "[status-limbs][status-limbs-live][F02680]") {
  LiveLimbFixture fixture;
  VS::Battle::stringContext.strings[0] = fixture.output.data();
  VS::Menu::Framework::stringBuf = nullptr;
  const auto refused = status::RefusedLiveLimbControllerCount();
  status::LiveShowSelectedLimbCondition();
  CHECK(status::RefusedLiveLimbControllerCount() == refused + 1);
  CHECK(VS::Battle::stringContext.strings[0] == fixture.output.data());
  CHECK(static_cast<std::uint8_t>(fixture.output[0]) == 0xAA);
}

TEST_CASE("Limb module render entries emit into the real frame owners",
          "[status-limbs][status-limbs-live][F02662][F02663][F02664]") {
  equipment_detail_tests::LiveFixture frame;
  LiveLimbFixture fixture;
  const auto refused = status::RefusedLiveLimbControllerCount();
  auto* before =
      static_cast<u_long*>(VS::Render::FrameScratchpad().primitive_cursor);
  ModuleEntry(0x80103E58, &status::LiveDrawLimbConditionIndicator)(0, 0, 0);
  CHECK(VS::Render::FrameScratchpad().primitive_cursor == before + 7);
  CHECK(nextPrim(VS::Render::FrameScratchpad().overlay_ordering_table) ==
        before);
  before = static_cast<u_long*>(VS::Render::FrameScratchpad().primitive_cursor);
  ModuleEntry(0x80103EF8, &status::LiveDrawLimbLeaderLine)(0, 0, 0, 0);
  CHECK(VS::Render::FrameScratchpad().primitive_cursor == before + 9);
  CHECK(nextPrim(frame.ordering.data() +
                 equipment_detail_tests::kSceneOrderingEntry - 1) == before);
  const auto sprites_before = frame.Sprites().size();
  before = static_cast<u_long*>(VS::Render::FrameScratchpad().primitive_cursor);
  ModuleEntry(0x80103FEC, &status::LiveDrawStatusEffectIcons)(&fixture.stats,
                                                              0);
  CHECK(VS::Render::FrameScratchpad().primitive_cursor > before);
  CHECK(frame.Sprites().size() == sprites_before + 2);
  CHECK(status::RefusedLiveLimbControllerCount() == refused);
}

TEST_CASE("Production MENU4 completion and cold entry reset limb state",
          "[status-limbs][status-limbs-live][status-limbs-reload]") {
  const bool cold = GENERATE(false, true);
  equipment_detail_tests::LiveFixture frame;
  std::vector<std::byte> arena(0x10000);
  VS::Core::Memory::ScopedLiveGameHeap heap{arena};
  LiveLimbFixture fixture;
  const auto release =
      +[](char* payload) { VS::Core::Memory::FreeLiveFromHighEnd(payload); };
  std::unique_ptr<char, decltype(release)> workspace{
      static_cast<char*>(
          VS::Core::Memory::AllocateLiveFromHighEnd(fw::kStringBufferBytes)),
      release};
  REQUIRE(workspace != nullptr);
  VS::Menu::Framework::stringBuf = workspace.get();
  const auto heap_refusals = VS::Core::Memory::RefusedLiveHeapRequestCount();
  fixture.PrimeReloadState();
  fixture.ResetThroughBoundary(cold);
  fixture.CheckResetState();
  fixture.CheckBorrowedOwners(workspace.get(), heap_refusals);
  CHECK(VS::Menu::Framework::menuItems == frame.rows.data());
  REQUIRE(status::LiveNavigateStatusEffects(&fixture.stats, 0) == 0);
  CHECK(status::LiveStatusEffectNavigationControllerState().count == 2);
  // MENU4 also resets the outer caller's actor choice before fresh entry.
  status::selectedActor = 2;
  REQUIRE(status::LiveUpdateLimbPanel(-1) == 0);
  CHECK(status::LiveLimbPanelControllerState().phase == 1);
}

TEST_CASE(
    "Limb Live boundaries diagnose missing actors indices and encoded text",
    "[status-limbs][status-limbs-live]") {
  LiveLimbFixture fixture;
  const auto refused = status::RefusedLiveLimbControllerCount();
  status::LiveDrawLimbConditionIndicator(-1, 0, 0);
  status::LiveDrawLimbLeaderLine(6, 0, 8, 0);
  status::LiveDrawStatusEffectIcons(nullptr, 0);
  status::selectedElement = 6;
  status::LivePopulateSelectedLimbDefense();
  status::selectedElement = 0;
  VS::Battle::statusStrings[1] = 0xFFFF;
  status::LiveShowSelectedLimbCondition();
  status::selectedActor = 0;
  status::LiveShowSelectedLimbCondition();
  CHECK(status::RefusedLiveLimbControllerCount() == refused + 6);
  CHECK(static_cast<std::uint8_t>(fixture.output[0]) == 0xAA);
}
