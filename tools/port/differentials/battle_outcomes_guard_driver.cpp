#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string_view>
#include <vector>

#include "Actors/ActorStateLive.hpp"
#include "Actors/AnimationControlLive.hpp"
#include "Audio/SfxLive.hpp"
#include "Battle/ActionOutcomesLive.hpp"
#include "Battle/ActionResultsLive.hpp"
#include "Battle/Ai/ActorCleanupLive.hpp"
#include "Items/LootLive.hpp"
#include "State/KillScoreLive.hpp"
extern "C" {
#include "Actors/Location.h"
#include "Battle/Action.h"
#include "Battle/ActionFlow.h"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Battle/Feedback.h"
#include "State/Progress.h"
Battle_Actor* Battle_actors[42]{};
Battle_ActionState* Battle_actionState{};
Battle_ActionDefinition Battle_actionDefinitions[256]{};
char State_learnedActionFlags[32]{};
void Core_Errorf(const char*, const char* message, ...) {
  std::fprintf(stderr, "%s\n", message);
  std::_Exit(85);
}
}

namespace {
namespace b = VS::Battle;
using Event = std::array<int, 5>;
std::vector<Event> events;
Battle_ActionState first{}, replacement{};
Battle_Actor actor{}, other{};
Battle_ActorStats stats{};
std::string_view selected;
bool valid = true;
int selected_actor = 1;
void Record(int operation, int a = 0, int x = 0, int y = 0, int z = 0) {
  events.push_back({operation, a, x, y, z});
}
void ChangePrimary() {
  first.queue[3].primaryTarget.target.targetActor = 41;
  first.queue[3].primaryTarget.target.impactKind = 255;
  first.queue[3].primaryTarget.targetsPosition = 1;
  first.queue[3].primaryTarget.weaponDpChange = -99;
  Battle_actionState = &replacement;
}
}  // namespace

