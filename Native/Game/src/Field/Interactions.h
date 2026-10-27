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
void Field_ApplyUnlockEffectToRoomObject(SVECTOR* target_position);
void Field_BeginObjectInteractionWait(void);
void Field_EndObjectInteractionWait(void);
int Field_InteractWithLockedRoomObject(Field_MpdLockTrigger* door_trigger);
void Field_TriggerActorTrap(Field_MpdLockTrigger* trap_trigger, Actors_ActorLocationQuery* victim_placement, int targetActor);
void Field_TryOpenChest(Field_MpdLockTrigger* chest_trigger, int arg1);
void Field_BeginTrapInteraction(Field_MpdLockTrigger* trap_trigger, Actors_ActorLocation* victim_location, int victim_actor_index);
void Field_BeginChestInteraction(Field_MpdLockTrigger* chest_trigger, int interacting_actor_index);
int Field_WaitForChestCameraFocus(void);
int Field_StepTrapCameraFocus(void);
void Field_DispatchTrapOrChestInteraction(void);
int Field_IsChestAnimationReady(void);
void Field_FinishChestInteraction(void);
void Field_FinishTrapInteraction(void);
int Field_TrackInteractionCameraToAshley(void);
void Field_OpenChestContents();
int Field_UpdateChestContentsSelection(void);
int Field_TileInFrontOfActor(Actors_ActorLocationQuery* actor_placement, u_char* output_facing_tile);
int Field_TryTriggerFacingTileEvent(int actor_index);
Field_MpdLockTrigger* Field_FindFacingRoomObject(int actor_index);
Items_TreasureChest* Field_FindFacingUnlootedChest(int actor_index);
int Field_CheckAshleyTileTriggers(int actorId, u_char* inputPosition);
int Field_CheckActorTrapAndFallTriggers(int actor_index, Actors_ActorLocation* actor_location);

#endif // VS_FIELD_INTERACTIONS_H_DECLS
