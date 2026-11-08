#include "Field/ChestInteractionsLive.h"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Actors/ActorStateLive.hpp"
#include "Actors/AnimationControlLive.hpp"
#include "Audio/SfxLive.hpp"
#include "Battle/Ai/NavigationPrimitivesLive.hpp"
#include "Field/CameraLive.hpp"
#include "Field/ChestInteractions.hpp"
#include "Field/GameModeLive.hpp"
#include "Field/InteractionTargetLive.hpp"
#include "Field/RoomSwitchesLive.h"
#include "Input/ControllerLiveState.h"
#include "Items/InventoryLive.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "State/ScriptFlagsLive.hpp"

extern "C" {
#include "Battle/Action.h"
#include "Battle/ActionFlow.h"
#include "Battle/ActionResolution.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Effects/Runtime.h"
#include "Field/Doors.h"
#include "Field/Gimmicks.h"
#include "Field/InteractionFocusLive.h"
#include "Field/Interactions.h"
#include "Field/Mpd.h"
#include "Items/TreasureChests.h"
#include "Menu/Framework/ActionSelection.h"
#include "Menu/TextBox.h"
#include "State/Timers.h"
}

namespace {

constexpr std::int32_t kActorSlots = 17;
constexpr std::int32_t kIdleActorCount = 16;
constexpr std::uint32_t kChestControlMode = 9;
constexpr std::uint32_t kTrapControlMode = 8;
constexpr std::int32_t kFinishedActionFlow = 4;
constexpr std::int32_t kContentsActionFlow = 3;
constexpr std::int32_t kUnlockedNotification = 4;
constexpr std::int32_t kTrapNotification = 0xB;
constexpr std::int32_t kTrapActionOffset = 6;
constexpr std::int32_t kTrapTickSpeed = 4;
constexpr std::size_t kPoseWords = 4;
constexpr std::size_t kOriginalLockRecordBytes = 0x14;
constexpr std::int32_t kRoomObjectSoundGroup = 0x7E;
constexpr std::int32_t kTrapPrimaryTargetPosition = 3;
constexpr std::uint16_t kTrapSourcePointType = 5;
constexpr std::uint16_t kTrapActorTargetType = 4;

class LiveTrapActionHost final : public VS::Field::TrapActionHost {
 public:
  bool HasActionState(const VS::Field::TrapActionInput& input) const override {
    return Battle_actionState != nullptr && input.actor >= 0 &&
           input.actor < kActorSlots && Battle_actors[input.actor] != nullptr &&
           Battle_actors[input.actor]->stats != nullptr;
  }
  std::optional<std::int16_t> Kind(
      VS::Field::TrapTarget target) const override {
    const auto* record = VS::Field::ResolveLiveTrapTarget(target);
    return record ? std::optional<std::int16_t>{record->kind} : std::nullopt;
  }
  bool StageVictim(const VS::Field::TrapActionInput& input) override {
    if (!HasActionState(input)) {
      return false;
    }
    auto& action = *Battle_actionState;
    action.selection.primaryTarget.targetsPosition = kTrapPrimaryTargetPosition;
    action.selection.targetCount = 1;
    action.selection.targets[0].targetsPosition = 0;
    action.selection.targets[0].target.targetActor =
        static_cast<std::uint8_t>(input.actor);
    action.selection.targets[0].target.targetLimb =
        std::bit_cast<std::int8_t>(input.default_limb);
    action.chainStep = 1;
    action.queueIndex = 0;
    action.rangeOrigin.vx = input.position.x;
    action.rangeOrigin.vy = input.position.y;
    action.rangeOrigin.vz = input.position.z;
    return true;
  }
  bool ResolveAction(std::uint16_t action_id) override {
    if (Battle_actionState == nullptr) {
      return false;
    }
    auto& action = *Battle_actionState;
    Battle_ResolveActionTargets(action_id, &action.queue[0], &action.selection,
                                1);
    return true;
  }
  bool BeginEffect(const VS::Field::TrapEffectRequest& effect) override {
    if (!HasActionState({.actor = effect.actor})) {
      return false;
    }
    auto request = VS::Field::LiveTrapEffectRecord(effect);
    static_cast<void>(Effects_BeginSecondaryEffects(&request, 0));
    return true;
  }
};

class LiveRoomObjectInteractionHost final
    : public VS::Field::RoomObjectInteractionHost {
 public:
  explicit LiveRoomObjectInteractionHost(const void* address)
      : generation_(VS::Field::LiveInteractionTargetDomain().generation) {
    const auto records = Records();
    for (std::size_t index = 0; index < records.size(); ++index) {
      if (static_cast<const void*>(&records[index]) == address) {
        index_ = index;
        break;
      }
    }
  }
  std::optional<VS::Field::RoomObjectLockState> State() const override {
    const auto* record = Record();
    if (record == nullptr) {
      return std::nullopt;
    }
    return VS::Field::RoomObjectLockState{
        .tile_x = record->trigger.tileX,
        .tile_z = record->trigger.tileZ,
        .required_item = record->keyOrSigil,
        .lock_state =
            static_cast<std::uint8_t>(record->trigger.lockState.lock.state),
        .is_open = record->isOpen != 0,
        .is_triggered = record->isTriggered != 0};
  }
  bool HasItem(std::int16_t item) override {
    return VS::Items::CountLiveInventoryItem(item) != 0;
  }
  bool PreviousUnlockFlag(std::int32_t flag) override {
    return VS::State::LiveScriptFlag(flag) != 0;
  }
  void SetUnlockFlag(std::int32_t flag, std::int16_t value) override {
    VS::State::SetLiveScriptFlag(flag, value);
  }
  void ConsumeItem(std::int16_t item) override {
    VS::Items::ConsumeLiveMiscItem(item);
  }
  bool Unlock() override {
    auto* record = Record();
    if (record == nullptr) {
      return false;
    }
    record->trigger.lockState.lock.state = 0;
    return true;
  }
  void ShowNotification(std::int32_t kind, std::int16_t item) override {
    Menu_ShowSceneNotification(kind, item, 1);
  }
  void PlayPose(const VS::Field::RoomObjectLockState& state,
                std::uint8_t pose) override {
    const auto x = static_cast<std::uint16_t>(
        static_cast<std::int32_t>(state.tile_x) * 128 + 64);
    const auto z = static_cast<std::uint16_t>(
        static_cast<std::int32_t>(state.tile_z) * 128 + 64);
    std::array<std::int16_t, kPoseWords> position{
        std::bit_cast<std::int16_t>(x), 0, std::bit_cast<std::int16_t>(z), 0};
    VS::Actors::PlayLiveActorActionPose(0, position.data(), pose);
  }
  void BeginToggle() override {
    if (auto* record = Record()) {
      Field_BeginDoorToggle(record);
    }
  }
  void PlaySound(std::int32_t sound) override {
    VS::Audio::LivePlaySfxCentered(kRoomObjectSoundGroup, sound);
  }
  void RestoreIdle(std::int32_t actor) override {
    VS::Actors::RestoreLiveActorIdlePose(actor);
  }

 private:
  std::span<Field_MpdLockTrigger> Records() const {
    if (generation_ != VS::Field::LiveInteractionTargetDomain().generation ||
        Field_roomData.lockTriggers == nullptr) {
      return {};
    }
    return {
        Field_roomData.lockTriggers,
        Field_roomData.header.lockTriggersLength / kOriginalLockRecordBytes};
  }
  Field_MpdLockTrigger* Record() const {
    const auto records = Records();
    return index_ && *index_ < records.size() ? &records[*index_] : nullptr;
  }

  std::uint64_t generation_ = 0;
  std::optional<std::size_t> index_;
};

class LiveChestInteractionHost final : public VS::Field::ChestInteractionHost {
 public:
  std::optional<VS::Field::ChestLockState> Chest(
      VS::Field::ChestTarget target) const override {
    const auto* chest = VS::Field::ResolveLiveChestTarget(target);
    if (chest == nullptr) {
      return std::nullopt;
    }
    return VS::Field::ChestLockState{chest->tileX, chest->tileZ,
                                     chest->lockLevel, chest->reservedE,
                                     chest->openedFlag};
  }
  bool ScriptFlag(std::int32_t flag) override {
    return VS::State::LiveScriptFlag(flag);
  }
  bool HasItem(std::int16_t item) override {
    return VS::Items::CountLiveInventoryItem(item) != 0;
  }
  void SetScriptFlag(std::int32_t flag) override {
    VS::State::SetLiveScriptFlag(flag, 1);
  }
  void ConsumeItem(std::int16_t item) override {
    VS::Items::ConsumeLiveMiscItem(item);
  }
  bool Unlock(VS::Field::ChestTarget target) override {
    auto* chest = VS::Field::ResolveLiveChestTarget(target);
    if (chest == nullptr) {
      return false;
    }
    chest->lockLevel = 0;
    return true;
  }
  void ShowUnlocked(std::int16_t item) override {
    Menu_ShowSceneNotification(kUnlockedNotification, item, 1);
  }
  void PlayPose(const VS::Field::ChestLockState& chest,
                std::uint8_t pose) override {
    const auto x = static_cast<std::uint16_t>(
        static_cast<std::int32_t>(chest.tile_x) * 128 + 64);
    const auto z = static_cast<std::uint16_t>(
        static_cast<std::int32_t>(chest.tile_z) * 128 + 64);
    std::array<std::int16_t, kPoseWords> position{
        std::bit_cast<std::int16_t>(x), 0, std::bit_cast<std::int16_t>(z), 0};
    VS::Actors::PlayLiveActorActionPose(0, position.data(), pose);
  }
  bool StartAnimation(VS::Field::ChestTarget target) override {
    return VS::Field::BeginLiveChestSwitch(target);
  }
  bool SelectChest(VS::Field::ChestTarget target, std::int32_t actor) override {
    if (VS::Field::ResolveLiveChestTarget(target) == nullptr || actor < 0 ||
        actor >= kActorSlots) {
      return false;
    }
    VS::Field::LiveInteractionTargetState().Select(target);
    VS::Actors::LiveActorPlacement(actor, &Field_interactionFocus.actor_query);
    Field_interactionFocus.actor_query.actorIndex =
        static_cast<std::int16_t>(actor);
    return true;
  }
  std::optional<VS::Field::ChestTarget> SelectedChest() const override {
    const auto* target = VS::Field::LiveInteractionTargetState().Chest(
        VS::Field::LiveInteractionTargetDomain());
    return target ? std::optional{*target} : std::nullopt;
  }
  void BeginChestFlow() override {
    Field_controlMode = kChestControlMode;
    Battle_actionFlowMode = 1;
    VS::Field::LiveStorePreferredActionCameraDistance();
  }
  void FinishChestFlow() override {
    Battle_actionFlowMode = kFinishedActionFlow;
    State_tickSpeed = 2;
  }
  void RestoreIdlePoses() override {
    for (int actor = 0; actor < kIdleActorCount; ++actor) {
      VS::Actors::RestoreLiveActorIdlePose(actor);
    }
  }
  void ResetActionSelection() override {
    if (Battle_actionState != nullptr) {
      Battle_actionState->chainStep = 0;
      Battle_actionState->selection.actionId = 0;
    }
  }
  void ReleaseNotification() override { Menu_ReleaseSceneNotification(); }
  void ShowLocked(std::int32_t notification, std::int16_t item) override {
    Battle_actionFlowMode = kContentsActionFlow;
    Menu_ShowSceneNotification(notification, item, 1);
  }
  void OpenChestLoot(VS::Field::ChestTarget target) override {
    auto* chest = VS::Field::ResolveLiveChestTarget(target);
    if (chest != nullptr) {
      Battle_actionFlowMode = kContentsActionFlow;
      VS::Menu::Framework::OpenLiveLootEntryMenu(&chest->loot);
    }
  }
  void OpenChestStorage(std::int32_t storage) override {
    Battle_actionFlowMode = kContentsActionFlow;
    VS::Menu::Framework::OpenLiveChestStorageMenu(storage);
  }
  void PauseForDialog() override {
    VS::Field::LivePauseFieldForInteractionDialog();
  }
  void PauseForMenu() override { VS::Field::LivePauseFieldForMenu(); }
  bool NotificationPending() override {
    return Menu_IsSceneNotificationPending() != 0;
  }
  VS::Field::ChestSelectionResult UpdateContentsSelection() override {
    VS::Menu::Framework::UpdateLiveMenuOverlay();
    const auto* selection = VS::Menu::Framework::LiveActionSelectionState();
    if (selection == nullptr || selection->menuStatus == 1) {
      return VS::Field::ChestSelectionResult::kPending;
    }
    return selection->selectionKind != 0
               ? VS::Field::ChestSelectionResult::kAccepted
               : VS::Field::ChestSelectionResult::kCancelled;
  }
  void ReleaseContentsRows() override {
    VS::Menu::Framework::ReleaseLiveMenuRows();
  }
  void ResumeField() override { VS::Field::LiveResumeFieldAfterMenu(); }
};

class LiveInteractionDispatchHost final
    : public VS::Field::InteractionDispatchHost {
 public:
  VS::Field::InteractionTarget Target() const override {
    return VS::Field::LiveInteractionTargetState().Target();
  }
  bool TrapMode() const override {
    return Field_controlMode == kTrapControlMode;
  }
  void BeginDispatch() override { Battle_actionFlowMode = 2; }
  bool DispatchChest(VS::Field::ChestTarget target) override {
    return VS::Field::TryOpenLiveChest(target);
  }
  bool DispatchTrap(VS::Field::TrapTarget target) override {
    const auto* trap = VS::Field::ResolveLiveTrapTarget(target);
    const int actor = Field_interactionFocus.actor_query.actorIndex;
    if (trap == nullptr || actor < 0 || actor >= kActorSlots ||
        Battle_actionState == nullptr || Battle_actors[actor] == nullptr ||
        Battle_actors[actor]->stats == nullptr) {
      return false;
    }
    const auto& point = Field_interactionFocus.actor_query.location.position;
    State_tickSpeed = kTrapTickSpeed;
    return VS::Field::TriggerLiveActorTrap(
        target, {.x = point.vx, .y = point.vy, .z = point.vz}, actor);
  }
};

class LiveTrapFinishHost final : public VS::Field::TrapFinishHost {
 public:
  std::optional<VS::Field::TrapTarget> Target() const override {
    const auto* target = VS::Field::LiveInteractionTargetState().Trap(
        VS::Field::LiveInteractionTargetDomain());
    return target ? std::optional{*target} : std::nullopt;
  }
  bool DisableTrap(VS::Field::TrapTarget target) override {
    auto* trap = VS::Field::ResolveLiveTrapTarget(target);
    if (trap == nullptr) {
      return false;
    }
    Field_SetRoomTriggerEnabled(trap, 1);
    VS::Battle::Ai::MarkLiveAiTileDynamicObstacle(trap->tileX, trap->tileZ);
    return true;
  }
  void FinishFlow() override {
    Battle_actionFlowMode = kFinishedActionFlow;
    State_tickSpeed = 2;
  }
  void ReleaseNotification() override { Menu_ReleaseSceneNotification(); }
  void ClearTarget() override {
    VS::Field::LiveInteractionTargetState().Clear();
  }
};

class LiveTrapSelectionHost final : public VS::Field::TrapSelectionHost {
 public:
  explicit LiveTrapSelectionHost(Actors_ActorLocation* location)
      : location_(location) {}
  bool Valid(VS::Field::TrapTarget target, std::int32_t actor) const override {
    return location_ != nullptr && actor >= 0 && actor < kActorSlots &&
           VS::Field::ResolveLiveTrapTarget(target) != nullptr;
  }
  void BeginFlow() override {
    Field_controlMode = kTrapControlMode;
    Battle_actionFlowMode = 1;
    VS::Field::LiveStorePreferredActionCameraDistance();
  }
  void Select(VS::Field::TrapTarget target, std::int32_t actor) override {
    VS::Field::LiveInteractionTargetState().Select(target);
    Field_interactionFocus.actor_query.location = *location_;
    Field_interactionFocus.actor_query.actorIndex =
        static_cast<std::int16_t>(actor);
  }
  void RestoreIdle(std::int32_t actor) override {
    VS::Actors::RestoreLiveActorIdlePose(actor);
  }
  void ShowInformation(VS::Field::TrapTarget target) override {
    const auto* trap = VS::Field::ResolveLiveTrapTarget(target);
    if (trap != nullptr && Input_settings.information != 0) {
      Menu_ShowSceneNotification(kTrapNotification,
                                 trap->kind + kTrapActionOffset, 1);
    }
  }
  void ResetActionSelection() override {
    if (Battle_actionState != nullptr) {
      Battle_actionState->chainStep = 0;
      Battle_actionState->selection.actionId = 0;
    }
  }

