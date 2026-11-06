#pragma once

#include <array>
#include <cstdint>
#include <optional>

#include "Actors/ActorMotion.hpp"
#include "Actors/TextureAnimation.hpp"
#include "Content/ActorAssets.hpp"

// The actor animation controls (BATTLE.PRG 146C.c, 30DB0.c, 38C1C.c, 3A1A0.c
// and 40564.c): the sequence-bank asset requests and the shared bank
// retirement and selection, the weapon draw and sheathe, the action, turn,
// idle and base poses, the animation start with its stance substitutions,
// the walk and run cycle, the queued and travel animations, the sound
// overrides and the bone pose locks. The functions keep the original names
// and order of effects; a missing actor record makes the functions that
// checked for one do nothing, and those that dereferenced one now refuse it
// with a warning on the "actors" channel (#295).
namespace VS::Actors {
// A loaded sequence bank (an animation header's owner) or none.
enum class SequenceHandle : std::uintptr_t { kNone = 0 };
// A position in an animation event script or none.
enum class EventScriptHandle : std::uintptr_t { kNone = 0 };

// A pose's event cursor (Actors_AnimationPose eventScript, nextEventFrame).
struct PoseEventCursor {
  EventScriptHandle script = EventScriptHandle::kNone;
  std::uint16_t next_event_frame = 0;

  friend bool operator==(const PoseEventCursor&,
                         const PoseEventCursor&) = default;
};

// An actor's sound effect setting (Actors_ModelEffectSlot).
struct SoundEffectSlot {
  std::uint8_t kind = 0;
  std::uint8_t level = 0;
  std::uint8_t flags = 0;
  std::uint8_t parameter = 0;

  friend bool operator==(const SoundEffectSlot&,
                         const SoundEffectSlot&) = default;
};

inline constexpr int kSharedSequenceBanks = 3;
inline constexpr int kBonePoseLocks = 2;

// The actor-record fields the animation controls read and write; the fields
// marked read are never stored. Bitfield widths are noted.
struct AnimationActor {
  ActorMotion motion;
  // Model instance.
  bool weapon_drawn = false;
  std::uint8_t reaction_state = 0;  // 4 bits, read
  std::uint8_t support_layer = 0;   // 4 bits, read
  std::uint8_t bone_count = 0;      // read
  bool has_model = false;           // read
  // Actor record.
  std::uint8_t animation_id = 0;
  std::uint8_t animation_set = 0;
  SequenceHandle animation_header = SequenceHandle::kNone;
  std::int16_t animation_length = 0;
  std::uint8_t queued_animation = 0;
  std::uint8_t queued_blend_frames = 0;
  std::uint8_t queued_start_frame = 0;
  std::uint8_t travel_animation = 0;
  bool animation_locked = false;
  bool animation_event_flag = false;
  bool pose_override_active = false;
  bool fast_animation = false;
  bool advance_requested = false;
  std::uint8_t locomotion_animation_state = 0;  // 1 bit
  std::uint8_t locomotion_counter = 0;
  std::uint8_t blend_frame = 0;
  std::uint8_t blend_frames = 0;
  std::uint16_t collision_radius = 0;  // read
  std::uint16_t move_facing = 0;
  std::uint8_t sequence_model_id = 0;  // read
  std::uint16_t model_id = 0;          // read
  std::uint8_t frames_per_step = 0;
  std::array<std::uint8_t, kTextureAnimationChannels> texture_overlay_index{};
  // read
  std::array<std::uint8_t, kTextureAnimationChannels> texture_current_frame{};
  // The texture row each shared bank gives the actor, 0xFF for none; read.
  std::array<std::uint8_t, kSharedSequenceBanks> texture_rows{};
  // read
  std::array<SequenceHandle, kSharedSequenceBanks> shared_sequences{};
  SoundEffectSlot sound_effect;
  SoundEffectSlot sound_effect_target;
  std::array<std::uint8_t, kBonePoseLocks> locked_bones{};
  PoseEventCursor pose;
  PoseEventCursor blend_pose;
  bool holding_last_frame = false;  // read

  friend bool operator==(const AnimationActor&,
                         const AnimationActor&) = default;
};

// An animation lookup (func_800AD494): the bank holding the animation, or
// none, whether the bank has the animation's entry, the entry's frame count
// and the map index the lookup read.
struct AnimationLookup {
  SequenceHandle bank = SequenceHandle::kNone;
  bool has_entry = false;
  std::int16_t length = 0;
  std::uint8_t resolved_set = 0;
};

struct AnimationGimmick {
  std::int16_t mode = 0;
  std::uint8_t support_index = 0;
};

struct SoundPanVolume {
  int pan = 0;
  int volume = 0;
};

class AnimationControlHost {
 public:
  AnimationControlHost() = default;
  AnimationControlHost(const AnimationControlHost&) = delete;
  AnimationControlHost& operator=(const AnimationControlHost&) = delete;
  AnimationControlHost(AnimationControlHost&&) = delete;
  AnimationControlHost& operator=(AnimationControlHost&&) = delete;
  virtual ~AnimationControlHost() = default;

