// The actor AI tables and bindings (ActorState.hpp).
#include "Battle/Ai/ActorState.hpp"

namespace VS::Battle::Ai {
// decomp: D_80069C18 (D_80069C18@BATTLE/BATTLE.PRG); contents: module image
// (ModuleData.cpp)
TerrainMaskTable kTerrainRejectMasks;
// decomp: D_80069C1C (D_80069C1C@BATTLE/BATTLE.PRG); contents: module image
// (ModuleData.cpp)
RangeTable kPerceptionRangesSquared;
// decomp: D_80069C3C (D_80069C3C@BATTLE/BATTLE.PRG); contents: module image
// (ModuleData.cpp)
std::array<WanderTiming, kWanderTimingPresetCount> kWanderTimingPresets;
// decomp: D_800F1790 (D_800F1790@BATTLE/BATTLE.PRG); contents: module image,
// relocated (ModulePointers.cpp)
std::array<DecisionCallback, kDecisionCallbackCount> decisionCallbacks;
// decomp: D_800F17B8 (D_800F17B8@BATTLE/BATTLE.PRG); contents: module image
// (ModuleData.cpp)
std::array<WordPair, kViewAngleCount> viewAngles;
// decomp: D_800F17C8 (D_800F17C8@BATTLE/BATTLE.PRG); contents: module image
// (ModuleData.cpp)
std::array<WordPair, kStepHeightCount> stepHeights;
// decomp: D_800F17D8 (D_800F17D8@BATTLE/BATTLE.PRG); contents: module image
// (ModuleData.cpp)
std::array<std::uint8_t, kFacingDelayScaleCount> facingDelayScales;
// decomp: D_800F5878 (D_800F5878@BATTLE/BATTLE.PRG)
std::array<VS::Battle::Ai::ActorAiState*, kActorAiStateSlots> actorAiStates;
// decomp: D_800F58BC (D_800F58BC@BATTLE/BATTLE.PRG)
VS::Battle::Ai::RoomAiState* roomAiState;
// decomp: D_800F58C0 (D_800F58C0@BATTLE/BATTLE.PRG)
void* roomAuxiliaryData;
// decomp: D_800F590C (D_800F590C@BATTLE/BATTLE.PRG)
std::int32_t attributeActorMask;
// decomp: D_80069C08 (D_80069C08@BATTLE/BATTLE.PRG)
const std::array<std::uint8_t, 4> kPatrolRates = {9, 7, 0, 0};
}  // namespace VS::Battle::Ai
