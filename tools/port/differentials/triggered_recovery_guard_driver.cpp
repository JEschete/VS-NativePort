// Checked F00476-F00483 live boundaries; real owned dispatch/recovery by default.
#include <algorithm>
#include <array>
#include <bit>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <type_traits>
#include "Battle/BasicActionEffectsLive.hpp"
#include "Battle/DamagePoliciesLive.hpp"
#include "Battle/HitCalculationsLive.hpp"
#include "Battle/HitModifiersLive.hpp"
#include "Battle/StatusQueueLive.hpp"
#include "Core/RandomLive.hpp"
#include "State/ScriptFlagsLive.hpp"
extern "C" {
#include "Battle/ActionFlow.h"
#include "Battle/ActionResolution.h"
#include "Battle/ActionTable.h"
#include "Battle/Damage.h"
#include "Battle/HitChance.h"
Battle_Actor* Battle_actors[42]{};
Battle_ActionState* Battle_actionState{};
Battle_ActionDefinition Battle_actionDefinitions[256]{};
short (*Battle_statCalculators[27])(Battle_ActionDefinition*, Battle_HitResult*, Battle_HitResult*, int, int){};
int (*Battle_hitFunctions[8])(Battle_ActionDefinition*, Battle_HitResult*, Battle_HitResult*, int, int){};
short Battle_limbArmorDpLoss[8]{};
u_int Battle_lastActionStatValue{}, Battle_hitPrerequisiteResult{};
}
namespace {
namespace b = VS::Battle;
using Handler = std::remove_extent_t<decltype(Battle_actionEffectMediators)>;
constexpr std::array<Handler, 8> handlers{
    b::QueueLiveSpellTriggeredRecovery, b::QueueLiveNonSpellTriggeredRecovery,
    b::QueueLivePhysicalAffinityRecovery, b::QueueLiveAirAffinityRecovery,
    b::QueueLiveFireAffinityRecovery, b::QueueLiveEarthAffinityRecovery,
    b::QueueLiveWaterAffinityRecovery, b::QueueLiveLightAffinityRecovery};
template<class T> auto bytes(const T& value) {
  return std::bit_cast<std::array<std::byte, sizeof(T)>>(value);
}
struct Guarded {
  std::array<unsigned,4> before{1,2,3,4};
  Battle_HitResult hit{};
  std::array<unsigned,4> after{5,6,7,8};
};
unsigned calls{}, recoveries{};
Battle_ActionDefinition* expected_action{};
Battle_HitResult *expected_source{}, *cached{}, *selected{}, *alternate{};
int expected_index{}, expected_execute{};
short result{};
bool mutate{}, remove_stats{};
Battle_ActorStats other_stats{};
Battle_Actor other_actor{};
short calculator(Battle_ActionDefinition* action, Battle_HitResult* source,
                 Battle_HitResult* target, int index, int execute) {
  if (action != expected_action || source != expected_source || target != cached ||
      index != expected_index || execute != expected_execute) std::_Exit(90);
  ++calls;
  if (mutate) {
    target->target.hp = 32767;
    target->hpEffect = 3;
    target->mpEffect = 2;
    target->removeStatuses = 0x12345678;
    Battle_actionState = nullptr;
    selected = alternate;
    Battle_actors[0] = &other_actor;
  }
  if (remove_stats) Battle_actors[0]->stats = nullptr;
  return result;
}
}
extern "C" void Core_Errorf(const char*, const char* message, ...) {
  va_list arguments;
  va_start(arguments, message);
  std::vfprintf(stderr, message, arguments);
  va_end(arguments);
  std::fputc('\n', stderr);
  std::_Exit(calls == 0 ? 85 : 86);
}
namespace VS::State {
void SetLiveScriptFlag(int, int) { std::_Exit(88); }
}
namespace VS::Core {
std::int32_t NextLiveRandomBounded(std::int32_t) { std::_Exit(88); }
std::int32_t NextLiveRandomAveraged(std::int32_t) { std::_Exit(88); }
}
namespace VS::Battle {
int QueueLiveStatusRemovalMask(int, Battle_HitResult*) { std::_Exit(88); }
std::int16_t AttackLiveGemHitBonus(Battle_ActionDefinition*, Battle_ActorStats*) { std::_Exit(88); }
std::int16_t DefenseLiveGemHitBonus(Battle_ActionDefinition*, Battle_ActorStats*, int) { std::_Exit(88); }
#if defined(CONTROLLED_HELPERS)
std::int16_t LiveCalculateActionStatChange(Battle_ActionDefinition* action,
    Battle_HitResult* source, Battle_HitResult* target, int hit, int execute) {
  return calculator(action, source, target, hit, execute);
}
void LiveDistributeLimbRecovery(Battle_ActionDefinition* action, Battle_HitResult* target, std::int16_t amount) {
  if (action != expected_action || target != cached || amount != result || calls != 1 ||
      target->target.hp != std::bit_cast<short>(static_cast<unsigned short>(32767 + result)) ||
      target->hpEffect != 2 || target->mpEffect != 2 || target->removeStatuses != 0x12345678) std::_Exit(91);
  ++recoveries;
}
#endif
}
#if defined(PROBE_WRONG_ABI)
int Wrong(Battle_ActionDefinition*, Battle_HitResult*, Battle_HitResult*, int, int);
Handler wrong = &Wrong;
#endif
int main(int argc, char** argv) {
  const auto function = argc > 1 ? static_cast<unsigned>(std::atoi(argv[1])) : 0;
  const char* mode = argc > 2 ? argv[2] : "valid";
  Battle_ActionState state{};
  Battle_ActionDefinition action{};
  Battle_Actor actor{};
  Battle_ActorStats stats{};
  Guarded target{}, source{}, other{};
  stats.actorId = 255;
  stats.limbs[0].maxHp = 100;
  stats.currentHp = 123;
  other_stats.limbs[5].maxHp = 100;
  other_stats.currentHp = 321;
  other_actor.stats = &other_stats;
  actor.stats = &stats;
  Battle_actors[0] = &actor;
  Battle_actionState = &state;
  state.selection.actionId = function < 2 ? 255 : 65535;
  state.selection.effectVariant = static_cast<char>(function - 2);
  Battle_actionDefinitions[255].type = function == 0 ? 1 : 7;
  action.type = function == 0 ? 7 : 1;
  action.hits[1].statCalculatorId = 1;
  Battle_statCalculators[1] = calculator;
  expected_action = &action;
  expected_source = &source.hit;
  cached = selected = &target.hit;
  alternate = &other.hit;
  expected_index = 1;
  expected_execute = -1;
  result = -19;
  auto is = [mode](const char* name) { return std::strcmp(mode, name) == 0; };
  if (is("state")) Battle_actionState = nullptr;
  if (is("selected-id")) state.selection.actionId = 256;
  if (is("target")) cached = selected = nullptr;
  if (is("action")) expected_action = nullptr;
  if (is("index-low")) expected_index = -1;
  if (is("index-high")) expected_index = 2;
  if (is("selector-zero")) action.hits[1].statCalculatorId = 0;
  if (is("selector-high")) action.hits[1].statCalculatorId = 27;
  if (is("callback")) Battle_statCalculators[1] = nullptr;
  if (is("actor-index")) target.hit.target.targetActor = 42;
  if (is("actor")) Battle_actors[0] = nullptr;
  if (is("stats")) actor.stats = nullptr;
  if (is("recovery-stats")) remove_stats = true;
  if (is("source-null")) expected_source = nullptr;
  if (is("mutation") || is("zero") || is("alias")) mutate = true;
  if (is("zero")) result = 0;
  if (is("alias")) expected_source = cached;
  const auto source_before = bytes(source), other_before = bytes(other);
  handlers.at(function)(expected_action, expected_source, selected, expected_index, expected_execute);
  if (calls != 1 || bytes(source) != source_before || bytes(other) != other_before) return 2;
  if (target.before != std::array<unsigned,4>{1,2,3,4} ||
      target.after != std::array<unsigned,4>{5,6,7,8}) return 3;
  if (mutate && (selected != alternate || target.hit.hpEffect != 2 ||
      target.hit.target.hp != std::bit_cast<short>(static_cast<unsigned short>(32767 + result)))) return 4;
  std::printf("F%05u %s: checked live pass; calls=%u recovery-observations=%u\n",
              476 + function, mode, calls, recoveries);
}
