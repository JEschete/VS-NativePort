#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Boot/GameplayLoop.hpp"
#include "Input/Pad.hpp"

namespace {
using VS::Boot::BattleActionFlow;
using VS::Boot::FieldControlMode;
using VS::Boot::GameplayLoopState;
using VS::Boot::InteractionFlow;
using VS::Boot::RoomActorOutcome;
using VS::Boot::ScreenTransitionStep;
using VS::Input::Button;
namespace actor_state_flag = VS::Boot::actor_state_flag;

constexpr int kOpaque = 255;
constexpr int kAttractDemo = 3;
constexpr int kAttractDemoAlt = 4;
constexpr int kNormalEntry = 0;
constexpr int kDoubleSpeedTicks = 4;
constexpr int kEventActor = 3;
constexpr int kPresentActor = 2;
constexpr int kRecoveryFrames = 5;

constexpr std::int32_t Flow(InteractionFlow flow) {
  return static_cast<std::int32_t>(flow);
}
constexpr std::int32_t Flow(BattleActionFlow flow) {
  return static_cast<std::int32_t>(flow);
}

struct LoopExit {};

// Records every host effect in order; queries return configurable values.
// State-changing effects update the matching query so the core's re-reads
// observe them, as they would observe the live globals.
class RecordingLoopHost final : public VS::Boot::GameplayLoopHost {
 public:
  std::vector<std::string> calls;

  // Query results.
  int entry_mode = kNormalEntry;
  FieldControlMode control_mode = FieldControlMode::kExploration;
  std::int32_t action_flow = 0;
  ScreenTransitionStep transition_step = ScreenTransitionStep::kIdle;
  int transition_alpha = 0;
  bool menu_queued = false;
  std::uint32_t pressed = 0;
  std::uint32_t previous = 0;
  std::array<std::uint32_t, VS::Boot::kBattleActorSlotCount> actor_flags{};
  std::array<bool, VS::Boot::kBattleActorSlotCount> actor_present{};
  std::int32_t interaction_flags = 0;
  int recovery_timer = 0;
  std::uint32_t ashley_statuses = 0;
  bool weapon_drawn = false;
  int yaw_delta = 0;
  int delta_mode = 0;
  std::uint32_t effect_busy = 0;
  bool rendering_enabled = true;
  int tick_speed = 2;

  // Effect results.
  bool poll_result = true;
  std::int32_t field_trigger_result = 0;
  std::int32_t playback_status = 1;
  bool actor_trigger_begins = false;
  bool tile_trigger_moved = false;
  bool facing_tile_event = false;
  RoomActorOutcome queued_outcome = RoomActorOutcome::kNone;
  RoomActorOutcome vitals_outcome = RoomActorOutcome::kNone;
  int gimmick_ahead = -1;
  bool gimmicks_moving = false;
  int nearest_actor = -1;
  bool facing_room_object = false;
  bool room_object_declined = false;
  bool facing_chest = false;
  bool scene_notification_pending = false;
  bool shortcut_opens = true;
  bool game_menu_opens = true;
  int action_menu_status = 0;
  bool camera_returned = true;
  bool look_at_on_ashley = false;
  bool trap_focus_pending = false;
  bool chest_focus_pending = false;
  bool action_effect_busy = false;
  bool chest_ready = false;
  bool chest_selecting = false;
  bool can_start_effect = false;
  bool animation_busy = false;
  // When set, RunBattleActionFlow leaves this flow behind.
  std::int32_t flow_after_battle_step = -1;

  // Room and zone scripting: each room runs frames_per_room frames and then
  // leaves with the next step in room_exits.
  int frames_per_room = 1;
  std::vector<ScreenTransitionStep> room_exits;
  std::size_t room_index = 0;
  int room_frames = 0;
  int zones_before_exit = -1;
  int zones_released = 0;
  bool room_load_succeeds = true;

  [[nodiscard]] std::ptrdiff_t Count(const std::string& call) const {
    return std::ranges::count(calls, call);
  }
  [[nodiscard]] bool Called(const std::string& call) const {
    return Count(call) != 0;
  }

  void InitializeGameplayOverlay() override { calls.emplace_back("init"); }
  void FinishPendingZoneLoad() override { calls.emplace_back("zone-load"); }
  bool FinishPendingRoomLoad() override {
    calls.emplace_back("room-load");
    transition_step = ScreenTransitionStep::kIdle;
    transition_alpha = 0;
    room_frames = 0;
    return room_load_succeeds;
  }
  void UnloadCurrentRoom() override {
    calls.emplace_back("room-unload");
    ++room_index;
  }
  void ReleaseZoneData() override {
    calls.emplace_back("zone-release");
    ++zones_released;
    if (zones_released == zones_before_exit) {
      throw LoopExit{};
    }
  }
  void ResetToTitle() override { calls.emplace_back("reset-title"); }

