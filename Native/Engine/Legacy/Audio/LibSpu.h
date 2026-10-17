#pragma once

// libspu/libsnd surface (plan §6.3, strategy T): voice and reverb control translated into Engine\Audio.

#include "Legacy/Core/PsxTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SPU_OFF 0
#define SPU_ON 1
#define SPU_TRANSFER_BY_DMA 0L
#define SPU_REV_MODE_CLEAR_WA 0x100
#define SPU_MALLOC_RECSIZ 8

typedef struct SpuVolume {
    short left, right;
} SpuVolume;

void SpuStart(void);
long SpuSetNoiseClock(long nClock);
long SpuSetReverb(long onOff);
unsigned long SpuWrite(unsigned char* addr, unsigned long size);
unsigned long SpuRead(unsigned char* addr, unsigned long size);
void SpuGetVoiceEnvelope(int voice, short* envx);
void SpuSetCommonMasterVolume(short left, short right);
long SpuSetReverbModeType(long mode);
void SpuSetReverbModeDepth(short left, short right);
void SpuGetReverbModeType(long* mode);
void SsUtReverbOn(void);
void SsUtReverbOff(void);

#ifdef __cplusplus
}
#endif
