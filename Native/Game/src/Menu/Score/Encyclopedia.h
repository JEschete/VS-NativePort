// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"


#include "DiscQueue.h"
#include "Menu/Score/ScoreScreen.h"

extern int Menu_Score_enemyNavigationState;;
extern int Menu_Score_modelVariant;;
extern char Menu_Score_modelVariants[];
extern int Menu_Score_paletteFlag;;
extern char Menu_Score_paletteFlags[256];
extern char Menu_Score_variantMap[];
extern void* Menu_Score_zudBuffer;;
extern Content_DiscQueueSlot* Menu_Score_zudCdSlot;;
extern Menu_Score_ZudHeader* Menu_Score_zudData;;
extern int Menu_Score_zudLbas[];
extern int Menu_Score_zudSizes[];
void Menu_Score_Encyclopedia_ResetMenu9Statics();
