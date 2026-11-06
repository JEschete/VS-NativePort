// Hand-maintained (tools/port/owned.csv).
// Live binding for the typed animation pose (AnimationPose.cpp) and event
// script (AnimationEvents.cpp): actor records and their poses, the SEQ banks
// they bind, the module tables the events read, and the model workspace.
#include "Actors/AnimationPoseLive.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <type_traits>

#include "Actors/ActorMotionLive.hpp"
#include "Actors/ActorTableLive.hpp"
#include "Actors/AnimationControlLive.hpp"
#include "Actors/AnimationEvents.hpp"
#include "Actors/ModelRenderingLive.hpp"
#include "Actors/MotionRequestsLive.hpp"
#include "Actors/SequencePlaybackLive.hpp"
#include "Actors/TextureAnimationLive.hpp"
#include "Audio/ActorSoundLive.hpp"
#include "Audio/SfxLive.hpp"
#include "Core/Math/MatrixLive.hpp"
#include "Field/CollisionMapLive.hpp"
#include "Render/ModelGeometryLive.hpp"
extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/ModelInstance.h"
#include "Actors/Movement.h"
#include "Actors/SkeletonLiveState.h"
#include "Actors/WeaponModel.h"
#include "Battle/ActionAnimation.h"
#include "Core/Diagnostics.h"
#include "Effects/ActionEffects.h"
#include "Effects/Runtime.h"
#include "Event/ScriptVm.h"
#include "Field/Gimmicks.h"
#include "Render/ModelDraw.h"
#include "Render/ModelWorkspace.h"
}

namespace VS::Actors {
namespace {
using Core::Math::Transform3;
using Core::Math::Vec3;

constexpr const char* kChannel = "actors.sequence";
constexpr const char* kLookOperation = "UpdateMotionAndLookAt";
constexpr const char* kCompanionOperation = "FinishCompanionAction";

// Ids 220..253 play from the selected shared bank; 254 and 255 are the
// special clips of shared slot 3.
constexpr int kFirstSharedAnimation = 220;
constexpr int kFirstSpecialAnimation = 254;
// The shared slots a displayed clip can be bound from.
constexpr std::size_t kBoundSharedSlots = 3;
// Finished steps, falls and reactions store this to reinitialize the next
// clip. The old pose stays bound, so it must still unwind (#252).
constexpr std::uint8_t kReinitializeSet = 0xFF;

// The opcodes of the event length table (D_800E9298) and the sequence models
// of the drawn weapon sounds (D_800E92DC).
constexpr std::size_t kAnimationEventOpcodes = 68;
constexpr std::size_t kDrawnWeaponSounds = 12;
static_assert(std::extent_v<decltype(Actors_animationEventLengths)> >=
              kAnimationEventOpcodes);
static_assert(std::extent_v<decltype(Actors_drawnWeaponSoundIds)> >=
              kDrawnWeaponSounds);
static_assert(std::extent_v<decltype(Actors_animationEventPoints)> >=
              kAnimationEventPointCount);
static_assert(std::tuple_size_v<ShadowPointWords> ==
              std::extent_v<decltype(Actors_animationEventPoints), 1>);

// The event playback phase in which the events and head look always run.
constexpr std::int16_t kEventPlaybackRunning = 4;
// Effects_effectBusyFlags while an action's effects play.
constexpr int kEffectsBusy = 1;
// Event 29's palette fade: from 0 over 0x20 steps.
constexpr u_char kPaletteFadeSteps = 0x20;
// The preview model faces its first bone forward.
constexpr std::int16_t kPreviewFacing = 2048;
constexpr int kLockedBoneMatrices = 2;

u_char Bit(bool value) { return value ? 1 : 0; }

std::intptr_t BankHandle(const void* bank) {
  return reinterpret_cast<std::intptr_t>(bank);
}

bool MatchesBoundSet(int clip, std::uint8_t bound_set) {
  return bound_set == kReinitializeSet || clip == bound_set;
}

void SetLastResolvedClip(int clip) {
  Actors_lastResolvedSequenceIndex =
      std::bit_cast<char>(static_cast<std::uint8_t>(clip));
}

// The actor's banks by entry, remembering the address of the last one asked
// for: the record holds them as writable addresses, which the event script
// cursor keeps.
class LiveAnimationBanks final : public AnimationBankSource {
 public:
  explicit LiveAnimationBanks(const Actors_ActorRecord& actor)
      : actor_(actor) {}