  [[nodiscard]] int GameEntryMode() const override { return entry_mode; }
  [[nodiscard]] FieldControlMode ControlMode() const override {
    return control_mode;
  }
  void SetControlMode(FieldControlMode mode) override {
    calls.emplace_back("control-mode:" +
                       std::to_string(static_cast<int>(mode)));
    control_mode = mode;
  }
  [[nodiscard]] std::int32_t ActionFlowMode() const override {
    return action_flow;
  }
  [[nodiscard]] ScreenTransitionStep TransitionStep() const override {
    return transition_step;
  }
  [[nodiscard]] int TransitionAlpha() const override {
    return transition_alpha;
  }
  [[nodiscard]] bool IsMenuRequestQueued() const override {
    return menu_queued;
  }
  void SetMenuRequestQueued(bool queued) override {
    calls.emplace_back(queued ? "menu-queued:1" : "menu-queued:0");
    menu_queued = queued;
  }
  [[nodiscard]] std::uint32_t ButtonsPressed() const override {
    return pressed;
  }
  [[nodiscard]] std::uint32_t PreviousButtons() const override {
    return previous;
  }
  [[nodiscard]] std::uint32_t ActorStateFlags(int slot) override {
    return actor_flags.at(static_cast<std::size_t>(slot));
  }
  [[nodiscard]] bool HasBattleActor(int slot) const override {
    return actor_present.at(static_cast<std::size_t>(slot));
  }
  [[nodiscard]] std::int32_t ActorInteractionFlags(
      int /*slot*/) const override {
    return interaction_flags;
  }
  [[nodiscard]] int AshleyActionRecoveryTimer() const override {
    return recovery_timer;
  }
  [[nodiscard]] std::uint32_t AshleyStatuses() const override {
    return ashley_statuses;
  }
  [[nodiscard]] bool IsAshleyWeaponDrawn() const override {
    return weapon_drawn;
  }
  [[nodiscard]] int CameraYawDelta() const override { return yaw_delta; }
  [[nodiscard]] int CameraDeltaMode() const override { return delta_mode; }
  [[nodiscard]] std::uint32_t EffectBusyFlags() const override {
    return effect_busy;
  }
  [[nodiscard]] bool IsFieldRenderingEnabled() const override {
    return rendering_enabled;
  }
  [[nodiscard]] int TickSpeed() const override { return tick_speed; }

  bool PollControllers() override {
    calls.emplace_back("poll");
    return poll_result;
  }
  void PollEncounterStop() override { calls.emplace_back("encounter-stop"); }
  void BeginFrame() override { calls.emplace_back("begin-frame"); }
  void UpdateMovingPlatforms() override { calls.emplace_back("platforms"); }
  void CaptureAshleyPlacement() override { calls.emplace_back("placement"); }

  std::int32_t EvaluateRoomEventTriggers(std::int32_t request) override {
    calls.emplace_back("room-trigger:" + std::to_string(request));
    return request == VS::Boot::kFieldEventTriggerRequest ? field_trigger_result
                                                          : playback_status;
  }
  bool EvaluateActorRoomEventTrigger(int slot) override {
    calls.emplace_back("actor-trigger:" + std::to_string(slot));
    return actor_trigger_begins;
  }
  bool CheckAshleyTileTriggers() override {
    calls.emplace_back("tile-triggers");
    return tile_trigger_moved;
  }
  bool TryTriggerFacingTileEvent(int /*slot*/) override {
    calls.emplace_back("tile-event");
    return facing_tile_event;
  }
  void BeginQueuedCutscene() override { calls.emplace_back("begin-cutscene"); }
  void StartPreparedCutscene() override {
    calls.emplace_back("start-cutscene");
  }
  void EndFieldCutscene() override { calls.emplace_back("end-cutscene"); }
  void UpdateEventPlayback() override { calls.emplace_back("event-playback"); }

  void UpdateAshleyMovementInput() override {
    calls.emplace_back("movement-input");
  }
  RoomActorOutcome ExecuteQueuedActorCommands() override {
    calls.emplace_back("queued-commands");
    return queued_outcome;
  }
  RoomActorOutcome UpdateRoomActorVitals() override {
    calls.emplace_back("vitals");
    return vitals_outcome;
  }
  void SetRecoveryRewardMode(int recovery_reward_mode) override {
    calls.emplace_back("reward-mode:" + std::to_string(recovery_reward_mode));
  }
  void OpenPendingEnemyLoot() override { calls.emplace_back("enemy-loot"); }
  void EnterFieldExplorationMode() override {
    calls.emplace_back("enter-exploration");
  }
  int FindInteractableGimmickAhead(int /*slot*/) override {
    calls.emplace_back("find-gimmick");
    return gimmick_ahead;
  }
  void BeginGimmickInteraction(int slot) override {
    calls.emplace_back("gimmick:" + std::to_string(slot));
  }
  [[nodiscard]] bool AreGimmicksMoving() override { return gimmicks_moving; }
  int FindNearestActorAhead(int /*slot*/, int horizontal_reach,
                            int vertical_reach) override {
    calls.emplace_back("find-actor:" + std::to_string(horizontal_reach) + ":" +
                       std::to_string(vertical_reach));
    return nearest_actor;
  }
  bool FindFacingRoomObject(int /*slot*/) override {
    calls.emplace_back("find-room-object");
    return facing_room_object;
  }
  bool InteractWithFacingRoomObject() override {
    calls.emplace_back("room-object");
    return room_object_declined;
  }
  bool FindFacingUnlootedChest(int /*slot*/) override {
    calls.emplace_back("find-chest");
    return facing_chest;
  }
  void BeginFacingChestInteraction(int slot) override {
    calls.emplace_back("chest:" + std::to_string(slot));
  }
  void BeginObjectInteractionWait() override {
    calls.emplace_back("object-wait");
  }
  void EndObjectInteractionWait() override {
    calls.emplace_back("end-object-wait");
  }
  [[nodiscard]] bool IsSceneNotificationPending() override {
    return scene_notification_pending;
  }
  void ReleaseSceneNotification() override {
    calls.emplace_back("release-notification");
  }
  void PlayCenteredSound(int channel_group, int sound) override {
    calls.emplace_back("sound:" + std::to_string(channel_group) + ":" +
                       std::to_string(sound));
  }

