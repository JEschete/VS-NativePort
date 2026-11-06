// Hand-maintained (tools/port/owned.csv).
// Live binding for the typed actor animation controls (AnimationControl.cpp).
#include "Actors/AnimationControlLive.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <optional>

#include "Actors/ActorMotionLive.hpp"
#include "Actors/ActorMovementLive.hpp"
#include "Actors/ActorStateLive.hpp"
#include "Actors/AnimationControl.hpp"
#include "Actors/AnimationPoseLive.hpp"
#include "Actors/AppearanceLive.hpp"
#include "Actors/SequencePlaybackLive.hpp"
#include "Actors/SkeletonLive.hpp"
#include "Actors/TextureAnimationLive.hpp"
#include "Audio/ActorSoundLive.hpp"
#include "Battle/Ai/ActorCleanupLive.hpp"
#include "Content/ActorAssets.hpp"
#include "Content/ActorAssetsLive.hpp"
#include "Core/Log.hpp"

extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/Movement.h"
#include "Actors/Skeleton.h"
#include "Actors/SkeletonLiveState.h"
#include "Actors/WeaponModel.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Event/ScriptVm.h"
#include "Field/GimmickLookup.h"
#include "Field/Gimmicks.h"
#include "State/Timers.h"
}

namespace VS::Actors {
namespace {
constexpr std::size_t kActorSlots = std::size(Actors_actorRecords);
// Every table slot plus one record a caller names outside the table.
constexpr std::size_t kRegistryCapacity = kActorSlots + 1;
constexpr int kQuadrants = 4;
// The weapon models' draw state (Actors_WeaponModel reserved11, reserved12).
constexpr u_char kDrawnWeaponModel = 0x40;
// The battle-window mode of the animation event runner.
constexpr int kBattleWindowEvents = 3;
// The texture animation restart (channel 1, timeline 1, command 1).
constexpr int kRestartChannel = 1;
constexpr int kRestartTimeline = 1;
constexpr int kRestartCommand = 1;

template <typename Handle, typename Pointer>
Handle HandleOf(Pointer* pointer) {
  return static_cast<Handle>(reinterpret_cast<std::uintptr_t>(pointer));
}

template <typename Pointer, typename Handle>
Pointer* PointerOf(Handle handle) {
  return reinterpret_cast<Pointer*>(static_cast<std::uintptr_t>(handle));
}

SoundEffectSlot LoadSlot(const Actors_ModelEffectSlot& live) {
  return {.kind = live.kind,
          .level = live.level,
          .flags = live.flags,
          .parameter = live.parameter};
}

void StoreSlot(const SoundEffectSlot& slot, Actors_ModelEffectSlot& live) {
  live.kind = slot.kind;
  live.level = slot.level;
  live.flags = slot.flags;
  live.parameter = slot.parameter;
}

PoseEventCursor LoadCursor(const Actors_AnimationPose& pose) {
  return {.script = HandleOf<EventScriptHandle>(pose.eventScript),
          .next_event_frame = pose.nextEventFrame};
}

void StoreCursor(const PoseEventCursor& cursor, Actors_AnimationPose& pose) {
  pose.eventScript = PointerOf<u_char>(cursor.script);
  pose.nextEventFrame = cursor.next_event_frame;
}

u_char Bit(bool value) { return value ? 1 : 0; }

AnimationActor LoadLiveAnimationActor(const Actors_ActorRecord& live) {
  AnimationActor actor;
  actor.motion = LoadLiveActorMotion(live);
  const Actors_ModelInstance& instance = live.instance;
  actor.weapon_drawn = instance.weaponDrawn != 0;
  actor.reaction_state = instance.reactionState;
  actor.support_layer = instance.supportLayer;
  actor.bone_count = instance.boneCount;
  actor.has_model = instance.model != nullptr;
  actor.animation_id = live.animationId;
  actor.animation_set = live.animationSet;
  actor.animation_header = static_cast<SequenceHandle>(
      static_cast<std::uintptr_t>(live.animationHeader));
  actor.animation_length = live.animationLength;
  actor.queued_animation = live.queuedAnimation;
  actor.queued_blend_frames = live.queuedParameterA;
  actor.queued_start_frame = live.queuedParameterB;
  actor.travel_animation = live.travelAnimation;
  actor.animation_locked = live.animationLocked != 0;
  actor.animation_event_flag = live.animationEventFlag != 0;
  actor.pose_override_active = live.poseOverrideActive != 0;
  actor.fast_animation = live.fastAnimation != 0;
  actor.advance_requested = live.advanceRequested != 0;
  actor.locomotion_animation_state = live.locomotionAnimationState;
  actor.locomotion_counter = live.locomotionCounter;
  actor.blend_frame = static_cast<std::uint8_t>(live.blendFrame);
  actor.blend_frames = static_cast<std::uint8_t>(live.blendFrames);
  actor.collision_radius = live.collisionRadius;
  actor.move_facing = live.moveFacing;
  actor.sequence_model_id = live.sequenceModelId;
  actor.model_id = live.modelId;
  actor.frames_per_step = live.framesPerStep;
  for (std::size_t channel = 0; channel < kTextureAnimationChannels;
       ++channel) {
    actor.texture_overlay_index.at(channel) =
        live.textureAnimationFrames[channel].overlayIndex;
    actor.texture_current_frame.at(channel) =
        live.textureAnimationSlots[channel].currentFrame;
  }
  for (std::size_t bank = 0; bank < kSharedSequenceBanks; ++bank) {
    // textureRowOffsets[2] aliases the low byte of momentumDistance.
    actor.texture_rows.at(bank) = live.textureRowOffsets[bank];
    actor.shared_sequences.at(bank) =
        HandleOf<SequenceHandle>(live.sharedSequences[bank]);
  }
  actor.sound_effect = LoadSlot(live.soundEffect);
  actor.sound_effect_target = LoadSlot(live.soundEffectTarget);
  for (std::size_t lock = 0; lock < kBonePoseLocks; ++lock) {
    actor.locked_bones.at(lock) = live.lockedBones[lock];
  }
  actor.pose = LoadCursor(live.pose);
  actor.blend_pose = LoadCursor(live.blendPose);
  actor.holding_last_frame = live.pose.holdingLastFrame != 0;
  return actor;
}

// Stores the fields the controls write; the read-only fields stay as loaded.
void StoreLiveAnimationActor(const AnimationActor& actor,
                             Actors_ActorRecord& live) {
  StoreLiveActorMotion(actor.motion, live);
  live.instance.weaponDrawn = Bit(actor.weapon_drawn);
  live.animationId = actor.animation_id;
  live.animationSet = actor.animation_set;
  live.animationHeader = static_cast<std::intptr_t>(
      static_cast<std::uintptr_t>(actor.animation_header));
  live.animationLength = actor.animation_length;
  live.queuedAnimation = actor.queued_animation;
  live.queuedParameterA = actor.queued_blend_frames;
  live.queuedParameterB = actor.queued_start_frame;
  live.travelAnimation = actor.travel_animation;
  live.animationLocked = Bit(actor.animation_locked);
  live.animationEventFlag = Bit(actor.animation_event_flag);
  live.poseOverrideActive = Bit(actor.pose_override_active);
  live.fastAnimation = Bit(actor.fast_animation);
  live.advanceRequested = Bit(actor.advance_requested);
  live.locomotionAnimationState = actor.locomotion_animation_state & 1U;
  live.locomotionCounter = actor.locomotion_counter;
  live.blendFrame = static_cast<char>(actor.blend_frame);
  live.blendFrames = static_cast<char>(actor.blend_frames);
  live.moveFacing = actor.move_facing;
  live.framesPerStep = actor.frames_per_step;
  for (std::size_t channel = 0; channel < kTextureAnimationChannels;
       ++channel) {
    live.textureAnimationFrames[channel].overlayIndex =
        actor.texture_overlay_index.at(channel);
  }
  StoreSlot(actor.sound_effect, live.soundEffect);
  StoreSlot(actor.sound_effect_target, live.soundEffectTarget);
  for (std::size_t lock = 0; lock < kBonePoseLocks; ++lock) {
    live.lockedBones[lock] = actor.locked_bones.at(lock);
  }
  StoreCursor(actor.pose, live.pose);
  StoreCursor(actor.blend_pose, live.blendPose);
}

// The actors one control run touches, loaded on first use. Every legacy call
// that may read or write records runs inside a Synced scope, which stores
// the typed actors first and reloads them afterwards; the registry stores
// them once more when the run ends.
class AnimationRegistry {
 public:
  AnimationRegistry() = default;
  AnimationRegistry(const AnimationRegistry&) = delete;
  AnimationRegistry& operator=(const AnimationRegistry&) = delete;
  AnimationRegistry(AnimationRegistry&&) = delete;
  AnimationRegistry& operator=(AnimationRegistry&&) = delete;
  ~AnimationRegistry() { StoreAll(); }

