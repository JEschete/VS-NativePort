#include "Field/SegmentCollisionLive.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <optional>
#include <span>
#include <vector>

#include "Core/Log.hpp"
#include "Field/CollisionMapLive.hpp"
#include "Field/SegmentCollision.hpp"
extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/ModelRenderingLiveState.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Field/Collision.h"
#include "Field/Gimmicks.h"
#include "Field/Mpd.h"
#include "Field/Platforms.h"
#include "Field/Room.h"
}

namespace VS::Field {
namespace {
constexpr unsigned kExcludedKindMask = 0xEA;
constexpr int kGimmickRadius = 63;
constexpr int kGimmickHalfHeight = 64;
constexpr std::size_t kSourcePlatformBytes = 184;
static_assert(sizeof(Field_MpdMovingPlatform) == kSourcePlatformBytes);

[[noreturn]] void InvalidTrace(const char* reason) {
  VS_LOG_ERROR("field.collision", "segment trace: {}", reason);
  std::abort();
}

SolidVolume ToSolidVolume(const Field_PlatformContactPoint& contact) {
  return {static_cast<std::uint16_t>(contact.x), contact.height, contact.z};
}

class LiveSegmentTraceWorld final : public SegmentTraceWorld {
 public:
  [[nodiscard]] std::optional<ActorTraceBounds> ActorBounds(
      int slot) const override {
    const auto* actor = Battle_actors[slot];
    if (actor == nullptr ||
        (static_cast<unsigned char>(actor->kind) & kExcludedKindMask) != 0) {
      return std::nullopt;
    }
    if (const auto* object = Field_gimmickInstances[slot]; object != nullptr) {
      if (object->parent == 0 ||
          static_cast<unsigned char>(object->mode) != 0) {
        return std::nullopt;
      }
      return ActorTraceBounds{object->positionX,
                              object->positionY - kGimmickHalfHeight,
                              static_cast<std::uint16_t>(object->positionZ),
                              kGimmickRadius, kGimmickHalfHeight};
    }
    const auto* model = Actors_actorRecords[slot];
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

  [[nodiscard]] std::vector<SolidVolume> CollectSolidVolumes() override {
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
    std::vector<Field_PlatformContactPoint> contacts(count);
    const int collected = Field_CollectMovingPlatformPositions(contacts.data());
    if (collected < 0 ||
        static_cast<std::size_t>(collected) > contacts.size()) {
      InvalidTrace("platform collector exceeded its native result capacity");
    }
    std::vector<SolidVolume> volumes;
    volumes.reserve(static_cast<std::size_t>(collected));
    for (const auto& contact :
         std::span(contacts).first(static_cast<std::size_t>(collected))) {
      volumes.push_back(ToSolidVolume(contact));
    }
    return volumes;
  }

  [[nodiscard]] std::uint16_t FloorHeightWord(std::int32_t x,
                                              std::int32_t z) const override {
    return static_cast<std::uint16_t>(VS::Field::LiveFloorHeightAt(x, z));
  }
};
}  // namespace

int IsLivePointOutsideSolidVolumes(const VECTOR* point,
                                   const Field_PlatformContactPoint* volumes,
                                   std::size_t count) {
  if (point == nullptr || (count != 0 && volumes == nullptr)) {
    InvalidTrace("solid-volume predicate requires a point and valid span");
  }
  std::vector<SolidVolume> typed;
  typed.reserve(count);
  if (count != 0) {
    for (const auto& contact : std::span(volumes, count)) {
      typed.push_back(ToSolidVolume(contact));
    }
  }
  return IsPointOutsideSolidVolumes({point->vx, point->vy, point->vz}, typed)
             ? 1
             : 0;
}

SVECTOR* TraceLiveActorAndTerrainCollision(const SVECTOR* from,
                                           const SVECTOR* to, SVECTOR* output,
                                           int ignored_actor) {
  if (from == nullptr || to == nullptr || output == nullptr) {
    InvalidTrace("segment endpoints and output are required");
  }
  LiveSegmentTraceWorld world;
  const auto hit = TraceActorAndTerrainCollision({from->vx, from->vy, from->vz},
                                                 {to->vx, to->vy, to->vz},
                                                 ignored_actor, world);
  if (!hit) return nullptr;
  output->vx = hit->x;
  output->vy = hit->y;
  output->vz = hit->z;
  output->pad = static_cast<std::int16_t>(
      hit->actor_slot.has_value() ? *hit->actor_slot + 1 : 0);
  return output;
}

}  // namespace VS::Field
