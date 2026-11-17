#include <array>
#include <bit>
#include <cstring>
#include <iostream>
#include <limits>
#include <source_location>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include "Core/Log.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Menu/BattleAbilitiesUnlockLive.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/TextBoxStateLive.hpp"

static_assert(std::is_same_v<
              decltype(&VS::Menu::BattleAbilities::LiveCreateAbilityRowsEntry),
              int (*)(int, int, std::uint16_t**)>);

extern "C" {
#include "State/Progress.h"
#include "Audio/Seq/Channel.h"
#include "Battle/AbilityState.h"
#include "Battle/ActionTable.h"
#include "Battle/TargetingState.h"
#include "Input/ControllerLiveState.h"
#include "Menu/BattleAbilities.h"
#include "Menu/Framework/FrameworkLiveState.h"
#include "Menu/TextBox.h"

State_ArtsStatus Audio_Seq_unusedArtsStatus{};
Battle_Actor* Battle_characterState = nullptr;
u_short Battle_breakArtPointRequirements[15][4]{};
u_short Battle_battleAbilityPointRequirements[150]{};
Battle_ActionDefinition Battle_actionDefinitions[256]{};
Menu_StringContext Battle_stringContext{};
Input_PressedButtonsView Input_buttonsPressed{};
State_Settings Input_settings{};
Menu_TextBoxState Menu_textBoxes[8]{};
Menu_Framework_MenuMemory Menu_Framework_menuMemory{};
char Menu_Framework_unlockedBreakArt = 0;
char Menu_Framework_unlockedBattleAbility = 0;
char Menu_BattleAbilities_abilityCount = 0;
u_int Menu_BattleAbilities_abilityMenuState = 0;
u_short* Menu_BattleAbilities_abilityText = nullptr;
u_short Menu_BattleAbilities_battleAbilityMenuStrings[292]{};
u_short Menu_BattleAbilities_battleAbilityStrings[1060]{};
char Menu_BattleAbilities_firstAbility = 0;
int Menu_BattleAbilities_highlightedAbility = 0;
char Menu_BattleAbilities_selectedAbilityType = 0;
char Menu_BattleAbilities_unlockedChainAbilities[16]{};
char Menu_BattleAbilities_unlockedDefenseAbilities[16]{};
}

namespace {
using Words = std::vector<std::int64_t>;
std::array<std::uint16_t, 16 * 64 + 1> g_text{};
std::vector<Words> g_calls;

void Unexpected() { throw std::runtime_error("unexpected row-binding dependency"); }
char Byte(int value) {
  return std::bit_cast<char>(static_cast<std::uint8_t>(value));
}

void Run(const Words& in) {
  namespace a = VS::Menu::BattleAbilities;
  a::ResetLiveAbilityFoundationState();
  g_calls.clear();
  const int count = static_cast<int>(in.at(0));
  const int type = static_cast<int>(in.at(1));
  const auto seed = static_cast<std::uint32_t>(in.at(8));
  Menu_BattleAbilities_abilityCount = 6;
  Menu_BattleAbilities_abilityMenuState = 0x12345678;
  Menu_BattleAbilities_selectedAbilityType = 25;
  Menu_BattleAbilities_highlightedAbility = 3;
  Menu_BattleAbilities_firstAbility = 2;
  Menu_BattleAbilities_abilityText = in.at(5) != 0 ? g_text.data() : nullptr;
  Input_settings.cursorMemory = Byte(static_cast<int>(in.at(2)));
  Menu_Framework_menuMemory.cursorMemories[type * 2] = Byte(static_cast<int>(in.at(3)));
  Menu_Framework_menuMemory.cursorMemories[type * 2 + 1] = Byte(static_cast<int>(in.at(4)));
  for (std::size_t i = 0; i < g_text.size(); ++i) {
    g_text[i] = static_cast<std::uint16_t>(seed + i * 31);
  }
  for (std::size_t i = 0; i < 256; ++i) {
    Battle_actionDefinitions[i] = {};
    // Unique byte costs make the nested Risk display identify its action.
    Battle_actionDefinitions[i].cost = static_cast<u_char>(i);
  }
  std::array<std::uint16_t*, 32> pointers{};
  for (int row = 0; row < count; ++row) {
    std::array<std::uint16_t, 14> name{};
    for (std::size_t i = 0; i < name.size(); ++i) {
      name[i] = static_cast<std::uint16_t>(seed ^ (row * 131 + i * 19));
    }
    auto* name_bytes = reinterpret_cast<unsigned char*>(Battle_actionDefinitions) +
                       row * sizeof(Battle_ActionDefinition) + 28;
    std::memcpy(name_bytes, name.data(), sizeof(name));
    auto* description = Menu_BattleAbilities_battleAbilityStrings + row * 50;
    for (std::size_t i = 0; i < 46; ++i) {
      description[i] = static_cast<std::uint16_t>(seed ^ (row * 313 + i * 23 + 0x1000));
    }
    if ((in[6] & (1ll << row)) == 0) {
      pointers[row * 2] = reinterpret_cast<std::uint16_t*>(name_bytes);
    }
    if ((in[7] & (1ll << row)) == 0) pointers[row * 2 + 1] = description;
  }
  for (std::size_t i = 0; i < 16; ++i) {
    Menu_BattleAbilities_unlockedChainAbilities[i] = Byte(24 + static_cast<int>(i));
    Menu_BattleAbilities_unlockedDefenseAbilities[i] = Byte(40 + static_cast<int>(i));
  }
  const int result = a::LiveCreateAbilityRowsEntry(count, type, pointers.data());
  Words out{
      result, static_cast<unsigned char>(Menu_BattleAbilities_abilityCount),
      Menu_BattleAbilities_abilityMenuState,
      static_cast<unsigned char>(Menu_BattleAbilities_selectedAbilityType),
      Menu_BattleAbilities_highlightedAbility,
      static_cast<unsigned char>(Menu_BattleAbilities_firstAbility),
      Menu_BattleAbilities_abilityText != nullptr ? 1 : 0};
  out.insert(out.end(), g_text.begin(), g_text.end());
  out.push_back(g_calls.size());
  for (const auto& call : g_calls) {
    out.push_back(call.size());
    out.insert(out.end(), call.begin(), call.end());
  }
  for (const auto word : out) std::cout << word << ' ';
  std::cout << '\n';
}
}  // namespace

