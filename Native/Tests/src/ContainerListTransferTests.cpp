#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Core/Memory/HeapLive.hpp"
#include "Items/ItemRecordsLive.hpp"
#include "Menu/Container/ContainerListTransferLive.hpp"
#include "Menu/Container/ItemView.h"
#include "Menu/ContainerListTransferWorld.hpp"
#include "Menu/ContainerPendingRowsLiveFixture.hpp"
#include "Menu/ContainerPendingRowsWorld.hpp"
#include "Menu/Framework/FrameworkLive.hpp"

namespace {
namespace world_data = container_list_transfer_tests;
namespace transfer = world_data::transfer;
namespace items = world_data::items;
using world_data::Event;
using world_data::kItemHelpBase;
using world_data::kMenuTextBase;
using world_data::Refused;
using world_data::TransferFlag;
using world_data::TransferSound;
using world_data::TransferStorage;
using world_data::TransferText;
using world_data::World;
namespace rows_data = container_pending_rows_tests;
using transfer::ItemListState;
using transfer::ScreenState;
using transfer::TransferDirectionState;
using transfer::TransferTextLayer;
using transfer::TransferTextTable;

constexpr std::uint32_t kR1 = 0x8;
constexpr std::uint32_t kL1 = 0x4;
constexpr std::uint32_t kTriangle = 0x10;
constexpr std::uint32_t kCircle = 0x20;
constexpr std::uint32_t kCross = 0x40;
constexpr std::uint32_t kSquare = 0x80;
constexpr std::uint32_t kUp = 0x1000;
constexpr std::uint32_t kDown = 0x4000;
constexpr std::int32_t kExit = -2;
constexpr std::int32_t kCancel = -1;
constexpr std::size_t kBladeOrder = 32;
constexpr std::size_t kMiscOrder = 448;
constexpr std::uint8_t kWeaponToken = 1;
constexpr std::uint8_t kBladeToken = 2;
constexpr std::uint8_t kMiscToken = 7;
constexpr std::uint8_t kManageToken = 8;

TransferText Menu(std::uint32_t entry) {
  return {TransferTextTable::kMenuText, kMenuTextBase + entry};
}
TransferText Raw(std::uint32_t offset) {
  return {TransferTextTable::kMenuText, offset};
}
TransferText Help(std::uint32_t offset) {
  return {TransferTextTable::kItemHelp, offset};
}

std::int32_t List(ItemListState& state, World& world) {
  return transfer::UpdateItemList(0, state, world);
}
std::int32_t Direction(TransferDirectionState& state, World& world) {
  return transfer::UpdateTransferDirection(0, state, world);
}
}  // namespace

TEST_CASE("F02951 weapon rows name the weapon record and report stat status",
          "[container-transfer][F02951]") {
  rows_data::RowTextHost host;
  rows_data::Records records;
  rows_data::Populate(records);
  std::array<std::uint8_t, VS::Menu::Framework::kRowDescriptionBufferBytes>
      buffer{};
  const auto row = transfer::BuildWeaponRow(
      host, records.storage.items.weapons[0], records, buffer);
  REQUIRE(row.status == items::ItemsStatus::kOk);
  REQUIRE(row.text.name ==
          std::optional(VS::Menu::Framework::TextRef{
              .source = VS::Menu::Framework::TextSource::kWeaponName}));
  REQUIRE(row.text.description.has_value());
  REQUIRE(row.text.row_type != 0);
}

TEST_CASE("F02951 a weapon naming a part outside the records is refused",
          "[container-transfer][F02951]") {
  rows_data::RowTextHost host;
  rows_data::Records records;
  rows_data::Populate(records);
  auto weapon = records.storage.items.weapons[0];
  weapon.blade = 0xFF;
  std::array<std::uint8_t, VS::Menu::Framework::kRowDescriptionBufferBytes>
      buffer{};
  REQUIRE_THROWS(transfer::BuildWeaponRow(host, weapon, records, buffer));
}

TEST_CASE("F02952 shield rows keep the formatted shield text",
          "[container-transfer][F02952]") {
  rows_data::RowTextHost host;
  rows_data::Records records;
  rows_data::Populate(records);
  std::array<std::uint8_t, VS::Menu::Framework::kRowDescriptionBufferBytes>
      buffer{};
  const auto row = transfer::BuildShieldRow(
      host, records.storage.items.shields[0], records, buffer);
  REQUIRE(row.name.has_value());
  REQUIRE(row.name !=
          std::optional(VS::Menu::Framework::TextRef{
              .source = VS::Menu::Framework::TextSource::kWeaponName}));
  REQUIRE(row.row_type != 0);
  auto shield = records.storage.items.shields[0];
  shield.gems = {};
  const auto reads_before = records.reads;
  REQUIRE(transfer::BuildShieldRow(host, shield, records, buffer).name);
  const auto plain_reads = records.reads - reads_before;
  shield.gems[0] = 1;
  REQUIRE(transfer::BuildShieldRow(host, shield, records, buffer).name);
  REQUIRE(records.reads - reads_before == plain_reads * 2 + 1);
}

TEST_CASE("F02986 initialization forgets the direction only on first entry",
          "[container-transfer][F02986]") {
  World world;
  TransferDirectionState state;
  world.direction = 2;
  REQUIRE(transfer::UpdateTransferDirection(1, state, world) == 0);
  REQUIRE(world.direction == 0);
  REQUIRE(state.step == 0);
  world.direction = 1;
  REQUIRE(transfer::UpdateTransferDirection(1, state, world) == 0);
  REQUIRE(world.direction == 1);

  TransferDirectionState remembered;
  world.cursor_memory = 1;
  REQUIRE(transfer::UpdateTransferDirection(1, remembered, world) == 0);
  REQUIRE(world.direction == 1);
  REQUIRE_FALSE(remembered.cursor_reset_pending);
}

TEST_CASE("F02986 places both direction rows", "[container-transfer][F02986]") {
  World world;
  TransferDirectionState state;
  REQUIRE(Direction(state, world) == 0);
  REQUIRE(Direction(state, world) == 0);
  REQUIRE(world.placements.size() == 2);
  REQUIRE(world.placements[0] ==
          transfer::MenuItemPlacement{10, 0x140, 0x22, 0x7E, 0, Menu(12)});
  REQUIRE(world.placements[1] ==
          transfer::MenuItemPlacement{11, 0x140, 0x32, 0x7E, 0, Menu(14)});
  REQUIRE(world.events[0] == Event{"slide", 10, 0x200C2});
}

