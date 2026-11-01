#pragma once
// Hand-maintained (tools/port/owned.csv).

struct Actors_ActorRecord;

// Binds an actor record and the frame tick speed to the typed sound glide.
namespace VS::Actors {
// Steps the actor's sound transition toward its target for this frame.
void UpdateActorSoundTransition(Actors_ActorRecord& actor);
}  // namespace VS::Actors
