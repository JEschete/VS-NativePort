// Hand-maintained (tools/port/owned.csv).
#include "Field/ActorCollisionLive.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <optional>

#include "Actors/ActorMotionLive.hpp"
#include "Actors/ActorMovementLive.hpp"
#include "Actors/ActorRecord.hpp"
#include "Actors/ModelInstance.hpp"
#include "Core/Log.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Field/ActorCollision.hpp"
#include "Field/Collision.hpp"
#include "Field/CollisionMap.hpp"
#include "Field/CollisionMapLive.hpp"
#include "Field/Gimmicks.hpp"
#include "Field/PlatformPlacement.hpp"
#include "Legacy/Psx.h"

namespace VS::Field {
namespace {
using Actors::LiveMotionVector;
using Actors::LiveSvector;
constexpr std::size_t kActorSlots = std::size(VS::Actors::actorRecords);
static_assert(kActorSlots == kCollisionActorSlots);
constexpr std::uint8_t kThreeBits = 0x7;
constexpr std::uint8_t kFourBits = 0xF;

template <typename Value>
Value& Required(Value* value, const char* what) {
  if (value == nullptr) {
    VS::Core::LogError("field.collision", "Actor collision requires {}", what);
    std::abort();
  }
  return *value;
}

std::int32_t TableIndex(const VS::Actors::ActorRecord* record) {
  for (std::size_t index = 0; index < kActorSlots; ++index) {
    if (VS::Actors::actorRecords[index] == record) {
      return static_cast<std::int32_t>(index);
    }
  }
  return -1;
}

VS::Actors::ActorRecord* TableRecord(std::int32_t index) {
  if (index < 0 || static_cast<std::size_t>(index) >= kActorSlots) {
    return nullptr;
  }
  return VS::Actors::actorRecords[index];
}

CollisionActor LoadRecord(const VS::Actors::ActorRecord& record,
                          std::int32_t table_index) {
  const auto& instance = record.instance;
  return CollisionActor{
      .table_index = table_index,
      .skipped = instance.is_skipped != 0,
      .movement_pending = instance.movement_pending != 0,
      .movement_mode = instance.movement_mode,
      .locomotion_state = instance.locomotion_state,
      .motion_locked = instance.motion_locked != 0,
      .weapon_drawn = instance.weapon_drawn != 0,
      .reaction_state = instance.reaction_state,
      .pose_state = instance.pose_state,
      .support_layer = instance.support_layer,
      .slot = instance.slot,
      .attach_bone = instance.attach_bone,
      .position = LiveMotionVector(instance.position),
      .root_motion = LiveMotionVector(instance.root_motion),
      .tile_x = instance.current_tile_x,
      .tile_layer = instance.current_tile_layer,
      .tile_z = instance.current_tile_z,
      .action_pose_active = record.action_pose_active != 0,
      .bypass_actor_collision = record.bypass_actor_collision != 0,
      .bypass_terrain_collision = record.bypass_terrain_collision != 0,
      .ignored_by_collision = record.ignored_by_collision != 0,
      .wall_contact = record.wall_contact != 0,
      .on_ledge = record.on_ledge != 0,
      .was_on_ledge = record.was_on_ledge != 0,
      .animation_locked = record.animation_locked != 0,
      .support_gimmick = record.support_gimmick,
      .route_target = LiveMotionVector(record.route_target),
      .collision_radius = record.collision_radius,
      .occupancy_radius = record.occupancy_radius,
      .occupancy_height = record.occupancy_height,
      .model_id = record.model_id,
      .ledge_height = record.ledge_height,
      .platform_offset_x = record.platform_offset_x,
      .platform_offset_z = record.platform_offset_z,
      .platform_anchor = LiveMotionVector(record.platform_anchor),
      .terrain_facing = record.terrain_facing,
      .move_facing = record.move_facing,
      .turn_direction = record.turn_state.direction,
      .turn_distance = record.turn_state.distance};
}

// The fields the collision rules change.
void StoreRecord(const CollisionActor& actor, VS::Actors::ActorRecord& record) {
  auto& instance = record.instance;
  instance.movement_pending = actor.movement_pending ? 1 : 0;
  instance.movement_mode = actor.movement_mode & kThreeBits;
  instance.locomotion_state = actor.locomotion_state & kThreeBits;
  instance.support_layer = actor.support_layer & kFourBits;
  instance.position = LiveSvector(actor.position);
  instance.root_motion = LiveSvector(actor.root_motion);
  record.bypass_terrain_collision = actor.bypass_terrain_collision ? 1 : 0;
  record.wall_contact = actor.wall_contact ? 1 : 0;
  record.on_ledge = actor.on_ledge ? 1 : 0;
  record.was_on_ledge = actor.was_on_ledge ? 1 : 0;
  record.platform_offset_x = actor.platform_offset_x;
  record.platform_offset_z = actor.platform_offset_z;
  record.terrain_facing = actor.terrain_facing;
  record.move_facing = actor.move_facing;
}

SupportSample ToSupportSample(int mode) {
  switch (mode) {
    case static_cast<int>(SupportSample::kFloor):
      return SupportSample::kFloor;
    case static_cast<int>(SupportSample::kBelowReference):
      return SupportSample::kBelowReference;
    case static_cast<int>(SupportSample::kShadow):
      return SupportSample::kShadow;
    case static_cast<int>(SupportSample::kNoPlatforms):
      return SupportSample::kNoPlatforms;
    default:
      return SupportSample::kInterpolated;
  }
}

// The room and actor table the bindings run against. The probe position and
// sector are loaded from the generated globals and mirrored back when the
// binding returns, so the movement rules that still read them see each call.
class LiveActorCollisionHost final : public ActorCollisionHost {
 public:
  LiveActorCollisionHost() {
    CollisionProbe& probe = LiveCollisionProbe();
    probe.position = LiveMotionVector(collisionProbePosition);
    probe.direction = collisionProbeDirection;
  }
  LiveActorCollisionHost(const LiveActorCollisionHost&) = delete;
  LiveActorCollisionHost& operator=(const LiveActorCollisionHost&) = delete;
  LiveActorCollisionHost(LiveActorCollisionHost&&) = delete;
  LiveActorCollisionHost& operator=(LiveActorCollisionHost&&) = delete;
  ~LiveActorCollisionHost() override {
    const CollisionProbe& probe = LiveCollisionProbe();
    collisionProbePosition = LiveSvector(probe.position);
    collisionProbeDirection = probe.direction;
  }

