// The target sphere's tables and live sphere (Sphere.hpp).
#include "Battle/Sphere.hpp"

namespace VS::Battle {
// decomp: D_800EA234 (D_800EA234@BATTLE/BATTLE.PRG); contents: module image
// (ModuleData.cpp), as are the tables below
std::array<std::int16_t, kSphereArcBaseCapacity> sphereArcBases;
// decomp: D_800EA2C4 (D_800EA2C4@BATTLE/BATTLE.PRG)
SphereMirroredVertexTable sphereMirroredVertices;
// decomp: D_800EA438 (D_800EA438@BATTLE/BATTLE.PRG)
SphereShapeCommandStartTable sphereShapeCommandStarts;
// decomp: D_800EA46C (D_800EA46C@BATTLE/BATTLE.PRG)
SphereWireframeCommandTable sphereWireframeCommands;
// decomp: D_800EA670 (D_800EA670@BATTLE/BATTLE.PRG)
PositionCursorSpriteOffsetTable positionCursorSpriteOffsets;
// decomp: D_800EA684 (D_800EA684@BATTLE/BATTLE.PRG)
PositionCursorSpriteSizeTable positionCursorSpriteSizes;
// decomp: D_800EA698 (D_800EA698@BATTLE/BATTLE.PRG)
PositionCursorSpriteTexCoordTable positionCursorSpriteTexCoords;
// decomp: D_800EA6AC (D_800EA6AC@BATTLE/BATTLE.PRG)
std::uint8_t positionCursorBlinkFrame;
// decomp: D_800EB9AC (D_800EB9AC@BATTLE/BATTLE.PRG)
std::uint8_t reuseSphereVolumes;
// decomp: D_800EB9B8 (D_800EB9B8@BATTLE/BATTLE.PRG); allocated when the
// sphere opens
Battle_TargetSphereSetupView* targetSphere;
}  // namespace VS::Battle