namespace VS::State {
void RecordLiveEnemyKillScore(Battle_Actor* enemy) {
  valid &= enemy == &actor && actor.defeated == 0;
  Record(1);
  Battle_actors[1] = &other;
  actor.roomSpawnIndex = 0x80;
  actor.defeated = 255;
}
void UpdateLiveWeaponKillStreak(int unused) {
  valid &= unused == 0 && actor.defeated == 255;
  Record(2);
}
}
namespace VS::Items {
void QueueLiveEnemyLoot(const Battle_Actor* enemy) {
  valid &= enemy == &actor;
  if (selected == "enemy-cached") valid &= actor.defeated == 255;
  Record(3);
  actor.defeated = 0;
}
}
namespace VS::Actors {
int LiveActorPlacement(int slot, Actors_ActorLocationQuery* output) {
  Record(4, slot);
  if (selected == "missing-placement") return -1;
  output->location.position = {static_cast<short>(slot), -22, 33, 99};
  if (selected == "sounds-cached" && slot == 7) {
    auto& action = first.queue[3];
    action.targetCount = 2;
    action.targets[0].target.impactKind = 0;
    action.targets[0].targetsPosition = 1;
    action.targets[1].target.targetActor = 8;
    action.targets[1].target.impactKind = 4;
    Battle_actionState = &replacement;
  }
  return 0;
}
void PlayLiveActorActionPose(int slot, const std::int16_t* point, int pose) {
  valid &= slot == 0 && point == nullptr && pose == 6;
  Record(7, slot, pose);
  ChangePrimary();
}
void BeginLiveActorDeath(int slot, void* context, int animate) {
  valid &= slot == selected_actor && animate == 1;
  valid &= context == &replacement.rangeOrigin;
  valid &= other.commandState == 0x20;
  Record(10, slot);
  Battle_actors[slot] = &actor;
  actor.roomSpawnIndex = 1;
}
void SetLiveActorAnimationLocked(int slot, int locked) {
  valid &= slot == selected_actor && locked == 1;
  Record(11, slot, locked);
  Battle_actors[slot] = &other;
  other.roomSpawnIndex =
      selected == "death-current" || selected == "death-zero" ? 0x80 : 1;
}
}
namespace VS::Audio {
void LivePlayWorldSfx(int group, int sound, SVECTOR* point) {
  valid &= group == 0x7E;
  Record(5, sound, point->vx, point->vy, point->vz);
  if (selected == "sounds-cached" && sound == 0x47) {
    Battle_actors[41] = &other;
    other.equippedWeaponCategory = 6;
    first.queue[3].targets[0].target.targetActor = 255;
    first.queue[3].targets[1].target.targetActor = 9;
    first.queueIndex = 0;
  }
}
}
namespace VS::Battle::Ai {
void RefreshLiveActorAiReservations(int slot) {
  Record(8, slot);
  if (selected == "refresh-create") Battle_actors[slot] = &actor;
  if (selected == "refresh-remove") Battle_actors[slot] = nullptr;
}
}
namespace VS::Battle {
void ApplyLiveActorActionResult(Battle_HitResult* hit) {
  valid &= hit == &first.queue[3].primaryTarget;
  valid &= hit->target.targetActor == 41 && hit->target.impactKind == 255;
  valid &= hit->targetsPosition == 1 && hit->weaponDpChange == -99;
  Record(12);
}
}
extern "C" {
void Battle_PlayActorDamageReaction(int slot, SVECTOR* point) {
  valid &= slot == 0 && point == &first.queue[3].focusPosition;
  Record(6, slot, point->vx, point->vy, point->vz);
  ChangePrimary();
}
void Battle_SetActorMarker(int slot, int marker) {
  valid &= slot == selected_actor && marker == 0 && actor.commandState == 0;
  Record(9, slot, marker);
  if (selected == "missing-after-marker") {
    Battle_actors[slot] = nullptr;
    return;
  }
  if (selected != "missing-death-state") {
    Battle_actors[slot] = &other;
    other.retainAfterDefeat = selected == "retained" ? 255 : 0;
    Battle_actionState = selected == "retained" ? nullptr : &replacement;
  }
}
}