  [[nodiscard]] AnimationBankView Bank(const AnimationEntry& entry) override {
    address_ = static_cast<u_char*>(Address(entry));
    return {.loaded = address_ != nullptr,
            .bytes = SequenceBankBytes(address_)};
  }
  [[nodiscard]] u_char* LastAddress() const { return address_; }

 private:
  [[nodiscard]] void* Address(const AnimationEntry& entry) const {
    switch (entry.bank) {
      case AnimationBank::kOwned:
        return actor_.ownedSequence;
      case AnimationBank::kCommon:
        return actor_.commonSequence;
      case AnimationBank::kBattle:
        return actor_.battleSequence;
      case AnimationBank::kShared:
        return SharedBank(entry.shared_slot);
      case AnimationBank::kWork:
        return Actors_sequenceWorkBuffer;
    }
    return nullptr;
  }
  [[nodiscard]] void* SharedBank(int slot) const {
    if (slot < 0 ||
        static_cast<std::size_t>(slot) >= std::size(actor_.sharedSequences)) {
      Core_Errorf(kChannel, "actor=%u shared SEQ slot %d refused",
                  static_cast<unsigned>(actor_.instance.slot), slot);
      return nullptr;
    }
    return actor_.sharedSequences[slot];
  }

  const Actors_ActorRecord& actor_;
  u_char* address_ = nullptr;
};

struct LiveHeader {
  AnimationHeader header;
  // The bank's writable address, when an entry selected one.
  u_char* address = nullptr;
};

// decomp: BATTLE.PRG func_800AD494 (src/BATTLE/BATTLE.PRG/40564.c:2719)
LiveHeader FindLiveHeader(const Actors_ActorRecord& actor, int animation_id) {
  LiveAnimationBanks banks(actor);
  LiveHeader found{.header = FindActorAnimationHeader(
                       LiveAnimationBankLimits(actor), banks, animation_id),
                   .address = banks.LastAddress()};
  ReportLiveAnimationHeader(found.address, animation_id, found.header.status);
  switch (found.header.status) {
    case AnimationHeaderStatus::kFound:
    case AnimationHeaderStatus::kNoClip:
    case AnimationHeaderStatus::kHeaderOutsideBank:
      SetLastResolvedClip(found.header.clip);
      break;
    case AnimationHeaderStatus::kNoEntry:
    case AnimationHeaderStatus::kNoBank:
    case AnimationHeaderStatus::kUnregisteredBank:
    case AnimationHeaderStatus::kMapOutsideBank:
      break;
  }
  return found;
}

struct LiveBoundClip {
  AnimationClipRef clip;
  u_char* address = nullptr;
};

void ReportBoundLookup(const void* bank, int animation,
                       const ClipLookup& lookup, bool registered) {
  if (!registered) {
    ReportLiveAnimationHeader(bank, animation,
                              AnimationHeaderStatus::kUnregisteredBank);
    return;
  }
  switch (lookup.status) {
    case ClipLookupStatus::kMapOutsideBank:
      ReportLiveAnimationHeader(bank, animation,
                                AnimationHeaderStatus::kMapOutsideBank);
      return;
    case ClipLookupStatus::kHeaderOutsideBank:
      ReportLiveAnimationHeader(bank, animation,
                                AnimationHeaderStatus::kHeaderOutsideBank);
      return;
    case ClipLookupStatus::kFound:
    case ClipLookupStatus::kNoClip:
      return;
  }
}

// The shared bank (slots 0..2) the record's animation header names.
u_char* BoundSharedBank(const Actors_ActorRecord& actor) {
  if (actor.animationHeader == 0) {
    return nullptr;
  }
  for (std::size_t slot = 0; slot < kBoundSharedSlots; ++slot) {
    if (BankHandle(actor.sharedSequences[slot]) == actor.animationHeader) {
      return static_cast<u_char*>(actor.sharedSequences[slot]);
    }
  }
  return nullptr;
}

// F00998/44F14.c:610 and F00279/146C.c:9008 must undo the bound old pose.
// The selector used for new clips can change before either consumer runs, so
// a shared clip is looked up in the bank the record bound, not the selected
// shared slot.
std::optional<LiveBoundClip> FindLiveBoundClip(
    const Actors_ActorRecord& actor) {
  const int animation = actor.animationId;
  const int bones = actor.instance.boneCount;
  if (animation < kFirstSharedAnimation ||
      animation >= kFirstSpecialAnimation) {
    const LiveHeader found = FindLiveHeader(actor, animation);
    if (found.header.status != AnimationHeaderStatus::kFound ||
        BankHandle(found.address) != actor.animationHeader ||
        !MatchesBoundSet(found.header.clip, actor.animationSet)) {
      return std::nullopt;
    }
    return LiveBoundClip{.clip = {.bank = found.header.bank,
                                  .header_offset = found.header.header_offset,
                                  .bones = bones},
                         .address = found.address};
  }
  u_char* bound = BoundSharedBank(actor);
  if (bound == nullptr) {
    return std::nullopt;
  }
  const std::span<const std::uint8_t> bank = SequenceBankBytes(bound);
  const ClipLookup lookup =
      LookupAnimationClip(bank, animation - kFirstSharedAnimation, bones);
  ReportBoundLookup(bound, animation, lookup, !bank.empty());
  if (lookup.status != ClipLookupStatus::kFound ||
      !MatchesBoundSet(lookup.clip, actor.animationSet)) {
    return std::nullopt;
  }
  SetLastResolvedClip(lookup.clip);
  return LiveBoundClip{.clip = {.bank = bank,
                                .header_offset = lookup.header_offset,
                                .bones = bones},
                       .address = bound};
}

const char* Describe(AnimationEventStatus status) {
  switch (status) {
    case AnimationEventStatus::kDone:
      return "done";
    case AnimationEventStatus::kNoScript:
      return "has a script in no registered bank";
    case AnimationEventStatus::kScriptOverrun:
      return "runs past its bank";
    case AnimationEventStatus::kReloadLimit:
      return "reloaded past the reload limit";
    case AnimationEventStatus::kDispatchLimit:
      return "dispatched past the event limit";
    case AnimationEventStatus::kTableOutOfRange:
      return "has an opcode past the length table";
    case AnimationEventStatus::kPointOutOfRange:
      return "names a point past the point table";
    case AnimationEventStatus::kVertexOutOfRange:
      return "moves a vertex outside the model";
    case AnimationEventStatus::kSoundOutOfRange:
      return "names a weapon sound past its table";
    case AnimationEventStatus::kNoSoundTarget:
      return "has no record to place a sound at";
    case AnimationEventStatus::kNoCompanion:
      return "names a companion without a record";
  }
  return "is invalid";
}

struct EventProblem {
  unsigned actor = 0;
  unsigned animation = 0;
  AnimationEventStatus status = AnimationEventStatus::kDone;
  std::uint8_t op = 0;

