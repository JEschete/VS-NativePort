#include "Audio/StreamPlayback.h"

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <utility>

#include "Audio/SequencerOutput.h"
#include "Content/AudioStream.hpp"
#include "Content/DataStore.hpp"
#include "Core/Log.hpp"
#include "Legacy/Psx.h"
#include "Platform/Paths.hpp"

extern "C" {
#include "Audio/Cutscene/Stream.h"
#include "Audio/Driver/SoundDriver.h"
#include "Audio/Mixer/VoiceMixer.h"
#include "Audio/SoundBanks.h"
}

namespace {
constexpr int kRoomSlot = 2;
constexpr int kMaximumVolume = 0x7F00;
constexpr int kMaximumFadeTicks = 0x7FFF;
constexpr unsigned kVoiceModesDirty = 0x100;
constexpr unsigned kMono = 2;
constexpr int kCenterPan = 128;
constexpr float kPitchUnity = 4096;
constexpr float kOriginalRate = 44100;
constexpr VS::usize kGainCount = 4;
constexpr unsigned kVoicePairMask = 3;
constexpr unsigned kPanFractionMask = 0xFF;
constexpr int kMaximumPackedPan = 0xFF00;

struct BoundStream {
  void* source = nullptr;
  VS::Content::AudioStreamInfo info;
  std::array<std::string, 2> instruments;
};
std::optional<BoundStream> g_bound;
std::optional<std::array<unsigned, kGainCount>> g_gains;
int g_fade_target = kMaximumVolume;

std::array<unsigned, kGainCount> StreamGains() {
  const auto& state = Audio_Cutscene_streamState;
  const auto volume = static_cast<unsigned>(state.volume);
  if ((Audio_Driver_controlLatches.mixBehavior & kMono) != 0) {
    const unsigned gain = (volume * Audio_Mixer_kPanGains[kCenterPan]) >> 16;
    return {gain, gain, gain, gain};
  }
  if ((state.formatFlags & 1) != 0) {
    return {volume / 2, 0, 0, volume / 2};
  }
  const unsigned pan = (state.pan.word >> 8) & 0xFF;
  const unsigned left = (volume * Audio_Mixer_kPanGains[pan]) >> 16;
  const unsigned right = (volume * Audio_Mixer_kPanGains[pan ^ 0xFF]) >> 16;
  return {left, right, left, right};
}

void ApplyGains() {
  if (Audio_Cutscene_streamState.voiceMask == 0) return;
  const auto gains = StreamGains();
  if (g_gains == gains) return;
  const int voice = Audio_Cutscene_streamState.voiceIndex;
  Audio_Driver_SetVoiceVolume(voice, gains[0], gains[1], 0);
  Audio_Driver_SetVoiceVolume(voice + 1, gains[2], gains.back(), 0);
  g_gains = gains;
}

void PauseVoices(bool paused) {
  if (Audio_Cutscene_streamState.voiceMask == 0) return;
  const auto voice =
      static_cast<unsigned>(Audio_Cutscene_streamState.voiceIndex);
  Audio_Native_PauseVoice(voice, paused ? 1 : 0);
  Audio_Native_PauseVoice(voice + 1, paused ? 1 : 0);
}

void ReserveVoices(int voice, int reverb) {
  const unsigned mask = kVoicePairMask << voice;
  auto& state = Audio_Cutscene_streamState;
  state.voiceIndex = voice;
  state.voiceMask = static_cast<int>(mask);
  auto& scheduler = Audio_Mixer_voiceScheduler;
  scheduler.activeVoices &= ~mask;
  scheduler.pendingKeyOn &= ~mask;
  scheduler.pendingKeyOff &= ~mask;
  scheduler.keyedVoices &= ~mask;
  scheduler.reverbVoices &= ~mask;
  if (reverb != 0) scheduler.reverbVoices |= mask;
  scheduler.fmVoices &= ~mask;
  scheduler.noiseVoices &= ~mask;
  Audio_Driver_controlLatches.updateFlags |= kVoiceModesDirty;
}
}  // namespace

