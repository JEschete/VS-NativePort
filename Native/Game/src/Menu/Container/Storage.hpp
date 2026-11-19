#pragma once
// MENUD.PRG's container storage (src/MENU/MENUD.PRG/234.c): the heap block the
// container screen works in, its per-category container capacities and the
// misc count its transfer preview leaves behind.
#include <array>
#include <cstddef>
#include <cstdint>

#include "Items/ContainerIndexing.hpp"
#include "Items/ItemRecords.hpp"

namespace VS::Menu::container {
inline constexpr std::size_t kPendingTransferSlots = 64;

// The screen's working copies: Ashley's inventory before the screen opened,
// the container as stored on the card, the two transfer previews, the pending
// transfer list and the two storages the screen browses.
// decomp: vs_InventoryStorage (src/SLUS_010.40/inventory.h:145)
struct InventoryStorage {
  Items::InventoryContents saved_inventory{};
  Items::InventoryIndices saved_inventory_indices{};
  Items::ContainerData stored_container{};
  Items::ContainerData transfer_preview{};
  Items::ContainerData staged_transfer{};
  // Each pending row's token, (category << 8) | 1-based slot; 0 ends the list.
  std::array<std::uint16_t, kPendingTransferSlots> items_to_transfer{};
  Items::Container container{};
  Items::Container gadget_bag{};
};

namespace layout {
inline constexpr std::size_t kSavedInventoryIndicesOffset = 0xF00;
static_assert(offsetof(InventoryStorage, saved_inventory_indices) ==
              kSavedInventoryIndicesOffset);
inline constexpr std::size_t kStoredContainerOffset = 0xFB0;
static_assert(offsetof(InventoryStorage, stored_container) ==
              kStoredContainerOffset);
inline constexpr std::size_t kTransferPreviewOffset = 0x4BB0;
static_assert(offsetof(InventoryStorage, transfer_preview) ==
              kTransferPreviewOffset);
inline constexpr std::size_t kStagedTransferOffset = 0x87B0;
static_assert(offsetof(InventoryStorage, staged_transfer) ==
              kStagedTransferOffset);
inline constexpr std::size_t kItemsToTransferOffset = 0xC3B0;
static_assert(offsetof(InventoryStorage, items_to_transfer) ==
              kItemsToTransferOffset);
inline constexpr std::size_t kContainerOffset = 0xC430;
static_assert(offsetof(InventoryStorage, container) == kContainerOffset);
inline constexpr std::size_t kGadgetBagOffset = 0x105B0;
static_assert(offsetof(InventoryStorage, gadget_bag) == kGadgetBagOffset);
inline constexpr std::size_t kInventoryStorageBytes = 0x14730;
static_assert(sizeof(InventoryStorage) == kInventoryStorageBytes);
}  // namespace layout

// decomp: D_80109A82 (src/MENU/MENUD.PRG/234.c:257)
extern std::uint16_t transferredMiscCount;
// decomp: containerItemCapacities (src/MENU/MENUD.PRG/234.c:87): starts as
// Items::kContainerCapacities.
extern Items::ContainerCapacities containerItemCapacities;

// MENUD.PRG reloads restore the initialized statics (PORTING_WATCHLIST.md §1).
void ResetMenudStatics();
}  // namespace VS::Menu::container
