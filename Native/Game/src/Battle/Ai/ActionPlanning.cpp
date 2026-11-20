// The action planner's working state (ActionPlanning.hpp).
#include "Battle/Ai/ActionPlanning.hpp"

namespace VS::Battle::Ai {
// decomp: D_80069BBC (D_80069BBC@BATTLE/BATTLE.PRG); contents: module image
// (ModuleData.cpp)
std::array<std::uint32_t, kTimedStatusFlagWords> kTimedStatusFlags;
// decomp: D_800F58E8 (D_800F58E8@BATTLE/BATTLE.PRG)
VS::Battle::ActorStats* planningStatsSnapshot;
// decomp: D_800F58EC (D_800F58EC@BATTLE/BATTLE.PRG)
VS::Battle::Ai::CandidateEntryView (*planningPassCandidates)[kPassCandidates];
// decomp: D_800F58F0 (D_800F58F0@BATTLE/BATTLE.PRG)
std::int32_t fallbackCandidateCount;
// decomp: D_800F58F4 (D_800F58F4@BATTLE/BATTLE.PRG)
std::int32_t unusedPlanCounter;
// decomp: D_800F58F8 (D_800F58F8@BATTLE/BATTLE.PRG)
VS::Battle::Ai::CandidateEntryView* fallbackCandidates;
// decomp: D_800F5900 (D_800F5900@BATTLE/BATTLE.PRG)
VS::Battle::Ai::ReactionRecord* planningWorkspace;
// decomp: D_800F5904 (D_800F5904@BATTLE/BATTLE.PRG)
std::uint8_t (*candidateExclusions)[kCandidateExclusionBytes];
}  // namespace VS::Battle::Ai