 private:
  Actors_ActorLocation* location_ = nullptr;
};

class LiveObjectInteractionWaitHost final
    : public VS::Field::ObjectInteractionWaitHost {
 public:
  void SetControlMode(VS::Field::FieldControlMode mode) override {
    Field_controlMode = static_cast<std::uint32_t>(mode);
  }
  void PauseField() override {
    VS::Field::LivePauseFieldForObjectInteraction();
  }
  void ReleaseNotification() override { Menu_ReleaseSceneNotification(); }
  void ResumeField() override { VS::Field::LiveResumeFieldAfterMenu(); }
  void EnterExploration() override {
    VS::Field::LiveEnterFieldExplorationMode();
  }
};

}  // namespace

namespace VS::Field {
Effects_ActionEffectRequest LiveTrapEffectRecord(
    const TrapEffectRequest& effect) {
  Effects_ActionEffectRequest request{};
  request.effectId = effect.effect_id;
  request.count = 1;
  request.source.type = kTrapSourcePointType;
  if (effect.source_position) {
    request.source.value.position.vx = effect.source_position->x;
    request.source.value.position.vy = effect.source_position->y;
    request.source.value.position.vz = effect.source_position->z;
  }
  request.targets[0].type = kTrapActorTargetType;
  request.targets[0].value.actor.actor =
      static_cast<std::uint8_t>(effect.actor);
  request.targets[0].value.actor.part = effect.ground_anchor;
  return request;
}

bool TryOpenLiveChest(ChestTarget target) {
  LiveChestInteractionHost host;
  return TryOpenChest(target, host);
}
bool BeginLiveChestInteraction(ChestTarget target, std::int32_t actor) {
  LiveChestInteractionHost host;
  return BeginChestInteraction(target, actor, host);
}
bool TriggerLiveActorTrap(TrapTarget target, const TrapPosition& point,
                          std::int32_t actor) {
  LiveTrapActionHost host;
  TrapActionInput input{.actor = actor, .position = point};
  if (!host.HasActionState(input)) {
    return false;
  }
  input.default_limb =
      static_cast<std::uint8_t>(Battle_actors[actor]->stats->defaultTargetLimb);
  return TriggerActorTrap(target, input, host);
}
}  // namespace VS::Field