  // The actor in table slot `id` (0 to 16), or null for an empty slot or an
  // id outside the table. One id names one actor for the host's lifetime.
  [[nodiscard]] virtual AnimationActor* Actor(int id) = 0;
  // Whether battle actor `index` (0 to 15) is present.
  [[nodiscard]] virtual bool HasBattleActor(int index) = 0;
  [[nodiscard]] virtual int TickSpeed() = 0;
  [[nodiscard]] virtual int EventPlaybackState() = 0;
  [[nodiscard]] virtual std::optional<AnimationGimmick> GimmickForActorSlot(
      int slot) = 0;
  virtual void SetGimmickMode(int slot, std::int16_t mode) = 0;

  // The weapon draw and sheathe sound bases by sequence model.
  [[nodiscard]] virtual int DrawnWeaponSoundBase(int sequence_model) = 0;
  [[nodiscard]] virtual int SheathedWeaponSoundBase(int sequence_model) = 0;
  // The jump travel animation for a heading quadrant (0 to 3).
  [[nodiscard]] virtual int QuadrantMoveAnimation(bool fast, int quadrant) = 0;

  // The shared sequence bank index (bank - 1) and the transition control.
  [[nodiscard]] virtual int SharedSequenceBankIndex() = 0;
  virtual void SetSharedSequenceBankIndex(int index) = 0;
  virtual void SetTransitionControl(int control) = 0;
  virtual void QueueSequenceAsset(
      const Content::ActorAssetRequest& request) = 0;
  // Releases shared bank `bank`'s sequence; the release status.
  [[nodiscard]] virtual int ReleaseSharedSequence(int bank) = 0;
  virtual void ReleaseSequenceEffectTextures(int bank) = 0;

  virtual void BeginActorDeath(int id, int animate) = 0;
  // Shows or hides actor `id`'s two weapon models.
  virtual void SetWeaponModelsDrawn(int id, bool drawn) = 0;
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
  virtual void PlaySoundPair(AnimationActor& actor, int first, int second) = 0;
  [[nodiscard]] virtual SoundPanVolume SoundPanAndVolume(int id) = 0;
  virtual void CancelTeleportTransition(AnimationActor& actor) = 0;
  virtual void ReleaseAiDestinationTile(int tile_x, int tile_z) = 0;
  virtual void UploadTextureFrame(AnimationActor& actor, int channel,
                                  int frame) = 0;
  // Restarts actor `id`'s texture animation (timeline 1 of channel 1).
  virtual void RestartTextureAnimation(int id) = 0;

  // Pose playback.
  [[nodiscard]] virtual AnimationLookup FindAnimationHeader(
      AnimationActor& actor, int animation) = 0;
  // Whether the actor's bound clip resolves to its animation header with an
  // unwind entry.
  [[nodiscard]] virtual bool CanUnwindBoundAnimation(AnimationActor& actor) = 0;
  // Restarts the pose override's events; negative when it cannot.
  [[nodiscard]] virtual int RestartAnimationEvents(AnimationActor& actor) = 0;
  virtual void InitializePose(AnimationActor& actor) = 0;
  virtual void BeginShadowBlend(AnimationActor& actor, int blend_frames) = 0;
  // Runs the pose's events in battle-window mode; the frame they name.
  [[nodiscard]] virtual int RunBattleWindowEvents(AnimationActor& actor) = 0;
  virtual void AdvanceSkeletalAnimation(AnimationActor& actor) = 0;

  // Bones; `bone` is below the actor's bone count.
  virtual void CaptureBoneAnchor(AnimationActor& actor, int bone,
                                 int lock_index) = 0;
  virtual void SetBonePoseLockSlot(AnimationActor& actor, int bone,
                                   int lock_slot) = 0;
};

// Whether an actor's action animation still plays (func_8009E4B0).
enum class ActionAnimationBusy : std::int8_t {
  kNoActor = -1,  // no record and no gimmick in the slot
  kIdle = 0,
  kBusy = 1,
};

// func_800770FC
void QueueActorBaseSequenceLoad(AnimationControlHost& host, int actor_id);
// func_800771E0: queues the weapon sequence when the actor carries one;
// `equipped_weapon_category` is read only then.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void QueueActorWeaponSequenceLoad(AnimationControlHost& host,
                                  std::optional<int> equipped_weapon_category,
                                  int actor_id, int weapon_category,
                                  int spawn_flags);
// func_8007C348
void InterruptActorAction(AnimationControlHost& host, int actor_id,
                          int animate);
// func_8007CB84
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void QueueSharedSequenceLoad(AnimationControlHost& host, int index,
                             int weapon_id);
// func_8007CBBC: unwinds every actor posed from bank `bank` (1 to 3) and
// releases it; -1 when the bank is invalid or an actor cannot unwind, else
// the release status.
int UnloadSharedSequence(AnimationControlHost& host, int bank);
// func_8007CBDC
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void QueueSharedSequenceWithTextures(AnimationControlHost& host, int index,
                                     int weapon_id, int texture_slot);
