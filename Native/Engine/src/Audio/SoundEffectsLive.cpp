#include "Audio/SfxLive.hpp"
#include "Audio/SoundEffects.hpp"

namespace VS::Audio {

void PlayCenteredSoundEffect(std::int32_t channel_group,
                             std::int32_t effect_id) {
  LivePlaySfxCentered(channel_group, effect_id);
}

}  // namespace VS::Audio
