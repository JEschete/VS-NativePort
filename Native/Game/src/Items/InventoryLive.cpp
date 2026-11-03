// Hand-maintained (tools/port/owned.csv).
// Live binding for the inventory rules (Inventory.hpp): Ashley's inventory
// (Audio_Seq_unusedInventory), its menu display order
// (Audio_Seq_unusedInventoryIndices), her battle actor and the misc stack the
// item menu last invoked.
#include "Items/InventoryLive.hpp"

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <span>
#include <type_traits>

#include "Items/EquipmentStats.hpp"
#include "Items/Inventory.hpp"
#include "Items/InventoryLiveState.h"
#include "Items/ItemRecords.hpp"
#include "Items/ItemRecordsLive.hpp"

extern "C" {
#include "Audio/Seq/Channel.h"
#include "Items/Categories.h"

unsigned char Items_miscItemInvoked = 0;
const unsigned char Items_Inventory_inventoryItemCapacities[8] = {
    VS::Items::kCategoryCapacities[0], VS::Items::kCategoryCapacities[1],
    VS::Items::kCategoryCapacities[2], VS::Items::kCategoryCapacities[3],
    VS::Items::kCategoryCapacities[4], VS::Items::kCategoryCapacities[5],
    VS::Items::kCategoryCapacities[6], VS::Items::kCategoryCapacities[7],
};
}

static_assert(std::size(Items_Inventory_inventoryItemCapacities) ==
              VS::Items::kItemCategoryCount);
static_assert(std::extent_v<decltype(Items_itemCategoryUpperIds)> ==
              VS::Items::kItemIdCategoryCount);
static_assert(sizeof(Items_itemCategoryUpperIds[0]) == sizeof(std::uint32_t));

namespace VS::Items {
namespace {
using Live::Commit;
using Live::Typed;
using Live::WarnNullRecord;
using Live::WarnRefusal;

// A menu category the copied callers pass; one outside 0-7 is reported.
std::optional<ItemCategory> ToCategory(int category, const char* operation) {
  if (category < 0 ||
      static_cast<std::size_t>(category) >= kItemCategoryCount) {
    WarnRefusal(operation, ItemsStatus::kCategoryOutOfRange);
    return std::nullopt;
  }
  return static_cast<ItemCategory>(category);
}

// A category with a display order; the manage row has none (the original read
// past Menu_Inventory_inventoryIndices for it).
std::optional<ItemCategory> ToListedCategory(int category,
                                             const char* operation) {
  if (category == static_cast<int>(ItemCategory::kManage)) {
    WarnRefusal(operation, ItemsStatus::kCategoryOutOfRange);
    return std::nullopt;
  }
  return ToCategory(category, operation);
}

const Items_Inventory& OrAshleyInventory(const Items_Inventory* inventory) {
  return inventory != nullptr ? *inventory : Audio_Seq_unusedInventory;
}
}  // namespace

// decomp: BATTLE.PRG vs_battle_itemIdIsInInventory
// (src/BATTLE/BATTLE.PRG/146C.c:9646)
int CountLiveInventoryItem(int id) {
  const std::span<const std::uint32_t, kItemIdCategoryCount> upper_ids(
      Items_itemCategoryUpperIds);
  return InventoryItemCount(id, upper_ids, Typed(Audio_Seq_unusedInventory));
}

// decomp: BATTLE.PRG vs_battle_decreaseMiscCount
// (src/BATTLE/BATTLE.PRG/5BF94.c:4454)
int ConsumeLiveMiscItem(int misc_id) {
  auto inventory = Typed(Audio_Seq_unusedInventory);
  std::uint8_t invoked_slot = Items_miscItemInvoked;
  const bool consumed = ConsumeMiscItem(misc_id, invoked_slot, inventory);
  Items_miscItemInvoked = invoked_slot;
  Commit(Audio_Seq_unusedInventory, inventory);
  return consumed ? 1 : 0;
}
}  // namespace VS::Items

