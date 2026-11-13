#include <array>
#include <cstdint>
#include <limits>

#include <catch2/catch_test_macros.hpp>

#include "Input/Pad.hpp"
#include "Menu/ContainerMenuDialogsWorld.hpp"

namespace {
using container_dialog_tests::DialogWorld;
using container_dialog_tests::Event;
using container_dialog_tests::Operation;
namespace dialogs = container_dialog_tests::dialogs;
namespace framework = container_dialog_tests::framework;
namespace items = container_dialog_tests::items;
using Category = items::ItemCategory;
using Sound = dialogs::DialogSound;
using Step = dialogs::DiscardStep;
constexpr std::uint8_t kUnrecognizedByte = 255;
constexpr std::uint8_t kLastMiscIndex = 255;
constexpr std::uint8_t kMaximumQuantity = 255;
constexpr std::uint16_t kMiscItemId = 321;
constexpr std::uint16_t kOtherMiscItemId = 322;
constexpr std::uint8_t kAlmostHundred = 99;
constexpr std::uint8_t kCountLimit = 120;
constexpr std::uint8_t kChosenQuantity = 3;
constexpr std::uint8_t kSmallStack = 10;
constexpr std::uint8_t kStaleQuantity = 20;
constexpr std::uint8_t kLastBladeToken = 64;
constexpr std::uint8_t kExampleBladeId = 9;
constexpr std::uint8_t kExampleQuantity = 7;
constexpr std::uint8_t kTwoRowsRemaining = 14;
constexpr std::int32_t kFirstListRow = 20;
constexpr std::int32_t kSecondListRow = 21;
constexpr std::size_t kContainerCursorByte = 162;
constexpr std::size_t kWrappedCursorByte = 160;
constexpr std::int16_t kTargetX = 123;
constexpr std::int32_t kCallbackResult = 4;
constexpr std::int32_t kTriangleAction = -3;
}  // namespace

TEST_CASE("F02969 builds every original category criterion table",
          "[container-menu-dialogs]") {
  const std::array<std::size_t, 7> counts{24, 24, 7, 24, 23, 16, 2};
  for (std::size_t category = 0; category < counts.size(); ++category) {
    DialogWorld world;
    dialogs::SortMenuState state;
    REQUIRE(dialogs::UpdateSortMenu(state, world,
                                    static_cast<Category>(category)) == 0);
    REQUIRE(world.events.empty());
    REQUIRE(dialogs::UpdateSortMenu(state, world) == 0);
    REQUIRE(world.actions.size() == counts[category]);
    world.action = static_cast<std::int32_t>(counts[category] - 1);
    REQUIRE(dialogs::UpdateSortMenu(state, world) ==
            static_cast<std::int32_t>(counts[category]));
    REQUIRE(world.events.back().operation == Operation::kSort);
  }
}

TEST_CASE("F02969 observes a sound callback's category but retains the action",
          "[container-menu-dialogs]") {
  DialogWorld world;
  dialogs::SortMenuState state{dialogs::ActionDialogStep::kInput, 0};
  world.action = 1;
  world.callback = [&](Operation operation) {
    if (operation == Operation::kSound) {
      state.category = static_cast<std::uint8_t>(Category::kMisc);
    }
  };
  REQUIRE(dialogs::UpdateSortMenu(state, world) == 2);
  REQUIRE(world.events.back() == Event{Operation::kSort, 6, 56});
  world.action = std::numeric_limits<std::int32_t>::min();
  REQUIRE(dialogs::UpdateSortMenu(state, world) ==
          std::numeric_limits<std::int32_t>::min() + 1);
  REQUIRE(world.events.back() ==
          Event{Operation::kSound, static_cast<int>(Sound::kLeave)});
}

TEST_CASE("F02969 keeps cancellation lazy and refuses only reached bad text",
          "[container-menu-dialogs]") {
  DialogWorld world;
  world.help.clear();
  dialogs::SortMenuState state{dialogs::ActionDialogStep::kInput,
                               kUnrecognizedByte};
  REQUIRE(dialogs::UpdateSortMenu(state, world) == 0);
  world.action = kTriangleAction;
  REQUIRE(dialogs::UpdateSortMenu(state, world) == -2);
  state.step = dialogs::ActionDialogStep::kCreate;
  world.ready = 0;
  REQUIRE(dialogs::UpdateSortMenu(state, world) == 0);
  world.ready = 1;
  REQUIRE_THROWS_AS(dialogs::UpdateSortMenu(state, world), std::out_of_range);
}

