#pragma once

#include "Battle/Action.hpp"
#include "Battle/Ai/ActionRecords.hpp"
#include "Battle/Ai/PlanningWorkspace.hpp"

namespace VS::Battle {
struct CombatActionResult;
}

namespace VS::Battle::Ai {

// Call-local storage for one plan rebuild; it replaces the original fixed
// 0x1ECC allocation and its PS1 offsets.
struct PlanningWorkspace {
  static constexpr int kPasses = 3;
  static constexpr int kCandidates = 16;
  static constexpr int kActors = 16;
  static constexpr int kActions = 256;

  VS::Battle::ActionRequestRecord result{};
  VS::Battle::Ai::CandidateEntryView candidates[kPasses][kCandidates]{};
  unsigned char exclusions[kActors][kActions]{};
  VS::Battle::Ai::CandidateEntryView fallback[kCandidates]{};
};

// Reads the queued action's primary target actor (+0x04, lbu) and its
// targets-position word (+0x44, lw), as func_800DEC88 does (#255).
PlanningRequest PlanningRequestOf(
    const VS::Battle::ActionRequestRecord& request);

// Rebuilds every live actor's action plan against `request`, the queued action
// (or null), with call-local planning storage bound to the planner globals for
// the call only. Returns 0, as func_800DEC88 does for its callers' tests.
int RebuildLiveActorActionPlans(VS::Battle::ActionRequestRecord* request);
int RebuildLiveCombatActionPlans(const CombatActionResult& result);

}  // namespace VS::Battle::Ai