TEST_CASE("F02986 waits for idle rows after placing the direction rows",
          "[container-transfer][F02986]") {
  World world;
  TransferDirectionState state;
  static_cast<void>(Direction(state, world));
  static_cast<void>(Direction(state, world));
  world.idle = false;
  REQUIRE(Direction(state, world) == 0);
  REQUIRE(state.step == 2);
  world.idle = true;
  REQUIRE(Direction(state, world) == 0);
  REQUIRE(state.step == 3);
  REQUIRE(world.Count("draw-capacities") == 4);
}

TEST_CASE("F02986 Circle chooses, Cross cancels and Triangle leaves",
          "[container-transfer][F02986]") {
  World world;
  TransferDirectionState state{.step = 3};
  world.direction = 1;
  world.pressed = kCircle;
  REQUIRE(Direction(state, world) == 2);
  REQUIRE(world.sounds.back() == TransferSound::kSelect);
  REQUIRE(world.events[0] == Event{"row-selected", 10, 0});
  REQUIRE(world.events[1] == Event{"row-selected", 11, 1});
  world.pressed = kCross;
  REQUIRE(Direction(state, world) == kCancel);
  world.pressed = kTriangle;
  REQUIRE(Direction(state, world) == kExit);
  REQUIRE(world.Logged("clear-menu"));
  REQUIRE(world.events.back() == Event{"clear-menu", 40});
  REQUIRE_FALSE(world.Logged("draw-capacities"));
}

TEST_CASE("F02986 Up wraps to end transfer", "[container-transfer][F02986]") {
  World world;
  TransferDirectionState state{.step = 3};
  world.repeated = kUp;
  REQUIRE(Direction(state, world) == 0);
  REQUIRE(world.direction == 2);
  REQUIRE(world.sounds == std::vector{TransferSound::kChange});
  REQUIRE(world.messages.back() == Raw(40));
  REQUIRE(state.ok_blink == 16);
  REQUIRE(world.Logged("cursor"));
  REQUIRE(world.events[2] == Event{"cursor", 0, 0x1A00B4});
}

TEST_CASE("F02986 the end transfer row blinks OK above the menu",
          "[container-transfer][F02986]") {
  World world;
  TransferDirectionState state{.step = 3};
  world.repeated = kUp;
  REQUIRE(Direction(state, world) == 0);
  REQUIRE(world.texts.size() == 1);
  REQUIRE(world.texts[0].text == "OK");
  REQUIRE(world.texts[0].xy == 0x4200C8);
  REQUIRE(world.texts[0].layer == TransferTextLayer::kAboveMenu);
}

TEST_CASE("F02986 opposite directions cancel and Down wraps forward once",
          "[container-transfer][F02986]") {
  World world;
  TransferDirectionState state{.step = 3};
  world.direction = 2;
  world.repeated = kUp | kDown;
  REQUIRE(Direction(state, world) == 0);
  REQUIRE(world.direction == 2);
  REQUIRE(world.sounds.empty());
  world.repeated = kDown;
  REQUIRE(Direction(state, world) == 0);
  REQUIRE(world.direction == 0);
  world.repeated = kDown;
  REQUIRE(Direction(state, world) == 0);
  REQUIRE(world.direction == 1);
  REQUIRE(world.messages.back() == Menu(15));
}

TEST_CASE("F02987 opening the screen returns before drawing panels",
          "[container-transfer][F02987]") {
  World world;
  ScreenState state{.reopen_delay = 3, .capacity_slide = 4};
  std::uint8_t step = 0;
  REQUIRE(transfer::UpdateScreen(step, state, world) == 0);
  REQUIRE(step == 1);
  REQUIRE(state.reopen_delay == 0);
  REQUIRE(state.capacity_slide == 0);
  REQUIRE(world.events[0] == Event{"publish"});
  REQUIRE(world.events[1] == Event{"load-text", 1});
  REQUIRE(world.Logged("store-snapshot"));
  REQUIRE(world.Logged("snapshot-bag"));
  REQUIRE_FALSE(world.Logged("icons"));
}

TEST_CASE("F02987 icons draw once the item text has loaded",
          "[container-transfer][F02987]") {
  World world;
  ScreenState state;
  std::uint8_t step = 0;
  static_cast<void>(transfer::UpdateScreen(step, state, world));
  REQUIRE_FALSE(world.Logged("icons"));
  world.load_result = 1;
  REQUIRE(transfer::UpdateScreen(step, state, world) == 0);
  REQUIRE(step == 2);
  REQUIRE(world.Logged("icons"));
}

TEST_CASE("F02987 direction result 1 places the list title",
          "[container-transfer][F02987]") {
  World world;
  ScreenState state;
  std::uint8_t step = 3;
  REQUIRE(transfer::UpdateScreen(step, state, world) == 0);
  REQUIRE(step == 3);
  world.direction_result = 1;
  REQUIRE(transfer::UpdateScreen(step, state, world) == 0);
  REQUIRE(step == 4);
  REQUIRE(world.placements.front() ==
          transfer::MenuItemPlacement{0, 0xB4, 0x12, 0x8C, 8, Menu(16)});
}

TEST_CASE("F02987 the list opens once the title rows are idle",
          "[container-transfer][F02987]") {
  World world;
  ScreenState state;
  std::uint8_t step = 4;
  world.idle = false;
  REQUIRE(transfer::UpdateScreen(step, state, world) == 0);
  REQUIRE(step == 4);
  world.idle = true;
  REQUIRE(transfer::UpdateScreen(step, state, world) == 0);
  REQUIRE(step == 5);
  REQUIRE(world.Logged("item-list"));
}

TEST_CASE("F02987 direction result 3 closes the screen",
          "[container-transfer][F02987]") {
  World world;
  ScreenState state;
  std::uint8_t step = 3;
  world.direction_result = 3;
  REQUIRE(transfer::UpdateScreen(step, state, world) == 0);
  REQUIRE(step == 6);
}

TEST_CASE("F02987 the list ends into a delayed direction menu",
          "[container-transfer][F02987]") {
  World world;
  ScreenState state;
  std::uint8_t step = 5;
  world.list_result = 1;
  REQUIRE(transfer::UpdateScreen(step, state, world) == 0);
  REQUIRE(step == 2);
  REQUIRE(state.reopen_delay == 5);
  REQUIRE(world.placements.front().text == Raw(35));
}

