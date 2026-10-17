// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Audio/Seq/Channel.h"
#include "Audio/SoundBanks.h"
#include "Core/Memory/Copy.h"
#include "Field/Doors.h"
#include "Field/Mpd.h"
#include "Input/Controller.h"
#include "Items/Inventory.h"
#include "Items/Weapons.h"
#include "Save/MemoryCard/GameFiles.h"
#include "Save/Snapshot.h"
#include "State/Ashley.h"
#include "State/GameTime.h"
#include "State/Location.h"
#include "State/PlayTime.h"
#include "State/Progress.h"
#include "State/RoomCache.h"
#include "State/RoomMemory.h"
#include "State/Settings.h"
#include "State/StateFlags.h"
#include "Title/SaveCard.h"
#include "Title/SaveFiles.h"

// decomp: _0 (_0@src/TITLE/TITLE.PRG/22C.c)
char Title_saveDirectoryPadding[4];

// decomp: _1 (_1@src/TITLE/TITLE.PRG/22C.c)
char Title_saveTransferPadding;

// decomp: _dirEntBuf (_dirEntBuf@src/TITLE/TITLE.PRG/22C.c)
struct DIRENTRY* Title_cardDirectoryEntryBuffer;

// decomp: _loadSaveDataErrorOffset (_loadSaveDataErrorOffset@src/TITLE/TITLE.PRG/22C.c)
char Title_saveTransferErrorOffset;

// decomp: _memcardEventDescriptors (_memcardEventDescriptors@src/TITLE/TITLE.PRG/22C.c)
long Title_cardEventDescriptors[8];

// decomp: _memcardFiles (_memcardFiles@src/TITLE/TITLE.PRG/22C.c)
struct DIRENTRY* Title_cardDirectoryEntries[15];

// decomp: TITLE.PRG _testMemcardEvents (src/TITLE/TITLE.PRG/22C.c:61)
enum Save_CardEventSignal Title_SaveCard_PollEvents(enum Save_CardEventGroup type)
{
    int i;

    for (i = 0; i < 4; ++i) {
        if (TestEvent(Title_cardEventDescriptors[i + type]) == 1) {
            return i;
        }
    }
    return i;
}

// decomp: TITLE.PRG _resetMemcardEvents (src/TITLE/TITLE.PRG/22C.c:73)
void Title_SaveCard_ClearEvents(enum Save_CardEventGroup type)
{
    int i;

    for (i = 0; i < 4; ++i) {
    }
}

// decomp: TITLE.PRG _getNewestSaveFile (src/TITLE/TITLE.PRG/22C.c:192)
u_int Title_SaveCard_NewestSaveSlot(void)
{
    u_int i;
    u_int maxCounter = 0;
    u_int fileIndex = 0;

    for (i = 0; i < 5; ++i) {
        if (Title_saveFileSummaries[i].summary.base.slotState >= 3) {
            if (maxCounter < Title_saveFileSummaries[i].summary.base.generation) {
                maxCounter = Title_saveFileSummaries[i].summary.base.generation;
                fileIndex = i;
            }
        }
    }
    return fileIndex;
}

// decomp: TITLE.PRG _findCurrentSaveOnActiveMemcard (src/TITLE/TITLE.PRG/22C.c:209)
int Title_SaveCard_FindMatchingSaveSlot(void)
{
    int i;
    for (i = 0; i < 5; ++i) {
        if ((Title_saveFileSummaries[i].summary.base.slotState >= Save_CardFileSlotStatus_InUse)
            && (Title_saveFileSummaries[i].summary.base.slotState == Input_settings.slotState)
            && (Title_saveFileSummaries[i].key == Input_settings.key)
            && (Title_saveFileSummaries[i].summary.stats.cumulativeSaveCount
                == Input_settings.cumulativeSaveCount)
            && (Title_saveFileSummaries[i].summary.base.generation
                == Input_settings.saveFileGeneration)) {
            return i + 1;
        }
    }
    return 0;
}

