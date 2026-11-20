#pragma once
// Hand-maintained (tools/port/owned.csv).

namespace VS::Actors {
struct ActorRecord;
}

// Binds an actor record and the frame tick speed to the typed sound glide.
namespace VS::Actors {
// Steps the actor's sound transition toward its target for this frame.
void UpdateActorSoundTransition(VS::Actors::ActorRecord& actor);
}  // namespace VS::Actors