  friend bool operator==(const EventProblem&, const EventProblem&) = default;
};

// Reports each problem once until a different one occurs.
void ReportEventProblemOnce(const EventProblem& problem) {
  static std::optional<EventProblem> last;
  if (last == problem) {
    return;
  }
  last = problem;
  Core_Errorf(kChannel, "actor=%u animation=%u event %u %s; stopped or skipped",
              problem.actor, problem.animation,
              static_cast<unsigned>(problem.op), Describe(problem.status));
}

// The module tables the events read: the lengths and weapon sounds in place,
// the event points copied out of their halfword rows.
class LiveEventTables {
 public:
  LiveEventTables() {
    for (std::size_t row = 0; row < kAnimationEventPointCount; ++row) {
      std::ranges::copy(Actors_animationEventPoints[row], points_[row].begin());
    }
  }

  [[nodiscard]] AnimationEventTables View() const {
    return {
        .lengths = std::span<const std::uint8_t>(Actors_animationEventLengths)
                       .first(kAnimationEventOpcodes),
        .weapon_sounds =
            std::span<const std::uint8_t>(Actors_drawnWeaponSoundIds)
                .first(kDrawnWeaponSounds),
        .points = points_};
  }

 private:
  std::array<ShadowPointWords, kAnimationEventPointCount> points_{};
};

bool EventPlaybackRunning() {
  return static_cast<std::int16_t>(Event_EventPlaybackState()) ==
         kEventPlaybackRunning;
}

// The record and pose an event run acts on. The script cursor is kept as an
// address into its bank: the pose's own bank, or the bound bank a restart
// starts it in.
class LiveEventHost final : public AnimationEventHost {
 public:
  LiveEventHost(Actors_ActorRecord& actor, Actors_AnimationPose& pose,
                u_char* bound_bank)
      : actor_(actor),
        pose_(pose),
        bound_bank_(bound_bank),
        id_(static_cast<int>(actor.instance.slot)) {}

