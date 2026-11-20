#pragma once
// Hand-maintained (tools/port/owned.csv).

// Live binding for the floating label producers (FloatingLabelProducers.hpp)
// over VS::Battle::floatingLabels, the floating label queue's live reserve,
// remove and commit (FloatingLabelQueueLive.hpp), the actor skeleton's bone
// services (SkeletonLive.hpp) and the BIOS rand(). The entries keep the
// generated Battle_* signatures for the callers and the module's function
// pointers. A record outside the 32-record owner is refused with a diagnostic;
// queue count and slot corruption stay with the queue binding, which aborts.
namespace VS::Battle {
// func_80093B68 (2842C.c:1544).
int LiveQueueActorValueLabel(int actor_slot, int limb_tag,
                             unsigned int encoded_amount, int label_flags);
// vs_battle_renderBattleAbilityTimingResult (2842C.c:1643).
int LiveQueueAbilityTimingResult(int timing_result);
// func_80093F24 (2842C.c:1666).
int LiveQueueChainCount(int chain_count);
// func_80093FEC (2842C.c:1695): `arg1` is unused, as in the original.
int LiveQueueActorStatusLabels(int actor_slot, int arg1, int status_mask,
                               int label_flags);
// func_8009406C (2842C.c:1708).
int LiveQueueActorStatusLabel(int actor_slot, int bone_tag, int status_index,
                              int label_flags);
}  // namespace VS::Battle
