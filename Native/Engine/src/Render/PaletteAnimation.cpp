// Hand-maintained (tools/port/owned.csv).
#include "Render/PaletteAnimation.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "Core/Math/ScalarMath.hpp"

namespace VS::Render {
namespace {

using Core::Math::WrappingAdd;
using Core::Math::WrappingMultiply;
using Core::Math::WrappingSubtract;
using Core::Math::WrapSigned16;

using Channels = std::array<std::int32_t, kRgbChannelCount>;

template <std::size_t Columns>
using DeltaTable = std::array<std::array<std::int16_t, Columns>, kStepRowCount>;

// _animOffset16, _animOffset8, _animOffset4 and _animOffset2 (SLUS_010.40
// data): row kHoldStepRow + d moves a channel by d over the row's columns, one
// column a step. The original's signed bytes, held as integers rather than
// characters.
constexpr DeltaTable<16> kSixteenFrameDeltas{{
    {-2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -1},
    {-2, -2, -2, -2, -2, -2, -2, -2, -1, -2, -2, -2, -2, -2, -2, -1},
    {-2, -2, -2, -2, -2, -1, -2, -2, -2, -2, -1, -2, -2, -2, -2, -1},
    {-2, -2, -2, -2, -1, -2, -2, -2, -1, -2, -2, -2, -1, -2, -2, -1},
    {-2, -2, -2, -1, -2, -2, -1, -2, -2, -1, -2, -2, -1, -2, -2, -1},
    {-2, -2, -1, -2, -2, -1, -2, -2, -1, -2, -1, -2, -2, -1, -2, -1},
    {-2, -2, -1, -2, -1, -2, -1, -2, -2, -1, -2, -1, -2, -1, -2, -1},
    {-2, -2, -1, -2, -1, -2, -1, -2, -1, -2, -1, -2, -1, -2, -1, -1},
    {-2, -1, -2, -1, -2, -1, -2, -1, -1, -2, -1, -2, -1, -2, -1, -1},
    {-2, -1, -2, -1, -1, -2, -1, -2, -1, -1, -2, -1, -1, -2, -1, -1},
    {-2, -1, -1, -2, -1, -1, -2, -1, -1, -2, -1, -1, -2, -1, -1, -1},
    {-2, -1, -1, -2, -1, -1, -1, -2, -1, -1, -1, -2, -1, -1, -1, -1},
    {-2, -1, -1, -1, -1, -2, -1, -1, -1, -1, -2, -1, -1, -1, -1, -1},
    {-2, -1, -1, -1, -1, -1, -1, -2, -1, -1, -1, -1, -1, -1, -1, -1},
    {-2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {-2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0},
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0},
    {-1, -1, -1, -1, -1, -1, -1, -1, 0, -1, -1, -1, -1, -1, -1, 0},
    {-1, -1, -1, -1, -1, 0, -1, -1, -1, -1, 0, -1, -1, -1, -1, 0},
    {-1, -1, -1, -1, 0, -1, -1, -1, 0, -1, -1, -1, 0, -1, -1, 0},
    {-1, -1, -1, 0, -1, -1, 0, -1, -1, 0, -1, -1, 0, -1, -1, 0},
    {-1, -1, 0, -1, -1, 0, -1, -1, 0, -1, 0, -1, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0, -1, 0, -1, -1, 0, -1, 0, -1, 0, -1, 0},
    {-1, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, 0},
    {-1, 0, -1, 0, -1, 0, -1, 0, 0, -1, 0, -1, 0, -1, 0, 0},
    {-1, 0, -1, 0, 0, -1, 0, -1, 0, 0, -1, 0, 0, -1, 0, 0},
    {-1, 0, 0, -1, 0, 0, -1, 0, 0, -1, 0, 0, -1, 0, 0, 0},
    {-1, 0, 0, -1, 0, 0, 0, -1, 0, 0, 0, -1, 0, 0, 0, 0},
    {-1, 0, 0, 0, 0, -1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0},
    {-1, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0},
    {-1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1},
    {0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1},
    {0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1},
    {0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1},
    {0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 1, 0, 1},
    {0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 1},
    {0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1},
    {0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 1},
    {0, 1, 0, 1, 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 1},
    {0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 1},
    {0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1},
    {0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1},
    {0, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1},
    {0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2},
    {1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 2},
    {1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 2, 1, 1, 1, 1, 2},
    {1, 1, 1, 2, 1, 1, 1, 2, 1, 1, 1, 2, 1, 1, 1, 2},
    {1, 1, 1, 2, 1, 1, 2, 1, 1, 2, 1, 1, 2, 1, 1, 2},
    {1, 1, 2, 1, 1, 2, 1, 2, 1, 1, 2, 1, 1, 2, 1, 2},
    {1, 1, 2, 1, 2, 1, 2, 1, 1, 2, 1, 2, 1, 2, 1, 2},
    {1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2},
    {1, 2, 1, 2, 1, 2, 1, 2, 2, 1, 2, 1, 2, 1, 2, 2},
    {1, 2, 1, 2, 2, 1, 2, 2, 1, 2, 1, 2, 2, 1, 2, 2},
    {1, 2, 2, 1, 2, 2, 1, 2, 2, 1, 2, 2, 1, 2, 2, 2},
    {1, 2, 2, 2, 1, 2, 2, 2, 1, 2, 2, 2, 1, 2, 2, 2},
    {1, 2, 2, 2, 2, 1, 2, 2, 2, 2, 1, 2, 2, 2, 2, 2},
    {1, 2, 2, 2, 2, 2, 2, 2, 1, 2, 2, 2, 2, 2, 2, 2},
    {1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2},
}};
constexpr DeltaTable<8> kEightFrameDeltas{{
    {-4, -4, -4, -4, -4, -4, -4, -3}, {-4, -4, -4, -4, -3, -4, -4, -3},
    {-4, -4, -3, -4, -4, -3, -4, -3}, {-4, -4, -3, -4, -3, -4, -3, -3},
    {-4, -3, -4, -3, -3, -4, -3, -3}, {-4, -3, -3, -4, -3, -3, -3, -3},
    {-4, -3, -3, -3, -3, -3, -3, -3}, {-4, -3, -3, -3, -3, -3, -3, -2},
    {-3, -3, -3, -3, -3, -3, -3, -2}, {-3, -3, -3, -3, -2, -3, -3, -2},
    {-3, -3, -2, -3, -3, -2, -3, -2}, {-3, -3, -2, -3, -2, -3, -2, -2},
    {-3, -2, -3, -2, -2, -3, -2, -2}, {-3, -2, -2, -3, -2, -2, -2, -2},
    {-3, -2, -2, -2, -2, -2, -2, -2}, {-3, -2, -2, -2, -2, -2, -2, -1},
    {-2, -2, -2, -2, -2, -2, -2, -1}, {-2, -2, -2, -2, -1, -2, -2, -1},
    {-2, -2, -1, -2, -2, -1, -2, -1}, {-2, -2, -1, -2, -1, -2, -1, -1},
    {-2, -1, -2, -1, -1, -2, -1, -1}, {-2, -1, -1, -2, -1, -1, -1, -1},
    {-2, -1, -1, -1, -1, -1, -1, -1}, {-2, -1, -1, -1, -1, -1, -1, 0},
    {-1, -1, -1, -1, -1, -1, -1, 0},  {-1, -1, -1, -1, 0, -1, -1, 0},
    {-1, -1, 0, -1, -1, 0, -1, 0},    {-1, -1, 0, -1, 0, -1, 0, 0},
    {-1, 0, -1, 0, 0, -1, 0, 0},      {-1, 0, 0, -1, 0, 0, 0, 0},
    {-1, 0, 0, 0, 0, 0, 0, 0},        {0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 1},         {0, 0, 0, 1, 0, 0, 0, 1},
    {0, 0, 1, 0, 0, 1, 0, 1},         {0, 1, 0, 1, 0, 1, 0, 1},
    {0, 1, 0, 1, 1, 0, 1, 1},         {0, 1, 1, 1, 0, 1, 1, 1},
    {0, 1, 1, 1, 1, 1, 1, 1},         {1, 1, 1, 1, 1, 1, 1, 1},
    {1, 1, 1, 1, 1, 1, 1, 2},         {1, 1, 1, 2, 1, 1, 1, 2},
    {1, 1, 2, 1, 1, 2, 1, 2},         {1, 2, 1, 2, 1, 2, 1, 2},
    {1, 2, 1, 2, 2, 1, 2, 2},         {1, 2, 2, 2, 1, 2, 2, 2},
    {1, 2, 2, 2, 2, 2, 2, 2},         {2, 2, 2, 2, 2, 2, 2, 2},
    {2, 2, 2, 2, 2, 2, 2, 3},         {2, 2, 2, 3, 2, 2, 2, 3},
    {2, 2, 3, 2, 2, 3, 2, 3},         {2, 3, 2, 3, 2, 3, 2, 3},
    {2, 3, 2, 3, 3, 2, 3, 3},         {2, 3, 3, 3, 2, 3, 3, 3},
    {2, 3, 3, 3, 3, 3, 3, 3},         {3, 3, 3, 3, 3, 3, 3, 3},
    {3, 3, 3, 3, 3, 3, 3, 4},         {3, 3, 3, 4, 3, 3, 3, 4},
    {3, 3, 4, 3, 3, 4, 3, 4},         {3, 4, 3, 4, 3, 4, 3, 4},
    {3, 4, 3, 4, 4, 3, 4, 4},         {3, 4, 4, 4, 3, 4, 4, 4},
    {3, 4, 4, 4, 4, 4, 4, 4},
}};
constexpr DeltaTable<4> kFourFrameDeltas{{
    {-8, -8, -8, -7}, {-8, -8, -7, -7}, {-8, -7, -7, -7}, {-8, -7, -7, -6},
    {-7, -7, -7, -6}, {-7, -7, -6, -6}, {-7, -6, -6, -6}, {-7, -6, -6, -5},
    {-6, -6, -6, -5}, {-6, -6, -5, -5}, {-6, -5, -5, -5}, {-6, -5, -5, -4},
    {-5, -5, -5, -4}, {-5, -5, -4, -4}, {-5, -4, -4, -4}, {-5, -4, -4, -3},
    {-4, -4, -4, -3}, {-4, -4, -3, -3}, {-4, -3, -3, -3}, {-4, -3, -3, -2},
    {-3, -3, -3, -2}, {-3, -3, -2, -2}, {-3, -2, -2, -2}, {-3, -2, -2, -1},
    {-2, -2, -2, -1}, {-2, -2, -1, -1}, {-2, -1, -1, -1}, {-2, -1, -1, 0},
    {-1, -1, -1, 0},  {-1, -1, 0, 0},   {-1, 0, 0, 0},    {0, 0, 0, 0},
    {0, 0, 0, 1},     {0, 1, 0, 1},     {0, 1, 1, 1},     {1, 1, 1, 1},
    {1, 1, 1, 2},     {1, 2, 1, 2},     {1, 2, 2, 2},     {2, 2, 2, 2},
    {2, 2, 2, 3},     {2, 3, 2, 3},     {2, 3, 3, 3},     {3, 3, 3, 3},
    {3, 3, 3, 4},     {3, 4, 3, 4},     {3, 4, 4, 4},     {4, 4, 4, 4},
    {4, 4, 4, 5},     {4, 5, 4, 5},     {4, 5, 5, 5},     {5, 5, 5, 5},
    {5, 5, 5, 6},     {5, 6, 5, 6},     {5, 6, 6, 6},     {6, 6, 6, 6},
    {6, 6, 6, 7},     {6, 7, 6, 7},     {6, 7, 7, 7},     {7, 7, 7, 7},
    {7, 7, 7, 8},     {7, 8, 7, 8},     {7, 8, 8, 8},
}};
constexpr DeltaTable<2> kTwoFrameDeltas{{
    {-16, -15}, {-16, -14}, {-15, -14}, {-15, -13}, {-14, -13}, {-14, -12},
    {-13, -12}, {-13, -11}, {-12, -11}, {-12, -10}, {-11, -10}, {-11, -9},
    {-10, -9},  {-10, -8},  {-9, -8},   {-9, -7},   {-8, -7},   {-8, -6},
    {-7, -6},   {-7, -5},   {-6, -5},   {-6, -4},   {-5, -4},   {-5, -3},
    {-4, -3},   {-4, -2},   {-3, -2},   {-3, -1},   {-2, -1},   {-2, 0},
    {-1, 0},    {0, 0},     {0, 1},     {1, 1},     {1, 2},     {2, 2},
    {2, 3},     {3, 3},     {3, 4},     {4, 4},     {4, 5},     {5, 5},
    {5, 6},     {6, 6},     {6, 7},     {7, 7},     {7, 8},     {8, 8},
    {8, 9},     {9, 9},     {9, 10},    {10, 10},   {10, 11},   {11, 11},
    {11, 12},   {12, 12},   {12, 13},   {13, 13},   {13, 14},   {14, 14},
    {14, 15},   {15, 15},   {15, 16},
}};

constexpr std::uint32_t kChannelMask = 0x1F;
constexpr unsigned kGreenShift = 5;
constexpr unsigned kBlueShift = 10;
constexpr unsigned kMaskShift = 15;
constexpr std::uint16_t kColorBits = 0x7FFF;
constexpr std::int32_t kGreenLuminanceWeight = 3;
constexpr std::int32_t kGreyDivisor = 6;
constexpr std::int32_t kDarkGreyDivisor = 12;
constexpr std::int32_t kThreeQuarters = 3;
constexpr unsigned kQuarterShift = 2;
// The CLUT transition's tick waits speed / 8 frames between steps, and the
// speed picks the table: 16 columns from 8, 8 from 4, 4 from 2, else 2.
constexpr std::uint8_t kTicksPerSpeedStep = 8;
constexpr std::uint8_t kSixteenColumnSpeed = 8;
constexpr std::uint8_t kEightColumnSpeed = 4;
constexpr std::uint8_t kFourColumnSpeed = 2;
constexpr std::int32_t kFixedOne = 0x10000;
constexpr std::uint32_t kWholeMask = 0xFFFF0000U;
constexpr std::int32_t kBackgroundChannelLimit = 0xFF0000;
constexpr std::int32_t kTintChannelLimit = 0x800000;
constexpr std::uint32_t kByteMask = 0xFF;
constexpr unsigned kByteShift = 8;
constexpr unsigned kWordShift = 16;
// The room tint shader takes the blend in quarters.
constexpr unsigned kRoomTintBlendShift = 2;

[[nodiscard]] constexpr Channels ChannelsOf(std::uint16_t color) noexcept {
  return {static_cast<std::int32_t>(color & kChannelMask),
          static_cast<std::int32_t>((color >> kGreenShift) & kChannelMask),
          static_cast<std::int32_t>((color >> kBlueShift) & kChannelMask)};
}

[[nodiscard]] constexpr std::int32_t LuminanceOf(
    const Channels& channels) noexcept {
  return (channels[0] * 2) + (channels[1] * kGreenLuminanceWeight) +
         channels[2];
}

// The 15-bit colour plus the STP bit, wrapped like the original's halfword
// store.
[[nodiscard]] constexpr std::uint16_t PackColor(std::uint32_t red,
                                                std::uint32_t green,
                                                std::uint32_t blue,
                                                std::uint32_t mask) noexcept {
  return static_cast<std::uint16_t>(red + (green << kGreenShift) +
                                    (blue << kBlueShift) +
                                    (mask << kMaskShift));
}

[[nodiscard]] constexpr std::uint16_t PackColor(const RgbBytes& rgb,
                                                std::uint8_t mask) noexcept {
  return PackColor(rgb[0], rgb[1], rgb[2], mask);
}

// The original kept only a channel's low 16 bits and clamped them to 0..31.
[[nodiscard]] constexpr std::uint8_t ClampChannel(std::int32_t value) noexcept {
  const std::int16_t low = WrapSigned16(static_cast<std::uint32_t>(value));
  if (low > kChannelMaximum) {
    return static_cast<std::uint8_t>(kChannelMaximum);
  }
  if (low <= 0) {
    return 0;
  }
  return static_cast<std::uint8_t>(low);
}

[[nodiscard]] constexpr RgbBytes ClampChannels(
    const Channels& target) noexcept {
  return {ClampChannel(target[0]), ClampChannel(target[1]),
          ClampChannel(target[2])};
}

[[nodiscard]] constexpr bool IsBlack(const RgbBytes& rgb) noexcept {
  return rgb[0] == 0 && rgb[1] == 0 && rgb[2] == 0;
}

[[nodiscard]] constexpr Channels CurrentChannels(
    const ClutColorFade& entry) noexcept {
  return {entry.rgb[0], entry.rgb[1], entry.rgb[2]};
}

[[nodiscard]] constexpr Channels OffsetEach(const ColorOffset& offset,
                                            std::int32_t value) noexcept {
  return {offset[0] + value, offset[1] + value, offset[2] + value};
}

[[nodiscard]] constexpr Channels OffsetChannels(
    const ColorOffset& offset, const Channels& value) noexcept {
  return {offset[0] + value[0], offset[1] + value[1], offset[2] + value[2]};
}

[[nodiscard]] constexpr Channels Halved(const Channels& value) noexcept {
  return {value[0] / 2, value[1] / 2, value[2] / 2};
}

[[nodiscard]] constexpr Channels ThreeQuartersOf(
    const Channels& value) noexcept {
  return {(value[0] * kThreeQuarters) >> kQuarterShift,
          (value[1] * kThreeQuarters) >> kQuarterShift,
          (value[2] * kThreeQuarters) >> kQuarterShift};
}

// _transformClutSlot's targets (main.c:10612-10706) for one entry.
[[nodiscard]] constexpr Channels ClutTarget(
    ColorTransform mode, const ClutColorFade& entry, std::uint16_t base,
    const ColorOffset& offset) noexcept {
  const Channels current = CurrentChannels(entry);
  const Channels base_channels = ChannelsOf(base);
  switch (mode) {
    case ColorTransform::kHalveCurrent:
      return OffsetChannels(offset, Halved(current));
    case ColorTransform::kGreyCurrent:
      return OffsetEach(offset, LuminanceOf(current) / kGreyDivisor);
    case ColorTransform::kDarkGreyCurrent:
      return OffsetEach(offset, LuminanceOf(current) / kDarkGreyDivisor);
    case ColorTransform::kOffsetBase:
    case ColorTransform::kPulse:
      return OffsetChannels(offset, base_channels);
    case ColorTransform::kHalveBase:
      return OffsetChannels(offset, Halved(base_channels));
    case ColorTransform::kGreyBase:
      return OffsetEach(offset, LuminanceOf(base_channels) / kGreyDivisor);
    case ColorTransform::kDarkGreyBase:
      return OffsetEach(offset, LuminanceOf(base_channels) / kDarkGreyDivisor);
    case ColorTransform::kRestoreBase:
    case ColorTransform::kRestoreBaseFinal:
      return base_channels;
    case ColorTransform::kInvert:
      return OffsetChannels(
          offset, {kChannelMaximum - current[0], kChannelMaximum - current[1],
                   kChannelMaximum - current[2]});
    case ColorTransform::kThreeQuarterBase:
      return OffsetChannels(offset, ThreeQuartersOf(base_channels));
    default:
      return OffsetChannels(offset, current);
  }
}

// func_80048B8C's targets (main.c:11512-11545); a zero colour stays zero.
[[nodiscard]] constexpr Channels BaseTarget(
    ColorTransform mode, std::uint16_t value,
    const ColorOffset& offset) noexcept {
  const Channels channels = ChannelsOf(value);
  if (value == 0) {
    return channels;
  }
  switch (mode) {
    case ColorTransform::kHalveBase:
      return OffsetChannels(offset, Halved(channels));
    case ColorTransform::kGreyBase:
      return OffsetEach(offset, LuminanceOf(channels) / kGreyDivisor);
    case ColorTransform::kDarkGreyBase:
      return OffsetEach(offset, LuminanceOf(channels) / kDarkGreyDivisor);
    default:
      return OffsetChannels(offset, channels);
  }
}

// Steps an entry toward its clamped target, or sets it, as _transformClutSlot
// does (main.c:10708-10746); a target clamped to black keeps blue at 1.
void StoreClutTarget(ClutColorFade& entry, std::uint16_t& packed,
                     const Channels& target, bool timed) noexcept {
  RgbBytes channels = ClampChannels(target);
  if (IsBlack(channels)) {
    channels[2] = 1;
  }
  if (timed) {
    for (std::size_t channel = 0; channel < kRgbChannelCount; ++channel) {
      entry.step_rows[channel] = static_cast<std::uint8_t>(
          channels[channel] - entry.rgb[channel] + kHoldStepRow);
    }
    return;
  }
  entry.rgb = channels;
  packed = PackColor(channels, entry.mask);
}

void RestartTimer(ColorTransitionTimer& timer, std::int16_t duration,
                  ColorTransform mode) noexcept {
  timer = {.active = true,
           .frame = 0,
           .tick = 0,
           .speed = static_cast<std::uint8_t>(duration),
           .mode = mode};
}

void StartTimer(ColorTransitionTimer& timer, std::int16_t duration) noexcept {
  RestartTimer(timer, duration, timer.mode);
}

[[nodiscard]] constexpr ColorOffset HalfOffset(
    const ColorOffset& offset) noexcept {
  return {static_cast<std::int16_t>(offset[0] / 2),
          static_cast<std::int16_t>(offset[1] / 2),
          static_cast<std::int16_t>(offset[2] / 2)};
}

[[nodiscard]] constexpr bool RowFits(std::size_t row) noexcept {
  return row < kAnimatedClutRowCount;
}

// The colours from first_color on stay within a 256-colour row.
[[nodiscard]] constexpr bool SpanFitsRow(std::uint32_t first_color,
                                         std::size_t count) noexcept {
  return first_color <= kClutColorCount &&
         count <= kClutColorCount - first_color;
}

// func_800483FC's step (main.c:11357-11440) of one row with one table; the
// returned column count ends the leg.
template <std::size_t Columns>
std::size_t StepClutRow(PaletteAnimation& animation, std::size_t row,
                        const DeltaTable<Columns>& deltas) noexcept {
  ClutRowAnimation& clut = animation.rows[row];
  const std::size_t column = clut.timer.frame;
  if (column >= Columns) {
    ++animation.refused_steps;
    return Columns;
  }
  for (std::size_t index = 0; index < kClutColorCount; ++index) {
    ClutColorFade& entry = clut.colors[index];
    if (entry.rgb[0] + entry.rgb[1] + entry.rgb[2] == 0) {
      continue;
    }
    std::array<std::uint16_t, kRgbChannelCount> next{};
    for (std::size_t channel = 0; channel < kRgbChannelCount; ++channel) {
      const std::size_t step_row = entry.step_rows[channel];
      std::int32_t delta = 0;
      if (step_row < kStepRowCount) {
        delta = deltas[step_row][column];
      } else {
        ++animation.refused_steps;
      }
      next[channel] = static_cast<std::uint16_t>(entry.rgb[channel] + delta);
    }
    if (next[0] == 0 && next[1] == 0 && next[2] == 0) {
      next[2] = 1;
    }
    for (std::size_t channel = 0; channel < kRgbChannelCount; ++channel) {
      entry.rgb[channel] = static_cast<std::uint8_t>(next[channel]);
    }
    animation.colors[row][index] =
        PackColor(next[0], next[1], next[2], entry.mask);
  }
  return Columns;
}

std::size_t StepClutRow(PaletteAnimation& animation, std::size_t row) noexcept {
  const std::uint8_t speed = animation.rows[row].timer.speed;
  if (speed >= kSixteenColumnSpeed) {
    return StepClutRow(animation, row, kSixteenFrameDeltas);
  }
  if (speed >= kEightColumnSpeed) {
    return StepClutRow(animation, row, kEightFrameDeltas);
  }
  if (speed >= kFourColumnSpeed) {
    return StepClutRow(animation, row, kFourFrameDeltas);
  }
  return StepClutRow(animation, row, kTwoFrameDeltas);
}

// func_800483FC's leg end (main.c:11442-11461): a pulse alternates between
// the base plus its offset and plus half of it; kFadeThenRestore restores the
// base.
void FinishClutLeg(PaletteAnimation& animation, std::size_t row) noexcept {
  ClutRowAnimation& clut = animation.rows[row];
  const std::int16_t speed = clut.timer.speed;
  const ColorOffset pulse{clut.pulse[0], clut.pulse[1], clut.pulse[2]};
  switch (clut.timer.mode) {
    case ColorTransform::kPulse:
      ApplyClutRowTransform(animation, ColorTransform::kOffsetBase, speed, row,
                            HalfOffset(pulse));
      clut.timer.mode = ColorTransform::kPulseReturn;
      break;
    case ColorTransform::kPulseReturn:
      ApplyClutRowTransform(animation, ColorTransform::kPulse, speed, row,
                            pulse);
      clut.timer.mode = ColorTransform::kPulse;
      break;
    case ColorTransform::kFadeThenRestore:
      ApplyClutRowTransform(animation, ColorTransform::kRestoreBaseFinal, speed,
                            row, {});
      clut.timer.mode = ColorTransform::kRestoreBaseFinal;
      break;
    default:
      clut.timer.active = false;
      break;
  }
}

// _weightedLuminance's 16.16 form over the current channels, wrapping like the
// original's word arithmetic.
[[nodiscard]] constexpr std::int32_t FixedLuminanceOf(
    const FixedRgb& color) noexcept {
  return WrappingAdd(
      WrappingAdd(WrappingMultiply(color[0].value, 2),
                  WrappingMultiply(color[1].value, kGreenLuminanceWeight)),
      color[2].value);
}

[[nodiscard]] constexpr std::int32_t BaseLuminanceOf(
    const RgbBytes& base) noexcept {
  return LuminanceOf({base[0], base[1], base[2]});
}

[[nodiscard]] constexpr std::int32_t WholePart(std::int32_t fixed) noexcept {
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(fixed) &
                                   kWholeMask);
}

// The whole part, truncated toward zero like the original's rounding shift.
[[nodiscard]] constexpr std::int32_t WholeOf(std::int32_t fixed) noexcept {
  return fixed / kFixedOne;
}

[[nodiscard]] constexpr std::int32_t FixedStep(std::int32_t target,
                                               std::int32_t current,
                                               std::int16_t duration) noexcept {
  return WrappingSubtract(target, WholePart(current)) /
         (std::int32_t{duration} * 2);
}

// The 16.16 targets func_800474DC and func_80047B30 share (main.c:10845-10930
// and 11067-11160). Only the model tints read kRestoreBaseFinal as a restore.
[[nodiscard]] std::int32_t FixedTarget(ColorTransform mode,
                                       const FixedRgb& current,
                                       const RgbBytes& base,
                                       std::size_t channel, std::int16_t offset,
                                       bool final_restores_base) noexcept {
  const std::int32_t fixed_offset = ToFixed16(offset);
  switch (mode) {
    case ColorTransform::kHalveCurrent:
      return WrappingAdd(current[channel].value / 2, fixed_offset);
    case ColorTransform::kGreyCurrent:
      return WrappingAdd(FixedLuminanceOf(current) / kGreyDivisor,
                         fixed_offset);
    case ColorTransform::kDarkGreyCurrent:
      return WrappingAdd(FixedLuminanceOf(current) / kDarkGreyDivisor,
                         fixed_offset);
    case ColorTransform::kOffsetBase:
    case ColorTransform::kPulse:
      return ToFixed16(base[channel] + offset);
    case ColorTransform::kHalveBase:
      return WrappingAdd(ToFixed16(base[channel]) / 2, fixed_offset);
    case ColorTransform::kGreyBase:
      return WrappingAdd(ToFixed16(BaseLuminanceOf(base)) / kGreyDivisor,
                         fixed_offset);
    case ColorTransform::kDarkGreyBase:
      return WrappingAdd(ToFixed16(BaseLuminanceOf(base)) / kDarkGreyDivisor,
                         fixed_offset);
    case ColorTransform::kRestoreBase:
      return ToFixed16(base[channel]);
    case ColorTransform::kRestoreBaseFinal:
      if (final_restores_base) {
        return ToFixed16(base[channel]);
      }
      return WrappingAdd(current[channel].value, fixed_offset);
    default:
      return WrappingAdd(current[channel].value, fixed_offset);
  }
}

[[nodiscard]] Channels FixedTargets(ColorTransform mode,
                                    const FixedRgb& current,
                                    const RgbBytes& base,
                                    const ColorOffset& offset,
                                    bool final_restores_base,
                                    std::int32_t limit) noexcept {
  Channels targets{};
  for (std::size_t channel = 0; channel < kRgbChannelCount; ++channel) {
    std::int32_t target = WholePart(FixedTarget(
        mode, current, base, channel, offset[channel], final_restores_base));
    if (target > limit) {
      target = limit;
    }
    if (target < 0) {
      target = 0;
    }
    targets[channel] = target;
  }
  return targets;
}

// Steps the channels toward the targets over the duration, or sets them.
void StoreFixedTargets(FixedRgb& color, const Channels& targets,
                       std::int16_t duration) noexcept {
  for (std::size_t channel = 0; channel < kRgbChannelCount; ++channel) {
    if (duration != 0) {
      color[channel].step =
          FixedStep(targets[channel], color[channel].value, duration);
    } else {
      color[channel].value = targets[channel];
    }
  }
}

[[nodiscard]] Channels FixedOf(const RgbBytes& color) noexcept {
  return {ToFixed16(color[0]), ToFixed16(color[1]), ToFixed16(color[2])};
}

void AdvanceFixed(FixedRgb& color) noexcept {
  for (FixedChannel& channel : color) {
    channel.value = WrappingAdd(channel.value, channel.step);
  }
}

[[nodiscard]] constexpr BackgroundColors PackBackground(
    const BackgroundGradient& background) noexcept {
  BackgroundColors packed{};
  for (std::size_t index = 0; index < kGradientColorCount; ++index) {
    const FixedRgb& color = background.colors[index];
    packed[index] = static_cast<std::int32_t>(
        static_cast<std::uint32_t>(WholeOf(color[0].value)) +
        (static_cast<std::uint32_t>(WholeOf(color[1].value)) << kByteShift) +
        (static_cast<std::uint32_t>(WholeOf(color[2].value)) << kWordShift));
  }
  return packed;
}

[[nodiscard]] constexpr TintCode TintCodeOf(const FixedRgb& color) noexcept {
  TintCode code{};
  for (std::size_t channel = 0; channel < kRgbChannelCount; ++channel) {
    code[channel] = static_cast<std::uint8_t>(WholeOf(color[channel].value) -
                                              kNeutralTintChannel);
  }
  return code;
}

// Tint 0 is the room's, blended by its blend; tint n is actor n - 1's.
void ApplyTint(PaletteHost& host, std::size_t tint, const ModelTintFade& fade) {
  const TintCode code = TintCodeOf(fade.color);
  if (tint != 0) {
    host.SetActorTint(static_cast<std::uint32_t>(tint - 1), code);
  } else {
    host.SetRoomTint(code, std::int32_t{fade.blend} >> kRoomTintBlendShift);
  }
}

[[nodiscard]] constexpr bool MovesRoomTintToGrey(ColorTransform mode) noexcept {
  switch (mode) {
    case ColorTransform::kGreyCurrent:
    case ColorTransform::kDarkGreyCurrent:
    case ColorTransform::kHalveBase:
    case ColorTransform::kGreyBase:
    case ColorTransform::kDarkGreyBase:
      return true;
    default:
      return false;
  }
}

// func_800481C0's leg end (main.c:11318-11338). The pulse's first leg goes
// through the gated request, as in the original.
void FinishModelTintLeg(PaletteAnimation& animation, PaletteHost& host,
                        std::size_t tint) {
  ModelTintFade& fade = animation.tints[tint];
  const std::int16_t speed = fade.timer.speed;
  switch (fade.timer.mode) {
    case ColorTransform::kPulse:
      RequestModelTintTransform(animation, host, ColorTransform::kOffsetBase,
                                speed, tint, HalfOffset(fade.pulse));
      fade.timer.mode = ColorTransform::kPulseReturn;
      break;
    case ColorTransform::kPulseReturn:
      TransformModelTint(animation, host, ColorTransform::kPulse, speed, tint,
                         fade.pulse);
      fade.timer.mode = ColorTransform::kPulse;
      break;
    case ColorTransform::kFadeThenRestore:
      TransformModelTint(animation, host, ColorTransform::kRestoreBaseFinal,
                         speed, tint, {});
      fade.timer.mode = ColorTransform::kRestoreBaseFinal;
      break;
    default:
      fade.timer.active = false;
      break;
  }
}

}  // namespace

