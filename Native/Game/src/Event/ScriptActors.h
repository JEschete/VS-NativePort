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
intptr_t Event_ScriptResetActorGroupState(u_char* instruction, short thread_index);
intptr_t Event_ScriptStoreActorTile(u_char* instruction, short thread_index);
intptr_t Event_ScriptSpawnActor(u_char* instruction, short thread_index);
intptr_t Event_ScriptNavigateActorToTile(u_char* instruction, short thread_index);
intptr_t Event_ScriptPlayActorAnimation(u_char* instruction, short thread_index);
intptr_t Event_ScriptSetActorGroupFlags(u_char* instruction, short thread_index);
intptr_t Event_ScriptSetActorSoundMix(u_char* instruction, short thread_index);
intptr_t Event_ScriptWalkActorToTile(u_char* instruction, short thread_index);
intptr_t Event_ScriptWalkActorToWorldPos(u_char* instruction, short thread_index);
intptr_t Event_ScriptSlideActorToTile(u_char* instruction, short thread_index);
intptr_t Event_ScriptSlideActorToWorldPos(u_char* instruction, short thread_index);
intptr_t Event_ScriptSetActorAttachment(u_char* instruction, short thread_index);
intptr_t Event_ScriptNavigateActorToWorldPos(u_char* instruction, short thread_index);
intptr_t Event_ScriptTurnActorToFacing(u_char* instruction, short thread_index);
intptr_t Event_ScriptFaceOtherActor(u_char* instruction, short thread_index);
intptr_t Event_ScriptFaceTile(u_char* instruction, short thread_index);
intptr_t Event_ScriptScaleActor(u_char* instruction, short thread_index);
intptr_t Event_ScriptRemoveActorGroup(u_char* instruction, short thread_index);
intptr_t Event_ScriptWaitForActorFlags(u_char* instruction, short thread_index);
intptr_t Event_ScriptSetActorColorOverride(u_char* instruction, short thread_index);
intptr_t Event_ScriptQueueActorAnimation(u_char* instruction, short thread_index);
intptr_t Event_ScriptRotateActorAxes(u_char* instruction, short thread_index);
intptr_t Event_ScriptSetActorHeadAngles(u_char* instruction, short thread_index);
intptr_t Event_ScriptPlayActorAnimationAtFrame(u_char* instruction, short thread_index);
intptr_t Event_ScriptQueueActorAnimationAtFrame(u_char* instruction, short thread_index);
intptr_t Event_ScriptSetActorIdleAnimation(u_char* instruction, short thread_index);
intptr_t Event_ScriptLookAtActor(u_char* instruction, short thread_index);
intptr_t Event_ScriptLookAtWorldPos(u_char* instruction, short thread_index);
intptr_t Event_ScriptLoadObjectModel(u_char* instruction, short thread_index);
intptr_t Event_ScriptWaitForObjectLoad(u_char* instruction, short thread_index);
intptr_t Event_ScriptReleaseObjectModel(u_char* instruction, short thread_index);
intptr_t Event_ScriptSelectObjectTextureBank(u_char* instruction, short thread_index);
intptr_t Event_ScriptSetActorOverlayPass(u_char* instruction, short thread_index);
intptr_t Event_ScriptSetActorWeaponDrawn(u_char* instruction, short thread_index);
intptr_t Event_ScriptPlayActorAnimationSound(u_char* instruction, short thread_index);
intptr_t Event_ScriptSetNpcUpdatesEnabled(u_char* instruction, short thread_index);
intptr_t Event_ScriptRestoreActorVitals(u_char* instruction, short thread_index);
intptr_t Event_ScriptSetActorEquipmentModels(u_char* instruction, short thread_index);
intptr_t Event_ScriptSetActorYawOscillation(u_char* instruction, short thread_index);
intptr_t Event_ScriptClearAshleyStatusEffects(u_char* instruction, short thread_index);
intptr_t Event_ScriptSetActorRenderMode(u_char* instruction, short thread_index);
intptr_t Event_CommandSetEnteredDoor(u_char* instruction, short thread_index);
intptr_t Event_CommandStoreActorRangeBearing(u_char* instruction, short thread_index);
intptr_t Event_CommandSetActorActivity(u_char* instruction, short thread_index);
intptr_t Event_CommandAwardMiscLoot(u_char* instruction, short thread_index);
intptr_t Event_CommandConsumeMiscItem(u_char* instruction, short thread_index);
intptr_t Event_CommandStoreActorDistance(u_char* instruction, short thread_index);
void Event_SetEventActorActivity(short arg0);
void Event_BuildScriptActorSelection(u_short arg0, Event_ActorSelection* arg1);
u_short Event_NextScriptSelectedActor(Event_ActorSelection* arg0);
int Event_ActorDialogAnchor(int arg0, SVECTOR* arg1);

#endif // VS_EVENT_SCRIPTACTORS_H_DECLS
