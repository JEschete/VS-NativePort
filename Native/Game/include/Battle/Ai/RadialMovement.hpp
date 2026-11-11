#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>
#include <optional>

#include "Battle/Ai/ActorCommands.hpp"
#include "Battle/Ai/NavigationPrimitives.hpp"
#include "Field/TerrainHeight.hpp"

// An actor's radial movement: projecting a destination along an angle until a
// tile is free, walking to it (or away from a reference), and recovering when
// the actor cannot move by turning toward its target, its point or the
// highest neighbouring ground. The battle runtime owns the actor AI state,
// the room's maps and the movement primitives; these routines see the actor's
// movement fields and ask the world for the rest, so they reach no frame
// global or PS1 address.
namespace VS::Battle::Ai {
// The radial movement fields of an actor AI state.
struct RadialMover {
  std::uint8_t toward_point = 0;  // 0x09: recover toward (target_x, target_z)
  // 0x0D: walk straight to the destination; set when its projection fell
  // back onto the actor's tile.
  std::uint8_t approach_directly = 0;
  std::uint32_t tile = 0;               // 0x34
  std::uint32_t saved_destination = 0;  // 0x48: bit 23 set when none
  std::int16_t x = 0;                   // 0x4C
  std::int16_t z = 0;                   // 0x50
  bool flies = false;                   // the model's flight flag
  std::int16_t model_look_yaw = 0;      // the model's +0x14
  std::int16_t model_facing = 0;        // the model's +0x26
  std::uint16_t move_attempts = 0;      // 0x8C
  std::int32_t move_distance = 0;       // 0xB4
  std::uint16_t turn_timer = 0;         // 0x13A
  std::uint16_t turn_period = 0;        // 0x13C
  std::int16_t target_x = 0;            // 0x158
  std::int16_t target_z = 0;            // 0x15A
  std::uint8_t movement_target = 0;     // 0x15C: an actor ID, 16 for none
  std::uint16_t movement_flags = 0;     // 0x15E
  std::uint32_t destination = 0;        // 0x168
  std::uint32_t next_tile = 0;          // 0x170
  std::int32_t move_x = 0;              // 0x17C
  std::int32_t move_z = 0;              // 0x180
  std::int16_t heading = 0;             // 0x188
  std::int16_t angle = 0;               // 0x18A
  std::int32_t stall_counter = 0;       // 0x228

  friend bool operator==(const RadialMover&, const RadialMover&) = default;
};

// A point in world units.
struct RadialPoint {
  std::int16_t x = 0;
  std::int16_t z = 0;
};

// The room, its actors and the movement primitives radial movement uses. Each
// call taking the mover may read and change it.
class RadialMovementWorld {
 public:
  virtual ~RadialMovementWorld() = default;
  [[nodiscard]] virtual NavigationDirectionCodes DirectionCodes() const = 0;
  // Nothing without the room's AI state.
  [[nodiscard]] virtual std::optional<RoomTileBounds> RoomBounds() const = 0;
  // The room's tile grid: width in bits 0-7, depth in bits 8-15.
  [[nodiscard]] virtual std::uint32_t GridBounds() const = 0;
  [[nodiscard]] virtual Field::TerrainGrid Terrain() const = 0;
  // Whether the actor may step onto the tile: below zero when another actor
  // holds it, zero when it is blocked and above zero when it is free
  // (func_800D954C). Nothing refuses.
  [[nodiscard]] virtual std::optional<int> DestinationOccupancy(
      RadialMover& mover, std::uint32_t tile) = 0;
  // An actor's model position; nothing for an ID outside the actor slots or
  // an empty slot.
  [[nodiscard]] virtual std::optional<RadialPoint> ActorPosition(
      int actor_id) const = 0;
  // func_800DEEFC.
  virtual void IssueCommand(RadialMover& mover, CommandIntent intent) = 0;
  // func_800E4B2C.
  virtual void MoveTowardDestinationTile(RadialMover& mover, int angle) = 0;
  // func_800E678C.
  virtual void ReleaseReservations(RadialMover& mover) = 0;
  // func_800E4B10.
  virtual void BeginTerrainMovement(RadialMover& mover) = 0;
};

// decomp: func_800E1388 (src/BATTLE/BATTLE.PRG/771A8.c:1330).
// Sets the destination to the tile `distance` units from `source` along
// `angle`, shortened by an eighth up to seven times until the tile is free,
// then clamped to the room bounds. The original kept the tag and top bytes of
// the caller's register s0; these are zero (#341). Returns false when the
// world refuses; the destination is then unchanged.
[[nodiscard]] bool ProjectClearRadialDestination(RadialMovementWorld& world,
                                                 RadialMover& mover,
                                                 std::uint32_t source,
                                                 int angle,
                                                 std::uint32_t distance);

// decomp: func_800E153C (src/BATTLE/BATTLE.PRG/771A8.c:1389).
// Walks toward the destination: one farther than the move distance is
// reprojected from the actor's tile at that distance along the angle between
// them, and actors projecting their destination step straight there when it
// is near or direct. Otherwise the sixth attempt walks to the destination or
// a free axis neighbour of it; anything else recovers (RecoverMovement).
// `angle` is unused, as in the original. Returns false when the world
// refuses; the mover may then be partly updated.
[[nodiscard]] bool MoveToRadialDestination(RadialMovementWorld& world,
                                           RadialMover& mover, int angle);

// decomp: func_800E1764 (src/BATTLE/BATTLE.PRG/771A8.c:1437).
// Projects destinations along `angle` and its opposite, keeps the one farther
// from the move point (the opposite one on a tie) and walks to it
// (MoveToRadialDestination). Returns false when the world refuses; the mover
// may then be partly updated.
[[nodiscard]] bool MoveAwayFromReference(RadialMovementWorld& world,
                                         RadialMover& mover, int angle);

// decomp: func_800E2CCC (src/BATTLE/BATTLE.PRG/771A8.c:2272).
// Recovers an actor that cannot move. A flying actor stalled for more than
// 120 calls on a free tile drops its reservations and starts terrain
// movement. Otherwise it turns toward its movement target, its point, or the
// axis neighbour inside the grid with the highest ground, nearest its facing
// first; with none it stands. Returns false for a movement target without a
// model, or a neighbour outside the terrain; the mover may then be partly
// updated.
[[nodiscard]] bool RecoverMovement(RadialMovementWorld& world,
                                   RadialMover& mover);
}  // namespace VS::Battle::Ai
