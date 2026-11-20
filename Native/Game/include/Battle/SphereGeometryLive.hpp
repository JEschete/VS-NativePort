#pragma once

#include <optional>

#include "Battle/BattleTypes.h"
#include "Battle/SphereGeometry.hpp"
#include "Battle/TargetVolume.hpp"
#include "Legacy/Psx.h"

namespace VS::Battle {
[[nodiscard]] std::optional<Core::Math::Mat3> BuildLiveSphereVolumeTransform(
    const Battle_TargetVolume* volume) noexcept;

// Reads and commits only the sphere's wire animation, vertex count and the
// vertices written by the checked core. Table extents are supplied explicitly.
[[nodiscard]] std::optional<SphereWireframeBuildResult>
BuildLiveSphereWireframe(VS::Battle::TargetSphereSetupView* sphere,
                         std::int32_t type,
                         const SphereGeometryTables& tables) noexcept;

[[nodiscard]] int AppendSphereArcVerticesEntry(int phase, int segments,
                                               int index);
[[nodiscard]] MATRIX* BuildSphereVolumeTransformEntry(u_char* scale, int angle);
[[nodiscard]] int AppendSphereCircleVerticesEntry(int angle, int index);
[[nodiscard]] int AppendSphereConeVerticesEntry(int step, int radius,
                                                int index);
void BuildSphereWireframeEntry(int type);
}  // namespace VS::Battle
