// Hand-maintained (tools/port/owned.csv).
// Live binding for the typed actor movement rules (ActorMovement.cpp).
#include "Actors/ActorMovementLive.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <optional>

#include "Actors/ActorMotionLive.hpp"
#include "Actors/AnimationControlLive.hpp"
#include "Actors/MovementCommand.hpp"
#include "Audio/ActorSoundLive.hpp"
#include "Battle/Ai/ActorCleanupLive.hpp"
#include "Battle/Ai/GroundNavigationLive.hpp"
#include "Core/Log.hpp"
#include "Field/ActorCollisionLive.hpp"
#include "Field/CollisionMapLive.hpp"
#include "Field/PlatformPlacementLive.hpp"

extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/Movement.h"
#include "Effects/ActionEffects.h"
#include "Field/Collision.h"
#include "Field/GimmickLookup.h"
#include "Field/Gimmicks.h"
#include "State/Timers.h"
}

namespace VS::Actors {
namespace {
constexpr std::size_t kActorSlots = std::size(Actors_actorRecords);
// Every table slot plus one record a caller names outside the table.
constexpr std::size_t kRegistryCapacity = kActorSlots + 1;
constexpr std::uint8_t kFourBits = 0xF;
using ContactPhase = PlatformContactPhase;
constexpr int kLastContactMode = static_cast<int>(ContactPhase::kCommit);
using GroundedPair = std::array<std::int16_t, 2>;
static_assert(sizeof(GroundedPair) == sizeof(std::int32_t));
static_assert(sizeof(PackedTile) == sizeof(std::int32_t));

template <std::size_t SourceSize, typename Target, std::size_t TargetSize>
void CopyTable(const u_char (&source)[SourceSize],
               std::array<Target, TargetSize>& target) {
  static_assert(SourceSize >= TargetSize);
  std::transform(std::begin(source), std::begin(source) + TargetSize,
                 target.begin(),
                 [](u_char value) { return static_cast<Target>(value); });
}

// The actors one rule run touches, loaded on first use. Every legacy call
// that may read or write records runs inside a Synced scope, which stores
// the typed actors first and reloads them afterwards; the registry stores
// them once more when the run ends.
class MovementRegistry {
 public:
  MovementRegistry() = default;
  MovementRegistry(const MovementRegistry&) = delete;
  MovementRegistry& operator=(const MovementRegistry&) = delete;
  MovementRegistry(MovementRegistry&&) = delete;
  MovementRegistry& operator=(MovementRegistry&&) = delete;
  ~MovementRegistry() { StoreAll(); }

  MovementActor& Attach(Actors_ActorRecord& record) {
    for (std::size_t index = 0; index < count_; ++index) {
      if (entries_.at(index).record == &record) {
        return entries_.at(index).actor;
      }
    }
    if (count_ == entries_.size()) {
      VS_LOG_ERROR("actors", "Actor movement touched more than {} records",
                   entries_.size());
      std::abort();
    }
    Entry& entry = entries_.at(count_++);
    entry.record = &record;
    entry.actor = LoadLiveMovementActor(record);
    return entry.actor;
  }

  Actors_ActorRecord& RecordOf(const MovementActor& actor) {
    for (std::size_t index = 0; index < count_; ++index) {
      if (&entries_.at(index).actor == &actor) {
        return *entries_.at(index).record;
      }
    }
    VS_LOG_ERROR("actors", "Actor movement named an actor it never loaded");
    std::abort();
  }

  void StoreAll() {
    for (std::size_t index = 0; index < count_; ++index) {
      StoreLiveMovementActor(entries_.at(index).actor,
                             *entries_.at(index).record);
    }
  }

  void ReloadAll() {
    for (std::size_t index = 0; index < count_; ++index) {
      entries_.at(index).actor =
          LoadLiveMovementActor(*entries_.at(index).record);
    }
  }

 private:
  struct Entry {
    Actors_ActorRecord* record = nullptr;
    MovementActor actor;
  };
  std::array<Entry, kRegistryCapacity> entries_{};
  std::size_t count_ = 0;
};

class Synced {
 public:
  explicit Synced(MovementRegistry& registry) : registry_(registry) {
    registry_.StoreAll();
  }
  Synced(const Synced&) = delete;
  Synced& operator=(const Synced&) = delete;
  Synced(Synced&&) = delete;
  Synced& operator=(Synced&&) = delete;
  ~Synced() { registry_.ReloadAll(); }