// decomp: BATTLE.PRG func_80073E30 (src/BATTLE/BATTLE.PRG/146C.c:4935).
void Field_TryOpenChestEntry(Field_MpdLockTrigger* address, int) {
  const auto target = VS::Field::FindLiveChestTargetAddress(address);
  if (target) {
    static_cast<void>(VS::Field::TryOpenLiveChest(*target));
  }
}

// decomp: BATTLE.PRG func_80074050 (src/BATTLE/BATTLE.PRG/146C.c:4993).
void Field_BeginChestInteractionEntry(Field_MpdLockTrigger* address,
                                      int actor) {
  const auto target = VS::Field::FindLiveChestTargetAddress(address);
  if (target) {
    static_cast<void>(VS::Field::BeginLiveChestInteraction(*target, actor));
  }
}

// decomp: BATTLE.PRG func_800742A4 (src/BATTLE/BATTLE.PRG/146C.c:5066).
void Field_FinishChestInteractionEntry() {
  LiveChestInteractionHost host;
  static_cast<void>(VS::Field::FinishChestInteraction(host));
}

// decomp: BATTLE.PRG func_800743E0 (src/BATTLE/BATTLE.PRG/146C.c:5107).
void Field_OpenChestContentsEntry() {
  LiveChestInteractionHost host;
  static_cast<void>(VS::Field::OpenChestContents(host));
}

