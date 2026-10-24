#pragma once
#include "Legacy/Psx.h"
#ifdef __cplusplus
extern "C" {
#endif
#include "Items/Inventory.h"

int Menu_Files_UpdateDataSaveMenu(int initState);
int Menu_Files_UpdateDataLoadMenu(int initFadeout);
int Menu_Files_UpdateLoadMenu(int initFadeout);
int Menu_Files_UpdateSaveMenu(int init_state);
int Save_NativeSelectContainer(int init);
int Save_NativeInitialTitleSelection(void);
Items_ContainerData* Save_NativeContainerData(void);
void Save_NativeReleaseContainer(void);

#ifdef __cplusplus
}
#endif
