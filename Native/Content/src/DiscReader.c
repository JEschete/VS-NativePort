// Hand-maintained (tools/port/owned.csv).
// The disc drive, modernized: a read the copied queue starts
// (Content_BeginDiscRead) is served from the extracted game data
// (Content/DataStore.hpp) on the next queue update, with the state transitions
// the queue and the blocking reader poll for. No drive, seek or sector
// callbacks remain.
#include "DiscReader.h"

#include <limits.h>

#include "Audio/XaPlayback.h"
#include "Content/DataAccess.h"
#include "DiscQueue.h"
#include "Legacy/Psx.h"

enum { kInactiveXaMarker = 128 };

// decomp: SLUS_010.40 _diskInitRead (src/SLUS_010.40/main.c:9140)
int Content_BeginDiscRead(int sector, u_int bytes, void* vram) {
  enum { kDataSectorBytes = 2048 };
  if (Content_discDriveState.state != Content_DiscDriveState_Idle ||
      bytes > INT_MAX) {
    return 0;
  }
  Content_discDriveState.startSector = sector;
  Content_discDriveState.byteCount = (int)bytes;
  Content_discDriveState.destination = vram;
  Content_discDriveState.ringReadIndex = 0;
  Content_discDriveState.ringWriteIndex = 0;
  Content_discDriveState.destinationSector = 0;
  Content_discDriveState.bufferedSectors = 0;
  Content_discDriveState.sectorsReceived = 0;
  Content_discDriveState.failureCount = 0;
  Content_discDriveState.ringSectorsWritten = 0;
  Content_discDriveState.sectorCount = bytes / kDataSectorBytes;
  if (bytes % kDataSectorBytes) {
    ++Content_discDriveState.sectorCount;
  }
  Content_discDriveState.state = Content_DiscDriveState_ReadInit;
  return 1;
}

// decomp: SLUS_010.40 func_800443CC (src/SLUS_010.40/main.c:9164)
void Content_UpdateDiscCommands(void) {
  switch (Content_discDriveState.state) {
    case Content_DiscDriveState_ReadInit:
      if (Content_discDriveState.byteCount != 0 &&
          !Content_ReadDiscBytes(Content_discDriveState.startSector,
                                 Content_discDriveState.byteCount,
                                 Content_discDriveState.destination)) {
        ++Content_discDriveState.failureCount;
        Content_discDriveState.state = Content_DiscDriveState_ReadError;
        break;
      }
      Content_discDriveState.sectorsReceived =
          Content_discDriveState.sectorCount;
      Content_discDriveState.destinationSector =
          Content_discDriveState.sectorCount;
      Content_discDriveState.state = Content_DiscDriveState_Idle;
      break;
    case Content_DiscDriveState_ReadXaInit:
      if (!Audio_Native_StartXaStream(Content_discDriveState.startSector,
                                      Content_discDriveState.byteCount)) {
        ++Content_discDriveState.failureCount;
        Content_discDriveState.state = Content_DiscDriveState_ReadError;
        break;
      }
      Content_discDriveState.xaModeMarker = 0;
      Content_discDriveState.position.minute = 0;
      Content_discDriveState.state = Content_DiscDriveState_StreamXa;
      break;
    case Content_DiscDriveState_StreamXa: {
      const int status = Audio_Native_XaStreamStatus();
      if (status < 0) {
        ++Content_discDriveState.failureCount;
        Content_discDriveState.state = Content_DiscDriveState_ReadError;
      } else if (status == 0) {
        Audio_Native_StopXaStream();
      }
      break;
    }
    case Content_DiscDriveState_StreamXaEnd:
      Audio_Native_StopXaStream();
      break;
    default:
      break;
  }
}

// decomp: SLUS_010.40 _diskReset (src/SLUS_010.40/main.c:9385)
void Content_ResetDiscDrive(void) {
  enum { kModeChangeTicks = 3 };
  Audio_Native_StopXaStream();
  Content_discDriveState.xaModeMarker = kInactiveXaMarker;
  Content_discDriveState.position.minute = kInactiveXaMarker;
  Content_discDriveState.reserved2 = 0;
  Content_discDriveState.state = Content_DiscDriveState_Idle;
  Content_discDriveState.bufferMode = Content_DiscBufferMode_Direct;
  Content_legacyDiscReadBuffer = NULL;
  VSync(kModeChangeTicks);  // the drive's mode change took three frames; kept
                            // so start-up timing matches
}
