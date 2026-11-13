#include "Items/ContainerTransfersLive.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <iterator>
#include <span>
#include <string_view>
#include <type_traits>
#include <variant>

#include "Items/ContainerIndexingLive.hpp"
#include "Items/ContainerTables.hpp"
#include "Items/ContainerTransfers.hpp"
#include "Items/ItemRecordsLive.hpp"

extern "C" {
#include "Audio/Seq/Channel.h"
#include "Core/Diagnostics.h"
#include "Items/InventoryLiveState.h"
#include "Menu/Container/ItemView.h"
#include "Menu/Container/Storage.h"
#include "Menu/Framework/FrameworkLiveState.h"
#include "Menu/Inventory/ItemList.h"
}

namespace VS::Items {
namespace {
constexpr unsigned kTransferCategoryMask = 0xF;
constexpr unsigned kTransferCopyFlag = 0x10;
constexpr int kTransferParentShift = 8;
constexpr auto kRecordCategories = kItemCategoryCount - 1;
using OrderTables = std::array<std::span<std::uint16_t>, kRecordCategories>;

[[noreturn]] void Refuse(std::string_view reason) {
  Core_Errorf("items.container", "Container transfer refused: %.*s",
              static_cast<int>(reason.size()), reason.data());
  std::abort();
}

ItemCategory Category(int category) {
  if (category < 0 ||
      static_cast<std::size_t>(category) >= kItemCategoryCount) {
    Refuse("category outside container categories");
  }
  return static_cast<ItemCategory>(category);
}

Menu_Container_InventoryStorage& Storage() {
  if (Menu_Framework_unusedInventoryStorage == nullptr) {
    Refuse("missing inventory-storage owner");
  }
  return *Menu_Framework_unusedInventoryStorage;
}

OrderTables Orders(Items_ContainerIndices& indices) {
  return {indices.weapons, indices.blades, indices.grips, indices.shields,
          indices.armor,   indices.gems,   indices.misc};
}

std::span<std::uint16_t> SelectedOrder(ItemCategory category,
                                       Items_Container* container) {
  auto* selected =
      LiveContainerCategoryIndices(static_cast<int>(category), container);
  if (container == nullptr) Refuse("missing container index owner");
  for (auto table : Orders(container->indices)) {
    for (std::size_t row = 0; row < table.size(); ++row) {
      if (table.data() + row == selected) return table.subspan(row);
    }
  }
  return {};
}

class LiveBag final : public GadgetBagWorld {
 public:
  LiveBag() : owner_(&Storage().gadgetBag), bag_(&owner_->items) {}
  ContainerWorld& Bag() override { return bag_; }
  void ClearBagOrder() override {
    auto tables = Orders(Storage().gadgetBag.indices);
    for (auto table = tables.rbegin(); table != tables.rend(); ++table) {
      std::fill(table->rbegin(), table->rend(), 0);
    }
  }
  std::span<std::uint16_t> BagOrder(ItemCategory category) override {
    return SelectedOrder(category, owner_);
  }
  std::uint8_t InventoryCapacity(ItemCategory category) override {
    return Items_Inventory_inventoryItemCapacities[static_cast<std::size_t>(
        category)];
  }
  std::span<std::uint8_t> InventoryOrder(ItemCategory category) override {
    const auto value = static_cast<std::size_t>(category);
    auto* pointer = Menu_Inventory_inventoryIndices[value];
    if (pointer == nullptr) return {};
    return {pointer, kCategoryCapacities[value]};
  }
  ContainerRecord ReadInventory(ItemCategory category,
                                std::size_t index) override {
    return Live::VisitContainerTable(
        Audio_Seq_unusedInventory, category,
        [&](const auto& table) -> ContainerRecord {
          if (index >= std::size(table)) {
            Refuse("record index outside inventory category storage");
          }
          return Live::Typed(table[index]);
        });
  }
  void WriteInventory(ItemCategory category, std::size_t index,
                      const ContainerRecord& item) override {
    Live::VisitContainerTable(
        Audio_Seq_unusedInventory, category, [&](auto& table) {
          if (index >= std::size(table)) {
            Refuse("record index outside inventory category storage");
          }
          using Legacy = std::remove_reference_t<decltype(table[index])>;
          const auto* record = std::get_if<Live::TypedRecordOf<Legacy>>(&item);
          if (record == nullptr) {
            Refuse("record type disagrees with inventory category");
          }
          Live::Commit(table[index], *record);
        });
  }

