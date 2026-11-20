#pragma once
// The model draw scratch records: the render context (the drawn model's
// projected vertices and depths, its scale and the actor's scale) and the
// skeleton pose (the drawn skeleton's bone matrices and scale).
// ModelRenderingLiveState.hpp and SkeletonLiveState.hpp declare the globals.

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "Core/Gte.h"

namespace VS::Actors {
namespace layout {
inline constexpr std::size_t kProjectedVertexCapacity = 768;
inline constexpr std::size_t kModelRenderReservedBytes = 0x7CC;
inline constexpr std::size_t kModelScaleAxes = 3;
inline constexpr std::size_t kSkeletonPoseBones = 63;
}  // namespace layout

// decomp: D_800F2458_t (src/BATTLE/BATTLE.PRG/30DB0.h:6-13). Upstream names
// the first 0x1800 bytes unk0 and places bones[42] and boneScale[41] after
// them; the native projection keeps its vertex count at 0x1800 instead.
struct ModelRenderContext {
  /* 0x0000 */ std::array<std::int32_t, layout::kProjectedVertexCapacity>
      projected_vertices;
  /* 0x0C00 */ std::array<std::int32_t, layout::kProjectedVertexCapacity>
      vertex_depths;
  /* 0x1800 */ std::uint32_t projected_vertex_count;
  /* 0x1804 */ std::array<std::uint8_t, layout::kModelRenderReservedBytes>
      reserved_1804;
  /* 0x1FD0 */ std::array<std::int32_t, layout::kModelScaleAxes> model_scale;
  /* 0x1FDC */ std::int32_t reserved_1fdc;
  /* 0x1FE0 */ std::uint16_t actor_scale;
};

// decomp: D_800F3C58_t (src/BATTLE/BATTLE.PRG/49268.h:4-7)
struct SkeletonPose {
  std::array<MATRIX, layout::kSkeletonPoseBones> bone_matrices;
  VECTOR scale;
};

namespace layout {
inline constexpr std::size_t kModelRenderContextBytes = 0x1FE4;
static_assert(sizeof(ModelRenderContext) == kModelRenderContextBytes);
inline constexpr std::size_t kVertexCountAt = 0x1800;
static_assert(offsetof(ModelRenderContext, projected_vertex_count) ==
              kVertexCountAt);
inline constexpr std::size_t kModelScaleOffset = 0x1FD0;
static_assert(offsetof(ModelRenderContext, model_scale) == kModelScaleOffset);
inline constexpr std::size_t kActorScaleOffset = 0x1FE0;
static_assert(offsetof(ModelRenderContext, actor_scale) == kActorScaleOffset);
static_assert(std::is_trivially_copyable_v<ModelRenderContext>);
inline constexpr std::size_t kSkeletonPoseBytes = 0x7F0;
static_assert(sizeof(SkeletonPose) == kSkeletonPoseBytes);
static_assert(std::is_trivially_copyable_v<SkeletonPose>);
}  // namespace layout
}  // namespace VS::Actors
