#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Content/CdDriveLive.hpp"
#include "Content/DiscReaderLive.hpp"
#include "Event/ScriptVmLive.hpp"
#include "Event/TriggersLive.hpp"
#include "State/ScriptFlagsLive.hpp"

extern "C" {
#include "DiscQueue.h"
#include "Event/ScriptActors.h"
#include "Event/ScriptVm.h"
#include "Event/Triggers.h"
}

namespace {
namespace event = VS::Event;
namespace content = VS::Content;
using Bytes = std::vector<std::uint8_t>;
using Handler = std::intptr_t (*)(std::uint8_t*, std::int16_t);

struct LiveVmFixture {
  std::array<Content_DiscQueueSlot, 32> slots{};
  Content_DiscRequestCounts counts = Content_discRequestCounts;
  Content_DiscDrive drive = Content_discDriveState;
  std::array<Handler, 256> handlers{};
  std::array<Handler, 256> skip_handlers{};
  std::array<std::uint8_t, 256> sizes{};
  std::array<char, 64> loop_counters{};
  std::uintptr_t subroutine_return = Event_subroutineReturnCursor;
  char phase = Event_playbackPhase;
  char confirmed = Event_isStartConfirmed;
  std::int16_t finished = Event_isMainThreadFinished;
  std::int16_t chained = Event_isChainedEventLoaded;
  char skip = Event_skipState;

  LiveVmFixture() {
    std::ranges::copy(Content_discRequestSlots, slots.begin());
    std::ranges::copy(Event_kOpcodeHandlers, handlers.begin());
    std::ranges::copy(Event_kSkipOpcodeHandlers, skip_handlers.begin());
    std::ranges::copy(Event_kOpcodeSizes, sizes.begin());
    std::ranges::copy(Event_loopCounters, loop_counters.begin());
    content::InitializeLiveDiscQueue();
    Content_discDriveState = {};
    Content_discDriveState.state = Content_DiscDriveState_Idle;
    event::ResetLiveScriptRuntime();
    Event_skipState = 0;
    std::ranges::fill(Event_kOpcodeSizes, 1);
  }
  ~LiveVmFixture() {
    event::ResetLiveScriptRuntime();
    std::ranges::copy(slots, Content_discRequestSlots);
    Content_discRequestCounts = counts;
    Content_discDriveState = drive;
    std::ranges::copy(handlers, Event_kOpcodeHandlers);
    std::ranges::copy(skip_handlers, Event_kSkipOpcodeHandlers);
    std::ranges::copy(sizes, Event_kOpcodeSizes);
    std::ranges::copy(loop_counters, Event_loopCounters);
    Event_subroutineReturnCursor = subroutine_return;
    Event_playbackPhase = phase;
    Event_isStartConfirmed = confirmed;
    Event_isMainThreadFinished = finished;
    Event_isChainedEventLoaded = chained;
    Event_skipState = skip;
  }
  LiveVmFixture(const LiveVmFixture&) = delete;
  LiveVmFixture& operator=(const LiveVmFixture&) = delete;
  LiveVmFixture(LiveVmFixture&&) = delete;
  LiveVmFixture& operator=(LiveVmFixture&&) = delete;
};

struct LiveRoomFixture {
  LiveVmFixture vm;
  std::int16_t change = Event_roomChangeState;
  std::int16_t entry = Event_hasRoomEntryScript;
  std::array<std::uint8_t, 32> flags{};
  std::uint8_t loading = VS::State::LiveScriptFlag(0xA9);