// decomp: TITLE.PRG _readSaveFileInfo (src/TITLE/TITLE.PRG/22C.c:322)
int Title_SaveCard_ReadSaveSummary(int id)
{
    Save_CardFileInfoRecord saveInfo[4];
    int i;
    int port = id >> 12;

    id &= 7;

    for (i = 0; i < 3; ++i) {
        int bytesRead;
        int file = LegacyApi_open(Save_MemoryCard_BuildSaveFilename(port, id), O_RDONLY);

        if (file == -1) {
            continue;
        }
        bytesRead = LegacyApi_read(file, saveInfo, sizeof saveInfo);
        LegacyApi_close(file);
        if (bytesRead != sizeof saveInfo) {
            continue;
        }
        Save_MemoryCard_DecodeSaveBytes(saveInfo[3].key, &saveInfo[3].summary.base.slotState,
            sizeof saveInfo[3] - sizeof(int));
        if (Save_MemoryCard_HasChecksumError((Save_GameSaveImage*)saveInfo, 2) == 0) {
            Core_Memory_CopyBytesBackward(&Title_saveFileSummaries[id - 1], &saveInfo[3], sizeof saveInfo[3]);
            return 0;
        }
    }

    memset(&Title_saveFileSummaries[id - 1], 0, sizeof Title_saveFileSummaries[id - 1]);
    return 1;
}

// decomp: TITLE.PRG _memcardSaveIdExists (src/TITLE/TITLE.PRG/22C.c:354)
int Title_SaveCard_HasSaveSuffix(int id)
{
    int i;

    for (i = 0; i < 15; ++i) {
        char* filename = Title_cardDirectoryEntries[i]->name;
        if ((filename != NULL) && (Save_MemoryCard_ParseSaveFileNumber(filename) != 0)
            && (filename[15] == id)) {
            return 1;
        }
    }
    return 0;
}

// decomp: TITLE.PRG _initSaveFileInfo (src/TITLE/TITLE.PRG/22C.c:385)
int Title_SaveCard_ScanSaveDirectory(int port)
{
    intptr_t fileNo;
    int i;
    int tempFilesDeleted;
    int slotsAvailable = 15;

    for (i = 14; i >= 0; --i) {
        Title_cardDirectoryEntries[i] = NULL;
    }

    fileNo = (intptr_t)LegacyApi_firstfile((port == 1) ? ("bu00:*") : ("bu10:*"), Title_cardDirectoryEntryBuffer);
    Title_cardDirectoryEntries[0] = (void*)fileNo;

    for (i = 1; (i < 15) && fileNo; ++i) {
        fileNo = (intptr_t)LegacyApi_nextfile(Title_cardDirectoryEntryBuffer + i);
        Title_cardDirectoryEntries[i] = (void*)fileNo;
    }

    memset(Title_saveFileSummaries, 0, sizeof *Title_saveFileSummaries * 5);
    tempFilesDeleted = Save_MemoryCard_DeleteRedundantTemporaryFiles((port - 1) * 16);
    if (tempFilesDeleted & 0x80) {
        return 1;
    }
    for (i = 0; i < 15; ++i) {
        struct DIRENTRY* file = Title_cardDirectoryEntries[i];

        if (file == NULL) {
            continue;
        }
        fileNo = Save_MemoryCard_ParseSaveFileNumber(file->name);
        if (fileNo != 0) {
            if (fileNo < 0) {
                fileNo = -fileNo;
                if ((tempFilesDeleted >> (fileNo - 1)) & 1) {
                    continue;
                }
                memset(&Title_saveFileSummaries[fileNo - 1], 0, sizeof Title_saveFileSummaries[fileNo - 1]);
                Title_saveFileSummaries[fileNo - 1].summary.base.slotState = Save_CardFileSlotStatus_Temporary;
            } else if (Title_SaveCard_ReadSaveSummary(((port - 1) << 16) | fileNo) != 0) {
                slotsAvailable += (file->size + 0x1FFF) >> 13;
            }
        }
        slotsAvailable -= (file->size + 0x1FFF) >> 13;
    }

    for (; slotsAvailable >= 3; slotsAvailable -= 3) {
        for (i = 0; i < 5; ++i) {
            if (Title_saveFileSummaries[i].summary.base.slotState == Save_CardFileSlotStatus_Unavailable) {
                Title_saveFileSummaries[i].summary.base.slotState = Save_CardFileSlotStatus_Available;
                break;
            }
        }
    }

    return 0;
}

