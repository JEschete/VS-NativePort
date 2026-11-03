#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Content/CdDrive.hpp"
#include "Content/CdDriveLive.hpp"

extern "C" {
#include "DiscQueue.h"
}

namespace {
namespace Content = VS::Content;
using Content::DiscDestination;
using Content::DiscDriveState;
using Content::DiscFileExtent;
using Content::DiscInterrupt;
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
    if (accept_reads && read_frames != 0) {
      state = read_state;
      frames_left = read_frames;
    }
    return accept_reads;
  }
  void UpdateCommands() override { ++commands; }
  void CountFrameSinceRead() override { ++counted_frames; }
  void RestartFramesSinceRead() override { ++restarts; }
  void WaitFrame() override {
    ++waited_frames;
    if (frames_left == 0) return;
    --frames_left;
    if (frames_left == 0) state = DiscDriveState::kIdle;
  }

  DiscDriveState state = DiscDriveState::kIdle;
  bool accept_reads = true;
  DiscDriveState read_state = DiscDriveState::kReadInit;
  std::uint32_t read_frames = 0;
  std::uint32_t frames_left = 0;
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
  }
  LiveDiscQueueFixture(const LiveDiscQueueFixture&) = delete;
  LiveDiscQueueFixture& operator=(const LiveDiscQueueFixture&) = delete;
  LiveDiscQueueFixture(LiveDiscQueueFixture&&) = delete;
  LiveDiscQueueFixture& operator=(LiveDiscQueueFixture&&) = delete;
};
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

TEST_CASE(
    "Blocking disc reads wait while the drive reads and fail when it refuses",
    "[content-slice][content][disc-queue]") {
  FakeDiscQueueHost host;
  const DiscFileExtent file{.sector = 1387, .bytes = 188416};
  host.read_frames = 3;
  CHECK(Content::ReadDiscFileBlocking(host, file, DestinationAt(0x40)));
  CHECK(host.waited_frames == 3);
  REQUIRE(host.reads.size() == 1);
  CHECK(SameRead(host.reads[0], file, DestinationAt(0x40)));
  // vs_main_diskLoadFile stops waiting outside the seek and read phases.
  host.read_state = DiscDriveState::kReadError;
  CHECK(Content::ReadDiscFileBlocking(host, file, DestinationAt(0x40)));
  CHECK(host.waited_frames == 3);
  host.state = DiscDriveState::kIdle;
  host.accept_reads = false;
  CHECK_FALSE(Content::ReadDiscFileBlocking(host, file, DestinationAt(0x40)));
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