  void BeginShortcutMenu() override { calls.emplace_back("shortcut-menu"); }
  void EndShortcutMenu() override { calls.emplace_back("end-shortcut-menu"); }
  void ShowShortcutHud() override { calls.emplace_back("shortcut-hud"); }
  bool OpenShortcutMenu(int shortcut) override {
    calls.emplace_back("shortcut:" + std::to_string(shortcut));
    return shortcut_opens;
  }
  bool OpenGameMenu() override {
    calls.emplace_back("game-menu");
    return game_menu_opens;
  }
  void BeginBattleActionMenu() override { calls.emplace_back("action-menu"); }
  void UpdateMenuOverlay() override { calls.emplace_back("menu-overlay"); }
  int SelectActionMenu() override {
    calls.emplace_back("select-menu");
    return action_menu_status;
  }
  void DispatchActionMenuResult() override {
    calls.emplace_back("menu-result");
  }

  void TrackActionCameraTarget(int slot) override {
    calls.emplace_back("track-target:" + std::to_string(slot));
  }
  void UpdateCameraOrbitInput() override { calls.emplace_back("orbit-input"); }
  void UpdateRoomActorMovement() override {
    calls.emplace_back("actor-movement");
  }
  void SetCameraPositionFromAngles() override {
    calls.emplace_back("camera-angles");
  }
  void UpdateAshleyFollowCamera() override {
    calls.emplace_back("follow-camera");
  }
  void BuildCameraView() override { calls.emplace_back("camera-view"); }
  void BeginFirstPersonView() override { calls.emplace_back("first-person"); }
  void UpdateFirstPersonViewEntry() override {
    calls.emplace_back("first-person-entry");
  }
  void CheckFirstPersonViewExit() override {
    calls.emplace_back("first-person-check");
  }
  void UpdateFirstPersonCamera() override {
    calls.emplace_back("first-person-camera");
  }
  void UpdateFirstPersonViewExit() override {
    calls.emplace_back("first-person-exit");
  }
  bool TrackInteractionCameraToAshley() override {
    calls.emplace_back("camera-return");
    return camera_returned;
  }
  [[nodiscard]] bool IsCameraLookAtOnAshley() override {
    return look_at_on_ashley;
  }

  bool StepTrapCameraFocus() override {
    calls.emplace_back("trap-focus");
    return trap_focus_pending;
  }
  bool WaitForChestCameraFocus() override {
    calls.emplace_back("chest-focus");
    return chest_focus_pending;
  }
  void DispatchTrapOrChestInteraction() override {
    calls.emplace_back("dispatch-interaction");
  }
  [[nodiscard]] bool IsActionEffectBusy() override {
    return action_effect_busy;
  }
  void FinishTrapInteraction() override { calls.emplace_back("finish-trap"); }
  [[nodiscard]] bool IsChestAnimationReady() override { return chest_ready; }
  void OpenChestContents() override { calls.emplace_back("chest-contents"); }
  bool UpdateChestContentsSelection() override {
    calls.emplace_back("chest-selection");
    return chest_selecting;
  }
  void FinishChestInteraction() override { calls.emplace_back("finish-chest"); }
  void RebuildActorActionPlans() override {
    calls.emplace_back("rebuild-plans");
  }
  void BeginPostActionRecovery(int recovery_reward_mode) override {
    calls.emplace_back("recovery:" + std::to_string(recovery_reward_mode));
  }

  [[nodiscard]] bool CanStartEffectTrack() override { return can_start_effect; }
  void StartEffectTrack() override { calls.emplace_back("effect-track"); }
  void EndAshleyStandaloneEffect() override {
    calls.emplace_back("end-standalone-effect");
  }
  void BeginActorActionSelection(int slot, int action) override {
    calls.emplace_back("select-action:" + std::to_string(slot) + ":" +
                       std::to_string(action));
    control_mode = FieldControlMode::kBattleAction;
  }
  void BeginAshleyWeaponToggle() override {
    calls.emplace_back("weapon-toggle");
  }
  void StopAshleyDirectMovement() override { calls.emplace_back("stop-move"); }
  [[nodiscard]] bool IsAshleyActionAnimationBusy() override {
    return animation_busy;
  }
  void EndAshleyWeaponToggle() override {
    calls.emplace_back("end-weapon-toggle");
  }
  void UpdateGimImages() override { calls.emplace_back("gim-images"); }

  void UpdateTargetingRiskPenalty() override {
    calls.emplace_back("risk-penalty");
  }
  void RunBattleActionFlow(BattleActionFlow flow) override {
    calls.emplace_back("flow:" + std::to_string(static_cast<int>(flow)));
    if (flow_after_battle_step >= 0) {
      action_flow = flow_after_battle_step;
    }
  }
  void DrawSphere() override { calls.emplace_back("sphere"); }

  void DrawRoomScenery() override { calls.emplace_back("scenery"); }
  void DrawRoomActors() override { calls.emplace_back("actors"); }
  void UpdateEffectRuntime() override { calls.emplace_back("effects"); }
  void DrawBattleHud() override { calls.emplace_back("hud"); }
  void UpdateTextBoxes() override { calls.emplace_back("text-boxes"); }
  void UpdateTickTransitions() override { calls.emplace_back("tick"); }
  void DrawScreenOverlays() override { calls.emplace_back("overlays"); }
  void RunActorAiUpdateStages() override { calls.emplace_back("ai-stages"); }
  void UpdateActorNavigationOverrides() override {
    calls.emplace_back("ai-navigation");
  }
  void PresentFrame() override {
    calls.emplace_back("present");
    ++room_frames;
    if (room_frames == frames_per_room && room_index < room_exits.size()) {
      transition_step = room_exits[room_index];
      transition_alpha = kOpaque;
    }
  }
};

bool RunFrame(RecordingLoopHost& host) {
  GameplayLoopState state;
  return VS::Boot::RunGameplayFrame(state, host);
}

// The calls made after the frame-start sequence and before the event update.
std::vector<std::string> DispatchCalls(const RecordingLoopHost& host) {
  const auto begin = std::ranges::find(host.calls, "placement");
  const auto end = std::ranges::find(host.calls, "event-playback");
  if (begin == host.calls.end() || end == host.calls.end()) {
    return {};
  }
  return {begin + 1, end};
}
}  // namespace

