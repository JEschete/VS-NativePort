#include <cstddef>
#include <cstdint>
#include <type_traits>

#include <catch2/catch_test_macros.hpp>

#include "Content/ModuleData.h"
#include "Items/Inventory.hpp"
#include "Menu/Loot/LootStaging.hpp"
#include "Menu/Loot/LootStagingLive.hpp"
extern "C" {
#include "Menu/Loot/LootScreen.h"
}

namespace {
namespace items = VS::Items;
namespace staging = VS::Menu::Loot;
namespace live = VS::Menu::Loot::Live;
namespace inventory = VS::Items::Inventory;

struct LiveStageFixture {
  Items_Inventory staged{};
  Items_Inventory* previous{Menu_Loot_inventory};

  LiveStageFixture() { Menu_Loot_inventory = &staged; }
  ~LiveStageFixture() { Menu_Loot_inventory = previous; }
};

items::LootedWeapon MakeWeapon() {
  items::LootedWeapon source{};
  source.blade.id = 0x31;
  source.blade.index = 0xE1;
  source.grip.id = 0x1234;
  source.grip.index = 0xE2;
  source.gems[0].id = 0x234;
  source.gems[0].index = 0xE3;
  source.gems[2].id = 0x236;
  source.name[0] = 'W';
  source.name.back() = 'Z';
  return source;
}

items::LootedShield MakeShield() {
  items::LootedShield source{};
  source.shield.id = 0x45;
  source.shield.limb = 3;
  source.shield.index = 0xE2;
  source.gems[0].id = 0x234;
  source.gems[2].id = 0x236;
  return source;
}

template <typename Entry>
bool IsBound(unsigned address, Entry entry) {
  return VS::Content::ResolveModuleAddress(
             {Content_moduleSymbols, Content_moduleSymbolCount},
             "MENU/MENUB.PRG", address) == reinterpret_cast<const void*>(entry);
}

void FillBlades(items::InventoryContents& target, std::size_t count) {
  for (std::size_t i = 0; i < count; ++i) {
    target.blades[i].id = static_cast<std::uint8_t>(i + 1);
  }
}

void FillGrips(items::InventoryContents& target, std::size_t count) {
  for (std::size_t i = 0; i < count; ++i) {
    target.grips[i].id = static_cast<std::uint16_t>(i + 1);
  }
}

void FillGems(items::InventoryContents& target, std::size_t count) {
  for (std::size_t i = 0; i < count; ++i) {
    target.gems[i].id = static_cast<std::uint16_t>(i + 1);
  }
}

void FillWeapons(items::InventoryContents& target, std::size_t count) {
  for (std::size_t i = 0; i < count; ++i) {
    target.weapons[i].blade = 1;
  }
}

void FillShields(items::InventoryContents& target, std::size_t count) {
  for (std::size_t i = 0; i < count; ++i) {
    target.shields[i].base.id = static_cast<std::uint8_t>(i + 1);
  }
}

void FillArmor(items::InventoryContents& target, std::size_t count) {
  for (std::size_t i = 0; i < count; ++i) {
    target.armor[i].id = static_cast<std::uint8_t>(i + 1);
  }
}

void FillMisc(items::InventoryContents& target, std::size_t count) {
  for (std::size_t i = 0; i < count; ++i) {
    target.misc[i].id = static_cast<std::uint16_t>(i + 1);
  }
}
}  // namespace

TEST_CASE("F02846 blade staging copies the record and uses all sixteen slots",
          "[loot-staging][F02846]") {
  items::InventoryContents target{};
  items::InventoryBlade blade{};
  blade.id = 0x35;
  blade.material = 0x1245;
  blade.assembled_weapon_index = static_cast<char>(-31);
  blade.index = static_cast<char>(-30);

  CHECK(staging::StageBlade(blade, 7, target) == 1);
  CHECK(target.blades[0].id == blade.id);
  CHECK(target.blades[0].material == blade.material);
  CHECK(target.blades[0].assembled_weapon_index == 7);
  CHECK(target.blades[0].index == 1);
}

TEST_CASE("F02846 blade staging uses every inventory slot",
          "[loot-staging][F02846]") {
  items::InventoryBlade blade{};
  items::InventoryContents last{};
  FillBlades(last, items::kBladeSlots - 1);
  CHECK(staging::StageBlade(blade, 3, last) ==
        static_cast<int>(items::kBladeSlots));
  CHECK(last.blades.back().id == blade.id);
  CHECK(last.blades.back().index == items::kBladeSlots);

  FillBlades(last, items::kBladeSlots);
  const auto full = last;
  CHECK(staging::StageBlade(blade, 3, last) == 0);
  CHECK(last == full);
}

