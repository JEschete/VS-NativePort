// Hand-maintained (tools/port/owned.csv).
#include "Effects/CurvesLive.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <span>
#include <string_view>
#include <type_traits>

#include "Core/Log.hpp"
#include "Effects/Curves.hpp"
#include "Effects/EffectResources.hpp"
#include "Effects/EffectResourcesLive.hpp"
#include "Effects/Package.hpp"
#include "Effects/PackageExtents.hpp"
#include "Effects/Runtime.hpp"
#include "Legacy/Psx.hpp"

namespace VS::Effects {
namespace {
using PackageCurves = decltype(LiveEffectPackage::curves);
static_assert(std::tuple_size_v<PackageCurves> == kEffectCurveCapacity);

std::uint64_t refusedSamples = 0;
std::uint64_t refusedSegments = 0;
std::uint64_t refusedSweeps = 0;

// True on the 1st, 2nd, 4th, 8th... occurrence, to keep repeated
// diagnostics readable.
[[nodiscard]] bool ShouldReport(std::uint64_t& count) {
  return std::has_single_bit(++count);
}

// The current package's curves, each within the extents recorded when its
// curve block was bound; none without a package or a curve block.
struct LiveCurveBlock {
  std::array<std::span<const std::uint8_t>, kEffectCurveCapacity> curves{};
  EffectCurveBlock block;
};

[[nodiscard]] LiveCurveBlock CurrentCurveBlock() {
  LiveCurveBlock live;
  if (VS::Effects::package == nullptr ||
      VS::Effects::package->curve_block == nullptr) {
    return live;
  }
  const LiveCurveTable& header = *VS::Effects::package->curve_block;
  const std::size_t bound =
      std::min(LiveEffectCurveCount(), live.curves.size());
  for (std::size_t i = 0; i < bound; ++i) {
    // A curve the binder refused stays unbound (no data).
    if (VS::Effects::package->curves[i] != nullptr) {
      live.curves.at(i) = {VS::Effects::package->curves[i], header.Size(i)};
    }
  }
  live.block = {.header_count = header.count,
                .curves = std::span(live.curves).first(bound)};
  return live;
}

// The bound segmented curve table (block 11) as bytes.
[[nodiscard]] std::span<const std::uint8_t> SegmentedTable() {
  const std::span<const std::byte> table =
      LiveBoundPackageBlock(PackageBlockType::kSoundSequences);
  return {reinterpret_cast<const std::uint8_t*>(table.data()), table.size()};
}

void ReportRefusal(std::string_view query, std::int32_t curve,
                   std::int32_t distance, CurveSampleStatus status,
                   std::uint64_t& count) {
  if (ShouldReport(count)) {
    VS::Core::LogWarn(
        "effects",
        "{}: curve {} at {} refused (status {}); sampled as 0 ({} "
        "refusals)",
        query, curve, distance, static_cast<int>(status), count);
  }
}
}  // namespace

std::span<std::int32_t> LiveEffectCurveSamples() {
  static_assert(sizeof(curveSamples[0]) == sizeof(std::int32_t));
  return {reinterpret_cast<std::int32_t*>(curveSamples),
          std::size(curveSamples)};
}

CurveSample SampleLiveEffectCurve(std::int32_t curve_id,
                                  std::int32_t distance) {
  if (VS::Effects::package == nullptr ||
      VS::Effects::package->curve_block == nullptr) {
    return {.value = 0, .status = CurveSampleStatus::kNoBlock};
  }
  const LiveCurveBlock live = CurrentCurveBlock();
  return SampleEffectCurve(live.block, curve_id, distance);
}

CurveSample SampleLiveSegmentedEffectCurve(std::int32_t curve_index,
                                           std::int32_t distance) {
  return SampleSegmentedEffectCurve(SegmentedTable(), curve_index, distance);
}

int LiveSampleEffectCurve(int curve_id, int distance) {
  const CurveSample sample = SampleLiveEffectCurve(curve_id, distance);
  if (IsCurveRefusal(sample.status)) {
    ReportRefusal("sample curve", curve_id, distance, sample.status,
                  refusedSamples);
  }
  return sample.value;
}

void LiveSampleAllEffectCurves(int distance) {
  if (VS::Effects::package == nullptr ||
      VS::Effects::package->curve_block == nullptr) {
    ReportRefusal("sample all curves", 0, distance, CurveSampleStatus::kNoBlock,
                  refusedSweeps);
    return;
  }
  const LiveCurveBlock live = CurrentCurveBlock();
  const CurveSweep sweep =
      SampleAllEffectCurves(live.block, distance, LiveEffectCurveSamples());
  if (sweep.refused != 0 && ShouldReport(refusedSweeps)) {
    VS::Core::LogWarn(
        "effects",
        "sample all curves: {} of {} curves at {} refused and kept "
        "their previous samples ({} sweeps)",
        sweep.refused, live.block.header_count, distance, refusedSweeps);
  }
}

int LiveSampleSegmentedEffectCurve(int curve_index, int distance) {
  const CurveSample sample =
      SampleLiveSegmentedEffectCurve(curve_index, distance);
  if (IsCurveRefusal(sample.status)) {
    ReportRefusal("sample segmented curve", curve_index, distance,
                  sample.status, refusedSegments);
  }
  return sample.value;
}

int LiveSegmentedEffectCurveDuration(int curve_index) {
  const CurveSample duration =
      SegmentedEffectCurveDuration(SegmentedTable(), curve_index);
  if (IsCurveRefusal(duration.status)) {
    ReportRefusal("segmented curve duration", curve_index, 0, duration.status,
                  refusedSegments);
  }
  return duration.value;
}
}  // namespace VS::Effects
