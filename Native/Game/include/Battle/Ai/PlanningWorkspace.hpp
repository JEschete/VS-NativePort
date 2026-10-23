#ifndef NATIVE_GAME_INCLUDE_BATTLE_AI_PLANNINGWORKSPACE_HPP_
#define NATIVE_GAME_INCLUDE_BATTLE_AI_PLANNINGWORKSPACE_HPP_

#include "Legacy/Psx.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "Battle/Action.h"
#include "Battle/Ai/AiViews.h"
int Battle_Ai_RebuildActorActionPlans(void* input);
#ifdef __cplusplus
}

namespace VS::Game::ai {

struct PlanningWorkspace {
  static constexpr int kPasses = 3;
  static constexpr int kCandidates = 16;
  static constexpr int kActors = 16;
  static constexpr int kActions = 256;

  Battle_ActionRequest result{};
  Battle_Ai_CandidateEntryView candidates[kPasses][kCandidates]{};
  unsigned char exclusions[kActors][kActions]{};
  Battle_Ai_CandidateEntryView fallback[kCandidates]{};
};

}  // namespace VS::Game::ai
#endif

#endif  // NATIVE_GAME_INCLUDE_BATTLE_AI_PLANNINGWORKSPACE_HPP_
