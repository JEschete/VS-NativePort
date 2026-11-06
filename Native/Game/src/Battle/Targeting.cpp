// Hand-maintained (tools/port/owned.csv).
#include "Battle/Targeting.hpp"  // NOLINT(build/include): typed API

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <vector>

#include "Core/Log.hpp"
#include "Core/Math/ScalarMath.hpp"

namespace VS::Battle {
namespace {
using Core::Math::Arctangent2;
using Core::Math::Cosine;
using Core::Math::R3000Quotient;
using Core::Math::Sine;
using Core::Math::SquareRootFixed;
using Core::Math::SquareRootInteger;
using Core::Math::WrappingAdd;
using Core::Math::WrappingMultiply;
using Core::Math::WrappingSubtract;

// First-hit effects whose preview shows an amount: the HP pair and the MP
// pair.
constexpr int kHpAmountEffect = 0x3A;
constexpr int kMpAmountEffect = 0x3B;
constexpr int kHpAmountEffectAlt = 0x3E;
constexpr int kMpAmountEffectAlt = 0x3F;
// The status bits a preview lists as removed.
constexpr std::uint32_t kRemovableStatusMask = 0x1FFFFFE0;
// A hit target word's actor slot bits.
constexpr int kActorSlotMask = 15;

// Field control and action flow modes.
constexpr int kFieldControlTargeting = 2;
constexpr int kFieldControlActionMenu = 5;
constexpr int kFlowTargetList = 1;
constexpr int kFlowGroundCursor = 2;
constexpr int kFlowAreaConfirm = 3;
// The HUD mode bits: weapon drawn, and the action menu.
constexpr int kHudWeaponBit = 1;
constexpr int kHudActionMenu = 2;
// The recovery an action that cannot return to the menu starts.
constexpr int kNoRecoveryReward = 0;

// The targeting modes recorded as sourceActorId: ground areas (actors or
// body parts), direct targets (actors or body parts), the area around the
// source, and environment actions.
constexpr int kGroundAreaMode = 0;
constexpr int kGroundBodyPartMode = 1;
constexpr int kDirectActorMode = 2;
constexpr int kDirectBodyPartMode = 3;
constexpr int kEnvironmentMode = 6;

// Volume kinds of the action state.
constexpr int kDirectRangeKind = 1;
constexpr int kSourceAreaKind = 1;
constexpr int kGroundRangeKind = 2;
constexpr int kTestVolumeKind = 3;
constexpr int kTestRangeShape = 1;
constexpr int kTestAreaShape = 2;
constexpr int kTestVolumeExtent = 4;
constexpr int kTestTargetActor = 1;
// Angles: volume angles are stored in eighths, the area pitch in sixteenths
// above level.
constexpr int kVolumeAngleScale = 8;
constexpr int kAreaPitchDivisor = 16;
constexpr int kLevelAreaPitch = 64;
constexpr int kHalfTurn = 2048;

// Bones and tags: the source's root bone, an actor's centre and the volume
// origin tag, and the bone-tag kinds of body parts.
constexpr int kSourceRootBone = 0;
constexpr int kActorCentreTag = 255;
constexpr int kVolumeOriginTag = 250;
constexpr int kBodyPartTagKind = 2;
constexpr int kBoneTagKind = 0;

// Actors a collector may target.
constexpr std::uint32_t kTargetableStateMask = 0x3000001;
constexpr std::uint32_t kTargetableState = 1;
constexpr int kUntargetableActorKind = 0x8000;
constexpr std::uint32_t kHiddenDisplayFlag = 4;
// selectedPreview while an area waits for confirmation.
constexpr int kAreaPendingPreview = 240;

// Environment actions and their targets.
constexpr int kTriggerActionFirst = 93;
constexpr int kTriggerActionSecond = 241;
constexpr int kPlatformAction = 79;
constexpr int kInactiveTriggerKind = 16;
constexpr int kTriggerTargetMode = 3;
constexpr int kPlatformTargetMode = 5;
constexpr int kTriggerTargetRadius = 128;
constexpr int kPlatformTargetRadius = 64;
// Position target modes of a confirmed selection.
constexpr int kPositionTargetMode = 1;
constexpr int kFlaggedPositionTargetMode = 4;
constexpr int kFlaggedPositionBit = 0x10;
// Room tiles and the floor height's 15 significant bits.
constexpr int kTileShift = 7;
constexpr int kTileCentre = 64;
constexpr int kFloorHeightShift = 17;

// Treasure chests as targets.
constexpr int kChestCategoryBit = 0x80;
constexpr int kChestOpenedFlagLimit = 64;
constexpr int kChestTargetLimb = 16;
constexpr int kChestActionKind = 10;
constexpr std::uint32_t kFirstLockedLevel = 2;
constexpr std::uint32_t kLockedLevelCount = 2;
constexpr int kChestRiskBase = 150;
constexpr int kPercent = 100;
constexpr int kChestRiskDivisor = 256;

// Pad bits (the original PADxxx values).
constexpr std::uint32_t kPadL2 = 1u << 0;
constexpr std::uint32_t kPadCircle = 1u << 5;
constexpr std::uint32_t kPadCross = 1u << 6;
constexpr std::uint32_t kPadSquare = 1u << 7;
constexpr std::uint32_t kPadUp = 1u << 12;
constexpr std::uint32_t kPadRight = 1u << 13;
constexpr std::uint32_t kPadDown = 1u << 14;
constexpr std::uint32_t kPadLeft = 1u << 15;
constexpr std::uint32_t kPadCrossOrCircle = kPadCross | kPadCircle;

// The targeting sphere.
constexpr int kVolumeKindMask = 0x7F;
constexpr int kActiveVolumeBit = 0x80;
constexpr std::size_t kExtentRadiusByte = 1;
constexpr int kExtentUnit = 32;
constexpr int kPitchStickShift = 3;
constexpr int kPitchStep = 8;
constexpr int kDigitalStickStep = 64;
constexpr int kRotationShift = 10;
constexpr int kRotationRound = 512;
constexpr int kUnitCircle = 4096;
constexpr int kTurnMask = 0xFFF;
constexpr int kAlignedTurnLow = 8;
constexpr int kAlignedTurnHigh = 4087;
constexpr int kFixedShift = 12;
constexpr int kFixedRound = 2048;
constexpr int kDeterminantRound = 4095;
constexpr int kGroundLimit = -1;
constexpr int kGroundClearance = 2;
constexpr int kConeScale = 128;
constexpr int kSphereVolume = 1;
constexpr int kColumnVolume = 2;
constexpr int kBoxVolume = 3;
constexpr int kRisingConeVolume = 4;
constexpr int kFallingConeVolume = 5;
constexpr int kMapTileUnits = 8;
constexpr int kHalfwordMask = 0xFFFF;
constexpr int kHalfwordShift = 16;
constexpr int kGroupShift = 4;
constexpr int kActorGroupMask = 0xF0;
constexpr int kAreaList = 1;
constexpr int kTargetList = 0;

// Highlight modes: clear the old one, highlight the selection, or both.
constexpr int kClearHighlight = 1;
constexpr int kShowHighlight = 2;
constexpr int kMoveHighlight = 3;
constexpr int kNeutralTint = 0x808080;

// The target list: label motion states (dismissal swaps each column's),
// row placement and the selection ring's cycle.
constexpr int kRightLabelState = 2;
constexpr int kLeftLabelState = 3;
constexpr int kLabelRowHeight = 11;
constexpr int kLabelTopRow = 32;
constexpr int kRightLabelX = 320;
constexpr int kLeftLabelX = -72;
constexpr int kByteMask = 0xFF;
constexpr int kRingSizeWrap = 0x22;
constexpr int kRingSizeRestart = 0x12;
constexpr int kLargestGroup = 6;
constexpr int kReusedRangeKind = 2;
// Target list steps: rows reveal one per step below kNoTargetsStep.
constexpr int kNoTargetsStep = 16;
constexpr int kNoTargetsCloseStep = 17;
constexpr int kSettleStep = 18;
constexpr int kChooseStep = 19;
constexpr int kConfirmedStep = 20;
constexpr int kCloseStep = 21;

// Battle_statusStrings indices (Content/StringTables.h).
constexpr int kStrDownText = 57;
constexpr int kBlockerText = 18;
constexpr int kMpTemplateText = 22;
constexpr int kBreakableText = 24;
constexpr int kIndestructibleText = 25;
constexpr int kEmptyText = 40;
constexpr int kPhysicalText = 44;
constexpr int kHumanText = 51;
constexpr int kEmptyStatusOffset = 246;
constexpr int kStatusShift = 5;
constexpr int kStatusTextCount = 24;
constexpr int kBlockerGroupShift = 5;
// The target information's string and integer slots.
constexpr int kNameString = 0;
constexpr int kDetailString = 1;
constexpr int kActionKindString = 2;
constexpr int kDamageTypeString = 3;
constexpr int kEffectVariantString = 4;
constexpr int kEnemyClassString = 5;
constexpr int kStatusString = 6;
constexpr int kAmountInteger = 0;
constexpr int kHitChanceInteger = 1;
constexpr int kObjectPreview = 5;
constexpr int kObjectActorKind = 0x10;

// Range shapes: the shapes whose reach is their z extent, the shape whose
// reach is its y extent with a cosine, and the scale of the extents.
constexpr std::uint32_t kZExtentShapes = 0x16;
constexpr std::uint32_t kYCosineShapes = 0x8;
constexpr int kRangeShapeMask = 7;
constexpr int kRangeAngleShift = 7;
constexpr int kRangeUnitShift = 5;
constexpr std::uint32_t kRangeMargin = 0x20;

// Low 32 bits of a wide product sum (the R3000's MFLO).
[[nodiscard]] std::int32_t Low32(std::int64_t value) {
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(value));
}

[[nodiscard]] std::int16_t Halfword(int value) {
  return static_cast<std::int16_t>(value);
}

[[nodiscard]] TargetText StatusIndexText(int index) {
  return TargetText{.source = TargetTextSource::kStatusIndex, .value = index};
}

[[nodiscard]] bool PreviewSlotAvailable(int count) {
  if (count < kTargetPreviewCapacity) {
    return true;
  }
  VS_LOG_ERROR("battle.targeting",
               "target previews are full ({}); candidate dropped",
               kTargetPreviewCapacity);
  return false;
}

[[nodiscard]] int ClampedPreviewCount(const ActionTargetingHost& host) {
  const int count = host.PreviewCount();
  if (count <= kTargetPreviewCapacity) {
    return count;
  }
  VS_LOG_ERROR("battle.targeting", "preview count {} exceeds capacity {}",
               count, kTargetPreviewCapacity);
  return kTargetPreviewCapacity;
}

// The 15 significant bits of a floor height, sign-extended.
[[nodiscard]] int FloorHeightBits(int height) {
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(height)
                                   << kFloorHeightShift) >>
         kFloorHeightShift;
}