std::int32_t ToFixed16(std::int32_t value) noexcept {
  constexpr unsigned kFractionBits = 16;
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(value)
                                   << kFractionBits);
}

std::int32_t WeightedLuminance(std::uint16_t color) noexcept {
  return LuminanceOf(ChannelsOf(color));
}

bool BuildClutTransition(PaletteAnimation& animation, std::int16_t duration,
                         std::size_t row,
                         std::span<const std::uint16_t> source) noexcept {
  if (!RowFits(row) || source.size() < kClutColorCount) {
    return false;
  }
  ClutRowAnimation& clut = animation.rows[row];
  for (std::size_t index = 0; index < kClutColorCount; ++index) {
    const Channels channels = ChannelsOf(source[index]);
    ClutColorFade& entry = clut.colors[index];
    if (duration != 0) {
      for (std::size_t channel = 0; channel < kRgbChannelCount; ++channel) {
        entry.step_rows[channel] = static_cast<std::uint8_t>(
            channels[channel] - entry.rgb[channel] + kHoldStepRow);
      }
    } else {
      entry.rgb = {static_cast<std::uint8_t>(channels[0]),
                   static_cast<std::uint8_t>(channels[1]),
                   static_cast<std::uint8_t>(channels[2])};
      animation.colors[row][index] = PackColor(entry.rgb, entry.mask);
    }
  }
  if (duration != 0) {
    StartTimer(clut.timer, duration);
  } else {
    clut.timer.active = false;
  }
  return true;
}

