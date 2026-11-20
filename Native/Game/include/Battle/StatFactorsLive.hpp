#pragma once

#include <cstdint>

namespace VS::Battle {

struct ActionDefinition;
struct HitResult;
}  // namespace VS::Battle

namespace VS::Battle {
std::int16_t LiveScalePreviousTargetHit(VS::Battle::ActionDefinition*,
                                        VS::Battle::HitResult*,
                                        VS::Battle::HitResult*, int, int);
std::int16_t LiveScaleSourceMissingHp(VS::Battle::ActionDefinition*,
                                      VS::Battle::HitResult*,
                                      VS::Battle::HitResult*, int, int);
std::int16_t LiveScaleTargetCurrentHp(VS::Battle::ActionDefinition*,
                                      VS::Battle::HitResult*,
                                      VS::Battle::HitResult*, int, int);
std::int16_t LiveWeaponPpWithChainOffset(VS::Battle::ActionDefinition*,
                                         VS::Battle::HitResult*,
                                         VS::Battle::HitResult*, int, int);
std::int16_t LiveScaleIncomingSpellDamage(VS::Battle::ActionDefinition*,
                                          VS::Battle::HitResult*,
                                          VS::Battle::HitResult*, int, int);
std::int16_t LiveScaleSourceMissingMp(VS::Battle::ActionDefinition*,
                                      VS::Battle::HitResult*,
                                      VS::Battle::HitResult*, int, int);
std::int16_t LiveScaleIncomingPhysicalDamage(VS::Battle::ActionDefinition*,
                                             VS::Battle::HitResult*,
                                             VS::Battle::HitResult*, int, int);
std::int16_t LiveTargetMissingHp(VS::Battle::ActionDefinition*,
                                 VS::Battle::HitResult*, VS::Battle::HitResult*,
                                 int, int);
std::int16_t LiveTargetMissingMp(VS::Battle::ActionDefinition*,
                                 VS::Battle::HitResult*, VS::Battle::HitResult*,
                                 int, int);
std::int16_t LiveScaleIncomingSpellCost(VS::Battle::ActionDefinition*,
                                        VS::Battle::HitResult*,
                                        VS::Battle::HitResult*, int, int);
std::int16_t LiveScaleIncomingDamage(VS::Battle::ActionDefinition*,
                                     VS::Battle::HitResult*,
                                     VS::Battle::HitResult*, int, int);
std::int16_t LiveSourceShieldPp(VS::Battle::ActionDefinition*,
                                VS::Battle::HitResult*, VS::Battle::HitResult*,
                                int, int);
}  // namespace VS::Battle
