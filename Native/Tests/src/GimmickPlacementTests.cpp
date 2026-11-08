#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Content/ModuleData.h"
#include "Event/RoomTriggerEntryLive.h"
#include "Event/ScriptVmLive.hpp"
#include "Event/TriggersLive.hpp"
#include "Field/ActorCollisionLive.hpp"
#include "Field/ChestInteractions.hpp"
#include "Field/ChestInteractionsLive.h"
#include "Field/GimmickAnimationQueue.hpp"
#include "Field/GimmickAnimationQueueLive.h"
#include "Field/GimmickPlacement.hpp"
#include "Field/InteractionTarget.hpp"
#include "Field/InteractionTargetLive.hpp"
#include "Field/RoomSwitches.hpp"
#include "Field/RoomSwitchesLive.h"

extern "C" {
#include "Battle/ActionFlow.h"
#include "Effects/Runtime.h"
#include "Event/Triggers.h"
#include "Field/GimmickPlacementLive.h"
#include "Field/Gimmicks.h"
#include "Field/Interactions.h"
}

namespace {

struct RecordingChestInteractionHost final : VS::Field::ChestInteractionHost {
  std::optional<VS::Field::ChestLockState> chest{VS::Field::ChestLockState{}};
  bool flag = false;
  bool item = false;
  std::vector<std::int32_t> calls;
  std::optional<VS::Field::ChestTarget> selected;
  bool notification_pending = false;
  VS::Field::ChestSelectionResult contents_result =
      VS::Field::ChestSelectionResult::kPending;

  std::optional<VS::Field::ChestLockState> Chest(
      VS::Field::ChestTarget) const override {
    return chest;
  }
  bool ScriptFlag(std::int32_t) override { return flag; }
  bool HasItem(std::int16_t) override { return item; }
  void SetScriptFlag(std::int32_t value) override {
    calls.push_back(value);
    flag = true;
  }
  void ConsumeItem(std::int16_t value) override { calls.push_back(value); }
  bool Unlock(VS::Field::ChestTarget) override {
    if (!chest) {
      return false;
    }
    chest->lock_level = 0;
    calls.push_back(1000);
    return true;
  }
  void ShowUnlocked(std::int16_t) override { calls.push_back(1001); }
  void PlayPose(const VS::Field::ChestLockState&, std::uint8_t pose) override {
    calls.push_back(pose);
  }
  bool StartAnimation(VS::Field::ChestTarget) override {
    calls.push_back(1002);
    return true;
  }
  bool SelectChest(VS::Field::ChestTarget target, std::int32_t) override {
    selected = target;
    return true;
  }
  std::optional<VS::Field::ChestTarget> SelectedChest() const override {
    return selected;
  }
  void BeginChestFlow() override { calls.push_back(1010); }
  void FinishChestFlow() override { calls.push_back(1011); }
  void RestoreIdlePoses() override { calls.push_back(1012); }
  void ResetActionSelection() override { calls.push_back(1013); }
  void ReleaseNotification() override { calls.push_back(1014); }
  void ShowLocked(std::int32_t kind, std::int16_t) override {
    calls.push_back(kind);
  }
  void OpenChestLoot(VS::Field::ChestTarget) override { calls.push_back(1015); }
  void OpenChestStorage(std::int32_t storage) override {
    calls.push_back(storage);
  }
  void PauseForDialog() override { calls.push_back(1016); }
  void PauseForMenu() override { calls.push_back(1017); }
  bool NotificationPending() override { return notification_pending; }
  VS::Field::ChestSelectionResult UpdateContentsSelection() override {
    return contents_result;
  }
  void ReleaseContentsRows() override { calls.push_back(1018); }
  void ResumeField() override { calls.push_back(1019); }
};

struct RecordingInteractionDispatchHost final
    : VS::Field::InteractionDispatchHost {
  VS::Field::InteractionTarget target;
  bool trap_mode = false;
  std::vector<int> calls;
  VS::Field::InteractionTarget Target() const override { return target; }
  bool TrapMode() const override { return trap_mode; }
  void BeginDispatch() override { calls.push_back(0); }
  bool DispatchTrap(VS::Field::TrapTarget) override {
    calls.push_back(1);
    return true;
  }
  bool DispatchChest(VS::Field::ChestTarget) override {
    calls.push_back(2);
    return true;
  }
};

struct RecordingTrapInteractionHost final : VS::Field::TrapSelectionHost,
                                            VS::Field::TrapFinishHost {
  bool valid = true;
  bool disable_succeeds = true;
  std::optional<VS::Field::TrapTarget> selected;
  std::optional<VS::Field::TrapTarget> disabled;
  std::int32_t selected_actor = -1;
  std::vector<std::int32_t> calls;

  bool Valid(VS::Field::TrapTarget, std::int32_t) const override {
    return valid;
  }
  void BeginFlow() override { calls.push_back(1000); }
  void Select(VS::Field::TrapTarget target, std::int32_t actor) override {
    selected = target;
    selected_actor = actor;
    calls.push_back(1001);
  }
  void RestoreIdle(std::int32_t actor) override { calls.push_back(actor); }
  void ShowInformation(VS::Field::TrapTarget) override {
    calls.push_back(1002);
  }
  void ResetActionSelection() override { calls.push_back(1003); }
  std::optional<VS::Field::TrapTarget> Target() const override {
    return selected;
  }
  void FinishFlow() override { calls.push_back(1004); }
  void ReleaseNotification() override { calls.push_back(1005); }
  bool DisableTrap(VS::Field::TrapTarget target) override {
    disabled = target;
    calls.push_back(1006);
    return disable_succeeds;
  }
  void ClearTarget() override {
    selected.reset();
    calls.push_back(1007);
  }
};

using SnapshotBytes = std::array<std::byte, sizeof(Field_GimmickSnapshot)>;

struct RecordingTrapActionHost final : VS::Field::TrapActionHost {
  bool action_state = true;
  bool stage_succeeds = true;
  bool resolve_succeeds = true;
  bool remove_during_resolve = false;
  bool change_during_resolve = false;
  mutable std::optional<std::int16_t> kind = 0;
  std::optional<VS::Field::TrapActionInput> staged;
  std::optional<VS::Field::TrapEffectRequest> effect;
  std::vector<std::int32_t> calls;

  bool HasActionState(const VS::Field::TrapActionInput&) const override {
    return action_state;
  }
  std::optional<std::int16_t> Kind(VS::Field::TrapTarget) const override {
    return kind;
  }
  bool StageVictim(const VS::Field::TrapActionInput& input) override {
    calls.push_back(1000);
    staged = input;
    return stage_succeeds;
  }
  bool ResolveAction(std::uint16_t action_id) override {
    calls.push_back(action_id);
    if (remove_during_resolve) {
      kind.reset();
    } else if (change_during_resolve) {
      kind = 1;
    }
    return resolve_succeeds;
  }
  bool BeginEffect(const VS::Field::TrapEffectRequest& request) override {
    calls.push_back(1001);
    effect = request;
    return true;
  }
};

struct RecordingInteractionCameraHost final : VS::Field::InteractionCameraHost {
  std::optional<VS::Field::TrapPosition> focus{
      VS::Field::TrapPosition{1, 2, 3}};
  std::optional<VS::Field::TrapPosition> ashley{
      VS::Field::TrapPosition{4, 5, 6}};
  std::int32_t speed = 0;
  std::uint32_t flags = 0;
  bool pending = false;
  bool transition = false;
  std::vector<std::int32_t> calls;
  mutable std::size_t flag_reads = 0;
  VS::Field::TrapPosition stepped{};
  std::int32_t mode = -1;
  std::int32_t requested_speed = 0;

  std::optional<VS::Field::TrapPosition> Focus() const override {
    return focus;
  }
  std::optional<VS::Field::TrapPosition> AshleyPosition() const override {
    return ashley;
  }
  std::int32_t StepLookAt(const VS::Field::TrapPosition& point,
                          std::int32_t step_mode,
                          std::int32_t step_speed) override {
    stepped = point;
    mode = step_mode;
    requested_speed = step_speed;
    calls.push_back(0);
    return speed;
  }
  std::uint32_t AshleyStateFlags() const override {
    ++flag_reads;
    return flags;
  }
  bool NotificationPending() override {
    calls.push_back(1);
    return pending;
  }
  bool AshleyTransitionPose() override {
    calls.push_back(2);
    return transition;
  }
};

struct RecordingObjectInteractionWaitHost final
    : VS::Field::ObjectInteractionWaitHost {
  VS::Field::FieldControlMode mode = VS::Field::FieldControlMode::kNone;
  std::vector<int> calls;

  void SetControlMode(VS::Field::FieldControlMode value) override {
    mode = value;
    calls.push_back(0);
  }
  void PauseField() override { calls.push_back(1); }
  void ReleaseNotification() override { calls.push_back(2); }
  void ResumeField() override { calls.push_back(3); }
  void EnterExploration() override {
    mode = VS::Field::FieldControlMode::kExploration;
    calls.push_back(4);
  }
};

struct RecordingRoomObjectInteractionHost final
    : VS::Field::RoomObjectInteractionHost {
  std::optional<VS::Field::RoomObjectLockState> state{
      VS::Field::RoomObjectLockState{}};
  bool has_item = false;
  bool previous_flag = false;
  bool unlock_succeeds = true;
  std::int16_t flag_value = 0;
  std::vector<std::int32_t> calls;

  std::optional<VS::Field::RoomObjectLockState> State() const override {
    return state;
  }
  bool HasItem(std::int16_t) override {
    calls.push_back(0);
    return has_item;
  }
  bool PreviousUnlockFlag(std::int32_t) override {
    calls.push_back(1);
    return previous_flag;
  }
  void SetUnlockFlag(std::int32_t flag, std::int16_t value) override {
    calls.push_back(flag);
    flag_value = value;
  }
  void ConsumeItem(std::int16_t item) override { calls.push_back(item); }
  bool Unlock() override {
    calls.push_back(1000);
    if (unlock_succeeds && state) {
      state->lock_state = 0;
    }
    return unlock_succeeds;
  }
  void ShowNotification(std::int32_t kind, std::int16_t) override {
    calls.push_back(kind);
  }
  void PlayPose(const VS::Field::RoomObjectLockState&,
                std::uint8_t pose) override {
    calls.push_back(pose);
  }
  void BeginToggle() override { calls.push_back(1001); }
  void PlaySound(std::int32_t sound) override { calls.push_back(sound); }
  void RestoreIdle(std::int32_t actor) override {
    CHECK(actor == 0);
    calls.push_back(1002);
  }
};

struct RoomObjectRecordFixture {
  Field_RoomRecord saved = Field_roomData;
  std::array<Field_MpdLockTrigger, 2> locks{};

  RoomObjectRecordFixture() {
    Field_roomData.lockTriggers = locks.data();
    Field_roomData.header.lockTriggersLength = 0x28;
  }
  ~RoomObjectRecordFixture() {
    Field_roomData = saved;
    VS::Field::InvalidateLiveInteractionTargets();
  }
};

struct SnapshotFixture {
  std::array<Field_GimmickInstance*, 16> saved{};
  std::unique_ptr<Field_GimmickInstance> object =
      std::make_unique<Field_GimmickInstance>();

