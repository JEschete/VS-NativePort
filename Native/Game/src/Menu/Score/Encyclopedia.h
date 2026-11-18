// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_MENU_SCORE_ENCYCLOPEDIA_H_TYPES
#define VS_MENU_SCORE_ENCYCLOPEDIA_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_MENU_SCORE_ENCYCLOPEDIA_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_MENU_SCORE_ENCYCLOPEDIA_H_DECLS)
#define VS_MENU_SCORE_ENCYCLOPEDIA_H_DECLS
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

#endif // VS_MENU_SCORE_ENCYCLOPEDIA_H_DECLS
