// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_BOOT_GAMELOOP_H_TYPES
#define VS_BOOT_GAMELOOP_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_BOOT_GAMELOOP_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_BOOT_GAMELOOP_H_DECLS)
#define VS_BOOT_GAMELOOP_H_DECLS

extern int Boot_gameEntryMode;
extern int Boot_titleReturnStackPointer;
extern int Boot_isSoftResetEnabled;
extern int Boot_battleReturnStackPointer;
extern int Boot_isClearSavePending;
extern int Boot_isGameplayActive;
void Boot_RunGameLoop(void);
void Boot_InitializeGameplayOverlay(int startState __attribute__((unused)));

#endif // VS_BOOT_GAMELOOP_H_DECLS