  LiveRoomFixture() {
    for (std::size_t index = 0; index < flags.size(); ++index) {
      flags[index] = VS::State::LiveScriptFlag(32 + static_cast<int>(index));
    }
    Event_roomChangeState = 0;
    Event_hasRoomEntryScript = 0;
  }
  ~LiveRoomFixture() {
    Event_roomChangeState = change;
    Event_hasRoomEntryScript = entry;
    for (std::size_t index = 0; index < flags.size(); ++index) {
      VS::State::SetLiveScriptFlag(32 + static_cast<int>(index), flags[index]);
    }
    VS::State::SetLiveScriptFlag(0xA9, loading);
  }
  LiveRoomFixture(const LiveRoomFixture&) = delete;
  LiveRoomFixture& operator=(const LiveRoomFixture&) = delete;
  LiveRoomFixture(LiveRoomFixture&&) = delete;
  LiveRoomFixture& operator=(LiveRoomFixture&&) = delete;
};

event::ScriptLocation MountScript(
    Bytes bytes, std::weak_ptr<const Bytes>* lifetime = nullptr) {
  const auto owner = std::make_shared<const Bytes>(std::move(bytes));
  if (lifetime != nullptr) {
    *lifetime = owner;
  }
  REQUIRE(event::MountLiveScriptSource(event::ScriptRegion::kEventScript, owner)
              .has_value());
  const auto start =
      event::LeaseLiveScriptSource(event::ScriptRegion::kEventScript)
          .value()
          .At(0);
  if (!start) {
    throw std::logic_error("Mounted test script has no first instruction");
  }
  return *start;
}

Content_DiscQueueSlot& PendingRequest() {
  const auto found =
      std::ranges::find_if(Content_discRequestSlots, [](const auto& request) {
        return request.state == Content_DiscRequestState_Enqueued;
      });
  REQUIRE(found != std::end(Content_discRequestSlots));
  return *found;
}

void CompleteSyntheticFile(std::uint8_t first) {
  auto& request = PendingRequest();
  REQUIRE(request.cdFile.size == static_cast<int>(event::kEventScriptBytes));
  REQUIRE(request.vram != nullptr);
  auto bytes = std::span{static_cast<std::uint8_t*>(request.vram),
                         event::kEventScriptBytes};
  std::ranges::fill(bytes, first);
  request.state = Content_DiscRequestState_Loaded;
}

void CheckMountedByte(std::uint8_t value, std::uint64_t generation) {
  const auto source =
      event::LeaseLiveScriptSource(event::ScriptRegion::kEventScript).value();
  CHECK(source.Generation() == generation);
  REQUIRE_FALSE(source.Bytes().empty());
  CHECK(source.Bytes().front() == value);
}

std::array<event::ScriptLocation, 2> observed{};
int observations = 0;

event::ScriptFlow ObserveJump(const event::ScriptInstruction& instruction,
                              event::ScriptVmHost& host) {
  observed.at(static_cast<std::size_t>(observations++)) = instruction.location;
  return event::ScriptJumpForward(instruction, host);
}

event::ScriptFlow ObserveStop(const event::ScriptInstruction& instruction,
                              event::ScriptVmHost& host) {
  observed.at(static_cast<std::size_t>(observations++)) = instruction.location;
  return event::CommandStopScriptContext(instruction, host);
}

std::intptr_t JumpBridge(std::uint8_t* instruction, std::int16_t thread) {
  return event::DispatchLiveScriptControlOpcode(instruction, thread,
                                                ObserveJump);
}

std::intptr_t StopBridge(std::uint8_t* instruction, std::int16_t thread) {
  return event::DispatchLiveScriptControlOpcode(instruction, thread,
                                                ObserveStop);
}

std::intptr_t PointerResult(std::uint8_t*, std::int16_t) { return 0x12345678; }

void BindTestData(event::ScriptLocation start) {
  auto& host = event::LiveScriptVm();
  host.SetDialogStrings(start);
  ++start.offset;
  host.SetTextBoxFrameStyle(9, start);
  ++start.offset;
  host.SetVibrationPattern(7, start);
}

void CheckDataBytes(const event::ScriptDataLease& lease, std::uint8_t first,
                    std::size_t length) {
  const auto bytes = lease.Bytes();
  if (!bytes || bytes->empty()) {
    FAIL("Test data lease has no bytes");
    return;
  }
  CHECK(bytes->size() == length);
  CHECK(bytes->front() == first);
}

void CheckDataError(
    const std::expected<event::ScriptDataLease, event::ScriptSourceError>& data,
    event::ScriptSourceError error) {
  REQUIRE_FALSE(data.has_value());
  if (!data) {
    CHECK(data.error() == error);
  }
}

bool nested_finished = false;
std::intptr_t NestedJumpBridge(std::uint8_t* instruction, std::int16_t thread) {
  nested_finished = !event::ExecuteLiveScriptThread(1);
  return event::DispatchLiveScriptControlOpcode(instruction, thread,
                                                event::ScriptJumpForward);
}

void MarkRoomStopPending() {
  auto& host = event::LiveScriptVm();
  host.SetRoomChangeState(3);
  host.SetPhase(event::PlaybackPhase::kRunning);
  host.SetStartConfirmed(true);
  VS::State::SetLiveScriptFlag(32, 0x55);
  VS::State::SetLiveScriptFlag(0xA9, 0x66);
}

void CheckRoomEntryState(event::PlaybackPhase phase, std::int16_t change,
                         bool confirmed) {
  auto& host = event::LiveScriptVm();
  CHECK(host.RoomChangeState() == change);
  CHECK(host.Phase() == phase);
  CHECK(host.StartConfirmed() == confirmed);
  CHECK(Event_hasRoomEntryScript == 1);
}

void CheckContinuingRoomSources(std::uint64_t main_generation,
                                std::uint64_t door_generation) {
  const auto room = event::LiveRoomScriptSections();
  REQUIRE(room);
  if (room) {
    CHECK(room->EntryScript().empty());
  }
  CheckMountedByte(0x11, main_generation);
  CHECK(event::LeaseLiveScriptSource(event::ScriptRegion::kDoorEvents)
            .value()
            .Generation() == door_generation);
}

struct MountedRoomSources {
  std::weak_ptr<const event::RoomEventSections> room;
  std::uint64_t main_generation;
  std::uint64_t door_generation;
};

MountedRoomSources ImportTestRoomSources() {
  Bytes entry{0x11};
  const Bytes doors{0x22};
  VS::State::SetLiveScriptFlag(32, 0x7F);
  REQUIRE(event::ImportLiveRoomScripts(
              {.entry_script = entry, .doors = doors, .first_entry = true}) ==
          event::RoomEventLoadResult::kReady);
  CHECK(VS::State::LiveScriptFlag(32) == 0x7F);
  const auto main_generation =
      event::LeaseLiveScriptSource(event::ScriptRegion::kEventScript)
          .value()
          .Generation();
  entry.front() = 0x99;
  CheckMountedByte(0x11, main_generation);
  REQUIRE(event::LiveScriptVm().BindRoomDoorSource());
  const auto door_generation =
      event::LeaseLiveScriptSource(event::ScriptRegion::kDoorEvents)
          .value()
          .Generation();
  return {event::LiveRoomScriptSections(), main_generation, door_generation};
}
}  // namespace

