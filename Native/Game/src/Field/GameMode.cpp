// Hand-maintained (tools/port/owned.csv).
#include "Field/GameMode.hpp"

#include <cstdint>

namespace VS::Field {
namespace {
void SettleActorPoses(FieldModeHost& host) {
  for (int actor = kFirstPausedActor; actor < kPausedActorEnd; ++actor) {
    host.RestoreActorIdlePose(actor);
  }
}

int HudWeaponBit(const FieldModeHost& host) {
  return host.IsWeaponDrawn() ? kHudWeaponDrawn : 0;
}

void CastTeleport(FieldModeHost& host, const ActionMenuResult& result) {
  host.SpawnTrapTileEffectAtAshley();
  host.SetScriptFlag(kTeleportScriptFlag, 1);
  host.TeleportToLocation(TeleportRequest{
      .location_id = result.location_id,
      .tile_x = result.tile_x,
      .tile_z = result.tile_z,
      .effect = kTeleportEffect,
      .speed = kTeleportSpeed,
  });
  const int mp = host.AshleyMp();
  if (mp >= result.mp_cost) {
    host.SetAshleyMp(static_cast<std::int16_t>(mp - result.mp_cost));
  }
  host.PlayAshleyActionPose(kTeleportActionPose);
}
}  // namespace

void PauseFieldForInteractionDialog(FieldModeHost& host) {
  SettleActorPoses(host);
  host.SetMinimapVisible(false);
  host.SetActorMarkersHidden(true);
  host.SetInteractionPaused(true);
}

void PauseFieldForObjectInteraction(FieldModeHost& host) {
  SettleActorPoses(host);
  host.SetActorMarkersHidden(true);
  host.SetInteractionPaused(true);
}

void PauseFieldForMenu(FieldModeHost& host) {
  host.TransformRoomPalettes(kMenuRoomPaletteTransform);
  host.SetClutAnimationEnabled(false);
  host.UnfreezeActorAnimations();
  host.SetRoomAnimationsPaused(true);
  host.SetMovingPlatformsPaused(true);
  host.SetMinimapVisible(false);
  host.SetRoomAmbienceDucked(true);
  host.SetActorMarkersHidden(true);
  host.SetInteractionPaused(true);
}

void ResumeFieldAfterMenu(FieldModeHost& host) {
  host.SetClutAnimationEnabled(true);
  host.TransformSceneColors(kResumeSceneColorTransform);
  host.FreezeActorAnimations();
  host.SetRoomAnimationsPaused(false);
  host.SetMovingPlatformsPaused(false);
  host.SetMinimapVisible(true);
  host.SetRoomAmbienceDucked(false);
  host.SetActorMarkersHidden(false);
  host.SetInteractionPaused(false);
}

void EnterFieldExplorationMode(FieldModeHost& host) {
  host.SetControlMode(FieldControlMode::kExploration);
  host.SetActionFlowMode(kActionFlowIdle);
  host.MarkCurrentRoomVisited();
}

void BeginShortcutMenu(FieldModeHost& host) {
  host.SetControlMode(FieldControlMode::kShortcutMenu);
  PauseFieldForMenu(host);
}

void EndShortcutMenu(FieldModeHost& host) {
  host.SetControlMode(FieldControlMode::kExploration);
  ResumeFieldAfterMenu(host);
}

void BeginBattleActionMenu(FieldModeHost& host) {
  host.SetControlMode(FieldControlMode::kActionMenu);
  host.SetActionFlowMode(kActionFlowInitialTargetSelection);
  host.SetHudBattleMode(HudWeaponBit(host) | kHudActionMenu);
  PauseFieldForMenu(host);
}

void CloseBattleActionMenu(FieldModeHost& host) {
  host.ReleaseMenuRows();
  host.ClearActionMenuStatus();
  ResumeFieldAfterMenu(host);
}

void DispatchBattleActionMenuResult(FieldModeHost& host,
                                    const ActionMenuResult& result) {
  host.SetMinimapVisible(true);
  host.SetHudBattleMode(HudWeaponBit(host));
  switch (static_cast<ActionMenuChoice>(result.kind)) {
    case ActionMenuChoice::kBreakArt:
    case ActionMenuChoice::kSpell:
    case ActionMenuChoice::kItem:
      host.BeginActorActionSelection(0, result.action_id);
      return;
    case ActionMenuChoice::kTeleport:
      CastTeleport(host, result);
      break;
    default:
      break;
  }
  EnterFieldExplorationMode(host);
  CloseBattleActionMenu(host);
}
}  // namespace VS::Field