bool StartClutTransition(PaletteAnimation& animation, std::int16_t duration,
                         std::size_t row, std::span<const std::uint16_t> source,
                         std::uint32_t clut_offset) noexcept {
  const std::size_t begin = std::size_t{clut_offset} * kClutOffsetColors;
  if (!RowFits(row) || begin > source.size() ||
      source.size() - begin < kClutColorCount) {
    return false;
  }
  if (!animation.enabled) {
    return true;
  }
  BuildClutTransition(animation, duration, row,
                      source.subspan(begin, kClutColorCount));
  animation.upload_pending = true;
  return true;
}

bool StartClutTransitionWithBase(PaletteAnimation& animation,
                                 std::int16_t duration, std::size_t row,
                                 std::span<const std::uint16_t> source,
                                 std::uint32_t clut_offset) noexcept {
  if (!StartClutTransition(animation, duration, row, source, clut_offset)) {
    return false;
  }
  if (animation.enabled) {
    ClutRowColors& base = animation.rows[row].base;
    for (std::size_t index = 0; index < kClutColorCount; ++index) {
      base[index] = source[index];
    }
  }
  return true;
}

bool SaveClutBase(PaletteAnimation& animation, std::size_t row) noexcept {
  if (!RowFits(row)) {
    return false;
  }
  if (animation.enabled) {
    animation.rows[row].base = animation.colors[row];
  }
  return true;
}