// decomp: TITLE.PRG _memcardEventHandler (src/TITLE/TITLE.PRG/22C.c:457)
enum Save_CardReadinessResult Title_SaveCard_AdvanceCardReadiness(int initPort)
{
    enum Save_CardReadinessState {
        Save_CardReadinessState_Init = 0,
        Save_CardReadinessState_Ready = 1,
        Save_CardReadinessState_Unformatted = 2,
        Save_CardReadinessState_Confirmed = 3,
        Save_CardReadinessState_LoadReady = 4,
        Save_CardReadinessState_Loaded = 5,
    };

    static char Title_cardPollPhase;
    static char Title_cardPollPort;
    static char Title_cardInitializationPollCount;
    static char Title_cardEventPollTimeout;
    static char Title_cardPollEventType;

    int event;

    if (initPort != 0) {
        Title_cardPollPort = (initPort - 1) * 16;
        Title_cardPollPhase = Save_CardReadinessState_Init;
        Title_cardInitializationPollCount = 0;
        return Save_CardReadinessResult_Pending;
    }
    switch (Title_cardPollPhase) {
    case Save_CardReadinessState_Init:
        if (++Title_cardInitializationPollCount >= 4) {
            return Save_CardReadinessResult_Timeout;
        }
        Title_SaveCard_ClearEvents(Save_CardEventGroup_Software);
        if (_card_info(Title_cardPollPort) == 0) {
            break;
        }
        Title_cardPollPhase = Save_CardReadinessState_Ready;
        Title_cardEventPollTimeout = 0;
        Title_cardPollEventType = Save_CardEventGroup_Software;
        // fallthrough
    case Save_CardReadinessState_Ready:
        switch (Title_SaveCard_PollEvents(Save_CardEventGroup_Software)) {
        case Save_CardEventSignal_IoEnd:
            Title_cardPollPhase = Save_CardReadinessState_LoadReady;
            break;
        case Save_CardEventSignal_Error:
        case Save_CardEventSignal_Timeout:
            Title_cardPollPhase = Save_CardReadinessState_Init;
            break;
        case Save_CardEventSignal_Unformatted:
            Title_cardPollPhase = Save_CardReadinessState_Unformatted;
            break;
        case Save_CardEventSignal_None:
            if (Title_cardEventPollTimeout++ > 64) {
                Title_cardPollPhase = Save_CardReadinessState_Init;
            }
            break;
        }
        break;
    case Save_CardReadinessState_Unformatted:
        Title_SaveCard_ClearEvents(Save_CardEventGroup_Hardware);
        if (_card_clear(Title_cardPollPort) == 0) {
            break;
        }
        Title_cardPollPhase = Save_CardReadinessState_Confirmed;
        Title_cardEventPollTimeout = 0;
        Title_cardPollEventType = Save_CardEventGroup_Hardware;
        // fallthrough
    case Save_CardReadinessState_Confirmed:
        do {
            event = Title_SaveCard_PollEvents(Save_CardEventGroup_Hardware);
        } while (event == Save_CardEventSignal_None);
        if (event == Save_CardEventSignal_IoEnd) {
            Title_cardPollPhase = Save_CardReadinessState_LoadReady;
            break;
        }
        if (event < Save_CardEventSignal_IoEnd) {
            break;
        }
        if (event >= Save_CardEventSignal_None) {
            break;
        }
        Title_cardPollPhase = Save_CardReadinessState_Init;
        break;
    case Save_CardReadinessState_LoadReady:
        Title_SaveCard_ClearEvents(Save_CardEventGroup_Software);
        if (_card_load(Title_cardPollPort) == 0) {
            break;
        }
        Title_cardPollPhase = Save_CardReadinessState_Loaded;
        Title_cardEventPollTimeout = 0;
        // fallthrough
    case Save_CardReadinessState_Loaded:
        event = Title_SaveCard_PollEvents(Save_CardEventGroup_Software);
        switch (event) {
        case Save_CardEventSignal_IoEnd:
            return Title_cardPollEventType + Save_CardReadinessResult_Ready;
        case Save_CardEventSignal_Error:
        case Save_CardEventSignal_Timeout:
            Title_cardPollPhase = Save_CardReadinessState_Init;
            break;
        case Save_CardEventSignal_Unformatted:
            return Title_cardPollEventType + Save_CardReadinessResult_Unformatted;
        case Save_CardEventSignal_None:
            if (Title_cardEventPollTimeout++ > 64) {
                Title_cardPollPhase = Save_CardReadinessState_Init;
            }
            break;
        }
        break;
    }
    return Save_CardReadinessResult_Pending;
}