// The centre of a room tile at floor height; pad is kept.
[[nodiscard]] TargetPoint TileCentre(const ActionTargetingHost& host,
                                     int tile_x, int tile_z,
                                     TargetPoint point) {
  const auto tile_origin = [](int tile) {
    return static_cast<int>(static_cast<std::uint32_t>(tile) << kTileShift);
  };
  point.x = Halfword(tile_origin(tile_x) + kTileCentre);
  point.z = Halfword(tile_origin(tile_z) + kTileCentre);
  point.y = Halfword(FloorHeightBits(host.FloorHeightAt(
      tile_origin(tile_x) | kTileCentre, tile_origin(tile_z) | kTileCentre)));
  return point;
}

// Previews the action against one target and fills the preview from it.
[[nodiscard]] ActionTargetPreview PreviewAgainst(ActionTargetingHost& host,
                                                 ActionTargetPreview preview,
                                                 int action, int target_actor,
                                                 int limb) {
  const ActionPreviewResult result = host.PreviewActionAgainstLimb(
      action, host.PrimaryActor(), target_actor, limb);
  PopulateActionTargetPreview(host, preview, result);
  return preview;
}

// The actor scan of the collectors: the action, its source and category,
// and the volume the candidates must fall inside.
struct CandidateScan {
  int action = 0;
  int primary = 0;
  TargetCategoryRules rules;
  TargetingVolumeSlot slot = TargetingVolumeSlot::kRange;
};

[[nodiscard]] bool IsTargetCandidate(const CandidateScan& scan, int slot,
                                     const TargetingActor& actor,
                                     std::uint32_t state_flags) {
  return (slot != scan.primary || scan.rules.includes_acting_actor) &&
         (state_flags & kTargetableStateMask) == kTargetableState &&
         (actor.kind & scan.rules.actor_mask) != 0 &&
         (actor.kind & kUntargetableActorKind) == 0;
}

// The body parts of one actor that fall inside the scanned volume.
int CollectBodyPartTargets(ActionTargetingHost& host, const CandidateScan& scan,
                           int actor, int count) {
  for (int limb = 0; limb < kTargetLimbCount; ++limb) {
    const int part = host.FindBoneTag(actor, limb, kBodyPartTagKind);
    if (part < 0) {
      continue;
    }
    if (!PreviewSlotAvailable(count)) {
      return count;
    }
    ActionTargetPreview preview = host.Preview(count);
    preview.position = host.LowGroupBonePosition(actor, limb, preview.position);
    host.SetPreview(count, preview);
    if (!host.IsPointInVolume(scan.slot, preview.position)) {
      continue;
    }
    preview.kind =
        static_cast<std::uint8_t>(host.BoneTag(actor, part, kBoneTagKind));
    preview.limb = static_cast<std::int8_t>(limb);
    preview.actor_id = static_cast<std::uint8_t>(actor);
    host.SetPreview(count,
                    PreviewAgainst(host, preview, scan.action, actor, limb));
    ++count;
  }
  return count;
}

// One actor as a whole target when its centre falls inside the volume.
int CollectWholeActorTarget(ActionTargetingHost& host,
                            const CandidateScan& scan, int actor,
                            int default_limb, int count) {
  if (!PreviewSlotAvailable(count)) {
    return count;
  }
  ActionTargetPreview preview = host.Preview(count);
  preview.position =
      host.TaggedBonePosition(actor, kActorCentreTag, preview.position);
  host.SetPreview(count, preview);
  if (!host.IsPointInVolume(scan.slot, preview.position)) {
    return count;
  }
  preview.kind = kWholeActorTargetKind;
  preview.limb = kNoTargetLimb;
  preview.actor_id = static_cast<std::uint8_t>(actor);
  host.SetPreview(
      count, PreviewAgainst(host, preview, scan.action, actor, default_limb));
  return count + 1;
}

[[nodiscard]] int CollectActorTargets(ActionTargetingHost& host,
                                      const CandidateScan& scan) {
  int count = 0;
  for (int slot = 0; slot < kTargetableActorSlots; ++slot) {
    const std::uint32_t state_flags = host.ActorStateFlags(slot);
    const std::optional<TargetingActor> actor = host.Actor(slot);
    if (!actor || !IsTargetCandidate(scan, slot, *actor, state_flags)) {
      continue;
    }
    count = scan.rules.targets_body_parts
                ? CollectBodyPartTargets(host, scan, slot, count)
                : CollectWholeActorTarget(host, scan, slot, actor->default_limb,
                                          count);
  }
  return count;
}

// Body parts placed without a volume test (ground areas and the test
// action).
int PlaceBodyPartTargets(ActionTargetingHost& host, int actor, int count) {
  for (int limb = 0; limb < kTargetLimbCount; ++limb) {
    const int part = host.FindBoneTag(actor, limb, kBodyPartTagKind);
    if (part < 0) {
      continue;
    }
    if (!PreviewSlotAvailable(count)) {
      return count;
    }
    ActionTargetPreview preview = host.Preview(count);
    preview.position = host.LowGroupBonePosition(actor, limb, preview.position);
    preview.kind =
        static_cast<std::uint8_t>(host.BoneTag(actor, part, kBoneTagKind));
    preview.limb = static_cast<std::int8_t>(limb);
    preview.actor_id = static_cast<std::uint8_t>(actor);
    host.SetPreview(count, preview);
    ++count;
  }
  return count;
}

int PlaceWholeActorTarget(ActionTargetingHost& host, int actor, int count) {
  if (!PreviewSlotAvailable(count)) {
    return count;
  }
  // decomp: the kind byte keeps its previous value here.
  ActionTargetPreview preview = host.Preview(count);
  preview.position =
      host.TaggedBonePosition(actor, kActorCentreTag, preview.position);
  preview.limb = kNoTargetLimb;
  preview.actor_id = static_cast<std::uint8_t>(actor);
  host.SetPreview(count, preview);
  return count + 1;
}

// Sets the focus behind the selection's source, when it has a placement.
void AimSelectionBehindSource(ActionTargetingHost& host) {
  const std::optional<TargetPoint> focus =
      AimActionBehindSource(host, host.PrimaryActor(), host.FocusPosition());
  if (focus) {
    host.SetFocusPosition(*focus);
  }
}

// One hit target from a preview: an actor and limb, or a position whose pad
// carries the encoded id.
void SetSelectionTargetFromPreview(ActionTargetingHost& host, int index,
                                   const ActionTargetPreview& preview,
                                   int position_mode) {
  if (preview.actor_id >= kFirstPositionTargetId) {
    TargetPoint point = preview.position;
    point.pad = static_cast<std::int16_t>(preview.actor_id);
    host.SetSelectionPositionTarget(index, position_mode, point);
    return;
  }
  // The limb is a signed byte widened; -1 names no limb.
  // NOLINTNEXTLINE(bugprone-signed-char-misuse)
  int limb = preview.limb;
  if (limb == kNoTargetLimb) {
    const std::optional<TargetingActor> actor = host.Actor(preview.actor_id);
    if (actor) {
      limb = actor->default_limb;
    } else {
      VS_LOG_ERROR("battle.targeting",
                   "target actor {} is absent; its default limb is unknown",
                   preview.actor_id);
    }
  }
  host.SetSelectionActorTarget(
      index, ActorLimbTarget{.actor = preview.actor_id, .limb = limb});
}

void CancelAreaSelection(ActionTargetingHost& host) {
  host.ClearSphereTargets();
  host.ClearPrimarySphereVolume();
  host.ClearSecondarySphereVolume();
  host.FreeSphere();
  if (!host.Action(host.SelectionActionId()).returns_to_menu_on_cancel) {
    host.ReleaseMenuRows();
    host.BeginPostActionRecovery(kNoRecoveryReward);
    return;
  }
  ReturnToBattleActionMenu(host);
}

// A preview kept inside the area: body parts keep their record, whole
// actors are rebuilt; none when the actor is gone.
[[nodiscard]] std::optional<ActionTargetPreview> FilteredAreaPreview(
    ActionTargetingHost& host, int action, bool body_parts,
    const ActionTargetPreview& source, ActionTargetPreview kept) {
  if (body_parts) {
    return PreviewAgainst(host, source, action, source.actor_id, source.limb);
  }
  const std::optional<TargetingActor> actor = host.Actor(source.actor_id);
  if (!actor) {
    VS_LOG_ERROR("battle.targeting", "area target actor {} is absent",
                 source.actor_id);
    return std::nullopt;
  }
  kept.kind = kWholeActorTargetKind;
  kept.limb = kNoTargetLimb;
  kept.actor_id = source.actor_id;
  kept.position = source.position;
  return PreviewAgainst(host, kept, action, kept.actor_id, actor->default_limb);
}

