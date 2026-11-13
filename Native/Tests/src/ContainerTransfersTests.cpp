#include <array>
#include <stdexcept>

#include <catch2/catch_test_macros.hpp>

#include "Items/ContainerTransfersWorld.hpp"

namespace {
namespace items = VS::Items;
using container_transfers_tests::BagWorld;
using container_transfers_tests::Check;
using container_transfers_tests::Copy;
using items::ItemCategory;
}  // namespace

TEST_CASE("F02934 copies only misc and merges selected pending slots",
          "[container-transfers][F02934]") {
  items::ContainerStorage preview;
  items::ContainerStorage other;
  items::ContainerStorage staged;
  preview.items.weapons[0].blade = 9;
  other.items.misc[0] = {.id = 12, .count = 95, .index = 41};
  staged.items.misc[255] = {.id = 12, .count = 8, .index = 99};
  const std::array<std::uint16_t, 4> pending{0x100, 0x600, 0, 0x601};
  items::PreviewTransferredMisc(preview, other, staged, pending);
  CHECK(preview.items.misc[0].count == 100);
  CHECK(preview.items.misc[1].count == 3);
  CHECK(preview.items.misc[1].index == 0);
  CHECK(preview.items.weapons[0].blade == 9);
  CHECK(other.items.misc[0].count == 95);
  CHECK(staged.items.misc[255].count == 8);
}

TEST_CASE("F02934 refuses a missing terminator after completed preview writes",
          "[container-transfers][F02934]") {
  items::ContainerStorage preview;
  items::ContainerStorage other;
  items::ContainerStorage staged;
  other.items.misc[0].id = 15;
  const std::array<std::uint16_t, 1> pending{0x101};
  CHECK_THROWS_AS(
      items::PreviewTransferredMisc(preview, other, staged, pending),
      std::out_of_range);
  CHECK(preview.items.misc[0].id == 15);
}

TEST_CASE("F02935 counts occupied preview records rather than quantities",
          "[container-transfers][F02935]") {
  items::ContainerStorage preview;
  items::ContainerStorage other;
  items::ContainerStorage staged;
  other.items.misc[0] = {.id = 3, .count = 100};
  other.items.misc[255] = {.id = 4, .count = 0};
  staged.items.misc[0] = {.id = 3, .count = 1};
  const std::array<std::uint16_t, 2> pending{0x601, 0};
  CHECK(items::RefreshMiscTransferCount(preview, other, staged, pending) == 3);
  CHECK(preview.items.misc[1].id == 3);
  other.items.misc = {};
  CHECK(items::RefreshMiscTransferCount(preview, other, staged,
                                        std::array<std::uint16_t, 1>{0}) == 0);
}

TEST_CASE(
    "F02936 weapon dry check and shared gem copies preserve source ownership",
    "[container-transfers][F02936]") {
  items::ContainerStorage target;
  items::ContainerStorage source;
  source.items.weapons[0] = {.index = 5,
                             .blade = 64,
                             .grip = 64,
                             .is_equipped = 1,
                             .gems = {192, 192, 0, 77}};
  source.items.weapons[0].name[0] = 'W';
  source.items.blades[63].id = 11;
  source.items.grips[63].id = 12;
  source.items.gems[191].id = 13;
  target.items.weapons[0].is_equipped = 1;
  target.items.weapons[0].gems[3] = 165;
  const auto original = source.items;
  const auto original_target = target.items;
  CHECK(items::CopyContainerItem(Check(ItemCategory::kWeapon), target, 0,
                                 source) == 1);
  CHECK(target.items == original_target);
  CHECK(items::CopyContainerItem(Copy(ItemCategory::kWeapon), target, 0,
                                 source) == 1);
  CHECK(target.items.weapons[0].gems ==
        std::array<std::uint8_t, 4>{1, 2, 0, 0});
  CHECK(target.items.weapons[0].name[0] == 'W');
  CHECK(target.items.weapons[0].is_equipped == 0);
  CHECK(target.items.gems[1].set_item_index == 1);
  CHECK(source.items == original);
}