  [[nodiscard]] std::optional<EventScriptPosition> Script() const override {
    const auto location = LocateInSequenceBank(pose_.eventScript);
    if (!location) {
      return std::nullopt;
    }
    return EventScriptPosition{.bank = location->bank,
                               .offset = location->offset};
  }
  void StoreCursor(const EventScriptPosition& position,
                   std::uint16_t frame) override {
    u_char* bank = WritableBank(position.bank);
    if (bank == nullptr) {
      ReportEventProblem(AnimationEventStatus::kNoScript, 0);
      return;
    }
    pose_.eventScript = bank + position.offset;
    pose_.nextEventFrame = frame;
  }
  [[nodiscard]] std::uint16_t NextEventFrame() const override {
    return pose_.nextEventFrame;
  }
  void SetNextEventFrame(std::uint16_t frame) override {
    pose_.nextEventFrame = frame;
  }
  [[nodiscard]] std::int16_t Frame() const override { return pose_.frame; }
  void SetFrame(std::int16_t frame) override { pose_.frame = frame; }
  [[nodiscard]] std::uint8_t EventWait() const override {
    return pose_.eventWait;
  }
  void SetEventWait(std::uint8_t wait) override { pose_.eventWait = wait; }

  [[nodiscard]] int AnimationId() const override { return actor_.animationId; }
  [[nodiscard]] std::uint16_t AnimationLength() const override {
    return static_cast<std::uint16_t>(actor_.animationLength);
  }
  [[nodiscard]] bool EventFlag() const override {
    return actor_.animationEventFlag != 0;
  }
  void SetEventFlag(bool flag) override {
    actor_.animationEventFlag = Bit(flag);
  }
  [[nodiscard]] bool AdvanceRequested() const override {
    return actor_.advanceRequested != 0;
  }
  void ClearAdvanceRequest() override { actor_.advanceRequested = 0; }
  [[nodiscard]] bool WeaponDrawn() const override {
    return actor_.instance.weaponDrawn != 0;
  }
  [[nodiscard]] std::uint8_t SequenceModelId() const override {
    return actor_.sequenceModelId;
  }
  [[nodiscard]] bool EventPlaybackActive() const override {
    return EventPlaybackRunning();
  }
  [[nodiscard]] bool EffectsBusy() const override {
    return Effects_effectBusyFlags == kEffectsBusy;
  }
  [[nodiscard]] ActorMotion LoadMotion() const override {
    return LoadLiveActorMotion(actor_);
  }
  void StoreMotion(const ActorMotion& motion) override {
    StoreLiveActorMotion(motion, actor_);
  }
  [[nodiscard]] std::uint8_t CompanionSlot() const override {
    return actor_.companionSlot;
  }
  bool FinishCompanionAction(std::uint8_t slot) override {
    Actors_ActorRecord* companion = LiveActorRecord(slot, kCompanionOperation);
    if (companion == nullptr) {
      return false;
    }
    ActorMotion motion = LoadLiveActorMotion(*companion);
    motion.action_animation_state = kFinishedActionState;
    StoreLiveActorMotion(motion, *companion);
    return true;
  }
  void SetFastAnimation(bool fast) override {
    actor_.fastAnimation = Bit(fast);
  }

  bool QueueImpactEffects() override {
    return Effects_QueueActionImpactEffects(id_) != 0;
  }
  void PlayAnimation(int animation, int blend_frames) override {
    static_cast<void>(PlayLiveActorAnimation(id_, animation, 0, blend_frames));
  }
  void InitializePose() override {
    InitializeLiveActorAnimationPose(&actor_, &pose_);
  }

  [[nodiscard]] std::optional<Audio::SfxMix> SoundMix() override {
    return Audio::LiveActorSoundMix(id_);
  }
  void PlaySfx(std::int32_t group, int effect,
               const Audio::SfxMix& mix) override {
    Audio::LivePlaySfx(group, effect, mix.pan, mix.volume);
  }
  void StopSfx(std::int32_t group, int effect) override {
    Audio::LiveStopSfx(group, effect);
  }
  void PlaySoundPair(int sound) override {
    Audio::LivePlayActorSoundPair(&actor_, sound, 0);
  }
  void PlayModelSound(int sound) override {
    Audio::LivePlayActorModelSound(&actor_.instance, sound);
  }
  void PlaySurfaceContactSound(int contact) override {
    Audio::LivePlayActorSurfaceContactSound(id_, &actor_, contact);
  }