  [[nodiscard]] std::optional<CollisionCellRef> CellAt(
      std::int16_t world_x, std::int16_t world_z,
      std::int32_t layer) const override {
    return LiveCollisionCellRefAtPosition(world_x, world_z, layer);
  }
  [[nodiscard]] std::optional<std::uint32_t> CellWord(
      CollisionCellRef cell) const override {
    return LiveCollisionCellWord(cell);
  }
  [[nodiscard]] std::int16_t FloorHeight(std::int32_t world_x,
                                         std::int32_t world_z) const override {
    return LiveFloorHeightAt(world_x, world_z);
  }
  [[nodiscard]] std::int16_t InterpolatedFloorHeight(
      std::int32_t world_x, std::int32_t world_z) const override {
    return LiveInterpolatedFloorHeight(world_x, world_z);
  }
  [[nodiscard]] std::int16_t CeilingHeight(
      std::int32_t world_x, std::int32_t world_z) const override {
    return LiveCeilingHeightAt(world_x, world_z);
  }
  GimmickSupport SupportingGimmick(std::int32_t tile_x,
                                   std::int32_t tile_z) override {
    return LiveGimmickSupport(tile_x, tile_z);
  }
  [[nodiscard]] std::int32_t GimmickSupportBelow(
      std::int32_t tile_x, std::int32_t tile_z,
      std::int32_t reference_height) const override {
    return LiveGimmickSupportHeightBelow(tile_x, tile_z, reference_height);
  }
  PlatformContactHeight PlatformContactAt(PlatformProbe probe) override {
    return LiveMovingPlatformContact(probe, 0);
  }
  [[nodiscard]] std::optional<PlatformContact> VisiblePlatform(
      std::int32_t support_layer) const override {
    return LiveVisiblePlatform(support_layer);
  }
  [[nodiscard]] std::uint8_t TileSurfaceId(
      std::uint32_t surface) const override {
    if (surface >= std::size(tileSurfaceIds)) return 0;
    return tileSurfaceIds[surface];
  }

