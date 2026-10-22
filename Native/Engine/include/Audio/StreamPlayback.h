#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct Audio_CommandParams;

int Audio_BindRoomCutsceneStream(void* data, size_t byte_count);
int Audio_UnbindRoomCutsceneStream(void);
void Audio_Cutscene_StartStream(uint32_t* data, int pan, int reverb);
void Audio_Cutscene_ReplaceStream(struct Audio_CommandParams* params);
void Audio_Cutscene_StopStreamVoices(void);
void Audio_Cutscene_StopStream(struct Audio_CommandParams* params);
void Audio_Cutscene_PauseStream(struct Audio_CommandParams* params);
void Audio_Cutscene_ResumeStream(struct Audio_CommandParams* params);
void Audio_Cutscene_SetStreamVolume(struct Audio_CommandParams* params);
void Audio_Cutscene_SlideStreamVolume(struct Audio_CommandParams* params);
void Audio_Cutscene_SetStreamPan(struct Audio_CommandParams* params);
void Audio_Native_UpdateCutsceneStream(void);
void Audio_Native_ResetCutsceneStream(void);

#ifdef __cplusplus
}
#endif
