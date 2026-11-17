#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <span>
#include <stdexcept>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Boot/SessionLive.hpp"
#include "Content/DataStore.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Menu/EquipmentDetailControllersLiveFixture.hpp"
#include "Menu/GameClearLifecycleLiveFixture.hpp"
#include "Menu/SceneNotifications.hpp"
#include "Menu/SceneNotificationsLive.hpp"
#include "Menu/TextBoxStateLive.hpp"
#include "Menu/TextBoxUpdate.hpp"
#include "Menu/TextBoxUpdateLive.hpp"

extern "C" {
#include "Battle/ActionTable.h"
#include "Battle/Hud.h"
#include "Battle/TextBoxes.h"
#include "Core/Random.h"
#include "Effects/ScreenEffectState.h"
#include "Effects/ScreenEffects.h"
#include "Event/ScriptVm.h"
#include "Menu/Framework/Menus.h"
#include "State/Timers.h"
}

namespace {
namespace menu = VS::Menu;
namespace memory = VS::Core::Memory;
using equipment_detail_tests::ArrayRestore;
using equipment_detail_tests::ValueRestore;
constexpr std::size_t kAction = 24;
constexpr std::int32_t kFirstItem = 0x1CA;
constexpr std::size_t kNotificationText = 32;
constexpr std::size_t kActionText = 100;
constexpr std::size_t kSpellText = 224;
constexpr std::size_t kActionType = 2;
constexpr std::size_t kActionTextIndex = 22 + kActionType;
constexpr char kEnd = static_cast<char>(VS::Render::kTextTerminator);
constexpr std::size_t kFirstBox = 7;

struct NotificationFixture : equipment_detail_tests::LiveFixture {
  std::vector<std::byte> arena = std::vector<std::byte>(0x20000);
  memory::ScopedLiveGameHeap heap{arena};
  ValueRestore<int*> saved_buffer{Menu_sceneNotificationBuffer};
  ValueRestore<char> saved_timer{Menu_sceneNotificationTimer};
  ValueRestore<char> saved_timed{Menu_sceneNotificationTimed};
  ValueRestore<Battle_ActionDefinition> saved_action{
      Battle_actionDefinitions[kAction]};
  ArrayRestore<u_short, std::size(Battle_doorStrings)> saved_door{
      Battle_doorStrings};
  ArrayRestore<u_short, std::size(Menu_menuStrings)> saved_menu{
      Menu_menuStrings};
  ArrayRestore<char, std::size(Menu_kNotificationItemNames[0])> saved_item{
      Menu_kNotificationItemNames[0]};
  ArrayRestore<u_long, std::size(Menu_textBoxOrderingTables)> saved_tables{
      Menu_textBoxOrderingTables};
  ValueRestore<u_long*> saved_active{Menu_activeTextBoxOrderingTable};
  ValueRestore<char> saved_index{Menu_textBoxBufferIndex};
  ValueRestore<decltype(State_tickSpeed)> saved_speed{State_tickSpeed};
  ValueRestore<decltype(Battle_lowerScreenUiState)> saved_ui{
      Battle_lowerScreenUiState};
  ValueRestore<decltype(Core_interfaceRandomState)> saved_random{
      Core_interfaceRandomState};
  ValueRestore<decltype(Effects_screenEffectOverlay)> saved_overlay{
      Effects_screenEffectOverlay};
  ValueRestore<decltype(Effects_screenEffectDefaults)> saved_defaults{
      Effects_screenEffectDefaults};