TEST_CASE("F02847 grip staging copies the record and uses all sixteen slots",
          "[loot-staging][F02847]") {
  items::InventoryContents target{};
  items::InventoryGrip grip{};
  grip.id = 0x1234;
  grip.gem_slots = 3;
  grip.assembled_weapon_index = 0xE1;
  grip.index = 0xE2;

  CHECK(staging::StageGrip(grip, 6, target) == 1);
  CHECK(target.grips[0].id == grip.id);
  CHECK(target.grips[0].gem_slots == grip.gem_slots);
  CHECK(target.grips[0].assembled_weapon_index == 6);
  CHECK(target.grips[0].index == 1);
}

TEST_CASE("F02847 grip staging uses every inventory slot",
          "[loot-staging][F02847]") {
  items::InventoryGrip grip{};
  items::InventoryContents last{};
  FillGrips(last, items::kGripSlots - 1);
  CHECK(staging::StageGrip(grip, 2, last) ==
        static_cast<int>(items::kGripSlots));
  CHECK(last.grips.back().index == items::kGripSlots);

  FillGrips(last, items::kGripSlots);
  const auto full = last;
  CHECK(staging::StageGrip(grip, 2, last) == 0);
  CHECK(last == full);
}

TEST_CASE(
    "F02848 gem staging preserves its owner and uses all forty-eight slots",
    "[loot-staging][F02848]") {
  items::InventoryContents target{};
  items::InventoryGem gem{};
  gem.id = 0x1234;
  gem.strength = -7;
  gem.set_item_index = 0xE1;
  gem.index = 0xE2;

  CHECK(staging::StageGem(gem, 0x82, target) == 1);
  CHECK(target.gems[0].id == gem.id);
  CHECK(target.gems[0].strength == gem.strength);
  CHECK(target.gems[0].set_item_index == 0x82);
  CHECK(target.gems[0].index == 1);
}

TEST_CASE("F02848 gem staging uses every inventory slot",
          "[loot-staging][F02848]") {
  items::InventoryGem gem{};
  items::InventoryContents last{};
  FillGems(last, items::kGemSlots - 1);
  CHECK(staging::StageGem(gem, 4, last) == static_cast<int>(items::kGemSlots));
  CHECK(last.gems.back().index == items::kGemSlots);

  FillGems(last, items::kGemSlots);
  const auto full = last;
  CHECK(staging::StageGem(gem, 4, last) == 0);
  CHECK(last == full);
}

TEST_CASE("F02849 weapon staging copies its entry", "[loot-staging][F02849]") {
  const auto source = MakeWeapon();
  items::InventoryContents target{};
  CHECK(staging::StageWeapon(source, target) == 1);
  CHECK(target.weapons[0].index == 1);
  CHECK(target.weapons[0].blade == 1);
  CHECK(target.weapons[0].grip == 1);
  CHECK(target.weapons[0].name == source.name);
}

TEST_CASE("F02849 weapon staging copies blade and grip components",
          "[loot-staging][F02849]") {
  const auto source = MakeWeapon();
  items::InventoryContents target{};
  CHECK(staging::StageWeapon(source, target) == 1);
  CHECK(target.blades[0].id == source.blade.id);
  CHECK(target.blades[0].assembled_weapon_index == 1);
  CHECK(target.blades[0].index == 1);
  CHECK(target.grips[0].id == source.grip.id);
  CHECK(target.grips[0].assembled_weapon_index == 1);
  CHECK(target.grips[0].index == 1);
}

TEST_CASE("F02849 weapon staging maps each gem reference",
          "[loot-staging][F02849]") {
  const auto source = MakeWeapon();
  items::InventoryContents target{};
  CHECK(staging::StageWeapon(source, target) == 1);
  CHECK(target.weapons[0].gems[0] == 1);
  CHECK(target.weapons[0].gems[1] == 0);
  CHECK(target.weapons[0].gems[2] == 2);
}

TEST_CASE("F02849 weapon staging copies each gem component",
          "[loot-staging][F02849]") {
  const auto source = MakeWeapon();
  items::InventoryContents target{};
  CHECK(staging::StageWeapon(source, target) == 1);
  CHECK(target.gems[0].id == source.gems[0].id);
  CHECK(target.gems[0].set_item_index == 1);
  CHECK(target.gems[0].index == 1);
  CHECK(target.gems[1].id == source.gems[2].id);
  CHECK(target.gems[1].set_item_index == 1);
  CHECK(target.gems[1].index == 2);
}

