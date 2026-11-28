#include "Field/SegmentCollisionLive.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "Actors/ActorRecord.hpp"
#include "Actors/ActorSpawningLive.hpp"
#include "Actors/ModelRenderingLiveState.hpp"
#include "Battle/Actor.hpp"
#include "Battle/Damage.hpp"
#include "Core/Log.hpp"
#include "Field/CollisionMap.hpp"
#include "Field/Gimmicks.hpp"
#include "Field/Mpd.hpp"
#include "Field/PlatformPlacementLive.hpp"
#include "Field/Room.hpp"
#include "Field/SegmentCollision.hpp"

using VS::Core::Gte::SVECTOR;
using VS::Core::Gte::VECTOR;

namespace VS::Field {
namespace {
constexpr unsigned kExcludedKindMask = 0xEA;
constexpr int kGimmickRadius = 63;
constexpr int kGimmickHalfHeight = 64;
[[noreturn]] void InvalidTrace(const char* reason) {
  VS::Core::LogError("field.collision", "segment trace: {}", reason);
  std::abort();
}

SolidVolume ToSolidVolume(const PlatformContact& contact) {
  return {static_cast<std::uint16_t>(contact.x), contact.height, contact.z};
}

std::optional<ActorTraceBounds> BoundsOf(const Battle::Actor* actor,
                                         const GimmickInstance* object,
                                         const Actors::ActorRecord* model) {
  if (actor == nullptr ||
      (static_cast<unsigned char>(actor->kind) & kExcludedKindMask) != 0) {
    return std::nullopt;
  }
  if (object != nullptr) {
    if (object->parent == 0 || static_cast<unsigned char>(object->mode) != 0) {
      return std::nullopt;
    }
    return ActorTraceBounds{object->position_x,
                            object->position_y - kGimmickHalfHeight,
                            static_cast<std::uint16_t>(object->position_z),
                            kGimmickRadius, kGimmickHalfHeight};
  }
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

SegmentColliderInput CaptureCollider(std::size_t slot, bool owned) {
  if (!owned) {
    return {.bounds = BoundsOf(Battle::actors[slot], gimmickInstances[slot],
                               Actors::actorRecords[slot])};
  }
  const auto identity = Actors::LiveActorIdentityAt(slot);
  if (identity == Actors::SpawnedActor::kNone) {
    if (Battle::actors[slot] != nullptr) {
      return {.bounds =
                  std::unexpected("combat collider is not a published actor")};
    }
    return {};
  }
  const auto* actor = Actors::ResolveLiveActorInstance(identity);
  const auto components = Actors::LiveActorComponents(identity);
  if (actor == nullptr || !components) {
    return {.identity = identity,
            .bounds = std::unexpected(
                "combat collider lost its expected components")};
  }
  return {.identity = identity,
          .bounds = BoundsOf(actor, components->gimmick, components->body)};
}

SegmentCollisionInputs CaptureInputs(bool owned) {
  SegmentCollisionInputs inputs;
  for (std::size_t slot = 0; slot < inputs.actors.size(); ++slot) {
    inputs.actors[slot] = CaptureCollider(slot, owned);
  }
  inputs.platforms = CaptureLiveMovingPlatformQueryInputs();
  if (roomData.collision_map == nullptr &&
      roomData.header.collision_map_length != 0) {
    inputs.terrain = std::unexpected("collision height section is missing");
  } else {
    inputs.terrain = CollisionHeightMap::ImportLegacy(LiveRoomCollisionMap());
  }
  return inputs;
}
}  // namespace

SegmentCollisionContext::SegmentCollisionContext(SegmentCollisionInputs inputs)
    : inputs_(std::move(inputs)) {}

Actors::SpawnedActor SegmentCollisionContext::ActorIdentity(int slot) const {
  return inputs_.actors.at(static_cast<std::size_t>(slot)).identity;
}

std::optional<ActorTraceBounds> SegmentCollisionContext::ActorBounds(
    int slot) const {
  const auto& bounds = inputs_.actors.at(static_cast<std::size_t>(slot)).bounds;
  if (!bounds) InvalidTrace(bounds.error().c_str());
  return *bounds;
}

std::vector<SolidVolume> SegmentCollisionContext::CollectSolidVolumes() {
  const auto& platforms = inputs_.platforms;
  if (!platforms) InvalidTrace(platforms.error().c_str());
  for (const auto& platform : platforms->platforms) {
    if (!platform.hidden &&
        (platform.bounce_frame < 0 ||
         std::cmp_greater(platform.bounce_frame,
                          platforms->bounce_offsets.size()))) {
      InvalidTrace("platform bounce frame is outside its offsets");
    }
  }
  const auto contacts = CollectMovingPlatformPositions(platforms->View());
  std::vector<SolidVolume> volumes;
  volumes.reserve(contacts.size());
  for (const auto& contact : contacts) {
    volumes.push_back(ToSolidVolume(contact));
  }
  return volumes;
}

std::uint16_t SegmentCollisionContext::FloorHeightWord(std::int32_t x,
                                                       std::int32_t z) const {
  const auto& terrain = inputs_.terrain;
  if (!terrain) InvalidTrace(terrain.error().c_str());
  return static_cast<std::uint16_t>(FloorHeightAt(*terrain, x, z));
}

SegmentCollisionContext CaptureLiveSegmentCollisionContext() {
  return SegmentCollisionContext(CaptureInputs(true));
}

std::optional<SegmentHit> TraceLiveOwnedActorAndTerrainCollision(
    SegmentEndpoint from, SegmentEndpoint to, int ignored_actor) {
  auto world = CaptureLiveSegmentCollisionContext();
  return TraceActorAndTerrainCollision(from, to, ignored_actor, world);
}

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
  SegmentCollisionContext world(CaptureInputs(false));
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