// decomp: TITLE.PRG _applyLoadedSaveFile (src/TITLE/TITLE.PRG/22C.c:569)
int Title_SaveCard_ValidateAndApplyLoadedSave(int verifyOnly)
{
    int blockCount;
    Save_GameSaveImage* spmcimg = (Save_GameSaveImage*)Title_memoryCardUiResource;
    void* spmcimg2 = spmcimg + 1;
    Save_GameSaveImage* s4 = spmcimg + 1;
    Save_SaveCipherHeader* unk180 = &spmcimg[1].protectedHeader.cipherHeader;

    Save_MemoryCard_DecodeSaveBytes(unk180->cipherKey, &unk180->slotHeader.slotState,
        sizeof(Save_GameSaveImage) - (u_long) & ((Save_GameSaveImage*)0)->protectedHeader.cipherHeader.slotHeader.slotState);

    blockCount = 92;
    if (verifyOnly != 0) {
        blockCount = 32;
    }

    if ((Save_MemoryCard_HasChecksumError(spmcimg + 1, blockCount) != 0)
        || (unk180->slotHeader.opaque8 != 0x20000107)) {
        do {
            return 1;
        } while (0);
    }

    if (verifyOnly == 0) {
        return 0;
    }

    Core_Memory_CopyBytesBackward(&Audio_Seq_unusedStateFlags, spmcimg[1].stateFlags, sizeof Audio_Seq_unusedStateFlags);
    Core_Memory_CopyBytesBackward(State_learnedActionFlags, spmcimg[1].learnedActions, sizeof State_learnedActionFlags);
    Core_Memory_CopyBytesBackward(&State_roomVisitProgress, spmcimg[1].mapStatus, sizeof State_roomVisitProgress);
    Core_Memory_CopyBytesBackward(&Input_settings, &spmcimg[1].settings, sizeof Input_settings);
    Core_Memory_CopyBytesBackward(&State_ashleySnapshot, &spmcimg[1].ashleySnapshot, sizeof State_ashleySnapshot);
    Core_Memory_CopyBytesBackward(&Audio_Seq_unusedInventory, &spmcimg[1].inventory, sizeof Audio_Seq_unusedInventory);
    Core_Memory_CopyBytesBackward(
        &Audio_Seq_unusedInventoryIndices, &spmcimg[1].inventoryIndices, sizeof Audio_Seq_unusedInventoryIndices);
    Core_Memory_CopyBytesBackward(&State_savedEntryDoor, &spmcimg[1].entryDoor, sizeof State_savedEntryDoor);
    Core_Memory_CopyBytesBackward(&Audio_Seq_unusedScoredata, &spmcimg[1].scoreData, sizeof Audio_Seq_unusedScoredata);
    State_currentRoomStateSlot = s4->roomStateSlot;
    Core_Memory_CopyBytesBackward(State_recentRoomEnemyStates, spmcimg[1].recentRoomEnemyStates, sizeof State_recentRoomEnemyStates);
    spmcimg2 = &Audio_Seq_unusedArtsStatus;
    Core_Memory_CopyBytesBackward(&Audio_Seq_unusedArtsStatus, &spmcimg[1].artsStatus, sizeof Audio_Seq_unusedArtsStatus);
    State_playTime.t = spmcimg[1].protectedHeader.summary.gameTime.t;
    Save_MarkAshleySnapshotValid();
    Audio_SetMonoOutput(Input_settings.monoSound);
    return 0;
}

