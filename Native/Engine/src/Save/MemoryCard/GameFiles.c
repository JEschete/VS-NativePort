// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Audio/Seq/Channel.h"
#include "Audio/SoundBanks.h"
#include "Boot/Lifecycle.h"
#include "CdDrive.h"
#include "Content/DiscLayout.h"
#include "Core/Memory/Copy.h"
#include "Core/Memory/Heap.h"
#include "Core/Random.h"
#include "DiscQueue.h"
#include "Field/Doors.h"
#include "Field/Map.h"
#include "Field/Mpd.h"
#include "Input/Controller.h"
#include "Items/Inventory.h"
#include "Items/Weapons.h"
#include "Menu/Files.h"
#include "Menu/Framework/Framework.h"
#include "Render/Font.h"
#include "Render/TextureAnimation.h"
#include "Render/Textures.h"
#include "Save/MemoryCard/GameFiles.h"
#include "Save/MemoryCard/Resources.h"
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

// decomp: local static of _keystreamBits, at file scope so the module reset reaches it
static u_int Save_MemoryCard_keystreamSeed = 0x19660D;

// decomp: _memcardFilenameTemplate (_memcardFilenameTemplate@src/TITLE/TITLE.PRG/22C.c)
char const* Save_MemoryCard_kOriginalFilenameTemplate = "bu00:BASLUS-01040VAG0";

// decomp: MENU7.PRG _testMemcardEvents (src/MENU/MENU7.PRG/260.c:172)
enum Save_CardEventSignal Save_MemoryCard_PollEvents(enum Save_CardEventGroup type)
{
    int i;

    for (i = 0; i < 4; ++i) {
        if (TestEvent(Title_cardEventDescriptors[i + type]) == 1) {
            return i;
        }
    }
    return i;
}

// decomp: MENU7.PRG _resetMemcardEvents (src/MENU/MENU7.PRG/260.c:187)
void Save_MemoryCard_ClearEvents(int type)
{
    int i;

    for (i = 0; i < 4; ++i) {
    }
}

// decomp: MENU7.PRG _memcardMakeFilename (src/MENU/MENU7.PRG/260.c:238)
char* Save_MemoryCard_BuildSaveFilename(int port, int fileNo)
{
    static char Save_MemoryCard_legacySaveFilename[32];

    memset(Save_MemoryCard_legacySaveFilename, 0, ' ');
    strcpy(Save_MemoryCard_legacySaveFilename, Save_MemoryCard_kOriginalFilenameTemplate);
    Save_MemoryCard_legacySaveFilename[2] = port == 0 ? '0' : '1';
    Save_MemoryCard_legacySaveFilename[20] = fileNo + '0';
    return Save_MemoryCard_legacySaveFilename;
}

// decomp: MENU7.PRG _memcardMakeTempFilename (src/MENU/MENU7.PRG/260.c:254)
char* Save_MemoryCard_BuildTemporaryFilename(int port, int fileNo)
{
    static char Save_MemoryCard_legacyStagingFilename[32];

    memset(Save_MemoryCard_legacyStagingFilename, 0, ' ');
    strcpy(Save_MemoryCard_legacyStagingFilename, Save_MemoryCard_kOriginalFilenameTemplate);
    Save_MemoryCard_legacyStagingFilename[2] = port == 0 ? '0' : '1';
    Save_MemoryCard_legacyStagingFilename[20] = fileNo + 'A' - 1;
    return Save_MemoryCard_legacyStagingFilename;
}

// decomp: MENU7.PRG _getNewestSaveFile (src/MENU/MENU7.PRG/260.c:278)
u_int Save_MemoryCard_NewestSaveSlot(void)
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

// decomp: MENU7.PRG _findCurrentSaveOnActiveMemcard (src/MENU/MENU7.PRG/260.c:300)
int Save_MemoryCard_FindMatchingSaveSlot(void)
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

// decomp: MENU7.PRG _memcardFileNumberFromFilename (src/MENU/MENU7.PRG/260.c:323)
int Save_MemoryCard_ParseSaveFileNumber(char* filename)
{
    int i;
    char const* gameCode = &Save_MemoryCard_kOriginalFilenameTemplate[5];

    for (i = 0; i < 15; ++i) {
        if (gameCode[i] != filename[i]) {
            return 0;
        }
    }

    i = filename[15] - 'A';

    if (i < 5u) {
        return -(filename[15] - 'A' + 1);
    }

    i = filename[15] - '1';

    if (i < 5u) {
        return filename[15] - '0';
    }

    return 0;
}

// decomp: MENU7.PRG _verifySaveChecksums (src/MENU/MENU7.PRG/260.c:355)
int Save_MemoryCard_HasChecksumError(Save_GameSaveImage data[], int sectorCount)
{
    int checksum;
    int i;
    int j;
    char* p = (char*)data;

    for (i = 0; i < sectorCount; ++i) {
        checksum = 0;
        if (i != 1) {
            for (j = 0; j < 256; ++j) {
                checksum ^= p[j];
            }

            if (data->protectedHeader.blockChecksums[i] != checksum) {
                return 1;
            }
        }
        p += 256;
    }

    p = data->reserved100;
    checksum = 0;
    for (j = 0; j < 256; ++j) {
        checksum ^= p[j];
    }

    return checksum != 0;
}

// decomp: MENU7.PRG _decode (src/MENU/MENU7.PRG/260.c:388)
void Save_MemoryCard_DecodeSaveBytes(u_int key, void* buf, int count)
{
    for (; count != 0; --count) {
        key *= 0x19660D;
        *((char*)buf++) -= key >> 24;
    }
}

