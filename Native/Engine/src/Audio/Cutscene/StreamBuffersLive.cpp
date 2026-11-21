// Hand-maintained (tools/port/owned.csv).
// Live binding for StreamBuffers.hpp: the driver's stream state
// (g_Sound_Cutscene_StreamState, _isSpuTransfer), the bound room stream's
// bytes, the sequencer's voice scheduler, the voice output and the SPU
// transfer. The SPU raises no buffer interrupts natively (the generated copy
// dropped SpuSetIRQ, SpuSetIRQAddr and SpuSetIRQCallback), so arming and
// disarming one does nothing and no refill runs from an interrupt.
#include "Audio/StreamBuffersLive.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <span>

#include "Audio/Cutscene/Stream.hpp"
#include "Audio/Driver/SoundDriver.hpp"
#include "Audio/Mixer/VoiceMixer.hpp"
#include "Audio/SeqRecordsLive.hpp"
#include "Audio/SoundDriverLive.hpp"
#include "Audio/SoundEffectProgramsLive.hpp"
#include "Audio/StreamBuffers.hpp"
#include "Audio/StreamControl.hpp"
#include "Audio/StreamControlLive.hpp"
#include "Audio/VoiceOutputLive.hpp"
#include "Legacy/Psx.hpp"

using VS::Legacy::SpuSetTransferCallback;
using VS::Legacy::SpuSetTransferStartAddr;
using VS::Legacy::SpuWrite;

namespace VS::Audio::Cutscene {
namespace {
static_assert(std::size(VS::Audio::Mixer::kPanGains) == kPanGainCount);
static_assert(sizeof(VS::Audio::CutsceneStreamState::total_pages) ==
              sizeof(std::uint32_t));
static_assert(sizeof(VS::Audio::CutsceneStreamState::repeat_pages) ==
              sizeof(std::int32_t));
static_assert(sizeof(VS::Audio::CutsceneStreamState::voice_mask) ==
              sizeof(std::int32_t));
static_assert(sizeof(VS::Audio::CutsceneStreamState::voice_index) ==
              sizeof(std::int32_t));
static_assert(sizeof(VS::Audio::CutsceneStreamState::volume) ==
              sizeof(std::int32_t));
static_assert(sizeof(VS::Audio::CutsceneStreamState::sample_rate) ==
              sizeof(std::int32_t));
static_assert(sizeof(VS::Audio::Cutscene::legacySpuTransferBusy) ==
              sizeof(std::int32_t));
// The pan byte is byte 1 of the packed pan word.
static_assert(sizeof(VS::Audio::Cutscene::streamState.pan.bytes) ==
              sizeof(std::int32_t));
constexpr std::size_t kPanByte = 1;
constexpr unsigned kMonoOutput = 2;
// Sound_EvictSfxVoice: releases the lowest priority effect voice.
constexpr int kLowestPriorityEffect = 0x40000000;

// The offset of `cursor` in `bytes`; nullopt when it lies outside them.
std::optional<std::size_t> OffsetIn(std::span<const std::uint8_t> bytes,
                                    const void* cursor) {
  const auto base = std::bit_cast<std::uintptr_t>(bytes.data());
  const auto at = std::bit_cast<std::uintptr_t>(cursor);
  if (cursor == nullptr || bytes.empty() || at < base ||
      at - base > bytes.size()) {
    return std::nullopt;
  }
  return at - base;
}

const char* StatusReason(StreamBufferStatus status) {
  switch (status) {
    case StreamBufferStatus::kApplied:
    case StreamBufferStatus::kIdle:
      return nullptr;
    case StreamBufferStatus::kInvalidVoice:
      return "the stream voice pair is outside the SPU voices";
    case StreamBufferStatus::kChunkOutOfRange:
      return "the stream chunk lies outside the bound room stream";
  }
  return "refused";
}

void Report(const char* operation, StreamBufferStatus status) {
  if (const char* reason = StatusReason(status)) {
    Seq::ReportRefusal(operation, reason);
  }
}

class LiveStreamBuffers final : public StreamBufferHost {
 public:
  LiveStreamBuffers() : stream_(LiveBoundStreamBytes()) {}

