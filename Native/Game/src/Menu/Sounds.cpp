#include "Menu/Sounds.h"

#include "Legacy/Psx.h"
extern "C" {
#include "Audio/Sfx.h"
}

namespace {
constexpr int kMenuChannelGroup = 0x7E;
constexpr int kOpenSound = 10;
constexpr int kInvalidSound = 7;
constexpr int kLeaveSound = 6;
constexpr int kSelectSound = 5;
constexpr int kChangeSound = 4;
}  // namespace

// vs_battle_playSfx10, BATTLE/BATTLE.PRG/573B8.c:330; shared tail .L800C02FC.
void Menu_PlayOpenSound(void) {
  Audio_PlaySfxCentered(kMenuChannelGroup, kOpenSound);
}

// vs_battle_playInvalidSfx, BATTLE/BATTLE.PRG/573B8.c:335.
void Menu_PlayInvalidSound(void) {
  Audio_PlaySfxCentered(kMenuChannelGroup, kInvalidSound);
}

// vs_battle_playMenuLeaveSfx, BATTLE/BATTLE.PRG/573B8.c:340.
void Menu_PlayLeaveSound(void) {
  Audio_PlaySfxCentered(kMenuChannelGroup, kLeaveSound);
}

// vs_battle_playMenuSelectSfx, BATTLE/BATTLE.PRG/573B8.c:345.
void Menu_PlaySelectSound(void) {
  Audio_PlaySfxCentered(kMenuChannelGroup, kSelectSound);
}

// vs_battle_playMenuChangeSfx, BATTLE/BATTLE.PRG/573B8.c:350.
void Menu_PlayChangeSound(void) {
  Audio_PlaySfxCentered(kMenuChannelGroup, kChangeSound);
}
