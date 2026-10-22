#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void Audio_Seq_ReleaseSavedVoiceConflicts(void);
void Audio_Native_SaveMusicSnapshot(void);
void Audio_Native_RestoreMusicSnapshot(int swap);

#ifdef __cplusplus
}
#endif