// decomp: MENU7.PRG _readSaveFileInfo (src/MENU/MENU7.PRG/260.c:404)
int Save_MemoryCard_ReadSaveSummary(int id)
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
            sizeof(Save_CardFileInfoRecord) - sizeof(int));
        if (Save_MemoryCard_HasChecksumError((Save_GameSaveImage*)saveInfo, 2) == 0) {
            Core_Memory_CopyBytesBackward(&Title_saveFileSummaries[id - 1], &saveInfo[3], sizeof saveInfo[3]);
            return 0;
        }
    }

    memset(&Title_saveFileSummaries[id - 1], 0, sizeof Title_saveFileSummaries[id - 1]);
    return 1;
}

// decomp: MENU7.PRG _memcardSaveIdExists (src/MENU/MENU7.PRG/260.c:439)
int Save_MemoryCard_HasSaveSuffix(int id)
{
    int i;

    for (i = 0; i < 15; ++i) {
        char* filename = Title_cardDirectoryEntries[i]->name;
        if ((filename != 0) && (Save_MemoryCard_ParseSaveFileNumber(filename) != 0)
            && (filename[15] == id)) {
            return 1;
        }
    }
    return 0;
}

// decomp: MENU7.PRG _deleteRedundantTempFiles (src/MENU/MENU7.PRG/260.c:458)
int Save_MemoryCard_DeleteRedundantTemporaryFiles(int port)
{
    int i;
    int ret = 0;

    for (i = 0; i < 5; ++i) {
        if ((Save_MemoryCard_HasSaveSuffix(i + '1') != 0)
            && (Save_MemoryCard_HasSaveSuffix(i + 'A') != 0)) {
            if (LegacyApi_erase(Save_MemoryCard_BuildTemporaryFilename(port, i + 1)) == 0) {
                return 0x80;
            }
            ret |= 1 << i;
        }
    }
    return ret;
}