  SnapshotFixture() {
    std::ranges::copy(Field_gimmickInstances, saved.begin());
    std::ranges::fill(Field_gimmickInstances, nullptr);
    object->parent = 0xFF;
  }
  ~SnapshotFixture() { std::ranges::copy(saved, Field_gimmickInstances); }
};

struct TriggerFixture {
  SnapshotFixture snapshots;
  VS::Event::RoomEventLease room = VS::Event::LiveRoomScriptSections();
  VS::Event::PlaybackPhase phase = VS::Event::LiveScriptVm().Phase();
  VS::Event::EventSource source = VS::Event::LiveScriptVm().Source();
  std::int16_t event_id = VS::Event::LiveScriptVm().EventId();
  std::int16_t entry = Event_hasRoomEntryScript;
  char direct = Event_isDirectEventRequest;

  TriggerFixture() {
    VS::Event::LiveScriptVm().SetPhase(VS::Event::PlaybackPhase::kIdle);
    Event_isDirectEventRequest = 0;
    Event_hasRoomEntryScript = 0;
  }
  ~TriggerFixture() {
    VS::Event::PublishLiveRoomScriptSections(room);
    VS::Event::LiveScriptVm().SetPhase(phase);
    VS::Event::LiveScriptVm().SetSource(source);
    VS::Event::LiveScriptVm().SetEventId(event_id);
    Event_hasRoomEntryScript = entry;
    Event_isDirectEventRequest = direct;
  }
};

void PublishTrigger(std::initializer_list<std::uint8_t> bytes) {
  const std::vector<std::uint8_t> data(bytes);
  const auto room = VS::Event::RoomEventSections::Copy({.triggers = data});
  REQUIRE(room);
  VS::Event::PublishLiveRoomScriptSections(*room);
}

Field_GimmickSnapshot SentinelSnapshot() {
  return {0xAA, 0xBB, 0xCC, 0xDD, 0x12345678, 0x7F7F7F7F};
}

std::uint8_t ExpectedHeight(const Field_GimmickInstance& object) {
  const auto support = VS::Field::LiveSampleSupportHeightAt(
      object.positionX, object.positionY, object.positionZ, 0, 0, 0);
  const auto difference = static_cast<std::int64_t>(support) - object.positionY;
  const auto wrapped =
      std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(difference));
  return static_cast<std::uint8_t>(wrapped / 128);
}

void CheckValidSnapshot(int slot, const Field_GimmickInstance& object) {
  const auto expected_height = ExpectedHeight(object);
  auto output = SentinelSnapshot();
  REQUIRE(Field_GimmickPlacementEntry(slot, &output) == 0);
  CHECK(output.placementByte0 == 5);
  CHECK(output.placementByte2 == 9);
  CHECK(output.orientation == 3);
  CHECK(output.heightAboveGround == expected_height);
  CHECK((std::bit_cast<std::uint32_t>(output.positionXY) & 0xFFFF) == 0xFB2E);
  CHECK((std::bit_cast<std::uint32_t>(output.positionXY) >> 16) ==
        static_cast<std::uint16_t>(object.positionY));
  CHECK(std::bit_cast<std::uint32_t>(output.positionZ) == 0x13570929);
}

struct RecordingGimmickHost final : VS::Field::GimmickPlacementHost {
  static constexpr std::size_t kSlots = 16;
  std::array<std::optional<VS::Field::GimmickPlacementState>, kSlots> states{};
  mutable std::size_t reads = 0;
  std::size_t support_calls = 0;
  std::int32_t support_height = 0;

  std::size_t GimmickSlots() const override { return states.size(); }
  std::optional<VS::Field::GimmickPlacementState> Gimmick(
      std::size_t slot) const override {
    ++reads;
    return states.at(slot);
  }
  std::int32_t SupportHeight(const VS::Field::GimmickPlacementState&) override {
    ++support_calls;
    return support_height;
  }
};

std::uint8_t OriginalGimmickHeight(std::int32_t support_height,
                                   std::int16_t position_y) {
  // CPU 800A00BC subu, 800A00C0 bgez, 800A00C8 addiu127, 800A00CC sra7.
  const auto difference =
      static_cast<std::int64_t>(support_height) - position_y;
  const auto wrapped =
      std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(difference));
  const auto rounded =
      wrapped < 0 ? static_cast<std::int64_t>(wrapped) + 127 : wrapped;
  return static_cast<std::uint8_t>(static_cast<std::uint32_t>(rounded) >> 7);
}

void CheckTypedGimmickPlacement(RecordingGimmickHost& host,
                                const VS::Field::GimmickPlacementState& state) {
  const auto snapshot = VS::Field::GimmickPlacement(0, host);
  if (!snapshot) {
    FAIL("Stationary gimmick placement must produce a snapshot");
    return;
  }
  REQUIRE(snapshot);
  CHECK(snapshot->tile_x == 0xFF);
  CHECK(snapshot->tile_z == 9);
  CHECK(snapshot->orientation == 0x81);
  CHECK(snapshot->height_above_ground ==
        OriginalGimmickHeight(host.support_height, state.position_y));
  CHECK(
      std::bit_cast<std::uint32_t>(snapshot->position_xy) ==
      (0x0000FB2E |
       (static_cast<std::uint32_t>(static_cast<std::uint16_t>(state.position_y))
        << 16)));
  CHECK(std::bit_cast<std::uint32_t>(snapshot->position_z_tail) == 0xFEDC0929);
}

}  // namespace

namespace {

struct RecordingAnimationQueueHost final
    : VS::Field::GimmickAnimationQueueHost {
  bool target = true;
  bool refuse_store = false;
  VS::Field::GimmickAnimationOccupancy slots{};
  mutable std::size_t reads = 0;
  std::size_t stores = 0;
  std::size_t count_writes = 0;
  std::size_t pending = 0;
  std::optional<std::size_t> stored_slot;

  bool HasRequestedTarget() const override { return target; }
  VS::Field::GimmickAnimationOccupancy Occupancy() const override {
    ++reads;
    return slots;
  }
  bool StoreRequestedJob(std::size_t slot,
                         VS::Field::GimmickAnimationKind) override {
    if (refuse_store) {
      return false;
    }
    ++stores;
    stored_slot = slot;
    slots.at(slot) = true;
    return true;
  }
  void SetPendingCount(std::size_t count) override {
    ++count_writes;
    pending = count;
  }
};

struct RecordingAnimationDispatchHost final
    : VS::Field::GimmickAnimationDispatchHost {
  std::array<std::optional<VS::Field::GimmickAnimationJobSnapshot>,
             VS::Field::kGimmickAnimationSlots>
      jobs{};
  std::vector<std::int32_t> calls;
  bool finish_ready = true;
  bool replace_during_finish = false;
  bool reset_during_finish = false;

  std::int32_t PendingCount() const override {
    return static_cast<std::int32_t>(std::ranges::count_if(
        jobs, [](const auto& job) { return job.has_value(); }));
  }
  VS::Field::GimmickAnimationOccupancy Occupancy() const override {
    VS::Field::GimmickAnimationOccupancy occupied{};
    for (std::size_t slot = 0; slot < jobs.size(); ++slot) {
      occupied[slot] = jobs[slot].has_value();
    }
    return occupied;
  }
  std::optional<VS::Field::GimmickAnimationJobSnapshot> Job(
      std::size_t slot) const override {
    return jobs.at(slot);
  }
  bool Matches(std::size_t slot,
               const VS::Field::GimmickAnimationJobSnapshot& expected) const {
    const auto& job = jobs.at(slot);
    return job && job->serial == expected.serial;
  }
  bool StoreCountdown(std::size_t slot,
                      const VS::Field::GimmickAnimationJobSnapshot& expected,
                      std::uint8_t countdown) override {
    if (!Matches(slot, expected)) {
      return false;
    }
    if (auto& job = jobs.at(slot)) {
      job->countdown = countdown;
    }
    return true;
  }
  bool FinishDoor(std::size_t slot,
                  const VS::Field::GimmickAnimationJobSnapshot&) override {
    calls.push_back(1);
    if (reset_during_finish) {
      for (auto& job : jobs) {
        job.reset();
      }
    }
    if (replace_during_finish) {
      if (auto& job = jobs.at(slot)) {
        ++job->serial;
      }
    }
    return finish_ready;
  }
  void ToggleSwitch(std::size_t,
                    const VS::Field::GimmickAnimationJobSnapshot&) override {
    calls.push_back(2);
  }
  void StartDoor(std::size_t,
                 const VS::Field::GimmickAnimationJobSnapshot&) override {
    calls.push_back(3);
  }
  bool RetireJob(
      std::size_t slot,
      const VS::Field::GimmickAnimationJobSnapshot& expected) override {
    if (!Matches(slot, expected)) {
      return false;
    }
    calls.push_back(4);
    jobs.at(slot).reset();
    return true;
  }
};

struct RecordingAnimationResetHost final
    : VS::Field::GimmickAnimationResetHost {
  std::vector<std::size_t> calls;
  void ClearPendingCount() override { calls.push_back(8); }
  void ClearMarkerTicks() override { calls.push_back(9); }
  void ClearJob(std::size_t slot) override { calls.push_back(slot); }
};

struct AnimationQueueFixture {
  static constexpr std::size_t kSlots = 8;
  std::array<Field_GimmickAnimationJob, kSlots> saved{};
  std::int16_t pending = Field_pendingGimmickAnimationCount;
  std::uint16_t marker_ticks = Field_triggerMarkerFrameTicks;

  AnimationQueueFixture() {
    std::ranges::copy(Field_gimmickAnimationQueue, saved.begin());
    Field_ResetGimmickAnimationQueueEntry();
  }
  ~AnimationQueueFixture() {
    std::ranges::copy(saved, Field_gimmickAnimationQueue);
    Field_pendingGimmickAnimationCount = pending;
    Field_triggerMarkerFrameTicks = marker_ticks;
  }
  AnimationQueueFixture(const AnimationQueueFixture&) = delete;
  AnimationQueueFixture& operator=(const AnimationQueueFixture&) = delete;
};

struct ChestTargetFixture {
  Field_RoomRecord saved = Field_roomData;
  std::uint32_t control_mode = Field_controlMode;
  decltype(Battle_actionFlowMode) action_flow = Battle_actionFlowMode;
  Actors_ActorLocationQuery actor_query = Field_interactionFocus.actor_query;
  AnimationQueueFixture queue;
  std::array<Items_TreasureChest, 1> chests{};

