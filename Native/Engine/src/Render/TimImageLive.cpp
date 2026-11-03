// Hand-maintained (tools/port/owned.csv).
// Live binding for the TIM parse (TimImage.cpp).
#include "Render/TimImageLive.hpp"

#include <bit>
#include <cinttypes>
#include <cstddef>
#include <cstring>
#include <optional>
#include <span>

extern "C" {
#include "Core/Diagnostics.h"
}

namespace VS::Render {
namespace {
constexpr std::size_t kRectWidthOffset = kTimRectOffset + 4;
constexpr std::size_t kRectHeightOffset = kTimRectOffset + 6;

std::uint64_t refused_tims = 0;

template <typename T>
T ReadField(const std::byte* data, std::size_t offset) {
  T value{};
  std::memcpy(&value, data + offset, sizeof value);
  return value;
}

void ReportRefusal(const void* data, std::uint32_t mode) {
  if (std::has_single_bit(++refused_tims)) {
    Core_Errorf("render",
                "TIM at %p (mode 0x%" PRIX32
                ") refused: its blocks are inconsistent or exceed VRAM "
                "(%" PRIu64 " refused)",
                data, mode, refused_tims);
  }
}

// The bytes the TIM declares. Only the mode, the CLUT block's length and the
// image block's size are read, each where a well-formed TIM has it.
std::optional<std::size_t> DeclaredLiveTimSize(const std::byte* data,
                                               std::uint32_t mode) {
  const std::uint32_t clut_length =
      (mode & kTimClutFlag) != 0
          ? ReadField<std::uint32_t>(data, kTimHeaderBytes)
          : 0;
  const std::optional<std::size_t> image_offset =
      TimImageBlockOffset(mode, clut_length);
  if (!image_offset.has_value()) {
    return std::nullopt;
  }
  return DeclaredTimSize(
      *image_offset,
      ReadField<std::uint16_t>(data, *image_offset + kRectWidthOffset),
      ReadField<std::uint16_t>(data, *image_offset + kRectHeightOffset));
}
}  // namespace

void ParseLiveTim(void* data, TIM_IMAGE* tim) {
  if (tim == nullptr) {
    ReportRefusal(data, 0);
    return;
  }
  *tim = {};
  if (data == nullptr) {
    ReportRefusal(data, 0);
    return;
  }
  auto* bytes = static_cast<std::byte*>(data);
  const auto mode = ReadField<std::uint32_t>(bytes, kTimModeOffset);
  tim->mode = mode;
  const std::optional<std::size_t> size = DeclaredLiveTimSize(bytes, mode);
  const std::optional<TimImage> image =
      size.has_value() ? ParseTim(std::span<const std::byte>(bytes, *size))
                       : std::nullopt;
  if (!image.has_value()) {
    ReportRefusal(data, mode);
    return;
  }
  if (image->clut.has_value()) {
    tim->crect = reinterpret_cast<RECT*>(bytes + image->clut->rect_offset);
    tim->caddr = reinterpret_cast<u_long*>(bytes + image->clut->pixel_offset);
  }
  tim->prect = reinterpret_cast<RECT*>(bytes + image->image.rect_offset);
  tim->paddr = reinterpret_cast<u_long*>(bytes + image->image.pixel_offset);
}

std::uint64_t RefusedLiveTimCount() { return refused_tims; }
}  // namespace VS::Render
