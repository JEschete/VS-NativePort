#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Battle/ActionResults.hpp"

struct SVECTOR;
namespace VS::Battle {
struct HitResult;
}  // namespace VS::Battle

// Binding from the battle action queue to the typed target effects.
namespace VS::Battle {
// Applies the current action's hits on `actor_id` and plays its reaction, or
// queues a world-locked label at `position` for a tagged world target id.
// Aborts on a negative id, a missing action state, an invalid hit count or
// action id, or a tagged world target without a position.
void ApplyLiveActionTargetEffects(int actor_id, SVECTOR* position);
// Applies one hit through the same result pipeline used by target effects.
// Refuses a missing hit/target/stats or a selected armor limb out of range.
void ApplyLiveActorActionResult(VS::Battle::HitResult* hit);
}  // namespace VS::Battle