TEST_CASE("F02987 the direction menu reopens after the delay",
          "[container-transfer][F02987]") {
  World world;
  ScreenState state;
  std::uint8_t step = 5;
  world.list_result = 1;
  bool frames_ok = transfer::UpdateScreen(step, state, world) == 0;
  for (int frame = 0; frame < 5; ++frame) {
    frames_ok = frames_ok && transfer::UpdateScreen(step, state, world) == 0;
  }
  REQUIRE(frames_ok);
  REQUIRE_FALSE(world.Logged("direction-menu"));
  REQUIRE(transfer::UpdateScreen(step, state, world) == 0);
  REQUIRE(world.events.back().name != "direction-menu");
  REQUIRE(world.Count("direction-menu") == 1);
  REQUIRE(step == 3);
}

TEST_CASE("F02987 the list exit closes the screen",
          "[container-transfer][F02987]") {
  World world;
  ScreenState state;
  std::uint8_t step = 5;
  world.list_result = kExit;
  REQUIRE(transfer::UpdateScreen(step, state, world) == 0);
  REQUIRE(step == 6);
}

TEST_CASE("F02987 closing waits for the item text to unload",
          "[container-transfer][F02987]") {
  World world;
  ScreenState state{.result = 2};
  std::uint8_t step = 6;
  REQUIRE(transfer::UpdateScreen(step, state, world) == 0);
  REQUIRE(step == 6);
  REQUIRE(world.Logged("icons"));
}

TEST_CASE("F02987 closing reports an empty container",
          "[container-transfer][F02987]") {
  World world;
  ScreenState state{.result = 2};
  std::uint8_t step = 6;
  world.unload_ready = true;
  REQUIRE(transfer::UpdateScreen(step, state, world) == 0);
  REQUIRE(step == 0);
  REQUIRE(world.Count("rebuild-indices") == 7);
  REQUIRE(world.events.back() == Event{"return", 4});
  REQUIRE(world.Logged("container-empty"));
  REQUIRE(world.events[world.events.size() - 3] == Event{"container-empty", 1});
  REQUIRE_FALSE(world.Logged("icons"));
}

TEST_CASE("F02987 closing a stocked container reports it is not empty",
          "[container-transfer][F02987]") {
  World world;
  world.unload_ready = true;
  world.Of(TransferStorage::kContainer).storage.items.misc[0] = {.id = 1,
                                                                 .count = 1};
  ScreenState state{.result = static_cast<std::uint32_t>(kCancel)};
  std::uint8_t step = 6;
  REQUIRE(transfer::UpdateScreen(step, state, world) == 0);
  REQUIRE(world.events.back() == Event{"return", 1});
  REQUIRE(world.events[world.events.size() - 3] == Event{"container-empty", 0});
}

TEST_CASE("F02987 the pending header slides in with its transfer count",
          "[container-transfer][F02987]") {
  World world;
  ScreenState state;
  std::uint8_t step = 5;
  world.token = kManageToken;
  world.FlagRef(TransferFlag::kTitleVisible) = 1;
  world.FlagRef(TransferFlag::kBrowsingGadgetBag) = 1;
  world.pending[0] = 0x101;
  world.pending[1] = 0x102;
  world.pending[2] = 0x601;
  REQUIRE(transfer::UpdateScreen(step, state, world) == 0);
  REQUIRE(world.FlagRef(TransferFlag::kTitleSlide) == 1);
  REQUIRE(world.placements.size() == 1);
  REQUIRE(world.placements[0] ==
          transfer::MenuItemPlacement{0x1F, 0xB4 + 27, 0x22, 0x8C, 8,
                                      Menu(7 + 0x12 + 1)});
  REQUIRE(world.Logged("counts"));
  REQUIRE(world.events[3] == Event{"counts", 0x1F, 0x30040});
  REQUIRE(world.events.back() == Event{"icons", 0, 0});

  world.pending_limit = 3;
  REQUIRE_THROWS_AS(transfer::UpdateScreen(step, state, world), Refused);
}

namespace {
void PrepareControlsPrompt(World& world, ScreenState& state) {
  std::uint8_t step = 5;
  world.FlagRef(TransferFlag::kControlsMode) = 4;
  world.transferred_misc = 9;
  REQUIRE(transfer::UpdateScreen(step, state, world) == 0);
}
}  // namespace

TEST_CASE("F02987 the controls prompt slides in after refreshing misc",
          "[container-transfer][F02987]") {
  World world;
  ScreenState state;
  PrepareControlsPrompt(world, state);
  REQUIRE(world.events[3] == Event{"refresh-misc"});
  REQUIRE(world.FlagRef(TransferFlag::kControlsSlide) == 1);
  REQUIRE(state.capacity_slide == 1);
  REQUIRE(world.events[4] == Event{"prompt", 0x10 - 27, 0x26});
}

TEST_CASE("F02987 the controls prompt labels its rows",
          "[container-transfer][F02987]") {
  World world;
  ScreenState state;
  PrepareControlsPrompt(world, state);
  REQUIRE(world.texts[0].text == "STATUS");
  REQUIRE(world.texts[0].color == std::optional<std::uint32_t>(0x202020));
  REQUIRE(world.texts[2].text == "END");
  REQUIRE(world.texts[2].color == std::optional<std::uint32_t>(0x404040));
  REQUIRE(world.texts[3].text == "GADGET BAG");
  REQUIRE(world.texts.back().text == "MISC");
}

TEST_CASE("F02987 the controls prompt draws the capacity list",
          "[container-transfer][F02987]") {
  World world;
  ScreenState state;
  PrepareControlsPrompt(world, state);
  REQUIRE(world.Count("capacity") == 7);
  REQUIRE(world.events[world.events.size() - 2] == Event{"capacity", 15, 0});
  REQUIRE(world.events.back() == Event{"capacity", 0x40, 9});

  std::uint8_t step = 5;
  world.FlagRef(TransferFlag::kBrowsingGadgetBag) = 2;
  REQUIRE_THROWS_AS(transfer::UpdateScreen(step, state, world), Refused);
}

TEST_CASE("F02985 initialization selects the browsed storage",
          "[container-transfer][F02985]") {
  World world;
  ItemListState state{.step = 9};
  REQUIRE(transfer::UpdateItemList(2, state, world) == 0);
  REQUIRE(state.step == 1);
  REQUIRE(state.first_entry);
  REQUIRE(world.FlagRef(TransferFlag::kBrowsingGadgetBag) == 1);
  REQUIRE(world.events == std::vector<Event>{{"panel", 1},
                                             {"clear-category-memory"},
                                             {"clear-staged"},
                                             {"select-storages", 1}});
  world.events.clear();
  world.cursor_memory = 1;
  REQUIRE(transfer::UpdateItemList(1, state, world) == 0);
  REQUIRE(world.FlagRef(TransferFlag::kBrowsingGadgetBag) == 0);
  REQUIRE_FALSE(world.Logged("clear-category-memory"));
}

