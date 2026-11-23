#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <span>
#include <stdexcept>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Content/DiscLayout.hpp"
#include "Menu/EquipmentDetailControllersLiveFixture.hpp"
#include "Menu/OverlayLoadFixture.hpp"
#include "Menu/Status/ComponentDescriptionsLive.hpp"
#include "Menu/StatusComponentsWorld.hpp"

using VS::Content::disc::Locate;

namespace {
namespace status = VS::Menu::Status;
namespace fw = VS::Menu::Framework;
namespace data = vs_test::status_components;
using PartEntry = void (*)(VS::Items::EquipmentPart*, char*[], int*);
constexpr auto kPartAddresses = std::to_array<unsigned>({
    0x80102F64U,
    0x80103080U,
    0x80103118U,
});
constexpr auto kPartEntries = std::to_array<PartEntry>({
    &status::FormatLiveBladeDescription,
    &status::FormatLiveGripDescription,
    &status::CopyLiveGemDescription,
});
constexpr int kWeaponStrength = 17;
constexpr int kComponentRow = 21;
constexpr std::size_t kAccessoryHelp = 641;
constexpr std::size_t kMaterialArgument = 9;
constexpr std::size_t kGripArgument = 5;

struct LiveFixture : equipment_detail_tests::LiveFixture {
  equipment_detail_tests::ArrayRestore<
      std::uint8_t, std::size(VS::Menu::Framework::weaponHands)>
      saved_hands{VS::Menu::Framework::weaponHands};
  data::World text;

  LiveFixture() {
    std::memcpy(help.data(), text.help.data(), text.help.size());
    std::memcpy(names.data(), text.names.data(), text.names.size());
    std::ranges::copy(text.hands, VS::Menu::Framework::weaponHands);
    VS::Battle::stringContext = {};
  }
  char* HelpEntry(std::size_t index) {
    return reinterpret_cast<char*>(&help[help[index]]);
  }
  char* NoGems() {
    return reinterpret_cast<char*>(&help[fw::item_text::kNoGemsOffset]);
  }
};

template <typename Entry>
Entry ModuleEntry(unsigned address, Entry expected) {
  auto* resolved = VS::Content::ResolveModuleAddress(
      VS::Content::ModuleSymbols(), "MENU/MENU4.PRG", address);
  CHECK(resolved == reinterpret_cast<void*>(expected));
  if (resolved == nullptr) {
    throw std::runtime_error("missing status component module entry");
  }
  return reinterpret_cast<Entry>(resolved);
}
}  // namespace

TEST_CASE("Status part module entries preserve name and row outputs",
          "[status-components-live][F02647][F02648][F02649]") {
  LiveFixture fixture;
  const std::size_t kind = GENERATE(0U, 1U, 2U);
  const std::array parts{data::Blade(),
                         VS::Items::EquipmentPart{.id = 101, .category = 4},
                         VS::Items::EquipmentPart{.id = 261}};
  auto part = parts[kind];
  const std::array<std::size_t, 3> help_indices{16, 95,
                                                fw::item_text::kGemFeldspar};
  std::array<char, 2> name{};
  std::array<char*, 2> output{name.data(), nullptr};
  int row_type = 0x1234;
  ModuleEntry(kPartAddresses[kind], kPartEntries[kind])(&part, output.data(),
                                                        &row_type);
  CHECK(output[0] == name.data());
  REQUIRE(output[1] == fixture.description.data());
  CHECK(row_type == 0x1234);
  CHECK(output[1][0] == fixture.HelpEntry(help_indices[kind])[0]);
  if (kind == 0) {
    CHECK(VS::Battle::stringContext.strings[kMaterialArgument] ==
          fixture.names[parts[kind].material + fw::item_text::kWoodName - 1]
              .data());
  } else if (kind == 1) {
    CHECK(VS::Battle::stringContext.strings[kGripArgument] ==
          fixture.HelpEntry(parts[kind].category + fw::item_text::kShortGrip -
                            1));
  } else {
    CHECK(std::equal(fixture.description.begin(), fixture.description.end(),
                     fixture.HelpEntry(help_indices[kind])));
  }
}

