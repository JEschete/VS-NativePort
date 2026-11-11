#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>
#include <optional>

#include "Battle/Ai/NavigationPrimitives.hpp"

// An idle actor's wander: alternating rest and move periods in which it
// either roams along a random heading (wide ranges) or walks a chain of
// random neighbouring tiles (narrow ranges). The battle runtime owns the
// actor AI state, the room's tile maps and the movement primitives; these
// routines see the actor's wander fields and ask the world for the rest, so
// they reach no frame global or PS1 address.
namespace VS::Battle::Ai {
// The wander fields of an actor AI state.
struct WanderActor {
  std::uint8_t is_stuck = 0;            // 0x02
  std::uint8_t height_filter = 0;       // 0x03
  std::uint8_t stayed_on_tile = 0;      // 0x08: set by perception
  std::uint32_t tile = 0;               // 0x34
  std::uint32_t saved_destination = 0;  // 0x48
  std::int16_t x = 0;                   // 0x4C
  std::int16_t z = 0;                   // 0x50
  bool flies = false;                   // the model's flight flag
  std::uint32_t move_distance = 0;      // 0xB4
  std::uint16_t phase = 0;              // 0xC0
  std::uint16_t timer = 0;              // 0xC2
  std::int32_t range_squared = 0;       // 0xC4
  std::int16_t heading = 0;             // 0xD0
  std::uint16_t cooldown = 0;           // 0xD2
  std::uint16_t rest_period = 0;        // 0xD4
  std::uint16_t move_period = 0;        // 0xD6
  std::uint8_t rate = 0;                // 0xD8: 1 in 2^rate restarts
  std::uint32_t goal = 0;               // 0x11C
  std::uint8_t behavior_mode = 0;       // 0x12D
  std::uint8_t count = 0;               // 0x15D: tiles walked
  std::uint16_t movement_flags = 0;     // 0x15E
  std::uint32_t destination = 0;        // 0x168

  friend bool operator==(const WanderActor&, const WanderActor&) = default;
};

// The room's walkable tile bounds (D_800F58BC), inclusive.
struct WanderRoomBounds {
  std::uint8_t min_x = 0;
  std::uint8_t min_z = 0;
  std::uint8_t max_x = 0;
  std::uint8_t max_z = 0;
};

// The room, the actor random stream and the movement primitives the wander
// uses. Each call taking the actor may read and change it.
class WanderWorld {
 public:
  virtual ~WanderWorld() = default;
  [[nodiscard]] virtual NavigationDirectionCodes DirectionCodes() const = 0;
  // Whether the room has route data; its terrain regions then hold each
  // tile's blocked directions.
  [[nodiscard]] virtual bool HasRoomData() const = 0;
  // Nothing outside the room's regions.
  [[nodiscard]] virtual std::optional<std::uint8_t> TerrainRegionCell(
      int x, int z) const = 0;
  // Nothing outside the 32x32 grid or in a room without tile flags.
  [[nodiscard]] virtual std::optional<std::uint16_t> TileFlags(int x,
                                                               int z) const = 0;
  [[nodiscard]] virtual std::optional<WanderRoomBounds> RoomBounds() const = 0;
  // A draw masked by range - 1 (func_800E45D4).
  virtual int NextRandomMasked(int range) = 0;
  // func_800E4690.
  [[nodiscard]] virtual bool IsAdjacentStepBlocked(std::uint32_t tile,
                                                   int direction) = 0;
  // func_800D96C8.
  [[nodiscard]] virtual bool CanTraverseEdge(WanderActor& actor,
                                             std::uint32_t from,
                                             std::uint32_t to,
                                             int direction) = 0;
  // func_800E0678.
  [[nodiscard]] virtual bool CanFlyInDirection(WanderActor& actor,
                                               int direction) = 0;
  // func_800D9B6C.
  [[nodiscard]] virtual bool CanWalkInDirection(WanderActor& actor,
                                                int direction) = 0;
  // Invalidates the frame's walk direction cache (0x1F8003D0).
  virtual void ClearDirectionCache() = 0;
  // func_800E1388.
  virtual void ProjectClearRadialDestination(WanderActor& actor,
                                             std::uint32_t source, int angle,
                                             std::uint32_t distance) = 0;
  // func_800E50A0.
  virtual void DispatchLocomotion(WanderActor& actor, int kind, int angle) = 0;
  // func_800E2CCC.
  virtual void RecoverMovement(WanderActor& actor) = 0;
};

// decomp: wanderDirection (src/BATTLE/BATTLE.PRG/771A8.c:1633).
// The tile step of direction `index`. Nothing refuses an index outside 0..7.
[[nodiscard]] std::optional<NavigationStep> WanderDirectionStep(
    const NavigationDirectionCodes& directions, int index);

// How choosing the next wander tile ends.
enum class WanderTileChoice : std::int8_t {
  kBlocked = -1,  // no neighbour to walk to
  kExtended = 0,  // the goal moved one tile
  kComplete = 1,  // the goal moved and the chain is longer than the range
};

// decomp: func_800E1BB8 (src/BATTLE/BATTLE.PRG/771A8.c:1638).
// Extends the actor's wander chain: from the goal (the tile, when the chain
// is empty) it draws one of the even directions to a neighbour that is not
// blocked, keeps the goal's area unless the actor is stuck and has no 0x20
// tile flag. It redraws once a draw that would turn straight back, moves the
// goal there and counts the tile. A zero range is blocked. Nothing refuses a
// goal or neighbour outside the regions or the tile grid, which the original
// read past; the actor may then be partly updated.
[[nodiscard]] std::optional<WanderTileChoice> ChooseNextWanderTile(
    WanderWorld& world, WanderActor& actor, std::uint32_t tile,
    std::uint32_t range);

// decomp: wanderRandomHeading (src/BATTLE/BATTLE.PRG/771A8.c:1700).
// A heading within a quarter of a direction either side of `direction`'s
// centre, in 4096-per-turn units wrapped to a signed halfword: direction 0
// gives -256..255.
[[nodiscard]] std::int16_t RandomWanderHeading(WanderWorld& world,
                                               int direction);

// decomp: wanderStartWalk (src/BATTLE/BATTLE.PRG/771A8.c:1708).
// Starts a new tile chain: no heading, no tiles, a 60-tick cooldown and the
// next phase, then the movement recovery.
void StartWanderWalk(WanderWorld& world, WanderActor& actor);

// decomp: func_800E1EAC (src/BATTLE/BATTLE.PRG/771A8.c:1717).
// One wander tick for an actor in the area around `tile` within `range`. The
// timer alternates move and rest periods (and holds an actor in behaviour
// mode 1 for 30 ticks); while resting the actor recovers. Ranges of 63 and
// more roam along a random heading, restarted when the cooldown runs out,
// one in 2^rate ticks or at the first tick; the cooldown runs out when the
// heading leads outside the room bounds or onto a 0x800 tile. Narrower
// ranges walk tile chains (ChooseNextWanderTile) to their goal.
// The original read the roaming heading's direction byte before the codes
// (0x1F8003EB, a stale register save) for a negative heading; this uses
// direction 7, which the heading points along. Returns false when the room
// bounds, tile flags or a wander tile choice refuse; the actor may then be
// partly updated.
[[nodiscard]] bool UpdateWanderMovement(WanderWorld& world, WanderActor& actor,
                                        std::uint32_t tile,
                                        std::uint32_t range);
}  // namespace VS::Battle::Ai