TEST_CASE(
    "F02936 preserves partial target construction before missing mandatory "
    "grip",
    "[container-transfers][F02936]") {
  items::ContainerStorage target;
  items::ContainerStorage source;
  source.items.weapons[0].blade = 1;
  source.items.blades[0].id = 7;
  target.items.weapons[0].name[0] = 'X';
  CHECK_THROWS_AS(
      items::CopyContainerItem(Copy(ItemCategory::kWeapon), target, 0, source),
      std::out_of_range);
  CHECK(target.items.weapons[0].index == 1);
  CHECK(target.items.weapons[0].blade == 1);
  CHECK(target.items.weapons[0].name[0] == 0);
  CHECK(target.items.blades[0].id == 7);
  CHECK(target.items.grips[0].id == 0);
}

TEST_CASE("F02936 diagnoses zero blade after clearing aliased source weapon",
          "[container-transfers][F02936]") {
  items::ContainerStorage owner;
  owner.items.weapons[0].grip = 1;
  owner.items.weapons[0].name[0] = 'A';
  CHECK_THROWS_AS(
      items::CopyContainerItem(Copy(ItemCategory::kWeapon), owner, 0, owner),
      std::out_of_range);
  CHECK(owner.items.weapons[0].index == 1);
  CHECK(owner.items.weapons[0].grip == 0);
  CHECK(owner.items.weapons[0].name[0] == 0);
}

TEST_CASE(
    "F02936 positive mandatory references may name valid empty part records",
    "[container-transfers][F02936]") {
  items::ContainerStorage target;
  items::ContainerStorage source;
  source.items.weapons[0].blade = 1;
  source.items.weapons[0].grip = 64;
  source.items.blades[0].opaque12 = 32769;
  source.items.grips[63].sub_id = 231;
  CHECK(items::CopyContainerItem(Copy(ItemCategory::kWeapon), target, 0,
                                 source) == 1);
  CHECK(target.items.weapons[0].blade == 1);
  CHECK(target.items.weapons[0].grip == 1);
  CHECK(target.items.blades[0].id == 0);
  CHECK(target.items.blades[0].opaque12 == 32769);
  CHECK(target.items.grips[0].sub_id == 231);
}

TEST_CASE("F02936 shield copies three optional gems with shield parent flag",
          "[container-transfers][F02936]") {
  items::ContainerStorage target;
  items::ContainerStorage source;
  source.items.shields[31].base.id = 7;
  source.items.shields[31].base.index = 91;
  source.items.shields[31].is_equipped = 1;
  source.items.shields[31].gems = {192, 0, 192, 17};
  source.items.gems[191].id = 88;
  CHECK(items::CopyContainerItem(Copy(ItemCategory::kShield), target, 31,
                                 source) == 1);
  CHECK(target.items.shields[0].base == source.items.shields[31].base);
  CHECK(target.items.shields[0].is_equipped == 0);
  CHECK(target.items.shields[0].gems ==
        std::array<std::uint8_t, 4>{1, 0, 2, 0});
  CHECK(target.items.gems[1].set_item_index == 0x81);
}

TEST_CASE("F02936 aliased empty shield is zeroed before its base is reread",
          "[container-transfers][F02936]") {
  items::ContainerStorage owner;
  owner.items.shields[0].base.material = 73;
  owner.items.shields[0].gems[0] = 192;
  owner.items.gems[191].id = 8;
  CHECK(items::CopyContainerItem(Copy(ItemCategory::kShield), owner, 0,
                                 owner) == 1);
  CHECK(owner.items.shields[0].base.material == 0);
  CHECK(owner.items.shields[0].gems[0] == 0);
  CHECK(owner.items.gems[0].id == 0);
  CHECK(owner.items.gems[191].id == 8);
}

