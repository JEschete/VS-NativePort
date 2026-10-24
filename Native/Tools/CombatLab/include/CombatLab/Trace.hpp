#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "Battle/Encounter.hpp"
#include "CombatLab/DoorTests.hpp"
#include "Diagnostics/GameplayTrace.hpp"
#include "nlohmann/json.hpp"

namespace VS::combat_lab {

inline constexpr std::uint64_t kScriptStopGraceTicks = 600;

using Trace = Game::GameplayTrace;
using Game::EncodeActor;

nlohmann::json EncodeSetup(const Game::EncounterSetup& setup);
Game::EncounterSetup DecodeSetup(const nlohmann::json& json);

struct ScriptPress {
  std::uint64_t tick = 0;
  std::uint32_t buttons = 0;
};

struct EncounterScript {
  std::string name;
  std::filesystem::path scenario;
  Game::EncounterSetup setup;
  std::uint64_t frames = 0;
  std::vector<ScriptPress> presses;
  std::optional<DoorTestCase> door;
};

std::uint64_t ParseTick(std::string_view value);
std::vector<ScriptPress> DecodePresses(std::string_view value);
std::vector<EncounterScript> ReadScriptSequence(
    const std::filesystem::path& path);

}  // namespace VS::combat_lab
