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
#include "Audio/Cutscene/Stream.hpp"
#include "Audio/Driver/SoundDriver.hpp"
#include "Audio/Samples/SampleBanks.hpp"
#include "Audio/Seq/Channel.hpp"
#include "Audio/StreamPlayback.hpp"
#include "Audio/VoiceEngine.hpp"
#include "Audio/VoiceOutput.hpp"
#include "Audio/XaPlayback.hpp"
#include "Content/AudioBank.hpp"
#include "Content/DataStore.hpp"
#include "Core/Log.hpp"
#include "Legacy/Psx.hpp"
#include "Platform/Paths.hpp"

namespace {
using VS::Audio::VoiceControls;
using VS::Audio::VoiceEngine;
namespace Driver = VS::Audio::Driver;
namespace Samples = VS::Audio::Samples;

static_assert(VoiceEngine::kVoices == Driver::kVoiceCount);

constexpr unsigned kSilentSource = 0x1010;
constexpr std::size_t kInstrumentSlots = Samples::kInstrumentSlotCount;
std::array<std::filesystem::path, kInstrumentSlots> instrumentPaths;
std::array<VoiceControls, VoiceEngine::kVoices> voiceControls;
std::array<unsigned, VoiceEngine::kVoices> voiceInstruments{};
VS::u64 sampleClock = 0;
VS::Audio::ReverbControls reverbControls;
bool monoOutput = false;

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
    VS::Core::LogError("audio", "invalid sequencer voice {}", voice);
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
      VS::Core::LogError("audio", "voice {} instrument {}: {}", voice,
                         voiceInstruments[voice], result.error());
      return;
    }
    VS::Core::LogInfo("audio",
                      "note-on sample={} voice={} instrument={} rate={} "
                      "noise={} fm={} reverb={}",
                      sampleClock, voice, voiceInstruments[voice],
                      voiceControls[voice].rate, voiceControls[voice].noise,
                      voiceControls[voice].frequency_modulation,
                      voiceControls[voice].reverb);
  }

  void KeyOff(std::uint32_t voice) override { Engine().NoteOff(voice); }

  void SetVoiceMode(std::uint32_t voice, Driver::VoiceMode mode,
                    bool enabled) override {
    auto& controls = voiceControls[voice];
    bool& flag = ModeFlag(controls, mode);
    if (flag == enabled) return;
    flag = enabled;
    Engine().Configure(voice, controls);
  }

  void SetVoiceGains(std::uint32_t voice,
                     const Driver::VoiceGains& gains) override {
    auto& controls = voiceControls[voice];
    controls.left = gains.left;
    controls.right = gains.right;
    Engine().Configure(voice, controls);
  }

  void SetVoiceRate(std::uint32_t voice, float rate) override {
    auto& controls = voiceControls[voice];
    controls.rate = rate;
    Engine().Configure(voice, controls);
  }

  void SetVoiceRelease(std::uint32_t voice,
                       const VS::Audio::EnvelopeRate& release) override {
    voiceControls[voice].envelope.release = release;
    Engine().SetRelease(voice, release);
  }
};

void ReportMask(const char* operation, const Driver::VoiceMaskResult& result) {
  if (result.ignored != 0) {
    VS::Core::LogDebug("audio", "{}: ignored mask bits {:08x} above voice 23",
                       operation, result.ignored);
  }
}