// The facing and pitch of a source area aimed from origin at focus.
void AimSourceArea(TargetingVolume& area, TargetPoint origin,
                   TargetPoint focus) {
  const int dx = Halfword(focus.x - origin.x);
  const int dz = Halfword(focus.z - origin.z);
  const int dy = Halfword(focus.y - origin.y);
  area.facing = Arctangent2(dx, dz);
  area.angle =
      Arctangent2(dy, SquareRootInteger(WrappingAdd(dx * dx, dz * dz))) /
          kAreaPitchDivisor +
      kLevelAreaPitch;
}

[[nodiscard]] std::int16_t ChestUnlockChance(const TargetingActor& attacker,
                                             const TargetChest& chest) {
  const std::uint32_t lock =
      static_cast<std::uint32_t>(static_cast<std::uint16_t>(chest.lock_level)) -
      kFirstLockedLevel;
  if (lock < kLockedLevelCount && !chest.is_open) {
    return Halfword((attacker.risk + kChestRiskBase) * kPercent /
                    kChestRiskDivisor);
  }
  return 0;
}

int CollectChestTargets(ActionTargetingHost& host,
                        const TargetingActor& attacker, int count) {
  const std::vector<TargetChest> chests = host.RoomChests();
  for (std::size_t index = 0; index < chests.size(); ++index) {
    if (!PreviewSlotAvailable(count)) {
      return count;
    }
    const TargetChest& chest = chests[index];
    ActionTargetPreview preview = host.Preview(count);
    preview.position =
        TileCentre(host, chest.tile_x, chest.tile_z, preview.position);
    host.SetPreview(count, preview);
    if (chest.opened_flag >= kChestOpenedFlagLimit ||
        !host.IsPointInVolume(TargetingVolumeSlot::kRange, preview.position)) {
      continue;
    }
    preview.kind = kWholeActorTargetKind;
    preview.limb = kChestTargetLimb;
    preview.actor_id = static_cast<std::uint8_t>(index | kChestTargetFlag);
    preview.preview_amount = 0;
    preview.hit_chance = ChestUnlockChance(attacker, chest);
    preview.action_kind = kChestActionKind;
    preview.enemy_class = 0;
    preview.effect_variant = 0;
    preview.damage_type = 0;
    preview.removed_statuses = 0;
    preview.effect = 0;
    host.SetPreview(count, preview);
    ++count;
  }
  return count;
}

// Selects the preview the source's command names: its actor and limb, or
// failing that its actor; the last match wins.
void SelectCommandedPreview(ActionTargetingHost& host,
                            const TargetingActor& source, int count) {
  for (int index = 0; index < count; ++index) {
    const ActionTargetPreview preview = host.Preview(index);
    if (preview.actor_id == source.command_actor &&
        preview.limb == source.command_limb) {
      host.SetSelectedPreview(index);
    }
  }
  if (host.SelectedPreview() != kNoPreviewSelected) {
    return;
  }
  for (int index = 0; index < count; ++index) {
    if (host.Preview(index).actor_id == source.command_actor) {
      host.SetSelectedPreview(index);
    }
  }
}

// The facing an actor's placement gives, or the volume's own when the actor
// has none.
[[nodiscard]] int PlacementFacing(const ActionTargetingHost& host, int actor,
                                  int fallback) {
  const std::optional<TargetPoint> placement = host.ActorPlacement(actor);
  if (placement) {
    return placement->pad;
  }
  VS_LOG_WARN("battle.targeting", "actor {} has no placement; facing kept",
              actor);
  return fallback;
}

[[nodiscard]] TargetingVolume TestVolume(const ActionTargetingHost& host,
                                         TargetingVolumeSlot slot, int shape,
                                         int actor) {
  TargetingVolume volume = host.Volume(slot);
  volume.shape = shape;
  volume.kind = kTestVolumeKind;
  volume.facing = PlacementFacing(host, actor, volume.facing);
  volume.x = kTestVolumeExtent;
  volume.y = kTestVolumeExtent;
  volume.z = kTestVolumeExtent;
  volume.angle = 0;
  volume.origin =
      host.TaggedBonePosition(actor, kVolumeOriginTag, volume.origin);
  return volume;
}

// Action 0 shows fixed volumes around the first two actors and the second
// actor's body parts.
void ShowTestActionTargets(ActionTargetingHost& host) {
  host.SetVolume(
      TargetingVolumeSlot::kRange,
      TestVolume(host, TargetingVolumeSlot::kRange, kTestRangeShape, 0));
  host.ShowRangeVolume(false, 0);
  host.SetVolume(TargetingVolumeSlot::kArea,
                 TestVolume(host, TargetingVolumeSlot::kArea, kTestAreaShape,
                            kTestTargetActor));
  host.ShowAreaVolume();
  const int count = PlaceBodyPartTargets(host, kTestTargetActor, 0);
  host.SetPreviewCount(count);
  host.ShowSphereTargets(count, 0);
  host.SetActionFlowMode(kFlowTargetList);
}

void RevealHiddenActors(ActionTargetingHost& host, int source_slot) {
  for (int slot = 0; slot < kTargetableActorSlots; ++slot) {
    if (slot == source_slot) {
      continue;
    }
    const std::uint32_t flags = host.ActorStateFlags(slot);
    if ((flags & kHiddenDisplayFlag) != 0) {
      host.SetActorDisplayFlags(slot, flags & ~kHiddenDisplayFlag);
    }
  }
}
}  // namespace

// decomp: BATTLE.PRG func_8006F630 (src/BATTLE/BATTLE.PRG/146C.c:3235)
void PopulateActionTargetPreview(const ActionTargetingHost& host,
                                 ActionTargetPreview& preview,
                                 const ActionPreviewResult& result) {
  const TargetingActionView action = host.Action(result.action_id);
  if (action.effect == kHpAmountEffect || action.effect == kHpAmountEffectAlt) {
    preview.preview_amount = static_cast<std::uint16_t>(result.target_hp);
  } else if (action.effect == kMpAmountEffect ||
             action.effect == kMpAmountEffectAlt) {
    preview.preview_amount = static_cast<std::uint16_t>(result.target_mp);
  }
  preview.effect = static_cast<std::uint8_t>(action.effect);
  preview.hit_chance = Halfword(result.hit_threshold);
  preview.removed_statuses = result.remove_statuses & kRemovableStatusMask;
  preview.action_kind = static_cast<std::uint8_t>(action.category);
  preview.damage_type = static_cast<std::uint8_t>(result.damage_type);
  preview.effect_variant = static_cast<std::uint8_t>(result.effect_variant);
  const std::optional<TargetingActor> target =
      host.Actor(result.target_actor & kActorSlotMask);
  if (!target) {
    VS_LOG_ERROR("battle.targeting",
                 "previewed actor {} is absent; enemy class kept",
                 result.target_actor & kActorSlotMask);
    return;
  }
  preview.enemy_class = static_cast<std::uint8_t>(target->enemy_class);
}

// decomp: BATTLE.PRG func_800704D8 (src/BATTLE/BATTLE.PRG/146C.c:3578)
void ReturnToBattleActionMenu(ActionTargetingHost& host) {
  host.SetFieldControlMode(kFieldControlActionMenu);
  host.SetSelectionActionId(0);
  host.SetActionFlowMode(kFlowTargetList);
  host.SetHudBattleMode((host.WeaponDrawn() ? kHudWeaponBit : 0) |
                        kHudActionMenu);
  host.ResumeAfterActionSelection();
  host.PauseFieldForMenu();
}

// decomp: BATTLE.PRG func_8007053C (src/BATTLE/BATTLE.PRG/146C.c:3588)
void FilterAreaActionTargets(ActionTargetingHost& host) {
  const TargetPoint saved = host.AreaCenter();
  host.SetSavedAreaCenter(saved);
  host.ClearPrimarySphereVolume();
  if (saved.pad != 0) {
    CancelAreaSelection(host);
    return;
  }
  const int action = host.SelectionActionId();
  TargetingVolume area = host.Volume(TargetingVolumeSlot::kArea);
  area.origin = saved;
  host.SetVolume(TargetingVolumeSlot::kArea, area);
  if (host.IsPlayerAction()) {
    host.ShowAreaVolume();
  }
  const bool body_parts =
      host.CategoryRules(host.Action(action).category).targets_body_parts;
  const int count = ClampedPreviewCount(host);
  int kept = 0;
  for (int index = 0; index < count; ++index) {
    const ActionTargetPreview source = host.Preview(index);
    if (!host.IsPointInVolume(TargetingVolumeSlot::kArea, source.position)) {
      continue;
    }
    const std::optional<ActionTargetPreview> filtered = FilteredAreaPreview(
        host, action, body_parts, source, host.Preview(kept));
    if (filtered) {
      host.SetPreview(kept, *filtered);
      ++kept;
    }
  }
  host.SetPreviewCount(kept);
  host.ShowSphereTargets(kept, 1);
  host.SetActionFlowMode(kFlowAreaConfirm);
}

// decomp: BATTLE.PRG func_8007087C (src/BATTLE/BATTLE.PRG/146C.c:3650)
std::optional<TargetPoint> AimActionBehindSource(
    const ActionTargetingHost& host, int source_actor, TargetPoint focus) {
  const std::optional<TargetPoint> placement =
      host.ActorPlacement(source_actor);
  if (!placement) {
    VS_LOG_ERROR("battle.targeting",
                 "source actor {} has no placement; focus kept", source_actor);
    return std::nullopt;
  }
  focus.x = Halfword(placement->x + Sine(placement->pad + kHalfTurn));
  focus.z = Halfword(placement->z + Cosine(placement->pad + kHalfTurn));
  focus.y = placement->y;
  return focus;
}

