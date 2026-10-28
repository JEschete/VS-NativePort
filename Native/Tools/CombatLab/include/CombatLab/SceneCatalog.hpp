#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include "Battle/Encounter.hpp"
#include "nlohmann/json.hpp"

namespace VS::combat_lab {

enum class SceneFlow {
  kNext,
  kBranch,
  kJump,
  kLoop,
  kThread,
  kReturn,
  kFinish,
  kSpecial
};

enum class SceneOperation { kNone, kChainEvent, kChangeRoom };

struct SceneOpcode {
  std::uint8_t size = 0;
  SceneFlow flow = SceneFlow::kNext;
  std::uint8_t branch_operand = 0;
  SceneOperation operation = SceneOperation::kNone;
};

using SceneOpcodes = std::array<SceneOpcode, 256>;

nlohmann::json DecodeSceneScript(std::span<const std::uint8_t> bytes,
                                 const SceneOpcodes& opcodes);
nlohmann::json DecodeSceneTriggers(std::span<const std::uint8_t> bytes);
nlohmann::json ReadSceneCatalog(
    const std::vector<Game::EncounterZone>& catalog);

}  // namespace VS::combat_lab
