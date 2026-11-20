#pragma once
// The model draw workspace the original kept in the scratchpad, and the views
// of it that individual draw routines used. The workspace holds pointers, so
// natively it is wider than the original's; the views keep the original
// offsets of the fields they name.
#include <cstddef>
#include <cstdint>

#include "Core/Gte.h"

namespace VS::Actors {
struct ShpModel;
}

namespace VS::Render {
namespace layout {
inline constexpr std::size_t kModelScratchpadLeadBytes = 0x34;
inline constexpr std::size_t kModelScratchpadGapBytes = 0xE;
inline constexpr std::size_t kAnglesViewLeadBytes = 0x3A;
inline constexpr std::size_t kAnglesViewGapBytes = 0x16;
inline constexpr std::size_t kViewRootGapBytes = 0x8;
inline constexpr std::size_t kCameraViewLeadBytes = 0x64;
inline constexpr std::size_t kCameraViewGapBytes = 0x8;
inline constexpr std::size_t kBoneViewLeadBytes = 0x4C;
}  // namespace layout

// decomp: D_1F80035C_t (src/BATTLE/BATTLE.PRG/30DB0.h:43)
struct ModelScratchpad {
  std::uint8_t reserved0[layout::kModelScratchpadLeadBytes];
  std::uint16_t texture_page;
  std::uint16_t palette;
  std::uint16_t reserved38;
  std::uint16_t angle_x;
  std::uint16_t angle_y;
  std::uint8_t reserved3e[layout::kModelScratchpadGapBytes];
  SVECTOR position;
  SVECTOR root_offset;
  MATRIX* parent_bone_matrix;
  std::int32_t reserved60;
  MATRIX matrix;
  SVECTOR rotation;
  VECTOR scale;
  VS::Actors::ShpModel* model;
};

// decomp: actorTransformScratch (src/BATTLE/BATTLE.PRG/44F14.c:75)
struct ModelScratchpadAnglesView {
  std::uint8_t reserved0[layout::kAnglesViewLeadBytes];
  std::uint16_t angle_x;
  std::uint16_t angle_y;
  std::uint8_t reserved3e[layout::kAnglesViewGapBytes];
  SVECTOR root_offset;
  std::uint8_t reserved5c[layout::kViewRootGapBytes];
  MATRIX matrix;
  SVECTOR rotation;
};

// decomp: cameraTransformScratch (src/BATTLE/BATTLE.PRG/44F14.c:82)
struct ModelScratchpadCameraView {
  std::uint8_t reserved0[layout::kCameraViewLeadBytes];
  MATRIX reserved64;
  std::uint8_t reserved84[layout::kCameraViewGapBytes];
  VECTOR reserved8c;
};

// decomp: frameScratch (src/BATTLE/BATTLE.PRG/44F14.c:137)
struct DrawDepthScratch {
  std::int32_t reserved0;
  std::int32_t depth;
};

// decomp: func_800A1DE8_t (src/BATTLE/BATTLE.PRG/38C1C.c:16)
struct ModelScratchpadBoneView {
  std::uint8_t reserved0[layout::kBoneViewLeadBytes];
  SVECTOR position;
  SVECTOR root_offset;
  std::uint8_t reserved5c[layout::kViewRootGapBytes];
  MATRIX matrix;
};

namespace layout {
inline constexpr std::size_t kModelScratchpadAnglesViewBytes = 0x8C;
static_assert(sizeof(ModelScratchpadAnglesView) ==
              kModelScratchpadAnglesViewBytes);
inline constexpr std::size_t kModelScratchpadCameraViewBytes = 0x9C;
static_assert(sizeof(ModelScratchpadCameraView) ==
              kModelScratchpadCameraViewBytes);
inline constexpr std::size_t kDrawDepthScratchBytes = 0x8;
static_assert(sizeof(DrawDepthScratch) == kDrawDepthScratchBytes);
inline constexpr std::size_t kModelScratchpadBoneViewBytes = 0x84;
static_assert(sizeof(ModelScratchpadBoneView) == kModelScratchpadBoneViewBytes);
}  // namespace layout
}  // namespace VS::Render
