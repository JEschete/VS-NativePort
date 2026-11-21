#include "Menu/Container/ContainerCapacityLive.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "Core/Diagnostics.hpp"
#include "Items/ContainerIndexingLive.hpp"
#include "Items/ItemRecords.hpp"
#include "Menu/Container/ContainerCapacity.hpp"
#include "Menu/Container/ItemView.hpp"
#include "Menu/Container/Storage.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Framework/FrameworkLiveState.hpp"
#include "Render/GameFontLive.hpp"

namespace VS::Menu::container {
namespace {
[[noreturn]] void Refuse(std::string_view reason) {
  VS::Core::Errorf("menu.container", "Container capacity/staging refused: %.*s",
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
  return area == StorageArea::kGadgetBag ? VS::Items::kCategoryCapacities[index]
                                         : containerItemCapacities[index];
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
    auto* const storage = VS::Menu::Framework::inventoryStorage;
    VS::Items::Container* const owner = storage == nullptr ? nullptr
                                        : area == StorageArea::kGadgetBag
                                            ? &storage->gadget_bag
                                            : &storage->container;
    Items::LiveContainerWorld records(owner == nullptr ? nullptr
                                                       : &owner->items);
    return Items::CountContainerItems(category, records);
  }
  std::uint16_t Limit(StorageArea area, Items::ItemCategory category) override {
    return container::Limit(area, category);
  }
  Items::ContainerWorld& Active() override {
    auto* const owner = containerData;
    return active_.emplace(owner == nullptr ? nullptr : &owner->items);
  }
  Items::ContainerWorld& Receiving() override {
    auto* const owner = otherStorage;
    return receiving_.emplace(owner == nullptr ? nullptr : &owner->items);
  }
  Items::ContainerWorld& Staging() override {
    auto* const storage = VS::Menu::Framework::inventoryStorage;
    return staging_.emplace(storage == nullptr ? nullptr
                                               : &storage->staged_transfer);
  }
  std::span<const std::uint16_t> PendingTransfers() override {
    auto* const storage = VS::Menu::Framework::inventoryStorage;
    if (storage == nullptr) Refuse("missing pending-transfer owner");
    return storage->items_to_transfer;
  }
  std::uint16_t ReceivingLimit(Items::ItemCategory category) override {
    const auto area = browsingGadgetBag != 0 ? StorageArea::kContainer
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

// decomp: func_80106464 (src/MENU/MENUD.PRG/234.c:1926)
void DrawLiveCapacityCount(int limit, int position, int occupied) {
  LiveWorld world;
  DrawCapacityCount(world, {.limit = limit,
                            .position = static_cast<std::uint32_t>(position),
                            .occupied = occupied});
}

// decomp: func_80106504 (src/MENU/MENUD.PRG/234.c:1941)
void DrawLiveStorageCapacities() {
  LiveWorld world;
  DrawStorageCapacities(world);
}

// decomp: func_801066DC (src/MENU/MENUD.PRG/234.c:1984)
int WouldExceedLiveCapacity(int category, int additional) {
  LiveWorld world;
  return WouldExceedCapacity(Category(category), additional, world) ? 1 : 0;
}

// decomp: func_80106784 (src/MENU/MENUD.PRG/234.c:1996)
int LiveTransferCapacityError(int category, int slot) {
  LiveWorld world;
  return static_cast<int>(
      TransferCapacityError({Category(category), slot}, world));
}

// decomp: func_801068BC (src/MENU/MENUD.PRG/234.c:2041)
void UndoLivePendingTransfer(int row) {
  LiveWorld world;
  UndoPendingTransfer(row, world);
}

// decomp: func_80106948 (src/MENU/MENUD.PRG/234.c:2055)
void StageLiveItemTransfer(int category, int slot) {
  LiveWorld world;
  StageItemTransfer({Category(category), slot}, world);
}
}  // namespace VS::Menu::container
