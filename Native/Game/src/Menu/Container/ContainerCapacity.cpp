#include "Menu/Container/ContainerCapacity.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "Items/ContainerGemCounts.hpp"
#include "Items/ContainerTransfers.hpp"
#include "Items/ItemRecords.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Render/GameFont.hpp"

namespace VS::Menu::container {
namespace {
constexpr std::uint32_t kCountSeparation = 32;
constexpr std::uint32_t kBelowHalfColor = 0x804020;
constexpr int kStorageHeadingX = 0x3A;
constexpr int kStorageHeadingY = 0x58;
constexpr int kStorageHeadingAdvance = 0x80;
constexpr int kFirstCategoryY = 0x64;
constexpr int kCategoryAdvance = 12;
constexpr int kBagCountX = 0x70;
constexpr int kCategoryLabelX = 0xA0;
constexpr int kContainerCountX = 0xF8;
constexpr std::size_t kRecordCategories = Items::kItemCategoryCount - 1;
constexpr int kPendingCategoryShift = 8;
constexpr std::uint16_t kPendingSlotMask = 0xFF;

std::uint16_t PendingToken(std::span<const std::uint16_t> pending,
                           std::size_t row, CapacityTransferWorld& world) {
  if (row >= pending.size() || row >= kPendingTransferEntries) {
    world.Refuse("pending-transfer row outside supplied storage");
  }
  return pending[row];
}
}  // namespace

// decomp: func_80106464 (src/MENU/MENUD.PRG/234.c:1925)
void DrawCapacityCount(CapacityRenderer& renderer,
                       const CapacityReadout& readout) {
  renderer.DrawNumber({.value = readout.limit,
                       .position = readout.position,
                       .color = Render::kCompactTextGray});
  const auto used_position = readout.position - kCountSeparation;
  renderer.DrawLabel("/", used_position, Render::kCompactTextGray);
  const auto doubled = static_cast<std::int32_t>(
      static_cast<std::uint32_t>(readout.occupied) * 2U);
  renderer.DrawNumber({.value = readout.occupied,
                       .position = used_position,
                       .color = doubled < readout.limit
                                    ? kBelowHalfColor
                                    : Render::kCompactTextGray});
}

// decomp: func_80106504 (src/MENU/MENUD.PRG/234.c:1940)
void DrawStorageCapacities(StorageCapacityWorld& world) {
  constexpr std::array<std::string_view, 2> kStorageLabels{"GADGET BAG",
                                                           "CONTAINER"};
  constexpr std::array<std::string_view, kRecordCategories> kCategoryLabels{
      "$WEAPON", "$BLADE", "$GRIP", "$SHIELD", "$ARMOR", "$GEM", "$MISC"};
  for (std::size_t area = 0; area < kStorageLabels.size(); ++area) {
    world.DrawLabel(
        kStorageLabels[area],
        Framework::PackXy(
            kStorageHeadingX + static_cast<int>(area) * kStorageHeadingAdvance,
            kStorageHeadingY),
        Render::kCompactTextGray);
  }
  for (std::size_t value = 0; value < kRecordCategories; ++value) {
    const auto category = static_cast<Items::ItemCategory>(value);
    const auto y = kFirstCategoryY + static_cast<int>(value) * kCategoryAdvance;
    const auto bag_count = world.Occupied(StorageArea::kGadgetBag, category);
    const auto bag_limit = world.Limit(StorageArea::kGadgetBag, category);
    DrawCapacityCount(world, {.limit = bag_limit,
                              .position = Framework::PackXy(kBagCountX, y),
                              .occupied = bag_count});
    world.DrawLabel(kCategoryLabels[value],
                    Framework::PackXy(kCategoryLabelX, y),
                    Render::kCompactTextGray);
    const auto container_count =
        world.Occupied(StorageArea::kContainer, category);
    const auto container_limit = world.Limit(StorageArea::kContainer, category);
    DrawCapacityCount(world,
                      {.limit = container_limit,
                       .position = Framework::PackXy(kContainerCountX, y),
                       .occupied = container_count});
  }
}

// decomp: func_801066DC (src/MENU/MENUD.PRG/234.c:1983)
bool WouldExceedCapacity(Items::ItemCategory category, std::int32_t additional,
                         CapacityTransferWorld& world) {
  auto& receiving = world.Receiving();
  const auto destination_count =
      Items::CountContainerItems(category, receiving);
  auto& staging = world.Staging();
  const auto staged_count = Items::CountContainerItems(category, staging);
  const auto projected =
      static_cast<std::int32_t>(static_cast<std::uint32_t>(additional) +
                                static_cast<std::uint32_t>(destination_count) +
                                static_cast<std::uint32_t>(staged_count));
  return world.ReceivingLimit(category) < projected;
}

// decomp: func_80106784 (src/MENU/MENUD.PRG/234.c:1995)
TransferError TransferCapacityError(TransferItem item,
                                    CapacityTransferWorld& world) {
  const auto pending = world.PendingTransfers();
  std::size_t occupied = 0;
  for (; occupied < kPendingTransferEntries; ++occupied) {
    if (PendingToken(pending, occupied, world) == 0) break;
  }
  if (item.category == Items::ItemCategory::kWeapon) {
    occupied += 2 + Items::CountContainerWeaponGems(item.slot, world.Active());
  }
  if (item.category == Items::ItemCategory::kShield) {
    occupied += Items::CountContainerShieldGems(item.slot, world.Active());
  }
  if (occupied >= kPendingTransferEntries) {
    return TransferError::kPendingListFull;
  }
  if (WouldExceedCapacity(item.category, 1, world)) {
    return TransferError::kDestinationFull;
  }
  if (item.category == Items::ItemCategory::kWeapon) {
    if (WouldExceedCapacity(Items::ItemCategory::kBlade, 1, world) ||
        WouldExceedCapacity(Items::ItemCategory::kGrip, 1, world)) {
      return TransferError::kDestinationFull;
    }
    const auto gems =
        Items::CountContainerWeaponGems(item.slot, world.Active());
    if (WouldExceedCapacity(Items::ItemCategory::kGem, gems, world)) {
      return TransferError::kDestinationFull;
    }
  }
  if (item.category == Items::ItemCategory::kShield) {
    const auto gems =
        Items::CountContainerShieldGems(item.slot, world.Active());
    if (WouldExceedCapacity(Items::ItemCategory::kGem, gems, world)) {
      return TransferError::kDestinationFull;
    }
  }
  return TransferError::kFits;
}

// decomp: func_801068BC (src/MENU/MENUD.PRG/234.c:2040)
void UndoPendingTransfer(std::int32_t row, CapacityTransferWorld& world) {
  const auto pending = world.PendingTransfers();
  const auto token =
      PendingToken(pending, static_cast<std::size_t>(row), world);
  const auto category =
      static_cast<Items::ItemCategory>(token >> kPendingCategoryShift);
  const auto slot = (token - 1) & kPendingSlotMask;
  auto& staging = world.Staging();
  auto& active = world.Active();
  static_cast<void>(Items::CopyContainerItem(
      {category, Items::ContainerTransferMode::kCopy, 0}, active, slot,
      staging));
  auto& current_staging = world.Staging();
  Items::ClearContainerItem(category, slot, current_staging);
}

// decomp: func_80106948 (src/MENU/MENUD.PRG/234.c:2054)
void StageItemTransfer(TransferItem item, CapacityTransferWorld& world) {
  auto& staging = world.Staging();
  auto& active = world.Active();
  static_cast<void>(Items::CopyContainerItem(
      {item.category, Items::ContainerTransferMode::kCopy, 0}, staging,
      item.slot, active));
  auto& current_active = world.Active();
  Items::ClearContainerItem(item.category, item.slot, current_active);
}
}  // namespace VS::Menu::container
