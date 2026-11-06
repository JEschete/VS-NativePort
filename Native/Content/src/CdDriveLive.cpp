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
#include "Content/DiscReaderLive.hpp"
#include "Core/Diagnostics.h"
#include "State/GameClockLive.hpp"

extern "C" {
#include "DiscQueue.h"
#include "Legacy/LibDs.h"
#include "State/PlayTime.h"
}

namespace VS::Content {
namespace {
static_assert(std::size(Content_discRequestSlots) == kDiscRequestCapacity);
static_assert(offsetof(Content_DiscQueueSlot, state) == 0);
static_assert(offsetof(Content_DiscQueueSlot, priority) ==
              sizeof(Content_DiscQueueSlot::state));
static_assert(offsetof(Content_DiscQueueSlot, cdFile) ==
              offsetof(Content_DiscQueueSlot, priority) +
                  sizeof(Content_DiscQueueSlot::priority));
static_assert(offsetof(Content_DiscQueueSlot, opaqueC) ==
              offsetof(Content_DiscQueueSlot, cdFile) +
                  sizeof(Content_DiscFile));
static_assert(offsetof(Content_DiscQueueSlot, vram) ==
              offsetof(Content_DiscQueueSlot, opaqueC) +
                  sizeof(Content_DiscQueueSlot::opaqueC));
static_assert(sizeof(Content_DiscQueueSlot) ==
              offsetof(Content_DiscQueueSlot, vram) + sizeof(void*));
static_assert(offsetof(Content_DiscFile, lba) == 0);
static_assert(offsetof(Content_DiscFile, size) ==
              sizeof(Content_DiscFile::lba));
static_assert(offsetof(Content_DiscRequestCounts, reading) == 0);
static_assert(offsetof(Content_DiscRequestCounts, waiting) ==
              sizeof(Content_DiscRequestCounts::reading));
static_assert(offsetof(Content_DiscDrive, state) == 0);
static_assert(offsetof(Content_DiscDrive, sectorsReceived) ==
              sizeof(std::uint32_t));
static_assert(offsetof(Content_DiscDrive, sectorCount) ==
              offsetof(Content_DiscDrive, sectorsReceived) +
                  sizeof(Content_DiscDrive::sectorsReceived));
static_assert(sizeof(Content_DiscDrive::bufferMode) == sizeof(std::int32_t));
static_assert(sizeof(Content_DiscDrive::framesSinceLastRead) ==
              sizeof(std::int32_t));
static_assert(std::is_same_v<decltype(Content_DiscDrive::destinationSector),
                             std::uint32_t>);
static_assert(
    std::is_same_v<decltype(Content_DiscDrive::ringWriteIndex), std::uint32_t>);

constexpr std::uint32_t kSectorBytes = 2048;
constexpr int kSectorWords = kSectorBytes / 4;
// ReadLiveRefusedDiscFile waits at most a minute of frames for the drive.
constexpr std::uint32_t kRefusedReadWaitFrames = 60 * 60;

std::uint64_t refused_requests = 0;
std::uint64_t request_serial = 0;
std::array<std::uint64_t, kDiscRequestCapacity> request_generations{};

struct RefusedRecord {
  Content_DiscQueueSlot slot{};
  std::uint64_t generation = 0;
};
// #282: completed records for refused requests, outside the request array.
// A released record returns to Free and is reused, as the array's slots are,
// so a caller's stale pointer stays readable (each record is allocated on
// its own, so its address stays put).
std::vector<std::unique_ptr<RefusedRecord>> refused_records;

RefusedRecord* FindRefusedRecord(const Content_DiscQueueSlot* request) {
  const auto record = std::ranges::find_if(
      refused_records,
      [request](const auto& candidate) { return &candidate->slot == request; });
  return record == refused_records.end() ? nullptr : record->get();
}

RefusedRecord& FreeRefusedRecord() {
  const auto record =
      std::ranges::find_if(refused_records, [](const auto& candidate) {
        return candidate->slot.state == Content_DiscRequestState_Free;
      });
  if (record != refused_records.end()) return **record;
  return *refused_records.emplace_back(std::make_unique<RefusedRecord>());
}

std::uint64_t NextRequestGeneration() {
  if (request_serial == std::numeric_limits<std::uint64_t>::max()) {
    Core_Warnf("content", "Disc request generation exhausted");
    return 0;
  }
  return ++request_serial;
}

DiscDestination Destination(void* pointer) {
  return static_cast<DiscDestination>(
      reinterpret_cast<std::uintptr_t>(pointer));
}

void* Pointer(DiscDestination destination) {
  return reinterpret_cast<void*>(static_cast<std::uintptr_t>(destination));
}

DiscFileExtent Extent(const Content_DiscFile& file) {
  return {.sector = file.lba, .bytes = file.size};
}

DiscRequestQueue LoadQueue() {
  DiscRequestQueue queue;
  for (std::size_t i = 0; i < kDiscRequestCapacity; ++i) {
    const Content_DiscQueueSlot& slot = Content_discRequestSlots[i];
    queue.requests[i] = {
        .state = static_cast<DiscRequestState>(slot.state),
        .priority = static_cast<std::uint16_t>(slot.priority),
        .file = Extent(slot.cdFile),
        .reserved = slot.opaqueC,
        .destination = Destination(slot.vram),
    };
  }
  queue.reading = static_cast<std::uint16_t>(Content_discRequestCounts.reading);
  queue.waiting = static_cast<std::uint16_t>(Content_discRequestCounts.waiting);
  return queue;
}

void CommitQueue(const DiscRequestQueue& queue) {
  for (std::size_t i = 0; i < kDiscRequestCapacity; ++i) {
    const DiscRequest& request = queue.requests[i];
    Content_DiscQueueSlot& slot = Content_discRequestSlots[i];
    slot.state = static_cast<std::int16_t>(request.state);
    slot.priority = static_cast<std::int16_t>(request.priority);
    slot.cdFile.lba = request.file.sector;
    slot.cdFile.size = request.file.bytes;
    slot.opaqueC = request.reserved;
    slot.vram = Pointer(request.destination);
  }
  Content_discRequestCounts.reading = static_cast<std::int16_t>(queue.reading);
  Content_discRequestCounts.waiting = static_cast<std::int16_t>(queue.waiting);
}

// The index of `slot` in the request array.
std::optional<std::size_t> IndexOf(const Content_DiscQueueSlot* slot) {
  const auto address = reinterpret_cast<std::uintptr_t>(slot);
  const auto base =
      reinterpret_cast<std::uintptr_t>(std::data(Content_discRequestSlots));
  if (address < base || address - base >= sizeof Content_discRequestSlots ||
      (address - base) % sizeof(Content_DiscQueueSlot) != 0) {
    return std::nullopt;
  }
  return (address - base) / sizeof(Content_DiscQueueSlot);
}

DiscDriveState LiveDriveState() {
  return static_cast<DiscDriveState>(
      static_cast<std::uint8_t>(Content_discDriveState.state));
}

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
      Core_Warnf("content", "Disc read refused: sector %d, %d bytes, state %d",
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
    Core_Warnf("content", "Queued disc read failed: state %d",
               static_cast<int>(DriveState()));
    CancelLiveDiscRead(Content_discDriveState.destination);
    queued_request_.reset();
  }
  void CancelRead(DiscDestination destination) override {
    CancelLiveDiscRead(Pointer(destination));
  }
  // Deferred until the queue is committed (the original served the commands
  // after its scan).
  void UpdateCommands() override { commands_pending_ = true; }
  void CountFrameSinceRead() override {
    Content_discDriveState.framesSinceLastRead = static_cast<int>(
        static_cast<std::uint32_t>(Content_discDriveState.framesSinceLastRead) +
        1U);
  }
  void RestartFramesSinceRead() override {
    Content_discDriveState.framesSinceLastRead = 0;
  }
  void WaitFrame() override { VS::State::UpdateLivePlayTimeAndLoading(0); }

