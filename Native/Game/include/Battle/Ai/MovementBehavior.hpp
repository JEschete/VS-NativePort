#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Battle/Ai/Waypoints.hpp"
#include "Core/Math/Distance.hpp"
#include "Core/Math/ScalarMath.hpp"

// Battle AI movement behaviours: the movement mode an AI actor runs (pursuit,
// walking to a tile, wandering, fleeing, patrolling or facing a target), the
// tile it steers toward and the relationship queries that pick whom to follow
// or flee. The battle runtime owns the actor AI state; these routines see one
// actor's movement fields through MovementState and every other actor, the
// room and the movement primitives through MovementHost, so they reach no AI
// state record, PS1 address or callback table.
namespace VS::Battle::Ai {
// Actor slots the relation words, the frame actor table and the battle actor
// IDs cover.
inline constexpr int kMovementActorSlots = 16;
inline constexpr std::size_t kMovementModeCounterBytes = 13;
// Moving-platform groups a route gate can belong to.
inline constexpr int kPlatformGroupCount = 4;

enum class MovementMode : std::uint8_t {
  kIdle = 0,
  kPursuit = 1,
  kDestination = 2,
  kWander = 3,
  kFlee = 4,
  kPatrol = 5,
  kFacing = 6,
};
inline constexpr int kMovementModeCount = 7;

// A packed AI tile word: X in bits 0-7, layer in 8-15 and Z in 16-23.
inline constexpr std::uint32_t kPackedTileFieldMask = 0xFFu;
inline constexpr unsigned kPackedTileLayerShift = 8;
inline constexpr unsigned kPackedTileZShift = 16;

[[nodiscard]] constexpr unsigned PackedTileX(std::uint32_t tile) {
  return tile & kPackedTileFieldMask;
}
[[nodiscard]] constexpr unsigned PackedTileLayer(std::uint32_t tile) {
  return (tile >> kPackedTileLayerShift) & kPackedTileFieldMask;
}
[[nodiscard]] constexpr unsigned PackedTileZ(std::uint32_t tile) {
  return (tile >> kPackedTileZShift) & kPackedTileFieldMask;
}

// Pursuit progress the target approach keeps (AI state 0xE4-0xEB). Entering
// pursuit clears the first six bytes; changing target clears all eight.
struct PursuitProgress {
  std::uint8_t opaque_e4 = 0;
  std::uint8_t target_id = 0;
  std::uint8_t timeout = 0;
  std::uint8_t opaque_e7 = 0;
  std::uint8_t opaque_e8 = 0;
  std::uint8_t range_mode = 0;
  std::uint8_t previous_option = 0;
  std::uint8_t previous_target_id = 0;
};

// The patrol waypoint the actor walks to (AI state 0x100, 0x101 and 0x104).
struct PatrolProgress {
  std::uint8_t index = 0;
  std::uint8_t row = 0;
  std::uint8_t cooldown = 0;
};

// One actor's movement fields (offsets in the AI state record).
struct MovementState {
  std::uint8_t actor_id = 0;         // 0x88
  std::uint32_t tile = 0;            // 0x34
  Core::Math::Position3 position{};  // 0x4C
  bool movement_enabled = false;     // 0x23
  // Settings bit 14 (0x29): pursue a target standing on a platform.
  bool follows_platform_targets = false;
  std::uint16_t movement_flags = 0;  // 0x15E
  std::uint8_t requested_mode = 0;   // 0x89
  std::uint8_t current_mode = 0;     // 0x8A
  std::array<std::uint8_t, kMovementModeCounterBytes> mode_counters{};
  std::uint32_t navigation_flags = 0;    // 0x190
  PursuitProgress pursuit{};             // 0xE4
  std::int32_t approach_range = 0;       // 0xDC
  std::uint32_t cached_destination = 0;  // 0xEC
  // The target's position in half tiles: X in byte 0, Z in byte 2 (0xF4).
  std::uint32_t target_half_tile = 0;
  std::uint16_t wander_phase = 0;         // 0xC0
  std::uint16_t wander_cooldown = 0;      // 0xD2
  std::int32_t wander_center_radius = 0;  // 0xC8
  std::uint32_t wander_center = 0;        // 0xCC
  PatrolProgress patrol{};
  std::uint32_t previous_facing_tile = 0;  // 0x108
  std::uint8_t facing_pending = 0;         // 0x10C
  std::uint32_t flee_tile = 0;             // 0x110
  std::uint16_t flee_timer = 0;            // 0x114
  // Byte 0x04: ground steps count as arrived once near the destination.
  std::uint8_t arrive_when_near = 0;
  std::uint32_t destination = 0;                               // 0x168
  std::int32_t move_x = 0;                                     // 0x17C
  std::int32_t move_z = 0;                                     // 0x180
  std::array<std::uint32_t, kMovementActorSlots> relations{};  // 0x1A4
  std::uint8_t plan_mode = 0;                                  // 0x12E
};

// Where another actor's model stands.
struct TargetPlacement {
  // Model byte 0x0A bits 0-4: it is stepping toward its planned tile.
  bool planned_move = false;
  // Model byte 0x0C bits 0-3: the moving platform it stands on, plus two.
  std::uint8_t support_layer = 0;
  std::int16_t x = 0;              // model 0x1C
  std::int16_t z = 0;              // model 0x20
  std::int16_t planned_x = 0;      // model 0x4C
  std::int16_t planned_z = 0;      // model 0x50
  std::uint32_t current_tile = 0;  // model 0x5C
  std::uint32_t planned_tile = 0;  // model 0x60
};

// The entry tile of a moving platform's route gate.
struct RouteGate {
  std::uint8_t x = 0;
  std::uint8_t z = 0;
};

// The battle actor list in link order.
struct ActorRoster {
  std::array<std::uint8_t, kMovementActorSlots> ids{};
  std::size_t count = 0;

