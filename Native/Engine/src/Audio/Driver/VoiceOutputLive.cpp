// Hand-maintained (tools/port/owned.csv).
#include "Audio/VoiceOutputLive.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <optional>
#include <span>
#include <utility>

#include "Audio/AudioDevice.hpp"
#include "Audio/SequencerOutput.h"
#include "Audio/StreamPlayback.h"
#include "Audio/VoiceEngine.hpp"
#include "Audio/VoiceOutput.hpp"
#include "Audio/XaPlayback.h"
#include "Content/AudioBank.hpp"
#include "Content/DataStore.hpp"
#include "Core/Log.hpp"
#include "Legacy/Psx.h"
#include "Platform/Paths.hpp"

extern "C" {
#include "Audio/Cutscene/Stream.h"
#include "Audio/Driver/SoundDriver.h"
#include "Audio/Samples/SampleBanks.h"
#include "Audio/Seq/Channel.h"
}

namespace {
using VS::Audio::VoiceControls;
using VS::Audio::VoiceEngine;
namespace Driver = VS::Audio::Driver;
namespace Samples = VS::Audio::Samples;

static_assert(VoiceEngine::kVoices == Driver::kVoiceCount);

constexpr unsigned kSilentSource = 0x1010;
std::array<std::filesystem::path, Samples::kInstrumentSlotCount> g_instruments;
std::array<VoiceControls, VoiceEngine::kVoices> g_controls;
std::array<unsigned, VoiceEngine::kVoices> g_voice_instruments{};
VS::u64 g_sample_clock = 0;
VS::Audio::ReverbControls g_reverb;
bool g_mono = false;

VoiceEngine& Engine() {
  static VoiceEngine engine;
  return engine;
}

std::map<unsigned, VS::Content::AudioBank>& Banks() {
  static std::map<unsigned, VS::Content::AudioBank> banks;
  return banks;
}

VS::usize CheckedVoice(unsigned voice) {
  if (voice >= VoiceEngine::kVoices) {
    VS_LOG_ERROR("audio", "invalid sequencer voice {}", voice);
    std::abort();
  }
  return voice;
}

bool& ModeFlag(VoiceControls& controls, Driver::VoiceMode mode) {
  switch (mode) {
    case Driver::VoiceMode::kReverb:
      return controls.reverb;
    case Driver::VoiceMode::kNoise:
      return controls.noise;
    case Driver::VoiceMode::kFrequencyModulation:
      return controls.frequency_modulation;
  }
  std::abort();
}

class LiveVoiceOutput final : public Driver::VoiceOutputHost {
 public:
  void KeyOn(std::uint32_t voice) override {
    if (auto result = Engine().NoteOn(voice); !result) {
      VS_LOG_ERROR("audio", "voice {} instrument {}: {}", voice,
                   g_voice_instruments[voice], result.error());
      return;
    }
    VS_LOG_INFO("audio",
                "note-on sample={} voice={} instrument={} rate={} "
                "noise={} fm={} reverb={}",
                g_sample_clock, voice, g_voice_instruments[voice],
                g_controls[voice].rate, g_controls[voice].noise,
                g_controls[voice].frequency_modulation,
                g_controls[voice].reverb);
  }

  void KeyOff(std::uint32_t voice) override { Engine().NoteOff(voice); }

  void SetVoiceMode(std::uint32_t voice, Driver::VoiceMode mode,
                    bool enabled) override {
    auto& controls = g_controls[voice];
    bool& flag = ModeFlag(controls, mode);
    if (flag == enabled) return;
    flag = enabled;
    Engine().Configure(voice, controls);
  }

  void SetVoiceGains(std::uint32_t voice,
                     const Driver::VoiceGains& gains) override {
    auto& controls = g_controls[voice];
    controls.left = gains.left;
    controls.right = gains.right;
    Engine().Configure(voice, controls);
  }

  void SetVoiceRate(std::uint32_t voice, float rate) override {
    auto& controls = g_controls[voice];
    controls.rate = rate;
    Engine().Configure(voice, controls);
  }