  StreamBufferState State() override {
    const VS::Audio::CutsceneStreamState& state =
        VS::Audio::Cutscene::streamState;
    std::optional<std::size_t> chunk = OffsetIn(stream_, state.current_chunk);
    std::optional<std::size_t> repeat_chunk;
    if (state.repeat_chunk != nullptr) {
      repeat_chunk = OffsetIn(stream_, state.repeat_chunk);
      // A repeat section outside the stream detaches the stream as a whole.
      if (!repeat_chunk) chunk.reset();
    }
    return {.chunk = chunk,
            .repeat_chunk = repeat_chunk,
            .pages = state.total_pages,
            .repeat_pages = static_cast<std::uint32_t>(state.repeat_pages),
            .voice = state.voice_index,
            .voice_mask = static_cast<std::uint32_t>(state.voice_mask),
            .transfer_busy = VS::Audio::Cutscene::legacySpuTransferBusy != 0};
  }

  std::uint32_t BusyVoices() override {
    const VS::Audio::VoiceScheduler& scheduler =
        VS::Audio::Mixer::voiceScheduler;
    return scheduler.active_voices | scheduler.paused_effect_voices;
  }

  // A detached cursor is left as the core found it.
  void StoreState(const StreamBufferState& typed) override {
    VS::Audio::CutsceneStreamState& state = VS::Audio::Cutscene::streamState;
    if (typed.chunk) state.current_chunk = stream_.subspan(*typed.chunk).data();
    state.total_pages = typed.pages;
    state.voice_index = typed.voice;
    state.voice_mask = static_cast<int>(typed.voice_mask);
    VS::Audio::Cutscene::legacySpuTransferBusy = typed.transfer_busy ? 1 : 0;
  }

  void ReleaseLowestEffectVoice() override {
    SoundEffects::LiveReleaseMatchingVoices(0, kLowestPriorityEffect);
  }

  std::span<const std::uint8_t> StreamBytes() override { return stream_; }

  void SetVoiceGains(std::int32_t voice, StreamVoiceGains gains) override {
    // The 16-bit gains widen with their sign, as the original's calls did.
    Driver::SetLiveVoiceVolume(voice, static_cast<unsigned>(gains.left),
                               static_cast<unsigned>(gains.right), 0);
  }

  StreamVoiceMix VoiceMix() override {
    const VS::Audio::CutsceneStreamState& state =
        VS::Audio::Cutscene::streamState;
    return {.volume = state.volume,
            .pan = state.pan.bytes[kPanByte],
            .sample_rate = state.sample_rate,
            .mono_output = (VS::Audio::Driver::controlLatches.mix_behavior &
                            kMonoOutput) != 0};
  }

  void SetVoiceStartAddress(std::int32_t voice,
                            std::uint32_t address) override {
    Driver::LiveSetVoiceStartAddress(voice, address);
  }

  std::span<const std::int16_t, kPanGainCount> PanGains() override {
    return VS::Audio::Mixer::kPanGains;
  }

  void SetVoiceSampleRate(std::int32_t voice,
                          std::int32_t sample_rate) override {
    Driver::SetLiveVoiceSampleRate(voice, sample_rate);
  }

  void SetVoiceEnvelope(std::int32_t voice,
                        const StreamVoiceEnvelope& envelope) override {
    Driver::LiveSetVoiceAttackRateAndMode(voice, envelope.attack_step,
                                          envelope.attack_mode);
    Driver::LiveSetVoiceDecayRate(voice, envelope.decay_rate);
    Driver::LiveSetVoiceSustainLevel(voice, envelope.sustain_level);
    Driver::LiveSetVoiceSustainRateAndMode(voice, envelope.sustain_rate,
                                           envelope.sustain_direction);
    Driver::SetLiveVoiceReleaseRateAndMode(voice, envelope.release_rate,
                                           envelope.release_mode);
  }

