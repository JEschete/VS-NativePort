#pragma once
// The target sphere's geometry tables, the position cursor's sprite tables,
// and the live target sphere. Sphere.cpp defines them; the module image fills
// the tables at start-up.

#include <array>
#include <cstddef>
#include <cstdint>

struct Battle_TargetSphereSetupView;

namespace VS::Battle {

// The tables' capacities; ModulePointers.cpp records the module image span
// of each.
inline constexpr std::size_t kSphereArcBaseCapacity = 502;
inline constexpr std::size_t kSphereMirroredVertexCapacity = 430;
inline constexpr std::size_t kSphereShapeCommandStartCapacity = 488;
inline constexpr std::size_t kSphereWireframeCommandCapacity = 218;
inline constexpr std::size_t kPositionCursorSpriteOffsetCapacity = 126;
inline constexpr std::size_t kPositionCursorSpriteSizeCapacity = 121;
inline constexpr std::size_t kPositionCursorSpriteTexCoordCapacity = 116;

// clang-format wraps these aliases inside the namespace, which cpplint
// reads as indentation.
// NOLINTBEGIN
using SphereMirroredVertexTable =
    std::array<std::int16_t, kSphereMirroredVertexCapacity>;
using SphereShapeCommandStartTable =
    std::array<std::uint8_t, kSphereShapeCommandStartCapacity>;
using SphereWireframeCommandTable =
    std::array<std::uint16_t, kSphereWireframeCommandCapacity>;
using PositionCursorSpriteOffsetTable =
    std::array<std::int32_t, kPositionCursorSpriteOffsetCapacity>;
using PositionCursorSpriteSizeTable =
    std::array<std::int32_t, kPositionCursorSpriteSizeCapacity>;
using PositionCursorSpriteTexCoordTable =
    std::array<std::int32_t, kPositionCursorSpriteTexCoordCapacity>;
// NOLINTEND

// decomp: D_800EA234 (src/BATTLE/BATTLE.PRG/58578.c:44)
extern std::array<std::int16_t, kSphereArcBaseCapacity> sphereArcBases;
// decomp: D_800EA2C4 (src/BATTLE/BATTLE.PRG/58578.c:45)
extern SphereMirroredVertexTable sphereMirroredVertices;
// decomp: D_800EA438 (src/BATTLE/BATTLE.PRG/58578.c:46)
extern SphereShapeCommandStartTable sphereShapeCommandStarts;
// decomp: D_800EA46C (src/BATTLE/BATTLE.PRG/58578.c:47)
extern SphereWireframeCommandTable sphereWireframeCommands;
// decomp: D_800EA670 (src/BATTLE/BATTLE.PRG/58578.c:48)
extern PositionCursorSpriteOffsetTable positionCursorSpriteOffsets;
// decomp: D_800EA684 (src/BATTLE/BATTLE.PRG/58578.c:49)
extern PositionCursorSpriteSizeTable positionCursorSpriteSizes;
// decomp: D_800EA698 (src/BATTLE/BATTLE.PRG/58578.c:50)
extern PositionCursorSpriteTexCoordTable positionCursorSpriteTexCoords;
// decomp: D_800EA6AC (src/BATTLE/BATTLE.PRG/58578.c:51)
extern std::uint8_t positionCursorBlinkFrame;
// decomp: D_800EB9AC (src/BATTLE/BATTLE.PRG/5BF94Private.h:146)
extern std::uint8_t reuseSphereVolumes;
// decomp: D_800EB9B8 (src/BATTLE/BATTLE.PRG/58578.h:43)
extern Battle_TargetSphereSetupView* targetSphere;

}  // namespace VS::Battle
