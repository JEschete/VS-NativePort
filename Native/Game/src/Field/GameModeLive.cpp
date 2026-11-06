// Hand-maintained (tools/port/owned.csv).
#include "Field/GameModeLive.hpp"

#include <cstdint>

#include "Actors/ActorRosterLive.hpp"
#include "Actors/ActorStateLive.hpp"
#include "Actors/AnimationControlLive.hpp"
#include "Audio/AmbienceLive.hpp"
#include "Battle/TargetingLive.hpp"
#include "Core/Log.hpp"
#include "Field/MapLive.hpp"
#include "Field/RoomRenderingLive.hpp"
#include "Field/RoomStateLive.hpp"
#include "Field/TransitionsLive.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Render/PaletteAnimationLive.hpp"
#include "State/ScriptFlagsLive.hpp"

extern "C" {
#include "Actors/Location.h"
#include "Battle/AbilityState.h"
#include "Battle/ActionFlow.h"
#include "Battle/Actor.h"
#include "Battle/Feedback.h"
#include "Battle/Hud.h"
#include "Battle/TargetingState.h"
#include "Effects/ActionEffects.h"
#include "Field/GameModeLiveState.h"
#include "Field/Gimmicks.h"
#include "Field/Interactions.h"
#include "Legacy/Psx.h"
#include "Menu/Framework/ActionSelection.h"

// decomp: D_800F1968 (D_800F1968@BATTLE/BATTLE.PRG)
int Field_isInteractionPaused;
}

namespace VS::Field {
namespace {
constexpr const char* kChannel = "field";
constexpr int kAshley = 0;

int Flag(bool value) { return value ? 1 : 0; }

class LiveFieldModeHost final : public FieldModeHost {
 public:
  explicit LiveFieldModeHost(
      Menu_Framework_ActionSelection* selection = nullptr)
      : selection_(selection) {}

  void SetControlMode(FieldControlMode mode) override {
    Field_controlMode = static_cast<u_int>(mode);
  }
  void SetActionFlowMode(int mode) override {
    Battle_actionFlowMode = static_cast<u_int>(mode);
  }
  void SetInteractionPaused(bool paused) override {
    Field_isInteractionPaused = Flag(paused);
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
    Battle_SetActorMarkersHidden(Flag(hidden));
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
    Field_SetMovingPlatformsPaused(Flag(paused));
  }
  void SetRoomAmbienceDucked(bool ducked) override {
    Audio::LiveSetRoomAmbienceDucked(Flag(ducked));
  }

  [[nodiscard]] bool IsWeaponDrawn() const override {
    return (Battle_characterState->weaponDrawn & kHudWeaponDrawn) != 0;
  }
  void SetHudBattleMode(int mode) override { Battle_SetHudBattleMode(mode); }
  void ReleaseMenuRows() override { Menu::Framework::ReleaseLiveMenuRows(); }
  void ClearActionMenuStatus() override {
    if (selection_ != nullptr) {
      selection_->menuStatus = 0;
    }
  }
  void BeginActorActionSelection(int actor_id,
                                 std::uint16_t action_id) override {
    VS::Battle::LiveBeginActorActionSelection(actor_id, action_id);
  }
  void SpawnTrapTileEffectAtAshley() override {
    Actors_ActorLocationQuery placement{};
    static_cast<void>(Actors::LiveActorPlacement(kAshley, &placement));
    Effects_SpawnTrapTileEffect(&placement.location);
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
    return Battle_characterState->stats->currentMp;
  }
  void SetAshleyMp(std::int16_t mp) override {
    Battle_characterState->stats->currentMp = mp;
  }
  void PlayAshleyActionPose(int pose) override {
    Actors::PlayLiveActorActionPose(kAshley, nullptr, pose);
  }

 private:
  Menu_Framework_ActionSelection* selection_;
};

template <typename Rule>
void RunFieldMode(Rule&& rule,
                  Menu_Framework_ActionSelection* selection = nullptr) {
  LiveFieldModeHost host(selection);
  rule(host);
}
}  // namespace

ActionMenuResult ActionMenuResultOf(
    const Menu_Framework_ActionSelection& selection) {
  const auto& ability = selection.executeAbility;
  return ActionMenuResult{
      .kind = selection.selectionKind,
      .action_id = static_cast<std::uint16_t>(ability.word),
      .location_id = static_cast<int>(ability.bits.location),
      .tile_x = static_cast<int>(ability.bits.tileX),
      .tile_z = static_cast<int>(ability.bits.tileZ),
      .mp_cost = static_cast<int>(ability.bits.mpCost),
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

void LiveCloseBattleActionMenu(Menu_Framework_ActionSelection* selection) {
  if (selection == nullptr) {
    VS_LOG_WARN(kChannel, "closing the action menu without its selection");
  }
  RunFieldMode(CloseBattleActionMenu, selection);
}

void LiveDispatchBattleActionMenuResult(
    Menu_Framework_ActionSelection* selection) {
  if (selection == nullptr) {
    VS_LOG_WARN(kChannel, "action menu result without its selection");
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
