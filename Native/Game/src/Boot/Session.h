#pragma once
// Hand-maintained (tools/port/owned.csv).
#ifndef VS_OWNED_BOOT_SESSION_H
#define VS_OWNED_BOOT_SESSION_H
#include "Legacy/Psx.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*Boot_SessionEntry)(void);
int Boot_RunSessionLoop(Boot_SessionEntry title, Boot_SessionEntry battle);

int Boot_RunTitleAndBattle(void);

void Boot_RunGame(void);

void Boot_JumpToTitle(int*);

void Boot_JumpToGameLoop(int*);

void Boot_DelayAfterOverlayLoad(void);

#ifdef __cplusplus
}
#endif

#endif // VS_OWNED_BOOT_SESSION_H