TEST_CASE(
    "F02970 creates unsigned last-slot quantities and genuine space glyphs",
    "[container-menu-dialogs]") {
  DialogWorld world;
  dialogs::DiscardMenuState state{Step::kCreateQuantity, Category::kMisc,
                                  kLastMiscIndex};
  dialogs::DiscardPresentation presentation;
  world.first_owner.items.misc.back() = {.id = kMiscItemId,
                                         .count = kMaximumQuantity};
  REQUIRE(dialogs::UpdateDiscardMenu(state, presentation, world) == 0);
  REQUIRE(state.maximum == 255);
  REQUIRE(presentation.text[6] == 2);
  REQUIRE(presentation.text[7] == 5);
  REQUIRE(presentation.text[8] == 5);
  REQUIRE(world.rows[34].state == framework::RowState::kDismiss);
  REQUIRE(dialogs::UpdateDiscardMenu(state, presentation, world) == 0);
  REQUIRE(presentation.text[2] == VS::Render::kTextSpaceGlyph);
  REQUIRE(state.step == Step::kQuantityInput);
}

TEST_CASE(
    "F02970 respects simultaneous repeat and non-clamping quantity bounds",
    "[container-menu-dialogs]") {
  DialogWorld world;
  dialogs::DiscardMenuState state{
      Step::kQuantityInput, Category::kMisc, 0, 1, 2,
      kAlmostHundred,       kCountLimit};
  dialogs::DiscardPresentation presentation;
  world.repeated = VS::Input::Up;
  REQUIRE(dialogs::UpdateDiscardMenu(state, presentation, world) == 0);
  REQUIRE(state.quantity == 99);
  world.repeated =
      VS::Input::Up | VS::Input::Down | VS::Input::Left | VS::Input::Right;
  REQUIRE(dialogs::UpdateDiscardMenu(state, presentation, world) == 0);
  REQUIRE(state.quantity == 99);
  world.repeated = VS::Input::Left;
  REQUIRE(dialogs::UpdateDiscardMenu(state, presentation, world) == 0);
  REQUIRE(state.quantity == 120);
  state.place = 0;
  world.repeated = VS::Input::Right;
  REQUIRE(dialogs::UpdateDiscardMenu(state, presentation, world) == 0);
  REQUIRE(state.quantity == 0);
}

TEST_CASE(
    "F02970 gives triangle priority and never discards on quantity cancel",
    "[container-menu-dialogs]") {
  DialogWorld world;
  dialogs::DiscardMenuState state{
      Step::kQuantityInput, Category::kMisc, 0, 1, 0,
      kChosenQuantity,      kSmallStack};
  dialogs::DiscardPresentation presentation;
  world.first_owner.items.misc[0] = {.id = kMiscItemId, .count = kSmallStack};
  world.pressed = VS::Input::Circle | VS::Input::Triangle | VS::Input::Cross;
  REQUIRE(dialogs::UpdateDiscardMenu(state, presentation, world) == -2);
  REQUIRE(world.first_owner.items.misc[0].count == 10);
  REQUIRE(state.step == Step::kQuantityInput);
  world.pressed = VS::Input::Circle;
  REQUIRE(dialogs::UpdateDiscardMenu(state, presentation, world) == 0);
  REQUIRE(state.step == Step::kCreateConfirmation);
}

TEST_CASE(
    "F02970 uses the post-sound owner and preserves byte subtraction wrap",
    "[container-menu-dialogs]") {
  DialogWorld world;
  dialogs::DiscardMenuState state{
      Step::kConfirmationInput, Category::kMisc, kLastMiscIndex, 1, 0,
      kStaleQuantity,           kStaleQuantity};
  dialogs::DiscardPresentation presentation;
  world.first_owner.items.misc.back() = {.id = kMiscItemId,
                                         .count = kStaleQuantity};
  world.second_owner.items.misc.back() = {.id = kOtherMiscItemId,
                                          .count = kSmallStack};
  world.action = 0;
  world.callback = [&](Operation operation) {
    if (operation == Operation::kSound) world.active = &world.second_owner;
  };
  REQUIRE(dialogs::UpdateDiscardMenu(state, presentation, world) == 1);
  REQUIRE(world.first_owner.items.misc.back().count == 20);
  REQUIRE(world.second_owner.items.misc.back().count == 246);
  REQUIRE(world.second_owner.items.misc.back().id == 322);
}

TEST_CASE(
    "F02970 preserves partial equipment clears at a missing required grip",
    "[container-menu-dialogs]") {
  DialogWorld world;
  dialogs::DiscardMenuState state{Step::kSingleInput, Category::kWeapon};
  dialogs::DiscardPresentation presentation;
  world.first_owner.items.weapons[0] = {.blade = kLastBladeToken, .grip = 0};
  world.first_owner.items.blades.back().id = kExampleBladeId;
  world.action = 0;
  REQUIRE_THROWS_AS(dialogs::UpdateDiscardMenu(state, presentation, world),
                    std::out_of_range);
  REQUIRE(world.first_owner.items.blades.back().id == 0);
  REQUIRE(world.first_owner.items.blades.back().index == 64);
  REQUIRE(world.first_owner.items.weapons[0].blade == 64);
}