void ReportVoice(const char* operation, Driver::VoiceStatus status, int voice,
                 int first, unsigned second) {
  switch (status) {
    case Driver::VoiceStatus::kApplied:
      return;
    case Driver::VoiceStatus::kInvalidVoice:
      VS::Core::LogError("audio", "{}: invalid voice {}", operation, voice);
      return;
    case Driver::VoiceStatus::kInvalidSampleRate:
      VS::Core::LogError("audio", "{}: invalid sample rate {} for voice {}",
                         operation, first, voice);
      return;
    case Driver::VoiceStatus::kInvalidRelease:
      VS::Core::LogError("audio",
                         "{}: invalid release rate {} mode {} for voice {}",
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
      VS::Core::LogError("audio", "bank {} requires an open data store",
                         bank_id);
      return std::nullopt;
    }
    auto& banks = Banks();
    auto found = banks.find(bank_id);
    if (found == banks.end()) {
      auto bank = VS::Content::LoadAudioBank(store->Root(), bank_id);
      if (!bank) {
        VS::Core::LogError("audio", "{}", bank.error());
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
    VS::Audio::Samples::instruments[slot] = {
        record.sample_start, record.loop_start, record.fine_tune,
        record.root_note,    record.adsr_lower, record.adsr_upper};
    instrumentPaths[slot] =
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
    VS::Core::LogError("audio", "null bank source");
    VS::Audio::Cutscene::legacySpuTransferBusy = -1;
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
    VS::Core::LogError("audio", "bank {} at instrument {}: {}",
                       result.header.bank_id, first_instrument,
                       BankUploadError(result.status));
    VS::Audio::Cutscene::legacySpuTransferBusy = -1;
    return -1;
  }
  VS::Audio::Cutscene::legacySpuTransferBusy = 0;
  VS::Core::LogInfo("audio", "native bank {}: instruments {}..{}",
                    result.header.bank_id, first_instrument,
                    first_instrument + result.header.instrument_count - 1);
  return 0;
}
}  // namespace VS::Audio::Samples

void VS::Audio::Driver::ResetVoiceOutput() {
  VS::Audio::ResetCutsceneStream();
  VS::Audio::StopXaStream();
  Engine().Reset();
  instrumentPaths = {};
  voiceControls = {};
  reverbControls = {};
  monoOutput = false;
  Banks().clear();
  if (auto* device = VS::Audio::AudioDevice::Primary()) {
    device->Clear();
  }
}

void VS::Audio::Driver::ConfigureOutputChannel(VS::Audio::SeqChannel* channel) {
  const auto voice = CheckedVoice(channel->voice.assigned_voice);
  if (channel->instrument_index >= instrumentPaths.size()) {
    VS::Core::LogError("audio", "invalid instrument {}",
                       channel->instrument_index);
    std::abort();
  }
  auto& controls = voiceControls[voice];
  controls.instrument = instrumentPaths[channel->instrument_index];
  controls.rate = Driver::VoiceSampleRateRatio(channel->voice.sample_rate);
  controls.left = Driver::VoiceVolumeGain(Driver::VoiceVolumeRegister(
      channel->voice.volume.left, channel->voice.volume_scale));
  controls.right = Driver::VoiceVolumeGain(Driver::VoiceVolumeRegister(
      channel->voice.volume.right, channel->voice.volume_scale));
  controls.envelope = VS::Audio::DecodeAkaoEnvelope(channel->voice.adsr_lower,
                                                    channel->voice.adsr_upper);
  voiceInstruments[voice] = channel->instrument_index;
  controls.silent = channel->voice.sample_start == kSilentSource;
  Engine().Configure(voice, controls);
  channel->voice.dirty_flags = 0;
}

// decomp: SpuSetNoiseClock, SLUS_010.40/libspu/S_SNC.c.
void VS::Audio::Driver::SetOutputNoiseClock(int clock) {
  constexpr int kMaximumClock = 63;
  Engine().SetNoiseClock(
      static_cast<unsigned>(std::clamp(clock, 0, kMaximumClock)));
}

// decomp: SLUS_010.40/libspu/S_SRMT.c, S_SRMD.c, S_SR.c and S_GRMT.c.
int VS::Audio::Driver::OutputReverbMode() {
  return static_cast<int>(reverbControls.preset);
}

void VS::Audio::Driver::SetOutputReverbMode(int mode) {
  if (mode < 0 || mode > static_cast<int>(VS::Audio::ReverbPreset::kPipe)) {
    VS::Core::LogError("audio", "invalid reverb preset {}", mode);
    std::abort();
  }
  reverbControls.preset = static_cast<VS::Audio::ReverbPreset>(mode);
  reverbControls.left = reverbControls.right = 0;
  Engine().SetReverb(reverbControls, true);
  VS::Core::LogInfo("audio", "reverb preset {}", mode);
}

void VS::Audio::Driver::SetOutputReverbDepth(std::int16_t left,
                                             std::int16_t right) {
  constexpr float kDepthUnit = 32768;
  reverbControls.left = static_cast<float>(left) / kDepthUnit;
  reverbControls.right = static_cast<float>(right) / kDepthUnit;
  Engine().SetReverb(reverbControls);
}

void VS::Audio::Driver::EnableOutputReverb(int enabled) {
  if (enabled != 0 && enabled != 1) {
    VS::Core::LogError("audio", "invalid reverb enable {}", enabled);
    std::abort();
  }
  reverbControls.enabled = enabled != 0;
  Engine().SetReverb(reverbControls);
}

std::int16_t VS::Audio::Driver::OutputVoiceEnvelope(unsigned voice) {
  return static_cast<int16_t>(Engine().EnvelopeLevel(CheckedVoice(voice)));
}

int VS::Audio::Driver::StartOutputStreamVoice(unsigned voice,
                                              const char* instrument,
                                              float rate, unsigned left,
                                              unsigned right, int reverb) {
  auto& controls = voiceControls[CheckedVoice(voice)];
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
    VS::Core::LogError("audio", "stream voice {}: {}", voice, result.error());
    return 0;
  }
  VS::Core::LogInfo("audio", "stream-note-on sample={} voice={} asset={}",
                    sampleClock, voice, instrument);
  return 1;
}

void VS::Audio::Driver::PauseOutputVoice(unsigned voice, int paused) {
  auto& controls = voiceControls[CheckedVoice(voice)];
  controls.paused = paused != 0;
  Engine().Configure(voice, controls);
}

int VS::Audio::Driver::OutputVoiceActive(unsigned voice) {
  return Engine().IsActive(CheckedVoice(voice)) ? 1 : 0;
}

void VS::Audio::Driver::TickVoiceOutput() {
  VS::Audio::UpdateXaControls();
  constexpr unsigned kMonoOutput = 2;
  const bool mono =
      (VS::Audio::Driver::controlLatches.mix_behavior & kMonoOutput) != 0;
  if (mono != monoOutput) {
    Engine().SetMonoOutput(mono);
    monoOutput = mono;
    VS::Core::LogInfo("audio", "native output mode: {}",
                      mono ? "mono" : "stereo");
  }
  Engine().CommitTick();
  sampleClock += VoiceEngine::kFramesPerTick;
  auto* device = VS::Audio::AudioDevice::Primary();
  if (device && device->IsRealtime()) {
    if (auto result = device->RenderFrom(Engine()); !result) {
      VS::Core::LogError("audio", "starting native renderer: {}",
                         result.error());
      std::abort();
    }
    device->Pump();
  } else {
    std::array<float, VoiceEngine::kFramesPerTick * 2> frames{};
    Engine().Render(frames);
    if (device) device->Submit(frames);
  }
}
