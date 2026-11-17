#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Content/DataStore.hpp"
#include "Core/Random.hpp"
#include "Menu/ContainerMenuDialogsLiveFixture.hpp"
#include "Menu/ContainerPendingRowsLiveFixture.hpp"
#include "Menu/EquipmentDetailControllersLiveFixture.hpp"
#include "Menu/TextBoxStateLive.hpp"
#include "Menu/TextBoxUpdate.hpp"
#include "Menu/TextBoxUpdateLive.hpp"

extern "C" {
#include "Battle/Hud.h"
#include "Core/Random.h"
#include "Effects/ScreenEffectState.h"
#include "Effects/ScreenEffects.h"
#include "State/Timers.h"
}

namespace {
namespace menu = VS::Menu;
using equipment_detail_tests::ArrayRestore;
using equipment_detail_tests::ValueRestore;
constexpr std::size_t kFrameEntries = 32;
constexpr std::size_t kSceneEntry = 18;
constexpr std::size_t kTextEntry = 12;
constexpr u_long kTerminator = 0xFFFFFF;
constexpr std::uint32_t kRandomSeed = 0x12345678;

struct LiveUpdateFixture {
  ArrayRestore<Menu_TextBoxState, menu::kTextBoxCount> saved_boxes{
      Menu_textBoxes};
  ArrayRestore<u_long, std::size(Menu_textBoxOrderingTables)> saved_tables{
      Menu_textBoxOrderingTables};
  ValueRestore<u_long*> saved_active{Menu_activeTextBoxOrderingTable};
  ValueRestore<char> saved_index{Menu_textBoxBufferIndex};
  ValueRestore<decltype(Render_frameScratch)> saved_frame{Render_frameScratch};
  ValueRestore<void*> saved_ordering{Render_legacyOrderingTables[0]};
  ValueRestore<int> saved_framebuffer{Render_frameBuf};
  ValueRestore<decltype(State_tickSpeed)> saved_speed{State_tickSpeed};
  ValueRestore<decltype(Battle_lowerScreenUiState)> saved_ui{
      Battle_lowerScreenUiState};
  ValueRestore<decltype(Core_interfaceRandomState)> saved_random{
      Core_interfaceRandomState};
  ValueRestore<decltype(Effects_screenEffectOverlay)> saved_overlay{
      Effects_screenEffectOverlay};
  ValueRestore<decltype(Effects_screenEffectDefaults)> saved_defaults{
      Effects_screenEffectDefaults};
  std::array<u_long, kFrameEntries> ordering{};

  explicit LiveUpdateFixture(std::uint8_t buffer = 0) {
    std::ranges::fill(Menu_textBoxes, Menu_TextBoxState{});
    std::ranges::fill(ordering, kTerminator);
    Menu_textBoxBufferIndex = static_cast<char>(buffer);
    Menu_activeTextBoxOrderingTable =
        Menu_textBoxOrderingTables + buffer * menu::kTextBoxOrderingTags;
    ClearOTag(Menu_activeTextBoxOrderingTable,
              static_cast<int>(menu::kTextBoxOrderingTags));
    Render_frameBuf = 0;
    Render_legacyOrderingTables[0] = ordering.data();
    Render_frameScratch.orderingTable = ordering.data() + kSceneEntry;
    State_tickSpeed = 2;
    Battle_lowerScreenUiState = 0;
    Core_interfaceRandomState = kRandomSeed;
    Effects_screenEffectOverlay = nullptr;
    Effects_screenEffectDefaults = {};
    auto& box = Menu_textBoxes[0];
    box.flags.kind = static_cast<unsigned>(menu::TextBoxKind::kHidden);
    box.flags.openStyle = 2;
    box.state = 2;
    box.openDuration = 4;
    box.x = 40;
    box.y = 60;
  }
};
}  // namespace

