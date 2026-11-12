// Standalone checked-live probe: diagnostic exit 85; missed refusal exit 86.
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <type_traits>

#include "Battle/StatFactorsLive.hpp"
extern "C" {
#include "Battle/Action.h"
#include "Battle/ActionFlow.h"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Battle/Hit.h"
Battle_Actor* Battle_actors[42]{};
Battle_ActionState* Battle_actionState{};
Battle_ActionDefinition Battle_actionDefinitions[256]{};
short (*Battle_statCalculators[27])(Battle_ActionDefinition*, Battle_HitResult*,
                                   Battle_HitResult*, int, int){};
void Core_Errorf(const char*, const char* message, ...) {
  std::fprintf(stderr, "%s\n", message);
  std::_Exit(85);
}
}
namespace {
unsigned refusals{};
int selected = -1;
template<class F> void Refuses(F operation) {
  if (static_cast<int>(refusals++) == selected) {
    operation();
    std::_Exit(86);
  }
}
}
#if defined(PROBE_WRONG_INT)
int Wrong(Battle_ActionDefinition*, Battle_HitResult*, Battle_HitResult*, int, int);
std::remove_reference_t<decltype(Battle_statCalculators[0])> wrong = Wrong;
#elif defined(PROBE_WRONG_VOID)
void Wrong(Battle_ActionDefinition*, Battle_HitResult*, Battle_HitResult*, int, int);
std::remove_reference_t<decltype(Battle_statCalculators[0])> wrong = Wrong;
#endif
int main(int argc, char** argv) {
  if (argc > 1) selected = std::atoi(argv[1]);
  namespace b = VS::Battle;
  auto state = std::make_unique<Battle_ActionState>();
  Battle_ActionDefinition action{};
  Battle_HitResult source{}, target{};
  Battle_Actor actor{};
  Battle_ActorStats stats{};
  actor.stats = &stats;
  Battle_actors[0] = &actor;
  Battle_actionState = state.get();
  Battle_actionDefinitions[0].type = 1;
  state->selection.primaryTarget.hpEffect = 1;
  const std::array callbacks{b::LiveScalePreviousTargetHit, b::LiveScaleSourceMissingHp,
      b::LiveScalePreviousTargetHit, b::LiveScaleTargetCurrentHp,
      b::LiveWeaponPpWithChainOffset, b::LiveScaleIncomingSpellDamage,
      b::LiveScaleSourceMissingMp, b::LiveScalePreviousTargetHit,
      b::LiveScaleIncomingPhysicalDamage, b::LiveTargetMissingHp, b::LiveTargetMissingMp,
      b::LiveScaleIncomingSpellDamage, b::LiveScaleIncomingPhysicalDamage,
      b::LiveScaleIncomingSpellCost, b::LiveScaleIncomingDamage, b::LiveSourceShieldPp};
  constexpr std::array ids{412, 413, 414, 415, 416, 417, 419, 426,
                           418, 424, 425, 427, 428, 429, 430, 431};
  for (unsigned index = 0; index < callbacks.size(); ++index) {
    auto callback = callbacks[index];
    const auto first = refusals;
    Battle_actionDefinitions[0].type = index == 8 || index == 12 ? 7 : 1;
    if (index != 9 && index != 10 && index != 15) {
      Battle_actionState = nullptr;
      Refuses([&] { callback(&action, &source, &target, 0, 0); });
    }
    Battle_actionState = state.get();
    if (index != 4 && index != 9 && index != 10 && index != 15) {
      Refuses([&] { callback(nullptr, &source, &target, 0, 0); });
      for (auto hit : {-1, 2}) {
        Refuses([&] { callback(&action, &source, &target, hit, 0); });
      }
    }
    if (index == 1 || index == 3 || index == 4 || index == 6 ||
        index == 9 || index == 10 || index == 15) {
      source.targetsPosition = target.targetsPosition = 1;
      Refuses([&] { callback(&action, nullptr, nullptr, 0, 0); });
      Battle_actors[0] = nullptr;
      Refuses([&] { callback(&action, &source, &target, 0, 0); });
      Battle_actors[0] = &actor;
      actor.stats = nullptr;
      Refuses([&] { callback(&action, &source, &target, 0, 0); });
      actor.stats = &stats;
      for (auto slot : {42, 255}) {
        source.target.targetActor = target.target.targetActor = static_cast<unsigned char>(slot);
        Refuses([&] { callback(&action, &source, &target, 0, 0); });
      }
      source.target.targetActor = target.target.targetActor = 0;
    }
    if (index == 5 || index == 8 || index == 11 || index == 12 || index == 13) {
      for (auto id : {256, 65535}) {
        state->selection.actionId = static_cast<unsigned short>(id);
        Refuses([&] { callback(nullptr, nullptr, nullptr, -1, 0); });
      }
      state->selection.actionId = 0;
    }
    std::printf("F%05d refusal cases [%u,%u)\n", ids[index], first, refusals);
  }
  state->selection.actionId = 0;
  for (unsigned type = 0; type < 8; ++type) {
    Battle_actionDefinitions[0].type = type;
    for (unsigned effect = 0; effect < 4; ++effect) {
      state->selection.primaryTarget.hpEffect = effect;
      if (type != 1 || (effect != 1 && effect != 3)) {
        if (b::LiveScaleIncomingSpellDamage(nullptr, nullptr, nullptr, -1, -1) != 0) return 1;
      }
      if (type == 1 || (effect != 1 && effect != 3)) {
        if (b::LiveScaleIncomingPhysicalDamage(nullptr, nullptr, nullptr, 2, -1) != 0) return 1;
      }
      if (type != 1 &&
          b::LiveScaleIncomingSpellCost(nullptr, nullptr, nullptr, -1, -1) != 0) return 1;
      if (effect != 1 && effect != 3) {
        state->selection.actionId = 65535;
        if (b::LiveScaleIncomingDamage(nullptr, nullptr, nullptr, 2, -1) != 0) return 1;
        state->selection.actionId = 0;
      }
    }
  }
  stats.weapon.currentPp = 65535;
  if (b::LiveWeaponPpWithChainOffset(nullptr, &source, nullptr, -1, -1) != -2) return 1;
  Battle_actionState = nullptr;
  stats.shield.currentPp = 32768;
  if (b::LiveSourceShieldPp(nullptr, &source, nullptr, 2, -1) != -32768) return 1;
  if (b::LiveTargetMissingHp(nullptr, nullptr, &target, -1, -1) != 0) return 1;
  if (b::LiveTargetMissingMp(nullptr, nullptr, &target, 2, -1) != 0) return 1;
  std::printf("%u negative cases; lazy type/effect and unused action/index checks passed\n", refusals);
}