// decomp: MENU7.PRG _initSaveFileInfo (src/MENU/MENU7.PRG/260.c:480)
int Save_MemoryCard_ScanSaveDirectory(int port)
{
    intptr_t fileNo;
    int i;
    int tempFilesDeleted;
    int slotsAvailable = 15;

    for (i = 14; i >= 0; --i) {
        Title_cardDirectoryEntries[i] = 0;
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

        if (file == 0) {
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
            } else if (Save_MemoryCard_ReadSaveSummary(((port - 1) << 16) | fileNo) != 0) {
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

// decomp: MENU7.PRG _createSaveFile (src/MENU/MENU7.PRG/260.c:543)
int Save_MemoryCard_CreateSaveFile(int port, int id)
{
    long file;
    char* fileName = Save_MemoryCard_BuildSaveFilename((port - 1) * 16, id);

    LegacyApi_erase(fileName);
    file = LegacyApi_open(fileName, O_CREAT | (3 << 16));
    if (file != -1) {
        LegacyApi_close(file);
        return 0;
    }
    return -1;
}

// decomp: MENU7.PRG _memcardEventHandler (src/MENU/MENU7.PRG/260.c:563)
enum Save_CardReadinessResult Save_MemoryCard_AdvanceCardReadiness(int initPort)
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
        Save_MemoryCard_ClearEvents(Save_CardEventGroup_Software);
        if (_card_info(Title_cardPollPort) == 0) {
            break;
        }
        Title_cardPollPhase = Save_CardReadinessState_Ready;
        Title_cardEventPollTimeout = 0;
        Title_cardPollEventType = Save_CardEventGroup_Software;
        // fallthrough
    case Save_CardReadinessState_Ready:
        switch (Save_MemoryCard_PollEvents(Save_CardEventGroup_Software)) {
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
        Save_MemoryCard_ClearEvents(Save_CardEventGroup_Hardware);
        if (_card_clear(Title_cardPollPort) == 0) {
            break;
        }
        Title_cardPollPhase = Save_CardReadinessState_Confirmed;
        Title_cardEventPollTimeout = 0;
        Title_cardPollEventType = Save_CardEventGroup_Hardware;
        // fallthrough
    case Save_CardReadinessState_Confirmed:
        do {
            event = Save_MemoryCard_PollEvents(Save_CardEventGroup_Hardware);
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
        Save_MemoryCard_ClearEvents(Save_CardEventGroup_Software);
        if (_card_load(Title_cardPollPort) == 0) {
            break;
        }
        Title_cardPollPhase = Save_CardReadinessState_Loaded;
        Title_cardEventPollTimeout = 0;
        // fallthrough
    case Save_CardReadinessState_Loaded:
        event = Save_MemoryCard_PollEvents(Save_CardEventGroup_Software);
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

// decomp: MENU7.PRG _applyLoadedSaveFile (src/MENU/MENU7.PRG/260.c:681)
int Save_MemoryCard_ValidateAndApplyLoadedSave(int write)
{
    int blockCount;
    Save_GameSaveImage* spmcimg = (Save_GameSaveImage*)Title_memoryCardUiResource;
    void* spmcimg2 = spmcimg + 1;
    Save_GameSaveImage* s4 = spmcimg + 1;
    Save_SaveCipherHeader* unk180 = &spmcimg[1].protectedHeader.cipherHeader;

    Save_MemoryCard_DecodeSaveBytes(unk180->cipherKey, &unk180->slotHeader.slotState,
        sizeof(Save_GameSaveImage) - (u_long) & ((Save_GameSaveImage*)0)->protectedHeader.cipherHeader.slotHeader.slotState);

    blockCount = 92;
    if (write != 0) {
        blockCount = 32;
    }

    if ((Save_MemoryCard_HasChecksumError(spmcimg + 1, blockCount) != 0)
        || (unk180->slotHeader.opaque8 != 0x20000107)) {
        do {
            return 1;
        } while (0);
    }

    if (write == 0) {
        return 0;
    }

    Core_Memory_CopyBytesBackward(&Audio_Seq_unusedStateFlags, spmcimg[1].stateFlags, sizeof Audio_Seq_unusedStateFlags);
    Core_Memory_CopyBytesBackward(
        State_learnedActionFlags, spmcimg[1].learnedActions, sizeof State_learnedActionFlags);
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

// decomp: MENU7.PRG _packageGameSaveData (src/MENU/MENU7.PRG/260.c:735)
void Save_MemoryCard_PackageGameSave(int targetFile)
{
    static const u_short Save_MemoryCard_saveTitleTemplate[] = { 0x7582, 0x6082, 0x6682, 0x7182, 0x6082, 0x6D82,
        0x7382, 0x4081, 0x7282, 0x7382, 0x6E82, 0x7182, 0x7882, 0x4081, 0x6582, 0x6882,
        0x6B82, 0x6482, 0x4081, 0x5082, 0x4081, 0x4081, 0x4081, 0x4F82, 0x4F82, 0x4681,
        0x4F82, 0x4F82, 0x4681, 0x4F82, 0x4F82, 0x4081 };

    intptr_t i;
    int j;
    int var_a0;
    u_short const* s0 = Save_MemoryCard_saveTitleTemplate;
    Save_GameSaveImage* savedata = (Save_GameSaveImage*)Title_memoryCardUiResource;
    Save_GameSaveImage* savedata2 = savedata + 1;
    Save_GameSaveImage* savedata3 = savedata;
    Save_SaveProtectedHeader* s5 = &savedata->protectedHeader;

    Save_CaptureAshleySnapshot();
    memset(savedata, 0, sizeof *savedata);
    ((char*)&savedata->cardHeader.key)[0] = 0x53;
    ((char*)&savedata->cardHeader.key)[1] = 0x43;
    ((char*)&savedata->cardHeader.key)[2] = 0x11;
    ((char*)&savedata->cardHeader.key)[3] = 3;
    Core_Memory_CopyBytesBackward(&savedata->cardHeader.summary, s0, sizeof savedata->cardHeader.summary);
    savedata->cardHeader.summary.opaque20[3] += (targetFile << 8);

    if (State_playTime.t.hours == 100) {
        savedata->cardHeader.summary.opaque20[6] = 0x5082;
    } else {
        savedata->cardHeader.summary.opaque20[7] += ((State_playTime.t.hours / 10) << 8);
        savedata->cardHeader.summary.opaque20[8] += ((State_playTime.t.hours % 10) << 8);
        savedata->cardHeader.summary.opaque20[10] += ((State_playTime.t.minutes / 10) << 8);
        savedata->cardHeader.summary.opaque20[11] += ((State_playTime.t.minutes % 10) << 8);
        savedata->cardHeader.summary.opaque20[13] += ((State_playTime.t.seconds / 10) << 8);
        savedata->cardHeader.summary.opaque20[14] += ((State_playTime.t.seconds % 10) << 8);
    }
    savedata3->cardHeader.summary.opaque20[15] = 0;
    Core_Memory_CopyBytesBackward(savedata3->cardHeader.iconHeaderBytes, Save_MemoryCard_uiResources->fileIconPalettes[targetFile],
        sizeof savedata->cardHeader.iconHeaderBytes);
    Core_Memory_CopyBytesBackward(savedata3->iconBitmap, Save_MemoryCard_uiResources->fileIconBitmaps[targetFile], sizeof savedata->iconBitmap);

    if (Input_settings.slotState == 0) {
        Input_settings.slotState = Core_Random_NextInterfaceRandomBits(0x20);
        if (Input_settings.slotState < 3) {
            Input_settings.slotState = 0x17385CA9;
        }
        if (!(Menu_Framework_actionRestrictions & 0x400)) {
            memset(&savedata2->containerData, 0, sizeof savedata2->containerData);
        }
    }

    Input_settings.key = Core_Random_NextInterfaceRandomBits(0x20);
    s5->cipherHeader.cipherKey = Input_settings.key;
    s5->cipherHeader.slotHeader.slotState = Input_settings.slotState;
    Input_settings.saveFileGeneration = 0;

    for (i = 0; i < 5; ++i) {
        if (Title_saveFileSummaries[i].summary.base.slotState >= Save_CardFileSlotStatus_InUse) {
            if (Input_settings.saveFileGeneration
                < Title_saveFileSummaries[i].summary.base.generation) {
                Input_settings.saveFileGeneration =
                    Title_saveFileSummaries[i].summary.base.generation;
            }
        }
    }
    s5->cipherHeader.slotHeader.generation = ++Input_settings.saveFileGeneration;

    if (Input_settings.cumulativeSaveCount < 9999) {
        ++Input_settings.cumulativeSaveCount;
    }

    if (Input_settings.currentGameSaveCount < 9999) {
        ++Input_settings.currentGameSaveCount;
    }

    s5->cipherHeader.slotHeader.opaque8 = 0x20000107;
    s5->summary.gameTime.t = State_playTime.t;
    s5->summary.currentHP = State_ashleySnapshot.stats.currentHp;
    s5->summary.maxHP = State_ashleySnapshot.stats.maxHp;
    s5->summary.cumulativeSaveCount = Input_settings.cumulativeSaveCount;
    s5->summary.currentGameSaveCount = Input_settings.currentGameSaveCount;
    s5->summary.saveLocation = Menu_saveLocationId;
    s5->summary.mapCompletion = Field_MapCompletionPercent();
    s5->summary.clearCount = Audio_Seq_unusedStateFlags.clearCount;
    s5->summary.currentMP = State_ashleySnapshot.stats.currentMp;
    s5->summary.maxMP = State_ashleySnapshot.stats.maxMp;
    Core_Memory_CopyBytesBackward(savedata->stateFlags, &Audio_Seq_unusedStateFlags, sizeof savedata->stateFlags);
    Core_Memory_CopyBytesBackward(savedata->learnedActions, State_learnedActionFlags,
        sizeof savedata->learnedActions);
    Core_Memory_CopyBytesBackward(savedata->mapStatus, &State_roomVisitProgress, sizeof savedata->mapStatus);
    Core_Memory_CopyBytesBackward(&savedata->settings, &Input_settings, sizeof savedata->settings);
    Core_Memory_CopyBytesBackward(&savedata->ashleySnapshot, &State_ashleySnapshot, sizeof savedata->ashleySnapshot);
    Core_Memory_CopyBytesBackward(&savedata->inventory, &Audio_Seq_unusedInventory, sizeof savedata->inventory);
    Core_Memory_CopyBytesBackward(&savedata->inventoryIndices, &Audio_Seq_unusedInventoryIndices, sizeof savedata->inventoryIndices);
    Core_Memory_CopyBytesBackward(&savedata->entryDoor, &State_savedEntryDoor, sizeof savedata->entryDoor);
    Core_Memory_CopyBytesBackward(&savedata->scoreData, &Audio_Seq_unusedScoredata, sizeof savedata->scoreData);
    Core_Memory_CopyBytesBackward(&savedata->containerData, &savedata2->containerData,
        sizeof savedata->containerData);
    savedata->roomStateSlot = State_currentRoomStateSlot;
    Core_Memory_CopyBytesBackward(savedata->recentRoomEnemyStates, State_recentRoomEnemyStates, sizeof savedata->recentRoomEnemyStates);
    Core_Memory_CopyBytesBackward(&savedata->artsStatus, &Audio_Seq_unusedArtsStatus, sizeof savedata->artsStatus);

    for (i = 0; i < 92; ++i) {
        var_a0 = 0;
        if (i != 1) {
            for (j = 0; j < 256; ++j) {
                var_a0 ^= Title_memoryCardUiResource[i * 0x100 + j];
            }
            s5->blockChecksums[i] = var_a0;
        }
    }
    var_a0 = 0;
    for (j = 0; j < 256; ++j) {
        var_a0 ^= Title_memoryCardUiResource[j + 256];
    }
    s5->blockChecksums[1] = var_a0;
    for (i = (intptr_t)&((Save_GameSaveImage*)0)->protectedHeader.cipherHeader.slotHeader.slotState;
        i < (int)sizeof(Save_GameSaveImage); ++i) {
        Title_memoryCardUiResource[i] += Core_Random_NextInterfaceRandomBits(8);
    }
}

// decomp: MENU7.PRG _loadSaveData (src/MENU/MENU7.PRG/260.c:871)
int Save_MemoryCard_ReadSaveStep(int portFileno)
{
    enum Save_SaveReadState { Save_SaveReadState_Init = 0, Save_SaveReadState_Reading = 1 };

    static char Title_saveReadPhase;
    static char Title_saveReadCardSlot;
    static char Title_saveReadFileIndex;
    static char Title_isReadingTemporarySave;
    static char Title_saveReadErrorCount;
    static char Save_MemoryCard_readSaveStepLayoutPadding[2] __attribute__((unused));
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
            ((Menu_Files_fileProgressCounter - Title_previousTransferProgress)
                * ((Title_saveTransferErrorOffset * 20) - (Title_transferProgressPosition - new_var)))
            / Title_transferProgressTarget;
        Title_saveTransferErrorOffset = Title_saveReadErrorCount;
        Title_transferProgressTarget = 192 - (Title_isReadingTemporarySave << 7);
        Title_previousTransferProgress = Menu_Files_fileProgressCounter;

        memset(temp_s2, 0, sizeof(Save_GameSaveImage));

        if (Title_saveReadFileIndex & 8) {
            filename = Save_MemoryCard_BuildTemporaryFilename(Title_saveReadCardSlot, Title_saveReadFileIndex & 7);
        } else {
            filename = Save_MemoryCard_BuildSaveFilename(Title_saveReadCardSlot, Title_saveReadFileIndex);
        }
        Title_saveReadFileDescriptor = LegacyApi_open(filename, 0x8000 | 1);
        if (Title_saveReadFileDescriptor == -1) {
            ++Title_saveReadErrorCount;
            break;
        }
        Save_MemoryCard_ClearEvents(Save_CardEventGroup_Software);
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
        int event = Save_MemoryCard_PollEvents(Save_CardEventGroup_Software);
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

// decomp: MENU7.PRG _saveFile (src/MENU/MENU7.PRG/260.c:960)
int Save_MemoryCard_WriteSaveStep(int portFile)
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
    static char Save_MemoryCard_writeSaveStepLayoutPadding[3] __attribute__((unused));
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
        Title_saveWritePhase = Save_MemoryCard_HasSaveSuffix(Title_saveWriteFileIndex + 'A' - 1);
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
        temp_v1_2 = ((Menu_Files_fileProgressCounter - Title_previousTransferProgress)
                        * (320 - Title_transferProgressPosition))
                  / Title_transferProgressTarget;
        Title_previousTransferProgress = Menu_Files_fileProgressCounter;
        Title_transferProgressTarget = 384;
        Title_transferProgressPosition += temp_v1_2;
        Title_saveWriteFileDescriptor = LegacyApi_open(Save_MemoryCard_BuildTemporaryFilename(Title_saveWriteCardSlot, Title_saveWriteFileIndex), 0x8000 | 0x0002);
        ;
        if (Title_saveWriteFileDescriptor == -1) {
            ++Title_saveWriteErrorCount;
            break;
        }
        Save_MemoryCard_ClearEvents(Save_CardEventGroup_Software);
        if (LegacyApi_write(Title_saveWriteFileDescriptor, Title_memoryCardUiResource, sizeof(Save_GameSaveImage)) == -1) {
            LegacyApi_close(Title_saveWriteFileDescriptor);
            ++Title_saveWriteErrorCount;
            break;
        }
        Title_saveWritePhase = Save_SaveWriteState_ReadReady;
        // fallthrough
    case Save_SaveWriteState_ReadReady: {
        temp_s3 = Save_MemoryCard_PollEvents(Save_CardEventGroup_Software);
        if (temp_s3 < Save_CardEventSignal_None) {
            LegacyApi_close(Title_saveWriteFileDescriptor);
            if (temp_s3 == Save_CardEventSignal_IoEnd) {
                Title_saveWritePhase = Save_SaveWriteState_VerifyPending;
                i = Title_transferProgressPosition;
                temp_s3 = Title_previousTransferProgress;
                Save_MemoryCard_ReadSaveStep((Title_saveWriteCardSlot << 12) | (Title_saveWriteFileIndex + 8));
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
        temp_s3 = Save_MemoryCard_ReadSaveStep(0);
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

// decomp: MENU7.PRG _initMemcard (src/MENU/MENU7.PRG/260.c:1080)
int Save_MemoryCard_InitializeMenuResourcesStep(int init)
{
    enum Save_ResourceInitState {
        Save_ResourceInitState_None = 0,
        Save_ResourceInitState_QueueReady = 1,
        Save_ResourceInitState_Enqueued = 2,
    };

    static u_short Save_MemoryCard_legacyEventSpecifications[] = { EvSpIOE, EvSpERROR, EvSpTIMOUT, EvSpNEW };
    static char Save_MemoryCard_resourceLoadPhase;
    static char Save_MemoryCard_initializeMenuResourcesStepLayoutPadding[3] __attribute__((unused));
    static Content_DiscQueueSlot* Save_MemoryCard_resourceDiscRequest;

    Content_DiscFile cdFile;
    int i;
    u_int event;

    if (init != 0) {
        Render_ReserveTexturePageRange(0x1D, 3, 6);
        Title_memoryCardUiResource = Core_Memory_AllocateFromLowEnd(VS_SPMCIMG_BIN_SIZE);
        Save_MemoryCard_uiResources = (Save_CardUiResources*)(Title_memoryCardUiResource + sizeof(Save_GameSaveImage) * 3);
        Menu_Files_textTable = Save_MemoryCard_uiResources->textTable;
        Title_saveFileSummaries = Save_MemoryCard_uiResources->fileHeaders;
        Title_cardDirectoryEntryBuffer = &Save_MemoryCard_uiResources->directoryEntry;
        cdFile.lba = VS_SPMCIMG_BIN_LBA;
        cdFile.size = VS_SPMCIMG_BIN_SIZE;
        Save_MemoryCard_resourceDiscRequest = Content_AllocateDiscRequest(&cdFile);
        Content_EnqueueDiscRequest(Save_MemoryCard_resourceDiscRequest, Title_memoryCardUiResource);
        Save_MemoryCard_resourceLoadPhase = Save_ResourceInitState_None;
        return 0;
    }

    switch (Save_MemoryCard_resourceLoadPhase) {
    case Save_ResourceInitState_None:
        if (Save_MemoryCard_resourceDiscRequest->state == Content_DiscRequestState_Loaded) {
            Content_ReleaseDiscRequest(Save_MemoryCard_resourceDiscRequest);
            Render_UploadPackedRectangle(vs_getXY(800, 256), Title_memoryCardUiResource, vs_getWH(224, 256));
            Save_MemoryCard_resourceLoadPhase = Save_ResourceInitState_QueueReady;
        }
        return 0;
    case Save_ResourceInitState_QueueReady:
        cdFile.lba = VS_MCDATA_BIN_LBA; // MCMAN.BIN must immediately follow MCDATA.BIN on
                                        // the disk
        cdFile.size = VS_MCDATA_BIN_SIZE + VS_MCMAN_BIN_SIZE;
        Save_MemoryCard_resourceDiscRequest = Content_AllocateDiscRequest(&cdFile);
        Content_EnqueueDiscRequest(Save_MemoryCard_resourceDiscRequest, Save_MemoryCard_uiResources);
        Save_MemoryCard_resourceLoadPhase = Save_ResourceInitState_Enqueued;
        break;
    case Save_ResourceInitState_Enqueued:
        break;
    default:
        return 0;
    }

    if (Save_MemoryCard_resourceDiscRequest->state == Content_DiscRequestState_Loaded) {
        Content_ReleaseDiscRequest(Save_MemoryCard_resourceDiscRequest);
        Boot_SetSoftResetEnabled(0);

        for (i = 0; i < 8; ++i) {
            event = SwCARD;
            if (i & 4) {
                event = HwCARD;
            }
            Title_cardEventDescriptors[i] =
                OpenEvent(event, Save_MemoryCard_legacyEventSpecifications[i & 3], EvMdNOINTR, 0);
        }


        for (i = 0; i < 8; ++i) {
        }
        return 1;
    }
    return 0;
}

// decomp: MENU7.PRG _shutdownMemcard (src/MENU/MENU7.PRG/260.c:1161)
void Save_MemoryCard_ReleaseMenuResources(void)
{
    int i;

    for (i = 0; i < 8; ++i) {
    }


    for (i = 0; i < 8; ++i) {
    }

    Boot_SetSoftResetEnabled(1);
    Core_Memory_FreeFromLowEnd(Title_memoryCardUiResource);
    Render_RetireTexturePageRange(0x1D, 3, 6);
}

// decomp: MENU7.PRG _loadIntroSaveFile (src/MENU/MENU7.PRG/260.c:3315)
int Save_MemoryCard_LoadIntroSnapshotStep(int action)
{
    static Content_DiscQueueSlot* Save_MemoryCard_legacyIntroSnapshotDiscRequest;
    static Save_GameSaveImage* Save_MemoryCard_legacyIntroSnapshotImage;

    Content_DiscFile file;
    void* new_var2;
    State_PlayTime gametime;
    int lba;
    State_PlayTime* new_var;

    switch (action) {
    case 0:
        new_var = &gametime;
        if (Save_MemoryCard_legacyIntroSnapshotDiscRequest->state == Content_DiscRequestState_Loaded) {
            Content_ReleaseDiscRequest(Save_MemoryCard_legacyIntroSnapshotDiscRequest);
            gametime.t = State_playTime.t;
            Title_memoryCardUiResource = (char*)(Save_MemoryCard_legacyIntroSnapshotImage - 1);
            Save_MemoryCard_ValidateAndApplyLoadedSave(1);
            State_playTime.t = gametime.t;
            new_var2 = Save_MemoryCard_legacyIntroSnapshotImage;
            Core_Memory_FreeFromHighEnd(new_var2);
            return 1;
        }
        return 0;
    case 1: // First combat in courtyard
        lba = VS_OPMCIMG1_BIN_LBA;
        break;
    case 2: // D'tok
        lba = VS_OPMCIMG2_BIN_LBA;
        break;
    default:
        return 0;
    }

    file.lba = lba;
    file.size = VS_OPMCIMG1_BIN_SIZE;
    Save_MemoryCard_legacyIntroSnapshotDiscRequest = Content_AllocateDiscRequest(&file);
    new_var2 = Core_Memory_AllocateFromHighEnd(sizeof *Save_MemoryCard_legacyIntroSnapshotImage);
    Save_MemoryCard_legacyIntroSnapshotImage = new_var2;
    Content_EnqueueDiscRequest(Save_MemoryCard_legacyIntroSnapshotDiscRequest, Save_MemoryCard_legacyIntroSnapshotImage);
    return 0;
}

// decomp: TITLE.PRG _keystreamBits (src/TITLE/TITLE.PRG/22C.c:50)
u_int Save_MemoryCard_NextCipherBits(int value)
{
    u_int seed = Save_MemoryCard_keystreamSeed;

    Save_MemoryCard_keystreamSeed = seed * 0x19660D;
    return seed >> (32 - value);
}

// decomp: TITLE.PRG _packageGameSaveData (src/TITLE/TITLE.PRG/22C.c:616)
void Save_MemoryCard_PackageClearSave(int targetFile)
{
    static const u_short Save_MemoryCard_saveTitleTemplate[] = { 0x7582, 0x6082, 0x6682, 0x7182, 0x6082, 0x6D82,
        0x7382, 0x4081, 0x7282, 0x7382, 0x6E82, 0x7182, 0x7882, 0x4081, 0x6582, 0x6882,
        0x6B82, 0x6482, 0x4081, 0x5082, 0x4081, 0x4081, 0x4081, 0x4F82, 0x4F82, 0x4681,
        0x4F82, 0x4F82, 0x4681, 0x4F82, 0x4F82, 0x4081 };

    intptr_t i;
    int j;
    int var_a0;
    u_short const* s0 = Save_MemoryCard_saveTitleTemplate;
    Save_GameSaveImage* savedata = (Save_GameSaveImage*)Title_memoryCardUiResource;
    Save_GameSaveImage* savedata2 = savedata + 1;
    Save_GameSaveImage* savedata3 = savedata;
    Save_SaveProtectedHeader* s5 = &savedata->protectedHeader;

    State_playTime.all = 0;
    Input_settings.menuFlags &= 0xFFDF;
    Audio_Seq_unusedStateFlags.puzzleMode = ~(*(u_int*)&Input_settings >> 3) & 1;
    memset(savedata, 0, sizeof *savedata);
    ((char*)&savedata->cardHeader.key)[0] = 0x53;
    ((char*)&savedata->cardHeader.key)[1] = 0x43;
    ((char*)&savedata->cardHeader.key)[2] = 0x11;
    ((char*)&savedata->cardHeader.key)[3] = 3;
    Core_Memory_CopyBytesBackward(&savedata->cardHeader.summary, s0, sizeof savedata->cardHeader.summary);
    savedata->cardHeader.summary.opaque20[3] += (targetFile << 8);

    if (State_playTime.t.hours == 100) {
        savedata->cardHeader.summary.opaque20[6] = 0x5082;
    } else {
        savedata->cardHeader.summary.opaque20[7] += ((State_playTime.t.hours / 10) << 8);
        savedata->cardHeader.summary.opaque20[8] += ((State_playTime.t.hours % 10) << 8);
        savedata->cardHeader.summary.opaque20[10] += ((State_playTime.t.minutes / 10) << 8);
        savedata->cardHeader.summary.opaque20[11] += ((State_playTime.t.minutes % 10) << 8);
        savedata->cardHeader.summary.opaque20[13] += ((State_playTime.t.seconds / 10) << 8);
        savedata->cardHeader.summary.opaque20[14] += ((State_playTime.t.seconds % 10) << 8);
    }
    savedata3->cardHeader.summary.opaque20[15] = 0;
    Core_Memory_CopyBytesBackward(savedata3->cardHeader.iconHeaderBytes, Save_MemoryCard_uiResources->fileIconPalettes[targetFile],
        sizeof savedata->cardHeader.iconHeaderBytes);
    Core_Memory_CopyBytesBackward(savedata3->iconBitmap, Save_MemoryCard_uiResources->fileIconBitmaps[targetFile], sizeof savedata->iconBitmap);

    if (Input_settings.slotState == 0) {
        Input_settings.slotState = Save_MemoryCard_NextCipherBits(0x20);
        if (Input_settings.slotState < 3) {
            Input_settings.slotState = 0x17385CA9;
        }
        memset(&savedata2->containerData, 0, sizeof savedata2->containerData);
    }

    Input_settings.key = Save_MemoryCard_NextCipherBits(0x20);
    s5->cipherHeader.cipherKey = Input_settings.key;
    s5->cipherHeader.slotHeader.slotState = Input_settings.slotState;
    Input_settings.saveFileGeneration = 0;

    for (i = 0; i < 5; ++i) {
        if (Title_saveFileSummaries[i].summary.base.slotState >= Save_CardFileSlotStatus_InUse) {
            if (Input_settings.saveFileGeneration
                < Title_saveFileSummaries[i].summary.base.generation) {
                Input_settings.saveFileGeneration =
                    Title_saveFileSummaries[i].summary.base.generation;
            }
        }
    }

    s5->cipherHeader.slotHeader.generation = ++Input_settings.saveFileGeneration;

    if (Input_settings.cumulativeSaveCount < 9999) {
        ++Input_settings.cumulativeSaveCount;
    }

    Input_settings.currentGameSaveCount = 0;
    s5->cipherHeader.slotHeader.opaque8 = 0x20000107;
    s5->summary.gameTime.t = State_playTime.t;
    s5->summary.currentHP = State_ashleySnapshot.stats.currentHp;
    s5->summary.maxHP = State_ashleySnapshot.stats.maxHp;
    s5->summary.cumulativeSaveCount = Input_settings.cumulativeSaveCount;
    s5->summary.currentGameSaveCount = Input_settings.currentGameSaveCount;
    s5->summary.saveLocation = 0x30;
    s5->summary.mapCompletion = 0;
    memset(&Audio_Seq_unusedInventory.misc, 0, sizeof Audio_Seq_unusedInventory.misc);
    memset(&Audio_Seq_unusedInventoryIndices.misc, 0, sizeof Audio_Seq_unusedInventoryIndices.misc);
    memset(&savedata2->containerData.misc, 0,
        sizeof savedata2->containerData.misc / 4); // Why / 4?
    s5->summary.clearCount = Audio_Seq_unusedStateFlags.clearCount;
    s5->summary.currentMP = State_ashleySnapshot.stats.currentMp;
    s5->summary.maxMP = State_ashleySnapshot.stats.maxMp;
    Core_Memory_CopyBytesBackward(savedata->stateFlags, &Audio_Seq_unusedStateFlags, sizeof savedata->stateFlags);
    Core_Memory_CopyBytesBackward(savedata->learnedActions, State_learnedActionFlags, sizeof savedata->learnedActions);
    Core_Memory_CopyBytesBackward(savedata->mapStatus, &State_roomVisitProgress, sizeof savedata->mapStatus);
    Core_Memory_CopyBytesBackward(&savedata->settings, &Input_settings, sizeof savedata->settings);
    Core_Memory_CopyBytesBackward(&savedata->ashleySnapshot, &State_ashleySnapshot, sizeof savedata->ashleySnapshot);
    Core_Memory_CopyBytesBackward(&savedata->inventory, &Audio_Seq_unusedInventory, sizeof savedata->inventory);
    Core_Memory_CopyBytesBackward(&savedata->inventoryIndices, &Audio_Seq_unusedInventoryIndices, sizeof savedata->inventoryIndices);
    Core_Memory_CopyBytesBackward(&savedata->entryDoor, &State_savedEntryDoor, sizeof savedata->entryDoor);
    Core_Memory_CopyBytesBackward(&savedata->scoreData, &Audio_Seq_unusedScoredata, sizeof savedata->scoreData);
    Core_Memory_CopyBytesBackward(&savedata->containerData, &savedata2->containerData,
        sizeof savedata->containerData);
    savedata->roomStateSlot = State_currentRoomStateSlot;
    Core_Memory_CopyBytesBackward(savedata->recentRoomEnemyStates, State_recentRoomEnemyStates, sizeof savedata->recentRoomEnemyStates);
    Core_Memory_CopyBytesBackward(&savedata->artsStatus, &Audio_Seq_unusedArtsStatus, sizeof savedata->artsStatus);

    for (i = 0; i < 92; ++i) {
        var_a0 = 0;
        if (i != 1) {
            for (j = 0; j < 256; ++j) {
                var_a0 ^= Title_memoryCardUiResource[i * 0x100 + j];
            }
            s5->blockChecksums[i] = var_a0;
        }
    }
    var_a0 = 0;
    for (j = 0; j < 256; ++j) {
        var_a0 ^= Title_memoryCardUiResource[j + 256];
    }
    s5->blockChecksums[1] = var_a0;
    for (i = (intptr_t)&((Save_GameSaveImage*)0)->protectedHeader.cipherHeader.slotHeader.slotState;
        i < (int)sizeof(Save_GameSaveImage); ++i) {
        Title_memoryCardUiResource[i] += Save_MemoryCard_NextCipherBits(8);
    }
}

// decomp: TITLE.PRG _initMemcard (src/TITLE/TITLE.PRG/22C.c:940)
int Save_MemoryCard_InitializeResourcesStep(int init)
{
    enum Save_ResourceInitState {
        Save_ResourceInitState_None = 0,
        Save_ResourceInitState_QueueReady = 1,
        Save_ResourceInitState_Enqueued = 2,
    };

    static u_short Save_MemoryCard_legacyEventSpecifications[] = { EvSpIOE, EvSpERROR, EvSpTIMOUT, EvSpNEW };
    static char Save_MemoryCard_resourceLoadPhase;
    static char Title_cardInitializationPadding[3] __attribute__((unused));
    static Content_DiscQueueSlot* Save_MemoryCard_resourceDiscRequest;

    Content_DiscFile cdFile;
    int i;
    u_int event;

    if (init != 0) {
        Title_memoryCardUiResource = Core_Memory_AllocateFromLowEnd(VS_SPMCIMG_BIN_SIZE);
        Save_MemoryCard_uiResources = (Save_CardUiResources*)(Title_memoryCardUiResource + sizeof(Save_GameSaveImage) * 3);
        Title_saveMenuText = Save_MemoryCard_uiResources->textTable;
        Title_saveFileSummaries = Save_MemoryCard_uiResources->fileHeaders;
        Title_cardDirectoryEntryBuffer = &Save_MemoryCard_uiResources->directoryEntry;
        cdFile.lba = VS_SPMCIMG_BIN_LBA;
        cdFile.size = VS_SPMCIMG_BIN_SIZE;
        Save_MemoryCard_resourceDiscRequest = Content_AllocateDiscRequest(&cdFile);
        Content_EnqueueDiscRequest(Save_MemoryCard_resourceDiscRequest, Title_memoryCardUiResource);
        Save_MemoryCard_resourceLoadPhase = Save_ResourceInitState_None;
        return 0;
    }

    switch (Save_MemoryCard_resourceLoadPhase) {
    case Save_ResourceInitState_None:
        if (Save_MemoryCard_resourceDiscRequest->state == Content_DiscRequestState_Loaded) {
            Content_ReleaseDiscRequest(Save_MemoryCard_resourceDiscRequest);
            Render_UploadPackedRectangle(vs_getXY(800, 256), Title_memoryCardUiResource, vs_getWH(224, 256));
            Save_MemoryCard_resourceLoadPhase = Save_ResourceInitState_QueueReady;
        }
        return 0;
    case Save_ResourceInitState_QueueReady:
        cdFile.lba = VS_MCDATA_BIN_LBA; // MCMAN.BIN must immediately follow MCDATA.BIN on
                                        // the disk
        cdFile.size = VS_MCDATA_BIN_SIZE + VS_MCMAN_BIN_SIZE;
        Save_MemoryCard_resourceDiscRequest = Content_AllocateDiscRequest(&cdFile);
        Content_EnqueueDiscRequest(Save_MemoryCard_resourceDiscRequest, Save_MemoryCard_uiResources);
        Save_MemoryCard_resourceLoadPhase = Save_ResourceInitState_Enqueued;
        break;
    case Save_ResourceInitState_Enqueued:
        break;
    default:
        return 0;
    }

    if (Save_MemoryCard_resourceDiscRequest->state == Content_DiscRequestState_Loaded) {
        Content_ReleaseDiscRequest(Save_MemoryCard_resourceDiscRequest);
        Boot_SetSoftResetEnabled(0);

        for (i = 0; i < 8; ++i) {
            event = SwCARD;
            if (i & 4) {
                event = HwCARD;
            }
            Title_cardEventDescriptors[i] =
                OpenEvent(event, Save_MemoryCard_legacyEventSpecifications[i & 3], EvMdNOINTR, NULL);
        }


        for (i = 0; i < 8; ++i) {
        }
        return 1;
    }
    return 0;
}

// decomp: TITLE.PRG _shutdownMemcard (src/TITLE/TITLE.PRG/22C.c:1017)
void Save_MemoryCard_ReleaseResources(void)
{
    int i;

    for (i = 0; i < 8; ++i) {
    }


    for (i = 0; i < 8; ++i) {
    }

    Boot_SetSoftResetEnabled(1);
    Core_Memory_FreeFromLowEnd(Title_memoryCardUiResource);
}

// decomp: TITLE.PRG _saveFileExists (src/TITLE/TITLE.PRG/22C.c:3189)
int Save_MemoryCard_AnySaveFileExists(void)
{
    struct DIRENTRY dir;
    char filename[22];
    int memCardState;
    int i;
    int ret = 0;

    Save_MemoryCard_InitializeResourcesStep(1);

    do {
        State_UpdatePlayTimeAndLoading(2);
    } while (Save_MemoryCard_InitializeResourcesStep(0) == 0);

    for (i = 1; i <= 2; ++i) {
        Title_SaveCard_AdvanceCardReadiness(i);
        do {
            memCardState = Title_SaveCard_AdvanceCardReadiness(0) & 3;
            VSync(2);
        } while (memCardState == Save_CardReadinessResult_Pending);

        if (memCardState == Save_CardReadinessResult_Ready) {
            Core_Memory_CopyBytesBackward(filename, Save_MemoryCard_kOriginalFilenameTemplate, sizeof filename);

            filename[2] = i + '/';
            filename[20] = '?';

            if (LegacyApi_firstfile(filename, &dir) != NULL) {
                ret = 1;
                break;
            }
        }
    }
    Save_MemoryCard_ReleaseResources();
    return ret;
}

// TITLE.PRG reloads restore these statics' initial values (PORTING_WATCHLIST.md §1).
void Save_MemoryCard_GameFiles_ResetTitleStatics(void)
{
    {
        static const u_int initial = 0x19660D;
        memcpy((void*)&Save_MemoryCard_keystreamSeed, &initial, sizeof Save_MemoryCard_keystreamSeed);
    }
}

