#include "Audio/SequencerOutput.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <map>
#include <utility>

#include "Audio/AudioDevice.hpp"
#include "Audio/StreamPlayback.h"
#include "Audio/VoiceEngine.hpp"
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
constexpr unsigned kInstrumentCount = 256;
constexpr int kUnityPitch = 4096;
constexpr int kMaximumPitch = 16384;
constexpr unsigned kVolumeMask = 0x7FFF;
constexpr unsigned kVolumeSign = 0x4000;
constexpr int kVolumeModulus = 0x8000;
constexpr unsigned kSilentSource = 0x1010;
constexpr int kVolumeScaleBits = 7;
std::array<std::filesystem::path, kInstrumentCount> g_instruments;
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

float Gain(unsigned value, unsigned scale) {
  if (scale != 0) {
    value = (value * scale) >> kVolumeScaleBits;
  }
  value &= kVolumeMask;
  const int signed_value = (value & kVolumeSign) != 0
                               ? static_cast<int>(value) - kVolumeModulus
                               : static_cast<int>(value);
  return static_cast<float>(signed_value) / kVolumeSign;
}

void SetModeMask(unsigned voices, bool VoiceControls::* mode) {
  for (unsigned voice = 0; voice < VoiceEngine::kVoices; ++voice) {
    auto& controls = g_controls[voice];
    const bool enabled = (voices & (1U << voice)) != 0;
    if (controls.*mode == enabled) continue;
    controls.*mode = enabled;
    Engine().Configure(voice, controls);
  }
}
}  // namespace

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

// decomp: func_800135D8, SLUS_010.40/25AC.c:1033. The copied loader
// chooses the table range; decoded asset ownership replaces its SPU transfer.
int Audio_Samples_UploadBank(void* data, int /*wait*/, int first_instrument,
                             int source_destination) {
  constexpr unsigned kAkaoMagic = 0x4F414B41;
  constexpr VS::usize kHeaderWords = 8;
  constexpr VS::usize kInstrumentCountWord = 7;
  std::array<unsigned, kHeaderWords> header{};
  if (data == nullptr) {
    VS_LOG_ERROR("audio", "null bank source");
    return -1;
  }
  std::memcpy(header.data(), data, sizeof(header));
  const auto* store = VS::Content::DataStore::Get();
  if (header[0] != kAkaoMagic || store == nullptr) {
    VS_LOG_ERROR("audio",
                 "bank requires an AKAO header and an open data store");
    return -1;
  }
  const unsigned bank_id = header[1];
  auto& banks = Banks();
  auto found = banks.find(bank_id);
  if (found == banks.end()) {
    auto bank = VS::Content::LoadAudioBank(store->Root(), bank_id);
    if (!bank) {
      VS_LOG_ERROR("audio", "{}", bank.error());
      Audio_Cutscene_legacySpuTransferBusy = -1;
      return -1;
    }
    found = banks.emplace(bank_id, std::move(*bank)).first;
  }
  const auto& bank = found->second;
  if (first_instrument < 0 ||
      static_cast<unsigned>(first_instrument) >= kInstrumentCount ||
      bank.instruments.size() > kInstrumentCount - first_instrument ||
      bank.instruments.size() != header[kInstrumentCountWord]) {
    VS_LOG_ERROR("audio", "bank {} has an invalid destination/count", bank_id);
    return -1;
  }
  for (VS::usize index = 0; index < bank.instruments.size(); ++index) {
    const auto& source = bank.instruments[index];
    auto& destination = Audio_Samples_instruments[first_instrument + index];
    destination = {
        source.sample_offset + static_cast<unsigned>(source_destination),
        source.loop_offset + static_cast<unsigned>(source_destination),
        source.fine_tune,
        source.root_note,
        source.adsr_lower,
        source.adsr_upper};
    g_instruments[first_instrument + index] =
        source.available
            ? VS::Content::AudioInstrumentPath(store->Root(), bank_id, index)
            : std::filesystem::path{};
  }
  Audio_Cutscene_legacySpuTransferBusy = 0;
  VS_LOG_INFO("audio", "native bank {}: instruments {}..{}", bank_id,
              first_instrument, first_instrument + bank.instruments.size() - 1);
  return 0;
}

