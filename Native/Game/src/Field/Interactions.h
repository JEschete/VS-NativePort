// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_FIELD_INTERACTIONS_H_TYPES
#define VS_FIELD_INTERACTIONS_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_FIELD_INTERACTIONS_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_FIELD_INTERACTIONS_H_DECLS)
#define VS_FIELD_INTERACTIONS_H_DECLS
#include "Actors/Location.h"
#include "Field/InteractionFocusLive.h"
#include "Field/Mpd.h"
#include "Items/Loot.h"
#include "Items/TreasureChests.h"

extern Field_InteractionFocus Field_interactionFocus;
extern u_int Field_controlMode;
void Field_ApplyUnlockEffectToRoomObject(SVECTOR* target_position);
int Field_TileInFrontOfActor(Actors_ActorLocationQuery* actor_placement, u_char* output_facing_tile);
int Field_TryTriggerFacingTileEvent(int actor_index);
Field_MpdLockTrigger* Field_FindFacingRoomObject(int actor_index);
Items_TreasureChest* Field_FindFacingUnlootedChest(int actor_index);
int Field_CheckAshleyTileTriggers(int actorId, u_char* inputPosition);
int Field_CheckActorTrapAndFallTriggers(int actor_index, Actors_ActorLocation* actor_location);

#endif // VS_FIELD_INTERACTIONS_H_DECLS