TEST_CASE("Gameplay loop skips the frame when the controller poll fails",
          "[boot-gameplay-loop]") {
  RecordingLoopHost host;
  host.poll_result = false;
  CHECK_FALSE(RunFrame(host));
  CHECK(host.calls == std::vector<std::string>{"poll"});
}

TEST_CASE("Gameplay loop runs an idle exploration frame in original order",
          "[boot-gameplay-loop]") {
  RecordingLoopHost host;
  host.tick_speed = kDoubleSpeedTicks;
  CHECK(RunFrame(host));
  const std::vector<std::string> expected{"poll",
                                          "encounter-stop",
                                          "begin-frame",
                                          "platforms",
                                          "placement",
                                          "room-trigger:32",
                                          "tile-triggers",
                                          "movement-input",
                                          "queued-commands",
                                          "event-playback",
                                          "follow-camera",
                                          "camera-view",
                                          "vitals",
                                          "scenery",
                                          "actors",
                                          "effects",
                                          "hud",
                                          "text-boxes",
                                          "tick",
                                          "tick",
                                          "overlays",
                                          "ai-stages",
                                          "present"};
  CHECK(host.calls == expected);
}

TEST_CASE("Gameplay loop draws without the room when rendering is disabled",
          "[boot-gameplay-loop]") {
  RecordingLoopHost host;
  host.rendering_enabled = false;
  host.control_mode = FieldControlMode::kCutscene;
  host.action_flow = 0;
  CHECK(RunFrame(host));
  CHECK_FALSE(host.Called("scenery"));
  CHECK_FALSE(host.Called("actors"));
  CHECK(host.Count("tick") == 1);
  CHECK(host.Called("ai-navigation"));
  CHECK_FALSE(host.Called("ai-stages"));
}

TEST_CASE("Gameplay loop stops exploration while Ashley is animation locked",
          "[boot-gameplay-loop]") {
  RecordingLoopHost host;
  host.actor_flags[0] = actor_state_flag::kAnimationLocked;
  host.pressed = Button::Triangle | Button::Cross | Button::Circle;
  CHECK(RunFrame(host));
  CHECK(DispatchCalls(host).empty());
}

TEST_CASE("Gameplay loop gives room events and tile triggers priority",
          "[boot-gameplay-loop]") {
  RecordingLoopHost host;
  host.pressed = Button::Triangle;

  SECTION("a field event begins a cutscene") {
    host.field_trigger_result = VS::Boot::kEventTriggerBegins;
    CHECK(RunFrame(host));
    CHECK(DispatchCalls(host) ==
          std::vector<std::string>{"room-trigger:32", "begin-cutscene"});
  }
  SECTION("a tile trigger ends the dispatch") {
    host.tile_trigger_moved = true;
    CHECK(RunFrame(host));
    CHECK(DispatchCalls(host) ==
          std::vector<std::string>{"room-trigger:32", "tile-triggers"});
  }
}

TEST_CASE(
    "Gameplay loop opens menus only when Ashley is ready: held L2 opens the "
    "shortcut menu",
    "[boot-gameplay-loop]") {
  RecordingLoopHost host;
  host.previous = Button::L2;
  host.pressed = Button::Triangle;
  CHECK(RunFrame(host));
  CHECK(DispatchCalls(host) == std::vector<std::string>{"room-trigger:32",
                                                        "tile-triggers",
                                                        "shortcut-menu"});
}

TEST_CASE(
    "Gameplay loop opens menus only when Ashley is ready: Triangle while "
    "recovering queues the menu for later",
    "[boot-gameplay-loop]") {
  RecordingLoopHost host;
  host.pressed = Button::Triangle;
  host.recovery_timer = kRecoveryFrames;
  CHECK(RunFrame(host));
  CHECK(host.menu_queued);
  CHECK_FALSE(host.Called("game-menu"));

  host.calls.clear();
  host.pressed = 0;
  host.recovery_timer = 0;
  CHECK(RunFrame(host));
  CHECK(DispatchCalls(host) ==
        std::vector<std::string>{"room-trigger:32", "tile-triggers",
                                 "game-menu", "action-menu", "menu-queued:0"});
}

TEST_CASE(
    "Gameplay loop opens menus only when Ashley is ready: a queued menu does "
    "not open the shortcut menu",
    "[boot-gameplay-loop]") {
  RecordingLoopHost host;
  host.menu_queued = true;
  host.previous = Button::L2;
  host.game_menu_opens = false;
  CHECK(RunFrame(host));
  CHECK_FALSE(host.Called("shortcut-menu"));
  CHECK(host.Called("game-menu"));
  CHECK(host.menu_queued);
}

TEST_CASE(
    "Gameplay loop opens menus only when Ashley is ready: a moving Ashley "
    "refuses the shortcut menu",
    "[boot-gameplay-loop]") {
  RecordingLoopHost host;
  host.previous = Button::L2;
  host.actor_flags[0] = actor_state_flag::kMoving;
  CHECK(RunFrame(host));
  CHECK_FALSE(host.Called("shortcut-menu"));
}

TEST_CASE(
    "Gameplay loop resolves Cross against what Ashley faces: a gimmick ahead "
    "takes precedence while the weapon is sheathed",
    "[boot-gameplay-loop]") {
  RecordingLoopHost host;
  host.pressed = Button::Cross;
  host.gimmick_ahead = 0;
  CHECK(RunFrame(host));
  CHECK(host.Called("gimmick:0"));
  CHECK_FALSE(host.Called("find-actor:160:128"));
}