  [[nodiscard]] bool CommandsPending() const { return commands_pending_; }

 private:
  bool commands_pending_ = false;
  std::optional<std::size_t> queued_request_;
  std::uint64_t read_generation_ = 0;
};

DiscTransfer LoadTransfer() {
  const Content_DiscDrive& drive = Content_discDriveState;
  return {
      .state = LiveDriveState(),
      .sectors_received = drive.sectorsReceived,
      .sector_count = drive.sectorCount,
      .buffer_mode = static_cast<DiscBufferMode>(drive.bufferMode),
      .destination_sector = drive.destinationSector,
      .buffered_sectors = drive.bufferedSectors,
      .ring_sectors_written = drive.ringSectorsWritten,
      .ring_write_index = drive.ringWriteIndex,
      .ring_read_index = drive.ringReadIndex,
  };
}

void CommitTransfer(const DiscTransfer& transfer) {
  Content_DiscDrive& drive = Content_discDriveState;
  drive.state = static_cast<char>(transfer.state);
  drive.sectorsReceived = transfer.sectors_received;
  drive.sectorCount = transfer.sector_count;
  drive.bufferMode = static_cast<Content_DiscBufferMode>(transfer.buffer_mode);
  drive.destinationSector = transfer.destination_sector;
  drive.bufferedSectors = transfer.buffered_sectors;
  drive.ringSectorsWritten = transfer.ring_sectors_written;
  drive.ringWriteIndex = transfer.ring_write_index;
  drive.ringReadIndex = transfer.ring_read_index;
}

// The drive's ready-callback system (DsReadyCallback, DsStartReadySystem,
// DsEndReadySystem) is not modelled: native reads are served by
// DiscReaderLive and nothing registers these handlers, so those requests do
// nothing, as the generated copy's dropped calls did.
class LiveDiscInterruptHost final : public DiscInterruptHost {
 public:
  void ReceiveSector(std::uint32_t sector) override {
    // sll 11: the sector's byte offset wraps at 32 bits.
    DsGetSector(static_cast<u_char*>(Content_discDriveState.destination) +
                    static_cast<std::uint32_t>(sector * kSectorBytes),
                kSectorWords);
  }
  void ReceiveRingSector(std::uint32_t slot) override {
    DsGetSector(Content_legacyDiscReadBuffer +
                    static_cast<std::uint32_t>(slot * kSectorBytes),
                kSectorWords);
  }
  void StartSectorDelivery() override {}
  void ClearReadyCallback() override {}
  void EndSectorDelivery() override {}
  void RecordXaSector(const XaSectorInfo& info) override {
    Content_legacyXaReadyBcdField = info.bcd_field;
    Content_legacyXaReadyByteThree = info.byte_three;
    Content_legacyXaReadyByteFour = info.byte_four;
    Content_xaReadyMetadataWord = info.metadata;
  }
};

void WarnForeignRequest(const char* operation,
                        const Content_DiscQueueSlot* request) {
  Core_Warnf("content", "%s refused: %p is not a disc request", operation,
             static_cast<const void*>(request));
}
}  // namespace

