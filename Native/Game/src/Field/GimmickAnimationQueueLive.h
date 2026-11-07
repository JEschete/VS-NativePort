#pragma once

#ifdef __cplusplus
extern "C" {
#endif
struct Field_MpdLockTrigger;
void Field_QueueGimmickAnimationEntry(
    int queued_job_kind, struct Field_MpdLockTrigger* lock_trigger);
#ifdef __cplusplus
}
#endif
