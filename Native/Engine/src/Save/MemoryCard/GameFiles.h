// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_SAVE_MEMORYCARD_GAMEFILES_H_TYPES
#define VS_SAVE_MEMORYCARD_GAMEFILES_H_TYPES
#include "Legacy/Psx.h"

typedef struct Save_CardFileInfoRecord Save_CardFileInfoRecord;
typedef struct Save_GameSaveImage Save_GameSaveImage;
typedef struct Save_SaveCipherHeader Save_SaveCipherHeader;
typedef struct Save_SaveFileStats Save_SaveFileStats;
typedef struct Save_SaveFileSummary Save_SaveFileSummary;
typedef struct Save_SaveProtectedHeader Save_SaveProtectedHeader;
typedef struct Save_SaveSlotBase Save_SaveSlotBase;

#ifndef VS_TYPES_ONLY
#define VS_TYPES_ONLY
#define VS_SAVE_MEMORYCARD_GAMEFILES_H_SET_TYPES_ONLY
#endif
#include "Field/Mpd.h"
#include "Items/Inventory.h"
#include "State/Ashley.h"
#include "State/GameTime.h"
#include "State/Progress.h"
#include "State/Settings.h"
#ifdef VS_SAVE_MEMORYCARD_GAMEFILES_H_SET_TYPES_ONLY
#undef VS_TYPES_ONLY
#undef VS_SAVE_MEMORYCARD_GAMEFILES_H_SET_TYPES_ONLY
#endif

// decomp: src/include/gpu.h:95
#define vs_getXY(x, y) (((y) << 16) | ((x) & 0xFFFF))

// decomp: _findCurrentSave_e (src/MENU/MENU7.PRG/260.c:1561)
enum Save_FindSaveResult {
    Save_FindSaveResult_Timeout = -2,
    Save_FindSaveResult_NotFound = -1,
    Save_FindSaveResult_Pending = 0
};
typedef enum Save_FindSaveResult Save_FindSaveResult;

// decomp: _memcardEventMask_e (src/MENU/MENU7.PRG/260.c:1624)
enum Save_EventMask {
    Save_EventMask_IgnoreNoEvent = 0x30,
    Save_EventMask_All = 0x70
};
typedef enum Save_EventMask Save_EventMask;

// decomp: maskedHandler_e (src/MENU/MENU7.PRG/260.c:1629)
enum Save_MaskedHandlerResult {
    Save_MaskedHandlerResult_Error = -1,
    Save_MaskedHandlerResult_Pending = 0,
    Save_MaskedHandlerResult_Complete = 1
};
typedef enum Save_MaskedHandlerResult Save_MaskedHandlerResult;

// decomp: memcardEventHandler_e (src/MENU/MENU7.PRG/260.c:38)
enum Save_CardReadinessResult {
    Save_CardReadinessResult_Pending = 0,
    Save_CardReadinessResult_Ready = 1,
    Save_CardReadinessResult_Timeout = 2,
    Save_CardReadinessResult_Unformatted = 3
};
typedef enum Save_CardReadinessResult Save_CardReadinessResult;

// decomp: memcardEvents_e (src/MENU/MENU7.PRG/260.c:36)
enum Save_CardEventGroup { Save_CardEventGroup_Software, Save_CardEventGroup_Hardware = 4 };
typedef enum Save_CardEventGroup Save_CardEventGroup;

// decomp: slotState_e (src/MENU/MENU7.PRG/260.c:21)
enum Save_CardFileSlotStatus {
    Save_CardFileSlotStatus_Unavailable = 0,
    Save_CardFileSlotStatus_Available = 1,
    Save_CardFileSlotStatus_Temporary = 2,
    Save_CardFileSlotStatus_InUse = 3
};
typedef enum Save_CardFileSlotStatus Save_CardFileSlotStatus;

// decomp: testMemcardEvents_e (src/MENU/MENU7.PRG/260.c:28)
enum Save_CardEventSignal {
    Save_CardEventSignal_IoEnd,
    Save_CardEventSignal_Error,
    Save_CardEventSignal_Timeout,
    Save_CardEventSignal_Unformatted,
    Save_CardEventSignal_None,
};
typedef enum Save_CardEventSignal Save_CardEventSignal;

// decomp: saveBase_t (src/MENU/MENU7.PRG/260.c:49)
typedef struct Save_SaveSlotBase {
    enum Save_CardFileSlotStatus slotState;
    u_int generation;
    int opaque8;
} Save_SaveSlotBase;
_Static_assert(sizeof(Save_SaveSlotBase) == 0xC, "Save_SaveSlotBase must keep its PS1 layout");

