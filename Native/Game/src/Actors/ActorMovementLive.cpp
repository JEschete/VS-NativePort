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
#include "Actors/ActorRecord.hpp"
#include "Actors/Animation.hpp"
#include "Actors/AnimationControlLive.hpp"
#include "Actors/Movement.hpp"
#include "Actors/MovementCommand.hpp"
#include "Audio/ActorSoundLive.hpp"
#include "Battle/Ai/ActorCleanupLive.hpp"
#include "Battle/Ai/GroundNavigationLive.hpp"
#include "Core/Log.hpp"
#include "Effects/SecondaryEffectRequestsLive.hpp"
#include "Field/ActorCollisionLive.hpp"
#include "Field/Collision.hpp"
#include "Field/CollisionMapLive.hpp"
#include "Field/GimmickDynamicsLive.hpp"
#include "Field/GimmickLookup.hpp"
#include "Field/Gimmicks.hpp"
#include "Field/PlatformPlacementLive.hpp"
#include "State/Timers.hpp"

using VS::Core::Gte::SVECTOR;

namespace VS::Actors {
namespace {
constexpr std::size_t kActorSlots = std::size(VS::Actors::actorRecords);
// Every table slot plus one record a caller names outside the table.
constexpr std::size_t kRegistryCapacity = kActorSlots + 1;
constexpr std::uint8_t kFourBits = 0xF;
using ContactPhase = PlatformContactPhase;
constexpr int kLastContactMode = static_cast<int>(ContactPhase::kCommit);
using GroundedPair = std::array<std::int16_t, 2>;
static_assert(sizeof(GroundedPair) == sizeof(std::int32_t));
static_assert(sizeof(PackedTile) == sizeof(std::int32_t));

template <std::size_t SourceSize, typename Target, std::size_t TargetSize>
void CopyTable(const std::uint8_t (&source)[SourceSize],
               std::array<Target, TargetSize>& target) {
  static_assert(SourceSize >= TargetSize);
  std::transform(std::begin(source), std::begin(source) + TargetSize,
                 target.begin(),
                 [](std::uint8_t value) { return static_cast<Target>(value); });
}

template <std::size_t SourceSize, typename Target, std::size_t TargetSize>
void CopyTable(const std::array<std::uint8_t, SourceSize>& source,
               std::array<Target, TargetSize>& target) {
  static_assert(SourceSize >= TargetSize);
  std::transform(source.begin(), source.begin() + TargetSize, target.begin(),
                 [](std::uint8_t value) { return static_cast<Target>(value); });
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

  MovementActor& Attach(VS::Actors::ActorRecord& record) {
    for (std::size_t index = 0; index < count_; ++index) {
      if (entries_.at(index).record == &record) {
        return entries_.at(index).actor;
      }
    }
    if (count_ == entries_.size()) {
      VS::Core::LogError("actors",
                         "Actor movement touched more than {} records",
                         entries_.size());
      std::abort();
    }
    Entry& entry = entries_.at(count_++);
    entry.record = &record;
    entry.actor = LoadLiveMovementActor(record);
    return entry.actor;
  }

  VS::Actors::ActorRecord& RecordOf(const MovementActor& actor) {
    for (std::size_t index = 0; index < count_; ++index) {
      if (&entries_.at(index).actor == &actor) {
        return *entries_.at(index).record;
      }
    }
    VS::Core::LogError("actors",
                       "Actor movement named an actor it never loaded");
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
    VS::Actors::ActorRecord* record = nullptr;
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

MovementGimmick GimmickOf(const VS::Field::GimmickInstance& gimmick) {
  return {.mode = gimmick.mode,
          .position_y = gimmick.position_y,
          .motion_frame = gimmick.object_info.motion_frame,
          .bone_count = static_cast<std::uint8_t>(gimmick.bone_count)};
}

std::optional<MovementGimmick> OptionalGimmick(
    const VS::Field::GimmickInstance* gimmick) {
  if (gimmick == nullptr) return std::nullopt;
  return GimmickOf(*gimmick);
}

class LiveMovementHost final : public MovementHost {
 public:
  LiveMovementHost() : tables_(LiveMovementTables()), terrain_(registry_) {}

  MovementActor& Attach(VS::Actors::ActorRecord& record) {
    return registry_.Attach(record);
  }

  MovementActor* Actor(int id) override {
    if (id < 0 || static_cast<std::size_t>(id) >= kActorSlots) return nullptr;
    VS::Actors::ActorRecord* record = VS::Actors::actorRecords[id];
    return record != nullptr ? &registry_.Attach(*record) : nullptr;
  }
  const MovementTables& Tables() override { return tables_; }
  int TickSpeed() override { return VS::State::tickSpeed; }
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
    VS::Actors::ActorRecord& record = registry_.RecordOf(actor);
    const Synced synced(registry_);
    UpdateLiveActorLocomotionAnimation(&record, distance, direction);
  }

  void PlayModelSound(const MovementActor& actor, int sound) override {
    VS::Actors::ActorRecord& record = registry_.RecordOf(actor);
    const Synced synced(registry_);
    VS::Audio::LivePlayActorModelSound(&record.instance, sound);
  }
  void PlaySoundPair(const MovementActor& actor, int first,
                     int second) override {
    VS::Actors::ActorRecord& record = registry_.RecordOf(actor);
    const Synced synced(registry_);
    VS::Audio::LivePlayActorSoundPair(&record, first, second);
  }
  void PlaySurfaceContactSound(int id, const MovementActor& actor,
                               int sound) override {
    VS::Actors::ActorRecord& record = registry_.RecordOf(actor);
    const Synced synced(registry_);
    VS::Audio::LivePlayActorSurfaceContactSound(id, &record, sound);
  }
  // The effect reads the X and Z components of a three-halfword position.
  void SpawnGroundImpact(int x, int z) override {
    std::array<std::uint16_t, kPositionAxes> position{
        static_cast<std::uint16_t>(x), 0, static_cast<std::uint16_t>(z)};
    VS::Effects::LiveSpawnGroundPlaneImpactEffect(position.data());
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
    return {.layer = VS::Actors::supportingPlatformLayer,
            .gimmick = VS::Actors::supportingGimmickIndex};
  }
  void ClearSupportLayer() override { VS::Actors::supportingPlatformLayer = 0; }
  void SetContactSearchDepth(int depth) override {
    VS::Field::platformContactSearchDepth = static_cast<std::int16_t>(depth);
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
            std::size(VS::Field::platformBounceFrames)) {
      VS::Core::LogWarn(
          "actors", "Platform bounce frame refused for platform {}", platform);
      return 0;
    }
    return VS::Field::platformBounceFrames[platform];
  }
  void SetPlatformRider(int platform, int rider) override {
    VS::Field::SetMovingPlatformActorEntry(platform, rider);
  }
  int TileSurfaceId(int index) override {
    if (index < 0 || static_cast<std::size_t>(index) >=
                         std::size(VS::Field::tileSurfaceIds)) {
      return 0;
    }
    return VS::Field::tileSurfaceIds[index];
  }

  TerrainResolve ResolveTerrainMotion(MovementActor& actor,
                                      MotionVector& motion,
                                      bool probe_only) override {
    VS::Actors::ActorRecord& record = registry_.RecordOf(actor);
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
    return {.position = LiveMotionVector(VS::Field::collisionProbePosition),
            .direction = VS::Field::collisionProbeDirection,
            .cell_word = VS::Field::LiveCollisionProbeCellWord()};
  }
  bool CanHangFromLedge(MovementActor& actor, MotionVector& offset,
                        MotionVector& motion) override {
    VS::Actors::ActorRecord& record = registry_.RecordOf(actor);
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
    VS::Actors::ActorRecord& record = registry_.RecordOf(actor);
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
    VS::Actors::ActorRecord& record = registry_.RecordOf(actor);
    const Synced synced(registry_);
    return VS::Field::LiveHasActorLedgeSideSupport(&record) != 0;
  }

  std::optional<MovementGimmick> Gimmick(int index) override {
    if (index < 0 || static_cast<std::size_t>(index) >=
                         std::size(VS::Field::gimmickInstances)) {
      return std::nullopt;
    }
    return OptionalGimmick(VS::Field::gimmickInstances[index]);
  }
  std::optional<MovementGimmick> GimmickForActorSlot(int slot) override {
    return OptionalGimmick(VS::Field::FindGimmickForActorSlot(slot));
  }
  void UpdateGimmickMotion(int slot, int) override {
    VS::Field::GimmickInstance* gimmick =
        VS::Field::FindGimmickForActorSlot(slot);
    if (gimmick == nullptr) return;
    const Synced synced(registry_);
    static_cast<void>(VS::Field::UpdateLiveGimmickMotion(gimmick));
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
  void SetActorUpdateComplete() override {
    VS::Actors::isActorUpdateComplete = 1;
  }

 private:
  MovementTables tables_;
  MovementRegistry registry_;
  LiveMovementTerrain terrain_;
};

// Runs `rule` over the actor `actor` names; a null record is refused.
template <typename Rule>
void WithPointer(VS::Actors::ActorRecord* actor, const char* name, Rule rule) {
  if (actor == nullptr) {
    VS::Core::LogWarn("actors", "{} refused: no actor record", name);
    return;
  }
  LiveMovementHost host;
  rule(host, host.Attach(*actor));
}
}  // namespace

MovementActor LoadLiveMovementActor(const VS::Actors::ActorRecord& live) {
  const VS::Actors::ModelInstance& instance = live.instance;
  MovementActor actor;
  actor.motion = LoadLiveActorMotion(live);
  actor.skipped = instance.is_skipped != 0;
  actor.reaction_state = instance.reaction_state;
  actor.was_drawn = instance.was_drawn != 0;
  actor.weapon_drawn = instance.weapon_drawn != 0;
  actor.support_layer = instance.support_layer;
  actor.placement_layer = instance.placement_layer;
  actor.attach_parent = instance.attach_parent;
  actor.attach_bone = instance.attach_bone;
  actor.base_color_code = instance.base_color.cd;
  actor.tile_spare = instance.reserved5f;
  actor.previous_tile = {.x = instance.previous_tile_x,
                         .layer = instance.previous_tile_layer,
                         .z = instance.previous_tile_z,
                         .spare = instance.reserved63};
  // Original 0x4C/0x50 history packs X/Y then Z.
  const auto grounded = std::bit_cast<GroundedPair>(instance.grounded_xy);
  actor.grounded_position = {grounded[0], grounded[1], instance.grounded_z};
  actor.previous_facing = instance.previous_facing;
  actor.saved_placement = std::bit_cast<PackedTile>(live.saved_placement);
  actor.wall_contact = live.wall_contact != 0;
  actor.landing_pending = live.landing_pending != 0;
  actor.jump_started = live.jump_started != 0;
  actor.support_gimmick = live.support_gimmick;
  actor.tick_remainder = live.tick_remainder;
  actor.animation_id = live.animation_id;
  actor.speed_quarters = live.speed_quarters;
  actor.blend_frame = static_cast<std::int8_t>(live.blend_frame);
  actor.collision_radius = live.collision_radius;
  actor.collision_height = live.collision_height;
  actor.queued_animation = live.queued_animation;
  actor.animation_set = live.animation_set;
  actor.contact_kind = live.contact_kind;
  actor.ledge_height = live.ledge_height;
  actor.platform_offset_x = live.platform_offset_x;
  actor.platform_offset_z = live.platform_offset_z;
  actor.pose_reached_end = live.pose.reached_end != 0;
  actor.platform_anchor = LiveMotionVector(live.platform_anchor);
  actor.companion_slot = live.companion_slot;
  actor.ledge_attempts = live.ledge_attempts;
  actor.move_facing = live.move_facing;
  actor.jump_frames = live.jump_frames;
  actor.momentum_direction = live.momentum_direction;
  actor.slow_ticks = live.slow_ticks;
  actor.request_step = LiveMotionVector(live.turn_state.step);
  actor.pending_target_x = live.pending_target_x;
  actor.pending_target_z = live.pending_target_z;
  actor.previous_target_x = live.previous_target_x;
  actor.previous_target_z = live.previous_target_z;
  return actor;
}

// The fields marked read in MovementActor are never stored.
void StoreLiveMovementActor(const MovementActor& actor,
                            VS::Actors::ActorRecord& live) {
  StoreLiveActorMotion(actor.motion, live);
  VS::Actors::ModelInstance& instance = live.instance;
  instance.reaction_state = actor.reaction_state & kFourBits;
  instance.support_layer = actor.support_layer & kFourBits;
  instance.placement_layer = actor.placement_layer & kFourBits;
  instance.base_color.cd = actor.base_color_code;
  instance.previous_tile_x = actor.previous_tile.x;
  instance.previous_tile_layer = actor.previous_tile.layer;
  instance.previous_tile_z = actor.previous_tile.z;
  instance.reserved63 = actor.previous_tile.spare;
  instance.grounded_xy = std::bit_cast<std::int32_t>(
      GroundedPair{actor.grounded_position[0], actor.grounded_position[1]});
  instance.grounded_z = actor.grounded_position[2];
  instance.previous_facing = actor.previous_facing;
  live.saved_placement = std::bit_cast<std::int32_t>(actor.saved_placement);
  live.landing_pending = actor.landing_pending ? 1 : 0;
  live.jump_started = actor.jump_started ? 1 : 0;
  live.support_gimmick = actor.support_gimmick & kFourBits;
  live.tick_remainder = actor.tick_remainder;
  live.speed_quarters = actor.speed_quarters;
  live.queued_animation = actor.queued_animation;
  live.animation_set = actor.animation_set;
  live.contact_kind = actor.contact_kind;
  live.ledge_height = actor.ledge_height;
  live.platform_offset_x = actor.platform_offset_x;
  live.platform_offset_z = actor.platform_offset_z;
  live.platform_anchor = LiveSvector(actor.platform_anchor);
  live.ledge_attempts = actor.ledge_attempts;
  live.move_facing = actor.move_facing;
  live.jump_frames = actor.jump_frames;
  live.momentum_direction = actor.momentum_direction;
  live.slow_ticks = actor.slow_ticks;
  live.pending_target_x = actor.pending_target_x;
  live.pending_target_z = actor.pending_target_z;
  live.previous_target_x = actor.previous_target_x;
  live.previous_target_z = actor.previous_target_z;
}

// The BATTLE.PRG tables D_800E90AC-D_800E9300 and Field D_800E86C0.
MovementTables LiveMovementTables() {
  MovementTables tables;
  CopyTable(VS::Actors::jumpHangFrames, tables.hang_frames);
  CopyTable(VS::Actors::jumpHeightAdjustments, tables.height_adjustments);
  CopyTable(VS::Actors::motionResultAnimations, tables.result_animations);
  tables.result_one_animation = VS::Actors::motionResultOneAnimationId;
  tables.hang_animation = VS::Actors::hangMotionAnimationId;
  CopyTable(VS::Actors::airTimerEndAnimations, tables.air_timer_end);
  CopyTable(VS::Actors::softLandingAnimations, tables.soft_landing);
  CopyTable(VS::Actors::hardLandingAnimations, tables.hard_landing);
  CopyTable(VS::Actors::landingRecoveryAnimations, tables.landing_recovery);
  CopyTable(VS::Field::kGimmickSpinHeightSteps, tables.gimmick_spin_steps);
  return tables;
}

// decomp: BATTLE.PRG func_800A208C (src/BATTLE/BATTLE.PRG/38C1C.c:450)
void BeginLiveActorJumpToTile(VS::Actors::ActorRecord* actor, PackedTile target,
                              int speed, std::int16_t facing) {
  WithPointer(actor, "Actor tile jump",
              [&](LiveMovementHost& host, MovementActor& moving) {
                BeginActorJumpToTile(host, moving, target, speed, facing);
              });
}

// decomp: BATTLE.PRG func_800A2C48 (src/BATTLE/BATTLE.PRG/3A1A0.c:153)
void CancelLiveActorTeleportTransition(VS::Actors::ActorRecord* actor) {
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
void UpdateLiveActorPlatformContact(VS::Actors::ActorRecord* actor, int mode) {
  if (mode < 0 || mode > kLastContactMode) {
    VS::Core::LogWarn("actors",
                      "Platform contact refused: mode {} is not 0 to {}", mode,
                      kLastContactMode);
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
void UpdateLiveActorJumpAndFall(int actor_id, VS::Actors::ActorRecord* actor) {
  WithPointer(actor, "Actor jump and fall",
              [actor_id](LiveMovementHost& host, MovementActor& moving) {
                UpdateActorJumpAndFall(host, actor_id, moving);
              });
}
}  // namespace VS::Actors