TEST_CASE("F02849 weapon staging uses the final component slots",
          "[loot-staging][F02849]") {
  auto source = MakeWeapon();
  source.gems[2].id = 0;
  items::InventoryContents last{};
  FillWeapons(last, items::kWeaponSlots - 1);
  FillBlades(last, items::kBladeSlots - 1);
  FillGrips(last, items::kGripSlots - 1);
  FillGems(last, items::kGemSlots - 1);
  CHECK(staging::StageWeapon(source, last) ==
        static_cast<int>(items::kWeaponSlots));
  CHECK(last.weapons.back().index == items::kWeaponSlots);
  CHECK(last.blades.back().index == items::kBladeSlots);
  CHECK(last.grips.back().index == items::kGripSlots);
  CHECK(last.gems.back().index == items::kGemSlots);
}

TEST_CASE("F02849 weapon staging rejects a full parent table unchanged",
          "[loot-staging][F02849]") {
  const auto source = MakeWeapon();
  items::InventoryContents full{};
  FillWeapons(full, items::kWeaponSlots);
  const auto before = full;
  CHECK(staging::StageWeapon(source, full) == 0);
  CHECK(full == before);
}

TEST_CASE("F02849 weapon staging rejects full component tables unchanged",
          "[loot-staging][F02849]") {
  const auto source = MakeWeapon();
  for (const auto full_table : {0, 1, 2}) {
    items::InventoryContents full{};
    if (full_table == 0) FillBlades(full, items::kBladeSlots);
    if (full_table == 1) FillGrips(full, items::kGripSlots);
    if (full_table == 2) FillGems(full, items::kGemSlots);
    const auto before = full;
    CHECK(staging::StageWeapon(source, full) == 0);
    CHECK(full == before);
  }
}

TEST_CASE("F02850 armor staging copies all fields and uses all sixteen slots",
          "[loot-staging][F02850]") {
  items::InventoryContents target{};
  items::InventoryArmor armor{};
  armor.id = 0x35;
  armor.limb = 4;
  armor.index = 0xE2;
  armor.current_dp = 0x1234;

  CHECK(staging::StageArmor(armor, target) == 1);
  CHECK(target.armor[0].id == armor.id);
  CHECK(target.armor[0].limb == armor.limb);
  CHECK(target.armor[0].current_dp == armor.current_dp);
  CHECK(target.armor[0].index == 1);
}

TEST_CASE("F02850 armor staging uses every inventory slot",
          "[loot-staging][F02850]") {
  items::InventoryArmor armor{};
  items::InventoryContents last{};
  FillArmor(last, items::kArmorSlots - 1);
  CHECK(staging::StageArmor(armor, last) ==
        static_cast<int>(items::kArmorSlots));
  CHECK(last.armor.back().index == items::kArmorSlots);

  FillArmor(last, items::kArmorSlots);
  const auto full = last;
  CHECK(staging::StageArmor(armor, last) == 0);
  CHECK(last == full);
}

TEST_CASE("F02851 shield staging copies its header and references",
          "[loot-staging][F02851]") {
  const auto source = MakeShield();
  items::InventoryContents target{};
  CHECK(staging::StageShield(source, target) == 1);
  CHECK(target.shields[0].index == 0);
  CHECK(target.shields[0].base.id == source.shield.id);
  CHECK(target.shields[0].base.limb == 1);
  CHECK(target.shields[0].base.index == source.shield.index);
  CHECK(target.shields[0].gems[0] == 1);
  CHECK(target.shields[0].gems[1] == 0);
  CHECK(target.shields[0].gems[2] == 2);
}

TEST_CASE("F02851 shield staging copies the gem components",
          "[loot-staging][F02851]") {
  const auto source = MakeShield();
  items::InventoryContents target{};
  CHECK(staging::StageShield(source, target) == 1);
  CHECK(target.gems[0].id == source.gems[0].id);
  CHECK(target.gems[0].set_item_index == (1 | items::kShieldGemFlag));
  CHECK(target.gems[0].index == 1);
  CHECK(target.gems[1].id == source.gems[2].id);
  CHECK(target.gems[1].set_item_index == (1 | items::kShieldGemFlag));
  CHECK(target.gems[1].index == 2);
}

