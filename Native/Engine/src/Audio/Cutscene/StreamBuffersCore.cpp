// Hand-maintained (tools/port/owned.csv).
// Typed cutscene stream buffering (StreamBuffers.hpp).
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <span>

#include "Audio/StreamBuffers.hpp"
#include "Audio/StreamControl.hpp"

namespace VS::Audio::Cutscene {
namespace {
// Sound_Cutscene_FindFreeVoicePair walks the pairs down from voices 22-23
// (mask bits 22 and 23) to voices 12-13, as a step from 12 to 2 plus 10.
constexpr std::uint32_t kHighestPairBits = 0x00C00000;
constexpr std::int32_t kHighestPairStep = 12;
constexpr std::int32_t kPairStepVoice = 10;
constexpr std::int32_t kPairStep = 2;
constexpr unsigned kPairShift = 2;
constexpr std::size_t kCenterPan = 128;
constexpr unsigned kPanMirror = 0xFF;
constexpr unsigned kGainShift = 16;
// The page counts BeginPlayback and LoadNextBuffer take off per buffer.
constexpr std::uint32_t kPlaybackPages = 0x1000;
constexpr std::uint32_t kRefillPages = 0x800;

bool ValidVoice(std::int32_t voice) {
  return voice >= 0 && voice < kStreamVoiceCount;
}

bool ValidPair(std::int32_t voice) {
  return voice >= 0 && voice <= kLastPairVoice;
}

// Whether `bytes` bytes from `chunk` lie within the stream.
bool ChunkFits(StreamBufferHost& host, std::optional<std::size_t> chunk,
               std::size_t bytes) {
  const std::size_t size = host.StreamBytes().size();
  return chunk && *chunk <= size && size - *chunk >= bytes;
}

// The high half of the original's 32-bit product of the volume and a Q15
// gain, as the 16-bit volume register takes it.
std::int16_t ScaledGain(std::int32_t volume, std::int16_t gain) {
  const std::uint32_t product =
      static_cast<std::uint32_t>(volume) *
      static_cast<std::uint32_t>(static_cast<std::int32_t>(gain));
  return static_cast<std::int16_t>(product >> kGainShift);
}

StreamVoiceGains VoiceGains(
    const StreamVoiceMix& mix, StreamPanMode mode,
    std::span<const std::int16_t, kPanGainCount> pan_gains) {
  if (mix.mono_output) {
    const std::int16_t centered = ScaledGain(mix.volume, pan_gains[kCenterPan]);
    return {.left = centered, .right = centered};
  }
  // Half the volume, as the 16-bit register keeps it.
  const auto half =
      static_cast<std::int16_t>(static_cast<std::uint32_t>(mix.volume) >> 1);
  switch (mode) {
    case StreamPanMode::kLeft:
      return {.left = half, .right = 0};
    case StreamPanMode::kRight:
      return {.left = 0, .right = half};
    case StreamPanMode::kCentered: {
      // Three quarters of the sign-extended half volume.
      const std::int32_t wide = half;
      const auto level = static_cast<std::int16_t>((wide >> 1) + (wide >> 2));
      return {.left = level, .right = level};
    }
    case StreamPanMode::kPanned:
      break;
  }
  return {.left = ScaledGain(mix.volume, pan_gains[mix.pan]),
          .right = ScaledGain(mix.volume, pan_gains[mix.pan ^ kPanMirror])};
}
}  // namespace

// decomp: SLUS_010.40 Sound_Cutscene_FindFreeVoicePair
// (src/SLUS_010.40/25AC.c:6254)
std::optional<std::int32_t> FindFreeVoicePair(StreamBufferHost& host) {
  std::uint32_t busy = 0;
  do {
    std::uint32_t bits = kHighestPairBits;
    std::int32_t step = kHighestPairStep;
    busy = host.BusyVoices();
    while (step != 0 && (busy & bits) != 0) {
      step -= kPairStep;
      bits >>= kPairShift;
    }
    if (step != 0) return step + kPairStepVoice;
    host.ReleaseLowestEffectVoice();
  } while (busy != host.BusyVoices());
  return std::nullopt;
}

// decomp: SLUS_010.40 func_8001CE60 (src/SLUS_010.40/25AC.c:6284)
StreamBufferStatus UploadSecondMonoBuffer(StreamBufferHost& host) {
  const StreamBufferState state = host.State();
  if (!ChunkFits(host, state.chunk, std::size_t{2} * kMonoBufferBytes)) {
    return StreamBufferStatus::kChunkOutOfRange;
  }
  host.SetTransferAddress(kSecondBufferAddress);
  host.SetTransferCompletion(StreamTransferCompletion::kBeginMonoPlayback);
  host.WriteSamples(host.StreamBytes().subspan(*state.chunk + kMonoBufferBytes,
                                               kMonoBufferBytes));
  return StreamBufferStatus::kApplied;
}

// decomp: SLUS_010.40 Sound_Cutscene_InitVoice (src/SLUS_010.40/25AC.c:6358)
StreamBufferStatus InitializeStreamVoice(StreamBufferHost& host,
                                         const StreamVoiceSetup& setup) {
  if (!ValidVoice(setup.voice)) return StreamBufferStatus::kInvalidVoice;
  const StreamVoiceMix mix = host.VoiceMix();
  host.SetVoiceGains(setup.voice, VoiceGains(mix, setup.mode, host.PanGains()));
  host.SetVoiceSampleRate(setup.voice, mix.sample_rate);
  host.SetVoiceStartAddress(setup.voice, setup.start_address);
  host.SetVoiceLoopAddress(setup.voice, setup.loop_address);
  host.SetVoiceEnvelope(setup.voice, StreamVoiceEnvelope{});
  return StreamBufferStatus::kApplied;
}

// decomp: SLUS_010.40 Sound_Cutscene_BeginPlayback
// (src/SLUS_010.40/25AC.c:6404)
StreamBufferStatus BeginStreamPlayback(StreamBufferHost& host,
                                       const StreamPlaybackStart& start) {
  StreamBufferState state = host.State();
  if (state.voice_mask == 0) return StreamBufferStatus::kIdle;
  const bool more = state.pages > kPlaybackPages;
  if (more && !ChunkFits(host, state.chunk, start.advance)) {
    return StreamBufferStatus::kChunkOutOfRange;
  }
  host.SetTransferCompletion(StreamTransferCompletion::kNone);
  state.transfer_busy = false;
  std::optional<StreamRefill> next = start.next;
  std::uint32_t interrupt_address = start.interrupt_address;
  if (more) {
    state.pages -= kPlaybackPages;
    *state.chunk += start.advance;
  } else {
    next = std::nullopt;
    interrupt_address = kSilentLoopAddress;
    state.pages = 0;
  }
  host.StoreState(state);
  host.KeyOnVoices(state.voice_mask);
  host.ArmBufferInterrupt(next, interrupt_address + kBufferInterruptOffset);
  return StreamBufferStatus::kApplied;
}

// decomp: SLUS_010.40 Sound_Cutscene_OnInitialTransferComplete
// (src/SLUS_010.40/25AC.c:6426)
StreamBufferStatus BeginMonoPlayback(StreamBufferHost& host) {
  const std::int32_t voice = host.State().voice;
  if (!ValidPair(voice)) return StreamBufferStatus::kInvalidVoice;
  for (const std::int32_t pair_voice : {voice, voice + 1}) {
    InitializeStreamVoice(host, {.voice = pair_voice,
                                 .mode = StreamPanMode::kPanned,
                                 .start_address = kFirstBufferAddress,
                                 .loop_address = kSecondBufferAddress});
  }
  return BeginStreamPlayback(host, {.advance = 2 * kMonoBufferBytes,
                                    .interrupt_address = kSecondBufferAddress,
                                    .next = StreamRefill::kMonoFirst});
}

// decomp: SLUS_010.40 func_8001D3D4 (src/SLUS_010.40/25AC.c:6436)
StreamBufferStatus BeginStereoPlayback(StreamBufferHost& host) {
  const std::int32_t voice = host.State().voice;
  if (!ValidPair(voice)) return StreamBufferStatus::kInvalidVoice;
  InitializeStreamVoice(host, {.voice = voice,
                               .mode = StreamPanMode::kLeft,
                               .start_address = kFirstBufferAddress,
                               .loop_address = kSecondBufferAddress});
  InitializeStreamVoice(host, {.voice = voice + 1,
                               .mode = StreamPanMode::kRight,
                               .start_address = kFirstRightBufferAddress,
                               .loop_address = kSecondRightBufferAddress});
  return BeginStreamPlayback(host, {.advance = 2 * kStereoBufferBytes,
                                    .interrupt_address = kSecondBufferAddress,
                                    .next = StreamRefill::kStereoFirst});
}

// decomp: SLUS_010.40 Sound_Cutscene_LoadNextBuffer
// (src/SLUS_010.40/25AC.c:6444)
StreamBufferStatus RefillStreamBuffer(StreamBufferHost& host,
                                      const StreamRefillBuffers& refill) {
  StreamBufferState state = host.State();
  if (state.voice_mask == 0 || state.pages == 0) {
    return StreamBufferStatus::kIdle;
  }
  if (!ValidPair(state.voice)) return StreamBufferStatus::kInvalidVoice;
  if (!ChunkFits(host, state.chunk, refill.bytes)) {
    return StreamBufferStatus::kChunkOutOfRange;
  }
  host.SetTransferAddress(refill.left_address);
  host.SetTransferCompletion(StreamTransferCompletion::kSampleTransfer);
  host.WriteSamples(host.StreamBytes().subspan(*state.chunk, refill.bytes));
  host.DisarmBufferInterrupt();
  std::optional<StreamRefill> next = refill.next;
  std::uint32_t left = refill.left_address;
  std::uint32_t right = refill.right_address;
  if (state.pages > kRefillPages) {
    state.pages -= kRefillPages;
    *state.chunk += refill.bytes;
  } else if (state.repeat_chunk) {
    state.chunk = state.repeat_chunk;
    state.pages = state.repeat_pages;
  } else {
    next = std::nullopt;
    left = kSilentLoopAddress;
    right = kSilentLoopAddress;
    state.pages = 0;
  }
  host.StoreState(state);
  host.SetVoiceLoopAddress(state.voice, left);
  host.SetVoiceLoopAddress(state.voice + 1, right);
  host.ArmBufferInterrupt(next, left + kBufferInterruptOffset);
  return StreamBufferStatus::kApplied;
}

// decomp: SLUS_010.40 Sound_Cutscene_OnBufferAComplete
// (src/SLUS_010.40/25AC.c:6477)
StreamBufferStatus RefillMonoFirstBuffer(StreamBufferHost& host) {
  return RefillStreamBuffer(host, {.left_address = kFirstBufferAddress,
                                   .right_address = kFirstBufferAddress,
                                   .bytes = kMonoBufferBytes,
                                   .next = StreamRefill::kMonoSecond});
}

// decomp: SLUS_010.40 Sound_Cutscene_OnBufferBComplete
// (src/SLUS_010.40/25AC.c:6483)
StreamBufferStatus RefillMonoSecondBuffer(StreamBufferHost& host) {
  return RefillStreamBuffer(host, {.left_address = kSecondBufferAddress,
                                   .right_address = kSecondBufferAddress,
                                   .bytes = kMonoBufferBytes,
                                   .next = StreamRefill::kMonoFirst});
}

// decomp: SLUS_010.40 func_8001D5E4 (src/SLUS_010.40/25AC.c:6491)
StreamBufferStatus RefillStereoFirstBuffer(StreamBufferHost& host) {
  return RefillStreamBuffer(host, {.left_address = kFirstBufferAddress,
                                   .right_address = kFirstRightBufferAddress,
                                   .bytes = kStereoBufferBytes,
                                   .next = StreamRefill::kStereoSecond});
}

// decomp: SLUS_010.40 func_8001D614 (src/SLUS_010.40/25AC.c:6496)
StreamBufferStatus RefillStereoSecondBuffer(StreamBufferHost& host) {
  return RefillStreamBuffer(host, {.left_address = kSecondBufferAddress,
                                   .right_address = kSecondRightBufferAddress,
                                   .bytes = kStereoBufferBytes,
                                   .next = StreamRefill::kStereoFirst});
}
}  // namespace VS::Audio::Cutscene
