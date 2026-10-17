#include "Legacy/Audio/LibSpu.h"
#include "Core/Unported.h"

// Transitional shims (plan §2.2). Each records VS_UNPORTED until it translates into the modern system.

extern "C" {
void SpuStart(void)
{
    VS_UNPORTED("libspu:SpuStart");
}

long SpuSetNoiseClock(long nClock)
{
    (void)nClock;
    VS_UNPORTED("libspu:SpuSetNoiseClock");
    return {};
}

long SpuSetReverb(long onOff)
{
    (void)onOff;
    VS_UNPORTED("libspu:SpuSetReverb");
    return {};
}

unsigned long SpuWrite(unsigned char* addr, unsigned long size)
{
    (void)addr;
    (void)size;
    VS_UNPORTED("libspu:SpuWrite");
    return {};
}

unsigned long SpuRead(unsigned char* addr, unsigned long size)
{
    (void)addr;
    (void)size;
    VS_UNPORTED("libspu:SpuRead");
    return {};
}

void SpuGetVoiceEnvelope(int voice, short* envx)
{
    (void)voice;
    (void)envx;
    VS_UNPORTED("libspu:SpuGetVoiceEnvelope");
}

void SpuSetCommonMasterVolume(short left, short right)
{
    (void)left;
    (void)right;
    VS_UNPORTED("libspu:SpuSetCommonMasterVolume");
}

long SpuSetReverbModeType(long mode)
{
    (void)mode;
    VS_UNPORTED("libspu:SpuSetReverbModeType");
    return {};
}

void SpuSetReverbModeDepth(short left, short right)
{
    (void)left;
    (void)right;
    VS_UNPORTED("libspu:SpuSetReverbModeDepth");
}

void SpuGetReverbModeType(long* mode)
{
    (void)mode;
    VS_UNPORTED("libspu:SpuGetReverbModeType");
}

void SsUtReverbOn(void)
{
    VS_UNPORTED("libspu:SsUtReverbOn");
}

void SsUtReverbOff(void)
{
    VS_UNPORTED("libspu:SsUtReverbOff");
}

} // extern "C"
