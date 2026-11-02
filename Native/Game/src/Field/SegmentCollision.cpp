#include "Field/SegmentCollision.hpp"

#include <cstdint>
#include <cstdlib>
#include <optional>
#include <span>
#include <vector>

#include "Core/Math/ScalarMath.hpp"

namespace VS::Field {
namespace {
constexpr unsigned kTraceStepShift = 5;
constexpr int kVolumeHorizontalExtent = 64;
constexpr int kVolumeVerticalExtent = 32;
constexpr unsigned kFloorHeightMask = 0x7FFF;
constexpr unsigned kFloorHeightSign = 0x4000;
constexpr int kFloorHeightPeriod = 0x8000;

std::optional<int> ActorAt(const SegmentSample& point,
                           const SegmentEndpoint& start,
                           const SegmentSample& step, int ignored_actor,
                           const SegmentTraceWorld& world) {
  for (int slot = 0; slot < kSegmentTraceActorSlots; ++slot) {
    if (slot == ignored_actor) continue;
    const auto bounds = world.ActorBounds(slot);
    if (!bounds) continue;
    const auto ahead = (std::int64_t{bounds->x} - start.x) * step.x +
                       (std::int64_t{bounds->z} - start.z) * step.z;
    if (ahead >= 0 &&
        std::abs(std::int64_t{bounds->x} - point.x) < bounds->radius &&
        std::abs(std::int64_t{bounds->y} - point.y) < bounds->half_height &&
        std::abs(std::int64_t{bounds->z} - point.z) < bounds->radius) {
      return slot;
    }
  }
  return std::nullopt;
}

int DecodeFloorHeight(std::uint16_t word) {
  const unsigned bits = word & kFloorHeightMask;
  return bits < kFloorHeightSign ? static_cast<int>(bits)
                                 : static_cast<int>(bits) - kFloorHeightPeriod;
}

std::int16_t Wrap(std::int32_t value) {
  return VS::Core::Math::WrapSigned16(static_cast<std::uint32_t>(value));
}
}  // namespace

// decomp: BATTLE.PRG func_800E6BA0 (src/BATTLE/BATTLE.PRG/7E05C.s:283).
// Saved-register sample and temporary platform output become explicit inputs.
bool IsPointOutsideSolidVolumes(const SegmentSample& point,
                                std::span<const SolidVolume> volumes) {
  for (const auto& volume : volumes) {
    if (std::abs(std::int64_t{volume.x} - point.x) < kVolumeHorizontalExtent &&
        std::abs(std::int64_t{volume.height} + kVolumeVerticalExtent -
                 point.y) < kVolumeVerticalExtent &&
        std::abs(std::int64_t{volume.z} - point.z) < kVolumeHorizontalExtent) {
      return false;
    }
  }
  return true;
}

// decomp: BATTLE.PRG func_800E6C34, return continuation800E6EAC
// (src/BATTLE/BATTLE.PRG/7E05C.s:331-508).
std::optional<SegmentHit> TraceActorAndTerrainCollision(
    const SegmentEndpoint& from, const SegmentEndpoint& to, int ignored_actor,
    SegmentTraceWorld& world) {
  SegmentSample point{from.x, from.y, from.z};
  const SegmentSample step{(to.x - point.x) >> kTraceStepShift,
                           (to.y - point.y) >> kTraceStepShift,
                           (to.z - point.z) >> kTraceStepShift};
  std::vector<SolidVolume> volumes;
  bool collected = false;
  for (int sample = 0; sample < kSegmentTraceSamples; ++sample) {
    point.x += step.x;
    point.y += step.y;
    point.z += step.z;
    const auto actor = ActorAt(point, from, step, ignored_actor, world);
    if (!actor) {
      if (!collected) {
        volumes = world.CollectSolidVolumes();
        collected = true;
      }
      if (IsPointOutsideSolidVolumes(point, volumes) &&
          point.y <
              DecodeFloorHeight(world.FloorHeightWord(point.x, point.z))) {
        continue;
      }
    }
    return SegmentHit{Wrap(point.x), Wrap(point.y), Wrap(point.z), actor};
  }
  return std::nullopt;
}

}  // namespace VS::Field