  void FadeAttachedWeapons(const WeaponFade& fade, bool show) override {
    for (Actors_WeaponModel* weapon : Actors_weaponModels) {
      if (weapon == nullptr || weapon->boneCount == 0 ||
          weapon->attachmentBone == 0 || weapon->actorId != id_) {
        continue;
      }
      if (show) {
        weapon->reserved08Bit4 = 1;
      }
      weapon->reserved11 = fade.scale;
      weapon->reserved12 = fade.target;
      weapon->reserved13 = fade.frames;
    }
  }
  void SetTextureCellSheet(int channel,
                           const TextureCellSheet& sheet) override {
    if (channel < 0 || static_cast<std::size_t>(channel) >=
                           std::size(actor_.textureAnimationFrames)) {
      return;
    }
    Actors_TextureAnimationFrame& frame =
        actor_.textureAnimationFrames[channel];
    frame.overlayIndex = sheet.overlay;
    frame.sourceX = sheet.source_x;
    frame.sourceY = sheet.source_y;
    frame.columns = sheet.columns;
  }
  void SetTextureAnimation(int channel, int timeline,
                           TextureAnimationCommand command) override {
    static_cast<void>(SetLiveActorTextureAnimation(id_, channel, timeline,
                                                   static_cast<int>(command)));
  }
  void UploadTextureOverlay(const TextureOverlayCopy& copy) override {
    UploadLiveActorTextureOverlay(id_, copy);
  }
  void StartPaletteFade() override {
    actor_.paletteFadeActive = 1;
    actor_.paletteFadeState[0] = 0;
    actor_.paletteFadeReverse = 0;
    actor_.paletteFadeState[1] = kPaletteFadeSteps;
  }
  bool MoveModelVertex(std::size_t vertex,
                       const Render::ModelVertex& delta) override {
    return Render::MoveLiveActorModelVertex(actor_, vertex, delta);
  }
  void MarkPoseOverridden() override { actor_.poseOverrideActive = 1; }
  [[nodiscard]] ShadowBlend LoadShadow() const override {
    return LoadLiveShadowBlend(actor_.shadowBlend);
  }
  void StoreShadow(const ShadowBlend& shadow) override {
    StoreLiveShadowBlend(shadow, actor_.shadowBlend);
  }
  void LockBonePose(int bone, bool locked) override {
    SetLiveActorBonePoseLock(&actor_, bone, locked ? 1 : 0);
  }

  void StartWeaponTrail(int trail) override {
    Effects_StartWeaponTrail(id_, trail);
  }
  void FadeWeaponTrails(int frames) override {
    Effects_FadeWeaponTrails(id_, frames);
  }
  void StartEffectTrack(int track) override {
    static_cast<void>(Effects_StartEffectTrack(track));
  }
  void SpawnSourceEffect() override { Effects_SpawnSourceActorEffect(); }
  void ApplyGimmickInteraction(int command) override {
    Field_ApplyGimmickInteractionCommand(&actor_, command);
  }

  void ReportEventProblem(AnimationEventStatus status,
                          std::uint8_t op) override {
    ReportEventProblemOnce({.actor = static_cast<unsigned>(id_),
                            .animation = actor_.animationId,
                            .status = status,
                            .op = op});
  }

 private:
  // The event 58 action state of a finished companion.
  static constexpr std::uint8_t kFinishedActionState = 2;

  // The writable address of `bank`: the bound bank, or the bank the pose's
  // script lies in.
  [[nodiscard]] u_char* WritableBank(std::span<const std::uint8_t> bank) const {
    if (bound_bank_ != nullptr && bound_bank_ == bank.data()) {
      return bound_bank_;
    }
    const auto location = LocateInSequenceBank(pose_.eventScript);
    if (location && location->bank.data() == bank.data()) {
      return pose_.eventScript - location->offset;
    }
    return nullptr;
  }

  Actors_ActorRecord& actor_;
  Actors_AnimationPose& pose_;
  u_char* bound_bank_;
  int id_;
};

AnimationPoseState LoadPoseState(const Actors_AnimationPose& pose) {
  return {.pose = LoadLiveSkeletalPose(pose),
          .frame = pose.frame,
          .next_event_frame = pose.nextEventFrame,
          .event_offset = std::nullopt,
          .event_wait = pose.eventWait,
          .holding_last_frame = pose.holdingLastFrame != 0,
          .animation_id = pose.animationId};
}

void StorePoseState(const AnimationPoseState& state, u_char* bank,
                    Actors_AnimationPose& pose) {
  StoreLiveSkeletalPose(state.pose, pose);
  pose.frame = state.frame;
  pose.nextEventFrame = state.next_event_frame;
  pose.eventWait = state.event_wait;
  pose.holdingLastFrame = state.holding_last_frame ? -1 : 0;
  pose.animationId = state.animation_id;
  if (state.event_offset && bank != nullptr) {
    pose.eventScript = bank + *state.event_offset;
  }
}

class LiveMotionLookHost final : public MotionLookHost {
 public:
  LiveMotionLookHost(Actors_ActorRecord& actor,
                     Render_ModelScratchpad& workspace)
      : actor_(actor), workspace_(workspace) {}

