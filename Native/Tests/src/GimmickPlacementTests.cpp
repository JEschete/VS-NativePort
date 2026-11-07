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
#include "Field/GimmickAnimationQueue.hpp"
#include "Field/GimmickAnimationQueueLive.h"
#include "Field/GimmickPlacement.hpp"
#include "Field/RoomSwitches.hpp"
#include "Field/RoomSwitchesLive.h"

extern "C" {
#include "Event/Triggers.h"
#include "Field/GimmickPlacementLive.h"
#include "Field/Gimmicks.h"
}

namespace {

using SnapshotBytes = std::array<std::byte, sizeof(Field_GimmickSnapshot)>;

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

}  // namespace

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
