#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Items/ItemRecords.hpp"
#include "Menu/Framework/ItemRows.hpp"
#include "Menu/Status/ComponentDescriptions.hpp"
#include "Menu/StatusComponentsWorld.hpp"

namespace {
namespace fixture = vs_test::status_components;
namespace status = VS::Menu::Status;
namespace Framework = VS::Menu::Framework;
using fixture::World;
}  // namespace

TEST_CASE("F02647 blade description expands material category damage and hands",
          "[status-components][F02647]") {
  World world;
  const auto before = world.buffer;
  const auto blade = fixture::Blade();
  const auto result =
      status::FormatBladeDescription(world, blade, world.buffer);
  REQUIRE(result);
  CHECK(world.Address(result) == fixture::kBufferAddress);
  CHECK(world.arguments.strings[9].data() ==
        world.names.data() + std::size_t{259} * 24);
  CHECK(world.arguments.strings[8].data() == world.EntryText(407).data());
  CHECK(world.arguments.strings[7].data() == world.EntryText(411).data());
  CHECK(world.arguments.strings[6].data() == world.EntryText(644).data());
  CHECK(world.buffer[0] == world.EntryText(16)[0]);
  CHECK(world.buffer.back() == before.back());
  CHECK(world.string_slots == std::vector<std::size_t>{9, 8, 7, 6});
}

TEST_CASE("F02647 missing hand category refuses without publishing text",
          "[status-components][F02647]") {
  World world;
  auto blade = fixture::Blade();
  const auto before = world.buffer;
  const auto strings = world.arguments.strings;
  for (const auto category : {0, 11, 255}) {
    blade.category = static_cast<std::uint8_t>(category);
    CHECK_FALSE(status::FormatBladeDescription(world, blade, world.buffer));
  }
  CHECK(world.buffer == before);
  CHECK(world.arguments.strings[9].data() == strings[9].data());
  CHECK(world.refusals.size() == 3);
}

TEST_CASE("F02647 malformed expansion and short destination are refused",
          "[status-components][F02647]") {
  World world;
  const auto before = world.buffer;
  auto blade = fixture::Blade();
  CHECK_FALSE(status::FormatBladeDescription(world, blade,
                                             std::span(world.buffer).first(2)));
  world.help[Framework::item_text::kWeaponDescTemplateOffset * 2 + 1] = 10;
  CHECK_FALSE(status::FormatBladeDescription(world, blade, world.buffer));
  CHECK(world.buffer == before);
  CHECK(world.refusals.size() == 2);
}

TEST_CASE("F02648 grip uses full unsigned category and id-minus-six entry",
          "[status-components][F02648]") {
  World world;
  VS::Items::EquipmentPart grip{.id = 701, .category = 255};
  const auto untouched = world.arguments.strings[6].data();
  const auto result = status::FormatGripDescription(world, grip, world.buffer);
  REQUIRE(result);
  CHECK(world.arguments.strings[5].data() == world.EntryText(895).data());
  CHECK(world.arguments.strings[6].data() == untouched);
  CHECK(world.buffer[0] == world.EntryText(695)[0]);
  CHECK(world.buffer[1] == world.EntryText(695)[1]);
  CHECK(world.refusals.empty());
  CHECK(world.string_slots == std::vector<std::size_t>{5});
}

TEST_CASE("F02648 negative entry and truncated help refuse without writes",
          "[status-components][F02648]") {
  World world;
  const auto before = world.buffer;
  VS::Items::EquipmentPart grip{.id = 5, .category = 1};
  CHECK_FALSE(status::FormatGripDescription(world, grip, world.buffer));
  world.help_extent = 2;
  grip.id = 101;
  CHECK_FALSE(status::FormatGripDescription(world, grip, world.buffer));
  CHECK(world.buffer == before);
  CHECK(world.refusals.size() == 2);
}

TEST_CASE("F02649 gem copies exactly 96 bytes including bytes after terminator",
          "[status-components][F02649]") {
  World world;
  VS::Items::EquipmentPart gem{.id = 322};
  std::array<std::uint8_t, 98> destination{};
  destination.fill(0x93);
  const auto result = status::CopyGemDescription(
      world, gem, std::span(destination).subspan(1, 96));
  REQUIRE(result);
  CHECK(std::ranges::equal(std::span(destination).subspan(1, 96),
                           world.EntryText(182).first(96)));
  CHECK(destination.front() == 0x93);
  CHECK(destination.back() == 0x93);
  CHECK(world.refusals.empty());
}

