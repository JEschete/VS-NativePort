#pragma once

#include <cstdint>

#include "Items/ContainerIndexing.hpp"

namespace VS::Items {
[[nodiscard]] std::int32_t CountContainerWeaponGems(std::int32_t weapon_slot,
                                                    ContainerWorld& source);
[[nodiscard]] std::int32_t CountContainerShieldGems(std::int32_t shield_slot,
                                                    ContainerWorld& source);
}  // namespace VS::Items
