#include "Legacy/Audio/LibSpu.hpp"

#include <cstdint>
#include <cstring>

#include "Core/Unported.hpp"

// Transitional shims (plan §2.2). Transfers act on the sample memory block
// synchronously and then report completion through the registered callback, as
// the DMA interrupt did; the rest call RecordUnported until the audio
// backend.

namespace VS::Legacy {
namespace {

alignas(16) std::uint8_t spuRegisters[kSpuRegisterBytes];
std::uint8_t spuRam[kSpuRamBytes];
std::uint32_t transferAddr = 0;
SpuTransferCallbackProc transferDone = nullptr;

void CompleteTransfer() {
  // the callback may replace itself (the sound driver's clears it)
  if (SpuTransferCallbackProc done = transferDone) {
    done();
  }
}

}  // namespace

void* SpuRegisters() { return spuRegisters; }

SpuTransferCallbackProc SpuSetTransferCallback(SpuTransferCallbackProc func) {
  SpuTransferCallbackProc previous = transferDone;
  transferDone = func;
  return previous;
}

std::uint32_t SpuSetTransferStartAddr(std::uint32_t addr) {
  // SPU addresses are in 8-byte units of the 512 KiB memory; the first 0x1010
  // bytes are reserved
  if (addr < 0x1010 || addr >= kSpuRamBytes) {
    return 0;
  }
  transferAddr = addr & ~7u;
  return transferAddr;
}

std::uint32_t SpuWrite(std::uint8_t* addr, std::uint32_t size) {
  const std::uint32_t n =
      size <= kSpuRamBytes - transferAddr ? size : kSpuRamBytes - transferAddr;
  std::memcpy(spuRam + transferAddr, addr, n);
  CompleteTransfer();
  return n;
}

std::uint32_t SpuRead(std::uint8_t* addr, std::uint32_t size) {
  const std::uint32_t n =
      size <= kSpuRamBytes - transferAddr ? size : kSpuRamBytes - transferAddr;
  std::memcpy(addr, spuRam + transferAddr, n);
  CompleteTransfer();
  return n;
}

void SpuStart() {}

std::int32_t SpuSetNoiseClock(std::int32_t n_clock) {
  (void)n_clock;
  VS::Core::RecordUnported("libspu:SpuSetNoiseClock");
  return {};
}

std::int32_t SpuSetReverb(std::int32_t on_off) {
  (void)on_off;
  VS::Core::RecordUnported("libspu:SpuSetReverb");
  return {};
}

void SpuGetVoiceEnvelope(std::int32_t voice, std::int16_t* envx) {
  (void)voice;
  (void)envx;
  VS::Core::RecordUnported("libspu:SpuGetVoiceEnvelope");
}

void SpuSetCommonMasterVolume(std::int16_t left, std::int16_t right) {
  (void)left;
  (void)right;
  VS::Core::RecordUnported("libspu:SpuSetCommonMasterVolume");
}

std::int32_t SpuSetReverbModeType(std::int32_t mode) {
  (void)mode;
  VS::Core::RecordUnported("libspu:SpuSetReverbModeType");
  return {};
}

void SpuSetReverbModeDepth(std::int16_t left, std::int16_t right) {
  (void)left;
  (void)right;
  VS::Core::RecordUnported("libspu:SpuSetReverbModeDepth");
}

void SpuGetReverbModeType(std::int32_t* mode) {
  (void)mode;
  VS::Core::RecordUnported("libspu:SpuGetReverbModeType");
}

void SsUtReverbOn() { VS::Core::RecordUnported("libspu:SsUtReverbOn"); }

void SsUtReverbOff() { VS::Core::RecordUnported("libspu:SsUtReverbOff"); }

}  // namespace VS::Legacy
