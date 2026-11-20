// Action resolution's shared state (ActionResolution.hpp).
#include "Battle/ActionResolution.hpp"

namespace VS::Battle {
// decomp: D_800E8200 (D_800E8200@BATTLE/BATTLE.PRG); contents: module image
// (ModuleData.cpp)
std::array<std::uint8_t, kLimbInjuryStatusBytes> limbInjuryStatusIds;
// decomp: _actionEffectMediators (_actionEffectMediators@BATTLE/BATTLE.PRG);
// contents: module image, relocated (ModulePointers.cpp)
ActionEffectMediatorTable actionEffectMediators;
// decomp: D_800F1A00 (D_800F1A00@BATTLE/BATTLE.PRG)
std::uint32_t hitPrerequisiteResult;
// decomp: D_800F1A04 (D_800F1A04@BATTLE/BATTLE.PRG)
std::uint32_t previousHitResult;
}  // namespace VS::Battle
