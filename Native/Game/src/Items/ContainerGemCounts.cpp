#include "Items/ContainerGemCounts.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <variant>

namespace VS::Items {
namespace {
template <typename Record>
Record Read(ItemCategory category, std::int32_t slot, ContainerWorld& source) {
  const auto category_index = static_cast<std::size_t>(category);
  if (slot < 0 ||
      static_cast<std::size_t>(slot) >= kContainerCapacities[category_index]) {
    source.Refuse("gem-count record outside container category storage");
  }
  const auto record = source.Read(category, static_cast<std::size_t>(slot));
  const auto* typed = std::get_if<Record>(&record);
  if (typed == nullptr) {
    source.Refuse("gem-count record type disagrees with category");
  }
  return *typed;
}

std::int32_t CountSlots(std::span<const std::uint8_t> references,
                        std::uint8_t slots, ContainerWorld& source) {
  std::int32_t count = 0;
  for (std::size_t slot = 0; slot < slots; ++slot) {
    if (slot >= references.size()) {
      source.Refuse("gem slot outside attached-reference storage");
    }
    if (references[slot] != 0) ++count;
  }
  return count;
}
}  // namespace

// decomp: _getWeaponsubItemCount (src/MENU/MENUD.PRG/234.c:1959)
std::int32_t CountContainerWeaponGems(std::int32_t weapon_slot,
                                      ContainerWorld& source) {
  const auto weapon =
      Read<InventoryWeapon>(ItemCategory::kWeapon, weapon_slot, source);
  const auto grip =
      Read<InventoryGrip>(ItemCategory::kGrip, weapon.grip - 1, source);
  return CountSlots(weapon.gems, grip.gem_slots, source);
}

// decomp: _getShieldsubItemCount (src/MENU/MENUD.PRG/234.c:1971)
std::int32_t CountContainerShieldGems(std::int32_t shield_slot,
                                      ContainerWorld& source) {
  const auto shield =
      Read<InventoryShield>(ItemCategory::kShield, shield_slot, source);
  return CountSlots(shield.gems, shield.base.gem_slots, source);
}
}  // namespace VS::Items
