#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

// The disc request queue and the drive's interrupt handlers (SLUS_010.40
// main.c:9021-9500). Loaders allocate a request for a file extent, enqueue it
// with a destination and poll it until the queue reports it loaded; the
// queue starts one read whenever the drive is idle. The live binding
// (CdDriveLive.hpp) keeps the legacy request array and drive record.
namespace VS::Content {

inline constexpr std::size_t kDiscRequestCapacity = 32;
inline constexpr std::uint32_t kDiscRingSectors = 16;
// vs_main_nop9 codes the original reported.
inline constexpr int kDiscQueueFullDiagnostic = 0xA2;
inline constexpr int kDiscRequestBusyDiagnostic = 0xA3;

// vs_main_disk_t.state (diskState_e, main.c:77). Values the enum does not
// name (3) are kept as stored.
enum class DiscDriveState : std::uint8_t {
  kIdle = 0,
  kSeekReady = 1,
  kReadReady = 2,
  kUnnamed3 = 3,
  kReadError = 4,
  kReadInit = 5,
  kReadXaInit = 6,
  kStreamXa = 7,
  kCommandFailed = 8,
  kStreamXaEnd = 9,
};

// vs_main_CdQueueState (main.h:26); other stored values are kept.
enum class DiscRequestState : std::int16_t {
  kFree = 0,
  kAllocated = 1,
  kReading = 2,
  kEnqueued = 3,
  kLoaded = 4,
  kError = 6,
  kDispatched = 7,
};

// cdBufferModes_e (main.c:89).
enum class DiscBufferMode : std::int32_t {
  kDirect = 0,
  kBuffered = 1,
  kManual = 2,
};

// The drive interrupt codes the handlers distinguish (DslNoIntr ...).
enum class DiscInterrupt : std::uint8_t {
  kNone = 0,
  kDataReady = 1,
  kComplete = 2,
  kDiskError = 5,
  kNoResult = 6,
};

// vs_main_CdFile: a file's first sector and byte count.
struct DiscFileExtent {
  std::int32_t sector = 0;
  std::int32_t bytes = 0;
};

// The buffer a request reads into, as an opaque handle: the queue stores it
// and hands it back to the host, never dereferences it.
enum class DiscDestination : std::uintptr_t { kNone = 0 };

enum class DiscReadResult : std::uint8_t { kRefused, kFailed, kComplete };

// vs_main_CdQueueSlot.
struct DiscRequest {
  DiscRequestState state = DiscRequestState::kFree;
  // Frames to wait behind earlier requests once enqueued (0 starts next).
  std::uint16_t priority = 0;
  DiscFileExtent file;
  // The slot's unused word, cleared on enqueue.
  std::int32_t reserved = 0;
  DiscDestination destination = DiscDestination::kNone;
};

struct DiscRequestQueue {
  std::array<DiscRequest, kDiscRequestCapacity> requests{};
  // Requests reading and waiting (D_80050118); both zero skips the scan.
  std::uint16_t reading = 0;
  std::uint16_t waiting = 0;
};

// The drive services the queue and the blocking reader use.
class DiscQueueHost {
 public:
  DiscQueueHost() = default;
  DiscQueueHost(const DiscQueueHost&) = delete;
  DiscQueueHost& operator=(const DiscQueueHost&) = delete;
  DiscQueueHost(DiscQueueHost&&) = delete;
  DiscQueueHost& operator=(DiscQueueHost&&) = delete;
  virtual ~DiscQueueHost() = default;