TEST_CASE("F02649 short copy or invalid help offset leaves buffer unchanged",
          "[status-components][F02649]") {
  World world;
  VS::Items::EquipmentPart gem{.id = 261};
  const auto before = world.buffer;
  CHECK_FALSE(status::CopyGemDescription(world, gem,
                                         std::span(world.buffer).first(95)));
  world.Entry(121, world.help.size() / 2 - 47);
  CHECK_FALSE(status::CopyGemDescription(world, gem, world.buffer));
  gem.id = 139;
  CHECK_FALSE(status::CopyGemDescription(world, gem, world.buffer));
  CHECK(world.buffer == before);
  CHECK(world.refusals.size() == 3);
}

TEST_CASE("F02650 weapon parts format before stats and highlight the right row",
          "[status-components][F02650]") {
  for (const int row : {1, 2, 3, 5}) {
    World world;
    const auto weapon = fixture::Weapon();
    world.origin = &weapon;
    auto context = world.Context();
    const auto result = status::SelectWeaponComponent(context, row, weapon);
    REQUIRE(result);
    REQUIRE(world.events.size() > 5);
    CHECK(world.events.front() == (row < 3 ? row : 3));
    CHECK(world.events[world.events.size() - 2] == 6);
    CHECK(world.events.back() == row + 20);
    CHECK(world.selected[static_cast<std::size_t>(row + 20)] == 1);
    CHECK(world.refusals.empty());
  }
}

TEST_CASE("F02650 empty socket hides bars before clearing stats",
          "[status-components][F02650]") {
  World world;
  const auto weapon = fixture::Weapon();
  auto context = world.Context();
  const auto result = status::SelectWeaponComponent(context, 4, weapon);
  CHECK(world.Address(result) ==
        fixture::kHelpAddress + Framework::item_text::kNoGemsOffset * 2);
  CHECK(world.events == std::vector<std::uint32_t>{4, 8, 5, 6, 24});
  CHECK(world.selected[24] == 1);
}

TEST_CASE("F02650 whole weapon and default rows retain original boundaries",
          "[status-components][F02650]") {
  for (const int row : {0, -20, -1, 6, 19}) {
    World world;
    const auto weapon = fixture::Weapon();
    auto context = world.Context();
    const auto result = status::SelectWeaponComponent(context, row, weapon);
    REQUIRE(result);
    CHECK(world.events ==
          std::vector<std::uint32_t>{6, static_cast<std::uint32_t>(row + 20)});
    CHECK(world.selected[static_cast<std::size_t>(row + 20)] == 1);
  }
}

TEST_CASE("F02650 invalid or missing row storage produces an explicit refusal",
          "[status-components][F02650]") {
  World world;
  const auto weapon = fixture::Weapon();
  auto context = world.Context();
  for (const int row : {-21, 20, std::numeric_limits<int>::max()}) {
    CHECK_FALSE(status::SelectWeaponComponent(context, row, weapon));
  }
  CHECK(world.events.empty());
  world.rows_available = false;
  CHECK_FALSE(status::SelectWeaponComponent(context, -1, weapon));
  CHECK(world.refusals.size() == 4);
}

TEST_CASE("F02651 shield has three selectable gem slots not four",
          "[status-components][F02651]") {
  for (const int row : {1, 2, 3, 4}) {
    World world;
    VS::Items::ShieldLoadout shield{};
    for (auto& gem : shield.gems) {
      gem.id = 261;
    }
    world.origin = &shield;
    auto context = world.Context();
    const auto result = status::SelectShieldComponent(context, row, shield);
    REQUIRE(result);
    CHECK(world.events.front() == (row == 4 ? 6 : 3));
    CHECK(world.events.back() == row + 20);
    CHECK(world.selected[static_cast<std::size_t>(row + 20)] == 1);
  }
}