bool ApplyClutRowTransform(PaletteAnimation& animation, ColorTransform mode,
                           std::int16_t duration, std::size_t row,
                           const ColorOffset& offset) noexcept {
  if (!RowFits(row)) {
    return false;
  }
  ClutRowAnimation& clut = animation.rows[row];
  for (std::size_t index = 0; index < kClutColorCount; ++index) {
    ClutColorFade& entry = clut.colors[index];
    if (entry.rgb[0] + entry.rgb[1] + entry.rgb[2] + entry.mask == 0) {
      entry.step_rows = {kHoldStepRow, kHoldStepRow, kHoldStepRow};
      continue;
    }
    // The original returned from inside its colour loop, so entries before
    // the first non-black one still hold.
    if (mode == ColorTransform::kCancelPulse) {
      clut.timer.mode = ColorTransform::kOffsetCurrent;
      return true;
    }
    StoreClutTarget(entry, animation.colors[row][index],
                    ClutTarget(mode, entry, clut.base[index], offset),
                    duration != 0);
  }
  if (duration != 0) {
    RestartTimer(clut.timer, duration, mode);
    if (mode == ColorTransform::kPulse) {
      clut.pulse = {static_cast<std::int8_t>(offset[0]),
                    static_cast<std::int8_t>(offset[1]),
                    static_cast<std::int8_t>(offset[2])};
    }
  } else {
    clut.timer.active = false;
    animation.upload_pending = true;
  }
  return true;
}