TEST_CASE("F02985 building an empty gadget bag category shows its message",
          "[container-transfer][F02985]") {
  World world;
  ItemListState state{.step = 1};
  world.FlagRef(TransferFlag::kBrowsingGadgetBag) = 1;
  REQUIRE(List(state, world) == 0);
  REQUIRE(state.step == 2);
  REQUIRE(state.category_empty);
  REQUIRE(world.messages.back() == Menu(1 + 27));
  REQUIRE(world.Count("rebuild-indices") == 7);
  REQUIRE_FALSE(world.Logged("info-init"));
  world.token = kManageToken;
  state.step = 1;
  REQUIRE(List(state, world) == 0);
  REQUIRE(world.messages.back() == Menu(1 + 27 + 2));
}

TEST_CASE("F02985 the first build restores the remembered rows",
          "[container-transfer][F02985]") {
  World world;
  world.FlagRef(TransferFlag::kBrowsingGadgetBag) = 1;
  world.token = kWeaponToken;
  world.item_rows = 3;
  ItemListState state{.step = 0, .first_entry = true};
  REQUIRE(List(state, world) == 0);
  REQUIRE_FALSE(state.category_empty);
  REQUIRE_FALSE(state.first_entry);
  REQUIRE(world.events.front() == Event{"info-init"});
  REQUIRE(world.Logged("restore-rows"));
  REQUIRE(world.Count("cursor-memory") == 1);
  REQUIRE(world.events[world.events.size() - 4] ==
          Event{"create-rows", 3, 0x19251});
}

TEST_CASE("F02985 later builds keep the cursor memory without restoring",
          "[container-transfer][F02985]") {
  World world;
  world.FlagRef(TransferFlag::kBrowsingGadgetBag) = 1;
  world.token = kWeaponToken;
  world.item_rows = 3;
  ItemListState state{.step = 0, .first_entry = true};
  static_cast<void>(List(state, world));
  world.events.clear();
  state.step = 1;
  REQUIRE(List(state, world) == 0);
  REQUIRE(world.Count("cursor-memory") == 2);
  REQUIRE_FALSE(world.Logged("restore-rows"));
  REQUIRE(world.cursor_memory == 0);
}

TEST_CASE("F02985 an empty category leaves or cancels",
          "[container-transfer][F02985]") {
  World world;
  ItemListState state{.step = 2, .category_empty = true};
  world.pressed = kTriangle;
  REQUIRE(List(state, world) == kExit);
  world.pressed = kCross;
  REQUIRE(List(state, world) == kCancel);
  world.sounds.clear();
  world.pressed = kSquare;
  REQUIRE(List(state, world) == 0);
  REQUIRE(state.step == 2);
  REQUIRE(world.sounds == std::vector{TransferSound::kInvalid});
}

TEST_CASE("F02985 an empty category with pending rows begins the transfer",
          "[container-transfer][F02985]") {
  World world;
  ItemListState state{.step = 2, .category_empty = true};
  world.pressed = kCross;
  world.pending[0] = 0x101;
  REQUIRE(List(state, world) == 0);
  REQUIRE(state.step == 13);
  REQUIRE(world.token == 0);
  REQUIRE(world.events.back() == Event{"clear-menu", 0});
  state.step = 2;
  world.token = kManageToken;
  REQUIRE(List(state, world) == 0);
  REQUIRE(state.step == 15);
  REQUIRE(world.events.back() == Event{"confirmation", 1});
}

TEST_CASE("F02985 picking a row marks it", "[container-transfer][F02985]") {
  World world;
  ItemListState state{.step = 2};
  world.selected = 1;
  world.item_rows = 4;
  REQUIRE(List(state, world) == 0);
  REQUIRE(state.selected_row == 2);
  REQUIRE(state.step == 6);
  REQUIRE(world.FlagRef(TransferFlag::kControlsMode) == 1);
  REQUIRE(world.Logged("mark-row"));
  REQUIRE(world.events[2] == Event{"mark-row", 1, 8});
  REQUIRE(world.events[3] == Event{"cursor-memory", 1});
  REQUIRE(world.events.back() == Event{"release-rows"});
}

TEST_CASE("F02985 picking a pending row undoes its transfer",
          "[container-transfer][F02985]") {
  World world;
  ItemListState state{.step = 2};
  world.selected = 1;
  world.item_rows = 4;
  world.token = kManageToken;
  REQUIRE(List(state, world) == 0);
  REQUIRE(state.step == 4);
  REQUIRE(world.events ==
          std::vector<Event>{{"undo", 1}, {"refresh-misc"}, {"clear-menu", 0}});
}

TEST_CASE("F02985 Square opens details for detail categories",
          "[container-transfer][F02985]") {
  World world;
  ItemListState state{.step = 2};
  world.pressed = kSquare;
  world.closed_cursor = 0x203;
  REQUIRE(List(state, world) == 0);
  REQUIRE(state.step == 7);
  REQUIRE(world.Logged("clear-icon"));
  REQUIRE(world.events.back() == Event{"details", 0x2040});
  REQUIRE(world.events[world.events.size() - 2] == Event{"clear-icon", 2});
}

TEST_CASE("F02985 Square is refused for misc and waits for idle rows",
          "[container-transfer][F02985]") {
  World world;
  ItemListState state{.step = 2};
  world.pressed = kSquare;
  world.closed_cursor = 0x203;
  world.token = kMiscToken;
  REQUIRE(List(state, world) == 0);
  REQUIRE(state.step == 2);
  REQUIRE(world.sounds.back() == TransferSound::kInvalid);
  REQUIRE(world.FlagRef(TransferFlag::kControlsMode) == 1);

  world.token = kWeaponToken;
  world.idle = false;
  REQUIRE(List(state, world) == 0);
  REQUIRE(state.step == 2);
  REQUIRE(world.FlagRef(TransferFlag::kControlsMode) == 2);
}

