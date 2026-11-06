#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Content/CdDrive.hpp"
#include "Content/CdDriveLive.hpp"
#include "Content/DiscReaderLive.hpp"
#include "State/GameClockLive.hpp"

extern "C" {
#include "DiscQueue.h"
}

namespace {
namespace Content = VS::Content;
using Content::DiscDestination;
using Content::DiscDriveState;
using Content::DiscFileExtent;
using Content::DiscInterrupt;
using Content::DiscReadResult;
using Content::DiscRequestState;

struct DiscRead {
  DiscFileExtent file;
  DiscDestination destination = DiscDestination::kNone;
};

// A drive that records the queue's reads. A started read keeps the drive in
// `read_state` for `read_frames` waited frames, then returns it to idle.
class FakeDiscQueueHost final : public Content::DiscQueueHost {
 public:
  [[nodiscard]] DiscDriveState DriveState() const override { return state; }
  bool BeginRead(const DiscFileExtent& file,
                 DiscDestination destination) override {
    reads.push_back({file, destination});
    if (accept_reads) {
      read_canceled = false;
      active_destination = destination;
      active_request.reset();
    }
    if (accept_reads && read_frames != 0) {
      state = read_state;
      frames_left = read_frames;
    }
    return accept_reads;
  }
  bool ReadCompleted() const override {
    return state == DiscDriveState::kIdle && !read_canceled;
  }
  bool BeginQueuedRead(std::size_t index, const DiscFileExtent& file,
                       DiscDestination destination) override {
    if (!BeginRead(file, destination)) return false;
    active_request = index;
    return true;
  }
  std::optional<std::size_t> FailedRequest() const override {
    return state == DiscDriveState::kReadError ||
                   state == DiscDriveState::kCommandFailed
               ? active_request
               : std::nullopt;
  }
  void ClearReadFailure() override {
    ++cleared_failures;
    state = DiscDriveState::kIdle;
    read_canceled = true;
    active_request.reset();
  }
  void CancelRead(DiscDestination destination) override {
    if (cancel_observer) cancel_observer();
    cancellations.push_back(destination);
    if (active_destination == destination) {
      read_canceled = true;
      active_destination = DiscDestination::kNone;
      active_request.reset();
      state = DiscDriveState::kIdle;
    }
  }
  void UpdateCommands() override { ++commands; }
  void CountFrameSinceRead() override { ++counted_frames; }
  void RestartFramesSinceRead() override { ++restarts; }
  void WaitFrame() override {
    ++waited_frames;
    if (frame_observer) frame_observer();
    if (frames_left == 0) return;
    --frames_left;
    if (frames_left == 0) state = DiscDriveState::kIdle;
  }

  DiscDriveState state = DiscDriveState::kIdle;
  bool accept_reads = true;
  bool read_canceled = false;
  DiscDriveState read_state = DiscDriveState::kReadInit;
  std::uint32_t read_frames = 0;
  std::uint32_t frames_left = 0;
  DiscDestination active_destination = DiscDestination::kNone;
  std::optional<std::size_t> active_request;
  std::vector<DiscDestination> cancellations;
  int cleared_failures = 0;
  std::function<void()> cancel_observer;
  std::function<void()> frame_observer;
  std::vector<DiscRead> reads;
  int commands = 0;
  int counted_frames = 0;
  int restarts = 0;
  std::uint32_t waited_frames = 0;
};

class FakeDiscInterruptHost final : public Content::DiscInterruptHost {
 public:
  void ReceiveSector(std::uint32_t sector) override {
    direct_sectors.push_back(sector);
  }
  void ReceiveRingSector(std::uint32_t slot) override {
    ring_slots.push_back(slot);
  }
  void StartSectorDelivery() override { ++delivery_starts; }
  void ClearReadyCallback() override { ++ready_clears; }
  void EndSectorDelivery() override { ++delivery_ends; }
  void RecordXaSector(const Content::XaSectorInfo& info) override {
    xa_sectors.push_back(info);
  }

  std::vector<std::uint32_t> direct_sectors;
  std::vector<std::uint32_t> ring_slots;
  std::vector<Content::XaSectorInfo> xa_sectors;
  int delivery_starts = 0;
  int ready_clears = 0;
  int delivery_ends = 0;
};

// DiscDestination is an opaque address handle; any address is valid.
DiscDestination DestinationAt(std::uintptr_t address) {
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  return static_cast<DiscDestination>(address);
}

bool IsFree(const Content::DiscRequest& request) {
  return request.state == DiscRequestState::kFree;
}

bool IsLoaded(const Content_DiscQueueSlot& slot) {
  return slot.state == Content_DiscRequestState_Loaded;
}

std::pair<int, int> Counts(const Content::DiscRequestQueue& queue) {
  return {queue.reading, queue.waiting};
}

bool SameRead(const DiscRead& read, const DiscFileExtent& file,
              DiscDestination destination) {
  return read.file.sector == file.sector && read.file.bytes == file.bytes &&
         read.destination == destination;
}

// Two allocated requests (sectors 10 and 20) waiting in order.
Content::DiscRequestQueue TwoWaitingRequests() {
  Content::DiscRequestQueue queue;
  queue.requests[0].file = {.sector = 10, .bytes = 2048};
  queue.requests[1].file = {.sector = 20, .bytes = 4096};
  REQUIRE(Content::EnqueueDiscRequest(queue, 0, DestinationAt(0x100)));
  REQUIRE(Content::EnqueueDiscRequest(queue, 1, DestinationAt(0x200)));
  return queue;
}

// A ring transfer of 100 sectors in `mode`.
Content::DiscTransfer RingTransfer(Content::DiscBufferMode mode) {
  Content::DiscTransfer ring;
  ring.buffer_mode = mode;
  ring.sector_count = 100;
  return ring;
}

bool SameQueue(const Content::DiscRequestQueue& left,
               const Content::DiscRequestQueue& right) {
  const auto same_request = [](const Content::DiscRequest& a,
                               const Content::DiscRequest& b) {
    return a.state == b.state && a.priority == b.priority &&
           a.file.sector == b.file.sector && a.file.bytes == b.file.bytes &&
           a.reserved == b.reserved && a.destination == b.destination;
  };
  return std::ranges::equal(left.requests, right.requests, same_request) &&
         left.reading == right.reading && left.waiting == right.waiting;
}

// Saves the live request array, counts and drive record, leaving the drive
// idle and every request free.
struct LiveDiscQueueFixture {
  std::array<Content_DiscQueueSlot, 32> saved_slots{};
  Content_DiscRequestCounts saved_counts = Content_discRequestCounts;
  Content_DiscDrive saved_drive = Content_discDriveState;
  VS::State::PlayTime saved_play_time = VS::State::LivePlayTime();
  decltype(Content_legacyDiscReadBuffer) saved_read_buffer =
      Content_legacyDiscReadBuffer;

