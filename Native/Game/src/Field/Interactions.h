// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_FIELD_INTERACTIONS_H_TYPES
#define VS_FIELD_INTERACTIONS_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_FIELD_INTERACTIONS_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_FIELD_INTERACTIONS_H_DECLS)
#define VS_FIELD_INTERACTIONS_H_DECLS
#include "Actors/Location.h"
#include "Field/Interaction.h"
#include "Field/Mpd.h"
#include "Items/Loot.h"
#include "Items/TreasureChests.h"

extern Field_InteractionFocus Field_interactionFocus;
extern u_int Field_controlMode;
void Field_ApplyUnlockEffectToRoomObject(SVECTOR* arg0);
void Field_BeginObjectInteractionWait(void);
void Field_EndObjectInteractionWait(void);
int Field_InteractWithLockedRoomObject(Field_MpdLockTrigger* arg0);
void Field_TriggerActorTrap(Field_MpdLockTrigger* arg0, Actors_ActorLocationQuery* arg1, int targetActor);
void Field_TryOpenChest(Field_MpdLockTrigger* arg0, int arg1);
void Field_BeginTrapInteraction(Field_MpdLockTrigger* arg0, Actors_ActorLocation* arg1, int arg2);
void Field_BeginChestInteraction(Field_MpdLockTrigger* arg0, int arg1);
int Field_WaitForChestCameraFocus(void);
int Field_StepTrapCameraFocus(void);
void Field_DispatchTrapOrChestInteraction(void);
int Field_IsChestAnimationReady(void);
void Field_FinishChestInteraction(void);
void Field_FinishTrapInteraction(void);
int Field_TrackInteractionCameraToAshley(void);
void Field_OpenChestContents();
int Field_UpdateChestContentsSelection(void);
int Field_TileInFrontOfActor(Actors_ActorLocationQuery* arg0, u_char* arg1);
int Field_TryTriggerFacingTileEvent(int arg0);
Field_MpdLockTrigger* Field_FindFacingRoomObject(int arg0);
Items_TreasureChest* Field_FindFacingUnlootedChest(int arg0);
int Field_CheckAshleyTileTriggers(int actorId, u_char* inputPosition);
int Field_CheckActorTrapAndFallTriggers(int arg0, Actors_ActorLocation* arg1);

#endif // VS_FIELD_INTERACTIONS_H_DECLS
