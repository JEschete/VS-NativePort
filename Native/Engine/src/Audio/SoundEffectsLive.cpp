#include "Audio/SoundEffects.hpp"
#include "Legacy/Psx.h"
extern "C" {
#include "Audio/Sfx.h"
}

namespace VS::Audio {

void PlayCenteredSoundEffect(std::int32_t channel_group,
                             std::int32_t effect_id) {
  Audio_PlaySfxCentered(channel_group, effect_id);
}

}  // namespace VS::Audio
