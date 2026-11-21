#include "Core/Math/Projection.hpp"

#include <algorithm>
#include <array>
#include <cstdint>

#include "Core/Math/Distance.hpp"
#include "Core/Math/Matrix.hpp"
#include "Core/Math/ScalarMath.hpp"

// Fixed-point RTV0TR and RTPS (plan §6.1). Arithmetic follows the documented
// GTE behaviour: 4.12 matrices, 44-bit MAC accumulation, >>12 scaling,
// saturation of the 16-bit results and the Newton-Raphson projection divide.
// The GTE parity tests check the rounding and flags.
namespace VS::Core::Math {
namespace {
// FLAG bits: MAC1-3 positive/negative overflow (30-28/27-25), IR1-3 (24-22),
// SZ3 (18), divide overflow (17), MAC0 (16/15), SX2/SY2 (14/13) and IR0
// (12). Bit 31 summarizes bits 30-23 and 18-13.
constexpr std::uint32_t kFlagErrorBits = 0x7F87E000u;
constexpr std::uint32_t kFlagError = 0x80000000u;
constexpr std::int64_t kMacLimit = std::int64_t{1} << 43;
constexpr std::int64_t kIrMinimum = INT16_MIN;
constexpr std::int64_t kIrMaximum = INT16_MAX;
constexpr std::int64_t kScreenMinimum = -0x400;
constexpr std::int64_t kScreenMaximum = 0x3FF;
constexpr std::int64_t kDepthMaximum = UINT16_MAX;
constexpr std::int64_t kDepthCueMaximum = 0x1000;
constexpr unsigned kScreenShift = 16;
constexpr unsigned kQ12Shift = 12;

enum FlagBit : unsigned {
  kMacPositive = 30,
  kMacNegative = 27,
  kIrSaturated = 24,
  kDepthSaturated = 18,
  kDivideOverflow = 17,
  kMac0Positive = 16,
  kMac0Negative = 15,
  kScreenXSaturated = 14,
  kScreenYSaturated = 13,
  kDepthCueSaturated = 12,
};

[[nodiscard]] std::uint32_t FinishFlag(std::uint32_t flag) {
  return (flag & kFlagErrorBits) != 0 ? flag | kFlagError : flag;
}

// The GTE keeps MAC1-3 intermediates in signed 44 bits.
[[nodiscard]] std::int64_t WrapSigned44(std::int64_t value) {
  constexpr std::uint64_t kMask = (std::uint64_t{1} << 44) - 1;
  return static_cast<std::int64_t>((static_cast<std::uint64_t>(value) +
                                    static_cast<std::uint64_t>(kMacLimit)) &
                                   kMask) -
         kMacLimit;
}

[[nodiscard]] std::int64_t Limit(std::int64_t value, std::int64_t minimum,
                                 std::int64_t maximum, unsigned flag_bit,
                                 std::uint32_t& flag) {
  if (value < minimum || value > maximum) {
    flag |= 1u << flag_bit;
  }
  return std::clamp(value, minimum, maximum);
}

// MAC0 keeps the low word of a wider sum; flag the sum when it leaves the
// signed word range.
void CheckMac0(std::int64_t value, std::uint32_t& flag) {
  if (value > INT32_MAX) {
    flag |= 1u << kMac0Positive;
  } else if (value < INT32_MIN) {
    flag |= 1u << kMac0Negative;
  }
}

// RTV0TR without the summary bit, which RTPS sets after its own flags.
[[nodiscard]] RotatedVertex RotateTranslateUnsummarized(
    const Transform3& transform, Position3 vertex) {
  constexpr unsigned kRows = kMatrixOrder;
  const std::array<std::int64_t, kRows> input{vertex.x, vertex.y, vertex.z};
  const std::array<std::int64_t, kRows> translation{transform.translation.x,
                                                    transform.translation.y,
                                                    transform.translation.z};
  std::array<std::int32_t, kRows> mac{};
  RotatedVertex result;
  for (unsigned row = 0; row < kRows; ++row) {
    std::int64_t sum = translation.at(row) * kQ12Unit;
    for (unsigned column = 0; column < kRows; ++column) {
      sum += std::int64_t{transform.rotation.m.at(row).at(column)} *
             input.at(column);
      if (sum >= kMacLimit) {
        result.flag |= 1u << (kMacPositive - row);
      } else if (sum < -kMacLimit) {
        result.flag |= 1u << (kMacNegative - row);
      }
      sum = WrapSigned44(sum);
    }
    mac.at(row) = static_cast<std::int32_t>(sum >> kQ12Shift);
    result.ir.at(row) = static_cast<std::int16_t>(Limit(
        mac.at(row), kIrMinimum, kIrMaximum, kIrSaturated - row, result.flag));
  }
  result.mac = {mac[0], mac[1], mac[2]};
  return result;
}
}  // namespace

// decomp: SLUS_010.40 RotTrans (libgte), the RTV0TR operation.
RotatedVertex RotateTranslateVertex(const Transform3& transform,
                                    Position3 vertex) noexcept {
  RotatedVertex result = RotateTranslateUnsummarized(transform, vertex);
  result.flag = FinishFlag(result.flag);
  return result;
}

// decomp: SLUS_010.40 RotTransPers (src/SLUS_010.40/libgte/SMP_02.s,
// 0x80041814), the RTPS operation. The reciprocal divide, screen and
// depth-cue sums are wider than a word: SX2/SY2 and IR0 saturate from the
// wide sums while MAC0 keeps the low word.
PerspectiveVertex ProjectVertex(const Transform3& transform,
                                const PerspectiveProjection& projection,
                                Position3 vertex) noexcept {
  const RotatedVertex rotated = RotateTranslateUnsummarized(transform, vertex);
  PerspectiveVertex result;
  result.mac = rotated.mac;
  result.ir = rotated.ir;
  std::uint32_t flag = rotated.flag;
  result.depth = static_cast<std::uint16_t>(
      Limit(rotated.mac.z, 0, kDepthMaximum, kDepthSaturated, flag));
  if (PerspectiveDivideOverflows(projection.distance, result.depth)) {
    flag |= 1u << kDivideOverflow;
  }
  const std::int64_t gain =
      DividePerspective(projection.distance, result.depth);
  const std::int64_t x = gain * rotated.ir[0] + projection.offset_x;
  CheckMac0(x, flag);
  const std::int64_t y = gain * rotated.ir[1] + projection.offset_y;
  CheckMac0(y, flag);
  result.screen_x =
      static_cast<std::int16_t>(Limit(x >> kScreenShift, kScreenMinimum,
                                      kScreenMaximum, kScreenXSaturated, flag));
  result.screen_y =
      static_cast<std::int16_t>(Limit(y >> kScreenShift, kScreenMinimum,
                                      kScreenMaximum, kScreenYSaturated, flag));
  const std::int64_t cue =
      gain * projection.depth_cue_coefficient + projection.depth_cue_offset;
  CheckMac0(cue, flag);
  result.mac0 = static_cast<std::int32_t>(static_cast<std::uint32_t>(cue));
  result.ir0 = static_cast<std::int16_t>(
      Limit(cue >> kQ12Shift, 0, kDepthCueMaximum, kDepthCueSaturated, flag));
  result.flag = FinishFlag(flag);
  return result;
}
}  // namespace VS::Core::Math
