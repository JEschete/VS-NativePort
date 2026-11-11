#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "Battle/Ai/NavigationPrimitives.hpp"
#include "Field/TerrainHeight.hpp"

// Battle AI ground navigation: tile-edge and occupancy tests, directional
// walk probes, detour planning, hesitation turns and the locomotion entry
// points that hand a chosen step to the movement code. The battle runtime owns
// the actor AI state, room maps and movement code; these routines see them
// only through GroundNavigator and GroundNavigationWorld.
namespace VS::Battle::Ai {
inline constexpr std::size_t kMovementModeCounterCount = 13;

// The movement fields of one actor's AI state.
struct GroundNavigator {
  std::uint8_t actor_id = 0;
  std::uint8_t gait = 0;
  std::uint8_t is_stuck = 0;
  std::uint8_t height_filter = 0;
  std::uint8_t near_target_detour = 0;
  std::uint8_t target_distance_filter = 0;
  std::uint8_t ignore_floor = 0;
  std::uint8_t movement_disabled = 0;
  std::uint8_t toward_point = 0;
  std::uint8_t facing_locked = 0;
  std::uint8_t height_filtered_movement = 0;
  std::int8_t movement_enabled = 0;
  std::uint8_t area_lock = 0;
  std::uint8_t terrain_reject_mask = 0;
  std::uint32_t tile = 0;
  std::int32_t previous_tile = 0;
  std::int16_t x = 0;
  std::int16_t y = 0;
  std::int16_t z = 0;
  std::uint8_t requested_movement_mode = 0;
  std::uint16_t collision_radius = 0;
  std::uint16_t step_height = 0;
  std::uint8_t tile_step_limit = 0;
  std::uint8_t max_range = 0;
  std::int16_t body_height = 0;
  std::uint8_t fine_x = 0;
  std::uint8_t fine_z = 0;
  std::uint8_t locomotion_mode = 0;
  std::uint8_t ground_area = 0;
  std::uint16_t turn_timer = 0;
  std::uint8_t area_exception = 0;
  std::uint16_t movement_wait = 0;
  std::int16_t target_x = 0;
  std::int16_t target_z = 0;
  std::uint8_t movement_target = 0;
  std::uint16_t movement_flags = 0;
  std::uint32_t destination = 0;
  std::uint32_t next_tile = 0;
  std::int32_t move_x = 0;
  std::int32_t move_z = 0;
  std::int16_t heading = 0;
  std::uint32_t navigation_flags = 0;
  std::uint32_t detour_tile = 0;
  std::uint8_t detour_repeats = 0;
  std::uint8_t detour_run = 0;
  std::uint8_t repeat_wait = 0;
  std::uint8_t route_gate = 0;
  std::uint32_t route_exit = 0;
  std::uint16_t locomotion_flags = 0;
  std::uint8_t walk_step = 0;
  std::uint8_t run_step = 0;
  std::int32_t stall_counter = 0;
  std::array<std::uint8_t, kMovementModeCounterCount> mode_counters{};
  std::uint8_t reaction_pending = 0;
  // The approach target and range, read where the approach plan stores
  // them; the navigation routines never write them.
  std::uint8_t approach_target_id = 0;
  std::int32_t approach_range = 0;
  // The target approach's pursuit progress flags (AI state 0xE4, 0xE7 and
  // 0xE8, PursuitProgress); a detour that leaves the room clears them.
  std::uint8_t pursuit_e4 = 0;
  std::uint8_t pursuit_e7 = 0;
  std::uint8_t pursuit_e8 = 0;
};

// Another actor's movement flags and detour wait (AI state 0x15E and 0x19A).
struct ActorDetourWait {
  std::uint16_t movement_flags = 0;
  std::uint8_t repeat_wait = 0;
};

// Another battle actor's AI collision record and model placement.
struct GroundCollisionActor {
  bool has_ai_state = false;
  bool model_moving = false;
  std::int16_t model_x = 0;
  std::int16_t model_y = 0;
  std::int16_t model_z = 0;
  std::uint16_t model_radius = 0;
  std::uint16_t model_height = 0;
  std::int16_t planned_x = 0;
  std::int16_t planned_y = 0;
  std::int16_t planned_z = 0;
  std::uint16_t collision_radius = 0;
  std::uint8_t reach = 0;
  std::int16_t body_height = 0;
};

// An actor model's world position on the ground plane.
struct GroundPoint {
  std::int16_t x = 0;
  std::int16_t z = 0;
};

// The room grid, per-frame direction caches, battle actor list, models,
// random stream and the movement code. Each movement operation receives the
// navigator, which the host stores before and reloads after the movement code
// runs.
class GroundNavigationWorld {
 public:
  virtual ~GroundNavigationWorld() = default;
  // Room bounds: width in bits 0-7, height in bits 8-15.
  [[nodiscard]] virtual std::uint32_t RoomBounds() const = 0;
  // The room AI state's walkable tile bounds; nothing without a room AI
  // state.
  [[nodiscard]] virtual std::optional<RoomTileBounds> RoomTileLimits()
      const = 0;
  // Whether the room's tile flag and height maps are loaded.
  [[nodiscard]] virtual bool HasTileMaps() const = 0;
  [[nodiscard]] virtual std::uint16_t TileFlags(int x, int z) const = 0;
  [[nodiscard]] virtual Field::TerrainGrid Terrain() const = 0;
  [[nodiscard]] virtual NavigationDirectionCodes DirectionCodes() const = 0;
  [[nodiscard]] virtual bool HasRoomData() const = 0;
  // The tile's blocked-direction bits; nothing without region data.
  [[nodiscard]] virtual std::optional<std::uint8_t> TerrainRegionCell(
      int x, int z) const = 0;