// decomp: BATTLE.PRG func_800708EC (src/BATTLE/BATTLE.PRG/146C.c:3660)
void SetSingleActionTarget(ActionTargetingHost& host) {
  const int selected = host.SelectedPreview();
  if (selected < 0 || selected >= kTargetPreviewCapacity) {
    VS_LOG_ERROR("battle.targeting", "no preview is selected ({})", selected);
    return;
  }
  host.SetSelectionTargetCount(1);
  const ActionTargetPreview preview = host.Preview(selected);
  SetSelectionTargetFromPreview(host, 0, preview,
                                (preview.actor_id & kFlaggedPositionBit) != 0
                                    ? kFlaggedPositionTargetMode
                                    : kPositionTargetMode);
  if (!host.PrimaryTargetsPosition() &&
      preview.actor_id == host.PrimaryActor()) {
    AimSelectionBehindSource(host);
    return;
  }
  host.SetFocusPosition(preview.position);
}

// decomp: BATTLE.PRG func_80070B04 (src/BATTLE/BATTLE.PRG/146C.c:3695)
void SetAllActionTargets(ActionTargetingHost& host) {
  const int count = ClampedPreviewCount(host);
  host.SetSelectionTargetCount(count);
  for (int index = 0; index < count; ++index) {
    SetSelectionTargetFromPreview(host, index, host.Preview(index),
                                  kPositionTargetMode);
  }
  const int primary = host.PrimaryActor();
  if (primary == 0) {
    host.SetFocusPosition(host.SavedAreaCenter());
    return;
  }
  const std::optional<TargetingActor> source = host.Actor(primary);
  if (!source) {
    VS_LOG_ERROR("battle.targeting", "source actor {} is absent; focus kept",
                 primary);
    return;
  }
  host.SetFocusPosition(source->command_point);
}

// decomp: BATTLE.PRG func_80070CAC (src/BATTLE/BATTLE.PRG/146C.c:3728)
void CollectEnvironmentActionTargets(ActionTargetingHost& host) {
  host.SetSelectionTargetCount(0);
  const int action = host.SelectionActionId();
  int count = 0;
  const auto add_target = [&](int mode, TargetPoint point) {
    if (count >= kSelectionTargetCapacity) {
      VS_LOG_ERROR("battle.targeting",
                   "environment targets are full ({}); target dropped",
                   kSelectionTargetCapacity);
      return;
    }
    host.SetSelectionPositionTarget(count, mode, point);
    ++count;
  };
  if (action == kTriggerActionFirst || action == kTriggerActionSecond) {
    for (const EnvironmentTrigger& trigger : host.RoomTriggers()) {
      if (trigger.kind != kInactiveTriggerKind) {
        TargetPoint point{.pad = Halfword(kTriggerTargetRadius)};
        add_target(kTriggerTargetMode,
                   TileCentre(host, trigger.tile_x, trigger.tile_z, point));
      }
    }
  } else if (action == kPlatformAction) {
    for (const PlatformPoint& platform : host.MovingPlatformPoints()) {
      add_target(kPlatformTargetMode,
                 TargetPoint{.x = Halfword(platform.x),
                             .y = Halfword(platform.height),
                             .z = Halfword(platform.z),
                             .pad = Halfword(kPlatformTargetRadius)});
    }
  }
  host.SetSelectionTargetCount(count);
  AimSelectionBehindSource(host);
}

// decomp: BATTLE.PRG func_800719FC (src/BATTLE/BATTLE.PRG/146C.c:4086)
void CollectSourceAreaActionTargets(ActionTargetingHost& host) {
  const int action = host.SelectionActionId();
  const int primary = host.PrimaryActor();
  const TargetingActionView definition = host.Action(action);
  // decomp: the origin's pad is never written; it starts at 0 here.
  const TargetPoint origin =
      host.BonePosition(primary, kSourceRootBone, TargetPoint{});
  TargetingVolume area = host.Volume(TargetingVolumeSlot::kArea);
  area.shape = definition.range.shape;
  area.kind = kSourceAreaKind;
  area.x = definition.range.x;
  area.y = definition.range.y;
  area.z = definition.range.z;
  area.angle = kLevelAreaPitch;
  area.facing = 0;
  if (primary != 0) {
    const std::optional<TargetingActor> source = host.Actor(primary);
    if (source) {
      host.SetFocusPosition(source->command_point);
      AimSourceArea(area, origin, source->command_point);
    } else {
      VS_LOG_ERROR("battle.targeting", "source actor {} is absent; area level",
                   primary);
    }
  }
  area.origin = origin;
  host.SetVolume(TargetingVolumeSlot::kArea, area);
  if (host.IsPlayerAction()) {
    host.ShowAreaVolume();
  }
  const CandidateScan scan{.action = action,
                           .primary = primary,
                           .rules = host.CategoryRules(definition.category),
                           .slot = TargetingVolumeSlot::kArea};
  const int count = CollectActorTargets(host, scan);
  host.SetPreviewCount(count);
  host.ShowSphereTargets(count, 1);
  host.SetSelectedPreview(kAreaPendingPreview);
  host.SetActionFlowMode(kFlowAreaConfirm);
}

// decomp: BATTLE.PRG func_80071F68 (src/BATTLE/BATTLE.PRG/146C.c:4176)
void CollectDirectActionTargets(ActionTargetingHost& host, int source_slot) {
  const int action = host.SelectionActionId();
  const int primary = host.PrimaryActor();
  const std::optional<TargetingActor> attacker = host.Actor(primary);
  const std::optional<TargetPoint> placement = host.ActorPlacement(primary);
  if (!attacker || !placement) {
    VS_LOG_ERROR("battle.targeting",
                 "attacker {} is absent; direct targets not collected",
                 primary);
    return;
  }
  const TargetingActionView definition = host.Action(action);
  const TargetCategoryRules rules = host.CategoryRules(definition.category);
  host.SetSourceActorId(source_slot);
  TargetingVolume range = host.Volume(TargetingVolumeSlot::kRange);
  range.shape = definition.range.shape;
  range.kind = kDirectRangeKind;
  range.facing = placement->pad;
  const RangeShape& reach = definition.range.x == kWeaponRangeSentinel
                                ? attacker->weapon_range
                                : definition.range;
  range.x = reach.x;
  range.y = reach.y;
  range.z = reach.z;
  range.angle = reach.angle * kVolumeAngleScale;
  range.origin =
      host.TaggedBonePosition(primary, kVolumeOriginTag, range.origin);
  host.SetVolume(TargetingVolumeSlot::kRange, range);
  if (host.IsPlayerAction()) {
    host.ShowRangeVolume(false, 0);
  }
  const CandidateScan scan{.action = action,
                           .primary = primary,
                           .rules = rules,
                           .slot = TargetingVolumeSlot::kRange};
  int count = CollectActorTargets(host, scan);
  if ((rules.actor_mask & kChestCategoryBit) != 0) {
    count = CollectChestTargets(host, *attacker, count);
  }
  host.SetPreviewCount(count);
  if (host.IsPlayerAction()) {
    host.ShowSphereTargets(count, 0);
  }
  if (primary != 0) {
    SelectCommandedPreview(host, *attacker, count);
  }
  host.SetActionFlowMode(kFlowTargetList);
}

// decomp: BATTLE.PRG func_80072734 (src/BATTLE/BATTLE.PRG/146C.c:4329)
void CollectGroundAreaActionTargets(ActionTargetingHost& host,
                                    int source_slot) {
  const int action = host.SelectionActionId();
  const int primary = host.PrimaryActor();
  const std::optional<TargetingActor> source = host.Actor(primary);
  const std::optional<TargetPoint> placement = host.ActorPlacement(primary);
  if (!source || !placement) {
    VS_LOG_ERROR("battle.targeting",
                 "source actor {} is absent; ground area not collected",
                 primary);
    return;
  }
  const TargetingActionView definition = host.Action(action);
  const TargetCategoryRules rules = host.CategoryRules(definition.category);
  host.SetSourceActorId(source_slot);
  // decomp: the weapon-range branch tests bits that can never equal 0xFF, so
  // the action's own range always applies.
  TargetingVolume range = host.Volume(TargetingVolumeSlot::kRange);
  range.shape = definition.range.shape;
  range.kind = kGroundRangeKind;
  range.facing = placement->pad;
  range.x = definition.range.x;
  range.y = definition.range.y;
  range.z = definition.range.z;
  range.angle = definition.range.angle * kVolumeAngleScale;
  TargetingVolume area = host.Volume(TargetingVolumeSlot::kArea);
  area.facing = 0;
  area.shape = definition.area.shape;
  area.x = definition.area.x;
  area.y = definition.area.y;
  area.z = definition.area.z;
  area.angle = definition.area.angle * kVolumeAngleScale;
  host.SetVolume(TargetingVolumeSlot::kArea, area);
  const CandidateScan scan{
      .action = action, .primary = primary, .rules = rules};
  int count = 0;
  for (int slot = 0; slot < kGroundAreaActorSlots; ++slot) {
    const std::uint32_t state_flags = host.ActorStateFlags(slot);
    const std::optional<TargetingActor> actor = host.Actor(slot);
    if (!actor || !IsTargetCandidate(scan, slot, *actor, state_flags)) {
      continue;
    }
    count = rules.targets_body_parts ? PlaceBodyPartTargets(host, slot, count)
                                     : PlaceWholeActorTarget(host, slot, count);
  }
  host.SetPreviewCount(count);
  range.origin =
      host.TaggedBonePosition(primary, kVolumeOriginTag, range.origin);
  host.SetVolume(TargetingVolumeSlot::kRange, range);
  if (primary != 0) {
    TargetPoint centre = source->command_point;
    centre.pad = 0;
    host.SetAreaCenter(centre);
  }
  if (host.IsPlayerAction()) {
    host.ShowRangeVolume(true, count);
  }
  host.SetActionFlowMode(kFlowGroundCursor);
}

