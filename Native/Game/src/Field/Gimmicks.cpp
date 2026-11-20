// The room gimmick globals (Gimmicks.hpp).
#include "Field/Gimmicks.hpp"

namespace VS::Field {
// decomp: D_800E85A0 (D_800E85A0@BATTLE/BATTLE.PRG); contents: module image
// (ModuleData.cpp)
std::array<char, kTriggerMarkerRecordTailBytes> triggerMarkerRecordTail;

// decomp: D_800E8FA8 (D_800E8FA8@BATTLE/BATTLE.PRG); contents: module image
// (ModuleData.cpp)
GimmickBytes<kGimmickPaletteParametersBytes> gimmickPaletteParameters;

// decomp: D_800E8FB4 (D_800E8FB4@BATTLE/BATTLE.PRG); contents: module image,
// relocated (ModulePointers.cpp)
std::array<void*, kGimmickModelTemplateCount> gimmickModelTemplates;

// decomp: D_800E8FD0 (D_800E8FD0@BATTLE/BATTLE.PRG); contents: module image
// (ModuleData.cpp)
GimmickBytes<kGimmickRollOrientationsBytes> kGimmickRollOrientations;

// decomp: D_800E9030 (D_800E9030@BATTLE/BATTLE.PRG); contents: module image
// (ModuleData.cpp)
GimmickBytes<kOrientationRotationsBytes> kGimmickOrientationRotations;

// decomp: D_800E90A8 (D_800E90A8@BATTLE/BATTLE.PRG); contents: module image
// (ModuleData.cpp)
std::int32_t interactingGimmickId;

// decomp: D_800E919C (D_800E919C@BATTLE/BATTLE.PRG); contents: module image
// (ModuleData.cpp)
GimmickBytes<kGimmickTemplateIndicesBytes> gimmickTemplateIndices;

// decomp: D_800E91AC (D_800E91AC@BATTLE/BATTLE.PRG); contents: module image
// (ModuleData.cpp)
GimmickBytes<kSpinHeightStepsBytes> kGimmickSpinHeightSteps;

// decomp: D_800E91BC (D_800E91BC@BATTLE/BATTLE.PRG); contents: module image
// (ModuleData.cpp)
GimmickBytes<kSpinPitchStepsBytes> kGimmickSpinPitchSteps;

// decomp: D_800E91CC (D_800E91CC@BATTLE/BATTLE.PRG); contents: module image
// (ModuleData.cpp)
GimmickBytes<kSpinYawStepsBytes> kGimmickSpinYawSteps;

// decomp: D_800E91DC (D_800E91DC@BATTLE/BATTLE.PRG); contents: module image
// (ModuleData.cpp)
GimmickBytes<kSpinRollStepsBytes> kGimmickSpinRollSteps;

// decomp: D_800E91EC (D_800E91EC@BATTLE/BATTLE.PRG); contents: module image
// (ModuleData.cpp)
GimmickBytes<kBounceHeightStepsBytes> kGimmickBounceHeightSteps;

// decomp: D_800E91F0 (D_800E91F0@BATTLE/BATTLE.PRG); contents: module image
// (ModuleData.cpp)
GimmickBytes<kTiltDepthStepsBytes> kGimmickTiltDepthSteps;

// decomp: D_800E91FC (D_800E91FC@BATTLE/BATTLE.PRG); contents: module image
// (ModuleData.cpp)
GimmickBytes<kTiltAngleStepsBytes> kGimmickTiltAngleSteps;

// decomp: D_800E9208 (D_800E9208@BATTLE/BATTLE.PRG); contents: module image
// (ModuleData.cpp)
GimmickBytes<kRollHeightStepsBytes> kGimmickRollHeightSteps;

// decomp: D_800E9218 (D_800E9218@BATTLE/BATTLE.PRG); contents: module image
// (ModuleData.cpp)
GimmickBytes<kRollDisplacementStepsBytes> kGimmickRollDisplacementSteps;

// decomp: D_800E9228 (D_800E9228@BATTLE/BATTLE.PRG); contents: module image
// (ModuleData.cpp)
GimmickBytes<kRollAngleStepsBytes> kGimmickRollAngleSteps;

// decomp: D_800E9238 (D_800E9238@BATTLE/BATTLE.PRG); contents: module image
// (ModuleData.cpp)
GimmickBytes<kSquashHeightStepsBytes> kGimmickSquashHeightSteps;

// decomp: D_800E9248 (D_800E9248@BATTLE/BATTLE.PRG); contents: module image
// (ModuleData.cpp)
GimmickBytes<kSquashScaleXBytes> kGimmickSquashScaleX;

// decomp: D_800E9258 (D_800E9258@BATTLE/BATTLE.PRG); contents: module image
// (ModuleData.cpp)
GimmickBytes<kSquashScaleYBytes> kGimmickSquashScaleY;

// decomp: D_800E9268 (D_800E9268@BATTLE/BATTLE.PRG); contents: module image
// (ModuleData.cpp)
GimmickBytes<kSquashScaleZBytes> kGimmickSquashScaleZ;

// decomp: vs_battle_roomData (vs_battle_roomData@BATTLE/BATTLE.PRG)
RoomDataRecord roomData;

// decomp: D_800F1CDC (D_800F1CDC@BATTLE/BATTLE.PRG)
std::uint16_t roomAnimationTicks;

// decomp: D_800F1D28 (D_800F1D28@BATTLE/BATTLE.PRG)
std::array<GimmickAnimationJob, kGimmickJobCount> gimmickAnimationQueue;

// decomp: D_800F1D68 (D_800F1D68@BATTLE/BATTLE.PRG)
std::int16_t platformFreezeFrames;

// decomp: D_800F1D6A (D_800F1D6A@BATTLE/BATTLE.PRG)
std::int16_t pendingGimmickAnimationCount;

// decomp: D_800F1D6C (D_800F1D6C@BATTLE/BATTLE.PRG)
std::uint16_t triggerMarkerFrameTicks;

// decomp: D_800F1D6E (D_800F1D6E@BATTLE/BATTLE.PRG)
std::uint8_t areMovingPlatformsPaused;

// decomp: D_800F1D6F (D_800F1D6F@BATTLE/BATTLE.PRG)
char isPlatformFreezeTintActive;

// decomp: D_800F1D70 (D_800F1D70@BATTLE/BATTLE.PRG)
GimmickBytes<kPlatformBounceFramesBytes> platformBounceFrames;

// decomp: D_800F4448 (D_800F4448@BATTLE/BATTLE.PRG)
std::array<char, kGimmickPaletteColorsBytes> gimmickPaletteColors;

// decomp: D_800F45E0 (D_800F45E0@BATTLE/BATTLE.PRG)
std::array<GimmickInstance*, kGimmickInstanceCount> gimmickInstances;

// decomp: D_800F49E8 (D_800F49E8@BATTLE/BATTLE.PRG)
std::int32_t gimmickInteractionDirection;

// decomp: D_80068BEC (D_80068BEC@src/BATTLE/BATTLE.PRG/146C.c)
const BounceOffsetTable kPlatformBounceOffsets = {8, 14, 16, 11, 7, 3, 1};
}  // namespace VS::Field