 private:
  MovementRegistry& registry_;
};

// The support samples read the actor table, so they also run synced.
class LiveMovementTerrain final : public MotionTerrainHost {
 public:
  explicit LiveMovementTerrain(MovementRegistry& registry)
      : registry_(registry) {}

  int SupportHeight(const MotionVector& origin, SupportProbe probe) override {
    const SVECTOR point = LiveSvector(origin);
    const Synced synced(registry_);
    return VS::Field::LiveSampleActorSupportHeight(&point, 0, 0,
                                                   static_cast<int>(probe));
  }
  int InterpolatedFloorHeight(int x, int z) override {
    const Synced synced(registry_);
    return VS::Field::LiveInterpolatedFloorHeight(x, z);
  }
  int GimmickSupportHeight(int tile_x, int tile_z) override {
    const Synced synced(registry_);
    return VS::Field::LiveGimmickSupportHeight(tile_x, tile_z);
  }

 private:
  MovementRegistry& registry_;
};

MovementGimmick GimmickOf(const Field_GimmickInstance& gimmick) {
  return {.mode = gimmick.mode,
          .position_y = gimmick.positionY,
          .support_index = gimmick.objectInfo.supportIndex,
          .bone_count = static_cast<std::uint8_t>(gimmick.boneCount)};
}

std::optional<MovementGimmick> OptionalGimmick(
    const Field_GimmickInstance* gimmick) {
  if (gimmick == nullptr) return std::nullopt;
  return GimmickOf(*gimmick);
}

class LiveMovementHost final : public MovementHost {
 public:
  LiveMovementHost() : tables_(LiveMovementTables()), terrain_(registry_) {}

  MovementActor& Attach(Actors_ActorRecord& record) {
    return registry_.Attach(record);
  }

  MovementActor* Actor(int id) override {
    if (id < 0 || static_cast<std::size_t>(id) >= kActorSlots) return nullptr;
    Actors_ActorRecord* record = Actors_actorRecords[id];
    return record != nullptr ? &registry_.Attach(*record) : nullptr;
  }
  const MovementTables& Tables() override { return tables_; }
  int TickSpeed() override { return State_tickSpeed; }
  MotionTerrainHost& Terrain() override { return terrain_; }

  // Cross-slice calls into the typed actor animation controls (row 20b).
  void PlayAnimation(int slot, int animation, int blend_frames) override {
    const Synced synced(registry_);
    PlayLiveActorAnimation(slot, animation, 0, blend_frames);
  }
  void QueueAnimation(int slot, int animation, int blend_frames,
                      int start_frame) override {
    const Synced synced(registry_);
    QueueLiveActorAnimation(slot, animation, blend_frames, start_frame);
  }
  void UpdateLocomotionAnimation(MovementActor& actor, int distance,
                                 int direction) override {
    Actors_ActorRecord& record = registry_.RecordOf(actor);
    const Synced synced(registry_);
    UpdateLiveActorLocomotionAnimation(&record, distance, direction);
  }

  void PlayModelSound(const MovementActor& actor, int sound) override {
    Actors_ActorRecord& record = registry_.RecordOf(actor);
    const Synced synced(registry_);
    VS::Audio::LivePlayActorModelSound(&record.instance, sound);
  }
  void PlaySoundPair(const MovementActor& actor, int first,
                     int second) override {
    Actors_ActorRecord& record = registry_.RecordOf(actor);
    const Synced synced(registry_);
    VS::Audio::LivePlayActorSoundPair(&record, first, second);
  }
  void PlaySurfaceContactSound(int id, const MovementActor& actor,
                               int sound) override {
    Actors_ActorRecord& record = registry_.RecordOf(actor);
    const Synced synced(registry_);
    VS::Audio::LivePlayActorSurfaceContactSound(id, &record, sound);
  }
  // The effect reads the X and Z components of a three-halfword position.
  void SpawnGroundImpact(int x, int z) override {
    std::array<u_short, kPositionAxes> position{static_cast<u_short>(x), 0,
                                                static_cast<u_short>(z)};
    Effects_SpawnGroundPlaneImpactEffect(position.data());
  }

