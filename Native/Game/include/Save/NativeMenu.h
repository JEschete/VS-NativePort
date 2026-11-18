#pragma once
#include "Items/Inventory.h"
#include "Legacy/Psx.h"

int Save_NativeSelectContainer(int init);
int Save_NativeInitialTitleSelection();
void Save_NativeDrawFileOverlay(int title);
Items_ContainerData* Save_NativeContainerData();
void Save_NativeReleaseContainer();
// TITLE calls only while Boot_isClearSavePending still identifies the ending.
// Reset/return to title/shutdown release the owned staging copy.
void Save_NativeStageClearGame();
