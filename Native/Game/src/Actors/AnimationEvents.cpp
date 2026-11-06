// Hand-maintained (tools/port/owned.csv).
#include "Actors/AnimationEvents.hpp"

#include <algorithm>
#include <array>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Actors/ActorMotion.hpp"
#include "Audio/ActorSound.hpp"
#include "Audio/SoundBanks.hpp"

namespace VS::Actors {
namespace {
enum class EventFlow : std::uint8_t {
  // Steps to the next event.
  kNext,
  // The event restarted the pose's clip: the cursor reloads from the pose.
  kReload,
  // The event holds the pose's frame: the run ends.
  kHold,
};

// The opcodes the script dispatches; every other opcode ends it.
enum class EventOp : std::uint8_t {
  kInitializePose = 1,
  kImpact = 2,
  kSoundPair = 4,
  kActorSound = 5,
  kStopActorSound = 6,
  kMixedEffectSound = 7,
  kEffectSound = 8,
  kStopEffectSound = 9,
  kModelSound = 10,
  kSurfaceContact = 11,
  kEffectTrack = 12,
  kRestartUnlessAdvanced = 13,
  kWeaponTrail = 14,
  kFadeWeaponTrails = 15,
  kPlayAnimation13 = 19,
  kPlayAnimation1 = 20,
  kPlayAnimation = 21,
  kPlayBlendedAnimation = 22,
  kPreviousAnimation = 23,
  kNextAnimation = 24,
  kWeaponPoseOn = 25,
  kShowWeapons = 26,
  kHideWeapons = 27,
  kGimmickInteraction = 28,
  kPaletteFade = 29,
  kCellSheet0 = 30,
  kCellSheet1 = 31,
  kPlayTimeline1 = 32,
  kStopTimeline1 = 33,
  kPlayTimeline0 = 34,
  kStopTimeline0 = 35,
  kMoveVertexX = 36,
  kMoveVertexY = 37,
  kMoveVertexZ = 38,
  kMoveVertex = 39,
  kIgnored = 40,
  kIgnoredLong = 41,
  kTextureOverlay = 44,
  kShadowExpand = 51,
  kShadowShrink = 52,
  kShadowRadius = 53,
  kShadowLevel = 54,
  kWait = 55,
  kUnlockBone = 56,
  kLockBone = 57,
  kFinishAction = 58,
  kResetMovement = 59,
  kShadowPoint = 60,
  kNormalSpeed = 61,
  kFastSpeed = 62,
  kSourceEffect = 63,
  kWeaponPoseOff = 64,
};

// Opcodes 0-64; 65 and above end the script.
constexpr std::size_t kEventOpCount = 65;
// Event 41 skips five bytes past its length.
constexpr std::size_t kIgnoredLongExtraBytes = 5;

// Operand bytes by event layout.
constexpr std::uint8_t kOverlayCopyOperands = 3;
constexpr std::uint8_t kShadowRateOperands = 3;
constexpr std::uint8_t kShadowLevelOperands = 3;
constexpr std::uint8_t kCellSheetOperands = 4;
constexpr std::uint8_t kVertexMoveOperands = 4;
constexpr std::uint8_t kShadowRadiusOperands = 5;
// Operand positions past the second (the opcode is byte 0).
constexpr std::size_t kThirdOperand = 3;
constexpr std::size_t kFourthOperand = 4;
constexpr std::size_t kFifthOperand = 5;

// Ids of 100 and above step back into 0-99 (events 23 and 24).
constexpr int kAnimationIdWrap = 100;
// Event 19's animation; event 20 and an impactless event 2 play animation 1.
constexpr int kEvent19Animation = 13;
constexpr int kEvent20Animation = 1;
// Event 4's sound id for the drawn weapon's sound.
constexpr std::uint8_t kDrawnWeaponSound = 0x58;
// Event 4 plays only while the actor's y lies below this.
constexpr std::int16_t kSoundPairHeightLimit = 3000;
// The texture animation channel the timeline events 32-35 drive.
constexpr int kEventTextureChannel = 1;
// A vertex move packs a 9-bit vertex index under a 7-bit signed delta.
constexpr int kVertexIndexBits = 9;
constexpr std::uint16_t kVertexIndexMask = 0x1FF;
// Event 58's finished action state and the first companion slot it ends.
constexpr std::uint8_t kFinishedActionAnimation = 2;
constexpr std::uint8_t kFirstCompanionSlot = 2;
// Event 59's root motion y and movement mode.
constexpr std::int16_t kResetRootMotionY = 3;
constexpr std::uint8_t kResetMovementMode = 3;

struct EventContext {
  AnimationEventHost& host;
  const AnimationEventTables& tables;
  AnimationEventMode mode;
  // The opcode and at least its operand bytes.
  std::span<const std::uint8_t> bytes;
  // The frame the run reached: event 2 starts its fallback animation there.
  int carried;
  std::optional<AnimationEventStatus> problem;

