// Hand-maintained (tools/port/owned.csv).
// The actor lifecycle rules (BATTLE.PRG 30DB0.c:2073-3650).
#include "Actors/ActorLifecycle.hpp"

#include <cstdint>
#include <optional>

#include "Actors/ActorMotion.hpp"
#include "Actors/ActorMovement.hpp"
#include "Actors/ActorStateRecord.hpp"
#include "Actors/SequencePlayback.hpp"
#include "Core/Log.hpp"

namespace VS::Actors {
namespace {
constexpr int kAshleyStandAnimation = 1;
constexpr int kActorStandAnimation = 0x62;
constexpr std::int8_t kNoHeadTrackTarget = -2;
constexpr std::int16_t kFullCollision = 4;
constexpr std::int16_t kUnitScale = 0x1000;
constexpr std::uint8_t kModeAirborne = 3;
constexpr std::uint8_t kLocomotionStopping = 2;
constexpr std::int16_t kDeathFallSpeed = -6;
constexpr int kDeathAnimation = 0x46;
constexpr int kPoseDeathAnimation = 0x3A;
constexpr int kGroundDeathAnimation = 0x47;
// The animation whose presence gives a grounded actor its own death.
constexpr int kGroundDeathProbe = 0xAB;
constexpr int kDeathBlendFrames = 6;
constexpr std::uint8_t kDeathAnimating = 1;
constexpr std::uint8_t kDeathMarked = 2;
constexpr std::int16_t kBrokenGimmickMode = 0xF7;
constexpr int kBreakSound = 0x32;
// Parents above this are outside the actor table.
constexpr int kLastAttachParent = 16;

ActorStateRecord* KnownRecord(ActorStateHost& host, int id,
                              const char* operation) {
  if (id < 0 || id >= kActorStateSlots) {
    VS_LOG_WARN("actors", "{} refused for actor {} outside the actor table",
                operation, id);
    return nullptr;
  }
  return host.Record(id);
}

void ClearMovementRequest(ActorStateRecord& record) {
  record.motion.request_mode = 0;
  record.motion.request_direction = 0;
  record.motion.request_facing = 0;
  record.motion.request_distance = 0;
  record.request_extras = {};
}

// Everything the reset clears before the head-tracking reset.
void ClearTweens(ActorStateRecord& record) {
  ActorMotion& motion = record.motion;
  motion.scale.x = kUnitScale;
  motion.scale.y = kUnitScale;
  motion.scale.z = kUnitScale;
  record.head_track_target = kNoHeadTrackTarget;
  motion.jump_countdown = 0;
  motion.scale_frames = 0;
  motion.turn_frames = 0;
  motion.rotation_x = 0;
  motion.facing = 0;
  motion.rotation_z = 0;
  record.rotation_reserved = 0;
  motion.move_frames = 0;
  record.look_pitch = 0;
  record.look_yaw = 0;
  motion.movement_mode = 0;
  motion.route_phase = 0;
  record.reaction_state = 0;
  motion.pose_state = 0;
}

void ClearPaletteEffects(ActorStateRecord& record) {
  record.palette_fade_state = 0;
  record.texture_timelines[0] = 0;
  record.palette_fade_active = false;
  record.texture_timelines[1] = 0;
  record.palette_fade_reverse = false;
}

// Everything the reset clears between the sound and animation resets.
void ClearLayers(ActorStateRecord& record) {
  record.collision_state = kFullCollision;
  record.reserved_17ff = 0;
  record.palette_pulse_kind = 0;
  record.reserved_1860 = 0;
  record.support_layer = 0;
  record.placement_layer = 0;
  record.motion.locomotion_state = 0;
  record.motion.movement_pending = false;
  record.motion.action_pose_active = false;
  record.motion.action_animation_state = 0;
}

// Everything the reset clears after the standing animation starts.
void FinishReset(ActorStateRecord& record) {
  record.queued_animation = 0;
  record.ledge_height = 0;
  record.platform_offset_x = 0;
  record.platform_offset_z = 0;
  record.motion.run_frames = 0;
  record.animation_locked = false;
  record.skipped = true;
  record.visible = true;
  record.frozen = true;
  record.motion.is_running = false;
  record.shadow = ShadowBlend{.radiusExpandRate = record.shadow_expand_rate,
                              .radiusShrinkRate = record.shadow_shrink_rate};
  record.shadow_extras = {};
  ClearMovementRequest(record);
}

void DropMovingActor(ActorStateHost& host, ActorStateRecord& record) {
  ActorMotion& motion = record.motion;
  if (motion.movement_mode == 0 && !motion.motion_locked &&
      motion.locomotion_state != kLocomotionStopping) {
    return;
  }
  if (record.support_layer != 0) {
    // Support layers from kFirstPlatformLayer up are moving platforms.
    host.ReleaseMovingPlatform(record.support_layer - kFirstPlatformLayer);
    record.support_layer = 0;
  }
  motion.move_frames = 0;
  motion.root_motion.x = 0;
  motion.root_motion.z = 0;
  if (motion.slot != 0 || motion.movement_mode != kModeAirborne ||
      motion.root_motion.y <= 0) {
    motion.root_motion.y = kDeathFallSpeed;
  }
  motion.destination_mode = 0;
  motion.movement_mode = kModeAirborne;
}

void PlayDeath(ActorStateHost& host, int id, ActorStateRecord& record) {
  int animation =
      record.motion.action_pose_active ? kPoseDeathAnimation : kDeathAnimation;
  if (host.HasAnimation(id, kGroundDeathProbe) &&
      record.motion.movement_mode == 0) {
    animation = kGroundDeathAnimation;
  }
  host.PlayAnimation(id, animation, kDeathBlendFrames);
  host.ResetHeadTrackingSpeed(id);
  record.motion.action_animation_state = kDeathAnimating;
}

void BreakGimmick(ActorStateHost& host, int id) {
  if (!host.Gimmick(id).has_value()) return;
  host.SetGimmickMode(id, kBrokenGimmickMode);
  host.ClearGimmickSupport(id);
  host.PlayGimmickSound(id, kBreakSound, 0);
}
}  // namespace

void ResetActorMotionState(ActorStateHost& host, int id) {
  ActorStateRecord* record = KnownRecord(host, id, "Motion reset");
  if (record == nullptr) return;
  ClearTweens(*record);
  host.ResetHeadTrackingSpeed(id);
  if (host.SharedBankHasTexturePages()) {
    host.UploadTextureFrame(id, 0, 0);
    host.UploadTextureFrame(id, 1, 0);
  }
  ClearPaletteEffects(*record);
  host.ClearTint(id);
  if (record->companion_slot == 1) record->companion_slot = 0;
  record->sound_enabled = true;
  host.ClearSoundOverrides(id);
  ClearLayers(*record);
  host.PlayAnimation(id, id == 0 ? kAshleyStandAnimation : kActorStandAnimation,
                     0);
  FinishReset(*record);
}

void ResetAllActorMovement(ActorStateHost& host) {
  for (int id = 0; id < kActorStateSlots; ++id) {
    ActorStateRecord* record = host.Record(id);
    if (record == nullptr) continue;
    ActorMotion& motion = record->motion;
    motion.action_pose_active = false;
    motion.jump_countdown = 0;
    motion.weapon_pose_active = false;
    record->support_layer = 0;
    record->placement_layer = 0;
    motion.locomotion_state = 0;
    motion.movement_pending = false;
    motion.is_running = false;
    motion.movement_mode = 0;
    ClearMovementRequest(*record);
  }
}

void BeginActorDeath(ActorStateHost& host, int id, bool animate) {
  int current = id;
  // Every record once, then the slot after the last of them.
  for (int visited = 0; visited <= kActorStateSlots; ++visited) {
    ActorStateRecord* record = KnownRecord(host, current, "Death");
    if (record == nullptr) {
      BreakGimmick(host, current);
      return;
    }
    DropMovingActor(host, *record);
    if (animate) {
      PlayDeath(host, current, *record);
    } else {
      record->motion.action_animation_state = kDeathMarked;
    }
    if (record->companion_slot < kFirstCompanionRecord) return;
    current = record->companion_slot;
  }
  VS_LOG_WARN("actors", "Death stops at a repeating companion chain from {}",
              id);
}

void SetActorCompanion(ActorStateHost& host, int id,
                       std::uint8_t companion_slot,
                       const std::optional<CompanionStyle>& style) {
  ActorStateRecord* record = KnownRecord(host, id, "Companion");
  if (record == nullptr) return;
  record->companion_slot = companion_slot;
  if (companion_slot == 0) return;
  if (!style.has_value()) {
    VS_LOG_WARN("actors", "Companion {} of actor {} has no style; keeping it",
                companion_slot, id);
    return;
  }
  record->companion = *style;
}

void AttachActorToParentBone(ActorStateHost& host, int id,
                             const std::optional<AttachPoint>& point) {
  ActorStateRecord* record = KnownRecord(host, id, "Attachment");
  if (record == nullptr) {
    VS_LOG_WARN("actors", "No actor {} to attach", id);
    return;
  }
  if (point.has_value()) {
    if (record->attach_parent != kNoAttachParent) {
      ActorStateRecord* old_parent = host.Record(record->attach_parent);
      if (old_parent != nullptr) {
        if (old_parent->attached_child_count == 0) {
          VS_LOG_WARN("actors", "Actor {} has no attached child to release",
                      record->attach_parent);
        } else {
          --old_parent->attached_child_count;
        }
      }
    }
    record->attach_parent = point->parent;
    record->attach_bone = point->bone;
  }
  record->motion.position = {};
  record->motion.move_frames = 0;
  record->motion.rotation_x = 0;
  record->motion.facing = 0;
  record->motion.rotation_z = 0;
  record->rotation_reserved = 0;
  record->motion.turn_frames = 0;
  record->attached = true;
}

AttachmentRelease ReleaseActorAttachmentLock(ActorStateHost& host, int id) {
  ActorStateRecord* record = KnownRecord(host, id, "Attachment release");
  if (record == nullptr || record->attach_parent == kNoAttachParent) {
    return AttachmentRelease::kNoParent;
  }
  const int parent_id = record->attach_parent;
  if (parent_id > kLastAttachParent) return AttachmentRelease::kParentNotReady;
  const ActorStateRecord* parent = host.Record(parent_id);
  if (parent == nullptr || parent->bone_count == 0) {
    return AttachmentRelease::kParentNotReady;
  }
  record->attached = false;
  return AttachmentRelease::kReleased;
}
}  // namespace VS::Actors