  void SetVoiceLoopAddress(std::int32_t voice, std::uint32_t address) override {
    Driver::LiveSetVoiceLoopAddress(voice, address);
  }

  void KeyOnVoices(std::uint32_t mask) override {
    Driver::KeyOnLiveVoices(mask);
  }

  void SetTransferCompletion(StreamTransferCompletion completion) override {
    switch (completion) {
      case StreamTransferCompletion::kNone:
        SpuSetTransferCallback(nullptr);
        return;
      case StreamTransferCompletion::kBeginMonoPlayback:
        SpuSetTransferCallback(&LiveBeginMonoPlayback);
        return;
      case StreamTransferCompletion::kSampleTransfer:
        Driver::LiveArmSampleTransferCallback();
        return;
    }
  }

  void SetTransferAddress(std::uint32_t address) override {
    SpuSetTransferStartAddr(address);
  }

  // The samples come from stream_, written through its mutable view.
  void WriteSamples(std::span<const std::uint8_t> samples) override {
    const std::size_t offset =
        static_cast<std::size_t>(samples.data() - stream_.data());
    SpuWrite(stream_.subspan(offset, samples.size()).data(), samples.size());
  }

  void DisarmBufferInterrupt() override {}

  void ArmBufferInterrupt(std::optional<StreamRefill> /*next*/,
                          std::uint32_t /*address*/) override {}

 private:
  std::span<std::uint8_t> stream_;
};
}  // namespace

// decomp: SLUS_010.40 Sound_Cutscene_FindFreeVoicePair
// (src/SLUS_010.40/25AC.c:6394)
int LiveFindFreeVoicePair() {
  LiveStreamBuffers host;
  return FindFreeVoicePair(host).value_or(-1);
}

// decomp: SLUS_010.40 Sound_Cutscene_OnFirstHalfTransferred
// (src/SLUS_010.40/25AC.c:6422; was func_8001CE60)
void LiveUploadSecondMonoBuffer() {
  LiveStreamBuffers host;
  Report("upload the second mono stream buffer", UploadSecondMonoBuffer(host));
}

// decomp: SLUS_010.40 Sound_Cutscene_OnInitialTransferComplete
// (src/SLUS_010.40/25AC.c:6563)
void LiveBeginMonoPlayback() {
  LiveStreamBuffers host;
  Report("begin mono stream playback", BeginMonoPlayback(host));
}

// decomp: SLUS_010.40 Sound_Cutscene_OnInitialTransferCompleteStereo
// (src/SLUS_010.40/25AC.c:6573; was func_8001D3D4)
void LiveBeginStereoPlayback() {
  LiveStreamBuffers host;
  Report("begin stereo stream playback", BeginStereoPlayback(host));
}

// decomp: SLUS_010.40 Sound_Cutscene_OnBufferAComplete
// (src/SLUS_010.40/25AC.c:6614)
void LiveRefillMonoFirstBuffer() {
  LiveStreamBuffers host;
  Report("refill the first mono stream buffer", RefillMonoFirstBuffer(host));
}

// decomp: SLUS_010.40 Sound_Cutscene_OnBufferBComplete
// (src/SLUS_010.40/25AC.c:6620)
void LiveRefillMonoSecondBuffer() {
  LiveStreamBuffers host;
  Report("refill the second mono stream buffer", RefillMonoSecondBuffer(host));
}

// decomp: SLUS_010.40 Sound_Cutscene_OnStereoBufferAComplete
// (src/SLUS_010.40/25AC.c:6628; was func_8001D5E4)
void LiveRefillStereoFirstBuffer() {
  LiveStreamBuffers host;
  Report("refill the first stereo stream buffers",
         RefillStereoFirstBuffer(host));
}

// decomp: SLUS_010.40 Sound_Cutscene_OnStereoBufferBComplete
// (src/SLUS_010.40/25AC.c:6634; was func_8001D614)
void LiveRefillStereoSecondBuffer() {
  LiveStreamBuffers host;
  Report("refill the second stereo stream buffers",
         RefillStereoSecondBuffer(host));
}
}  // namespace VS::Audio::Cutscene