// func_80046608, SLUS_010.40/main.c:10339. The old unk14[22] is slot 3,
// not a native musicHandles index; the caller supplies the bounded P block.
int Audio_BindRoomCutsceneStream(void* data, size_t byte_count) {
  if (Audio_soundState.sfxStreamData[kRoomSlot] != nullptr) {
    VS_LOG_DEBUG("audio", "room stream slot is already bound");
    return 0;
  }
  const auto* store = VS::Content::DataStore::Get();
  if (data == nullptr || store == nullptr) {
    VS_LOG_ERROR("audio",
                 "stream binding requires data and an open data store");
    return 0;
  }
  auto info = VS::Content::InspectAudioStream(
      {static_cast<const VS::u8*>(data), byte_count});
  if (!info) {
    VS_LOG_ERROR("audio", "stream binding: {}", info.error());
    return 0;
  }
  if (auto result =
          VS::Content::ValidateAudioStreamAssets(*info, store->Root());
      !result) {
    VS_LOG_ERROR("audio", "{}", result.error());
    return 0;
  }
  BoundStream binding{data, *info, {}};
  for (VS::usize channel = 0; channel < binding.instruments.size(); ++channel) {
    binding.instruments[channel] =
        VS::Platform::PathText(VS::Content::AudioStreamInstrumentPath(
            store->Root(), *info, channel % info->channels));
  }
  g_bound = std::move(binding);
  Audio_soundState.sfxStreamData[kRoomSlot] = data;
  VS_LOG_INFO("audio", "bound stream {}: {} channels, {} frames", info->id,
              info->channels, info->frames);
  return kRoomSlot + 1;
}

// func_80046634, SLUS_010.40/main.c:10348; the package owns the source bytes.
int Audio_UnbindRoomCutsceneStream(void) {
  if (Audio_soundState.sfxStreamData[kRoomSlot] == nullptr) return 0;
  Audio_Cutscene_StopStreamVoices();
  Audio_soundState.sfxStreamData[kRoomSlot] = nullptr;
  g_bound.reset();
  return 1;
}

// Sound_Cutscene_StartStream, SLUS_010.40/25AC.c:6294.
void Audio_Cutscene_StartStream(uint32_t* data, int pan, int reverb) {
  if (!g_bound || data != g_bound->source) {
    VS_LOG_ERROR("audio", "cannot play an unbound cutscene stream");
    return;
  }
  if (pan < 0 || pan > kMaximumPackedPan || (pan & kPanFractionMask) != 0) {
    VS_LOG_ERROR("audio", "invalid packed stream pan {}", pan);
    return;
  }
  const int voice = Audio_Cutscene_FindFreeVoicePair();
  if (voice < 0) {
    VS_LOG_WARN("audio", "no voice pair available for stream {}",
                g_bound->info.id);
    return;
  }
  auto& state = Audio_Cutscene_streamState;
  state.pan.word = pan;
  state.formatFlags =
      (g_bound->info.channels == 2 ? 1 : 0) | (g_bound->info.repeat ? 2 : 0);
  state.sampleRate = static_cast<int>(g_bound->info.source_pitch);
  ReserveVoices(voice, reverb);
  g_gains = StreamGains();
  const float rate =
      static_cast<float>(g_bound->info.source_pitch) * kOriginalRate /
      (static_cast<float>(g_bound->info.sample_rate) * kPitchUnity);
  for (VS::usize channel = 0; channel < g_bound->instruments.size();
       ++channel) {
    if (Audio_Native_StartStreamVoice(
            static_cast<unsigned>(voice) + static_cast<unsigned>(channel),
            g_bound->instruments[channel].c_str(), rate,
            (*g_gains)[channel * 2], (*g_gains)[channel * 2 + 1],
            reverb) == 0) {
      Audio_Cutscene_StopStreamVoices();
      return;
    }
  }
}

// func_8001D644, SLUS_010.40/25AC.c:6503; native command pointer width.
void Audio_Cutscene_ReplaceStream(Audio_CommandParams* params) {
  if (params->param2 > kMaximumPackedPan) {
    VS_LOG_ERROR("audio", "invalid packed stream pan {}", params->param2);
    return;
  }
  Audio_Cutscene_StopStreamVoices();
  Audio_Cutscene_StartStream(reinterpret_cast<uint32_t*>(params->param1),
                             static_cast<int>(params->param2),
                             params->param3 != 0);
}