  LiveDiscQueueFixture() {
    std::memcpy(saved_slots.data(), Content_discRequestSlots,
                sizeof Content_discRequestSlots);
    std::memset(Content_discRequestSlots, 0, sizeof Content_discRequestSlots);
    Content_discRequestCounts = Content_DiscRequestCounts{};
    Content_discDriveState.state =
        static_cast<char>(Content_DiscDriveState_Idle);
  }
  ~LiveDiscQueueFixture() {
    std::memcpy(Content_discRequestSlots, saved_slots.data(),
                sizeof Content_discRequestSlots);
    Content_discRequestCounts = saved_counts;
    Content_discDriveState = saved_drive;
    VS::State::StoreLivePlayTime(saved_play_time);
    Content_legacyDiscReadBuffer = saved_read_buffer;
  }
  LiveDiscQueueFixture(const LiveDiscQueueFixture&) = delete;
  LiveDiscQueueFixture& operator=(const LiveDiscQueueFixture&) = delete;
  LiveDiscQueueFixture(LiveDiscQueueFixture&&) = delete;
  LiveDiscQueueFixture& operator=(LiveDiscQueueFixture&&) = delete;
};

Content_DiscQueueSlot* QueueLiveRead(const Content_DiscFile& file,
                                     std::span<std::byte> destination) {
  auto* request = Content::AllocateLiveDiscRequest(&file);
  REQUIRE(request != nullptr);
  Content::EnqueueLiveDiscRequest(request, destination.data());
  return request;
}
}  // namespace

TEST_CASE(
    "Disc queue initialization frees every request and clears both counts",
    "[content-slice][content][disc-queue]") {
  Content::DiscRequestQueue queue;
  queue.requests[0].state = DiscRequestState::kReading;
  queue.requests[0].file = {.sector = 40, .bytes = 2048};
  queue.requests[31].state = DiscRequestState::kDispatched;
  queue.requests[31].priority = 9;
  queue.reading = 1;
  queue.waiting = 3;
  Content::InitializeDiscQueue(queue);
  CHECK(std::ranges::all_of(queue.requests, IsFree));
  CHECK(queue.reading == 0);
  CHECK(queue.waiting == 0);
  // _initCdQueue stores only the state halfword.
  CHECK(queue.requests[0].file.sector == 40);
  CHECK(queue.requests[31].priority == 9);
}

TEST_CASE("Disc request allocation records the file in the first free request",
          "[content-slice][content][disc-queue]") {
  Content::DiscRequestQueue queue;
  const DiscFileExtent file{.sector = 0x1234, .bytes = 5000};
  REQUIRE(Content::AllocateDiscRequest(queue, file) == std::size_t{0});
  CHECK(queue.requests[0].state == DiscRequestState::kAllocated);
  CHECK(queue.requests[0].file.sector == 0x1234);
  CHECK(queue.requests[0].file.bytes == 5000);
  queue.requests[1].state = DiscRequestState::kLoaded;
  queue.requests[3].state = DiscRequestState::kError;
  CHECK(Content::AllocateDiscRequest(queue, file) == std::size_t{2});
  CHECK(Content::AllocateDiscRequest(queue, file) == std::size_t{4});
}

TEST_CASE(
    "Disc request allocation takes the first free request and refuses a full "
    "queue",
    "[content-slice][content][disc-queue][issue-282]") {
  Content::DiscRequestQueue queue;
  const DiscFileExtent file{.sector = 0x1234, .bytes = 5000};
  queue.requests[1].state = DiscRequestState::kLoaded;
  queue.requests[3].state = DiscRequestState::kError;
  for (std::size_t i = 0; i < Content::kDiscRequestCapacity; ++i) {
    if (i == 1 || i == 3) continue;
    REQUIRE(Content::AllocateDiscRequest(queue, file) == i);
  }
  // #282: the original fell off the end here; now nothing changes and the
  // refusal is a defined empty result.
  const Content::DiscRequestQueue full = queue;
  CHECK_FALSE(Content::AllocateDiscRequest(queue, {.sector = 9, .bytes = 1})
                  .has_value());
  CHECK(SameQueue(queue, full));
  CHECK(Content::ReleaseDiscRequest(queue, 17));
  CHECK(Content::AllocateDiscRequest(queue, file) == std::size_t{17});
}

TEST_CASE("Disc request release frees busy requests and leaves the counts",
          "[content-slice][content][disc-queue]") {
  Content::DiscRequestQueue queue;
  queue.requests[2].state = DiscRequestState::kReading;
  queue.requests[3].state = DiscRequestState::kEnqueued;
  queue.requests[4].state = DiscRequestState::kLoaded;
  queue.requests[4].file = {.sector = 77, .bytes = 1};
  queue.reading = 1;
  queue.waiting = 1;
  CHECK(Content::ReleaseDiscRequest(queue, 2));
  CHECK(Content::ReleaseDiscRequest(queue, 3));
  CHECK(Content::ReleaseDiscRequest(queue, 4));
  CHECK(std::ranges::all_of(queue.requests, IsFree));
  CHECK(queue.requests[4].file.sector == 77);
  // The original leaves the counts to the queue update.
  CHECK(Counts(queue) == std::pair{1, 1});
}

TEST_CASE("Disc request operations refuse an index outside the queue",
          "[content-slice][content][disc-queue]") {
  Content::DiscRequestQueue queue = TwoWaitingRequests();
  const Content::DiscRequestQueue before = queue;
  CHECK_FALSE(
      Content::ReleaseDiscRequest(queue, Content::kDiscRequestCapacity));
  CHECK_FALSE(Content::EnqueueDiscRequest(queue, 40, DestinationAt(8)));
  CHECK_FALSE(Content::EnqueuePriorityDiscRequest(queue, 99, DestinationAt(8)));
  CHECK(SameQueue(queue, before));
}

TEST_CASE("Disc request enqueues record the destination and wait their turn",
          "[content-slice][content][disc-queue]") {
  Content::DiscRequestQueue queue;
  queue.requests[0].reserved = 0x55;
  queue.requests[1].state = DiscRequestState::kAllocated;
  REQUIRE(Content::EnqueueDiscRequest(queue, 0, DestinationAt(0x100)));
  REQUIRE(Content::EnqueueDiscRequest(queue, 1, DestinationAt(0x200)));
  CHECK(queue.requests[0].state == DiscRequestState::kEnqueued);
  CHECK(queue.requests[0].priority == 0);
  CHECK(queue.requests[0].reserved == 0);
  CHECK(queue.requests[0].destination == DestinationAt(0x100));
  CHECK(queue.requests[1].priority == 1);
  CHECK(queue.waiting == 2);
}

TEST_CASE("Priority disc request enqueues go before every waiting request",
          "[content-slice][content][disc-queue]") {
  Content::DiscRequestQueue queue = TwoWaitingRequests();
  queue.requests[5].state = DiscRequestState::kReading;
  queue.requests[5].priority = 7;
  REQUIRE(Content::EnqueuePriorityDiscRequest(queue, 2, DestinationAt(0x300)));
  CHECK(queue.requests[2].priority == 0);
  CHECK(queue.requests[2].destination == DestinationAt(0x300));
  CHECK(queue.requests[0].priority == 1);
  CHECK(queue.requests[1].priority == 2);
  // Only enqueued requests move back.
  CHECK(queue.requests[5].priority == 7);
  CHECK(queue.waiting == 3);
}