TEST_CASE(
    "Gameplay loop resolves Cross against what Ashley faces: a gimmick ahead "
    "with the weapon drawn sheathes the weapon",
    "[boot-gameplay-loop]") {
  RecordingLoopHost host;
  host.pressed = Button::Cross;
  host.gimmick_ahead = 0;
  host.weapon_drawn = true;
  CHECK(RunFrame(host));
  CHECK_FALSE(host.Called("gimmick:0"));
  CHECK(host.Called("weapon-toggle"));
}

TEST_CASE(
    "Gameplay loop resolves Cross against what Ashley faces: an event actor "
    "within reach begins its cutscene",
    "[boot-gameplay-loop]") {
  RecordingLoopHost host;
  host.pressed = Button::Cross;
  host.nearest_actor = kEventActor;
  host.interaction_flags = VS::Boot::kEventInteractionActor;
  host.actor_trigger_begins = true;
  CHECK(RunFrame(host));
  const std::vector<std::string> expected{
      "room-trigger:32",    "tile-triggers",   "find-gimmick",
      "find-actor:160:128", "actor-trigger:3", "begin-cutscene"};
  CHECK(DispatchCalls(host) == expected);
}

TEST_CASE(
    "Gameplay loop resolves Cross against what Ashley faces: an actor without "
    "an interaction event falls through to tiles",
    "[boot-gameplay-loop]") {
  RecordingLoopHost host;
  host.pressed = Button::Cross;
  host.nearest_actor = kEventActor;
  host.facing_tile_event = true;
  CHECK(RunFrame(host));
  CHECK_FALSE(host.Called("actor-trigger:3"));
  CHECK(host.Called("tile-event"));
  CHECK(host.Called("begin-cutscene"));
}

TEST_CASE(
    "Gameplay loop resolves Cross against what Ashley faces: a room object "
    "starts the interaction wait",
    "[boot-gameplay-loop]") {
  RecordingLoopHost host;
  host.pressed = Button::Cross;
  host.facing_room_object = true;
  CHECK(RunFrame(host));
  CHECK(host.Called("room-object"));
  CHECK(host.Called("object-wait"));
  CHECK_FALSE(host.Called("find-chest"));
}

TEST_CASE(
    "Gameplay loop resolves Cross against what Ashley faces: a declined room "
    "object falls through to stance and movement",
    "[boot-gameplay-loop]") {
  RecordingLoopHost host;
  host.pressed = Button::Cross;
  host.facing_room_object = true;
  host.room_object_declined = true;
  CHECK(RunFrame(host));
  CHECK_FALSE(host.Called("object-wait"));
  CHECK_FALSE(host.Called("find-chest"));
  CHECK(host.Called("movement-input"));
}

TEST_CASE(
    "Gameplay loop resolves Cross against what Ashley faces: an unlooted chest "
    "begins the chest interaction",
    "[boot-gameplay-loop]") {
  RecordingLoopHost host;
  host.pressed = Button::Cross;
  host.facing_chest = true;
  CHECK(RunFrame(host));
  CHECK(host.Called("chest:0"));
  CHECK_FALSE(host.Called("movement-input"));
}

TEST_CASE(
    "Gameplay loop resolves Cross against what Ashley faces: movement refuses "
    "actor, tile and object interactions",
    "[boot-gameplay-loop]") {
  RecordingLoopHost host;
  host.pressed = Button::Cross;
  host.actor_flags[0] = actor_state_flag::kLocomotion;
  CHECK(RunFrame(host));
  CHECK(host.Called("find-gimmick"));
  CHECK_FALSE(host.Called("find-actor:160:128"));
  CHECK_FALSE(host.Called("find-chest"));
}

TEST_CASE(
    "Gameplay loop handles stance commands while standing: R3 enters "
    "first-person view",
    "[boot-gameplay-loop]") {
  RecordingLoopHost host;
  host.pressed = Button::R3;
  CHECK(RunFrame(host));
  CHECK(host.Called("first-person"));
}

TEST_CASE(
    "Gameplay loop handles stance commands while standing: a posed Ashley "
    "cannot enter first-person view or draw",
    "[boot-gameplay-loop]") {
  RecordingLoopHost host;
  host.pressed = Button::Start | Button::Circle;
  host.actor_flags[0] = actor_state_flag::kActionPose;
  CHECK(RunFrame(host));
  CHECK_FALSE(host.Called("first-person"));
  CHECK_FALSE(host.Called("weapon-toggle"));
  CHECK(host.Called("movement-input"));
}

TEST_CASE(
    "Gameplay loop handles stance commands while standing: Circle with the "
    "weapon sheathed draws it",
    "[boot-gameplay-loop]") {
  RecordingLoopHost host;
  host.pressed = Button::Circle;
  CHECK(RunFrame(host));
  CHECK(host.Called("weapon-toggle"));
}

TEST_CASE(
    "Gameplay loop handles stance commands while standing: paralysis refuses "
    "the attack with the refusal sound",
    "[boot-gameplay-loop]") {
  RecordingLoopHost host;
  host.pressed = Button::Circle;
  host.weapon_drawn = true;
  host.ashley_statuses = VS::Boot::kParalysisStatus;
  CHECK(RunFrame(host));
  CHECK(host.Called("sound:126:12"));
  CHECK_FALSE(host.Called("select-action:0:1"));
  CHECK(host.Called("movement-input"));
}

TEST_CASE("Gameplay loop attack start skips the camera-dependent update",
          "[boot-gameplay-loop]") {
  RecordingLoopHost host;
  GameplayLoopState state;
  host.pressed = Button::Circle;
  host.weapon_drawn = true;
  host.vitals_outcome = RoomActorOutcome::kOpenEnemyLoot;

  CHECK(VS::Boot::RunGameplayFrame(state, host));
  CHECK(state.skip_camera_update);
  CHECK(DispatchCalls(host) ==
        std::vector<std::string>{"room-trigger:32", "tile-triggers",
                                 "sound:126:9", "select-action:0:1"});
  CHECK(host.Called("camera-angles"));
  CHECK_FALSE(host.Called("risk-penalty"));
  CHECK_FALSE(host.Called("vitals"));

  host.calls.clear();
  host.pressed = 0;
  CHECK(VS::Boot::RunGameplayFrame(state, host));
  CHECK_FALSE(state.skip_camera_update);
  CHECK(host.Called("risk-penalty"));
}

