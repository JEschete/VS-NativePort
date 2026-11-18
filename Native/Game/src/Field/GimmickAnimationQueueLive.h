#pragma once

struct Field_MpdLockTrigger;
void Field_QueueGimmickAnimationEntry(
    int queued_job_kind, struct Field_MpdLockTrigger* lock_trigger);
void Field_UpdateGimmickAnimationQueueEntry(void);
void Field_ResetGimmickAnimationQueueEntry(void);

#include "Field/InteractionTarget.hpp"
namespace VS::Field {
bool QueueLiveGimmickAnimation(int kind, Field_MpdLockTrigger* target);
bool QueueLiveGimmickAnimation(int kind, ChestTarget target);
bool QueueLiveGimmickAnimation(int kind, RoomObjectTarget target);
}  // namespace VS::Field