TEST_CASE("F02985 R1 switches category and waits for release",
          "[container-transfer][F02985]") {
  World world;
  ItemListState state{.step = 2};
  world.held = kR1;
  world.repeated = kR1;
  REQUIRE(List(state, world) == 0);
  REQUIRE(world.token == kBladeToken);
  REQUIRE(state.step == 3);
  REQUIRE(world.sounds.back() == TransferSound::kChange);
  REQUIRE(world.Logged("close-rows"));
  world.repeated = 0;
  REQUIRE(List(state, world) == 0);
  REQUIRE(state.step == 3);
  REQUIRE(world.token == kBladeToken);
}

TEST_CASE("F02985 releasing R1 rebuilds the switched category once",
          "[container-transfer][F02985]") {
  World world;
  ItemListState state{.step = 2};
  world.held = kR1;
  world.repeated = kR1;
  static_cast<void>(List(state, world));
  world.held = 0;
  world.repeated = 0;
  REQUIRE(List(state, world) == 0);
  REQUIRE(state.step == 2);
  REQUIRE(world.events.back() == Event{"release-rows"});
  REQUIRE(world.Logged("build-rows"));
  REQUIRE(std::ranges::count(world.events, Event{"build-rows", 1}) == 1);
}

TEST_CASE("F02985 L1 switches back and both shoulders keep the category",
          "[container-transfer][F02985]") {
  World world;
  world.token = kWeaponToken;
  ItemListState state{.step = 2};
  world.held = kL1;
  world.repeated = kL1;
  REQUIRE(List(state, world) == 0);
  REQUIRE(world.token == 0);
  world.held = kL1 | kR1;
  world.token = kWeaponToken;
  state.step = 2;
  REQUIRE(List(state, world) == 0);
  REQUIRE(world.token == kWeaponToken);
}

TEST_CASE("F02985 moving a row onto another swaps their order tokens",
          "[container-transfer][F02985]") {
  World world;
  auto& order = world.Of(TransferStorage::kActive).storage.indices;
  order[0] = 1;
  order[1] = 2;
  order[2] = 3;
  ItemListState state{.step = 6, .selected_row = 1};
  world.selected = 2;
  REQUIRE(List(state, world) == 0);
  REQUIRE(order[0] == 3);
  REQUIRE(order[2] == 1);
  REQUIRE(state.step == 4);
}

TEST_CASE("F02985 the move cursor results choose the next step",
          "[container-transfer][F02985]") {
  World world;
  ItemListState state{.step = 6, .selected_row = 1};
  world.selected = 0;
  REQUIRE(List(state, world) == 0);
  REQUIRE(state.step == 8);
  state.step = 6;
  world.selected = -2;
  REQUIRE(List(state, world) == 0);
  REQUIRE(state.step == 0);
  state.step = 6;
  world.selected = -3;
  REQUIRE(List(state, world) == kExit);
  state.step = 6;
  world.selected = 704;
  REQUIRE_THROWS_AS(List(state, world), Refused);
}

namespace {
items::ContainerStorage& PrepareMiscMerge(World& world) {
  auto& active = world.Of(TransferStorage::kActive).storage;
  active.indices[kMiscOrder] = 1;
  active.indices[kMiscOrder + 1] = 2;
  active.items.misc[0] = {.id = 5, .count = 60};
  active.items.misc[1] = {.id = 5, .count = 70};
  world.token = kMiscToken;
  world.selected = 1;
  return active;
}
}  // namespace

TEST_CASE("F02985 moving a misc stack onto the same item merges to 100",
          "[container-transfer][F02985]") {
  World world;
  auto& active = PrepareMiscMerge(world);
  ItemListState state{.step = 6, .selected_row = 1};
  REQUIRE(List(state, world) == 0);
  REQUIRE(active.items.misc[0].count == 100);
  REQUIRE(active.items.misc[1].count == 30);
  REQUIRE(active.indices[kMiscOrder] == 1);
}

TEST_CASE("F02985 merging misc stacks keeps full stacks and empties sources",
          "[container-transfer][F02985]") {
  World world;
  auto& active = PrepareMiscMerge(world);
  active.items.misc[0].count = 100;
  active.items.misc[1].count = 20;
  ItemListState state{.step = 6, .selected_row = 1};
  REQUIRE(List(state, world) == 0);
  REQUIRE(active.items.misc[0].count == 100);
  REQUIRE(active.items.misc[1].count == 20);
  active.items.misc[0].count = 30;
  state.step = 6;
  REQUIRE(List(state, world) == 0);
  REQUIRE(active.items.misc[0].count == 50);
  REQUIRE(active.items.misc[1].id == 0);
}

namespace {
items::ContainerStorage& PrepareStoredWeapon(World& world) {
  auto& active = world.Of(TransferStorage::kActive).storage;
  active.indices[0] = 1;
  active.items.weapons[0] = {.index = 1, .blade = 1, .grip = 1};
  return active;
}
}  // namespace

TEST_CASE("F02985 a stored weapon offers transfer, discard and sort",
          "[container-transfer][F02985]") {
  World world;
  PrepareStoredWeapon(world);
  ItemListState state{.step = 8, .selected_row = 1};
  REQUIRE(List(state, world) == 0);
  REQUIRE(state.step == 9);
  REQUIRE(world.action_menu.has_value());
  if (!world.action_menu) return;
  const auto& menu = *world.action_menu;
  REQUIRE(menu.count == 3);
  REQUIRE(menu.menu == 0x59);
  REQUIRE(menu.texts == std::vector{Menu(0x1F), Menu(0x20), Help(0x354A),
                                    Help(0x354F), Help(0x3586), Help(0x358A)});
  REQUIRE(menu.types == std::vector<std::int32_t>{0, 0, 0, 0});
  REQUIRE(state.action_ids == std::array<std::uint8_t, 8>{0, 1, 3, 5});
  REQUIRE(world.events.front() == Event{"capacity-error", 0, 0});
}

TEST_CASE("F02985 an equipped bag weapon disables transfer and discard",
          "[container-transfer][F02985]") {
  World world;
  auto& active = PrepareStoredWeapon(world);
  active.items.weapons[0].is_equipped = 1;
  world.FlagRef(TransferFlag::kBrowsingGadgetBag) = 1;
  ItemListState state{.step = 8, .selected_row = 1};
  REQUIRE(List(state, world) == 0);
  REQUIRE(world.action_menu.has_value());
  if (!world.action_menu) return;
  REQUIRE(world.action_menu->texts[0] == Menu(0x21));
  REQUIRE(world.action_menu->texts[1] == Raw(0x4A));
  REQUIRE(world.action_menu->texts[3] == Help(kItemHelpBase + 1 + 663));
  REQUIRE(world.action_menu->types == std::vector<std::int32_t>{1, 1, 0, 0});
}