TEST_CASE(
    "Gameplay loop keeps the camera skip across transition frames: a fresh "
    "loop state does not skip",
    "[boot-gameplay-loop][issue-286]") {
  RecordingLoopHost host;
  host.control_mode = FieldControlMode::kBattleAction;
  host.transition_step = ScreenTransitionStep::kRoomExit;
  GameplayLoopState state;
  CHECK_FALSE(state.skip_camera_update);
  CHECK(VS::Boot::RunGameplayFrame(state, host));
  CHECK(host.Called("risk-penalty"));
}

TEST_CASE(
    "Gameplay loop keeps the camera skip across transition frames: a "
    "transition frame neither dispatches nor clears the skip",
    "[boot-gameplay-loop][issue-286]") {
  RecordingLoopHost host;
  host.control_mode = FieldControlMode::kBattleAction;
  host.transition_step = ScreenTransitionStep::kRoomExit;
  GameplayLoopState state;
  state.skip_camera_update = true;
  CHECK(VS::Boot::RunGameplayFrame(state, host));
  CHECK(state.skip_camera_update);
  CHECK_FALSE(host.Called("track-target:0"));
  CHECK_FALSE(host.Called("risk-penalty"));
}

TEST_CASE(
    "Gameplay loop keeps the camera skip across transition frames: the first "
    "dispatching frame clears it",
    "[boot-gameplay-loop][issue-286]") {
  RecordingLoopHost host;
  host.control_mode = FieldControlMode::kBattleAction;
  host.transition_step = ScreenTransitionStep::kRoomExit;
  GameplayLoopState state;
  state.skip_camera_update = true;
  host.transition_step = ScreenTransitionStep::kIdle;
  CHECK(VS::Boot::RunGameplayFrame(state, host));
  CHECK_FALSE(state.skip_camera_update);
  CHECK(host.Called("track-target:0"));
  CHECK(host.Called("risk-penalty"));
}

TEST_CASE("Gameplay loop transition frames keep only movement and images",
          "[boot-gameplay-loop]") {
  RecordingLoopHost host;
  host.transition_step = ScreenTransitionStep::kRoomEntryReveal;
  host.pressed = Button::Triangle;

  SECTION("exploration only continues movement") {
    CHECK(RunFrame(host));
    CHECK(DispatchCalls(host) == std::vector<std::string>{"movement-input"});
    CHECK_FALSE(host.Called("vitals"));
  }
  SECTION("an animation lock stops that movement") {
    host.actor_flags[0] = actor_state_flag::kAnimationLocked;
    CHECK(RunFrame(host));
    CHECK(DispatchCalls(host).empty());
  }
  SECTION("a cutscene keeps its images updating") {
    host.control_mode = FieldControlMode::kCutscene;
    CHECK(RunFrame(host));
    CHECK(DispatchCalls(host) == std::vector<std::string>{"gim-images"});
  }
}

TEST_CASE(
    "Gameplay loop attract demos return to title on a button press: Start "
    "cancels entry 3 after the frame begins",
    "[boot-gameplay-loop]") {
  RecordingLoopHost host;
  host.control_mode = FieldControlMode::kNone;
  host.entry_mode = kAttractDemo;
  host.pressed = Button::Start;
  CHECK(RunFrame(host));
  REQUIRE(host.calls.size() > 4);
  CHECK(host.calls[2] == "begin-frame");
  CHECK(host.calls[3] == "reset-title");
}

TEST_CASE(
    "Gameplay loop attract demos return to title on a button press: Square "
    "cancels entry 4",
    "[boot-gameplay-loop]") {
  RecordingLoopHost host;
  host.control_mode = FieldControlMode::kNone;
  host.entry_mode = kAttractDemoAlt;
  host.pressed = Button::Square;
  CHECK(RunFrame(host));
  CHECK(host.Called("reset-title"));
}

TEST_CASE(
    "Gameplay loop attract demos return to title on a button press: the d-pad "
    "does not cancel a demo",
    "[boot-gameplay-loop]") {
  RecordingLoopHost host;
  host.control_mode = FieldControlMode::kNone;
  host.entry_mode = kAttractDemo;
  host.pressed = Button::Up | Button::Left;
  CHECK(RunFrame(host));
  CHECK_FALSE(host.Called("reset-title"));
}

TEST_CASE(
    "Gameplay loop attract demos return to title on a button press: a normal "
    "session ignores Start",
    "[boot-gameplay-loop]") {
  RecordingLoopHost host;
  host.control_mode = FieldControlMode::kNone;
  host.pressed = Button::Start;
  CHECK(RunFrame(host));
  CHECK_FALSE(host.Called("reset-title"));
}

TEST_CASE(
    "Gameplay loop shortcut menu checks buttons in priority order: Circle wins "
    "over Cross",
    "[boot-gameplay-loop]") {
  RecordingLoopHost host;
  host.control_mode = FieldControlMode::kShortcutMenu;
  host.previous = Button::L2;
  host.pressed = Button::Circle | Button::Cross;
  CHECK(RunFrame(host));
  CHECK(DispatchCalls(host) ==
        std::vector<std::string>{"shortcut-hud", "shortcut:1", "action-menu"});
}

