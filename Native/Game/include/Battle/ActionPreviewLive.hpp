#pragma once

#include "Core/Gte.hpp"

namespace VS::Battle {

struct ActionRequestRecord;
struct HitResult;
}  // namespace VS::Battle

namespace VS::Battle {

struct HitStatusView;
void ClearLiveActionRequest(void* request);
void InitializeLiveActionHit(VS::Battle::HitResult* hit);
void FinalizeLiveActionHit(VS::Battle::HitStatusView* hit);
void ResolveLiveActionTargets(int action,
                              VS::Battle::ActionRequestRecord* result,
                              VS::Battle::ActionRequestRecord* source,
                              int execute);
// Nonmigrated actions and tests of the retained callback/alias contract.
void ResolveLiveCompatibilityActionTargets(int action,
                                           ActionRequestRecord* result,
                                           ActionRequestRecord* source,
                                           int execute);
int PreviewLiveActionAgainstLimb(int action, int source, int target, int limb,
                                 void* result);
int PreviewLiveActionAgainstActor(int action, int source, int target, int limb,
                                  VS::Core::Gte::SVECTOR* source_position,
                                  VS::Core::Gte::SVECTOR* target_position,
                                  VS::Battle::ActionRequestRecord* result);
}  // namespace VS::Battle
