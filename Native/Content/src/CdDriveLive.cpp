// Hand-maintained (tools/port/owned.csv).
// Live binding for the disc request queue and drive handlers (CdDrive.hpp):
// the legacy request array and counts, the drive record and the native drive
// (DiscReaderLive.hpp).
#include "Content/CdDriveLive.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <type_traits>
#include <vector>

#include "Content/CdDrive.hpp"
#include "Content/DiscQueue.hpp"
#include "Content/DiscReaderLive.hpp"
#include "Core/Diagnostics.hpp"
#include "Legacy/LibDs.hpp"
#include "State/GameClockLive.hpp"
#include "State/PlayTime.hpp"

namespace VS::Content {
namespace {
static_assert(std::size(discRequestSlots) == kDiscRequestCapacity);
static_assert(offsetof(DiscQueueSlot, state) == 0);
static_assert(offsetof(DiscQueueSlot, priority) ==
              sizeof(DiscQueueSlot::state));
static_assert(offsetof(DiscQueueSlot, file) ==
              offsetof(DiscQueueSlot, priority) +
                  sizeof(DiscQueueSlot::priority));
static_assert(offsetof(DiscQueueSlot, reserved) ==
              offsetof(DiscQueueSlot, file) + sizeof(DiscFile));
static_assert(offsetof(DiscQueueSlot, destination) ==
              offsetof(DiscQueueSlot, reserved) +
                  sizeof(DiscQueueSlot::reserved));
static_assert(sizeof(DiscQueueSlot) ==
              offsetof(DiscQueueSlot, destination) + sizeof(void*));
static_assert(offsetof(DiscFile, lba) == 0);
static_assert(offsetof(DiscFile, size) == sizeof(DiscFile::lba));
static_assert(offsetof(DiscRequestCounts, reading) == 0);
static_assert(offsetof(DiscRequestCounts, waiting) ==
              sizeof(DiscRequestCounts::reading));
static_assert(offsetof(DiscDrive, state) == 0);
static_assert(offsetof(DiscDrive, sectors_received) == sizeof(std::uint32_t));
static_assert(offsetof(DiscDrive, sector_count) ==
              offsetof(DiscDrive, sectors_received) +
                  sizeof(DiscDrive::sectors_received));
static_assert(sizeof(DiscDrive::buffer_mode) == sizeof(std::int32_t));
static_assert(sizeof(DiscDrive::frames_since_last_read) ==
              sizeof(std::int32_t));
static_assert(
    std::is_same_v<decltype(DiscDrive::destination_sector), std::uint32_t>);
static_assert(
    std::is_same_v<decltype(DiscDrive::ring_write_index), std::uint32_t>);

constexpr std::uint32_t kSectorBytes = 2048;
constexpr int kSectorWords = kSectorBytes / 4;
// ReadLiveRefusedDiscFile waits at most a minute of frames for the drive.
constexpr std::uint32_t kRefusedReadWaitFrames = 60 * 60;

std::uint64_t refusedRequests = 0;
std::uint64_t requestSerial = 0;
std::array<std::uint64_t, kDiscRequestCapacity> requestGenerations{};

struct RefusedRecord {
  DiscQueueSlot slot{};
  std::uint64_t generation = 0;
};
// #282: completed records for refused requests, outside the request array.
// A released record returns to Free and is reused, as the array's slots are,
// so a caller's stale pointer stays readable (each record is allocated on
// its own, so its address stays put).
std::vector<std::unique_ptr<RefusedRecord>> refusedRecords;

RefusedRecord* FindRefusedRecord(const DiscQueueSlot* request) {
  const auto record = std::ranges::find_if(
      refusedRecords,
      [request](const auto& candidate) { return &candidate->slot == request; });
  return record == refusedRecords.end() ? nullptr : record->get();
}

RefusedRecord& FreeRefusedRecord() {
  const auto record =
      std::ranges::find_if(refusedRecords, [](const auto& candidate) {
        return candidate->slot.state == DiscRequestState::kFree;
      });
  if (record != refusedRecords.end()) return **record;
  return *refusedRecords.emplace_back(std::make_unique<RefusedRecord>());
}

std::uint64_t NextRequestGeneration() {
  if (requestSerial == std::numeric_limits<std::uint64_t>::max()) {
    VS::Core::Warnf("content", "Disc request generation exhausted");
    return 0;
  }
  return ++requestSerial;
}

DiscDestination Destination(void* pointer) {
  return static_cast<DiscDestination>(
      reinterpret_cast<std::uintptr_t>(pointer));
}

void* Pointer(DiscDestination destination) {
  return reinterpret_cast<void*>(static_cast<std::uintptr_t>(destination));
}

DiscFileExtent Extent(const DiscFile& file) {
  return {.sector = file.lba, .bytes = file.size};
}

DiscRequestQueue LoadQueue() {
  DiscRequestQueue queue;
  for (std::size_t i = 0; i < kDiscRequestCapacity; ++i) {
    const DiscQueueSlot& slot = discRequestSlots[i];
    queue.requests[i] = {
        .state = slot.state,
        .priority = static_cast<std::uint16_t>(slot.priority),
        .file = Extent(slot.file),
        .reserved = slot.reserved,
        .destination = Destination(slot.destination),
    };
  }
  queue.reading = static_cast<std::uint16_t>(discRequestCounts.reading);
  queue.waiting = static_cast<std::uint16_t>(discRequestCounts.waiting);
  return queue;
}

void CommitQueue(const DiscRequestQueue& queue) {
  for (std::size_t i = 0; i < kDiscRequestCapacity; ++i) {
    const DiscRequest& request = queue.requests[i];
    DiscQueueSlot& slot = discRequestSlots[i];
    slot.state = request.state;
    slot.priority = static_cast<std::int16_t>(request.priority);
    slot.file.lba = request.file.sector;
    slot.file.size = request.file.bytes;
    slot.reserved = request.reserved;
    slot.destination = Pointer(request.destination);
  }
  discRequestCounts.reading = static_cast<std::int16_t>(queue.reading);
  discRequestCounts.waiting = static_cast<std::int16_t>(queue.waiting);
}

// The index of `slot` in the request array.
std::optional<std::size_t> IndexOf(const DiscQueueSlot* slot) {
  const auto address = reinterpret_cast<std::uintptr_t>(slot);
  const auto base =
      reinterpret_cast<std::uintptr_t>(std::data(discRequestSlots));
  if (address < base || address - base >= sizeof discRequestSlots ||
      (address - base) % sizeof(DiscQueueSlot) != 0) {
    return std::nullopt;
  }
  return (address - base) / sizeof(DiscQueueSlot);
}

DiscDriveState LiveDriveState() { return discDriveState.state; }

class LiveDiscQueueHost final : public DiscQueueHost {
 public:
  [[nodiscard]] DiscDriveState DriveState() const override {
    return LiveDriveState();
  }
  bool BeginRead(const DiscFileExtent& file,
                 DiscDestination destination) override {
    const bool accepted =
        BeginLiveDiscRead(file.sector, static_cast<std::uint32_t>(file.bytes),
                          Pointer(destination));
    if (!accepted) {
      VS::Core::Warnf("content",
                      "Disc read refused: sector %d, %d bytes, state %d",
                      file.sector, file.bytes, static_cast<int>(DriveState()));
    } else {
      queued_request_.reset();
      read_generation_ = LiveDiscReadGeneration();
    }
    return accepted;
  }
  bool ReadCompleted() const override {
    return LiveDiscReadCompleted(read_generation_);
  }
  bool BeginQueuedRead(std::size_t index, const DiscFileExtent& file,
                       DiscDestination destination) override {
    if (!BeginRead(file, destination)) return false;
    queued_request_ = index;
    return true;
  }
  std::optional<std::size_t> FailedRequest() const override {
    const auto state = DriveState();
    return read_generation_ == LiveDiscReadGeneration() &&
                   (state == DiscDriveState::kReadError ||
                    state == DiscDriveState::kCommandFailed)
               ? queued_request_
               : std::nullopt;
  }
  void ClearReadFailure() override {
    VS::Core::Warnf("content", "Queued disc read failed: state %d",
                    static_cast<int>(DriveState()));
    CancelLiveDiscRead(discDriveState.destination);
    queued_request_.reset();
  }
  void CancelRead(DiscDestination destination) override {
    CancelLiveDiscRead(Pointer(destination));
  }
  // Deferred until the queue is committed (the original served the commands
  // after its scan).
  void UpdateCommands() override { commands_pending_ = true; }
  void CountFrameSinceRead() override {
    discDriveState.frames_since_last_read = static_cast<int>(
        static_cast<std::uint32_t>(discDriveState.frames_since_last_read) + 1U);
  }
  void RestartFramesSinceRead() override {
    discDriveState.frames_since_last_read = 0;
  }
  void WaitFrame() override { VS::State::UpdateLivePlayTimeAndLoading(); }

