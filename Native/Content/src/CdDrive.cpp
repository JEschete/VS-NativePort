// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "CdDrive.h"
#include "Core/Memory/Heap.h"
#include "Core/Runtime.h"
#include "DiscQueue.h"
#include "DiscReader.h"
#include "State/PlayTime.h"

// decomp: SLUS_010.40 _diskReadCallback (src/SLUS_010.40/main.c:9021)
void Content_ReadSectorCallback(u_char intr, u_char result[] __attribute__((unused)),
    u_long* arg2 __attribute__((unused)))
{

    if ((intr == DslDiskError) || (intr == DslNoIntr)
        || (Content_discDriveState.state == Content_DiscDriveState_ReadError)) {
        Content_discDriveState.state = Content_DiscDriveState_ReadError;
        return;
    }

    if (intr != DslDataReady) {
        return;
    }

    Content_discDriveState.state = Content_DiscDriveState_ReadReady;

    if (Content_discDriveState.bufferMode == Content_DiscBufferMode_Direct) {
        DsGetSector(((u_char*)(Content_discDriveState.destination)) + Content_discDriveState.destinationSector * 2048, 512);
    } else {
        DsGetSector(Content_legacyDiscReadBuffer + Content_discDriveState.ringWriteIndex * 128, 512);
        if (++Content_discDriveState.ringWriteIndex >= 16) {
            Content_discDriveState.ringWriteIndex = 0;
        }
    }

    switch (Content_discDriveState.bufferMode) {
    case Content_DiscBufferMode_Direct:
        if (++Content_discDriveState.destinationSector >= Content_discDriveState.sectorCount) {
            Content_discDriveState.state = Content_DiscDriveState_Idle;
        }
        break;
    case Content_DiscBufferMode_Buffered:
        ++Content_discDriveState.ringSectorsWritten;
        ++Content_discDriveState.bufferedSectors;
        break;
    case Content_DiscBufferMode_Manual:
        ++Content_discDriveState.ringSectorsWritten;
        if (++Content_discDriveState.ringReadIndex >= 16) {
            Content_discDriveState.ringReadIndex = 0;
        }
        break;
    default:
        break;
    }

    if (++Content_discDriveState.sectorsReceived >= Content_discDriveState.sectorCount) {
    }
}

// decomp: SLUS_010.40 _diskReadCommandCallback (src/SLUS_010.40/main.c:9073)
void Content_ReadCommandCallback(u_char intr, u_char result[] __attribute__((unused)))
{
    switch (intr) {
    case DslComplete:
        return;
    case DslDiskError:
        break;
    default:
        break;
    case DslNoResult:
    case DslNoIntr:
        return;
    }
    Content_discDriveState.state = Content_DiscDriveState_ReadError;
}

// decomp: SLUS_010.40 _diskSeekCommandCallback (src/SLUS_010.40/main.c:9090)
void Content_SeekCommandCallback(u_char intr, u_char result[] __attribute__((unused)))
{
    switch (intr) {
    case DslNoIntr:
        break;
    case DslComplete:
        Content_discDriveState.state = Content_DiscDriveState_Idle;
        break;
    case DslDiskError:
        Content_discDriveState.state = Content_DiscDriveState_ReadError;
        break;
    case DslNoResult:
        break;
    default:
        Content_discDriveState.state = Content_DiscDriveState_ReadError;
        break;
    }
}

// decomp: SLUS_010.40 _diskPcmReadReady (src/SLUS_010.40/main.c:9109)
void Content_XaReadyCallback(u_char intr, u_char result[])
{
    switch (intr) {
    case DslComplete:
        Content_discDriveState.state = Content_DiscDriveState_Idle;
        break;
    case DslDataReady:
        if (!(result[4] & 0x80)) {
            Content_legacyXaReadyBcdField = ((result[1] >> 4) * 0xA) + (result[1] & 0xF);
            Content_legacyXaReadyByteThree = result[3];
            Content_legacyXaReadyByteFour = result[4];
            Content_xaReadyMetadataWord = (result[6] << 8) | result[7];
        }
        break;
    case DslDiskError:
        Content_discDriveState.state = Content_DiscDriveState_CommandFailed;
        break;
    }
}

// decomp: SLUS_010.40 _diskGetState (src/SLUS_010.40/main.c:9131)
int Content_DiscState(void) { return Content_discDriveState.state; }

// decomp: SLUS_010.40 _diskClearError (src/SLUS_010.40/main.c:9133)
void Content_ClearDiscReadError(void)
{
    if (Content_discDriveState.state == Content_DiscDriveState_ReadError) {
        Content_discDriveState.state = Content_DiscDriveState_Idle;
    }
}