// decomp: BATTLE.PRG func_800744B8 (src/BATTLE/BATTLE.PRG/146C.c:5139).
int Field_UpdateChestContentsSelectionEntry() {
  LiveChestInteractionHost host;
  return VS::Field::UpdateChestContentsSelection(host) ? 1 : 0;
}

// decomp: BATTLE.PRG func_800741D4 (src/BATTLE/BATTLE.PRG/146C.c:5044).
void Field_DispatchTrapOrChestInteractionEntry() {
  LiveInteractionDispatchHost host;
  static_cast<void>(VS::Field::DispatchTrapOrChestInteraction(host));
}

// decomp: BATTLE.PRG func_80073F7C (src/BATTLE/BATTLE.PRG/146C.c:4973).
void Field_BeginTrapInteractionEntry(Field_MpdLockTrigger* address,
                                     Actors_ActorLocation* victim_location,
                                     int actor) {
  const auto target = VS::Field::FindLiveTrapTargetAddress(address);
  if (target) {
    LiveTrapSelectionHost host(victim_location);
    static_cast<void>(VS::Field::BeginTrapInteraction(*target, actor, host));
  }
}

// decomp: BATTLE.PRG func_80074314 (src/BATTLE/BATTLE.PRG/146C.c:5080).
void Field_FinishTrapInteractionEntry() {
  LiveTrapFinishHost host;
  static_cast<void>(VS::Field::FinishTrapInteraction(host));
}