  [[nodiscard]] std::uint8_t Op() const { return bytes[0]; }
  [[nodiscard]] std::uint8_t Operand(std::size_t index) const {
    return bytes[index];
  }
  [[nodiscard]] std::uint16_t Word(std::size_t index) const {
    return static_cast<std::uint16_t>((bytes[index] << CHAR_BIT) |
                                      bytes[index + 1]);
  }
  EventFlow Skip(AnimationEventStatus status) {
    host.ReportEventProblem(status, Op());
    if (!problem) {
      problem = status;
    }
    return EventFlow::kNext;
  }
};

// Animation.

EventFlow InitializePose(EventContext& ctx) {
  ctx.host.InitializePose();
  ctx.host.SetEventFlag(true);
  return EventFlow::kReload;
}

EventFlow PlayAndReload(EventContext& ctx, int animation, int blend_frames) {
  ctx.host.PlayAnimation(animation, blend_frames);
  return EventFlow::kReload;
}

EventFlow Impact(EventContext& ctx) {
  if (ctx.host.QueueImpactEffects()) {
    return EventFlow::kNext;
  }
  return PlayAndReload(ctx, kEvent20Animation, ctx.carried);
}

EventFlow RestartUnlessAdvanced(EventContext& ctx) {
  if (ctx.host.EffectsBusy() && !ctx.host.AdvanceRequested()) {
    return InitializePose(ctx);
  }
  ctx.host.ClearAdvanceRequest();
  return EventFlow::kNext;
}

EventFlow PlayAnimation13(EventContext& ctx) {
  return PlayAndReload(ctx, kEvent19Animation, ctx.Operand(1));
}

EventFlow PlayAnimation1(EventContext& ctx) {
  return PlayAndReload(ctx, kEvent20Animation, ctx.Operand(1));
}

EventFlow PlayScriptAnimation(EventContext& ctx, int blend_frames) {
  ctx.host.PlayAnimation(ctx.Operand(1), blend_frames);
  ctx.host.SetEventFlag(false);
  return EventFlow::kReload;
}

EventFlow PlayAnimation(EventContext& ctx) {
  return PlayScriptAnimation(ctx, 0);
}

EventFlow PlayBlendedAnimation(EventContext& ctx) {
  return PlayScriptAnimation(ctx, ctx.Operand(2));
}

EventFlow PlayRelativeAnimation(EventContext& ctx, int step) {
  int animation = ctx.host.AnimationId() + step;
  if (animation >= kAnimationIdWrap) {
    animation -= kAnimationIdWrap;
  }
  return PlayAndReload(ctx, animation, 0);
}

EventFlow PreviousAnimation(EventContext& ctx) {
  return PlayRelativeAnimation(ctx, -1);
}

EventFlow NextAnimation(EventContext& ctx) {
  return PlayRelativeAnimation(ctx, 1);
}

// Sound.

EventFlow SoundPair(EventContext& ctx) {
  int sound = ctx.Operand(1);
  if (sound == kDrawnWeaponSound) {
    const std::uint8_t model = ctx.host.SequenceModelId();
    const std::size_t index = model == 0 || !ctx.host.WeaponDrawn() ? 0 : model;
    if (index >= ctx.tables.weapon_sounds.size()) {
      return ctx.Skip(AnimationEventStatus::kSoundOutOfRange);
    }
    sound = ctx.tables.weapon_sounds[index];
  }
  if (ctx.host.LoadMotion().position.y < kSoundPairHeightLimit) {
    ctx.host.PlaySoundPair(sound);
  }
  return EventFlow::kNext;
}

// Events 5 and 8: the actor's pan at the script's volume. The mix is read
// even at volume 0, as it projects the actor's sound position.
EventFlow PannedSound(EventContext& ctx, std::int32_t group) {
  const std::optional<Audio::SfxMix> mix = ctx.host.SoundMix();
  if (!mix) {
    return ctx.Skip(AnimationEventStatus::kNoSoundTarget);
  }
  const int volume = ctx.Operand(2);
  if (volume != 0) {
    ctx.host.PlaySfx(group, ctx.Operand(1),
                     {.pan = mix->pan, .volume = volume});
  }
  return EventFlow::kNext;
}

EventFlow ActorSound(EventContext& ctx) {
  return PannedSound(ctx, Audio::kActorSoundGroup);
}

EventFlow EffectSound(EventContext& ctx) {
  return PannedSound(ctx, Audio::kSfxBankEffectTags);
}

EventFlow MixedEffectSound(EventContext& ctx) {
  const std::optional<Audio::SfxMix> mix = ctx.host.SoundMix();
  if (!mix) {
    return ctx.Skip(AnimationEventStatus::kNoSoundTarget);
  }
  if (mix->volume != 0) {
    ctx.host.PlaySfx(Audio::kSfxBankEffectTags, ctx.Operand(1), *mix);
  }
  return EventFlow::kNext;
}

EventFlow StopActorSound(EventContext& ctx) {
  ctx.host.StopSfx(Audio::kActorSoundGroup, ctx.Operand(1));
  return EventFlow::kNext;
}

EventFlow StopEffectSound(EventContext& ctx) {
  ctx.host.StopSfx(Audio::kSfxBankEffectTags, ctx.Operand(1));
  return EventFlow::kNext;
}

EventFlow ModelSound(EventContext& ctx) {
  ctx.host.PlayModelSound(ctx.Operand(1));
  return EventFlow::kNext;
}

EventFlow SurfaceContact(EventContext& ctx) {
  ctx.host.PlaySurfaceContactSound(ctx.Op());
  return EventFlow::kNext;
}

// Trails and effects.

EventFlow EffectTrack(EventContext& ctx) {
  if (!ctx.host.EventFlag()) {
    ctx.host.StartEffectTrack(ctx.Operand(1));
  }
  return EventFlow::kNext;
}

EventFlow WeaponTrail(EventContext& ctx) {
  ctx.host.StartWeaponTrail(ctx.Operand(1));
  return EventFlow::kNext;
}

EventFlow FadeWeaponTrails(EventContext& ctx) {
  ctx.host.FadeWeaponTrails(ctx.Operand(1));
  return EventFlow::kNext;
}

EventFlow GimmickInteraction(EventContext& ctx) {
  ctx.host.ApplyGimmickInteraction(ctx.Operand(1));
  return EventFlow::kNext;
}

EventFlow SourceEffect(EventContext& ctx) {
  if (!ctx.host.EventFlag()) {
    ctx.host.SpawnSourceEffect();
  }
  return EventFlow::kNext;
}

// Weapons, textures and palette.

EventFlow ShowWeapons(EventContext& ctx) {
  if (!ctx.host.EventPlaybackActive() && !ctx.host.WeaponDrawn()) {
    return EventFlow::kNext;
  }
  ctx.host.FadeAttachedWeapons(
      {.scale = 0,
       .target = static_cast<std::uint8_t>(kUnitModelScale),
       .frames = ctx.Operand(1)},
      true);
  return EventFlow::kNext;
}

EventFlow HideWeapons(EventContext& ctx) {
  ctx.host.FadeAttachedWeapons(
      {.scale = static_cast<std::uint8_t>(kUnitModelScale),
       .target = 0,
       .frames = ctx.Operand(1)},
      false);
  return EventFlow::kNext;
}

EventFlow PaletteFade(EventContext& ctx) {
  ctx.host.StartPaletteFade();
  return EventFlow::kNext;
}

EventFlow CellSheet(EventContext& ctx) {
  ctx.host.SetTextureCellSheet(
      ctx.Op() - static_cast<int>(EventOp::kCellSheet0),
      {.overlay = ctx.Operand(1),
       .source_x = ctx.Operand(2),
       .source_y = ctx.Operand(kThirdOperand),
       .columns = ctx.Operand(kFourthOperand)});
  return EventFlow::kNext;
}

EventFlow Timeline(EventContext& ctx) {
  const auto op = static_cast<EventOp>(ctx.Op());
  const int timeline =
      op == EventOp::kPlayTimeline1 || op == EventOp::kStopTimeline1 ? 1 : 0;
  const TextureAnimationCommand command =
      op == EventOp::kPlayTimeline1 || op == EventOp::kPlayTimeline0
          ? TextureAnimationCommand::kPlay
          : TextureAnimationCommand::kStop;
  ctx.host.SetTextureAnimation(kEventTextureChannel, timeline, command);
  return EventFlow::kNext;
}

EventFlow TextureOverlay(EventContext& ctx) {
  ctx.host.UploadTextureOverlay({.overlay = ctx.Operand(1),
                                 .source_x = ctx.Operand(2),
                                 .source_y = ctx.Operand(kThirdOperand)});
  return EventFlow::kNext;
}

// Model vertices.

struct VertexMove {
  std::size_t vertex = 0;
  std::int16_t delta = 0;
};

VertexMove UnpackVertexMove(std::uint16_t word) {
  const auto packed = static_cast<std::int16_t>(word);
  return {.vertex = static_cast<std::size_t>(word & kVertexIndexMask),
          .delta = static_cast<std::int16_t>(packed >> kVertexIndexBits)};
}

std::int16_t Directed(const EventContext& ctx, int delta) {
  return static_cast<std::int16_t>(
      ctx.mode == AnimationEventMode::kUnwind ? -delta : delta);
}

EventFlow MoveVertex(EventContext& ctx, std::size_t vertex,
                     const Render::ModelVertex& delta) {
  if (!ctx.host.MoveModelVertex(vertex, delta)) {
    return ctx.Skip(AnimationEventStatus::kVertexOutOfRange);
  }
  ctx.host.MarkPoseOverridden();
  return EventFlow::kNext;
}

EventFlow MoveVertexComponent(EventContext& ctx) {
  const VertexMove move = UnpackVertexMove(ctx.Word(1));
  const std::int16_t delta = Directed(ctx, move.delta);
  Render::ModelVertex vertex{};
  switch (static_cast<EventOp>(ctx.Op())) {
    case EventOp::kMoveVertexX:
      vertex.x = delta;
      break;
    case EventOp::kMoveVertexY:
      vertex.y = delta;
      break;
    default:
      vertex.z = delta;
      break;
  }
  return MoveVertex(ctx, move.vertex, vertex);
}

EventFlow MoveWholeVertex(EventContext& ctx) {
  const VertexMove move = UnpackVertexMove(ctx.Word(1));
  return MoveVertex(
      ctx, move.vertex,
      {.x = Directed(ctx, move.delta),
       .y = Directed(ctx, static_cast<std::int8_t>(ctx.Operand(kThirdOperand))),
       .z = Directed(ctx,
                     static_cast<std::int8_t>(ctx.Operand(kFourthOperand)))});
}

EventFlow Ignored(EventContext& /*ctx*/) {
  IgnoreAnimationEvent();
  return EventFlow::kNext;
}

// Shadow blend.

std::int16_t RateDelta(std::uint16_t target, std::uint16_t rate) {
  return static_cast<std::int16_t>(target - rate);
}

void ApplyRadiusDeltas(ShadowBlend& shadow) {
  shadow.radiusExpandRate = static_cast<std::uint16_t>(
      shadow.radiusExpandRate + shadow.radiusExpandDelta);
  shadow.radiusShrinkRate = static_cast<std::uint16_t>(
      shadow.radiusShrinkRate + shadow.radiusShrinkDelta);
}

void SetRadiusFrames(ShadowBlend& shadow, std::uint8_t frames) {
  if (frames == 0) {
    ApplyRadiusDeltas(shadow);
  }
  shadow.radiusFrames = frames;
}

// The shortest way round to `target`, a 4096-unit turn apart.
void SetLevelDelta(ShadowBlend& shadow, int target) {
  int delta = static_cast<std::int16_t>(target - shadow.level);
  if (delta < -kHalfTurn) {
    delta += kAngleOne;
  }
  if (delta > kHalfTurn) {
    delta -= kAngleOne;
  }
  shadow.levelDelta = static_cast<std::int16_t>(delta);
}

void ApplyLevelDelta(ShadowBlend& shadow) {
  shadow.level = static_cast<std::uint16_t>((shadow.level + shadow.levelDelta) &
                                            kAngleMask);
}

EventFlow ShadowRates(EventContext& ctx) {
  ShadowBlend shadow = ctx.host.LoadShadow();
  const auto op = static_cast<EventOp>(ctx.Op());
  std::uint8_t frames = ctx.Operand(kThirdOperand);
  if (op == EventOp::kShadowShrink) {
    shadow.radiusShrinkDelta = RateDelta(ctx.Word(1), shadow.radiusShrinkRate);
  } else {
    shadow.radiusExpandDelta = RateDelta(ctx.Word(1), shadow.radiusExpandRate);
  }
  if (op == EventOp::kShadowRadius) {
    shadow.radiusShrinkDelta =
        RateDelta(ctx.Word(kThirdOperand), shadow.radiusShrinkRate);
    frames = ctx.Operand(kFifthOperand);
  }
  SetRadiusFrames(shadow, frames);
  ctx.host.StoreShadow(shadow);
  return EventFlow::kNext;
}

EventFlow ShadowLevel(EventContext& ctx) {
  ShadowBlend shadow = ctx.host.LoadShadow();
  SetLevelDelta(shadow, ctx.Word(1));
  const std::uint8_t frames = ctx.Operand(kThirdOperand);
  if (frames == 0) {
    ApplyLevelDelta(shadow);
  }
  shadow.levelFrames = frames;
  ctx.host.StoreShadow(shadow);
  return EventFlow::kNext;
}

EventFlow ShadowPoint(EventContext& ctx) {
  const std::size_t index = ctx.Operand(1);
  if (index >= ctx.tables.points.size()) {
    return ctx.Skip(AnimationEventStatus::kPointOutOfRange);
  }
  const ShadowPointWords& point = ctx.tables.points[index];
  ShadowBlend shadow = ctx.host.LoadShadow();
  shadow.radiusExpandDelta = RateDelta(point[0], shadow.radiusExpandRate);
  shadow.radiusShrinkDelta = RateDelta(point[1], shadow.radiusShrinkRate);
  if (shadow.radiusFrames == 0) {
    ApplyRadiusDeltas(shadow);
  }
  SetLevelDelta(shadow, static_cast<std::int16_t>(point[2]));
  if (shadow.levelFrames == 0) {
    ApplyLevelDelta(shadow);
  }
  ctx.host.StoreShadow(shadow);
  return EventFlow::kNext;
}

// Actor state.

EventFlow Wait(EventContext& ctx) {
  const std::uint8_t wait = ctx.host.EventWait();
  if (wait == 0) {
    ctx.host.SetEventWait(ctx.Operand(1));
  } else {
    const auto left = static_cast<std::uint8_t>(wait - 1);
    ctx.host.SetEventWait(left);
    if (left == 0) {
      return EventFlow::kNext;
    }
  }
  ctx.host.SetFrame(static_cast<std::int16_t>(ctx.host.Frame() - 1));
  return EventFlow::kHold;
}

EventFlow BoneLock(EventContext& ctx) {
  ctx.host.LockBonePose(ctx.Operand(1),
                        static_cast<EventOp>(ctx.Op()) == EventOp::kLockBone);
  return EventFlow::kNext;
}

EventFlow SetWeaponPose(EventContext& ctx, bool active) {
  ActorMotion motion = ctx.host.LoadMotion();
  motion.weapon_pose_active = active;
  ctx.host.StoreMotion(motion);
  return EventFlow::kNext;
}

EventFlow WeaponPoseOn(EventContext& ctx) { return SetWeaponPose(ctx, true); }

EventFlow WeaponPoseOff(EventContext& ctx) { return SetWeaponPose(ctx, false); }

EventFlow FinishAction(EventContext& ctx) {
  ActorMotion motion = ctx.host.LoadMotion();
  motion.action_animation_state = kFinishedActionAnimation;
  ctx.host.StoreMotion(motion);
  const std::uint8_t slot = ctx.host.CompanionSlot();
  if (slot >= kFirstCompanionSlot && !ctx.host.FinishCompanionAction(slot)) {
    return ctx.Skip(AnimationEventStatus::kNoCompanion);
  }
  return EventFlow::kNext;
}

EventFlow ResetMovement(EventContext& ctx) {
  ActorMotion motion = ctx.host.LoadMotion();
  motion.root_motion.y = kResetRootMotionY;
  motion.movement_mode = kResetMovementMode;
  motion.locomotion_state = 0;
  motion.movement_pending = false;
  ctx.host.StoreMotion(motion);
  return EventFlow::kNext;
}

EventFlow NormalSpeed(EventContext& ctx) {
  ctx.host.SetFastAnimation(false);
  return EventFlow::kNext;
}

EventFlow FastSpeed(EventContext& ctx) {
  ctx.host.SetFastAnimation(true);
  return EventFlow::kNext;
}

// The opcode table: each handled opcode's handler and the operand bytes it
// reads after the opcode.
using EventHandler = EventFlow (*)(EventContext&);

struct EventDefinition {
  EventHandler run = nullptr;
  std::uint8_t operand_bytes = 0;
};

using EventTable = std::array<EventDefinition, kEventOpCount>;

constexpr EventTable BuildEventTable() {
  EventTable table{};
  const auto define = [&table](EventOp op, EventHandler run,
                               std::uint8_t operand_bytes) {
    table[static_cast<std::size_t>(op)] = {.run = run,
                                           .operand_bytes = operand_bytes};
  };
  define(EventOp::kInitializePose, &InitializePose, 0);
  define(EventOp::kImpact, &Impact, 0);
  define(EventOp::kSoundPair, &SoundPair, 1);
  define(EventOp::kActorSound, &ActorSound, 2);
  define(EventOp::kStopActorSound, &StopActorSound, 1);
  define(EventOp::kMixedEffectSound, &MixedEffectSound, 1);
  define(EventOp::kEffectSound, &EffectSound, 2);
  define(EventOp::kStopEffectSound, &StopEffectSound, 1);
  define(EventOp::kModelSound, &ModelSound, 1);
  define(EventOp::kSurfaceContact, &SurfaceContact, 0);
  define(EventOp::kEffectTrack, &EffectTrack, 1);
  define(EventOp::kRestartUnlessAdvanced, &RestartUnlessAdvanced, 0);
  define(EventOp::kWeaponTrail, &WeaponTrail, 1);
  define(EventOp::kFadeWeaponTrails, &FadeWeaponTrails, 1);
  define(EventOp::kPlayAnimation13, &PlayAnimation13, 1);
  define(EventOp::kPlayAnimation1, &PlayAnimation1, 1);
  define(EventOp::kPlayAnimation, &PlayAnimation, 1);
  define(EventOp::kPlayBlendedAnimation, &PlayBlendedAnimation, 2);
  define(EventOp::kPreviousAnimation, &PreviousAnimation, 0);
  define(EventOp::kNextAnimation, &NextAnimation, 0);
  define(EventOp::kWeaponPoseOn, &WeaponPoseOn, 0);
  define(EventOp::kShowWeapons, &ShowWeapons, 1);
  define(EventOp::kHideWeapons, &HideWeapons, 1);
  define(EventOp::kGimmickInteraction, &GimmickInteraction, 1);
  define(EventOp::kPaletteFade, &PaletteFade, 0);
  define(EventOp::kCellSheet0, &CellSheet, kCellSheetOperands);
  define(EventOp::kCellSheet1, &CellSheet, kCellSheetOperands);
  define(EventOp::kPlayTimeline1, &Timeline, 0);
  define(EventOp::kStopTimeline1, &Timeline, 0);
  define(EventOp::kPlayTimeline0, &Timeline, 0);
  define(EventOp::kStopTimeline0, &Timeline, 0);
  define(EventOp::kMoveVertexX, &MoveVertexComponent, 2);
  define(EventOp::kMoveVertexY, &MoveVertexComponent, 2);
  define(EventOp::kMoveVertexZ, &MoveVertexComponent, 2);
  define(EventOp::kMoveVertex, &MoveWholeVertex, kVertexMoveOperands);
  define(EventOp::kIgnored, &Ignored, 0);
  define(EventOp::kIgnoredLong, &Ignored, 0);
  define(EventOp::kTextureOverlay, &TextureOverlay, kOverlayCopyOperands);
  define(EventOp::kShadowExpand, &ShadowRates, kShadowRateOperands);
  define(EventOp::kShadowShrink, &ShadowRates, kShadowRateOperands);
  define(EventOp::kShadowRadius, &ShadowRates, kShadowRadiusOperands);
  define(EventOp::kShadowLevel, &ShadowLevel, kShadowLevelOperands);
  define(EventOp::kWait, &Wait, 1);
  define(EventOp::kUnlockBone, &BoneLock, 1);
  define(EventOp::kLockBone, &BoneLock, 1);
  define(EventOp::kFinishAction, &FinishAction, 0);
  define(EventOp::kResetMovement, &ResetMovement, 0);
  define(EventOp::kShadowPoint, &ShadowPoint, 1);
  define(EventOp::kNormalSpeed, &NormalSpeed, 0);
  define(EventOp::kFastSpeed, &FastSpeed, 0);
  define(EventOp::kSourceEffect, &SourceEffect, 0);
  define(EventOp::kWeaponPoseOff, &WeaponPoseOff, 0);
  return table;
}

constexpr EventTable kEvents = BuildEventTable();

[[nodiscard]] const EventDefinition* FindEvent(std::uint8_t op) {
  if (op >= kEvents.size() || kEvents[op].run == nullptr) {
    return nullptr;
  }
  return &kEvents[op];
}

[[nodiscard]] bool InRange(std::uint8_t op, EventOp first, EventOp last) {
  return op >= static_cast<std::uint8_t>(first) &&
         op <= static_cast<std::uint8_t>(last);
}

// func_800AD714's mode filter, applied to the dispatched opcodes.
[[nodiscard]] bool ModeDispatches(AnimationEventMode mode, std::uint8_t op) {
  switch (mode) {
    case AnimationEventMode::kPlay:
      return true;
    case AnimationEventMode::kPresentation:
      return op == static_cast<std::uint8_t>(EventOp::kWeaponPoseOn) ||
             InRange(op, EventOp::kPaletteFade, EventOp::kTextureOverlay) ||
             InRange(op, EventOp::kShadowExpand, EventOp::kShadowLevel) ||
             InRange(op, EventOp::kUnlockBone, EventOp::kLockBone);
    case AnimationEventMode::kUnwind:
      return InRange(op, EventOp::kMoveVertexX, EventOp::kMoveVertex);
    case AnimationEventMode::kFindImpact:
      return op == static_cast<std::uint8_t>(EventOp::kImpact);
  }
  return false;
}

AnimationEventRun Stop(AnimationEventHost& host, AnimationEventStatus status,
                       std::uint8_t op) {
  host.ReportEventProblem(status, op);
  host.SetNextEventFrame(kNoEventFrame);
  return {.status = status};
}

[[nodiscard]] bool Fits(std::span<const std::uint8_t> bank, std::size_t offset,
                        std::size_t count) {
  return offset <= bank.size() && count <= bank.size() - offset;
}

// One step of the run: what the event at `position` did, or the stop.
struct EventStep {
  std::optional<EventFlow> flow;
  AnimationEventStatus stop = AnimationEventStatus::kDone;
  std::uint8_t op = 0;
  std::size_t length = 0;
};

EventStep DispatchEvent(EventContext& ctx,
                        const EventScriptPosition& position) {
  EventStep step{};
  if (!Fits(position.bank, position.offset, 1)) {
    step.stop = AnimationEventStatus::kScriptOverrun;
    return step;
  }
  step.op = position.bank[position.offset];
  const EventDefinition* event = FindEvent(step.op);
  if (event == nullptr) {
    return step;
  }
  if (step.op >= ctx.tables.lengths.size()) {
    step.stop = AnimationEventStatus::kTableOutOfRange;
    return step;
  }
  step.length = ctx.tables.lengths[step.op];
  if (step.op == static_cast<std::uint8_t>(EventOp::kIgnoredLong)) {
    step.length += kIgnoredLongExtraBytes;
  }
  const std::size_t extent =
      std::max(step.length, std::size_t{event->operand_bytes} + 1);
  if (!Fits(position.bank, position.offset, extent)) {
    step.stop = AnimationEventStatus::kScriptOverrun;
    return step;
  }
  ctx.bytes = position.bank.subspan(position.offset, extent);
  step.flow =
      ModeDispatches(ctx.mode, step.op) ? event->run(ctx) : EventFlow::kNext;
  return step;
}
}  // namespace

bool StopsAnimationEvents(AnimationEventStatus status) noexcept {
  switch (status) {
    case AnimationEventStatus::kNoScript:
    case AnimationEventStatus::kScriptOverrun:
    case AnimationEventStatus::kReloadLimit:
    case AnimationEventStatus::kDispatchLimit:
    case AnimationEventStatus::kTableOutOfRange:
      return true;
    default:
      return false;
  }
}

// decomp: BATTLE.PRG func_800AD714 (src/BATTLE/BATTLE.PRG/44F14.c:168)
// The script is read through its bank's registered extent instead of a
// pointer, and every table index is bounded. The frame event 2 starts its
// fallback animation at is 0 on the first event, as the original's $s1 is
// (#297). A terminating opcode ends the script in every mode, where the
// filtered modes stepped over it by an unbounded length.
AnimationEventRun RunAnimationEvents(AnimationEventHost& host,
                                     const AnimationEventTables& tables,
                                     AnimationEventMode mode) {
  EventContext ctx{.host = host,
                   .tables = tables,
                   .mode = mode,
                   .bytes = {},
                   .carried = 0,
                   .problem = std::nullopt};
  std::optional<EventScriptPosition> position = host.Script();
  int reloads = 0;
  for (int dispatched = 0;; ++dispatched) {
    if (dispatched >= kAnimationEventDispatchLimit) {
      return Stop(host, AnimationEventStatus::kDispatchLimit, 0);
    }
    if (!position) {
      return Stop(host, AnimationEventStatus::kNoScript, 0);
    }
    if (mode == AnimationEventMode::kFindImpact &&
        Fits(position->bank, position->offset, 1) &&
        position->bank[position->offset] ==
            static_cast<std::uint8_t>(EventOp::kImpact)) {
      return {.status = ctx.problem.value_or(AnimationEventStatus::kDone),
              .value = host.NextEventFrame()};
    }
    const EventStep step = DispatchEvent(ctx, *position);
    if (step.stop != AnimationEventStatus::kDone) {
      return Stop(host, step.stop, step.op);
    }
    if (!step.flow) {
      host.SetNextEventFrame(kNoEventFrame);
      return {.status = ctx.problem.value_or(AnimationEventStatus::kDone)};
    }
    switch (*step.flow) {
      case EventFlow::kHold:
        return {.status = ctx.problem.value_or(AnimationEventStatus::kDone)};
      case EventFlow::kReload:
        if (++reloads > kAnimationEventReloadLimit) {
          return Stop(host, AnimationEventStatus::kReloadLimit, step.op);
        }
        ctx.carried = host.NextEventFrame();
        position = host.Script();
        host.SetFrame(0);
        break;
      case EventFlow::kNext: {
        const auto cursor =
            ReadEventCursor(position->bank, position->offset + step.length);
        if (!cursor) {
          return Stop(host, AnimationEventStatus::kScriptOverrun, step.op);
        }
        position->offset = cursor->offset;
        host.StoreCursor(*position, cursor->frame);
        ctx.carried = cursor->frame;
        break;
      }
    }
    const int reached = mode == AnimationEventMode::kUnwind ||
                                mode == AnimationEventMode::kFindImpact
                            ? int{host.AnimationLength()}
                            : int{host.Frame()};
    if (reached < ctx.carried) {
      return {.status = ctx.problem.value_or(AnimationEventStatus::kDone)};
    }
  }
}

// decomp: BATTLE.PRG func_800AE46C (src/BATTLE/BATTLE.PRG/44F14.c:606)
void IgnoreAnimationEvent() {}

// decomp: BATTLE.PRG func_800AE47C (src/BATTLE/BATTLE.PRG/44F14.c:610)
// The bound clip comes from the caller with its bank's registered extent,
// and its event word and first cursor are bounded by that bank.
EventRestart RestartActorAnimationEvents(
    AnimationEventHost& host, const AnimationEventTables& tables,
    const std::optional<AnimationClipRef>& bound, bool pose_overridden) {
  if (!bound) {
    return EventRestart::kUnavailable;
  }
  const ClipEvents events = FindClipEvents(*bound);
  switch (events.status) {
    case ClipEventsStatus::kOutOfBank:
      return EventRestart::kUnavailable;
    case ClipEventsStatus::kNone:
      return pose_overridden ? EventRestart::kUnavailable
                             : EventRestart::kNoEvents;
    case ClipEventsStatus::kStarts:
      break;
  }
  host.StoreCursor({.bank = bound->bank, .offset = events.cursor.offset},
                   events.cursor.frame);
  RunAnimationEvents(host, tables, AnimationEventMode::kUnwind);
  return EventRestart::kUnwound;
}
}  // namespace VS::Actors
