#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Render/DisplayControl.hpp"

// The PlayStation TIM image: an 8-byte header (id, mode), a CLUT block when
// mode bit 3 is set, then the image block. Each block is a byte length, its
// VRAM rectangle (x, y, width, height as 16-bit values, width in halfwords)
// and its pixels. The parse locates the blocks by offset within the bytes it
// is given and copies nothing (TimImageLive.cpp binds the original
// TIM_IMAGE callers).
namespace VS::Render {
inline constexpr std::size_t kTimHeaderBytes = 8;
inline constexpr std::size_t kTimBlockHeaderBytes = 12;
inline constexpr std::size_t kTimModeOffset = 4;
inline constexpr std::size_t kTimRectOffset = 4;
inline constexpr std::uint32_t kTimClutFlag = 8;
// VRAM is 1024 x 512 halfwords; no block can cover more.
inline constexpr std::uint32_t kTimMaxWidth = 1024;
inline constexpr std::uint32_t kTimMaxHeight = 512;

struct TimBlock {
  ScreenRect area;
  // Byte offsets of the block's rectangle and pixels in the parsed bytes.
  std::size_t rect_offset = 0;
  std::size_t pixel_offset = 0;
  std::size_t pixel_bytes = 0;
};

struct TimImage {
  std::uint32_t mode = 0;
  std::optional<TimBlock> clut;
  TimBlock image;
};

// BATTLE.PRG vs_battle_parseTim (146C.c:16935) and ENDING.PRG _parseTim
// (D4.c:2235). The image block starts the CLUT block's length, rounded down
// to a word, after it, as in the original. Nothing is returned when a block
// header or its pixels fall outside the bytes, a CLUT block's length cannot
// hold its header and pixels, or a rectangle exceeds VRAM; the original
// pointed past such data.
[[nodiscard]] std::optional<TimImage> ParseTim(std::span<const std::byte> data);

// The image block's byte offset in a TIM of this mode whose CLUT block, when
// the mode has one, declares clut_length bytes; nothing when that length
// cannot hold the block header or exceeds what VRAM holds.
[[nodiscard]] std::optional<std::size_t> TimImageBlockOffset(
    std::uint32_t mode, std::uint32_t clut_length) noexcept;

// The bytes a TIM declares whose image block at block_offset holds width x
// height halfwords; nothing when the rectangle is larger than VRAM.
[[nodiscard]] std::optional<std::size_t> DeclaredTimSize(
    std::size_t block_offset, std::uint32_t width,
    std::uint32_t height) noexcept;
}  // namespace VS::Render