  [[nodiscard]] std::optional<CollisionActor> Actor(
      std::size_t index) const override {
    const VS::Actors::ActorRecord* record =
        TableRecord(static_cast<std::int32_t>(index));
    if (record == nullptr) return std::nullopt;
    return LoadRecord(*record, static_cast<std::int32_t>(index));
  }
  void StoreActor(const CollisionActor& actor) override {
    VS::Actors::ActorRecord* record = TableRecord(actor.table_index);
    if (record == nullptr) {
      VS::Core::LogWarn("field.collision",
                        "Actor collision store refused slot {}",
                        actor.table_index);
      return;
    }
    StoreRecord(actor, *record);
  }
  [[nodiscard]] std::optional<GimmickTile> SupportGimmickTile(
      std::uint8_t slot) const override {
    if (slot >= std::size(VS::Field::gimmickInstances)) return std::nullopt;
    const VS::Field::GimmickInstance* gimmick =
        VS::Field::gimmickInstances[slot];
    if (gimmick == nullptr) return std::nullopt;
    return GimmickTile{.x = static_cast<std::int8_t>(gimmick->tile_x),
                       .z = static_cast<std::int8_t>(gimmick->tile_z)};
  }
  // Actor movement (row 21).
  void UpdatePlatformContact(std::int32_t actor_index,
                             std::int32_t mode) override {
    VS::Actors::ActorRecord* record = TableRecord(actor_index);
    if (record == nullptr) {
      VS::Core::LogWarn("field.collision",
                        "Platform contact update refused slot {}", actor_index);
      return;
    }
    Actors::UpdateLiveActorPlatformContact(record, mode);
  }

