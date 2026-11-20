#pragma once

// libspu/libsnd surface (plan §6.3, strategy T): voice and reverb control
// translated into Engine\Audio.

#include <cstddef>
#include <cstdint>

namespace VS::Legacy {

// SpuSetReverb and friends' switch values (SPU_OFF, SPU_ON).
inline constexpr std::int32_t kSpuOff = 0;
inline constexpr std::int32_t kSpuOn = 1;
// Bytes of one SpuInitMalloc allocation record (SPU_MALLOC_RECSIZ).
inline constexpr std::size_t kSpuMallocRecordSize = 8;

using SpuTransferCallbackProc = void (*)();

// The SPU's register file (0x1F801C00-0x1F801DFF) as ordinary memory: the
// copied code's register literals map here, and the audio backend reads the
// voice state the sound driver writes.
inline constexpr std::size_t kSpuRegisterBytes = 0x200;
void* SpuRegisters();
// The SPU's 512 KiB sample memory, filled by SpuWrite.
inline constexpr std::uint32_t kSpuRamBytes = 0x80000;

SpuTransferCallbackProc SpuSetTransferCallback(SpuTransferCallbackProc func);
std::uint32_t SpuSetTransferStartAddr(std::uint32_t addr);

struct SpuVolume {
  std::int16_t left;
  std::int16_t right;
};

void SpuStart();
std::int32_t SpuSetNoiseClock(std::int32_t n_clock);
std::int32_t SpuSetReverb(std::int32_t on_off);
std::uint32_t SpuWrite(std::uint8_t* addr, std::uint32_t size);
std::uint32_t SpuRead(std::uint8_t* addr, std::uint32_t size);
void SpuGetVoiceEnvelope(std::int32_t voice, std::int16_t* envx);
void SpuSetCommonMasterVolume(std::int16_t left, std::int16_t right);
std::int32_t SpuSetReverbModeType(std::int32_t mode);
void SpuSetReverbModeDepth(std::int16_t left, std::int16_t right);
void SpuGetReverbModeType(std::int32_t* mode);
void SsUtReverbOn();
void SsUtReverbOff();

}  // namespace VS::Legacy
