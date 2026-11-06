// Hand-maintained (tools/port/owned.csv).
#include "Content/CdDrive.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Core/Runtime.hpp"

namespace VS::Content {
namespace {
// The phases vs_main_diskLoadFile keeps waiting in.
bool BlockingReadPending(DiscDriveState state) {
  switch (state) {
    case DiscDriveState::kSeekReady:
    case DiscDriveState::kReadReady:
    case DiscDriveState::kUnnamed3:
    case DiscDriveState::kReadInit:
      return true;
    default:
      return false;
  }
}

DiscRequest* At(DiscRequestQueue& queue, std::size_t index) {
  return index < queue.requests.size() ? &queue.requests[index] : nullptr;
}

void Queue(DiscRequest& request, DiscDestination destination,
           std::uint16_t priority) {
  request.state = DiscRequestState::kEnqueued;
  request.destination = destination;
  request.reserved = 0;
  request.priority = priority;
}

void RecountRequests(DiscRequestQueue& queue) {
  queue.reading = 0;
  queue.waiting = 0;
  for (const DiscRequest& request : queue.requests) {
    if (request.state == DiscRequestState::kReading) ++queue.reading;
    if (request.state == DiscRequestState::kEnqueued) ++queue.waiting;
  }
}
}  // namespace

// decomp: SLUS_010.40 _diskReadCallback (src/SLUS_010.40/main.c:9021)
void ReadSectorCallback(DiscTransfer& drive, DiscInterrupt interrupt,
                        DiscInterruptHost& host) {
  if (interrupt == DiscInterrupt::kDiskError ||
      interrupt == DiscInterrupt::kNone ||
      drive.state == DiscDriveState::kReadError) {
    host.ClearReadyCallback();
    host.EndSectorDelivery();
    drive.state = DiscDriveState::kReadError;
    return;
  }
  if (interrupt != DiscInterrupt::kDataReady) return;

  drive.state = DiscDriveState::kReadReady;
  if (drive.buffer_mode == DiscBufferMode::kDirect) {
    host.ReceiveSector(drive.destination_sector);
  } else {
    host.ReceiveRingSector(drive.ring_write_index);
    // sltiu 0x10
    if (++drive.ring_write_index >= kDiscRingSectors) {
      drive.ring_write_index = 0;
    }
  }

  switch (drive.buffer_mode) {
    case DiscBufferMode::kDirect:
      // sltu: both counts are unsigned.
      if (++drive.destination_sector >= drive.sector_count) {
        drive.state = DiscDriveState::kIdle;
      }
      break;
    case DiscBufferMode::kBuffered:
      ++drive.ring_sectors_written;
      // The original's 32-bit add wraps.
      drive.buffered_sectors = static_cast<std::int32_t>(
          static_cast<std::uint32_t>(drive.buffered_sectors) + 1U);
      break;
    case DiscBufferMode::kManual:
      ++drive.ring_sectors_written;
      if (++drive.ring_read_index >= kDiscRingSectors) {
        drive.ring_read_index = 0;
      }
      break;
    default:
      break;
  }

  if (++drive.sectors_received >= drive.sector_count) {
    host.EndSectorDelivery();
  }
}

// decomp: SLUS_010.40 _diskReadCommandCallback
// (src/SLUS_010.40/main.c:9073)
void ReadCommandCallback(DiscTransfer& drive, DiscInterrupt interrupt,
                         DiscInterruptHost& host) {
  switch (interrupt) {
    case DiscInterrupt::kComplete:
      host.StartSectorDelivery();
      return;
    case DiscInterrupt::kNone:
    case DiscInterrupt::kNoResult:
      return;
    default:
      drive.state = DiscDriveState::kReadError;
      return;
  }
}

// decomp: SLUS_010.40 _diskSeekCommandCallback
// (src/SLUS_010.40/main.c:9090)
void SeekCommandCallback(DiscTransfer& drive, DiscInterrupt interrupt) {
  switch (interrupt) {
    case DiscInterrupt::kComplete:
      drive.state = DiscDriveState::kIdle;
      return;
    case DiscInterrupt::kNone:
    case DiscInterrupt::kNoResult:
      return;
    default:
      drive.state = DiscDriveState::kReadError;
      return;
  }
}

// decomp: SLUS_010.40 _diskPcmReadReady (src/SLUS_010.40/main.c:9109)
bool XaReadyCallback(DiscTransfer& drive, DiscInterrupt interrupt,
                     std::span<const std::uint8_t> result,
                     DiscInterruptHost& host) {
  constexpr std::uint8_t kNoSubheader = 0x80;
  constexpr std::int32_t kBcdTens = 10;
  constexpr std::uint8_t kBcdDigit = 0xF;
  constexpr unsigned kBcdTensShift = 4;
  constexpr unsigned kBitsPerByte = 8;
  constexpr std::size_t kBcdFieldAt = 1;
  constexpr std::size_t kByteThreeAt = 3;
  constexpr std::size_t kByteFourAt = 4;
  constexpr std::size_t kMetadataHighAt = 6;
  constexpr std::size_t kMetadataLowAt = 7;
  switch (interrupt) {
    case DiscInterrupt::kComplete:
      host.ClearReadyCallback();
      drive.state = DiscDriveState::kIdle;
      return true;
    case DiscInterrupt::kDataReady: {
      if (result.size() < kXaReadyResultBytes) return false;
      if ((result[kByteFourAt] & kNoSubheader) != 0) return true;
      // lbu; srl 4: unsigned BCD digits.
      host.RecordXaSector(
          {.bcd_field = ((result[kBcdFieldAt] >> kBcdTensShift) * kBcdTens) +
                        (result[kBcdFieldAt] & kBcdDigit),
           .byte_three = result[kByteThreeAt],
           .byte_four = result[kByteFourAt],
           .metadata = (std::int32_t{result[kMetadataHighAt]} << kBitsPerByte) |
                       result[kMetadataLowAt]});
      return true;
    }
    case DiscInterrupt::kDiskError:
      host.ClearReadyCallback();
      drive.state = DiscDriveState::kCommandFailed;
      return true;
    default:
      return true;
  }
}

// decomp: SLUS_010.40 _diskGetState (src/SLUS_010.40/main.c:9131)
DiscDriveState DiscState(const DiscTransfer& drive) { return drive.state; }

// decomp: SLUS_010.40 _diskClearError (src/SLUS_010.40/main.c:9133)
void ClearDiscReadError(DiscTransfer& drive) {
  if (drive.state == DiscDriveState::kReadError) {
    drive.state = DiscDriveState::kIdle;
  }
}

// decomp: SLUS_010.40 vs_main_diskLoadFile (src/SLUS_010.40/main.c:9334)
DiscReadResult ReadDiscFileBlocking(DiscQueueHost& host,
                                    const DiscFileExtent& file,
                                    DiscDestination destination) {
  if (!host.BeginRead(file, destination)) return DiscReadResult::kRefused;
  while (BlockingReadPending(host.DriveState())) {
    host.WaitFrame();
  }
  return host.ReadCompleted() ? DiscReadResult::kComplete
                              : DiscReadResult::kFailed;
}

bool ReadRefusedDiscFile(DiscQueueHost& host, const DiscFileExtent& file,
                         DiscDestination destination,
                         std::uint32_t max_wait_frames) {
  for (std::uint32_t frames = 0; host.DriveState() != DiscDriveState::kIdle;
       ++frames) {
    if (frames >= max_wait_frames) return false;
    host.WaitFrame();
  }
  return ReadDiscFileBlocking(host, file, destination) ==
         DiscReadResult::kComplete;
}

// decomp: SLUS_010.40 _initCdQueue (src/SLUS_010.40/main.c:9401)
void InitializeDiscQueue(DiscRequestQueue& queue) {
  for (DiscRequest& request : queue.requests) {
    request.state = DiscRequestState::kFree;
  }
  queue.reading = 0;
  queue.waiting = 0;
}

// decomp: SLUS_010.40 vs_main_allocateCdQueueSlot
// (src/SLUS_010.40/main.c:9411)
std::optional<std::size_t> AllocateDiscRequest(DiscRequestQueue& queue,
                                               const DiscFileExtent& file) {
  for (std::size_t index = 0; index < queue.requests.size(); ++index) {
    DiscRequest& request = queue.requests[index];
    if (request.state == DiscRequestState::kFree) {
      request.state = DiscRequestState::kAllocated;
      request.file = file;
      return index;
    }
  }
  Core::IgnoreDiagnostic(kDiscQueueFullDiagnostic, 0);
  return std::nullopt;
}

// decomp: SLUS_010.40 vs_main_freeCdQueueSlot (src/SLUS_010.40/main.c:9425)
bool ReleaseDiscRequest(DiscRequestQueue& queue, std::size_t index) {
  DiscRequest* const request = At(queue, index);
  if (request == nullptr) return false;
  // lhu; addiu -2; sltiu 2: still reading or enqueued.
  if (request->state == DiscRequestState::kReading ||
      request->state == DiscRequestState::kEnqueued) {
    Core::IgnoreDiagnostic(kDiscRequestBusyDiagnostic, 0);
  }
  request->state = DiscRequestState::kFree;
  return true;
}

// decomp: SLUS_010.40 vs_main_cdEnqueue (src/SLUS_010.40/main.c:9434)
bool EnqueueDiscRequest(DiscRequestQueue& queue, std::size_t index,
                        DiscDestination destination) {
  DiscRequest* const request = At(queue, index);
  if (request == nullptr) return false;
  Queue(*request, destination, queue.waiting++);
  return true;
}

// decomp: SLUS_010.40 vs_main_cdEnqueuePriority
// (src/SLUS_010.40/main.c:9442)
bool EnqueuePriorityDiscRequest(DiscRequestQueue& queue, std::size_t index,
                                DiscDestination destination) {
  DiscRequest* const request = At(queue, index);
  if (request == nullptr) return false;
  for (DiscRequest& waiting : queue.requests) {
    if (waiting.state == DiscRequestState::kEnqueued) {
      ++waiting.priority;
    }
  }
  Queue(*request, destination, 0);
  ++queue.waiting;
  return true;
}

// decomp: SLUS_010.40 _processCdQueue (src/SLUS_010.40/main.c:9461)
void UpdateDiscQueue(DiscRequestQueue& queue, DiscQueueHost& host) {
  ResolveFailedDiscRequest(queue, host);
  host.CountFrameSinceRead();
  if ((queue.reading != 0 || queue.waiting != 0) &&
      host.DriveState() == DiscDriveState::kIdle) {
    for (std::size_t index = 0; index < queue.requests.size(); ++index) {
      DiscRequest& request = queue.requests[index];
      if (request.state == DiscRequestState::kEnqueued) {
        if (request.priority == 0) {
          const bool accepted =
              host.BeginQueuedRead(index, request.file, request.destination);
          request.state =
              accepted ? DiscRequestState::kReading : DiscRequestState::kError;
          host.RestartFramesSinceRead();
          --queue.waiting;
          if (accepted) ++queue.reading;
        } else {
          --request.priority;
        }
      } else if (request.state == DiscRequestState::kReading) {
        request.state = DiscRequestState::kLoaded;
        --queue.reading;
      }
    }
  }
  host.UpdateCommands();
  ResolveFailedDiscRequest(queue, host);
}

bool ResolveFailedDiscRequest(DiscRequestQueue& queue, DiscQueueHost& host) {
  const std::optional<std::size_t> failed = host.FailedRequest();
  if (!failed) return false;
  DiscRequest* const request = At(queue, *failed);
  if (request == nullptr || request->state != DiscRequestState::kReading) {
    return false;
  }
  request->state = DiscRequestState::kError;
  RecountRequests(queue);
  host.ClearReadFailure();
  return true;
}

bool CancelDiscWrites(DiscRequestQueue& queue, DiscDestination destination,
                      DiscQueueHost& host) {
  if (destination == DiscDestination::kNone) return false;
  host.CancelRead(destination);
  for (DiscRequest& request : queue.requests) {
    if (request.destination != destination) continue;
    if (request.state == DiscRequestState::kEnqueued) {
      const auto removed_priority = request.priority;
      request.state = DiscRequestState::kError;
      request.destination = DiscDestination::kNone;
      for (DiscRequest& waiting : queue.requests) {
        if (waiting.state == DiscRequestState::kEnqueued &&
            waiting.priority > removed_priority) {
          --waiting.priority;
        }
      }
    } else if (request.state == DiscRequestState::kReading) {
      request.state = DiscRequestState::kError;
      request.destination = DiscDestination::kNone;
    }
  }
  RecountRequests(queue);
  return true;
}

}  // namespace VS::Content