TEST_CASE("Live VM dispatch carries typed jumps without pointer results",
          "[event-vm-live]") {
  const LiveVmFixture fixture;
  const auto start = MountScript({0x42, 3, 0, 0xF0});
  observations = 0;
  Event_kOpcodeHandlers[0x42] = JumpBridge;
  Event_kOpcodeHandlers[0xF0] = StopBridge;
  Event_kOpcodeSizes[0x42] = 3;
  event::LiveScriptVm().SetThreadCursor(0, start);
  CHECK_FALSE(event::ExecuteLiveScriptThread(0));
  CHECK(observations == 2);
  CHECK(observed[0] == start);
  CHECK(observed[1] ==
        event::ScriptLocation{start.region, 3, start.generation});
  CHECK_FALSE(event::LiveScriptVm().ThreadCursor(0).Present());
}

TEST_CASE("Live VM routes the original control handler through typed dispatch",
          "[event-vm-live]") {
  const LiveVmFixture fixture;
  const auto start = MountScript({0x42, 3, 0, 0xF0});
  observations = 0;
  Event_kOpcodeHandlers[0x42] = Event_ScriptJumpForward;
  Event_kOpcodeHandlers[0xF0] = StopBridge;
  Event_kOpcodeSizes[0x42] = 3;
  event::LiveScriptVm().SetThreadCursor(0, start);
  CHECK_FALSE(event::ExecuteLiveScriptThread(0));
  REQUIRE(observations == 1);
  CHECK(observed[0] ==
        event::ScriptLocation{start.region, 3, start.generation});
}

TEST_CASE("Live VM selects typed control handlers from the skip table",
          "[event-vm-live]") {
  const LiveVmFixture fixture;
  const auto start = MountScript({0x42, 3, 0, 0xF0});
  observations = 0;
  Event_skipState = 2;
  Event_kOpcodeHandlers[0x42] = PointerResult;
  Event_kSkipOpcodeHandlers[0x42] = Event_ScriptJumpForward;
  Event_kSkipOpcodeHandlers[0xF0] = StopBridge;
  Event_kOpcodeSizes[0x42] = 3;
  event::LiveScriptVm().SetThreadCursor(0, start);
  CHECK_FALSE(event::ExecuteLiveScriptThread(0));
  REQUIRE(observations == 1);
  CHECK(observed[0] ==
        event::ScriptLocation{start.region, 3, start.generation});
}