bool TransformClutRow(PaletteAnimation& animation, ColorTransform mode,
                      std::int16_t duration, std::size_t row,
                      const ColorOffset& offset) noexcept {
  if (!RowFits(row)) {
    return false;
  }
  if (animation.enabled) {
    ApplyClutRowTransform(animation, mode, duration, row, offset);
  }
  return true;
}

void SetBackgroundGradientColors(
    PaletteAnimation& animation, PaletteHost& host, std::int16_t duration,
    std::span<const RgbBytes, kGradientColorCount> colors) {
  if (!animation.enabled) {
    return;
  }
  BackgroundGradient& background = animation.background;
  for (std::size_t index = 0; index < kGradientColorCount; ++index) {
    StoreFixedTargets(background.colors[index], FixedOf(colors[index]),
                      duration);
  }
  if (duration != 0) {
    StartTimer(background.timer, duration);
    return;
  }
  background.timer.active = false;
  host.SetRoomBackgroundColors(PackBackground(background));
}

void SetBackgroundGradientBase(
    PaletteAnimation& animation, PaletteHost& host, std::int16_t duration,
    std::span<const RgbBytes, kGradientColorCount> colors) {
  if (!animation.enabled) {
    return;
  }
  SetBackgroundGradientColors(animation, host, duration, colors);
  for (std::size_t index = 0; index < kGradientColorCount; ++index) {
    animation.background.base[index] = colors[index];
  }
}

