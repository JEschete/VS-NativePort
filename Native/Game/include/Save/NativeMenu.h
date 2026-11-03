#pragma once
#include "Legacy/Psx.h"
#ifdef __cplusplus
extern "C" {
#endif
#include "Items/Inventory.h"

int Save_NativeSelectContainer(int init);
int Save_NativeInitialTitleSelection(void);
void Save_NativeDrawFileOverlay(int title);
Items_ContainerData* Save_NativeContainerData(void);
void Save_NativeReleaseContainer(void);
// TITLE calls only while Boot_isClearSavePending still identifies the ending.
// Reset/return to title/shutdown release the owned staging copy.
void Save_NativeStageClearGame(void);

#ifdef __cplusplus
}
#endif
