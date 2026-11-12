#pragma once

#include "Legacy/Psx.h"

struct Battle_ActionRequest;
struct Battle_HitResult;
struct Battle_HitStatusView;

namespace VS::Battle {
void ClearLiveActionRequest(void* request);
void InitializeLiveActionHit(Battle_HitResult* hit);
void FinalizeLiveActionHit(Battle_HitStatusView* hit);
void ResolveLiveActionTargets(int action, Battle_ActionRequest* result,
                              Battle_ActionRequest* source, int execute);
int PreviewLiveActionAgainstLimb(int action, int source, int target, int limb,
                                 void* result);
int PreviewLiveActionAgainstActor(int action, int source, int target, int limb,
                                  SVECTOR* source_position,
                                  SVECTOR* target_position,
                                  Battle_ActionRequest* result);
}  // namespace VS::Battle
