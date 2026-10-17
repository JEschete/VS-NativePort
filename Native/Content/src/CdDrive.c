// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Audio/Control/SoundControl.h"
#include "Audio/Samples/SampleBanks.h"
#include "CdDrive.h"
#include "Core/Memory/Heap.h"
#include "Core/Runtime.h"
#include "DiscQueue.h"
#include "Ending/Credits/StaffRoll.h"
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
        DsGetSector(Content_discDriveState.destination + Content_discDriveState.destinationSector * 2048, 512);
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

// decomp: SLUS_010.40 _diskInitRead (src/SLUS_010.40/main.c:9140)
int Content_BeginDiscRead(int sector, u_int bytes, void* vram)
{
    if (Content_discDriveState.state == Content_DiscDriveState_Idle) {
        Content_discDriveState.startSector = sector;
        Content_discDriveState.byteCount = bytes;
        Content_discDriveState.destination = vram;
        Content_discDriveState.ringReadIndex = 0;
        Content_discDriveState.ringWriteIndex = 0;
        Content_discDriveState.destinationSector = 0;
        Content_discDriveState.bufferedSectors = 0;
        Content_discDriveState.sectorsReceived = 0;
        Content_discDriveState.failureCount = 0;
        Content_discDriveState.ringSectorsWritten = 0;
        Content_discDriveState.sectorCount = bytes / 2048;
        if (bytes % 2048) {
            ++Content_discDriveState.sectorCount;
        }
        DsIntToPos(sector, &Content_discDriveState.position);
        Content_discDriveState.state = Content_DiscDriveState_ReadInit;
        return 1;
    }
    return 0;
}