  NotificationFixture() {
    Menu_sceneNotificationBuffer = nullptr;
    Menu_sceneNotificationTimer = 77;
    Menu_sceneNotificationTimed = 0;
    Battle_stringContext = {};
    std::ranges::fill(Menu_textBoxes, Menu_TextBoxState{});
    std::ranges::fill(Battle_doorStrings, u_short{0});
    std::ranges::fill(Menu_menuStrings, u_short{0});
    Battle_doorStrings[1] = kNotificationText;
    SetTemplate(Battle_doorStrings, kNotificationText);
    Menu_menuStrings[kActionTextIndex] = kActionText;
    SetTemplate(Menu_menuStrings, kActionText);
    SetTemplate(Menu_menuStrings, kSpellText);
    auto& action = Battle_actionDefinitions[kAction];
    action = {};
    action.type = kActionType;
    action.name[0] = 5;
    action.name[1] = 6;
    action.name[2] = kEnd;
    Menu_kNotificationItemNames[0][0] = 7;
    Menu_kNotificationItemNames[0][1] = 8;
    Menu_kNotificationItemNames[0][2] = kEnd;
    Menu_textBoxBufferIndex = 0;
    Menu_activeTextBoxOrderingTable = Menu_textBoxOrderingTables;
    ClearOTag(Menu_activeTextBoxOrderingTable,
              static_cast<int>(menu::kTextBoxOrderingTags));
    State_tickSpeed = 2;
    Battle_lowerScreenUiState = 0;
    Effects_screenEffectOverlay = nullptr;
    Effects_screenEffectDefaults = {};
  }
  ~NotificationFixture() { menu::ReleaseLiveSceneNotification(); }
  static void SetTemplate(std::span<u_short> table, std::size_t offset) {
    table[offset] = 0x00FF;
    table[offset + 1] = 0x00E7;
  }
  std::span<const std::uint8_t> Buffer() const {
    const auto capacity =
        memory::LivePayloadCapacity(Menu_sceneNotificationBuffer);
    if (!capacity || *capacity < menu::kSceneNotificationBytes) {
      throw std::runtime_error("notification did not allocate its text");
    }
    return {reinterpret_cast<const std::uint8_t*>(Menu_sceneNotificationBuffer),
            menu::kSceneNotificationBytes};
  }
};
}  // namespace

TEST_CASE("Live action and mastered notices format into a bounded heap owner",
          "[scene-notifications-live][F01379]") {
  NotificationFixture fixture;
  const int kind = GENERATE(11, 12);
  const auto refusals = menu::RefusedLiveSceneNotificationCount();
  menu::ShowLiveSceneNotification(kind, kAction, 0x101);
  const auto bytes = fixture.Buffer();
  const std::array<std::uint8_t, 7> expected{0xF8, 0, 0xFB, 4, 5, 6, 0xE7};
  CHECK(std::ranges::equal(bytes.first(expected.size()), expected));
  const auto box = menu::ReadLiveTextBox(kFirstBox);
  CHECK(box.text == reinterpret_cast<const char*>(bytes.data()));
  CHECK(box.flags.kind ==
        static_cast<std::uint8_t>(menu::TextBoxKind::kInformation));
  CHECK(box.flags.frame_shape == (kind == 11 ? 0 : 3));
  CHECK(Menu_sceneNotificationTimer == 120);
  CHECK(Menu_sceneNotificationTimed == 1);
  CHECK(menu::RefusedLiveSceneNotificationCount() == refusals);
}

TEST_CASE("Live item notices do not index the smaller action table",
          "[scene-notifications-live][F01379]") {
  NotificationFixture fixture;
  menu::ShowLiveSceneNotification(1, kFirstItem, 256);
  const auto bytes = fixture.Buffer();
  const std::array<std::uint8_t, 4> expected{7, 8, 0xE7, 4};
  CHECK(std::ranges::equal(bytes.first(expected.size()), expected));
  CHECK(Battle_stringContext.strings[0] == Menu_kNotificationItemNames[0]);
  CHECK(Menu_sceneNotificationTimed == 0);
  CHECK(Menu_sceneNotificationTimer == 77);
}

