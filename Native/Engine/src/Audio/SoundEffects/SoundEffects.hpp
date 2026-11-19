#pragma once
// The sound-effect channels.
#include <cstddef>

#include "Audio/Seq/Channel.hpp"

namespace VS::Audio::SoundEffects {
// The decomp declares D_80035910 with ten channels; its SfxSoundChannels view
// starts at channels + 10. One array holds both.
inline constexpr std::size_t kChannelSlots = 22;

// decomp: D_80035910 (D_80035910@SLUS_010.40)
extern SeqChannel channels[kChannelSlots];
}  // namespace VS::Audio::SoundEffects
