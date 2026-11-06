#include "Event/CutsceneLifecycle.hpp"

namespace VS::Event {
namespace {

constexpr std::uint8_t kScriptReady = 0x10;
constexpr std::uint8_t kScriptBusy = 0x20;
constexpr std::uint8_t kFieldReady = 0x40;
constexpr std::uint8_t kAiReserved = 0x80;
constexpr int kWeaponDrawnFlag = 0xAC;
constexpr int kCameraPitchFlag = 0xAF;
constexpr int kCameraDistanceFlag = 0xB0;
constexpr int kCameraPitchStep = 512;
constexpr int kCameraPitchMask = 7;
constexpr int kNearCameraDistance = 0x600;

void PrepareActors(CutsceneHost& host, const CutscenePreferences& preferences) {
  host.SetPreparation(CutscenePreparation::kPreparing);
  host.SetFlag(kWeaponDrawnFlag, preferences.weapon_drawn ? 1 : 0);
  host.SetFlag(
      kCameraPitchFlag,
      static_cast<std::uint8_t>((preferences.camera_pitch / kCameraPitchStep) &
                                kCameraPitchMask));
  host.SetFlag(kCameraDistanceFlag,
               preferences.camera_distance == kNearCameraDistance ? 0 : 1);
  for (int actor = 0; actor < kCutsceneActorCount; ++actor) {
    if (!host.ActorCommand(actor)) {
      continue;
    }
    host.ResetActorHeadTracking(actor);
    const auto command = host.ActorCommand(actor);
    if (!command || *command == kScriptReady || *command == kScriptBusy) {
      continue;
    }
    if (*command == kAiReserved) {
      host.RefreshActorReservations(actor);
    }
    host.SetActorCommand(actor, *command == kFieldReady ? kScriptReady : 0);
  }
  host.SetMarkersHidden(true);
  host.ClearMenuRequest();
  host.PauseMovingPlatforms(true);
}

void RestoreMusic(CutsceneHost& host) {
  const auto music = host.Music();
  if (music.room_slot == 0) {
    return;
  }
  if (music.room_slot < 0 ||
      static_cast<std::size_t>(music.room_slot) > music.handles.size()) {
    host.Refuse(CutsceneRefusal::kMusicSlot, music.room_slot);
    return;
  }
  if (music.playing_handle == kNoCutsceneMusicHandle) {
    host.PlayRoomMusic(music.room_slot);
  } else if (music.playing_handle ==
             music.handles[static_cast<std::size_t>(music.room_slot - 1)]) {
    host.FadeRoomMusic(music.room_slot);
  }
}

}  // namespace

// decomp: BATTLE.PRG func_80073718 (src/BATTLE/BATTLE.PRG/146C.c:4720)
bool PrepareCutsceneActors(CutsceneHost& host) {
  const auto preferences = host.Preferences();
  if (!preferences) {
    host.Refuse(CutsceneRefusal::kMissingPlayer, 0);
    return false;
  }
  PrepareActors(host, *preferences);
  return true;
}

// decomp: BATTLE.PRG func_80073870 (src/BATTLE/BATTLE.PRG/146C.c:4761)
bool StartPreparedCutscene(CutsceneHost& host) {
  const auto preferences = host.Preferences();
  if (!preferences) {
    host.Refuse(CutsceneRefusal::kMissingPlayer, 0);
    return false;
  }
  host.RequestEventStart();
  PrepareActors(host, *preferences);
  return true;
}

// decomp: BATTLE.PRG func_80073898 (src/BATTLE/BATTLE.PRG/146C.c:4767)
bool BeginQueuedCutscene(CutsceneHost& host) {
  if (host.PendingEventSelector() != 0) {
    host.SetPreparation(CutscenePreparation::kWaiting);
    return true;
  }
  return StartPreparedCutscene(host);
}

// decomp: BATTLE.PRG func_800738E4 (src/BATTLE/BATTLE.PRG/146C.c:4777)
void EndFieldCutscene(CutsceneHost& host) {
  host.PauseMovingPlatforms(false);
  host.SetMarkersHidden(false);
  host.DisableFirstPersonView();
  for (int actor = 0; actor < kCutsceneActorCount; ++actor) {
    if (host.ActorCommand(actor) == kScriptReady) {
      host.SetActorCommand(actor, kFieldReady);
    }
  }
  host.RebuildActionPlans();
  host.UpdateRouteGates();
  if (host.EvaluateFollowOnTrigger() ||
      (host.SceneTimerActive() && host.EvaluateTimerTrigger())) {
    (void)StartPreparedCutscene(host);
    return;
  }
  host.ClearActorSoundOverrides();
  for (int actor = 0; actor < kCutsceneActorCount; ++actor) {
    host.ResetActorPaletteFade(actor);
    host.ResetActorTint(actor);
  }
  host.RestoreRoomEffectVolume();
  host.RestoreCamera();
  host.ShowGameplayHud();
  RestoreMusic(host);
  host.EnterFieldExploration();
}

}  // namespace VS::Event
