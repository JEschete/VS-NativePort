#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace VS::Field {

inline constexpr int kSegmentTraceActorSlots = 16;
inline constexpr int kSegmentTraceSamples = 32;

// Full-word sample position; the source keeps the walking point in saved
// registers rather than a halfword vector.
struct SegmentSample {
  std::int32_t x;
  std::int32_t y;
  std::int32_t z;
};

struct SegmentEndpoint {
  std::int16_t x;
  std::int16_t y;
  std::int16_t z;
};

// Source platform contact widths: X is read as an unsigned halfword while
// height and Z stay signed.
struct SolidVolume {
  std::uint16_t x;
  std::int16_t height;
  std::int16_t z;
};

// Eligible actor or Gimmick bounds; y is the vertical centre.
struct ActorTraceBounds {
  std::int32_t x;
  std::int32_t y;
  std::int32_t z;
  std::int32_t radius;
  std::int32_t half_height;
};

struct SegmentHit {
  std::int16_t x;
  std::int16_t y;
  std::int16_t z;
  // Empty for solid-volume and terrain hits.
  std::optional<int> actor_slot;
};

class SegmentTraceWorld {
 public:
  SegmentTraceWorld() = default;
  SegmentTraceWorld(const SegmentTraceWorld&) = delete;
  SegmentTraceWorld& operator=(const SegmentTraceWorld&) = delete;
  SegmentTraceWorld(SegmentTraceWorld&&) = delete;
  SegmentTraceWorld& operator=(SegmentTraceWorld&&) = delete;
  virtual ~SegmentTraceWorld() = default;

  // Bounds for an occupied, eligible slot; the trace applies the ignored
  // actor itself.
  [[nodiscard]] virtual std::optional<ActorTraceBounds> ActorBounds(
      int slot) const = 0;
  // Called at most once per trace, on the first sample without an actor hit.
  [[nodiscard]] virtual std::vector<SolidVolume> CollectSolidVolumes() = 0;
  // Raw source floor word; the trace decodes its signed 15-bit height.
  [[nodiscard]] virtual std::uint16_t FloorHeightWord(std::int32_t x,
                                                      std::int32_t z) const = 0;
};

[[nodiscard]] bool IsPointOutsideSolidVolumes(
    const SegmentSample& point, std::span<const SolidVolume> volumes);

[[nodiscard]] std::optional<SegmentHit> TraceActorAndTerrainCollision(
    const SegmentEndpoint& from, const SegmentEndpoint& to, int ignored_actor,
    SegmentTraceWorld& world);

}  // namespace VS::Field
