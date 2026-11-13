#pragma once

#include <exception>
#include <utility>

#include "Items/ItemRecords.hpp"

namespace VS::Items::Live {
// Both bindings and the native owner select an actual member array, never a
// flattened pointer advanced across adjacent C++ array subobjects.
template <typename Storage, typename Visitor>
decltype(auto) VisitContainerTable(Storage& storage, ItemCategory category,
                                   Visitor&& visit) {
  switch (category) {
    case ItemCategory::kWeapon:
      return std::forward<Visitor>(visit)(storage.weapons);
    case ItemCategory::kBlade:
      return std::forward<Visitor>(visit)(storage.blades);
    case ItemCategory::kGrip:
      return std::forward<Visitor>(visit)(storage.grips);
    case ItemCategory::kShield:
      return std::forward<Visitor>(visit)(storage.shields);
    case ItemCategory::kArmor:
      return std::forward<Visitor>(visit)(storage.armor);
    case ItemCategory::kGem:
      return std::forward<Visitor>(visit)(storage.gems);
    case ItemCategory::kMisc:
      return std::forward<Visitor>(visit)(storage.misc);
    case ItemCategory::kManage:
      break;
  }
  std::terminate();
}
}  // namespace VS::Items::Live
