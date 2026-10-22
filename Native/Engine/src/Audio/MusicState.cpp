#include "Audio/MusicState.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include "Core/Log.hpp"
#include "Legacy/Psx.h"

extern "C" {
#include "Audio/Cutscene/Stream.h"
#include "Audio/Mixer/VoiceMixer.h"
#include "Audio/Seq/Channel.h"
}

// decomp: func_80015BAC (SLUS_010.40/25AC.c:2326); original 80015BAC-80015C98.
void Audio_Seq_ReleaseSavedVoiceConflicts(void) {
  constexpr unsigned kVoiceCount = 24;
  constexpr unsigned kVoiceMask = (1U << kVoiceCount) - 1;
  const auto* saved = Audio_Seq_secondaryMusicConfig;
  if (!saved) return;
  const auto* current = Audio_Seq_activeMusicConfig;
  auto* channels = Audio_Seq_secondaryMusicChannels;
  if (!current || !channels) {
    VS_LOG_ERROR("audio", "secondary music layer has incomplete native state");
    return;
  }
  const unsigned current_keyed =
      current->keyedVoices & current->allocatedVoices;
  const unsigned saved_keyed = saved->keyedVoices & saved->allocatedVoices;
  const unsigned retained =
      saved->allocatedVoices & (~current_keyed | saved_keyed);
  const unsigned conflicts = current->allocatedVoices & ~retained & kVoiceMask;
  unsigned released = 0;
  for (unsigned channel = 0; channel < std::size(Audio_Seq_musicChannels);
       ++channel) {
    auto& voice = channels[channel].voice.assignedVoice;
    if (voice < kVoiceCount && (conflicts & (1U << voice)) != 0) {
      released |= 1U << voice;
      voice = kVoiceCount;
    }
  }
  auto& scheduler = Audio_Mixer_voiceScheduler;
  scheduler.pendingKeyOff |=
      released & ~(scheduler.activeVoices | Audio_Cutscene_streamVoices);
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
