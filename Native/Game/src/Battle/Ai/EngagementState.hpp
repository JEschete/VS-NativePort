#pragma once

#include <cstdint>

struct Battle_Ai_RoomAiState;

namespace VS::Battle::ai {
std::uint16_t EngagedMask(const Battle_Ai_RoomAiState& room);
void StoreEngagedMask(Battle_Ai_RoomAiState& room, std::uint16_t mask);
}  // namespace VS::Battle::ai