TEST_CASE("Disc queue updates wait while the drive is busy",
          "[content-slice][content][disc-queue]") {
  Content::DiscRequestQueue queue = TwoWaitingRequests();
  FakeDiscQueueHost host;
  host.state = DiscDriveState::kReadReady;
  Content::UpdateDiscQueue(queue, host);
  CHECK(host.counted_frames == 1);
  CHECK(host.commands == 1);
  CHECK(host.reads.empty());
  CHECK(queue.requests[1].priority == 1);
}

TEST_CASE("Disc queue updates start the first waiting read on an idle drive",
          "[content-slice][content][disc-queue]") {
  Content::DiscRequestQueue queue = TwoWaitingRequests();
  FakeDiscQueueHost host;
  Content::UpdateDiscQueue(queue, host);
  REQUIRE(host.reads.size() == 1);
  CHECK(SameRead(host.reads[0], {.sector = 10, .bytes = 2048},
                 DestinationAt(0x100)));
  CHECK(host.restarts == 1);
  CHECK(queue.requests[0].state == DiscRequestState::kReading);
  CHECK(queue.requests[1].state == DiscRequestState::kEnqueued);
  CHECK(queue.requests[1].priority == 0);
  CHECK(Counts(queue) == std::pair{1, 1});
}

TEST_CASE("Disc queue updates load finished reads and start the next one",
          "[content-slice][content][disc-queue]") {
  Content::DiscRequestQueue queue = TwoWaitingRequests();
  FakeDiscQueueHost host;
  Content::UpdateDiscQueue(queue, host);
  Content::UpdateDiscQueue(queue, host);
  CHECK(queue.requests[0].state == DiscRequestState::kLoaded);
  CHECK(queue.requests[1].state == DiscRequestState::kReading);
  CHECK(host.reads.size() == 2);
  CHECK(Counts(queue) == std::pair{1, 0});
  Content::UpdateDiscQueue(queue, host);
  CHECK(queue.requests[1].state == DiscRequestState::kLoaded);
  CHECK(Counts(queue) == std::pair{0, 0});
}

TEST_CASE("Disc queue updates skip the scan while both counts are zero",
          "[content-slice][content][disc-queue]") {
  Content::DiscRequestQueue queue = TwoWaitingRequests();
  FakeDiscQueueHost host;
  for (int i = 0; i < 3; ++i) Content::UpdateDiscQueue(queue, host);
  REQUIRE(Counts(queue) == std::pair{0, 0});
  queue.requests[9].state = DiscRequestState::kEnqueued;
  Content::UpdateDiscQueue(queue, host);
  CHECK(queue.requests[9].state == DiscRequestState::kEnqueued);
  CHECK(host.reads.size() == 2);
  CHECK(host.counted_frames == 4);
  CHECK(host.commands == 4);
}

TEST_CASE("Rejected queued disc reads never become loaded",
          "[content][disc-queue][disc-failure]") {
  Content::DiscRequestQueue queue = TwoWaitingRequests();
  FakeDiscQueueHost host;
  host.accept_reads = false;
  Content::UpdateDiscQueue(queue, host);
  CHECK(queue.requests[0].state == DiscRequestState::kError);
  CHECK(Counts(queue) == std::pair{0, 1});
  Content::UpdateDiscQueue(queue, host);
  CHECK(queue.requests[0].state == DiscRequestState::kError);
  CHECK(queue.requests[1].state == DiscRequestState::kError);
  CHECK(Counts(queue) == std::pair{0, 0});
}

TEST_CASE("Queued disc read errors become terminal instead of waiting forever",
          "[content][disc-queue][disc-failure]") {
  Content::DiscRequestQueue queue = TwoWaitingRequests();
  FakeDiscQueueHost host;
  host.read_frames = 1;
  host.read_state = DiscDriveState::kReadError;
  Content::UpdateDiscQueue(queue, host);
  CHECK(queue.requests[0].state == DiscRequestState::kError);
  CHECK(queue.requests[1].state == DiscRequestState::kEnqueued);
  CHECK(Counts(queue) == std::pair{0, 1});
}

TEST_CASE("Blocking disc reads report read failure rather than acceptance",
          "[content][disc-queue][disc-failure]") {
  FakeDiscQueueHost host;
  host.read_frames = 1;
  host.read_state = DiscDriveState::kReadError;
  CHECK(Content::ReadDiscFileBlocking(host, {.sector = 30, .bytes = 2048},
                                      DestinationAt(8)) ==
        DiscReadResult::kFailed);
  CHECK(host.state == DiscDriveState::kReadError);
}

TEST_CASE("Disc read failures do not consume an unrelated blocking read error",
          "[content][disc-queue][disc-failure]") {
  Content::DiscRequestQueue queue = TwoWaitingRequests();
  FakeDiscQueueHost host;
  Content::UpdateDiscQueue(queue, host);
  host.read_frames = 1;
  host.read_state = DiscDriveState::kReadError;
  REQUIRE(host.BeginRead({.sector = 99, .bytes = 8}, DestinationAt(0x900)));
  CHECK_FALSE(Content::ResolveFailedDiscRequest(queue, host));
  CHECK(queue.requests[0].state == DiscRequestState::kReading);
  CHECK(host.state == DiscDriveState::kReadError);
  CHECK(host.cleared_failures == 0);
}

TEST_CASE("Disc failed requests permit later independent requests to complete",
          "[content][disc-queue][disc-failure]") {
  Content::DiscRequestQueue queue = TwoWaitingRequests();
  FakeDiscQueueHost host;
  host.read_frames = 1;
  host.read_state = DiscDriveState::kReadError;
  Content::UpdateDiscQueue(queue, host);
  REQUIRE(queue.requests[0].state == DiscRequestState::kError);
  CHECK(host.state == DiscDriveState::kIdle);
  CHECK(host.cleared_failures == 1);
  host.read_frames = 0;
  Content::UpdateDiscQueue(queue, host);
  Content::UpdateDiscQueue(queue, host);
  CHECK(queue.requests[1].state == DiscRequestState::kLoaded);
  CHECK(queue.requests[0].state == DiscRequestState::kError);
  CHECK(Counts(queue) == std::pair{0, 0});
}

TEST_CASE("Disc cancellation stops the destination before changing requests",
          "[content][disc-queue][disc-cancel]") {
  Content::DiscRequestQueue queue = TwoWaitingRequests();
  FakeDiscQueueHost host;
  host.read_frames = 1;
  Content::UpdateDiscQueue(queue, host);
  host.cancel_observer = [&queue] {
    CHECK(queue.requests[0].state == DiscRequestState::kReading);
    CHECK(Counts(queue) == std::pair{1, 1});
  };
  REQUIRE(Content::CancelDiscWrites(queue, DestinationAt(0x100), host));
  CHECK(queue.requests[0].state == DiscRequestState::kError);
  CHECK(queue.requests[1].state == DiscRequestState::kEnqueued);
  CHECK(host.state == DiscDriveState::kIdle);
  CHECK(host.active_destination == DiscDestination::kNone);
  CHECK(Counts(queue) == std::pair{0, 1});
}