TEST_CASE(
    "F01392 Live submits the original scene slot and rotates three buffers",
    "[text-box-update-live][F01392]") {
  const auto buffer = static_cast<std::uint8_t>(GENERATE(0, 1, 2));
  LiveUpdateFixture fixture(buffer);
  auto* submitted = Menu_activeTextBoxOrderingTable;
  const auto next = static_cast<std::uint8_t>((buffer + 1) % 3);
  auto expected_random = kRandomSeed;
  VS::Core::Random::NextInterfaceRandomBits(expected_random, 0);
  menu::UpdateLiveTextBoxes();
  CHECK(Menu_textBoxes[0].state == 3);
  CHECK(Menu_textBoxes[0].frameX == 40);
  CHECK(fixture.ordering[kTextEntry] == LegacyGpu_Handle(submitted));
  CHECK(submitted[menu::kTextBoxOrderingTags - 1] == kTerminator);
  CHECK(static_cast<std::uint8_t>(Menu_textBoxBufferIndex) == next);
  CHECK(Menu_activeTextBoxOrderingTable ==
        Menu_textBoxOrderingTables + next * menu::kTextBoxOrderingTags);
  CHECK(Menu_activeTextBoxOrderingTable[menu::kTextBoxOrderingTags - 1] ==
        kTerminator);
  CHECK(Core_interfaceRandomState == expected_random);
}

TEST_CASE(
    "F01392 Live refuses invalid ordering bindings without updating boxes",
    "[text-box-update-live][F01392]") {
  LiveUpdateFixture fixture;
  switch (GENERATE(0, 1, 2, 3)) {
    case 0:
      Render_frameScratch.orderingTable = nullptr;
      break;
    case 1:
      Menu_textBoxBufferIndex = 3;
      break;
    case 2:
      Menu_activeTextBoxOrderingTable = fixture.ordering.data();
      break;
    default:
      Menu_activeTextBoxOrderingTable = nullptr;
      break;
  }
  const auto box = menu::ReadLiveTextBox(0);
  const auto ordering = fixture.ordering;
  menu::UpdateLiveTextBoxes();
  CHECK(menu::ReadLiveTextBox(0) == box);
  CHECK(fixture.ordering == ordering);
  CHECK(Core_interfaceRandomState == kRandomSeed);
}

TEST_CASE("Original text-box update module entry advances the production frame",
          "[scene-notification-integration][text-box-update-live][F01392]") {
  LiveUpdateFixture fixture;
  auto* const submitted = Menu_activeTextBoxOrderingTable;
  auto* resolved = VS::Content::ResolveModuleAddress(
      {Content_moduleSymbols, Content_moduleSymbolCount}, "BATTLE/BATTLE.PRG",
      0x800CB83C);
  REQUIRE(resolved == reinterpret_cast<void*>(&menu::UpdateLiveTextBoxes));
  reinterpret_cast<decltype(&menu::UpdateLiveTextBoxes)>(resolved)();
  CHECK(Menu_textBoxes[0].state == 3);
  CHECK(Menu_textBoxBufferIndex == 1);
  CHECK(fixture.ordering[kTextEntry] == LegacyGpu_Handle(submitted));
}

namespace {
template <typename Fixture>
void CheckContainerFrameAfterRotation() {
  LiveUpdateFixture prior(2);
  auto* const prior_ordering = Menu_activeTextBoxOrderingTable;
  {
    Fixture fixture;
    auto& box = Menu_textBoxes[0];
    box = {};
    box.flags.kind = static_cast<unsigned>(menu::TextBoxKind::kHidden);
    box.state = 1;
    box.openDuration = 4;
    menu::UpdateLiveTextBoxes();
    CHECK(box.state == 2);
  }
  CHECK(Menu_textBoxBufferIndex == 2);
  CHECK(Menu_activeTextBoxOrderingTable == prior_ordering);
}
}  // namespace

TEST_CASE("Container fixtures preserve text progression across buffer rotation",
          "[text-box-update-live][text-box-update-fixtures][F01392]") {
  if (GENERATE(false, true)) {
    CheckContainerFrameAfterRotation<
        container_pending_rows_live_tests::PendingRowsLiveFixture>();
  } else {
    CheckContainerFrameAfterRotation<
        container_dialog_live_tests::DialogLiveFixture>();
  }
}
