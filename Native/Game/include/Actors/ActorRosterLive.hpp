#pragma once
// Hand-maintained (tools/port/owned.csv).

struct Battle_Actor;

// Bindings from Battle_actors and the actor records to the typed roster
// (ActorRoster.cpp). Refusals are reported on the "actors" channel.
namespace VS::Actors {
// _insertActor (146C.c:884): 1 when `actor` joined the list after Ashley.
// Refused (0) for a null actor or an empty Ashley slot.
int LinkLiveActorAfterAshley(Battle_Actor* actor);
// _removeActor (146C.c:909): 1 when `actor` left the list; its next link is
// cleared. Refused (0) for a null actor.
int UnlinkLiveActorFromRoomList(Battle_Actor* actor);
// _removeActorAtIndex (146C.c:926): 0 for an index past the roster, else 1.
int RemoveLiveActorAtSlot(unsigned index, int hidden);
// func_8006C250 (146C.c:1842).
void FreezeLiveActorAnimations();
// func_8006C2A0 (146C.c:1854).
void UnfreezeLiveActorAnimations();
// func_8007647C (146C.c:6028): resets the stats of actor slot `actor_id` to
// a placeholder. Refused for an id outside Battle_actors or an actor without
// stats.
void InitializeLivePlaceholderActor(int actor_id, int auxiliary);
}  // namespace VS::Actors