  ChestTargetFixture() {
    Field_roomData.containers = chests.data();
    Field_roomData.header.containersLength = 0x234;
    VS::Field::InvalidateLiveInteractionTargets();
    chests[0].tileX = -1;
    chests[0].tileZ = -1;
    chests[0].layer = -1;
    chests[0].lidRotationId = -1;
  }
  ~ChestTargetFixture() {
    Field_roomData = saved;
    VS::Field::InvalidateLiveInteractionTargets();
    Field_controlMode = control_mode;
    Battle_actionFlowMode = action_flow;
    Field_interactionFocus.actor_query = actor_query;
  }
};

void ConfigureOneFrameDelayedDoor(Field_MpdLockTrigger& target) {
  target.trigger.tileX = -1;
  target.trigger.tileZ = -1;
  target.trigger.floorLayer = -1;
  target.trigger.lockState.lock.roomEffect = -1;
  target.linkedLockIndex = -1;
  target.isTriggered = 1;
  target.delayFrames = 1;
}

void QueueOneFrameSwitchJobs(std::span<Field_MpdLockTrigger> targets) {
  for (std::size_t slot = 0; slot < AnimationQueueFixture::kSlots; ++slot) {
    auto& target = targets[slot];
    target.trigger.tileX = -1;
    target.trigger.tileZ = -1;
    target.trigger.floorLayer = -1;
    target.trigger.kind = -1;
    target.trigger.activation = 1;
    Field_QueueGimmickAnimationEntry(2, &target);
  }
  Field_QueueGimmickAnimationEntry(2, &targets.back());
}

void CheckDrainedSwitchJobs(std::span<const Field_MpdLockTrigger> targets) {
  for (std::size_t slot = 0; slot < AnimationQueueFixture::kSlots; ++slot) {
    CHECK(targets[slot].trigger.activation == 0);
    CHECK(targets[slot].trigger.lockState.switchValue == 1);
    CHECK(Field_gimmickAnimationQueue[slot].jobKind == 0);
    CHECK(Field_gimmickAnimationQueue[slot].trigger == nullptr);
  }
}

void CheckAnimationKindResets(Field_MpdLockTrigger& target) {
  for (const auto kind : {1, 2, 3}) {
    Field_QueueGimmickAnimationEntry(kind, &target);
    CHECK(Field_pendingGimmickAnimationCount == 1);
    Field_ResetGimmickAnimationQueueEntry();
    CHECK(Field_pendingGimmickAnimationCount == 0);
  }
}

enum class SwitchCallKind { kSound, kEffect, kStore, kCell };

struct SwitchCall {
  SwitchCallKind kind = SwitchCallKind::kSound;
  std::int32_t value = 0;
  friend bool operator==(const SwitchCall&, const SwitchCall&) = default;
};

struct RecordingSwitchHost final : VS::Field::RoomSwitchHost {
  std::optional<VS::Field::RoomSwitchState> state;
  std::optional<VS::Field::RoomSwitchState> after_effect;
  bool remove_after_sound = false;
  std::vector<SwitchCall> calls;
  std::array<std::int32_t, 3> cell_query{};

  std::optional<VS::Field::RoomSwitchState> State() const override {
    return state;
  }
  void PlaySound(std::int32_t sound) override {
    calls.push_back({SwitchCallKind::kSound, sound});
    if (remove_after_sound) {
      state.reset();
    }
  }
  void StartEffect(std::int32_t kind) override {
    calls.push_back({SwitchCallKind::kEffect, kind});
    if (after_effect) {
      state = after_effect;
    }
  }
  void StoreSwitch(std::uint16_t value, std::uint8_t marker_fade) override {
    calls.push_back({SwitchCallKind::kStore, value});
    if (state) {
      state->switch_value = value;
      state->marker_fade = marker_fade;
    }
  }
  void SetCellPassability(const VS::Field::RoomSwitchState& query,
                          std::uint8_t passability) override {
    calls.push_back({SwitchCallKind::kCell, passability});
    cell_query = {query.tile_x, query.tile_z, query.floor_layer};
  }
};

struct RecordingSwitchStartHost final : VS::Field::RoomSwitchStartHost {
  std::optional<VS::Field::RoomSwitchState> state{VS::Field::RoomSwitchState{}};
  bool refuse = false;
  bool refuse_store = false;
  std::vector<std::int32_t> calls;

  std::optional<VS::Field::RoomSwitchState> State() const override {
    return state;
  }
  bool StoreStartState(std::uint8_t marker, std::uint8_t activation) override {
    if (refuse_store || !state) {
      return false;
    }
    state->marker_fade = marker;
    state->activation = activation;
    calls.push_back(marker);
    calls.push_back(activation);
    return true;
  }
  bool QueueToggle() override {
    calls.push_back(2);
    return !refuse;
  }
  bool ToggleImmediately() override {
    calls.push_back(3);
    return !refuse;
  }
};

void CheckOriginalCountdownByte(VS::Field::GimmickAnimationKind kind,
                                int byte) {
  RecordingAnimationDispatchHost host;
  host.jobs[0] = VS::Field::GimmickAnimationJobSnapshot{
      kind, static_cast<std::uint8_t>(byte), 1, true};
  const int original_decrement = byte - 1;
  const auto original_byte = static_cast<std::uint8_t>(original_decrement);
  const bool complete = kind == VS::Field::GimmickAnimationKind::kToggleSwitch
                            ? std::bit_cast<std::int8_t>(original_byte) <= 0
                            : original_byte == 0;
  CHECK(VS::Field::UpdateGimmickAnimationQueue(host) ==
        static_cast<std::size_t>(complete));
  CHECK(host.PendingCount() == static_cast<std::int32_t>(!complete));
  if (const auto& job = host.jobs[0]) {
    CHECK(job->countdown == original_byte);
  }
}

void CheckChestOpenAndBlockedRules(VS::Field::ChestTarget target) {
  RecordingChestInteractionHost open;
  CHECK(VS::Field::TryOpenChest(target, open));
  CHECK(open.calls == std::vector<std::int32_t>{2, 1002});
  RecordingChestInteractionHost blocked;
  blocked.chest = VS::Field::ChestLockState{.lock_level = 2};
  CHECK_FALSE(VS::Field::TryOpenChest(target, blocked));
  CHECK(blocked.calls == std::vector<std::int32_t>{3});
  blocked.chest.reset();
  CHECK_FALSE(VS::Field::TryOpenChest(target, blocked));
}

void CheckChestLockedItemRules(VS::Field::ChestTarget target) {
  RecordingChestInteractionHost locked;
  locked.chest =
      VS::Field::ChestLockState{.lock_level = 1, .required_item = 0x1D2};
  CHECK_FALSE(VS::Field::TryOpenChest(target, locked));
  CHECK(locked.calls == std::vector<std::int32_t>{3});
  locked.item = true;
  locked.calls.clear();
  CHECK(VS::Field::TryOpenChest(target, locked));
  CHECK(locked.calls ==
        std::vector<std::int32_t>{0x388, 0x1D2, 1000, 1001, 2, 1002});
}

void CheckChestBeginFinishRules(RecordingChestInteractionHost& host,
                                VS::Field::ChestTarget target) {
  CHECK(VS::Field::BeginChestInteraction(target, 0, host));
  CHECK(host.selected == target);
  CHECK(host.calls == std::vector<std::int32_t>{1010, 1012, 1, 1013});
  host.calls.clear();
  CHECK(VS::Field::FinishChestInteraction(host));
  CHECK(host.calls == std::vector<std::int32_t>{1011, 4, 1014});
  host.calls.clear();
}

void CheckChestContentsRules(RecordingChestInteractionHost& host) {
  CHECK(VS::Field::OpenChestContents(host));
  CHECK(host.calls == std::vector<std::int32_t>{1015, 1017});
  host.calls.clear();
  host.chest = VS::Field::ChestLockState{.lock_level = 3, .required_item = 9};
  CHECK(VS::Field::OpenChestContents(host));
  CHECK(host.calls == std::vector<std::int32_t>{3, 1016});
  host.calls.clear();
  host.chest = VS::Field::ChestLockState{.opened_flag = 67};
  CHECK(VS::Field::OpenChestContents(host));
  CHECK(host.calls == std::vector<std::int32_t>{3, 1017});
  host.selected.reset();
  CHECK_FALSE(VS::Field::OpenChestContents(host));
}

void CheckTargetKindSelection(VS::Field::InteractionTargetState& state) {
  state.Select(VS::Field::ChestTarget{3, 7});
  CHECK(state.Trap() == nullptr);
  REQUIRE(state.Chest());
  CHECK(*state.Chest() == VS::Field::ChestTarget{3, 7});
  state.Select(VS::Field::TrapTarget{4, 8});
  CHECK(state.Chest() == nullptr);
  REQUIRE(state.Trap());
  CHECK(*state.Trap() == VS::Field::TrapTarget{4, 8});
}

void CheckTargetDomainBounds(VS::Field::InteractionTargetDomain domain) {
  CHECK(VS::Field::ResolveTrapTarget({1, 7}, domain) == 1);
  CHECK_FALSE(VS::Field::ResolveTrapTarget({2, 7}, domain));
  CHECK_FALSE(VS::Field::ResolveTrapTarget({0, 6}, domain));
  CHECK(VS::Field::ResolveChestTarget({2, 7}, domain) == 2);
  CHECK_FALSE(VS::Field::ResolveChestTarget({3, 7}, domain));
  CHECK_FALSE(VS::Field::ResolveChestTarget({0, 8}, domain));
}

void CheckSelectedTargetDomain(VS::Field::InteractionTargetDomain domain) {
  VS::Field::InteractionTargetState state;
  state.Select(VS::Field::ChestTarget{2, 7});
  CHECK(state.Chest(domain) != nullptr);
  CHECK(state.Trap(domain) == nullptr);
  CHECK(state.Chest({8, 2, 3}) == nullptr);
  CHECK(state.Chest({7, 2, 2}) == nullptr);
}

}  // namespace

TEST_CASE("Typed chest interaction keeps lock item and pose rules separate",
          "[chest-interaction-core][field]") {
  const VS::Field::ChestTarget target{0, 1};
  CheckChestOpenAndBlockedRules(target);
  CheckChestLockedItemRules(target);
}

namespace {

void CheckStagedTrapVictim(const RecordingTrapActionHost& host,
                           const VS::Field::TrapActionInput& input) {
  if (!host.staged) {
    FAIL("Trap action must retain staged victim values");
    return;
  }
  REQUIRE(host.staged);
  CHECK(host.staged->actor == 3);
  CHECK(host.staged->default_limb == 7);
  CHECK(host.staged->position == input.position);
}

void CheckStagedTrapEffect(const RecordingTrapActionHost& host) {
  if (!host.effect) {
    FAIL("Trap action must retain initialized effect values");
    return;
  }
  REQUIRE(host.effect);
  CHECK(host.effect->effect_id == 6);
  CHECK(host.effect->actor == 3);
  CHECK(host.effect->ground_anchor == 0xFD);
  CHECK_FALSE(host.effect->source_position);
}

void CheckTrapCompatibilitySource(const Effects_ActionEffectRequest& request) {
  CHECK(request.source.type == 5);
  CHECK(request.source.colorPreset == 0);
  CHECK(request.source.value.position.vx == 0);
  CHECK(request.source.value.position.vy == 0);
  CHECK(request.source.value.position.vz == 0);
  CHECK(request.source.value.position.pad == 0);
}

void CheckTrapCompatibilityVictim(const Effects_ActionEffectRequest& request) {
  CHECK(request.targets[0].type == 4);
  CHECK(request.targets[0].colorPreset == 0);
  CHECK(request.targets[0].value.actor.actor == 3);
  CHECK(request.targets[0].value.actor.part == 0xFD);
  CHECK(request.targets[0].value.actor.eventActorSlot == 0);
  CHECK(request.targets[0].value.actor.reserved3 == 0);
}

void CheckInvalidTrapActors(RecordingTrapActionHost& host) {
  for (const auto actor : {-1, 17}) {
    CHECK_FALSE(VS::Field::TriggerActorTrap({0, 1}, {.actor = actor}, host));
    CHECK(host.calls.empty());
  }
}

void CheckInvalidTrapKinds(RecordingTrapActionHost& host) {
  for (const auto kind : {-1, 16}) {
    host.kind = static_cast<std::int16_t>(kind);
    CHECK_FALSE(VS::Field::TriggerActorTrap({0, 1}, {.actor = 0}, host));
    CHECK(host.calls.empty());
  }
}

}  // namespace

