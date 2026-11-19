// Storage for the campaign progress records (Progress.hpp).
#include "State/Progress.hpp"

namespace VS::State {
// decomp: vs_main_actionsLearned (vs_main_actionsLearned@SLUS_010.40)
std::uint8_t learnedActionFlags[kLearnedActionFlagBytes];

// decomp: vs_main_mapStatus (vs_main_mapStatus@SLUS_010.40)
RoomVisitFlags roomVisitProgress;

// decomp: vs_main_scoredata (vs_main_scoredata@SLUS_010.40)
ScoreData scoreData;

// decomp: vs_main_artsStatus (vs_main_artsStatus@SLUS_010.40)
ArtsStatus artsStatus;
}  // namespace VS::State
