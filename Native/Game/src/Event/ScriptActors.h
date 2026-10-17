// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_EVENT_SCRIPTACTORS_H_TYPES
#define VS_EVENT_SCRIPTACTORS_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_EVENT_SCRIPTACTORS_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_EVENT_SCRIPTACTORS_H_DECLS)
#define VS_EVENT_SCRIPTACTORS_H_DECLS
#include "Event/Actors.h"

extern char Event_scriptControlledActors[17];
extern char Event_skipState;
int Event_ScriptResetActorGroupState(u_char* arg0, short arg1);
int Event_ScriptStoreActorTile(u_char* arg0, short arg1);
int Event_ScriptSpawnActor(u_char* arg0, short arg1);
int Event_ScriptNavigateActorToTile(u_char* arg0, short arg1);
int Event_ScriptPlayActorAnimation(u_char* arg0);
int Event_ScriptSetActorGroupFlags(u_char* arg0, short arg1);
int Event_ScriptSetActorSoundMix(u_char* arg0, short arg1);
int Event_ScriptWalkActorToTile(u_char* arg0, short arg1);
int Event_ScriptWalkActorToWorldPos(u_char* arg0, short arg1);
int Event_ScriptSlideActorToTile(u_char* arg0, short arg1);
int Event_ScriptSlideActorToWorldPos(u_char* arg0, short arg1);
int Event_ScriptSetActorAttachment(u_char* arg0, short arg1);
int Event_ScriptNavigateActorToWorldPos(u_char* arg0, short arg1);
int Event_ScriptTurnActorToFacing(u_char* arg0, short arg1);
int Event_ScriptFaceOtherActor(u_char* arg0, short arg1);
int Event_ScriptFaceTile(u_char* arg0, short arg1);
int Event_ScriptScaleActor(u_char* arg0, short arg1);
int Event_ScriptRemoveActorGroup(u_char* arg0, short arg1);
int Event_ScriptWaitForActorFlags(u_char* arg0);
int Event_ScriptSetActorColorOverride(u_char* arg0, short arg1);
int Event_ScriptQueueActorAnimation(u_char* arg0);
int Event_ScriptRotateActorAxes(u_char* arg0, short arg1);
int Event_ScriptSetActorHeadAngles(u_char* arg0, short arg1);
int Event_ScriptPlayActorAnimationAtFrame(u_char* arg0);
int Event_ScriptQueueActorAnimationAtFrame(u_char* arg0);
int Event_ScriptSetActorIdleAnimation(u_char* arg0, short arg1);
int Event_ScriptLookAtActor(u_char* arg0, short arg1);
int Event_ScriptLookAtWorldPos(u_char* arg0, short arg1);
int Event_ScriptLoadObjectModel(u_char* arg0, short arg1);
int Event_ScriptWaitForObjectLoad(u_char* arg0, short arg1);
int Event_ScriptReleaseObjectModel(u_char* arg0, short arg1);
int Event_ScriptSelectObjectTextureBank(u_char* arg0, short arg1);
int Event_ScriptSetActorOverlayPass(u_char* arg0, short arg1);
int Event_ScriptSetActorWeaponDrawn(u_char* arg0, short arg1);
int Event_ScriptPlayActorAnimationSound(u_char* arg0, short arg1);
int Event_ScriptSetNpcUpdatesEnabled(u_char* arg0, short arg1);
int Event_ScriptRestoreActorVitals(u_char* arg0, short arg1);
int Event_ScriptSetActorEquipmentModels(u_char* arg0, short arg1);
int Event_ScriptSetActorYawOscillation(u_char* arg0, short arg1);
int Event_ScriptClearAshleyStatusEffects(u_char* arg0, short arg1);
int Event_ScriptSetActorRenderMode(u_char* arg0, short arg1);
int Event_ScriptSetAshleyBattleMode(u_char*, short);
int Event_CommandSetEnteredDoor(u_char* arg0, short arg1);
int Event_CommandStoreActorRangeBearing(u_char* arg0, short arg1);
int Event_CommandSetActorActivity(u_char* arg0, short arg1);
int Event_CommandAwardMiscLoot(u_char* arg0);
int Event_CommandConsumeMiscItem(u_char* arg0, short arg1);
int Event_CommandStoreActorDistance(u_char* arg0, short arg1);
void Event_SetEventActorActivity(short arg0);
void Event_BuildScriptActorSelection(u_short arg0, Event_ActorSelection* arg1);
u_short Event_NextScriptSelectedActor(Event_ActorSelection* arg0);
int Event_ActorDialogAnchor(int arg0, SVECTOR* arg1);

#endif // VS_EVENT_SCRIPTACTORS_H_DECLS
