// HUD value views against the original instructions. The minimap operation
// uses the owned Field implementation; no status/limb calculations are stubbed.
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

#include "Battle/HudState.hpp"
#include "Field/Map.hpp"

namespace {
namespace b = VS::Battle;
constexpr std::size_t kPeerWordCount = 8;
constexpr std::uint32_t kByteMask = 0xFF;
constexpr std::size_t kHiddenWord = 0;
constexpr unsigned kHiddenShift = 24;
constexpr std::size_t kModeWord = 2;
constexpr std::size_t kShortcutWord = 4;
constexpr std::size_t kLowerScreenWord = 5;
constexpr std::size_t kMinimapWord = 7;
constexpr unsigned kMinimapShift = 16;

std::uint8_t Byte(std::uint32_t word, unsigned shift = 0) {
  return static_cast<std::uint8_t>(word >> shift);
}

// Fixture serialization only: these windows include independently owned
// legacy peers, so a wider original store is visible to the comparator.
void StoreByte(std::array<std::uint32_t, kPeerWordCount> &words,
               std::size_t index, unsigned shift, std::uint8_t value) {
  words.at(index) = (words.at(index) & ~(kByteMask << shift)) |
                    (std::uint32_t{value} << shift);
}

class HudWorld final : public b::GameplayHudWorld {
public:
  b::HudState state{};
  std::uint8_t minimap{};
  std::vector<std::array<std::uint32_t, 2>> calls;
  void SetHudVisibility(b::HudVisibility visibility) override {
    calls.push_back({0, static_cast<std::uint8_t>(visibility)});
    b::SetHudHidden(state, b::HudVisibilityCommand{visibility});
  }
  void SetFieldOverlayVisible(bool visible) override {
    const int command = visible ? 1 : 0;
    calls.push_back({1, static_cast<std::uint32_t>(command)});
    VS::Field::SetMinimapVisible(minimap, command);
  }
};

bool RunVector() {
  int operation = 0;
  std::int32_t command = 0;
  std::uint32_t statuses = 0;
  int hp = 0;
  int max_hp = 0;
  if (!(std::cin >> operation >> command >> statuses >> hp >> max_hp)) {
    return false;
  }
  std::array<std::uint32_t, kPeerWordCount> words{};
  std::array<std::uint16_t, b::kHudMappedStatusCount> mapping{};
  for (auto &word : words)
    std::cin >> word;
  for (auto &flag : mapping)
    std::cin >> flag;
  if (!std::cin || hp < -32768 || hp > 32767 || max_hp < -32768 ||
      max_hp > 32767) {
    std::cerr << "invalid HUD fixture input\n";
    std::exit(64);
  }
  HudWorld world;
  world.state = {
      .visibility =
          b::HudVisibilityCommand{Byte(words[kHiddenWord], kHiddenShift)},
      .battle_mode = b::HudBattleModeCommand{Byte(words[kModeWord])},
      .lower_screen = Byte(words[kLowerScreenWord]),
      .shortcut = Byte(words[kShortcutWord]),
  };
  world.minimap = Byte(words[kMinimapWord], kMinimapShift);
  bool refused = false;
  std::uint32_t returned = 0;
  switch (operation) {
  case 0:
    returned =
        static_cast<std::uint32_t>(b::SetGameplayHudHidden(world, command));
    break;
  case 1: {
    const b::HudStatusView state{statuses};
    const auto flags = b::StatusIconFlags(state, mapping);
    if (!flags.has_value()) {
      refused = true;
    } else {
      returned = flags.value();
    }
    break;
  }
  case 2:
    returned = static_cast<std::uint32_t>(b::LimbCondition(
        {static_cast<std::int16_t>(hp), static_cast<std::int16_t>(max_hp)}));
    break;
  case 3:
    b::ShowShortcutHud(world.state);
    break;
  case 4:
    b::ShowPuzzleTimer(world.state);
    break;
  case 5:
    b::HidePuzzleTimer(world.state);
    break;
  case 6:
    b::SetHudHidden(world.state, b::HudVisibilityCommand{
                                     static_cast<std::uint8_t>(command)});
    break;
  case 7:
    b::SetHudBattleMode(world.state, b::HudBattleModeCommand{
                                         static_cast<std::uint8_t>(command)});
    break;
  default:
    std::cerr << "unknown HUD fixture operation\n";
    std::exit(64);
  }
  StoreByte(words, kHiddenWord, kHiddenShift, world.state.visibility.value);
  StoreByte(words, kModeWord, 0, world.state.battle_mode.value);
  StoreByte(words, kShortcutWord, 0,
            static_cast<std::uint8_t>(world.state.shortcut));
  StoreByte(words, kLowerScreenWord, 0,
            static_cast<std::uint8_t>(world.state.lower_screen));
  StoreByte(words, kMinimapWord, kMinimapShift, world.minimap);
  std::cout << refused << ' ' << returned;
  for (const auto word : words)
    std::cout << ' ' << word;
  std::cout << ' ' << world.calls.size();
  for (const auto &call : world.calls) {
    std::cout << ' ' << call[0] << ' ' << call[1];
  }
  std::cout << '\n';
  return true;
}
} // namespace

int main() {
  while (RunVector()) {
  }
  return std::cin.eof() ? 0 : 64;
}
