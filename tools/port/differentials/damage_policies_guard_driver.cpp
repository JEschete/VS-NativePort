// Checked-live probes: exit85 is a diagnostic; exit86 is a missed refusal.
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <type_traits>

#include "Battle/DamagePoliciesLive.hpp"
extern "C" {
#include "Battle/Action.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Battle/Hit.h"
Battle_Actor* Battle_actors[42]{};
unsigned Battle_lastActionStatValue{};
short Battle_limbArmorDpLoss[8]{};
short (*Battle_statCalculators[27])(Battle_ActionDefinition*, Battle_HitResult*,
                                   Battle_HitResult*, int, int){};
}
namespace {
unsigned count{};
int selected=-1;
bool damage{};
template<class F> void Refuses(F operation) {
  if (static_cast<int>(count++)==selected) {
    operation();
    std::_Exit(86);
  }
}
}
extern "C" void Core_Errorf(const char*, const char* message, ...) {
  std::fprintf(stderr,"%s\n",message);
  for (int i=0; i<8; ++i) {
    if (Battle_limbArmorDpLoss[i] != (damage && i<6 ? 0 : 200+i)) std::_Exit(87);
  }
  std::_Exit(85);
}
#if defined(PROBE_WRONG_INT)
int Wrong(Battle_ActionDefinition*, Battle_HitResult*, Battle_HitResult*, int, int);
std::remove_reference_t<decltype(Battle_statCalculators[0])> wrong = Wrong;
#elif defined(PROBE_WRONG_VOID)
void Wrong(Battle_ActionDefinition*, Battle_HitResult*, Battle_HitResult*, int, int);
std::remove_reference_t<decltype(Battle_statCalculators[0])> wrong = Wrong;
#endif
int main(int argc, char** argv) {
  if (argc>1) selected=std::atoi(argv[1]);
  namespace b=VS::Battle;
  Battle_ActionDefinition action{};
  Battle_HitResult hit{};
  Battle_Actor actor{};
  Battle_ActorStats stats{};
  for (int i=0;i<8;++i) Battle_limbArmorDpLoss[i]=200+i;
  actor.stats=&stats;
  Battle_actors[0]=&actor;
  Refuses([&] { b::LiveApplyDamageGemMultiplier(nullptr,1,&stats,&stats); });
  Refuses([&] { b::LiveApplyDamageGemMultiplier(&action,1,nullptr,&stats); });
  Refuses([&] { b::LiveApplyDamageGemMultiplier(&action,1,&stats,nullptr); });
  std::printf("F00394 refusal cases [0,%u)\n",count);
  auto first=count;
  damage=true;
  Refuses([&] { b::LiveDistributeHitLimbDamage(nullptr,&hit,1); });
  const std::array callbacks{b::LiveDistributeHitLimbDamage,b::LiveDistributeLimbRecovery};
  for (unsigned f=0;f<callbacks.size();++f) {
    auto call=callbacks[f];
    damage=f==0;
    Refuses([&] { call(&action,nullptr,1); });
    Battle_actors[0]=nullptr;
    Refuses([&] { call(&action,&hit,1); });
    Battle_actors[0]=&actor;
    for (unsigned slot:{42U,255U}) {
      hit.target.targetActor=static_cast<unsigned char>(slot);
      Refuses([&] { call(&action,&hit,1); });
    }
    hit.target.targetActor=0;
    actor.stats=nullptr;
    if (f==0) {
      for (unsigned category:{0U,1U,2U,3U,4U,5U,7U,14U}) {
        action.category=category;
        Refuses([&] { call(&action,&hit,1); });
      }
      actor.stats=&stats;
      for (unsigned category:{5U,6U,7U,8U,11U,12U,14U}) {
        action.category=category;
        for (int limb:{-128,-1,6,127}) {
          hit.target.targetLimb=static_cast<signed char>(limb);
          Refuses([&] { call(&action,&hit,1); });
        }
      }
      hit.target.targetLimb=0;
    } else {
      Refuses([&] { call(nullptr,&hit,1); });
    }
    actor.stats=&stats;
    std::printf("F%05u refusal cases [%u,%u)\n",396+f,first,count);
    first=count;
  }
  damage=false;
  Refuses([&] { b::LiveScaleLastActionStatValue(nullptr,nullptr,nullptr,0,0); });
  for (int index:{-1,2}) {
    Refuses([&] { b::LiveScaleLastActionStatValue(&action,nullptr,nullptr,index,0); });
  }
  std::printf("F00432 refusal cases [%u,%u)\n",first,count);
  first=count;
  Refuses([&] { b::LiveTargetRisk(nullptr,nullptr,nullptr,0,0); });
  Battle_actors[0]=nullptr;
  Refuses([&] { b::LiveTargetRisk(nullptr,nullptr,&hit,0,0); });
  Battle_actors[0]=&actor;
  actor.stats=nullptr;
  Refuses([&] { b::LiveTargetRisk(nullptr,nullptr,&hit,0,0); });
  actor.stats=&stats;
  for (unsigned slot:{42U,255U}) {
    hit.target.targetActor=static_cast<unsigned char>(slot);
    Refuses([&] { b::LiveTargetRisk(nullptr,nullptr,&hit,0,0); });
  }
  hit.target.targetActor=0;
  std::printf("F00434 refusal cases [%u,%u)\n",first,count);
  Battle_lastActionStatValue=0xABCD8000U;
  if (b::LiveLastActionStatValue(nullptr,nullptr,nullptr,-1,-1)!=-32768) return 1;
  actor.stats=nullptr;
  for (unsigned category:{6U,8U,9U,10U,11U,12U,13U,15U}) {
    action.category=category;
    b::LiveDistributeHitLimbDamage(&action,&hit,1);
  }
  std::printf("%u diagnostic refusals; unused null stats and all-null last callback pass\n",count);
}