  int SampleSupportHeight(const MotionVector& origin, int offset_x,
                          int offset_z, int mode) override {
    const SVECTOR point = LiveSvector(origin);
    const Synced synced(registry_);
    return VS::Field::LiveSampleActorSupportHeight(&point, offset_x, offset_z,
                                                   mode);
  }
  int TileSupportHeight(int x, int z) override {
    const Synced synced(registry_);
    return VS::Field::LiveActorSupportHeight(x, z);
  }
  bool HasCollisionCell(const MotionVector& position, int layer) override {
    const SVECTOR point = LiveSvector(position);
    return VS::Field::FindLiveCollisionCellAtPosition(&point, layer) != nullptr;
  }
  int PlatformContactHeight(int x, int y, int z, int mode) override {
    const Synced synced(registry_);
    return VS::Field::LiveMovingPlatformContactHeight(x, y, z, mode);
  }
  SupportContact CurrentSupport() override {
    return {.layer = Actors_supportingPlatformLayer,
            .gimmick = Actors_supportingGimmickIndex};
  }
  void ClearSupportLayer() override { Actors_supportingPlatformLayer = 0; }
  void SetContactSearchDepth(int depth) override {
    Field_platformContactSearchDepth = static_cast<std::int16_t>(depth);
  }
  int CeilingHeight(int x, int z) override {
    return VS::Field::LiveCeilingHeightAt(x, z);
  }
  std::optional<PlatformPoint> MovingPlatform(int layer) override {
    const auto contact = VS::Field::FindLiveMovingPlatformById(layer);
    if (!contact) return std::nullopt;
    return PlatformPoint{
        .x = contact->x, .height = contact->height, .z = contact->z};
  }
  bool IsPointOnMovingPlatform(int layer,
                               const MotionVector& position) override {
    const SVECTOR point = LiveSvector(position);
    return VS::Field::LiveIsPointOnMovingPlatform(layer, &point) != 0;
  }
  int MovingPlatformCount() override {
    return VS::Field::LiveMovingPlatformCount();
  }
  // #293: the bounce frames start at the first platform.
  int PlatformBounceFrame(int platform) override {
    if (platform < 0 || platform >= MovingPlatformCount() ||
        static_cast<std::size_t>(platform) >=
            std::size(Field_platformBounceFrames)) {
      VS_LOG_WARN("actors", "Platform bounce frame refused for platform {}",
                  platform);
      return 0;
    }
    return Field_platformBounceFrames[platform];
  }
  void SetPlatformRider(int platform, int rider) override {
    Field_SetMovingPlatformActor(platform, rider);
  }
  int TileSurfaceId(int index) override {
    if (index < 0 ||
        static_cast<std::size_t>(index) >= std::size(Field_kTileSurfaceIds)) {
      return 0;
    }
    return Field_kTileSurfaceIds[index];
  }

  TerrainResolve ResolveTerrainMotion(MovementActor& actor,
                                      MotionVector& motion,
                                      bool probe_only) override {
    Actors_ActorRecord& record = registry_.RecordOf(actor);
    SVECTOR live_motion = LiveSvector(motion);
    TerrainResolve resolve;
    {
      const Synced synced(registry_);
      resolve.status = VS::Field::LiveResolveActorTerrainMotion(
          &record, &live_motion, &resolve.climb, probe_only ? 1 : 0);
    }
    motion = LiveMotionVector(live_motion);
    return resolve;
  }
  MovementProbe Probe() override {
    return {.position = LiveMotionVector(Field_collisionProbePosition),
            .direction = Field_collisionProbeDirection,
            .cell_word = VS::Field::LiveCollisionProbeCellWord()};
  }
  bool CanHangFromLedge(MovementActor& actor, MotionVector& offset,
                        MotionVector& motion) override {
    Actors_ActorRecord& record = registry_.RecordOf(actor);
    SVECTOR live_offset = LiveSvector(offset);
    SVECTOR live_motion = LiveSvector(motion);
    bool hangs = false;
    {
      const Synced synced(registry_);
      hangs = VS::Field::LiveCanActorHangFromLedge(&record, &live_offset,
                                                   &live_motion) != 0;
    }
    offset = LiveMotionVector(live_offset);
    motion = LiveMotionVector(live_motion);
    return hangs;
  }
  bool CalcLedgeOffset(MovementActor& actor, MotionVector& offset) override {
    Actors_ActorRecord& record = registry_.RecordOf(actor);
    SVECTOR live_offset = LiveSvector(offset);
    bool found = false;
    {
      const Synced synced(registry_);
      found = VS::Field::LiveCalcActorLedgeOffset(&record, &live_offset) != 0;
    }
    offset = LiveMotionVector(live_offset);
    return found;
  }
  bool HasLedgeSideSupport(MovementActor& actor) override {
    Actors_ActorRecord& record = registry_.RecordOf(actor);
    const Synced synced(registry_);
    return VS::Field::LiveHasActorLedgeSideSupport(&record) != 0;
  }