TEST_CASE("Live VM original loop handler obeys the bounded counter contract",
          "[event-vm-live]") {
  const LiveVmFixture fixture;
  const auto start = MountScript({0xE1, 20, 7, 0xF0});
  observations = 0;
  Event_loopCounters[20] = 0x55;
  Event_kOpcodeHandlers[0xE1] = Event_ScriptSetLoopCounter;
  Event_kOpcodeHandlers[0xF0] = StopBridge;
  Event_kOpcodeSizes[0xE1] = 3;
  event::LiveScriptVm().SetThreadCursor(0, start);
  CHECK_FALSE(event::ExecuteLiveScriptThread(0));
  CHECK(Event_loopCounters[20] == 0x55);
  CHECK(observations == 1);
}

TEST_CASE("Live VM original return handler uses the owned native location",
          "[event-vm-live]") {
  const LiveVmFixture fixture;
  const auto start = MountScript({0xE1, 0xF3, 0xF0});
  observations = 0;
  Event_subroutineReturnCursor = 0;
  Event_kOpcodeHandlers[0xE1] = Event_ScriptReturnToSavedAddress;
  Event_kOpcodeHandlers[0xF3] = StopBridge;
  Event_kOpcodeHandlers[0xF0] = StopBridge;
  auto& host = event::LiveScriptVm();
  host.SetSubroutineReturn({start.region, 2, start.generation});
  host.SetThreadCursor(0, start);
  CHECK_FALSE(event::ExecuteLiveScriptThread(0));
  REQUIRE(observations == 1);
  CHECK(observed[0] ==
        event::ScriptLocation{start.region, 2, start.generation});
}

TEST_CASE("Live VM refuses pointer results and detached control adapters",
          "[event-vm-live]") {
  const LiveVmFixture fixture;
  std::array<std::uint8_t, 3> detached{0x42, 0, 0};
  CHECK(JumpBridge(detached.data(), 0) == 3);
  const auto start = MountScript({0x42, 0, 0});
  Event_kOpcodeHandlers[0x42] = PointerResult;
  event::LiveScriptVm().SetThreadCursor(0, start);
  CHECK_FALSE(event::ExecuteLiveScriptThread(0));
  CHECK_FALSE(event::LiveScriptVm().ThreadCursor(0).Present());
}

TEST_CASE("Live VM file publication waits for completed owned staging",
          "[event-vm-live][disc-lifetime]") {
  const LiveVmFixture fixture;
  const auto start = MountScript({0x55});
  auto& host = event::LiveScriptVm();
  REQUIRE(host.QueueEventFileLoad(0, event::EventBuffer::kScript));
  CHECK(host.EventFile() == event::EventFileStatus::kLoading);
  CheckMountedByte(0x55, start.generation);
  CompleteSyntheticFile(0xAA);
  host.ReleaseEventFile();
  CHECK(host.EventFile() == event::EventFileStatus::kNone);
  CheckMountedByte(0xAA, start.generation + 1);
}

TEST_CASE("Live VM cancellation stops deferred writes and releases its slot",
          "[event-vm-live][disc-lifetime]") {
  const LiveVmFixture fixture;
  const auto start = MountScript({0x55});
  auto& host = event::LiveScriptVm();
  REQUIRE(host.QueueEventFileLoad(0, event::EventBuffer::kScript));
  auto& request = PendingRequest();
  REQUIRE(content::BeginLiveDiscRead(-1, 4, request.vram));
  host.CancelEventFile();
  CHECK(host.EventFile() == event::EventFileStatus::kNone);
  CHECK(request.state == Content_DiscRequestState_Free);
  CHECK(Content_discDriveState.destination == nullptr);
  content::UpdateLiveDiscCommands();
  CHECK(content::LiveDiscState() == Content_DiscDriveState_Idle);
  CheckMountedByte(0x55, start.generation);
}

TEST_CASE("Live VM cancellation cannot release a reused terminal queue slot",
          "[event-vm-live][disc-lifetime]") {
  const LiveVmFixture fixture;
  auto& host = event::LiveScriptVm();
  REQUIRE(host.QueueEventFileLoad(0, event::EventBuffer::kScript));
  auto& original = PendingRequest();
  const auto file = original.cdFile;
  content::ReleaseLiveDiscRequest(&original);
  auto* replacement = content::AllocateLiveDiscRequest(&file);
  REQUIRE(replacement == &original);
  std::array<std::byte, event::kEventScriptBytes> other{};
  content::EnqueueLiveDiscRequest(replacement, other.data());
  REQUIRE(content::CancelLiveDiscWrites(other));
  CHECK(host.EventFile() == event::EventFileStatus::kFailed);
  host.CancelEventFile();
  CHECK(replacement->state == Content_DiscRequestState_Error);
  content::ReleaseLiveDiscRequest(replacement);
}

