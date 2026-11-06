// Hand-maintained (tools/port/owned.csv).
// The battle-actor rules of the actor state code (BATTLE.PRG
// 146C.c:8686-9230).
#include "Actors/BattleActorState.hpp"

#include <cstdint>

#include "Core/Log.hpp"

namespace VS::Actors {
namespace {
constexpr int kFullPercentage = 100;
constexpr int kFirstOtherActor = 1;

BattleActorState* KnownActor(BattleActorHost& host, int id) {
  if (!IsBattleActorSlot(id)) {
    VS_LOG_WARN("actors", "Battle actor {} is outside the actor table", id);
    return nullptr;
  }
  return host.Actor(id);
}
}  // namespace

bool IsBattleActorSlot(int id) { return id >= 0 && id < kBattleActorSlots; }

void SetActorHostile(BattleActorHost& host, int id, bool hostile) {
  BattleActorState* actor = KnownActor(host, id);
  if (actor == nullptr) return;
  actor->kind = hostile ? kHostileActorKind : kPeacefulActorKind;
}

int HpPercentage(int current_hp, int max_hp) {
  if (current_hp <= 0) return 0;
  if (max_hp == 0) {
    VS_LOG_WARN("actors",
                "An actor with {} HP has no maximum HP; reporting 1 percent",
                current_hp);
    return 1;
  }
  const int percentage = current_hp * kFullPercentage / max_hp;
  return percentage == 0 ? 1 : percentage;
}

int ActorHpPercentage(BattleActorHost& host, int id) {
  if (KnownActor(host, id) == nullptr) {
    VS_LOG_WARN("actors", "No battle actor {} for the HP percentage", id);
    return 0;
  }
  const ActorVitals* vitals = host.Vitals(id);
  if (vitals == nullptr) return 0;
  return HpPercentage(vitals->current_hp, vitals->max_hp);
}

void RestoreActorVitals(BattleActorHost& host, int id) {
  if (KnownActor(host, id) == nullptr) return;
  ActorVitals* vitals = host.Vitals(id);
  if (vitals == nullptr) return;
  vitals->risk = 0;
  vitals->action_recovery_timer = 0;
  vitals->current_hp = vitals->max_hp;
  vitals->current_mp = vitals->max_mp;
  for (LimbVitals& limb : vitals->limbs) limb.hp = limb.max_hp;
  host.ClearStatuses(id);
}

int FindActorByModelId(BattleActorHost& host, int model_id) {
  for (int id = 0; id < kBattleActorScanSlots; ++id) {
    const BattleActorState* actor = host.Actor(id);
    if (actor != nullptr && static_cast<int>(actor->model_id) == model_id) {
      return id;
    }
  }
  return kModelNotFoundSlot;
}

int CountActorsByDisposition(BattleActorHost& host, int kind) {
  int count = 0;
  for (int id = 0; id < kBattleActorScanSlots; ++id) {
    const BattleActorState* actor = host.Actor(id);
    if (actor != nullptr && static_cast<int>(actor->kind) == kind) ++count;
  }
  return count;
}

bool IsActorRetainedAfterDefeat(BattleActorHost& host, int id) {
  const BattleActorState* actor = KnownActor(host, id);
  return actor != nullptr && actor->retain_after_defeat;
}

void SetActorLogicEnabled(BattleActorHost& host, int id, bool enabled) {
  BattleActorState* actor = KnownActor(host, id);
  if (actor == nullptr) return;
  if (enabled) {
    if (actor->command_state == kLogicDisabledCommand) {
      actor->command_state = 0;
      actor->kind &= static_cast<std::uint16_t>(~kLogicDisabledKind);
    }
    return;
  }
  actor->command_state = kLogicDisabledCommand;
  actor->kind |= kLogicDisabledKind;
  host.SetMarker(id, kClearMarkerKind);
  host.SetMarker(id, kClearPinnedMarker);
}

void SetOtherActorLogicEnabled(BattleActorHost& host, bool enabled) {
  for (int id = kFirstOtherActor; id < kBattleActorScanSlots; ++id) {
    SetActorLogicEnabled(host, id, enabled);
  }
}

void SetActorDetached(BattleActorHost& host, int id, bool detached) {
  if (KnownActor(host, id) == nullptr) return;
  if (detached) {
    const bool list_changed = host.UnlinkFromRoomList(id);
    host.SetAnimationLocked(id, true);
    if (list_changed) {
      host.ClearAiLinksAndPlan(id);
      host.RebuildActionPlans();
    }
    return;
  }
  const bool list_changed = host.LinkAfterAshley(id);
  host.SetAnimationLocked(id, false);
  if (list_changed) {
    host.ResolveAiLinkedActors();
    host.RebuildActionPlans();
  }
}
}  // namespace VS::Actors