  AnimationActor& Attach(Actors_ActorRecord& record) {
    for (std::size_t index = 0; index < count_; ++index) {
      if (entries_.at(index).record == &record) {
        return entries_.at(index).actor;
      }
    }
    if (count_ == entries_.size()) {
      VS_LOG_ERROR("actors", "Actor animation touched more than {} records",
                   entries_.size());
      std::abort();
    }
    Entry& entry = entries_.at(count_++);
    entry.record = &record;
    entry.actor = LoadLiveAnimationActor(record);
    return entry.actor;
  }

  Actors_ActorRecord& RecordOf(const AnimationActor& actor) {
    for (std::size_t index = 0; index < count_; ++index) {
      if (&entries_.at(index).actor == &actor) {
        return *entries_.at(index).record;
      }
    }
    VS_LOG_ERROR("actors", "Actor animation named an actor it never loaded");
    std::abort();
  }

  void StoreAll() {
    for (std::size_t index = 0; index < count_; ++index) {
      StoreLiveAnimationActor(entries_.at(index).actor,
                              *entries_.at(index).record);
    }
  }

  void ReloadAll() {
    for (std::size_t index = 0; index < count_; ++index) {
      entries_.at(index).actor =
          LoadLiveAnimationActor(*entries_.at(index).record);
    }
  }

