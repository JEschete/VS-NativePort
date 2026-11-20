// Hand-maintained (tools/port/owned.csv).
#include "Field/GameModeLive.hpp"

#include <cstdint>

#include "Actors/ActorRosterLive.hpp"
#include "Actors/ActorStateLive.hpp"
#include "Actors/AnimationControlLive.hpp"
#include "Actors/Location.hpp"
#include "Audio/AmbienceLive.hpp"
#include "Battle/AbilityState.hpp"
#include "Battle/ActionFlow.hpp"
#include "Battle/Actor.hpp"
#include "Battle/Feedback.hpp"
#include "Battle/Hud.hpp"
#include "Battle/HudOrchestrationLive.hpp"
#include "Battle/HudStateLive.hpp"
#include "Battle/TargetingLive.hpp"
#include "Battle/TargetingState.hpp"
#include "Core/Log.hpp"
#include "Effects/SecondaryEffectRequestsLive.hpp"
#include "Field/GameModeLiveState.hpp"
#include "Field/Gimmicks.hpp"
#include "Field/Interactions.hpp"
#include "Field/MapLive.hpp"
#include "Field/PlatformPlacementLive.hpp"
#include "Field/RoomRenderingLive.hpp"
#include "Field/RoomStateLive.hpp"
#include "Field/TransitionsLive.hpp"
#include "Legacy/Psx.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/MagicCosts.hpp"
#include "Render/PaletteAnimationLive.hpp"
#include "State/ScriptFlagsLive.hpp"

// decomp: D_800F1968 (D_800F1968@BATTLE/BATTLE.PRG)
int VS::Field::isInteractionPaused;

namespace VS::Field {
namespace {
constexpr const char* kChannel = "field";
constexpr int kAshley = 0;

int Flag(bool value) { return value ? 1 : 0; }

class LiveFieldModeHost final : public FieldModeHost {
 public:
  explicit LiveFieldModeHost(
      Menu::Framework::ActionSelection* selection = nullptr)
      : selection_(selection) {}

  void SetControlMode(FieldControlMode mode) override {
    controlMode = static_cast<std::uint32_t>(mode);
  }
  void SetActionFlowMode(int mode) override {
    VS::Battle::actionFlowMode = static_cast<std::uint32_t>(mode);
  }
  void SetInteractionPaused(bool paused) override {
    isInteractionPaused = Flag(paused);
  }
  void MarkCurrentRoomVisited() override { LiveMarkCurrentRoomVisited(); }

  void RestoreActorIdlePose(int actor_id) override {
    Actors::RestoreLiveActorIdlePose(actor_id);
  }
  void FreezeActorAnimations() override { Actors::FreezeLiveActorAnimations(); }
  void UnfreezeActorAnimations() override {
    Actors::UnfreezeLiveActorAnimations();
  }
  void SetActorMarkersHidden(bool hidden) override {
    VS::Battle::SetLiveActorMarkersHidden(Flag(hidden));
  }
  void SetMinimapVisible(bool visible) override {
    static_cast<void>(SetLiveMinimapVisible(Flag(visible)));
  }
  void TransformRoomPalettes(const ColorTransform& transform) override {
    Render::TransformLiveRoomPalettes(
        transform.mode, transform.duration, transform.red_offset,
        transform.green_offset, transform.blue_offset);
  }
  void TransformSceneColors(const ColorTransform& transform) override {
    Render::TransformLiveSceneColors(
        transform.mode, transform.duration, transform.red_offset,
        transform.green_offset, transform.blue_offset);
  }
  void SetClutAnimationEnabled(bool enabled) override {
    Render::SetLiveClutAnimationEnabled(Flag(enabled));
  }
  void SetRoomAnimationsPaused(bool paused) override {
    LiveSetRoomAnimationsPaused(Flag(paused));
  }
  void SetMovingPlatformsPaused(bool paused) override {
    SetMovingPlatformsPausedEntry(Flag(paused));
  }
  void SetRoomAmbienceDucked(bool ducked) override {
    Audio::LiveSetRoomAmbienceDucked(Flag(ducked));
  }

