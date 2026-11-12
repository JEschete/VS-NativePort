#pragma once
// Hand-maintained (tools/port/owned.csv).

struct Battle_ActorStats;

namespace VS::Battle {
void ApplyLiveWeaponElementEnchantment(Battle_ActorStats* stats, int increased,
                                       int decreased, int amount);
void ApplyLiveArmorElementEnchantment(Battle_ActorStats* stats, int increased,
                                      int decreased, int amount);
void QueueLiveActorStatChangeNumber(int actor_slot, int limb_tag,
                                    int encoded_amount, int label_flags);
void UpdateLiveTargetingRiskPenalty();
void ResetLiveTargetingRiskPenaltyTimer();
}  // namespace VS::Battle
