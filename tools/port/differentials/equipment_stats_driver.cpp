#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <iterator>
#include <sstream>
#include <vector>

#include "Items/ItemRecordsLive.hpp"
#include "Menu/Status/EquipmentStats.hpp"

extern "C" {
#include "Items/Loadout.h"
}

namespace {
using VS::Menu::Framework::EquipmentDetailsState;

template <typename Legacy>
Legacy Record(const std::vector<std::uint8_t>& bytes) {
  Legacy legacy{};
  std::memcpy(&legacy, bytes.data(), sizeof(legacy));
  return legacy;
}

void Print(const EquipmentDetailsState& state) {
  bool first = true;
  const auto write = [&first](int value) {
    if (!first) std::cout << ' ';
    first = false;
    std::cout << value;
  };
  for (const auto value : state.stats) write(value);
  for (const auto value : state.primary) write(value);
  for (std::size_t i = 0; i < 4; ++i) write(state.range_risk[i]);
  write(state.current_dp);
  write(state.max_dp);
  write(state.current_pp);
  write(state.max_pp);
  write(state.subtype);
  write(state.enabled_pages);
  std::cout << '\n';
}
}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) return 2;
  std::string line;
  while (std::getline(std::cin, line)) {
    std::istringstream input(line);
    int kind = 0;
    int equipped = 0;
    if (!(input >> kind >> equipped) || kind < 0 || kind > 6) return 3;

    EquipmentDetailsState state{};
    int value = 0;
    for (auto& stat : state.stats) {
      if (!(input >> value)) return 4;
      stat = static_cast<std::int16_t>(value);
    }
    for (auto& stat : state.primary) {
      if (!(input >> value)) return 5;
      stat = static_cast<std::int16_t>(value);
    }
    for (std::size_t i = 0; i < 4; ++i) {
      if (!(input >> value)) return 6;
      state.range_risk[i] = static_cast<std::uint8_t>(value);
    }
    std::array<std::uint16_t, 4> dp_pp{};
    for (auto& field : dp_pp) {
      if (!(input >> value)) return 7;
      field = static_cast<std::uint16_t>(value);
    }
    state.current_dp = dp_pp[0];
    state.max_dp = dp_pp[1];
    state.current_pp = dp_pp[2];
    state.max_pp = dp_pp[3];

    constexpr std::array<std::size_t, 7> record_sizes{
        sizeof(Items_WeaponLoadout), sizeof(Items_ShieldLoadout),
        sizeof(Items_ArmorLoadout), sizeof(Items_AccessorySpawnLoadout),
        sizeof(Items_WeaponLoadout), sizeof(Items_EquipmentPartStats),
        sizeof(Items_EquipmentPartStats)};
    std::vector<std::uint8_t> bytes(record_sizes[kind]);
    for (auto& byte : bytes) {
      if (!(input >> value)) return 8;
      byte = static_cast<std::uint8_t>(value);
    }

    switch (kind) {
      case 0: {
        const auto typed =
            VS::Items::Live::Typed(Record<Items_WeaponLoadout>(bytes));
        const auto effects =
            VS::Menu::Status::PopulateWeaponStats(state, typed, equipped != 0);
        state.enabled_pages = static_cast<std::uint8_t>(effects.enabled_pages);
        break;
      }
      case 1: {
        const auto typed =
            VS::Items::Live::Typed(Record<Items_ShieldLoadout>(bytes));
        const auto effects =
            VS::Menu::Status::PopulateShieldStats(state, typed, equipped != 0);
        state.enabled_pages = static_cast<std::uint8_t>(effects.enabled_pages);
        break;
      }
      case 2: {
        const auto typed =
            VS::Items::Live::Typed(Record<Items_ArmorLoadout>(bytes));
        const auto effects =
            VS::Menu::Status::PopulateArmorStats(state, typed, equipped != 0);
        state.enabled_pages = static_cast<std::uint8_t>(effects.enabled_pages);
        break;
      }
      case 3: {
        const auto typed = VS::Items::Live::Typed(
            Record<Items_AccessorySpawnLoadout>(bytes));
        const auto effects =
            VS::Menu::Status::PopulateAccessoryStats(state, typed);
        state.enabled_pages = static_cast<std::uint8_t>(effects.enabled_pages);
        break;
      }
      case 4: {
        const auto typed =
            VS::Items::Live::Typed(Record<Items_WeaponLoadout>(bytes));
        const auto effects = VS::Menu::Status::PopulateBladeStats(
            state, typed.blade, typed.current_pp, typed.max_pp);
        state.enabled_pages = static_cast<std::uint8_t>(effects.enabled_pages);
        break;
      }
      case 5: {
        const auto typed =
            VS::Items::Live::Typed(Record<Items_EquipmentPartStats>(bytes));
        const auto effects = VS::Menu::Status::PopulateGripStats(state, typed);
        state.enabled_pages = static_cast<std::uint8_t>(effects.enabled_pages);
        break;
      }
      case 6: {
        const auto typed =
            VS::Items::Live::Typed(Record<Items_EquipmentPartStats>(bytes));
        const auto effects = VS::Menu::Status::PopulateGemStats(state, typed);
        state.enabled_pages = static_cast<std::uint8_t>(effects.enabled_pages);
        break;
      }
      default:
        return 9;
    }
    Print(state);
  }
}