TEST_CASE("Status weapon module entry publishes real blade stats and highlight",
          "[status-components-live][F02650]") {
  LiveFixture fixture;
  auto typed = data::Weapon();
  typed.blade.strength = kWeaponStrength;
  auto weapon = typed;
  auto entry = ModuleEntry(0x8010317C, &status::SelectLiveWeaponComponent);
  CHECK(entry(1, &weapon) == fixture.description.data());
  CHECK(VS::Menu::Framework::strIntAgi[0] == kWeaponStrength);
  CHECK(fixture.rows[kComponentRow].selected == 1);
  CHECK(weapon == typed);
}

TEST_CASE("Status shield module entry clears an empty socket but not its owner",
          "[status-components-live][F02651]") {
  LiveFixture fixture;
  VS::Items::ShieldLoadout shield{};
  VS::Menu::Framework::equipmentStats[0] = 37;
  auto entry = ModuleEntry(0x801032C4, &status::SelectLiveShieldComponent);
  CHECK(entry(1, &shield) == fixture.NoGems());
  CHECK(VS::Menu::Framework::equipmentStats[0] == 0);
  CHECK(fixture.rows[kComponentRow].selected == 1);
  CHECK(VS::Menu::Framework::stringBuf == fixture.description.data());
}

TEST_CASE("Status armor and accessory module entries use real item text",
          "[status-components-live][F02652][F02653]") {
  LiveFixture fixture;
  VS::Items::ArmorLoadout typed_armor{};
  typed_armor.armor = {.id = 145, .category = 2, .material = 3};
  auto armor = typed_armor;
  CHECK(ModuleEntry(0x801033A4, &status::LiveArmorDescription)(&armor) ==
        fixture.description.data());
  VS::Items::AccessorySpawnLoadout typed_accessory{};
  typed_accessory.accessory = {.id = 254, .category = 7, .strength = -128};
  auto accessory = typed_accessory;
  const auto buffer = fixture.description;
  CHECK(ModuleEntry(0x801033D4, &status::LiveAccessoryDescription)(
            &accessory) == fixture.HelpEntry(kAccessoryHelp));
  CHECK(fixture.description == buffer);
  CHECK(accessory == typed_accessory);
}

TEST_CASE("Empty live accessory refuses without buffer context or row writes",
          "[status-components-live][F02653]") {
  LiveFixture fixture;
  VS::Items::AccessorySpawnLoadout accessory{};
  fixture.description.fill('Q');
  VS::Battle::stringContext.strings[0] = fixture.description.data();
  const auto buffer = fixture.description;
  const auto arguments = VS::Battle::stringContext;
  const auto rows =
      std::bit_cast<std::array<std::byte, sizeof(fixture.rows)>>(fixture.rows);
  CHECK(status::LiveAccessoryDescription(&accessory) == nullptr);
  CHECK(fixture.description == buffer);
  CHECK(std::equal(std::begin(arguments.strings), std::end(arguments.strings),
                   std::begin(VS::Battle::stringContext.strings)));
  CHECK((std::bit_cast<std::array<std::byte, sizeof(fixture.rows)>>(
             fixture.rows) == rows));
}

TEST_CASE("Status selectable module entry reads the current stat page",
          "[status-components-live][F02654]") {
  LiveFixture fixture;
  const auto entry =
      ModuleEntry(0x8010341C, &status::IsLiveDetailElementSelectable);
  VS::Menu::Framework::itemStatPage = fw::stat_page::kType;
  CHECK(entry(5, 0xFFFF) == 0);
  VS::Menu::Framework::itemStatPage = fw::stat_page::kClass;
  CHECK(entry(5, 0) == 1);
  CHECK(entry(12, 3) == 1);
  CHECK(entry(13, 3) == 0);
}

TEST_CASE("MENU4 completion preserves borrowed component text and row owners",
          "[status-components-live][menu-overlay-reload]") {
  LiveFixture fixture;
  auto weapon = data::Weapon();
  REQUIRE(status::SelectLiveWeaponComponent(1, &weapon) ==
          fixture.description.data());
  const auto buffer = fixture.description;
  menu_overlay_tests::Load load(
      5, {Locate("MENU/MENU4.PRG").lba, Locate("MENU/MENU4.PRG").size});
  CHECK(load.Complete() == 1);
  CHECK(VS::Menu::Framework::itemHelp == fixture.help.data());
  CHECK(VS::Menu::Framework::stringBuf == fixture.description.data());
  CHECK(VS::Menu::Framework::menuItems == fixture.rows.data());
  CHECK(fixture.description == buffer);
}