// vs_battle_loadEtm
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void QueueSequenceEffectTextures(AnimationControlHost& host, int index,
                                 int weapon_id, int texture_slot);
// func_8007CC4C
void ReleaseSequenceEffectTextures(AnimationControlHost& host, int bank);
// func_8007CC6C
void SetSharedSequenceBank(AnimationControlHost& host, int bank);
// func_8007CC8C: reads the bank id and discards it.
void SharedSequenceBank(AnimationControlHost& host);
// func_8007CEA0
void ReturnActorToIdle(AnimationControlHost& host, int actor_id);
// func_8007CEC0
void ReturnAllActorsToIdle(AnimationControlHost& host);
// func_8009CE9C: -1 for a bank outside 1 to 3, else 0.
int SelectSharedSequenceBank(AnimationControlHost& host, int bank);
// func_8009CFA0
[[nodiscard]] int SharedSequenceBankId(AnimationControlHost& host);
// func_8009D854
void RequestActorAnimationAdvance(AnimationControlHost& host, int actor_id);
// func_8009D934: the low bit of `weapon_drawn` is stored; transition mode 2
// only shows or hides the weapon models.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void SetActorWeaponDrawn(AnimationControlHost& host, int actor_id,
                         int weapon_drawn, int transition_mode);
// func_8009DBDC
void PlayActorWeaponDrawSound(AnimationControlHost& host, int actor_id,
                              int sound_offset);
// func_8009E070: pose kinds 1 to 7; another kind does nothing.
void PlayActorActionPose(AnimationControlHost& host, int actor_id,
                         const MotionVector* facing_target, int pose_kind);
// func_8009E480
[[nodiscard]] bool IsAshleyTransitionPose(AnimationControlHost& host);
// func_8009E4B0
[[nodiscard]] ActionAnimationBusy IsActorActionAnimationBusy(
    AnimationControlHost& host, int actor_id);
// func_8009E5C4
void RestoreActorIdlePose(AnimationControlHost& host, int actor_id);
// func_8009E614
void RestoreActorNeutralPose(AnimationControlHost& host, int actor_id);
// func_8009E700
void RestoreActorBasePose(AnimationControlHost& host, int actor_id,
                          int animation);
// func_8009F8DC: the frame the pose's battle-window event names, leaving the
// pose's event cursor unchanged; 0 without events.
[[nodiscard]] int AnimationBattleWindowFrame(AnimationControlHost& host,
                                             int actor_id);
// func_8009F990: actor -1 sets every actor.
void SetActorSoundOverrides(AnimationControlHost& host, int actor_id,
                            const SoundEffectSlot& sound_override);
// func_8009F9F4
void ApplyActorSoundOverrides(AnimationControlHost& host, int actor_id,
                              const SoundEffectSlot& sound_override);
// func_8009FBD8: animation -1 sets the default walk (0xD); -1 without an
// actor, else 0.
int SetActorTravelAnimation(AnimationControlHost& host, int actor_id,
                            int animation);
// func_8009FC20
[[nodiscard]] std::optional<int> ActorTravelAnimation(
    AnimationControlHost& host, int actor_id);
// func_800A01C8: -1 without an actor, else 0.
int QueueActorAnimation(AnimationControlHost& host, int actor_id, int animation,
                        int blend_frames, int start_frame);
// func_800A0204: starts `animation` (0 does nothing) with its stance
// substitutions, falling back to animation 0 when the actor's banks lack it;
// -1 without an actor or when the pose override cannot unwind, else 0.
int PlayActorAnimation(AnimationControlHost& host, int actor_id, int animation,
                       int blend_frames);
// func_800A0618: plays from `start_frame` less half a tick.
int PlayActorAnimationAtFrame(AnimationControlHost& host, int actor_id,
                              int animation, int blend_frames, int start_frame);
// func_800A0870
void SetActorTransitionMode(AnimationControlHost& host, int control);
// func_800A09D8: the low bit of `locked` is stored.
void SetActorAnimationLocked(AnimationControlHost& host, int actor_id,
                             int locked);
// func_800A249C
void PlayActorTurnAnimation(AnimationControlHost& host, int actor_id,
                            int target_facing);
// func_800A6AA0: picks the walk, run or idle animation for `speed`; the
// heading comes from the actor's movement request (the original's direction
// argument is unused).
void UpdateActorLocomotionAnimation(AnimationControlHost& host,
                                    AnimationActor& actor, int speed);
// func_800AC440: locks (`lock` nonzero) or unlocks `bone`'s pose.
void SetActorBonePoseLock(AnimationControlHost& host, AnimationActor& actor,
                          int bone, int lock);
// func_800AC500
void ClearActorBonePoseLocks(AnimationControlHost& host, AnimationActor& actor);
}  // namespace VS::Actors
