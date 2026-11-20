#pragma once

#include "Field/InteractionTarget.hpp"

namespace VS::Field {
struct MpdLockTrigger;
}  // namespace VS::Field

namespace VS::Field {

void QueueGimmickAnimationEntry(int queued_job_kind,
                                VS::Field::MpdLockTrigger* lock_trigger);
void UpdateGimmickAnimationQueueEntry();
void ResetGimmickAnimationQueueEntry();

bool QueueLiveGimmickAnimation(int kind, VS::Field::MpdLockTrigger* target);
bool QueueLiveGimmickAnimation(int kind, ChestTarget target);
bool QueueLiveGimmickAnimation(int kind, RoomObjectTarget target);

}  // namespace VS::Field