// decomp: BATTLE.PRG func_80072BA8 (src/BATTLE/BATTLE.PRG/146C.c:4416)
void InitializeActionTargeting(ActionTargetingHost& host, int action_id) {
  host.SetSelectionActionId(action_id);
  host.ClearSphereTargets();
  host.ClearPrimarySphereVolume();
  host.ClearSecondarySphereVolume();
  if (action_id == 0) {
    ShowTestActionTargets(host);
    return;
  }
  const TargetCategoryRules rules =
      host.CategoryRules(host.Action(action_id).category);
  if (rules.needs_target_selection) {
    if (rules.area_target) {
      CollectGroundAreaActionTargets(host, rules.targets_body_parts
                                               ? kGroundBodyPartMode
                                               : kGroundAreaMode);
    } else {
      CollectDirectActionTargets(host, rules.targets_body_parts
                                           ? kDirectBodyPartMode
                                           : kDirectActorMode);
    }
  } else if (rules.area_target) {
    CollectSourceAreaActionTargets(host);
  } else {
    host.CommitEnvironmentAction(kEnvironmentMode);
  }
}

// decomp: BATTLE.PRG func_80072EC4 (src/BATTLE/BATTLE.PRG/146C.c:4489)
void BeginActorActionSelection(ActionTargetingHost& host, int source_slot,
                               int action_id) {
  const std::optional<TargetingActor> source = host.Actor(source_slot);
  if (!source) {
    VS_LOG_ERROR("battle.targeting",
                 "action selection refused: actor {} is absent", source_slot);
    return;
  }
  host.SetFieldControlMode(kFieldControlTargeting);
  host.ResetSelection(source_slot, action_id);
  host.StorePreferredCameraDistance();
  host.ResetRiskPenaltyTimer();
  const bool player = source_slot == 0;
  host.SetIsPlayerAction(player);
  host.SetSelectedPreview(kNoPreviewSelected);
  host.SetCameraPreview(kNoPreviewSelected);
  ActionTargetPreview first = host.Preview(0);
  first.actor_id = static_cast<std::uint8_t>(source->command_target_byte);
  first.limb = static_cast<std::int8_t>(source->command_limb_byte);
  host.SetPreview(0, first);
  host.SetSelectionTargetCount(0);
  host.ResetSelectionTimers();
  host.RestoreIdlePose(source_slot);
  if (player) {
    host.InitSphere();
    // The room palette transform of the targeting view.
    constexpr RoomPaletteTransform kTargetingPalette{
        .mode = 0xD, .duration = 4, .red = -4, .green = -4, .blue = 0};
    host.TransformRoomPalettes(kTargetingPalette);
  }
  host.FreezeActorAnimations();
  RevealHiddenActors(host, source_slot);
  if (player) {
    host.SetMinimapVisible(false);
  }
  host.SetRoomAnimationsPaused(true);
  host.SetMovingPlatformsPaused(true);
  if (player) {
    host.SetActorMarkersHidden(true);
  }
  InitializeActionTargeting(host, host.SelectionActionId());
  host.SetClutAnimationEnabled(false);
}

namespace {
[[nodiscard]] int VolumeRadius(const SphereVolume& volume) {
  return volume.extents[kExtentRadiusByte];
}

// Raises or lowers the cursor (the pitch-locked mode).
void MoveSphereCursorPitch(TargetSelectionHost& host, SphereCursor cursor,
                           const SphereVolume& volume, StickInput stick,
                           std::uint32_t held) {
  const int radius = VolumeRadius(volume);
  int pitch = stick.y >> kPitchStickShift;
  if (pitch == 0) {
    if ((held & kPadUp) != 0) {
      pitch = -kPitchStep;
    }
    if ((held & kPadDown) != 0) {
      pitch += kPitchStep;
    }
  }
  pitch += cursor.y;
  const auto kind = static_cast<std::uint32_t>(volume.kind & kVolumeKindMask);
  const int ceiling = kind - 1 < 2u ? radius * kExtentUnit : 0;
  pitch = std::max(pitch, -radius * kExtentUnit);
  pitch = std::min(pitch, ceiling);
  cursor.y = Halfword(pitch);
  host.SetCursor(cursor);
}

[[nodiscard]] StickInput DigitalStick(std::uint32_t held) {
  StickInput stick;
  if ((held & kPadUp) != 0) {
    stick.y = -kDigitalStickStep;
  }
  if ((held & kPadDown) != 0) {
    stick.y = kDigitalStickStep;
  }
  if ((held & kPadLeft) != 0) {
    stick.x = -kDigitalStickStep;
  }
  if ((held & kPadRight) != 0) {
    stick.x = kDigitalStickStep;
  }
  return stick;
}

// Moves the cursor across the ground in camera space, keeping it inside the
// unit circle unless it would only slide along the rim.
void MoveSphereCursorAcross(TargetSelectionHost& host, SphereCursor cursor,
                            const SphereVolume& volume, StickInput stick,
                            std::uint32_t held) {
  const int angle = -volume.angle - host.CameraYaw();
  if (stick.x == 0 && stick.y == 0) {
    stick = DigitalStick(held);
  }
  const int old_x = cursor.x;
  const int old_z = cursor.z;
  const int cosine = Cosine(angle);
  const int sine = Sine(angle);
  int x =
      Halfword(old_x + ((-stick.x * cosine - stick.y * sine + kRotationRound) >>
                        kRotationShift));
  int z =
      Halfword(old_z + ((-stick.x * sine + stick.y * cosine + kRotationRound) >>
                        kRotationShift));
  const int length = SquareRootInteger(WrappingAdd(x * x, z * z));
  if (length > kUnitCircle) {
    const int step_x = x - old_x;
    const int step_z = z - old_z;
    x = Halfword(x * kUnitCircle / length);
    z = Halfword(z * kUnitCircle / length);
    const int turn =
        (Arctangent2(x, z) - Arctangent2(step_x, step_z)) & kTurnMask;
    if (turn < kAlignedTurnLow || turn > kAlignedTurnHigh) {
      x = old_x;
      z = old_z;
    }
  }
  cursor.x = x;
  cursor.z = z;
  host.SetCursor(cursor);
}

void LockSphereCursor(TargetSelectionHost& host) {
  SphereCursor cursor = host.Cursor();
  cursor.locked = 1;
  host.SetCursor(cursor);
  SphereVolume primary = host.Volume(0);
  primary.kind |= kActiveVolumeBit;
  host.SetVolume(0, primary);
  SphereVolume secondary = host.Volume(1);
  secondary.kind &= kVolumeKindMask;
  host.SetVolume(1, secondary);
}

void UnlockSphereCursor(TargetSelectionHost& host) {
  SphereVolume primary = host.Volume(0);
  primary.kind &= kVolumeKindMask;
  host.SetVolume(0, primary);
  SphereVolume secondary = host.Volume(1);
  secondary.kind |= kActiveVolumeBit;
  host.SetVolume(1, secondary);
  SphereCursor cursor = host.Cursor();
  cursor.locked = 0;
  host.SetCursor(cursor);
  const int count = host.SecondaryTargetCount();
  for (int index = 0; index < count; ++index) {
    const ActionTargetPreview target = host.SphereTarget(kAreaList, index);
    if ((target.actor_id >> kGroupShift) == 0) {
      host.DisablePalettePulse(target.actor_id, target.limb);
    }
  }
}

enum class SphereButton : std::uint8_t { kNone, kCancel, kConfirm };

[[nodiscard]] SphereButton HandleSphereButtons(TargetSelectionHost& host,
                                               bool was_locked) {
  const std::uint32_t pressed = host.ButtonsPressed();
  if (!was_locked) {
    if ((pressed & kPadCross) != 0) {
      host.PlayLeaveSound();
      return SphereButton::kCancel;
    }
    if ((pressed & kPadCircle) != 0) {
      host.PlaySelectSound();
      LockSphereCursor(host);
    }
    return SphereButton::kNone;
  }
  if ((pressed & kPadCross) != 0) {
    host.PlayLeaveSound();
    UnlockSphereCursor(host);
    return SphereButton::kNone;
  }
  if ((pressed & kPadCircle) != 0) {
    host.PlaySelectSound();
    return SphereButton::kConfirm;
  }
  return SphereButton::kNone;
}

// How far the cursor's ground offset reaches at its height.
[[nodiscard]] int SphereCursorScale(int kind, int radius, int pitch) {
  switch (kind) {
    case kSphereVolume: {
      const int ratio = R3000Quotient(pitch * 2, radius);
      return SquareRootFixed(
          WrappingSubtract(kUnitCircle, WrappingMultiply(ratio, ratio)));
    }
    case kColumnVolume:
    case kBoxVolume:
      return kUnitCircle;
    case kRisingConeVolume:
      return R3000Quotient((radius * kExtentUnit + pitch) * kConeScale, radius);
    case kFallingConeVolume:
      return R3000Quotient(-pitch * kConeScale, radius);
    default:
      // decomp: other shapes keep a stale scale; this runs every frame, so
      // the defined 1.0 is logged at debug level.
      VS_LOG_DEBUG("battle.targeting",
                   "sphere volume kind {} has no cursor scale; using 1.0",
                   kind);
      return kUnitCircle;
  }
}

// Puts the target at the cursor's height, raising a cursor below ground to
// just under it (the second pass always lands at -2).
[[nodiscard]] int SettleSphereCursorHeight(TargetSelectionHost& host,
                                           const SphereVolume& volume,
                                           TargetPoint& target) {
  const int kind = volume.kind & kVolumeKindMask;
  const int radius = VolumeRadius(volume);
  SphereCursor cursor = host.Cursor();
  int scale = SphereCursorScale(kind, radius, cursor.y);
  target.x = volume.position.x;
  target.y = Halfword(volume.position.y + cursor.y);
  target.z = volume.position.z;
  if (target.y < kGroundLimit) {
    return scale;
  }
  cursor.y = Halfword(-volume.position.y - kGroundClearance);
  host.SetCursor(cursor);
  scale = SphereCursorScale(kind, radius, cursor.y);
  target.y = Halfword(volume.position.y + cursor.y);
  return scale;
}

struct GroundOffset {
  int x = 0;
  int z = 0;
};

// The cursor's ground offset through the volume transform.
[[nodiscard]] GroundOffset TransformedCursorOffset(const SphereCursor& cursor,
                                                   const SphereBasis& basis,
                                                   int scale) {
  const int scaled_x =
      Low32((static_cast<std::int64_t>(cursor.x) * scale) + kFixedRound) >>
      kFixedShift;
  const int scaled_z =
      Low32((static_cast<std::int64_t>(cursor.z) * scale) + kFixedRound) >>
      kFixedShift;
  const auto combine = [&](int along_x, int along_z) {
    return Low32((static_cast<std::int64_t>(scaled_x) * along_x) +
                 (static_cast<std::int64_t>(scaled_z) * along_z) +
                 kFixedRound) >>
           kFixedShift;
  };
  return GroundOffset{.x = combine(basis.m00, basis.m20),
                      .z = combine(basis.m02, basis.m22)};
}

[[nodiscard]] std::int16_t ClampToMap(int value, int size) {
  if (value < 0) {
    value = 0;
  }
  if (value >= size) {
    value = size - 1;
  }
  return Halfword(value);
}

// Refits the cursor offset to a target clamped onto the map.
void RefitSphereCursor(TargetSelectionHost& host, const SphereVolume& volume,
                       const SphereBasis& basis, int scale,
                       TargetPoint& target) {
  const std::int64_t dx = target.x - volume.position.x;
  const std::int64_t dz = target.z - volume.position.z;
  const int determinant =
      Low32((static_cast<std::int64_t>(basis.m00) * basis.m22) -
            (static_cast<std::int64_t>(basis.m02) * basis.m20) +
            kDeterminantRound) /
      kUnitCircle;
  SphereCursor cursor = host.Cursor();
  if (scale != 0 && determinant != 0) {
    const auto unproject = [&](std::int64_t sum) {
      return Halfword(R3000Quotient(
          WrappingMultiply(R3000Quotient(Low32(sum), determinant), kUnitCircle),
          scale));
    };
    cursor.x = unproject((dx * basis.m22) - (dz * basis.m20));
    cursor.z = unproject((-dx * basis.m02) + (dz * basis.m00));
    host.SetCursor(cursor);
  }
  const GroundOffset offset = TransformedCursorOffset(cursor, basis, scale);
  target.x = Halfword(volume.position.x + offset.x);
  target.z = Halfword(volume.position.z + offset.z);
}

// Moves the target by the cursor's ground offset and keeps it on the map.
void PlaceSphereTarget(TargetSelectionHost& host, const SphereVolume& volume,
                       const SphereBasis& basis, int scale,
                       TargetPoint& target) {
  const GroundOffset offset =
      TransformedCursorOffset(host.Cursor(), basis, scale);
  target.x = Halfword(target.x + offset.x);
  target.z = Halfword(target.z + offset.z);
  const int dimensions = host.CollisionMapDimensions();
  const int width = (dimensions & kHalfwordMask) * kMapTileUnits;
  const int depth = (dimensions >> kHalfwordShift) * kMapTileUnits;
  const auto outside = [](int value, int size) {
    return static_cast<std::uint32_t>(value) >=
           static_cast<std::uint32_t>(size);
  };
  if (!outside(target.x, width) && !outside(target.z, depth)) {
    return;
  }
  target.x = ClampToMap(target.x, width);
  target.z = ClampToMap(target.z, depth);
  RefitSphereCursor(host, volume, basis, scale, target);
}
}  // namespace