// decomp: TITLE.PRG _loadSaveData (src/TITLE/TITLE.PRG/22C.c:745)
int Title_SaveCard_ReadSaveStep(int portFileno)
{
    enum Save_SaveReadState { Save_SaveReadState_Init = 0, Save_SaveReadState_Reading = 1 };

    static char Title_saveReadPhase;
    static char Title_saveReadCardSlot;
    static char Title_saveReadFileIndex;
    static char Title_isReadingTemporarySave;
    static char Title_saveReadErrorCount;
    static short Title_saveReadPadding __attribute__((unused));
    static int Title_saveReadFileDescriptor;

    void* temp_s2 = (Save_GameSaveImage*)Title_memoryCardUiResource + 1;

    if (portFileno != 0) {
        Title_saveReadPhase = Save_SaveReadState_Init;
        Title_saveReadErrorCount = 0;
        Title_saveTransferErrorOffset = 0;
        Title_saveReadCardSlot = portFileno >> 0xC;
        Title_isReadingTemporarySave = (portFileno >> 8) & 1;
        Title_saveReadFileIndex = portFileno & 0xF;
        Title_transferProgressPosition = 80;
        Title_previousTransferProgress = 0;
        return 0;
    }

    switch (Title_saveReadPhase) {
    case Save_SaveReadState_Init: {
        int nBytes;
        char* filename;
        int new_var = 320;

        Title_transferProgressPosition +=
            ((Title_transferProgressCounter - Title_previousTransferProgress)
                * ((Title_saveTransferErrorOffset * 20) - (Title_transferProgressPosition - new_var)))
            / Title_transferProgressTarget;
        Title_saveTransferErrorOffset = Title_saveReadErrorCount;
        Title_transferProgressTarget = 192 - (Title_isReadingTemporarySave << 7);
        Title_previousTransferProgress = Title_transferProgressCounter;

        memset(temp_s2, 0, sizeof(Save_GameSaveImage));

        if (Title_saveReadFileIndex & 8) {
            filename = Save_MemoryCard_BuildTemporaryFilename(Title_saveReadCardSlot, Title_saveReadFileIndex & 7);
        } else {
            filename = Save_MemoryCard_BuildSaveFilename(Title_saveReadCardSlot, Title_saveReadFileIndex);
        }
        Title_saveReadFileDescriptor = LegacyApi_open(filename, O_NOWAIT | O_RDONLY);
        if (Title_saveReadFileDescriptor == -1) {
            ++Title_saveReadErrorCount;
            break;
        }
        Title_SaveCard_ClearEvents(Save_CardEventGroup_Software);
        nBytes = sizeof(Save_GameSaveImage);
        if (Title_isReadingTemporarySave != 0) {
            nBytes = 0x2000;
        }
        if (LegacyApi_read(Title_saveReadFileDescriptor, temp_s2, nBytes) == -1) {
            LegacyApi_close(Title_saveReadFileDescriptor);
            ++Title_saveReadErrorCount;
            break;
        }
        Title_saveReadPhase = Save_SaveReadState_Reading;
    }
        // fallthrough
    case Save_SaveReadState_Reading: {
        int event = Title_SaveCard_PollEvents(Save_CardEventGroup_Software);
        if (event < Save_CardEventSignal_None) {
            LegacyApi_close(Title_saveReadFileDescriptor);
            if (event == Save_CardEventSignal_IoEnd) {
                return 1;
            }
            Title_saveReadPhase = Save_SaveReadState_Init;
            ++Title_saveReadErrorCount;
        }
        break;
    }
    }
    return Title_saveReadErrorCount == 3 ? -1 : 0;
}