TEST_CASE("Live notification rejects invalid and oversized text before opening",
          "[scene-notifications-live][F01379]") {
  NotificationFixture fixture;
  const int failure = GENERATE(0, 1, 2, 3);
  int kind = 1;
  int subject = kFirstItem;
  if (failure == 0) {
    kind = -1;
  } else if (failure == 1) {
    kind = 11;
    subject = 256;
  } else if (failure == 2) {
    subject += 33;
  } else {
    constexpr std::size_t kLongTextWords = 60;
    std::fill_n(Battle_doorStrings + kNotificationText, kLongTextWords,
                u_short{0x0101});
    Battle_doorStrings[kNotificationText + kLongTextWords] = 0x00E7;
  }
  const auto box = menu::ReadLiveTextBox(kFirstBox);
  const auto refusals = menu::RefusedLiveSceneNotificationCount();
  menu::ShowLiveSceneNotification(kind, subject, 1);
  CHECK(Menu_sceneNotificationBuffer == nullptr);
  CHECK(menu::ReadLiveTextBox(kFirstBox) == box);
  CHECK(Menu_sceneNotificationTimer == 77);
  CHECK(menu::RefusedLiveSceneNotificationCount() == refusals + 1);
}

TEST_CASE("A busy notification retains its original allocation and text",
          "[scene-notifications-live][F01379]") {
  NotificationFixture fixture;
  menu::ShowLiveSceneNotification(11, kAction, 1);
  const auto bytes = fixture.Buffer();
  const std::vector<std::uint8_t> text(bytes.begin(), bytes.end());
  auto* owner = Menu_sceneNotificationBuffer;
  const auto refusals = menu::RefusedLiveSceneNotificationCount();
  menu::ShowLiveSceneNotification(1, kFirstItem, 0);
  CHECK(Menu_sceneNotificationBuffer == owner);
  CHECK(std::ranges::equal(fixture.Buffer(), text));
  CHECK(menu::RefusedLiveSceneNotificationCount() == refusals + 1);
}

TEST_CASE("Live notification polling waits for the box then narrows its timer",
          "[scene-notifications-live][F01380]") {
  NotificationFixture fixture;
  menu::ShowLiveSceneNotification(11, kAction, 1);
  CHECK(menu::IsLiveSceneNotificationPending() == 1);
  CHECK(Menu_sceneNotificationTimer == 120);
  auto box = menu::ReadLiveTextBox(kFirstBox);
  box.flags.done = true;
  menu::WriteLiveTextBox(kFirstBox, box);
  CHECK(menu::IsLiveSceneNotificationPending() == 1);
  CHECK(Menu_sceneNotificationTimer == 118);
  Menu_sceneNotificationTimer = 2;
  CHECK(menu::IsLiveSceneNotificationPending() == 0);
  CHECK(Menu_sceneNotificationTimer == 0);
  CHECK(memory::LivePayloadCapacity(Menu_sceneNotificationBuffer).has_value());
}

TEST_CASE("Live untimed notification polls text completion without ticking",
          "[scene-notifications-live][F01380]") {
  NotificationFixture fixture;
  menu::ShowLiveSceneNotification(11, kAction, 0);
  auto box = menu::ReadLiveTextBox(kFirstBox);
  box.flags.done = true;
  box.speed = menu::kTextBoxFinishedSpeed;
  menu::WriteLiveTextBox(kFirstBox, box);
  CHECK(menu::IsLiveSceneNotificationPending() == 0);
  CHECK(Menu_sceneNotificationTimer == 77);
}

TEST_CASE(
    "Live notification release detaches its text while the backdrop closes",
    "[scene-notifications-live][F01381]") {
  NotificationFixture fixture;
  menu::ShowLiveSceneNotification(11, kAction, 1);
  auto* owner = Menu_sceneNotificationBuffer;
  menu::ReleaseLiveSceneNotification();
  CHECK(Menu_sceneNotificationBuffer == nullptr);
  CHECK_FALSE(memory::LivePayloadCapacity(owner).has_value());
  const auto closing = menu::ReadLiveTextBox(kFirstBox);
  CHECK(closing.state < 0);
  CHECK(closing.text == nullptr);
  auto* cursor = Render_frameScratch.primitiveCursor;
  menu::UpdateLiveTextBoxes();
  CHECK(menu::ReadLiveTextBox(kFirstBox).state < closing.state);
  CHECK(Render_frameScratch.primitiveCursor != cursor);
  CHECK(Menu_sceneNotificationTimer == 120);
  CHECK(Menu_sceneNotificationTimed == 1);
}

