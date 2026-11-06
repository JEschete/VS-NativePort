#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

struct Actors_ActorRecord;
struct Actors_ModelEffectSlot;
struct Battle_Actor;

// Binding from the actor table and the original arguments to the typed actor
// animation controls (AnimationControl.hpp). Each adapter keeps the original
// argument order for the generated callers. An actor id outside the actor
// table, a null record or a null argument the original dereferenced is
// refused with a warning on the "actors" channel.
namespace VS::Actors {
// `actor` is unused, as in the original.
void QueueLiveActorBaseSequenceLoad(Battle_Actor* actor, int actor_id);
// `actor_bytes` is the Battle_Actor whose equipped weapon category is read
// when the weapon sequence is queued.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void QueueLiveActorWeaponSequenceLoad(char* actor_bytes, int actor_id,
                                      int weapon_category, int spawn_flags);
void InterruptLiveActorAction(int actor_id, int animate);
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void QueueLiveSharedSequenceLoad(int index, int weapon_id);
int UnloadLiveSharedSequence(int bank);
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void QueueLiveSharedSequenceWithTextures(int index, int weapon_id,
                                         int texture_slot);
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void QueueLiveSequenceEffectTextures(int index, int weapon_id,
                                     int texture_slot);
void ReleaseLiveSequenceEffectTextures(int bank);
void SetLiveSharedSequenceBank(int bank);
void ReadLiveSharedSequenceBank();
void ReturnLiveActorToIdle(int actor_id);
void ReturnAllLiveActorsToIdle();
int SelectLiveSharedSequenceBank(unsigned int bank);
int LiveSharedSequenceBankId();
void RequestLiveActorAnimationAdvance(int actor_id);
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void SetLiveActorWeaponDrawn(int actor_id, int weapon_drawn,
                             int transition_mode);
void PlayLiveActorWeaponDrawSound(int actor_id, int sound_offset);
// The facing point keeps the original's vector layout (x, y, z, pad); the
// generated callers pass a four-short array with only x and z set, and only
// those are read. Null faces nothing.
void PlayLiveActorActionPose(int actor_id, const std::int16_t* facing_target,
                             int pose_kind);
int IsLiveAshleyTransitionPose();
int IsLiveActorActionAnimationBusy(int actor_id);
void RestoreLiveActorIdlePose(int actor_id);
void RestoreLiveActorNeutralPose(int actor_id);
void RestoreLiveActorBasePose(int actor_id, int animation);
int LiveAnimationBattleWindowFrame(int actor_id);
void SetLiveActorSoundOverrides(int actor_id,
                                Actors_ModelEffectSlot* sound_override);
void ApplyLiveActorSoundOverrides(int actor_id,
                                  Actors_ModelEffectSlot* sound_override);
int SetLiveActorTravelAnimation(int actor_id, int animation);
// -1 without an actor or an output, else 0 with the animation written.
int LiveActorTravelAnimation(int actor_id, int* animation_out);
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
int QueueLiveActorAnimation(int actor_id, int animation, int blend_frames,
                            int start_frame);
// `unused` is the original's always-zero third argument.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
int PlayLiveActorAnimation(int actor_id, int animation, int unused,
                           int blend_frames);
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
int PlayLiveActorAnimationAtFrame(int actor_id, int animation, int blend_frames,
                                  int start_frame);
void SetLiveActorTransitionMode(int control);
void SetLiveActorAnimationLocked(int actor_id, int locked);
void PlayLiveActorTurnAnimation(int actor_id, int target_facing);
// `direction` is unused, as in the original; returns 0.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
int UpdateLiveActorLocomotionAnimation(Actors_ActorRecord* actor, int speed,
                                       int direction);
void SetLiveActorBonePoseLock(Actors_ActorRecord* actor, int bone, int lock);
void ClearLiveActorBonePoseLocks(Actors_ActorRecord* actor);
}  // namespace VS::Actors
