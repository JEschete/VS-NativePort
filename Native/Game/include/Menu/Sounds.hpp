#pragma once

#include <cstdint>

namespace VS::Menu {

// Source sound-effect ids played on the menu channel group.
enum class MenuSound : std::uint8_t {
  kChange = 4,
  kSelect = 5,
  kLeave = 6,
  kInvalid = 7,
  kOpen = 10
};

inline constexpr std::int32_t kMenuSoundChannelGroup = 0x7E;

void PlayMenuSound(MenuSound sound);

void PlayOpenSound();
void PlayInvalidSound();
void PlayLeaveSound();
void PlaySelectSound();
void PlayChangeSound();

}  // namespace VS::Menu