TEST_CASE("Live notification release preserves a foreign text borrow",
          "[scene-notifications-live][F01381]") {
  NotificationFixture fixture;
  menu::ShowLiveSceneNotification(11, kAction, 1);
  const std::array<char, 2> foreign{9, kEnd};
  auto box = menu::ReadLiveTextBox(kFirstBox);
  box.text = foreign.data();
  menu::WriteLiveTextBox(kFirstBox, box);
  menu::ReleaseLiveSceneNotification();
  CHECK(menu::ReadLiveTextBox(kFirstBox).text == foreign.data());
  CHECK(menu::ReadLiveTextBox(kFirstBox).state < 0);
  const auto refused_frees = memory::RefusedLiveHeapRequestCount();
  menu::ReleaseLiveSceneNotification();
  CHECK(memory::RefusedLiveHeapRequestCount() == refused_frees);
}

TEST_CASE("Invalid live notification ownership never reports false completion",
          "[scene-notifications-live][F01380]") {
  NotificationFixture fixture;
  ValueRestore<int*> restore{Menu_sceneNotificationBuffer};
  std::array<int, menu::kSceneNotificationBytes / sizeof(int)> foreign{};
  Menu_sceneNotificationBuffer = foreign.data();
  const auto refusals = menu::RefusedLiveSceneNotificationCount();
  CHECK(menu::IsLiveSceneNotificationPending() == 1);
  CHECK(Menu_sceneNotificationBuffer == foreign.data());
  CHECK(menu::RefusedLiveSceneNotificationCount() == refusals + 1);
}

namespace {
template <typename Entry>
Entry NotificationModuleEntry(unsigned address, Entry expected) {
  auto* resolved = VS::Content::ResolveModuleAddress(
      {Content_moduleSymbols, Content_moduleSymbolCount}, "BATTLE/BATTLE.PRG",
      address);
  CHECK(resolved == reinterpret_cast<void*>(expected));
  if (resolved != reinterpret_cast<void*>(expected)) {
    throw std::runtime_error("incorrect scene notification module entry");
  }
  return reinterpret_cast<Entry>(resolved);
}

void CheckColdNotificationReleased(void* owner) {
  CHECK(Menu_sceneNotificationBuffer == nullptr);
  CHECK_FALSE(memory::LivePayloadCapacity(owner).has_value());
  CHECK(menu::ReadLiveTextBox(kFirstBox).text == nullptr);
  CHECK(Menu_sceneNotificationTimer == 0);
  CHECK(Menu_sceneNotificationTimed == 0);
}
}  // namespace

TEST_CASE("Original notification module entries operate the real text owner",
          "[scene-notification-integration][F01379][F01380][F01381]") {
  NotificationFixture fixture;
  NotificationModuleEntry(0x800CB2B8, &menu::ShowLiveSceneNotification)(
      11, kAction, 1);
  auto* const owner = Menu_sceneNotificationBuffer;
  REQUIRE(owner != nullptr);
  CHECK(fixture.Buffer()[0] == 0xF8);
  CHECK(NotificationModuleEntry(0x800CB45C,
                                &menu::IsLiveSceneNotificationPending)() == 1);
  NotificationModuleEntry(0x800CB50C, &menu::ReleaseLiveSceneNotification)();
  CHECK(Menu_sceneNotificationBuffer == nullptr);
  CHECK_FALSE(memory::LivePayloadCapacity(owner).has_value());
}

