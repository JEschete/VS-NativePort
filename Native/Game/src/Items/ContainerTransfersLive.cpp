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

#include "Core/Diagnostics.hpp"
#include "Items/ContainerIndexingLive.hpp"
#include "Items/ContainerTables.hpp"
#include "Items/ContainerTransfers.hpp"
#include "Items/InventoryLive.hpp"
#include "Items/ItemRecords.hpp"
#include "Items/ItemRecordsLive.hpp"
#include "Menu/Container/ItemView.hpp"
#include "Menu/Container/Storage.hpp"
#include "Menu/Framework/FrameworkLiveState.hpp"
#include "Menu/Inventory/ItemList.hpp"

namespace VS::Items {
namespace {
constexpr unsigned kTransferCategoryMask = 0xF;
constexpr unsigned kTransferCopyFlag = 0x10;
constexpr int kTransferParentShift = 8;
constexpr auto kRecordCategories = kItemCategoryCount - 1;
using OrderTables = std::array<std::span<std::uint16_t>, kRecordCategories>;

[[noreturn]] void Refuse(std::string_view reason) {
  VS::Core::Errorf("items.container", "Container transfer refused: %.*s",
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

VS::Menu::container::InventoryStorage& Storage() {
  if (VS::Menu::Framework::inventoryStorage == nullptr) {
    Refuse("missing inventory-storage owner");
  }
  return *VS::Menu::Framework::inventoryStorage;
}

OrderTables Orders(VS::Items::ContainerIndices& indices) {
  return {indices.weapons, indices.blades, indices.grips, indices.shields,
          indices.armor,   indices.gems,   indices.misc};
}

class LiveBag final : public GadgetBagWorld {
 public:
  LiveBag() : owner_(&Storage().gadget_bag), bag_(&owner_->items) {}
  ContainerWorld& Bag() override { return bag_; }
  void ClearBagOrder() override {
    auto tables = Orders(Storage().gadget_bag.indices);
    for (auto table = tables.rbegin(); table != tables.rend(); ++table) {
      std::fill(table->rbegin(), table->rend(), 0);
    }
  }
  std::span<std::uint16_t> BagOrder(ItemCategory category) override {
    return LiveContainerCategoryOrder(category, owner_);
  }
  std::uint8_t InventoryCapacity(ItemCategory category) override {
    return VS::Items::kCategoryCapacities[static_cast<std::size_t>(category)];
  }
  std::span<std::uint8_t> InventoryOrder(ItemCategory category) override {
    const auto value = static_cast<std::size_t>(category);
    auto* pointer = VS::Menu::Inventory::categoryOrders[value];
    if (pointer == nullptr) return {};
    return {pointer, kCategoryCapacities[value]};
  }
  ContainerRecord ReadInventory(ItemCategory category,
                                std::size_t index) override {
    return Live::VisitContainerTable(
        inventory, category, [&](const auto& table) -> ContainerRecord {
          if (index >= std::size(table)) {
            Refuse("record index outside inventory category storage");
          }
          return table[index];
        });
  }
  void WriteInventory(ItemCategory category, std::size_t index,
                      const ContainerRecord& item) override {
    Live::VisitContainerTable(inventory, category, [&](auto& table) {
      if (index >= std::size(table)) {
        Refuse("record index outside inventory category storage");
      }
      using Record = std::remove_cvref_t<decltype(table[index])>;
      const auto* record = std::get_if<Record>(&item);
      if (record == nullptr) {
        Refuse("record type disagrees with inventory category");
      }
      table[index] = *record;
    });
  }

 private:
  VS::Items::Container* const owner_;
  LiveContainerWorld bag_;
};

std::uint16_t Preview(bool count) {
  auto& storage = Storage();
  LiveContainerWorld preview(&storage.transfer_preview);
  LiveContainerWorld other(VS::Menu::container::otherStorage == nullptr
                               ? nullptr
                               : &VS::Menu::container::otherStorage->items);
  LiveContainerWorld staged(&storage.staged_transfer);
  if (count) {
    return RefreshMiscTransferCount(preview, other, staged,
                                    storage.items_to_transfer);
  }
  PreviewTransferredMisc(preview, other, staged, storage.items_to_transfer);
  return 0;
}
}  // namespace

std::span<std::uint16_t> LiveContainerCategoryOrder(
    ItemCategory category, VS::Items::Container* container) {
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

// decomp: func_801031A0 (src/MENU/MENUD.PRG/234.c:262)
void PreviewLiveTransferredMisc() { static_cast<void>(Preview(false)); }

// decomp: func_80103270 (src/MENU/MENUD.PRG/234.c:279)
void RefreshLiveMiscTransferCount() {
  VS::Menu::container::transferredMiscCount = Preview(true);
}

// decomp: copyContainerItem (src/MENU/MENUD.PRG/234.c:286; was
// _copyContainerItem)
int CopyLiveContainerItem(int category_flags, VS::Items::ContainerData* target,
                          int source_slot, VS::Items::ContainerData* source) {
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

// decomp: defragmentContainerItems (src/MENU/MENUD.PRG/234.c:501; was
// _defragmentContainerItems)
void RebuildLiveContainerCategoryIndices(int category,
                                         VS::Items::Container* container) {
  const auto typed_category = Category(category);
  auto order = LiveContainerCategoryOrder(typed_category, container);
  LiveContainerWorld world(container == nullptr ? nullptr : &container->items);
  RebuildContainerCategoryIndices(typed_category, order, world);
}

// decomp: copyInventoryToContainer (src/MENU/MENUD.PRG/234.c:558; was
// _copyInventoryToContainer)
void SnapshotLiveGadgetBag() {
  LiveBag world;
  SnapshotGadgetBag(world);
}

// decomp: copyContainerToInventory (src/MENU/MENUD.PRG/234.c:595; was
// _copyContainerToInventory)
void CommitLiveGadgetBag() {
  LiveBag world;
  CommitGadgetBag(world);
}

// decomp: getParentItemIndex (src/MENU/MENUD.PRG/234.c:628; was
// _getParentItemIndex)
int LiveContainerParentWeaponIndex(int category, int index,
                                   VS::Items::ContainerData* container) {
  if (category != static_cast<int>(ItemCategory::kBlade) &&
      category != static_cast<int>(ItemCategory::kGrip) &&
      category != static_cast<int>(ItemCategory::kGem)) {
    return 0;
  }
  LiveContainerWorld world(container);
  return ContainerParentWeaponIndex(static_cast<ItemCategory>(category), index,
                                    world);
}

// decomp: getSetShieldIndex (src/MENU/MENUD.PRG/234.c:647; was
// _getSetShieldIndex)
int LiveContainerParentShieldIndex(int category, int index,
                                   VS::Items::ContainerData* container) {
  if (category != static_cast<int>(ItemCategory::kGem)) return 0;
  LiveContainerWorld world(container);
  return ContainerParentShieldIndex(ItemCategory::kGem, index, world);
}
}  // namespace VS::Items