 private:
  struct Entry {
    Actors_ActorRecord* record = nullptr;
    AnimationActor actor;
  };
  std::array<Entry, kRegistryCapacity> entries_{};
  std::size_t count_ = 0;
};

class Synced {
 public:
  explicit Synced(AnimationRegistry& registry) : registry_(registry) {
    registry_.StoreAll();
  }
  Synced(const Synced&) = delete;
  Synced& operator=(const Synced&) = delete;
  Synced(Synced&&) = delete;
  Synced& operator=(Synced&&) = delete;
  ~Synced() { registry_.ReloadAll(); }

 private:
  AnimationRegistry& registry_;
};

template <std::size_t Size>
int TableByte(const u_char (&table)[Size], int index, const char* what) {
  if (index < 0 || static_cast<std::size_t>(index) >= Size) {
    VS_LOG_WARN("actors", "{} refused index {}", what, index);
    return 0;
  }
  return table[index];
}

class LiveAnimationHost final : public AnimationControlHost {
 public:
  AnimationActor& Attach(Actors_ActorRecord& record) {
    return registry_.Attach(record);
  }

  AnimationActor* Actor(int id) override {
    if (id < 0 || static_cast<std::size_t>(id) >= kActorSlots) {
      VS_LOG_WARN("actors", "Actor animation refused actor id {}", id);
      return nullptr;
    }
    Actors_ActorRecord* record = Actors_actorRecords[id];
    return record != nullptr ? &registry_.Attach(*record) : nullptr;
  }
  bool HasBattleActor(int index) override {
    if (index < 0 ||
        static_cast<std::size_t>(index) >= std::size(Battle_actors)) {
      return false;
    }
    return Battle_actors[index] != nullptr;
  }
  int TickSpeed() override { return State_tickSpeed; }
  int EventPlaybackState() override {
    const Synced synced(registry_);
    return Event_EventPlaybackState();
  }
  std::optional<AnimationGimmick> GimmickForActorSlot(int slot) override {
    const Field_GimmickInstance* gimmick = Field_FindGimmickForActorSlot(slot);
    if (gimmick == nullptr) return std::nullopt;
    return AnimationGimmick{.mode = gimmick->mode,
                            .support_index = gimmick->objectInfo.supportIndex};
  }
  void SetGimmickMode(int slot, std::int16_t mode) override {
    Field_GimmickInstance* gimmick = Field_FindGimmickForActorSlot(slot);
    if (gimmick == nullptr) {
      VS_LOG_WARN("actors", "Gimmick mode refused: no gimmick in slot {}",
                  slot);
      return;
    }
    gimmick->mode = mode;
  }

