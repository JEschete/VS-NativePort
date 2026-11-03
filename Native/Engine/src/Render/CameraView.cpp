// Hand-maintained (tools/port/owned.csv).
#include "Render/CameraView.hpp"

#include <algorithm>
#include <array>
#include <cstdint>

#include "Core/Math/Vector.hpp"

namespace VS::Render {
namespace {
using Core::Math::kMatrixOrder;
using Core::Math::Mat3;
using Core::Math::Transform3;
using Core::Math::Vec3;

constexpr std::int32_t kOne = 4096;
constexpr int kFixedShift = 12;
// The view x axis is widened by 9/8 for the non-square field pixels.
constexpr std::int32_t kViewWidthScale = kOne * 9 / 8;
// Actor matrices narrow x by 3640/4096 instead.
constexpr std::int32_t kActorWidthScale = 3640;
constexpr std::int32_t kActorScaleDividend = kOne * kOne;
constexpr Vec3 kWorldUp{0, kOne, 0};
constexpr Vec3 kNegativeYAxis{0, -kOne, 0};
constexpr std::int32_t kFarClipToLodShift = 3;
// ApplyMatrixLV splits each word into sign-magnitude halves at bit 15.
constexpr int kLongSplitShift = 15;
constexpr std::uint32_t kLongSplitMask = (1u << kLongSplitShift) - 1;
constexpr std::int32_t kHighHalfScale = 1 << (kLongSplitShift - kFixedShift);

using Halfwords = std::array<std::int16_t, kMatrixOrder>;

std::int16_t Halfword(std::int32_t value) {
  return Core::Math::WrapSigned16(static_cast<std::uint32_t>(value));
}

std::int16_t NegatedHalfword(std::int32_t value) {
  return Halfword(Core::Math::WrappingNegate(value));
}

std::int32_t TruncatedWord(std::int64_t value) {
  return static_cast<std::int32_t>(
      static_cast<std::uint32_t>(static_cast<std::uint64_t>(value)));
}

std::int64_t RowProduct(const Mat3& matrix, int row, const Halfwords& vector) {
  std::int64_t sum = 0;
  for (int column = 0; column < kMatrixOrder; ++column) {
    sum += static_cast<std::int64_t>(matrix.m[row][column]) * vector[column];
  }
  return sum;
}

// libgte ApplyMatrix (MTX_05.s): RTV0 leaves (m * v) >> 12 in MAC1-MAC3,
// which are stored unsaturated.
Vec3 ApplyToHalfwords(const Mat3& matrix, const Halfwords& vector) {
  std::array<std::int32_t, kMatrixOrder> result{};
  for (int row = 0; row < kMatrixOrder; ++row) {
    result[row] = TruncatedWord(RowProduct(matrix, row, vector) >> kFixedShift);
  }
  return {result[0], result[1], result[2]};
}

struct SplitWords {
  Halfwords high;
  Halfwords low;
};

// Each component as high * 0x8000 + low with both halves carrying its sign;
// the high half reaches the GTE as a signed halfword.
SplitWords SplitLongComponents(const Vec3& vector) {
  const std::array<std::int32_t, kMatrixOrder> words{vector.x, vector.y,
                                                     vector.z};
  SplitWords split{};
  for (int index = 0; index < kMatrixOrder; ++index) {
    const std::int32_t word = words[index];
    const std::uint32_t magnitude = word < 0
                                        ? 0u - static_cast<std::uint32_t>(word)
                                        : static_cast<std::uint32_t>(word);
    std::int32_t high = static_cast<std::int32_t>(magnitude >> kLongSplitShift);
    std::int32_t low = static_cast<std::int32_t>(magnitude & kLongSplitMask);
    if (word < 0) {
      high = Core::Math::WrappingNegate(high);
      low = -low;
    }
    split.high[index] = Halfword(high);
    split.low[index] = static_cast<std::int16_t>(low);
  }
  return split;
}

// libgte ApplyMatrixLV (MTX_004.s): RTIR0 multiplies the high halves without
// a shift and RTIR12 the low halves with one; the high result is scaled by 8
// and the words added, all wrapping.
Vec3 ApplyToWords(const Mat3& matrix, const Vec3& vector) {
  const SplitWords split = SplitLongComponents(vector);
  std::array<std::int32_t, kMatrixOrder> result{};
  for (int row = 0; row < kMatrixOrder; ++row) {
    const std::int32_t high =
        TruncatedWord(RowProduct(matrix, row, split.high));
    const std::int32_t low =
        TruncatedWord(RowProduct(matrix, row, split.low) >> kFixedShift);
    result[row] = Core::Math::WrappingAdd(
        low, Core::Math::WrappingMultiply(high, kHighHalfScale));
  }
  return {result[0], result[1], result[2]};
}

Mat3 RollMatrix(std::int32_t angle) {
  const std::int16_t cosine = Halfword(Core::Math::Cosine(angle));
  const std::int16_t sine = Halfword(Core::Math::Sine(angle));
  Mat3 roll{};
  roll.m[0][0] = cosine;
  roll.m[1][1] = cosine;
  roll.m[1][0] = sine;
  roll.m[0][1] = NegatedHalfword(sine);
  roll.m[2][2] = kOne;
  return roll;
}

Vec3 Direction(const Vec3& vector) {
  return Core::Math::NormalizeVector(vector).direction;
}

Mat3 Transpose(const Mat3& matrix) {
  Mat3 transposed{};
  for (int row = 0; row < kMatrixOrder; ++row) {
    for (int column = 0; column < kMatrixOrder; ++column) {
      transposed.m[row][column] = matrix.m[column][row];
    }
  }
  return transposed;
}
}  // namespace

CameraView BuildCameraViewMatrix(const CameraPose& pose) noexcept {
  using Core::Math::WrappingSubtract;
  Vec3 delta{WrappingSubtract(pose.position.x, pose.look_at.x) / kOne,
             WrappingSubtract(pose.look_at.y, pose.position.y) / kOne,
             WrappingSubtract(pose.look_at.z, pose.position.z) / kOne};
  if ((delta.x | delta.z) == 0) {
    delta.z = 1;
  }
  const Vec3 forward = Direction(delta);
  const Vec3 right =
      Direction(Core::Math::CrossProductFixed(forward, kWorldUp));
  const Vec3 up = Direction(Core::Math::CrossProductFixed(right, forward));

  CameraView result{};
  result.basis.m[0] = {NegatedHalfword(right.x), Halfword(right.y),
                       Halfword(right.z)};
  result.basis.m[1] = {NegatedHalfword(up.x), Halfword(up.y), Halfword(up.z)};
  result.basis.m[2] = {NegatedHalfword(forward.x), Halfword(forward.y),
                       Halfword(forward.z)};

  const Mat3 roll = Core::Math::ScaleMatrixColumns(
      RollMatrix(pose.roll), {kViewWidthScale, kOne, kOne});
  const Mat3 rotation = Core::Math::MultiplyMatrixIntoRight(roll, result.basis);
  const Halfwords negative_position{
      Halfword(Core::Math::WrappingNegate(pose.position.x) / kOne),
      Halfword(Core::Math::WrappingNegate(pose.position.y) / kOne),
      Halfword(Core::Math::WrappingNegate(pose.position.z) / kOne)};
  const Vec3 translation = ApplyToHalfwords(rotation, negative_position);

  const std::int32_t far_clip = pose.far_clip;
  result.view.rotation =
      Core::Math::ScaleMatrixColumns(rotation, {far_clip, far_clip, far_clip});
  result.view.translation = {
      Core::Math::WrappingMultiply(translation.x, far_clip) / kOne,
      Core::Math::WrappingMultiply(translation.y, far_clip) / kOne,
      Core::Math::WrappingMultiply(translation.z, far_clip) / kOne};
  return result;
}

Transform3 BuildLookAtViewMatrix(Vec3 position, Vec3 target) noexcept {
  using Core::Math::WrappingSubtract;
  const Vec3 forward = Direction({WrappingSubtract(target.x, position.x),
                                  WrappingSubtract(target.y, position.y),
                                  WrappingSubtract(target.z, position.z)});
  const Vec3 right =
      Direction(Core::Math::CrossProductFixed(kNegativeYAxis, forward));
  const Vec3 up = Direction(Core::Math::CrossProductFixed(right, forward));

  Transform3 view{};
  view.rotation.m[0] = {NegatedHalfword(right.x), NegatedHalfword(right.y),
                        NegatedHalfword(right.z)};
  view.rotation.m[1] = {Halfword(up.x), Halfword(up.y), Halfword(up.z)};
  view.rotation.m[2] = {Halfword(forward.x), Halfword(forward.y),
                        Halfword(forward.z)};
  const Vec3 translation = ApplyToWords(view.rotation, position);
  view.translation = {Core::Math::WrappingNegate(translation.x),
                      Core::Math::WrappingNegate(translation.y),
                      Core::Math::WrappingNegate(translation.z)};
  return view;
}

ActorCameraMatrices PrepareActorCameraMatrices(const Mat3& basis,
                                               std::int32_t roll,
                                               std::int32_t far_clip) noexcept {
  const Mat3 inverse = Core::Math::ScaleMatrixColumns(
      Transpose(basis), {kActorWidthScale, kOne, kOne});
  const Mat3 rolled = Core::Math::MultiplyMatrixIntoRight(
      inverse, RollMatrix(Core::Math::WrappingNegate(roll)));
  const std::int32_t far_scale =
      Core::Math::R3000Quotient(kActorScaleDividend, far_clip);
  return {
      Core::Math::ScaleMatrixColumns(rolled, {far_scale, far_scale, far_scale}),
      far_scale};
}

std::int32_t TextureLodDepthForFarClip(std::int32_t far_clip) noexcept {
  return std::clamp(far_clip / (1 << kFarClipToLodShift), 0,
                    kMaxRoomTextureLodDepth);
}

void SetCameraNearClip(CameraClipTarget& target, std::int32_t near_clip) {
  target.RecordNearClip(near_clip);
  target.ClipRoomPolygonsNear(near_clip);
}

void SetCameraProjectionDistance(CameraClipTarget& target,
                                 std::int32_t distance) {
  target.RecordProjectionDistance(distance);
  target.SetScreenDistance(distance);
}

void SetCameraFarClip(CameraClipTarget& target, std::int32_t far_clip,
                      bool preserve_texture_lod) {
  target.RecordFarClip(far_clip);
  if (!preserve_texture_lod) {
    target.SetRoomTextureLodDepth(TextureLodDepthForFarClip(far_clip));
  }
}
}  // namespace VS::Render