// decomp: SLUS_010.40 func_800443CC (src/SLUS_010.40/main.c:9164)
void Content_UpdateDiscCommands(void)
{
    DslATV vol;
    int i;
    u_int seconds;

    switch (Content_discDriveState.state) {
    case Content_DiscDriveState_Idle:
    default:
        break;
    case Content_DiscDriveState_ReadXaInit:
        if (DsSystemStatus() == DslReady) {
            Content_discDriveState.xaModeMarker = 0;
            Content_discDriveState.position.minute = 0;
            while (DsControlB(DslSetmode, Content_discControlResponseBytes, NULL) == 0)
                ;
            VSync(3);
            vol.val2 = 0x80;
            vol.val0 = 0x80;
            vol.val3 = 0;
            vol.val1 = 0;
            Audio_Control_SetDiscVolume(Content_discDriveState.byteCount);
            Audio_Control_SetDiscMixVolume(0x7F);
            Content_discDriveState.xaFilter.file = 1;
            Content_discDriveState.xaFilter.chan = 0;
            DsControl(DslSetfilter, (char*)&Content_discDriveState.xaFilter, NULL);
            DsIntToPos(Content_discDriveState.startSector, &Content_discDriveState.position);
            Content_discDriveState.commandId =
                DsPacket(DslModeRT | DslModeSF, &Content_discDriveState.position, DslReadS, NULL, -1);
            Content_discDriveState.state = Content_DiscDriveState_StreamXa;
        }
        if (Content_discDriveState.commandId == 0) {
            Content_discDriveState.state = Content_DiscDriveState_CommandFailed;
            ++Content_discDriveState.failureCount;
        }
        break;
    case Content_DiscDriveState_ReadInit: {
        int status = DsSystemStatus();
        if (status == DslReady) {
            Content_discDriveState.sectorsReceived = 0;
            Content_discDriveState.destinationSector = 0;
            if (Content_discDriveState.byteCount != 0) {
                Content_discDriveState.state = Content_DiscDriveState_ReadReady;
                Content_discDriveState.commandId = DsPacket(DslModeSpeed | DslModeSize1,
                    &Content_discDriveState.position, DslReadN, Content_ReadCommandCallback, -1);
            } else {
                Content_discDriveState.state = Content_DiscDriveState_SeekReady;
                Content_discDriveState.commandId = DsPacket(DslModeSpeed | DslModeSize1,
                    &Content_discDriveState.position, DslSeekL, Content_SeekCommandCallback, -1);
            }

            if (Content_discDriveState.commandId == 0) {
                Content_discDriveState.state = Content_DiscDriveState_ReadError;
                ++Content_discDriveState.failureCount;
            }
        }
        break;
    }
    case Content_DiscDriveState_CommandFailed: {
        int status = DsSystemStatus();
        if (status == DslReady) {
            Content_discDriveState.sectorsReceived = 0;
            ++Content_discDriveState.failureCount;
            seconds = Ending_Credits_xaResumeFrameCount / 60;
            if (seconds < 420) {
                DsIntToPos(
                    Content_discDriveState.startSector + (seconds * 75) + (Ending_Credits_xaResumeFrameCount % 60),
                    &Content_legacyDiscReadLocation);
                vol.val2 = 0x80;
                vol.val0 = 0x80;
                vol.val3 = 0;
                vol.val1 = 0;
                Audio_Control_SetDiscVolume(0);
                Audio_Control_FadeDiscVolume(0x3C, Content_discDriveState.byteCount);
                Content_discDriveState.xaFilter.file = 1;
                Content_discDriveState.xaFilter.chan = 0;
                DsControl(DslSetfilter, (char*)&Content_discDriveState.xaFilter, NULL);
                Content_discDriveState.commandId =
                    DsPacket(DslModeRT | DslModeSF, &Content_legacyDiscReadLocation, DslReadS, NULL, -1);
                Content_discDriveState.state = Content_DiscDriveState_StreamXa;
            } else {
                Content_discDriveState.commandId = DsCommand(DslModeSF | DslModeDA, NULL, NULL, -1);
                Content_discDriveState.state = Content_DiscDriveState_Idle;
            }
        }
        if (Content_discDriveState.commandId == 0) {
            Content_discDriveState.state = Content_DiscDriveState_CommandFailed;
            ++Content_discDriveState.failureCount;
        }
        break;
    }
    case Content_DiscDriveState_ReadError: {
        int status = DsSystemStatus();
        if (status == DslReady) {
            Content_discDriveState.sectorsReceived = 0;
            Content_discDriveState.destinationSector = 0;
            ++Content_discDriveState.failureCount;

            if (Content_discDriveState.byteCount != 0) {
                Content_discDriveState.state = Content_DiscDriveState_ReadReady;
                Content_discDriveState.commandId = DsPacket(DslModeSpeed | DslModeSize1,
                    &Content_discDriveState.position, DslReadN, Content_ReadCommandCallback, -1);
            } else {
                Content_discDriveState.state = Content_DiscDriveState_SeekReady;
                Content_discDriveState.commandId = DsPacket(DslModeSpeed | DslModeSize1,
                    &Content_discDriveState.position, DslSeekL, Content_SeekCommandCallback, -1);
            }

            if (Content_discDriveState.commandId == 0) {
                Content_discDriveState.state = Content_DiscDriveState_ReadError;
                ++Content_discDriveState.failureCount;
            }
        }
        break;
    }
    case Content_DiscDriveState_StreamXaEnd:
        Audio_Control_SetDiscVolume(0);
        Content_discDriveState.commandId = DsCommand(DslPause, NULL, NULL, -1);
        if (Content_discDriveState.commandId > 0) {
            Content_discDriveState.state = Content_DiscDriveState_Idle;
        }
        Content_discDriveState.xaModeMarker = 128;
        Content_discDriveState.position.minute = 128;
        while (DsControlB(DslSetmode, Content_discControlResponseBytes, NULL) == 0)
            ;
        VSync(3);
        break;
    }

    if (Content_discDriveState.bufferMode != Content_DiscBufferMode_Direct) {
        for (i = 0; i < 16; ++i) {
            if (Content_discDriveState.destinationSector >= Content_discDriveState.ringSectorsWritten) {
                return;
            }
            if (Content_discDriveState.bufferMode == Content_DiscBufferMode_Buffered) {
                Core_Memory_CopyAlignedWords(
                    ((char*)Content_discDriveState.destination + (Content_discDriveState.destinationSector * 2048)),
                    Content_legacyDiscReadBuffer + (Content_discDriveState.ringReadIndex * 128), 2048);

                ++Content_discDriveState.ringReadIndex;
                if (Content_discDriveState.ringReadIndex >= 16) {
                    Content_discDriveState.ringReadIndex = 0;
                }
                --Content_discDriveState.bufferedSectors;
                ++Content_discDriveState.destinationSector;
                if (Content_discDriveState.destinationSector >= Content_discDriveState.sectorCount) {
                    Content_discDriveState.state = Content_DiscDriveState_Idle;
                    Core_Memory_FreeFromHighEnd(Content_legacyDiscReadBuffer);
                    Content_legacyDiscReadBuffer = NULL;
                }
            } else if (Content_discDriveState.bufferMode == Content_DiscBufferMode_Manual) {
                ++Content_discDriveState.destinationSector;
                if ((Content_discDriveState.destinationSector >= Content_discDriveState.sectorCount)
                    && (Audio_Samples_TransferStatus() == 0)) {
                    Content_discDriveState.state = Content_DiscDriveState_Idle;
                    Core_Memory_FreeFromHighEnd(Content_legacyDiscReadBuffer);
                    Content_legacyDiscReadBuffer = NULL;
                }
            }
        }
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

// decomp: SLUS_010.40 _diskReset (src/SLUS_010.40/main.c:9385)
void Content_ResetDiscDrive(void)
{
    while (DsControlB(DslPause, NULL, NULL) == 0)
        ;
    Content_discDriveState.xaModeMarker = 128;
    Content_discDriveState.position.minute = 128;
    Content_discDriveState.reserved2 = 0;
    Content_discDriveState.state = Content_DiscDriveState_Idle;
    Content_discDriveState.bufferMode = Content_DiscBufferMode_Direct;
    Content_legacyDiscReadBuffer = NULL;
    while (DsControlB(DslSetmode, Content_discControlResponseBytes, NULL) == 0)
        ;
    VSync(3);
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