namespace {
items::ContainerStorage& PrepareMiscRow(World& world, std::uint16_t id) {
  auto& active = world.Of(TransferStorage::kActive).storage;
  active.indices[kMiscOrder] = 1;
  active.items.misc[0] = {.id = id, .count = 1};
  world.token = kMiscToken;
  return active;
}
}  // namespace

TEST_CASE("F02985 misc rows block key items", "[container-transfer][F02985]") {
  World world;
  PrepareMiscRow(world, 0x1CA);
  ItemListState state{.step = 8, .selected_row = 1};
  REQUIRE(List(state, world) == 0);
  REQUIRE(world.action_menu.has_value());
  if (!world.action_menu) return;
  const auto& menu = *world.action_menu;
  REQUIRE(menu.count == 4);
  REQUIRE(menu.texts[1] == Raw(0x33));
  REQUIRE(menu.texts[3] == Help(kItemHelpBase + 3 + 663));
  REQUIRE(menu.texts[5] == Help(0x350E));
  REQUIRE(menu.types == std::vector<std::int32_t>{1, 1, 1, 0});
  REQUIRE(state.action_ids == std::array<std::uint8_t, 8>{0, 1, 3, 4, 5});
}

TEST_CASE("F02985 misc rows offer a single discard",
          "[container-transfer][F02985]") {
  World world;
  PrepareMiscRow(world, 5);
  world.copy_result = 1;
  ItemListState state{.step = 8, .selected_row = 1};
  REQUIRE(List(state, world) == 0);
  REQUIRE(world.action_menu.has_value());
  if (!world.action_menu) return;
  REQUIRE(world.action_menu->types == std::vector<std::int32_t>{0, 0, 0, 0});
  REQUIRE(world.action_menu->texts[1] == Menu(0x20));
  REQUIRE(world.Logged("preview-misc"));
  REQUIRE(world.events.back() == Event{"copy-owners", 3, 0});
}

namespace {
ItemListState PrepareChosenActions(World& world) {
  world.Of(TransferStorage::kActive).storage.indices[0] = 4;
  return {.step = 9, .selected_row = 1, .action_ids = {0, 1, 3, 5, 0, 0, 0, 0}};
}
}  // namespace

TEST_CASE("F02985 the transfer action stages the chosen row",
          "[container-transfer][F02985]") {
  World world;
  auto state = PrepareChosenActions(world);
  REQUIRE(List(state, world) == 0);
  REQUIRE_FALSE(world.Logged("fly-out"));
  world.action_result = 0;
  REQUIRE(List(state, world) == 0);
  REQUIRE(state.step == 4);
  REQUIRE(world.Logged("stage"));
  REQUIRE(world.events[world.events.size() - 2] == Event{"stage", 0, 3});
}

TEST_CASE("F02985 the discard and sort actions open their dialogs",
          "[container-transfer][F02985]") {
  World world;
  auto state = PrepareChosenActions(world);
  world.action_result = 1;
  REQUIRE(List(state, world) == 0);
  REQUIRE(state.step == 11);
  REQUIRE(world.events.back() == Event{"discard", 4});
  state.step = 9;
  world.action_result = 2;
  REQUIRE(List(state, world) == 0);
  REQUIRE(state.step == 12);
  REQUIRE(world.events.back() == Event{"sort", 1});
}

TEST_CASE("F02985 leaving the action menu and an unknown choice",
          "[container-transfer][F02985]") {
  World world;
  auto state = PrepareChosenActions(world);
  world.action_result = -2;
  REQUIRE(List(state, world) == 0);
  REQUIRE(state.step == 0);
  state.step = 9;
  world.action_result = -3;
  REQUIRE(List(state, world) == kExit);
  REQUIRE(world.events.back() == Event{"info-dismiss"});
  state.step = 9;
  world.action_result = 7;
  REQUIRE_THROWS_AS(List(state, world), Refused);
}

namespace {
items::ContainerStorage& PrepareParentJump(World& world) {
  auto& active = world.Of(TransferStorage::kActive).storage;
  for (std::uint16_t slot = 0; slot < 20; ++slot) {
    active.items.weapons.at(slot).blade = 1;
    active.indices.at(slot) = static_cast<std::uint16_t>(slot + 1);
  }
  active.items.blades[0] = {.id = 1, .assembled_weapon_index = 20};
  active.indices[kBladeOrder] = 1;
  world.token = kBladeToken;
  return active;
}
}  // namespace

TEST_CASE("F02985 jumping to a parent weapon waits then selects its page",
          "[container-transfer][F02985]") {
  World world;
  PrepareParentJump(world);
  ItemListState state{.step = 10, .selected_row = 1};
  world.idle = false;
  REQUIRE(List(state, world) == 0);
  REQUIRE(state.step == 10);
  world.idle = true;
  REQUIRE(List(state, world) == 0);
  REQUIRE(state.step == 1);
  REQUIRE(world.token == kWeaponToken);
  REQUIRE(world.events.back() == Event{"category-cursor", 0, 7 * 0x100 + 12});
}

TEST_CASE("F02985 jumping to earlier parents selects their page and row",
          "[container-transfer][F02985]") {
  World world;
  auto& active = PrepareParentJump(world);
  ItemListState state{.step = 10, .selected_row = 1};
  active.items.blades[0].assembled_weapon_index = 10;
  REQUIRE(List(state, world) == 0);
  REQUIRE(world.events.back() == Event{"category-cursor", 0, 1 * 0x100 + 8});

  world.token = kBladeToken;
  state.step = 10;
  active.items.blades[0].assembled_weapon_index = 3;
  REQUIRE(List(state, world) == 0);
  REQUIRE(world.events.back() == Event{"category-cursor", 0, 0x200});
}

TEST_CASE("F02985 confirming commits each pending transfer once",
          "[container-transfer][F02985]") {
  World world;
  world.pending[0] = 0x102;
  world.pending[1] = 0x601;
  world.on_rebuild_pending = [&world] {
    std::ranges::shift_left(world.pending, 1);
    world.pending.back() = 0;
  };
  world.confirmation_result = 1;
  ItemListState state{.step = 15};
  REQUIRE(List(state, world) == 0);
  REQUIRE(state.step == 17);
  REQUIRE(world.Count("copy") == 2);
  REQUIRE(world.Logged("copy"));
  REQUIRE(world.events[2] == Event{"copy", 0x11, 1});
  REQUIRE(world.events[3] == Event{"copy-owners", 1, 2});
  REQUIRE(world.events[5] == Event{"copy", 0x16, 0});
  REQUIRE(world.Count("rebuild-indices") == 7);
}

