#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Battle/Hud.hpp"
#include "Content/DataStore.hpp"
#include "Core/Random.hpp"
#include "Core/RandomLive.hpp"
#include "Effects/ScreenEffectState.hpp"
#include "Menu/ContainerMenuDialogsLiveFixture.hpp"
#include "Menu/ContainerPendingRowsLiveFixture.hpp"
#include "Menu/EquipmentDetailControllersLiveFixture.hpp"
#include "Menu/TextBoxStateLive.hpp"
#include "Menu/TextBoxUpdate.hpp"
#include "Menu/TextBoxUpdateLive.hpp"
#include "Menu/TextBoxUpdateLiveRestore.hpp"
#include "State/Timers.hpp"

using VS::Legacy::ClearOTag;
using VS::Legacy::GpuHandle;

namespace {
namespace menu = VS::Menu;
using equipment_detail_tests::ArrayRestore;
using equipment_detail_tests::ValueRestore;
constexpr std::size_t kFrameEntries = 32;
constexpr std::size_t kSceneEntry = 18;
constexpr std::size_t kTextEntry = 12;
constexpr std::uint32_t kTerminator = 0xFFFFFF;
constexpr std::uint32_t kRandomSeed = 0x12345678;

struct LiveUpdateFixture {
  ArrayRestore<menu::TextBoxState, menu::kTextBoxCount> saved_boxes{
      menu::textBoxes};
  ValueRestore<decltype(VS::Render::FrameScratchpad())> saved_frame{
      VS::Render::FrameScratchpad()};
  ValueRestore<void*> saved_ordering{VS::Render::legacyOrderingTables[0]};
  ValueRestore<int> saved_framebuffer{VS::Render::frameBuf};
  text_box_tests::LiveUpdateRestore saved_update;
  std::array<std::uint32_t, kFrameEntries> ordering{};

  explicit LiveUpdateFixture(std::uint8_t buffer = 0) {
    std::ranges::fill(menu::textBoxes, menu::TextBoxState{});
    std::ranges::fill(ordering, kTerminator);
    VS::Menu::textBoxBufferIndex = static_cast<char>(buffer);
    menu::activeTextBoxOrderingTable =
        VS::Menu::textBoxOrderingTables + buffer * menu::kTextBoxOrderingTags;
    ClearOTag(menu::activeTextBoxOrderingTable,
              static_cast<int>(menu::kTextBoxOrderingTags));
    VS::Render::frameBuf = 0;
    VS::Render::legacyOrderingTables[0] = ordering.data();
    VS::Render::FrameScratchpad().ordering_table =
        ordering.data() + kSceneEntry;
    VS::State::tickSpeed = 2;
    VS::Battle::lowerScreenUiState = 0;
    VS::Core::interfaceRandomState = kRandomSeed;
    VS::Effects::screenEffectOverlay = nullptr;
    VS::Effects::screenEffectDefaults = {};
    auto& box = menu::textBoxes[0];
    box.flags.kind = static_cast<unsigned>(menu::TextBoxKind::kHidden);
    box.flags.open_style = 2;
    box.state = 2;
    box.open_duration = 4;
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
  auto* submitted = menu::activeTextBoxOrderingTable;
  const auto next = static_cast<std::uint8_t>((buffer + 1) % 3);
  auto expected_random = kRandomSeed;
  VS::Core::Random::NextInterfaceRandomBits(expected_random, 0);
  menu::UpdateLiveTextBoxes();
  CHECK(menu::textBoxes[0].state == 3);
  CHECK(menu::textBoxes[0].frame_x == 40);
  CHECK(fixture.ordering[kTextEntry] == GpuHandle(submitted));
  CHECK(submitted[menu::kTextBoxOrderingTags - 1] == kTerminator);
  CHECK(static_cast<std::uint8_t>(VS::Menu::textBoxBufferIndex) == next);
  CHECK(menu::activeTextBoxOrderingTable ==
        VS::Menu::textBoxOrderingTables + next * menu::kTextBoxOrderingTags);
  CHECK(menu::activeTextBoxOrderingTable[menu::kTextBoxOrderingTags - 1] ==
        kTerminator);
  CHECK(VS::Core::interfaceRandomState == expected_random);
}

TEST_CASE(
    "F01392 Live refuses invalid ordering bindings without updating boxes",
    "[text-box-update-live][F01392]") {
  LiveUpdateFixture fixture;
  switch (GENERATE(0, 1, 2, 3)) {
    case 0:
      VS::Render::FrameScratchpad().ordering_table = nullptr;
      break;
    case 1:
      VS::Menu::textBoxBufferIndex = 3;
      break;
    case 2:
      menu::activeTextBoxOrderingTable = fixture.ordering.data();
      break;
    default:
      menu::activeTextBoxOrderingTable = nullptr;
      break;
  }
  const auto box = menu::ReadLiveTextBox(0);
  const auto ordering = fixture.ordering;
  menu::UpdateLiveTextBoxes();
  CHECK(menu::ReadLiveTextBox(0) == box);
  CHECK(fixture.ordering == ordering);
  CHECK(VS::Core::interfaceRandomState == kRandomSeed);
}

TEST_CASE("Original text-box update module entry advances the production frame",
          "[scene-notification-integration][text-box-update-live][F01392]") {
  LiveUpdateFixture fixture;
  auto* const submitted = menu::activeTextBoxOrderingTable;
  auto* resolved = VS::Content::ResolveModuleAddress(
      VS::Content::ModuleSymbols(), "BATTLE/BATTLE.PRG", 0x800CB83C);
  REQUIRE(resolved == reinterpret_cast<void*>(&menu::UpdateLiveTextBoxes));
  reinterpret_cast<decltype(&menu::UpdateLiveTextBoxes)>(resolved)();
  CHECK(menu::textBoxes[0].state == 3);
  CHECK(VS::Menu::textBoxBufferIndex == 1);
  CHECK(fixture.ordering[kTextEntry] == GpuHandle(submitted));
}

namespace {
template <typename Fixture>
void CheckContainerFrameAfterRotation() {
  LiveUpdateFixture prior(2);
  auto* const prior_ordering = menu::activeTextBoxOrderingTable;
  {
    Fixture fixture;
    auto& box = menu::textBoxes[0];
    box = {};
    box.flags.kind = static_cast<unsigned>(menu::TextBoxKind::kHidden);
    box.state = 1;
    box.open_duration = 4;
    menu::UpdateLiveTextBoxes();
    CHECK(box.state == 2);
  }
  CHECK(VS::Menu::textBoxBufferIndex == 2);
  CHECK(menu::activeTextBoxOrderingTable == prior_ordering);
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
