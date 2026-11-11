#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Battle/Ai/ActorCreation.hpp"

struct Battle_Actor;

// Binding from the AI state table, the room AI state, the tile flag grid, the
// battle actors, their actor records, skeletons and the heap to the typed
// actor AI state creation (ActorCreation.cpp).
namespace VS::Battle::Ai {
// Creates the actor's AI state (func_800E6178). `options` are the actor's
// remembered AI flags (PackLiveActorAiPersistenceFlags), narrowed to a
// halfword, or negative for a fresh state. Returns 1 when the actor was
// remembered as idle and 0 otherwise or without an AI state. The battle
// module's pointer table stores this binding (0x800E6178). Aborts with a
// diagnostic when creation refuses the actor.
int CreateLiveActorAiState(Battle_Actor* actor, int options);
}  // namespace VS::Battle::Ai