void TransformBackgroundGradient(PaletteAnimation& animation, PaletteHost& host,
                                 ColorTransform mode, std::int16_t duration,
                                 const ColorOffset& offset) {
  BackgroundGradient& background = animation.background;
  if (mode == ColorTransform::kCancelPulse) {
    background.timer.mode = ColorTransform::kOffsetCurrent;
    return;
  }
  for (std::size_t index = 0; index < kGradientColorCount; ++index) {
    FixedRgb& color = background.colors[index];
    StoreFixedTargets(color,
                      FixedTargets(mode, color, background.base[index], offset,
                                   false, kBackgroundChannelLimit),
                      duration);
  }
  if (duration != 0) {
    RestartTimer(background.timer, duration, mode);
    if (mode == ColorTransform::kPulse) {
      background.pulse = offset;
    }
    return;
  }
  background.timer.active = false;
  host.SetRoomBackgroundColors(PackBackground(background));
}

void RequestBackgroundGradientTransform(PaletteAnimation& animation,
                                        PaletteHost& host, ColorTransform mode,
                                        std::int16_t duration,
                                        const ColorOffset& offset) {
  if (animation.enabled) {
    TransformBackgroundGradient(animation, host, mode, duration, offset);
  }
}

bool SetModelTint(PaletteAnimation& animation, PaletteHost& host,
                  std::int16_t duration, std::size_t tint,
                  const RgbBytes& color) {
  if (tint >= kModelTintCount) {
    return false;
  }
  if (!animation.enabled) {
    return true;
  }
  ModelTintFade& fade = animation.tints[tint];
  StoreFixedTargets(fade.color, FixedOf(color), duration);
  if (duration != 0) {
    StartTimer(fade.timer, duration);
    return true;
  }
  fade.timer.active = false;
  ApplyTint(host, tint, fade);
  return true;
}