TEST_CASE(
    "F02936 copies valid empty parts and preserves exact parent field widths",
    "[container-transfers][F02936]") {
  items::ContainerStorage target;
  items::ContainerStorage source;
  source.items.blades[63].opaque12 = 49152;
  source.items.grips[63].sub_id = 231;
  source.items.gems[191].reserved3 = 199;
  CHECK(items::CopyContainerItem(Copy(ItemCategory::kBlade, -1), target, 63,
                                 source) == 1);
  CHECK(items::CopyContainerItem(Copy(ItemCategory::kGrip, -1), target, 63,
                                 source) == 1);
  CHECK(items::CopyContainerItem(Copy(ItemCategory::kGem, -1), target, 191,
                                 source) == 1);
  CHECK(target.items.blades[0].assembled_weapon_index == 255);
  CHECK(target.items.grips[0].assembled_weapon_index == 65535);
  CHECK(target.items.gems[0].set_item_index == 65535);
  CHECK(target.items.blades[0].opaque12 == 49152);
}

TEST_CASE(
    "F02936 full target and plain-part dry check do not read missing source",
    "[container-transfers][F02936]") {
  items::ContainerStorage target;
  items::ContainerStorage source;
  for (auto& blade : target.items.blades) blade.id = 1;
  CHECK(items::CopyContainerItem(Copy(ItemCategory::kBlade), target, -1,
                                 source) == 0);
  target.items.blades[63].id = 0;
  CHECK(items::CopyContainerItem(Check(ItemCategory::kBlade), target, -1,
                                 source) == 64);
  CHECK(target.items.blades[63].id == 0);
  CHECK_THROWS_AS(
      items::CopyContainerItem(Copy(ItemCategory::kBlade), target, -1, source),
      std::out_of_range);
}

TEST_CASE("F02936 gem capacity counts duplicate attached references separately",
          "[container-transfers][F02936]") {
  items::ContainerStorage target;
  items::ContainerStorage source;
  source.items.weapons[0] = {.blade = 1, .grip = 1, .gems = {1, 1, 1, 0}};
  for (auto& gem : target.items.gems) gem.id = 1;
  target.items.gems[191].id = 0;
  target.items.gems[190].id = 0;
  CHECK(items::CopyContainerItem(Check(ItemCategory::kWeapon), target, 0,
                                 source) == 0);
  target.items.gems[189].id = 0;
  CHECK(items::CopyContainerItem(Check(ItemCategory::kWeapon), target, 0,
                                 source) == 1);
}

TEST_CASE(
    "F02936 misc merging preserves existing indices and continues after zero "
    "remainder",
    "[container-transfers][F02936]") {
  items::ContainerStorage target;
  items::ContainerStorage source;
  target.items.misc[0] = {.id = 2, .count = 99, .index = 91};
  target.items.misc[1] = {.id = 2, .count = 150, .index = 92};
  source.items.misc[0] = {.id = 2, .count = 1, .index = 4};
  CHECK(items::CopyContainerItem(Copy(ItemCategory::kMisc), target, 0,
                                 source) == 3);
  CHECK(target.items.misc[0].count == 100);
  CHECK(target.items.misc[1].count == 100);
  CHECK(target.items.misc[2].count == 50);
  CHECK(target.items.misc[0].index == 91);
  CHECK(target.items.misc[2].index == 0);
}

TEST_CASE(
    "F02936 misc dry run and aliased records retain original merge behavior",
    "[container-transfers][F02936]") {
  items::ContainerStorage owner;
  owner.items.misc[0] = {.id = 2, .count = 60, .index = 77};
  const auto original = owner.items;
  CHECK(items::CopyContainerItem(Check(ItemCategory::kMisc), owner, 0, owner) ==
        2);
  CHECK(owner.items == original);
  CHECK(items::CopyContainerItem(Copy(ItemCategory::kMisc), owner, 0, owner) ==
        2);
  CHECK(owner.items.misc[0].count == 100);
  CHECK(owner.items.misc[1].count == 20);
  CHECK(owner.items.misc[1].id == 2);
}

