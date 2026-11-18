#pragma once

#include "Field/InteractionTarget.hpp"

struct Field_MpdLockTrigger;

namespace VS::Field {

void QueueGimmickAnimationEntry(int queued_job_kind,
                                Field_MpdLockTrigger* lock_trigger);
void UpdateGimmickAnimationQueueEntry();
void ResetGimmickAnimationQueueEntry();

bool QueueLiveGimmickAnimation(int kind, Field_MpdLockTrigger* target);
bool QueueLiveGimmickAnimation(int kind, ChestTarget target);
bool QueueLiveGimmickAnimation(int kind, RoomObjectTarget target);

}  // namespace VS::Field
