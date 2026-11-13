#include "Items/ContainerIndexingLive.hpp"

#include <array>
#include <cstddef>
#include <cstdlib>
#include <iterator>
#include <span>
#include <string_view>
#include <type_traits>
#include <variant>

#include "Items/ContainerIndexing.hpp"
#include "Items/ContainerTables.hpp"
#include "Items/ItemRecordsLive.hpp"

extern "C" {
#include "Core/Diagnostics.h"
#include "Items/Inventory.h"
#include "Menu/Container/Storage.h"
}

namespace VS::Items {
namespace {
[[noreturn]] void Refuse(std::string_view reason) {
  Core_Errorf("items.container", "Container indexing refused: %.*s",
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

std::uint16_t* IndexPointer(Items_ContainerIndices& indices,
                            std::size_t offset) {
  const std::array<std::span<std::uint16_t>, kItemCategoryCount - 1> tables{
      indices.weapons, indices.blades, indices.grips, indices.shields,
      indices.armor,   indices.gems,   indices.misc};
  for (auto table : tables) {
    if (offset < table.size()) return table.data() + offset;
    offset -= table.size();
  }
  if (offset != 0) Refuse("category indices outside container display storage");
  return tables.back().data() + tables.back().size();
}

std::span<std::uint16_t> Order(ItemCategory category, std::uint16_t* indices) {
  if (indices == nullptr) return {};
  // The original ABI supplies no extent. Audited callers pass the selected
  // category's actual member array; the typed policy separately checks its
  // span.
  return {indices, kContainerCapacities[static_cast<std::size_t>(category)]};
}

static_assert(sizeof(Items_ContainerIndices) ==
              kContainerIndexEntries * sizeof(std::uint16_t));
constexpr std::size_t kOriginalIndicesOffset = 0x3C00;
static_assert(offsetof(Items_Container, indices) == kOriginalIndicesOffset);
}  // namespace

LiveContainerWorld::LiveContainerWorld(Items_ContainerData* owner)
    : owner_(owner) {}
std::uint16_t LiveContainerWorld::Capacity(ItemCategory category) {
  return Menu_Container_containerItemCapacities[static_cast<std::size_t>(
      category)];
}
ContainerRecord LiveContainerWorld::Read(ItemCategory category,
                                         std::size_t index) {
  if (owner_ == nullptr) Refuse("missing container record owner");
  return Live::VisitContainerTable(
      *owner_, category, [&](const auto& table) -> ContainerRecord {
        if (index >= std::size(table)) {
          Refuse("record index outside live container table");
        }
        return Live::Typed(table[index]);
      });
}
void LiveContainerWorld::Write(ItemCategory category, std::size_t index,
                               const ContainerRecord& item) {
  if (owner_ == nullptr) Refuse("missing container record owner");
  Live::VisitContainerTable(*owner_, category, [&](auto& table) {
    if (index >= std::size(table)) {
      Refuse("record index outside live container table");
    }
    using Legacy = std::remove_reference_t<decltype(table[index])>;
    const auto* record = std::get_if<Live::TypedRecordOf<Legacy>>(&item);
    if (record == nullptr) {
      Refuse("record type disagrees with live container table");
    }
    Live::Commit(table[index], *record);
  });
}
[[noreturn]] void LiveContainerWorld::Refuse(std::string_view reason) {
  Items::Refuse(reason);
}

// decomp: _getContainerIndicesOffset (src/MENU/MENUD.PRG/234.c:85)
std::uint16_t* LiveContainerCategoryIndices(int category,
                                            Items_Container* container) {
  LiveContainerWorld world;
  const auto offset = ContainerCategoryIndices(Category(category), world);
  if (container == nullptr) Refuse("missing container index owner");
  return IndexPointer(container->indices, offset);
}

// decomp: _getContainerItemIndex (src/MENU/MENUD.PRG/234.c:97)
int FindLiveContainerItemRow(int category, int index, std::uint16_t* indices) {
  const auto typed_category = Category(category);
  LiveContainerWorld world;
  return FindContainerItemRow(typed_category, index,
                              Order(typed_category, indices), world);
}

// decomp: _setContainerItemIndex (src/MENU/MENUD.PRG/234.c:109)
void AppendLiveContainerItemIndex(int category, int index,
                                  std::uint16_t* indices) {
  const auto typed_category = Category(category);
  LiveContainerWorld world;
  AppendContainerItemIndex(typed_category, index,
                           Order(typed_category, indices), world);
}

// decomp: _initContainerObject (src/MENU/MENUD.PRG/234.c:121)
void ClearLiveContainerItem(int category, int index,
                            Items_ContainerData* container) {
  LiveContainerWorld world(container);
  ClearContainerItem(Category(category), index, world);
}

// decomp: _getContainerItemId (src/MENU/MENUD.PRG/234.c:191)
int LiveContainerItemId(int category, int index,
                        Items_ContainerData* container) {
  LiveContainerWorld world(container);
  return ContainerItemId(Category(category), index, world);
}

// decomp: _countContainerItems (src/MENU/MENUD.PRG/234.c:221)
int CountLiveContainerItems(int category, Items_ContainerData* container) {
  LiveContainerWorld world(container);
  return CountContainerItems(Category(category), world);
}

// decomp: _getEmptyContainerSlot (src/MENU/MENUD.PRG/234.c:234)
int FindLiveEmptyContainerSlot(int category, Items_ContainerData* container) {
  LiveContainerWorld world(container);
  return FindEmptyContainerSlot(Category(category), world);
}
}  // namespace VS::Items
