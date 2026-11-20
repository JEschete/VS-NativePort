#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "Battle/BattleTypes.hpp"
#include "Battle/SphereDraw.hpp"
#include "Battle/SphereGeometry.hpp"
#include "Battle/TargetVolume.hpp"
#include "Legacy/Psx.hpp"

namespace VS::Battle {
// The camera yaw basis: three rows of four halfwords.
inline constexpr std::size_t kSphereCameraBasisHalfwords = 12;
// The cone basis: rows of three halfwords, read by the arcs four apart.
inline constexpr std::size_t kSphereBasisHalfwords = 12;

// The working copies the target sphere's geometry and drawing share, which
// the original kept in the scratchpad at 0x1F800350-0x1F800400
// (SphereGeometryLive.cpp). Each draw (func_800C0D78) rewrites every part
// before reading it: the sine samples and staged volumes on entry, the volume
// transform per volume and the basis per wire command, so nothing carries
// between frames. The session reset only keeps a stray read deterministic.
struct SphereWorkspace {
  // decomp: 0x1F800350, func_800C085C (src/BATTLE/BATTLE.PRG/57B1C.c:175):
  // the volume's scale-and-tilt matrix.
  VS::Core::Gte::MATRIX volume_transform;
  // decomp: 0x1F800350, func_800C282C (src/BATTLE/BATTLE.PRG/58578.c:914):
  // the yaw rows (cosine, sine; -sine, cosine at halfwords 0, 2, 8 and 10).
  // The original wrote them over the volume transform; its only caller
  // discards them and every transform read follows a fresh func_800C085C.
  std::array<std::int16_t, kSphereCameraBasisHalfwords> camera_basis;
  // decomp: 0x1F800378, func_800C0D78 (src/BATTLE/BATTLE.PRG/58578.c:53):
  // the working copies of the sphere's two volumes.
  std::array<TargetVolume, kSphereDrawVolumes> staged_volumes;
  // decomp: 0x1F800398, func_800C253C and func_800C2368
  // (src/BATTLE/BATTLE.PRG/58578.c:794,836): an arc's or cone's basis. The
  // original's func_800C0B50 also built its view matrix over these bytes;
  // every wire command rewrites the basis before reading it.
  std::array<std::int16_t, kSphereBasisHalfwords> basis;
  // decomp: 0x1F8003B0, func_800C0D78: Q12 sine of 0, 128, ... 4992.
  std::array<std::int16_t, kSphereSineSamples> sine_samples;
};

[[nodiscard]] SphereWorkspace& LiveSphereWorkspace() noexcept;
// Restores the cold-boot state (all zero) at a session boundary.
void ResetLiveSphereWorkspace() noexcept;

[[nodiscard]] std::optional<Core::Math::Mat3> BuildLiveSphereVolumeTransform(
    const VS::Battle::TargetVolume* volume) noexcept;

// Reads and commits only the sphere's wire animation, vertex count and the
// vertices written by the checked core. Table extents are supplied explicitly.
[[nodiscard]] std::optional<SphereWireframeBuildResult>
BuildLiveSphereWireframe(VS::Battle::TargetSphereSetupView* sphere,
                         std::int32_t type,
                         const SphereGeometryTables& tables) noexcept;

[[nodiscard]] int AppendSphereArcVerticesEntry(int phase, int segments,
                                               int index);
[[nodiscard]] VS::Core::Gte::MATRIX* BuildSphereVolumeTransformEntry(
    std::uint8_t* scale, int angle);
[[nodiscard]] int AppendSphereCircleVerticesEntry(int angle, int index);
[[nodiscard]] int AppendSphereConeVerticesEntry(int step, int radius,
                                                int index);
void BuildSphereWireframeEntry(int type);
}  // namespace VS::Battle