TEST_CASE(
    "Gameplay loop shortcut menu checks buttons in priority order: the d-pad "
    "maps to shortcuts 5 to 8",
    "[boot-gameplay-loop]") {
  RecordingLoopHost host;
  host.control_mode = FieldControlMode::kShortcutMenu;
  host.previous = Button::L2;
  host.pressed = Button::Left;
  CHECK(RunFrame(host));
  CHECK(host.Called("shortcut:8"));
}

TEST_CASE(
    "Gameplay loop shortcut menu checks buttons in priority order: an "
    "unavailable shortcut keeps the shortcut menu",
    "[boot-gameplay-loop]") {
  RecordingLoopHost host;
  host.control_mode = FieldControlMode::kShortcutMenu;
  host.previous = Button::L2;
  host.pressed = Button::Square;
  host.shortcut_opens = false;
  CHECK(RunFrame(host));
  CHECK(host.Called("shortcut:3"));
  CHECK_FALSE(host.Called("action-menu"));
}

TEST_CASE(
    "Gameplay loop shortcut menu checks buttons in priority order: releasing "
    "L2 ends the shortcut menu",
    "[boot-gameplay-loop]") {
  RecordingLoopHost host;
  host.control_mode = FieldControlMode::kShortcutMenu;
  host.previous = Button::L2;
  host.previous = 0;
  host.pressed = Button::Circle;
  CHECK(RunFrame(host));
  CHECK(DispatchCalls(host) == std::vector<std::string>{"end-shortcut-menu"});
}

TEST_CASE("Gameplay loop cutscenes wait for battle actors to settle",
          "[boot-gameplay-loop]") {
  RecordingLoopHost host;
  host.control_mode = FieldControlMode::kCutscene;
  host.action_flow = VS::Boot::kCutscenePreparingFlow;
  host.delta_mode = 1;
  host.actor_present[kPresentActor] = true;

  SECTION("a walking actor delays the start") {
    host.actor_flags[kPresentActor] = actor_state_flag::kWalking;
    CHECK(RunFrame(host));
    CHECK_FALSE(host.Called("start-cutscene"));
  }
  SECTION("settled actors start the prepared cutscene") {
    CHECK(RunFrame(host));
    CHECK(DispatchCalls(host) ==
          std::vector<std::string>{"orbit-input", "actor-movement",
                                   "camera-angles", "start-cutscene"});
  }
  SECTION("a turning camera delays the start") {
    host.yaw_delta = 1;
    CHECK(RunFrame(host));
    CHECK_FALSE(host.Called("start-cutscene"));
  }
}

TEST_CASE("Gameplay loop ends finished cutscenes by entry mode",
          "[boot-gameplay-loop]") {
  RecordingLoopHost host;
  host.control_mode = FieldControlMode::kCutscene;
  host.playback_status = VS::Boot::kEventPlaybackIdle;

  SECTION("a normal session returns to the field") {
    CHECK(RunFrame(host));
    CHECK(DispatchCalls(host) ==
          std::vector<std::string>{"gim-images", "queued-commands",
                                   "room-trigger:255", "end-cutscene"});
  }
  SECTION("an attract demo returns to title") {
    host.entry_mode = kAttractDemo;
    CHECK(RunFrame(host));
    CHECK(host.Called("reset-title"));
    CHECK_FALSE(host.Called("end-cutscene"));
  }
}

TEST_CASE("Gameplay loop trap interaction returns to battle control",
          "[boot-gameplay-loop]") {
  RecordingLoopHost host;
  host.control_mode = FieldControlMode::kTrapInteraction;

  SECTION("a focused camera dispatches the trap") {
    host.action_flow = Flow(InteractionFlow::kCameraFocus);
    CHECK(RunFrame(host));
    CHECK(DispatchCalls(host) ==
          std::vector<std::string>{"trap-focus", "dispatch-interaction",
                                   "camera-angles"});
  }
  SECTION("the trap finishes when its effect is idle") {
    host.action_flow = Flow(InteractionFlow::kTrapEffectOrChestOpening);
    CHECK(RunFrame(host));
    CHECK(host.Called("finish-trap"));
  }
  SECTION("the camera return hands control back") {
    host.action_flow = Flow(InteractionFlow::kReturnCamera);
    host.look_at_on_ashley = true;
    CHECK(RunFrame(host));
    const std::vector<std::string> expected{"camera-return", "rebuild-plans",
                                            "recovery:0", "control-mode:2",
                                            "camera-angles"};
    CHECK(DispatchCalls(host) == expected);
    CHECK(host.control_mode == FieldControlMode::kBattleAction);
  }
}

TEST_CASE("Gameplay loop chest interaction needs both camera checks",
          "[boot-gameplay-loop]") {
  RecordingLoopHost host;
  host.control_mode = FieldControlMode::kChestInteraction;
  host.action_flow = Flow(InteractionFlow::kReturnCamera);
  host.look_at_on_ashley = true;

  SECTION("an unfinished return waits") {
    host.camera_returned = false;
    CHECK(RunFrame(host));
    CHECK_FALSE(host.Called("recovery:0"));
  }
  SECTION("a finished return releases the notification") {
    CHECK(RunFrame(host));
    const std::vector<std::string> expected{
        "camera-return", "release-notification", "recovery:0", "control-mode:2",
        "camera-angles"};
    CHECK(DispatchCalls(host) == expected);
  }
  SECTION("the contents close when selection ends") {
    host.action_flow = Flow(InteractionFlow::kChestContents);
    CHECK(RunFrame(host));
    CHECK(host.Called("chest-selection"));
    CHECK(host.Called("finish-chest"));
  }
}