TEST_CASE("Live VM chained storage publishes only after completed adoption",
          "[event-vm-live][disc-lifetime]") {
  const LiveVmFixture fixture;
  const auto start = MountScript({0x55});
  auto& host = event::LiveScriptVm();
  REQUIRE(host.AllocateChainedEventBuffer());
  REQUIRE(host.QueueEventFileLoad(1, event::EventBuffer::kChained));
  CHECK_FALSE(host.AdoptChainedEvent());
  CompleteSyntheticFile(0x77);
  host.ReleaseEventFile();
  CheckMountedByte(0x55, start.generation);
  REQUIRE(host.AdoptChainedEvent());
  CheckMountedByte(0x77, start.generation + 1);
}

TEST_CASE("Live VM data leases retain exact bytes across rebinding and reset",
          "[event-vm-live][script-sources]") {
  const LiveVmFixture fixture;
  std::weak_ptr<const Bytes> old_lifetime;
  BindTestData(MountScript({0x11, 0x22, 0x33}, &old_lifetime));
  std::array leases{event::LeaseLiveDialogStrings().value(),
                    event::LeaseLiveTextBoxFrame(9).value(),
                    event::LeaseLiveVibrationPattern(7).value()};
  std::weak_ptr<const Bytes> new_lifetime;
  BindTestData(MountScript({0x44, 0x55, 0x66}, &new_lifetime));
  CheckDataBytes(event::LeaseLiveDialogStrings().value(), 0x44, 3);
  CheckDataBytes(event::LeaseLiveTextBoxFrame(9).value(), 0x55, 2);
  CheckDataBytes(event::LeaseLiveVibrationPattern(7).value(), 0x66, 1);
  event::ResetLiveScriptRuntime();
  CHECK(new_lifetime.expired());
  CHECK_FALSE(old_lifetime.expired());
  CheckDataBytes(leases[0], 0x11, 3);
  CheckDataBytes(leases[1], 0x22, 2);
  CheckDataBytes(leases[2], 0x33, 1);
  leases = {};
  CHECK(old_lifetime.expired());
}

TEST_CASE("Live VM data access rejects absent slots and invalid indices",
          "[event-vm-live][script-sources]") {
  const LiveVmFixture fixture;
  CheckDataError(event::LeaseLiveDialogStrings(),
                 event::ScriptSourceError::kMissingOwner);
  CheckDataError(event::LeaseLiveTextBoxFrame(9),
                 event::ScriptSourceError::kMissingOwner);
  CheckDataError(event::LeaseLiveVibrationPattern(7),
                 event::ScriptSourceError::kMissingOwner);
  for (const int style : {-1, event::kTextBoxFrameStyleCount}) {
    CheckDataError(event::LeaseLiveTextBoxFrame(style),
                   event::ScriptSourceError::kExtent);
  }
  for (const int pattern : {-1, event::kVibrationPatternSlots}) {
    CheckDataError(event::LeaseLiveVibrationPattern(pattern),
                   event::ScriptSourceError::kExtent);
  }
}

TEST_CASE("Live VM data bindings reject stale and out of range locations",
          "[event-vm-live][script-sources]") {
  const LiveVmFixture fixture;
  auto location = MountScript({0x11, 0x22, 0x33});
  BindTestData(location);
  auto lease = event::LeaseLiveDialogStrings().value();
  ++location.generation;
  event::LiveScriptVm().SetDialogStrings(location);
  CheckDataError(event::LeaseLiveDialogStrings(),
                 event::ScriptSourceError::kMissingOwner);
  CheckDataBytes(lease, 0x11, 3);
  lease.location = location;
  CHECK_FALSE(lease.Bytes().has_value());
  --location.generation;
  location.offset = 3;
  event::LiveScriptVm().SetTextBoxFrameStyle(9, location);
  CheckDataError(event::LeaseLiveTextBoxFrame(9),
                 event::ScriptSourceError::kMissingOwner);
  lease.location = location;
  CHECK_FALSE(lease.Bytes().has_value());
  event::LiveScriptVm().SetVibrationPattern(7, {});
  CheckDataError(event::LeaseLiveVibrationPattern(7),
                 event::ScriptSourceError::kMissingOwner);
}

