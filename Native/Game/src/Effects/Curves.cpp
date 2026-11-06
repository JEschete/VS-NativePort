// Hand-maintained (tools/port/owned.csv).
#include "Effects/Curves.hpp"  // NOLINT(build/include): typed API

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Core/Math/ScalarMath.hpp"

namespace VS::Effects {
namespace {
namespace Math = VS::Core::Math;
// pFileBlock11 (5BF94.c:248): {s32 count; s16 offsets[count]}.
constexpr std::size_t kSegmentCountBytes = 4;
constexpr std::size_t kSegmentOffsetBytes = 2;
// A segment is {u8 step, u8 value}; the next segment's value follows it.
constexpr std::int32_t kSegmentBytes = 2;
constexpr std::int32_t kNextValue = 3;
constexpr int kByteBits = 8;

[[nodiscard]] constexpr CurveSample Refused(CurveSampleStatus status) noexcept {
  return {.value = 0, .status = status};
}

[[nodiscard]] constexpr CurveSample Sampled(std::int32_t value) noexcept {
  return {.value = value, .status = CurveSampleStatus::kSampled};
}

[[nodiscard]] std::optional<std::uint8_t> ByteAt(
    std::span<const std::uint8_t> table, std::int64_t offset) noexcept {
  if (offset < 0 || static_cast<std::uint64_t>(offset) >= table.size()) {
    return std::nullopt;
  }
  return table[static_cast<std::size_t>(offset)];
}

[[nodiscard]] std::int32_t SegmentCount(
    std::span<const std::uint8_t> table) noexcept {
  std::uint32_t count = 0;
  for (std::size_t i = 0; i < kSegmentCountBytes; ++i) {
    count |= static_cast<std::uint32_t>(table[i]) << (i * kByteBits);
  }
  return static_cast<std::int32_t>(count);
}

// The offset of curve `curve_index`'s first segment: its table offset (an s16)
// plus the curve's header byte, or nullopt when either leaves the block.
[[nodiscard]] std::optional<std::int64_t> FirstSegment(
    std::span<const std::uint8_t> table, std::int32_t curve_index) noexcept {
  const std::int64_t entry = static_cast<std::int64_t>(kSegmentCountBytes) +
                             (static_cast<std::int64_t>(curve_index) *
                              static_cast<std::int64_t>(kSegmentOffsetBytes));
  const auto low = ByteAt(table, entry);
  const auto high = ByteAt(table, entry + 1);
  if (!low || !high) {
    return std::nullopt;
  }
  const auto offset = static_cast<std::int16_t>(
      static_cast<std::uint16_t>(*low | (*high << kByteBits)));
  const auto header = ByteAt(table, offset);
  if (!header) {
    return std::nullopt;
  }
  return static_cast<std::int64_t>(offset) + *header;
}

// The checks both segmented queries share before walking a curve.
[[nodiscard]] std::optional<CurveSample> SegmentedPrologue(
    std::span<const std::uint8_t> table, std::int32_t curve_index) noexcept {
  if (table.size() < kSegmentCountBytes) {
    return Refused(CurveSampleStatus::kNoBlock);
  }
  if (curve_index >= SegmentCount(table)) {
    return Refused(CurveSampleStatus::kNoCurve);
  }
  if (curve_index < 0) {
    return Refused(CurveSampleStatus::kNegativeCurve);
  }
  return std::nullopt;
}
}  // namespace

// decomp: BATTLE.PRG vs_battle_sampleCurve (src/BATTLE/BATTLE.PRG/5BF94.c:6324)
CurveSample SampleEffectCurve(const EffectCurveBlock& block,
                              std::int32_t curve_id,
                              std::int32_t distance) noexcept {
  if (curve_id == 0) {
    return Refused(CurveSampleStatus::kNoCurve);
  }
  if (curve_id < 0) {
    return Refused(CurveSampleStatus::kNegativeCurve);
  }
  if (static_cast<std::uint32_t>(curve_id) > block.header_count) {
    return Refused(CurveSampleStatus::kNoCurve);
  }
  const auto index = static_cast<std::size_t>(curve_id) - 1;
  if (index >= block.curves.size() || block.curves[index].data() == nullptr) {
    return Refused(CurveSampleStatus::kUnboundCurve);
  }
  const std::span<const std::uint8_t> curve = block.curves[index];
  if (curve.empty()) {
    return Refused(CurveSampleStatus::kEmptyCurve);
  }
  const std::int32_t sample =
      distance % static_cast<std::int32_t>(curve.size());
  if (sample < 0) {
    return Refused(CurveSampleStatus::kNegativeDistance);
  }
  return Sampled(curve[static_cast<std::size_t>(sample)]);
}

// decomp: BATTLE.PRG func_800D1104 (src/BATTLE/BATTLE.PRG/5BF94.c:6314)
CurveSweep SampleAllEffectCurves(const EffectCurveBlock& block,
                                 std::int32_t distance,
                                 std::span<std::int32_t> samples) noexcept {
  CurveSweep sweep;
  for (std::uint32_t i = 0; i < block.header_count; ++i) {
    const std::size_t slot = static_cast<std::size_t>(i) + 1;
    if (slot >= samples.size()) {
      sweep.refused += static_cast<std::int32_t>(block.header_count - i);
      break;
    }
    const CurveSample sample =
        SampleEffectCurve(block, static_cast<std::int32_t>(slot), distance);
    if (sample.status != CurveSampleStatus::kSampled) {
      ++sweep.refused;
      continue;
    }
    samples[slot] = sample.value;
    ++sweep.written;
  }
  return sweep;
}

// decomp: BATTLE.PRG func_800D11F4 (src/BATTLE/BATTLE.PRG/5BF94.c:6335)
CurveSample SampleSegmentedEffectCurve(std::span<const std::uint8_t> table,
                                       std::int32_t curve_index,
                                       std::int32_t distance) noexcept {
  if (const auto refused = SegmentedPrologue(table, curve_index)) {
    return *refused;
  }
  const auto first = FirstSegment(table, curve_index);
  if (!first) {
    return Refused(CurveSampleStatus::kLeavesBlock);
  }
  std::int64_t offset = *first;
  auto step = ByteAt(table, offset);
  while (step && *step != 0) {
    const std::int32_t remaining = Math::WrappingSubtract(distance, *step);
    if (remaining < 0) {
      break;
    }
    offset += kSegmentBytes;
    distance = remaining;
    step = ByteAt(table, offset);
  }
  const auto value = ByteAt(table, offset + 1);
  if (!step || !value) {
    return Refused(CurveSampleStatus::kLeavesBlock);
  }
  if (*step != 0) {
    const auto next = ByteAt(table, offset + kNextValue);
    if (!next) {
      return Refused(CurveSampleStatus::kLeavesBlock);
    }
    // The R3000 MULT keeps the low word of the product (5BF94.c:6352).
    const auto span = static_cast<std::int64_t>(*next) - *value;
    const auto product =
        static_cast<std::int32_t>(static_cast<std::uint32_t>(span * distance));
    return Sampled((product / *step) + *value);
  }
  return Sampled(distance == 0 ? *value : -1);
}

// decomp: BATTLE.PRG func_800D12D8 (src/BATTLE/BATTLE.PRG/5BF94.c:6365)
CurveSample SegmentedEffectCurveDuration(std::span<const std::uint8_t> table,
                                         std::int32_t curve_index) noexcept {
  if (const auto refused = SegmentedPrologue(table, curve_index)) {
    return *refused;
  }
  const auto first = FirstSegment(table, curve_index);
  if (!first) {
    return Refused(CurveSampleStatus::kLeavesBlock);
  }
  std::int64_t offset = *first;
  std::int32_t frames = 0;
  auto step = ByteAt(table, offset);
  while (step && *step != 0) {
    frames += *step;
    offset += kSegmentBytes;
    step = ByteAt(table, offset);
  }
  if (!step) {
    return Refused(CurveSampleStatus::kLeavesBlock);
  }
  return Sampled(frames + 1);
}
}  // namespace VS::Effects
