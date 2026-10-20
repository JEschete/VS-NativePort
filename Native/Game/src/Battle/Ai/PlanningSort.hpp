#pragma once

#include <span>

struct Battle_Ai_CandidateEntryView;

namespace VS::Battle::ai {
void SortCandidatePointers(std::span<Battle_Ai_CandidateEntryView*> candidates);
}
