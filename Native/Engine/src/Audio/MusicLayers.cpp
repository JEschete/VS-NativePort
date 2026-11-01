// Hand-maintained (tools/port/owned.csv).
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <span>
#include <utility>

#include "Audio/MusicState.h"
#include "Audio/MusicVoiceConflicts.hpp"
#include "Core/Log.hpp"
#include "Legacy/Psx.h"

extern "C" {
#include "Audio/Cutscene/Stream.h"
#include "Audio/Mixer/VoiceMixer.h"
#include "Audio/Seq/Channel.h"
}

namespace {
class SequenceChannelVoices final : public VS::Audio::Seq::MusicChannelVoices {
 public:
  explicit SequenceChannelVoices(std::span<Audio_SeqChannel> channels)
      : channels_(channels) {}

  [[nodiscard]] std::size_t ChannelCount() const override {
    return channels_.size();
  }
  [[nodiscard]] std::uint32_t AssignedVoice(
      std::size_t channel) const override {
    return channels_[channel].voice.assignedVoice;
  }
  void DetachVoice(std::size_t channel) override {
    channels_[channel].voice.assignedVoice = VS::Audio::Seq::kUnassignedVoice;
  }

 private:
  std::span<Audio_SeqChannel> channels_;
};

VS::Audio::Seq::MusicLayerVoices LayerVoices(const Audio_MusicConfig& config) {
  return {static_cast<std::uint32_t>(config.allocatedVoices),
          static_cast<std::uint32_t>(config.keyedVoices)};
}
}  // namespace

void VS::Audio::Seq::ReleaseLiveSavedVoiceConflicts() {
  const Audio_MusicConfig* saved = Audio_Seq_secondaryMusicConfig;
  if (saved == nullptr) return;
  const Audio_MusicConfig* playing = Audio_Seq_activeMusicConfig;
  Audio_SeqChannel* channels = Audio_Seq_secondaryMusicChannels;
  if (playing == nullptr || channels == nullptr) {
    VS_LOG_ERROR("audio", "secondary music layer has incomplete native state");
    return;
  }
  // The secondary table mirrors the music channel table's extent.
  SequenceChannelVoices saved_channels(std::span<Audio_SeqChannel>(
      channels, std::size(Audio_Seq_musicChannels)));
  auto& scheduler = Audio_Mixer_voiceScheduler;
  const auto reserved = static_cast<std::uint32_t>(scheduler.activeVoices |
                                                   Audio_Cutscene_streamVoices);
  scheduler.pendingKeyOff |= ReleaseSavedVoiceConflicts(
      LayerVoices(*playing), LayerVoices(*saved), saved_channels, reserved);
}

// Sound_Cmd_40/19/1A snapshots (SLUS_010.40/25AC.c:2969,2983,3001).
void Audio_Native_SaveMusicSnapshot(void) {
  Audio_Seq_savedMusicConfig = Audio_Seq_primaryMusicConfig;
  std::copy(std::begin(Audio_Seq_musicChannels),
            std::end(Audio_Seq_musicChannels),
            std::begin(Audio_Seq_savedMusicChannels));
}

// Sound_SetMusicSequence snapshot restore/swap (SLUS_010.40/25AC.c:2847).
void Audio_Native_RestoreMusicSnapshot(int swap) {
  if (swap != 0) {
    std::swap(Audio_Seq_primaryMusicConfig, Audio_Seq_savedMusicConfig);
    std::swap_ranges(std::begin(Audio_Seq_musicChannels),
                     std::end(Audio_Seq_musicChannels),
                     std::begin(Audio_Seq_savedMusicChannels));
  } else {
    Audio_Seq_primaryMusicConfig = Audio_Seq_savedMusicConfig;
    std::copy(std::begin(Audio_Seq_savedMusicChannels),
              std::end(Audio_Seq_savedMusicChannels),
              std::begin(Audio_Seq_musicChannels));
  }
}
