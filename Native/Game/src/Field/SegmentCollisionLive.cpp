#include "Field/SegmentCollisionLive.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <optional>
#include <span>
#include <vector>

#include "Actors/ActorRecord.hpp"
#include "Actors/ModelRenderingLiveState.hpp"
#include "Battle/Actor.hpp"
#include "Battle/Damage.hpp"
#include "Core/Log.hpp"
#include "Field/CollisionMapLive.hpp"
#include "Field/Gimmicks.hpp"
#include "Field/Mpd.hpp"
#include "Field/PlatformPlacementLive.hpp"
#include "Field/Room.hpp"
#include "Field/SegmentCollision.hpp"

namespace VS::Field {
namespace {
constexpr unsigned kExcludedKindMask = 0xEA;
constexpr int kGimmickRadius = 63;
constexpr int kGimmickHalfHeight = 64;
constexpr std::size_t kSourcePlatformBytes = 184;
static_assert(sizeof(VS::Field::MpdMovingPlatform) == kSourcePlatformBytes);

[[noreturn]] void InvalidTrace(const char* reason) {
  VS::Core::LogError("field.collision", "segment trace: {}", reason);
  std::abort();
}

SolidVolume ToSolidVolume(const PlatformContact& contact) {
  return {static_cast<std::uint16_t>(contact.x), contact.height, contact.z};
}

class LiveSegmentTraceWorld final : public SegmentTraceWorld {
 public:
  [[nodiscard]] std::optional<ActorTraceBounds> ActorBounds(
      int slot) const override {
    const auto* actor = VS::Battle::actors[slot];
    if (actor == nullptr ||
        (static_cast<unsigned char>(actor->kind) & kExcludedKindMask) != 0) {
      return std::nullopt;
    }
    if (const auto* object = VS::Field::gimmickInstances[slot];
        object != nullptr) {
      if (object->parent == 0 ||
          static_cast<unsigned char>(object->mode) != 0) {
        return std::nullopt;
      }
      return ActorTraceBounds{object->position_x,
                              object->position_y - kGimmickHalfHeight,
                              static_cast<std::uint16_t>(object->position_z),
                              kGimmickRadius, kGimmickHalfHeight};
    }
    const auto* model = VS::Actors::actorRecords[slot];
    if (model == nullptr || model->animation_locked ||
        model->instance.reaction_state == 1 ||
        model->instance.reaction_state == 2) {
      return std::nullopt;
    }
    const int half_height = model->collision_height / 2;
    return ActorTraceBounds{
        model->instance.position.vx, model->instance.position.vy - half_height,
        static_cast<std::uint16_t>(model->instance.position.vz),
        model->collision_radius, half_height};
  }

  [[nodiscard]] std::vector<SolidVolume> CollectSolidVolumes() override {
    const auto bytes = VS::Field::roomData.header.moving_platforms_length;
    if (VS::Field::roomData.moving_platforms == nullptr) {
      if (bytes != 0) InvalidTrace("platform section is missing");
      return {};
    }
    if (bytes % sizeof(VS::Field::MpdMovingPlatform) != 0) {
      InvalidTrace("platform section contains a partial record");
    }
    const auto count = bytes / sizeof(VS::Field::MpdMovingPlatform);
    if (count > std::size(VS::Field::platformBounceFrames)) {
      InvalidTrace("platform count exceeds native animation storage");
    }
    const auto contacts = CollectLiveMovingPlatformPositions();
    if (contacts.size() > count) {
      InvalidTrace("platform collector exceeded its native result capacity");
    }
    std::vector<SolidVolume> volumes;
    volumes.reserve(contacts.size());
    for (const auto& contact : contacts) {
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

// The original entry reads 8-byte SVECTOR contacts.
static_assert(sizeof(PlatformContact) == sizeof(SVECTOR));
int IsLivePointOutsideSolidVolumes(const VECTOR* point,
                                   const PlatformContact* volumes,
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