  [[nodiscard]] bool IsWeaponDrawn() const override {
    return (VS::Battle::characterState->weapon_drawn & kHudWeaponDrawn) != 0;
  }
  void SetHudBattleMode(int mode) override {
    VS::Battle::SetLiveHudBattleMode(mode);
  }
  void ReleaseMenuRows() override { Menu::Framework::ReleaseLiveMenuRows(); }
  void ClearActionMenuStatus() override {
    if (selection_ != nullptr) {
      selection_->menu_status = 0;
    }
  }
  void BeginActorActionSelection(int actor_id,
                                 std::uint16_t action_id) override {
    VS::Battle::LiveBeginActorActionSelection(actor_id, action_id);
  }
  void SpawnTrapTileEffectAtAshley() override {
    VS::Actors::ActorLocationQuery placement{};
    static_cast<void>(Actors::LiveActorPlacement(kAshley, &placement));
    VS::Effects::LiveSpawnTrapTileEffect(&placement.location);
  }
  void SetScriptFlag(int flag, int value) override {
    State::SetLiveScriptFlag(flag, value);
  }
  void TeleportToLocation(const TeleportRequest& request) override {
    static_cast<void>(LiveTeleportToLocation(request.location_id,
                                             request.tile_x, request.tile_z,
                                             request.effect, request.speed));
  }
  [[nodiscard]] std::int16_t AshleyMp() const override {
    return VS::Battle::characterState->stats->current_mp;
  }
  void SetAshleyMp(std::int16_t mp) override {
    VS::Battle::characterState->stats->current_mp = mp;
  }
  void PlayAshleyActionPose(int pose) override {
    Actors::PlayLiveActorActionPose(kAshley, nullptr, pose);
  }

 private:
  Menu::Framework::ActionSelection* selection_;
};

template <typename Rule>
void RunFieldMode(Rule&& rule,
                  Menu::Framework::ActionSelection* selection = nullptr) {
  LiveFieldModeHost host(selection);
  rule(host);
}
}  // namespace

ActionMenuResult ActionMenuResultOf(
    const Menu::Framework::ActionSelection& selection) {
  const Menu::Magic::TeleportDestination teleport =
      Menu::Magic::UnpackTeleportDestination(selection.execute_ability);
  return ActionMenuResult{
      .kind = selection.selection_kind,
      .action_id = Menu::Framework::ExecuteAbilityAction(selection),
      .location_id = teleport.location,
      .tile_x = teleport.tile_x,
      .tile_z = teleport.tile_z,
      .mp_cost = teleport.mp_cost,
  };
}

void LivePauseFieldForInteractionDialog() {
  RunFieldMode(PauseFieldForInteractionDialog);
}

void LivePauseFieldForObjectInteraction() {
  RunFieldMode(PauseFieldForObjectInteraction);
}

void LivePauseFieldForMenu() { RunFieldMode(PauseFieldForMenu); }

void LiveResumeFieldAfterMenu() { RunFieldMode(ResumeFieldAfterMenu); }

void LiveEnterFieldExplorationMode() {
  RunFieldMode(EnterFieldExplorationMode);
}

void LiveBeginShortcutMenu() { RunFieldMode(BeginShortcutMenu); }

void LiveEndShortcutMenu() { RunFieldMode(EndShortcutMenu); }

void LiveBeginBattleActionMenu(int /*unused*/, int /*unused*/) {
  RunFieldMode(BeginBattleActionMenu);
}

void LiveCloseBattleActionMenu(Menu::Framework::ActionSelection* selection) {
  if (selection == nullptr) {
    VS::Core::LogWarn(kChannel,
                      "closing the action menu without its selection");
  }
  RunFieldMode(CloseBattleActionMenu, selection);
}

void LiveDispatchBattleActionMenuResult(
    Menu::Framework::ActionSelection* selection) {
  if (selection == nullptr) {
    VS::Core::LogWarn(kChannel, "action menu result without its selection");
    return;
  }
  const ActionMenuResult result = ActionMenuResultOf(*selection);
  RunFieldMode(
      [&result](FieldModeHost& host) {
        DispatchBattleActionMenuResult(host, result);
      },
      selection);
}
}  // namespace VS::Field
