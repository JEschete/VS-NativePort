#include "Items/ContainerIndexing.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <variant>

#include "Items/ContainerTables.hpp"

namespace VS::Items {
namespace {
std::size_t CategoryIndex(ItemCategory category, ContainerWorld& world) {
  const auto value = static_cast<std::size_t>(category);
  if (value >= kItemCategoryCount) {
    world.Refuse("category outside container categories");
  }
  return value;
}

std::uint16_t CheckedCapacity(ItemCategory category, ContainerWorld& world) {
  const auto category_index = CategoryIndex(category, world);
  const auto capacity = world.Capacity(category);
  if (capacity > kContainerCapacities[category_index]) {
    world.Refuse("capacity exceeds container category storage");
  }
  return capacity;
}

std::size_t RecordIndex(ItemCategory category, std::int32_t index,
                        ContainerWorld& world) {
  const auto category_index = CategoryIndex(category, world);
  if (index < 0 ||
      static_cast<std::size_t>(index) >= kContainerCapacities[category_index]) {
    world.Refuse("record index outside container category storage");
  }
  return static_cast<std::size_t>(index);
}

template <typename Record>
Record ReadRecord(ItemCategory category, std::size_t index,
                  ContainerWorld& world) {
  const auto item = world.Read(category, index);
  const auto* record = std::get_if<Record>(&item);
  if (record == nullptr) {
    world.Refuse("record type disagrees with container category");
  }
  return *record;
}

template <typename Record>
ContainerRecord ClearedRecord(std::size_t index) {
  Record item{};
  item.index = static_cast<decltype(item.index)>(index + 1U);
  return item;
}

ContainerRecord Cleared(ItemCategory category, std::size_t index) {
  switch (category) {
    case ItemCategory::kWeapon:
      return ClearedRecord<InventoryWeapon>(index);
    case ItemCategory::kBlade:
      return ClearedRecord<InventoryBlade>(index);
    case ItemCategory::kGrip:
      return ClearedRecord<InventoryGrip>(index);
    case ItemCategory::kShield:
      return ClearedRecord<InventoryShield>(index);
    case ItemCategory::kArmor:
      return ClearedRecord<InventoryArmor>(index);
    case ItemCategory::kGem:
      return ClearedRecord<InventoryGem>(index);
    case ItemCategory::kMisc:
      return ClearedRecord<InventoryMisc>(index);
    case ItemCategory::kManage:
      break;
  }
  std::terminate();
}

void ClearWeaponParts(std::size_t index, ContainerWorld& world) {
  auto weapon =
      ReadRecord<InventoryWeapon>(ItemCategory::kWeapon, index, world);
  ClearContainerItem(ItemCategory::kBlade, weapon.blade - 1, world);
  weapon = ReadRecord<InventoryWeapon>(ItemCategory::kWeapon, index, world);
  ClearContainerItem(ItemCategory::kGrip, weapon.grip - 1, world);
  for (std::size_t gem = 0; gem < kWeaponGemCount; ++gem) {
    weapon = ReadRecord<InventoryWeapon>(ItemCategory::kWeapon, index, world);
    if (weapon.gems[gem] != 0) {
      ClearContainerItem(ItemCategory::kGem, weapon.gems[gem] - 1, world);
    }
  }
}

void ClearShieldGems(std::size_t index, ContainerWorld& world) {
  for (std::size_t gem = 0; gem < kWeaponGemCount; ++gem) {
    const auto shield =
        ReadRecord<InventoryShield>(ItemCategory::kShield, index, world);
    if (shield.gems[gem] != 0) {
      ClearContainerItem(ItemCategory::kGem, shield.gems[gem] - 1, world);
    }
  }
}
}  // namespace

std::uint16_t ContainerStorage::Capacity(ItemCategory category) {
  return capacities[CategoryIndex(category, *this)];
}

ContainerRecord ContainerStorage::Read(ItemCategory category,
                                       std::size_t index) {
  if (category == ItemCategory::kManage) {
    Refuse("manage category has no item records");
  }
  CategoryIndex(category, *this);
  return Live::VisitContainerTable(
      items, category, [&](const auto& table) -> ContainerRecord {
        if (index >= table.size()) {
          Refuse("record index outside native container table");
        }
        return table[index];
      });
}

void ContainerStorage::Write(ItemCategory category, std::size_t index,
                             const ContainerRecord& item) {
  if (category == ItemCategory::kManage) {
    Refuse("manage category has no item records");
  }
  CategoryIndex(category, *this);
  Live::VisitContainerTable(items, category, [&](auto& table) {
    if (index >= table.size()) {
      Refuse("record index outside native container table");
    }
    using Record =
        typename std::remove_reference_t<decltype(table)>::value_type;
    const auto* record = std::get_if<Record>(&item);
    if (record == nullptr) {
      Refuse("record type disagrees with native container table");
    }
    table[index] = *record;
  });
}

[[noreturn]] void ContainerStorage::Refuse(std::string_view reason) {
  throw std::out_of_range(std::string(reason));
}

// decomp: _getContainerIndicesOffset (src/MENU/MENUD.PRG/234.c:85)
std::size_t ContainerCategoryIndices(ItemCategory category,
                                     ContainerWorld& world) {
  const auto category_index = CategoryIndex(category, world);
  std::size_t offset = 0;
  for (std::size_t i = 0; i < category_index; ++i) {
    offset += CheckedCapacity(static_cast<ItemCategory>(i), world);
  }
  return offset;
}

// decomp: _getContainerItemIndex (src/MENU/MENUD.PRG/234.c:97)
std::int32_t FindContainerItemRow(ItemCategory category, std::int32_t index,
                                  std::span<const std::uint16_t> indices,
                                  ContainerWorld& world) {
  const auto capacity = CheckedCapacity(category, world);
  if (capacity == 0) return 0;
  const auto listed = static_cast<std::uint32_t>(index) + 1U;
  for (std::size_t row = 0; row < capacity; ++row) {
    if (row >= indices.size()) {
      world.Refuse("missing or short container display order");
    }
    if (indices[row] == listed) return static_cast<std::int32_t>(row) + 1;
  }
  return 0;
}

// decomp: _setContainerItemIndex (src/MENU/MENUD.PRG/234.c:109)
void AppendContainerItemIndex(ItemCategory category, std::int32_t index,
                              std::span<std::uint16_t> indices,
                              ContainerWorld& world) {
  for (std::size_t row = 0; row < CheckedCapacity(category, world); ++row) {
    if (row >= indices.size()) {
      world.Refuse("missing or short container display order");
    }
    if (indices[row] == 0) {
      indices[row] =
          static_cast<std::uint16_t>(static_cast<std::uint32_t>(index) + 1U);
      return;
    }
  }
}

// decomp: _initContainerObject (src/MENU/MENUD.PRG/234.c:121)
void ClearContainerItem(ItemCategory category, std::int32_t index,
                        ContainerWorld& world) {
  CategoryIndex(category, world);
  if (category == ItemCategory::kManage) return;
  const auto slot = RecordIndex(category, index, world);
  if (category == ItemCategory::kWeapon) ClearWeaponParts(slot, world);
  if (category == ItemCategory::kShield) ClearShieldGems(slot, world);
  world.Write(category, slot, Cleared(category, slot));
}

// decomp: _getContainerItemId (src/MENU/MENUD.PRG/234.c:191)
std::uint16_t ContainerItemId(ItemCategory category, std::int32_t index,
                              ContainerWorld& world) {
  CategoryIndex(category, world);
  if (category == ItemCategory::kManage) return 0;
  const auto slot = RecordIndex(category, index, world);
  switch (category) {
    case ItemCategory::kWeapon:
      return ReadRecord<InventoryWeapon>(category, slot, world).blade;
    case ItemCategory::kBlade:
      return ReadRecord<InventoryBlade>(category, slot, world).id;
    case ItemCategory::kGrip:
      return ReadRecord<InventoryGrip>(category, slot, world).id;
    case ItemCategory::kShield:
      return ReadRecord<InventoryShield>(category, slot, world).base.id;
    case ItemCategory::kArmor:
      return ReadRecord<InventoryArmor>(category, slot, world).id;
    case ItemCategory::kGem:
      return ReadRecord<InventoryGem>(category, slot, world).id;
    case ItemCategory::kMisc:
      return ReadRecord<InventoryMisc>(category, slot, world).id;
    case ItemCategory::kManage:
      break;
  }
  std::terminate();
}

// decomp: _countContainerItems (src/MENU/MENUD.PRG/234.c:221)
std::int32_t CountContainerItems(ItemCategory category, ContainerWorld& world) {
  std::int32_t count = 0;
  for (std::int32_t index = 0; index < CheckedCapacity(category, world);
       ++index) {
    if (ContainerItemId(category, index, world) != 0) ++count;
  }
  return count;
}

// decomp: _getEmptyContainerSlot (src/MENU/MENUD.PRG/234.c:234)
std::int32_t FindEmptyContainerSlot(ItemCategory category,
                                    ContainerWorld& world) {
  std::int32_t index = 0;
  for (; index < CheckedCapacity(category, world); ++index) {
    if (ContainerItemId(category, index, world) == 0) break;
  }
  return index;
}
}  // namespace VS::Items