// decomp: TITLE.PRG _saveFile (src/TITLE/TITLE.PRG/22C.c:826)
int Title_SaveCard_WriteSaveStep(int portFile)
{
    enum Save_SaveWriteState {
        Save_SaveWriteState_Init = 0,
        Save_SaveWriteState_TemporaryFileCreated = 1,
        Save_SaveWriteState_ReadReady = 2,
        Save_SaveWriteState_VerifyPending = 3,
        Save_SaveWriteState_FileVerified = 4
    };

    static char Title_saveWritePhase;
    static char Title_saveWriteFileIndex;
    static char Title_saveWriteCardSlot;
    static char Title_saveWriteErrorCount;
    static char Title_saveRenameErrorCount;
    static char Title_saveWritePadding[3] __attribute__((unused));
    static int Title_saveWriteFileDescriptor;

    int temp_v1_2;
    int i;
    int temp_s3;

    if (portFile != 0) {
        Title_saveWriteErrorCount = 0;
        Title_saveRenameErrorCount = 0;
        Title_saveTransferErrorOffset = 0;
        Title_saveWriteCardSlot = portFile >> 0xC;
        Title_saveWriteFileIndex = portFile & 7;
        Title_transferProgressPosition = 80;
        Title_previousTransferProgress = 0;
        Title_saveWritePhase = Title_SaveCard_HasSaveSuffix(Title_saveWriteFileIndex + 'A' - 1);
        return 0;
    }
    switch (Title_saveWritePhase) {
    case Save_SaveWriteState_Init:
        if (LegacyApi_rename(Save_MemoryCard_BuildSaveFilename(Title_saveWriteCardSlot, Title_saveWriteFileIndex), Save_MemoryCard_BuildTemporaryFilename(Title_saveWriteCardSlot, Title_saveWriteFileIndex))
            != 0) {
            Title_saveWriteErrorCount = 0;
            Title_saveRenameErrorCount = 0;
            Title_saveWritePhase = Save_SaveWriteState_TemporaryFileCreated;
        } else {
            ++Title_saveRenameErrorCount;
            Title_saveWriteErrorCount = Title_saveRenameErrorCount >> 4;
        }
        break;
    case Save_SaveWriteState_TemporaryFileCreated:
        temp_v1_2 = ((Title_transferProgressCounter - Title_previousTransferProgress)
                        * (320 - Title_transferProgressPosition))
                  / Title_transferProgressTarget;
        Title_previousTransferProgress = Title_transferProgressCounter;
        Title_transferProgressTarget = 384;
        Title_transferProgressPosition += temp_v1_2;
        Title_saveWriteFileDescriptor = LegacyApi_open(Save_MemoryCard_BuildTemporaryFilename(Title_saveWriteCardSlot, Title_saveWriteFileIndex), O_NOWAIT | O_WRONLY);
        ;
        if (Title_saveWriteFileDescriptor == -1) {
            ++Title_saveWriteErrorCount;
            break;
        }
        Title_SaveCard_ClearEvents(Save_CardEventGroup_Software);
        if (LegacyApi_write(Title_saveWriteFileDescriptor, Title_memoryCardUiResource, sizeof(Save_GameSaveImage)) == -1) {
            LegacyApi_close(Title_saveWriteFileDescriptor);
            ++Title_saveWriteErrorCount;
            break;
        }
        Title_saveWritePhase = Save_SaveWriteState_ReadReady;
        // fallthrough
    case Save_SaveWriteState_ReadReady: {
        temp_s3 = Title_SaveCard_PollEvents(Save_CardEventGroup_Software);
        if (temp_s3 < Save_CardEventSignal_None) {
            LegacyApi_close(Title_saveWriteFileDescriptor);
            if (temp_s3 == Save_CardEventSignal_IoEnd) {
                Title_saveWritePhase = Save_SaveWriteState_VerifyPending;
                i = Title_transferProgressPosition;
                temp_s3 = Title_previousTransferProgress;
                Title_SaveCard_ReadSaveStep((Title_saveWriteCardSlot << 12) | (Title_saveWriteFileIndex + 8));
                Title_transferProgressPosition = i;
                Title_previousTransferProgress = temp_s3;
            } else {
                ++Title_saveWriteErrorCount;
                Title_saveWritePhase = Save_SaveWriteState_TemporaryFileCreated;
            }
        }
        break;
    }
    case Save_SaveWriteState_VerifyPending:
        temp_s3 = Title_SaveCard_ReadSaveStep(0);
        if (temp_s3 == 0) {
            break;
        }
        if (temp_s3 < 0) {
            return -1;
        }
        for (i = 0; i < (int)sizeof(Save_GameSaveImage); ++i) {
            if (Title_memoryCardUiResource[i] != Title_memoryCardUiResource[sizeof(Save_GameSaveImage) + i]) {
                break;
            }
        }
        if (i < (int)sizeof(Save_GameSaveImage)) {
            return -1;
        }
        Title_saveWritePhase = Save_SaveWriteState_FileVerified;
        break;
    case Save_SaveWriteState_FileVerified:
        if (LegacyApi_rename(Save_MemoryCard_BuildTemporaryFilename(Title_saveWriteCardSlot, Title_saveWriteFileIndex), Save_MemoryCard_BuildSaveFilename(Title_saveWriteCardSlot, Title_saveWriteFileIndex))
            == 0) {
            ++Title_saveRenameErrorCount;
            Title_saveWriteErrorCount = (Title_saveRenameErrorCount >> 4);
            break;
        }
        return 1;
    }
    return Title_saveWriteErrorCount == 3 ? -1 : 0;
}