// IRQCallbackProc, SLUS_010.40/25AC.c:6238; completion is a native voice
// report.
void Audio_Cutscene_StopStreamVoices(void) {
  const unsigned mask = Audio_Cutscene_streamState.voiceMask;
  if (mask == 0) return;
  PauseVoices(false);
  Audio_Driver_KeyOffVoices(mask);
  Audio_Cutscene_streamState.voiceMask = 0;
  Audio_Mixer_voiceScheduler.reverbVoices &= ~mask;
  Audio_Driver_controlLatches.updateFlags |= kVoiceModesDirty;
  g_gains.reset();
  VS_LOG_INFO("audio", "stream stopped: voices={:06x}", mask);
}

// Sound_Cmd_9F_unk / Sound_Cmd_9E_unk, SLUS_010.40/25AC.c:3786,3794.
void Audio_Cutscene_PauseStream(Audio_CommandParams*) { PauseVoices(true); }
void Audio_Cutscene_ResumeStream(Audio_CommandParams*) { PauseVoices(false); }

// func_8001D698, SLUS_010.40/25AC.c:6511; typed command-table callback.
void Audio_Cutscene_StopStream(Audio_CommandParams*) {
  Audio_Cutscene_StopStreamVoices();
}

// Sound_Cmd_E4_SetCutsceneVolume, SLUS_010.40/25AC.c:6513.
void Audio_Cutscene_SetStreamVolume(Audio_CommandParams* params) {
  if (params->param1 > kMaximumVolume) {
    VS_LOG_ERROR("audio", "invalid stream volume {}", params->param1);
    return;
  }
  Audio_Cutscene_streamState.volume = static_cast<int>(params->param1);
  Audio_Cutscene_streamState.volumeFadeTicksLeft = 0;
  ApplyGains();
}

// Sound_Cmd_E5_FadeOutCutscene, SLUS_010.40/25AC.c:6525.
void Audio_Cutscene_SlideStreamVolume(Audio_CommandParams* params) {
  if (params->param1 > kMaximumFadeTicks || params->param2 > kMaximumVolume) {
    VS_LOG_ERROR("audio", "invalid stream fade duration or target");
    return;
  }
  auto& state = Audio_Cutscene_streamState;
  const int ticks = std::max(1, static_cast<int>(params->param1));
  g_fade_target = static_cast<int>(params->param2);
  state.volumeFadeStep =
      (g_fade_target - static_cast<int>(state.volume)) / ticks;
  state.volumeFadeTicksLeft = ticks;
}

// func_8001D7A8, SLUS_010.40/25AC.c:6537; native command pointer width.
void Audio_Cutscene_SetStreamPan(Audio_CommandParams* params) {
  if (params->param1 > kMaximumPackedPan ||
      (params->param1 & kPanFractionMask) != 0) {
    VS_LOG_ERROR("audio", "invalid packed stream pan {}", params->param1);
    return;
  }
  Audio_Cutscene_streamState.pan.word = static_cast<int>(params->param1);
  ApplyGains();
}

// Replaces the stream portion of func_8001924C, SLUS_010.40/25AC.c:4024.
void Audio_Native_UpdateCutsceneStream(void) {
  auto& state = Audio_Cutscene_streamState;
  if (state.voiceMask == 0) return;
  const auto voice = static_cast<unsigned>(state.voiceIndex);
  if (Audio_Native_VoiceActive(voice) == 0 &&
      Audio_Native_VoiceActive(voice + 1) == 0) {
    Audio_Cutscene_StopStreamVoices();
    return;
  }
  if (state.volumeFadeTicksLeft > 0) {
    --state.volumeFadeTicksLeft;
    state.volume =
        state.volumeFadeTicksLeft == 0
            ? g_fade_target
            : std::clamp(static_cast<int>(state.volume) + state.volumeFadeStep,
                         0, kMaximumVolume);
  }
  ApplyGains();
}

void Audio_Native_ResetCutsceneStream(void) {
  Audio_Cutscene_StopStreamVoices();
  Audio_soundState.sfxStreamData[kRoomSlot] = nullptr;
  Audio_Cutscene_streamState = {};
  Audio_Cutscene_streamState.volume = kMaximumVolume;
  g_bound.reset();
  g_gains.reset();
  g_fade_target = kMaximumVolume;
}
