#include "Field/SegmentCollision.h"

#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <optional>
#include <vector>

#include "Core/Log.hpp"

extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/ModelRendering.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Core/Math/ScalarMath.h"
#include "Field/Collision.h"
#include "Field/Gimmicks.h"
#include "Field/Mpd.h"
#include "Field/Platforms.h"
#include "Field/Room.h"
}

namespace {
constexpr int kTraceSlots = 16;
constexpr int kTraceSteps = 32;
constexpr unsigned kTraceStepShift = 5;
constexpr unsigned kExcludedKindMask = 0xEA;
constexpr int kGimmickRadius = 63;
constexpr int kGimmickHalfHeight = 64;
constexpr int kVolumeHorizontalExtent = 64;
constexpr int kVolumeVerticalExtent = 32;
constexpr unsigned kFloorHeightMask = 0x7FFF;
constexpr unsigned kFloorHeightSign = 0x4000;
constexpr int kFloorHeightPeriod = 0x8000;
constexpr std::size_t kSourcePlatformBytes = 184;
static_assert(sizeof(Field_MpdMovingPlatform) == kSourcePlatformBytes);

[[noreturn]] void InvalidTrace(const char* reason) {
  VS_LOG_ERROR("field.collision", "segment trace: {}", reason);
  std::abort();
}

std::vector<Field_PlatformContactPoint> CollectTraceVolumes() {
  const auto bytes = Field_roomData.header.movingPlatformsLength;
  if (Field_roomData.movingPlatforms == nullptr) {
    if (bytes != 0) InvalidTrace("platform section is missing");
    return {};
  }
  if (bytes % sizeof(Field_MpdMovingPlatform) != 0) {
    InvalidTrace("platform section contains a partial record");
  }
  const auto count = bytes / sizeof(Field_MpdMovingPlatform);
  if (count > std::size(Field_platformBounceFrames)) {
    InvalidTrace("platform count exceeds native animation storage");
  }
  std::vector<Field_PlatformContactPoint> volumes(count);
  const int collected = Field_CollectMovingPlatformPositions(volumes.data());
  if (collected < 0 || static_cast<std::size_t>(collected) > volumes.size()) {
    InvalidTrace("platform collector exceeded its native result capacity");
  }
  volumes.resize(static_cast<std::size_t>(collected));
  return volumes;
}

struct ActorTraceBounds {
  int x;
  int y;
  int z;
  int radius;
  int half_height;
};

std::optional<ActorTraceBounds> TraceBounds(int id, int ignored_actor) {
  const auto* actor = Battle_actors[id];
  if (actor == nullptr || id == ignored_actor ||
      (static_cast<unsigned char>(actor->kind) & kExcludedKindMask) != 0) {
    return std::nullopt;
  }
  if (const auto* object = Field_gimmickInstances[id]; object != nullptr) {
    if (object->parent == 0 || static_cast<unsigned char>(object->mode) != 0) {
      return std::nullopt;
    }
    return ActorTraceBounds{object->positionX,
                            object->positionY - kGimmickHalfHeight,
                            static_cast<std::uint16_t>(object->positionZ),
                            kGimmickRadius, kGimmickHalfHeight};
  }
  const auto* model = Actors_actorRecords[id];
  if (model == nullptr || model->animationLocked ||
      model->instance.reactionState == 1 ||
      model->instance.reactionState == 2) {
    return std::nullopt;
  }
  const int half_height = model->collisionHeight / 2;
  return ActorTraceBounds{
      model->instance.position.vx, model->instance.position.vy - half_height,
      static_cast<std::uint16_t>(model->instance.position.vz),
      model->collisionRadius, half_height};
}

int TraceActorAt(const VECTOR& point, const SVECTOR& start, const VECTOR& step,
                 int ignored_actor) {
  for (int id = 0; id < kTraceSlots; ++id) {
    const auto bounds = TraceBounds(id, ignored_actor);
    if (!bounds) continue;
    const auto ahead = (std::int64_t{bounds->x} - start.vx) * step.vx +
                       (std::int64_t{bounds->z} - start.vz) * step.vz;
    if (ahead >= 0 &&
        std::abs(std::int64_t{bounds->x} - point.vx) < bounds->radius &&
        std::abs(std::int64_t{bounds->y} - point.vy) < bounds->half_height &&
        std::abs(std::int64_t{bounds->z} - point.vz) < bounds->radius) {
      return id + 1;
    }
  }
  return 0;
}

int TraceFloorHeight(const VECTOR& point) {
  const auto bits =
      static_cast<std::uint16_t>(Field_FloorHeightAt(point.vx, point.vz)) &
      kFloorHeightMask;
  return bits < kFloorHeightSign ? static_cast<int>(bits)
                                 : static_cast<int>(bits) - kFloorHeightPeriod;
}
}  // namespace

