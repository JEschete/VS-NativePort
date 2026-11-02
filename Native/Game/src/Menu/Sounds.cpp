#include "Menu/Sounds.hpp"

#include "Audio/SoundEffects.hpp"

namespace VS::Menu {

void PlayMenuSound(MenuSound sound) {
  VS::Audio::PlayCenteredSoundEffect(kMenuSoundChannelGroup,
                                     static_cast<std::int32_t>(sound));
}

// vs_battle_playSfx10, BATTLE/BATTLE.PRG/573B8.c:330; shared tail .L800C02FC.
void PlayOpenSound() { PlayMenuSound(MenuSound::kOpen); }

// vs_battle_playInvalidSfx, BATTLE/BATTLE.PRG/573B8.c:335.
void PlayInvalidSound() { PlayMenuSound(MenuSound::kInvalid); }

// vs_battle_playMenuLeaveSfx, BATTLE/BATTLE.PRG/573B8.c:340.
void PlayLeaveSound() { PlayMenuSound(MenuSound::kLeave); }

// vs_battle_playMenuSelectSfx, BATTLE/BATTLE.PRG/573B8.c:345.
void PlaySelectSound() { PlayMenuSound(MenuSound::kSelect); }

// vs_battle_playMenuChangeSfx, BATTLE/BATTLE.PRG/573B8.c:350.
void PlayChangeSound() { PlayMenuSound(MenuSound::kChange); }

}  // namespace VS::Menu
