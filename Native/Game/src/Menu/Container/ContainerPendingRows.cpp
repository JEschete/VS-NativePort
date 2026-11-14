#include "Menu/Container/ContainerPendingRows.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <variant>

#include "Items/ContainerTransfers.hpp"
#include "Menu/Container/ContainerDetailHelpers.hpp"

namespace VS::Menu::container {
namespace {
constexpr unsigned kCategoryShift = 8;
constexpr std::uint16_t kSlotMask = 0xFF;
constexpr std::uint32_t kEquipped = 0xCA00;
constexpr std::uint32_t kAttached = 0xCC00;
constexpr std::uint32_t kPendingAttached = 0xCC01;
constexpr auto kRecordCategories = Items::kItemCategoryCount - 1;
constexpr auto kMiscRows = Items::kContainerCapacities[static_cast<std::size_t>(
    Items::ItemCategory::kMisc)];

std::uint32_t KeyWord(PendingTransferKey key) {
  return (static_cast<std::uint32_t>(key.category) << kCategoryShift) |
         (static_cast<std::uint32_t>(key.slot) + 1U);
}

std::uint16_t& PendingEntry(std::span<std::uint16_t> pending, std::size_t row,
                            PendingTransferWorld& world) {
  if (row >= pending.size()) {
    world.Refuse("missing or short pending-transfer list");
  }
  return pending[row];
}

std::uint16_t OrderEntry(std::span<const std::uint16_t> order, std::size_t row,
                         PendingTransferWorld& world) {
  if (row >= order.size()) {
    world.Refuse("item row outside captured container display order");
  }
  return order[row];
}

template <typename Record>
Record Read(Items::ItemCategory category, std::int32_t slot,
            Items::ContainerWorld& records) {
  const auto category_index = static_cast<std::size_t>(category);
  if (category_index >= kRecordCategories || slot < 0 ||
      static_cast<std::size_t>(slot) >=
          Items::kContainerCapacities[category_index]) {
    records.Refuse("item row record outside container category storage");
  }
  const auto record = records.Read(category, static_cast<std::size_t>(slot));
  const auto* typed = std::get_if<Record>(&record);
  if (typed == nullptr) {
    records.Refuse("item row record type disagrees with category");
  }
  return *typed;
}

std::optional<Framework::ItemRowText> FormatRow(Items::ItemCategory category,
                                                std::int32_t slot,
                                                std::size_t row,
                                                Items::ContainerWorld& records,
                                                ContainerItemRowsWorld& world) {
  using Category = Items::ItemCategory;
  auto& host = world.TextHost();
  const auto buffer = world.Description(row);
  switch (category) {
    case Category::kWeapon: {
      const auto weapon = Read<Items::InventoryWeapon>(category, slot, records);
      Items::WeaponLoadout loadout{};
      world.ReportItemStatus(BuildWeaponStats(loadout, &weapon, records));
      auto text = Framework::FormatWeaponRow(host, loadout, buffer);
      text.name =
          Framework::TextRef{.source = Framework::TextSource::kWeaponName};
      return text;
    }
    case Category::kBlade:
      return Framework::FormatBladeRow(
          host, Read<Items::InventoryBlade>(category, slot, records), buffer);
    case Category::kGrip:
      return Framework::FormatGripRow(
          host, Read<Items::InventoryGrip>(category, slot, records), buffer);
    case Category::kShield: {
      const auto shield = Read<Items::InventoryShield>(category, slot, records);
      Items::ShieldLoadout loadout{};
      BuildShieldStats(loadout, &shield, records);
      return Framework::FormatShieldRow(host, loadout, buffer);
    }
    case Category::kArmor:
      return Framework::FormatInventoryArmorRow(
          host, Read<Items::InventoryArmor>(category, slot, records), buffer);
    case Category::kGem:
      return Framework::FormatGemRow(
          host, Read<Items::InventoryGem>(category, slot, records), buffer);
    case Category::kMisc:
      return Framework::FormatMiscItemRow(
          host, Read<Items::InventoryMisc>(category, slot, records), buffer);
    case Category::kManage:
      break;
  }
  return std::nullopt;
}

std::uint32_t EquippedFlag(Items::ItemCategory category, std::int32_t slot,
                           ContainerItemRowsWorld& world) {
  using Category = Items::ItemCategory;
  if (category == Category::kWeapon) {
    return Read<Items::InventoryWeapon>(category, slot, world.ActiveRecords())
                       .is_equipped != 0
               ? kEquipped
               : 0;
  }
  if (category == Category::kShield) {
    return Read<Items::InventoryShield>(category, slot, world.ActiveRecords())
                       .is_equipped != 0
               ? kEquipped
               : 0;
  }
  if (category == Category::kArmor) {
    return Read<Items::InventoryArmor>(category, slot, world.ActiveRecords())
                       .limb != 0
               ? kEquipped
               : 0;
  }
  return 0;
}

std::uint32_t ActiveFlags(Items::ItemCategory category, std::int32_t slot,
                          ContainerItemRowsWorld& world) {
  auto flags = EquippedFlag(category, slot, world);
  const auto weapon =
      Items::ContainerParentWeaponIndex(category, slot, world.ActiveRecords());
  if (weapon != 0) {
    flags = EquippedFlag(Items::ItemCategory::kWeapon, weapon - 1, world) != 0
                ? kEquipped
                : kAttached;
  }
  const auto shield =
      Items::ContainerParentShieldIndex(category, slot, world.ActiveRecords());
  if (shield != 0) {
    flags = EquippedFlag(Items::ItemCategory::kShield, shield - 1, world) != 0
                ? kEquipped
                : kAttached;
  }
  return flags;
}

std::int32_t BuildPendingRows(ContainerItemRowsWorld& world) {
  auto& records = world.StagingRecords();
  std::int32_t count = 0;
  for (std::size_t row = 0; row < kPendingTransferEntries; ++row) {
    const auto token = PendingEntry(world.PendingTransfers(), row, world);
    const auto one_based = token & kSlotMask;
    if (one_based == 0) break;
    const auto category =
        static_cast<Items::ItemCategory>(token >> kCategoryShift);
    const auto slot = static_cast<std::int32_t>(one_based) - 1;
    // Unknown token categories still count as rows, but format no record.
    if (static_cast<std::size_t>(category) < kRecordCategories) {
      const auto text = FormatRow(category, slot, row, records, world);
      if (text) world.StoreRow({row, slot, ItemRowStorage::kStaged, *text});
    }
    std::uint32_t flags = 0;
    if (category == Items::ItemCategory::kGem &&
        Items::ContainerParentShieldIndex(category, slot, records) != 0) {
      flags = kPendingAttached;
    }
    if (Items::ContainerParentWeaponIndex(category, slot, records) != 0) {
      flags = kPendingAttached;
    }
    ++count;
    world.AddRowFlags(row, flags);
  }
  return count;
}
}  // namespace

// decomp: func_801069B0 (src/MENU/MENUD.PRG/234.c:2061), 801069B0.
std::int32_t FindPendingTransfer(PendingTransferKey key,
                                 PendingTransferWorld& world) {
  const auto sought = KeyWord(key);
  const auto pending = world.PendingTransfers();
  for (std::size_t row = 0; row < kPendingTransferEntries; ++row) {
    if (PendingEntry(pending, row, world) == sought) {
      return static_cast<std::int32_t>(row) + 1;
    }
  }
  return 0;
}

// decomp: func_80106A04 (src/MENU/MENUD.PRG/234.c:2073), 80106A04.
void AppendPendingTransfer(PendingTransferKey key,
                           PendingTransferWorld& world) {
  const auto pending = world.PendingTransfers();
  for (std::size_t row = 0; row < kPendingTransferEntries; ++row) {
    auto& token = PendingEntry(pending, row, world);
    if (token == 0) {
      token = static_cast<std::uint16_t>(KeyWord(key));
      return;
    }
  }
}

// decomp: func_80106A50 (src/MENU/MENUD.PRG/234.c:2088), 80106A50.
void RebuildPendingTransfers(PendingTransferWorld& world) {
  const auto pending = world.PendingTransfers();
  for (std::size_t row = 0; row < kPendingTransferEntries; ++row) {
    auto& token = PendingEntry(pending, row, world);
    if (token == 0) continue;
    const auto category =
        static_cast<Items::ItemCategory>(token >> kCategoryShift);
    const auto slot = static_cast<std::int32_t>((token - 1U) & kSlotMask);
    if (static_cast<std::size_t>(category) >= kRecordCategories ||
        Items::ContainerItemId(category, slot, world.StagingRecords()) == 0) {
      token = 0;
    }
  }
  std::size_t occupied = 0;
  for (std::size_t row = 0; row < kPendingTransferEntries; ++row) {
    if (pending[row] != 0) pending[occupied++] = pending[row];
  }
  for (auto row = occupied; row < kPendingTransferEntries; ++row) {
    pending[row] = 0;
  }
  for (std::size_t value = 0; value < kRecordCategories; ++value) {
    const auto category = static_cast<Items::ItemCategory>(value);
    for (std::int32_t slot = 0; slot < world.Capacity(category); ++slot) {
      if (Items::ContainerItemId(category, slot, world.StagingRecords()) == 0) {
        continue;
      }
      const PendingTransferKey key{static_cast<std::int32_t>(value), slot};
      if (FindPendingTransfer(key, world) == 0) {
        AppendPendingTransfer(key, world);
      }
    }
  }
}

// decomp: func_80106C64 (src/MENU/MENUD.PRG/234.c:2145), 80106C64.
std::int32_t BuildItemRows(Items::ItemCategory category,
                           ContainerItemRowsWorld& world) {
  if (category == Items::ItemCategory::kManage) return BuildPendingRows(world);
  if (static_cast<std::size_t>(category) >= kRecordCategories) {
    world.Refuse("item row category outside container categories");
  }
  const auto order = world.CaptureOrder(category);
  std::int32_t count = 0;
  for (std::size_t row = 0; row < (category == Items::ItemCategory::kMisc
                                       ? kMiscRows
                                       : world.Capacity(category));
       ++row) {
    const auto token = OrderEntry(order, row, world);
    if (token == 0) break;
    const auto slot = static_cast<std::int32_t>(token) - 1;
    const auto text =
        FormatRow(category, slot, row, world.ActiveRecords(), world);
    if (text) world.StoreRow({row, slot, ItemRowStorage::kActive, *text});
    if (category != Items::ItemCategory::kMisc) {
      world.AddRowFlags(row, ActiveFlags(category, slot, world));
    }
    ++count;
  }
  return count;
}
}  // namespace VS::Menu::container