  std::optional<MovementGimmick> Gimmick(int index) override {
    if (index < 0 ||
        static_cast<std::size_t>(index) >= std::size(Field_gimmickInstances)) {
      return std::nullopt;
    }
    return OptionalGimmick(Field_gimmickInstances[index]);
  }
  std::optional<MovementGimmick> GimmickForActorSlot(int slot) override {
    return OptionalGimmick(Field_FindGimmickForActorSlot(slot));
  }
  // Cross-slice call (gimmick motion): retarget when that row is promoted.
  void UpdateGimmickMotion(int slot, int ticks) override {
    Field_GimmickInstance* gimmick = Field_FindGimmickForActorSlot(slot);
    if (gimmick == nullptr) return;
    const Synced synced(registry_);
    Field_UpdateGimmickMotion(
        reinterpret_cast<Field_GimmickPhysicsView*>(gimmick), ticks);
  }

  void SetAiContactOccupancy(int layer) override {
    const Synced synced(registry_);
    VS::Battle::Ai::SetLiveAiContactOccupancy(layer);
  }
  void ClearAiContactOccupancy(int layer) override {
    const Synced synced(registry_);
    VS::Battle::Ai::ClearLiveAiContactOccupancy(layer);
  }
  void ReleaseAiDestinationTile(int x, int z) override {
    const Synced synced(registry_);
    VS::Battle::Ai::ReleaseLiveAiDestinationTile(x, z);
  }
  void ForceReleaseAiReservations(int id) override {
    const Synced synced(registry_);
    VS::Battle::Ai::ForceReleaseLiveActorAiReservations(id);
  }
  void ClearAiNavigationReset(int slot) override {
    const Synced synced(registry_);
    VS::Battle::Ai::ClearLiveActorAiNavigationReset(slot);
  }
  void ReleaseAiContactOccupancy(int slot) override {
    const Synced synced(registry_);
    VS::Battle::Ai::ReleaseLiveActorAiContactOccupancy(slot);
  }
  void SetDirectMovement(int id, int direction, int distance) override {
    const Synced synced(registry_);
    VS::Actors::SetDirectMovement(ActorRecordMovementRequests(), id, direction,
                                  distance);
  }
  void SetActorUpdateComplete() override { Actors_isActorUpdateComplete = 1; }