// decomp: BATTLE.PRG func_800C4794 (src/BATTLE/BATTLE.PRG/5BF94.c:640)
bool UpdateSphereTargetPosition(TargetSelectionHost& host,
                                TargetPoint& target) {
  const SphereCursor start = host.Cursor();
  const SphereVolume volume = host.Volume(0);
  target.pad = 0;
  if (host.WireShape() != 0) {
    target.x = volume.position.x;
    target.y = volume.position.y;
    target.z = volume.position.z;
    host.CacheTerrainRow(host.WireShape() - 1);
    return false;
  }
  host.BuildCameraBasis();
  const StickInput stick = host.RightStick();
  const std::uint32_t held = host.ButtonsHeld();
  const bool was_locked = start.locked != 0;
  const bool lock_pitch = !was_locked || (held & kPadSquare) != 0;
  if (lock_pitch) {
    MoveSphereCursorPitch(host, start, volume, stick, held);
  } else {
    MoveSphereCursorAcross(host, start, volume, stick, held);
  }
  if (was_locked) {
    host.UpdateTargetHighlights();
  }
  const SphereButton button = HandleSphereButtons(host, was_locked);
  if (button == SphereButton::kCancel) {
    target.pad = 1;
    return true;
  }
  const SphereBasis basis = host.BuildVolumeTransform();
  const int scale = SettleSphereCursorHeight(host, volume, target);
  host.DrawTerrainIntersection(target, scale);
  PlaceSphereTarget(host, volume, basis, scale, target);
  host.DrawPositionCursor(target, lock_pitch);
  SphereVolume secondary = host.Volume(1);
  if ((secondary.kind & kActiveVolumeBit) == 0) {
    secondary.range_kind = 0;
    secondary.position = target;
    host.SetVolume(1, secondary);
  }
  const bool confirmed = button == SphereButton::kConfirm;
  if (confirmed) {
    host.UpdateTargetHighlights();
  }
  return confirmed;
}

// decomp: BATTLE.PRG func_800C513C (src/BATTLE/BATTLE.PRG/5BF94.c:869)
void ClearTargetActorHighlight(TargetSelectionHost& host, int actor,
                               int pulse_slot) {
  if ((actor & kActorGroupMask) == 0) {
    host.DisablePalettePulse(actor, pulse_slot);
  }
}

// decomp: BATTLE.PRG func_800C5164 (src/BATTLE/BATTLE.PRG/5BF94.c:876)
void SetTargetActorHighlight(TargetSelectionHost& host, int actor,
                             int pulse_flags, int pulse_slot) {
  if ((actor & kActorGroupMask) == 0) {
    host.EnablePalettePulse(actor, pulse_flags, pulse_slot);
  }
}

// decomp: BATTLE.PRG func_800C518C (src/BATTLE/BATTLE.PRG/5BF94.c:883)
void SetTargetActorColorOverride(TargetSelectionHost& host, int actor,
                                 bool tinted) {
  if ((actor & kActorGroupMask) == 0) {
    host.SetActorTint(actor, tinted);
  }
}

namespace {
void HighlightAllTargets(TargetSelectionHost& host, int mode) {
  if (mode == kMoveHighlight) {
    return;
  }
  const int count = host.TargetCount();
  for (int index = 0; index < count; ++index) {
    const ActionTargetPreview target = host.SphereTarget(kTargetList, index);
    const int group = target.actor_id;
    if ((group >> kGroupShift) != 0) {
      continue;
    }
    if (mode == kClearHighlight) {
      ClearTargetActorHighlight(host, group, target.limb);
      SetTargetActorColorOverride(host, group, false);
    } else {
      SetTargetActorHighlight(host, group, 0, target.limb);
      SetTargetActorColorOverride(host, group, true);
    }
  }
}
}  // namespace

// decomp: BATTLE.PRG func_800C51B4 (src/BATTLE/BATTLE.PRG/5BF94.c:890)
void UpdateTargetSelectionHighlight(TargetSelectionHost& host, int mode) {
  if (host.AllTargetsMode()) {
    HighlightAllTargets(host, mode);
    return;
  }
  const int selected = host.SelectedTarget();
  if (selected == kNoPreviewSelected) {
    return;
  }
  if ((mode & kClearHighlight) != 0) {
    const HighlightedTarget old = host.Highlighted();
    if ((old.group >> kGroupShift) == 0) {
      ClearTargetActorHighlight(host, old.group, old.pulse_slot);
      SetTargetActorColorOverride(host, old.group, false);
    }
  }
  if ((mode & kShowHighlight) == 0) {
    return;
  }
  const ActionTargetPreview target = host.SphereTarget(kTargetList, selected);
  const HighlightedTarget now{.group = target.actor_id,
                              .pulse_slot = target.limb};
  host.SetHighlighted(now);
  if ((now.group >> kGroupShift) != 0) {
    return;
  }
  SetTargetActorHighlight(host, now.group, 0, now.pulse_slot);
  SetTargetActorColorOverride(host, now.group, true);
}