int main(int argc, char** argv) {
  if (argc != 2) return 2;
  selected = argv[1];
  actor.stats = &stats;
  actor.roomSpawnIndex = 1;
  actor.commandState = 77;
  other.commandState = 99;
  other.defeated = 77;
  Battle_actors[1] = &actor;
  if (selected == "missing-enemy") {
    b::ResolveLiveEnemyDefeat(0, 2, 1);
    return 86;
  }
  if (selected == "enemy-unused") {
    actor.stats = nullptr;
    actor.roomSpawnIndex = 0x80;
    b::ResolveLiveEnemyDefeat(-256, 1, -1);
    return valid && events.empty() && actor.defeated == 0 ? 0 : 1;
  }
  if (selected == "enemy-cached") {
    b::ResolveLiveEnemyDefeat(0, 1, -256);
    return valid && actor.defeated == 1 && other.defeated == 77 &&
                   events == std::vector<Event>{{1}, {2}, {3}} ? 0 : 1;
  }
  if (selected == "enemy-full-source") {
    b::ResolveLiveEnemyDefeat(256, 1, 1);
    return valid && actor.defeated == 1 && events == std::vector<Event>{{3}} ? 0 : 1;
  }
  if (selected == "missing-sound-state") {
    b::PlayLiveWeaponHitSounds(0);
    return 86;
  }
  if (selected == "source-unused-state") {
    b::ApplyLiveActionSourceEffects(16, nullptr);
    b::ApplyLiveActionSourceEffects(256, reinterpret_cast<SVECTOR*>(1));
    return valid && events.empty() ? 0 : 1;
  }
  if (selected == "source-negative-missing") {
    b::ApplyLiveActionSourceEffects(-1, nullptr);
    return 86;
  }
  first.queueIndex = 0xFFFFFFFF;
  Battle_actionState = &first;
  auto& action = first.queue[3];
  if (selected == "bad-count") {
    action.targetCount = 256;
    b::PlayLiveWeaponHitSounds(0);
    return 86;
  }
  if (selected == "missing-placement") {
    action.targetCount = 1;
    action.targets[0].target.impactKind = 5;
    b::PlayLiveWeaponHitSounds(-1000);
    return 86;
  }
  if (selected == "sounds-unused-source") {
    valid &= b::PlayLiveWeaponHitSounds(-1000) == 1;
    action.targetCount = 3;
    action.targets[0].targetsPosition = -1;
    action.targets[1].target.impactKind = 3;
    action.targets[2].target.impactKind = 255;
    valid &= b::PlayLiveWeaponHitSounds(256) == 1;
    return valid && events == std::vector<Event>{{4}, {5, 0x33, 0, -22, 33}, {4}} ? 0 : 1;
  }
  if (selected == "sounds-cached") {
    action.targetCount = 1;
    action.targets[0].target.targetActor = 7;
    action.targets[0].target.impactKind = 5;
    actor.equippedWeaponCategory = 1;
    Battle_actors[41] = &actor;
    const int result = b::PlayLiveWeaponHitSounds(41);
    return valid && result == 1 &&
                   events == std::vector<Event>{{4, 7}, {5, 0x47, 7, -22, 33},
                                                 {5, 0x48, 7, -22, 33}, {4, 9},
                                                 {5, 0x4D, 9, -22, 33}} ? 0 : 1;
  }
  if (selected == "source-reaction" || selected == "source-recovery") {
    action.primaryTarget.target.impactKind = selected == "source-recovery" ? 5 : 0;
    action.focusPosition = {11, -22, 33, 44};
    b::ApplyLiveActionSourceEffects(0, reinterpret_cast<SVECTOR*>(1));
    const Event expected = selected == "source-reaction" ? Event{6, 0, 11, -22, 33}
                                                         : Event{7, 0, 6};
    return valid && events == std::vector<Event>{expected, {12}} ? 0 : 1;
  }
  if (selected == "source-negative") {
    action.primaryTarget.target.targetActor = 255;
    b::ApplyLiveActionSourceEffects(-1, nullptr);
    return valid && events.empty() ? 0 : 1;
  }
  if (selected == "refresh-create" || selected == "refresh-remove") {
    Battle_actionState = nullptr;
    stats.currentHp = -1;
    if (selected == "refresh-create") Battle_actors[1] = nullptr;
    const int result = b::BeginLiveActorDefeatAnimation(1);
    return valid && result == 0 && actor.commandState ==
                       (selected == "refresh-create" ? 0 : 77) &&
                   events == std::vector<Event>{{8, 1}} ? 0 : 1;
  }
  if (selected == "missing-stats") {
    actor.stats = nullptr;
    b::BeginLiveActorDefeatAnimation(1);
    return 86;
  }
  if (selected == "retained" || selected == "missing-death-state" ||
      selected == "missing-after-marker") {
    Battle_actionState = nullptr;
    const int result = b::BeginLiveActorDefeatAnimation(1);
    if (selected != "retained") return 86;
    return valid && result == 0 && actor.commandState == 0 && other.commandState == 0x20 &&
                   events == std::vector<Event>{{8, 1}, {9, 1}} ? 0 : 1;
  }
  if (selected == "death-current" || selected == "death-zero" || selected == "death-new-spawn") {
    selected_actor = selected == "death-zero" ? 0 : 1;
    Battle_actors[selected_actor] = &actor;
    if (selected == "death-zero") other.roomSpawnIndex = 0x80;
    const int result = b::BeginLiveActorDefeatAnimation(selected_actor);
    return valid && result == (selected == "death-current" ? 0 : 1) &&
                   events == std::vector<Event>{{8, selected_actor}, {9, selected_actor},
                                                 {10, selected_actor}, {11, selected_actor, 1}}
               ? 0 : 1;
  }
  return 2;
}