  [[nodiscard]] virtual std::uint8_t WalkValid() const = 0;
  virtual void SetWalkValid(std::uint8_t bits) = 0;
  [[nodiscard]] virtual std::uint8_t WalkPassable() const = 0;
  virtual void SetWalkPassable(std::uint8_t bits) = 0;
  [[nodiscard]] virtual std::uint8_t BlockerValid() const = 0;
  virtual void SetBlockerValid(std::uint8_t bits) = 0;
  [[nodiscard]] virtual std::uint8_t BlockerActor(int direction) const = 0;
  virtual void SetBlockerActor(int direction, std::uint8_t actor) = 0;

  // The battle actor list from its head; each call yields the next actor's
  // ID, nothing at the end.
  virtual void RewindCollisionActors() = 0;
  [[nodiscard]] virtual std::optional<int> NextCollisionActor() = 0;
  // Nothing for an actor without a model.
  [[nodiscard]] virtual std::optional<GroundCollisionActor> CollisionActor(
      int actor_id) const = 0;
  [[nodiscard]] virtual std::optional<std::int16_t> ModelFacing(
      int actor_id) const = 0;
  [[nodiscard]] virtual std::optional<GroundPoint> ModelPosition(
      int actor_id) const = 0;
  // Another actor's detour wait; nothing for an actor without an AI state.
  [[nodiscard]] virtual std::optional<ActorDetourWait> OtherActorDetourWait(
      int actor_id) const = 0;
  virtual void SetOtherActorRepeatWait(int actor_id, std::uint8_t wait) = 0;

  // The actor random stream: a value below range (a power of two).
  virtual int NextRandomMasked(int range) = 0;
  // Hesitation length for kinds 1..3; 0 before the table loads.
  [[nodiscard]] virtual std::uint8_t HesitationLength(int kind) const = 0;

  // The room's contact occupancy bits; none without room AI state.
  [[nodiscard]] virtual bool HasRoomAiState() const = 0;
  [[nodiscard]] virtual std::uint8_t ContactMask() const = 0;
  virtual void SetContactMask(std::uint8_t mask) = 0;

  [[nodiscard]] virtual bool HasRouteGates() const = 0;
  // Index of the route link leaving at the tile, or negative.
  virtual int FindRouteGateAtTile(GroundNavigator& navigator,
                                  std::uint32_t tile) = 0;
  // The first end of a route link below 8.
  [[nodiscard]] virtual RouteGateEnd RouteLinkStart(int link) const = 0;

