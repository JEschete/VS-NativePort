#pragma once

#ifdef __cplusplus
extern "C" {
#endif

int Save_NativeResumePending(void);
int Save_NativeFinishResume(void);
void Save_NativeCancelResume(void);
void Save_NativeResetSession(void);
const char* Save_NativeResumeError(void);
void Save_NativeClearResumeError(void);

#ifdef __cplusplus
}
#endif