TEST_CASE("F02936 dry checking a zero quantity clamps the merge remainder",
          "[container-transfers][F02936]") {
  items::ContainerStorage target;
  items::ContainerStorage source;
  target.items.misc[0] = {.id = 12, .count = 1};
  source.items.misc[0] = {.id = 12, .count = 0};
  CHECK(items::CopyContainerItem(Check(ItemCategory::kMisc), target, 0,
                                 source) == 1);
  CHECK(target.items.misc[0].count == 1);
  CHECK(target.items.misc[1].id == 0);
}

TEST_CASE(
    "F02937 prunes empty slots preserves duplicate order and appends missing "
    "records",
    "[container-transfers][F02937]") {
  BagWorld world;
  world.storage.items.blades[0].id = 1;
  world.storage.items.blades[2].id = 2;
  world.storage.items.blades[4].id = 3;
  world.storage.items.blades[63].id = 4;
  auto order = world.BagOrder(ItemCategory::kBlade);
  order[0] = 5;
  order[2] = 2;
  order[3] = 5;
  order[5] = 3;
  items::RebuildContainerCategoryIndices(ItemCategory::kBlade, order,
                                         world.storage);
  CHECK(order[0] == 5);
  CHECK(order[1] == 5);
  CHECK(order[2] == 3);
  CHECK(order[3] == 1);
  CHECK(order[4] == 64);
  CHECK(order[5] == 0);
}

TEST_CASE("F02937 retains earlier pruning before a later invalid record token",
          "[container-transfers][F02937]") {
  BagWorld world;
  auto order = world.BagOrder(ItemCategory::kBlade);
  order[0] = 1;
  order[1] = 65;
  CHECK_THROWS_AS(items::RebuildContainerCategoryIndices(ItemCategory::kBlade,
                                                         order, world.storage),
                  std::out_of_range);
  CHECK(order[0] == 0);
  CHECK(order[1] == 65);
}

TEST_CASE(
    "F02937 refuses short and degenerate used orders without fabricated "
    "completion",
    "[container-transfers][F02937]") {
  items::ContainerStorage owner;
  std::array<std::uint16_t, 1> short_order{};
  CHECK_THROWS_AS(items::RebuildContainerCategoryIndices(ItemCategory::kBlade,
                                                         short_order, owner),
                  std::out_of_range);
  owner.capacities[1] = 1;
  CHECK_NOTHROW(items::RebuildContainerCategoryIndices(ItemCategory::kBlade,
                                                       short_order, owner));
  owner.items.blades[0].id = 1;
  short_order[0] = 1;
  CHECK_THROWS_AS(items::RebuildContainerCategoryIndices(ItemCategory::kBlade,
                                                         short_order, owner),
                  std::out_of_range);
}

TEST_CASE(
    "F02938 snapshots real inventory extents and zero extends byte display "
    "tokens",
    "[container-transfers][F02938]") {
  BagWorld world;
  world.storage.items.misc[255].id = 8;
  world.storage.items.weapons[31].index = 32;
  world.storage.indices.fill(65535);
  world.inventory.weapons[0].is_equipped = 1;
  world.inventory.blades[15].opaque12 = 32769;
  world.inventory.misc[63] = {.id = 65535, .count = 255, .index = 99};
  world.indices.misc[0] = 255;
  items::SnapshotGadgetBag(world);
  CHECK(world.storage.items.misc[63] == world.inventory.misc[63]);
  CHECK(world.storage.items.misc[255].id == 0);
  CHECK(world.storage.items.weapons[31].index == 0);
  CHECK(world.storage.items.weapons[0].is_equipped == 1);
  CHECK(world.storage.items.blades[15].opaque12 == 32769);
  CHECK(world.BagOrder(ItemCategory::kMisc)[0] == 255);
  CHECK(world.BagOrder(ItemCategory::kMisc)[64] == 0);
}