  [[nodiscard]] ActorMotion LoadMotion() const override {
    return LoadLiveActorMotion(actor_);
  }
  void StoreMotion(const ActorMotion& motion) override {
    StoreLiveActorMotion(motion, actor_);
  }
  [[nodiscard]] Vec3 RootKey() const override {
    const SVECTOR& root = actor_.blendPose.boneKeys[kRootPoseKey];
    return {root.vx, root.vy, root.vz};
  }
  [[nodiscard]] std::int32_t ActorScale() const override {
    return actor_.scale;
  }
  void SetRootOffset(Vec3 offset) override {
    workspace_.rootOffset.vx = static_cast<std::int16_t>(offset.x);
    workspace_.rootOffset.vy = static_cast<std::int16_t>(offset.y);
    workspace_.rootOffset.vz = static_cast<std::int16_t>(offset.z);
  }
  [[nodiscard]] bool PreviewPose() const override {
    return actor_.previewPose != 0;
  }
  void CenterPreviewPose() override {
    actor_.blendPose.boneKeys[0].vx = kPreviewFacing;
    actor_.pose.boneKeys[0].vx = kPreviewFacing;
  }
  void FinishLocomotion() override { actor_.animationSet = kReinitializeSet; }
  void SettleOnTile(int tile_x, int tile_z) override {
    static_cast<void>(Field::LiveGimmickSupportHeight(tile_x, tile_z));
    actor_.supportGimmick = Actors_supportingGimmickIndex;
  }
  void TransformRootOffset() override { TransformLiveActorRootOffset(&actor_); }
  void SaveGroundedTransform() override {
    SaveLiveActorGroundedTransform(&actor_);
  }
  [[nodiscard]] HeadLook LoadHeadLook() const override {
    return {.target = actor_.headTrackTarget,
            .tracking = LiveMotionVector(actor_.headTracking),
            .yaw = static_cast<std::int16_t>(actor_.instance.lookYaw),
            .pitch = static_cast<std::int16_t>(actor_.instance.lookPitch)};
  }
  void StoreHeadLook(const HeadLook& look) override {
    actor_.headTrackTarget = look.target;
    actor_.headTracking = LiveSvector(look.tracking);
    actor_.instance.lookYaw = static_cast<u_short>(look.yaw);
    actor_.instance.lookPitch = static_cast<u_short>(look.pitch);
  }
  [[nodiscard]] HeadLookContext LookContext(const HeadLook& look) override {
    const Actors_ModelInstance& instance = actor_.instance;
    HeadLookContext context{.event_playback_active = EventPlaybackRunning(),
                            .was_drawn = instance.wasDrawn != 0,
                            .animation_id = actor_.animationId,
                            .is_jumping = instance.isJumping != 0,
                            .motion_locked = instance.motionLocked != 0,
                            .position = LiveMotionVector(instance.position),
                            .rotation_x = instance.rotationX,
                            .facing = instance.facing,
                            .target_position = std::nullopt};
    if (look.target >= 0) {
      if (const Actors_ActorRecord* target =
              LiveActorRecord(look.target, kLookOperation)) {
        context.target_position = LiveMotionVector(target->instance.position);
      }
    }
    return context;
  }
  void SetLookAngles(LookAngles angles) override {
    workspace_.angleX = static_cast<u_short>(angles.yaw);
    workspace_.angleY = static_cast<u_short>(angles.pitch);
  }

 private:
  Actors_ActorRecord& actor_;
  Render_ModelScratchpad& workspace_;
};

void UpdateLiveMotion(Actors_ActorRecord& actor, bool skip_tweens) {
  Render_ModelScratchpad* workspace = Render_ModelWorkspace();
  if (workspace == nullptr) {
    Core_Errorf(kChannel, "Motion update has no model workspace");
    return;
  }
  LiveMotionLookHost host(actor, *workspace);
  UpdateMotionAndLookAt(host, skip_tweens);
}

class LiveBlendHost final : public AnimationBlendHost {
 public:
  explicit LiveBlendHost(Actors_ActorRecord& actor) : actor_(actor) {}