TEST_CASE("Disc cancellation repairs waiting priorities for later reads",
          "[content][disc-queue][disc-cancel]") {
  Content::DiscRequestQueue queue = TwoWaitingRequests();
  FakeDiscQueueHost host;
  REQUIRE(Content::EnqueueDiscRequest(queue, 2, DestinationAt(0x300)));
  REQUIRE(Content::CancelDiscWrites(queue, DestinationAt(0x200), host));
  CHECK(queue.requests[1].state == DiscRequestState::kError);
  CHECK(queue.requests[0].priority == 0);
  CHECK(queue.requests[2].priority == 1);
  CHECK(Counts(queue) == std::pair{0, 2});
  Content::UpdateDiscQueue(queue, host);
  Content::UpdateDiscQueue(queue, host);
  CHECK(queue.requests[0].state == DiscRequestState::kLoaded);
  CHECK(queue.requests[2].state == DiscRequestState::kReading);
}

TEST_CASE("Disc cancellation is repeatable without changing terminal requests",
          "[content][disc-queue][disc-cancel]") {
  Content::DiscRequestQueue queue = TwoWaitingRequests();
  FakeDiscQueueHost host;
  REQUIRE(Content::CancelDiscWrites(queue, DestinationAt(0x200), host));
  const auto once = queue;
  REQUIRE(Content::CancelDiscWrites(queue, DestinationAt(0x200), host));
  CHECK(SameQueue(queue, once));
}

TEST_CASE("Disc cancellation keeps another active destination untouched",
          "[content][disc-queue][disc-cancel]") {
  Content::DiscRequestQueue queue = TwoWaitingRequests();
  FakeDiscQueueHost host;
  host.read_frames = 1;
  Content::UpdateDiscQueue(queue, host);
  const auto first = queue.requests[0];
  REQUIRE(Content::CancelDiscWrites(queue, DestinationAt(0x200), host));
  CHECK(queue.requests[0].state == first.state);
  CHECK(queue.requests[0].destination == first.destination);
  CHECK(host.state == DiscDriveState::kReadInit);
  CHECK(host.active_destination == DestinationAt(0x100));
  CHECK(Counts(queue) == std::pair{1, 0});
}

TEST_CASE("Disc cancellation only marks pending requests and recounts them",
          "[content][disc-queue][disc-cancel]") {
  Content::DiscRequestQueue queue = TwoWaitingRequests();
  FakeDiscQueueHost host;
  queue.requests[1].destination = DestinationAt(0x100);
  queue.requests[2].destination = DestinationAt(0x100);
  queue.requests[2].state = DiscRequestState::kLoaded;
  queue.requests[3].destination = DestinationAt(0x100);
  queue.requests[3].state = DiscRequestState::kAllocated;
  queue.reading = queue.waiting = 0;
  REQUIRE(Content::CancelDiscWrites(queue, DestinationAt(0x100), host));
  CHECK(queue.requests[0].state == DiscRequestState::kError);
  CHECK(queue.requests[1].state == DiscRequestState::kError);
  CHECK(queue.requests[2].state == DiscRequestState::kLoaded);
  CHECK(queue.requests[3].state == DiscRequestState::kAllocated);
  CHECK(Counts(queue) == std::pair{0, 0});
}

TEST_CASE(
    "Disc cancellation refuses an absent destination without side effects",
    "[content][disc-queue][disc-cancel]") {
  Content::DiscRequestQueue queue = TwoWaitingRequests();
  const auto before = queue;
  FakeDiscQueueHost host;
  CHECK_FALSE(Content::CancelDiscWrites(queue, DiscDestination::kNone, host));
  CHECK(SameQueue(queue, before));
  CHECK(host.cancellations.empty());
}

TEST_CASE("Cancellation during a blocking frame is not successful completion",
          "[content][disc-queue][disc-cancel]") {
  Content::DiscRequestQueue queue;
  FakeDiscQueueHost host;
  host.read_frames = 1;
  host.frame_observer = [&queue, &host] {
    REQUIRE(Content::CancelDiscWrites(queue, DestinationAt(8), host));
  };
  CHECK(Content::ReadDiscFileBlocking(host, {.sector = 30, .bytes = 8},
                                      DestinationAt(8)) ==
        DiscReadResult::kFailed);
  CHECK(host.state == DiscDriveState::kIdle);
  CHECK(host.waited_frames == 1);
}

TEST_CASE("Queue exhaustion fallback does not turn a read error into success",
          "[content][disc-queue][disc-failure]") {
  FakeDiscQueueHost host;
  host.read_frames = 1;
  host.read_state = DiscDriveState::kReadError;
  CHECK_FALSE(Content::ReadRefusedDiscFile(host, {.sector = 30, .bytes = 2048},
                                           DestinationAt(8), 0));
  CHECK(host.state == DiscDriveState::kReadError);
}

TEST_CASE(
    "Blocking disc reads wait while the drive reads and fail when it refuses",
    "[content-slice][content][disc-queue]") {
  FakeDiscQueueHost host;
  const DiscFileExtent file{.sector = 1387, .bytes = 188416};
  host.read_frames = 3;
  CHECK(Content::ReadDiscFileBlocking(host, file, DestinationAt(0x40)) ==
        DiscReadResult::kComplete);
  CHECK(host.waited_frames == 3);
  REQUIRE(host.reads.size() == 1);
  CHECK(SameRead(host.reads[0], file, DestinationAt(0x40)));
  // The native result distinguishes a failed accepted read from completion.
  host.read_state = DiscDriveState::kReadError;
  CHECK(Content::ReadDiscFileBlocking(host, file, DestinationAt(0x40)) ==
        DiscReadResult::kFailed);
  CHECK(host.waited_frames == 3);
  host.state = DiscDriveState::kIdle;
  host.accept_reads = false;
  CHECK(Content::ReadDiscFileBlocking(host, file, DestinationAt(0x40)) ==
        DiscReadResult::kRefused);
  CHECK(host.waited_frames == 3);
}

TEST_CASE("Refused disc reads wait for an idle drive within their frame limit",
          "[content-slice][content][disc-queue][issue-282]") {
  FakeDiscQueueHost host;
  const DiscFileExtent file{.sector = 30, .bytes = 2048};
  host.state = DiscDriveState::kStreamXa;
  CHECK_FALSE(Content::ReadRefusedDiscFile(host, file, DestinationAt(8), 4));
  CHECK(host.waited_frames == 4);
  CHECK(host.reads.empty());
  // A drive that goes idle within the limit reads the file.
  host.frames_left = 2;
  host.waited_frames = 0;
  host.read_frames = 1;
  CHECK(Content::ReadRefusedDiscFile(host, file, DestinationAt(8), 4));
  CHECK(host.waited_frames == 3);
  REQUIRE(host.reads.size() == 1);
  CHECK(SameRead(host.reads[0], file, DestinationAt(8)));
  CHECK(host.state == DiscDriveState::kIdle);
}