namespace VS::Items::Inventory {
// decomp: MAINMENU.PRG vs_mainMenu_unequipWeapon
// (src/MENU/MAINMENU.PRG/2D10.c:1008)
void UnequipLiveWeapon() {
  auto* stats = Live::AshleyStats("UnequipWeapon");
  if (stats == nullptr) return;
  auto& actor = *Battle_characterState;
  auto ashley = Live::ToAshleyWeapon(*stats, actor);
  Live::LiveEquipStatusHost host(
      *stats, [&] { Live::CommitAshleyWeapon(*stats, actor, ashley); });
  auto inventory = Typed(Audio_Seq_unusedInventory);
  const auto status = UnequipWeapon(ashley, inventory, host);
  host.Finish();
  Commit(Audio_Seq_unusedInventory, inventory);
  WarnRefusal("UnequipWeapon", status);
}

// decomp: MAINMENU.PRG vs_mainMenu_unequipShield
// (src/MENU/MAINMENU.PRG/2D10.c:1019)
void UnequipLiveShield() {
  auto* stats = Live::AshleyStats("UnequipShield");
  if (stats == nullptr) return;
  auto shield = Typed(stats->shield);
  Live::LiveEquipStatusHost host(*stats,
                                 [&] { Commit(stats->shield, shield); });
  auto inventory = Typed(Audio_Seq_unusedInventory);
  const auto status = UnequipShield(shield, inventory, host);
  host.Finish();
  Commit(Audio_Seq_unusedInventory, inventory);
  WarnRefusal("UnequipShield", status);
}

// decomp: MAINMENU.PRG vs_mainMenu_initItem
// (src/MENU/MAINMENU.PRG/2D10.c:1030)
void ClearLiveItemSlot(int category, int slot) {
  const auto typed_category = ToCategory(category, "ClearItemSlot");
  if (!typed_category) return;
  auto inventory = Typed(Audio_Seq_unusedInventory);
  const auto status = ClearItemSlot(*typed_category, slot, inventory);
  Commit(Audio_Seq_unusedInventory, inventory);
  WarnRefusal("ClearItemSlot", status);
}

// decomp: MAINMENU.PRG vs_mainMenu_findItem
// (src/MENU/MAINMENU.PRG/2D10.c:1222)
int FindLiveDisplayPosition(int category, int slot) {
  const auto typed_category = ToListedCategory(category, "FindDisplayPosition");
  if (!typed_category) return 0;
  auto indices = Typed(Audio_Seq_unusedInventoryIndices);
  return FindDisplayPosition(DisplayOrder(indices, *typed_category), slot);
}

// decomp: MAINMENU.PRG vs_mainMenu_rebuildInventory
// (src/MENU/MAINMENU.PRG/2D10.c:1254)
void RebuildLiveDisplayIndices(int category) {
  const auto typed_category =
      ToListedCategory(category, "RebuildDisplayIndices");
  if (!typed_category) return;
  auto indices = Typed(Audio_Seq_unusedInventoryIndices);
  const auto status = RebuildDisplayIndices(*typed_category, indices,
                                            Typed(Audio_Seq_unusedInventory));
  Commit(Audio_Seq_unusedInventoryIndices, indices);
  WarnRefusal("RebuildDisplayIndices", status);
}

// decomp: MAINMENU.PRG vs_mainMenu_getItemCount
// (src/MENU/MAINMENU.PRG/2D10.c:1307)
int CountLiveItems(int category, const Items_Inventory* inventory) {
  const auto typed_category = ToCategory(category, "CountItems");
  if (!typed_category) return 0;
  return CountItems(*typed_category, Typed(OrAshleyInventory(inventory)));
}

// decomp: MAINMENU.PRG vs_mainMenu_getFirstEmptyItemSlot
// (src/MENU/MAINMENU.PRG/2D10.c:1321)
int FindLiveFirstEmptySlot(int category, const Items_Inventory* inventory) {
  const auto typed_category = ToCategory(category, "FirstEmptySlot");
  if (!typed_category) return 0;
  const auto slot =
      FirstEmptySlot(*typed_category, Typed(OrAshleyInventory(inventory)));
  return static_cast<int>(slot);
}

// decomp: MAINMENU.PRG vs_mainMenu_copyItem
// (src/MENU/MAINMENU.PRG/2D10.c:1334)
// A misc copy rebuilds Ashley's misc display order and merges into the
// target's stacks in that order, whichever inventory the target is.
int CopyLiveItem(int flags, Items_Inventory* target, int source_index,
                 const Items_Inventory* source) {
  if (target == nullptr || source == nullptr) {
    WarnNullRecord("CopyItem");
    return 0;
  }
  const bool target_is_ashley = target == &Audio_Seq_unusedInventory;
  auto typed_target = Typed(*target);
  const auto typed_source =
      source == target ? std::nullopt : std::optional(Typed(*source));
  const auto ashley = target_is_ashley
                          ? std::nullopt
                          : std::optional(Typed(Audio_Seq_unusedInventory));
  auto indices = Typed(Audio_Seq_unusedInventoryIndices);
  const auto copy = CopyItem(flags, typed_target, source_index,
                             typed_source ? *typed_source : typed_target,
                             indices, ashley ? *ashley : typed_target);
  Commit(*target, typed_target);
  Commit(Audio_Seq_unusedInventoryIndices, indices);
  WarnRefusal("CopyItem", copy.status);
  return copy.slot;
}

// decomp: MAINMENU.PRG vs_mainMenu_initInventory
// (src/MENU/MAINMENU.PRG/C48.c:44)
void StoreLiveEquippedStats() {
  auto* stats = Live::AshleyStats("StoreEquippedStats");
  if (stats == nullptr) return;
  EquippedLoadouts loadouts{
      .weapon = Typed(stats->weapon),
      .shield = Typed(stats->shield),
      .limb_armor = {},
      .accessory = Typed(stats->accessory),
  };
  for (std::size_t limb = 0; limb < kLimbCount; ++limb) {
    loadouts.limb_armor[limb] = Typed(stats->limbs[limb].armor);
  }
  auto inventory = Typed(Audio_Seq_unusedInventory);
  const auto status = StoreEquippedStats(loadouts, inventory);
  Commit(Audio_Seq_unusedInventory, inventory);
  Commit(stats->weapon, loadouts.weapon);
  Commit(stats->shield, loadouts.shield);
  for (std::size_t limb = 0; limb < kLimbCount; ++limb) {
    Commit(stats->limbs[limb].armor, loadouts.limb_armor[limb]);
  }
  Commit(stats->accessory, loadouts.accessory);
  WarnRefusal("StoreEquippedStats", status);
}
}  // namespace VS::Items::Inventory