TEST_CASE("F02851 shield staging uses the final shield and gem slots",
          "[loot-staging][F02851]") {
  auto source = MakeShield();
  source.gems[2].id = 0;
  items::InventoryContents last{};
  FillShields(last, items::kShieldSlots - 1);
  FillGems(last, items::kGemSlots - 1);
  CHECK(staging::StageShield(source, last) ==
        static_cast<int>(items::kShieldSlots));
  CHECK(last.shields.back().index == 0);
  CHECK(last.shields.back().base.limb == items::kShieldSlots);
  CHECK(last.gems.back().index == items::kGemSlots);
  CHECK(last.gems.back().set_item_index ==
        (items::kShieldSlots | items::kShieldGemFlag));
}

TEST_CASE("F02851 shield staging rejects a full shield table unchanged",
          "[loot-staging][F02851]") {
  const auto source = MakeShield();
  items::InventoryContents full{};
  FillShields(full, items::kShieldSlots);
  const auto before = full;
  CHECK(staging::StageShield(source, full) == 0);
  CHECK(full == before);
}

TEST_CASE("F02851 shield staging rejects a full gem table unchanged",
          "[loot-staging][F02851]") {
  const auto source = MakeShield();
  items::InventoryContents full{};
  FillGems(full, items::kGemSlots);
  const auto before = full;
  CHECK(staging::StageShield(source, full) == 0);
  CHECK(full == before);
}

TEST_CASE("F02852 misc staging does not merge and preserves the reserved index",
          "[loot-staging][F02852]") {
  items::InventoryContents target{};
  items::LootedItem source{
      .item_id = 0x1234, .difficulty_flags = 0x7F, .count = 9};
  target.misc[0] = {source.item_id, 100, 0x44};
  target.misc[1].index = 0x77;

  CHECK(staging::StageMiscItem(source, target) == 2);
  CHECK(target.misc[0].count == 100);
  CHECK(target.misc[1].id == source.item_id);
  CHECK(target.misc[1].count == source.count);
  CHECK(target.misc[1].index == 0x77);
}

TEST_CASE("F02852 misc staging uses every inventory slot",
          "[loot-staging][F02852]") {
  items::LootedItem source{
      .item_id = 0x1234, .difficulty_flags = 0x7F, .count = 9};
  items::InventoryContents last{};
  FillMisc(last, items::kMiscSlots - 1);
  last.misc.back().index = 0x66;
  CHECK(staging::StageMiscItem(source, last) ==
        static_cast<int>(items::kMiscSlots));
  CHECK(last.misc.back().id == source.item_id);
  CHECK(last.misc.back().count == source.count);
  CHECK(last.misc.back().index == 0x66);

  FillMisc(last, items::kMiscSlots);
  const auto full = last;
  CHECK(staging::StageMiscItem(source, last) == 0);
  CHECK(last == full);
}

TEST_CASE("F02846 live binding writes staged blades through the menu inventory",
          "[loot-staging][F02846][live]") {
  static_assert(std::is_same_v<decltype(&live::StageBlade),
                               int (*)(Items_InventoryBlade*, int)>);
  LiveStageFixture fixture;
  Items_InventoryBlade blade{};
  blade.id = 0x35;
  blade.assembledWeaponIndex = static_cast<char>(-31);
  blade.index = static_cast<char>(-30);

  CHECK(live::StageBlade(&blade, 7) == 1);
  CHECK(fixture.staged.blades[0].id == blade.id);
  CHECK(fixture.staged.blades[0].assembledWeaponIndex == 7);
  CHECK(fixture.staged.blades[0].index == 1);
}

TEST_CASE("F02847 live binding writes staged grips through the menu inventory",
          "[loot-staging][F02847][live]") {
  static_assert(std::is_same_v<decltype(&live::StageGrip),
                               int (*)(Items_InventoryGrip*, int)>);
  LiveStageFixture fixture;
  Items_InventoryGrip grip{};
  grip.id = 0x1234;
  grip.assembledWeaponIndex = 0xE1;
  grip.index = 0xE2;

  CHECK(live::StageGrip(&grip, 6) == 1);
  CHECK(fixture.staged.grips[0].id == grip.id);
  CHECK(fixture.staged.grips[0].assembledWeaponIndex == 6);
  CHECK(fixture.staged.grips[0].index == 1);
}

