// Copied from the decomp by tools/port/copy_to_native.py; see each item's
// decomp line.
#include "Menu/Container/Storage.hpp"

#include <cstdint>

#include "Items/ContainerIndexing.hpp"

// decomp: D_80109A82 (D_80109A82@src/MENU/MENUD.PRG/234.c)
std::uint16_t VS::Menu::container::transferredMiscCount;

// decomp: containerItemCapacities
// (containerItemCapacities@src/MENU/MENUD.PRG/234.c)
VS::Items::ContainerCapacities VS::Menu::container::containerItemCapacities =
    VS::Items::kContainerCapacities;

void VS::Menu::container::ResetMenudStatics() {
  containerItemCapacities = Items::kContainerCapacities;
}