  int DrawnWeaponSoundBase(int sequence_model) override {
    return TableByte(Actors_drawnWeaponAnimationBases, sequence_model,
                     "Drawn weapon sound");
  }
  int SheathedWeaponSoundBase(int sequence_model) override {
    return TableByte(Actors_sheathedWeaponAnimationBases, sequence_model,
                     "Sheathed weapon sound");
  }
  int QuadrantMoveAnimation(bool fast, int quadrant) override {
    if (quadrant < 0 || quadrant >= kQuadrants) {
      VS_LOG_WARN("actors", "Quadrant move animation refused quadrant {}",
                  quadrant);
      return 0;
    }
    return fast ? Actors_kFastQuadrantMoveAnimations[quadrant]
                : Actors_kSlowQuadrantMoveAnimations[quadrant];
  }

  int SharedSequenceBankIndex() override {
    return Actors_sharedSequenceBankIndex;
  }
  void SetSharedSequenceBankIndex(int index) override {
    Actors_sharedSequenceBankIndex = static_cast<u_char>(index);
  }
  void SetTransitionControl(int control) override {
    Actors_reservedControlByte = static_cast<char>(control);
  }
  void QueueSequenceAsset(const Content::ActorAssetRequest& request) override {
    static_cast<void>(Content::QueueLiveActorAsset(request));
  }
  int ReleaseSharedSequence(int bank) override {
    const Synced synced(registry_);
    return Content::ReleaseLiveActorSequence(bank, 1);
  }
  void ReleaseSequenceEffectTextures(int bank) override {
    const Synced synced(registry_);
    static_cast<void>(Content::ReleaseLiveSequenceEffectTexture(bank));
  }

  void BeginActorDeath(int id, int animate) override {
    const Synced synced(registry_);
    BeginLiveActorDeath(id, nullptr, animate);
  }
  void SetWeaponModelsDrawn(int id, bool drawn) override {
    for (int hand = 0; hand < 2; ++hand) {
      const int index = (id * 2) + hand;
      if (index < 0 || index >= kActorsWeaponModelCount) {
        VS_LOG_WARN("actors", "Weapon model draw refused actor id {}", id);
        return;
      }
      Actors_WeaponModel* model = Actors_weaponModels[index];
      if (model == nullptr) continue;
      const u_char state = drawn ? kDrawnWeaponModel : 0;
      model->reserved11 = state;
      model->reserved12 = state;
      model->reserved08Bit4 = Bit(drawn);
    }
  }
  void PlaySoundPair(AnimationActor& actor, int first, int second) override {
    Actors_ActorRecord& record = registry_.RecordOf(actor);
    const Synced synced(registry_);
    Audio::LivePlayActorSoundPair(&record, first, second);
  }
  SoundPanVolume SoundPanAndVolume(int id) override {
    SoundPanVolume result;
    const Synced synced(registry_);
    Audio::LiveActorSoundPanAndVolume(id, &result.pan, &result.volume);
    return result;
  }
  void CancelTeleportTransition(AnimationActor& actor) override {
    Actors_ActorRecord& record = registry_.RecordOf(actor);
    const Synced synced(registry_);
    CancelLiveActorTeleportTransition(&record);
  }
  void ReleaseAiDestinationTile(int tile_x, int tile_z) override {
    const Synced synced(registry_);
    Battle::Ai::ReleaseLiveAiDestinationTile(tile_x, tile_z);
  }
  void UploadTextureFrame(AnimationActor& actor, int channel,
                          int frame) override {
    Actors_ActorRecord& record = registry_.RecordOf(actor);
    const Synced synced(registry_);
    UploadLiveActorTextureFrame(&record, channel, frame);
  }
  void RestartTextureAnimation(int id) override {
    const Synced synced(registry_);
    static_cast<void>(SetLiveActorTextureAnimation(
        id, kRestartChannel, kRestartTimeline, kRestartCommand));
  }