TEST_CASE("F02848 live binding writes staged gems through the menu inventory",
          "[loot-staging][F02848][live]") {
  static_assert(std::is_same_v<decltype(&live::StageGem),
                               int (*)(Items_InventoryGem*, int)>);
  LiveStageFixture fixture;
  Items_InventoryGem gem{};
  gem.id = 0x1234;
  gem.setItemIndex = 0xE1;
  gem.index = 0xE2;

  CHECK(live::StageGem(&gem, 0x82) == 1);
  CHECK(fixture.staged.gems[0].id == gem.id);
  CHECK(fixture.staged.gems[0].setItemIndex == 0x82);
  CHECK(fixture.staged.gems[0].index == 1);
}

TEST_CASE("F02849 live binding stages a weapon and its components",
          "[loot-staging][F02849][live]") {
  static_assert(std::is_same_v<decltype(&live::StageWeapon),
                               int (*)(Items_LootedWeapon*)>);
  LiveStageFixture fixture;
  Items_LootedWeapon weapon{};
  weapon.blade.id = 0x31;
  weapon.grip.id = 0x1234;
  weapon.gems[0].id = 0x234;
  weapon.name[0] = 'W';

  CHECK(live::StageWeapon(&weapon) == 1);
  CHECK(fixture.staged.weapons[0].index == 1);
  CHECK(fixture.staged.weapons[0].blade == 1);
  CHECK(fixture.staged.weapons[0].grip == 1);
  CHECK(fixture.staged.weapons[0].gems[0] == 1);
  CHECK(fixture.staged.weapons[0].name[0] == 'W');
  CHECK(fixture.staged.blades[0].assembledWeaponIndex == 1);
  CHECK(fixture.staged.grips[0].assembledWeaponIndex == 1);
  CHECK(fixture.staged.gems[0].setItemIndex == 1);
}

TEST_CASE("F02850 live binding writes staged armor through the menu inventory",
          "[loot-staging][F02850][live]") {
  static_assert(std::is_same_v<decltype(&live::StageArmor),
                               int (*)(Items_InventoryArmor*)>);
  LiveStageFixture fixture;
  Items_InventoryArmor armor{};
  armor.id = 0x35;
  armor.limb = 4;
  armor.index = 0xE2;

  CHECK(live::StageArmor(&armor) == 1);
  CHECK(fixture.staged.armor[0].id == armor.id);
  CHECK(fixture.staged.armor[0].limb == armor.limb);
  CHECK(fixture.staged.armor[0].index == 1);
}

TEST_CASE("F02851 live binding stages a shield with its shield gem owner flag",
          "[loot-staging][F02851][live]") {
  static_assert(std::is_same_v<decltype(&live::StageShield),
                               int (*)(Items_LootedShield*)>);
  LiveStageFixture fixture;
  Items_LootedShield shield{};
  shield.shield.id = 0x45;
  shield.gems[0].id = 0x234;

  CHECK(live::StageShield(&shield) == 1);
  CHECK(fixture.staged.shields[0].index == 0);
  CHECK(fixture.staged.shields[0].base.limb == 1);
  CHECK(fixture.staged.shields[0].gems[0] == 1);
  CHECK(fixture.staged.gems[0].setItemIndex == (1 | items::kShieldGemFlag));
}

TEST_CASE("F02852 live binding writes the misc ID and count",
          "[loot-staging][F02852][live]") {
  static_assert(std::is_same_v<decltype(&live::StageMiscItem),
                               int (*)(Items_LootedItem*)>);
  LiveStageFixture fixture;
  Items_LootedItem item{};
  item.itemId = 0x1234;
  item.difficultyFlags = 0x7F;
  item.count = 9;
  fixture.staged.misc[0].index = 0x77;

  CHECK(live::StageMiscItem(&item) == 1);
  CHECK(fixture.staged.misc[0].id == item.itemId);
  CHECK(fixture.staged.misc[0].count == item.count);
  CHECK(fixture.staged.misc[0].index == 0x77);
}

TEST_CASE("F02846 through F02852 resolve their verified MENUB module entries",
          "[loot-staging][module]") {
  CHECK(IsBound(0x80107C54, &live::StageBlade));
  CHECK(IsBound(0x80107CE8, &live::StageGrip));
  CHECK(IsBound(0x80107D7C, &live::StageGem));
  CHECK(IsBound(0x80107E10, &live::StageWeapon));
  CHECK(IsBound(0x80107F1C, &live::StageArmor));
  CHECK(IsBound(0x80107F9C, &live::StageShield));
  CHECK(IsBound(0x8010808C, &live::StageMiscItem));
}
