#include <stdint.h>

#include "Actors/ActorRecord.hpp"
#include "Battle/Action.hpp"
#include "Battle/ActionFlow.hpp"
#include "Battle/ActionResultsLive.hpp"
#include "Battle/Actor.hpp"
#include "Battle/Damage.hpp"
#include "Battle/Feedback.hpp"
#include "Legacy/Psx.hpp"

using VS::Battle::ActionRequestRecord;
using VS::Battle::ActionState;

int Tests_CombatTargetResults(void) {
  enum { kInitialLimbHp = 20 };
  ActionState actionState = {0};
  VS::Battle::Actor target = {nullptr};
  VS::Battle::ActorStats stats = {0};
  VS::Battle::FloatingLabelSlots labels{};
  ActionState* saved_action = VS::Battle::actionState;
  VS::Battle::Actor* saved_target = VS::Battle::actors[2];
  VS::Actors::ActorRecord* savedModel = VS::Actors::actorRecords[2];
  auto* saved_labels = VS::Battle::floatingLabels;
  const int16_t saved_label_count = VS::Battle::floatingLabelCount;
  target.id = 2;
  target.room_spawn_index = 0x80;
  target.stats = &stats;
  stats.current_hp = stats.max_hp = 63;
  stats.current_mp = stats.max_mp = 40;
  for (size_t limb = 0; limb < sizeof stats.limbs / sizeof stats.limbs[0];
       ++limb) {
    stats.limbs[limb].hp = stats.limbs[limb].max_hp = kInitialLimbHp;
  }
  VS::Battle::actors[2] = &target;
  VS::Actors::actorRecords[2] = nullptr;
  VS::Battle::floatingLabels = &labels;
  VS::Battle::floatingLabelCount = 0;
  VS::Battle::actionState = &actionState;
  actionState.queue_index = 5;
  ActionRequestRecord* action = &actionState.queue[1];
  action->action_id = 1;
  action->target_count = 3;
  VS::Battle::HitResult* hit = &action->targets[0];
  hit->target.target_actor = 2;
  hit->target.target_limb = 0;
  hit->target.hp = 12;
  hit->hp_effect = 1;
  hit->limbs[0].effects.opaque0 = 4;
  action->targets[1] = *hit;
  action->targets[1].target.target_actor = 3;
  action->targets[2] = *hit;
  action->targets[2].targets_position = 1;
  VS::Battle::ApplyLiveActionTargetEffects(2, nullptr);
  int passed = stats.current_hp == 51 && stats.limbs[0].hp == 16 &&
               stats.limbs[1].hp == 20;
  hit->target.hp = 100;
  VS::Battle::ApplyLiveActionTargetEffects(2, nullptr);
  passed = passed && stats.current_hp == 0;
  hit->target.impact_kind = 2;
  hit->hp_effect = 2;
  hit->mp_effect = 1;
  hit->target.mp = 50;
  VS::Battle::ApplyLiveActionTargetEffects(2, nullptr);
  passed = passed && stats.current_hp == 63 && stats.current_mp == 0;
  hit->hp_effect = 0;
  hit->mp_effect = 2;
  VS::Battle::ApplyLiveActionTargetEffects(2, nullptr);
  passed = passed && stats.current_mp == 40;
  VS::Battle::actionState = saved_action;
  VS::Battle::actors[2] = saved_target;
  VS::Actors::actorRecords[2] = savedModel;
  VS::Battle::floatingLabels = saved_labels;
  VS::Battle::floatingLabelCount = saved_label_count;
  return passed;
}
