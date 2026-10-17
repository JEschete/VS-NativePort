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

typedef void (*SpuTransferCallbackProc)(void);

// The SPU's register file (0x1F801C00-0x1F801DFF) as ordinary memory: the copy tool maps the game's register
// literals here, and the audio backend reads the voice state the sound driver writes.
#define LEGACY_SPU_REGISTER_BYTES 0x200
void* LegacySpu_Registers(void);
// The SPU's 512 KiB sample memory, filled by SpuWrite.
#define LEGACY_SPU_RAM_BYTES 0x80000
unsigned char* LegacySpu_Ram(void);

SpuTransferCallbackProc SpuSetTransferCallback(SpuTransferCallbackProc func);
unsigned long SpuSetTransferStartAddr(unsigned long addr);

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
