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
#include "Actors/ActorRecord.hpp"
#include "Actors/ActorStateLive.hpp"
#include "Actors/Animation.hpp"
#include "Actors/AnimationControl.hpp"
#include "Actors/AnimationPoseLive.hpp"
#include "Actors/AppearanceLive.hpp"
#include "Actors/Movement.hpp"
#include "Actors/SequencePlaybackLive.hpp"
#include "Actors/ShpModel.hpp"
#include "Actors/SkeletonLive.hpp"
#include "Actors/SkeletonLiveState.hpp"
#include "Actors/TextureAnimationLive.hpp"
#include "Actors/WeaponModel.hpp"
#include "Audio/ActorSoundLive.hpp"
#include "Battle/Actor.hpp"
#include "Battle/Ai/ActorCleanupLive.hpp"
#include "Battle/Damage.hpp"
#include "Content/ActorAssets.hpp"
#include "Content/ActorAssetsLive.hpp"
#include "Core/Log.hpp"
#include "Event/ScriptVmLive.hpp"
#include "Field/GimmickLookup.hpp"
#include "Field/Gimmicks.hpp"
#include "State/Timers.hpp"

namespace VS::Actors {
namespace {
constexpr std::size_t kActorSlots = std::size(VS::Actors::actorRecords);
// Every table slot plus one record a caller names outside the table.
constexpr std::size_t kRegistryCapacity = kActorSlots + 1;
constexpr int kQuadrants = 4;
// The weapon models' draw state (VS::Actors::WeaponModel reserved11,
// reserved12).
constexpr std::uint8_t kDrawnWeaponModel = 0x40;
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

SoundEffectSlot LoadSlot(const VS::Actors::ModelEffectSlot& live) {
  return {.kind = live.kind,
          .level = live.level,
          .flags = live.flags,
          .parameter = live.parameter};
}

void StoreSlot(const SoundEffectSlot& slot, VS::Actors::ModelEffectSlot& live) {
  live.kind = slot.kind;
  live.level = slot.level;
  live.flags = slot.flags;
  live.parameter = slot.parameter;
}

PoseEventCursor LoadCursor(const VS::Actors::AnimationPose& pose) {
  return {.script = HandleOf<EventScriptHandle>(pose.event_script),
          .next_event_frame = pose.next_event_frame};
}

void StoreCursor(const PoseEventCursor& cursor,
                 VS::Actors::AnimationPose& pose) {
  pose.event_script = PointerOf<std::uint8_t>(cursor.script);
  pose.next_event_frame = cursor.next_event_frame;
}

std::uint8_t Bit(bool value) { return value ? 1 : 0; }

AnimationActor LoadLiveAnimationActor(const VS::Actors::ActorRecord& live) {
  AnimationActor actor;
  actor.motion = LoadLiveActorMotion(live);
  const VS::Actors::ModelInstance& instance = live.instance;
  actor.weapon_drawn = instance.weapon_drawn != 0;
  actor.reaction_state = instance.reaction_state;
  actor.support_layer = instance.support_layer;
  actor.bone_count = instance.bone_count;
  actor.has_model = instance.model != nullptr;
  actor.animation_id = live.animation_id;
  actor.animation_set = live.animation_set;
  actor.animation_header = static_cast<SequenceHandle>(
      static_cast<std::uintptr_t>(live.animation_header));
  actor.animation_length = live.animation_length;
  actor.queued_animation = live.queued_animation;
  actor.queued_blend_frames = live.queued_blend_frames;
  actor.queued_start_frame = live.queued_start_frame;
  actor.travel_animation = live.travel_animation;
  actor.animation_locked = live.animation_locked != 0;
  actor.animation_event_flag = live.animation_event_flag != 0;
  actor.vertices_displaced = live.vertices_displaced != 0;
  actor.fast_animation = live.fast_animation != 0;
  actor.advance_requested = live.advance_requested != 0;
  actor.stride_parity = live.stride_parity;
  actor.idle_ticks = live.idle_ticks;
  actor.blend_frame = static_cast<std::uint8_t>(live.blend_frame);
  actor.blend_frames = static_cast<std::uint8_t>(live.blend_frames);
  actor.collision_radius = live.collision_radius;
  actor.move_facing = live.move_facing;
  actor.sequence_model_id = live.sequence_model_id;
  actor.model_id = live.model_id;
  actor.frames_per_step = live.frames_per_step;
  for (std::size_t channel = 0; channel < kTextureAnimationChannels;
       ++channel) {
    actor.texture_overlay_index.at(channel) =
        live.texture_animation_frames[channel].overlay_index;
    actor.texture_current_frame.at(channel) =
        live.texture_animation_slots[channel].current_frame;
  }
  for (std::size_t bank = 0; bank < kSharedSequenceBanks; ++bank) {
    // texture_row_offsets[2] aliases the low byte of momentum_distance.
    actor.texture_rows.at(bank) = live.texture_row_offsets[bank];
    actor.shared_sequences.at(bank) =
        HandleOf<SequenceHandle>(live.shared_sequences[bank]);
  }
  actor.sound_effect = LoadSlot(live.sound_effect);
  actor.sound_effect_target = LoadSlot(live.sound_effect_target);
  for (std::size_t lock = 0; lock < kBonePoseLocks; ++lock) {
    actor.locked_bones.at(lock) = live.locked_bones[lock];
  }
  actor.pose = LoadCursor(live.pose);
  actor.blend_pose = LoadCursor(live.blend_pose);
  actor.holding_last_frame = live.pose.holding_last_frame != 0;
  return actor;
}

// Stores the fields the controls write; the read-only fields stay as loaded.
void StoreLiveAnimationActor(const AnimationActor& actor,
                             VS::Actors::ActorRecord& live) {
  StoreLiveActorMotion(actor.motion, live);
  live.instance.weapon_drawn = Bit(actor.weapon_drawn);
  live.animation_id = actor.animation_id;
  live.animation_set = actor.animation_set;
  live.animation_header = static_cast<std::intptr_t>(
      static_cast<std::uintptr_t>(actor.animation_header));
  live.animation_length = actor.animation_length;
  live.queued_animation = actor.queued_animation;
  live.queued_blend_frames = actor.queued_blend_frames;
  live.queued_start_frame = actor.queued_start_frame;
  live.travel_animation = actor.travel_animation;
  live.animation_locked = Bit(actor.animation_locked);
  live.animation_event_flag = Bit(actor.animation_event_flag);
  live.vertices_displaced = Bit(actor.vertices_displaced);
  live.fast_animation = Bit(actor.fast_animation);
  live.advance_requested = Bit(actor.advance_requested);
  live.stride_parity = actor.stride_parity & 1U;
  live.idle_ticks = actor.idle_ticks;
  live.blend_frame = static_cast<char>(actor.blend_frame);
  live.blend_frames = static_cast<char>(actor.blend_frames);
  live.move_facing = actor.move_facing;
  live.frames_per_step = actor.frames_per_step;
  for (std::size_t channel = 0; channel < kTextureAnimationChannels;
       ++channel) {
    live.texture_animation_frames[channel].overlay_index =
        actor.texture_overlay_index.at(channel);
  }
  StoreSlot(actor.sound_effect, live.sound_effect);
  StoreSlot(actor.sound_effect_target, live.sound_effect_target);
  for (std::size_t lock = 0; lock < kBonePoseLocks; ++lock) {
    live.locked_bones[lock] = actor.locked_bones.at(lock);
  }
  StoreCursor(actor.pose, live.pose);
  StoreCursor(actor.blend_pose, live.blend_pose);
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

  AnimationActor& Attach(VS::Actors::ActorRecord& record) {
    for (std::size_t index = 0; index < count_; ++index) {
      if (entries_.at(index).record == &record) {
        return entries_.at(index).actor;
      }
    }
    if (count_ == entries_.size()) {
      VS::Core::LogError("actors",
                         "Actor animation touched more than {} records",
                         entries_.size());
      std::abort();
    }
    Entry& entry = entries_.at(count_++);
    entry.record = &record;
    entry.actor = LoadLiveAnimationActor(record);
    return entry.actor;
  }

  VS::Actors::ActorRecord& RecordOf(const AnimationActor& actor) {
    for (std::size_t index = 0; index < count_; ++index) {
      if (&entries_.at(index).actor == &actor) {
        return *entries_.at(index).record;
      }
    }
    VS::Core::LogError("actors",
                       "Actor animation named an actor it never loaded");
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
    VS::Actors::ActorRecord* record = nullptr;
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
int TableByte(const std::array<std::uint8_t, Size>& table, int index,
              const char* what) {
  if (index < 0 || static_cast<std::size_t>(index) >= Size) {
    VS::Core::LogWarn("actors", "{} refused index {}", what, index);
    return 0;
  }
  return table[index];
}

class LiveAnimationHost final : public AnimationControlHost {
 public:
  AnimationActor& Attach(VS::Actors::ActorRecord& record) {
    return registry_.Attach(record);
  }

  AnimationActor* Actor(int id) override {
    if (id < 0 || static_cast<std::size_t>(id) >= kActorSlots) {
      VS::Core::LogWarn("actors", "Actor animation refused actor id {}", id);
      return nullptr;
    }
    VS::Actors::ActorRecord* record = VS::Actors::actorRecords[id];
    return record != nullptr ? &registry_.Attach(*record) : nullptr;
  }
  bool HasBattleActor(int index) override {
    if (index < 0 ||
        static_cast<std::size_t>(index) >= std::size(VS::Battle::actors)) {
      return false;
    }
    return VS::Battle::actors[index] != nullptr;
  }
  int TickSpeed() override { return VS::State::tickSpeed; }
  int EventPlaybackState() override {
    const Synced synced(registry_);
    return static_cast<int>(
        VS::Event::EventPlaybackState(VS::Event::LiveScriptVm()));
  }
  std::optional<AnimationGimmick> GimmickForActorSlot(int slot) override {
    const VS::Field::GimmickInstance* gimmick =
        VS::Field::FindGimmickForActorSlot(slot);
    if (gimmick == nullptr) return std::nullopt;
    return AnimationGimmick{.mode = gimmick->mode,
                            .motion_frame = gimmick->object_info.motion_frame};
  }
  void SetGimmickMode(int slot, std::int16_t mode) override {
    VS::Field::GimmickInstance* gimmick =
        VS::Field::FindGimmickForActorSlot(slot);
    if (gimmick == nullptr) {
      VS::Core::LogWarn("actors", "Gimmick mode refused: no gimmick in slot {}",
                        slot);
      return;
    }
    gimmick->mode = mode;
  }

  int DrawnWeaponSoundBase(int sequence_model) override {
    return TableByte(VS::Actors::drawnWeaponAnimationBases, sequence_model,
                     "Drawn weapon sound");
  }
  int SheathedWeaponSoundBase(int sequence_model) override {
    return TableByte(VS::Actors::sheathedWeaponAnimationBases, sequence_model,
                     "Sheathed weapon sound");
  }
  int QuadrantMoveAnimation(bool fast, int quadrant) override {
    if (quadrant < 0 || quadrant >= kQuadrants) {
      VS::Core::LogWarn("actors", "Quadrant move animation refused quadrant {}",
                        quadrant);
      return 0;
    }
    return fast ? VS::Actors::fastQuadrantMoveAnimations[quadrant]
                : VS::Actors::slowQuadrantMoveAnimations[quadrant];
  }

  int SharedSequenceBankIndex() override {
    return VS::Actors::sharedSequenceBankIndex;
  }
  void SetSharedSequenceBankIndex(int index) override {
    VS::Actors::sharedSequenceBankIndex = static_cast<std::uint8_t>(index);
  }
  void SetTransitionControl(int control) override {
    VS::Actors::forcedModelOverrides = static_cast<char>(control);
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
      if (index < 0 || index >= kWeaponModelCount) {
        VS::Core::LogWarn("actors", "Weapon model draw refused actor id {}",
                          id);
        return;
      }
      VS::Actors::WeaponModel* model = weaponModels[index];
      if (model == nullptr) continue;
      const std::uint8_t state = drawn ? kDrawnWeaponModel : 0;
      model->blend_level = state;
      model->blend_target = state;
      model->shown = Bit(drawn);
    }
  }
  void PlaySoundPair(AnimationActor& actor, int first, int second) override {
    VS::Actors::ActorRecord& record = registry_.RecordOf(actor);
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
    VS::Actors::ActorRecord& record = registry_.RecordOf(actor);
    const Synced synced(registry_);
    CancelLiveActorTeleportTransition(&record);
  }
  void ReleaseAiDestinationTile(int tile_x, int tile_z) override {
    const Synced synced(registry_);
    Battle::Ai::ReleaseLiveAiDestinationTile(tile_x, tile_z);
  }
  void UploadTextureFrame(AnimationActor& actor, int channel,
                          int frame) override {
    VS::Actors::ActorRecord& record = registry_.RecordOf(actor);
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
    VS::Actors::ActorRecord& record = registry_.RecordOf(actor);
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
            .resolved_set = static_cast<std::uint8_t>(
                VS::Actors::lastResolvedSequenceIndex)};
  }
  bool CanUnwindBoundAnimation(AnimationActor& actor) override {
    VS::Actors::ActorRecord& record = registry_.RecordOf(actor);
    const Synced synced(registry_);
    const std::optional<AnimationClipRef> bound =
        BoundLiveAnimationClip(record);
    return bound && FindClipEvents(*bound).status == ClipEventsStatus::kStarts;
  }
  int RestartAnimationEvents(AnimationActor& actor) override {
    VS::Actors::ActorRecord& record = registry_.RecordOf(actor);
    const Synced synced(registry_);
    return RestartLiveAnimationEvents(record);
  }
  void InitializePose(AnimationActor& actor) override {
    VS::Actors::ActorRecord& record = registry_.RecordOf(actor);
    const Synced synced(registry_);
    InitializeLiveActorAnimationPose(&record, &record.pose);
  }
  void BeginShadowBlend(AnimationActor& actor, int blend_frames) override {
    VS::Actors::ActorRecord& record = registry_.RecordOf(actor);
    const Synced synced(registry_);
    BeginLiveActorShadowBlend(&record, blend_frames);
  }
  int RunBattleWindowEvents(AnimationActor& actor) override {
    VS::Actors::ActorRecord& record = registry_.RecordOf(actor);
    const Synced synced(registry_);
    return RunLiveAnimationEvents(record, record.pose, kBattleWindowEvents);
  }
  void AdvanceSkeletalAnimation(AnimationActor& actor) override {
    VS::Actors::ActorRecord& record = registry_.RecordOf(actor);
    const Synced synced(registry_);
    AdvanceLiveSkeletalAnimation(record, 1);
  }

  void CaptureBoneAnchor(AnimationActor& actor, int bone,
                         int lock_index) override {
    if (lock_index < 0 || lock_index >= kBonePoseLocks) {
      VS::Core::LogWarn("actors", "Bone anchor refused lock {}", lock_index);
      return;
    }
    VS::Actors::ActorRecord& record = registry_.RecordOf(actor);
    const Synced synced(registry_);
    int length = 0;
    static_cast<void>(LiveActorAnchorMatrix(
        record.instance.slot, bone, &record.locked_bone_matrices[lock_index],
        &length));
  }
  void SetBonePoseLockSlot(AnimationActor& actor, int bone,
                           int lock_slot) override {
    VS::Actors::ActorRecord& record = registry_.RecordOf(actor);
    VS::Actors::ShpModel* model = record.instance.model;
    if (model == nullptr || bone < 0) {
      VS::Core::LogWarn("actors", "Bone pose lock refused bone {}", bone);
      return;
    }
    model->bones[bone].pose_lock_slot = static_cast<std::uint8_t>(lock_slot);
  }

 private:
  AnimationRegistry registry_;
};

SoundEffectSlot SlotOf(const VS::Actors::ModelEffectSlot& live) {
  return LoadSlot(live);
}
}  // namespace

// decomp: BATTLE.PRG func_800770FC (src/BATTLE/BATTLE.PRG/146C.c:6254)
void QueueLiveActorBaseSequenceLoad(VS::Battle::Actor* /*actor*/,
                                    int actor_id) {
  LiveAnimationHost host;
  QueueActorBaseSequenceLoad(host, actor_id);
}

// decomp: BATTLE.PRG func_800771E0 (src/BATTLE/BATTLE.PRG/146C.c:6294)
void QueueLiveActorWeaponSequenceLoad(char* actor_bytes, int actor_id,
                                      int weapon_category, int spawn_flags) {
  std::optional<int> equipped;
  if (actor_bytes != nullptr) {
    equipped = reinterpret_cast<const VS::Battle::Actor*>(actor_bytes)
                   ->equipped_weapon_category;
  }
  LiveAnimationHost host;
  QueueActorWeaponSequenceLoad(host, equipped, actor_id, weapon_category,
                               spawn_flags);
}

// decomp: BATTLE.PRG triggerActorDeath (src/BATTLE/BATTLE.PRG/146C.c:8689; was
// func_8007C348)
void InterruptLiveActorAction(int actor_id, int animate) {
  LiveAnimationHost host;
  InterruptActorAction(host, actor_id, animate);
}

// decomp: BATTLE.PRG enqueueEsq (src/BATTLE/BATTLE.PRG/146C.c:8963; was
// func_8007CB84)
void QueueLiveSharedSequenceLoad(int index, int weapon_id) {
  LiveAnimationHost host;
  QueueSharedSequenceLoad(host, index, weapon_id);
}

// decomp: BATTLE.PRG unloadEsqAndEtm (src/BATTLE/BATTLE.PRG/146C.c:8974; was
// func_8007CBBC)
int UnloadLiveSharedSequence(int bank) {
  LiveAnimationHost host;
  return UnloadSharedSequence(host, bank);
}

// decomp: BATTLE.PRG enqueueEsqAndEtm (src/BATTLE/BATTLE.PRG/146C.c:8976; was
// func_8007CBDC)
void QueueLiveSharedSequenceWithTextures(int index, int weapon_id,
                                         int texture_slot) {
  LiveAnimationHost host;
  QueueSharedSequenceWithTextures(host, index, weapon_id, texture_slot);
}

// decomp: BATTLE.PRG enqueueEtm (src/BATTLE/BATTLE.PRG/146C.c:8988; was
// vs_battle_loadEtm)
void QueueLiveSequenceEffectTextures(int index, int weapon_id,
                                     int texture_slot) {
  LiveAnimationHost host;
  QueueSequenceEffectTextures(host, index, weapon_id, texture_slot);
}

// decomp: BATTLE.PRG unloadEtm (src/BATTLE/BATTLE.PRG/146C.c:8999; was
// func_8007CC4C)
void ReleaseLiveSequenceEffectTextures(int bank) {
  LiveAnimationHost host;
  ReleaseSequenceEffectTextures(host, bank);
}

// decomp: BATTLE.PRG selectEventData (src/BATTLE/BATTLE.PRG/146C.c:9001; was
// func_8007CC6C)
void SetLiveSharedSequenceBank(int bank) {
  LiveAnimationHost host;
  SetSharedSequenceBank(host, bank);
}

// decomp: BATTLE.PRG func_8007CC8C (src/BATTLE/BATTLE.PRG/146C.c:9003)
void ReadLiveSharedSequenceBank() {
  LiveAnimationHost host;
  SharedSequenceBank(host);
}

// decomp: BATTLE.PRG func_8007CEA0 (src/BATTLE/BATTLE.PRG/146C.c:9081)
void ReturnLiveActorToIdle(int actor_id) {
  LiveAnimationHost host;
  ReturnActorToIdle(host, actor_id);
}

// decomp: BATTLE.PRG func_8007CEC0 (src/BATTLE/BATTLE.PRG/146C.c:9083)
void ReturnAllLiveActorsToIdle() {
  LiveAnimationHost host;
  ReturnAllActorsToIdle(host);
}

// decomp: BATTLE.PRG func_8009CE9C (src/BATTLE/BATTLE.PRG/30DB0.c:1954)
int SelectLiveSharedSequenceBank(unsigned int bank) {
  LiveAnimationHost host;
  return SelectSharedSequenceBank(host, static_cast<int>(bank));
}

// decomp: BATTLE.PRG func_8009CFA0 (src/BATTLE/BATTLE.PRG/30DB0.c:1978)
int LiveSharedSequenceBankId() {
  LiveAnimationHost host;
  return SharedSequenceBankId(host);
}

// decomp: BATTLE.PRG func_8009D854 (src/BATTLE/BATTLE.PRG/30DB0.c:2252)
void RequestLiveActorAnimationAdvance(int actor_id) {
  LiveAnimationHost host;
  RequestActorAnimationAdvance(host, actor_id);
}

// decomp: BATTLE.PRG func_8009D934 (src/BATTLE/BATTLE.PRG/30DB0.c:2283)
void SetLiveActorWeaponDrawn(int actor_id, int weapon_drawn,
                             int transition_mode) {
  LiveAnimationHost host;
  SetActorWeaponDrawn(host, actor_id, weapon_drawn, transition_mode);
}

// decomp: BATTLE.PRG playWeaponSfx (src/BATTLE/BATTLE.PRG/30DB0.c:2361; was
// func_8009DBDC)
void PlayLiveActorWeaponDrawSound(int actor_id, int sound_offset) {
  LiveAnimationHost host;
  PlayActorWeaponDrawSound(host, actor_id, sound_offset);
}

// decomp: BATTLE.PRG func_8009E070 (src/BATTLE/BATTLE.PRG/30DB0.c:2517)
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

// decomp: BATTLE.PRG func_8009E480 (src/BATTLE/BATTLE.PRG/30DB0.c:2685)
int IsLiveAshleyTransitionPose() {
  LiveAnimationHost host;
  return IsAshleyTransitionPose(host) ? 1 : 0;
}

// decomp: BATTLE.PRG func_8009E4B0 (src/BATTLE/BATTLE.PRG/30DB0.c:2693)
int IsLiveActorActionAnimationBusy(int actor_id) {
  LiveAnimationHost host;
  return static_cast<int>(IsActorActionAnimationBusy(host, actor_id));
}

// decomp: BATTLE.PRG func_8009E5C4 (src/BATTLE/BATTLE.PRG/30DB0.c:2751)
void RestoreLiveActorIdlePose(int actor_id) {
  LiveAnimationHost host;
  RestoreActorIdlePose(host, actor_id);
}

// decomp: BATTLE.PRG func_8009E614 (src/BATTLE/BATTLE.PRG/30DB0.c:2759)
void RestoreLiveActorNeutralPose(int actor_id) {
  LiveAnimationHost host;
  RestoreActorNeutralPose(host, actor_id);
}

// decomp: BATTLE.PRG func_8009E700 (src/BATTLE/BATTLE.PRG/30DB0.c:2801)
void RestoreLiveActorBasePose(int actor_id, int animation) {
  LiveAnimationHost host;
  RestoreActorBasePose(host, actor_id, animation);
}

// decomp: BATTLE.PRG func_8009F8DC (src/BATTLE/BATTLE.PRG/30DB0.c:3472)
int LiveAnimationBattleWindowFrame(int actor_id) {
  LiveAnimationHost host;
  return AnimationBattleWindowFrame(host, actor_id);
}

void SetLiveActorSoundOverrides(int actor_id,
                                const SoundEffectSlot& sound_override) {
  LiveAnimationHost host;
  SetActorSoundOverrides(host, actor_id, sound_override);
}

// decomp: BATTLE.PRG setActorsSfxOverride (src/BATTLE/BATTLE.PRG/30DB0.c:3501;
// was func_8009F990)
void SetLiveActorSoundOverrides(int actor_id,
                                VS::Actors::ModelEffectSlot* sound_override) {
  if (sound_override == nullptr) {
    VS::Core::LogWarn("actors", "Sound overrides refused: null override");
    return;
  }
  SetLiveActorSoundOverrides(actor_id, SlotOf(*sound_override));
}

// decomp: BATTLE.PRG setActorSfxOverride (src/BATTLE/BATTLE.PRG/30DB0.c:3514;
// was func_8009F9F4)
void ApplyLiveActorSoundOverrides(int actor_id,
                                  VS::Actors::ModelEffectSlot* sound_override) {
  if (sound_override == nullptr) {
    VS::Core::LogWarn("actors", "Sound override apply refused: null override");
    return;
  }
  LiveAnimationHost host;
  ApplyActorSoundOverrides(host, actor_id, SlotOf(*sound_override));
}

// decomp: BATTLE.PRG setActorMoveAnimation (src/BATTLE/BATTLE.PRG/30DB0.c:3580;
// was func_8009FBD8)
int SetLiveActorTravelAnimation(int actor_id, int animation) {
  LiveAnimationHost host;
  return SetActorTravelAnimation(host, actor_id, animation);
}

// decomp: BATTLE.PRG func_8009FC20 (src/BATTLE/BATTLE.PRG/30DB0.c:3598)
int LiveActorTravelAnimation(int actor_id, int* animation_out) {
  if (animation_out == nullptr) {
    VS::Core::LogWarn("actors", "Travel animation refused: null output");
    return -1;
  }
  LiveAnimationHost host;
  const std::optional<int> animation = ActorTravelAnimation(host, actor_id);
  if (!animation) return -1;
  *animation_out = *animation;
  return 0;
}

// decomp: BATTLE.PRG queueActorAnimation (src/BATTLE/BATTLE.PRG/30DB0.c:3811;
// was func_800A01C8)
int QueueLiveActorAnimation(int actor_id, int animation, int blend_frames,
                            int start_frame) {
  LiveAnimationHost host;
  return QueueActorAnimation(host, actor_id, animation, blend_frames,
                             start_frame);
}

// decomp: BATTLE.PRG vs_battle_setActorAnimation
// (src/BATTLE/BATTLE.PRG/30DB0.c:3826; was func_800A0204)
int PlayLiveActorAnimation(int actor_id, int animation, int /*unused*/,
                           int blend_frames) {
  LiveAnimationHost host;
  return PlayActorAnimation(host, actor_id, animation, blend_frames);
}

// decomp: BATTLE.PRG setActorAnimationFromFrame
// (src/BATTLE/BATTLE.PRG/30DB0.c:3969; was func_800A0618)
int PlayLiveActorAnimationAtFrame(int actor_id, int animation, int blend_frames,
                                  int start_frame) {
  LiveAnimationHost host;
  return PlayActorAnimationAtFrame(host, actor_id, animation, blend_frames,
                                   start_frame);
}

// decomp: BATTLE.PRG func_800A0870 (src/BATTLE/BATTLE.PRG/30DB0.c:4067)
void SetLiveActorTransitionMode(int control) {
  LiveAnimationHost host;
  SetActorTransitionMode(host, control);
}

// decomp: BATTLE.PRG setActorInactiveFlag (src/BATTLE/BATTLE.PRG/30DB0.c:4106;
// was func_800A09D8)
void SetLiveActorAnimationLocked(int actor_id, int locked) {
  LiveAnimationHost host;
  SetActorAnimationLocked(host, actor_id, locked);
}

// decomp: BATTLE.PRG func_800A249C (src/BATTLE/BATTLE.PRG/38C1C.c:578)
void PlayLiveActorTurnAnimation(int actor_id, int target_facing) {
  LiveAnimationHost host;
  PlayActorTurnAnimation(host, actor_id, target_facing);
}

// decomp: BATTLE.PRG func_800A6AA0 (src/BATTLE/BATTLE.PRG/3A1A0.c:1459)
int UpdateLiveActorLocomotionAnimation(VS::Actors::ActorRecord* actor,
                                       int speed, int /*direction*/) {
  if (actor == nullptr) {
    VS::Core::LogWarn("actors", "Locomotion animation refused: null actor");
    return 0;
  }
  LiveAnimationHost host;
  UpdateActorLocomotionAnimation(host, host.Attach(*actor), speed);
  return 0;
}

// decomp: BATTLE.PRG func_800AC440 (src/BATTLE/BATTLE.PRG/40564.c:1902)
void SetLiveActorBonePoseLock(VS::Actors::ActorRecord* actor, int bone,
                              int lock) {
  if (actor == nullptr) {
    VS::Core::LogWarn("actors", "Bone pose lock refused: null actor");
    return;
  }
  LiveAnimationHost host;
  SetActorBonePoseLock(host, host.Attach(*actor), bone, lock);
}

// decomp: BATTLE.PRG func_800AC500 (src/BATTLE/BATTLE.PRG/40564.c:1924)
void ClearLiveActorBonePoseLocks(VS::Actors::ActorRecord* actor) {
  if (actor == nullptr) {
    VS::Core::LogWarn("actors", "Bone pose lock clear refused: null actor");
    return;
  }
  LiveAnimationHost host;
  ClearActorBonePoseLocks(host, host.Attach(*actor));
}
}  // namespace VS::Actors
