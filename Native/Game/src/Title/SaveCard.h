// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_TITLE_SAVECARD_H_TYPES
#define VS_TITLE_SAVECARD_H_TYPES
#include "Legacy/Psx.h"


// decomp: _memcardMaskedHandler_e (src/TITLE/TITLE.PRG/22C.c:1482)
enum Title_CardPollResult {
    Title_CardPollResult_Error = -1,
    Title_CardPollResult_Pending = 0,
    Title_CardPollResult_Complete = 1
};
typedef enum Title_CardPollResult Title_CardPollResult;

#endif // VS_TITLE_SAVECARD_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_TITLE_SAVECARD_H_DECLS)
#define VS_TITLE_SAVECARD_H_DECLS
#include "Save/MemoryCard/GameFiles.h"

extern char Title_saveDirectoryPadding[4];;
extern char Title_saveTransferPadding;;
extern struct DIRENTRY* Title_cardDirectoryEntryBuffer;;
extern char Title_saveTransferErrorOffset;;
extern long Title_cardEventDescriptors[8];;
extern struct DIRENTRY* Title_cardDirectoryEntries[15];;
enum Save_CardEventSignal Title_SaveCard_PollEvents(enum Save_CardEventGroup type);
void Title_SaveCard_ClearEvents(enum Save_CardEventGroup type);
u_int Title_SaveCard_NewestSaveSlot(void);
int Title_SaveCard_FindMatchingSaveSlot(void);
int Title_SaveCard_ReadSaveSummary(int id);
int Title_SaveCard_HasSaveSuffix(int id);
int Title_SaveCard_ScanSaveDirectory(int port);
enum Save_CardReadinessResult Title_SaveCard_AdvanceCardReadiness(int initPort);
int Title_SaveCard_ValidateAndApplyLoadedSave(int apply_loaded_state);
int Title_SaveCard_ReadSaveStep(int portFileno);
int Title_SaveCard_WriteSaveStep(int portFile);

#endif // VS_TITLE_SAVECARD_H_DECLS
