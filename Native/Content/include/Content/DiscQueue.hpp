#pragma once
// The legacy disc request array, its counts and the drive record that the
// live binding (CdDriveLive.cpp) and the copied loaders share, at their PS1
// layouts. The queue's typed API is Content/CdDrive.hpp; these records carry
// its DiscRequestState, DiscDriveState and DiscBufferMode values.

#include <array>
#include <cstddef>
#include <cstdint>

#include "Content/CdDrive.hpp"
#include "Legacy/LibDs.hpp"

namespace VS::Content {

inline constexpr std::size_t kDiscRequestCountsBytes = 0x4;
inline constexpr std::size_t kDiscFileBytes = 0x8;
// The libds control-command response buffer's length.
inline constexpr std::size_t kDiscControlResponseBytes = 11;

// decomp: CdQueueCount (src/SLUS_010.40/main.c:199-202): the requests
// reading and waiting, 16-bit fields upstream.
struct DiscRequestCounts {
  std::int16_t reading;
  std::int16_t waiting;
};
static_assert(sizeof(DiscRequestCounts) == kDiscRequestCountsBytes,
              "DiscRequestCounts must keep its PS1 layout");

// decomp: vs_CdFile (src/SLUS_010.40/cd.h:5-8): a file's first sector and
// byte count.
struct DiscFile {
  std::int32_t lba;
  std::int32_t size;
};
static_assert(sizeof(DiscFile) == kDiscFileBytes,
              "DiscFile must keep its PS1 layout");

// decomp: vs_CdQueueSlot (src/SLUS_010.40/cd.h:20-26).
struct DiscQueueSlot {
  DiscRequestState state;
  std::int16_t priority;
  DiscFile file;
  // Upstream's UNUSED word: vs_main_cdEnqueue and vs_main_cdEnqueuePriority
  // only clear it (main.c:2411, 2428). The live binding round-trips it as
  // DiscRequest::reserved.
  std::int32_t reserved;
  // vram upstream: the buffer the read fills (main.c:2410, 2453).
  void* destination;
};

// decomp: Disk (src/SLUS_010.40/main.c:61-89).
struct DiscDrive {
  DiscDriveState state;
  // Upstream's UNUSED mode byte; the XA start and stop paths mark it.
  std::uint8_t xa_mode_marker;
  std::uint8_t reserved2;
  // errorCount upstream.
  std::uint8_t failure_count;
  std::uint32_t sectors_received;
  std::uint32_t sector_count;
  std::int32_t start_sector;
  // Upstream's request union: the byte count of a data read, the target CD
  // volume of an XA stream.
  std::int32_t byte_count;
  void* destination;
  Legacy::DslFILTER xa_filter;
  // Upstream's cd union: the seek/read position, whose first byte is also
  // the DslSetmode parameter.
  Legacy::DslLOC position;
  std::int32_t command_id;
  std::int32_t reserved24;
  std::int32_t reserved28;
  DiscBufferMode buffer_mode;
  std::uint32_t destination_sector;
  std::int32_t buffered_sectors;
  std::uint32_t ring_sectors_written;
  std::uint32_t ring_write_index;
  std::uint32_t ring_read_index;
  std::int32_t frames_since_last_read;
};

// decomp: cdReadBuffer (src/SLUS_010.40/main.c:837): the XA/ring read buffer.
extern std::uint8_t* legacyDiscReadBuffer;
// decomp: cdReadLoc (src/SLUS_010.40/main.c:847).
extern Legacy::DslLOC legacyDiscReadLocation;
// decomp: cdaTrack, cdaMinute, cdaSecond and cdaPeak
// (src/SLUS_010.40/main.c:851), set by diskPcmReadReady
// (src/SLUS_010.40/main.c:2018-2021).
extern std::int32_t legacyXaReadyBcdField;
extern std::int32_t legacyXaReadyByteThree;
extern std::int32_t legacyXaReadyByteFour;
extern std::int32_t xaReadyMetadataWord;
// decomp: cdQueue (src/SLUS_010.40/main.c:853).
extern std::array<DiscQueueSlot, kDiscRequestCapacity> discRequestSlots;
// decomp: cdQueueCount (src/SLUS_010.40/main.c:854).
extern DiscRequestCounts discRequestCounts;
// decomp: disk (src/SLUS_010.40/main.c:866).
extern DiscDrive discDriveState;
// decomp: _dsControlBuf (libds data at 0x80055D2C; no upstream source).
extern std::array<char, kDiscControlResponseBytes> discControlResponseBytes;

}  // namespace VS::Content