Content_DiscQueueSlot* AllocateLiveDiscRequest(const Content_DiscFile* file) {
  if (file == nullptr) {
    Core_Warnf("content", "AllocateDiscRequest refused: null file");
    return nullptr;
  }
  DiscRequestQueue queue = LoadQueue();
  const std::optional<std::size_t> index =
      AllocateDiscRequest(queue, Extent(*file));
  if (!index) {
    if (std::has_single_bit(++refused_requests)) {
      Core_Warnf("content",
                 "AllocateDiscRequest refused: all %zu disc requests are in "
                 "use (sector %d, %d bytes); %" PRIu64 " refused",
                 kDiscRequestCapacity, file->lba, file->size, refused_requests);
    }
    return nullptr;
  }
  const auto generation = NextRequestGeneration();
  if (generation == 0) {
    return nullptr;
  }
  CommitQueue(queue);
  request_generations[*index] = generation;
  return &Content_discRequestSlots[*index];
}

std::uint64_t LiveDiscRequestGeneration(const Content_DiscQueueSlot* request) {
  if (const auto index = IndexOf(request)) {
    return request->state == Content_DiscRequestState_Free
               ? 0
               : request_generations[*index];
  }
  if (const auto* record = FindRefusedRecord(request)) {
    return record->slot.state == Content_DiscRequestState_Free
               ? 0
               : record->generation;
  }
  WarnForeignRequest("DiscRequestGeneration", request);
  return 0;
}