bool SetModelTintBase(PaletteAnimation& animation, PaletteHost& host,
                      std::int16_t duration, std::size_t tint,
                      const RgbBytes& color) {
  if (tint >= kModelTintCount) {
    return false;
  }
  if (animation.enabled) {
    SetModelTint(animation, host, duration, tint, color);
    animation.tints[tint].base = color;
  }
  return true;
}

bool TransformModelTint(PaletteAnimation& animation, PaletteHost& host,
                        ColorTransform mode, std::int16_t duration,
                        std::size_t tint, const ColorOffset& offset) {
  if (tint >= kModelTintCount) {
    return false;
  }
  ModelTintFade& fade = animation.tints[tint];
  if (mode == ColorTransform::kCancelPulse) {
    fade.timer.mode = ColorTransform::kOffsetCurrent;
    return true;
  }
  const std::int16_t blend_target =
      MovesRoomTintToGrey(mode) ? kGreyRoomTintBlend : std::int16_t{0};
  StoreFixedTargets(fade.color,
                    FixedTargets(mode, fade.color, fade.base, offset, true,
                                 kTintChannelLimit),
                    duration);
  if (duration != 0) {
    fade.blend_step = static_cast<std::int16_t>((blend_target - fade.blend) /
                                                (std::int32_t{duration} * 2));
    RestartTimer(fade.timer, duration, mode);
    if (mode == ColorTransform::kPulse) {
      fade.pulse = offset;
    }
    return true;
  }
  fade.blend = blend_target;
  fade.timer.active = false;
  ApplyTint(host, tint, fade);
  return true;
}

bool RequestModelTintTransform(PaletteAnimation& animation, PaletteHost& host,
                               ColorTransform mode, std::int16_t duration,
                               std::size_t tint, const ColorOffset& offset) {
  if (tint >= kModelTintCount) {
    return false;
  }
  if (animation.enabled) {
    TransformModelTint(animation, host, mode, duration, tint, offset);
  }
  return true;
}

void UpdateBackgroundGradientTransition(PaletteAnimation& animation,
                                        PaletteHost& host) {
  BackgroundGradient& background = animation.background;
  if (!background.timer.active) {
    return;
  }
  for (FixedRgb& color : background.colors) {
    AdvanceFixed(color);
  }
  host.SetRoomBackgroundColors(PackBackground(background));
  ++background.timer.frame;
  if (background.timer.frame < background.timer.speed * 2) {
    return;
  }
  const std::int16_t speed = background.timer.speed;
  switch (background.timer.mode) {
    case ColorTransform::kPulse:
      TransformBackgroundGradient(animation, host, ColorTransform::kOffsetBase,
                                  speed, HalfOffset(background.pulse));
      background.timer.mode = ColorTransform::kPulseReturn;
      break;
    case ColorTransform::kPulseReturn:
      TransformBackgroundGradient(animation, host, ColorTransform::kPulse,
                                  speed, background.pulse);
      background.timer.mode = ColorTransform::kPulse;
      break;
    default:
      background.timer.active = false;
      break;
  }
}

void UpdateModelTintTransitions(PaletteAnimation& animation,
                                PaletteHost& host) {
  for (std::size_t tint = 0; tint < kModelTintCount; ++tint) {
    ModelTintFade& fade = animation.tints[tint];
    if (!fade.timer.active) {
      continue;
    }
    AdvanceFixed(fade.color);
    if (tint == 0) {
      fade.blend = static_cast<std::int16_t>(fade.blend + fade.blend_step);
    }
    ApplyTint(host, tint, fade);
    ++fade.timer.frame;
    if (fade.timer.frame >= fade.timer.speed * 2) {
      FinishModelTintLeg(animation, host, tint);
    }
  }
}

void UpdateClutTransitions(PaletteAnimation& animation) noexcept {
  for (std::size_t row = 0; row < kAnimatedClutRowCount; ++row) {
    ColorTransitionTimer& timer = animation.rows[row].timer;
    if (!timer.active) {
      continue;
    }
    ++timer.tick;
    if (timer.tick >= timer.speed / kTicksPerSpeedStep) {
      timer.tick = 0;
      const std::size_t columns = StepClutRow(animation, row);
      ++timer.frame;
      if (timer.frame >= columns) {
        FinishClutLeg(animation, row);
      }
    }
    // Every running row re-uploads each frame, stepped or not.
    animation.upload_pending = true;
  }
}

bool StopClutTransition(PaletteAnimation& animation, std::size_t row) noexcept {
  if (!RowFits(row)) {
    return false;
  }
  animation.rows[row].timer.active = false;
  return true;
}

bool LoadClutColors(PaletteAnimation& animation, PaletteHost& host,
                    std::uint32_t row, std::uint32_t first_color,
                    std::span<const std::uint16_t> colors) {
  if (!SpanFitsRow(first_color, colors.size())) {
    return false;
  }
  if (!RowFits(row)) {
    if (row >= kVramHeight - kClutVramY) {
      return false;
    }
    host.UploadClutColors(row, first_color, colors);
    return true;
  }
  ClutRowAnimation& clut = animation.rows[row];
  if (clut.locked) {
    return true;
  }
  clut.timer.active = false;
  for (std::size_t index = 0; index < colors.size(); ++index) {
    const std::uint16_t color = colors[index];
    const std::size_t target = first_color + index;
    const Channels channels = ChannelsOf(color);
    animation.colors[row][target] = color;
    clut.base[target] = color;
    clut.colors[target].rgb = {static_cast<std::uint8_t>(channels[0]),
                               static_cast<std::uint8_t>(channels[1]),
                               static_cast<std::uint8_t>(channels[2])};
    clut.colors[target].mask = static_cast<std::uint8_t>(color >> kMaskShift);
  }
  animation.upload_pending = true;
  return true;
}