  [[nodiscard]] std::span<const std::uint8_t> Ids() const {
    return std::span<const std::uint8_t>(ids).first(count);
  }
};

// The first listed actor with a relationship and the tile it stands on; the
// ID is -1 when no actor matches.
struct RelatedActor {
  int actor_id = -1;
  std::uint32_t tile = 0;
};

// The room, the other actors and the movement primitives one actor's movement
// behaviour uses. A primitive taking MovementState sees the state as stored
// before it runs, and its changes are read back into the state afterwards.
class MovementHost {
 public:
  virtual ~MovementHost() = default;

  // The room's patrol route rows, kPatrolWaypointsPerRow halfwords each;
  // empty when the room has none.
  [[nodiscard]] virtual std::span<const std::uint16_t> PatrolRoutes() const = 0;
  // Whether the frame actor table has an AI actor in slot `actor_id`, which
  // is below kMovementActorSlots.
  [[nodiscard]] virtual bool HasFrameActor(int actor_id) const = 0;
  // Nothing when actor `actor_id` has no model.
  [[nodiscard]] virtual std::optional<TargetPlacement> Placement(
      int actor_id) const = 0;
  // Nothing when the platform has no waypoint or the room no route gates.
  [[nodiscard]] virtual std::optional<RouteGate> PlatformRouteGate(
      int group) const = 0;
  // Nothing when the list holds an ID outside the slots or repeats one.
  [[nodiscard]] virtual std::optional<ActorRoster> BattleActors() const = 0;
  [[nodiscard]] virtual std::int32_t TileHeight(std::uint32_t tile) const = 0;
  // Whether the actor runs the patrol-and-combat decision behaviour.
  [[nodiscard]] virtual bool RunsPatrolCombatBehavior() const = 0;
  // The planner's movement action mask (AI state byte 0x2D).
  [[nodiscard]] virtual std::uint8_t ActionMovementMask() const = 0;
  // The best-ranked candidate's action flags; nothing without a candidate.
  [[nodiscard]] virtual std::optional<std::uint32_t> BestCandidateFlags()
      const = 0;

