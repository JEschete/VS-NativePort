#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <cstdint>

#include "Actors/ActorRoster.hpp"
#include "Actors/ActorStateRecord.hpp"

// The actor state and display flags (BATTLE.PRG 146C.c:8623-8646 and
// 30DB0.c:3950-4180): the word that summarises an actor's display, motion,
// attachment and animation state, the display flags written back from it,
// and the model enable and disable switches built on both. The rules read
// records and gimmicks through ActorStateHost, so the flags are composed from
// typed fields rather than the record's bitfield words.
namespace VS::Actors {
// The bits of the state flags word; the display flags use the same bits.
struct StateFlag {
  static constexpr std::uint32_t kShown = 0x1;
  static constexpr std::uint32_t kVisible = 0x2;
  static constexpr std::uint32_t kFrozen = 0x4;
  // A translation (or a route) is playing out.
  static constexpr std::uint32_t kTranslating = 0x8;
  // A turn (or a route) is playing out.
  static constexpr std::uint32_t kTurning = 0x10;
  static constexpr std::uint32_t kScaling = 0x20;
  static constexpr std::uint32_t kSoundEnabled = 0x40;
  // Holding the last pose frame also raises kEventBit.
  static constexpr std::uint32_t kHoldingLastFrame = 0x08000080;
  static constexpr std::uint32_t kPoseEnded = 0x100;
  static constexpr std::uint32_t kHasAttachedChildren = 0x200;
  static constexpr std::uint32_t kAttachedToParent = 0x400;
  static constexpr std::uint32_t kFirstWeaponShown = 0x800;
  static constexpr std::uint32_t kSecondWeaponShown = 0x1000;
  static constexpr std::uint32_t kAnimationLocked = 0x2000;
  static constexpr std::uint32_t kMovementActive = 0x4000;
  // Set for every actor record; gimmicks never carry it.
  static constexpr std::uint32_t kActorRecord = 0x8000;
  // The support layer occupies bits 16-19, overlapping kActionPose.
  static constexpr unsigned kSupportLayerShift = 16;
  static constexpr std::uint32_t kActionPose = 0x80000;
  static constexpr std::uint32_t kLocomotion = 0x100000;
  static constexpr std::uint32_t kMotionLocked = 0x200000;
  static constexpr std::uint32_t kOnGimmick = 0x400000;
  static constexpr std::uint32_t kActionAnimating = 0x01000000;
  static constexpr std::uint32_t kLightReaction = 0x02000000;
  static constexpr std::uint32_t kPoseActive = 0x04000000;
  // The animation event, actor collision bypass and held last frame share it.
  static constexpr std::uint32_t kEventBit = 0x08000000;
  static constexpr std::uint32_t kHeavyReaction = 0x10000000;
  static constexpr std::uint32_t kLocomotionStarting = 0x20000000;
  static constexpr std::uint32_t kMovementPending = 0x40000000;
};

// Whether the slot's two weapon models exist and are shown.
struct WeaponDisplay {
  bool first = false;
  bool second = false;
};

// The display settings a display flags word sets (the same bits as the
// state flags).
struct ActorDisplay {
  bool skipped = false;
  bool visible = false;
  bool frozen = false;
  bool sound_enabled = false;
  bool first_weapon_shown = false;
  bool second_weapon_shown = false;

  friend bool operator==(const ActorDisplay&, const ActorDisplay&) = default;
};

[[nodiscard]] std::uint32_t ComposeActorStateFlags(
    const ActorStateRecord& record, WeaponDisplay weapons);
[[nodiscard]] std::uint32_t ComposeGimmickStateFlags(
    const GimmickView& gimmick);
[[nodiscard]] ActorDisplay DecodeActorDisplayFlags(std::uint32_t flags);

// decomp: BATTLE.PRG func_800A0BE0 (src/BATTLE/BATTLE.PRG/30DB0.c:4065)
// The actor's state flags, a gimmick's display flags for an actor slot with
// no record, or 0. The weapon models are the record's own slot's.
[[nodiscard]] std::uint32_t ActorStateFlags(ActorStateHost& host, int id);
// decomp: BATTLE.PRG func_800A087C (src/BATTLE/BATTLE.PRG/30DB0.c:3950)
// Applies the display flags to the actor, its weapon models (two per actor
// id) and its companion chain, or to the gimmick in an empty slot. A
// companion chain that repeats stops after one pass over the actor table.
void SetActorDisplayFlags(ActorStateHost& host, int id, std::uint32_t flags);

// The roster slots the model switches cover, with their battle actors.
class ActorModelToggleHost : public DisplayFlagsHost {
 public:
  [[nodiscard]] virtual bool HasBattleActor(std::size_t slot) = 0;
};

// decomp: BATTLE.PRG func_8007BF6C (src/BATTLE/BATTLE.PRG/146C.c:8623)
void EnableActorModel(ActorModelToggleHost& host, std::size_t slot);
// decomp: BATTLE.PRG func_8007BFC0 (src/BATTLE/BATTLE.PRG/146C.c:8630)
void EnableAllActorModels(ActorModelToggleHost& host);
// decomp: BATTLE.PRG func_8007BFF8 (src/BATTLE/BATTLE.PRG/146C.c:8639)
void DisableActorModel(ActorModelToggleHost& host, std::size_t slot);
// decomp: BATTLE.PRG func_8007C050 (src/BATTLE/BATTLE.PRG/146C.c:8646)
void DisableAllActorModels(ActorModelToggleHost& host);
}  // namespace VS::Actors