 private:
  MovementTables tables_;
  MovementRegistry registry_;
  LiveMovementTerrain terrain_;
};

// Runs `rule` over the actor `actor` names; a null record is refused.
template <typename Rule>
void WithPointer(Actors_ActorRecord* actor, const char* name, Rule rule) {
  if (actor == nullptr) {
    VS_LOG_WARN("actors", "{} refused: no actor record", name);
    return;
  }
  LiveMovementHost host;
  rule(host, host.Attach(*actor));
}
}  // namespace

MovementActor LoadLiveMovementActor(const Actors_ActorRecord& live) {
  const Actors_ModelInstance& instance = live.instance;
  MovementActor actor;
  actor.motion = LoadLiveActorMotion(live);
  actor.skipped = instance.isSkipped != 0;
  actor.reaction_state = instance.reactionState;
  actor.was_drawn = instance.wasDrawn != 0;
  actor.weapon_drawn = instance.weaponDrawn != 0;
  actor.support_layer = instance.supportLayer;
  actor.placement_layer = instance.placementLayer;
  actor.attach_parent = instance.attachParent;
  actor.attach_bone = instance.attachBone;
  actor.base_color_code = instance.baseColor.cd;
  actor.tile_spare = instance.reserved5F;
  actor.previous_tile = {.x = instance.previousTileX,
                         .layer = instance.previousTileLayer,
                         .z = instance.previousTileZ,
                         .spare = instance.reserved63};
  // Original 0x4C/0x50 history packs X/Y then Z.
  const auto grounded = std::bit_cast<GroundedPair>(instance.supportOffset);
  actor.grounded_position = {grounded[0], grounded[1], instance.reserved50};
  actor.previous_facing = instance.previousFacing;
  actor.saved_placement = std::bit_cast<PackedTile>(live.savedPlacement);
  actor.wall_contact = live.wallContact != 0;
  actor.landing_pending = live.landingPending != 0;
  actor.jump_started = live.jumpStarted != 0;
  actor.support_gimmick = live.supportGimmick;
  actor.tick_remainder = live.jumpLanding;
  actor.animation_id = live.animationId;
  actor.collision_state = live.collisionState;
  actor.blend_frame = static_cast<std::int8_t>(live.blendFrame);
  actor.collision_radius = live.collisionRadius;
  actor.collision_height = live.collisionHeight;
  actor.queued_animation = live.queuedAnimation;
  actor.animation_set = live.animationSet;
  actor.contact_kind = live.contactKind;
  actor.ledge_height = live.ledgeHeight;
  actor.platform_offset_x = live.platformOffsetX;
  actor.platform_offset_z = live.platformOffsetZ;
  actor.pose_reached_end = live.pose.reachedEnd != 0;
  actor.platform_anchor = LiveMotionVector(live.platformAnchor);
  actor.companion_slot = live.companionSlot;
  actor.ledge_attempts = live.opaque17FF;
  actor.move_facing = live.moveFacing;
  actor.jump_frames = live.jumpFrames;
  actor.momentum_direction = live.momentumDirection;
  actor.run_turns = live.runTurns;
  actor.request_step = LiveMotionVector(live.turnState.step);
  actor.pending_target_x = live.pendingTargetX;
  actor.pending_target_z = live.pendingTargetZ;
  actor.previous_target_x = live.previousTargetX;
  actor.previous_target_z = live.previousTargetZ;
  return actor;
}

// The fields marked read in MovementActor are never stored.
void StoreLiveMovementActor(const MovementActor& actor,
                            Actors_ActorRecord& live) {
  StoreLiveActorMotion(actor.motion, live);
  Actors_ModelInstance& instance = live.instance;
  instance.reactionState = actor.reaction_state & kFourBits;
  instance.supportLayer = actor.support_layer & kFourBits;
  instance.placementLayer = actor.placement_layer & kFourBits;
  instance.baseColor.cd = actor.base_color_code;
  instance.previousTileX = actor.previous_tile.x;
  instance.previousTileLayer = actor.previous_tile.layer;
  instance.previousTileZ = actor.previous_tile.z;
  instance.reserved63 = actor.previous_tile.spare;
  instance.supportOffset = std::bit_cast<std::int32_t>(
      GroundedPair{actor.grounded_position[0], actor.grounded_position[1]});
  instance.reserved50 = actor.grounded_position[2];
  instance.previousFacing = actor.previous_facing;
  live.savedPlacement = std::bit_cast<std::int32_t>(actor.saved_placement);
  live.landingPending = actor.landing_pending ? 1 : 0;
  live.jumpStarted = actor.jump_started ? 1 : 0;
  live.supportGimmick = actor.support_gimmick & kFourBits;
  live.jumpLanding = actor.tick_remainder;
  live.collisionState = actor.collision_state;
  live.queuedAnimation = actor.queued_animation;
  live.animationSet = actor.animation_set;
  live.contactKind = actor.contact_kind;
  live.ledgeHeight = actor.ledge_height;
  live.platformOffsetX = actor.platform_offset_x;
  live.platformOffsetZ = actor.platform_offset_z;
  live.platformAnchor = LiveSvector(actor.platform_anchor);
  live.opaque17FF = actor.ledge_attempts;
  live.moveFacing = actor.move_facing;
  live.jumpFrames = actor.jump_frames;
  live.momentumDirection = actor.momentum_direction;
  live.runTurns = actor.run_turns;
  live.pendingTargetX = actor.pending_target_x;
  live.pendingTargetZ = actor.pending_target_z;
  live.previousTargetX = actor.previous_target_x;
  live.previousTargetZ = actor.previous_target_z;
}

// The BATTLE.PRG tables D_800E90AC-D_800E9300 and Field D_800E86C0.
MovementTables LiveMovementTables() {
  MovementTables tables;
  CopyTable(Actors_kJumpHangFrames, tables.hang_frames);
  CopyTable(Actors_kJumpHeightAdjustments, tables.height_adjustments);
  CopyTable(Actors_kMotionResultAnimations, tables.result_animations);
  tables.result_one_animation = Actors_kMotionResultOneAnimationId;
  tables.hang_animation = Actors_kHangMotionAnimationId;
  CopyTable(Actors_kAirTimerEndAnimations, tables.air_timer_end);
  CopyTable(Actors_kSoftLandingAnimations, tables.soft_landing);
  CopyTable(Actors_kHardLandingAnimations, tables.hard_landing);
  CopyTable(Actors_kLandingRecoveryAnimations, tables.landing_recovery);
  CopyTable(Field_kGimmickSpinHeightSteps, tables.gimmick_spin_steps);
  return tables;
}

// decomp: BATTLE.PRG func_800A208C (src/BATTLE/BATTLE.PRG/38C1C.c:450)
void BeginLiveActorJumpToTile(Actors_ActorRecord* actor, PackedTile target,
                              int speed, std::int16_t facing) {
  WithPointer(actor, "Actor tile jump",
              [&](LiveMovementHost& host, MovementActor& moving) {
                BeginActorJumpToTile(host, moving, target, speed, facing);
              });
}

// decomp: BATTLE.PRG func_800A2C48 (src/BATTLE/BATTLE.PRG/3A1A0.c:153)
void CancelLiveActorTeleportTransition(Actors_ActorRecord* actor) {
  WithPointer(actor, "Actor teleport cancel",
              [](LiveMovementHost& host, MovementActor& moving) {
                CancelActorTeleportTransition(host, moving);
              });
}

// decomp: BATTLE.PRG func_800A48CC (src/BATTLE/BATTLE.PRG/3A1A0.c:1048)
void BeginLiveActorJumpMovement(int actor_id, int direction, int distance) {
  LiveMovementHost host;
  BeginActorJumpMovement(host, actor_id, direction, distance);
}

// decomp: BATTLE.PRG func_800A4A88 (src/BATTLE/BATTLE.PRG/3A1A0.c:1107)
void UpdateLiveActorPlatformContact(Actors_ActorRecord* actor, int mode) {
  if (mode < 0 || mode > kLastContactMode) {
    VS_LOG_WARN("actors", "Platform contact refused: mode {} is not 0 to {}",
                mode, kLastContactMode);
    return;
  }
  WithPointer(actor, "Platform contact",
              [mode](LiveMovementHost& host, MovementActor& moving) {
                UpdateActorPlatformContact(host, moving,
                                           static_cast<ContactPhase>(mode));
              });
}

// decomp: BATTLE.PRG func_800A4D8C (src/BATTLE/BATTLE.PRG/3A1A0.c:1182)
void UpdateLiveRoomActorMovement() {
  LiveMovementHost host;
  UpdateRoomActorMovement(host);
}

// decomp: BATTLE.PRG func_800A4E68 (src/BATTLE/BATTLE.PRG/3A1A0.c:1199)
void UpdateLiveActorMovement(int actor_id) {
  LiveMovementHost host;
  UpdateActorMovement(host, actor_id);
}

// decomp: BATTLE.PRG func_800AC690 (src/BATTLE/BATTLE.PRG/40564.c:2297)
void UpdateLiveActorJumpAndFall(int actor_id, Actors_ActorRecord* actor) {
  WithPointer(actor, "Actor jump and fall",
              [actor_id](LiveMovementHost& host, MovementActor& moving) {
                UpdateActorJumpAndFall(host, actor_id, moving);
              });
}
}  // namespace VS::Actors