  void SetVoiceRelease(std::uint32_t voice,
                       const VS::Audio::EnvelopeRate& release) override {
    g_controls[voice].envelope.release = release;
    Engine().SetRelease(voice, release);
  }
};

void ReportMask(const char* operation, const Driver::VoiceMaskResult& result) {
  if (result.ignored != 0) {
    VS_LOG_DEBUG("audio", "{}: ignored mask bits {:08x} above voice 23",
                 operation, result.ignored);
  }
}

void ReportVoice(const char* operation, Driver::VoiceStatus status, int voice,
                 int first, unsigned second) {
  switch (status) {
    case Driver::VoiceStatus::kApplied:
      return;
    case Driver::VoiceStatus::kInvalidVoice:
      VS_LOG_ERROR("audio", "{}: invalid voice {}", operation, voice);
      return;
    case Driver::VoiceStatus::kInvalidSampleRate:
      VS_LOG_ERROR("audio", "{}: invalid sample rate {} for voice {}",
                   operation, first, voice);
      return;
    case Driver::VoiceStatus::kInvalidRelease:
      VS_LOG_ERROR("audio", "{}: invalid release rate {} mode {} for voice {}",
                   operation, first, second, voice);
      return;
  }
}

class LiveSampleBanks final : public Samples::SampleBankHost {
 public:
  std::optional<std::span<const VS::Content::AudioInstrument>> LoadBank(
      std::uint32_t bank_id) override {
    const auto* store = VS::Content::DataStore::Get();
    if (store == nullptr) {
      VS_LOG_ERROR("audio", "bank {} requires an open data store", bank_id);
      return std::nullopt;
    }
    auto& banks = Banks();
    auto found = banks.find(bank_id);
    if (found == banks.end()) {
      auto bank = VS::Content::LoadAudioBank(store->Root(), bank_id);
      if (!bank) {
        VS_LOG_ERROR("audio", "{}", bank.error());
        return std::nullopt;
      }
      found = banks.emplace(bank_id, std::move(*bank)).first;
    }
    root_ = store->Root();
    return std::span<const VS::Content::AudioInstrument>(
        found->second.instruments);
  }

  void BindInstrument(std::size_t slot, const Samples::InstrumentRecord& record,
                      std::uint32_t bank_id, std::size_t index,
                      bool available) override {
    Audio_Samples_instruments[slot] = {record.sample_start, record.loop_start,
                                       record.fine_tune,    record.root_note,
                                       record.adsr_lower,   record.adsr_upper};
    g_instruments[slot] =
        available ? VS::Content::AudioInstrumentPath(root_, bank_id, index)
                  : std::filesystem::path{};
  }

 private:
  std::filesystem::path root_;
};

const char* BankUploadError(Samples::BankUploadStatus status) {
  switch (status) {
    case Samples::BankUploadStatus::kUploaded:
      return "uploaded";
    case Samples::BankUploadStatus::kTruncatedHeader:
      return "truncated header";
    case Samples::BankUploadStatus::kNotAkao:
      return "not an AKAO header";
    case Samples::BankUploadStatus::kInvalidDestination:
      return "invalid instrument destination or count";
    case Samples::BankUploadStatus::kBankUnavailable:
      return "bank assets unavailable";
    case Samples::BankUploadStatus::kCountMismatch:
      return "decoded instrument count differs from the header";
  }
  return "unknown";
}
}  // namespace

namespace VS::Audio::Driver {
void KeyOnLiveVoices(unsigned mask) {
  LiveVoiceOutput host;
  ReportMask("key-on", KeyOnVoices(host, mask));
}

void KeyOffLiveVoices(unsigned mask) {
  LiveVoiceOutput host;
  ReportMask("key-off", KeyOffVoices(host, mask));
}

void SetLiveVoiceReverbMask(unsigned mask) {
  LiveVoiceOutput host;
  ReportMask("reverb mask", SetVoiceReverbMask(host, mask));
}

void SetLiveVoiceNoiseMask(unsigned mask) {
  LiveVoiceOutput host;
  ReportMask("noise mask", SetVoiceNoiseMask(host, mask));
}

void SetLiveVoiceFrequencyModulationMask(unsigned mask) {
  LiveVoiceOutput host;
  ReportMask("fm mask", SetVoiceFrequencyModulationMask(host, mask));
}

void SetLiveVoiceVolume(int voice, unsigned left, unsigned right,
                        unsigned scale) {
  LiveVoiceOutput host;
  ReportVoice("voice volume", SetVoiceVolume(host, voice, left, right, scale),
              voice, 0, 0);
}

void SetLiveVoiceSampleRate(int voice, int sample_rate) {
  LiveVoiceOutput host;
  ReportVoice("voice sample rate", SetVoiceSampleRate(host, voice, sample_rate),
              voice, sample_rate, 0);
}

void SetLiveVoiceReleaseRateAndMode(int voice, int rate, unsigned mode) {
  LiveVoiceOutput host;
  ReportVoice("voice release",
              SetVoiceReleaseRateAndMode(host, voice, rate, mode), voice, rate,
              mode);
}
}  // namespace VS::Audio::Driver