// decomp: BATTLE.PRG func_800C5360 (src/BATTLE/BATTLE.PRG/5BF94.c:939)
void AppendTargetGroupToColumn(const TargetSelectionHost& host,
                               TargetColumns& columns, int group) {
  TargetColumn& column =
      columns.columns[0].entry_count > columns.columns[1].entry_count
          ? columns.columns[1]
          : columns.columns[0];
  if (column.group_count >= kTargetColumnGroupCapacity) {
    VS_LOG_ERROR("battle.targeting",
                 "target column groups are full; group {} dropped", group);
    return;
  }
  int first = 0;
  const int count = host.TargetCount();
  for (int index = 0; index < count; ++index) {
    if (host.SphereTarget(kTargetList, index).actor_id != group) {
      continue;
    }
    if (first == 0) {
      first = index + 1;
    }
    if (column.entry_count >= kTargetColumnCapacity) {
      VS_LOG_ERROR("battle.targeting",
                   "target column is full; target {} dropped", index);
      continue;
    }
    column.entries[column.entry_count++] = static_cast<std::uint8_t>(index);
  }
  column.group_first_targets[column.group_count] =
      static_cast<std::uint8_t>(first - 1);
  column.groups[column.group_count++] = static_cast<std::uint8_t>(group);
}

// decomp: BATTLE.PRG func_800C553C (src/BATTLE/BATTLE.PRG/5BF94.c:970)
TargetColumns BuildTargetSelectionColumns(const TargetSelectionHost& host) {
  TargetColumns columns = host.Columns();
  for (TargetColumn& column : columns.columns) {
    column.entry_count = 0;
    column.group_count = 0;
  }
  const int count = host.TargetCount();
  const auto group_size = [&](int group) {
    int size = 0;
    for (int index = 0; index < count; ++index) {
      if (host.SphereTarget(kTargetList, index).actor_id == group) {
        ++size;
      }
    }
    return size;
  };
  for (int size = kLargestGroup; size > 0; --size) {
    for (int group = 0; group < kTargetGroupCount; ++group) {
      if (group_size(group) == size) {
        AppendTargetGroupToColumn(host, columns, group);
      }
    }
  }
  for (int index = 0; index < count; ++index) {
    const int group = host.SphereTarget(kTargetList, index).actor_id;
    if ((group >> kGroupShift) != 0) {
      AppendTargetGroupToColumn(host, columns, group);
    }
  }
  const int longest =
      std::max(columns.columns[0].entry_count, columns.columns[1].entry_count);
  columns.top_offset =
      ((kTargetColumnCapacity - longest) * kLabelRowHeight) >> 1;
  columns.top_offset &= kByteMask;
  return columns;
}

// decomp: BATTLE.PRG func_800C56C0 (src/BATTLE/BATTLE.PRG/5BF94.c:1006)
void DismissTargetSelectionRows(TargetSelectionHost& host) {
  UpdateTargetSelectionHighlight(host, kClearHighlight);
  const TargetColumns columns = host.Columns();
  const auto set_state = [&](int index, int state) {
    TargetSelectionLabel label = host.Label(index);
    label.state = state;
    host.SetLabel(index, label);
  };
  for (int row = 0; row < columns.columns[0].entry_count; ++row) {
    set_state(columns.columns[0].entries[row], kLeftLabelState);
  }
  for (int row = 0; row < columns.columns[1].entry_count; ++row) {
    set_state(columns.columns[1].entries[row], kRightLabelState);
  }
}

// decomp: BATTLE.PRG func_800C5798 (src/BATTLE/BATTLE.PRG/5BF94.c:1021)
void InitTargetSelectionRow(TargetSelectionHost& host, int target_index,
                            TargetListRow row) {
  const ActionTargetPreview target =
      host.SphereTarget(kTargetList, target_index);
  const bool from_right = row.state == kRightLabelState;
  TargetSelectionLabel label;
  label.state = row.state;
  label.offset_y = (row.index * kLabelRowHeight + kLabelTopRow) & kByteMask;
  label.world_position = target.position;
  label.screen_x = from_right ? kRightLabelX : kLeftLabelX;
  label.fade = from_right ? 0 : 1;
  host.SetLabel(target_index, label);
  if (target.kind >= kTargetLimbNameCount) {
    VS_LOG_ERROR("battle.targeting", "target kind {} has no limb name",
                 target.kind);
    return;
  }
  host.SetLabelText(target_index,
                    TargetLabelText{.limb_name = target.kind,
                                    .skip_first_character = from_right});
}

// decomp: BATTLE.PRG _getStatusString (src/BATTLE/BATTLE.PRG/5BF94.c:1045)
TargetText FirstTargetStatusText(std::uint32_t status_mask) {
  const std::uint32_t statuses = status_mask >> kStatusShift;
  for (int bit = 0; bit < kStatusTextCount; ++bit) {
    if (((statuses >> bit) & 1u) != 0) {
      return StatusIndexText(kStrDownText + bit);
    }
  }
  return TargetText{.source = TargetTextSource::kStatusOffset,
                    .value = kEmptyStatusOffset};
}