  CollisionSamples& Samples() override { return LiveCollisionSamples(); }
  CollisionProbe& Probe() override { return LiveCollisionProbe(); }
};

// An actor a binding names, loaded with its table slot.
CollisionActor LoadNamed(const VS::Actors::ActorRecord& record) {
  return LoadRecord(record, TableIndex(&record));
}
}  // namespace

CollisionProbe& LiveCollisionProbe() {
  static CollisionProbe probe;
  return probe;
}

CollisionSamples& LiveCollisionSamples() {
  static CollisionSamples samples;
  return samples;
}

std::uint32_t LiveCollisionProbeCellWord() {
  const std::optional<CollisionCellRef> cell = LiveCollisionProbe().cell;
  if (!cell) return kAbsentCellWord;
  return LiveCollisionCellWord(*cell).value_or(kAbsentCellWord);
}

VS::Actors::ActorRecord* LiveFindActorOccupyingTile(int tile_x, int tile_z,
                                                    int minimum_height,
                                                    int mode) {
  const LiveActorCollisionHost host;
  const std::optional<std::size_t> index =
      FindActorOccupyingTile(host, {.tile_x = tile_x,
                                    .tile_z = tile_z,
                                    .minimum_height = minimum_height,
                                    .mode = mode});
  return index ? VS::Actors::actorRecords[*index] : nullptr;
}

int LiveCheckPlatformActorObstruction(int layer, SVECTOR* point) {
  LiveActorCollisionHost host;
  const MotionVector typed =
      LiveMotionVector(Required(point, "a platform point"));
  return CheckPlatformActorObstruction(host, layer, typed) ? 1 : 0;
}

int LiveCanActorHangFromLedge(VS::Actors::ActorRecord* actor, SVECTOR* offset,
                              SVECTOR* motion) {
  LiveActorCollisionHost host;
  VS::Actors::ActorRecord& record = Required(actor, "an actor");
  CollisionActor typed = LoadNamed(record);
  MotionVector typed_offset =
      LiveMotionVector(Required(offset, "a ledge offset"));
  MotionVector typed_motion = LiveMotionVector(Required(motion, "a motion"));
  const bool valid =
      CanActorHangFromLedge(host, typed, typed_offset, typed_motion);
  StoreRecord(typed, record);
  *offset = LiveSvector(typed_offset);
  *motion = LiveSvector(typed_motion);
  return valid ? 1 : 0;
}

int LiveHasActorLedgeSideSupport(VS::Actors::ActorRecord* actor) {
  LiveActorCollisionHost host;
  const CollisionActor typed = LoadNamed(Required(actor, "an actor"));
  return HasActorLedgeSideSupport(host, typed) ? 1 : 0;
}

int LiveSampleActorSupportHeight(const SVECTOR* origin, int offset_x,
                                 int offset_z, int mode) {
  LiveActorCollisionHost host;
  return SampleActorSupportHeight(
      host, LiveMotionVector(Required(origin, "an origin")), offset_x, offset_z,
      ToSupportSample(mode));
}

int LiveSampleSupportHeightAt(int x, int y, int z, int offset_x, int offset_z,
                              int mode) {
  LiveActorCollisionHost host;
  const MotionVector origin{.x = Core::Math::WrapSigned16(x),
                            .y = Core::Math::WrapSigned16(y),
                            .z = Core::Math::WrapSigned16(z)};
  return SampleActorSupportHeight(host, origin, offset_x, offset_z,
                                  ToSupportSample(mode));
}

void LiveSampleActorCollisionPerimeter(VS::Actors::ActorRecord* actor,
                                       int diagonal) {
  LiveActorCollisionHost host;
  VS::Actors::ActorRecord& record = Required(actor, "an actor");
  CollisionActor typed = LoadNamed(record);
  SampleActorCollisionPerimeter(host, typed, diagonal);
  StoreRecord(typed, record);
}

// The motion may be the actor's own root motion, so it is stored last.
void LiveApplyActorWallRepulsion(VS::Actors::ActorRecord* actor,
                                 SVECTOR* motion) {
  VS::Actors::ActorRecord& record = Required(actor, "an actor");
  CollisionActor typed = LoadNamed(record);
  MotionVector typed_motion = LiveMotionVector(Required(motion, "a motion"));
  ApplyActorWallRepulsion(LiveCollisionSamples(), typed, typed_motion);
  StoreRecord(typed, record);
  *motion = LiveSvector(typed_motion);
}

int LiveResolveActorTerrainMotion(VS::Actors::ActorRecord* actor,
                                  SVECTOR* motion, int* result,
                                  int probe_only) {
  LiveActorCollisionHost host;
  VS::Actors::ActorRecord& record = Required(actor, "an actor");
  int& climb = Required(result, "a result");
  CollisionActor typed = LoadNamed(record);
  MotionVector typed_motion = LiveMotionVector(Required(motion, "a motion"));
  const TerrainMotionResult resolved =
      ResolveActorTerrainMotion(host, typed, typed_motion, probe_only != 0);
  StoreRecord(typed, record);
  *motion = LiveSvector(typed_motion);
  if (resolved.climb) climb = *resolved.climb;
  return static_cast<int>(resolved.motion);
}

int LiveCalcActorLedgeOffset(VS::Actors::ActorRecord* actor, SVECTOR* offset) {
  LiveActorCollisionHost host;
  VS::Actors::ActorRecord& record = Required(actor, "an actor");
  CollisionActor typed = LoadNamed(record);
  MotionVector typed_offset =
      LiveMotionVector(Required(offset, "a ledge offset"));
  const bool valid = CalcActorLedgeOffset(host, typed, typed_offset);
  StoreRecord(typed, record);
  *offset = LiveSvector(typed_offset);
  return valid ? 1 : 0;
}
}  // namespace VS::Field
