#include "Event/CutsceneLifecycleLive.hpp"

#include "Actors/AnimationControl.hpp"
#include "Actors/AnimationControlLive.hpp"
#include "Actors/AppearanceLive.hpp"
#include "Actors/SkeletonLive.hpp"
#include "Audio/MusicLive.hpp"
#include "Audio/SoundState.hpp"
#include "Battle/AbilityState.h"
#include "Battle/ActionFlow.hpp"
#include "Battle/Actor.h"
#include "Battle/Ai/ActorCleanupLive.hpp"
#include "Battle/Ai/NavigationPrimitivesLive.hpp"
#include "Battle/Ai/PlanningWorkspaceLive.hpp"
#include "Battle/Damage.h"
#include "Battle/Feedback.hpp"
#include "Battle/Hud.hpp"
#include "Battle/HudOrchestrationLive.hpp"
#include "Battle/HudStateLive.hpp"
#include "Core/Diagnostics.hpp"
#include "Event/CutsceneLifecycle.hpp"
#include "Event/RoomTriggerEntryLive.hpp"
#include "Event/ScriptVmLive.hpp"
#include "Event/Triggers.hpp"
#include "Field/CameraLive.hpp"
#include "Field/CameraLiveState.hpp"
#include "Field/GameModeLive.hpp"
#include "Field/Gimmicks.hpp"
#include "Field/Input.hpp"
#include "Field/Interactions.hpp"
#include "Field/PlatformPlacementLive.hpp"
#include "Field/RoomEffectsLive.hpp"
#include "Field/RoomLoadingLiveState.hpp"
#include "Render/CameraViewLive.hpp"
#include "State/ScriptFlagsLive.hpp"
#include "State/Timers.hpp"

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
        VS::Core::Warnf("cutscene", "Preparation refused: missing player");
        break;
      case CutsceneRefusal::kMusicSlot:
        VS::Core::Warnf(
            "cutscene",
            "Music restoration refused: slot %d is outside the handle table",
            value);
        break;
      case CutsceneRefusal::kMissingActor:
        VS::Core::Warnf("cutscene",
                        "Command update refused: actor %d was removed", value);
        break;
    }
  }
  std::optional<CutscenePreferences> Preferences() override {
    if (Battle_characterState == nullptr) {
      return std::nullopt;
    }
    return CutscenePreferences{(Battle_characterState->weaponDrawn & 1) != 0,
                               VS::Field::sphericalCamera.motion.target_yaw,
                               VS::Field::sphericalCamera.preferred_distance};
  }
  void SetPreparation(CutscenePreparation preparation) override {
    VS::Field::controlMode = kFieldCutsceneMode;
    VS::Battle::actionFlowMode = preparation == CutscenePreparation::kPreparing
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
    VS::Battle::SetLiveActorMarkersHidden(hidden ? 1 : 0);
  }
  void ClearMenuRequest() override { VS::Field::isMenuRequestQueued = 0; }
  void PauseMovingPlatforms(bool paused) override {
    VS::Field::SetMovingPlatformsPausedEntry(paused ? 1 : 0);
  }
  void RequestEventStart() override {
    VS::Event::RequestEventStart(VS::Event::LiveScriptVm());
  }
  std::uint16_t PendingEventSelector() override {
    return static_cast<std::uint16_t>(
        VS::Event::PendingEventSelector(VS::Event::LiveScriptVm()));
  }
  void DisableFirstPersonView() override {
    VS::Field::isFirstPersonViewEnabled = 0;
  }
  void RebuildActionPlans() override {
    Battle::Ai::RebuildLiveActorActionPlans(nullptr);
  }
  void UpdateRouteGates() override {
    Battle::Ai::UpdateLiveRouteGateAvailability();
  }
  bool EvaluateFollowOnTrigger() override {
    return EvaluateRoomEventTriggersEntry(kFollowOnTrigger, 0, nullptr, 0) == 1;
  }
  bool SceneTimerActive() override { return VS::State::sceneTimer.mode != 0; }
  bool EvaluateTimerTrigger() override {
    return EvaluateRoomEventTriggersEntry(kTimerTrigger, 0, nullptr, 0) == 1;
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
  void ShowGameplayHud() override { VS::Battle::SetLiveGameplayHudHidden(0); }
  CutsceneMusic Music() override {
    return {VS::Field::zoneContext.music_slot,
            VS::Audio::soundState.playing_music_handle,
            VS::Audio::soundState.music_handles};
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