TEST_CASE("F02651 empty shield socket resets but negative row only highlights",
          "[status-components][F02651]") {
  World world;
  VS::Items::ShieldLoadout shield{};
  auto context = world.Context();
  CHECK(status::SelectShieldComponent(context, 3, shield));
  CHECK(world.events == std::vector<std::uint32_t>{4, 8, 5, 6, 23});
  world.events.clear();
  CHECK(status::SelectShieldComponent(context, -1, shield));
  CHECK(world.events == std::vector<std::uint32_t>{6, 19});
  CHECK_FALSE(status::SelectShieldComponent(context, 20, shield));
}

TEST_CASE("F02652 armor returns buffer text or borrowed accessory help",
          "[status-components][F02652]") {
  World world;
  VS::Items::ArmorLoadout armor{};
  armor.armor = {.id = 145, .category = 2, .material = 3};
  CHECK(world.Address(status::ArmorDescription(world, armor, world.buffer)) ==
        fixture::kBufferAddress);
  armor.armor = {.id = 223, .category = 7};
  CHECK(world.Address(status::ArmorDescription(world, armor, world.buffer)) ==
        world.Address(world.EntryText(610)));
  CHECK(world.events.empty());
  CHECK(world.refusals.empty());
}

TEST_CASE("F02652 unavailable shared item text is refused",
          "[status-components][F02652]") {
  World world;
  VS::Items::ArmorLoadout armor{};
  armor.armor = {.id = 223, .category = 7};
  world.help_extent = 0;
  CHECK_FALSE(status::ArmorDescription(world, armor, world.buffer));
  CHECK_FALSE(world.refusals.empty());
}

TEST_CASE("F02653 accessory uses inventory conversion and preserves source",
          "[status-components][F02653]") {
  World world;
  VS::Items::AccessorySpawnLoadout accessory{};
  accessory.accessory = {.id = 254, .category = 7, .strength = -128};
  const auto before = accessory;
  const auto buffer = world.buffer;
  CHECK(world.Address(
            status::AccessoryDescription(world, accessory, world.buffer)) ==
        world.Address(world.EntryText(641)));
  CHECK(accessory == before);
  CHECK(world.buffer == buffer);
  CHECK(world.refusals.empty());
}

TEST_CASE("F02653 empty accessory refuses stale stack dependent lookup",
          "[status-components][F02653]") {
  World world;
  VS::Items::AccessorySpawnLoadout accessory{};
  const auto before = world.buffer;
  CHECK_FALSE(status::AccessoryDescription(world, accessory, world.buffer));
  CHECK(world.buffer == before);
  CHECK(world.refusals.size() == 1);
  CHECK(world.events.empty());
}

TEST_CASE("F02654 flags and page independently gate detail elements",
          "[status-components][F02654]") {
  CHECK(status::IsDetailElementSelectable(0, 0x80000020U, 0));
  CHECK_FALSE(status::IsDetailElementSelectable(1, 0x20, 0));
  CHECK(status::IsDetailElementSelectable(1, 0x40, 0));
  CHECK_FALSE(status::IsDetailElementSelectable(5, 0xFFFF, 2));
  CHECK(status::IsDetailElementSelectable(7, 0, 255));
  CHECK(status::IsDetailElementSelectable(8, 0, 1));
  CHECK_FALSE(status::IsDetailElementSelectable(8, 0, 0));
  CHECK(status::IsDetailElementSelectable(17, 0x10, 0));
  CHECK_FALSE(status::IsDetailElementSelectable(16, 0xEF, 0));
}

TEST_CASE(
    "F02654 inclusive socket limit ignores high bits and rejects outsiders",
    "[status-components][F02654]") {
  for (int maximum = 0; maximum < 16; ++maximum) {
    for (int element = 9; element <= 15; ++element) {
      CHECK(status::IsDetailElementSelectable(
                element, 0xFFFFFF00U | static_cast<std::uint32_t>(maximum),
                2) == (element - 9 <= maximum));
    }
  }
  for (const int element : {2, 3, 4, 18, 19, 20}) {
    CHECK(status::IsDetailElementSelectable(element, 0, 2));
  }
  for (const int element : {-1, 21, std::numeric_limits<int>::min(),
                            std::numeric_limits<int>::max()}) {
    CHECK_FALSE(status::IsDetailElementSelectable(element, 0xFFFFFFFFU, 1));
  }
}
