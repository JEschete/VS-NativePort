// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"

struct Save_CardFileInfoRecord;
struct Save_GameSaveImage;
struct Save_SaveCipherHeader;
struct Save_SaveFileStats;
struct Save_SaveFileSummary;
struct Save_SaveProtectedHeader;
struct Save_SaveSlotBase;

#include "Field/Mpd.h"
#include "Input/Settings.hpp"
#include "Items/Inventory.h"
#include "State/Ashley.hpp"
#include "State/GameClock.hpp"
#include "State/Progress.hpp"

// decomp: _findCurrentSave_e (src/MENU/MENU7.PRG/260.c:1561)
enum Save_FindSaveResult {
    Save_FindSaveResult_Timeout = -2,
    Save_FindSaveResult_NotFound = -1,
    Save_FindSaveResult_Pending = 0
};

// decomp: _memcardEventMask_e (src/MENU/MENU7.PRG/260.c:1624)
enum Save_EventMask {
    Save_EventMask_IgnoreNoEvent = 0x30,
    Save_EventMask_All = 0x70
};

// decomp: maskedHandler_e (src/MENU/MENU7.PRG/260.c:1629)
enum Save_MaskedHandlerResult {
    Save_MaskedHandlerResult_Error = -1,
    Save_MaskedHandlerResult_Pending = 0,
    Save_MaskedHandlerResult_Complete = 1
};

// decomp: memcardEventHandler_e (src/MENU/MENU7.PRG/260.c:38)
enum Save_CardReadinessResult {
    Save_CardReadinessResult_Pending = 0,
    Save_CardReadinessResult_Ready = 1,
    Save_CardReadinessResult_Timeout = 2,
    Save_CardReadinessResult_Unformatted = 3
};

// decomp: memcardEvents_e (src/MENU/MENU7.PRG/260.c:36)
enum Save_CardEventGroup { Save_CardEventGroup_Software, Save_CardEventGroup_Hardware = 4 };

// decomp: slotState_e (src/MENU/MENU7.PRG/260.c:21)
enum Save_CardFileSlotStatus {
    Save_CardFileSlotStatus_Unavailable = 0,
    Save_CardFileSlotStatus_Available = 1,
    Save_CardFileSlotStatus_Temporary = 2,
    Save_CardFileSlotStatus_InUse = 3
};

// decomp: testMemcardEvents_e (src/MENU/MENU7.PRG/260.c:28)
enum Save_CardEventSignal {
    Save_CardEventSignal_IoEnd,
    Save_CardEventSignal_Error,
    Save_CardEventSignal_Timeout,
    Save_CardEventSignal_Unformatted,
    Save_CardEventSignal_None,
};

// decomp: saveBase_t (src/MENU/MENU7.PRG/260.c:49)
struct Save_SaveSlotBase {
    enum Save_CardFileSlotStatus slotState;
    u_int generation;
    int formatSignature;
};
static_assert(sizeof(Save_SaveSlotBase) == 0xC, "Save_SaveSlotBase must keep its PS1 layout");

// decomp: stats_t (src/MENU/MENU7.PRG/260.c:63)
struct Save_SaveFileStats {
    VS::State::PlayTime gameTime;
    u_short cumulativeSaveCount;
    u_short currentGameSaveCount;
    u_short currentHP;
    u_short maxHP;
    char saveLocation;
    char clearCount;
    char mapCompletion;
    char reservedF;
    u_short currentMP;
    u_short maxMP;
};

// decomp: saveFileSubInfo_t (src/MENU/MENU7.PRG/260.c:69)
struct Save_SaveFileSummary {
    Save_SaveSlotBase base;
    Save_SaveFileStats stats;
    u_short cardTitleTail[16];
};

// decomp: saveFileInfo_t (src/MENU/MENU7.PRG/260.c:76)
struct Save_CardFileInfoRecord {
    u_int key;
    Save_SaveFileSummary summary;
    char reserved44[0x1C];
    char iconHeaderBytes[0x20];
};

// decomp: savedata_unk180_2_t (src/MENU/MENU7.PRG/260.c:81)
struct Save_SaveCipherHeader {
    int cipherKey;
    Save_SaveSlotBase slotHeader;
};
static_assert(sizeof(Save_SaveCipherHeader) == 0x10, "Save_SaveCipherHeader must keep its PS1 layout");

// decomp: savedata_unk180_t (src/MENU/MENU7.PRG/260.c:87)
struct Save_SaveProtectedHeader {
    Save_SaveCipherHeader cipherHeader;
    Save_SaveFileStats summary;
    char blockChecksums[0x5C];
};

// decomp: savedata_t (src/MENU/MENU7.PRG/260.c:108)
struct Save_GameSaveImage {
    Save_CardFileInfoRecord cardHeader;
    char iconBitmap[0x80];
    char reserved100[0x80];
    Save_SaveProtectedHeader protectedHeader;
    char stateFlags[0x440];
    char learnedActions[32];
    char mapStatus[0x48];
    VS::Input::Settings settings;
    VS::State::AshleySnapshot ashleySnapshot;
    Items_Inventory inventory;
    Items_InventoryIndices inventoryIndices;
    Field_MpdDoorRecord entryDoor;
    VS::State::ScoreData scoreData;
    int roomStateSlot;
    char recentRoomEnemyStates[0x520];
    VS::State::ArtsStatus artsStatus;
    Items_ContainerData containerData;
    char reserved59E0[0x220];
};
