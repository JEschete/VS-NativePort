// Hand-maintained (tools/port/owned.csv).
// The actor state and display flags (BATTLE.PRG 146C.c:8623-8646 and
// 30DB0.c:3950-4180).
#include "Actors/ActorFlags.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

#include "Actors/ActorStateRecord.hpp"
#include "Core/Log.hpp"

namespace VS::Actors {
namespace {
// The action animations (0x47-0x54) count as an action pose.
constexpr unsigned kFirstActionAnimation = 0x47;
constexpr unsigned kActionAnimationCount = 0xE;
// Action animation states 1-5 are playing; 6 and up are finished.
constexpr unsigned kActionAnimationStates = 5;
// Reaction states 1 and 2 are light; 3 and up heavy.
constexpr unsigned kHeavyReactionState = 3;
constexpr std::uint8_t kLocomotionStartingState = 1;
// The display flag bits SetActorDisplayFlags reads.
constexpr unsigned kVisibleBit = 1;
constexpr unsigned kFrozenBit = 2;
constexpr unsigned kSoundBit = 6;
constexpr unsigned kFirstWeaponBit = 11;
constexpr unsigned kSecondWeaponBit = 12;

bool Bit(std::uint32_t flags, unsigned bit) {
  return ((flags >> bit) & 1U) != 0;
}

std::uint32_t MotionFlags(const ActorMotion& motion) {
  std::uint32_t flags = 0;
  if (motion.route_phase != 0) {
    flags |= StateFlag::kTranslating | StateFlag::kTurning;
  } else {
    if (motion.move_frames != 0) flags |= StateFlag::kTranslating;
    if (motion.turn_frames != 0) flags |= StateFlag::kTurning;
  }
  if (motion.scale_frames != 0) flags |= StateFlag::kScaling;
  if (motion.movement_mode != 0) flags |= StateFlag::kMovementActive;
  if (motion.locomotion_state != 0) flags |= StateFlag::kLocomotion;
  if (motion.motion_locked) flags |= StateFlag::kMotionLocked;
  if (motion.pose_state != 0) flags |= StateFlag::kPoseActive;
  if (motion.locomotion_state == kLocomotionStartingState) {
    flags |= StateFlag::kLocomotionStarting;
  }
  if (motion.movement_pending) flags |= StateFlag::kMovementPending;
  if (motion.action_animation_state != 0 &&
      motion.action_animation_state <= kActionAnimationStates) {
    flags |= StateFlag::kActionAnimating;
  }
  return flags;
}

std::uint32_t RecordFlags(const ActorStateRecord& record) {
  std::uint32_t flags = 0;
  if (!record.skipped) flags |= StateFlag::kShown;
  if (record.visible) flags |= StateFlag::kVisible;
  if (record.frozen) flags |= StateFlag::kFrozen;
  if (record.sound_enabled) flags |= StateFlag::kSoundEnabled;
  if (record.holding_last_frame) flags |= StateFlag::kHoldingLastFrame;
  if (record.pose_reached_end) flags |= StateFlag::kPoseEnded;
  if (record.attached_child_count != 0) {
    flags |= StateFlag::kHasAttachedChildren;
  }
  if (record.attach_parent != kNoAttachParent) {
    flags |= StateFlag::kAttachedToParent;
  }
  if (record.animation_locked) flags |= StateFlag::kAnimationLocked;
  flags |= static_cast<std::uint32_t>(record.support_layer)
           << StateFlag::kSupportLayerShift;
  if (record.motion.action_pose_active ||
      static_cast<unsigned>(record.animation_id - kFirstActionAnimation) <
          kActionAnimationCount) {
    flags |= StateFlag::kActionPose;
  }
  if (record.support_gimmick != 0) flags |= StateFlag::kOnGimmick;
  if (record.animation_event || record.bypass_actor_collision) {
    flags |= StateFlag::kEventBit;
  }
  if (record.reaction_state != 0 &&
      record.reaction_state < kHeavyReactionState) {
    flags |= StateFlag::kLightReaction;
  }
  if (record.reaction_state >= kHeavyReactionState) {
    flags |= StateFlag::kHeavyReaction;
  }
  return flags;
}

void ApplyDisplay(ActorStateHost& host, int id, ActorStateRecord& record,
                  const ActorDisplay& display) {
  record.skipped = display.skipped;
  record.visible = display.visible;
  record.frozen = display.frozen;
  record.sound_enabled = display.sound_enabled;
  const int first_weapon = id * 2;
  host.SetWeaponModelShown(first_weapon, display.first_weapon_shown);
  host.SetWeaponModelShown(first_weapon + 1, display.second_weapon_shown);
}

void SetModelShown(ActorModelToggleHost& host, std::size_t slot, bool shown) {
  if (!host.HasBattleActor(slot)) return;
  const std::uint32_t flags = host.StateFlags(slot);
  host.SetDisplayFlags(
      slot, shown ? flags | StateFlag::kShown : flags & ~StateFlag::kShown);
}
}  // namespace

std::uint32_t ComposeActorStateFlags(const ActorStateRecord& record,
                                     WeaponDisplay weapons) {
  std::uint32_t flags = StateFlag::kActorRecord | RecordFlags(record) |
                        MotionFlags(record.motion);
  if (weapons.first) flags |= StateFlag::kFirstWeaponShown;
  if (weapons.second) flags |= StateFlag::kSecondWeaponShown;
  return flags;
}

std::uint32_t ComposeGimmickStateFlags(const GimmickView& gimmick) {
  std::uint32_t flags = 0;
  if (!gimmick.skipped) flags |= StateFlag::kShown;
  if (gimmick.visible) flags |= StateFlag::kVisible;
  return flags;
}

ActorDisplay DecodeActorDisplayFlags(std::uint32_t flags) {
  return {.skipped = (flags & StateFlag::kShown) == 0,
          .visible = Bit(flags, kVisibleBit),
          .frozen = Bit(flags, kFrozenBit),
          .sound_enabled = Bit(flags, kSoundBit),
          .first_weapon_shown = Bit(flags, kFirstWeaponBit),
          .second_weapon_shown = Bit(flags, kSecondWeaponBit)};
}

std::uint32_t ActorStateFlags(ActorStateHost& host, int id) {
  const ActorStateRecord* record = host.Record(id);
  if (record == nullptr) {
    const std::optional<GimmickView> gimmick = host.Gimmick(id);
    return gimmick.has_value() ? ComposeGimmickStateFlags(*gimmick) : 0;
  }
  const int first_weapon = record->motion.slot * 2;
  const WeaponDisplay weapons{
      .first = host.WeaponModelShown(first_weapon).value_or(false),
      .second = host.WeaponModelShown(first_weapon + 1).value_or(false)};
  return ComposeActorStateFlags(*record, weapons);
}

void SetActorDisplayFlags(ActorStateHost& host, int id, std::uint32_t flags) {
  const ActorDisplay display = DecodeActorDisplayFlags(flags);
  int current = id;
  // Every record once, then the slot after the last of them.
  for (int visited = 0; visited <= kActorStateSlots; ++visited) {
    ActorStateRecord* record = host.Record(current);
    if (record == nullptr) {
      if (host.Gimmick(current).has_value()) {
        host.SetGimmickDisplay(current, display.skipped, display.visible);
      }
      return;
    }
    ApplyDisplay(host, current, *record, display);
    if (record->companion_slot < kFirstCompanionRecord) return;
    current = record->companion_slot;
  }
  VS_LOG_WARN("actors",
              "Display flags stop at a repeating companion chain from actor {}",
              id);
}

void EnableActorModel(ActorModelToggleHost& host, std::size_t slot) {
  SetModelShown(host, slot, true);
}

void EnableAllActorModels(ActorModelToggleHost& host) {
  for (std::size_t slot = 0; slot < kRosterSlots; ++slot) {
    EnableActorModel(host, slot);
  }
}

void DisableActorModel(ActorModelToggleHost& host, std::size_t slot) {
  SetModelShown(host, slot, false);
}

void DisableAllActorModels(ActorModelToggleHost& host) {
  for (std::size_t slot = 0; slot < kRosterSlots; ++slot) {
    DisableActorModel(host, slot);
  }
}
}  // namespace VS::Actors
