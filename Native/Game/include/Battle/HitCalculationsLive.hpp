#pragma once

#include <cstdint>

namespace VS::Battle {

struct ActionDefinition;
struct HitResult;
}  // namespace VS::Battle

namespace VS::Battle {
int LiveAgilityHitChance(VS::Battle::ActionDefinition*, VS::Battle::HitResult*,
                         VS::Battle::HitResult*, int, int);
int LiveIntelligenceHitChance(VS::Battle::ActionDefinition*,
                              VS::Battle::HitResult*, VS::Battle::HitResult*,
                              int, int);
int LiveCostAdjustedAgilityHitChance(VS::Battle::ActionDefinition*,
                                     VS::Battle::HitResult*,
                                     VS::Battle::HitResult*, int, int);
int LiveRollActionHit(VS::Battle::ActionDefinition*, VS::Battle::HitResult*,
                      VS::Battle::HitResult*, int, int);
std::int16_t LiveCalculateActionStatChange(VS::Battle::ActionDefinition*,
                                           VS::Battle::HitResult*,
                                           VS::Battle::HitResult*, int, int);

// Typed relocation guards for retired, unreachable original table sentinels.
// These reject invalid content; they do not implement a placeholder return.
[[noreturn]] int RejectLiveHitSelector(VS::Battle::ActionDefinition*,
                                       VS::Battle::HitResult*,
                                       VS::Battle::HitResult*, int, int);
[[noreturn]] std::int16_t RejectLiveStatSelector(VS::Battle::ActionDefinition*,
                                                 VS::Battle::HitResult*,
                                                 VS::Battle::HitResult*, int,
                                                 int);
[[noreturn]] void RejectLiveEffectSelector(VS::Battle::ActionDefinition*,
                                           VS::Battle::HitResult*,
                                           VS::Battle::HitResult*, int, int);
}  // namespace VS::Battle
