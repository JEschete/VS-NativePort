#pragma once

#include <cstdint>

namespace VS::Audio {

// Plays a resident sound effect centred (pan 0x80) at full volume (0x7F) on
// the given source channel group: the native audio driver binding for
// SLUS_010.40 vs_main_playSfxDefault (SfxLive.hpp LivePlaySfxCentered).
void PlayCenteredSoundEffect(std::int32_t channel_group,
                             std::int32_t effect_id);

}  // namespace VS::Audio