TEST_CASE("Trap action stages victim then resolves before initialized effect",
          "[trap-action-core][field]") {
  RecordingTrapActionHost host;
  const VS::Field::TrapActionInput input{
      .actor = 3, .default_limb = 7, .position = {123, -234, 345}};
  REQUIRE(VS::Field::TriggerActorTrap({0, 1}, input, host));
  CHECK(host.calls == std::vector<std::int32_t>{1000, 6, 1001});
  CheckStagedTrapVictim(host, input);
  CheckStagedTrapEffect(host);
}

TEST_CASE("Trap effect compatibility record initializes every supplied field",
          "[trap-action-live][field]") {
  const auto request = VS::Field::LiveTrapEffectRecord(
      {.effect_id = 6, .actor = 3, .source_position = std::nullopt});
  CHECK(request.effectId == 6);
  CHECK(request.count == 1);
  CHECK(request.otherCount == 0);
  CheckTrapCompatibilitySource(request);
  CheckTrapCompatibilityVictim(request);
  CHECK(request.targets[1].type == 0);
  CHECK(request.others[0] == 0);
  CHECK(request.position.vx == 0);
  CHECK(request.position.vy == 0);
  CHECK(request.position.vz == 0);
}

TEST_CASE("Trap effect compatibility record retains explicitly supplied source",
          "[trap-action-live][field]") {
  const auto request = VS::Field::LiveTrapEffectRecord(
      {.effect_id = 9, .actor = 0, .source_position = {{123, -234, 345}}});
  CHECK(request.source.value.position.vx == 123);
  CHECK(request.source.value.position.vy == -234);
  CHECK(request.source.value.position.vz == 345);
}

TEST_CASE("Trap action refuses invalid actor state or kind before mutation",
          "[trap-action-core][field]") {
  RecordingTrapActionHost host;
  CheckInvalidTrapActors(host);
  CheckInvalidTrapKinds(host);
  host.kind = 0;
  host.action_state = false;
  CHECK_FALSE(VS::Field::TriggerActorTrap({0, 1}, {.actor = 0}, host));
  CHECK(host.calls.empty());
}

TEST_CASE("Trap action reobserves target after resolver callback",
          "[trap-action-core][field]") {
  RecordingTrapActionHost host;
  host.change_during_resolve = true;
  REQUIRE(VS::Field::TriggerActorTrap({0, 1}, {.actor = 0}, host));
  if (!host.effect) {
    FAIL("Resolver must publish the reobserved trap effect");
    return;
  }
  REQUIRE(host.effect);
  CHECK(host.effect->effect_id == 7);
  RecordingTrapActionHost removed;
  removed.remove_during_resolve = true;
  CHECK_FALSE(VS::Field::TriggerActorTrap({0, 1}, {.actor = 0}, removed));
  CHECK(removed.calls == std::vector<std::int32_t>{1000, 6});
  CHECK_FALSE(removed.effect);
}

TEST_CASE("Trap action stops on refused staging or resolution",
          "[trap-action-core][field]") {
  RecordingTrapActionHost staged;
  staged.stage_succeeds = false;
  CHECK_FALSE(VS::Field::TriggerActorTrap({0, 1}, {.actor = 0}, staged));
  CHECK(staged.calls == std::vector<std::int32_t>{1000});
  RecordingTrapActionHost resolved;
  resolved.resolve_succeeds = false;
  CHECK_FALSE(VS::Field::TriggerActorTrap({0, 1}, {.actor = 0}, resolved));
  CHECK(resolved.calls == std::vector<std::int32_t>{1000, 6});
  CHECK_FALSE(resolved.effect);
}

TEST_CASE("Native trap action preserves its original module binding",
          "[trap-action-live][field]") {
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x80073D30) ==
        reinterpret_cast<const void*>(&Field_TriggerActorTrapEntry));
}

TEST_CASE("Native trap action refuses invalid actor or stale target",
          "[trap-action-live][field]") {
  CHECK_FALSE(VS::Field::TriggerLiveActorTrap({}, {}, -1));
  CHECK_FALSE(VS::Field::TriggerLiveActorTrap({}, {}, 17));
  CHECK_FALSE(VS::Field::TriggerLiveActorTrap({}, {}, 0));
  Field_TriggerActorTrapEntry(nullptr, nullptr, -1);
}

TEST_CASE("Interaction camera focus wait preserves actor gate short circuit",
          "[interaction-camera-core][field]") {
  RecordingInteractionCameraHost host;
  CHECK(VS::Field::WaitForChestCameraFocus(host));
  host.flags = 0x08000000;
  CHECK_FALSE(VS::Field::WaitForChestCameraFocus(host));
  host.speed = 7;
  const auto reads = host.flag_reads;
  CHECK(VS::Field::WaitForChestCameraFocus(host));
  CHECK(host.flag_reads == reads);
  CHECK(host.mode == 0);
  CHECK(host.requested_speed == 0x28);
}

TEST_CASE("Interaction camera missing focus performs no camera step",
          "[interaction-camera-core][field]") {
  RecordingInteractionCameraHost host;
  host.focus.reset();
  CHECK(VS::Field::WaitForChestCameraFocus(host));
  CHECK_FALSE(VS::Field::StepTrapCameraFocus(host));
  CHECK(host.calls.empty());
  CHECK(host.flag_reads == 0);
}

TEST_CASE("Chest animation readiness observes notification only after actor",
          "[interaction-camera-core][field]") {
  RecordingInteractionCameraHost host;
  CHECK_FALSE(VS::Field::IsChestAnimationReady(host));
  CHECK(host.calls.empty());
  host.flags = 0x08000000;
  CHECK(VS::Field::IsChestAnimationReady(host));
  host.pending = true;
  CHECK_FALSE(VS::Field::IsChestAnimationReady(host));
  CHECK(host.calls == std::vector<std::int32_t>{1, 1});
}

TEST_CASE("Interaction camera returns from Ashley with original halfword wrap",
          "[interaction-camera-core][field]") {
  RecordingInteractionCameraHost host;
  host.ashley = VS::Field::TrapPosition{123, -32768, -234};
  host.speed = 9;
  CHECK(VS::Field::TrackInteractionCameraToAshley(host) == 9);
  CHECK(host.stepped == VS::Field::TrapPosition{123, 32678, -234});
  CHECK(host.mode == 1);
  CHECK(host.calls == std::vector<std::int32_t>{0, 2});
  host.transition = true;
  CHECK(VS::Field::TrackInteractionCameraToAshley(host) == 0);
  host.ashley.reset();
  host.calls.clear();
  CHECK_FALSE(VS::Field::TrackInteractionCameraToAshley(host));
  CHECK(host.calls.empty());
}

TEST_CASE("Interaction camera entries retain all original module addresses",
          "[interaction-camera-live][field]") {
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x80074120) ==
        reinterpret_cast<const void*>(&Field_WaitForChestCameraFocusEntry));
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x8007418C) ==
        reinterpret_cast<const void*>(&Field_StepTrapCameraFocusEntry));
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x8007424C) ==
        reinterpret_cast<const void*>(&Field_IsChestAnimationReadyEntry));
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x80074374) ==
        reinterpret_cast<const void*>(
            &Field_TrackInteractionCameraToAshleyEntry));
}

TEST_CASE("Interaction camera entries refuse an absent target focus",
          "[interaction-camera-live][field]") {
  const auto saved = VS::Field::LiveInteractionTargetState();
  VS::Field::LiveInteractionTargetState().Clear();
  CHECK(Field_WaitForChestCameraFocusEntry() == 1);
  CHECK(Field_StepTrapCameraFocusEntry() == 0);
  VS::Field::LiveInteractionTargetState() = saved;
}

TEST_CASE("Facing tile rounds every heading into original cardinal quadrants",
          "[facing-tile-core][field]") {
  VS::Actors::ActorPlacementView actor{};
  actor.tile.x = 17;
  actor.tile.z = 23;
  actor.tile.layer = 2;
  for (std::uint32_t heading = 0; heading < 65536; ++heading) {
    actor.position.w =
        std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(heading));
    const auto tile = VS::Field::TileInFrontOfActor(actor);
    const auto expected = ((heading + 0x200) & 0xFFF) >> 10;
    CHECK(static_cast<std::uint8_t>(tile.direction) == expected);
    CHECK(tile.layer == 2);
    CHECK(tile.x == (expected == 1 ? 16 : expected == 3 ? 18 : 17));
    CHECK(tile.z == (expected == 0 ? 22 : expected == 2 ? 24 : 23));
  }
}

TEST_CASE("Facing tile preserves original byte wrapping at room boundaries",
          "[facing-tile-core][field]") {
  VS::Actors::ActorPlacementView actor{};
  CHECK(VS::Field::TileInFrontOfActor(actor).z == 255);
  actor.position.w = 1024;
  CHECK(VS::Field::TileInFrontOfActor(actor).x == 255);
  actor.tile.x = 255;
  actor.tile.z = 255;
  actor.position.w = 2048;
  CHECK(VS::Field::TileInFrontOfActor(actor).z == 0);
  actor.position.w = 3072;
  CHECK(VS::Field::TileInFrontOfActor(actor).x == 0);
}

TEST_CASE("Facing tile entry preserves X layer Z and untouched fourth byte",
          "[facing-tile-live][field]") {
  Actors_ActorLocationQuery actor{};
  actor.location.packedTile.target.tileX = 17;
  actor.location.packedTile.target.tileZ = 23;
  actor.location.packedTile.target.tileLayer = 2;
  actor.location.position.pad = 1024;
  std::array<std::uint8_t, 4> output{0xA1, 0xA2, 0xA3, 0xA4};
  CHECK(Field_TileInFrontOfActorEntry(&actor, output.data()) == 1);
  CHECK(output == std::array<std::uint8_t, 4>{16, 2, 23, 0xA4});
  const auto saved = output;
  CHECK(Field_TileInFrontOfActorEntry(nullptr, output.data()) == -1);
  CHECK(output == saved);
  CHECK(Field_TileInFrontOfActorEntry(&actor, nullptr) == -1);
}

TEST_CASE("Facing tile entry retains original module address",
          "[facing-tile-live][field]") {
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x80074798) ==
        reinterpret_cast<const void*>(&Field_TileInFrontOfActorEntry));
}

