#pragma once
// The frame's drawing context that the original kept at the start of the
// scratchpad, the camera it carries, the GTE screen offset, and the GPU's
// texture, blend and dither modes.
#include <cstddef>
#include <cstdint>

#include "Core/Gte.h"
#include "Core/Scratch.hpp"

namespace VS::Render {
// decomp: vs_dithering (src/include/gpu.h:15)
enum class Dithering : std::uint8_t { kOff = 0, kOn = 1 };

// decomp: vs_semiTransparencyRate (src/include/gpu.h:8)
enum class BlendMode : std::uint8_t {
  kAverage = 0,
  kAdd = 1,
  kSubtract = 2,
  kAddQuarter = 3
};

// decomp: vs_texMode (src/include/gpu.h:6)
enum class TextureFormat : std::uint8_t {
  kPalette4Bit = 0,
  kPalette8Bit = 1,
  kDirect16Bit = 2
};

// decomp: vs_battle_geomOffset (src/BATTLE/BATTLE.PRG/2EA3C.h:7)
struct ScreenOffset {
  std::int32_t x;
  std::int32_t y;
};

// decomp: vs_camera_t (src/include/scratch.h:10)
struct Camera {
  VECTOR position;
  VECTOR look_at;
  VECTOR angles;
  std::int32_t far_clip;
};

// decomp: vs_scratch_t (src/include/scratch.h:20). The original's pointers
// were 4 bytes; natively they widen the record, which still fits the 1 KiB
// scratch block.
struct FrameScratch {
  void* primitive_cursor;
  void* ordering_table;
  void* overlay_ordering_table;
  void* reserved0c;
  std::uint32_t first_person_view_facing;
  MATRIX view_matrix;
  Camera camera;
};

// decomp: vs_scratch (src/include/scratch.h): the frame's drawing context at
// the start of scratch memory.
[[nodiscard]] inline FrameScratch& FrameScratchpad() noexcept {
  return *static_cast<FrameScratch*>(Core::ScratchBase());
}

namespace layout {
inline constexpr std::size_t kScreenOffsetBytes = 0x8;
static_assert(sizeof(ScreenOffset) == kScreenOffsetBytes);
inline constexpr std::size_t kCameraBytes = 0x34;
static_assert(sizeof(Camera) == kCameraBytes);
}  // namespace layout
}  // namespace VS::Render
