#pragma once

#include <cstdint>

struct Battle_ActionDefinition;
struct Battle_HitResult;

namespace VS::Battle {
std::int16_t LiveScalePreviousTargetHit(Battle_ActionDefinition*,
                                        Battle_HitResult*, Battle_HitResult*,
                                        int, int);
std::int16_t LiveScaleSourceMissingHp(Battle_ActionDefinition*,
                                      Battle_HitResult*, Battle_HitResult*, int,
                                      int);
std::int16_t LiveScaleTargetCurrentHp(Battle_ActionDefinition*,
                                      Battle_HitResult*, Battle_HitResult*, int,
                                      int);
std::int16_t LiveWeaponPpWithChainOffset(Battle_ActionDefinition*,
                                         Battle_HitResult*, Battle_HitResult*,
                                         int, int);
std::int16_t LiveScaleIncomingSpellDamage(Battle_ActionDefinition*,
                                          Battle_HitResult*, Battle_HitResult*,
                                          int, int);
std::int16_t LiveScaleSourceMissingMp(Battle_ActionDefinition*,
                                      Battle_HitResult*, Battle_HitResult*, int,
                                      int);
}  // namespace VS::Battle
