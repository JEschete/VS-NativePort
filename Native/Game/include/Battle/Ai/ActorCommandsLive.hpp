#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Battle/Ai/ActorCommands.hpp"

struct Battle_Ai_ActorAiState;

// Binding from the battle AI frame to the typed actor command dispatcher.
namespace VS::Battle::Ai {
// Queues `action` (a CommandIntent index) for the battle actor that owns
// `state`: marks the command pending, records the movement target and the
// frame's movement result, then dispatches through the frame terrain, actor
// model, selected candidate and destination reservations. Aborts with a
// diagnostic when the intent, actor slot or identity is invalid.
void IssueLiveActorCommand(Battle_Ai_ActorAiState* state, int action);
}  // namespace VS::Battle::Ai