  virtual int UpdateTargetApproach(MovementState& state, int option) = 0;
  virtual void UpdateWander(MovementState& state, std::uint32_t tile,
                            std::uint32_t range) = 0;
  virtual void UpdateWaypointPatrol(MovementState& state) = 0;
  virtual int UpdateTargetFacing(MovementState& state, int target) = 0;
  virtual void BeginMovementTowardPoint(MovementState& state, int kind) = 0;
  virtual void IssueCommand(MovementState& state, int command) = 0;
  virtual void DispatchLocomotion(MovementState& state) = 0;
  // Stores the nearest active waypoint as the planned tile.
  virtual void FindNearestActiveWaypoint(MovementState& state) = 0;
};

// The AI actors an action's relationship changes reach.
class RelationshipChangeHost {
 public:
  virtual ~RelationshipChangeHost() = default;
  // Nothing when the list holds an ID outside the slots or repeats one.
  [[nodiscard]] virtual std::optional<ActorRoster> BattleActors() const = 0;
  [[nodiscard]] virtual bool HasAiState(int actor_id) const = 0;
  virtual void ApplyActionReaction(int actor_id) = 0;
  virtual void PropagateRelationships(int actor_id) = 0;
};

// decomp: func_800E4BB8 (src/BATTLE/BATTLE.PRG/7BDD4.s:497).
// Clears the navigation step flags and, for an actor allowed to move,
// re-arms its movement flags.
[[nodiscard]] std::uint32_t RefreshAiMovementCooldown(
    std::uint32_t navigation_flags, bool movement_enabled);

// decomp: func_800DB4AC (src/BATTLE/BATTLE.PRG/72B70.s:102).
// Requests `mode`; entering a new mode resets its progress and counter.
// Returns `mode` when it is already current, otherwise 3 for pursuit, 6 for
// wandering, 1 for patrol and -1 for the rest. Nothing refuses a mode outside
// MovementMode or a patrol row outside `patrol_routes`.
std::optional<int> SetMovementBehavior(
    MovementState& state, std::span<const std::uint16_t> patrol_routes,
    int mode);

// decomp: func_800E4904 (src/BATTLE/BATTLE.PRG/7BDD4.s:279).
// Aims the pursuit at the target's position in half tiles, or at its
// platform's route gate. False refuses a target outside the slots, a target
// without a model or a missing route gate.
bool RefreshAiTargetTile(MovementState& state, const MovementHost& host);

// decomp: func_800E48F8 (src/BATTLE/BATTLE.PRG/7BDD4.s:270).
// Refreshes the target tile while pursuit is requested.
bool RefreshAiTargetTileWhenTracking(MovementState& state,
                                     const MovementHost& host);

// The actor to pursue, the distance to stop at and the option the target
// approach update receives.
struct ApproachRequest {
  int target = 0;
  int distance = 0;
  int option = 0;
};

// decomp: func_800DB370 (src/BATTLE/BATTLE.PRG/72B70.s:11).
// Pursues the request's target to within its distance, via the target's
// platform route gate when it rides one. Returns the pursuit mode result once
// the approach moves, else 0. Nothing refuses a target outside the slots, a
// target without a model or a missing route gate.
std::optional<int> ApproachActor(MovementState& state, MovementHost& host,
                                 const ApproachRequest& request);

// decomp: func_800DB5B8 (src/BATTLE/BATTLE.PRG/72B70.s:192).
// Wanders within `radius` tiles of `tile`. The radius is passed on unsigned,
// as the original does.
void WanderAroundTile(MovementState& state, MovementHost& host,
                      std::uint32_t tile, int radius);

// decomp: func_800DB5B0 (src/BATTLE/BATTLE.PRG/72B70.s:182).
void WanderAroundSavedCenter(MovementState& state, MovementHost& host);

// decomp: func_800DB5DC (src/BATTLE/BATTLE.PRG/72B70.s:209);
// completion tail L800E5030 (src/BATTLE/BATTLE.PRG/7C7E0.s:34).
// Walks to `tile`, stopping once within `radius` world units of its centre.
// The radius is squared in 32 bits, as the original does.
void MoveToTile(MovementState& state, MovementHost& host, std::uint32_t tile,
                int radius);

// decomp: func_800DB5FC (src/BATTLE/BATTLE.PRG/72B70.s:225).
// Patrols the actor's waypoint row. False refuses a row outside the routes.
bool PatrolWaypoints(MovementState& state, MovementHost& host);

// decomp: func_800DB61C (src/BATTLE/BATTLE.PRG/72B70.s:239).
// Flees from the averaged position of every visible hostile actor, and keeps
// fleeing for 90 frames after losing sight of them. Returns the flee mode
// result, or 0 when there is nothing to flee. Nothing refuses an invalid
// battle actor list or a visible hostile without a model.
std::optional<int> FleeVisibleEnemies(MovementState& state, MovementHost& host,
                                      int kind);

// decomp: func_800DB74C (src/BATTLE/BATTLE.PRG/72B70.s:325).
// Turns to face `target`; returns the facing mode result once it turns,
// else 0. Nothing refuses a target outside the slots or without a model.
std::optional<int> TrackActorPosition(MovementState& state, MovementHost& host,
                                      int target);

// decomp: func_800DBB2C (src/BATTLE/BATTLE.PRG/72B70.s:620).
// The first other listed actor whose relationship class is `relation`, and
// its tile when `want_tile`. A relation outside 0-3 matches no actor, as in
// the original. Nothing refuses an invalid battle actor list or a matched
// actor without a model when its tile is wanted.
std::optional<RelatedActor> FindActorByRelationship(const MovementState& state,
                                                    const MovementHost& host,
                                                    int relation,
                                                    bool want_tile);

// decomp: func_800E437C (src/BATTLE/BATTLE.PRG/7B5DC.s:451).
// The Q12 cosine between facing `angle` and the horizontal direction from
// `from` to `to`; 4096 when `distance` is 0.
[[nodiscard]] int TargetFacingCosine(Core::Math::Position3 from,
                                     Core::Math::Position3 to, int angle,
                                     std::uint32_t distance);

// decomp: func_800D9DD8 (src/BATTLE/BATTLE.PRG/70D4C.s:627).
// Plans walking to the nearest waypoint when the best candidate's action is a
// movement action, otherwise plans moving in place. False refuses a missing
// best candidate.
bool SelectActionMovementMode(MovementState& state, MovementHost& host);

// decomp: func_800DC810 (src/BATTLE/BATTLE.PRG/73C84.s:290).
// Applies an action's reaction to every listed AI actor, then propagates the
// relationship changes. False refuses an invalid battle actor list.
bool ApplyActionRelationshipChanges(RelationshipChangeHost& host);
}  // namespace VS::Battle::Ai