// decomp: BATTLE.PRG func_80073AA4 (src/BATTLE/BATTLE.PRG/146C.c:4823).
void Field_BeginObjectInteractionWaitEntry() {
  LiveObjectInteractionWaitHost host;
  VS::Field::BeginObjectInteractionWait(host);
}

// decomp: BATTLE.PRG func_80073ACC (src/BATTLE/BATTLE.PRG/146C.c:4829).
void Field_EndObjectInteractionWaitEntry() {
  LiveObjectInteractionWaitHost host;
  VS::Field::EndObjectInteractionWait(host);
}

// decomp: BATTLE.PRG func_80073AFC (src/BATTLE/BATTLE.PRG/146C.c:4836).
int Field_InteractWithLockedRoomObjectEntry(Field_MpdLockTrigger* address) {
  LiveRoomObjectInteractionHost host(address);
  return VS::Field::InteractWithLockedRoomObject(host) ==
                 VS::Field::RoomObjectInteractionResult::kWait
             ? 0
             : 1;
}

// decomp: BATTLE.PRG func_80073D30 (src/BATTLE/BATTLE.PRG/146C.c:4909).
void Field_TriggerActorTrapEntry(Field_MpdLockTrigger* address,
                                 Actors_ActorLocationQuery* location,
                                 int actor) {
  const auto target = VS::Field::FindLiveTrapTargetAddress(address);
  if (target && location != nullptr) {
    const auto& point = location->location.position;
    static_cast<void>(VS::Field::TriggerLiveActorTrap(
        *target, {.x = point.vx, .y = point.vy, .z = point.vz}, actor));
  }
}
