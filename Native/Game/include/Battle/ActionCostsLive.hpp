#pragma once

#include <cstdint>

struct Battle_ActorStats;

namespace VS::Battle {
[[nodiscard]] std::uint32_t LiveActionCostAndAvailability(
    std::uint32_t action, Battle_ActorStats* actor, int update);
void LivePreviewActorActionCost(int action, int slot);
void LiveAddActionRecoveryDelay(std::uint32_t action, Battle_ActorStats* actor,
                                int execute);
[[nodiscard]] int LiveActorActionRestrictionFlags(int slot, int action);
}  // namespace VS::Battle
