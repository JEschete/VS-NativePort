#include "Field/ChestInteractions.hpp"

#include <bit>

namespace VS::Field {
namespace {

constexpr std::int32_t kItemUnlockFlagBase = 0x1B6;
constexpr std::int16_t kConsumedSigilFirst = 0x1D2;
constexpr std::uint8_t kOpeningPose = 2;
constexpr std::uint8_t kRefusedPose = 3;
constexpr std::int16_t kKeyLock = 1;
constexpr std::int16_t kBreakableLock = 2;
constexpr std::int16_t kSigilLock = 3;
constexpr std::uint8_t kClosingPose = 4;
constexpr std::int32_t kStorageFlagFirst = 0x40;
constexpr std::int32_t kOpenedFlagBase = 0x3C0;
constexpr std::int32_t kIdleActorCount = 16;
constexpr std::uint8_t kSpecialRoomLock = 3;
constexpr std::uint8_t kRoomObjectPose = 5;
constexpr std::int32_t kRoomKeyUnlocked = 7;
constexpr std::int32_t kRoomSigilUnlocked = 9;
constexpr std::int32_t kRoomKeyLocked = 6;
constexpr std::int32_t kRoomSigilLocked = 8;
constexpr std::int32_t kRoomMagicLocked = 5;
constexpr std::int32_t kRoomSpecialLocked = 10;
constexpr std::int32_t kRoomRejectedSoundFirst = 0x65;
constexpr std::int32_t kRoomRejectedSoundSecond = 0x66;
constexpr std::int16_t kRoomSigilUnlockFlagValue = 2;
constexpr std::int32_t kTrapActorSlots = 17;
constexpr std::int16_t kTrapKindLimit = 0x10;
constexpr std::uint16_t kTrapActionBase = 6;
constexpr std::uint32_t kInteractionAnimationReady = 0x08000000;
constexpr std::int32_t kInteractionCameraSpeed = 0x28;
constexpr std::int32_t kInteractionCameraReturnOffset = 0x5A;
constexpr std::uint16_t kFacingRoundBias = 0x200;
constexpr std::uint16_t kFacingHeadingMask = 0xFFF;
constexpr unsigned kFacingQuadrantShift = 10;
constexpr std::uint32_t kFacingFloorMask = 0x7FFF;
constexpr std::uint32_t kFacingFloorSign = 0x4000;
constexpr std::int32_t kFacingRoomHeightBias = 0x3F;
constexpr std::int32_t kFacingRoomHeightWindow = 0x7F;
constexpr std::int32_t kFacingChestHeightOffset = 0x60;
constexpr std::uint32_t kTriggerLayerMask = 0x70000;
constexpr unsigned kTriggerLayerShift = 16;
constexpr unsigned kTriggerModeShift = 14;
constexpr std::uint32_t kActorTriggerDisabled = 0x01000000;
constexpr std::uint32_t kActorTrapSuppressed = 0x16704000;
constexpr std::uint32_t kTriggerLocationHeld = 0x504000;
constexpr std::int16_t kEffectOnlyTrap = 0x10;
constexpr std::int16_t kFatalPitHeight = 0x11;

std::int32_t FacingFloorHeight(std::int32_t height) {
  const auto low = static_cast<std::uint32_t>(height) & kFacingFloorMask;
  return static_cast<std::int32_t>(low ^ kFacingFloorSign) -
         static_cast<std::int32_t>(kFacingFloorSign);
}

std::int32_t RoomObjectLockedNotification(const RoomObjectLockState& state) {
  if (state.lock_state == kKeyLock) {
    return state.required_item >= kConsumedSigilFirst ? kRoomSigilLocked
                                                      : kRoomKeyLocked;
  }
  return state.lock_state == kBreakableLock ? kRoomMagicLocked
                                            : kRoomSpecialLocked;
}

}  // namespace

// decomp: BATTLE.PRG func_8008A4FC (146C.c:15530), original53MIPS.
std::uint8_t CurrentSavePointId(SavePointQueryHost& host) {
  constexpr std::int16_t kSavePointKind = 16;
  const auto placement = host.PlayerPlacement();
  if (!placement) return 0;
  const auto flags = host.PlayerStateFlags();
  auto probe = placement->tile;
  if ((flags & kTriggerLayerMask) != 0) {
    probe.x = 0;
    probe.z = 0;
    probe.layer = static_cast<std::uint8_t>((flags & kTriggerLayerMask) >>
                                            kTriggerLayerShift);
  }
  if (!host.HasRoomTrigger(probe) || (flags & kTriggerLocationHeld) != 0) {
    return 0;
  }
  const auto trigger = host.Trigger(placement->tile);
  return trigger && trigger->kind == kSavePointKind ? trigger->activation : 0;
}

// decomp: BATTLE.PRG func_8009E634 (30DB0.c:2695), original51MIPS.
bool RefreshAshleyInteractionTile(const AshleyInteractionTileState& state,
                                  AshleyInteractionTileHost& host) {
  constexpr std::uint32_t kFirstShortReachAnimation = 0x47;
  constexpr std::uint32_t kSingleReachAnimation = 0x4C;
  constexpr std::uint32_t kFirstLongReachAnimation = 0x4F;
  constexpr std::uint32_t kAnimationRange = 4;
  constexpr std::int32_t kLongReach = 2;
  constexpr std::int32_t kNegativeQuadrantCount = 2;
  if (state.actor_slot != 0) return true;
  const auto animation = static_cast<std::uint32_t>(state.animation_id);
  std::int32_t reach = 1;
  if (animation - kFirstLongReachAnimation < kAnimationRange) {
    reach = kLongReach;
  } else if (animation - kFirstShortReachAnimation >= kAnimationRange &&
             animation != kSingleReachAnimation) {
    return true;
  }
  const auto quadrant =
      ((static_cast<std::int32_t>(state.facing) + kFacingRoundBias) &
       kFacingHeadingMask) >>
      kFacingQuadrantShift;
  if (quadrant < kNegativeQuadrantCount) reach = -reach;
  const bool along_x = (quadrant & 1) != 0;
  return host.ReleaseTile(
      static_cast<std::int32_t>(state.tile_x) + (along_x ? reach : 0),
      static_cast<std::int32_t>(state.tile_z) + (along_x ? 0 : reach));
}

// decomp: scripted-fall door in BATTLE.PRG func_80074BAC (146C.c:5365).
DoorRecord BuildScriptedFallDoor(const ScriptedFallDestination& destination,
                                 const Actors::PackedTile& tile) {
  constexpr unsigned kFallElevationShift = 6;
  constexpr std::uint8_t kFallEntryDirection = 7;
  return {.destination_zone_id = destination.zone_id,
          .destination_room_id = destination.room_id,
          .tile_x = static_cast<std::uint8_t>(tile.x & kDoorTileMask),
          .tile_z = static_cast<std::uint8_t>(tile.z & kDoorTileMask),
          .floor_layer =
              static_cast<std::uint8_t>(tile.layer & kDoorFloorLayerMask),
          .entry_direction = kFallEntryDirection,
          .entry_tile_x = static_cast<std::uint8_t>(tile.x & kDoorTileMask),
          .entry_tile_z = static_cast<std::uint8_t>(tile.z & kDoorTileMask),
          .entry_elevation =
              static_cast<std::uint8_t>(tile.spare >> kFallElevationShift)};
}

// decomp: fall tail of BATTLE.PRG func_80074BAC (146C.c:5365), 800750CC-75184.
AshleyFallResult CheckAshleyFall(std::int32_t actor,
                                 const Actors::ActorPlacementView& placement,
                                 bool pit, AshleyFallHost& host) {
  constexpr std::int16_t kPitHeightThreshold = 16;
  constexpr std::int16_t kDefeatHpThreshold = 21;
  constexpr std::int16_t kFallDamage = 20;
  if (actor < 0 || actor >= kTrapActorSlots || !pit ||
      placement.position.y <= kPitHeightThreshold) {
    return AshleyFallResult::kNone;
  }
  if (const auto destination = host.ScriptedDestination()) {
    const auto door = BuildScriptedFallDoor(*destination, placement.tile);
    host.BeginTransition(door, door.destination_zone_id == host.CurrentZoneId()
                                   ? AshleyTransitionRoute::kRoom
                                   : AshleyTransitionRoute::kZone);
    host.ResetRunMomentum(actor);
    host.SetEntrance(std::nullopt);
    return AshleyFallResult::kScriptedTransition;
  }
  const auto hp = host.CurrentHp(actor);
  if (!hp) {
    return AshleyFallResult::kNone;
  }
  if (*hp < kDefeatHpThreshold) {
    if (*hp != 0) {
      host.ShowFallDamage(actor);
    }
    host.SetHp(actor, 0);
    host.BeginDefeat(actor);
    return AshleyFallResult::kHandled;
  }
  host.SetHp(actor, static_cast<std::int16_t>(*hp - kFallDamage));
  const auto door = host.StoreEntranceForReload();
  host.BeginTransition(door, AshleyTransitionRoute::kRoom);
  host.ShowFallDamage(actor);
  host.ResetRunMomentum(actor);
  host.SetEntrance(door);
  return AshleyFallResult::kHandled;
}

namespace {
constexpr std::int16_t kAshleyEffectOnlyTrapKind = 0x10;

bool TryAshleyTileEvent(std::int32_t actor,
                        const Actors::ActorPlacementView& placement,
                        const AshleyTileTriggerCell& cell, std::uint8_t mode,
                        AshleyTileTriggerHost& host) {
  if (!cell.event_trigger || !host.TriggerPlayerTileEvent()) {
    return false;
  }
  host.BeginQueuedCutscene();
  host.StoreTriggerTile(actor, placement.tile, mode, true);
  if (cell.actor.room_trigger) {
    const auto trap = host.Trap(placement.tile);
    if (trap && trap->kind == kAshleyEffectOnlyTrapKind) {
      host.SpawnTrapTileEffect(placement);
    }
  }
  host.FallHost().ResetRunMomentum(actor);
  return true;
}

bool TryAshleyTileTrap(std::int32_t actor,
                       const Actors::ActorPlacementView& placement,
                       const AshleyTileTriggerCell& cell, std::uint32_t flags,
                       std::uint8_t mode, AshleyTileTriggerHost& host) {
  constexpr std::uint32_t kTrapSuppressionMask = 0x504000;
  if (!cell.actor.room_trigger || (flags & kTrapSuppressionMask) != 0) {
    return false;
  }
  const auto trap = host.Trap(placement.tile);
  if (!trap) {
    return false;
  }
  if (trap->kind == kAshleyEffectOnlyTrapKind) {
    host.SpawnTrapTileEffect(placement);
    return false;
  }
  host.BeginTrap(trap->target, placement, actor);
  host.StoreTriggerTile(actor, placement.tile, mode, true);
  return true;
}

bool TryAshleyTileDoor(std::int32_t actor, const Actors::PackedTile& lookup,
                       const AshleyTileTriggerCell& cell, std::uint32_t flags,
                       bool forced_layer, AshleyTileTriggerHost& host) {
  constexpr std::uint32_t kDoorSuppressionFlag = 0x4000;
  constexpr std::uint32_t kForcedDoorSuppressionFlag = 0x20000000;
  if (!cell.docked_door || (flags & kDoorSuppressionFlag) != 0 ||
      (forced_layer && (flags & kForcedDoorSuppressionFlag) != 0)) {
    return false;
  }
  for (const auto& door : host.DoorsAt(lookup)) {
    if (host.ScriptFlag(door.required_flag_id) >= door.required_flag_value) {
      auto& fall = host.FallHost();
      fall.BeginTransition(door,
                           door.destination_zone_id == fall.CurrentZoneId()
                               ? AshleyTransitionRoute::kRoom
                               : AshleyTransitionRoute::kZone);
      fall.ResetRunMomentum(actor);
      fall.SetEntrance(door);
      return true;
    }
  }
  return false;
}
}  // namespace

// decomp: BATTLE.PRG func_80074BAC (src/BATTLE/BATTLE.PRG/146C.c:5365).
bool CheckAshleyTileTriggers(std::int32_t actor,
                             const Actors::ActorPlacementView& placement,
                             AshleyTileTriggerHost& host) {
  constexpr std::uint32_t kHeldLocationMask = 0x104000;
  if (actor < 0 || actor >= kTrapActorSlots) {
    return false;
  }
  const auto flags = host.StateFlags(actor);
  const auto layer = static_cast<std::uint8_t>((flags & kTriggerLayerMask) >>
                                               kTriggerLayerShift);
  const auto lookup =
      layer != 0 ? Actors::PackedTile{.layer = layer} : placement.tile;
  const auto cell = host.Collision(lookup);
  if (host.TransitionInProgress() || (flags & kActorTriggerDisabled) != 0) {
    return false;
  }
  const auto last = host.LastTriggerTile(actor);
  if (!last) {
    return false;
  }
  const auto mode = static_cast<std::uint8_t>((flags >> kTriggerModeShift) & 1);
  const auto& tile = placement.tile;
  const bool changed = tile.x != last->x || tile.layer != last->layer ||
                       tile.z != last->z || mode != last->spare || layer != 0;
  if (changed) {
    if (!host.EventOwnsControl()) {
      if (TryAshleyTileEvent(actor, placement, cell, mode, host)) {
        return true;
      }
      if (TryAshleyTileTrap(actor, placement, cell, flags, mode, host)) {
        return true;
      }
      if (TryAshleyTileDoor(actor, lookup, cell, flags, layer != 0, host)) {
        return true;
      }
    }
    host.StoreTriggerTile(actor, tile, mode, (flags & kHeldLocationMask) == 0);
  }
  return CheckAshleyFall(actor, placement, cell.actor.pit, host.FallHost()) ==
         AshleyFallResult::kHandled;
}

// decomp: BATTLE.PRG func_800751B8 (src/BATTLE/BATTLE.PRG/146C.c:5514).
ActorTileTriggerResult CheckActorTrapAndFallTriggers(
    std::int32_t actor, const Actors::ActorPlacementView& placement,
    ActorTileTriggerHost& host) {
  if (actor < 0 || actor >= kTrapActorSlots) {
    return ActorTileTriggerResult::kNone;
  }
  const auto flags = host.StateFlags(actor);
  const auto layer_flags = flags & kTriggerLayerMask;
  const auto mode = static_cast<std::uint8_t>((flags >> kTriggerModeShift) & 1);
  const auto collision_tile =
      layer_flags != 0
          ? Actors::PackedTile{.layer = static_cast<std::uint8_t>(
                                   layer_flags >> kTriggerLayerShift)}
          : placement.tile;
  const auto cell = host.Collision(collision_tile);
  if (host.TransitionInProgress() || (flags & kActorTriggerDisabled) != 0) {
    return ActorTileTriggerResult::kNone;
  }
  const auto last = host.LastTriggerTile(actor);
  if (!last) {
    return ActorTileTriggerResult::kNone;
  }
  if (last->x != placement.tile.x || last->z != placement.tile.z ||
      last->layer != placement.tile.layer || last->spare != mode ||
      layer_flags != 0) {
    if (!host.EventOwnsControl() && cell.room_trigger &&
        (flags & kActorTrapSuppressed) == 0) {
      const auto trap = host.Trap(placement.tile);
      if (trap && trap->kind != kEffectOnlyTrap) {
        host.BeginTrap(trap->target, placement, actor);
        host.StoreTriggerTile(actor, placement.tile, mode, true);
        return ActorTileTriggerResult::kTrap;
      }
    }
    host.StoreTriggerTile(actor, placement.tile, mode,
                          (flags & kTriggerLocationHeld) == 0);
  }
  if (cell.pit && placement.position.y >= kFatalPitHeight &&
      host.DefeatForFall(actor)) {
    return host.TriggerFallEvent() ? ActorTileTriggerResult::kFallEvent
                                   : ActorTileTriggerResult::kDefeated;
  }
  return ActorTileTriggerResult::kNone;
}

// decomp: BATTLE.PRG func_80073E30 (src/BATTLE/BATTLE.PRG/146C.c:4935).
bool TryOpenChest(ChestTarget target, ChestInteractionHost& host) {
  const auto chest = host.Chest(target);
  if (!chest) {
    return false;
  }
  bool can_open = chest->lock_level == 0;
  if (chest->lock_level == kKeyLock || chest->lock_level == kSigilLock) {
    const auto flag = kItemUnlockFlagBase + chest->required_item;
    const bool unlocked_before = host.ScriptFlag(flag);
    if (unlocked_before || host.HasItem(chest->required_item)) {
      if (!unlocked_before) {
        host.SetScriptFlag(flag);
        if (chest->required_item >= kConsumedSigilFirst) {
          host.ConsumeItem(chest->required_item);
        }
      }
      if (!host.Unlock(target)) {
        return false;
      }
      host.ShowUnlocked(chest->required_item);
      can_open = true;
    }
  }
  host.PlayPose(*chest, can_open ? kOpeningPose : kRefusedPose);
  return can_open && host.StartAnimation(target);
}

// decomp: BATTLE.PRG func_80074050 (src/BATTLE/BATTLE.PRG/146C.c:4993).
bool BeginChestInteraction(ChestTarget target, std::int32_t actor,
                           ChestInteractionHost& host) {
  const auto chest = host.Chest(target);
  if (!chest || !host.SelectChest(target, actor)) {
    return false;
  }
  host.BeginChestFlow();
  host.RestoreIdlePoses();
  host.PlayPose(*chest, 1);
  host.ResetActionSelection();
  return true;
}

// decomp: BATTLE.PRG func_800742A4 (src/BATTLE/BATTLE.PRG/146C.c:5066).
bool FinishChestInteraction(ChestInteractionHost& host) {
  const auto target = host.SelectedChest();
  const auto chest = target ? host.Chest(*target) : std::nullopt;
  if (!chest) {
    return false;
  }
  host.FinishChestFlow();
  host.PlayPose(*chest, kClosingPose);
  host.ReleaseNotification();
  return true;
}

// decomp: BATTLE.PRG func_800743E0 (src/BATTLE/BATTLE.PRG/146C.c:5107).
bool OpenChestContents(ChestInteractionHost& host) {
  const auto target = host.SelectedChest();
  const auto chest = target ? host.Chest(*target) : std::nullopt;
  if (!chest) {
    return false;
  }
  if (chest->lock_level != 0) {
    switch (chest->lock_level) {
      case kKeyLock:
        host.ShowLocked(2, chest->required_item);
        break;
      case kBreakableLock:
        host.ShowLocked(1, chest->required_item);
        break;
      case kSigilLock:
        host.ShowLocked(kSigilLock, chest->required_item);
        break;
      default:
        return false;
    }
    host.PauseForDialog();
  } else {
    if (chest->opened_flag < kStorageFlagFirst) {
      host.OpenChestLoot(*target);
    } else {
      host.OpenChestStorage(chest->opened_flag - kStorageFlagFirst);
    }
    host.PauseForMenu();
  }
  return true;
}

// decomp: BATTLE.PRG func_800744B8 (src/BATTLE/BATTLE.PRG/146C.c:5139).
bool UpdateChestContentsSelection(ChestInteractionHost& host) {
  const auto target = host.SelectedChest();
  const auto chest = target ? host.Chest(*target) : std::nullopt;
  if (!chest) {
    return false;
  }
  if (chest->lock_level != 0) {
    if (host.NotificationPending()) {
      return true;
    }
    host.ResumeField();
    return false;
  }
  const auto selection = host.UpdateContentsSelection();
  if (selection == ChestSelectionResult::kPending) {
    return true;
  }
  if (selection == ChestSelectionResult::kAccepted) {
    host.SetScriptFlag(chest->opened_flag + kOpenedFlagBase);
  } else {
    static_cast<void>(host.StartAnimation(*target));
  }
  host.ReleaseContentsRows();
  host.ResumeField();
  return false;
}

// decomp: BATTLE.PRG func_800741D4 (src/BATTLE/BATTLE.PRG/146C.c:5044).
bool DispatchTrapOrChestInteraction(InteractionDispatchHost& host) {
  const auto target = host.Target();
  if (host.TrapMode()) {
    const auto* trap = std::get_if<TrapTarget>(&target);
    if (trap == nullptr) {
      return false;
    }
    host.BeginDispatch();
    return host.DispatchTrap(*trap);
  }
  const auto* chest = std::get_if<ChestTarget>(&target);
  if (chest == nullptr) {
    return false;
  }
  host.BeginDispatch();
  return host.DispatchChest(*chest);
}

// decomp: BATTLE.PRG func_80074314 (src/BATTLE/BATTLE.PRG/146C.c:5080).
bool FinishTrapInteraction(TrapFinishHost& host) {
  const auto target = host.Target();
  if (!target) {
    return false;
  }
  host.FinishFlow();
  host.ReleaseNotification();
  const bool disabled = host.DisableTrap(*target);
  host.ClearTarget();
  return disabled;
}

// decomp: BATTLE.PRG func_80073F7C (src/BATTLE/BATTLE.PRG/146C.c:4973).
bool BeginTrapInteraction(TrapTarget target, std::int32_t actor,
                          TrapSelectionHost& host) {
  if (!host.Valid(target, actor)) {
    return false;
  }
  host.BeginFlow();
  host.Select(target, actor);
  for (std::int32_t slot = 0; slot < kIdleActorCount; ++slot) {
    host.RestoreIdle(slot);
  }
  host.ShowInformation(target);
  host.ResetActionSelection();
  return true;
}

// decomp: BATTLE.PRG func_80073D30 (src/BATTLE/BATTLE.PRG/146C.c:4909).
bool TriggerActorTrap(TrapTarget target, const TrapActionInput& input,
                      TrapActionHost& host) {
  if (input.actor < 0 || input.actor >= kTrapActorSlots ||
      !host.HasActionState(input)) {
    return false;
  }
  const auto kind = host.Kind(target);
  if (!kind || *kind < 0 || *kind >= kTrapKindLimit ||
      !host.StageVictim(input) ||
      !host.ResolveAction(
          static_cast<std::uint16_t>(*kind + kTrapActionBase))) {
    return false;
  }
  const auto effect_kind = host.Kind(target);
  if (!effect_kind || *effect_kind < 0 || *effect_kind >= kTrapKindLimit) {
    return false;
  }
  return host.BeginEffect(TrapEffectRequest{
      .effect_id = static_cast<std::uint16_t>(*effect_kind + kTrapActionBase),
      .actor = input.actor,
      .source_position = std::nullopt});
}

// decomp: BATTLE.PRG func_80074120 (src/BATTLE/BATTLE.PRG/146C.c:5019).
bool WaitForChestCameraFocus(InteractionCameraHost& host) {
  const auto focus = host.Focus();
  if (!focus) {
    return true;
  }
  return host.StepLookAt(*focus, 0, kInteractionCameraSpeed) != 0 ||
         (host.AshleyStateFlags() & kInteractionAnimationReady) == 0;
}

// decomp: BATTLE.PRG func_8007418C (src/BATTLE/BATTLE.PRG/146C.c:5034).
std::optional<std::int32_t> StepTrapCameraFocus(InteractionCameraHost& host) {
  const auto focus = host.Focus();
  if (!focus) {
    return std::nullopt;
  }
  return host.StepLookAt(*focus, 0, kInteractionCameraSpeed);
}

// decomp: BATTLE.PRG func_8007424C (src/BATTLE/BATTLE.PRG/146C.c:5055).
bool IsChestAnimationReady(InteractionCameraHost& host) {
  if ((host.AshleyStateFlags() & kInteractionAnimationReady) == 0) {
    return false;
  }
  return !host.NotificationPending();
}

// decomp: BATTLE.PRG func_80074374 (src/BATTLE/BATTLE.PRG/146C.c:5089).
std::optional<std::int32_t> TrackInteractionCameraToAshley(
    InteractionCameraHost& host) {
  auto point = host.AshleyPosition();
  if (!point) {
    return std::nullopt;
  }
  const auto y = static_cast<std::uint16_t>(
      static_cast<std::int32_t>(point->y) - kInteractionCameraReturnOffset);
  point->y = std::bit_cast<std::int16_t>(y);
  const auto speed = host.StepLookAt(*point, 1, kInteractionCameraSpeed);
  return host.AshleyTransitionPose() ? 0 : speed;
}

// decomp: BATTLE.PRG func_80074798 (src/BATTLE/BATTLE.PRG/146C.c:5232).
FacingTile TileInFrontOfActor(const VS::Actors::ActorPlacementView& actor) {
  const auto rounded = static_cast<std::uint16_t>(
      (static_cast<std::uint16_t>(actor.position.w) + kFacingRoundBias) &
      kFacingHeadingMask);
  FacingTile tile{.x = actor.tile.x,
                  .z = actor.tile.z,
                  .layer = actor.tile.layer,
                  .direction = static_cast<CardinalFacing>(
                      rounded >> kFacingQuadrantShift)};
  switch (tile.direction) {
    case CardinalFacing::kNegativeZ:
      tile.z = static_cast<std::uint8_t>(tile.z - 1);
      break;
    case CardinalFacing::kNegativeX:
      tile.x = static_cast<std::uint8_t>(tile.x - 1);
      break;
    case CardinalFacing::kPositiveZ:
      tile.z = static_cast<std::uint8_t>(tile.z + 1);
      break;
    case CardinalFacing::kPositiveX:
      tile.x = static_cast<std::uint8_t>(tile.x + 1);
      break;
  }
  return tile;
}

// decomp: BATTLE.PRG func_80074860 (src/BATTLE/BATTLE.PRG/146C.c:5260).
bool TryTriggerFacingTileEvent(std::int32_t actor, FacingEventHost& host) {
  if (actor < 0 || actor >= kTrapActorSlots) {
    return false;
  }
  const auto placement = host.Placement(actor);
  if (!placement) {
    return false;
  }
  const auto tile = TileInFrontOfActor(*placement);
  return host.EvaluateInteraction(tile);
}

// decomp: BATTLE.PRG func_800748B8 (src/BATTLE/BATTLE.PRG/146C.c:5271).
std::optional<RoomObjectTarget> FindFacingRoomObject(std::int32_t actor,
                                                     FacingResourceHost& host) {
  if (actor < 0 || actor >= kTrapActorSlots) {
    return std::nullopt;
  }
  const auto placement = host.Placement(actor);
  if (!placement) {
    return std::nullopt;
  }
  const auto tile = TileInFrontOfActor(*placement);
  const auto object = host.RoomObject(tile);
  const auto difference = FacingFloorHeight(host.FloorHeight(tile)) -
                          placement->position.y + kFacingRoomHeightBias;
  if (static_cast<std::uint32_t>(difference) >= kFacingRoomHeightWindow) {
    return std::nullopt;
  }
  return object;
}

// decomp: BATTLE.PRG func_80074950 (src/BATTLE/BATTLE.PRG/146C.c:5290).
std::optional<ChestTarget> FindFacingUnlootedChest(std::int32_t actor,
                                                   FacingResourceHost& host) {
  if (actor < 0 || actor >= kTrapActorSlots) {
    return std::nullopt;
  }
  const auto placement = host.Placement(actor);
  if (!placement) {
    return std::nullopt;
  }
  const auto tile = TileInFrontOfActor(*placement);
  const auto chest = host.Chest(tile);
  if (!chest || chest->facing != static_cast<std::int16_t>(tile.direction)) {
    return std::nullopt;
  }
  const auto flag = std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
      static_cast<std::int32_t>(chest->opened_flag) + kOpenedFlagBase));
  if (host.ScriptFlag(flag)) {
    return std::nullopt;
  }
  const auto height =
      FacingFloorHeight(host.FloorHeight(tile)) + kFacingChestHeightOffset;
  return placement->position.y == height ? std::optional{chest->target}
                                         : std::nullopt;
}

