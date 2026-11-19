// Hand-maintained (tools/port/owned.csv).
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <span>
#include <utility>

#include "Audio/Cutscene/Stream.hpp"
#include "Audio/Mixer/VoiceMixer.hpp"
#include "Audio/MusicState.hpp"
#include "Audio/MusicVoiceConflicts.hpp"
#include "Audio/Seq/Channel.hpp"
#include "Core/Log.hpp"
#include "Legacy/Psx.h"

namespace {
class SequenceChannelVoices final : public VS::Audio::Seq::MusicChannelVoices {
 public:
  explicit SequenceChannelVoices(std::span<VS::Audio::SeqChannel> channels)
      : channels_(channels) {}

  [[nodiscard]] std::size_t ChannelCount() const override {
    return channels_.size();
  }
  [[nodiscard]] std::uint32_t AssignedVoice(
      std::size_t channel) const override {
    return channels_[channel].voice.assigned_voice;
  }
  void DetachVoice(std::size_t channel) override {
    channels_[channel].voice.assigned_voice = VS::Audio::Seq::kUnassignedVoice;
  }

 private:
  std::span<VS::Audio::SeqChannel> channels_;
};

VS::Audio::Seq::MusicLayerVoices LayerVoices(
    const VS::Audio::MusicConfig& config) {
  return {static_cast<std::uint32_t>(config.allocated_voices),
          static_cast<std::uint32_t>(config.keyed_voices)};
}
}  // namespace

void VS::Audio::Seq::ReleaseLiveSavedVoiceConflicts() {
  const VS::Audio::MusicConfig* saved = VS::Audio::Seq::secondaryMusicConfig;
  if (saved == nullptr) return;
  const VS::Audio::MusicConfig* playing = VS::Audio::Seq::activeMusicConfig;
  VS::Audio::SeqChannel* channels = VS::Audio::Seq::secondaryMusicChannels;
  if (playing == nullptr || channels == nullptr) {
    VS::Core::LogError("audio",
                       "secondary music layer has incomplete native state");
    return;
  }
  // The secondary table mirrors the music channel table's extent.
  SequenceChannelVoices saved_channels(std::span<VS::Audio::SeqChannel>(
      channels, std::size(VS::Audio::Seq::musicChannels)));
  auto& scheduler = VS::Audio::Mixer::voiceScheduler;
  const auto reserved = static_cast<std::uint32_t>(
      scheduler.active_voices | VS::Audio::Cutscene::streamState.voice_mask);
  scheduler.pending_key_off |= ReleaseSavedVoiceConflicts(
      LayerVoices(*playing), LayerVoices(*saved), saved_channels, reserved);
}

// Sound_Cmd_40/19/1A snapshots (SLUS_010.40/25AC.c:2969,2983,3001).
void VS::Audio::SaveMusicSnapshot(void) {
  VS::Audio::Seq::savedMusicConfig = VS::Audio::Seq::primaryMusicConfig;
  std::copy(std::begin(VS::Audio::Seq::musicChannels),
            std::end(VS::Audio::Seq::musicChannels),
            std::begin(VS::Audio::Seq::savedMusicChannels));
}

// Sound_SetMusicSequence snapshot restore/swap (SLUS_010.40/25AC.c:2847).
void VS::Audio::RestoreMusicSnapshot(int swap) {
  if (swap != 0) {
    std::swap(VS::Audio::Seq::primaryMusicConfig,
              VS::Audio::Seq::savedMusicConfig);
    std::swap_ranges(std::begin(VS::Audio::Seq::musicChannels),
                     std::end(VS::Audio::Seq::musicChannels),
                     std::begin(VS::Audio::Seq::savedMusicChannels));
  } else {
    VS::Audio::Seq::primaryMusicConfig = VS::Audio::Seq::savedMusicConfig;
    std::copy(std::begin(VS::Audio::Seq::savedMusicChannels),
              std::end(VS::Audio::Seq::savedMusicChannels),
              std::begin(VS::Audio::Seq::musicChannels));
  }
}
