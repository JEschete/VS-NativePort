#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <cstdint>
#include <span>

struct Content_DiscFile;
struct Content_DiscQueueSlot;

// Live binding for the disc request queue and drive handlers (CdDrive.hpp)
// over the legacy request array (Content_discRequestSlots), its counts and
// the drive record (Content_discDriveState). Generated callers keep the
// original request-pointer call shapes.
//
// #282: when all 32 requests are in use AllocateLiveDiscRequest returns null
// (the original returned no value), counts the refusal and warns on the
// "content" channel at each power-of-two count. A null request is a refused
// one: releasing it does nothing; enqueueing it, or a pointer outside the
// array, is refused with a warning. Callers read a refused file with
// ReadLiveRefusedDiscFile, or with CompleteLiveRefusedDiscRequest, which
// returns a completed record that their polls and releases accept.
namespace VS::Content {

Content_DiscQueueSlot* AllocateLiveDiscRequest(const Content_DiscFile* file);
// Allocation identity, independent of the slot's address and destination.
// Zero means unallocated/released/reset. Reusing a queue or fallback record
// issues a fresh nonzero generation; clients can reject stale raw handles.
[[nodiscard]] std::uint64_t LiveDiscRequestGeneration(
    const Content_DiscQueueSlot* request);
void ReleaseLiveDiscRequest(Content_DiscQueueSlot* request);
// Game-thread cancellation for pending writes starting at destination.data().
// Stops the deferred command before marking matching requests Error. The owner
// can then release their handles and storage; unrelated destinations are
// untouched.
[[nodiscard]] bool CancelLiveDiscWrites(std::span<std::byte> destination);
void EnqueueLiveDiscRequest(Content_DiscQueueSlot* request, void* destination);
void EnqueueLivePriorityDiscRequest(Content_DiscQueueSlot* request,
                                    void* destination);
void InitializeLiveDiscQueue();
void UpdateLiveDiscQueue();
// Returns 1 only when the read completed; refusal/failure returns 0 and warns.
int ReadLiveDiscFileBlocking(int sector, int bytes, void* destination);
// Reads `file` into `destination` without a request (#282), once the drive
// is free. Returns 1 when it was read; 0 (with a warning) when the drive
// stayed busy for a minute of frames, refused the read, or failed to read.
int ReadLiveRefusedDiscFile(const Content_DiscFile* file, void* destination);
// #282: reads `file` as ReadLiveRefusedDiscFile does and returns a record
// outside the request array: Loaded when it was read, Error when not (a
// queued request that cannot be read never loads either). Releasing the
// record frees it for reuse; its address stays valid. Null for a null file.
Content_DiscQueueSlot* CompleteLiveRefusedDiscRequest(
    const Content_DiscFile* file, void* destination);
// Requests refused because the queue was full since start-up.
[[nodiscard]] std::uint64_t LiveDiscRequestRefusals() noexcept;
// Completed refused-request records not yet released.
[[nodiscard]] std::size_t LiveRefusedDiscRecordsInUse() noexcept;

int LiveDiscState();
void ClearLiveDiscReadError();
// The drive interrupt handlers over the drive record (interrupt codes are
// the drive's raw bytes).
void HandleLiveReadSectorInterrupt(std::uint8_t interrupt);
void HandleLiveReadCommandInterrupt(std::uint8_t interrupt);
void HandleLiveSeekCommandInterrupt(std::uint8_t interrupt);
void HandleLiveXaReadyInterrupt(std::uint8_t interrupt,
                                std::span<const std::uint8_t> result);

}  // namespace VS::Content