TEST_CASE("Object interaction wait selects its mode before pausing",
          "[object-interaction-wait-core][field]") {
  RecordingObjectInteractionWaitHost host;
  VS::Field::BeginObjectInteractionWait(host);
  CHECK(host.mode == VS::Field::FieldControlMode::kObjectInteractionWait);
  CHECK(host.calls == std::vector<int>{0, 1});
  VS::Field::BeginObjectInteractionWait(host);
  CHECK(host.calls == std::vector<int>{0, 1, 0, 1});
}

TEST_CASE("Object interaction wait releases and resumes before exploration",
          "[object-interaction-wait-core][field]") {
  RecordingObjectInteractionWaitHost host;
  VS::Field::BeginObjectInteractionWait(host);
  host.calls.clear();
  VS::Field::EndObjectInteractionWait(host);
  CHECK(host.mode == VS::Field::FieldControlMode::kExploration);
  CHECK(host.calls == std::vector<int>{2, 3, 4});
}

TEST_CASE("Object interaction wait keeps both original module bindings",
          "[object-interaction-wait-core][field]") {
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x80073AA4) ==
        reinterpret_cast<const void*>(&Field_BeginObjectInteractionWaitEntry));
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x80073ACC) ==
        reinterpret_cast<const void*>(&Field_EndObjectInteractionWaitEntry));
}

TEST_CASE("Room object interaction keeps reusable keys and consumed sigils",
          "[room-object-interaction-core][field]") {
  RecordingRoomObjectInteractionHost key;
  key.state =
      VS::Field::RoomObjectLockState{.required_item = 458, .lock_state = 1};
  key.has_item = true;
  CHECK(VS::Field::InteractWithLockedRoomObject(key) ==
        VS::Field::RoomObjectInteractionResult::kWait);
  CHECK(key.calls == std::vector<std::int32_t>{0, 1000, 7, 5, 1001});
  CHECK(key.state->lock_state == 0);
  RecordingRoomObjectInteractionHost sigil;
  sigil.state =
      VS::Field::RoomObjectLockState{.required_item = 466, .lock_state = 3};
  sigil.has_item = true;
  CHECK(VS::Field::InteractWithLockedRoomObject(sigil) ==
        VS::Field::RoomObjectInteractionResult::kWait);
  CHECK(sigil.calls ==
        std::vector<std::int32_t>{0, 0x388, 466, 1000, 9, 5, 1001});
  CHECK(sigil.flag_value == 2);
}

TEST_CASE("Room object prior unlock opens silently and busy records do nothing",
          "[room-object-interaction-core][field]") {
  RecordingRoomObjectInteractionHost host;
  host.state =
      VS::Field::RoomObjectLockState{.required_item = 458, .lock_state = 1};
  host.previous_flag = true;
  CHECK(VS::Field::InteractWithLockedRoomObject(host) ==
        VS::Field::RoomObjectInteractionResult::kWait);
  CHECK(host.calls == std::vector<std::int32_t>{0, 1, 1000, 5, 1001});
  host.calls.clear();
  host.state->is_open = true;
  CHECK(VS::Field::InteractWithLockedRoomObject(host) ==
        VS::Field::RoomObjectInteractionResult::kBusy);
  CHECK(host.calls.empty());
}

TEST_CASE("Room object rejection preserves notification sounds and idle order",
          "[room-object-interaction-core][field]") {
  RecordingRoomObjectInteractionHost host;
  host.state = VS::Field::RoomObjectLockState{.lock_state = 2};
  CHECK(VS::Field::InteractWithLockedRoomObject(host) ==
        VS::Field::RoomObjectInteractionResult::kWait);
  CHECK(host.calls == std::vector<std::int32_t>{5, 0x65, 0x66, 1002});
  host.calls.clear();
  host.state = VS::Field::RoomObjectLockState{.lock_state = 3};
  CHECK(VS::Field::InteractWithLockedRoomObject(host) ==
        VS::Field::RoomObjectInteractionResult::kWait);
  CHECK(host.calls == std::vector<std::int32_t>{0, 1, 10, 0x65, 0x66, 1002});
}

TEST_CASE("Room object busy and invalid targets perform no item reads",
          "[room-object-interaction-core][field]") {
  RecordingRoomObjectInteractionHost host;
  host.state = VS::Field::RoomObjectLockState{.is_triggered = true};
  CHECK(VS::Field::InteractWithLockedRoomObject(host) ==
        VS::Field::RoomObjectInteractionResult::kBusy);
  CHECK(host.calls.empty());
  host.state.reset();
  CHECK(VS::Field::InteractWithLockedRoomObject(host) ==
        VS::Field::RoomObjectInteractionResult::kRefused);
  CHECK(host.calls.empty());
  host.state = VS::Field::RoomObjectLockState{.lock_state = 4};
  CHECK(VS::Field::InteractWithLockedRoomObject(host) ==
        VS::Field::RoomObjectInteractionResult::kRefused);
  CHECK(host.calls.empty());
}

TEST_CASE("Room object refused unlock cannot start pose or toggle",
          "[room-object-interaction-core][field]") {
  RecordingRoomObjectInteractionHost host;
  CHECK(VS::Field::InteractWithLockedRoomObject(host) ==
        VS::Field::RoomObjectInteractionResult::kWait);
  CHECK(host.calls == std::vector<std::int32_t>{5, 1001});
  host.calls.clear();
  host.state =
      VS::Field::RoomObjectLockState{.required_item = 458, .lock_state = 1};
  host.has_item = true;
  host.unlock_succeeds = false;
  CHECK(VS::Field::InteractWithLockedRoomObject(host) ==
        VS::Field::RoomObjectInteractionResult::kRefused);
  CHECK(host.calls == std::vector<std::int32_t>{0, 1000});
  CHECK(host.state->lock_state == 1);
}

TEST_CASE("Native room object entry refuses null and foreign record addresses",
          "[room-object-interaction-live][field]") {
  RoomObjectRecordFixture fixture;
  Field_MpdLockTrigger foreign{};
  CHECK(Field_InteractWithLockedRoomObjectEntry(nullptr) == 1);
  CHECK(Field_InteractWithLockedRoomObjectEntry(&foreign) == 1);
  CHECK(Field_InteractWithLockedRoomObjectEntry(
            reinterpret_cast<Field_MpdLockTrigger*>(std::uintptr_t{1})) == 1);
}

TEST_CASE("Native room object entry reads only its present busy lock record",
          "[room-object-interaction-live][field]") {
  RoomObjectRecordFixture fixture;
  fixture.locks[1].isOpen = 1;
  CHECK(Field_InteractWithLockedRoomObjectEntry(&fixture.locks[1]) == 1);
  fixture.locks[1].isOpen = 0;
  fixture.locks[1].isTriggered = 1;
  CHECK(Field_InteractWithLockedRoomObjectEntry(&fixture.locks[1]) == 1);
}

TEST_CASE("Native room object interaction keeps its original module binding",
          "[room-object-interaction-live][field]") {
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(
      VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                        0x80073AFC) ==
      reinterpret_cast<const void*>(&Field_InteractWithLockedRoomObjectEntry));
}

TEST_CASE("Typed chest flow owns selected begin finish and contents decisions",
          "[chest-interaction-core][field]") {
  RecordingChestInteractionHost host;
  const VS::Field::ChestTarget target{0, 1};
  CheckChestBeginFinishRules(host, target);
  CheckChestContentsRules(host);
}

TEST_CASE("Typed chest selection preserves accepted and cancelled outcomes",
          "[chest-interaction-core][field]") {
  RecordingChestInteractionHost host;
  host.selected = VS::Field::ChestTarget{0, 1};
  host.chest = VS::Field::ChestLockState{.opened_flag = 7};
  CHECK(VS::Field::UpdateChestContentsSelection(host));
  host.contents_result = VS::Field::ChestSelectionResult::kAccepted;
  CHECK_FALSE(VS::Field::UpdateChestContentsSelection(host));
  CHECK(host.calls == std::vector<std::int32_t>{0x3C7, 1018, 1019});
  host.calls.clear();
  host.contents_result = VS::Field::ChestSelectionResult::kCancelled;
  CHECK_FALSE(VS::Field::UpdateChestContentsSelection(host));
  CHECK(host.calls == std::vector<std::int32_t>{1002, 1018, 1019});
}

TEST_CASE("Interaction dispatch refuses mismatched chest and trap modes",
          "[interaction-target][field]") {
  RecordingInteractionDispatchHost host;
  host.target = VS::Field::ChestTarget{0, 1};
  CHECK(VS::Field::DispatchTrapOrChestInteraction(host));
  CHECK(host.calls == std::vector<int>{0, 2});
  host.calls.clear();
  host.trap_mode = true;
  CHECK_FALSE(VS::Field::DispatchTrapOrChestInteraction(host));
  CHECK(host.calls.empty());
  host.target = VS::Field::TrapTarget{0, 1};
  CHECK(VS::Field::DispatchTrapOrChestInteraction(host));
  CHECK(host.calls == std::vector<int>{0, 1});
}

TEST_CASE("Typed trap begin preserves actor capture and original reset order",
          "[interaction-target][field]") {
  RecordingTrapInteractionHost host;
  const VS::Field::TrapTarget target{2, 7};
  REQUIRE(VS::Field::BeginTrapInteraction(target, 3, host));
  CHECK(host.selected == target);
  CHECK(host.selected_actor == 3);
  CHECK(host.calls == std::vector<std::int32_t>{
                          1000, 1001, 0,  1,  2,  3,  4,  5,  6,    7,
                          8,    9,    10, 11, 12, 13, 14, 15, 1002, 1003});
}

TEST_CASE("Typed trap begin refuses invalid capture before side effects",
          "[interaction-target][field]") {
  RecordingTrapInteractionHost host;
  host.valid = false;
  CHECK_FALSE(VS::Field::BeginTrapInteraction({2, 7}, 3, host));
  CHECK(host.calls.empty());
  CHECK_FALSE(host.selected);
}

TEST_CASE("Typed trap finish disables only its selected target then clears it",
          "[interaction-target][field]") {
  RecordingTrapInteractionHost host;
  const VS::Field::TrapTarget target{2, 7};
  host.selected = target;
  CHECK(VS::Field::FinishTrapInteraction(host));
  CHECK(host.disabled == target);
  CHECK_FALSE(host.selected);
  CHECK(host.calls == std::vector<std::int32_t>{1004, 1005, 1006, 1007});
  host.calls.clear();
  CHECK_FALSE(VS::Field::FinishTrapInteraction(host));
  CHECK(host.calls.empty());
}

TEST_CASE("Typed trap finish clears a target refused during disable",
          "[interaction-target][field]") {
  RecordingTrapInteractionHost host;
  host.selected = VS::Field::TrapTarget{2, 7};
  host.disable_succeeds = false;
  CHECK_FALSE(VS::Field::FinishTrapInteraction(host));
  CHECK_FALSE(host.selected);
  CHECK(host.calls == std::vector<std::int32_t>{1004, 1005, 1006, 1007});
}

