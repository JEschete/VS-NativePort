// Hand-maintained (tools/port/owned.csv).
// Live binding for the Ashley save snapshot (Snapshot.cpp): the pending door
// record, Ashley's placement and battle actor, the action table, the field
// camera and the snapshot globals the save image carries.
#include "Save/SnapshotLive.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <span>
#include <type_traits>

#include "Actors/ActorStateLive.hpp"
#include "Actors/Location.h"
#include "Actors/RoomEnemyMemoryLive.hpp"
#include "Battle/AbilityState.h"
#include "Battle/Action.h"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Field/CameraLiveState.hpp"
#include "Field/ChestInteractionsLive.hpp"
#include "Field/Doors.h"
#include "Field/Mpd.h"
#include "Legacy/Psx.h"
#include "Save/Snapshot.hpp"
#include "State/Ashley.hpp"
#include "State/Location.hpp"
#include "State/Progress.hpp"

namespace VS::Save {
namespace {
// The door and snapshot records the core addresses as bytes.
static_assert(sizeof(Field_MpdDoorRecord) == kEntryDoorBytes);
static_assert(sizeof(VS::State::savedEntryDoor) == kEntryDoorBytes);
static_assert(sizeof(VS::State::AshleySnapshot) == kAshleySnapshotBytes);
static_assert(offsetof(VS::State::AshleySnapshot, stats) == 0);
static_assert(offsetof(VS::State::AshleyStatsSnapshot, is_pending) == 0);
static_assert(sizeof(VS::State::AshleyStatsSnapshot::is_pending) ==
              sizeof(std::int32_t));
static_assert(std::extent_v<decltype(VS::State::learnedActionFlags)> ==
              kLearnedActionBytes);
static_assert(std::extent_v<decltype(Battle_actionDefinitions)> ==
              kLearnedActionCount);
// The statistics the capture copies field by field.
static_assert(std::extent_v<decltype(Battle_ActorStats::limbs)> ==
              kSnapshotLimbCount);
static_assert(std::extent_v<decltype(VS::State::AshleyStatsSnapshot::limbs)> ==
              kSnapshotLimbCount);
static_assert(std::extent_v<decltype(Battle_ActorStats::statusTimers)> ==
              kSnapshotStatusTimerCount);
static_assert(
    std::extent_v<decltype(VS::State::AshleyStatsSnapshot::status_timers)> ==
    kSnapshotStatusTimerCount);
static_assert(std::extent_v<decltype(Battle_LimbStats::types)> ==
              kSnapshotLimbTypeCount);
static_assert(std::extent_v<decltype(VS::State::AshleyLimbSnapshot::types)> ==
              kSnapshotLimbTypeCount);
static_assert(std::extent_v<decltype(Battle_LimbStats::affinities)> ==
              kSnapshotLimbAffinityCount);
static_assert(
    std::extent_v<decltype(VS::State::AshleyLimbSnapshot::affinities)> ==
    kSnapshotLimbAffinityCount);
static_assert(sizeof(Battle_ActorStats::risk) == sizeof(std::uint16_t));
static_assert(sizeof(Battle_ActorStats::actionRecoveryTimer) ==
              sizeof(std::uint16_t));
static_assert(sizeof(VS::State::AshleyStatsSnapshot::action_recovery_timer) ==
              sizeof(std::uint16_t));

template <typename T>
std::span<std::byte, sizeof(T)> BytesOf(T& value) {
  return std::as_writable_bytes(std::span<T, 1>{&value, 1});
}

SnapshotLimb LimbOf(const Battle_LimbStats& limb) {
  SnapshotLimb copy{.hp = static_cast<std::uint16_t>(limb.hp),
                    .max_hp = static_cast<std::uint16_t>(limb.maxHp)};
  std::ranges::transform(limb.types, copy.types.begin(), [](auto type) {
    return static_cast<std::uint16_t>(type);
  });
  std::ranges::transform(
      limb.affinities, copy.affinities.begin(),
      [](auto affinity) { return static_cast<std::uint16_t>(affinity); });
  return copy;
}

AshleyCombatStats StatsOf(const Battle_Actor& actor,
                          const Battle_ActorStats& stats) {
  AshleyCombatStats copy{
      .current_hp = stats.currentHp,
      .max_hp = stats.maxHp,
      .current_mp = stats.currentMp,
      .max_mp = stats.maxMp,
      .strength = static_cast<std::int16_t>(stats.strength),
      .total_strength = static_cast<std::int16_t>(stats.totalStrength),
      .intelligence = static_cast<std::int16_t>(stats.intelligence),
      .total_intelligence = static_cast<std::int16_t>(stats.totalIntelligence),
      .agility = static_cast<std::int16_t>(stats.agility),
      .total_agility = static_cast<std::int16_t>(stats.totalAgility),
      .risk = static_cast<std::int16_t>(stats.risk),
      .action_recovery_timer = stats.actionRecoveryTimer,
      .statuses = stats.statuses,
      .weapon_drawn = actor.weaponDrawn};
  std::ranges::transform(
      stats.statusTimers, copy.status_timers.begin(),
      [](auto timer) { return static_cast<std::int8_t>(timer); });
  std::ranges::transform(stats.limbs, copy.limbs.begin(), LimbOf);
  return copy;
}

void StoreLimb(const SnapshotLimb& limb,
               VS::State::AshleyLimbSnapshot& stored) {
  stored.hp = limb.hp;
  stored.max_hp = limb.max_hp;
  std::ranges::copy(limb.types, std::begin(stored.types));
  std::ranges::copy(limb.affinities, std::begin(stored.affinities));
}

class LiveSnapshotHost final : public SnapshotHost {
 public:
  EntryDoor PendingDestinationDoor() override {
    EntryDoor door{};
    std::ranges::copy(BytesOf(Field_pendingDestinationDoor), door.begin());
    return door;
  }
  std::uint16_t SnapshotDoorFlagId() override {
    return Field_snapshotDoorFlagSource.requiredFlagId;
  }
  TilePlacement AshleyPlacement() override {
    Actors_ActorLocationQuery query{};
    VS::Actors::LiveActorPlacement(0, &query);
    const Actors_TileTarget& target = query.location.packedTile.target;
    return TilePlacement{
        .tile_x = static_cast<std::uint8_t>(target.tileX),
        .tile_layer = static_cast<std::uint8_t>(target.tileLayer),
        .tile_z = static_cast<std::uint8_t>(target.tileZ),
        .facing = static_cast<std::uint8_t>(target.facing)};
  }
  bool ActionUnlocked(std::size_t action) override {
    if (action >= std::extent_v<decltype(Battle_actionDefinitions)>) {
      return false;
    }
    const std::span definitions{Battle_actionDefinitions};
    return definitions[action].unlocked != 0;
  }
  std::optional<AshleyCombatStats> AshleyStats() override {
    const Battle_Actor* actor = Battle_characterState;
    if (actor == nullptr || actor->stats == nullptr) {
      return std::nullopt;
    }
    return StatsOf(*actor, *actor->stats);
  }
  CameraPose Camera() override {
    return CameraPose{.pitch = VS::Field::sphericalCamera.delta.pitch,
                      .distance = VS::Field::sphericalCamera.initialDistance};
  }
  void StoreEntryDoor(const EntryDoor& door) override {
    std::ranges::copy(door, BytesOf(VS::State::savedEntryDoor).begin());
  }
  void StoreLearnedActions(const LearnedActions& learned) override {
    std::ranges::transform(
        learned, std::begin(VS::State::learnedActionFlags),
        [](std::uint8_t bits) { return static_cast<char>(bits); });
  }
  void StoreSnapshot(const AshleySnapshot& snapshot) override {
    VS::State::AshleyStatsSnapshot& stored = VS::State::ashleySnapshot.stats;
    const AshleyCombatStats& stats = snapshot.stats;
    stored.current_hp = stats.current_hp;
    stored.max_hp = stats.max_hp;
    stored.current_mp = stats.current_mp;
    stored.max_mp = stats.max_mp;
    stored.strength = stats.strength;
    stored.total_strength = stats.total_strength;
    stored.intelligence = stats.intelligence;
    stored.total_intelligence = stats.total_intelligence;
    stored.agility = stats.agility;
    stored.total_agility = stats.total_agility;
    stored.risk = stats.risk;
    stored.action_recovery_timer = stats.action_recovery_timer;
    stored.statuses = stats.statuses;
    std::ranges::transform(
        stats.status_timers, std::begin(stored.status_timers),
        [](std::int8_t timer) { return static_cast<std::uint8_t>(timer); });
    const std::span stored_limbs{stored.limbs};
    for (std::size_t limb = 0; limb < kSnapshotLimbCount; ++limb) {
      StoreLimb(snapshot.stats.limbs.at(limb), stored_limbs[limb]);
    }
    stored.weapon_drawn = stats.weapon_drawn;
    stored.camera_pitch = snapshot.camera.pitch;
    stored.camera_distance = snapshot.camera.distance;
  }
  void StoreRoomActorState(std::int32_t actor) override {
    VS::Actors::StoreLiveRoomActorState(actor, 0);
  }
  void StoreEntranceDoorForReload() override {
    VS::Field::StoreEntranceDoorForReloadEntry();
  }
};
}  // namespace

void MarkLiveAshleySnapshotValid() {
  MarkAshleySnapshotValid(BytesOf(VS::State::ashleySnapshot));
}

void ClearLiveAshleySnapshotPending() {
  VS::State::ashleySnapshot.stats.is_pending = 0;
}

void CaptureLiveAshleySnapshot() {
  LiveSnapshotHost host;
  CaptureAshleySnapshot(host);
}
}  // namespace VS::Save