TEST_CASE("Refused disc reads fail when the drive refuses the read",
          "[content-slice][content][disc-queue][issue-282]") {
  FakeDiscQueueHost host;
  host.accept_reads = false;
  CHECK_FALSE(Content::ReadRefusedDiscFile(host, {.sector = 30, .bytes = 2048},
                                           DestinationAt(8), 0));
  CHECK(host.reads.size() == 1);
  CHECK(host.waited_frames == 0);
}

TEST_CASE("Disc sector interrupts fill the direct destination until it ends",
          "[content-slice][content][disc-queue]") {
  FakeDiscInterruptHost host;
  Content::DiscTransfer direct;
  direct.sector_count = 2;
  direct.state = DiscDriveState::kReadInit;
  Content::ReadSectorCallback(direct, DiscInterrupt::kComplete, host);
  CHECK(direct.state == DiscDriveState::kReadInit);
  Content::ReadSectorCallback(direct, DiscInterrupt::kDataReady, host);
  CHECK(direct.state == DiscDriveState::kReadReady);
  Content::ReadSectorCallback(direct, DiscInterrupt::kDataReady, host);
  CHECK(host.direct_sectors == std::vector<std::uint32_t>{0, 1});
  CHECK(direct.state == DiscDriveState::kIdle);
  CHECK(direct.sectors_received == 2);
  CHECK(host.delivery_ends == 1);
}

TEST_CASE("Disc sector interrupts fill and wrap the buffered ring",
          "[content-slice][content][disc-queue]") {
  FakeDiscInterruptHost host;
  Content::DiscTransfer ring = RingTransfer(Content::DiscBufferMode::kBuffered);
  ring.ring_write_index = Content::kDiscRingSectors - 1;
  ring.buffered_sectors = -1;
  Content::ReadSectorCallback(ring, DiscInterrupt::kDataReady, host);
  CHECK(host.ring_slots == std::vector<std::uint32_t>{15});
  CHECK(ring.ring_write_index == 0);
  CHECK(ring.buffered_sectors == 0);
  CHECK(ring.ring_sectors_written == 1);
  CHECK(host.delivery_ends == 0);
}

TEST_CASE("Disc sector interrupts wrap the manual ring read index",
          "[content-slice][content][disc-queue]") {
  FakeDiscInterruptHost host;
  Content::DiscTransfer ring = RingTransfer(Content::DiscBufferMode::kManual);
  ring.ring_read_index = Content::kDiscRingSectors - 1;
  ring.buffered_sectors = 3;
  Content::ReadSectorCallback(ring, DiscInterrupt::kDataReady, host);
  CHECK(host.ring_slots == std::vector<std::uint32_t>{0});
  CHECK(ring.ring_write_index == 1);
  CHECK(ring.ring_read_index == 0);
  CHECK(ring.buffered_sectors == 3);
  CHECK(ring.ring_sectors_written == 1);
}

TEST_CASE("Disc sector interrupts stop delivery after a disk error",
          "[content-slice][content][disc-queue]") {
  FakeDiscInterruptHost host;
  Content::DiscTransfer ring = RingTransfer(Content::DiscBufferMode::kManual);
  Content::ReadSectorCallback(ring, DiscInterrupt::kDiskError, host);
  CHECK(ring.state == DiscDriveState::kReadError);
  CHECK(host.ready_clears == 1);
  CHECK(host.delivery_ends == 1);
  // Once in error every interrupt ends delivery again.
  Content::ReadSectorCallback(ring, DiscInterrupt::kDataReady, host);
  CHECK(host.ring_slots.empty());
  CHECK(host.delivery_ends == 2);
  CHECK(ring.ring_sectors_written == 0);
}

TEST_CASE("Disc read command interrupts start delivery or report a failure",
          "[content-slice][content][disc-queue]") {
  FakeDiscInterruptHost host;
  Content::DiscTransfer drive;
  drive.state = DiscDriveState::kReadInit;
  Content::ReadCommandCallback(drive, DiscInterrupt::kComplete, host);
  CHECK(host.delivery_starts == 1);
  Content::ReadCommandCallback(drive, DiscInterrupt::kNoResult, host);
  Content::ReadCommandCallback(drive, DiscInterrupt::kNone, host);
  CHECK(drive.state == DiscDriveState::kReadInit);
  Content::ReadCommandCallback(drive, DiscInterrupt::kDataReady, host);
  CHECK(drive.state == DiscDriveState::kReadError);
  CHECK(host.delivery_starts == 1);
}

TEST_CASE("Disc seek command interrupts finish the seek or report an error",
          "[content-slice][content][disc-queue]") {
  Content::DiscTransfer drive;
  drive.state = DiscDriveState::kSeekReady;
  Content::SeekCommandCallback(drive, DiscInterrupt::kNoResult);
  CHECK(drive.state == DiscDriveState::kSeekReady);
  Content::SeekCommandCallback(drive, DiscInterrupt::kComplete);
  CHECK(drive.state == DiscDriveState::kIdle);
  Content::SeekCommandCallback(drive, DiscInterrupt::kDiskError);
  CHECK(drive.state == DiscDriveState::kReadError);
}

TEST_CASE("Disc read errors clear to idle and other states stay",
          "[content-slice][content][disc-queue]") {
  Content::DiscTransfer drive;
  drive.state = DiscDriveState::kReadError;
  CHECK(Content::DiscState(drive) == DiscDriveState::kReadError);
  Content::ClearDiscReadError(drive);
  CHECK(Content::DiscState(drive) == DiscDriveState::kIdle);
  drive.state = DiscDriveState::kCommandFailed;
  Content::ClearDiscReadError(drive);
  CHECK(drive.state == DiscDriveState::kCommandFailed);
}

TEST_CASE("XA ready interrupts decode the sub-header",
          "[content-slice][content][disc-queue]") {
  FakeDiscInterruptHost host;
  Content::DiscTransfer drive;
  drive.state = DiscDriveState::kStreamXa;
  const std::array<std::uint8_t, 8> result{0,    0x59, 0,    0xAB,
                                           0x7F, 0,    0x12, 0x34};
  CHECK(
      Content::XaReadyCallback(drive, DiscInterrupt::kDataReady, result, host));
  REQUIRE(host.xa_sectors.size() == 1);
  CHECK(host.xa_sectors[0].bcd_field == 59);
  CHECK(host.xa_sectors[0].byte_three == 0xAB);
  CHECK(host.xa_sectors[0].byte_four == 0x7F);
  CHECK(host.xa_sectors[0].metadata == 0x1234);
  CHECK(drive.state == DiscDriveState::kStreamXa);
}