void ReleaseLiveDiscRequest(Content_DiscQueueSlot* request) {
  // A refused request (#282) holds nothing to free.
  if (request == nullptr) return;
  const std::optional<std::size_t> index = IndexOf(request);
  if (!index) {
    RefusedRecord* record = FindRefusedRecord(request);
    if (record == nullptr) {
      WarnForeignRequest("ReleaseDiscRequest", request);
    } else {
      record->slot.state = Content_DiscRequestState_Free;
    }
    return;
  }
  DiscRequestQueue queue = LoadQueue();
  ReleaseDiscRequest(queue, *index);
  CommitQueue(queue);
}

bool CancelLiveDiscWrites(std::span<std::byte> destination) {
  if (destination.empty()) {
    Core_Warnf("content", "CancelDiscWrites refused: empty destination");
    return false;
  }
  DiscRequestQueue queue = LoadQueue();
  LiveDiscQueueHost host;
  if (!CancelDiscWrites(queue, Destination(destination.data()), host)) {
    Core_Warnf("content", "CancelDiscWrites refused: null destination");
    return false;
  }
  CommitQueue(queue);
  return true;
}

Content_DiscQueueSlot* CompleteLiveRefusedDiscRequest(
    const Content_DiscFile* file, void* destination) {
  if (file == nullptr) {
    Core_Warnf("content", "CompleteRefusedDiscRequest refused: null file");
    return nullptr;
  }
  const auto generation = NextRequestGeneration();
  if (generation == 0) {
    return nullptr;
  }
  const bool read = ReadLiveRefusedDiscFile(file, destination) != 0;
  RefusedRecord& owner = FreeRefusedRecord();
  Content_DiscQueueSlot& record = owner.slot;
  owner.generation = generation;
  record = Content_DiscQueueSlot{};
  record.state = static_cast<std::int16_t>(
      read ? Content_DiscRequestState_Loaded : Content_DiscRequestState_Error);
  record.cdFile = *file;
  record.vram = destination;
  return &record;
}

std::size_t LiveRefusedDiscRecordsInUse() noexcept {
  return static_cast<std::size_t>(
      std::ranges::count_if(refused_records, [](const auto& record) {
        return record->slot.state != Content_DiscRequestState_Free;
      }));
}

void EnqueueLiveDiscRequest(Content_DiscQueueSlot* request, void* destination) {
  const std::optional<std::size_t> index = IndexOf(request);
  if (!index) {
    WarnForeignRequest("EnqueueDiscRequest", request);
    return;
  }
  DiscRequestQueue queue = LoadQueue();
  EnqueueDiscRequest(queue, *index, Destination(destination));
  CommitQueue(queue);
}

void EnqueueLivePriorityDiscRequest(Content_DiscQueueSlot* request,
                                    void* destination) {
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
  request_generations.fill(0);
  // The records are freed like the array's slots but never deallocated.
  for (const auto& record : refused_records) {
    record->slot.state = Content_DiscRequestState_Free;
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
    Core_Warnf("content",
               "Accepted disc read failed: sector %d, %d bytes, state %d",
               sector, bytes, static_cast<int>(host.DriveState()));
  }
  return result == DiscReadResult::kComplete ? 1 : 0;
}

int ReadLiveRefusedDiscFile(const Content_DiscFile* file, void* destination) {
  if (file == nullptr) {
    Core_Warnf("content", "ReadRefusedDiscFile refused: null file");
    return 0;
  }
  LiveDiscQueueHost host;
  if (!ReadRefusedDiscFile(host, Extent(*file), Destination(destination),
                           kRefusedReadWaitFrames)) {
    Core_Warnf("content",
               "ReadRefusedDiscFile: sector %d (%d bytes) was not read; the "
               "drive stayed in state %d",
               file->lba, file->size, static_cast<int>(host.DriveState()));
    return 0;
  }
  return 1;
}

std::uint64_t LiveDiscRequestRefusals() noexcept { return refused_requests; }

int LiveDiscState() {
  // lbu: the state byte is unsigned.
  return static_cast<int>(DiscState(LoadTransfer()));
}

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
    Core_Warnf("content",
               "XaReadyCallback refused: a %zu-byte result is shorter than "
               "%zu bytes",
               result.size(), kXaReadyResultBytes);
  }
  CommitTransfer(transfer);
}

}  // namespace VS::Content
