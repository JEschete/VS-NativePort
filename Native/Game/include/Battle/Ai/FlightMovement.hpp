#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

#include "Battle/Ai/FlightTerrain.hpp"
#include "Battle/Ai/NavigationPrimitives.hpp"
#include "Field/PlatformPlacement.hpp"
#include "Field/TerrainHeight.hpp"

// A flying actor's movement: testing the directions it can fly, switching
// between flight and ground movement under a low ceiling, hovering onto its
// destination or a target's tile, and gliding toward its destination over the
// ground in between. The battle runtime owns the actor AI state, the room's
// maps, the actor list and the movement primitives; these routines see the
// actor's flight fields and ask the world for the rest, so they reach no
// frame global or PS1 address.
namespace VS::Battle::Ai {
// The flight fields of an actor AI state.
struct FlightMover {
  // 0x00: set by the target approach when it closes in on its target.
  std::uint8_t close_approach = 0;
  std::uint8_t movement_disabled = 0;  // 0x07
  std::uint32_t tile = 0;              // 0x34
  std::uint32_t previous_tile = 0;     // 0x38
  std::int16_t x = 0;                  // 0x4C
  std::int16_t y = 0;                  // 0x4E
  std::int16_t z = 0;                  // 0x50
  // The model's flight flag; nothing without a model.
  std::optional<bool> flies;
  std::uint8_t actor_id = 0;          // 0x88
  std::uint16_t radius = 0;           // 0x92
  std::uint8_t max_range = 0;         // 0xB3
  std::int16_t body_height = 0;       // 0xB8
  std::int16_t ground_clearance = 0;  // 0xBC
  std::int16_t flight_height = 0;     // 0xBE
  std::uint8_t approach_target = 0;   // 0xE5: an actor ID
  std::uint16_t movement_flags = 0;   // 0x15E
  std::uint32_t destination = 0;      // 0x168
  std::int32_t move_x = 0;            // 0x17C
  std::int32_t move_z = 0;            // 0x180
  std::int32_t move_y = 0;            // 0x184
  std::int16_t heading = 0;           // 0x188
  std::int32_t stall_counter = 0;     // 0x228

  friend bool operator==(const FlightMover&, const FlightMover&) = default;
};

// The room AI frame's per-direction answers, one bit or byte per direction.
struct FlightDirectionCache {
  std::uint8_t fly_tested = 0;          // the flight test has run
  std::uint8_t fly_open = 0;            // and found the direction open
  std::uint8_t obstruction_tested = 0;  // the obstruction test has run
  // Its answer: 0 for none, 1 for a moving platform or an actor ID + 1.
  std::array<std::uint8_t, kFlightDirectionCount> obstruction{};