TEST_CASE("F02938 repeated entry clears stale records and display rows",
          "[container-transfers][F02938]") {
  BagWorld world;
  world.inventory.misc[0].id = 9;
  world.indices.misc[0] = 1;
  items::SnapshotGadgetBag(world);
  world.inventory.misc[0] = {};
  world.indices.misc[0] = 0;
  items::SnapshotGadgetBag(world);
  CHECK(world.storage.items.misc[0].id == 0);
  CHECK(world.BagOrder(ItemCategory::kMisc)[0] == 0);
  CHECK(world.clears == 2);
}

TEST_CASE(
    "F02939 keeps container slots distinct from truncated inventory display "
    "tokens",
    "[container-transfers][F02939]") {
  BagWorld world;
  world.storage.items.misc[0] = {.id = 5, .count = 3, .index = 71};
  world.storage.items.misc[255] = {.id = 6, .count = 4, .index = 72};
  auto order = world.BagOrder(ItemCategory::kMisc);
  order[0] = 256;
  order[1] = 1;
  items::CommitGadgetBag(world);
  CHECK(world.indices.misc[0] == 0);
  CHECK(world.indices.misc[1] == 1);
  CHECK(world.inventory.misc[0].id == 5);
  CHECK(world.inventory.misc[0].index == 71);
  CHECK(order[0] == 256);
  CHECK(world.storage.items.misc[255].id == 6);
  CHECK(world.order_calls[0] == 8);
}

TEST_CASE(
    "F02939 repeated commit preserves duplicate rows and copies empty record "
    "metadata",
    "[container-transfers][F02939]") {
  BagWorld world;
  world.storage.items.blades[15].material = 65535;
  world.storage.items.grips[0].id = 2;
  auto order = world.BagOrder(ItemCategory::kGrip);
  order[0] = 1;
  order[2] = 1;
  items::CommitGadgetBag(world);
  CHECK(world.indices.grips[0] == 1);
  CHECK(world.indices.grips[1] == 1);
  CHECK(world.inventory.blades[15].material == 65535);
  world.storage.items.grips[0].id = 0;
  items::CommitGadgetBag(world);
  CHECK(world.indices.grips[0] == 0);
  CHECK(world.inventory.grips[0].id == 0);
}

TEST_CASE(
    "F02940 returns parent field widths without masking nonshield upper bits",
    "[container-transfers][F02940]") {
  items::ContainerStorage owner;
  owner.items.blades[63].assembled_weapon_index = 255;
  owner.items.grips[63].assembled_weapon_index = 65535;
  owner.items.gems[191].set_item_index = 0xFF01;
  CHECK(items::ContainerParentWeaponIndex(ItemCategory::kBlade, 63, owner) ==
        255);
  CHECK(items::ContainerParentWeaponIndex(ItemCategory::kGrip, 63, owner) ==
        65535);
  CHECK(items::ContainerParentWeaponIndex(ItemCategory::kGem, 191, owner) ==
        0xFF01);
  owner.items.gems[191].set_item_index = 0xFF81;
  CHECK(items::ContainerParentWeaponIndex(ItemCategory::kGem, 191, owner) == 0);
  CHECK(items::ContainerParentWeaponIndex(ItemCategory::kArmor, -1, owner) ==
        0);
}

TEST_CASE(
    "F02941 returns only shield tagged seven-bit parent and checks used slots",
    "[container-transfers][F02941]") {
  items::ContainerStorage owner;
  owner.items.gems[191].set_item_index = 0xFF81;
  CHECK(items::ContainerParentShieldIndex(ItemCategory::kGem, 191, owner) == 1);
  owner.items.gems[191].set_item_index = 0xFF01;
  CHECK(items::ContainerParentShieldIndex(ItemCategory::kGem, 191, owner) == 0);
  CHECK(items::ContainerParentShieldIndex(ItemCategory::kBlade, -1, owner) ==
        0);
  CHECK_THROWS_AS(
      items::ContainerParentShieldIndex(ItemCategory::kGem, 192, owner),
      std::out_of_range);
  CHECK_THROWS_AS(
      items::ContainerParentWeaponIndex(ItemCategory::kGrip, -1, owner),
      std::out_of_range);
}
