#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct Audio_SeqChannel;

void Audio_Native_Reset(void);
void Audio_Native_Tick(void);
void Audio_Native_ConfigureChannel(struct Audio_SeqChannel* channel);
int16_t Audio_Native_Envelope(unsigned voice);
void Audio_Native_SetNoiseClock(int clock);
int Audio_Native_ReverbMode(void);
void Audio_Native_SetReverbMode(int mode);
void Audio_Native_SetReverbDepth(int16_t left, int16_t right);
void Audio_Native_EnableReverb(int enabled);
int Audio_Native_StartStreamVoice(unsigned voice, const char* instrument,
                                  float rate, unsigned left, unsigned right,
                                  int reverb);
void Audio_Native_PauseVoice(unsigned voice, int paused);
int Audio_Native_VoiceActive(unsigned voice);

#ifdef __cplusplus
}
#endif