// decomp: stats_t (src/MENU/MENU7.PRG/260.c:63)
typedef struct Save_SaveFileStats {
    State_PlayTime gameTime;
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
} Save_SaveFileStats;

// decomp: saveFileSubInfo_t (src/MENU/MENU7.PRG/260.c:69)
typedef struct Save_SaveFileSummary {
    Save_SaveSlotBase base;
    Save_SaveFileStats stats;
    u_short opaque20[16];
} Save_SaveFileSummary;

// decomp: saveFileInfo_t (src/MENU/MENU7.PRG/260.c:76)
typedef struct Save_CardFileInfoRecord {
    u_int key;
    Save_SaveFileSummary summary;
    char reserved44[0x1C];
    char iconHeaderBytes[0x20];
} Save_CardFileInfoRecord;

// decomp: savedata_unk180_2_t (src/MENU/MENU7.PRG/260.c:81)
typedef struct Save_SaveCipherHeader {
    int cipherKey;
    Save_SaveSlotBase slotHeader;
} Save_SaveCipherHeader;
_Static_assert(sizeof(Save_SaveCipherHeader) == 0x10, "Save_SaveCipherHeader must keep its PS1 layout");

// decomp: savedata_unk180_t (src/MENU/MENU7.PRG/260.c:87)
typedef struct Save_SaveProtectedHeader {
    Save_SaveCipherHeader cipherHeader;
    Save_SaveFileStats summary;
    char blockChecksums[0x5C];
} Save_SaveProtectedHeader;

// decomp: savedata_t (src/MENU/MENU7.PRG/260.c:108)
typedef struct Save_GameSaveImage {
    Save_CardFileInfoRecord cardHeader;
    char iconBitmap[0x80];
    char reserved100[0x80];
    Save_SaveProtectedHeader protectedHeader;
    char stateFlags[0x440];
    char learnedActions[32];
    char mapStatus[0x48];
    State_Settings settings;
    State_AshleySnapshot ashleySnapshot;
    Items_Inventory inventory;
    Items_InventoryIndices inventoryIndices;
    Field_MpdDoorRecord entryDoor;
    State_ScoreData scoreData;
    int roomStateSlot;
    char recentRoomEnemyStates[0x520];
    State_ArtsStatus artsStatus;
    Items_ContainerData containerData;
    char reserved59E0[0x220];
} Save_GameSaveImage;

#endif // VS_SAVE_MEMORYCARD_GAMEFILES_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_SAVE_MEMORYCARD_GAMEFILES_H_DECLS)
#define VS_SAVE_MEMORYCARD_GAMEFILES_H_DECLS

extern char const* Save_MemoryCard_kOriginalFilenameTemplate;
enum Save_CardEventSignal Save_MemoryCard_PollEvents(enum Save_CardEventGroup type);
void Save_MemoryCard_ClearEvents(int type);
char* Save_MemoryCard_BuildSaveFilename(int port, int fileNo);
char* Save_MemoryCard_BuildTemporaryFilename(int port, int fileNo);
u_int Save_MemoryCard_NewestSaveSlot(void);
int Save_MemoryCard_FindMatchingSaveSlot(void);
int Save_MemoryCard_ParseSaveFileNumber(char* filename);
int Save_MemoryCard_HasChecksumError(Save_GameSaveImage data[], int sectorCount);
void Save_MemoryCard_DecodeSaveBytes(u_int key, void* buf, int count);
int Save_MemoryCard_ReadSaveSummary(int id);
int Save_MemoryCard_HasSaveSuffix(int id);
int Save_MemoryCard_DeleteRedundantTemporaryFiles(int port);
int Save_MemoryCard_ScanSaveDirectory(int port);
int Save_MemoryCard_CreateSaveFile(int port, int id);
enum Save_CardReadinessResult Save_MemoryCard_AdvanceCardReadiness(int initPort);
int Save_MemoryCard_ValidateAndApplyLoadedSave(int write);
void Save_MemoryCard_PackageGameSave(int targetFile);
int Save_MemoryCard_ReadSaveStep(int portFileno);
int Save_MemoryCard_WriteSaveStep(int portFile);
int Save_MemoryCard_InitializeMenuResourcesStep(int init);
void Save_MemoryCard_ReleaseMenuResources(void);
int Save_MemoryCard_LoadIntroSnapshotStep(int action);
u_int Save_MemoryCard_NextCipherBits(int value);
void Save_MemoryCard_PackageClearSave(int targetFile);
int Save_MemoryCard_InitializeResourcesStep(int init);
void Save_MemoryCard_ReleaseResources(void);
int Save_MemoryCard_AnySaveFileExists(void);
void Save_MemoryCard_GameFiles_ResetTitleStatics(void);

#endif // VS_SAVE_MEMORYCARD_GAMEFILES_H_DECLS