TEST_CASE("Live VM restores the outer typed invocation after nested dispatch",
          "[event-vm-live]") {
  const LiveVmFixture fixture;
  const auto start = MountScript({0x42, 3, 0, 0xF0, 0xF0});
  observations = 0;
  nested_finished = false;
  Event_kOpcodeHandlers[0x42] = NestedJumpBridge;
  Event_kOpcodeHandlers[0xF0] = StopBridge;
  Event_kOpcodeSizes[0x42] = 3;
  auto& host = event::LiveScriptVm();
  host.SetThreadCursor(0, start);
  host.SetThreadCursor(1, {start.region, 4, start.generation});
  CHECK_FALSE(event::ExecuteLiveScriptThread(0));
  CHECK(nested_finished);
  CHECK(observations == 2);
  CHECK(observed[0] ==
        event::ScriptLocation{start.region, 4, start.generation});
  CHECK(observed[1] ==
        event::ScriptLocation{start.region, 3, start.generation});
  CHECK_FALSE(host.ThreadCursor(0).Present());
  CHECK_FALSE(host.ThreadCursor(1).Present());
}

TEST_CASE("Live room import owns entry data and preserves ongoing room scripts",
          "[event-vm-live][room-triggers][script-sources]") {
  const LiveRoomFixture fixture;
  const auto mounted = ImportTestRoomSources();
  Event_roomChangeState = 1;
  const Bytes ignored_entry(event::kEventScriptBytes + 1, 0x55);
  const Bytes next_doors{0x33};
  REQUIRE(event::ImportLiveRoomScripts(
              {.entry_script = ignored_entry, .doors = next_doors}) ==
          event::RoomEventLoadResult::kContinuingEvent);
  CHECK(VS::State::LiveScriptFlag(32) == 0);
  CheckRoomEntryState(event::PlaybackPhase::kIdle, 2, false);
  const std::weak_ptr new_room = event::LiveRoomScriptSections();
  CheckContinuingRoomSources(mounted.main_generation, mounted.door_generation);
  event::FreeLiveRoomScripts();
  CHECK_FALSE(event::LiveRoomScriptSections());
  CHECK(new_room.expired());
  CHECK_FALSE(mounted.room.expired());
  event::ResetLiveScriptRuntime();
  CHECK(mounted.room.expired());
}

TEST_CASE("Live room import rejects invalid entry before changing state",
          "[event-vm-live][room-triggers][script-sources]") {
  const LiveRoomFixture fixture;
  const Bytes entry{0x11};
  REQUIRE(event::ImportLiveRoomScripts({.entry_script = entry}) ==
          event::RoomEventLoadResult::kReady);
  const auto room = event::LiveRoomScriptSections();
  const auto generation =
      event::LeaseLiveScriptSource(event::ScriptRegion::kEventScript)
          .value()
          .Generation();
  MarkRoomStopPending();
  const Bytes oversized(event::kEventScriptBytes + 1, 0);
  const auto refused =
      event::ImportLiveRoomScripts({.entry_script = oversized});
  REQUIRE_FALSE(refused.has_value());
  if (!refused) {
    CHECK(refused.error().reason ==
          event::RoomEventRefusal::kEntryScriptExtent);
  }
  CHECK(event::LiveRoomScriptSections() == room);
  CheckMountedByte(0x11, generation);
  CheckRoomEntryState(event::PlaybackPhase::kRunning, 3, true);
  CHECK(VS::State::LiveScriptFlag(32) == 0x55);
  CHECK(VS::State::LiveScriptFlag(0xA9) == 0x66);
}

TEST_CASE("Live room import commits stop transitions and accepts empty rooms",
          "[event-vm-live][room-triggers][script-sources]") {
  const LiveRoomFixture fixture;
  std::weak_ptr<const Bytes> previous;
  const auto old_source = MountScript({0x11}, &previous);
  MarkRoomStopPending();
  const Bytes entry{0x22};
  REQUIRE(event::ImportLiveRoomScripts({.entry_script = entry}) ==
          event::RoomEventLoadResult::kReady);
  CHECK(previous.expired());
  CheckMountedByte(0x22, old_source.generation + 1);
  CheckRoomEntryState(event::PlaybackPhase::kIdle, 0, false);
  CHECK(VS::State::LiveScriptFlag(32) == 0);
  CHECK(VS::State::LiveScriptFlag(0xA9) == 0);
  REQUIRE(event::ImportLiveRoomScripts({}) ==
          event::RoomEventLoadResult::kReady);
  CHECK_FALSE(event::LiveRoomScriptSections());
  CHECK(Event_hasRoomEntryScript == 0);
  CheckMountedByte(0x22, old_source.generation + 1);
}
