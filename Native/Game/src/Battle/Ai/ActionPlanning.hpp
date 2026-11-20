#pragma once
// The action planner's working state: the timed status flag table, the
// stats snapshot and candidate lists of the current planning pass, the
// fallback candidates and the per-actor candidate exclusions.
// ActionPlanning.cpp defines them.

#include <array>
#include <cstddef>
#include <cstdint>

struct Battle_ActorStats;
struct Battle_Ai_CandidateEntryView;
struct Battle_Ai_ReactionRecord;

namespace VS::Battle::Ai {

inline constexpr std::size_t kTimedStatusFlagWords = 344;
inline constexpr std::size_t kPassCandidates = 16;
inline constexpr std::size_t kCandidateExclusionBytes = 256;

// decomp: D_80069BBC (src/BATTLE/BATTLE.PRG/6E4CCShared.h:509)
extern std::array<std::uint32_t, kTimedStatusFlagWords> kTimedStatusFlags;
// decomp: D_800F58E8 (src/BATTLE/BATTLE.PRG/6E4CCShared.h:528)
extern Battle_ActorStats* planningStatsSnapshot;
// decomp: D_800F58EC (src/BATTLE/BATTLE.PRG/6E4CCShared.h:529)
extern Battle_Ai_CandidateEntryView (*planningPassCandidates)[kPassCandidates];
// decomp: D_800F58F0 (src/BATTLE/BATTLE.PRG/6E4CCShared.h:530)
extern std::int32_t fallbackCandidateCount;
// decomp: D_800F58F4 (src/BATTLE/BATTLE.PRG/6E4CCShared.h:531)
extern std::int32_t unusedPlanCounter;
// decomp: D_800F58F8 (src/BATTLE/BATTLE.PRG/6E4CCShared.h:532)
extern Battle_Ai_CandidateEntryView* fallbackCandidates;
// decomp: D_800F5900 (src/BATTLE/BATTLE.PRG/6E4CCShared.h:533)
extern Battle_Ai_ReactionRecord* planningWorkspace;
// decomp: D_800F5904 (src/BATTLE/BATTLE.PRG/6E4CCShared.h:534)
extern std::uint8_t (*candidateExclusions)[kCandidateExclusionBytes];

}  // namespace VS::Battle::Ai
