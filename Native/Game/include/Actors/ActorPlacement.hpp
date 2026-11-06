#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>
#include <optional>

#include "Actors/ActorStateRecord.hpp"
#include "Actors/MotionRequests.hpp"

// The actor placement rules (BATTLE.PRG 30DB0.c:4182 and 3A1A0.c:274-500):
// where an actor or slot gimmick stands, placing an actor on its room tile
// (in the air for flyers, dropped from a height or on a moving platform),
// stacking flyers that would share a spot, the placement saved for an actor
// and the nearest actor in front of one. The rules see records only as
// ActorStateRecord values the host lends them.
namespace VS::Actors {
// The movement classes of flyers and of actors that land with a jump.
inline constexpr int kFlyingClass = 0x10;
inline constexpr int kHoveringClass = 0x20;
inline constexpr int kLeapingClass = 0x40;
inline constexpr int kBoundingClass = 0x80;
// A room placement on this layer drops the actor from its given height.
inline constexpr std::uint8_t kDropPlacementLayer = 7;

// Where an actor or gimmick stands: its tile and position, with the facing
// in position.w.
struct ActorPlacementView {
  PackedTile tile{};
  MotionVector position{};

  friend bool operator==(const ActorPlacementView&,
                         const ActorPlacementView&) = default;
};

[[nodiscard]] bool IsFlightClass(int movement_class);

// decomp: BATTLE.PRG func_800A1108 (src/BATTLE/BATTLE.PRG/30DB0.c:4182)
// Nothing for a slot with neither an actor nor a gimmick.
[[nodiscard]] std::optional<ActorPlacementView> ActorPlacement(
    ActorStateHost& host, int id);
// decomp: BATTLE.PRG func_800A30A0 (src/BATTLE/BATTLE.PRG/3A1A0.c:274)
// Places the actor on `tile` (its spare byte is the facing in sixteenths):
// flyers above the floor, the drop layer at `drop_height`, platform layers on
// their moving platform and everyone else on the floor.
void PlaceActorInRoom(ActorStateHost& host, int id, PackedTile tile,
                      int drop_height, int movement_class);
// decomp: BATTLE.PRG func_800A3310 (src/BATTLE/BATTLE.PRG/3A1A0.c:350)
// The earlier placed flyer (from slot 2 up to `id`) already at `position`.
[[nodiscard]] std::optional<int> FindActorAtFlightPosition(
    ActorStateHost& host, int id, const MotionVector& position);
// decomp: BATTLE.PRG func_800A3394 (src/BATTLE/BATTLE.PRG/3A1A0.c:365)
// Raises `position` above the flyers stacked there, keeping the actor below
// the room's ceiling where it can.
void SeparateActorFlightPlacement(ActorStateHost& host, int id,
                                  MotionVector& position);
// decomp: BATTLE.PRG func_800A36E0 (src/BATTLE/BATTLE.PRG/3A1A0.c:448)
// The actor's saved tile (its current tile for a placed flyer), with its
// facing in sixteenths in the spare byte.
[[nodiscard]] std::optional<PackedTile> ActorSavedPlacement(
    ActorStateHost& host, int id, int movement_class);
// decomp: BATTLE.PRG func_800A3760 (src/BATTLE/BATTLE.PRG/3A1A0.c:460)
// How far the nearest-actor search reaches: `horizontal` across the floor
// and `vertical` above or below the actor.
struct AheadRange {
  int horizontal = 0;
  int vertical = 0;
};
// The closest shown actor within the ranges and a quarter turn of the
// actor's facing.
[[nodiscard]] std::optional<int> FindNearestActorAhead(ActorStateHost& host,
                                                       int id,
                                                       AheadRange range);
}  // namespace VS::Actors