  [[nodiscard]] bool CommandsPending() const { return commands_pending_; }

 private:
  bool commands_pending_ = false;
  std::optional<std::size_t> queued_request_;
  std::uint64_t read_generation_ = 0;
};

DiscTransfer LoadTransfer() {
  const DiscDrive& drive = discDriveState;
  return {
      .state = LiveDriveState(),
      .sectors_received = drive.sectors_received,
      .sector_count = drive.sector_count,
      .buffer_mode = drive.buffer_mode,
      .destination_sector = drive.destination_sector,
      .buffered_sectors = drive.buffered_sectors,
      .ring_sectors_written = drive.ring_sectors_written,
      .ring_write_index = drive.ring_write_index,
      .ring_read_index = drive.ring_read_index,
  };
}

void CommitTransfer(const DiscTransfer& transfer) {
  DiscDrive& drive = discDriveState;
  drive.state = transfer.state;
  drive.sectors_received = transfer.sectors_received;
  drive.sector_count = transfer.sector_count;
  drive.buffer_mode = transfer.buffer_mode;
  drive.destination_sector = transfer.destination_sector;
  drive.buffered_sectors = transfer.buffered_sectors;
  drive.ring_sectors_written = transfer.ring_sectors_written;
  drive.ring_write_index = transfer.ring_write_index;
  drive.ring_read_index = transfer.ring_read_index;
}

// The drive's ready-callback system (DsReadyCallback, DsStartReadySystem,
// DsEndReadySystem) is not modelled: native reads are served by
// DiscReaderLive and nothing registers these handlers, so those requests do
// nothing, as the generated copy's dropped calls did.
class LiveDiscInterruptHost final : public DiscInterruptHost {
 public:
  void ReceiveSector(std::uint32_t sector) override {
    // sll 11: the sector's byte offset wraps at 32 bits.
    VS::Legacy::DsGetSector(
        static_cast<std::uint8_t*>(discDriveState.destination) +
            static_cast<std::uint32_t>(sector * kSectorBytes),
        kSectorWords);
  }
  void ReceiveRingSector(std::uint32_t slot) override {
    VS::Legacy::DsGetSector(
        legacyDiscReadBuffer + static_cast<std::uint32_t>(slot * kSectorBytes),
        kSectorWords);
  }
  void StartSectorDelivery() override {}
  void ClearReadyCallback() override {}
  void EndSectorDelivery() override {}
  void RecordXaSector(const XaSectorInfo& info) override {
    legacyXaReadyBcdField = info.bcd_field;
    legacyXaReadyByteThree = info.byte_three;
    legacyXaReadyByteFour = info.byte_four;
    xaReadyMetadataWord = info.metadata;
  }
};

void WarnForeignRequest(const char* operation, const DiscQueueSlot* request) {
  VS::Core::Warnf("content", "%s refused: %p is not a disc request", operation,
                  static_cast<const void*>(request));
}
}  // namespace

DiscQueueSlot* AllocateLiveDiscRequest(const DiscFile* file) {
  if (file == nullptr) {
    VS::Core::Warnf("content", "AllocateDiscRequest refused: null file");
    return nullptr;
  }
  DiscRequestQueue queue = LoadQueue();
  const std::optional<std::size_t> index =
      AllocateDiscRequest(queue, Extent(*file));
  if (!index) {
    if (std::has_single_bit(++refusedRequests)) {
      VS::Core::Warnf(
          "content",
          "AllocateDiscRequest refused: all %zu disc requests are in "
          "use (sector %d, %d bytes); %" PRIu64 " refused",
          kDiscRequestCapacity, file->lba, file->size, refusedRequests);
    }
    return nullptr;
  }
  const auto generation = NextRequestGeneration();
  if (generation == 0) {
    return nullptr;
  }
  CommitQueue(queue);
  requestGenerations[*index] = generation;
  return &discRequestSlots[*index];
}

std::uint64_t LiveDiscRequestGeneration(const DiscQueueSlot* request) {
  if (const auto index = IndexOf(request)) {
    return request->state == DiscRequestState::kFree
               ? 0
               : requestGenerations[*index];
  }
  if (const auto* record = FindRefusedRecord(request)) {
    return record->slot.state == DiscRequestState::kFree ? 0
                                                         : record->generation;
  }
  WarnForeignRequest("DiscRequestGeneration", request);
  return 0;
}

void ReleaseLiveDiscRequest(DiscQueueSlot* request) {
  // A refused request (#282) holds nothing to free.
  if (request == nullptr) return;
  const std::optional<std::size_t> index = IndexOf(request);
  if (!index) {
    RefusedRecord* record = FindRefusedRecord(request);
    if (record == nullptr) {
      WarnForeignRequest("ReleaseDiscRequest", request);
    } else {
      record->slot.state = DiscRequestState::kFree;
    }
    return;
  }
  DiscRequestQueue queue = LoadQueue();
  ReleaseDiscRequest(queue, *index);
  CommitQueue(queue);
}

bool CancelLiveDiscWrites(std::span<std::byte> destination) {
  if (destination.empty()) {
    VS::Core::Warnf("content", "CancelDiscWrites refused: empty destination");
    return false;
  }
  DiscRequestQueue queue = LoadQueue();
  LiveDiscQueueHost host;
  if (!CancelDiscWrites(queue, Destination(destination.data()), host)) {
    VS::Core::Warnf("content", "CancelDiscWrites refused: null destination");
    return false;
  }
  CommitQueue(queue);
  return true;
}

DiscQueueSlot* CompleteLiveRefusedDiscRequest(const DiscFile* file,
                                              void* destination) {
  if (file == nullptr) {
    VS::Core::Warnf("content", "CompleteRefusedDiscRequest refused: null file");
    return nullptr;
  }
  const auto generation = NextRequestGeneration();
  if (generation == 0) {
    return nullptr;
  }
  const bool read = ReadLiveRefusedDiscFile(file, destination) != 0;
  RefusedRecord& owner = FreeRefusedRecord();
  DiscQueueSlot& record = owner.slot;
  owner.generation = generation;
  record = DiscQueueSlot{};
  record.state = read ? DiscRequestState::kLoaded : DiscRequestState::kError;
  record.file = *file;
  record.destination = destination;
  return &record;
}

std::size_t LiveRefusedDiscRecordsInUse() noexcept {
  return static_cast<std::size_t>(
      std::ranges::count_if(refusedRecords, [](const auto& record) {
        return record->slot.state != DiscRequestState::kFree;
      }));
}

void EnqueueLiveDiscRequest(DiscQueueSlot* request, void* destination) {
  const std::optional<std::size_t> index = IndexOf(request);
  if (!index) {
    WarnForeignRequest("EnqueueDiscRequest", request);
    return;
  }
  DiscRequestQueue queue = LoadQueue();
  EnqueueDiscRequest(queue, *index, Destination(destination));
  CommitQueue(queue);
}

void EnqueueLivePriorityDiscRequest(DiscQueueSlot* request, void* destination) {
  const std::optional<std::size_t> index = IndexOf(request);
  if (!index) {
    WarnForeignRequest("EnqueuePriorityDiscRequest", request);
    return;
  }
  DiscRequestQueue queue = LoadQueue();
  EnqueuePriorityDiscRequest(queue, *index, Destination(destination));
  CommitQueue(queue);
}

void InitializeLiveDiscQueue() {
  DiscRequestQueue queue = LoadQueue();
  InitializeDiscQueue(queue);
  CommitQueue(queue);
  requestGenerations.fill(0);
  // The records are freed like the array's slots but never deallocated.
  for (const auto& record : refusedRecords) {
    record->slot.state = DiscRequestState::kFree;
    record->generation = 0;
  }
}

void UpdateLiveDiscQueue() {
  DiscRequestQueue queue = LoadQueue();
  LiveDiscQueueHost host;
  UpdateDiscQueue(queue, host);
  CommitQueue(queue);
  if (host.CommandsPending()) {
    UpdateLiveDiscCommands();
    queue = LoadQueue();
    if (ResolveFailedDiscRequest(queue, host)) CommitQueue(queue);
  }
}

int ReadLiveDiscFileBlocking(int sector, int bytes, void* destination) {
  LiveDiscQueueHost host;
  const auto result = ReadDiscFileBlocking(
      host, {.sector = sector, .bytes = bytes}, Destination(destination));
  if (result == DiscReadResult::kFailed) {
    VS::Core::Warnf("content",
                    "Accepted disc read failed: sector %d, %d bytes, state %d",
                    sector, bytes, static_cast<int>(host.DriveState()));
  }
  return result == DiscReadResult::kComplete ? 1 : 0;
}

int ReadLiveRefusedDiscFile(const DiscFile* file, void* destination) {
  if (file == nullptr) {
    VS::Core::Warnf("content", "ReadRefusedDiscFile refused: null file");
    return 0;
  }
  LiveDiscQueueHost host;
  if (!ReadRefusedDiscFile(host, Extent(*file), Destination(destination),
                           kRefusedReadWaitFrames)) {
    VS::Core::Warnf(
        "content",
        "ReadRefusedDiscFile: sector %d (%d bytes) was not read; the "
        "drive stayed in state %d",
        file->lba, file->size, static_cast<int>(host.DriveState()));
    return 0;
  }
  return 1;
}

std::uint64_t LiveDiscRequestRefusals() noexcept { return refusedRequests; }

DiscDriveState LiveDiscState() { return DiscState(LoadTransfer()); }

void ClearLiveDiscReadError() {
  DiscTransfer transfer = LoadTransfer();
  ClearDiscReadError(transfer);
  CommitTransfer(transfer);
}

void HandleLiveReadSectorInterrupt(std::uint8_t interrupt) {
  DiscTransfer transfer = LoadTransfer();
  LiveDiscInterruptHost host;
  ReadSectorCallback(transfer, static_cast<DiscInterrupt>(interrupt), host);
  CommitTransfer(transfer);
}

void HandleLiveReadCommandInterrupt(std::uint8_t interrupt) {
  DiscTransfer transfer = LoadTransfer();
  LiveDiscInterruptHost host;
  ReadCommandCallback(transfer, static_cast<DiscInterrupt>(interrupt), host);
  CommitTransfer(transfer);
}

void HandleLiveSeekCommandInterrupt(std::uint8_t interrupt) {
  DiscTransfer transfer = LoadTransfer();
  SeekCommandCallback(transfer, static_cast<DiscInterrupt>(interrupt));
  CommitTransfer(transfer);
}

void HandleLiveXaReadyInterrupt(std::uint8_t interrupt,
                                std::span<const std::uint8_t> result) {
  DiscTransfer transfer = LoadTransfer();
  LiveDiscInterruptHost host;
  if (!XaReadyCallback(transfer, static_cast<DiscInterrupt>(interrupt), result,
                       host)) {
    VS::Core::Warnf(
        "content",
        "XaReadyCallback refused: a %zu-byte result is shorter than "
        "%zu bytes",
        result.size(), kXaReadyResultBytes);
  }
  CommitTransfer(transfer);
}

}  // namespace VS::Content
