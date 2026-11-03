// Hand-maintained (tools/port/owned.csv).
#include "Items/Loot.hpp"  // NOLINT(build/include): native C++ interface.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Items/EquipmentStats.hpp"
#include "Items/ItemRecords.hpp"

namespace VS::Items {
namespace {
bool DrawDrops(std::uint8_t drop_rate, ItemRandom& random) {
  return random.NextBounded(kDropRateDraw) < drop_rate;
}

void CopyPresentGems(std::span<InventoryGem, kWeaponGemCount> looted,
                     std::span<const EquipmentPart, kWeaponGemCount> base) {
  for (std::size_t i = 0; i < kWeaponGemCount; ++i) {
    if (base[i].id != 0) WriteInventoryGemStats(looted[i], base[i]);
  }
}
}  // namespace

// decomp: BATTLE.PRG _dropWeaponRand (src/BATTLE/BATTLE.PRG/146C.c:1608)
bool RollEnemyWeaponDrop(LootedWeapon& looted, const WeaponLoadout& base,
                         ItemRandom& random) {
  if (!DrawDrops(base.drop_rate, random)) return false;
  WriteInventoryBladeStats(looted.blade, base.blade);
  WriteInventoryGripStats(looted.grip, base.grip);
  CopyPresentGems(looted.gems, base.gems);
  looted.name = base.name;
  looted.header.difficulty_flags = kLootDropped;
  return true;
}

// decomp: BATTLE.PRG _dropShieldRand (src/BATTLE/BATTLE.PRG/146C.c:1632)
bool RollEnemyShieldDrop(LootedShield& looted, const ShieldLoadout& base,
                         ItemRandom& random) {
  if (!DrawDrops(base.drop_rate, random)) return false;
  WriteInventoryArmorStats(looted.shield, base.base);
  CopyPresentGems(looted.gems, base.gems);
  looted.header.difficulty_flags = kLootDropped;
  return true;
}

// decomp: BATTLE.PRG _dropAccessoryRand (src/BATTLE/BATTLE.PRG/146C.c:1652)
bool RollEnemyAccessoryDrop(LootedArmor& looted,
                            const AccessorySpawnLoadout& base,
                            ItemRandom& random) {
  if (!DrawDrops(base.drop_rate, random)) return false;
  WriteInventoryArmorStats(looted.armor, base.accessory);
  looted.header.difficulty_flags = kLootDropped;
  return true;
}

// decomp: BATTLE.PRG _dropArmorRand (src/BATTLE/BATTLE.PRG/146C.c:1665)
bool RollEnemyArmorDrop(std::span<LootedArmor, kLootArmorSlots> looted,
                        const ArmorLoadout& base, ItemRandom& random) {
  if (!DrawDrops(base.drop_rate, random)) return false;
  const auto empty =
      std::find_if(looted.begin(), looted.end(),
                   [](const LootedArmor& slot) { return slot.armor.id == 0; });
  if (empty == looted.end()) return false;
  WriteInventoryArmorStats(empty->armor, base.armor);
  empty->header.difficulty_flags = kLootDropped;
  return true;
}

// decomp: BATTLE.PRG _dropMisc (src/BATTLE/BATTLE.PRG/146C.c:1684)
bool CopyGuaranteedEnemyItemDrops(
    BattleLoot& looted,
    std::span<const MiscItemStack, kGuaranteedDropCount> base) {
  bool dropped = false;
  for (std::size_t i = 0; i < kGuaranteedDropCount; ++i) {
    auto& item = looted.misc[i];
    item.item_id = base[i].id;
    if (item.item_id == 0) continue;
    item.count = base[i].count;
    item.difficulty_flags = kLootDropped;
    dropped = true;
  }
  return dropped;
}

// decomp: BATTLE.PRG _dropMiscRand (src/BATTLE/BATTLE.PRG/146C.c:1705)
bool RollEnemyItemDrop(BattleLoot& looted, const MiscItemDrop& base,
                       ItemRandom& random) {
  auto& item = looted.misc[kRandomDropSlot];
  item.item_id = base.id;
  if (item.item_id == 0 || !DrawDrops(base.drop_rate, random)) return false;
  item.count = 1;
  item.difficulty_flags = kLootDropped;
  return true;
}

// decomp: BATTLE.PRG _dropLoot (src/BATTLE/BATTLE.PRG/146C.c:1720)
// The original rolled into the reserved node; an enemy already listed (or one
// that dropped nothing) gives the node back.
void QueueEnemyLoot(const EnemyDrops& enemy, LootList& list,
                    ItemRandom& random) {
  if (!list.ReserveEntry()) return;
  BattleLoot loot{};
  bool dropped = RollEnemyWeaponDrop(loot.weapon, enemy.weapon, random);
  dropped |= RollEnemyShieldDrop(loot.shield, enemy.shield, random);
  dropped |= RollEnemyAccessoryDrop(loot.accessory, enemy.accessory, random);
  for (const auto& armor : enemy.limb_armor) {
    if (armor.armor.id != 0) {
      dropped |= RollEnemyArmorDrop(loot.armor, armor, random);
    }
  }
  dropped |= CopyGuaranteedEnemyItemDrops(loot, enemy.guaranteed_items);
  dropped |= RollEnemyItemDrop(loot, enemy.random_item, random);
  if (dropped && !list.HasEntryFrom(enemy.actor_id)) {
    list.PushReservedEntry(enemy.actor_id, loot);
    return;
  }
  list.ReleaseReservedEntry();
}

// decomp: BATTLE.PRG func_8006FB48 (src/BATTLE/BATTLE.PRG/146C.c:3372)
void OpenPendingEnemyLoot(const LootList& list, LootFlowHost& flow) {
  if (flow.RewardStage() <= kEnemyLootRewardStage) {
    flow.WaitForPostActionMovement();
    return;
  }
  if (list.IsEmpty()) {
    flow.BeginPendingAbilityReward();
    return;
  }
  flow.OpenEnemyLootList();
}

// decomp: BATTLE.PRG func_8007BA98 (src/BATTLE/BATTLE.PRG/146C.c:8470)
void PresentScriptedLoot(
    std::span<const MiscItemStack, kGuaranteedDropCount> items, LootList& list,
    LootFlowHost& flow) {
  if (list.ReserveEntry()) {
    BattleLoot loot{};
    if (CopyGuaranteedEnemyItemDrops(loot, items)) {
      list.PushReservedEntry(kScriptedLootDropper, loot);
    } else {
      list.ReleaseReservedEntry();
    }
  }
  if (list.IsEmpty()) {
    flow.ResumeAfterRewards();
    return;
  }
  flow.OpenScriptedLootList();
}

// decomp: BATTLE.PRG func_8008A6FC (src/BATTLE/BATTLE.PRG/146C.c:15596)
// Resetting the loot pages first is a port fix: room exit and a Combat Lab
// stop drop the menu's references before the entries are freed.
void ClearRoomLootList(LootList& list, LootFlowHost& flow) {
  flow.ResetLootPaging();
  list.Clear();
}

// decomp: BATTLE.PRG func_8008D508 (src/BATTLE/BATTLE.PRG/146C.c:16832)
std::optional<std::size_t> TreasureChestAtTile(
    std::span<const TreasureChest> chests, std::int32_t tile_x,
    std::int32_t tile_z, std::int32_t tile_layer) {
  const auto chest =
      std::find_if(chests.begin(), chests.end(), [&](const TreasureChest& c) {
        return c.tile_x == tile_x && c.tile_z == tile_z &&
               c.layer == tile_layer;
      });
  if (chest == chests.end()) return std::nullopt;
  return static_cast<std::size_t>(chest - chests.begin());
}

// decomp: BATTLE.PRG func_8008D710 (src/BATTLE/BATTLE.PRG/146C.c:16903)
void RestoreRoomChestStates(std::span<TreasureChest> chests,
                            ChestStateHost& host) {
  for (auto& chest : chests) {
    const auto flag =
        static_cast<std::int16_t>(chest.opened_flag + kChestOpenedFlagBase);
    if (host.ScriptFlag(flag)) {
      chest.is_open = 1;
      host.FinishLidRotation(chest.lid_rotation_id);
      host.SetTilePassability(chest, kOpenChestPassability);
    } else {
      chest.is_open = 0;
      host.SetTilePassability(chest, kClosedChestPassability);
    }
  }
}

// decomp: BATTLE.PRG func_8008E370 (src/BATTLE/BATTLE.PRG/146C.c:17285)
std::size_t RoomTreasureChests(std::optional<std::uint32_t> section_bytes) {
  return section_bytes ? *section_bytes / sizeof(TreasureChest) : 0;
}

// decomp: BATTLE.PRG _loadMpdLootSection (src/BATTLE/BATTLE.PRG/146C.c:17389)
// The original wrote the first record even when the section was too short to
// hold one.
ItemsStatus LoadRoomChestLoot(std::span<TreasureChest> chests,
                              const BattleLoot& loot) {
  if (chests.empty()) return ItemsStatus::kNoChests;
  chests.front().loot = loot;
  return ItemsStatus::kOk;
}
}  // namespace VS::Items