  void AdvanceAnimation() override { AdvanceLiveSkeletalAnimation(actor_, 0); }
  [[nodiscard]] BlendClock Clock() const override {
    return {.frame = static_cast<std::int8_t>(actor_.blendFrame),
            .frames = static_cast<std::int8_t>(actor_.blendFrames)};
  }
  void SetClock(BlendClock clock) override {
    actor_.blendFrame = static_cast<char>(clock.frame);
    actor_.blendFrames = static_cast<char>(clock.frames);
  }
  [[nodiscard]] int FramesPerStep() const override {
    return actor_.framesPerStep;
  }
  void AlignBlendRotations() override {
    PoseKeys keys = LoadLivePoseKeys(actor_.blendPose);
    AlignPoseRotationBranches(keys.rotations,
                              LoadLivePoseKeys(actor_.pose).rotations,
                              actor_.instance.boneCount);
    StoreLivePoseKeys(keys, actor_.blendPose);
  }
  void BlendTowardPose(BlendClock clock) override {
    BlendLiveActorPose(actor_.blendPose, actor_.pose, actor_.instance.boneCount,
                       clock.frame, clock.frames);
    actor_.blendPose.holdingLastFrame = actor_.pose.holdingLastFrame;
    actor_.blendPose.reachedEnd = actor_.pose.reachedEnd;
  }
  void CopyPoseToBlend() override { actor_.blendPose = actor_.pose; }
  void UpdateMotion() override { UpdateLiveMotion(actor_, false); }