TEST_CASE(
    "Interaction target state cannot expose a chest through the trap kind",
    "[interaction-target][field]") {
  VS::Field::InteractionTargetState state;
  CHECK(state.Trap() == nullptr);
  CHECK(state.Chest() == nullptr);
  CheckTargetKindSelection(state);
  state.Clear();
  CHECK(state.Trap() == nullptr);
  CHECK(state.Chest() == nullptr);
}

TEST_CASE("Interaction target domain refuses stale and out-of-range identities",
          "[interaction-target][field]") {
  const VS::Field::InteractionTargetDomain domain{7, 2, 3};
  CheckTargetDomainBounds(domain);
  CheckSelectedTargetDomain(domain);
}

TEST_CASE("Live interaction target indexes refuse room-generation replacement",
          "[interaction-target][field]") {
  const auto saved = Field_roomData;
  struct RestoreRoom {
    Field_RoomRecord record;
    ~RestoreRoom() {
      Field_roomData = record;
      VS::Field::InvalidateLiveInteractionTargets();
    }
  } restore{saved};
  std::array<Field_MpdFloorTrigger, 2> traps{};
  std::array<Items_TreasureChest, 2> chests{};
  Field_roomData.floorTriggers = traps.data();
  Field_roomData.header.floorTriggersLength = 24;
  Field_roomData.containers = chests.data();
  Field_roomData.header.containersLength = 0x468;
  const auto trap = VS::Field::FindLiveTrapTarget(&traps[1]);
  const auto chest = VS::Field::FindLiveChestTarget(&chests[1]);
  REQUIRE(trap);
  REQUIRE(chest);
  CHECK(VS::Field::ResolveLiveTrapTarget(*trap) == &traps[1]);
  CHECK(VS::Field::ResolveLiveChestTarget(*chest) == &chests[1]);
  CHECK_FALSE(VS::Field::FindLiveTrapTarget(nullptr));
  CHECK_FALSE(VS::Field::FindLiveChestTarget(nullptr));
  VS::Field::InvalidateLiveInteractionTargets();
  CHECK(VS::Field::ResolveLiveTrapTarget(*trap) == nullptr);
  CHECK(VS::Field::ResolveLiveChestTarget(*chest) == nullptr);
}

TEST_CASE("Typed chest animation never stores a lock-record alias",
          "[interaction-target][field]") {
  ChestTargetFixture fixture;
  const auto target = VS::Field::FindLiveChestTarget(&fixture.chests[0]);
  REQUIRE(target);
  REQUIRE(VS::Field::BeginLiveChestSwitch(*target));
  CHECK(Field_pendingGimmickAnimationCount == 1);
  CHECK(Field_gimmickAnimationQueue[0].jobKind == 2);
  CHECK(Field_gimmickAnimationQueue[0].trigger == nullptr);
  CHECK(static_cast<std::uint16_t>(fixture.chests[0].reservedA) == 0x0114);
  fixture.chests[0].reservedA = 0x0101;
  Field_UpdateGimmickAnimationQueueEntry();
  CHECK(fixture.chests[0].isOpen == 1);
  CHECK(fixture.chests[0].reservedA == 0);
  CHECK(Field_pendingGimmickAnimationCount == 0);
}

TEST_CASE("Typed chest animation refuses a stale room-generation target",
          "[interaction-target][field]") {
  ChestTargetFixture fixture;
  const auto target = VS::Field::FindLiveChestTarget(&fixture.chests[0]);
  REQUIRE(target);
  REQUIRE(VS::Field::BeginLiveChestSwitch(*target));
  VS::Field::InvalidateLiveInteractionTargets();
  const auto before = fixture.chests[0].reservedA;
  Field_UpdateGimmickAnimationQueueEntry();
  CHECK(Field_pendingGimmickAnimationCount == 0);
  CHECK(fixture.chests[0].reservedA == before);
  CHECK_FALSE(VS::Field::BeginLiveChestSwitch(*target));
}

TEST_CASE("Native chest focus selects typed record and refuses stale dispatch",
          "[interaction-target][field]") {
  ChestTargetFixture fixture;
  const auto target = VS::Field::FindLiveChestTarget(&fixture.chests[0]);
  REQUIRE(target);
  CHECK(VS::Field::BeginLiveChestInteraction(*target, 0));
  const auto* selected = VS::Field::LiveInteractionTargetState().Chest(
      VS::Field::LiveInteractionTargetDomain());
  REQUIRE(selected);
  CHECK(*selected == *target);
  CHECK(VS::Field::LiveInteractionTargetState().Trap() == nullptr);
  VS::Field::InvalidateLiveInteractionTargets();
  CHECK_FALSE(VS::Field::TryOpenLiveChest(*target));
}

TEST_CASE("Typed switch start restores state on refused delayed admission",
          "[gimmick-switch-admission-core][field]") {
  RecordingSwitchStartHost host;
  host.state = VS::Field::RoomSwitchState{.activation = 7};
  host.refuse = true;
  CHECK_FALSE(VS::Field::BeginSwitchToggle(host));
  REQUIRE(host.state);
  CHECK(host.state->marker_fade == 0);
  CHECK(host.state->activation == 7);
  CHECK(host.calls == std::vector<std::int32_t>{1, 20, 2, 0, 7});
  host.refuse = false;
  CHECK(VS::Field::BeginSwitchToggle(host));
  CHECK(host.state->marker_fade == 1);
  CHECK(host.state->activation == 20);
}

TEST_CASE("Typed switch start preserves immediate and busy behavior",
          "[gimmick-switch-admission-core][field]") {
  RecordingSwitchStartHost host;
  host.state = VS::Field::RoomSwitchState{.switch_value = 2, .activation = 7};
  CHECK(VS::Field::BeginSwitchToggle(host));
  CHECK(host.calls == std::vector<std::int32_t>{1, 7, 3});
  const auto before = host.calls;
  CHECK_FALSE(VS::Field::BeginSwitchToggle(host));
  CHECK(host.calls == before);
  host.state.reset();
  CHECK_FALSE(VS::Field::BeginSwitchToggle(host));
  CHECK(host.calls == before);
}

TEST_CASE("Typed switch start cannot dispatch after a refused state store",
          "[gimmick-switch-admission-core][field]") {
  RecordingSwitchStartHost host;
  host.refuse_store = true;
  CHECK_FALSE(VS::Field::BeginSwitchToggle(host));
  CHECK(host.calls.empty());
}

TEST_CASE("Typed animation reset preserves original count tick and slot order",
          "[gimmick-animation-reset-core][field]") {
  RecordingAnimationResetHost host;
  VS::Field::ResetGimmickAnimationQueue(host);
  CHECK(host.calls == std::vector<std::size_t>{8, 9, 0, 1, 2, 3, 4, 5, 6, 7});
  host.calls.clear();
  VS::Field::ResetGimmickAnimationQueue(host);
  CHECK(host.calls == std::vector<std::size_t>{8, 9, 0, 1, 2, 3, 4, 5, 6, 7});
}

TEST_CASE(
    "Native animation reset clears every slot counters and module binding",
    "[gimmick-animation-reset-core][field]") {
  const AnimationQueueFixture fixture;
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x8008B6B4) ==
        reinterpret_cast<const void*>(&Field_ResetGimmickAnimationQueueEntry));
  std::array<Field_MpdLockTrigger, AnimationQueueFixture::kSlots> targets{};
  for (auto& target : targets) {
    Field_QueueGimmickAnimationEntry(1, &target);
  }
  Field_triggerMarkerFrameTicks = 0xFFFF;
  Field_ResetGimmickAnimationQueueEntry();
  CHECK(Field_pendingGimmickAnimationCount == 0);
  CHECK(Field_triggerMarkerFrameTicks == 0);
  for (const auto& job : Field_gimmickAnimationQueue) {
    CHECK(job.jobKind == 0);
    CHECK(job.trigger == nullptr);
  }
  Field_ResetGimmickAnimationQueueEntry();
  CHECK(Field_pendingGimmickAnimationCount == 0);
  CHECK(Field_triggerMarkerFrameTicks == 0);
}

TEST_CASE("Typed animation dispatch tolerates a reset inside its callback",
          "[gimmick-animation-reset-core][field]") {
  RecordingAnimationDispatchHost host;
  host.jobs[0] = VS::Field::GimmickAnimationJobSnapshot{
      VS::Field::GimmickAnimationKind::kFinishDoor, 0, 1, true};
  host.reset_during_finish = true;
  CHECK(VS::Field::UpdateGimmickAnimationQueue(host) == 0);
  CHECK(host.PendingCount() == 0);
  CHECK(host.calls == std::vector<std::int32_t>{1});
}

TEST_CASE("Typed animation dispatch preserves every original countdown byte",
          "[gimmick-animation-dispatch-core][field]") {
  for (const auto kind : {VS::Field::GimmickAnimationKind::kToggleSwitch,
                          VS::Field::GimmickAnimationKind::kStartDoor}) {
    for (int byte = 0; byte < 256; ++byte) {
      CheckOriginalCountdownByte(kind, byte);
    }
  }
}

TEST_CASE("Typed animation dispatch reserves full door slot before callback",
          "[gimmick-animation-dispatch-core][field]") {
  RecordingAnimationDispatchHost host;
  for (std::size_t slot = 0; slot < host.jobs.size(); ++slot) {
    host.jobs[slot] = VS::Field::GimmickAnimationJobSnapshot{
        VS::Field::GimmickAnimationKind::kStartDoor, 1, slot + 1, true};
  }
  CHECK(VS::Field::UpdateGimmickAnimationQueue(host) == 8);
  REQUIRE(host.calls.size() == 16);
  CHECK(host.calls[0] == 4);
  CHECK(host.calls[1] == 3);
  CHECK(host.calls[2] == 3);
  CHECK(host.calls[3] == 4);
}

TEST_CASE("Typed animation dispatch never clears a callback replacement job",
          "[gimmick-animation-dispatch-core][field]") {
  RecordingAnimationDispatchHost host;
  host.jobs[0] = VS::Field::GimmickAnimationJobSnapshot{
      VS::Field::GimmickAnimationKind::kFinishDoor, 0, 1, true};
  host.replace_during_finish = true;
  CHECK(VS::Field::UpdateGimmickAnimationQueue(host) == 0);
  CHECK(host.PendingCount() == 1);
  CHECK(host.calls == std::vector<std::int32_t>{1});
}

TEST_CASE("Typed gimmick admission rejects invalid requests before slot reads",
          "[gimmick-animation-queue-core][field]") {
  RecordingAnimationQueueHost host;
  for (const auto kind :
       {-1, 0, 4, 257, std::numeric_limits<std::int32_t>::max()}) {
    CHECK_FALSE(VS::Field::QueueGimmickAnimation(
        static_cast<VS::Field::GimmickAnimationKind>(kind), host));
  }
  host.target = false;
  CHECK_FALSE(VS::Field::QueueGimmickAnimation(
      VS::Field::GimmickAnimationKind::kFinishDoor, host));
  CHECK(host.reads == 0);
  CHECK(host.stores == 0);
  CHECK(host.count_writes == 0);
}