 private:
  Items_Container* const owner_;
  LiveContainerWorld bag_;
};

std::uint16_t Preview(bool count) {
  auto& storage = Storage();
  LiveContainerWorld preview(&storage.transferPreview);
  LiveContainerWorld other(Menu_Container_otherStorage == nullptr
                               ? nullptr
                               : &Menu_Container_otherStorage->items);
  LiveContainerWorld staged(&storage.stagedTransfer);
  if (count) {
    return RefreshMiscTransferCount(preview, other, staged,
                                    storage.itemsToTransfer);
  }
  PreviewTransferredMisc(preview, other, staged, storage.itemsToTransfer);
  return 0;
}
}  // namespace

// decomp: func_801031A0 (src/MENU/MENUD.PRG/234.c:259)
void PreviewLiveTransferredMisc() { static_cast<void>(Preview(false)); }

// decomp: func_80103270 (src/MENU/MENUD.PRG/234.c:276)
void RefreshLiveMiscTransferCount() {
  Menu_Container_transferredMiscCount = Preview(true);
}

// decomp: _copyContainerItem (src/MENU/MENUD.PRG/234.c:283)
int CopyLiveContainerItem(int category_flags, Items_ContainerData* target,
                          int source_slot, Items_ContainerData* source) {
  const auto flags = static_cast<unsigned>(category_flags);
  const ContainerTransfer transfer{
      Category(static_cast<int>(flags & kTransferCategoryMask)),
      (flags & kTransferCopyFlag) != 0 ? ContainerTransferMode::kCopy
                                       : ContainerTransferMode::kCheck,
      category_flags >> kTransferParentShift};
  LiveContainerWorld target_world(target);
  LiveContainerWorld source_world(source);
  return CopyContainerItem(transfer, target_world, source_slot, source_world);
}

// decomp: _defragmentContainerItems (src/MENU/MENUD.PRG/234.c:498)
void RebuildLiveContainerCategoryIndices(int category,
                                         Items_Container* container) {
  const auto typed_category = Category(category);
  auto order = SelectedOrder(typed_category, container);
  LiveContainerWorld world(container == nullptr ? nullptr : &container->items);
  RebuildContainerCategoryIndices(typed_category, order, world);
}

// decomp: _copyInventoryToContainer (src/MENU/MENUD.PRG/234.c:555)
void SnapshotLiveGadgetBag() {
  LiveBag world;
  SnapshotGadgetBag(world);
}

// decomp: _copyContainerToInventory (src/MENU/MENUD.PRG/234.c:592)
void CommitLiveGadgetBag() {
  LiveBag world;
  CommitGadgetBag(world);
}

// decomp: _getParentItemIndex (src/MENU/MENUD.PRG/234.c:625)
int LiveContainerParentWeaponIndex(int category, int index,
                                   Items_ContainerData* container) {
  if (category != static_cast<int>(ItemCategory::kBlade) &&
      category != static_cast<int>(ItemCategory::kGrip) &&
      category != static_cast<int>(ItemCategory::kGem)) {
    return 0;
  }
  LiveContainerWorld world(container);
  return ContainerParentWeaponIndex(static_cast<ItemCategory>(category), index,
                                    world);
}

// decomp: _getSetShieldIndex (src/MENU/MENUD.PRG/234.c:645)
int LiveContainerParentShieldIndex(int category, int index,
                                   Items_ContainerData* container) {
  if (category != static_cast<int>(ItemCategory::kGem)) return 0;
  LiveContainerWorld world(container);
  return ContainerParentShieldIndex(ItemCategory::kGem, index, world);
}
}  // namespace VS::Items