void Audio_Native_ConfigureChannel(Audio_SeqChannel* channel) {
  const auto voice = CheckedVoice(channel->voice.assignedVoice);
  if (channel->instrumentIndex >= g_instruments.size()) {
    VS_LOG_ERROR("audio", "invalid instrument {}", channel->instrumentIndex);
    std::abort();
  }
  auto& controls = g_controls[voice];
  controls.instrument = g_instruments[channel->instrumentIndex];
  controls.rate = static_cast<float>(
                      std::min<int>(channel->voice.sampleRate, kMaximumPitch)) /
                  kUnityPitch;
  controls.left = Gain(channel->voice.volume.left, channel->voice.volumeScale);
  controls.right =
      Gain(channel->voice.volume.right, channel->voice.volumeScale);
  controls.envelope = VS::Audio::DecodeAkaoEnvelope(channel->voice.adsrLower,
                                                    channel->voice.adsrUpper);
  g_voice_instruments[voice] = channel->instrumentIndex;
  controls.silent = channel->voice.sampleStart == kSilentSource;
  Engine().Configure(voice, controls);
  channel->voice.dirtyFlags = 0;
}

// decomp: SetVoiceKeyOn/SetVoiceKeyOff, SLUS_010.40/25AC.c:1102-1112.
void Audio_Driver_KeyOnVoices(unsigned voices) {
  for (unsigned voice = 0; voice < VoiceEngine::kVoices; ++voice) {
    if ((voices & (1U << voice)) == 0) continue;
    if (auto result = Engine().NoteOn(voice); !result) {
      VS_LOG_ERROR("audio", "voice {} instrument {}: {}", voice,
                   g_voice_instruments[voice], result.error());
    } else {
      VS_LOG_INFO("audio",
                  "note-on sample={} voice={} instrument={} rate={} "
                  "noise={} fm={} reverb={}",
                  g_sample_clock, voice, g_voice_instruments[voice],
                  g_controls[voice].rate, g_controls[voice].noise,
                  g_controls[voice].frequency_modulation,
                  g_controls[voice].reverb);
    }
  }
}

void Audio_Driver_KeyOffVoices(unsigned voices) {
  for (unsigned voice = 0; voice < VoiceEngine::kVoices; ++voice) {
    if ((voices & (1U << voice)) != 0) {
      Engine().NoteOff(voice);
    }
  }
}

// decomp: SetVoiceAdsrReleaseRateAndMode, SLUS_010.40/25AC.c:1303.
void Audio_Driver_SetVoiceReleaseRateAndMode(int voice, int rate,
                                             unsigned mode) {
  constexpr unsigned kReleaseModeBit = 5;
  const auto envelope = VS::Audio::DecodeAkaoEnvelope(
      0, static_cast<VS::u16>((rate & 31) | ((mode >> 2) << kReleaseModeBit)));
  const auto index = CheckedVoice(voice);
  g_controls[index].envelope.release = envelope.release;
  Engine().SetRelease(index, envelope.release);
}

// decomp: SetVoiceVolume/SetVoiceSampleRate, SLUS_010.40/25AC.c:1132-1151.
void Audio_Driver_SetVoiceVolume(int voice, unsigned left, unsigned right,
                                 unsigned scale) {
  auto& controls = g_controls[CheckedVoice(voice)];
  controls.left = Gain(left, scale);
  controls.right = Gain(right, scale);
  Engine().Configure(voice, controls);
}

void Audio_Driver_SetVoiceSampleRate(int voice, int rate) {
  auto& controls = g_controls[CheckedVoice(voice)];
  controls.rate =
      static_cast<float>(std::clamp(rate, 0, kMaximumPitch)) / kUnityPitch;
  Engine().Configure(voice, controls);
}

// decomp: voice mode setters, SLUS_010.40/25AC.c:1114-1130.
void Audio_Driver_SetVoiceReverbMask(unsigned voices) {
  SetModeMask(voices, &VoiceControls::reverb);
}
void Audio_Driver_SetVoiceNoiseMask(unsigned voices) {
  SetModeMask(voices, &VoiceControls::noise);
}
void Audio_Driver_SetVoiceFrequencyModulationMask(unsigned voices) {
  SetModeMask(voices & ~1U, &VoiceControls::frequency_modulation);
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
  controls.left = Gain(left, 0);
  controls.right = Gain(right, 0);
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