TEST_CASE("F02985 committed transfers finish once the rows are idle",
          "[container-transfer][F02985]") {
  World world;
  ItemListState state{.step = 17};
  world.idle = false;
  REQUIRE(List(state, world) == 0);
  world.idle = true;
  REQUIRE(List(state, world) == 1);
}

TEST_CASE("F02985 confirmation refuses pending lists that never drain",
          "[container-transfer][F02985]") {
  World world;
  world.confirmation_result = 1;
  world.pending[0] = 0x101;
  ItemListState state{.step = 15};
  REQUIRE_THROWS_AS(List(state, world), Refused);
  REQUIRE(world.Count("copy") == 65);
  world.pending[0] = 0x701;
  state.step = 15;
  REQUIRE_THROWS_AS(List(state, world), Refused);

  world.confirmation_result = -1;
  state.step = 15;
  REQUIRE(List(state, world) == 0);
  REQUIRE(state.step == 5);
  world.confirmation_result = kExit;
  state.step = 15;
  REQUIRE(List(state, world) == kExit);
}

TEST_CASE("F02985 the discard dialog returns to the rebuilt list",
          "[container-transfer][F02985]") {
  World world;
  ItemListState state{.step = 11};
  REQUIRE(List(state, world) == 0);
  REQUIRE(state.step == 11);
  world.discard_result = 1;
  REQUIRE(List(state, world) == 0);
  REQUIRE(state.step == 4);
  REQUIRE(world.Logged("info-init"));
}

TEST_CASE("F02985 leaving the sort dialog leaves the list",
          "[container-transfer][F02985]") {
  World world;
  ItemListState state{.step = 12};
  world.sort_result = kExit;
  REQUIRE(List(state, world) == kExit);
  REQUIRE_FALSE(world.Logged("info-init"));
}

TEST_CASE("F02985 an emptied list asks to confirm the transfer",
          "[container-transfer][F02985]") {
  World world;
  ItemListState state{.step = 13};
  world.item_rows = 0;
  world.FlagRef(TransferFlag::kBrowsingGadgetBag) = 1;
  REQUIRE(List(state, world) == 0);
  REQUIRE(state.step == 14);
  REQUIRE(world.messages.back() == Menu(1 + 0x1D));
  REQUIRE(List(state, world) == 0);
  REQUIRE(state.step == 15);
  REQUIRE(world.events.back() == Event{"confirmation", 1});
}

namespace {
namespace live = container_pending_rows_live_tests;
using LiveFixture = live::PendingRowsLiveFixture;

struct DirectionFrame {
  int result = 0;
  u_short selection = 0;
  std::array<std::int16_t, 2> x{};
  std::array<u_char, 2> selected{};
  friend bool operator==(const DirectionFrame&,
                         const DirectionFrame&) = default;
};

constexpr int kDirectionFrames = 40;
constexpr std::size_t kFirstDirectionRow = 10;

void PressForFrame(int frame) {
  constexpr int kDownFrame = 24;
  constexpr int kUpFrame = 27;
  constexpr int kCircleFrame = 31;
  VS::Input::buttonRepeat = frame == kDownFrame ? static_cast<int>(kDown)
                            : frame == kUpFrame ? static_cast<int>(kUp)
                                                : 0;
  VS::Input::buttonsPressed =
      frame == kCircleFrame ? static_cast<int>(kCircle) : 0;
}

template <typename Entry>
std::vector<DirectionFrame> RunDirection(Entry entry) {
  LiveFixture fixture;
  VS::Menu::Framework::ResetLiveMainMenuStatics();
  VS::Menu::Framework::menuMemory.direction_selection = 1;
  std::vector<DirectionFrame> frames;
  frames.push_back(
      {entry(1), VS::Menu::Framework::menuMemory.direction_selection, {}, {}});
  for (int frame = 0; frame < kDirectionFrames; ++frame) {
    fixture.Frame();
    PressForFrame(frame);
    DirectionFrame record{};
    record.result = entry(0);
    record.selection = VS::Menu::Framework::menuMemory.direction_selection;
    for (std::size_t row = 0; row < 2; ++row) {
      const auto& item = fixture.rows.at(kFirstDirectionRow + row);
      record.x.at(row) = item.x;
      record.selected.at(row) = item.selected;
    }
    frames.push_back(record);
  }
  return frames;
}

std::array<char*, 2> WeaponStrings(VS::Items::ContainerData& owner,
                                   void (*entry)(VS::Items::ContainerData*,
                                                 char**, int*, char*, int),
                                   int& type, std::span<char> buffer) {
  std::array<char*, 2> strings{};
  entry(&owner, strings.data(), &type, buffer.data(), 0);
  return strings;
}

using RowEntry = void (*)(VS::Items::ContainerData*, char**, int*, char*, int);
constexpr auto kRowBytes = VS::Menu::Framework::kRowDescriptionBufferBytes;
using RowBuffer = std::array<char, kRowBytes>;

void RequireRowTextInBuffer(VS::Items::ContainerData& owner, RowEntry entry) {
  RowBuffer buffer{};
  int type = 0;
  const auto strings = WeaponStrings(owner, entry, type, buffer);
  REQUIRE(type != 0);
  REQUIRE(strings[0] != nullptr);
  REQUIRE(strings[1] >= buffer.data());
  REQUIRE(strings[1] < buffer.data() + buffer.size());
  REQUIRE(buffer.front() != 0);
}

void RequireRowRepeats(VS::Items::ContainerData& owner, RowEntry entry) {
  RowBuffer buffer{};
  RowBuffer again{};
  int type = 0;
  int again_type = 0;
  const auto strings = WeaponStrings(owner, entry, type, buffer);
  const auto repeated = WeaponStrings(owner, entry, again_type, again);
  REQUIRE(again_type == type);
  REQUIRE(repeated[0] == strings[0]);
  REQUIRE(again == buffer);
  REQUIRE(repeated[1] - again.data() == strings[1] - buffer.data());
}

void CommitRows(VS::Items::ContainerData& owner) {
  rows_data::Records source;
  rows_data::Populate(source);
  owner.weapons[0] = source.storage.items.weapons[0];
  owner.blades[0] = source.storage.items.blades[0];
  owner.grips[0] = source.storage.items.grips[0];
  owner.shields[0] = source.storage.items.shields[0];
}
}  // namespace

