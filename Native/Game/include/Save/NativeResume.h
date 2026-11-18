#pragma once

int Save_NativeResumePending();
int Save_NativeFinishResume();
void Save_NativeCancelResume();
void Save_NativeResetSession();
const char* Save_NativeResumeError();
void Save_NativeClearResumeError();