namespace VS::Core {
bool IsLogEnabled(LogLevel, std::string_view) { return false; }
void LogWrite(LogLevel, std::string_view, std::string_view) {}
}  // namespace VS::Core

namespace VS::Core::Memory {
void* AllocateLiveFromHighEnd(std::size_t bytes, std::source_location) {
  g_calls.push_back({12, static_cast<std::int64_t>(bytes)});
  if (bytes > sizeof(g_text)) throw std::runtime_error("oversized allocation");
  return g_text.data();
}
}  // namespace VS::Core::Memory

namespace VS::Menu {
void InitLiveInformationTextBox(Menu_InformationBoxHeader) { Unexpected(); }
int DismissLiveTextBox(int) { Unexpected(); return 0; }
}  // namespace VS::Menu

namespace VS::Menu::Framework {
void InitializeLiveInformationBox() { Unexpected(); }
void SetLiveInformationMessage(char*) { Unexpected(); }
int LiveSelectedRow() { Unexpected(); return 0; }
Menu_Framework_MenuRow* SetLiveMenuItem(int, int, int, int, int, char*) {
  Unexpected(); return nullptr;
}
void CreateLiveMenuRows(int, int, char**, int*) { Unexpected(); }
void ClearLiveMenuExcept(int) { Unexpected(); }
void FlyOutLiveRowsAndHoistSelection(int, int) { Unexpected(); }
int AreLiveMenuRowsIdle() { Unexpected(); return 0; }
void SetLiveActionCostText(int index, const char* text, int x, int disabled) {
  if (disabled != 0) throw std::runtime_error("disabled Risk text");
  if (index == 1 && x == 8 && std::string_view(text) == "RISK") return;
  if (index != 0 || x != 72 || text[0] != '#') {
    throw std::runtime_error("invalid Risk panel");
  }
  g_calls.push_back({11, std::stoi(text + 1)});
}
}  // namespace VS::Menu::Framework

int main(int argc, char**) {
  if (argc != 2) return 2;
  try {
    std::array<std::uint16_t*, 32> pointers{};
    Menu_BattleAbilities_abilityCount = 91;
    Menu_BattleAbilities_abilityMenuState = 0x76543210;
    const auto refused = [](int count, std::uint16_t** text) {
      if (VS::Menu::BattleAbilities::LiveCreateAbilityRowsEntry(count, 24, text) != 0 ||
          Menu_BattleAbilities_abilityCount != 91 ||
          Menu_BattleAbilities_abilityMenuState != 0x76543210 || !g_calls.empty()) {
        throw std::runtime_error("Live entry failed its refusal contract");
      }
    };
    for (const int count : {-1, 0, 17, (std::numeric_limits<int>::max)()}) {
      refused(count, pointers.data());
    }
    refused(1, nullptr);
    if (Menu_BattleAbilities_abilityText != nullptr) {
      throw std::runtime_error("refused row allocation was published");
    }
    Menu_BattleAbilities_abilityText = g_text.data();
    refused((std::numeric_limits<int>::max)(), nullptr);
    if (Menu_BattleAbilities_abilityText != g_text.data()) {
      throw std::runtime_error("existing row allocation was changed");
    }
    Menu_BattleAbilities_abilityText = nullptr;
    std::string line;
    while (std::getline(std::cin, line)) {
      std::istringstream stream(line);
      Words in;
      std::int64_t word = 0;
      while (stream >> word) in.push_back(word);
      Run(in);
    }
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 3;
  }
}