  AnimationLookup FindAnimationHeader(AnimationActor& actor,
                                      int animation) override {
    Actors_ActorRecord& record = registry_.RecordOf(actor);
    const Synced synced(registry_);
    const AnimationHeader header = FindLiveAnimationHeader(record, animation);
    const bool found = header.status == AnimationHeaderStatus::kFound;
    const bool selected =
        found || header.status == AnimationHeaderStatus::kNoClip;
    std::uint16_t length = 0;
    if (found) {
      std::memcpy(&length, header.bank.subspan(header.header_offset).data(),
                  sizeof(length));
    }
    return {.bank = selected ? HandleOf<SequenceHandle>(header.bank.data())
                             : SequenceHandle{},
            .has_entry = found,
            .length = static_cast<std::int16_t>(length),
            .resolved_set =
                static_cast<std::uint8_t>(Actors_lastResolvedSequenceIndex)};
  }
  bool CanUnwindBoundAnimation(AnimationActor& actor) override {
    Actors_ActorRecord& record = registry_.RecordOf(actor);
    const Synced synced(registry_);
    const std::optional<AnimationClipRef> bound =
        BoundLiveAnimationClip(record);
    return bound && FindClipEvents(*bound).status == ClipEventsStatus::kStarts;
  }
  int RestartAnimationEvents(AnimationActor& actor) override {
    Actors_ActorRecord& record = registry_.RecordOf(actor);
    const Synced synced(registry_);
    return RestartLiveAnimationEvents(record);
  }
  void InitializePose(AnimationActor& actor) override {
    Actors_ActorRecord& record = registry_.RecordOf(actor);
    const Synced synced(registry_);
    InitializeLiveActorAnimationPose(&record, &record.pose);
  }
  void BeginShadowBlend(AnimationActor& actor, int blend_frames) override {
    Actors_ActorRecord& record = registry_.RecordOf(actor);
    const Synced synced(registry_);
    BeginLiveActorShadowBlend(&record, blend_frames);
  }
  int RunBattleWindowEvents(AnimationActor& actor) override {
    Actors_ActorRecord& record = registry_.RecordOf(actor);
    const Synced synced(registry_);
    return RunLiveAnimationEvents(record, record.pose, kBattleWindowEvents);
  }
  void AdvanceSkeletalAnimation(AnimationActor& actor) override {
    Actors_ActorRecord& record = registry_.RecordOf(actor);
    const Synced synced(registry_);
    AdvanceLiveSkeletalAnimation(record, 1);
  }

  void CaptureBoneAnchor(AnimationActor& actor, int bone,
                         int lock_index) override {
    if (lock_index < 0 || lock_index >= kBonePoseLocks) {
      VS_LOG_WARN("actors", "Bone anchor refused lock {}", lock_index);
      return;
    }
    Actors_ActorRecord& record = registry_.RecordOf(actor);
    const Synced synced(registry_);
    int length = 0;
    static_cast<void>(
        LiveActorAnchorMatrix(record.instance.slot, bone,
                              &record.lockedBoneMatrices[lock_index], &length));
  }
  void SetBonePoseLockSlot(AnimationActor& actor, int bone,
                           int lock_slot) override {
    Actors_ActorRecord& record = registry_.RecordOf(actor);
    Actors_ShpModel* model = record.instance.model;
    if (model == nullptr || bone < 0) {
      VS_LOG_WARN("actors", "Bone pose lock refused bone {}", bone);
      return;
    }
    model->bones[bone].poseLockSlot = static_cast<u_char>(lock_slot);
  }

