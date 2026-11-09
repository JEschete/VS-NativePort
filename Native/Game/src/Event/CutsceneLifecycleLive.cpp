#include "Event/CutsceneLifecycleLive.hpp"

#include "Actors/AnimationControl.hpp"
#include "Actors/AnimationControlLive.hpp"
#include "Actors/AppearanceLive.hpp"
#include "Actors/SkeletonLive.hpp"
#include "Audio/MusicLive.hpp"
#include "Battle/Ai/ActorCleanupLive.hpp"
#include "Battle/Ai/NavigationPrimitivesLive.hpp"
#include "Battle/Ai/PlanningWorkspaceLive.hpp"
#include "Event/CutsceneLifecycle.hpp"
#include "Event/ScriptVmLive.hpp"
#include "Field/CameraLive.hpp"
#include "Field/GameModeLive.hpp"
#include "Field/RoomEffectsLive.hpp"
#include "Render/CameraViewLive.hpp"
#include "State/ScriptFlagsLive.hpp"
extern "C" {
#include "Audio/SoundBanks.h"
#include "Battle/AbilityState.h"
#include "Battle/ActionFlow.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Battle/Feedback.h"
#include "Battle/Hud.h"
#include "Core/Diagnostics.h"
#include "Event/RoomTriggerEntryLive.h"
#include "Event/ScriptVm.h"
#include "Event/Triggers.h"
#include "Field/CameraLiveState.h"
#include "Field/Gimmicks.h"
#include "Field/Input.h"
#include "Field/Interactions.h"
#include "Field/PlatformPlacementLive.h"
#include "Field/RoomLoadingLiveState.h"
#include "State/Timers.h"
}

namespace VS::Event {
namespace {

constexpr int kFieldCutsceneMode = 3;
constexpr int kPreparingActionFlow = 2;
constexpr int kWaitingActionFlow = 1;
constexpr int kRestoreFarClip = 0x1000;
constexpr int kRoomMusicVolume = 0x7F;
constexpr int kRoomMusicFadeFrames = 0x3C;
constexpr int kFollowOnTrigger = 0xA;
constexpr int kTimerTrigger = 0x10;

class LiveCutsceneHost final : public CutsceneHost {
 public:
  void Refuse(CutsceneRefusal reason, int value) override {
    switch (reason) {
      case CutsceneRefusal::kMissingPlayer:
        Core_Warnf("cutscene", "Preparation refused: missing player");
        break;
      case CutsceneRefusal::kMusicSlot:
        Core_Warnf(
            "cutscene",
            "Music restoration refused: slot %d is outside the handle table",
            value);
        break;
      case CutsceneRefusal::kMissingActor:
        Core_Warnf("cutscene", "Command update refused: actor %d was removed",
                   value);
        break;
    }
  }
  std::optional<CutscenePreferences> Preferences() override {
    if (Battle_characterState == nullptr) {
      return std::nullopt;
    }
    return CutscenePreferences{(Battle_characterState->weaponDrawn & 1) != 0,
                               Field_sphericalCamera.delta.pitch,
                               Field_sphericalCamera.initialDistance};
  }
  void SetPreparation(CutscenePreparation preparation) override {
    Field_controlMode = kFieldCutsceneMode;
    Battle_actionFlowMode = preparation == CutscenePreparation::kPreparing
                                ? kPreparingActionFlow
                                : kWaitingActionFlow;
  }
  void SetFlag(int flag, std::uint8_t value) override {
    State::SetLiveScriptFlag(flag, value);
  }
  std::optional<std::uint8_t> ActorCommand(int actor) override {
    if (Battle_actors[actor] == nullptr) {
      return std::nullopt;
    }
    return Battle_actors[actor]->commandState;
  }
  void SetActorCommand(int actor, std::uint8_t command) override {
    if (Battle_actors[actor] == nullptr) {
      Refuse(CutsceneRefusal::kMissingActor, actor);
      return;
    }
    Battle_actors[actor]->commandState = command;
  }
  void ResetActorHeadTracking(int actor) override {
    Actors::SetLiveActorHeadTracking(actor, -2, nullptr, -1);
  }
  void RefreshActorReservations(int actor) override {
    Battle::Ai::RefreshLiveActorAiReservations(actor);
  }
  void SetMarkersHidden(bool hidden) override {
    Battle_SetActorMarkersHidden(hidden ? 1 : 0);
  }
  void ClearMenuRequest() override { Field_isMenuRequestQueued = 0; }
  void PauseMovingPlatforms(bool paused) override {
    Field_SetMovingPlatformsPausedEntry(paused ? 1 : 0);
  }
  void RequestEventStart() override {
    VS::Event::RequestEventStart(VS::Event::LiveScriptVm());
  }
  std::uint16_t PendingEventSelector() override {
    return static_cast<std::uint16_t>(
        VS::Event::PendingEventSelector(VS::Event::LiveScriptVm()));
  }
  void DisableFirstPersonView() override { Field_isFirstPersonViewEnabled = 0; }
  void RebuildActionPlans() override {
    Battle::Ai::RebuildLiveActorActionPlans(nullptr);
  }
  void UpdateRouteGates() override {
    Battle::Ai::UpdateLiveRouteGateAvailability();
  }
  bool EvaluateFollowOnTrigger() override {
    return Event_EvaluateRoomEventTriggersEntry(kFollowOnTrigger, 0, nullptr,
                                                0) == 1;
  }
  bool SceneTimerActive() override { return State_sceneTimer.mode != 0; }
  bool EvaluateTimerTrigger() override {
    return Event_EvaluateRoomEventTriggersEntry(kTimerTrigger, 0, nullptr, 0) ==
           1;
  }
  void ClearActorSoundOverrides() override {
    Actors::SetLiveActorSoundOverrides(-1, Actors::SoundEffectSlot{});
  }
  void ResetActorPaletteFade(int actor) override {
    Actors::SetLiveActorPaletteFade(actor, 0);
  }
  void ResetActorTint(int actor) override {
    Actors::SetLiveActorTint(actor, nullptr);
  }
  void RestoreRoomEffectVolume() override {
    Field::LiveRestoreRoomEffectSoundVolume();
  }
  void RestoreCamera() override {
    Render::SetLiveCameraFarClip(kRestoreFarClip, 1);
    Field::LiveSetCameraRoll(0);
  }
  void ShowGameplayHud() override { Battle_SetGameplayHudHidden(0); }
  CutsceneMusic Music() override {
    return {Field_zoneContext.musicSlot, Audio_soundState.playingMusicHandle,
            Audio_soundState.musicHandles};
  }
  void PlayRoomMusic(int slot) override {
    Audio::LivePlayMusicSlot(slot, kRoomMusicVolume, 0);
  }
  void FadeRoomMusic(int slot) override {
    Audio::LiveFadeMusicSlotVolume(slot, kRoomMusicVolume,
                                   kRoomMusicFadeFrames);
  }
  void EnterFieldExploration() override {
    Field::LiveEnterFieldExplorationMode();
  }
};

}  // namespace

bool PrepareLiveCutsceneActors() {
  LiveCutsceneHost host;
  return PrepareCutsceneActors(host);
}

bool StartLivePreparedCutscene() {
  LiveCutsceneHost host;
  return StartPreparedCutscene(host);
}

bool BeginLiveQueuedCutscene() {
  LiveCutsceneHost host;
  return BeginQueuedCutscene(host);
}

void EndLiveFieldCutscene() {
  LiveCutsceneHost host;
  EndFieldCutscene(host);
}

}  // namespace VS::Event