TEST_CASE("F02970 requires the digit table only for a reached vertical repeat",
          "[container-menu-dialogs]") {
  DialogWorld world;
  dialogs::DiscardMenuState state{
      Step::kQuantityInput, Category::kMisc,  0,          1,
      kUnrecognizedByte,    kExampleQuantity, kSmallStack};
  dialogs::DiscardPresentation presentation;
  REQUIRE(dialogs::UpdateDiscardMenu(state, presentation, world) == 0);
  world.repeated = VS::Input::Up;
  REQUIRE_THROWS_AS(dialogs::UpdateDiscardMenu(state, presentation, world),
                    std::out_of_range);
  REQUIRE(state.quantity == 7);
}

TEST_CASE(
    "F02983 finishes row creation and reloads selection after row callbacks",
    "[container-menu-dialogs]") {
  DialogWorld world;
  world.list_row = kTwoRowsRemaining;
  world.rows[kFirstListRow].state = framework::RowState::kSlideX;
  world.rows[kFirstListRow].target0 = std::numeric_limits<std::int16_t>::min();
  world.memory[kContainerCursorByte] = 0;
  world.callback = [&](Operation operation) {
    if (operation == Operation::kRow &&
        world.events.back().first == kSecondListRow) {
      world.memory[kContainerCursorByte] = 1;
    }
  };
  dialogs::RestoreItemRows(world, 0);
  REQUIRE(world.Count(Operation::kAdvanceRows) == 2);
  REQUIRE(world.rows[20].state == framework::RowState::kIdle);
  REQUIRE(world.rows[20].x == -32768);
  REQUIRE(world.rows[20].selected == 1);
  REQUIRE(world.rows[21].selected == 1);
  REQUIRE(world.Count(Operation::kRow) == 20);
}

TEST_CASE("F02983 keeps word-wrapped cursor indexing and late bounds refusal",
          "[container-menu-dialogs]") {
  DialogWorld world;
  world.memory[kWrappedCursorByte] = kExampleQuantity;
  dialogs::RestoreItemRows(world, std::numeric_limits<std::int32_t>::max());
  REQUIRE(world.rows[27].selected == 1);
  world.rows[kFirstListRow].state = framework::RowState::kSlideX;
  world.rows[kFirstListRow].target0 = kTargetX;
  REQUIRE_THROWS_AS(dialogs::RestoreItemRows(world, -82), std::out_of_range);
  REQUIRE(world.rows[20].state == framework::RowState::kIdle);
  REQUIRE(world.rows[20].x == 123);
}

TEST_CASE("F02984 captures direction when building not when initializing",
          "[container-menu-dialogs]") {
  DialogWorld world;
  dialogs::TransferConfirmationState state;
  REQUIRE(dialogs::UpdateTransferConfirmation(state, world, true) == 0);
  world.direction = 1;
  REQUIRE(dialogs::UpdateTransferConfirmation(state, world) == 0);
  REQUIRE(world.actions.size() == 2);
  REQUIRE(world.TextIdentity(world.actions[0].description) == -823);
  world.action = 0;
  REQUIRE(dialogs::UpdateTransferConfirmation(state, world) == 0);
  REQUIRE(world.Count(Operation::kClearRows) == 1);
  REQUIRE(world.Count(Operation::kHidePanels) == 1);
  REQUIRE(dialogs::UpdateTransferConfirmation(state, world) == 1);
}

TEST_CASE("F02984 preserves callback result reloads and full-word cancellation",
          "[container-menu-dialogs]") {
  DialogWorld world;
  dialogs::TransferConfirmationState state{0,
                                           dialogs::ActionDialogStep::kInput};
  world.action = 0;
  world.callback = [&](Operation operation) {
    if (operation == Operation::kFlyOut && world.events.back().first == 1) {
      state.result = kCallbackResult;
    }
  };
  REQUIRE(dialogs::UpdateTransferConfirmation(state, world) == 0);
  REQUIRE(state.result == 4);
  REQUIRE(world.Count(Operation::kClearRows) == 0);
  world.ready = 0;
  REQUIRE(dialogs::UpdateTransferConfirmation(state, world) == 0);
  world.ready = 1;
  REQUIRE(dialogs::UpdateTransferConfirmation(state, world) == 4);
  world.callback = {};
  state.step = dialogs::ActionDialogStep::kInput;
  world.action = kTriangleAction;
  REQUIRE(dialogs::UpdateTransferConfirmation(state, world) == 0);
  REQUIRE(dialogs::UpdateTransferConfirmation(state, world) == -2);
}
