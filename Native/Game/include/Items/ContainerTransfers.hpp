#pragma once

#include <cstdint>
#include <span>

#include "Items/ContainerIndexing.hpp"

namespace VS::Items {
enum class ContainerTransferMode : std::uint8_t { kCheck, kCopy };

struct ContainerTransfer {
  ItemCategory category{};
  ContainerTransferMode mode{};
  std::int32_t parent_index{};
};

[[nodiscard]] std::int32_t CopyContainerItem(ContainerTransfer transfer,
                                             ContainerWorld& target,
                                             std::int32_t source_slot,
                                             ContainerWorld& source);
void PreviewTransferredMisc(ContainerWorld& preview, ContainerWorld& other,
                            ContainerWorld& staged,
                            std::span<const std::uint16_t> pending);
[[nodiscard]] std::uint16_t RefreshMiscTransferCount(
    ContainerWorld& preview, ContainerWorld& other, ContainerWorld& staged,
    std::span<const std::uint16_t> pending);
void RebuildContainerCategoryIndices(ItemCategory category,
                                     std::span<std::uint16_t> order,
                                     ContainerWorld& world);

// Owners provide actual records and category arrays, not packed memory.
// Inventory capacities govern display rows only; the fixed inventory record
// extents remain distinct from the larger container tables.
class GadgetBagWorld {
 public:
  GadgetBagWorld() = default;
  GadgetBagWorld(const GadgetBagWorld&) = delete;
  GadgetBagWorld& operator=(const GadgetBagWorld&) = delete;
  GadgetBagWorld(GadgetBagWorld&&) = delete;
  GadgetBagWorld& operator=(GadgetBagWorld&&) = delete;
  virtual ~GadgetBagWorld() = default;
  [[nodiscard]] virtual ContainerWorld& Bag() = 0;
  virtual void ClearBagOrder() = 0;
  [[nodiscard]] virtual std::span<std::uint16_t> BagOrder(
      ItemCategory category) = 0;
  [[nodiscard]] virtual std::uint8_t InventoryCapacity(
      ItemCategory category) = 0;
  [[nodiscard]] virtual std::span<std::uint8_t> InventoryOrder(
      ItemCategory category) = 0;
  [[nodiscard]] virtual ContainerRecord ReadInventory(ItemCategory category,
                                                      std::size_t index) = 0;
  virtual void WriteInventory(ItemCategory category, std::size_t index,
                              const ContainerRecord& record) = 0;
};

void SnapshotGadgetBag(GadgetBagWorld& world);
void CommitGadgetBag(GadgetBagWorld& world);
[[nodiscard]] std::uint16_t ContainerParentWeaponIndex(ItemCategory category,
                                                       std::int32_t index,
                                                       ContainerWorld& world);
[[nodiscard]] std::uint16_t ContainerParentShieldIndex(ItemCategory category,
                                                       std::int32_t index,
                                                       ContainerWorld& world);
}  // namespace VS::Items
