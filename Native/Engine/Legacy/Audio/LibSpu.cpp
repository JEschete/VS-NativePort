#include "Legacy/Audio/LibSpu.h"
#include "Core/Unported.h"

#include <cstring>

// Transitional shims (plan §2.2). Transfers act on the sample memory block synchronously and then report completion
// through the registered callback, as the DMA interrupt did; the rest record VS_UNPORTED until the audio backend.

namespace {

alignas(16) unsigned char g_registers[LEGACY_SPU_REGISTER_BYTES];
unsigned char g_ram[LEGACY_SPU_RAM_BYTES];
unsigned long g_transferAddr = 0;
SpuTransferCallbackProc g_transferDone = nullptr;

void CompleteTransfer()
{
    // the callback may replace itself (the sound driver's clears it)
    if (SpuTransferCallbackProc done = g_transferDone) {
        done();
    }
}

} // namespace

extern "C" {
void* LegacySpu_Registers(void)
{
    return g_registers;
}

unsigned char* LegacySpu_Ram(void)
{
    return g_ram;
}

SpuTransferCallbackProc SpuSetTransferCallback(SpuTransferCallbackProc func)
{
    SpuTransferCallbackProc previous = g_transferDone;
    g_transferDone = func;
    return previous;
}

unsigned long SpuSetTransferStartAddr(unsigned long addr)
{
    // SPU addresses are in 8-byte units of the 512 KiB memory; the first 0x1010 bytes are reserved
    if (addr < 0x1010 || addr >= LEGACY_SPU_RAM_BYTES) {
        return 0;
    }
    g_transferAddr = addr & ~7ul;
    return g_transferAddr;
}

unsigned long SpuWrite(unsigned char* addr, unsigned long size)
{
    const unsigned long n = size <= LEGACY_SPU_RAM_BYTES - g_transferAddr ? size : LEGACY_SPU_RAM_BYTES - g_transferAddr;
    std::memcpy(g_ram + g_transferAddr, addr, n);
    CompleteTransfer();
    return n;
}

unsigned long SpuRead(unsigned char* addr, unsigned long size)
{
    const unsigned long n = size <= LEGACY_SPU_RAM_BYTES - g_transferAddr ? size : LEGACY_SPU_RAM_BYTES - g_transferAddr;
    std::memcpy(addr, g_ram + g_transferAddr, n);
    CompleteTransfer();
    return n;
}

void SpuStart(void) {}

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
