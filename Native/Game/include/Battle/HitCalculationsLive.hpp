#pragma once

#include <cstdint>

struct Battle_ActionDefinition;
namespace VS::Battle {
struct HitResult;
}  // namespace VS::Battle

namespace VS::Battle {
int LiveAgilityHitChance(Battle_ActionDefinition*, VS::Battle::HitResult*,
                         VS::Battle::HitResult*, int, int);
int LiveIntelligenceHitChance(Battle_ActionDefinition*, VS::Battle::HitResult*,
                              VS::Battle::HitResult*, int, int);
int LiveCostAdjustedAgilityHitChance(Battle_ActionDefinition*,
                                     VS::Battle::HitResult*,
                                     VS::Battle::HitResult*, int, int);
int LiveRollActionHit(Battle_ActionDefinition*, VS::Battle::HitResult*,
                      VS::Battle::HitResult*, int, int);
std::int16_t LiveCalculateActionStatChange(Battle_ActionDefinition*,
                                           VS::Battle::HitResult*,
                                           VS::Battle::HitResult*, int, int);

// Typed relocation guards for retired, unreachable original table sentinels.
// These reject invalid content; they do not implement a placeholder return.
[[noreturn]] int RejectLiveHitSelector(Battle_ActionDefinition*,
                                       VS::Battle::HitResult*,
                                       VS::Battle::HitResult*, int, int);
[[noreturn]] std::int16_t RejectLiveStatSelector(Battle_ActionDefinition*,
                                                 VS::Battle::HitResult*,
                                                 VS::Battle::HitResult*, int,
                                                 int);
[[noreturn]] void RejectLiveEffectSelector(Battle_ActionDefinition*,
                                           VS::Battle::HitResult*,
                                           VS::Battle::HitResult*, int, int);
}  // namespace VS::Battle