// decomp: BATTLE.PRG func_80073AA4 (src/BATTLE/BATTLE.PRG/146C.c:4823).
void BeginObjectInteractionWait(ObjectInteractionWaitHost& host) {
  host.SetControlMode(FieldControlMode::kObjectInteractionWait);
  host.PauseField();
}

// decomp: BATTLE.PRG func_80073ACC (src/BATTLE/BATTLE.PRG/146C.c:4829).
void EndObjectInteractionWait(ObjectInteractionWaitHost& host) {
  host.ReleaseNotification();
  host.ResumeField();
  host.EnterExploration();
}

// decomp: BATTLE.PRG func_80073AFC (src/BATTLE/BATTLE.PRG/146C.c:4836).
// decomp: BATTLE.PRG func_8006CE70 (146C.c:2163),110MIPS8006CE70-D024.
bool ApplyUnlockEffectToRoomObject(
    const std::optional<UnlockEffectTarget>& target, const TrapPosition& point,
    UnlockEffectHost& host) {
  constexpr std::int16_t kFirstBreakableLock = 2;
  constexpr std::int16_t kLastBreakableLock = 3;
  constexpr std::uint16_t kOpenedFlagBase = 0x3C0;
  constexpr std::int32_t kUnlockSoundFirst = 0x3F;
  constexpr std::int32_t kUnlockSoundSecond = 0x40;
  if (!target) return false;
  const auto state = host.State(*target);
  bool eligible = state && state->lock_level >= kFirstBreakableLock &&
                  state->lock_level <= kLastBreakableLock;
  if (eligible && std::holds_alternative<ChestTarget>(*target)) {
    const auto flag = std::bit_cast<std::int16_t>(
        static_cast<std::uint16_t>(state->opened_flag + kOpenedFlagBase));
    eligible = !host.ChestOpened(flag);
  }
  if (!eligible || !host.RollLockBreak() || !host.ClearLock(*target)) {
    host.ShowLockedLabel(point);
    return false;
  }
  host.PlaySound(kUnlockSoundFirst);
  host.PlaySound(kUnlockSoundSecond);
  return true;
}

