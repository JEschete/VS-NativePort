// Hand-maintained (tools/port/owned.csv).
// The typed actor animation controls (AnimationControl.hpp).
#include "Actors/AnimationControl.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

#include "Core/Log.hpp"

namespace VS::Actors {
namespace {
using Content::ActorAssetRequest;

constexpr int kActorSlots = 17;
constexpr int kBattleActorSlots = 16;
constexpr int kAshleySlot = 0;

// Battle asset requests.
constexpr std::uint8_t kSequenceAsset = 7;
constexpr std::uint8_t kEffectTextureAsset = 9;
constexpr std::uint8_t kOwnSequence = 0;
constexpr std::uint8_t kSharedSequence = 1;
constexpr std::uint8_t kSharedSequenceWithTextures = 2;
// The party slots carry a weapon sequence only when armed; other actors only
// when spawned armed.
constexpr int kPartySlots = 2;
constexpr int kSpawnModeMask = 3;
constexpr int kArmedSpawnMode = 1;

// Event_EventPlaybackState while an event script runs: every pose restarts.
constexpr int kEventScriptRunning = 4;

// Animation ids. 100 is added to an id while the weapon is drawn.
constexpr int kDrawnAnimationOffset = 100;
constexpr int kIdleAnimation = 1;
constexpr int kNeutralAnimation = 6;
constexpr int kLockedIdleAnimation = 8;
constexpr int kFirstWeaponDrawAnimation = 9;  // draw, sheathe by mode: 9-12
constexpr int kLastWeaponDrawAnimation = 12;
constexpr int kSheatheAnimationStep = 10;  // mode * 2 + 10
constexpr int kDrawAnimationStep = 9;      // mode * 2 + 9
constexpr int kWalkAnimation = 0xD;        // two strides: 0xD, 0xE
constexpr int kBackWalkAnimation = 0xF;    // two strides: 0xF, 0x10
constexpr int kLastWalkAnimation = 0x10;
constexpr int kTurnRightAnimation = 0x11;
constexpr int kTurnLeftAnimation = 0x12;
constexpr int kRunAnimation = 0x13;  // two strides: 0x13, 0x14
constexpr int kLastRunAnimation = 0x14;
constexpr int kRunTurnAnimation = 0x19;  // 0x19 and 0x1A keep running
constexpr int kRunStraightAnimation = 0x1A;
constexpr int kReadyPoseAnimation = 0x1C;
constexpr int kGuardPoseAnimation = 0x1D;
constexpr int kBracePoseAnimation = 0x1E;
constexpr int kJumpIdleAnimation = 31;
constexpr int kJumpWalkAnimation = 32;
constexpr int kAirborneJumpIdleAnimation = 35;
constexpr int kClimbAnimation = 36;
constexpr int kClimbTurnAnimation = 39;
constexpr int kHangAnimation = 41;
constexpr int kFirstCycleAnimation = 0x2E;  // 0x2E-0x38
constexpr int kLastCycleAnimation = 0x38;
constexpr int kAirborneIdleAnimation = 48;
constexpr int kBusyAnimation = 0x3B;
constexpr int kHangTurnAnimation = 59;
constexpr int kFirstBusyAnimation = 0x40;  // 0x40-0x46
constexpr int kLastBusyAnimation = 0x46;
constexpr int kFirstNearReaction = 0x47;  // 0x47-0x4A and 0x4C
constexpr int kLastNearReaction = 0x4A;
constexpr int kStanceAnimation = 0x4B;
constexpr int kNearReaction = 0x4C;
constexpr int kFirstFarReaction = 0x4F;  // 0x4F-0x52
constexpr int kLastFarReaction = 0x52;
constexpr int kStanceWalkAnimation = 0x54;
constexpr int kFirstStrikeAnimation = 0x55;  // 0x55-0x5B
constexpr int kLastStrikeAnimation = 0x5B;
constexpr int kStrikeAnimation = 0x5E;
constexpr int kStanceHoldAnimation = 0x5F;
constexpr int kActionPoseBase = 0x5F;   // kinds 1-4: 0x60-0x63
constexpr int kFirstIdleAction = 0x62;  // 0x62-0x65 when drawn
constexpr int kLastIdleAction = 0x65;
constexpr int kKneelAnimation = 0x62;
constexpr int kTransitionAnimation = 0x63;
constexpr int kDrawnTransitionAnimation = 0xC7;
constexpr int kTurnLockAnimation = 0x75;
constexpr int kTurnLockDrawnAnimation = 0x76;
constexpr int kHeldPoseAnimation = 0xC6;
constexpr int kHeldDrawnPoseAnimation = 0xC8;

// Action pose kinds (PlayActorActionPose).
constexpr int kFirstStrikePose = 1;
constexpr int kLastStrikePose = 4;
constexpr int kBracePose = 5;
constexpr int kReadyPose = 6;
constexpr int kGuardPose = 7;

// Blend frames.
constexpr int kSheatheBlend = 5;
constexpr int kDrawBlend = 16;
constexpr int kActionPoseBlend = 6;
constexpr int kTurnBlend = 0xA;
constexpr int kStanceBlend = 0xC;
constexpr int kBasePoseBlend = 0x10;
constexpr int kStanceWalkBlend = 4;
constexpr int kStanceIdleBlend = 10;
constexpr int kJumpBlend = 20;
constexpr int kJumpIdleBlend = 10;
constexpr int kRunBlend = 8;
constexpr int kStrideBlend = 6;
constexpr int kQueuedStrideBlend = 4;
constexpr int kStopBlend = 10;

// Actor states.
constexpr std::uint8_t kAirborneMovement = 3;
constexpr std::uint8_t kFirstHeldMovement = 2;
constexpr std::uint8_t kClimbing = 1;
constexpr std::uint8_t kHanging = 2;
constexpr std::uint8_t kTeleportReaction = 3;
constexpr std::uint8_t kKnockedDownPose = 2;
constexpr std::uint8_t kFallenPose = 5;
constexpr std::uint8_t kGuardActionState = 5;
constexpr std::uint8_t kRecoverActionState = 11;
constexpr std::uint8_t kSteeringRequest = 1;
constexpr std::uint8_t kTurnRequest = 6;
constexpr std::uint8_t kActionTurnFrames = 0xA;
constexpr std::uint8_t kNoAnimationSet = 0xFF;
constexpr std::uint16_t kNoEventFrame = 0xFFFF;
constexpr int kSkippedTextureFrames = 2;
constexpr std::uint8_t kLocomotionCounterLimit = 250;
constexpr std::uint8_t kTravelWalkAnimation = 0xD;

// Gimmicks waiting to settle (mode 0xF9 or 0xFF on support 0) settle.
constexpr std::int16_t kGimmickSettling = 0xF9;
constexpr std::int16_t kGimmickSettlingAlt = 0xFF;

constexpr int kQuadrantCount = 4;
constexpr int kBackQuadrant = 2;
constexpr int kNearReactionTiles = 1;
constexpr int kFarReactionTiles = 2;

bool Within(int value, int first, int last) {
  return value >= first && value <= last;
}

std::int16_t Wrap16(int value) { return static_cast<std::int16_t>(value); }

std::uint8_t LowBit(int value) { return static_cast<std::uint8_t>(value & 1); }

void QueueSequence(AnimationControlHost& host, int index, int model_id,
                   std::uint8_t sub_kind, std::uint8_t texture_slot = 0) {
  ActorAssetRequest request;
  request.kind = kSequenceAsset;
  request.target_slot = static_cast<std::int8_t>(index);
  request.model_id = static_cast<std::int16_t>(model_id);
  request.sub_kind = sub_kind;
  request.texture_slot = texture_slot;
  host.QueueSequenceAsset(request);
}

// The heading quadrant of the actor's movement request (0 ahead, 2 behind).
int RequestQuadrant(const ActorMotion& motion) {
  if (motion.request_mode != kSteeringRequest) return 0;
  const int heading = (motion.request_facing - motion.request_direction +
                       kAngleOne / (2 * kQuadrantCount)) &
                      kAngleMask;
  return heading / (kAngleOne / kQuadrantCount);
}

// The animation that `animation` (below 100) plays as in the actor's stance;
// nullopt when the stance walk already plays.
std::optional<int> StanceAnimation(const AnimationActor& actor, int animation) {
  const ActorMotion& motion = actor.motion;
  if (motion.motion_locked) {
    if (animation == kIdleAnimation || animation == kNeutralAnimation) {
      animation = kLockedIdleAnimation;
    }
  } else if (motion.movement_mode == kAirborneMovement) {
    if (animation == kIdleAnimation) {
      animation = motion.is_jumping ? kAirborneJumpIdleAnimation
                                    : kAirborneIdleAnimation;
    }
  } else if (motion.is_jumping) {
    if (animation == kIdleAnimation) {
      animation = kJumpIdleAnimation;
    } else if (animation == kWalkAnimation) {
      animation = kJumpWalkAnimation;
    }
  }
  if (actor.weapon_drawn) return animation + kDrawnAnimationOffset;
  if (!motion.action_pose_active) return animation;
  if (animation == kIdleAnimation) return kStanceAnimation;
  if (animation == kWalkAnimation) animation = kStanceWalkAnimation;
  if (animation == kStanceWalkAnimation &&
      actor.animation_id == kStanceWalkAnimation) {
    return std::nullopt;
  }
  return animation;
}

// The original's reaction knock-back (slot 0): frees the AI destination the
// actor was pushed from, one or two tiles back along its facing.
void ReleaseReactionTile(AnimationControlHost& host,
                         const AnimationActor& actor) {
  const int animation = actor.animation_id;
  int distance = 0;
  if (Within(animation, kFirstFarReaction, kLastFarReaction)) {
    distance = kFarReactionTiles;
  } else if (Within(animation, kFirstNearReaction, kLastNearReaction) ||
             animation == kNearReaction) {
    distance = kNearReactionTiles;
  } else {
    return;
  }
  const ActorMotion& motion = actor.motion;
  const int direction =
      ((motion.facing + kAngleOne / (2 * kQuadrantCount)) & kAngleMask) /
      (kAngleOne / kQuadrantCount);
  if (direction < 2) distance = -distance;
  const int tile_dx = (direction & 1) != 0 ? distance : 0;
  const int tile_dz = (direction & 1) != 0 ? 0 : distance;
  host.ReleaseAiDestinationTile(motion.current_tile.x + tile_dx,
                                motion.current_tile.z + tile_dz);
}

// RestoreActorBasePose's settle: finishes a hang's step at once, or plays the
// base pose of an actor not mid-action.
void SettleBasePose(AnimationControlHost& host, AnimationActor& actor,
                    int actor_id, int animation) {
  ActorMotion& motion = actor.motion;
  if (motion.locomotion_state == kHanging) {
    if (motion.turn_frames != 0 || motion.move_frames != 0) {
      motion.move_frames = 0;
      motion.turn_frames = 0;
      motion.position.x = Wrap16(motion.position.x + motion.root_motion.x);
      motion.position.y = Wrap16(motion.position.y + motion.root_motion.y);
      motion.position.z = Wrap16(motion.position.z + motion.root_motion.z);
      motion.facing = Wrap16(motion.facing + motion.facing_delta);
    }
    return;
  }
  if (motion.action_animation_state != 0) return;
  int base_animation = actor.animation_id;
  if (base_animation > kDrawnAnimationOffset) {
    base_animation -= kDrawnAnimationOffset;
  }
  if (base_animation == kKneelAnimation) return;
  if (motion.slot == kAshleySlot) ReleaseReactionTile(host, actor);
  if (motion.action_pose_active) {
    PlayActorAnimation(host, motion.slot, kStanceAnimation, kStanceBlend);
  } else {
    PlayActorAnimation(host, actor_id, animation, kBasePoseBlend);
    actor.motion.run_frames = 0;
  }
}

void PlayStride(AnimationControlHost& host, AnimationActor& actor,
                int first_stride, int blend_frames) {
  const int stride = actor.locomotion_animation_state;
  actor.locomotion_animation_state = LowBit(stride + 1);
  PlayActorAnimation(host, actor.motion.slot, first_stride + stride,
                     blend_frames);
}

void UpdateRunAnimation(AnimationControlHost& host, AnimationActor& actor,
                        int animation) {
  if (animation == kRunTurnAnimation || animation == kRunStraightAnimation) {
    return;
  }
  int next = kRunStraightAnimation;
  if (animation != kTurnLeftAnimation && animation != kRunAnimation) {
    if (animation != kLastRunAnimation && animation != kTurnRightAnimation) {
      actor.motion.is_running = false;
      return;
    }
    next = kRunTurnAnimation;
  }
  if (!host.FindAnimationHeader(actor, next).has_entry) {
    next = kNeutralAnimation;
  }
  PlayActorAnimation(host, actor.motion.slot, next, kRunBlend);
}

void UpdateFastAnimation(AnimationControlHost& host, AnimationActor& actor,
                         int animation) {
  if (animation == kRunAnimation || animation == kLastRunAnimation) return;
  if (Within(animation, kBackWalkAnimation, kTurnLeftAnimation) ||
      Within(animation, kFirstCycleAnimation, kLastCycleAnimation) ||
      Within(animation, kFirstWeaponDrawAnimation, kLastWeaponDrawAnimation) ||
      animation == kIdleAnimation || animation == kNeutralAnimation) {
    PlayStride(host, actor, kRunAnimation, kStrideBlend);
    return;
  }
  // 0xF already took a stride above; the original tests it here too.
  const int next =
      animation == kWalkAnimation || animation == kBackWalkAnimation
          ? kLastRunAnimation
          : kRunAnimation;
  QueueActorAnimation(host, actor.motion.slot, next, kQueuedStrideBlend, 0);
}

void UpdateWalkAnimation(AnimationControlHost& host, AnimationActor& actor,
                         int animation, int quadrant) {
  bool stride = false;
  if (Within(animation, kFirstCycleAnimation, kLastCycleAnimation) ||
      Within(animation, kFirstWeaponDrawAnimation, kLastWeaponDrawAnimation)) {
    stride = true;
  } else if (animation == kWalkAnimation || animation == kWalkAnimation + 1) {
    if (quadrant != kBackQuadrant) return;
    stride = true;
  } else if (animation == kBackWalkAnimation ||
             animation == kLastWalkAnimation) {
    if (quadrant == kBackQuadrant) return;
    stride = true;
  } else {
    stride = animation == kIdleAnimation || animation == kNeutralAnimation ||
             animation == kTurnRightAnimation ||
             animation == kTurnLeftAnimation;
  }
  if (stride) {
    PlayStride(host, actor,
               quadrant == kBackQuadrant ? kBackWalkAnimation : kWalkAnimation,
               kStrideBlend);
    return;
  }
  int next = kWalkAnimation;
  if (animation == kRunAnimation || animation == kBackWalkAnimation ||
      animation == kWalkAnimation) {
    next += 1;
  }
  if (quadrant == kBackQuadrant) next += 2;
  QueueActorAnimation(host, actor.motion.slot, next, kQueuedStrideBlend, 0);
}

void UpdateIdleAnimation(AnimationControlHost& host, AnimationActor& actor,
                         int animation) {
  const int slot = actor.motion.slot;
  if (Within(animation, kWalkAnimation, kLastWalkAnimation)) {
    PlayActorAnimation(host, slot, kNeutralAnimation, kStopBlend);
    QueueActorAnimation(host, slot, kIdleAnimation, kStopBlend, 0);
    return;
  }
  if (animation != kIdleAnimation &&
      actor.motion.request_mode != kTurnRequest) {
    PlayActorAnimation(host, slot, kIdleAnimation, kStopBlend);
    actor.locomotion_counter = 0;
  }
  actor.locomotion_counter = static_cast<std::uint8_t>(
      actor.locomotion_counter + host.TickSpeed() / 2);
  if (actor.locomotion_counter >= kLocomotionCounterLimit) {
    actor.locomotion_counter = kLocomotionCounterLimit;
  }
}

AnimationActor* RequireActor(AnimationControlHost& host, int actor_id,
                             const char* operation) {
  AnimationActor* actor = host.Actor(actor_id);
  if (actor == nullptr) {
    VS_LOG_WARN("actors", "{} refused: no actor record for actor {}", operation,
                actor_id);
  }
  return actor;
}
}  // namespace

// decomp: BATTLE.PRG func_800770FC (src/BATTLE/BATTLE.PRG/146C.c:6444)
void QueueActorBaseSequenceLoad(AnimationControlHost& host, int actor_id) {
  QueueSequence(host, actor_id, 0, kOwnSequence);
}

// decomp: BATTLE.PRG func_800771E0 (src/BATTLE/BATTLE.PRG/146C.c:6480)
void QueueActorWeaponSequenceLoad(AnimationControlHost& host,
                                  std::optional<int> equipped_weapon_category,
                                  int actor_id, int weapon_category,
                                  int spawn_flags) {
  const bool armed = weapon_category != 0 || actor_id >= kPartySlots;
  const bool spawned_armed = actor_id < kPartySlots ||
                             (spawn_flags & kSpawnModeMask) == kArmedSpawnMode;
  if (!armed || !spawned_armed) return;
  if (!equipped_weapon_category) {
    VS_LOG_WARN("actors",
                "Weapon sequence load refused: no battle actor for actor {}",
                actor_id);
    return;
  }
  QueueSequence(host, actor_id, *equipped_weapon_category, kOwnSequence);
}

// decomp: BATTLE.PRG func_8007C348 (src/BATTLE/BATTLE.PRG/146C.c:8738)
void InterruptActorAction(AnimationControlHost& host, int actor_id,
                          int animate) {
  host.BeginActorDeath(actor_id, animate);
}

// decomp: BATTLE.PRG func_8007CB84 (src/BATTLE/BATTLE.PRG/146C.c:8997)
void QueueSharedSequenceLoad(AnimationControlHost& host, int index,
                             int weapon_id) {
  QueueSequence(host, index, weapon_id, kSharedSequence);
}

// decomp: BATTLE.PRG func_8007CBBC (src/BATTLE/BATTLE.PRG/146C.c:9008)
int UnloadSharedSequence(AnimationControlHost& host, int bank) {
  if (bank < 1 || bank > kSharedSequenceBanks) {
    VS_LOG_ERROR("actors.sequence", "invalid shared bank retirement={}", bank);
    return -1;
  }
  const auto bank_index = static_cast<std::size_t>(bank - 1);
  const auto posed_from_bank = [bank_index](const AnimationActor& actor) {
    const SequenceHandle sequence = actor.shared_sequences.at(bank_index);
    return sequence != SequenceHandle::kNone &&
           actor.animation_header == sequence;
  };
  // Every override posed from the bank must unwind before any actor changes.
  for (int slot = 0; slot < kActorSlots; ++slot) {
    AnimationActor* actor = host.Actor(slot);
    if (actor == nullptr || !posed_from_bank(*actor) ||
        !actor->pose_override_active) {
      continue;
    }
    if (!host.CanUnwindBoundAnimation(*actor)) {
      VS_LOG_ERROR("actors.sequence",
                   "bank={} cannot unwind actor={} animation={}", bank, slot,
                   actor->animation_id);
      return -1;
    }
  }
  for (int slot = 0; slot < kActorSlots; ++slot) {
    AnimationActor* actor = host.Actor(slot);
    if (actor == nullptr || !posed_from_bank(*actor)) continue;
    if (actor->pose_override_active &&
        host.RestartAnimationEvents(*actor) < 0) {
      return -1;
    }
    actor->pose_override_active = false;
    actor->animation_header = SequenceHandle::kNone;
    actor->animation_set = kNoAnimationSet;
    actor->pose = {.script = EventScriptHandle::kNone,
                   .next_event_frame = kNoEventFrame};
    actor->blend_pose = actor->pose;
  }
  const int result = host.ReleaseSharedSequence(bank);
  if (result < 0) {
    VS_LOG_ERROR("actors.sequence",
                 "shared bank={} retirement failed status={}", bank, result);
  }
  return result;
}

// decomp: BATTLE.PRG func_8007CBDC (src/BATTLE/BATTLE.PRG/146C.c:9010)
void QueueSharedSequenceWithTextures(AnimationControlHost& host, int index,
                                     int weapon_id, int texture_slot) {
  QueueSequence(host, index, weapon_id, kSharedSequenceWithTextures,
                static_cast<std::uint8_t>(texture_slot));
}

// decomp: BATTLE.PRG vs_battle_loadEtm (src/BATTLE/BATTLE.PRG/146C.c:9022)
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void QueueSequenceEffectTextures(AnimationControlHost& host, int index,
                                 int weapon_id, int texture_slot) {
  ActorAssetRequest request;
  request.kind = kEffectTextureAsset;
  request.target_slot = static_cast<std::int8_t>(index);
  request.model_id = static_cast<std::int16_t>(weapon_id);
  request.texture_slot = static_cast<std::uint8_t>(texture_slot);
  host.QueueSequenceAsset(request);
}

// decomp: BATTLE.PRG func_8007CC4C (src/BATTLE/BATTLE.PRG/146C.c:9033)
void ReleaseSequenceEffectTextures(AnimationControlHost& host, int bank) {
  host.ReleaseSequenceEffectTextures(bank);
}

// decomp: BATTLE.PRG func_8007CC6C (src/BATTLE/BATTLE.PRG/146C.c:9035)
void SetSharedSequenceBank(AnimationControlHost& host, int bank) {
  static_cast<void>(SelectSharedSequenceBank(host, bank));
}

// decomp: BATTLE.PRG func_8007CC8C (src/BATTLE/BATTLE.PRG/146C.c:9037)
void SharedSequenceBank(AnimationControlHost& host) {
  static_cast<void>(SharedSequenceBankId(host));
}

// decomp: BATTLE.PRG func_8007CEA0 (src/BATTLE/BATTLE.PRG/146C.c:9115)
void ReturnActorToIdle(AnimationControlHost& host, int actor_id) {
  RestoreActorIdlePose(host, actor_id);
}

// decomp: BATTLE.PRG func_8007CEC0 (src/BATTLE/BATTLE.PRG/146C.c:9117)
void ReturnAllActorsToIdle(AnimationControlHost& host) {
  for (int index = 0; index < kBattleActorSlots; ++index) {
    if (host.HasBattleActor(index)) ReturnActorToIdle(host, index);
  }
}

// decomp: BATTLE.PRG func_8009CE9C (src/BATTLE/BATTLE.PRG/30DB0.c:1894)
int SelectSharedSequenceBank(AnimationControlHost& host, int bank) {
  if (bank < 1 || bank > kSharedSequenceBanks) return -1;
  const int current = host.SharedSequenceBankIndex();
  if (current < 0 || current >= kSharedSequenceBanks) {
    VS_LOG_WARN("actors",
                "Shared bank texture reset refused: bank index {} is not 0 to "
                "{}",
                current, kSharedSequenceBanks - 1);
  } else {
    const auto from = static_cast<std::size_t>(current);
    const auto to = static_cast<std::size_t>(bank - 1);
    for (int slot = 0; slot < kActorSlots; ++slot) {
      AnimationActor* actor = host.Actor(slot);
      if (actor == nullptr || actor->texture_rows.at(from) == kNoTextureRow ||
          actor->texture_rows.at(to) != kNoTextureRow) {
        continue;
      }
      for (int channel = 0; channel < kTextureAnimationChannels; ++channel) {
        if (actor->texture_current_frame.at(
                static_cast<std::size_t>(channel)) >= kSkippedTextureFrames) {
          host.UploadTextureFrame(*actor, channel, 0);
        }
      }
    }
  }
  host.SetSharedSequenceBankIndex(bank - 1);
  return 0;
}

// decomp: BATTLE.PRG func_8009CFA0 (src/BATTLE/BATTLE.PRG/30DB0.c:1918)
int SharedSequenceBankId(AnimationControlHost& host) {
  return host.SharedSequenceBankIndex() + 1;
}

// decomp: BATTLE.PRG func_8009D854 (src/BATTLE/BATTLE.PRG/30DB0.c:2188)
void RequestActorAnimationAdvance(AnimationControlHost& host, int actor_id) {
  if (AnimationActor* actor = host.Actor(actor_id)) {
    actor->advance_requested = true;
  }
}

// decomp: BATTLE.PRG func_8009D934 (src/BATTLE/BATTLE.PRG/30DB0.c:2219)
void SetActorWeaponDrawn(AnimationControlHost& host, int actor_id,
                         int weapon_drawn, int transition_mode) {
  constexpr int kModelsOnly = 2;
  AnimationActor* actor = host.Actor(actor_id);
  if (actor == nullptr) return;
  if (transition_mode == kModelsOnly) {
    actor->weapon_drawn = LowBit(weapon_drawn) != 0;
    host.SetWeaponModelsDrawn(actor_id, weapon_drawn != 0);
    return;
  }
  int blend_frames = kSheatheBlend;
  int sound_base = 0;
  if (transition_mode != 0) {
    blend_frames = kDrawBlend;
    sound_base = host.DrawnWeaponSoundBase(actor->sequence_model_id);
  } else {
    sound_base = host.SheathedWeaponSoundBase(actor->sequence_model_id);
  }
  if (host.EventPlaybackState() == kEventScriptRunning) {
    actor->weapon_drawn = LowBit(weapon_drawn) != 0;
    PlayActorAnimation(host, actor_id, kIdleAnimation, kDrawBlend);
    return;
  }
  if (weapon_drawn == (actor->weapon_drawn ? 1 : 0)) return;
  if (weapon_drawn == 0) {
    if (actor->model_id == 0) host.PlaySoundPair(*actor, sound_base + 1, 0);
    PlayActorAnimation(host, actor_id,
                       transition_mode * 2 + kSheatheAnimationStep,
                       blend_frames);
    actor->motion.weapon_pose_active = true;
    actor->weapon_drawn = false;
  } else {
    if (actor->model_id == 0) host.PlaySoundPair(*actor, sound_base, 0);
    actor->weapon_drawn = LowBit(weapon_drawn) != 0;
    PlayActorAnimation(host, actor_id, transition_mode * 2 + kDrawAnimationStep,
                       blend_frames);
    actor->motion.weapon_pose_active = true;
  }
  QueueActorAnimation(host, actor_id, kIdleAnimation, kDrawBlend, 0);
}

// decomp: BATTLE.PRG func_8009DBDC (src/BATTLE/BATTLE.PRG/30DB0.c:2296)
void PlayActorWeaponDrawSound(AnimationControlHost& host, int actor_id,
                              int sound_offset) {
  if (AnimationActor* actor = host.Actor(actor_id)) {
    host.PlaySoundPair(
        *actor,
        host.DrawnWeaponSoundBase(actor->sequence_model_id) + sound_offset, 0);
  }
}

// decomp: BATTLE.PRG func_8009E070 (src/BATTLE/BATTLE.PRG/30DB0.c:2451)
void PlayActorActionPose(AnimationControlHost& host, int actor_id,
                         const MotionVector* facing_target, int pose_kind) {
  int animation = 0;
  if (Within(pose_kind, kFirstStrikePose, kLastStrikePose)) {
    animation = pose_kind + kActionPoseBase;
  } else if (pose_kind == kBracePose) {
    animation = kBracePoseAnimation;
  } else if (pose_kind == kGuardPose) {
    animation = kGuardPoseAnimation;
  } else if (pose_kind != kReadyPose) {
    return;
  }
  AnimationActor* actor = RequireActor(host, actor_id, "Action pose");
  if (actor == nullptr) return;
  ActorMotion& motion = actor->motion;
  if (pose_kind == kReadyPose) {
    animation =
        motion.action_pose_active ? kIdleAnimation : kReadyPoseAnimation;
    if (motion.movement_mode == kAirborneMovement ||
        motion.locomotion_state == kHanging) {
      return;
    }
  }
  motion.facing_delta = Wrap16(ActorFacingDeltaToPoint(motion, facing_target));
  motion.turn_frames = kActionTurnFrames;
  motion.run_frames = 0;
  motion.weapon_pose_active = true;
  PlayActorAnimation(host, actor_id, animation, kActionPoseBlend);
  actor->queued_animation = 0;
}

// decomp: BATTLE.PRG func_8009E480 (src/BATTLE/BATTLE.PRG/30DB0.c:2619)
bool IsAshleyTransitionPose(AnimationControlHost& host) {
  const AnimationActor* actor =
      RequireActor(host, kAshleySlot, "Ashley transition pose");
  return actor != nullptr && (actor->animation_id == kTransitionAnimation ||
                              actor->animation_id == kDrawnTransitionAnimation);
}

// decomp: BATTLE.PRG func_8009E4B0 (src/BATTLE/BATTLE.PRG/30DB0.c:2627)
ActionAnimationBusy IsActorActionAnimationBusy(AnimationControlHost& host,
                                               int actor_id) {
  const AnimationActor* actor = host.Actor(actor_id);
  if (actor == nullptr) {
    return host.GimmickForActorSlot(actor_id) ? ActionAnimationBusy::kIdle
                                              : ActionAnimationBusy::kNoActor;
  }
  const int state = actor->motion.action_animation_state;
  if (state != 0 && state != kGuardActionState &&
      state != kRecoverActionState) {
    return ActionAnimationBusy::kBusy;
  }
  if (actor->holding_last_frame) return ActionAnimationBusy::kIdle;
  int animation = actor->animation_id;
  bool busy = false;
  if (animation >= kDrawnAnimationOffset) {
    animation -= kDrawnAnimationOffset;
    busy = Within(animation, kFirstWeaponDrawAnimation,
                  kLastWeaponDrawAnimation) ||
           animation == kReadyPoseAnimation ||
           (!Within(animation, kFirstIdleAction, kLastIdleAction) &&
            animation >= kFirstBusyAnimation);
  } else {
    busy = animation == kReadyPoseAnimation || animation == kBusyAnimation ||
           Within(animation, kFirstBusyAnimation, kLastBusyAnimation) ||
           Within(animation, kFirstStrikeAnimation, kLastStrikeAnimation) ||
           animation == kStrikeAnimation;
  }
  return busy ? ActionAnimationBusy::kBusy : ActionAnimationBusy::kIdle;
}

// decomp: BATTLE.PRG func_8009E5C4 (src/BATTLE/BATTLE.PRG/30DB0.c:2685)
void RestoreActorIdlePose(AnimationControlHost& host, int actor_id) {
  const AnimationActor* actor = host.Actor(actor_id);
  if (actor == nullptr || (actor->animation_id != kTransitionAnimation &&
                           actor->animation_id != kDrawnTransitionAnimation)) {
    RestoreActorBasePose(host, actor_id, kIdleAnimation);
  }
}

// decomp: BATTLE.PRG func_8009E614 (src/BATTLE/BATTLE.PRG/30DB0.c:2693)
void RestoreActorNeutralPose(AnimationControlHost& host, int actor_id) {
  RestoreActorBasePose(host, actor_id, kNeutralAnimation);
}

// decomp: BATTLE.PRG func_8009E700 (src/BATTLE/BATTLE.PRG/30DB0.c:2727)
void RestoreActorBasePose(AnimationControlHost& host, int actor_id,
                          int animation) {
  AnimationActor* actor = host.Actor(actor_id);
  if (actor == nullptr) {
    const std::optional<AnimationGimmick> gimmick =
        host.GimmickForActorSlot(actor_id);
    if (gimmick &&
        (gimmick->mode == kGimmickSettling ||
         gimmick->mode == kGimmickSettlingAlt) &&
        gimmick->support_index == 0) {
      host.SetGimmickMode(actor_id, 0);
    }
    return;
  }
  if (actor->animation_id == kHeldPoseAnimation ||
      actor->animation_id == kHeldDrawnPoseAnimation) {
    return;
  }
  ActorMotion& motion = actor->motion;
  if (motion.slot == kAshleySlot && motion.locomotion_state == kClimbing &&
      !motion.movement_pending) {
    return;
  }
  if (motion.movement_mode < kFirstHeldMovement &&
      motion.pose_state != kKnockedDownPose &&
      motion.pose_state != kFallenPose) {
    bool settle = true;
    if (actor->reaction_state == kTeleportReaction) {
      host.CancelTeleportTransition(*actor);
    } else if (actor->reaction_state != 0) {
      settle = false;
    }
    if (settle) SettleBasePose(host, *actor, actor_id, animation);
  }
  if (motion.locomotion_state != kHanging) motion.locomotion_state = 0;
  motion.scale_frames = 0;
  motion.turn_frames = 0;
  motion.move_frames = 0;
  motion.movement_pending = false;
  ResetActorRunMomentum(motion);
}

// decomp: BATTLE.PRG func_8009F8DC (src/BATTLE/BATTLE.PRG/30DB0.c:3373)
int AnimationBattleWindowFrame(AnimationControlHost& host, int actor_id) {
  AnimationActor* actor = RequireActor(host, actor_id, "Battle window frame");
  if (actor == nullptr || actor->pose.script == EventScriptHandle::kNone) {
    return 0;
  }
  const PoseEventCursor saved = actor->pose;
  const int frame = host.RunBattleWindowEvents(*actor);
  actor->pose = saved;
  return frame;
}

// decomp: BATTLE.PRG func_8009F990 (src/BATTLE/BATTLE.PRG/30DB0.c:3401)
void SetActorSoundOverrides(AnimationControlHost& host, int actor_id,
                            const SoundEffectSlot& sound_override) {
  if (actor_id != -1) {
    ApplyActorSoundOverrides(host, actor_id, sound_override);
    return;
  }
  for (int slot = 0; slot < kActorSlots; ++slot) {
    ApplyActorSoundOverrides(host, slot, sound_override);
  }
}

// decomp: BATTLE.PRG func_8009F9F4 (src/BATTLE/BATTLE.PRG/30DB0.c:3414)
void ApplyActorSoundOverrides(AnimationControlHost& host, int actor_id,
                              const SoundEffectSlot& sound_override) {
  constexpr std::uint8_t kSilent = 0;
  constexpr std::uint8_t kPositional = 1;
  constexpr std::uint8_t kFixed = 2;
  constexpr std::uint8_t kFixedLevel = 0x80;
  AnimationActor* actor = host.Actor(actor_id);
  if (actor == nullptr) return;
  SoundEffectSlot& effect = actor->sound_effect;
  if (sound_override.kind == kSilent) {
    effect.level = 0;
    effect.flags = 0;
  } else if (sound_override.kind == kPositional) {
    if (sound_override.parameter == 0) {
      effect = sound_override;
    } else {
      actor->sound_effect_target = sound_override;
      effect.parameter = sound_override.parameter;
      if (effect.kind == kSilent) {
        const SoundPanVolume sound = host.SoundPanAndVolume(actor_id);
        effect.level = static_cast<std::uint8_t>(sound.pan);
        effect.flags = static_cast<std::uint8_t>(sound.volume);
      } else if (effect.kind == kPositional) {
        const SoundPanVolume sound = host.SoundPanAndVolume(actor_id);
        if (effect.level == 0) {
          effect.level = static_cast<std::uint8_t>(sound.pan);
        }
        if (effect.flags == 0) {
          effect.flags = static_cast<std::uint8_t>(sound.volume);
        }
      } else if (effect.kind == kFixed) {
        effect.level = kFixedLevel;
        effect.flags = kPositional;
      }
      if (sound_override.level == 0) effect.level = 0;
      if (sound_override.flags == 0) effect.flags = 0;
    }
  }
  effect.kind = sound_override.kind;
}

// decomp: BATTLE.PRG func_8009FBD8 (src/BATTLE/BATTLE.PRG/30DB0.c:3480)
int SetActorTravelAnimation(AnimationControlHost& host, int actor_id,
                            int animation) {
  AnimationActor* actor = host.Actor(actor_id);
  if (actor == nullptr) return -1;
  actor->travel_animation = animation == -1
                                ? kTravelWalkAnimation
                                : static_cast<std::uint8_t>(animation);
  return 0;
}

// decomp: BATTLE.PRG func_8009FC20 (src/BATTLE/BATTLE.PRG/30DB0.c:3498)
std::optional<int> ActorTravelAnimation(AnimationControlHost& host,
                                        int actor_id) {
  const AnimationActor* actor = host.Actor(actor_id);
  if (actor == nullptr) return std::nullopt;
  return actor->travel_animation;
}

// decomp: BATTLE.PRG func_800A01C8 (src/BATTLE/BATTLE.PRG/30DB0.c:3710)
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
int QueueActorAnimation(AnimationControlHost& host, int actor_id, int animation,
                        int blend_frames, int start_frame) {
  AnimationActor* actor = host.Actor(actor_id);
  if (actor == nullptr) return -1;
  actor->queued_animation = static_cast<std::uint8_t>(animation);
  actor->queued_blend_frames = static_cast<std::uint8_t>(blend_frames);
  actor->queued_start_frame = static_cast<std::uint8_t>(start_frame);
  return 0;
}

// decomp: BATTLE.PRG func_800A0204 (src/BATTLE/BATTLE.PRG/30DB0.c:3725)
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
int PlayActorAnimation(AnimationControlHost& host, int actor_id, int animation,
                       int blend_frames) {
  if (animation == 0) return 0;
  AnimationActor* actor = host.Actor(actor_id);
  if (actor == nullptr) return -1;
  if (actor->animation_locked) return 0;
  if (actor->pose_override_active && host.RestartAnimationEvents(*actor) < 0) {
    return -1;
  }
  actor->animation_event_flag = false;
  actor->pose_override_active = false;
  ActorMotion& motion = actor->motion;
  if (animation != kRunTurnAnimation && animation != kRunStraightAnimation) {
    motion.is_running = false;
  }
  actor->texture_overlay_index = {};
  host.RestartTextureAnimation(actor_id);
  ClearActorBonePoseLocks(host, *actor);
  actor->fast_animation = false;

  if (animation < kDrawnAnimationOffset) {
    const std::optional<int> stance = StanceAnimation(*actor, animation);
    if (!stance) return 0;
    animation = *stance;
  }

  motion.weapon_pose_active = false;
  const bool sheathed = !actor->weapon_drawn;
  if (sheathed &&
      (animation == kClimbAnimation || animation == kClimbTurnAnimation)) {
    motion.locomotion_state = kClimbing;
    actor->move_facing = static_cast<std::uint16_t>(
        (motion.facing + motion.facing_delta) & kAngleMask);
  } else if (sheathed &&
             (animation == kHangAnimation || animation == kHangTurnAnimation)) {
    motion.locomotion_state = kHanging;
    actor->move_facing = static_cast<std::uint16_t>(
        (motion.facing + motion.facing_delta) & kAngleMask);
  } else if (motion.root_motion.x == 0 && motion.root_motion.y == 0 &&
             motion.root_motion.z == 0) {
    motion.locomotion_state = 0;
    motion.movement_pending = false;
  }

  // An animation the actor's banks lack falls back to animation 0.
  AnimationLookup found = host.FindAnimationHeader(*actor, animation);
  while (found.bank == SequenceHandle::kNone || !found.has_entry) {
    motion.is_running = false;
    if (animation == 0) return 0;
    animation = 0;
    found = host.FindAnimationHeader(*actor, animation);
  }

  actor->animation_id = static_cast<std::uint8_t>(animation);
  if (host.EventPlaybackState() == kEventScriptRunning ||
      actor->animation_header != found.bank ||
      found.resolved_set != actor->animation_set) {
    actor->animation_set = found.resolved_set;
    actor->animation_header = found.bank;
    actor->animation_length = found.length;
    host.InitializePose(*actor);
    actor->queued_animation = 0;
    host.BeginShadowBlend(*actor, blend_frames);
    if (actor->collision_radius >= kLargeActorRadius) blend_frames *= 2;
    if (blend_frames >= 2) {
      actor->blend_frames = static_cast<std::uint8_t>(blend_frames);
      actor->blend_frame = 1;
    } else {
      actor->blend_frames = 0;
      actor->blend_frame = 0;
    }
  }
  return 0;
}

// decomp: BATTLE.PRG func_800A0618 (src/BATTLE/BATTLE.PRG/30DB0.c:3850)
int PlayActorAnimationAtFrame(AnimationControlHost& host, int actor_id,
                              int animation, int blend_frames,
                              int start_frame) {
  const int result =
      PlayActorAnimation(host, actor_id, animation, blend_frames);
  if (result < 0) return result;
  if (start_frame == 0) return 0;
  start_frame -= host.TickSpeed() / 2;
  AnimationActor* actor = RequireActor(host, actor_id, "Animation start frame");
  if (actor == nullptr) return 0;
  if (start_frame > 0) {
    actor->frames_per_step = static_cast<std::uint8_t>(start_frame);
    host.AdvanceSkeletalAnimation(*actor);
  }
  actor->frames_per_step = 1;
  return 0;
}

// decomp: BATTLE.PRG func_800A0870 (src/BATTLE/BATTLE.PRG/30DB0.c:3948)
void SetActorTransitionMode(AnimationControlHost& host, int control) {
  host.SetTransitionControl(control);
}

// decomp: BATTLE.PRG func_800A09D8 (src/BATTLE/BATTLE.PRG/30DB0.c:3987)
void SetActorAnimationLocked(AnimationControlHost& host, int actor_id,
                             int locked) {
  if (AnimationActor* actor = host.Actor(actor_id)) {
    actor->animation_locked = LowBit(locked) != 0;
  }
}

// decomp: BATTLE.PRG func_800A249C (src/BATTLE/BATTLE.PRG/38C1C.c:578)
void PlayActorTurnAnimation(AnimationControlHost& host, int actor_id,
                            int target_facing) {
  AnimationActor* actor = RequireActor(host, actor_id, "Turn animation");
  if (actor == nullptr) return;
  ActorMotion& motion = actor->motion;
  if (motion.motion_locked) return;
  int facing_delta = target_facing - motion.facing;
  if (facing_delta > kHalfTurn) facing_delta -= kAngleOne;
  if (facing_delta < -kHalfTurn) facing_delta += kAngleOne;
  if (facing_delta == 0) return;
  motion.request_mode = kTurnRequest;
  if (actor->animation_id == kTurnLockDrawnAnimation ||
      actor->animation_id == kTurnLockAnimation || actor->support_layer != 0) {
    return;
  }
  int turn = kJumpIdleAnimation;
  if (!motion.is_jumping) {
    turn = facing_delta < 0 ? kTurnLeftAnimation : kTurnRightAnimation;
  }
  PlayActorAnimation(host, actor_id, turn, kTurnBlend);
}

// decomp: BATTLE.PRG func_800A6AA0 (src/BATTLE/BATTLE.PRG/3A1A0.c:2032)
void UpdateActorLocomotionAnimation(AnimationControlHost& host,
                                    AnimationActor& actor, int speed) {
  int animation = actor.animation_id;
  if (animation >= kDrawnAnimationOffset) animation -= kDrawnAnimationOffset;
  const ActorMotion& motion = actor.motion;
  const int quadrant = RequestQuadrant(motion);
  if (motion.action_pose_active) {
    if (animation == kStanceHoldAnimation) return;
    PlayActorAnimation(host, motion.slot,
                       speed > 0 ? kStanceWalkAnimation : kStanceAnimation,
                       speed > 0 ? kStanceWalkBlend : kStanceIdleBlend);
    return;
  }
  if (motion.is_jumping) {
    int next = kIdleAnimation;
    int blend_frames = kJumpIdleBlend;
    if (speed >= motion.run_speed) {
      next = host.QuadrantMoveAnimation(true, quadrant);
      blend_frames = kJumpBlend;
    } else if (speed > 0) {
      next = host.QuadrantMoveAnimation(false, quadrant);
      blend_frames = kJumpBlend;
    }
    PlayActorAnimation(host, motion.slot, next, blend_frames);
    return;
  }
  if (motion.is_running) {
    UpdateRunAnimation(host, actor, animation);
  } else if (speed >= motion.run_speed - 1) {
    UpdateFastAnimation(host, actor, animation);
  } else if (speed > 0) {
    UpdateWalkAnimation(host, actor, animation, quadrant);
  } else {
    UpdateIdleAnimation(host, actor, animation);
  }
}

// decomp: BATTLE.PRG func_800AC440 (src/BATTLE/BATTLE.PRG/40564.c:2199)
void SetActorBonePoseLock(AnimationControlHost& host, AnimationActor& actor,
                          int bone, int lock) {
  if (!actor.has_model || bone < 0 || bone >= actor.bone_count) {
    VS_LOG_WARN("actors",
                "Bone pose lock refused: bone {} is outside the {} bones of "
                "actor {}'s model",
                bone, actor.has_model ? actor.bone_count : 0,
                actor.motion.slot);
    return;
  }
  for (int index = 0; index < kBonePoseLocks; ++index) {
    std::uint8_t& locked =
        actor.locked_bones.at(static_cast<std::size_t>(index));
    if (lock == 0) {
      if (locked == bone) {
        locked = 0;
        host.SetBonePoseLockSlot(actor, bone, 0);
        return;
      }
    } else if (locked == 0) {
      host.CaptureBoneAnchor(actor, bone, index);
      host.SetBonePoseLockSlot(actor, bone, index + 1);
      locked = static_cast<std::uint8_t>(bone);
      return;
    }
  }
}

// decomp: BATTLE.PRG func_800AC500 (src/BATTLE/BATTLE.PRG/40564.c:2221)
void ClearActorBonePoseLocks(AnimationControlHost& host,
                             AnimationActor& actor) {
  if (!actor.has_model && actor.bone_count != 0) {
    VS_LOG_WARN("actors",
                "Bone pose lock clear refused: actor {} has {} bones and no "
                "model",
                actor.motion.slot, actor.bone_count);
    return;
  }
  actor.locked_bones = {};
  for (int bone = 0; bone < actor.bone_count; ++bone) {
    host.SetBonePoseLockSlot(actor, bone, 0);
  }
}
}  // namespace VS::Actors