  [[nodiscard]] virtual DiscDriveState DriveState() const = 0;
  // Arms a read of `file` into `destination`; false when the drive refuses.
  virtual bool BeginRead(const DiscFileExtent& file,
                         DiscDestination destination) = 0;
  // Completion of this host's accepted operation, not merely an idle drive.
  [[nodiscard]] virtual bool ReadCompleted() const = 0;
  virtual bool BeginQueuedRead(std::size_t index, const DiscFileExtent& file,
                               DiscDestination destination) = 0;
  // Identify a failed queued command, never an unrelated blocking read.
  [[nodiscard]] virtual std::optional<std::size_t> FailedRequest() const = 0;
  virtual void ClearReadFailure() = 0;
  // Stop future writes to this destination without affecting another read.
  virtual void CancelRead(DiscDestination destination) = 0;
  // Serves the drive's pending commands (once per queue update).
  virtual void UpdateCommands() = 0;
  // The drive's frames-since-last-read counter (wraps).
  virtual void CountFrameSinceRead() = 0;
  virtual void RestartFramesSinceRead() = 0;
  // Runs one game frame while a blocking read waits.
  virtual void WaitFrame() = 0;
};

// _initCdQueue (main.c:9401): frees every request (only its state) and
// clears both counts.
void InitializeDiscQueue(DiscRequestQueue& queue);

// vs_main_allocateCdQueueSlot (main.c:9411): the first free request, now
// allocated for `file`. When all 32 are in use the original reported 0xA2 and
// returned no value (#282); this reports 0xA2 and returns nullopt.
[[nodiscard]] std::optional<std::size_t> AllocateDiscRequest(
    DiscRequestQueue& queue, const DiscFileExtent& file);

// vs_main_freeCdQueueSlot (main.c:9425): frees request `index`, reporting
// 0xA3 when it was still reading or enqueued. False (no change) for an index
// outside the queue.
bool ReleaseDiscRequest(DiscRequestQueue& queue, std::size_t index);

// vs_main_cdEnqueue (main.c:9434): queues request `index` behind the
// requests already waiting. False (no change) for an index outside the
// queue.
bool EnqueueDiscRequest(DiscRequestQueue& queue, std::size_t index,
                        DiscDestination destination);

// vs_main_cdEnqueuePriority (main.c:9442): queues request `index` ahead of
// every waiting request. False (no change) for an index outside the queue.
bool EnqueuePriorityDiscRequest(DiscRequestQueue& queue, std::size_t index,
                                DiscDestination destination);

// _processCdQueue (main.c:9461): counts the frame, and while the drive is
// idle marks finished reads loaded, starts the request whose turn it is and
// moves the others up; then serves the drive's commands.
void UpdateDiscQueue(DiscRequestQueue& queue, DiscQueueHost& host);
// The Live binding serves deferred commands after publishing the queue.
// Call again after those commands to publish their failure before polling.
bool ResolveFailedDiscRequest(DiscRequestQueue& queue, DiscQueueHost& host);
// Marks only pending writes to destination as Error, retaining their handles
// for the owner to release, but clearing their borrowed destinations.
// Cancellation precedes every ownership change.
[[nodiscard]] bool CancelDiscWrites(DiscRequestQueue& queue,
                                    DiscDestination destination,
                                    DiscQueueHost& host);

// vs_main_diskLoadFile (main.c:9334): starts a read and runs frames until
// the drive leaves its seek/read phases. Acceptance and completion are
// distinct.
[[nodiscard]] DiscReadResult ReadDiscFileBlocking(DiscQueueHost& host,
                                                  const DiscFileExtent& file,
                                                  DiscDestination destination);

// #282: reads a file whose request the full queue refused. Runs frames while
// the drive is busy with another command or stream, as the queue would have
// waited, at most `max_wait_frames`, then reads it blocking. False when the
// drive stayed busy, refused the read, or failed after accepting it.
bool ReadRefusedDiscFile(DiscQueueHost& host, const DiscFileExtent& file,
                         DiscDestination destination,
                         std::uint32_t max_wait_frames);

// The drive record fields the interrupt handlers use.
struct DiscTransfer {
  DiscDriveState state = DiscDriveState::kIdle;
  std::uint32_t sectors_received = 0;
  std::uint32_t sector_count = 0;
  DiscBufferMode buffer_mode = DiscBufferMode::kDirect;
  std::uint32_t destination_sector = 0;
  std::int32_t buffered_sectors = 0;
  std::uint32_t ring_sectors_written = 0;
  std::uint32_t ring_write_index = 0;
  std::uint32_t ring_read_index = 0;
};

// The sub-header fields an XA data-ready interrupt reports.
struct XaSectorInfo {
  std::int32_t bcd_field = 0;
  std::int32_t byte_three = 0;
  std::int32_t byte_four = 0;
  std::int32_t metadata = 0;
};
inline constexpr std::size_t kXaReadyResultBytes = 8;

// The drive controller services the interrupt handlers use.
class DiscInterruptHost {
 public:
  DiscInterruptHost() = default;
  DiscInterruptHost(const DiscInterruptHost&) = delete;
  DiscInterruptHost& operator=(const DiscInterruptHost&) = delete;
  DiscInterruptHost(DiscInterruptHost&&) = delete;
  DiscInterruptHost& operator=(DiscInterruptHost&&) = delete;
  virtual ~DiscInterruptHost() = default;

  // Copies the ready sector to sector `sector` of the read's destination.
  virtual void ReceiveSector(std::uint32_t sector) = 0;
  // Copies the ready sector to slot `slot` of the ring buffer.
  virtual void ReceiveRingSector(std::uint32_t slot) = 0;
  // DsStartReadySystem(_diskReadCallback, -1).
  virtual void StartSectorDelivery() = 0;
  // DsReadyCallback(0).
  virtual void ClearReadyCallback() = 0;
  // DsEndReadySystem().
  virtual void EndSectorDelivery() = 0;
  virtual void RecordXaSector(const XaSectorInfo& info) = 0;
};

// _diskReadCallback (main.c:9021).
void ReadSectorCallback(DiscTransfer& drive, DiscInterrupt interrupt,
                        DiscInterruptHost& host);
// _diskReadCommandCallback (main.c:9073).
void ReadCommandCallback(DiscTransfer& drive, DiscInterrupt interrupt,
                         DiscInterruptHost& host);
// _diskSeekCommandCallback (main.c:9090).
void SeekCommandCallback(DiscTransfer& drive, DiscInterrupt interrupt);
// _diskPcmReadReady (main.c:9109). A data-ready result shorter than
// kXaReadyResultBytes is ignored (returns false).
bool XaReadyCallback(DiscTransfer& drive, DiscInterrupt interrupt,
                     std::span<const std::uint8_t> result,
                     DiscInterruptHost& host);

// _diskGetState (main.c:9131).
[[nodiscard]] DiscDriveState DiscState(const DiscTransfer& drive);
// _diskClearError (main.c:9133): a read error returns the drive to idle.
void ClearDiscReadError(DiscTransfer& drive);

}  // namespace VS::Content