TEST_CASE(
    "XA ready interrupts skip missing sub-headers and refuse short results",
    "[content-slice][content][disc-queue]") {
  FakeDiscInterruptHost host;
  Content::DiscTransfer drive;
  drive.state = DiscDriveState::kStreamXa;
  std::array<std::uint8_t, 8> no_subheader{0, 0x59, 0, 0xAB, 0x80, 0, 0, 0};
  CHECK(Content::XaReadyCallback(drive, DiscInterrupt::kDataReady, no_subheader,
                                 host));
  no_subheader[4] = 0x7F;
  CHECK_FALSE(Content::XaReadyCallback(drive, DiscInterrupt::kDataReady,
                                       std::span(no_subheader).first(7), host));
  CHECK(host.xa_sectors.empty());
  CHECK(drive.state == DiscDriveState::kStreamXa);
  CHECK(host.ready_clears == 0);
}

TEST_CASE("XA ready interrupts end the stream on completion or a disk error",
          "[content-slice][content][disc-queue]") {
  FakeDiscInterruptHost host;
  Content::DiscTransfer drive;
  drive.state = DiscDriveState::kStreamXa;
  CHECK(Content::XaReadyCallback(drive, DiscInterrupt::kDiskError, {}, host));
  CHECK(drive.state == DiscDriveState::kCommandFailed);
  CHECK(Content::XaReadyCallback(drive, DiscInterrupt::kComplete, {}, host));
  CHECK(drive.state == DiscDriveState::kIdle);
  CHECK(host.ready_clears == 2);
}

TEST_CASE("Live disc requests refuse a full queue and complete a refused read",
          "[content-slice][content][disc-queue][issue-282]") {
  const LiveDiscQueueFixture fixture;
  // A size the drive refuses, so the refused read fails without the disc.
  const Content_DiscFile file{.lba = 2, .size = -1};
  for (auto& slot : Content_discRequestSlots) {
    slot.state = Content_DiscRequestState_Loaded;
  }
  const std::uint64_t refusals = Content::LiveDiscRequestRefusals();
  CHECK(Content::AllocateLiveDiscRequest(&file) == nullptr);
  CHECK(Content::LiveDiscRequestRefusals() == refusals + 1);
  CHECK(std::ranges::all_of(Content_discRequestSlots, IsLoaded));
  Content_DiscQueueSlot* const record =
      Content::CompleteLiveRefusedDiscRequest(&file, nullptr);
  REQUIRE(record != nullptr);
  CHECK(record->state == Content_DiscRequestState_Error);
  CHECK(record->cdFile.lba == 2);
  Content::ReleaseLiveDiscRequest(record);
}

TEST_CASE("Live refused disc records stay out of the queue and are reused",
          "[content-slice][content][disc-queue][issue-282]") {
  const LiveDiscQueueFixture fixture;
  const Content_DiscFile file{.lba = 2, .size = -1};
  const std::size_t in_use = Content::LiveRefusedDiscRecordsInUse();
  Content_DiscQueueSlot* const record =
      Content::CompleteLiveRefusedDiscRequest(&file, nullptr);
  REQUIRE(record != nullptr);
  CHECK(Content::LiveRefusedDiscRecordsInUse() == in_use + 1);
  // A record is not a queue slot: enqueueing it changes nothing.
  Content::EnqueueLiveDiscRequest(record, nullptr);
  CHECK(Content_discRequestCounts.waiting == 0);
  CHECK(record->state == Content_DiscRequestState_Error);
  Content::ReleaseLiveDiscRequest(record);
  CHECK(record->state == Content_DiscRequestState_Free);
  CHECK(Content::LiveRefusedDiscRecordsInUse() == in_use);
  Content_DiscQueueSlot* const reused =
      Content::CompleteLiveRefusedDiscRequest(&file, nullptr);
  CHECK(reused == record);
  Content::ReleaseLiveDiscRequest(reused);
}

TEST_CASE("Live disc request bindings refuse null records and reuse a slot",
          "[content-slice][content][disc-queue][issue-282]") {
  const LiveDiscQueueFixture fixture;
  const Content_DiscFile file{.lba = 2, .size = -1};
  const std::size_t in_use = Content::LiveRefusedDiscRecordsInUse();
  Content::ReleaseLiveDiscRequest(nullptr);
  CHECK(Content::CompleteLiveRefusedDiscRequest(nullptr, nullptr) == nullptr);
  CHECK(Content::ReadLiveRefusedDiscFile(nullptr, nullptr) == 0);
  CHECK(Content::LiveRefusedDiscRecordsInUse() == in_use);
  CHECK(Content::AllocateLiveDiscRequest(&file) ==
        &Content_discRequestSlots[0]);
  for (auto& slot : Content_discRequestSlots) {
    slot.state = Content_DiscRequestState_Loaded;
  }
  const std::uint64_t refusals = Content::LiveDiscRequestRefusals();
  Content::ReleaseLiveDiscRequest(&Content_discRequestSlots[5]);
  CHECK(Content::AllocateLiveDiscRequest(&file) ==
        &Content_discRequestSlots[5]);
  CHECK(Content::LiveDiscRequestRefusals() == refusals);
}

TEST_CASE("Live disc queue binding allocates requests in slot order",
          "[content-slice][content][disc-queue]") {
  const LiveDiscQueueFixture fixture;
  const Content_DiscFile file{.lba = 300, .size = 4096};
  Content_DiscQueueSlot* const first = Content::AllocateLiveDiscRequest(&file);
  Content_DiscQueueSlot* const second = Content::AllocateLiveDiscRequest(&file);
  REQUIRE(first == &Content_discRequestSlots[0]);
  REQUIRE(second == &Content_discRequestSlots[1]);
  CHECK(first->state == Content_DiscRequestState_Allocated);
  CHECK(first->cdFile.lba == 300);
  Content::ReleaseLiveDiscRequest(first);
  CHECK(first->state == Content_DiscRequestState_Free);
  CHECK(second->state == Content_DiscRequestState_Allocated);
}

TEST_CASE("Live rejected disc requests become Error without being loaded",
          "[content][disc-queue][disc-failure]") {
  const LiveDiscQueueFixture fixture;
  const Content_DiscFile file{.lba = 2, .size = -1};
  std::array<std::byte, 8> bytes{};
  auto* request = QueueLiveRead(file, bytes);
  Content::UpdateLiveDiscQueue();
  CHECK(request->state == Content_DiscRequestState_Error);
  CHECK(Content_discRequestCounts.reading == 0);
  CHECK(Content_discRequestCounts.waiting == 0);
  Content::UpdateLiveDiscQueue();
  CHECK(request->state == Content_DiscRequestState_Error);
  Content::ReleaseLiveDiscRequest(request);
  CHECK(request->state == Content_DiscRequestState_Free);
}