// decomp: SLUS_010.40 vs_main_diskLoadFile (src/SLUS_010.40/main.c:9334)
int Content_ReadDiscFileBlocking(int sector, int bytes, void* vram)
{
    int result = Content_BeginDiscRead(sector, bytes, vram);
    if (result != 0) {
        int seekReady = Content_DiscDriveState_SeekReady;
        int readReady = Content_DiscDriveState_ReadReady;

        while (1) {
            if (Content_DiscState() == seekReady) {
                State_UpdatePlayTimeAndLoading(0);
            } else if (Content_DiscState() == readReady) {
                State_UpdatePlayTimeAndLoading(0);
            } else if (Content_DiscState() == 3) {
                State_UpdatePlayTimeAndLoading(0);
            } else if (Content_DiscState() == Content_DiscDriveState_ReadInit) {
                State_UpdatePlayTimeAndLoading(0);
            } else {
                break;
            }
        }
    }
    return result;
}

// decomp: SLUS_010.40 _initCdQueue (src/SLUS_010.40/main.c:9401)
void Content_InitializeDiscQueue(void)
{
    int i;

    for (i = 31; i >= 0; --i) {
        Content_discRequestSlots[i].state = Content_DiscRequestState_Free;
    }
    *(int*)&Content_discRequestCounts = 0;
}

// decomp: SLUS_010.40 vs_main_allocateCdQueueSlot (src/SLUS_010.40/main.c:9411)
Content_DiscQueueSlot* Content_AllocateDiscRequest(Content_DiscFile const* file)
{
    int i;

    for (i = 0; i < 32; ++i) {
        if (Content_discRequestSlots[i].state == Content_DiscRequestState_Free) {
            Content_discRequestSlots[i].state = Content_DiscRequestState_Allocated;
            Content_discRequestSlots[i].cdFile = *file;
            return &Content_discRequestSlots[i];
        }
    }
    Core_IgnoreDiagnostic(162, 0);
}

// decomp: SLUS_010.40 vs_main_freeCdQueueSlot (src/SLUS_010.40/main.c:9425)
void Content_ReleaseDiscRequest(Content_DiscQueueSlot* slot)
{
    u_short state = slot->state - 2;
    if (state < Content_DiscRequestState_Loaded - 2) {
        Core_IgnoreDiagnostic(163, 0);
    }
    slot->state = Content_DiscRequestState_Free;
}

// decomp: SLUS_010.40 vs_main_cdEnqueue (src/SLUS_010.40/main.c:9434)
void Content_EnqueueDiscRequest(Content_DiscQueueSlot* slot, void* vram)
{
    slot->state = Content_DiscRequestState_Enqueued;
    slot->vram = vram;
    slot->opaqueC = 0;
    slot->priority = Content_discRequestCounts.waiting++;
}

// decomp: SLUS_010.40 vs_main_cdEnqueuePriority (src/SLUS_010.40/main.c:9442)
void Content_EnqueuePriorityDiscRequest(Content_DiscQueueSlot* slot, void* vram)
{
    int i;
    Content_DiscQueueSlot* queue = Content_discRequestSlots;

    for (i = 0; i < 32; ++i, ++queue) {
        if (queue->state == Content_DiscRequestState_Enqueued) {
            ++queue->priority;
        }
    }
    slot->state = Content_DiscRequestState_Enqueued;
    slot->vram = vram;
    slot->opaqueC = 0;
    slot->priority = 0;
    ++Content_discRequestCounts.waiting;
}

// decomp: SLUS_010.40 _processCdQueue (src/SLUS_010.40/main.c:9461)
void Content_UpdateDiscQueue(void)
{
    ++Content_discDriveState.framesSinceLastRead;

    if (*(int*)&Content_discRequestCounts != 0) {
        Content_DiscQueueSlot* slot = Content_discRequestSlots;
        int i = Content_DiscState();
        if (i == Content_DiscDriveState_Idle) {
            for (i = 0; i < 32; ++i, ++slot) {
                if (slot->state == Content_DiscRequestState_Enqueued) {
                    if (slot->priority == 0) {
                        slot->state = Content_DiscRequestState_Reading;
                        Content_BeginDiscRead(slot->cdFile.lba, slot->cdFile.size, slot->vram);
                        Content_discDriveState.framesSinceLastRead = 0;
                        --Content_discRequestCounts.waiting;
                        ++Content_discRequestCounts.reading;
                    } else {
                        --slot->priority;
                    }
                } else if (slot->state == Content_DiscRequestState_Reading) {
                    slot->state = Content_DiscRequestState_Loaded;
                    --Content_discRequestCounts.reading;
                }
            }
        }
    }
    Content_UpdateDiscCommands();
}

}  // extern "C"
