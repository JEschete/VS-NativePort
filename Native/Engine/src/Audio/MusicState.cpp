// Hand-maintained (tools/port/owned.csv).
#include <cstddef>
#include <cstdint>

#include "Audio/MusicVoiceConflicts.hpp"

namespace VS::Audio::Seq {
// decomp: func_80015BAC (SLUS_010.40/25AC.c:2326); original 80015BAC-80015C98.
std::uint32_t ReleaseSavedVoiceConflicts(const MusicLayerVoices& playing,
                                         const MusicLayerVoices& saved,
                                         MusicChannelVoices& saved_channels,
                                         std::uint32_t reserved_voices) {
  constexpr std::uint32_t kVoiceMask = (1U << kSpuVoiceCount) - 1;
  const std::uint32_t playing_keyed = playing.keyed & playing.allocated;
  const std::uint32_t saved_keyed = saved.keyed & saved.allocated;
  const std::uint32_t retained =
      saved.allocated & (~playing_keyed | saved_keyed);
  const std::uint32_t conflicts = playing.allocated & ~retained & kVoiceMask;
  std::uint32_t released = 0;
  for (std::size_t channel = 0; channel < saved_channels.ChannelCount();
       ++channel) {
    const std::uint32_t voice = saved_channels.AssignedVoice(channel);
    if (voice < kSpuVoiceCount && (conflicts & (1U << voice)) != 0) {
      released |= 1U << voice;
      saved_channels.DetachVoice(channel);
    }
  }
  return released & ~reserved_voices;
}
}  // namespace VS::Audio::Seq