 private:
  Actors_ActorRecord& actor_;
};
}  // namespace

AnimationBankLimits LiveAnimationBankLimits(const Actors_ActorRecord& actor) {
  return {
      .bones = actor.instance.boneCount,
      .slot = actor.instance.slot,
      .model_id = actor.modelId,
      .sequence_model_id = actor.sequenceModelId,
      .common_entries = actor.sequenceFlags,
      .battle_entries = actor.commonAnimationCount,
      .special_entries = actor.battleAnimationCount,
      .special_pitch = std::bit_cast<std::uint8_t>(Battle_specialActionPitch),
      .shared_slot = Actors_sharedSequenceBankIndex};
}

AnimationHeader FindLiveAnimationHeader(const Actors_ActorRecord& actor,
                                        int animation_id) {
  return FindLiveHeader(actor, animation_id).header;
}

std::optional<AnimationClipRef> BoundLiveAnimationClip(
    const Actors_ActorRecord& actor) {
  const auto bound = FindLiveBoundClip(actor);
  if (!bound) {
    return std::nullopt;
  }
  return bound->clip;
}

// decomp: BATTLE.PRG func_800AD714 (src/BATTLE/BATTLE.PRG/44F14.c:168)
int RunLiveAnimationEvents(Actors_ActorRecord& actor,
                           Actors_AnimationPose& pose, int mode) {
  if (mode < 0 || mode > static_cast<int>(AnimationEventMode::kFindImpact)) {
    Core_Errorf(kChannel, "actor=%u animation event mode %d refused",
                static_cast<unsigned>(actor.instance.slot), mode);
    return 0;
  }
  LiveEventHost host(actor, pose, nullptr);
  const LiveEventTables tables;
  return RunAnimationEvents(host, tables.View(),
                            static_cast<AnimationEventMode>(mode))
      .value;
}

// decomp: BATTLE.PRG func_800AE47C (src/BATTLE/BATTLE.PRG/44F14.c:610)
int RestartLiveAnimationEvents(Actors_ActorRecord& actor) {
  const std::optional<LiveBoundClip> bound = FindLiveBoundClip(actor);
  LiveEventHost host(actor, actor.blendPose, bound ? bound->address : nullptr);
  const LiveEventTables tables;
  const std::optional<AnimationClipRef> clip =
      bound ? std::optional(bound->clip) : std::nullopt;
  if (RestartActorAnimationEvents(host, tables.View(), clip,
                                  actor.poseOverrideActive != 0) ==
      EventRestart::kUnavailable) {
    Core_Errorf(kChannel,
                "cannot unwind actor=%u animation=%u: unavailable events",
                static_cast<unsigned>(actor.instance.slot),
                static_cast<unsigned>(actor.animationId));
    return -1;
  }
  return 0;
}

// decomp: BATTLE.PRG func_800AD494 (src/BATTLE/BATTLE.PRG/40564.c:2719)
// A bank without a registered extent, or a map entry or header outside it,
// is reported and refused instead of returned.
std::intptr_t FindLiveActorAnimationHeader(void* actor, int animation_id,
                                           u_short** header_out) {
  if (header_out == nullptr) {
    Core_Errorf(kChannel, "Animation header lookup requires a header output");
    return 0;
  }
  *header_out = nullptr;
  if (actor == nullptr) {
    Core_Errorf(kChannel, "Animation header lookup requires an actor");
    return 0;
  }
  const LiveHeader found = FindLiveHeader(
      *static_cast<const Actors_ActorRecord*>(actor), animation_id);
  switch (found.header.status) {
    case AnimationHeaderStatus::kFound:
      *header_out = reinterpret_cast<u_short*>(found.address +
                                               found.header.header_offset);
      return BankHandle(found.address);
    case AnimationHeaderStatus::kNoClip:
      return BankHandle(found.address);
    case AnimationHeaderStatus::kNoEntry:
    case AnimationHeaderStatus::kNoBank:
    case AnimationHeaderStatus::kUnregisteredBank:
    case AnimationHeaderStatus::kMapOutsideBank:
    case AnimationHeaderStatus::kHeaderOutsideBank:
      return 0;
  }
  return 0;
}

// decomp: BATTLE.PRG func_800AD008 (src/BATTLE/BATTLE.PRG/40564.c:2623)
void InitializeLiveActorAnimationPose(Actors_ActorRecord* actor,
                                      Actors_AnimationPose* pose) {
  if (actor == nullptr || pose == nullptr) {
    Core_Errorf(kChannel, "Pose start requires an actor and a pose");
    return;
  }
  if (actor->poseOverrideActive != 0 &&
      RestartLiveAnimationEvents(*actor) < 0) {
    return;
  }
  actor->poseOverrideActive = 0;
  const LiveHeader found = FindLiveHeader(*actor, actor->animationId);
  std::optional<AnimationClipRef> clip;
  if (found.header.status == AnimationHeaderStatus::kFound) {
    clip = AnimationClipRef{.bank = found.header.bank,
                            .header_offset = found.header.header_offset,
                            .bones = actor->instance.boneCount};
  }
  AnimationPoseState state = LoadPoseState(*pose);
  const PoseStart start =
      InitializeActorAnimationPose(clip, actor->animationId, state);
  StorePoseState(state, found.address, *pose);
  if (start == PoseStart::kOutOfBank) {
    Core_Errorf(kChannel,
                "actor=%u animation=%u has a clip start outside its bank",
                static_cast<unsigned>(actor->instance.slot),
                static_cast<unsigned>(actor->animationId));
  }
}

// decomp: BATTLE.PRG func_800AF6E8 (src/BATTLE/BATTLE.PRG/44F14.c:1146)
void UpdateLiveActorAnimationBlend(Actors_ActorRecord* actor) {
  if (actor == nullptr) {
    Core_Errorf(kChannel, "Animation blend requires an actor");
    return;
  }
  LiveBlendHost host(*actor);
  UpdateActorAnimationBlend(host);
}

// decomp: BATTLE.PRG func_800B002C (src/BATTLE/BATTLE.PRG/44F14.c:1486)
void UpdateLiveMotionAndLookAt(Actors_ActorRecord* actor, int skip_tweens) {
  if (actor == nullptr) {
    Core_Errorf(kChannel, "Motion update requires an actor");
    return;
  }
  UpdateLiveMotion(*actor, skip_tweens != 0);
}

// decomp: BATTLE.PRG func_800B26B8 (src/BATTLE/BATTLE.PRG/49268.c:246)
void ApplyLiveLockedBoneTransform(Actors_ActorRecord* actor,
                                  const MATRIX* parent, int bone, int saved) {
  if (actor == nullptr || parent == nullptr || bone < 0 ||
      static_cast<std::size_t>(bone) >= std::size(actor->boneMatrices) ||
      saved < 1 || saved > kLockedBoneMatrices) {
    Core_Errorf(kChannel, "Locked bone %d with saved matrix %d refused", bone,
                saved);
    return;
  }
  // The saved matrix's padding carries over, as the original copied it whole.
  MATRIX locked = actor->lockedBoneMatrices[saved - 1];
  const Transform3 transform = ApplyLockedBoneTransform(
      Core::Math::TransformOf(*parent), Core::Math::TransformOf(locked));
  Core::Math::StoreTransform(locked, transform);
  Actors_skeletonPose.boneMatrices[bone] = locked;
  actor->boneMatrices[bone] = locked;
}
}  // namespace VS::Actors