namespace VS::Audio::Samples {
int UploadLiveBank(const void* bank, int /*wait*/, int first_instrument,
                   int sample_address) {
  if (bank == nullptr) {
    VS_LOG_ERROR("audio", "null bank source");
    Audio_Cutscene_legacySpuTransferBusy = -1;
    return -1;
  }
  // The generated loaders pass the bank's bare address; the AKAO header is
  // the only part read.
  const std::span<const std::byte> header(static_cast<const std::byte*>(bank),
                                          kBankHeaderBytes);
  LiveSampleBanks host;
  const auto result = UploadBank(host, header, first_instrument,
                                 static_cast<std::uint32_t>(sample_address));
  if (result.status != BankUploadStatus::kUploaded) {
    VS_LOG_ERROR("audio", "bank {} at instrument {}: {}", result.header.bank_id,
                 first_instrument, BankUploadError(result.status));
    Audio_Cutscene_legacySpuTransferBusy = -1;
    return -1;
  }
  Audio_Cutscene_legacySpuTransferBusy = 0;
  VS_LOG_INFO("audio", "native bank {}: instruments {}..{}",
              result.header.bank_id, first_instrument,
              first_instrument + result.header.instrument_count - 1);
  return 0;
}
}  // namespace VS::Audio::Samples

void Audio_Native_Reset(void) {
  Audio_Native_ResetCutsceneStream();
  Audio_Native_StopXaStream();
  Engine().Reset();
  g_instruments = {};
  g_controls = {};
  g_reverb = {};
  g_mono = false;
  Banks().clear();
  if (auto* device = VS::Audio::AudioDevice::Primary()) {
    device->Clear();
  }
}

void Audio_Native_ConfigureChannel(Audio_SeqChannel* channel) {
  const auto voice = CheckedVoice(channel->voice.assignedVoice);
  if (channel->instrumentIndex >= g_instruments.size()) {
    VS_LOG_ERROR("audio", "invalid instrument {}", channel->instrumentIndex);
    std::abort();
  }
  auto& controls = g_controls[voice];
  controls.instrument = g_instruments[channel->instrumentIndex];
  controls.rate = Driver::VoiceSampleRateRatio(channel->voice.sampleRate);
  controls.left = Driver::VoiceVolumeGain(Driver::VoiceVolumeRegister(
      channel->voice.volume.left, channel->voice.volumeScale));
  controls.right = Driver::VoiceVolumeGain(Driver::VoiceVolumeRegister(
      channel->voice.volume.right, channel->voice.volumeScale));
  controls.envelope = VS::Audio::DecodeAkaoEnvelope(channel->voice.adsrLower,
                                                    channel->voice.adsrUpper);
  g_voice_instruments[voice] = channel->instrumentIndex;
  controls.silent = channel->voice.sampleStart == kSilentSource;
  Engine().Configure(voice, controls);
  channel->voice.dirtyFlags = 0;
}

// decomp: SpuSetNoiseClock, SLUS_010.40/libspu/S_SNC.c.
void Audio_Native_SetNoiseClock(int clock) {
  constexpr int kMaximumClock = 63;
  Engine().SetNoiseClock(
      static_cast<unsigned>(std::clamp(clock, 0, kMaximumClock)));
}

