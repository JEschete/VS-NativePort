#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Battle/Ai/ActorCommands.hpp"

namespace VS::Battle::Ai {
struct ActorAiState;
}  // namespace VS::Battle::Ai

// Binding from the battle AI frame to the typed actor command dispatcher.
namespace VS::Battle::Ai {
// Queues `action` (a CommandIntent index) for the battle actor that owns
// `state`: marks the command pending, records the movement target and the
// frame's movement result, then dispatches through the frame terrain, actor
// model, selected candidate and destination reservations. Aborts with a
// diagnostic when the intent, actor slot or identity is invalid.
void IssueLiveActorCommand(VS::Battle::Ai::ActorAiState* state, int action);
}  // namespace VS::Battle::Ai
