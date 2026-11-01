#pragma once

#include <cstddef>
#include <cstdint>

// The saved music layer keeps its channel voice assignments while another
// sequence plays. Before the saved layer resumes, voices the playing layer
// claimed from it are detached and keyed off. The policy sees the channels only
// through MusicChannelVoices, so it reaches no global sequencer state.
namespace VS::Audio::Seq {
inline constexpr std::uint32_t kSpuVoiceCount = 24;
// The assignment a channel holds when it owns no hardware voice.
inline constexpr std::uint32_t kUnassignedVoice = kSpuVoiceCount;

// One music layer's hardware voice masks.
struct MusicLayerVoices {
  std::uint32_t allocated = 0;
  std::uint32_t keyed = 0;
};

// The saved layer's sequence channels.
class MusicChannelVoices {
 public:
  virtual ~MusicChannelVoices() = default;
  [[nodiscard]] virtual std::size_t ChannelCount() const = 0;
  [[nodiscard]] virtual std::uint32_t AssignedVoice(
      std::size_t channel) const = 0;
  virtual void DetachVoice(std::size_t channel) = 0;
};

// Detaches the saved channels whose voice the playing layer allocated and the
// saved layer cannot keep (func_80015BAC). A voice stays with the saved layer
// when the saved layer also allocated it and either the playing layer has not
// keyed it or the saved layer keyed it too. Returns the detached voices that
// neither a sound effect nor a cutscene stream reserves, which the caller keys
// off.
[[nodiscard]] std::uint32_t ReleaseSavedVoiceConflicts(
    const MusicLayerVoices& playing, const MusicLayerVoices& saved,
    MusicChannelVoices& saved_channels, std::uint32_t reserved_voices);

// Applies ReleaseSavedVoiceConflicts to the live sequencer: the active and
// secondary music configurations, the secondary channel table and the mixer's
// pending key-off mask. Does nothing without a saved layer.
void ReleaseLiveSavedVoiceConflicts();
}  // namespace VS::Audio::Seq
