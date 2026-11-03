#pragma once

struct Actors_ActorRecord;

// The actor table lookups the Live adapters share. Refusals are reported on
// the "actors" channel as "<operation> refused: <reason>".
namespace VS::Actors {
void WarnActorRefused(const char* operation, const char* reason);
[[nodiscard]] bool IsActorTableId(int actor_id);
// The record `actor_id` names, or null for an empty slot. An id past the
// table (which the originals read past) is reported and treated as empty.
[[nodiscard]] Actors_ActorRecord* LiveActorRecord(int actor_id,
                                                  const char* operation);
}  // namespace VS::Actors