TEST_CASE(
    "Gameplay loop battle flows choose camera and sphere work: ability timing "
    "holds the camera and hides the sphere",
    "[boot-gameplay-loop]") {
  RecordingLoopHost host;
  host.control_mode = FieldControlMode::kBattleAction;
  host.action_flow = Flow(BattleActionFlow::kAbilityTiming);
  CHECK(RunFrame(host));
  CHECK_FALSE(host.Called("camera-angles"));
  CHECK(host.Called("risk-penalty"));
  CHECK(host.Called("flow:7"));
  CHECK_FALSE(host.Called("sphere"));
}

TEST_CASE(
    "Gameplay loop battle flows choose camera and sphere work: area selection "
    "moves the camera and draws the sphere",
    "[boot-gameplay-loop]") {
  RecordingLoopHost host;
  host.control_mode = FieldControlMode::kBattleAction;
  host.action_flow = Flow(BattleActionFlow::kAreaSelection);
  CHECK(RunFrame(host));
  CHECK(host.Called("camera-angles"));
  CHECK(host.Called("flow:2"));
  CHECK(host.Called("sphere"));
}

TEST_CASE(
    "Gameplay loop battle flows choose camera and sphere work: the sphere "
    "follows the flow left by the step",
    "[boot-gameplay-loop]") {
  RecordingLoopHost host;
  host.control_mode = FieldControlMode::kBattleAction;
  host.action_flow = Flow(BattleActionFlow::kAreaSelection);
  host.flow_after_battle_step = Flow(BattleActionFlow::kWaitEffectReady);
  CHECK(RunFrame(host));
  CHECK_FALSE(host.Called("sphere"));
}

TEST_CASE(
    "Gameplay loop battle flows choose camera and sphere work: an idle flow "
    "runs no step",
    "[boot-gameplay-loop]") {
  RecordingLoopHost host;
  host.control_mode = FieldControlMode::kBattleAction;
  CHECK(RunFrame(host));
  CHECK(host.Called("risk-penalty"));
  CHECK_FALSE(host.Called("sphere"));
  CHECK(DispatchCalls(host) == std::vector<std::string>{"track-target:0"});
}

TEST_CASE("Gameplay loop room vitals can open enemy loot",
          "[boot-gameplay-loop]") {
  RecordingLoopHost host;
  host.vitals_outcome = RoomActorOutcome::kOpenEnemyLoot;
  CHECK(RunFrame(host));
  const auto vitals = std::ranges::find(host.calls, "vitals");
  REQUIRE(std::distance(vitals, host.calls.end()) > 2);
  CHECK(*(vitals + 1) == "reward-mode:0");
  CHECK(*(vitals + 2) == "enemy-loot");
}

TEST_CASE("Gameplay loop ends rooms only on a fully faded exit",
          "[boot-gameplay-loop]") {
  RecordingLoopHost host;
  host.transition_alpha = kOpaque;
  host.transition_step = ScreenTransitionStep::kRoomExit;
  CHECK(VS::Boot::IsRoomExitComplete(host));
  host.transition_step = ScreenTransitionStep::kZoneExit;
  CHECK(VS::Boot::IsRoomExitComplete(host));
  host.transition_step = ScreenTransitionStep::kRoomEntryReveal;
  CHECK_FALSE(VS::Boot::IsRoomExitComplete(host));
  host.transition_step = ScreenTransitionStep::kRoomExit;
  host.transition_alpha = kOpaque - 1;
  CHECK_FALSE(VS::Boot::IsRoomExitComplete(host));
}

TEST_CASE("Gameplay loop runs rooms until the zone exit",
          "[boot-gameplay-loop]") {
  RecordingLoopHost host;
  GameplayLoopState state;
  host.frames_per_room = 2;
  host.room_exits = {ScreenTransitionStep::kRoomExit,
                     ScreenTransitionStep::kZoneExit};
  CHECK(VS::Boot::RunGameplayZone(state, host));
  CHECK(host.calls.front() == "zone-load");
  CHECK(host.calls.back() == "zone-release");
  CHECK(host.Count("room-load") == 2);
  CHECK(host.Count("room-unload") == 2);
  CHECK(host.Count("present") == 4);
  CHECK(host.Count("menu-queued:0") == 2);
}

TEST_CASE("Gameplay room refusal resets before running a frame or teardown",
          "[boot-gameplay-loop][room-event-failure]") {
  RecordingLoopHost host;
  GameplayLoopState state;
  host.room_load_succeeds = false;
  host.room_exits = {ScreenTransitionStep::kZoneExit};
  CHECK_FALSE(VS::Boot::RunGameplayRoom(state, host));
  CHECK(host.calls == std::vector<std::string>{"room-load", "reset-title"});
}

TEST_CASE("Gameplay load refusal exits the zone and outer loop",
          "[boot-gameplay-loop][room-event-failure]") {
  RecordingLoopHost host;
  GameplayLoopState state;
  host.room_load_succeeds = false;
  host.room_exits = {ScreenTransitionStep::kZoneExit};
  host.zones_before_exit = 1;
  CHECK_NOTHROW(VS::Boot::RunGameplayLoop(state, host));
  CHECK(host.calls == std::vector<std::string>{"menu-queued:0", "init",
                                               "zone-load", "room-load",
                                               "reset-title"});
}

TEST_CASE("Gameplay loop repeats zones after one initialization",
          "[boot-gameplay-loop]") {
  RecordingLoopHost host;
  GameplayLoopState state;
  host.room_exits = {ScreenTransitionStep::kZoneExit,
                     ScreenTransitionStep::kZoneExit};
  host.zones_before_exit = 2;
  CHECK_THROWS_AS(VS::Boot::RunGameplayLoop(state, host), LoopExit);
  REQUIRE(host.calls.size() > 2);
  CHECK(host.calls[0] == "menu-queued:0");
  CHECK(host.calls[1] == "init");
  CHECK(host.Count("init") == 1);
  CHECK(host.Count("zone-load") == 2);
  CHECK(host.Count("room-load") == 2);
}