// decomp: BATTLE.PRG func_800E6BA0 (src/BATTLE/BATTLE.PRG/7E05C.s:283).
// Saved-register sample and temporary platform output become explicit inputs.
extern "C" int Field_IsPointOutsideSolidVolumes(
    const VECTOR* point, const Field_PlatformContactPoint* volumes,
    std::size_t count) {
  if (point == nullptr || (count != 0 && volumes == nullptr)) {
    InvalidTrace("solid-volume predicate requires a point and valid span");
  }
  for (std::size_t index = 0; index < count; ++index) {
    const auto& volume = volumes[index];
    if (std::abs(std::int64_t{static_cast<std::uint16_t>(volume.x)} -
                 point->vx) < kVolumeHorizontalExtent &&
        std::abs(std::int64_t{volume.height} + kVolumeVerticalExtent -
                 point->vy) < kVolumeVerticalExtent &&
        std::abs(std::int64_t{volume.z} - point->vz) <
            kVolumeHorizontalExtent) {
      return 0;
    }
  }
  return 1;
}

// decomp: BATTLE.PRG func_800E6C34, return continuation800E6EAC
// (src/BATTLE/BATTLE.PRG/7E05C.s:331-508).
extern "C" SVECTOR* Field_TraceActorAndTerrainCollision(const SVECTOR* from,
                                                        const SVECTOR* to,
                                                        SVECTOR* output,
                                                        int ignored_actor) {
  if (from == nullptr || to == nullptr || output == nullptr) {
    InvalidTrace("segment endpoints and output are required");
  }
  const SVECTOR start = *from;
  VECTOR point{from->vx, from->vy, from->vz, 0};
  const VECTOR step{(to->vx - point.vx) >> kTraceStepShift,
                    (to->vy - point.vy) >> kTraceStepShift,
                    (to->vz - point.vz) >> kTraceStepShift, 0};
  std::vector<Field_PlatformContactPoint> volumes;
  bool have_volumes = false;
  for (int sample = 0; sample < kTraceSteps; ++sample) {
    point.vx += step.vx;
    point.vy += step.vy;
    point.vz += step.vz;
    const int hit = TraceActorAt(point, start, step, ignored_actor);
    if (hit == 0) {
      if (!have_volumes) {
        volumes = CollectTraceVolumes();
        have_volumes = true;
      }
      if (Field_IsPointOutsideSolidVolumes(&point, volumes.data(),
                                           volumes.size()) &&
          point.vy < TraceFloorHeight(point)) {
        continue;
      }
    }
    output->vx = Core_Math_WrapSigned16(static_cast<std::uint32_t>(point.vx));
    output->vy = Core_Math_WrapSigned16(static_cast<std::uint32_t>(point.vy));
    output->vz = Core_Math_WrapSigned16(static_cast<std::uint32_t>(point.vz));
    output->pad = static_cast<std::int16_t>(hit);
    return output;
  }
  return nullptr;
}
