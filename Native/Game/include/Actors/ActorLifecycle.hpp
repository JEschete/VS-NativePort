#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>
#include <optional>

#include "Actors/ActorStateRecord.hpp"

// The actor lifecycle rules (BATTLE.PRG 30DB0.c:2073-3650): the motion-state
// reset of a newly placed actor, the room-wide movement reset, the start of
// an actor's death, its companion model and its attachment to a parent's
// bone. The rules see records only as ActorStateRecord values the host lends
// them.
namespace VS::Actors {
// The parent actor and bone an actor attaches to.
struct AttachPoint {
  std::uint8_t parent = kNoAttachParent;
  std::uint8_t bone = 0;

  friend bool operator==(const AttachPoint&, const AttachPoint&) = default;
};

// What releasing an actor's attachment lock found.
enum class AttachmentRelease : std::int8_t {
  kReleased = 0,
  // The parent is outside the actor table, missing or has no skeleton.
  kParentNotReady = -1,
  // The actor is missing or attached to no parent.
  kNoParent = -2,
};

// decomp: BATTLE.PRG func_8009D468 (src/BATTLE/BATTLE.PRG/30DB0.c:2073)
// Returns the actor to its standing pose with every tween, effect, layer and
// movement request cleared, shown but frozen.
void ResetActorMotionState(ActorStateHost& host, int id);
// decomp: BATTLE.PRG func_8009D6F4 (src/BATTLE/BATTLE.PRG/30DB0.c:2147)
void ResetAllActorMovement(ActorStateHost& host);
// decomp: BATTLE.PRG func_8009F314 (src/BATTLE/BATTLE.PRG/30DB0.c:3202)
// Drops a moving or airborne actor, then plays (or only marks) its death and
// that of its companion models; a gimmick in an empty slot breaks instead.
void BeginActorDeath(ActorStateHost& host, int id, bool animate);
// decomp: BATTLE.PRG func_8009F940 (src/BATTLE/BATTLE.PRG/30DB0.c:3388)
// A nonzero slot takes the style; slot 0 clears the companion and keeps it.
void SetActorCompanion(ActorStateHost& host, int id,
                       std::uint8_t companion_slot,
                       const std::optional<CompanionStyle>& style);
// decomp: BATTLE.PRG func_8009FF08 (src/BATTLE/BATTLE.PRG/30DB0.c:3613)
// Attaches the actor (to `point`, or to its current parent when there is
// none) with its position, rotation and tweens cleared.
void AttachActorToParentBone(ActorStateHost& host, int id,
                             const std::optional<AttachPoint>& point);
// decomp: BATTLE.PRG func_8009FFA8 (src/BATTLE/BATTLE.PRG/30DB0.c:3635)
// Unlocks the actor from its parent once the parent has a skeleton.
AttachmentRelease ReleaseActorAttachmentLock(ActorStateHost& host, int id);
}  // namespace VS::Actors
