// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_BATTLE_TARGETING_H_TYPES
#define VS_BATTLE_TARGETING_H_TYPES
#include "Legacy/Psx.h"

typedef struct Battle_TargetPreview Battle_TargetPreview;

// decomp: D_800F19CC_t5 (src/BATTLE/BATTLE.PRG/146C.h:901)
typedef struct Battle_TargetPreview {
    SVECTOR position;
    u_char kind;
    u_char actorId;
    signed char limb;
    char effect;
    short previewAmount;
    short hitChance;
    int removedStatuses;
    char actionKind;
    char opaque15;
    char opaque16;
    char enemyClass;
} Battle_TargetPreview;

#endif // VS_BATTLE_TARGETING_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_BATTLE_TARGETING_H_DECLS)
#define VS_BATTLE_TARGETING_H_DECLS
#include "Battle/Action.h"
#include "Battle/ActionResolution.h"
#include "Battle/Ai/AiViews.h"
#include "Battle/BattleTypes.h"
#include "Battle/Hit.h"
#include "Menu/Framework/TextBoxes.h"

extern char Battle_categoryNeedsTargetSelection[96];
extern u_char Battle_categoryIsAreaTarget[80];
extern u_char Battle_categoryIncludesActingActor[48];
extern u_char Battle_categoryTargetsBodyParts[32];
extern u_char Battle_categoryTargetActorMasks[16];
extern u_short Battle_statusStrings[1712];
extern char Battle_targetInfoTemplate[3140];
extern char Battle_targetStatusLineTemplate[3090];
extern char Battle_emptyStatusString[2932];
extern u_char Battle_targetSelectionStep;
extern u_char Battle_targetColumnEntries[2][16];
extern u_char Battle_secondColumnGroups[8];
extern u_char Battle_firstColumnGroups[8];
extern u_char Battle_secondColumnGroupFirstTargets[8];
extern u_char Battle_firstColumnGroupFirstTargets[8];
extern u_char Battle_secondColumnEntryCount;
extern u_char Battle_firstColumnEntryCount;
extern u_char Battle_secondColumnGroupCount;
extern u_char Battle_firstColumnGroupCount;
extern u_char Battle_targetColumnTopOffset;
extern Menu_StringContext Battle_stringContext;
void Battle_PopulateActionTargetPreview(Battle_TargetPreview* arg0, Battle_ActionRequest* arg1, Battle_HitResult* arg2);
void Battle_ReturnToBattleActionMenu(void);
void Battle_FilterAreaActionTargets(void);
void Battle_AimActionBehindSource(Battle_ActionRequest* arg0);
void Battle_SetSingleActionTarget(void);
void Battle_SetAllActionTargets(void);
void Battle_CollectEnvironmentActionTargets(void);
void Battle_CollectSourceAreaActionTargets(int arg0 __attribute__((unused)));
void Battle_CollectDirectActionTargets(int arg0);
void Battle_CollectGroundAreaActionTargets(int arg0);
void Battle_InitializeActionTargeting(int action);
void Battle_BeginActorActionSelection(int arg0, u_short arg1);
int Battle_UpdateSphereTargetPosition(SVECTOR* arg0);
void Battle_ClearTargetActorHighlight(int arg0, int arg1);
void Battle_SetTargetActorHighlight(int arg0, int arg1, int arg2);
void Battle_SetTargetActorColorOverride(int arg0, void* arg1);
void Battle_UpdateTargetSelectionHighlight(int mode);
void Battle_AppendTargetGroupToColumn(int group);
void Battle_BuildTargetSelectionColumns(void);
void Battle_DismissTargetSelectionRows(void);
Battle_TargetSphereLabelEntry* Battle_InitTargetSelectionRow(int arg0, int arg1, int arg2);
char* Battle_FirstTargetStatusText(uint arg0);
int Battle_UpdateTargetSelectionMenu(u_char* target);
int Battle_CalculateRangeShapeExtent(Battle_Ai_ActionRangeShape* arg0);
int Battle_CalculateActionAreaReach(int arg0, int actionId, u_int arg2);
int Battle_CalculateActionRangeSquared(Battle_Ai_ActionRangeView* arg0, int actionId);

#endif // VS_BATTLE_TARGETING_H_DECLS
