#include "Items/ContainerTransfers.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <variant>

namespace VS::Items {
namespace {
constexpr std::int32_t kMiscStackLimit = 100;
constexpr std::size_t kTransferCategoryShift = 8;
constexpr std::uint16_t kTransferSlotMask = 0xFF;
constexpr auto kRecordCategories = kItemCategoryCount - 1;

std::size_t CategoryIndex(ItemCategory category, ContainerWorld& world) {
  const auto value = static_cast<std::size_t>(category);
  if (value >= kItemCategoryCount) {
    world.Refuse("category outside container categories");
  }
  return value;
}

template <typename Record>
Record Read(ItemCategory category, std::int32_t slot, ContainerWorld& world) {
  const auto category_index = CategoryIndex(category, world);
  if (slot < 0 ||
      static_cast<std::size_t>(slot) >= kContainerCapacities[category_index]) {
    world.Refuse("record index outside container category storage");
  }
  const auto record = world.Read(category, static_cast<std::size_t>(slot));
  const auto* typed = std::get_if<Record>(&record);
  if (typed == nullptr) {
    world.Refuse("record type disagrees with container category");
  }
  return *typed;
}

template <typename Record>
void Write(ItemCategory category, std::int32_t slot, const Record& record,
           ContainerWorld& world) {
  const auto category_index = CategoryIndex(category, world);
  if (slot < 0 ||
      static_cast<std::size_t>(slot) >= kContainerCapacities[category_index]) {
    world.Refuse("record index outside container category storage");
  }
  world.Write(category, static_cast<std::size_t>(slot), record);
}

template <typename Record>
std::int32_t GemCount(ItemCategory category, std::int32_t slot,
                      ContainerWorld& source) {
  std::int32_t count = 0;
  for (std::size_t gem = 0; gem < kWeaponGemCount; ++gem) {
    if (Read<Record>(category, slot, source).gems[gem] != 0) ++count;
  }
  return count;
}

template <typename Record>
void CopyGems(ItemCategory category, std::int32_t target_slot,
              std::int32_t source_slot, ContainerWorld& target,
              ContainerWorld& source, std::int32_t parent) {
  for (std::size_t gem = 0; gem < kWeaponGemCount; ++gem) {
    const auto reference =
        Read<Record>(category, source_slot, source).gems[gem];
    if (reference == 0) continue;
    const auto copied = CopyContainerItem(
        {ItemCategory::kGem, ContainerTransferMode::kCopy, parent}, target,
        reference - 1, source);
    auto record = Read<Record>(category, target_slot, target);
    record.gems[gem] = static_cast<std::uint8_t>(copied);
    Write(category, target_slot, record, target);
  }
}

void CopyWeapon(std::int32_t target_slot, std::int32_t source_slot,
                ContainerWorld& target, ContainerWorld& source) {
  InventoryWeapon record{};
  const auto index = target_slot + 1;
  record.index = static_cast<std::uint8_t>(index);
  Write(ItemCategory::kWeapon, target_slot, record, target);
  const auto blade =
      Read<InventoryWeapon>(ItemCategory::kWeapon, source_slot, source).blade;
  const auto copied_blade = CopyContainerItem(
      {ItemCategory::kBlade, ContainerTransferMode::kCopy, index}, target,
      blade - 1, source);
  record = Read<InventoryWeapon>(ItemCategory::kWeapon, target_slot, target);
  record.blade = static_cast<std::uint8_t>(copied_blade);
  Write(ItemCategory::kWeapon, target_slot, record, target);
  const auto grip =
      Read<InventoryWeapon>(ItemCategory::kWeapon, source_slot, source).grip;
  const auto copied_grip = CopyContainerItem(
      {ItemCategory::kGrip, ContainerTransferMode::kCopy, index}, target,
      grip - 1, source);
  record = Read<InventoryWeapon>(ItemCategory::kWeapon, target_slot, target);
  record.grip = static_cast<std::uint8_t>(copied_grip);
  Write(ItemCategory::kWeapon, target_slot, record, target);
  CopyGems<InventoryWeapon>(ItemCategory::kWeapon, target_slot, source_slot,
                            target, source, index);
  const auto name =
      Read<InventoryWeapon>(ItemCategory::kWeapon, source_slot, source).name;
  record = Read<InventoryWeapon>(ItemCategory::kWeapon, target_slot, target);
  record.name = name;
  Write(ItemCategory::kWeapon, target_slot, record, target);
}

void CopyShield(std::int32_t target_slot, std::int32_t source_slot,
                ContainerWorld& target, ContainerWorld& source) {
  Write(ItemCategory::kShield, target_slot, InventoryShield{}, target);
  const auto base =
      Read<InventoryShield>(ItemCategory::kShield, source_slot, source).base;
  auto record =
      Read<InventoryShield>(ItemCategory::kShield, target_slot, target);
  record.base = base;
  record.index = static_cast<std::uint8_t>(target_slot + 1);
  Write(ItemCategory::kShield, target_slot, record, target);
  CopyGems<InventoryShield>(ItemCategory::kShield, target_slot, source_slot,
                            target, source, (target_slot + 1) | kShieldGemFlag);
}

template <typename Record>
void CopyPart(ContainerTransfer transfer, std::int32_t target_slot,
              std::int32_t source_slot, ContainerWorld& target,
              ContainerWorld& source) {
  auto record = Read<Record>(transfer.category, source_slot, source);
  record.index = static_cast<decltype(record.index)>(target_slot + 1);
  if constexpr (requires { record.assembled_weapon_index; }) {
    record.assembled_weapon_index =
        static_cast<decltype(record.assembled_weapon_index)>(
            transfer.parent_index);
  }
  if constexpr (requires { record.set_item_index; }) {
    record.set_item_index = static_cast<std::uint16_t>(transfer.parent_index);
  }
  Write(transfer.category, target_slot, record, target);
}

std::int32_t CopyMisc(ContainerTransfer transfer, std::int32_t target_slot,
                      std::int32_t source_slot, std::int32_t item_count,
                      ContainerWorld& target, ContainerWorld& source) {
  auto remaining = static_cast<std::int32_t>(
      Read<InventoryMisc>(ItemCategory::kMisc, source_slot, source).count);
  for (std::int32_t slot = 0;
       slot <
       kContainerCapacities[static_cast<std::size_t>(ItemCategory::kMisc)];
       ++slot) {
    auto record = Read<InventoryMisc>(ItemCategory::kMisc, slot, target);
    const auto id =
        Read<InventoryMisc>(ItemCategory::kMisc, source_slot, source).id;
    if (record.id != id) continue;
    if (transfer.mode == ContainerTransferMode::kCheck) {
      remaining = std::max(0, remaining - kMiscStackLimit + record.count);
      continue;
    }
    if (kMiscStackLimit - record.count >= remaining) {
      record.count = static_cast<std::uint8_t>(record.count + remaining);
      remaining = 0;
    } else {
      remaining = remaining - kMiscStackLimit + record.count;
      record.count = kMiscStackLimit;
    }
    Write(ItemCategory::kMisc, slot, record, target);
  }
  if (remaining == 0) return 1;
  if (item_count ==
      kContainerCapacities[static_cast<std::size_t>(ItemCategory::kMisc)]) {
    return 0;
  }
  if (transfer.mode == ContainerTransferMode::kCopy) {
    const auto id =
        Read<InventoryMisc>(ItemCategory::kMisc, source_slot, source).id;
    auto record = Read<InventoryMisc>(ItemCategory::kMisc, target_slot, target);
    record.count = static_cast<std::uint8_t>(remaining);
    record.id = id;
    Write(ItemCategory::kMisc, target_slot, record, target);
  }
  return target_slot + 1;
}

std::uint16_t& OrderEntry(std::span<std::uint16_t> order, std::size_t index,
                          std::size_t capacity, ContainerWorld& world) {
  if (index >= capacity || index >= order.size()) {
    world.Refuse("missing or short container display order");
  }
  return order[index];
}

std::uint8_t& InventoryOrderEntry(std::span<std::uint8_t> order,
                                  std::size_t index, ContainerWorld& world) {
  if (index >= order.size()) {
    world.Refuse("missing or short inventory display order");
  }
  return order[index];
}

std::uint8_t InventoryCapacity(ItemCategory category, GadgetBagWorld& world) {
  const auto capacity = world.InventoryCapacity(category);
  if (capacity > kCategoryCapacities[static_cast<std::size_t>(category)]) {
    world.Bag().Refuse("capacity exceeds inventory category storage");
  }
  return capacity;
}

void ClearBagRecords(ContainerWorld& world) {
  // rMemzero clears the complete owner backwards, without recursive item
  // deletion or one-based index initialization.
  constexpr std::array<ItemCategory, kRecordCategories> kReverseLayout{
      ItemCategory::kMisc,  ItemCategory::kGem,   ItemCategory::kArmor,
      ItemCategory::kGrip,  ItemCategory::kBlade, ItemCategory::kShield,
      ItemCategory::kWeapon};
  const std::array<ContainerRecord, kRecordCategories> empty{
      InventoryWeapon{}, InventoryBlade{}, InventoryGrip{}, InventoryShield{},
      InventoryArmor{},  InventoryGem{},   InventoryMisc{}};
  for (const auto category : kReverseLayout) {
    const auto size = kContainerCapacities[static_cast<std::size_t>(category)];
    for (std::size_t slot = size; slot != 0; --slot) {
      world.Write(category, slot - 1,
                  empty[static_cast<std::size_t>(category)]);
    }
  }
}
}  // namespace

// decomp: _copyContainerItem (src/MENU/MENUD.PRG/234.c:283)
std::int32_t CopyContainerItem(ContainerTransfer transfer,
                               ContainerWorld& target, std::int32_t source_slot,
                               ContainerWorld& source) {
  const auto count = CountContainerItems(transfer.category, target);
  const auto slot = FindEmptyContainerSlot(transfer.category, target);
  const auto category = CategoryIndex(transfer.category, target);
  if (transfer.category == ItemCategory::kManage) return 0;
  if (transfer.category == ItemCategory::kMisc) {
    return CopyMisc(transfer, slot, source_slot, count, target, source);
  }
  if (count == kContainerCapacities[category]) return 0;
  if (transfer.category == ItemCategory::kWeapon) {
    if (CountContainerItems(ItemCategory::kBlade, target) ==
            kContainerCapacities[static_cast<std::size_t>(
                ItemCategory::kBlade)] ||
        CountContainerItems(ItemCategory::kGrip, target) ==
            kContainerCapacities[static_cast<std::size_t>(
                ItemCategory::kGrip)]) {
      return 0;
    }
  }
  if (transfer.category == ItemCategory::kWeapon ||
      transfer.category == ItemCategory::kShield) {
    const auto gems =
        transfer.category == ItemCategory::kWeapon
            ? GemCount<InventoryWeapon>(transfer.category, source_slot, source)
            : GemCount<InventoryShield>(transfer.category, source_slot, source);
    if (CountContainerItems(ItemCategory::kGem, target) + gems >
        kContainerCapacities[static_cast<std::size_t>(ItemCategory::kGem)]) {
      return 0;
    }
  }
  if (transfer.mode == ContainerTransferMode::kCheck) return slot + 1;
  switch (transfer.category) {
    case ItemCategory::kWeapon:
      CopyWeapon(slot, source_slot, target, source);
      break;
    case ItemCategory::kBlade:
      CopyPart<InventoryBlade>(transfer, slot, source_slot, target, source);
      break;
    case ItemCategory::kGrip:
      CopyPart<InventoryGrip>(transfer, slot, source_slot, target, source);
      break;
    case ItemCategory::kShield:
      CopyShield(slot, source_slot, target, source);
      break;
    case ItemCategory::kArmor:
      CopyPart<InventoryArmor>(transfer, slot, source_slot, target, source);
      break;
    case ItemCategory::kGem:
      CopyPart<InventoryGem>(transfer, slot, source_slot, target, source);
      break;
    case ItemCategory::kMisc:
    case ItemCategory::kManage:
      break;
  }
  return slot + 1;
}

// decomp: func_801031A0 (src/MENU/MENUD.PRG/234.c:259)
void PreviewTransferredMisc(ContainerWorld& preview, ContainerWorld& other,
                            ContainerWorld& staged,
                            std::span<const std::uint16_t> pending) {
  constexpr auto kMisc = ItemCategory::kMisc;
  for (std::int32_t slot = 0;
       slot < kContainerCapacities[static_cast<std::size_t>(kMisc)]; ++slot) {
    Write(kMisc, slot, Read<InventoryMisc>(kMisc, slot, other), preview);
  }
  for (std::size_t row = 0;; ++row) {
    if (row >= pending.size()) {
      preview.Refuse("missing pending-transfer terminator");
    }
    const auto token = pending[row];
    if (token == 0) return;
    if ((token >> kTransferCategoryShift) == static_cast<unsigned>(kMisc)) {
      static_cast<void>(
          CopyContainerItem({kMisc, ContainerTransferMode::kCopy, 0}, preview,
                            (token - 1) & kTransferSlotMask, staged));
    }
  }
}

// decomp: func_80103270 (src/MENU/MENUD.PRG/234.c:276)
std::uint16_t RefreshMiscTransferCount(ContainerWorld& preview,
                                       ContainerWorld& other,
                                       ContainerWorld& staged,
                                       std::span<const std::uint16_t> pending) {
  PreviewTransferredMisc(preview, other, staged, pending);
  return static_cast<std::uint16_t>(
      CountContainerItems(ItemCategory::kMisc, preview));
}

// decomp: _defragmentContainerItems (src/MENU/MENUD.PRG/234.c:498)
void RebuildContainerCategoryIndices(ItemCategory category,
                                     std::span<std::uint16_t> order,
                                     ContainerWorld& world) {
  const auto maximum = kContainerCapacities[CategoryIndex(category, world)];
  const auto capacity = world.Capacity(category);
  if (capacity > maximum) {
    world.Refuse("capacity exceeds container category storage");
  }
  for (std::size_t row = 0; row < capacity; ++row) {
    auto& token = OrderEntry(order, row, capacity, world);
    if (token != 0 && ContainerItemId(category, token - 1, world) == 0) {
      token = 0;
    }
  }
  if (capacity == 0) {
    world.Refuse("zero capacity cannot rebuild container order");
  }
  std::size_t row = 0;
  while (true) {
    if (OrderEntry(order, row, capacity, world) != 0) {
      ++row;
      if (row == static_cast<std::size_t>(capacity - 1)) break;
      continue;
    }
    auto next = row + 1;
    for (; next < capacity; ++next) {
      if (OrderEntry(order, next, capacity, world) != 0) break;
    }
    if (next == capacity) break;
    for (auto source = next; source < capacity; ++source) {
      OrderEntry(order, source + row - next, capacity, world) =
          OrderEntry(order, source, capacity, world);
    }
    for (std::size_t tail = capacity + row - next; tail < capacity; ++tail) {
      OrderEntry(order, tail, capacity, world) = 0;
    }
  }
  for (std::int32_t slot = 0; slot < capacity; ++slot) {
    if (ContainerItemId(category, slot, world) != 0 &&
        FindContainerItemRow(category, slot, order, world) == 0) {
      AppendContainerItemIndex(category, slot, order, world);
    }
  }
}

// decomp: _copyInventoryToContainer (src/MENU/MENUD.PRG/234.c:555)
void SnapshotGadgetBag(GadgetBagWorld& world) {
  ClearBagRecords(world.Bag());
  world.ClearBagOrder();
  for (std::size_t value = 0; value < kRecordCategories; ++value) {
    const auto category = static_cast<ItemCategory>(value);
    auto source = world.InventoryOrder(category);
    auto target = world.BagOrder(category);
    for (std::size_t row = 0; row < InventoryCapacity(category, world); ++row) {
      const auto token = InventoryOrderEntry(source, row, world.Bag());
      OrderEntry(target, row, kContainerCapacities[value], world.Bag()) = token;
    }
  }
  for (std::size_t value = 0; value < kRecordCategories; ++value) {
    const auto category = static_cast<ItemCategory>(value);
    for (std::size_t slot = 0; slot < kCategoryCapacities[value]; ++slot) {
      world.Bag().Write(category, slot, world.ReadInventory(category, slot));
    }
  }
}

// decomp: _copyContainerToInventory (src/MENU/MENUD.PRG/234.c:592)
void CommitGadgetBag(GadgetBagWorld& world) {
  for (std::size_t value = 0; value < kRecordCategories; ++value) {
    const auto category = static_cast<ItemCategory>(value);
    auto target = world.InventoryOrder(category);
    auto source = world.BagOrder(category);
    // The original rebuilds every category for each inventory category.
    for (std::size_t other = 0; other < kRecordCategories; ++other) {
      const auto rebuilt = static_cast<ItemCategory>(other);
      RebuildContainerCategoryIndices(rebuilt, world.BagOrder(rebuilt),
                                      world.Bag());
    }
    for (std::size_t row = 0; row < InventoryCapacity(category, world); ++row) {
      const auto token =
          OrderEntry(source, row, kContainerCapacities[value], world.Bag());
      InventoryOrderEntry(target, row, world.Bag()) =
          static_cast<std::uint8_t>(token);
    }
  }
  for (std::size_t value = 0; value < kRecordCategories; ++value) {
    const auto category = static_cast<ItemCategory>(value);
    for (std::size_t slot = 0; slot < kCategoryCapacities[value]; ++slot) {
      world.WriteInventory(category, slot, world.Bag().Read(category, slot));
    }
  }
}

// decomp: _getParentItemIndex (src/MENU/MENUD.PRG/234.c:625)
std::uint16_t ContainerParentWeaponIndex(ItemCategory category,
                                         std::int32_t index,
                                         ContainerWorld& world) {
  if (category == ItemCategory::kBlade) {
    return Read<InventoryBlade>(category, index, world).assembled_weapon_index;
  }
  if (category == ItemCategory::kGrip) {
    return Read<InventoryGrip>(category, index, world).assembled_weapon_index;
  }
  if (category != ItemCategory::kGem) return 0;
  const auto parent = Read<InventoryGem>(category, index, world).set_item_index;
  return (parent & kShieldGemFlag) != 0 ? 0 : parent;
}

// decomp: _getSetShieldIndex (src/MENU/MENUD.PRG/234.c:645)
std::uint16_t ContainerParentShieldIndex(ItemCategory category,
                                         std::int32_t index,
                                         ContainerWorld& world) {
  if (category != ItemCategory::kGem) return 0;
  const auto parent = Read<InventoryGem>(category, index, world).set_item_index;
  return (parent & kShieldGemFlag) == 0 ? 0 : parent & kShieldGemSlotMask;
}
}  // namespace VS::Items
