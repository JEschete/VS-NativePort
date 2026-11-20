#pragma once

#include <cstdint>

struct Battle_ActionDefinition;
namespace VS::Battle {
struct HitResult;
}  // namespace VS::Battle

namespace VS::Battle {
std::int16_t LiveScalePreviousTargetHit(Battle_ActionDefinition*,
                                        VS::Battle::HitResult*,
                                        VS::Battle::HitResult*, int, int);
std::int16_t LiveScaleSourceMissingHp(Battle_ActionDefinition*,
                                      VS::Battle::HitResult*,
                                      VS::Battle::HitResult*, int, int);
std::int16_t LiveScaleTargetCurrentHp(Battle_ActionDefinition*,
                                      VS::Battle::HitResult*,
                                      VS::Battle::HitResult*, int, int);
std::int16_t LiveWeaponPpWithChainOffset(Battle_ActionDefinition*,
                                         VS::Battle::HitResult*,
                                         VS::Battle::HitResult*, int, int);
std::int16_t LiveScaleIncomingSpellDamage(Battle_ActionDefinition*,
                                          VS::Battle::HitResult*,
                                          VS::Battle::HitResult*, int, int);
std::int16_t LiveScaleSourceMissingMp(Battle_ActionDefinition*,
                                      VS::Battle::HitResult*,
                                      VS::Battle::HitResult*, int, int);
std::int16_t LiveScaleIncomingPhysicalDamage(Battle_ActionDefinition*,
                                             VS::Battle::HitResult*,
                                             VS::Battle::HitResult*, int, int);
std::int16_t LiveTargetMissingHp(Battle_ActionDefinition*,
                                 VS::Battle::HitResult*, VS::Battle::HitResult*,
                                 int, int);
std::int16_t LiveTargetMissingMp(Battle_ActionDefinition*,
                                 VS::Battle::HitResult*, VS::Battle::HitResult*,
                                 int, int);
std::int16_t LiveScaleIncomingSpellCost(Battle_ActionDefinition*,
                                        VS::Battle::HitResult*,
                                        VS::Battle::HitResult*, int, int);
std::int16_t LiveScaleIncomingDamage(Battle_ActionDefinition*,
                                     VS::Battle::HitResult*,
                                     VS::Battle::HitResult*, int, int);
std::int16_t LiveSourceShieldPp(Battle_ActionDefinition*,
                                VS::Battle::HitResult*, VS::Battle::HitResult*,
                                int, int);
}  // namespace VS::Battle
