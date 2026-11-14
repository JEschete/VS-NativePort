#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

#include <catch2/catch_test_macros.hpp>

#include "Menu/Inventory/InventorySortingLive.hpp"
#include "Menu/InventorySortingLiveFixture.hpp"
#include "Menu/SortingKeysFixture.hpp"

extern "C" {
#include "Boot/OverlayReset.h"
}

namespace {
namespace sorting = VS::Menu::Inventory;
namespace fixture = sorting::sorting_test;
namespace items = VS::Items;
constexpr int kWeapon = static_cast<int>(items::ItemCategory::kWeapon);
constexpr int kBlade = static_cast<int>(items::ItemCategory::kBlade);
constexpr int kGrip = static_cast<int>(items::ItemCategory::kGrip);
constexpr int kArmor = static_cast<int>(items::ItemCategory::kArmor);
constexpr int kGem = static_cast<int>(items::ItemCategory::kGem);
constexpr int kStrength = 8;
constexpr int kMaterial = 1;
constexpr std::int8_t kLow = -128;
constexpr std::int8_t kHigh = 127;
constexpr std::uint8_t kFlaggedGem = items::kGemSlots + 128;
constexpr std::uint8_t kFullByte = 255;
constexpr std::uint8_t kLowMaterial = 2;
constexpr std::uint8_t kHighMaterial = 9;
constexpr std::int8_t kReserved = 91;
constexpr std::uint16_t kWide = 65535;
using RecordBytes = std::array<std::byte, sizeof(Items_Inventory)>;
}  // namespace

TEST_CASE("F02583 checked Live uses real weapon stats and fixed byte orders",
          "[inventory-sorting-live][F02583]") {
  fixture::LiveFixture state;
  auto& records = Audio_Seq_unusedInventory;
  auto& order = Audio_Seq_unusedInventoryIndices.weapons;
  records.weapons[0].blade = 1;
  std::span(records.weapons).back().blade = items::kBladeSlots;
  records.blades[0].material = kLowMaterial;
  std::span(records.blades).back().material = kHighMaterial;
  order[0] = items::kWeaponSlots;
  order[1] = 1;
  order[2] = items::kWeaponSlots;
  Menu_Inventory_inventoryIndices[kWeapon] = nullptr;
  const auto before = std::bit_cast<RecordBytes>(records);
  sorting::SortLiveWeapons(kMaterial);
  CHECK(order[0] == 1);
  CHECK(order[1] == items::kWeaponSlots);
  CHECK(order[2] == items::kWeaponSlots);
  CHECK(order[3] == 0);
  CHECK(std::bit_cast<RecordBytes>(records) == before);
}

TEST_CASE(
    "F02585 checked Live folds flagged inventory gems and signed attributes",
    "[inventory-sorting-live][F02585]") {
  fixture::LiveFixture state;
  auto& records = Audio_Seq_unusedInventory;
  auto& order = Audio_Seq_unusedInventoryIndices.shields;
  records.shields[0].base.strength = kHigh;
  auto& last = std::span(records.shields).back();
  last.base.strength = kHigh;
  last.gems[0] = kFlaggedGem;
  std::span(records.gems).back().strength = kLow;
  order[0] = items::kShieldSlots;
  order[1] = 1;
  order[2] = items::kShieldSlots;
  sorting::SortLiveShields(kStrength);
  CHECK(order[0] == 1);
  CHECK(order[1] == items::kShieldSlots);
  CHECK(order[2] == items::kShieldSlots);
  CHECK(last.gems[0] == kFlaggedGem);
}

TEST_CASE("F02587 checked Live keeps full unsigned misc counts and last slot",
          "[inventory-sorting-live][F02587]") {
  fixture::LiveFixture state;
  auto& records = Audio_Seq_unusedInventory;
  auto& order = Audio_Seq_unusedInventoryIndices.misc;
  records.misc[0].id = 0;
  records.misc[0].count = 1;
  auto& last = std::span(records.misc).back();
  last.id = kWide;
  last.count = std::bit_cast<char>(kFullByte);
  order[0] = 1;
  order[1] = items::kMiscSlots;
  order[2] = items::kMiscSlots;
  sorting::SortLiveMisc(kMaterial);
  CHECK(order[0] == items::kMiscSlots);
  CHECK(order[1] == items::kMiscSlots);
  CHECK(order[2] == 1);
  sorting::SortLiveMisc(0);
  CHECK(order[0] == 1);
  CHECK(order[1] == items::kMiscSlots);
  CHECK(static_cast<std::uint8_t>(last.count) == kFullByte);
}