TEST_CASE(
    "Cold session releases notification storage before clearing its alias",
    "[scene-notification-integration][scene-notification-boundary]"
    "[F01381]") {
  const game_clear_live_tests::SavedModuleData saved_module;
  const game_clear_live_tests::SavedGlobals<Menu_textBoxSelector,
                                            Event_isTextBoxAwaitingClose>
      saved_ui;
  NotificationFixture fixture;
  const int timed = GENERATE(0, 1);
  menu::ShowLiveSceneNotification(11, kAction, timed);
  auto* const owner = Menu_sceneNotificationBuffer;
  auto* const foreign = memory::AllocateLiveFromLowEnd(32);
  REQUIRE(owner != nullptr);
  REQUIRE(foreign != nullptr);
  CHECK(VS::Boot::RunLiveSessionLoop([] { return 7; }, [] { return 9; }) == 7);
  CheckColdNotificationReleased(owner);
  CHECK(memory::LivePayloadCapacity(foreign).has_value());
  memory::FreeLiveFromLowEnd(foreign);
  if (memory::LivePayloadCapacity(owner)) memory::FreeLiveFromHighEnd(owner);
}

TEST_CASE(
    "Stopped-frame heap reset detaches notification text and permits reentry",
    "[scene-notification-integration][scene-notification-boundary]"
    "[F01381]") {
  const game_clear_live_tests::SavedModuleData saved_module;
  NotificationFixture fixture;
  menu::ShowLiveSceneNotification(11, kAction, 1);
  const std::array<char, 2> foreign{9, kEnd};
  auto other = menu::ReadLiveTextBox(6);
  other.text = foreign.data();
  menu::WriteLiveTextBox(6, other);
  VS::Boot::ResetLiveSessionHeap();
  REQUIRE(Menu_sceneNotificationBuffer == nullptr);
  REQUIRE(menu::ReadLiveTextBox(kFirstBox).text == nullptr);
  CHECK(menu::ReadLiveTextBox(6) == other);
  CHECK(Menu_sceneNotificationTimer == 120);
  CHECK(Menu_sceneNotificationTimed == 1);
  menu::ShowLiveSceneNotification(11, kAction, 1);
  CHECK(memory::LivePayloadCapacity(Menu_sceneNotificationBuffer).has_value());
  CHECK(menu::IsLiveSceneNotificationPending() == 1);
}

TEST_CASE(
    "Checked notification cleanup refuses invalid ownership without mutation",
    "[scene-notification-integration][scene-notification-boundary]"
    "[F01381]") {
  NotificationFixture fixture;
  ValueRestore<int*> restore{Menu_sceneNotificationBuffer};
  const bool undersized_heap = GENERATE(false, true);
  std::array<int, menu::kSceneNotificationBytes / sizeof(int)> foreign{};
  auto* const owner =
      undersized_heap
          ? static_cast<int*>(memory::AllocateLiveFromHighEnd(sizeof(int)))
          : foreign.data();
  if (owner == nullptr) throw std::runtime_error("missing refusal-test owner");
  Menu_sceneNotificationBuffer = owner;
  const auto box = menu::ReadLiveTextBox(kFirstBox);
  const auto refusals = menu::RefusedLiveSceneNotificationCount();
  CHECK_FALSE(menu::CleanupLiveSceneNotificationOwnedState());
  CHECK(Menu_sceneNotificationBuffer == owner);
  CHECK(menu::ReadLiveTextBox(kFirstBox) == box);
  CHECK(Menu_sceneNotificationTimer == 77);
  CHECK(Menu_sceneNotificationTimed == 0);
  CHECK(menu::RefusedLiveSceneNotificationCount() == refusals + 1);
  if (undersized_heap) {
    CHECK(memory::LivePayloadCapacity(owner).has_value());
    if (memory::LivePayloadCapacity(owner)) memory::FreeLiveFromHighEnd(owner);
  }
}