bool TransformClutBase(PaletteAnimation& animation, ColorTransform mode,
                       std::span<const std::uint16_t, kClutColorCount> source,
                       std::size_t row, const ColorOffset& offset,
                       bool update_current_colors) noexcept {
  if (!RowFits(row)) {
    return false;
  }
  ClutRowAnimation& clut = animation.rows[row];
  for (std::size_t index = 0; index < kClutColorCount; ++index) {
    const std::uint16_t value = source[index];
    RgbBytes channels = ClampChannels(BaseTarget(mode, value, offset));
    if (IsBlack(channels) && (value & kColorBits) != 0) {
      channels[2] = 1;
    }
    const auto mask = static_cast<std::uint8_t>(value >> kMaskShift);
    const std::uint16_t packed = PackColor(channels, mask);
    if (update_current_colors) {
      animation.colors[row][index] = packed;
      if (!clut.timer.active) {
        clut.colors[index].rgb = channels;
        clut.colors[index].mask = mask;
      }
    }
    clut.base[index] = packed;
  }
  animation.upload_pending = true;
  return true;
}

bool OffsetClutBase(PaletteAnimation& animation,
                    std::span<const std::uint16_t, kClutColorCount> source,
                    std::size_t row, const ColorOffset& offset,
                    bool update_current_colors) noexcept {
  return TransformClutBase(animation, ColorTransform::kRestoreBase, source, row,
                           offset, update_current_colors);
}

void CaptureRoomBackgroundColors(PaletteAnimation& animation,
                                 const BackgroundColors& room_colors) noexcept {
  BackgroundGradient& background = animation.background;
  for (std::size_t index = 0; index < kGradientColorCount; ++index) {
    const auto word = static_cast<std::uint32_t>(room_colors[index]);
    const RgbBytes base{
        static_cast<std::uint8_t>(word & kByteMask),
        static_cast<std::uint8_t>((word >> kByteShift) & kByteMask),
        static_cast<std::uint8_t>((word >> kWordShift) & kByteMask)};
    background.base[index] = base;
    const Channels fixed = FixedOf(base);
    for (std::size_t channel = 0; channel < kRgbChannelCount; ++channel) {
      background.colors[index][channel].value = fixed[channel];
    }
  }
}

void CommitClutRows(PaletteAnimation& animation, PaletteHost& host) {
  if (animation.upload_pending) {
    host.UploadAnimatedClutRows(animation.colors);
    animation.upload_pending = false;
  }
}

void SetClutAnimationEnabled(PaletteAnimation& animation,
                             std::int32_t enabled) noexcept {
  animation.enabled = static_cast<std::uint16_t>(enabled) != 0;
}

void InitializeClutAnimation(PaletteAnimation& animation) noexcept {
  const std::int32_t neutral_tint = ToFixed16(kNeutralTintChannel);
  animation.upload_pending = false;
  animation.enabled = true;
  for (ClutRowAnimation& clut : animation.rows) {
    clut.locked = false;
    clut.timer.active = false;
    clut.timer.mode = ColorTransform::kOffsetCurrent;
  }
  animation.background.timer.active = false;
  animation.background.timer.mode = ColorTransform::kOffsetCurrent;
  for (ModelTintFade& fade : animation.tints) {
    fade.timer.active = false;
    fade.timer.mode = ColorTransform::kOffsetCurrent;
    fade.blend = 0;
    fade.base = {kNeutralTintChannel, kNeutralTintChannel, kNeutralTintChannel};
    for (FixedChannel& channel : fade.color) {
      channel.value = neutral_tint;
    }
  }
}

void TransformRoomPalettes(PaletteAnimation& animation, ColorTransform mode,
                           std::int16_t duration,
                           const ColorOffset& offset) noexcept {
  for (std::size_t row = 0; row < kRoomClutRowCount; ++row) {
    TransformClutRow(animation, mode, duration, row, offset);
  }
}

void TransformSceneColors(PaletteAnimation& animation, PaletteHost& host,
                          ColorTransform mode, std::int16_t duration,
                          const ColorOffset& offset) {
  TransformRoomPalettes(animation, mode, duration, offset);
  RequestModelTintTransform(animation, host, mode, duration, 0, offset);
  TransformClutRow(animation, mode, duration, kSceneEffectClutRow, offset);
}

void TransformActorColors(PaletteAnimation& animation, PaletteHost& host,
                          ColorTransform mode, std::int16_t duration,
                          std::uint32_t actor, const ColorOffset& offset) {
  if (actor >= kColorTransformActorLimit) {
    return;
  }
  TransformClutRow(animation, mode, duration, kActorClutRowBase + actor,
                   offset);
  RequestModelTintTransform(animation, host, mode, duration,
                            std::size_t{actor} + 1, offset);
}

void TransformBackgroundColors(PaletteAnimation& animation, PaletteHost& host,
                               ColorTransform mode, std::int16_t duration,
                               const ColorOffset& offset) {
  RequestBackgroundGradientTransform(animation, host, mode, duration, offset);
}

bool StartRoomPaletteTransition(PaletteAnimation& animation,
                                std::int16_t duration,
                                std::span<const std::uint16_t> source,
                                std::uint32_t clut_offset) noexcept {
  const std::size_t needed = ((kRoomClutRowCount - 1) * kClutColorCount) +
                             (std::size_t{clut_offset} * kClutOffsetColors) +
                             kClutColorCount;
  if (source.size() < needed) {
    return false;
  }
  for (std::size_t row = 0; row < kRoomClutRowCount; ++row) {
    StartClutTransition(animation, duration, row,
                        source.subspan(row * kClutColorCount), clut_offset);
  }
  return true;
}

bool StartSharedPaletteTransition(PaletteAnimation& animation,
                                  std::int16_t duration, std::size_t shared_row,
                                  std::span<const std::uint16_t> source,
                                  std::uint32_t clut_offset) noexcept {
  if (shared_row >= kAnimatedClutRowCount - kActorClutRowBase) {
    return false;
  }
  return StartClutTransition(
      animation, duration, kActorClutRowBase + shared_row, source, clut_offset);
}

void ApplyBackgroundGradientColors(
    PaletteAnimation& animation, PaletteHost& host, std::int16_t duration,
    std::span<const RgbBytes, kGradientColorCount> colors) {
  SetBackgroundGradientColors(animation, host, duration, colors);
}
}  // namespace VS::Render
