#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <variant>

#include "Items/ItemRecords.hpp"

namespace VS::Items {
using ContainerCapacities = std::array<std::uint16_t, kItemCategoryCount>;
inline constexpr std::size_t kContainerIndexEntries = 704;

struct ContainerContents {
  static constexpr ContainerCapacities kCapacities{32, 64,  64,  32,
                                                   64, 192, 256, 2};
  std::array<InventoryWeapon, kCapacities[0]> weapons{};
  std::array<InventoryBlade, kCapacities[1]> blades{};
  std::array<InventoryGrip, kCapacities[2]> grips{};
  std::array<InventoryShield,
             kCapacities[static_cast<std::size_t>(ItemCategory::kShield)]>
      shields{};
  std::array<InventoryArmor,
             kCapacities[static_cast<std::size_t>(ItemCategory::kArmor)]>
      armor{};
  std::array<InventoryGem,
             kCapacities[static_cast<std::size_t>(ItemCategory::kGem)]>
      gems{};
  std::array<InventoryMisc,
             kCapacities[static_cast<std::size_t>(ItemCategory::kMisc)]>
      misc{};
  friend bool operator==(const ContainerContents&,
                         const ContainerContents&) = default;
};
inline constexpr auto kContainerCapacities = ContainerContents::kCapacities;

class ContainerWorld {
 public:
  using Record = std::variant<InventoryWeapon, InventoryBlade, InventoryGrip,
                              InventoryShield, InventoryArmor, InventoryGem,
                              InventoryMisc>;
  ContainerWorld() = default;
  ContainerWorld(const ContainerWorld&) = delete;
  ContainerWorld& operator=(const ContainerWorld&) = delete;
  ContainerWorld(ContainerWorld&&) = delete;
  ContainerWorld& operator=(ContainerWorld&&) = delete;
  virtual ~ContainerWorld() = default;
  [[nodiscard]] virtual std::uint16_t Capacity(ItemCategory category) = 0;
  [[nodiscard]] virtual Record Read(ItemCategory category,
                                    std::size_t index) = 0;
  virtual void Write(ItemCategory category, std::size_t index,
                     const Record& item) = 0;
  [[noreturn]] virtual void Refuse(std::string_view reason) = 0;
};
using ContainerRecord = ContainerWorld::Record;

// Native-owned records and display order. Neither depends on a disk structure
// or pointer layout.
class ContainerStorage final : public ContainerWorld {
 public:
  ContainerContents items{};
  ContainerCapacities capacities = kContainerCapacities;
  std::array<std::uint16_t, kContainerIndexEntries> indices{};
  [[nodiscard]] std::uint16_t Capacity(ItemCategory category) override;
  [[nodiscard]] ContainerRecord Read(ItemCategory category,
                                     std::size_t index) override;
  void Write(ItemCategory category, std::size_t index,
             const ContainerRecord& item) override;
  [[noreturn]] void Refuse(std::string_view reason) override;
};

// An element position, not a byte offset or a pointer. The manage row names the
// one-past position; this query reads only the capacities preceding its
// category.
[[nodiscard]] std::size_t ContainerCategoryIndices(ItemCategory category,
                                                   ContainerWorld& world);
[[nodiscard]] std::int32_t FindContainerItemRow(
    ItemCategory category, std::int32_t index,
    std::span<const std::uint16_t> indices, ContainerWorld& world);
// The stored one-based token is truncated to a halfword, including -1 -> 0.
void AppendContainerItemIndex(ItemCategory category, std::int32_t index,
                              std::span<std::uint16_t> indices,
                              ContainerWorld& world);
// Zero-based record index. Invalid attached references refuse at their original
// point of use; earlier completed clears are not rolled back.
void ClearContainerItem(ItemCategory category, std::int32_t index,
                        ContainerWorld& world);
[[nodiscard]] std::uint16_t ContainerItemId(ItemCategory category,
                                            std::int32_t index,
                                            ContainerWorld& world);
[[nodiscard]] std::int32_t CountContainerItems(ItemCategory category,
                                               ContainerWorld& world);
// Full tables return their capacity, not -1 or a fabricated free slot.
[[nodiscard]] std::int32_t FindEmptyContainerSlot(ItemCategory category,
                                                  ContainerWorld& world);
}  // namespace VS::Items
