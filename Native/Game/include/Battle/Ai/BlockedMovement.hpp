#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "Battle/Ai/NavigationPrimitives.hpp"

// Blocked movement: the tile an actor steps to next to its own, the stall
// test over its recent positions, the saved movement random state and the
// detour around a tile it cannot leave. The battle runtime owns the actor AI
// state, the AI frame and the movement primitives; these routines see the
// fields they use and ask the world for the rest, so they reach no frame
// global or PS1 address.
namespace VS::Battle::Ai {
// The movement primitives FindAdjacentMovementDestination asks. Nothing
// refuses.
class AdjacentDestinationWorld {
 public:
  virtual ~AdjacentDestinationWorld() = default;
  [[nodiscard]] virtual NavigationDirectionCodes DirectionCodes() const = 0;
  // func_800D96C8: above zero for a walkable step, below zero inside the
  // approach radius and zero otherwise.
  [[nodiscard]] virtual std::optional<int> CanTraverseTileEdge(
      std::uint32_t from, std::uint32_t to, int direction) = 0;
  // func_800D954C: above zero for a free tile, below zero for one another
  // actor holds and zero for a blocked one.
  [[nodiscard]] virtual std::optional<int> DestinationOccupancy(
      std::uint32_t tile) = 0;
  // func_800DB93C: the first tile past a gap with its status in bits 24-31.
  [[nodiscard]] virtual std::optional<std::uint32_t> FindGapLandingTile(
      std::uint32_t tile, int direction, std::uint32_t from) = 0;
};

// The actor fields FindAdjacentMovementDestination reads.
struct AdjacentMover {
  bool movement_disabled = false;  // 0x07
  std::uint32_t tile = 0;          // 0x34
};

// decomp: func_800D8400 (src/BATTLE/BATTLE.PRG/6FC00.c:430).
// The tile next to the actor for a step: with equal `first` and `second` in
// the direction `direction`, otherwise in the axis direction nearest the
// angle `direction` and then in `first`, or `second` when `first` is that
// axis. A walkable edge returns the neighbour when it is free, a blocked
// result when another actor holds it and 0 when it is blocked; an edge
// inside the approach radius is blocked, and any other edge tries the gap
// landing, which returns a found landing, blocks on another status and
// otherwise goes on to the next direction. A blocked result has status 2 in
// bits 24-31; every result clears the tag byte (bits 8-15). A disabled mover
// or no settled direction returns 0.
//
// The original carried the destination in one register: the first direction
// returns status 1, but a second-direction neighbour returns the status the
// failed gap landing left, which is 0. The tag byte it passed to the
// primitives came from the caller's register; here it is 0 until a gap
// landing sets it. Nothing refuses a direction outside 0..7, where the
// original read past the frame's direction codes, or a refusing world.
[[nodiscard]] std::optional<std::uint32_t> FindAdjacentMovementDestination(
    AdjacentDestinationWorld& world, const AdjacentMover& mover, int first,
    int second, std::uint32_t direction);

// An actor's recent fine positions: eight x and eight z samples, and one mask
// bit per sample written.
struct MovementHistory {
  std::array<std::uint32_t, 2> x_words{};  // 0x70
  std::array<std::uint32_t, 2> z_words{};  // 0x78
  std::uint8_t mask = 0;                   // 0x81
};

// decomp: historyRepeats (src/BATTLE/BATTLE.PRG/6FC00.c:960), inlined in
// func_800D8EF4 at 0x800D928C-0x800D92B4.
// Whether the first four x samples equal the last four, and likewise z.
[[nodiscard]] bool HasRepeatingMovementHistory(const MovementHistory& history);

// decomp: historyStalled (src/BATTLE/BATTLE.PRG/6FC00.c:966), inlined in
// func_800D8EF4 at 0x800D927C-0x800D92B4.
// Whether all eight samples are written and the history repeats.
[[nodiscard]] bool IsMovementHistoryStalled(const MovementHistory& history);

// The AI frame's random state and the copy movement keeps of it.
struct MovementRandomStates {
  std::uint32_t frame = 0;  // the frame's random state
  std::int32_t saved = 0;   // D_800F58C4
};

// decomp: func_800D9538 (src/BATTLE/BATTLE.PRG/6FC00.c:1178).
// Saves the frame's random state.
void SaveMovementRandomState(MovementRandomStates& states);

// The detour directions, in the order a stuck actor tries them.
inline constexpr std::size_t kDetourDirectionCount = 4;
using DetourDirections = std::array<std::uint8_t, kDetourDirectionCount>;

// The detour fields of an actor AI state.
struct BlockedMover {
  std::uint8_t is_stuck = 0;  // 0x02
  // 0x1C: set while the actor waits on a tile it could not leave.
  std::uint8_t holds_stall_tile = 0;
  std::uint32_t tile = 0;           // 0x34
  std::uint32_t previous_tile = 0;  // 0x38
  std::int16_t x = 0;               // 0x4C
  std::int16_t z = 0;               // 0x50
  std::uint8_t detour_step = 0;     // 0xFA: the next detour direction plus one
  std::uint8_t detour_target = 0;   // 0xFB: an actor ID, 16 for none
  DetourDirections detour_directions{};  // 0x118
  std::uint8_t movement_target = 0;      // 0x15C
  std::uint32_t destination = 0;         // 0x168

  friend bool operator==(const BlockedMover&, const BlockedMover&) = default;
};

// The AI frame and the movement code a detour uses. Each call taking the
// mover may read and change it.
class BlockedMovementWorld {
 public:
  virtual ~BlockedMovementWorld() = default;
  [[nodiscard]] virtual NavigationDirectionCodes DirectionCodes() const = 0;
  // Whether the arena orbit (D_800F5920) is active.
  [[nodiscard]] virtual bool HasArenaOrbit() const = 0;
  // func_800E45D4.
  [[nodiscard]] virtual int NextRandomMasked(int range) = 0;
  // func_800D954C, as AdjacentDestinationWorld::DestinationOccupancy.
  [[nodiscard]] virtual std::optional<int> DestinationOccupancy(
      BlockedMover& mover, std::uint32_t tile) = 0;
  // func_800E4CE8.
  virtual void ForgetActionTarget(BlockedMover& mover) = 0;
  // func_800E50A0 with mode 0 and angle 0.
  virtual void DispatchLocomotion(BlockedMover& mover) = 0;
};

// decomp: func_800DB0BC (src/BATTLE/BATTLE.PRG/71618.c:1212).
// Detours an actor whose tile has not changed. Nothing happens while the
// arena orbit is active; an actor that moved clears its detour step. A stuck
// actor is marked; at step 0 it stores two lateral directions from the angle
// between it and its destination's tile centre, the angle's 45-degree sector
// and the direction two past it, the sector first only when it is the
// angle's nearest axis direction; then their opposites in a random order. It
// starts at the first whose neighbour is free (step 1 when none is). From
// step 5 it forgets its action target, clears 0x1C and the step, and returns
// false.
// Otherwise it moves the destination to the step's neighbour and copies the
// detour target to the movement target; a free destination dispatches
// locomotion, otherwise the step advances; it returns true. Nothing refuses a
// stored detour direction outside 0..7, where the original read past the
// frame's direction codes, or a refusing world; the mover may then be partly
// updated.
[[nodiscard]] std::optional<bool> TryRecoverBlockedMovement(
    BlockedMovementWorld& world, BlockedMover& mover);
}  // namespace VS::Battle::Ai