TEST_CASE("Live deferred read failures reach their request before polling",
          "[content][disc-queue][disc-failure]") {
  const LiveDiscQueueFixture fixture;
  const Content_DiscFile absent{.lba = -1, .size = 1};
  const Content_DiscFile empty{.lba = 0, .size = 0};
  std::array<std::byte, 2> bytes{std::byte{0x7F}, std::byte{0x7F}};
  auto* failed = QueueLiveRead(absent, bytes);
  auto* following = QueueLiveRead(empty, bytes);
  Content::UpdateLiveDiscQueue();
  CHECK(failed->state == Content_DiscRequestState_Error);
  CHECK_FALSE(
      Content::LiveDiscReadCompleted(Content::LiveDiscReadGeneration()));
  CHECK(following->state == Content_DiscRequestState_Enqueued);
  CHECK(Content::LiveDiscState() == Content_DiscDriveState_Idle);
  CHECK(bytes[1] == std::byte{0x7F});
  Content::UpdateLiveDiscQueue();
  Content::UpdateLiveDiscQueue();
  CHECK(following->state == Content_DiscRequestState_Loaded);
  CHECK(failed->state == Content_DiscRequestState_Error);
  CHECK(Content_discRequestCounts.reading == 0);
}

TEST_CASE("Live cancellation prevents a deferred destination write",
          "[content][disc-queue][disc-cancel]") {
  const LiveDiscQueueFixture fixture;
  const Content_DiscFile file{.lba = -1, .size = 8};
  std::array<std::byte, 8> bytes{};
  bytes.fill(std::byte{0x7F});
  const auto before = bytes;
  auto* request = QueueLiveRead(file, bytes);
  REQUIRE(Content::BeginLiveDiscRead(file.lba, file.size, bytes.data()));
  REQUIRE(Content::CancelLiveDiscWrites(bytes));
  CHECK(Content::LiveDiscState() == Content_DiscDriveState_Idle);
  CHECK(Content_discDriveState.destination == nullptr);
  CHECK(request->state == Content_DiscRequestState_Error);
  CHECK(request->vram == nullptr);
  Content::UpdateLiveDiscCommands();
  CHECK(bytes == before);
  CHECK(Content_discDriveState.failureCount == 0);
  CHECK(Content::LiveDiscState() == Content_DiscDriveState_Idle);
}

TEST_CASE("Live request generations distinguish reused queue slots",
          "[content][disc-queue][disc-lifetime]") {
  const LiveDiscQueueFixture fixture;
  const Content_DiscFile file{.lba = 0, .size = 0};
  auto* first = Content::AllocateLiveDiscRequest(&file);
  REQUIRE(first != nullptr);
  const auto generation = Content::LiveDiscRequestGeneration(first);
  REQUIRE(generation != 0);
  Content::ReleaseLiveDiscRequest(first);
  CHECK(Content::LiveDiscRequestGeneration(first) == 0);
  auto* next = Content::AllocateLiveDiscRequest(&file);
  REQUIRE(next == first);
  CHECK(Content::LiveDiscRequestGeneration(next) > generation);
}

TEST_CASE("Live request generations survive targeted destination cancellation",
          "[content][disc-queue][disc-lifetime]") {
  const LiveDiscQueueFixture fixture;
  const Content_DiscFile file{.lba = -1, .size = 8};
  std::array<std::byte, 8> bytes{};
  auto* request = QueueLiveRead(file, bytes);
  const auto generation = Content::LiveDiscRequestGeneration(request);
  REQUIRE(generation != 0);
  REQUIRE(Content::CancelLiveDiscWrites(bytes));
  CHECK(request->state == Content_DiscRequestState_Error);
  CHECK(request->vram == nullptr);
  CHECK(Content::LiveDiscRequestGeneration(request) == generation);
}

TEST_CASE("Live request generations do not repeat across queue resets",
          "[content][disc-queue][disc-lifetime]") {
  const LiveDiscQueueFixture fixture;
  const Content_DiscFile file{.lba = 0, .size = 0};
  auto* first = Content::AllocateLiveDiscRequest(&file);
  REQUIRE(first != nullptr);
  const auto generation = Content::LiveDiscRequestGeneration(first);
  REQUIRE(generation != 0);
  Content::InitializeLiveDiscQueue();
  CHECK(Content::LiveDiscRequestGeneration(first) == 0);
  auto* next = Content::AllocateLiveDiscRequest(&file);
  REQUIRE(next == first);
  CHECK(Content::LiveDiscRequestGeneration(next) > generation);
}

TEST_CASE("Live fallback request generations distinguish stable record reuse",
          "[content][disc-queue][disc-lifetime]") {
  const LiveDiscQueueFixture fixture;
  const Content_DiscFile file{.lba = 0, .size = 0};
  std::array<std::byte, 1> bytes{};
  auto* first = Content::CompleteLiveRefusedDiscRequest(&file, bytes.data());
  REQUIRE(first != nullptr);
  const auto generation = Content::LiveDiscRequestGeneration(first);
  REQUIRE(generation != 0);
  Content::ReleaseLiveDiscRequest(first);
  CHECK(Content::LiveDiscRequestGeneration(first) == 0);
  auto* next = Content::CompleteLiveRefusedDiscRequest(&file, bytes.data());
  REQUIRE(next == first);
  CHECK(Content::LiveDiscRequestGeneration(next) > generation);
  Content::ReleaseLiveDiscRequest(next);
}

TEST_CASE("Live blocking reads report completion rather than acceptance",
          "[content][disc-queue][disc-failure]") {
  const LiveDiscQueueFixture fixture;
  std::array<std::byte, 1> bytes{};
  CHECK(Content::ReadLiveDiscFileBlocking(-1, 1, bytes.data()) == 0);
  CHECK_FALSE(
      Content::LiveDiscReadCompleted(Content::LiveDiscReadGeneration()));
  Content::ClearLiveDiscReadError();
  CHECK(Content::ReadLiveDiscFileBlocking(0, 0, bytes.data()) == 1);
  CHECK(Content::LiveDiscReadCompleted(Content::LiveDiscReadGeneration()));
  CHECK(Content::ReadLiveDiscFileBlocking(0, -1, bytes.data()) == 0);
}

TEST_CASE("Live fallback records distinguish failure from completion",
          "[content][disc-queue][disc-failure]") {
  const LiveDiscQueueFixture fixture;
  std::array<std::byte, 1> bytes{};
  const Content_DiscFile absent{.lba = -1, .size = 1};
  auto* failed = Content::CompleteLiveRefusedDiscRequest(&absent, bytes.data());
  REQUIRE(failed != nullptr);
  CHECK(failed->state == Content_DiscRequestState_Error);
  Content::ReleaseLiveDiscRequest(failed);
  Content::ClearLiveDiscReadError();
  const Content_DiscFile empty{.lba = 0, .size = 0};
  auto* completed =
      Content::CompleteLiveRefusedDiscRequest(&empty, bytes.data());
  REQUIRE(completed != nullptr);
  CHECK(completed->state == Content_DiscRequestState_Loaded);
  Content::ReleaseLiveDiscRequest(completed);
}

