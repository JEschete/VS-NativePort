// Standalone live-binding refusal/ABI probe. Each negative case runs in its own
// process and exits 85 from the diagnostic; 86 means a missing refusal.
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <type_traits>

#include "Battle/HitCalculationsLive.hpp"
extern "C" {
#include "Battle/Action.h"
#include "Battle/ActionResolution.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Battle/Hit.h"
#include "Battle/HitChance.h"
Battle_Actor* Battle_actors[42]{};
int (*Battle_hitFunctions[8])(Battle_ActionDefinition*, Battle_HitResult*, Battle_HitResult*, int, int){};
short (*Battle_statCalculators[27])(Battle_ActionDefinition*, Battle_HitResult*, Battle_HitResult*, int, int){};
void (*Battle_actionEffectMediators[70])(Battle_ActionDefinition*, Battle_HitResult*, Battle_HitResult*, int, int){};
unsigned Battle_previousHitResult{}, Battle_hitPrerequisiteResult=1, Battle_lastActionStatValue{};
void Core_Errorf(const char*, const char* message, ...) {
  std::fprintf(stderr, "%s\n", message);
  std::_Exit(85);
}
}
namespace VS::Core {
int NextLiveRandomAveraged(int) { return 5; }
int NextLiveRandomBounded(int) { return 99; }
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
#if defined(PROBE_WRONG_HIT)
std::remove_reference_t<decltype(Battle_hitFunctions[0])> wrong = VS::Battle::RejectLiveEffectSelector;
#elif defined(PROBE_WRONG_STAT)
std::remove_reference_t<decltype(Battle_statCalculators[0])> wrong = VS::Battle::RejectLiveEffectSelector;
#elif defined(PROBE_WRONG_EFFECT)
std::remove_reference_t<decltype(Battle_actionEffectMediators[0])> wrong = VS::Battle::RejectLiveHitSelector;
#endif
int main(int argc, char** argv) {
  if (argc > 1) selected = std::atoi(argv[1]);
  namespace b=VS::Battle;
  Battle_ActionDefinition action{};
  Battle_HitResult source{},target{};
  Battle_Actor actor{};
  Battle_ActorStats stats{};
  actor.stats=&stats;
  Battle_actors[0]=&actor;
  stats.maxHp=1;
  for (auto function : {b::LiveAgilityHitChance,b::LiveIntelligenceHitChance,b::LiveCostAdjustedAgilityHitChance}) {
    Refuses([&]{ function(&action,nullptr,&target,0,0); });
    Refuses([&]{ function(&action,&source,nullptr,0,0); });
    for(auto index:{-128,-1,6,127}) {
      target.target.targetLimb=static_cast<unsigned char>(index);
      Refuses([&]{ function(&action,&source,&target,0,0); });
    }
    target.target.targetLimb=0;
    for(auto slot:{42,255}) {
      target.target.targetActor=static_cast<unsigned char>(slot);
      Refuses([&]{ function(&action,&source,&target,0,0); });
      target.target.targetActor=0;
      stats.actorId=static_cast<unsigned char>(slot);
      Refuses([&]{ function(&action,&source,&target,0,0); });
    }
    stats.actorId=0;
    actor.stats=nullptr;
    Refuses([&]{ function(&action,&source,&target,0,0); });
    actor.stats=&stats;
    actor.weaponDrawn=1;
    Refuses([&]{ function(nullptr,&source,&target,0,0); });
    Refuses([&]{ function(&action,&source,&target,-1,0); });
    Refuses([&]{ function(&action,&source,&target,2,0); });
    actor.weaponDrawn=0;
  }
  Refuses([&]{b::RejectLiveHitSelector(nullptr,nullptr,nullptr,0,0);});
  Refuses([&]{b::RejectLiveStatSelector(nullptr,nullptr,nullptr,0,0);});
  Refuses([&]{b::RejectLiveEffectSelector(nullptr,nullptr,nullptr,0,0);});
  for(auto index:{-1,0,1,2}) {
    Refuses([&]{b::LiveRollActionHit(&action,&source,&target,index,0);});
    Refuses([&]{b::LiveCalculateActionStatChange(&action,&source,&target,index,0);});
  }
  for(auto selector:{1,26,27,63}) {
    action.hits[0].statCalculatorId=selector;
    Refuses([&]{b::LiveCalculateActionStatChange(&action,&source,&target,0,0);});
  }
  action.hits[0].hitFunctionId=7;
  Refuses([&]{b::LiveRollActionHit(&action,&source,&target,0,0);});
  Refuses([&]{b::LiveRollActionHit(nullptr,&source,&target,0,0);});
  Refuses([&]{b::LiveCalculateActionStatChange(nullptr,&source,&target,0,0);});
  stats.actorId=128;
  if(b::LiveCalculateActionStatChange(nullptr,nullptr,&target,-1,0)!=999)return 1;
  if(b::LiveRollActionHit(nullptr,&source,&target,-1,0)!=1)return 1;
  std::printf("%u negative cases; both lazy exemptions passed\n",refusals);
}