 private:
  AnimationRegistry registry_;
};

SoundEffectSlot SlotOf(const Actors_ModelEffectSlot& live) {
  return LoadSlot(live);
}
}  // namespace

// decomp: BATTLE.PRG func_800770FC (src/BATTLE/BATTLE.PRG/146C.c:6444)
void QueueLiveActorBaseSequenceLoad(Battle_Actor* /*actor*/, int actor_id) {
  LiveAnimationHost host;
  QueueActorBaseSequenceLoad(host, actor_id);
}

// decomp: BATTLE.PRG func_800771E0 (src/BATTLE/BATTLE.PRG/146C.c:6480)
void QueueLiveActorWeaponSequenceLoad(char* actor_bytes, int actor_id,
                                      int weapon_category, int spawn_flags) {
  std::optional<int> equipped;
  if (actor_bytes != nullptr) {
    equipped = reinterpret_cast<const Battle_Actor*>(actor_bytes)
                   ->equippedWeaponCategory;
  }
  LiveAnimationHost host;
  QueueActorWeaponSequenceLoad(host, equipped, actor_id, weapon_category,
                               spawn_flags);
}

// decomp: BATTLE.PRG func_8007C348 (src/BATTLE/BATTLE.PRG/146C.c:8738)
void InterruptLiveActorAction(int actor_id, int animate) {
  LiveAnimationHost host;
  InterruptActorAction(host, actor_id, animate);
}

// decomp: BATTLE.PRG func_8007CB84 (src/BATTLE/BATTLE.PRG/146C.c:8997)
void QueueLiveSharedSequenceLoad(int index, int weapon_id) {
  LiveAnimationHost host;
  QueueSharedSequenceLoad(host, index, weapon_id);
}

// decomp: BATTLE.PRG func_8007CBBC (src/BATTLE/BATTLE.PRG/146C.c:9008)
int UnloadLiveSharedSequence(int bank) {
  LiveAnimationHost host;
  return UnloadSharedSequence(host, bank);
}

// decomp: BATTLE.PRG func_8007CBDC (src/BATTLE/BATTLE.PRG/146C.c:9010)
void QueueLiveSharedSequenceWithTextures(int index, int weapon_id,
                                         int texture_slot) {
  LiveAnimationHost host;
  QueueSharedSequenceWithTextures(host, index, weapon_id, texture_slot);
}

// decomp: BATTLE.PRG vs_battle_loadEtm (src/BATTLE/BATTLE.PRG/146C.c:9022)
void QueueLiveSequenceEffectTextures(int index, int weapon_id,
                                     int texture_slot) {
  LiveAnimationHost host;
  QueueSequenceEffectTextures(host, index, weapon_id, texture_slot);
}

// decomp: BATTLE.PRG func_8007CC4C (src/BATTLE/BATTLE.PRG/146C.c:9033)
void ReleaseLiveSequenceEffectTextures(int bank) {
  LiveAnimationHost host;
  ReleaseSequenceEffectTextures(host, bank);
}

// decomp: BATTLE.PRG func_8007CC6C (src/BATTLE/BATTLE.PRG/146C.c:9035)
void SetLiveSharedSequenceBank(int bank) {
  LiveAnimationHost host;
  SetSharedSequenceBank(host, bank);
}

// decomp: BATTLE.PRG func_8007CC8C (src/BATTLE/BATTLE.PRG/146C.c:9037)
void ReadLiveSharedSequenceBank() {
  LiveAnimationHost host;
  SharedSequenceBank(host);
}

// decomp: BATTLE.PRG func_8007CEA0 (src/BATTLE/BATTLE.PRG/146C.c:9115)
void ReturnLiveActorToIdle(int actor_id) {
  LiveAnimationHost host;
  ReturnActorToIdle(host, actor_id);
}

// decomp: BATTLE.PRG func_8007CEC0 (src/BATTLE/BATTLE.PRG/146C.c:9117)
void ReturnAllLiveActorsToIdle() {
  LiveAnimationHost host;
  ReturnAllActorsToIdle(host);
}

// decomp: BATTLE.PRG func_8009CE9C (src/BATTLE/BATTLE.PRG/30DB0.c:1894)
int SelectLiveSharedSequenceBank(unsigned int bank) {
  LiveAnimationHost host;
  return SelectSharedSequenceBank(host, static_cast<int>(bank));
}

// decomp: BATTLE.PRG func_8009CFA0 (src/BATTLE/BATTLE.PRG/30DB0.c:1918)
int LiveSharedSequenceBankId() {
  LiveAnimationHost host;
  return SharedSequenceBankId(host);
}

// decomp: BATTLE.PRG func_8009D854 (src/BATTLE/BATTLE.PRG/30DB0.c:2188)
void RequestLiveActorAnimationAdvance(int actor_id) {
  LiveAnimationHost host;
  RequestActorAnimationAdvance(host, actor_id);
}

// decomp: BATTLE.PRG func_8009D934 (src/BATTLE/BATTLE.PRG/30DB0.c:2219)
void SetLiveActorWeaponDrawn(int actor_id, int weapon_drawn,
                             int transition_mode) {
  LiveAnimationHost host;
  SetActorWeaponDrawn(host, actor_id, weapon_drawn, transition_mode);
}

// decomp: BATTLE.PRG func_8009DBDC (src/BATTLE/BATTLE.PRG/30DB0.c:2296)
void PlayLiveActorWeaponDrawSound(int actor_id, int sound_offset) {
  LiveAnimationHost host;
  PlayActorWeaponDrawSound(host, actor_id, sound_offset);
}

// decomp: BATTLE.PRG func_8009E070 (src/BATTLE/BATTLE.PRG/30DB0.c:2451)
void PlayLiveActorActionPose(int actor_id, const std::int16_t* facing_target,
                             int pose_kind) {
  constexpr std::size_t kX = 0;
  constexpr std::size_t kZ = 2;
  std::optional<MotionVector> target;
  if (facing_target != nullptr) {
    target = MotionVector{.x = facing_target[kX], .z = facing_target[kZ]};
  }
  LiveAnimationHost host;
  PlayActorActionPose(host, actor_id, target ? &*target : nullptr, pose_kind);
}

// decomp: BATTLE.PRG func_8009E480 (src/BATTLE/BATTLE.PRG/30DB0.c:2619)
int IsLiveAshleyTransitionPose() {
  LiveAnimationHost host;
  return IsAshleyTransitionPose(host) ? 1 : 0;
}

// decomp: BATTLE.PRG func_8009E4B0 (src/BATTLE/BATTLE.PRG/30DB0.c:2627)
int IsLiveActorActionAnimationBusy(int actor_id) {
  LiveAnimationHost host;
  return static_cast<int>(IsActorActionAnimationBusy(host, actor_id));
}

// decomp: BATTLE.PRG func_8009E5C4 (src/BATTLE/BATTLE.PRG/30DB0.c:2685)
void RestoreLiveActorIdlePose(int actor_id) {
  LiveAnimationHost host;
  RestoreActorIdlePose(host, actor_id);
}

// decomp: BATTLE.PRG func_8009E614 (src/BATTLE/BATTLE.PRG/30DB0.c:2693)
void RestoreLiveActorNeutralPose(int actor_id) {
  LiveAnimationHost host;
  RestoreActorNeutralPose(host, actor_id);
}

// decomp: BATTLE.PRG func_8009E700 (src/BATTLE/BATTLE.PRG/30DB0.c:2727)
void RestoreLiveActorBasePose(int actor_id, int animation) {
  LiveAnimationHost host;
  RestoreActorBasePose(host, actor_id, animation);
}

// decomp: BATTLE.PRG func_8009F8DC (src/BATTLE/BATTLE.PRG/30DB0.c:3373)
int LiveAnimationBattleWindowFrame(int actor_id) {
  LiveAnimationHost host;
  return AnimationBattleWindowFrame(host, actor_id);
}

// decomp: BATTLE.PRG func_8009F990 (src/BATTLE/BATTLE.PRG/30DB0.c:3401)
void SetLiveActorSoundOverrides(int actor_id,
                                Actors_ModelEffectSlot* sound_override) {
  if (sound_override == nullptr) {
    VS_LOG_WARN("actors", "Sound overrides refused: null override");
    return;
  }
  LiveAnimationHost host;
  SetActorSoundOverrides(host, actor_id, SlotOf(*sound_override));
}

// decomp: BATTLE.PRG func_8009F9F4 (src/BATTLE/BATTLE.PRG/30DB0.c:3414)
void ApplyLiveActorSoundOverrides(int actor_id,
                                  Actors_ModelEffectSlot* sound_override) {
  if (sound_override == nullptr) {
    VS_LOG_WARN("actors", "Sound override apply refused: null override");
    return;
  }
  LiveAnimationHost host;
  ApplyActorSoundOverrides(host, actor_id, SlotOf(*sound_override));
}

// decomp: BATTLE.PRG func_8009FBD8 (src/BATTLE/BATTLE.PRG/30DB0.c:3480)
int SetLiveActorTravelAnimation(int actor_id, int animation) {
  LiveAnimationHost host;
  return SetActorTravelAnimation(host, actor_id, animation);
}

// decomp: BATTLE.PRG func_8009FC20 (src/BATTLE/BATTLE.PRG/30DB0.c:3498)
int LiveActorTravelAnimation(int actor_id, int* animation_out) {
  if (animation_out == nullptr) {
    VS_LOG_WARN("actors", "Travel animation refused: null output");
    return -1;
  }
  LiveAnimationHost host;
  const std::optional<int> animation = ActorTravelAnimation(host, actor_id);
  if (!animation) return -1;
  *animation_out = *animation;
  return 0;
}

// decomp: BATTLE.PRG func_800A01C8 (src/BATTLE/BATTLE.PRG/30DB0.c:3710)
int QueueLiveActorAnimation(int actor_id, int animation, int blend_frames,
                            int start_frame) {
  LiveAnimationHost host;
  return QueueActorAnimation(host, actor_id, animation, blend_frames,
                             start_frame);
}

// decomp: BATTLE.PRG func_800A0204 (src/BATTLE/BATTLE.PRG/30DB0.c:3725)
int PlayLiveActorAnimation(int actor_id, int animation, int /*unused*/,
                           int blend_frames) {
  LiveAnimationHost host;
  return PlayActorAnimation(host, actor_id, animation, blend_frames);
}

// decomp: BATTLE.PRG func_800A0618 (src/BATTLE/BATTLE.PRG/30DB0.c:3850)
int PlayLiveActorAnimationAtFrame(int actor_id, int animation, int blend_frames,
                                  int start_frame) {
  LiveAnimationHost host;
  return PlayActorAnimationAtFrame(host, actor_id, animation, blend_frames,
                                   start_frame);
}

// decomp: BATTLE.PRG func_800A0870 (src/BATTLE/BATTLE.PRG/30DB0.c:3948)
void SetLiveActorTransitionMode(int control) {
  LiveAnimationHost host;
  SetActorTransitionMode(host, control);
}

// decomp: BATTLE.PRG func_800A09D8 (src/BATTLE/BATTLE.PRG/30DB0.c:3987)
void SetLiveActorAnimationLocked(int actor_id, int locked) {
  LiveAnimationHost host;
  SetActorAnimationLocked(host, actor_id, locked);
}

// decomp: BATTLE.PRG func_800A249C (src/BATTLE/BATTLE.PRG/38C1C.c:578)
void PlayLiveActorTurnAnimation(int actor_id, int target_facing) {
  LiveAnimationHost host;
  PlayActorTurnAnimation(host, actor_id, target_facing);
}

// decomp: BATTLE.PRG func_800A6AA0 (src/BATTLE/BATTLE.PRG/3A1A0.c:2032)
int UpdateLiveActorLocomotionAnimation(Actors_ActorRecord* actor, int speed,
                                       int /*direction*/) {
  if (actor == nullptr) {
    VS_LOG_WARN("actors", "Locomotion animation refused: null actor");
    return 0;
  }
  LiveAnimationHost host;
  UpdateActorLocomotionAnimation(host, host.Attach(*actor), speed);
  return 0;
}

// decomp: BATTLE.PRG func_800AC440 (src/BATTLE/BATTLE.PRG/40564.c:2199)
void SetLiveActorBonePoseLock(Actors_ActorRecord* actor, int bone, int lock) {
  if (actor == nullptr) {
    VS_LOG_WARN("actors", "Bone pose lock refused: null actor");
    return;
  }
  LiveAnimationHost host;
  SetActorBonePoseLock(host, host.Attach(*actor), bone, lock);
}

// decomp: BATTLE.PRG func_800AC500 (src/BATTLE/BATTLE.PRG/40564.c:2221)
void ClearLiveActorBonePoseLocks(Actors_ActorRecord* actor) {
  if (actor == nullptr) {
    VS_LOG_WARN("actors", "Bone pose lock clear refused: null actor");
    return;
  }
  LiveAnimationHost host;
  ClearActorBonePoseLocks(host, host.Attach(*actor));
}
}  // namespace VS::Actors