TEST_CASE("Typed gimmick admission counts only committed first-free slots",
          "[gimmick-animation-queue-core][field]") {
  RecordingAnimationQueueHost host;
  host.slots[0] = true;
  host.slots[2] = true;
  REQUIRE(VS::Field::QueueGimmickAnimation(
      VS::Field::GimmickAnimationKind::kToggleSwitch, host));
  CHECK(host.stored_slot == 1);
  CHECK(host.pending == 3);
  host.refuse_store = true;
  CHECK_FALSE(VS::Field::QueueGimmickAnimation(
      VS::Field::GimmickAnimationKind::kStartDoor, host));
  CHECK(host.pending == 3);
  CHECK(host.count_writes == 1);
}

TEST_CASE("Typed gimmick admission cannot drift when a full queue refuses",
          "[gimmick-animation-queue-core][field]") {
  RecordingAnimationQueueHost host;
  for (std::size_t slot = 0; slot < VS::Field::kGimmickAnimationSlots; ++slot) {
    REQUIRE(VS::Field::QueueGimmickAnimation(
        VS::Field::GimmickAnimationKind::kFinishDoor, host));
    CHECK(host.stored_slot == slot);
    CHECK(host.pending == slot + 1);
  }
  const auto before = host.slots;
  for (int refusal = 0; refusal < 16; ++refusal) {
    CHECK_FALSE(VS::Field::QueueGimmickAnimation(
        VS::Field::GimmickAnimationKind::kFinishDoor, host));
    CHECK(host.pending == 8);
    CHECK(host.slots == before);
  }
  CHECK(host.stores == 8);
  CHECK(host.count_writes == 8);
}

TEST_CASE("Gimmick animation queue does not count a refused ninth job",
          "[gimmick-animation-queue][field]") {
  const AnimationQueueFixture fixture;
  std::array<Field_MpdLockTrigger, AnimationQueueFixture::kSlots + 1> targets{};
  for (std::size_t slot = 0; slot < AnimationQueueFixture::kSlots; ++slot) {
    Field_QueueGimmickAnimationEntry(1, &targets[slot]);
  }
  REQUIRE(Field_pendingGimmickAnimationCount == 8);
  Field_QueueGimmickAnimationEntry(1, &targets.back());
  CHECK(Field_pendingGimmickAnimationCount == 8);
  for (std::size_t slot = 0; slot < AnimationQueueFixture::kSlots; ++slot) {
    CHECK(Field_gimmickAnimationQueue[slot].jobKind == 1);
    CHECK(Field_gimmickAnimationQueue[slot].trigger == &targets[slot]);
  }
}

TEST_CASE("Native gimmick admission refuses invalid calls and preserves module",
          "[gimmick-animation-queue][field]") {
  const AnimationQueueFixture fixture;
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x8008C070) ==
        reinterpret_cast<const void*>(&Field_QueueGimmickAnimationEntry));
  Field_MpdLockTrigger target{};
  for (const auto kind :
       {-1, 0, 4, 257, std::numeric_limits<std::int32_t>::max()}) {
    Field_QueueGimmickAnimationEntry(kind, &target);
  }
  Field_QueueGimmickAnimationEntry(1, nullptr);
  Field_QueueGimmickAnimationEntry(
      1, reinterpret_cast<Field_MpdLockTrigger*>(std::uintptr_t{1}));
  CHECK(Field_pendingGimmickAnimationCount == 0);
  for (const auto& job : Field_gimmickAnimationQueue) {
    CHECK(job.jobKind == 0);
    CHECK(job.trigger == nullptr);
  }
}

TEST_CASE("Native gimmick admission drains accepted jobs and resets cleanly",
          "[gimmick-animation-queue][field]") {
  const AnimationQueueFixture fixture;
  std::array<Field_MpdLockTrigger, AnimationQueueFixture::kSlots + 1> targets{};
  QueueOneFrameSwitchJobs(targets);
  REQUIRE(Field_pendingGimmickAnimationCount == 8);
  Field_UpdateGimmickAnimationQueueEntry();
  CHECK(Field_pendingGimmickAnimationCount == 0);
  CheckDrainedSwitchJobs(targets);
  CheckAnimationKindResets(targets.back());
}

TEST_CASE("Delayed door follow-up survives a saturated animation queue",
          "[gimmick-door-continuation][field]") {
  const AnimationQueueFixture fixture;
  std::array<Field_MpdLockTrigger, AnimationQueueFixture::kSlots> targets{};
  for (auto& target : targets) {
    ConfigureOneFrameDelayedDoor(target);
    Field_QueueGimmickAnimationEntry(3, &target);
  }
  REQUIRE(Field_pendingGimmickAnimationCount == 8);
  Field_UpdateGimmickAnimationQueueEntry();
  CHECK(Field_pendingGimmickAnimationCount == 8);
  for (const auto& target : targets) {
    const auto found = std::ranges::find_if(
        Field_gimmickAnimationQueue, [&target](const auto& job) {
          return job.jobKind == 1 && job.trigger == &target;
        });
    CHECK(found != std::end(Field_gimmickAnimationQueue));
  }
}

TEST_CASE("Saturated door continuations drain exactly on the following tick",
          "[gimmick-door-continuation][field]") {
  const AnimationQueueFixture fixture;
  std::array<Field_MpdLockTrigger, AnimationQueueFixture::kSlots> targets{};
  for (auto& target : targets) {
    ConfigureOneFrameDelayedDoor(target);
    Field_QueueGimmickAnimationEntry(3, &target);
  }
  Field_UpdateGimmickAnimationQueueEntry();
  REQUIRE(Field_pendingGimmickAnimationCount == 8);
  Field_UpdateGimmickAnimationQueueEntry();
  CHECK(Field_pendingGimmickAnimationCount == 0);
  for (std::size_t slot = 0; slot < targets.size(); ++slot) {
    CHECK(targets[slot].isOpen == 1);
    CHECK(targets[slot].isTriggered == 0);
    CHECK(Field_gimmickAnimationQueue[slot].jobKind == 0);
    CHECK(Field_gimmickAnimationQueue[slot].trigger == nullptr);
  }
}

TEST_CASE("Unsaturated delayed doors retain original same-tick finish order",
          "[gimmick-door-continuation][field]") {
  const AnimationQueueFixture fixture;
  Field_MpdLockTrigger target{};
  ConfigureOneFrameDelayedDoor(target);
  Field_QueueGimmickAnimationEntry(3, &target);
  Field_UpdateGimmickAnimationQueueEntry();
  CHECK(Field_pendingGimmickAnimationCount == 0);
  CHECK(target.isOpen == 1);
  CHECK(target.isTriggered == 0);
}

TEST_CASE(
    "Native animation dispatcher refuses bad entries and preserves module",
    "[gimmick-door-continuation][field]") {
  const AnimationQueueFixture fixture;
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x8008C0C0) ==
        reinterpret_cast<const void*>(&Field_UpdateGimmickAnimationQueueEntry));
  Field_gimmickAnimationQueue[0] = {2, nullptr};
  Field_pendingGimmickAnimationCount = 1;
  Field_UpdateGimmickAnimationQueueEntry();
  CHECK(Field_pendingGimmickAnimationCount == 0);
  CHECK(Field_gimmickAnimationQueue[0].jobKind == 0);
  Field_MpdLockTrigger target{};
  Field_gimmickAnimationQueue[0] = {99, &target};
  Field_pendingGimmickAnimationCount = 1;
  Field_UpdateGimmickAnimationQueueEntry();
  CHECK(Field_pendingGimmickAnimationCount == 0);
  CHECK(Field_gimmickAnimationQueue[0].jobKind == 0);
}

TEST_CASE("Refused switch start leaves its busy marker retryable",
          "[gimmick-switch-admission][field]") {
  const AnimationQueueFixture fixture;
  std::array<Field_MpdLockTrigger, AnimationQueueFixture::kSlots> occupied{};
  for (auto& target : occupied) {
    Field_QueueGimmickAnimationEntry(1, &target);
  }
  Field_MpdLockTrigger rejected{};
  rejected.trigger.activation = 7;
  Field_BeginSwitchToggleEntry(&rejected);
  CHECK(Field_pendingGimmickAnimationCount == 8);
  CHECK(rejected.trigger.markerFade == 0);
  CHECK(rejected.trigger.activation == 7);
}

TEST_CASE("Native switch start retries after queue capacity is freed",
          "[gimmick-switch-admission][field]") {
  const AnimationQueueFixture fixture;
  std::array<Field_MpdLockTrigger, AnimationQueueFixture::kSlots> occupied{};
  for (auto& target : occupied) {
    Field_QueueGimmickAnimationEntry(1, &target);
  }
  Field_MpdLockTrigger target{};
  target.trigger.activation = 7;
  Field_BeginSwitchToggleEntry(&target);
  REQUIRE(target.trigger.markerFade == 0);
  Field_ResetGimmickAnimationQueueEntry();
  Field_BeginSwitchToggleEntry(&target);
  CHECK(Field_pendingGimmickAnimationCount == 1);
  CHECK(target.trigger.markerFade == 1);
  CHECK(target.trigger.activation == 20);
  CHECK(Field_gimmickAnimationQueue[0].jobKind == 2);
  CHECK(Field_gimmickAnimationQueue[0].trigger == &target);
  Field_BeginSwitchToggleEntry(&target);
  CHECK(Field_pendingGimmickAnimationCount == 1);
}

TEST_CASE("Native switch start preserves immediate state and original module",
          "[gimmick-switch-admission][field]") {
  const AnimationQueueFixture fixture;
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x8008D5FC) ==
        reinterpret_cast<const void*>(&Field_BeginSwitchToggleEntry));
  Field_BeginSwitchToggleEntry(nullptr);
  Field_BeginSwitchToggleEntry(
      reinterpret_cast<Field_MpdLockTrigger*>(std::uintptr_t{1}));
  for (const auto value : {-1, 1, 32767, -32768}) {
    Field_MpdLockTrigger target{};
    target.trigger.lockState.switchValue = static_cast<std::int16_t>(value);
    target.trigger.kind = -1;
    target.trigger.tileX = -1;
    target.trigger.tileZ = -1;
    target.trigger.floorLayer = -1;
    Field_BeginSwitchToggleEntry(&target);
    CHECK(Field_pendingGimmickAnimationCount == 0);
    CHECK(target.trigger.lockState.switchValue == 0);
    CHECK(target.trigger.markerFade == 0);
  }
}

TEST_CASE("Room switches preserve original sounds and normalized state order",
          "[room-switches][field]") {
  for (const auto value : {0, 1, 2, 65535}) {
    RecordingSwitchHost host;
    host.state = VS::Field::RoomSwitchState{
        .switch_value = static_cast<std::uint16_t>(value),
        .marker_fade = 0xFF,
        .effect_kind = 7,
        .tile_x = 3,
        .tile_z = 9,
        .floor_layer = -1};
    REQUIRE(VS::Field::ToggleRoomSwitch(host));
    const std::vector<SwitchCall> expected{
        {SwitchCallKind::kSound, value == 0 ? 0x3B : 0x3D},
        {SwitchCallKind::kSound, value == 0 ? 0x3C : 0x3E},
        {SwitchCallKind::kEffect, 7},
        {SwitchCallKind::kStore, value == 0 ? 1 : 0},
        {SwitchCallKind::kCell, value == 0 ? 5 : 0}};
    CHECK(host.calls == expected);
    if (!host.state) {
      FAIL("Room switch state must remain valid after a completed toggle");
      return;
    }
    CHECK(host.state->marker_fade == 0);
    CHECK(host.cell_query == std::array<std::int32_t, 3>{3, 9, -1});
  }
}

