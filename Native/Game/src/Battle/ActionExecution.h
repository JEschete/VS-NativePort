// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_BATTLE_ACTIONEXECUTION_H_TYPES
#define VS_BATTLE_ACTIONEXECUTION_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_BATTLE_ACTIONEXECUTION_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_BATTLE_ACTIONEXECUTION_H_DECLS)
#define VS_BATTLE_ACTIONEXECUTION_H_DECLS

extern int Battle_settledCameraDistance;
extern int Battle_cameraEaseMode;
void Battle_ActionTargetLimbPosition(int arg0, int arg1, SVECTOR* arg2);
int Battle_ActionTargetDistance(void);
void Battle_ActionCameraMidpoint(SVECTOR* arg0);
void Battle_StepCameraTowardActionMidpoint(int arg0);
void Battle_UpdateInitialTargetSelection(void);
void Battle_UpdateActionAreaSelection(void);
void Battle_UpdateFinalTargetSelection(void);
void Battle_UpdateActionCameraZoom(void);
void Battle_WaitForActionEffectReady(void);
void Battle_StartReadyActionAnimation(void);
void Battle_StartFollowupActionEffects(void);
void Battle_WaitForActionResolution(void);
void Battle_WaitForTargetRecovery(void);
void Battle_FinishActionSequence(void);

#endif // VS_BATTLE_ACTIONEXECUTION_H_DECLS