TEST_CASE("F02951 F02952 live rows store their text in the caller buffer",
          "[container-transfer-live][F02951][F02952]") {
  LiveFixture fixture;
  auto& owner = fixture.containers.storage->container.items;
  CommitRows(owner);
  for (const RowEntry entry :
       {transfer::LiveBuildWeaponRow, transfer::LiveBuildShieldRow}) {
    RequireRowTextInBuffer(owner, entry);
    RequireRowRepeats(owner, entry);
  }
  int type = 0;
  RowBuffer buffer{};
  REQUIRE(WeaponStrings(owner, transfer::LiveBuildWeaponRow, type, buffer)[0] ==
          owner.weapons[0].name.data());
}

TEST_CASE("F02951 F02952 live rows refuse missing outputs and records",
          "[container-transfer-live][F02951][F02952]") {
  LiveFixture fixture;
  auto& owner = fixture.containers.storage->container.items;
  int type = -1;
  std::array<char*, 2> strings{};
  std::array<char, VS::Menu::Framework::kRowDescriptionBufferBytes> buffer{};
  transfer::LiveBuildWeaponRow(nullptr, strings.data(), &type, buffer.data(),
                               0);
  transfer::LiveBuildShieldRow(&owner, nullptr, &type, buffer.data(), 0);
  transfer::LiveBuildWeaponRow(&owner, strings.data(), &type, buffer.data(),
                               static_cast<int>(std::size(owner.weapons)));
  transfer::LiveBuildShieldRow(&owner, strings.data(), &type, buffer.data(),
                               -1);
  REQUIRE(type == -1);
  REQUIRE(strings == std::array<char*, 2>{});
}

TEST_CASE("F02986 live direction menu replays after its MENUD reset",
          "[container-transfer-live][F02986]") {
  transfer::ResetLiveContainerTransferStatics();
  const auto expected = RunDirection(transfer::LiveUpdateTransferDirection);
  transfer::ResetLiveContainerTransferStatics();
  const auto actual = RunDirection(transfer::LiveUpdateTransferDirection);
  transfer::ResetLiveContainerTransferStatics();
  REQUIRE(actual == expected);
  REQUIRE(std::ranges::count_if(actual, [](const DirectionFrame& frame) {
            return frame.result > 0;
          }) == 1);
}

TEST_CASE("F02986 live reset restores the first-entry direction reset",
          "[container-transfer-live][F02986]") {
  LiveFixture fixture;
  live::equipment::ValueRestore<std::uint8_t> memory{
      VS::Input::settings.cursor_memory};
  VS::Input::settings.cursor_memory = 0;
  transfer::ResetLiveContainerTransferStatics();
  VS::Menu::Framework::menuMemory.direction_selection = 2;
  REQUIRE(transfer::LiveUpdateTransferDirection(1) == 0);
  REQUIRE(VS::Menu::Framework::menuMemory.direction_selection == 0);
  VS::Menu::Framework::menuMemory.direction_selection = 2;
  REQUIRE(transfer::LiveUpdateTransferDirection(1) == 0);
  REQUIRE(VS::Menu::Framework::menuMemory.direction_selection == 2);
  transfer::ResetLiveContainerTransferStatics();
  REQUIRE(transfer::LiveUpdateTransferDirection(1) == 0);
  REQUIRE(VS::Menu::Framework::menuMemory.direction_selection == 0);
  transfer::ResetLiveContainerTransferStatics();
}

struct ListFrame {
  int result = 0;
  std::vector<std::array<int, 4>> rows;
  friend bool operator==(const ListFrame&, const ListFrame&) = default;
};

template <typename Entry>
std::vector<ListFrame> RunItemList(Entry entry) {
  constexpr int kListFrames = 12;
  constexpr std::size_t kOutsideBytes = 4096;
  constexpr std::byte kCanary{0xA5};
  LiveFixture fixture;
  std::vector<std::byte> backing(live::kHeapBytes + kOutsideBytes, kCanary);
  VS::Core::Memory::ScopedLiveGameHeap bounded{
      std::span(backing).first(live::kHeapBytes)};
  fixture.AddMisc();
  std::vector<ListFrame> frames;
  frames.push_back({entry(1), {}});
  VS::Menu::Framework::menuMemory.container_cursors.category = kMiscToken;
  const auto guard_before = VS::Core::Memory::DamagedLiveHeapGuardCount();
  for (int frame = 0; frame < kListFrames; ++frame) {
    ListFrame record{entry(0), {}};
    for (const auto& row : fixture.rows) {
      record.rows.push_back(
          {static_cast<int>(row.state), row.x, row.y, row.selected});
    }
    frames.push_back(record);
    fixture.Frame();
  }
  CHECK(VS::Core::Memory::DamagedLiveHeapGuardCount() == guard_before);
  CHECK(std::ranges::all_of(std::span(backing).subspan(live::kHeapBytes),
                            [](std::byte value) { return value == kCanary; }));
  return frames;
}

TEST_CASE("F02985 live item list replays after its MENUD reset",
          "[container-transfer-live][F02985]") {
  transfer::ResetLiveContainerTransferStatics();
  const auto expected = RunItemList(transfer::LiveUpdateItemList);
  transfer::ResetLiveContainerTransferStatics();
  const auto actual = RunItemList(transfer::LiveUpdateItemList);
  transfer::ResetLiveContainerTransferStatics();
  REQUIRE(actual == expected);
  REQUIRE(std::ranges::any_of(actual.back().rows, [](const auto& row) {
    return row[0] != static_cast<int>(VS::Menu::Framework::RowState::kInactive);
  }));
}
TEST_CASE("F02985 F02987 live entries refuse missing storage and steps",
          "[container-transfer-live][F02985][F02987]") {
  LiveFixture fixture;
  transfer::ResetLiveContainerTransferStatics();
  live::equipment::ValueRestore<Menu_Container_InventoryStorage*> storage{
      VS::Menu::Framework::inventoryStorage};
  VS::Menu::Framework::inventoryStorage = nullptr;
  REQUIRE(transfer::LiveUpdateItemList(1) == 0);
  REQUIRE(transfer::LiveUpdateScreen(nullptr) == 0);
  live::equipment::ValueRestore<VS::Items::Container*> active{
      Menu_Container_containerData};
  u_char step = 6;
  Menu_Container_containerData = nullptr;
  REQUIRE(transfer::LiveUpdateScreen(&step) == 0);
  REQUIRE(step == 6);
  transfer::ResetLiveContainerTransferStatics();
}