RoomObjectInteractionResult InteractWithLockedRoomObject(
    RoomObjectInteractionHost& host) {
  const auto state = host.State();
  if (!state) {
    return RoomObjectInteractionResult::kRefused;
  }
  if (state->is_open || state->is_triggered) {
    return RoomObjectInteractionResult::kBusy;
  }
  if (state->lock_state > kSpecialRoomLock) {
    return RoomObjectInteractionResult::kRefused;
  }
  bool toggle = state->lock_state == 0;
  if (state->lock_state == kKeyLock || state->lock_state == kSpecialRoomLock) {
    const bool has_item = host.HasItem(state->required_item);
    const auto flag = state->required_item + kItemUnlockFlagBase;
    if (has_item || host.PreviousUnlockFlag(flag)) {
      if (has_item && state->required_item >= kConsumedSigilFirst) {
        host.SetUnlockFlag(flag, kRoomSigilUnlockFlagValue);
        host.ConsumeItem(state->required_item);
      }
      if (!host.Unlock()) {
        return RoomObjectInteractionResult::kRefused;
      }
      if (has_item) {
        host.ShowNotification(state->required_item >= kConsumedSigilFirst
                                  ? kRoomSigilUnlocked
                                  : kRoomKeyUnlocked,
                              state->required_item);
      }
      toggle = true;
    }
  }
  if (toggle) {
    host.PlayPose(*state, kRoomObjectPose);
    host.BeginToggle();
    return RoomObjectInteractionResult::kWait;
  }
  host.ShowNotification(RoomObjectLockedNotification(*state),
                        state->required_item);
  host.PlaySound(kRoomRejectedSoundFirst);
  host.PlaySound(kRoomRejectedSoundSecond);
  host.RestoreIdle(0);
  return RoomObjectInteractionResult::kWait;
}

}  // namespace VS::Field