  friend bool operator==(const FlightDirectionCache&,
                         const FlightDirectionCache&) = default;
};

// A point in world units.
struct FlightPoint {
  std::int16_t x = 0;
  std::int16_t y = 0;
  std::int16_t z = 0;
};

// Another actor's AI state as an obstacle.
struct FlightObstacle {
  std::uint8_t actor_id = 0;     // 0x88: 0 for an inactive state
  std::uint16_t radius = 0;      // 0x92
  std::uint8_t max_range = 0;    // 0xB3
  std::int16_t body_height = 0;  // 0xB8
  // Its model's position; nothing without a model.
  std::optional<FlightPoint> position;
};

// The approach target's model.
struct FlightTargetRecord {
  std::int16_t y = 0;
  std::uint8_t tile_x = 0;
  std::uint8_t tile_z = 0;
  std::uint16_t collision_height = 0;
};

// The room AI state's walkable bounds and flight ceiling (0 for none).
struct FlightRoom {
  RoomTileBounds bounds;
  std::int32_t ceiling = 0;
};

// The vertical step a flying actor moves this frame and the command intent
// it moves with (func_800E4764).
struct FlightStep {
  std::int32_t step = 0;
  int intent = 0;
};

// The room, its actors and the movement primitives flight movement uses. Each
// call taking the mover may read and change it.
class FlightMovementWorld {
 public:
  virtual ~FlightMovementWorld() = default;
  [[nodiscard]] virtual NavigationDirectionCodes DirectionCodes() const = 0;
  [[nodiscard]] virtual FlightDirectionCache DirectionCache() const = 0;
  virtual void SetDirectionCache(const FlightDirectionCache& cache) = 0;
  // The moving platforms' contact points (func_8008D2C0).
  [[nodiscard]] virtual std::vector<Field::PlatformContact> PlatformPositions()
      const = 0;
  // Walks the battle actor list from its head.
  virtual void RewindActors() = 0;
  [[nodiscard]] virtual std::optional<int> NextActor() = 0;
  // An actor's AI state; nothing for an ID outside the AI slots or an empty
  // slot.
  [[nodiscard]] virtual std::optional<FlightObstacle> Obstacle(
      int actor_id) const = 0;
  // Nothing without the room's AI state.
  [[nodiscard]] virtual std::optional<FlightRoom> Room() const = 0;
  // A tile's navigation flags; nothing without a flag map or for a tile
  // outside it.
  [[nodiscard]] virtual std::optional<std::uint16_t> TileFlags(int x,
                                                               int z) const = 0;
  [[nodiscard]] virtual Field::TerrainGrid Terrain() const = 0;
  // An actor's model; nothing for an ID outside the actor table or an empty
  // entry.
  [[nodiscard]] virtual std::optional<FlightTargetRecord> TargetRecord(
      int actor_id) const = 0;
  // func_800E4764; nothing when the actor has no step.
  [[nodiscard]] virtual std::optional<FlightStep> SelectVerticalStep(
      FlightMover& mover) = 0;
  // func_800E3600; returns the frame's movement result.
  [[nodiscard]] virtual int ResolveDirectionalMovement(FlightMover& mover,
                                                       int angle, int mode) = 0;
  // func_800D954C: below zero when another actor holds the tile, zero when it
  // is blocked and above zero when it is free.
  [[nodiscard]] virtual int DestinationOccupancy(FlightMover& mover,
                                                 std::uint32_t tile) = 0;
  // func_800DEEFC.
  virtual void IssueCommand(FlightMover& mover, int intent) = 0;
  // func_800E2CCC.
  virtual void Recover(FlightMover& mover) = 0;
  // func_800E678C.
  virtual void ReleaseReservations(FlightMover& mover) = 0;
  // func_800E4B10.
  virtual void BeginTerrainMovement(FlightMover& mover) = 0;
  // func_800E4B18.
  virtual void BeginAlternateTerrainMovement(FlightMover& mover) = 0;
  // func_800E4B2C.
  virtual void MoveTowardDestinationTile(FlightMover& mover, int angle) = 0;
  // func_800E4B70.
  virtual void BeginGroundMovement(FlightMover& mover, int angle) = 0;
};

// decomp: func_800E02B4 (src/BATTLE/BATTLE.PRG/771A8.c:798).
// What blocks the direction (0-7): 1 for a moving platform, an actor ID + 1
// for another active actor, 0 for nothing; the answer is cached. The probe is
// the actor's position, moved its maximum range along the direction and
// ignoring anything behind it when `ahead`. Nothing refuses a direction
// outside 0-7, or an actor without an AI state or model.
[[nodiscard]] std::optional<int> FlightDirectionObstruction(
    FlightMovementWorld& world, const FlightMover& mover, int direction,
    bool ahead);

// decomp: func_800E0678 (src/BATTLE/BATTLE.PRG/771A8.c:904).
// Whether the actor can fly one tile in the direction (0-7): nothing ahead
// blocks it, the tile is inside the room, unflagged, not the previous tile,
// and the actor's flight height keeps it below a ceiling. The answer is
// cached, except a ceiling refusal. Nothing refuses a direction outside 0-7,
// a missing room AI state, or a tile outside the flag map or terrain.
[[nodiscard]] std::optional<bool> CanFlyInDirection(FlightMovementWorld& world,
                                                    const FlightMover& mover,
                                                    int direction);

// decomp: func_800E0918 (src/BATTLE/BATTLE.PRG/771A8.c:985).
// Moves an actor between flight and the ground: under a ceiling too low to
// fly, a flying actor on a free tile lands (BeginTerrainMovement) and a
// walking one keeps walking. A walking actor that is `toward_destination`
// and near its destination walks on. Returns whether the actor changed
// movement, nothing when the world refuses.
[[nodiscard]] std::optional<bool> UpdateFlightGroundTransition(
    FlightMovementWorld& world, FlightMover& mover, int angle,
    bool toward_destination);

// decomp: func_800E0A68 (src/BATTLE/BATTLE.PRG/771A8.c:1064).
// Flies toward the destination: hovering onto an approach target's flagged
// tile, settling over the destination, following the heading when the way is
// blocked, or gliding over the ground between, climbing first when it rises.
// The original divided by zero without trapping (#342); the R3000 quotients
// are kept. Returns false when the world refuses; the mover may then be
// partly updated.
[[nodiscard]] bool UpdateFlightMovement(FlightMovementWorld& world,
                                        FlightMover& mover, int angle);

// decomp: func_800E1238 (src/BATTLE/BATTLE.PRG/771A8.c:1278).
// Flies one step along `angle`, or the heading when the way is blocked,
// descending when every reachable neighbour is lower. Returns false when the
// world refuses; the mover may then be partly updated.
[[nodiscard]] bool UpdateFlightDirectionalMovement(FlightMovementWorld& world,
                                                   FlightMover& mover,
                                                   int angle);
}  // namespace VS::Battle::Ai
