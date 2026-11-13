#include "Menu/Container/ContainerCapacityLive.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "Items/ContainerIndexingLive.hpp"
#include "Items/ItemRecords.hpp"
#include "Menu/Container/ContainerCapacity.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Render/GameFontLive.hpp"

extern "C" {
#include "Core/Diagnostics.h"
#include "Items/InventoryLiveState.h"
#include "Menu/Container/ItemView.h"
#include "Menu/Container/Storage.h"
#include "Menu/Framework/FrameworkLiveState.h"
}

namespace VS::Menu::container {
namespace {
[[noreturn]] void Refuse(std::string_view reason) {
  Core_Errorf("menu.container", "Container capacity/staging refused: %.*s",
              static_cast<int>(reason.size()), reason.data());
  std::abort();
}

Items::ItemCategory Category(int category) {
  if (category < 0 ||
      static_cast<std::size_t>(category) >= Items::kItemCategoryCount) {
    Refuse("capacity category outside container categories");
  }
  return static_cast<Items::ItemCategory>(category);
}

std::uint16_t Limit(StorageArea area, Items::ItemCategory category) {
  const auto index = static_cast<std::size_t>(category);
  if (index >= Items::kItemCategoryCount) {
    Refuse("capacity category outside container categories");
  }
  return area == StorageArea::kGadgetBag
             ? Items_Inventory_inventoryItemCapacities[index]
             : Menu_Container_containerItemCapacities[index];
}

class LiveWorld final : public StorageCapacityWorld,
                        public CapacityTransferWorld {
 public:
  void DrawNumber(const Framework::NumberText& number) override {
    Framework::DrawLiveColoredInteger(number.value,
                                      static_cast<int>(number.position),
                                      static_cast<int>(number.color), nullptr);
  }
  void DrawLabel(std::string_view text, std::uint32_t position,
                 std::uint32_t color) override {
    const std::string terminated(text);
    Render::DrawLiveCompactTextColor(terminated.c_str(),
                                     static_cast<int>(position),
                                     static_cast<int>(color), nullptr);
  }
  std::int32_t Occupied(StorageArea area,
                        Items::ItemCategory category) override {
    auto* const storage = Menu_Framework_unusedInventoryStorage;
    Items_Container* const owner = storage == nullptr ? nullptr
                                   : area == StorageArea::kGadgetBag
                                       ? &storage->gadgetBag
                                       : &storage->container;
    Items::LiveContainerWorld records(owner == nullptr ? nullptr
                                                       : &owner->items);
    return Items::CountContainerItems(category, records);
  }
  std::uint16_t Limit(StorageArea area, Items::ItemCategory category) override {
    return container::Limit(area, category);
  }
  Items::ContainerWorld& Active() override {
    auto* const owner = Menu_Container_containerData;
    return active_.emplace(owner == nullptr ? nullptr : &owner->items);
  }
  Items::ContainerWorld& Receiving() override {
    auto* const owner = Menu_Container_otherStorage;
    return receiving_.emplace(owner == nullptr ? nullptr : &owner->items);
  }
  Items::ContainerWorld& Staging() override {
    auto* const storage = Menu_Framework_unusedInventoryStorage;
    return staging_.emplace(storage == nullptr ? nullptr
                                               : &storage->stagedTransfer);
  }
  std::span<const std::uint16_t> PendingTransfers() override {
    auto* const storage = Menu_Framework_unusedInventoryStorage;
    if (storage == nullptr) Refuse("missing pending-transfer owner");
    return storage->itemsToTransfer;
  }
  std::uint16_t ReceivingLimit(Items::ItemCategory category) override {
    const auto area = Menu_Container_browsingGadgetBag != 0
                          ? StorageArea::kContainer
                          : StorageArea::kGadgetBag;
    return Limit(area, category);
  }
  [[noreturn]] void Refuse(std::string_view reason) override {
    container::Refuse(reason);
  }

 private:
  std::optional<Items::LiveContainerWorld> active_;
  std::optional<Items::LiveContainerWorld> receiving_;
  std::optional<Items::LiveContainerWorld> staging_;
};
}  // namespace

// decomp: func_80106464 (src/MENU/MENUD.PRG/234.c:1925)
void DrawLiveCapacityCount(int limit, int position, int occupied) {
  LiveWorld world;
  DrawCapacityCount(world, {.limit = limit,
                            .position = static_cast<std::uint32_t>(position),
                            .occupied = occupied});
}

// decomp: func_80106504 (src/MENU/MENUD.PRG/234.c:1940)
void DrawLiveStorageCapacities() {
  LiveWorld world;
  DrawStorageCapacities(world);
}

// decomp: func_801066DC (src/MENU/MENUD.PRG/234.c:1983)
int WouldExceedLiveCapacity(int category, int additional) {
  LiveWorld world;
  return WouldExceedCapacity(Category(category), additional, world) ? 1 : 0;
}

// decomp: func_80106784 (src/MENU/MENUD.PRG/234.c:1995)
int LiveTransferCapacityError(int category, int slot) {
  LiveWorld world;
  return static_cast<int>(
      TransferCapacityError({Category(category), slot}, world));
}

// decomp: func_801068BC (src/MENU/MENUD.PRG/234.c:2040)
void UndoLivePendingTransfer(int row) {
  LiveWorld world;
  UndoPendingTransfer(row, world);
}

// decomp: func_80106948 (src/MENU/MENUD.PRG/234.c:2054)
void StageLiveItemTransfer(int category, int slot) {
  LiveWorld world;
  StageItemTransfer({Category(category), slot}, world);
}
}  // namespace VS::Menu::container
