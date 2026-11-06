#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>
#include <optional>

#include "Actors/AnimationPose.hpp"
#include "Legacy/Psx.h"

struct Actors_ActorRecord;
struct Actors_AnimationPose;

// Bindings from actor records to the typed animation pose (AnimationPose.cpp)
// and event script (AnimationEvents.cpp). Banks are read through the extents
// their loaders registered (SequencePlaybackLive.hpp); a bank without one, or
// a map entry, header or event outside it, is reported and refused.
// Generated callers keep the original call shapes (copy_fixups.py
// OWNED_CALL_BINDINGS).
namespace VS::Actors {
// The actor record fields that select and size its animation banks.
[[nodiscard]] AnimationBankLimits LiveAnimationBankLimits(
    const Actors_ActorRecord& actor);
// decomp: BATTLE.PRG func_800AD494. The bank and clip header `animation_id`
// names for the actor; bank problems are reported. A map read leaves the map
// entry in Actors_lastResolvedSequenceIndex, as the original did.
[[nodiscard]] AnimationHeader FindLiveAnimationHeader(
    const Actors_ActorRecord& actor, int animation_id);
// The clip bound to the actor's displayed pose: the bank its animation
// header names and the clip its animation set selects, or nothing when the
// bound bank or clip no longer matches.
[[nodiscard]] std::optional<AnimationClipRef> BoundLiveAnimationClip(
    const Actors_ActorRecord& actor);
// decomp: BATTLE.PRG func_800AD714. Runs `pose`'s due events in `mode`
// (AnimationEventMode); returns the impact frame for kFindImpact, else 0. A
// mode outside AnimationEventMode is refused.
int RunLiveAnimationEvents(Actors_ActorRecord& actor,
                           Actors_AnimationPose& pose, int mode);
// decomp: BATTLE.PRG func_800AE47C. Reverts the bound clip's vertex events
// on the displayed pose; -1 when they cannot be reverted, else 0.
int RestartLiveAnimationEvents(Actors_ActorRecord& actor);

// decomp: BATTLE.PRG func_800AD494, in the original call shape: the bank's
// address (0 when refused), and the clip header in `*header_out` (null
// without one).
std::intptr_t FindLiveActorAnimationHeader(void* actor, int animation_id,
                                           u_short** header_out);
// decomp: BATTLE.PRG func_800AD008. Starts `pose` at the actor's animation.
void InitializeLiveActorAnimationPose(Actors_ActorRecord* actor,
                                      Actors_AnimationPose* pose);
// decomp: BATTLE.PRG func_800AF6E8.
void UpdateLiveActorAnimationBlend(Actors_ActorRecord* actor);
// decomp: BATTLE.PRG func_800B002C.
void UpdateLiveMotionAndLookAt(Actors_ActorRecord* actor, int skip_tweens);
// decomp: BATTLE.PRG func_800B26B8. Stores the transform of `bone`, locked to
// saved matrix `saved` (1 or 2), in the skeleton pose and the actor.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void ApplyLiveLockedBoneTransform(Actors_ActorRecord* actor,
                                  const MATRIX* parent, int bone, int saved);
}  // namespace VS::Actors
