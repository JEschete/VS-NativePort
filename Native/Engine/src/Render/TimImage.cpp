// Hand-maintained (tools/port/owned.csv).
#include "Render/TimImage.hpp"

#include <algorithm>
#include <cstring>

namespace VS::Render {
namespace {
constexpr std::uint32_t kWordAlignMask = ~std::uint32_t{3};
constexpr std::size_t kClutBlockOffset = kTimHeaderBytes;
constexpr std::size_t kHalfwordBytes = 2;
constexpr std::size_t kRectHalfwords = 4;
constexpr std::size_t kRectBytes = kRectHalfwords * kHalfwordBytes;
constexpr auto kVramBytes = kHalfwordBytes * kTimMaxWidth * kTimMaxHeight;

template <typename T>
T Read(std::span<const std::byte> data, std::size_t offset) {
  T value{};
  std::memcpy(&value, data.subspan(offset, sizeof value).data(), sizeof value);
  return value;
}

// A block's rectangle and pixels at `offset`, or nothing when they do not fit
// within `limit` bytes of the data or the rectangle exceeds VRAM.
std::optional<TimBlock> ReadBlock(std::span<const std::byte> data,
                                  std::size_t offset, std::size_t limit) {
  if (limit > data.size() || offset > limit ||
      limit - offset < kTimBlockHeaderBytes) {
    return std::nullopt;
  }
  const std::size_t rect_offset = offset + kTimRectOffset;
  const auto x = Read<std::int16_t>(data, rect_offset);
  const auto y = Read<std::int16_t>(data, rect_offset + kHalfwordBytes);
  const auto width =
      Read<std::uint16_t>(data, rect_offset + 2 * kHalfwordBytes);
  const auto height =
      Read<std::uint16_t>(data, rect_offset + 3 * kHalfwordBytes);
  if (width > kTimMaxWidth || height > kTimMaxHeight) {
    return std::nullopt;
  }
  const std::size_t pixel_offset = rect_offset + kRectBytes;
  const std::size_t pixel_bytes = std::size_t{width} * height * kHalfwordBytes;
  if (limit - pixel_offset < pixel_bytes) {
    return std::nullopt;
  }
  return TimBlock{.area = {.x = x,
                           .y = y,
                           .width = static_cast<std::int16_t>(width),
                           .height = static_cast<std::int16_t>(height)},
                  .rect_offset = rect_offset,
                  .pixel_offset = pixel_offset,
                  .pixel_bytes = pixel_bytes};
}
}  // namespace

std::optional<std::size_t> TimImageBlockOffset(
    std::uint32_t mode, std::uint32_t clut_length) noexcept {
  if ((mode & kTimClutFlag) == 0) {
    return kClutBlockOffset;
  }
  if (clut_length < kTimBlockHeaderBytes ||
      clut_length > kTimBlockHeaderBytes + kVramBytes) {
    return std::nullopt;
  }
  return kClutBlockOffset + (clut_length & kWordAlignMask);
}

std::optional<std::size_t> DeclaredTimSize(std::size_t block_offset,
                                           std::uint32_t width,
                                           std::uint32_t height) noexcept {
  if (width > kTimMaxWidth || height > kTimMaxHeight) {
    return std::nullopt;
  }
  return block_offset + kTimBlockHeaderBytes +
         std::size_t{width} * height * kHalfwordBytes;
}

std::optional<TimImage> ParseTim(std::span<const std::byte> data) {
  if (data.size() < kTimHeaderBytes) {
    return std::nullopt;
  }
  TimImage tim{};
  tim.mode = Read<std::uint32_t>(data, kTimModeOffset);
  std::uint32_t clut_length = 0;
  if ((tim.mode & kTimClutFlag) != 0) {
    if (data.size() - kClutBlockOffset < sizeof clut_length) {
      return std::nullopt;
    }
    clut_length = Read<std::uint32_t>(data, kClutBlockOffset);
  }
  const std::optional<std::size_t> image_offset =
      TimImageBlockOffset(tim.mode, clut_length);
  if (!image_offset.has_value()) {
    return std::nullopt;
  }
  if ((tim.mode & kTimClutFlag) != 0) {
    tim.clut = ReadBlock(data, kClutBlockOffset,
                         std::min(data.size(), kClutBlockOffset + clut_length));
    if (!tim.clut.has_value()) {
      return std::nullopt;
    }
  }
  const std::optional<TimBlock> image =
      ReadBlock(data, *image_offset, data.size());
  if (!image.has_value()) {
    return std::nullopt;
  }
  tim.image = *image;
  return tim;
}
}  // namespace VS::Render
