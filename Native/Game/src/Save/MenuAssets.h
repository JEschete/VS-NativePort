#pragma once
// Hand-maintained (tools/port/owned.csv).
#ifndef VS_OWNED_SAVE_MENUASSETS_H
#define VS_OWNED_SAVE_MENUASSETS_H
#include "Field/Mpd.h"
#include "Items/Inventory.h"
#include "Legacy/Psx.h"
#include "State/Ashley.h"
#include "State/GameTime.h"
#include "State/Progress.h"
#include "State/Settings.h"

int Save_MemoryCard_InitializeMenuResourcesStep(int init);

void Save_MemoryCard_ReleaseMenuResources(void);

int Save_MemoryCard_InitializeResourcesStep(int init);

void Save_MemoryCard_ReleaseResources(void);

#endif  // VS_OWNED_SAVE_MENUASSETS_H