TEST_CASE(
    "F02589 checked Live preserves absent fields and reads current records",
    "[inventory-sorting-live][F02589]") {
  fixture::LiveFixture state;
  auto& blade = std::span(Audio_Seq_unusedInventory.blades).back();
  blade.id = kFullByte;
  blade.strength = kLow;
  blade.material = kWide;
  Items_EquipmentPartStats target{};
  items::EquipmentPart initial{};
  initial.types.fill(kReserved);
  initial.gem_effects = kFullByte;
  items::Live::Commit(target, initial);
  sorting::CopyLiveEquipmentStats(&target, kBlade, items::kBladeSlots - 1);
  const auto projected = items::Live::Typed(target);
  CHECK(projected.id == kFullByte);
  CHECK(projected.strength == kLow);
  CHECK(projected.material == kFullByte);
  CHECK(projected.types == initial.types);
  CHECK(projected.gem_effects == kFullByte);
  blade.id = kLowMaterial;
  sorting::CopyLiveEquipmentStats(&target, kBlade, items::kBladeSlots - 1);
  CHECK(items::Live::Typed(target).id == kLowMaterial);
}

TEST_CASE("F02589 Live uses real grip armor and gem readers without zeroing",
          "[inventory-sorting-live][F02589]") {
  fixture::LiveFixture state;
  auto& records = Audio_Seq_unusedInventory;
  records.grips[0].strength = kLow;
  records.armor[0].strength = kHigh;
  std::span(records.gems).back().strength = kLow;
  Items_EquipmentPartStats target{};
  target.currentDp = kWide;
  sorting::CopyLiveEquipmentStats(&target, kGrip, 0);
  CHECK(items::Live::Typed(target).strength == kLow);
  CHECK(target.currentDp == kWide);
  sorting::CopyLiveEquipmentStats(&target, kArmor, 0);
  CHECK(items::Live::Typed(target).strength == kHigh);
  target.category = kHighMaterial;
  sorting::CopyLiveEquipmentStats(&target, kGem, items::kGemSlots - 1);
  CHECK(items::Live::Typed(target).strength == kLow);
  CHECK(items::Live::Typed(target).category == kHighMaterial);
}

TEST_CASE("F02589 unsupported Live categories are lazy even for a null output",
          "[inventory-sorting-live][F02589]") {
  fixture::LiveFixture state;
  for (const int category :
       VS::Menu::sorting_test::kUnsupportedEquipmentCategories) {
    CHECK_NOTHROW(sorting::CopyLiveEquipmentStats(
        nullptr, category, std::numeric_limits<int>::min()));
  }
}

TEST_CASE(
    "F02590 Live preserves a known order alias across ordinary MENU3 reload",
    "[inventory-sorting-live][inventory-sorting-reload][F02590]") {
  fixture::LiveFixture state;
  auto& records = Audio_Seq_unusedInventory;
  auto& order = Audio_Seq_unusedInventoryIndices.misc;
  records.gems[0].strength = kLow;
  std::span(records.gems).back().strength = kHigh;
  order[0] = 1;
  order[1] = items::kGemSlots;
  order[2] = items::kGemSlots;
  Menu_Inventory_inventoryIndices[kGem] = order;
  sorting::SortLiveEquipment(kGem, kStrength);
  Boot_ResetMenu3Statics();
  CHECK(Menu_Inventory_inventoryIndices[kGem] == order);
  CHECK(order[0] == items::kGemSlots);
  CHECK(order[1] == items::kGemSlots);
  CHECK(order[2] == 1);
  Boot_ResetMenubStatics();
  CHECK(Menu_Inventory_inventoryIndices[kGem] == order);
  CHECK(order[2] == 1);
  records.gems[0].strength = kHigh;
  std::span(records.gems).back().strength = kLow;
  sorting::SortLiveEquipment(kGem, kStrength);
  CHECK(order[0] == 1);
  CHECK(order[1] == items::kGemSlots);
}

TEST_CASE(
    "MAINMENU reload restores only inventory pointer routes not sorted bytes",
    "[inventory-sorting-live][inventory-sorting-reload][F02590]") {
  fixture::LiveFixture state;
  auto& order = Audio_Seq_unusedInventoryIndices;
  order.misc[0] = items::kMiscSlots;
  Audio_Seq_unusedInventory.misc[0].id = kWide;
  Menu_Inventory_inventoryIndices[kBlade] = order.misc;
  Boot_ResetMainMenuStatics();
  CHECK(Menu_Inventory_inventoryIndices[kBlade] == order.blades);
  CHECK(order.misc[0] == items::kMiscSlots);
  CHECK(Audio_Seq_unusedInventory.misc[0].id == kWide);
}
