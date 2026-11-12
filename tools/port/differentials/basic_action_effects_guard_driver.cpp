// Checked live refusals and callback ordering for F00469-F00475.
#include <array>
#include <bit>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <type_traits>

#include "Battle/BasicActionEffectsLive.hpp"
#include "Battle/StatusQueueLive.hpp"
#include "Core/RandomLive.hpp"
#include "State/ScriptFlagsLive.hpp"
extern "C" {
#include "Battle/ActionResolution.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Battle/Hit.h"
#include "Battle/StatusEffects.h"
Battle_Actor* Battle_actors[42]{};
Battle_StatusEffectParams Battle_statusEffectParams[32]{};
}
namespace {
int selected = -1;
unsigned refusals{};
Battle_HitResult* observed_hit{};
std::array<std::byte, sizeof(Battle_HitResult)> before_hit{};
unsigned flags_count{}, flag_id{}, flag_value{};
bool mutate_after_removal{};
template<class F> void Refuses(F operation) {
  if (static_cast<int>(refusals++) == selected) {
    operation();
    std::_Exit(86);
  }
}
}
extern "C" void Core_Errorf(const char*, const char* message, ...) {
  va_list arguments;
  va_start(arguments, message);
  std::vfprintf(stderr, message, arguments);
  va_end(arguments);
  std::fputc('\n', stderr);
  if (observed_hit && std::bit_cast<decltype(before_hit)>(*observed_hit) != before_hit)
    std::_Exit(89);
  std::_Exit(85);
}
extern "C" void Core_Debugf(const char*, const char*, ...) {}
namespace VS::Core {
std::int32_t NextLiveRandomBounded(std::int32_t) { std::_Exit(88); }
}
namespace VS::State {
void SetLiveScriptFlag(int id, int value) {
  ++flags_count; flag_id = id; flag_value = value;
}
}
#if defined(PROBE_POST_REMOVAL)
namespace VS::Battle {
int QueueLiveStatusRemovalMask(int rule, Battle_HitResult* hit) {
  if (rule != 30 || !mutate_after_removal) std::_Exit(88);
  hit->target.targetActor = 1;
  Battle_actors[1]->stats->currentHp = 0xFEDC;
  hit->inflictStatuses = 0x12345678;
  return 0;
}
}
#endif
#if defined(PROBE_WRONG_ABI)
int Wrong(Battle_ActionDefinition*, Battle_HitResult*, Battle_HitResult*, int, int);
std::remove_extent_t<decltype(Battle_actionEffectMediators)> wrong = &Wrong;
#endif
int main(int argc, char** argv) {
  namespace b = VS::Battle;
  if (argc > 1) selected = std::atoi(argv[1]);
  Battle_HitResult hit{};
  Battle_Actor actor{}, other{};
  Battle_ActorStats stats{}, later{};
  actor.stats = &stats; other.stats = &later;
  Battle_actors[0] = &actor; Battle_actors[1] = &other;
#if defined(PROBE_POST_REMOVAL)
  stats.currentHp = 123;
  mutate_after_removal = true;
  b::QueueLiveInstantDeath(nullptr, nullptr, &hit, -999, 0);
  if (hit.target.hp != -292 || hit.hpEffect != 1 || hit.inflictStatuses != 0x12345678 ||
      stats.currentHp != 123 || std::bit_cast<std::uint16_t>(later.currentHp) != 0xFEDC) return 1;
  std::puts("post-helper target slot and HP reread passed");
  return 0;
#else
  for (int alias : {469, 472, 475}) {
    Refuses([&] { b::MarkLiveSourceActionSuccess(nullptr, nullptr, nullptr, 0, 0); });
    hit.sourceResult = 3;
    b::MarkLiveSourceActionSuccess(nullptr, &hit, nullptr, -999, -1);
    if (hit.sourceResult != 2) return 1;
    std::printf("F%05d source-only control passed\n", alias);
  }
  Refuses([&] { b::QueueLiveInstantDeath(nullptr, nullptr, nullptr, 0, 0); });
  Refuses([&] { b::QueueLiveDamageBasedArmorWear(nullptr, nullptr, nullptr, 0, 0); });
  observed_hit = &hit;
  for (int limb : {-128, -1, 6, 127}) {
    hit.target.targetLimb = static_cast<unsigned char>(limb);
    before_hit = std::bit_cast<decltype(before_hit)>(hit);
    Refuses([&] { b::QueueLiveDamageBasedArmorWear(nullptr, nullptr, &hit, 0, 0); });
  }
  hit.target.targetLimb = 0;
  for (unsigned slot : {42U, 255U}) {
    hit.target.targetActor = static_cast<unsigned char>(slot);
    before_hit = std::bit_cast<decltype(before_hit)>(hit);
    Refuses([&] { b::QueueLiveInstantDeath(nullptr, nullptr, &hit, 0, 0); });
  }
  hit.target.targetActor = 0;
  before_hit = std::bit_cast<decltype(before_hit)>(hit);
  Battle_actors[0] = nullptr;
  Refuses([&] { b::QueueLiveInstantDeath(nullptr, nullptr, &hit, 0, 0); });
  Battle_actors[0] = &actor; actor.stats = nullptr;
  Refuses([&] { b::QueueLiveInstantDeath(nullptr, nullptr, &hit, 0, 0); });
  actor.stats = &stats;
  for (const auto function : {b::SetLiveActionFlag64, b::SetLiveActionFlag32}) {
    for (int execute : {0, 1, -1, -2147483647}) {
      flags_count = 0;
      function(nullptr, nullptr, nullptr, -999, execute);
      if (flags_count != (execute != 0 ? 1U : 0U)) return 1;
      if (execute != 0 && (flag_value != 1 ||
          flag_id != (function == b::SetLiveActionFlag64 ? 64U : 32U))) return 1;
    }
  }
  std::printf("%u negative cases; exact script callback counts/indices/values passed\n", refusals);
#endif
}