TEST_CASE("Live cancellation leaves another deferred destination armed",
          "[content][disc-queue][disc-cancel]") {
  const LiveDiscQueueFixture fixture;
  std::array<std::byte, 8> canceled{};
  std::array<std::byte, 8> other{};
  const Content_DiscFile file{.lba = -1, .size = 8};
  auto* request = QueueLiveRead(file, canceled);
  REQUIRE(Content::BeginLiveDiscRead(file.lba, file.size, other.data()));
  REQUIRE(Content::CancelLiveDiscWrites(canceled));
  CHECK(request->state == Content_DiscRequestState_Error);
  CHECK(Content::LiveDiscState() == Content_DiscDriveState_ReadInit);
  CHECK(Content_discDriveState.destination == other.data());
  CHECK(Content_discDriveState.byteCount == file.size);
  CHECK(Content_discDriveState.sectorCount == 1);
}

TEST_CASE("Live repeated cancellation preserves completed handles and counts",
          "[content][disc-queue][disc-cancel]") {
  const LiveDiscQueueFixture fixture;
  std::array<std::byte, 8> bytes{};
  const Content_DiscFile file{.lba = 0, .size = 0};
  auto* completed = QueueLiveRead(file, bytes);
  auto* pending = Content::AllocateLiveDiscRequest(&file);
  REQUIRE(pending != nullptr);
  Content::UpdateLiveDiscQueue();
  Content::UpdateLiveDiscQueue();
  REQUIRE(completed->state == Content_DiscRequestState_Loaded);
  Content::EnqueueLiveDiscRequest(pending, bytes.data());
  REQUIRE(Content::CancelLiveDiscWrites(bytes));
  REQUIRE(Content::CancelLiveDiscWrites(bytes));
  CHECK(completed->state == Content_DiscRequestState_Loaded);
  CHECK(pending->state == Content_DiscRequestState_Error);
  CHECK(Content_discRequestCounts.reading == 0);
  CHECK(Content_discRequestCounts.waiting == 0);
}

TEST_CASE("Live cancellation drops an old destination without stopping XA",
          "[content][disc-queue][disc-cancel]") {
  const LiveDiscQueueFixture fixture;
  std::array<std::byte, 8> bytes{};
  Content_discDriveState.destination = bytes.data();
  Content_discDriveState.state = Content_DiscDriveState_StreamXa;
  Content_discDriveState.byteCount = 99;
  REQUIRE(Content::CancelLiveDiscWrites(bytes));
  CHECK(Content::LiveDiscState() == Content_DiscDriveState_StreamXa);
  CHECK(Content_discDriveState.destination == nullptr);
  CHECK(Content_discDriveState.byteCount == 99);
  CHECK_FALSE(Content::CancelLiveDiscWrites({}));
  CHECK(Content::LiveDiscState() == Content_DiscDriveState_StreamXa);
}

TEST_CASE("Live completion belongs to one generation and excludes cancellation",
          "[content][disc-queue][disc-cancel]") {
  const LiveDiscQueueFixture fixture;
  std::array<std::byte, 1> bytes{};
  REQUIRE(Content::BeginLiveDiscRead(0, 0, bytes.data()));
  const auto first = Content::LiveDiscReadGeneration();
  CHECK_FALSE(Content::LiveDiscReadCompleted(first));
  Content::UpdateLiveDiscCommands();
  CHECK(Content::LiveDiscReadCompleted(first));
  REQUIRE(Content::BeginLiveDiscRead(0, 0, bytes.data()));
  const auto second = Content::LiveDiscReadGeneration();
  CHECK(second != first);
  CHECK_FALSE(Content::LiveDiscReadCompleted(first));
  REQUIRE(Content::CancelLiveDiscWrites(bytes));
  Content::UpdateLiveDiscCommands();
  CHECK_FALSE(Content::LiveDiscReadCompleted(second));
}

TEST_CASE("Live nonempty reads refuse a null destination without arming",
          "[content][disc-queue][disc-failure]") {
  const LiveDiscQueueFixture fixture;
  const auto generation = Content::LiveDiscReadGeneration();
  CHECK_FALSE(Content::BeginLiveDiscRead(0, 1, nullptr));
  CHECK(Content::LiveDiscReadGeneration() == generation);
  CHECK(Content::LiveDiscState() == Content_DiscDriveState_Idle);
}

TEST_CASE("Live disc queue binding commits enqueues and initialization",
          "[content-slice][content][disc-queue]") {
  const LiveDiscQueueFixture fixture;
  const Content_DiscFile file{.lba = 300, .size = 4096};
  std::array<std::uint8_t, 16> buffer{};
  Content_DiscQueueSlot* const first = Content::AllocateLiveDiscRequest(&file);
  Content_DiscQueueSlot* const second = Content::AllocateLiveDiscRequest(&file);
  REQUIRE(first == &Content_discRequestSlots[0]);
  REQUIRE(second == &Content_discRequestSlots[1]);
  Content::EnqueueLiveDiscRequest(first, buffer.data());
  Content::EnqueueLivePriorityDiscRequest(second, nullptr);
  CHECK(first->state == Content_DiscRequestState_Enqueued);
  CHECK(first->vram == buffer.data());
  CHECK(first->priority == 1);
  CHECK(second->priority == 0);
  CHECK(Content_discRequestCounts.waiting == 2);
  Content::InitializeLiveDiscQueue();
  CHECK(Content_discRequestCounts.waiting == 0);
  CHECK(first->state == Content_DiscRequestState_Free);
}

TEST_CASE("Live disc drive binding commits seek, read and XA interrupts",
          "[content-slice][content][disc-queue]") {
  const LiveDiscQueueFixture fixture;
  Content_discDriveState.state =
      static_cast<char>(Content_DiscDriveState_SeekReady);
  Content::HandleLiveSeekCommandInterrupt(2);
  CHECK(Content::LiveDiscState() == Content_DiscDriveState_Idle);
  Content::HandleLiveReadCommandInterrupt(5);
  CHECK(Content::LiveDiscState() == Content_DiscDriveState_ReadError);
  Content::ClearLiveDiscReadError();
  CHECK(Content::LiveDiscState() == Content_DiscDriveState_Idle);
  const std::array<std::uint8_t, 8> result{0, 0x12, 0, 3, 4, 0, 0, 9};
  const std::array<int, 4> saved_xa{
      Content_legacyXaReadyBcdField, Content_legacyXaReadyByteThree,
      Content_legacyXaReadyByteFour, Content_xaReadyMetadataWord};
  Content::HandleLiveXaReadyInterrupt(1, result);
  CHECK(Content_legacyXaReadyBcdField == 12);
  CHECK(Content_xaReadyMetadataWord == 9);
  Content_legacyXaReadyBcdField = saved_xa[0];
  Content_legacyXaReadyByteThree = saved_xa[1];
  Content_legacyXaReadyByteFour = saved_xa[2];
  Content_xaReadyMetadataWord = saved_xa[3];
}
