#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

// The save snapshot of Ashley (SLUS_010.40 main.c func_80042CA0 and
// func_80042CB0): the entry door a reload starts from, the learned-action
// bits and Ashley's combat statistics and camera, captured before a save is
// packaged. The game state reaches the core through SnapshotHost.
namespace VS::Save {

inline constexpr std::size_t kAshleySnapshotBytes = 0x100;
inline constexpr std::size_t kEntryDoorBytes = 0xC;
inline constexpr std::size_t kLearnedActionBytes = 32;
inline constexpr std::size_t kActionsPerLearnedByte = 8;
inline constexpr std::size_t kLearnedActionCount = 256;
static_assert(kLearnedActionCount ==
              kLearnedActionBytes * kActionsPerLearnedByte);
inline constexpr std::size_t kSnapshotLimbCount = 6;
inline constexpr std::size_t kSnapshotLimbTypeCount = 4;
inline constexpr std::size_t kSnapshotLimbAffinityCount = 8;
inline constexpr std::size_t kSnapshotStatusTimerCount = 8;
// The room actors whose state the capture stores (VS::Actors::RememberEnemy).
inline constexpr std::int32_t kSnapshotRoomActorCount = 16;

// A Field_MpdDoorRecord's bytes.
using EntryDoor = std::array<std::byte, kEntryDoorBytes>;
// State_learnedActionFlags: action 8n+k is bit 7-k of byte n.
using LearnedActions = std::array<std::uint8_t, kLearnedActionBytes>;

// Actors_TileTarget: where Ashley stands.
struct TilePlacement {
  std::uint8_t tile_x = 0;
  std::uint8_t tile_layer = 0;
  std::uint8_t tile_z = 0;
  std::uint8_t facing = 0;
};

struct SnapshotLimb {
  std::uint16_t hp = 0;
  std::uint16_t max_hp = 0;
  std::array<std::uint16_t, kSnapshotLimbTypeCount> types{};
  std::array<std::uint16_t, kSnapshotLimbAffinityCount> affinities{};
};

// The Battle_ActorStats and Battle_Actor fields the snapshot keeps, in the
// snapshot's own field types.
struct AshleyCombatStats {
  std::int16_t current_hp = 0;
  std::int16_t max_hp = 0;
  std::int16_t current_mp = 0;
  std::int16_t max_mp = 0;
  std::int16_t strength = 0;
  std::int16_t total_strength = 0;
  std::int16_t intelligence = 0;
  std::int16_t total_intelligence = 0;
  std::int16_t agility = 0;
  std::int16_t total_agility = 0;
  std::int16_t risk = 0;
  std::uint16_t action_recovery_timer = 0;
  std::int32_t statuses = 0;
  std::array<std::int8_t, kSnapshotStatusTimerCount> status_timers{};
  std::array<SnapshotLimb, kSnapshotLimbCount> limbs{};
  std::int32_t weapon_drawn = 0;
};

// Field_sphericalCamera's pitch delta and initial distance.
struct CameraPose {
  std::int32_t pitch = 0;
  std::int32_t distance = 0;
};

// State_AshleyStatsSnapshot after its pending flag.
struct AshleySnapshot {
  AshleyCombatStats stats;
  CameraPose camera;
};

class SnapshotHost {
 public:
  virtual ~SnapshotHost() = default;
  // Field_pendingDestinationDoor.
  virtual EntryDoor PendingDestinationDoor() = 0;
  // Field_snapshotDoorFlagSource.requiredFlagId.
  virtual std::uint16_t SnapshotDoorFlagId() = 0;
  virtual TilePlacement AshleyPlacement() = 0;
  virtual bool ActionUnlocked(std::size_t action) = 0;
  // Nothing when Ashley has no battle actor.
  virtual std::optional<AshleyCombatStats> AshleyStats() = 0;
  virtual CameraPose Camera() = 0;
  virtual void StoreEntryDoor(const EntryDoor& door) = 0;
  virtual void StoreLearnedActions(const LearnedActions& learned) = 0;
  // Writes the statistics, leaving the pending flag as it is.
  virtual void StoreSnapshot(const AshleySnapshot& snapshot) = 0;
  virtual void StoreRoomActorState(std::int32_t actor) = 0;
  virtual void StoreEntranceDoorForReload() = 0;
};

// The pending door with Ashley's tile, layer, elevation and the snapshot
// flag folded into its entry fields.
[[nodiscard]] EntryDoor PlaceEntryDoor(const EntryDoor& pending,
                                       TilePlacement placement,
                                       std::uint16_t flag_id);
[[nodiscard]] LearnedActions PackLearnedActions(SnapshotHost& host);
// Sets State_AshleySnapshot's pending flag in its bytes.
void MarkAshleySnapshotValid(std::span<std::byte, kAshleySnapshotBytes> bytes);
void CaptureAshleySnapshot(SnapshotHost& host);
}  // namespace VS::Save
