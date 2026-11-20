#pragma once

#include <cstdint>

namespace VS::Battle {

struct ActorStats;
[[nodiscard]] std::uint32_t LiveActionCostAndAvailability(
    std::uint32_t action, VS::Battle::ActorStats* actor, int update);
void LivePreviewActorActionCost(int action, int slot);
void LiveAddActionRecoveryDelay(std::uint32_t action,
                                VS::Battle::ActorStats* actor, int execute);
[[nodiscard]] int LiveActorActionRestrictionFlags(int slot, int action);
}  // namespace VS::Battle