namespace {
template <std::size_t N>
[[nodiscard]] std::optional<int> FindEntry(
    const std::array<std::uint8_t, N>& items, int count, int value) {
  const int limit = std::min(count, static_cast<int>(N));
  for (int index = 0; index < limit; ++index) {
    if (items[static_cast<std::size_t>(index)] == value) {
      return index;
    }
  }
  return std::nullopt;
}

// Steps through the target groups: to the next group of the same column,
// then on to the other column's first.
[[nodiscard]] int CycleTargetGroups(const TargetColumns& columns, int value,
                                    int group) {
  const TargetColumn& first = columns.columns[0];
  const TargetColumn& second = columns.columns[1];
  if (const std::optional<int> found =
          FindEntry(first.groups, first.group_count, group)) {
    const int next = *found + 1;
    if (next != first.group_count) {
      return first.group_first_targets[static_cast<std::size_t>(next)];
    }
    return second.group_count == 0 ? value : second.entries[0];
  }
  const std::optional<int> found =
      FindEntry(second.groups, second.group_count, group);
  if (!found) {
    VS_LOG_WARN("battle.targeting", "target group {} is in no column", group);
    return value;
  }
  const int next = *found + 1;
  return next == second.group_count
             ? first.entries[0]
             : second.group_first_targets[static_cast<std::size_t>(next)];
}

[[nodiscard]] int MoveInFirstColumn(const TargetColumns& columns, int row,
                                    std::uint32_t pressed,
                                    std::uint32_t repeated) {
  const TargetColumn& first = columns.columns[0];
  const TargetColumn& second = columns.columns[1];
  if ((pressed & kPadLeft) != 0 && second.entry_count != 0) {
    return second.entries[static_cast<std::size_t>(
        std::min(row, second.entry_count - 1))];
  }
  if ((repeated & kPadUp) != 0) {
    if (row != 0) {
      --row;
    } else if (second.entry_count != 0) {
      return second.entries[static_cast<std::size_t>(second.entry_count - 1)];
    } else {
      row = first.entry_count - 1;
    }
  }
  if ((repeated & kPadDown) != 0) {
    if (row != first.entry_count - 1) {
      ++row;
    } else {
      row = 0;
      if (second.entry_count != 0) {
        return second.entries[0];
      }
    }
  }
  return first.entries[static_cast<std::size_t>(row)];
}

[[nodiscard]] int MoveInSecondColumn(const TargetColumns& columns, int row,
                                     std::uint32_t pressed,
                                     std::uint32_t repeated) {
  const TargetColumn& first = columns.columns[0];
  const TargetColumn& second = columns.columns[1];
  if ((pressed & kPadRight) != 0) {
    return first.entries[static_cast<std::size_t>(
        std::min(row, first.entry_count - 1))];
  }
  if ((repeated & kPadUp) != 0) {
    if (row == 0) {
      return first.entries[static_cast<std::size_t>(first.entry_count - 1)];
    }
    --row;
  }
  if ((repeated & kPadDown) != 0) {
    if (row == second.entry_count - 1) {
      return first.entries[0];
    }
    ++row;
  }
  return second.entries[static_cast<std::size_t>(row)];
}

// The target the pad moves the selection to.
[[nodiscard]] int NavigateTargets(const TargetSelectionHost& host, int value,
                                  std::uint32_t pressed,
                                  std::uint32_t repeated) {
  const TargetColumns columns = host.Columns();
  if ((repeated & kPadL2) != 0) {
    return CycleTargetGroups(columns, value,
                             host.SphereTarget(kTargetList, value).actor_id);
  }
  const TargetColumn& first = columns.columns[0];
  const TargetColumn& second = columns.columns[1];
  if (const std::optional<int> row =
          FindEntry(first.entries, first.entry_count, value)) {
    return MoveInFirstColumn(columns, *row, pressed, repeated);
  }
  const std::optional<int> row =
      FindEntry(second.entries, second.entry_count, value);
  if (!row || first.entry_count == 0) {
    VS_LOG_WARN("battle.targeting", "target {} is in no column", value);
    return value;
  }
  return MoveInSecondColumn(columns, *row, pressed, repeated);
}

[[nodiscard]] TargetText TargetNameText(int group, int kind) {
  if ((group >> kGroupShift) != 0) {
    return StatusIndexText((group >> kBlockerGroupShift) + kBlockerText);
  }
  if (kind == kWholeActorTargetKind) {
    return TargetText{.source = TargetTextSource::kActorName, .value = group};
  }
  return StatusIndexText(kind);
}

// The amount line a target shows: none, one of the four HP/MP effects (odd
// for HP), or an object's durability.
constexpr int kNoAmountPreview = 0;
constexpr int kHpAmountPreview = 1;
constexpr int kMpAmountPreview = 2;
constexpr int kHpAmountPreviewAlt = 3;
constexpr int kMpAmountPreviewAlt = 4;

[[nodiscard]] int AmountPreviewKind(int effect) {
  switch (effect) {
    case kHpAmountEffect:
      return kHpAmountPreview;
    case kMpAmountEffect:
      return kMpAmountPreview;
    case kHpAmountEffectAlt:
      return kHpAmountPreviewAlt;
    case kMpAmountEffectAlt:
      return kMpAmountPreviewAlt;
    default:
      return kNoAmountPreview;
  }
}

[[nodiscard]] bool IsObjectActor(const TargetSelectionHost& host, int actor) {
  const std::optional<int> kind = host.ActorKind(actor);
  if (!kind) {
    VS_LOG_ERROR("battle.targeting", "target actor {} is absent", actor);
    return false;
  }
  return (*kind & kObjectActorKind) != 0;
}

// Fills the target information box for one target.
void DescribeTarget(TargetSelectionHost& host, int selected) {
  const ActionTargetPreview entry = host.SphereTarget(kTargetList, selected);
  const int group = entry.actor_id;
  const int kind = entry.kind;
  host.SetContextText(kNameString, TargetNameText(group, kind));
  for (int slot = kDetailString; slot < kStatusString; ++slot) {
    host.SetContextText(slot, TargetText{});
  }
  int amount = AmountPreviewKind(entry.effect);
  if (kind == kWholeActorTargetKind && (group >> kGroupShift) == 0 &&
      IsObjectActor(host, group)) {
    amount = kObjectPreview;
    host.SetContextText(
        kDetailString,
        StatusIndexText(static_cast<std::uint16_t>(entry.hit_chance) == 0
                            ? kIndestructibleText
                            : kBreakableText));
  } else if (amount == kNoAmountPreview) {
    host.SetContextText(
        kDetailString,
        TargetText{.source = TargetTextSource::kStatusLineTemplate});
    host.SetContextText(kStatusString,
                        FirstTargetStatusText(entry.removed_statuses));
  }
  host.SetContextText(kActionKindString,
                      StatusIndexText(entry.action_kind + kIndestructibleText));
  if (amount != kNoAmountPreview) {
    if (amount < kObjectPreview) {
      host.SetContextText(kDetailString,
                          StatusIndexText(kMpTemplateText - (amount & 1)));
      host.SetContextText(kEnemyClassString,
                          StatusIndexText(entry.enemy_class + kHumanText));
    }
    host.SetContextText(kDamageTypeString,
                        StatusIndexText(entry.damage_type + kEmptyText));
    host.SetContextText(kEffectVariantString,
                        StatusIndexText(entry.effect_variant + kPhysicalText));
  }
  host.SetContextInteger(kAmountInteger, entry.preview_amount);
  host.SetContextInteger(kHitChanceInteger,
                         static_cast<std::uint16_t>(entry.hit_chance));
  host.ShowTargetInfo();
}

void StartTargetSelectionMenu(TargetSelectionHost& host) {
  host.SetHighlightTint(kNeutralTint);
  host.InitInformationBox();
  UpdateTargetSelectionHighlight(host, kShowHighlight);
  host.SetColumns(BuildTargetSelectionColumns(host));
  if (host.TargetCount() == 0) {
    host.SetSelectionStep(kNoTargetsStep);
  }
}

// Shows one column's row; a row alternates its fade from the row above when
// its group changes.
void RevealTargetRow(TargetSelectionHost& host, const TargetColumn& column,
                     int row, int state) {
  const int index = column.entries[static_cast<std::size_t>(row)];
  InitTargetSelectionRow(host, index,
                         TargetListRow{.state = state, .index = row});
  if (row == 0) {
    return;
  }
  const int previous = column.entries[static_cast<std::size_t>(row - 1)];
  const int previous_fade = host.Label(previous).fade;
  TargetSelectionLabel label = host.Label(index);
  label.fade = host.SphereTarget(kTargetList, index).actor_id !=
                       host.SphereTarget(kTargetList, previous).actor_id
                   ? 1 - previous_fade
                   : previous_fade;
  host.SetLabel(index, label);
}

void RevealTargetRows(TargetSelectionHost& host, int step) {
  const TargetColumns columns = host.Columns();
  const TargetColumn& first = columns.columns[0];
  const TargetColumn& second = columns.columns[1];
  if (step >= first.entry_count && step >= second.entry_count) {
    host.SetSelectionStep(kSettleStep);
    return;
  }
  if (step < first.entry_count) {
    RevealTargetRow(host, first, step, kRightLabelState);
  }
  if (step < second.entry_count) {
    RevealTargetRow(host, second, step, kLeftLabelState);
  }
  host.SetSelectionStep(step + 1);
}

void CloseTargetList(TargetSelectionHost& host) {
  host.SetReuseSphereVolumes(host.Volume(0).range_kind == kReusedRangeKind);
  host.SetSelectionStep(kCloseStep);
}

// The selected label pulses its ring and drops back while the pad is read.
void PulseSelectedLabel(TargetSelectionHost& host, int selected) {
  TargetSelectionLabel label = host.Label(selected);
  label.ring_size = (label.ring_size + 1) & kByteMask;
  if (label.ring_size == kRingSizeWrap) {
    label.ring_size = kRingSizeRestart;
  }
  label.depth = 0;
  host.SetLabel(selected, label);
}

void MarkSelectedLabel(TargetSelectionHost& host, int previous, int selected) {
  TargetSelectionLabel label = host.Label(selected);
  label.depth = 1;
  if (previous != selected) {
    label.ring_size = 0;
  }
  host.SetLabel(selected, label);
  if (previous != selected) {
    host.PlayChangeSound();
    UpdateTargetSelectionHighlight(host, kMoveHighlight);
  }
}

void ChooseTarget(TargetSelectionHost& host, int& target) {
  if (host.HandlePageButtons()) {
    target = kNoPreviewSelected;
    return;
  }
  const int previous = host.SelectedTarget();
  PulseSelectedLabel(host, previous);
  const std::uint32_t pressed = host.ButtonsPressed();
  if ((pressed & kPadCross) != 0) {
    host.PlayLeaveSound();
    DismissTargetSelectionRows(host);
    CloseTargetList(host);
    return;
  }
  target = host.MapPageIndex(previous);
  if ((pressed & kPadCircle) != 0) {
    DismissTargetSelectionRows(host);
    host.SetSelectionStep(kConfirmedStep);
    return;
  }
  const int selected =
      NavigateTargets(host, previous, pressed, host.ButtonsRepeated());
  host.SetSelectedTarget(selected);
  MarkSelectedLabel(host, previous, selected);
  DescribeTarget(host, selected);
}
}  // namespace

// decomp: BATTLE.PRG func_800C58F8 (src/BATTLE/BATTLE.PRG/5BF94.c:1058)
bool UpdateTargetSelectionMenu(TargetSelectionHost& host, int& target) {
  if (host.SelectionStep() == 0) {
    StartTargetSelectionMenu(host);
  }
  target = kNoPreviewSelected;
  const int step = host.SelectionStep();
  if (step < kNoTargetsStep) {
    RevealTargetRows(host, step);
    return false;
  }
  switch (step) {
    case kNoTargetsStep:
      host.ShowNoTargetsMessage();
      if ((host.ButtonsPressed() & kPadCrossOrCircle) != 0) {
        host.PlayLeaveSound();
        host.SetSelectionStep(kNoTargetsCloseStep);
      }
      return false;
    case kNoTargetsCloseStep:
      CloseTargetList(host);
      return false;
    case kSettleStep:
      if (host.LabelsSettled()) {
        host.SetSelectionStep(kChooseStep);
      }
      return false;
    case kChooseStep:
      ChooseTarget(host, target);
      return false;
    case kConfirmedStep:
      target = host.MapPageIndex(host.SelectedTarget());
      [[fallthrough]];
    case kCloseStep:
      host.DismissInformationBox();
      host.ClearPrimarySphereVolume();
      return true;
    default:
      return false;
  }
}

// decomp: BATTLE.PRG func_800E5568 (src/BATTLE/BATTLE.PRG/7CD68.c:437)
int CalculateRangeShapeExtent(const RangeShape& shape) {
  const std::uint32_t mask = 1u << (shape.shape & kRangeShapeMask);
  int reach = shape.y;
  bool cosine = true;
  if ((mask & kZExtentShapes) != 0) {
    reach = shape.z;
  } else if ((mask & kYCosineShapes) == 0) {
    cosine = false;
  }
  const int angle = shape.angle << kRangeAngleShift;
  const int value = cosine ? Cosine(angle) : Sine(angle);
  return ((reach << kRangeUnitShift) * std::abs(value)) >> kFixedShift;
}

// decomp: BATTLE.PRG func_800E5600 (src/BATTLE/BATTLE.PRG/7CD68.c:459)
int CalculateActionAreaReach(const TargetingActionView& action,
                             std::uint32_t maximum_reach) {
  if (action.radial_area) {
    return action.area.y << kRangeUnitShift;
  }
  if (!action.shaped_area) {
    return 0;
  }
  const int reach = CalculateRangeShapeExtent(action.area);
  return static_cast<std::uint32_t>(reach) < maximum_reach ? reach : 0;
}

// decomp: BATTLE.PRG func_800E5698 (src/BATTLE/BATTLE.PRG/7CD68.c:476)
int CalculateActionRangeSquared(const TargetingActionView& action,
                                const RangeShape& weapon_range) {
  const RangeShape& range =
      action.range.x == kWeaponRangeSentinel ? weapon_range : action.range;
  auto reach = static_cast<std::uint32_t>(CalculateRangeShapeExtent(range));
  if (reach > kRangeMargin) {
    reach -= kRangeMargin;
  }
  return static_cast<int>(reach * reach);
}
}  // namespace VS::Battle
