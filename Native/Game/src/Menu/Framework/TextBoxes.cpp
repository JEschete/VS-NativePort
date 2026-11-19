// BATTLE.PRG's text box anchor and information header tables.
#include "Menu/Framework/TextBoxes.hpp"

#include <cstdint>

namespace VS::Menu {
// decomp: D_800EC284 (src/BATTLE/BATTLE.PRG/5BF94.c:394); contents: module
// image (ModuleData.cpp).
std::uint32_t textBoxAnchors[kTextAnchorPresetCount][kTextAnchorFacings];

// decomp: D_800EC2CC (src/BATTLE/BATTLE.PRG/5BF94.c:395); contents: module
// image (ModuleData.cpp).
std::int32_t informationHeaderPositions[kInformationHeaderCount];
}  // namespace VS::Menu