  virtual void Recover(GroundNavigator& navigator) = 0;
  virtual void IssueCommand(GroundNavigator& navigator, int intent) = 0;
  virtual void UpdateNavigationAreaMovement(GroundNavigator& navigator) = 0;
  virtual void MoveWithHeightFilteredDirections(GroundNavigator& navigator) = 0;
  virtual void UpdateFlightMovement(GroundNavigator& navigator, int angle) = 0;
  virtual void UpdateFlightDirectionalMovement(GroundNavigator& navigator,
                                               int angle) = 0;
  virtual void MoveToRadialDestination(GroundNavigator& navigator,
                                       int angle) = 0;
  virtual void MoveAwayFromReference(GroundNavigator& navigator, int angle) = 0;
  virtual void RefreshMovementCooldown(GroundNavigator& navigator) = 0;
  virtual std::uint32_t FindAdjacentMovementDestination(
      GroundNavigator& navigator, int first, int second,
      std::uint32_t angle) = 0;
  virtual int CanFlyInDirection(GroundNavigator& navigator, int direction) = 0;
};

// A walk or flight probe of one of the eight directions; nothing refuses.
class GroundDirectionProbe {
 public:
  virtual ~GroundDirectionProbe() = default;
  [[nodiscard]] virtual std::optional<int> Probe(GroundNavigator& navigator,
                                                 int direction) = 0;
};

// decomp: func_800D954C (src/BATTLE/BATTLE.PRG/70D4C.s:12).
// 1 when the tile is free, -1 when another actor stands on it and 0 for the
// previous tile, a tile outside the room or a blocked cell. Nothing refuses a
// room without bounded maps or an invalid collision actor list.
[[nodiscard]] std::optional<int> DestinationOccupancy(
    GroundNavigationWorld& world, const GroundNavigator& navigator,
    std::uint32_t tile);

// decomp: func_800D96C8 (src/BATTLE/BATTLE.PRG/70D4C.s:123).
// 1 when the step from one tile to the next is walkable and closer to the
// destination, -1 inside the approach radius and 0 otherwise. Nothing refuses
// a direction outside 0..7, a room without bounded maps, an invalid collision
// actor list or an approach target without a model.
[[nodiscard]] std::optional<int> CanTraverseTileEdge(
    GroundNavigationWorld& world, const GroundNavigator& navigator,
    std::uint32_t from, std::uint32_t to, int direction);

// decomp: func_800D98E8 (src/BATTLE/BATTLE.PRG/70D4C.s:279).
// One more than the ID of the first actor ahead in the direction, cached per
// frame; 0 when none. Nothing refuses as CanTraverseTileEdge.
[[nodiscard]] std::optional<int> DirectionalBlockingActor(
    GroundNavigationWorld& world, const GroundNavigator& navigator,
    int direction);

// decomp: D_800D9B6C (src/BATTLE/BATTLE.PRG/70D4C.s:458).
// Whether the actor can walk a fine step in the direction, cached per frame.
// Nothing refuses as CanTraverseTileEdge or a missing region cell.
[[nodiscard]] std::optional<bool> CanWalkInDirection(
    GroundNavigationWorld& world, const GroundNavigator& navigator,
    int direction);

// decomp: func_800DB93C (src/BATTLE/BATTLE.PRG/72B70.s:483).
// The first tile past a gap in the direction with its edge status in bits
// 24-31, the tile itself when it is no gap and 0 outside the room. Nothing
// refuses as CanTraverseTileEdge.
[[nodiscard]] std::optional<std::uint32_t> FindGapLandingTile(
    GroundNavigationWorld& world, const GroundNavigator& navigator,
    std::uint32_t tile, int direction, std::uint32_t from);

// decomp: func_800E3BC8 (src/BATTLE/BATTLE.PRG/7B3C8.s:11).
// Stores the next tile toward the destination as the detour boundary.
void SetDetourBoundary(GroundNavigator& navigator);

// decomp: func_800E3CDC (src/BATTLE/BATTLE.PRG/7B3C8.s:90).
// Counts repeat visits, waits after five and returns the flags with a fresh
// detour step budget.
[[nodiscard]] std::uint32_t RefreshDetourProgress(GroundNavigationWorld& world,
                                                  GroundNavigator& navigator,
                                                  std::uint32_t flags);

// decomp: func_800E3D5C (src/BATTLE/BATTLE.PRG/7B55C.s:9).
// The four cardinal probe results, direction 0 in bit 3. Nothing when a probe
// refuses.
[[nodiscard]] std::optional<int> CardinalPassabilityMask(
    GroundNavigator& navigator, GroundDirectionProbe& probe);

// decomp: func_800E3DDC (src/BATTLE/BATTLE.PRG/7B5DC.s:10).
// Restarts a spent or stopped detour with a random turn side and stores the
// cardinal mask in bits 18-21. Nothing refuses a refused probe or hesitation.
[[nodiscard]] std::optional<std::uint32_t> InitializeObstacleDetour(
    GroundNavigationWorld& world, GroundNavigator& navigator,
    std::uint32_t flags, GroundDirectionProbe& probe);

// decomp: func_800E2F5C (src/BATTLE/BATTLE.PRG/771A8.c:2388).
// Follows an active obstacle detour (navigation_flags) one step around the
// obstacle: 5 keeps walking the detour heading, 14 steps to next_tile and -1
// ends this step, setting the stop bit when the detour is spent or leaves the
// room. Nudges an actor blocking the turn side to wait. The probe is the walk
// or flight test; a side tile whose flag bit `blocked_flag_bit` is set ends
// the detour. Nothing refuses a refused probe, direction or tile query, a room
// without AI bounds or tile maps, or a blocking actor ID outside the table.
[[nodiscard]] std::optional<int> FollowObstacleDetour(
    GroundNavigationWorld& world, GroundNavigator& navigator,
    GroundDirectionProbe& probe, int blocked_flag_bit);

// decomp: func_800E4180 (src/BATTLE/BATTLE.PRG/7B5DC.s).
// Starts or continues a hesitation turn of kind 1 or 2 (3 only polls) and
// returns 1 while two or more turns remain. Nothing refuses a slot outside
// 0..6, a kind outside 1..3 or an unloaded length table.
[[nodiscard]] std::optional<int> UpdateHesitationTurn(
    GroundNavigationWorld& world, GroundNavigator& navigator, int chance,
    int kind, int slot);

// The vertical step a gait configures and its movement code.
struct VerticalMovementStep {
  bool selected = false;
  int value = 0;
  int code = 0;
};

// decomp: func_800E4764 (src/BATTLE/BATTLE.PRG/7BDD4.s:145).
// The configured climb or descent step, preferring the current gait. Nothing
// refuses a running actor without a configured step.
[[nodiscard]] std::optional<VerticalMovementStep>
SelectActorVerticalMovementStep(const GroundNavigator& navigator);

// decomp: func_800E47E4 (src/BATTLE/BATTLE.PRG/7BDD4.s:189).
// Whether the tile lies behind the actor's facing or its movement target.
// Nothing refuses an actor ID or target without a model.
[[nodiscard]] std::optional<bool> IsTileBehindActorTarget(
    const GroundNavigationWorld& world, const GroundNavigator& navigator,
    std::uint32_t tile);

// decomp: func_800E49D8 (src/BATTLE/BATTLE.PRG/7BDD4.s:342).
// The first lower tile within the step count along the direction, kept only
// when its edge and occupancy pass; 0 otherwise. Nothing refuses as
// CanTraverseTileEdge or a current tile outside the terrain grid.
[[nodiscard]] std::optional<std::uint32_t> FindAiDescentTile(
    GroundNavigationWorld& world, const GroundNavigator& navigator,
    std::uint32_t tile, int direction, int steps);

// decomp: func_800E4BD8 (src/BATTLE/BATTLE.PRG/7BDD4.s:527).
// Clears a contact occupancy bit (the bit number modulo 32 of an 8-bit mask).
void ClearAiContactOccupancy(GroundNavigationWorld& world, int bit);

// decomp: func_800E4BE0 (src/BATTLE/BATTLE.PRG/7BDD4.s:533).
// Sets a contact occupancy bit.
void SetAiContactOccupancy(GroundNavigationWorld& world, int bit);

// decomp: func_800E4B10 (src/BATTLE/BATTLE.PRG/7BDD4.s:431).
// Holds the current tile and issues the terrain movement intent.
void BeginAiTerrainMovement(GroundNavigationWorld& world,
                            GroundNavigator& navigator);

// decomp: func_800E4B18 (src/BATTLE/BATTLE.PRG/7BDD4.s:438).
// Holds the current tile and issues the alternate terrain movement intent.
void BeginAiAlternateTerrainMovement(GroundNavigationWorld& world,
                                     GroundNavigator& navigator);

// decomp: func_800E4B2C (src/BATTLE/BATTLE.PRG/7BDD4.s:449).
// Continues a route link, recovers at the destination or moves toward the
// destination tile's centre. False refuses a link without route gates, a
// link index outside 0..7 or a refused occupancy test.
bool MoveActorTowardDestinationTile(GroundNavigationWorld& world,
                                    GroundNavigator& navigator);

// decomp: func_800E4B70 (src/BATTLE/BATTLE.PRG/7BDD4.s:473).
// Height-filtered movement, route continuation or a directional ground move
// at the angle. False refuses as MoveActorTowardDestinationTile or
// ResolveDirectionalMovement.
bool BeginAiGroundMovement(GroundNavigationWorld& world,
                           GroundNavigator& navigator, int angle);

// decomp: func_800E50A0 (src/BATTLE/BATTLE.PRG/7C7E0.s:66).
// Ground, flight or radial movement for the locomotion mode, toward the tile
// (kind 0) or along the angle (kind 1). False refuses a mode outside 0..2, a
// kind outside 0..1 or a refused ground move.
bool DispatchAiLocomotionMode(GroundNavigationWorld& world,
                              GroundNavigator& navigator, int kind, int angle);

// decomp: func_800E4FE0 (src/BATTLE/BATTLE.PRG/7C7E0.s:10).
// Targets the move point with the gait and dispatches a heading move toward
// it. False refuses as DispatchAiLocomotionMode.
bool BeginAiMovementTowardPoint(GroundNavigationWorld& world,
                                GroundNavigator& navigator, int kind);

// decomp: func_800E3600 (src/BATTLE/BATTLE.PRG/771A8.c:2549;
// nonmatchings/771A8/func_800E3600.s).
// Walks or flies (mode 2-3) toward the angle: follows an active detour, steps
// down a descent, walks the heading or a cardinal, takes an adjacent tile or
// starts a detour; modes 1 and 3 detour only near the target. False refuses a
// mode outside 0..3 or a refused probe, descent, detour or hesitation.
bool ResolveDirectionalMovement(GroundNavigationWorld& world,
                                GroundNavigator& navigator, int angle,
                                int mode);
}  // namespace VS::Battle::Ai