// decomp: SLUS_010.40/libspu/S_SRMT.c, S_SRMD.c, S_SR.c and S_GRMT.c.
int Audio_Native_ReverbMode(void) { return static_cast<int>(g_reverb.preset); }

void Audio_Native_SetReverbMode(int mode) {
  if (mode < 0 || mode > static_cast<int>(VS::Audio::ReverbPreset::kPipe)) {
    VS_LOG_ERROR("audio", "invalid reverb preset {}", mode);
    std::abort();
  }
  g_reverb.preset = static_cast<VS::Audio::ReverbPreset>(mode);
  g_reverb.left = g_reverb.right = 0;
  Engine().SetReverb(g_reverb, true);
  VS_LOG_INFO("audio", "reverb preset {}", mode);
}

void Audio_Native_SetReverbDepth(int16_t left, int16_t right) {
  constexpr float kDepthUnit = 32768;
  g_reverb.left = static_cast<float>(left) / kDepthUnit;
  g_reverb.right = static_cast<float>(right) / kDepthUnit;
  Engine().SetReverb(g_reverb);
}

void Audio_Native_EnableReverb(int enabled) {
  if (enabled != 0 && enabled != 1) {
    VS_LOG_ERROR("audio", "invalid reverb enable {}", enabled);
    std::abort();
  }
  g_reverb.enabled = enabled != 0;
  Engine().SetReverb(g_reverb);
}

int16_t Audio_Native_Envelope(unsigned voice) {
  return static_cast<int16_t>(Engine().EnvelopeLevel(CheckedVoice(voice)));
}

int Audio_Native_StartStreamVoice(unsigned voice, const char* instrument,
                                  float rate, unsigned left, unsigned right,
                                  int reverb) {
  auto& controls = g_controls[CheckedVoice(voice)];
  controls = {};
  controls.instrument = VS::Platform::Utf8Path(instrument);
  controls.rate = rate;
  controls.left = Driver::VoiceVolumeGain(Driver::VoiceVolumeRegister(left, 0));
  controls.right =
      Driver::VoiceVolumeGain(Driver::VoiceVolumeRegister(right, 0));
  controls.reverb = reverb != 0;
  // Sound_Cutscene_InitVoice, SLUS_010.40/25AC.c:6358-6402.
  constexpr VS::u16 kAttackDecay = 0x00FF;
  constexpr VS::u16 kSustainRelease = 0x1FC6;
  controls.envelope =
      VS::Audio::DecodeAkaoEnvelope(kAttackDecay, kSustainRelease);
  Engine().Configure(voice, controls);
  if (auto result = Engine().NoteOn(voice); !result) {
    VS_LOG_ERROR("audio", "stream voice {}: {}", voice, result.error());
    return 0;
  }
  VS_LOG_INFO("audio", "stream-note-on sample={} voice={} asset={}",
              g_sample_clock, voice, instrument);
  return 1;
}

void Audio_Native_PauseVoice(unsigned voice, int paused) {
  auto& controls = g_controls[CheckedVoice(voice)];
  controls.paused = paused != 0;
  Engine().Configure(voice, controls);
}

int Audio_Native_VoiceActive(unsigned voice) {
  return Engine().IsActive(CheckedVoice(voice)) ? 1 : 0;
}

void Audio_Native_Tick(void) {
  Audio_Native_UpdateXaControls();
  constexpr unsigned kMonoOutput = 2;
  const bool mono =
      (Audio_Driver_controlLatches.mixBehavior & kMonoOutput) != 0;
  if (mono != g_mono) {
    Engine().SetMonoOutput(mono);
    g_mono = mono;
    VS_LOG_INFO("audio", "native output mode: {}", mono ? "mono" : "stereo");
  }
  Engine().CommitTick();
  g_sample_clock += VoiceEngine::kFramesPerTick;
  auto* device = VS::Audio::AudioDevice::Primary();
  if (device && device->IsRealtime()) {
    if (auto result = device->RenderFrom(Engine()); !result) {
      VS_LOG_ERROR("audio", "starting native renderer: {}", result.error());
      std::abort();
    }
    device->Pump();
  } else {
    std::array<float, VoiceEngine::kFramesPerTick * 2> frames{};
    Engine().Render(frames);
    if (device) device->Submit(frames);
  }
}
