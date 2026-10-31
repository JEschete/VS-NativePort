// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_MENU_STATUS_ACTORSTATUS_H_TYPES
#define VS_MENU_STATUS_ACTORSTATUS_H_TYPES
#include "Legacy/Psx.h"


// decomp: _renderLimbUiState (src/MENU/MENU4.PRG/120.c:1138)
enum Menu_Status_LimbUiMode { Menu_Status_LimbUiMode_None, Menu_Status_LimbUiMode_Init };
typedef enum Menu_Status_LimbUiMode Menu_Status_LimbUiMode;

#endif // VS_MENU_STATUS_ACTORSTATUS_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_MENU_STATUS_ACTORSTATUS_H_DECLS)
#define VS_MENU_STATUS_ACTORSTATUS_H_DECLS
#include "Battle/Actor.h"
#include "Items/Equipment.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Items/Weapons.h"

extern char Menu_Status_layoutPadding1[24];;
extern int Menu_Status_animationStep;
extern char Menu_Status_basicStatsAnimDelay;
extern char Menu_Status_basicStatsAnimState;
extern char Menu_Status_delayScreenUpdate;
extern int Menu_Status_drawBackgroundFirst;
extern int Menu_Status_fadeScreen;
extern char Menu_Status_initializedActor;
extern char* Menu_Status_limbStatuses[];
extern int Menu_Status_previousActor;
extern char Menu_Status_renderBasicStatsState;
extern int Menu_Status_selectedActor;
extern u_char Menu_Status_selectedElement;;
extern char Menu_Status_selectedStatRow;;
extern u_short Menu_Status_statusStrings[1760]; /* contents: disc string table (ModuleData.cpp) */;
extern int Menu_Status_xPos;
extern int Menu_Status_yPos;
void Menu_Status_BeginVitalsTransition(int actor);
int Menu_Status_FindNextInspectableActor(int currentActor, int targetActor);
int Menu_Status_AnimateActorSwitch(int init);
void Menu_Status_DrawUnanalyzedValue(int xy, int withMax, u_long* before);
void Menu_Status_DrawVitalBar(int colorIndex, int current, int max, int xy);
void Menu_Status_DrawVitals(void);
void Menu_Status_DrawLimbConditionIndicator(int colorIndex, int xy, int subdued);
void Menu_Status_DrawLimbLeaderLine(
    int limb, int xy, int arg2 __attribute__((unused)), int opaque);
void Menu_Status_DrawStatusEffectIcons(Battle_ActorStats* actor, int animationStep);
int Menu_Status_NavigateStatusEffects(Battle_ActorStats* actor, int selection_active);
int Menu_Status_LimbCount(int actor);
int Menu_Status_UpdateLimbPanel(int init);
void Menu_Status_DrawModelSlideFade(void);
void Menu_Status_DrawBackground(void);
void Menu_Status_DrawFrame(void);
void Menu_Status_ShowSelectedLimbCondition(void);
void Menu_Status_PopulateSelectedLimbDefense(void);
int Menu_Status_Update(u_char* state);
void Menu_Status_ActorStatus_ResetMenu4Statics(void);

#endif // VS_MENU_STATUS_ACTORSTATUS_H_DECLS
