#pragma once

#ifdef __cplusplus
extern "C" {
#endif
struct Field_MpdLockTrigger;
void Field_QueueGimmickAnimationEntry(
    int queued_job_kind, struct Field_MpdLockTrigger* lock_trigger);
void Field_UpdateGimmickAnimationQueueEntry(void);
void Field_ResetGimmickAnimationQueueEntry(void);
#ifdef __cplusplus
}
#endif