TEST_CASE("Room switches use state after effect callbacks",
          "[room-switches][field]") {
  RecordingSwitchHost host;
  host.state = VS::Field::RoomSwitchState{.effect_kind = 7};
  host.after_effect = VS::Field::RoomSwitchState{.switch_value = 2,
                                                 .marker_fade = 5,
                                                 .tile_x = 11,
                                                 .tile_z = 12,
                                                 .floor_layer = 3};
  REQUIRE(VS::Field::ToggleRoomSwitch(host));
  const std::vector<SwitchCall> expected{{SwitchCallKind::kSound, 0x3B},
                                         {SwitchCallKind::kSound, 0x3C},
                                         {SwitchCallKind::kEffect, 7},
                                         {SwitchCallKind::kStore, 0},
                                         {SwitchCallKind::kCell, 0}};
  CHECK(host.calls == expected);
  CHECK(host.cell_query == std::array<std::int32_t, 3>{11, 12, 3});
}

TEST_CASE("Room switches refuse absent and invalidated targets",
          "[room-switches][field]") {
  RecordingSwitchHost host;
  CHECK_FALSE(VS::Field::ToggleRoomSwitch(host));
  CHECK(host.calls.empty());
  host.state = VS::Field::RoomSwitchState{};
  host.remove_after_sound = true;
  CHECK_FALSE(VS::Field::ToggleRoomSwitch(host));
  CHECK(host.calls.size() == 2);
}

TEST_CASE("Native switch entry preserves original module and signed word state",
          "[room-switches][room-switches-live][field]") {
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x8008D658) ==
        reinterpret_cast<const void*>(&Field_ToggleRoomSwitchEntry));
  Field_ToggleRoomSwitchEntry(nullptr);
  for (const auto value : {0, 1, 2, -1, -32768, 32767}) {
    Field_MpdLockTrigger record{};
    record.trigger.tileX = -1;
    record.trigger.tileZ = -1;
    record.trigger.floorLayer = -1;
    record.trigger.kind = -1;
    record.trigger.lockState.switchValue = static_cast<std::int16_t>(value);
    record.trigger.markerFade = -1;
    Field_ToggleRoomSwitchEntry(&record);
    CHECK(record.trigger.lockState.switchValue == (value == 0 ? 1 : 0));
    CHECK(record.trigger.markerFade == 0);
  }
}

TEST_CASE(
    "Native gimmick placement entry preserves the original module address",
    "[gimmick-placement][gimmick-placement-core][field]") {
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800A0024) ==
        reinterpret_cast<const void*>(&Field_GimmickPlacementEntry));
}

TEST_CASE("Typed gimmick placement refuses slots before record access",
          "[gimmick-placement][gimmick-placement-core][field]") {
  RecordingGimmickHost host;
  for (const auto slot : {std::numeric_limits<std::int32_t>::min(), -1, 16,
                          std::numeric_limits<std::int32_t>::max()}) {
    CHECK_FALSE(VS::Field::GimmickPlacement(slot, host));
  }
  CHECK(host.reads == 0);
  CHECK(host.support_calls == 0);
  for (const auto slot : {0, 15}) {
    CHECK_FALSE(VS::Field::GimmickPlacement(slot, host));
  }
  CHECK(host.reads == 2);
  CHECK(host.support_calls == 0);
}

TEST_CASE("Typed gimmick placement refuses motion before support sampling",
          "[gimmick-placement][gimmick-placement-core][field]") {
  RecordingGimmickHost host;
  for (const auto mode : {0, 1, -1, 32767, -32768}) {
    VS::Field::GimmickPlacementState state{
        .attached = mode == 0,
        .movement_mode = static_cast<std::int16_t>(mode)};
    host.states[0] = state;
    CHECK_FALSE(VS::Field::GimmickPlacement(0, host));
  }
  host.states[0] = VS::Field::GimmickPlacementState{.falling = true};
  CHECK_FALSE(VS::Field::GimmickPlacement(0, host));
  CHECK(host.support_calls == 0);
}

TEST_CASE("Typed gimmick placement matches original word stores and rounding",
          "[gimmick-placement][gimmick-placement-core][field]") {
  RecordingGimmickHost host;
  VS::Field::GimmickPlacementState state{
      .tile_x = 0xFF,
      .tile_z = 9,
      .orientation = 0x81,
      .position_x = -1234,
      .position_z = 2345,
      .position_tail = std::bit_cast<std::int16_t>(std::uint16_t{0xFEDC})};
  for (const auto support :
       {std::numeric_limits<std::int32_t>::min(), -3000, -1, 0, 1, 3000,
        std::numeric_limits<std::int32_t>::max()}) {
    host.support_height = support;
    for (const auto height :
         {-32768, -129, -128, -127, -1, 0, 1, 127, 128, 129, 32767}) {
      state.position_y = static_cast<std::int16_t>(height);
      host.states[0] = state;
      CheckTypedGimmickPlacement(host, state);
    }
  }
  CHECK(host.support_calls == 77);
}

TEST_CASE("Gimmick snapshots refuse invalid and absent slots without writing",
          "[gimmick-placement][field]") {
  SnapshotFixture fixture;
  auto output = SentinelSnapshot();
  const auto before = std::bit_cast<SnapshotBytes>(output);
  for (int slot = -2; slot < 18; ++slot) {
    CHECK(Field_GimmickPlacementEntry(slot, &output) == -1);
    CHECK(std::bit_cast<SnapshotBytes>(output) == before);
  }
  Field_gimmickInstances[0] = fixture.object.get();
  CHECK(Field_GimmickPlacementEntry(0, nullptr) == -1);
}

TEST_CASE("Gimmick snapshots reject attached falling and moving records",
          "[gimmick-placement][field]") {
  SnapshotFixture fixture;
  Field_gimmickInstances[0] = fixture.object.get();
  auto output = SentinelSnapshot();
  const auto before = std::bit_cast<SnapshotBytes>(output);
  for (int refusal = 0; refusal < 3; ++refusal) {
    fixture.object->parent = refusal == 0 ? 0 : 0xFF;
    fixture.object->objectInfo.isFalling = refusal == 1;
    fixture.object->mode = static_cast<std::int16_t>(refusal == 2);
    CHECK(Field_GimmickPlacementEntry(0, &output) == -1);
    CHECK(std::bit_cast<SnapshotBytes>(output) == before);
  }
}

TEST_CASE("Gimmick snapshots preserve packed coordinates and height rounding",
          "[gimmick-placement][field]") {
  SnapshotFixture fixture;
  auto& object = *fixture.object;
  object.tileX = 5;
  object.tileZ = 9;
  object.objectInfo.material = 3;
  object.positionX = -1234;
  object.positionZ = 2345;
  object.reserved22 = 0x1357;
  for (const int slot : {0, 15}) {
    Field_gimmickInstances[slot] = &object;
    for (const int height : {-129, -128, -127, 0, 127, 128, 129}) {
      object.positionY = static_cast<std::int16_t>(height);
      CheckValidSnapshot(slot, object);
    }
    Field_gimmickInstances[slot] = nullptr;
  }
}

TEST_CASE(
    "Native room triggers consume the bounded stationary gimmick snapshot",
    "[gimmick-placement][room-triggers][event-vm-live]") {
  TriggerFixture fixture;
  auto& object = *fixture.snapshots.object;
  object.tileX = 5;
  object.tileZ = 9;
  Field_gimmickInstances[0] = &object;
  PublishTrigger({4, 0, 255, 255, 2, 9, 0, 11, 0, 5, 9, 0, 0, 255});
  const VS::Event::RoomTriggerQuery query{
      .request = VS::Event::RoomTriggerRequest::kPlayerTile};
  CHECK(VS::Event::EvaluateLiveRoomEventTriggers(query) ==
        VS::Event::RoomTriggerResult::kStarted);
  CHECK(VS::Event::LiveScriptVm().EventId() == 9);
  CHECK(VS::Event::LiveScriptVm().Source() ==
        VS::Event::EventSource::kEventFile);
  VS::Event::LiveScriptVm().SetPhase(VS::Event::PlaybackPhase::kIdle);
  object.mode = 1;
  CHECK(VS::Event::EvaluateLiveRoomEventTriggers(query) ==
        VS::Event::RoomTriggerResult::kNoMatch);
  Field_gimmickInstances[0] = nullptr;
  CHECK(VS::Event::EvaluateLiveRoomEventTriggers(query) ==
        VS::Event::RoomTriggerResult::kNoMatch);
}

TEST_CASE("Native room trigger contexts preserve signed reference semantics",
          "[room-triggers][event-vm-live]") {
  TriggerFixture fixture;
  PublishTrigger({4, 0, 255, 255, 8, 7, 0, 6, 0, 128, 0, 32, 255});
  const VS::Event::RoomTriggerQuery query{
      .request = VS::Event::RoomTriggerRequest::kActorState,
      .context = VS::Event::TriggerActorPair{-32768, 8192}};
  CHECK(VS::Event::EvaluateLiveRoomEventTriggers(query) ==
        VS::Event::RoomTriggerResult::kStarted);
  CHECK(VS::Event::LiveScriptVm().EventId() == 7);
}

TEST_CASE("Native room trigger status queries require no section bytes",
          "[room-triggers][event-vm-live]") {
  TriggerFixture fixture;
  VS::Event::PublishLiveRoomScriptSections({});
  const VS::Event::RoomTriggerQuery query{
      .request = VS::Event::RoomTriggerRequest::kPlaybackStatus};
  CHECK(VS::Event::EvaluateLiveRoomEventTriggers(query) ==
        VS::Event::RoomTriggerResult::kNoMatch);
  VS::Event::LiveScriptVm().SetPhase(VS::Event::PlaybackPhase::kRunning);
  CHECK(VS::Event::EvaluateLiveRoomEventTriggers(query) ==
        VS::Event::RoomTriggerResult::kBusy);
}

TEST_CASE(
    "C trigger entry preserves tile contexts and refuses missing contexts",
    "[room-triggers][event-vm-live]") {
  TriggerFixture fixture;
  PublishTrigger({4, 0, 255, 255, 4, 12, 0, 8, 7, 9, 255});
  std::array<std::uint8_t, 4> tile{7, 0, 9, 0};
  CHECK(Event_EvaluateRoomEventTriggersEntry(4, 0, tile.data(), 1) == 1);
  CHECK(VS::Event::LiveScriptVm().EventId() == 12);
  VS::Event::LiveScriptVm().SetPhase(VS::Event::PlaybackPhase::kIdle);
  CHECK(Event_EvaluateRoomEventTriggersEntry(4, 0, nullptr, 1) == 0);
}
