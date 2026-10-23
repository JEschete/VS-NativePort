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
void Menu_Score_ShowEnemyDescription(void);
void Menu_Score_DrawEnemyNumber(int x, int y, int enemyNo, int dimmed);
void Menu_Score_InitializeEnemyList(void);
void Menu_Score_RenderEnemyList(int animState);
void Menu_Score_RenderEnemyViewer(int animState, int state);
void Menu_Score_UpdateEnemyNavigation(int direction);
int Menu_Score_LoadEnemyPreview(int zudId);
void Menu_Score_FreeEnemyPreview(void);
void Menu_Score_InitializeAshleyPreviewPose(void);
void Menu_Score_QueueEnemyModelRead(int zudId);
int Menu_Score_LoadEnemyModelParts(void);
void Menu_Score_FreeEnemyModelBuffer(void);
void Menu_Score_Encyclopedia_ResetMenu9Statics(void);

#endif // VS_MENU_SCORE_ENCYCLOPEDIA_H_DECLS
