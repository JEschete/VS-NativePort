// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_BOOT_LIFECYCLE_H_TYPES
#define VS_BOOT_LIFECYCLE_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_BOOT_LIFECYCLE_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_BOOT_LIFECYCLE_H_DECLS)
#define VS_BOOT_LIFECYCLE_H_DECLS

void Boot_LoadBattleOverlays(void);
void Boot_LoadTitleOverlay(void);
void Boot_LoadEndingOverlay(void);
void Boot_ResetToTitle(void);
void Boot_EnterBattle(void);
void Boot_RunEndingAndReturnToTitle(void);
void Boot_InitializePlatformServices(void);
void Boot_InitializeTitleSession(void);
int Boot_RunTitleAndBattle(void);
void Boot_RunGame(void);
void Boot_WaitForOverlayReady(void);
void Boot_SetSoftResetEnabled(int enable);
void Boot_CaptureOverlayStack(int*);
void Boot_JumpToTitle(int*);
void Boot_JumpToGameLoop(int*);
void Boot_DelayAfterOverlayLoad(void);

#endif // VS_BOOT_LIFECYCLE_H_DECLS
