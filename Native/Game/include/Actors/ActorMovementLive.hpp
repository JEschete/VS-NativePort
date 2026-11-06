#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

#include "Actors/ActorMovement.hpp"

struct Actors_ActorRecord;

// Binding from the actor table and the original arguments to the typed actor
// movement rules (ActorMovement.hpp). Each adapter keeps the original argument
// order for the generated callers. An actor id outside the table or without a
// record does nothing; a null record pointer is refused with a warning.
namespace VS::Actors {
// The record's movement fields as the typed MovementActor, and their store.
[[nodiscard]] MovementActor LoadLiveMovementActor(
    const Actors_ActorRecord& live);
void StoreLiveMovementActor(const MovementActor& actor,
                            Actors_ActorRecord& live);

// The current room's movement services over the live tables.
[[nodiscard]] MovementTables LiveMovementTables();

void BeginLiveActorJumpToTile(Actors_ActorRecord* actor, PackedTile target,
                              int speed, std::int16_t facing);
void CancelLiveActorTeleportTransition(Actors_ActorRecord* actor);
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void BeginLiveActorJumpMovement(int actor_id, int direction, int distance);
// Modes outside 0 to 2 are refused with a warning; the original read an
// unset platform contact for them.
void UpdateLiveActorPlatformContact(Actors_ActorRecord* actor, int mode);
void UpdateLiveRoomActorMovement();
void UpdateLiveActorMovement(int actor_id);
// Runs the arc of `actor`, which the generated callers pass alongside its id.
void UpdateLiveActorJumpAndFall(int actor_id, Actors_ActorRecord* actor);
}  // namespace VS::Actors
