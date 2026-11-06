#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Content/CdDriveLive.hpp"
#include "Content/DiscReaderLive.hpp"
#include "Event/ScriptVmLive.hpp"

extern "C" {
#include "DiscQueue.h"
#include "Event/ScriptActors.h"
#include "Event/ScriptVm.h"
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
  std::array<std::uint8_t, 256> sizes{};
  char phase = Event_playbackPhase;
  char confirmed = Event_isStartConfirmed;
  std::int16_t finished = Event_isMainThreadFinished;
  std::int16_t chained = Event_isChainedEventLoaded;
  char skip = Event_skipState;

  LiveVmFixture() {
    std::ranges::copy(Content_discRequestSlots, slots.begin());
    std::ranges::copy(Event_kOpcodeHandlers, handlers.begin());
    std::ranges::copy(Event_kOpcodeSizes, sizes.begin());
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
    std::ranges::copy(sizes, Event_kOpcodeSizes);
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

event::ScriptLocation MountScript(Bytes bytes) {
  REQUIRE(event::MountLiveScriptSource(
              event::ScriptRegion::kEventScript,
              std::make_shared<const Bytes>(std::move(bytes)))
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
