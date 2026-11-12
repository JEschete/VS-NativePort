// Real checked-live entry points; a refusal must diagnose, not return an amount.
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iterator>

#include "Battle/DamageCalculationsLive.hpp"
#include "Core/RandomLive.hpp"
#include "Items/EquipmentLive.hpp"
extern "C" {
#include "Battle/Action.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Battle/Hit.h"
Battle_Actor* Battle_actors[42]{};
unsigned short Battle_bladePpDamageBonuses[12]{};
int Battle_criticalHitOccurred=-7;
short (*Battle_statCalculators[27])(Battle_ActionDefinition*,Battle_HitResult*,Battle_HitResult*,int,int){};
void Core_Errorf(const char*,const char* message,...) {
  std::fprintf(stderr,"%s\n",message);
  std::_Exit(85);
}
}
namespace VS::Core {
i32 NextLiveRandomBounded(i32){std::_Exit(87);}
i32 NextLiveRandomAveraged(i32){std::_Exit(87);}
}
namespace VS::Items {
void GrowLiveAttackerWeaponRatings(const Battle_ActionDefinition*,Battle_ActorStats*,int,int){std::_Exit(88);}
void GrowLiveDefenderEquipmentRatings(const Battle_ActionDefinition*,Battle_ActorStats*,int,int,int){std::_Exit(88);}
}
namespace {
int selected=-1;
unsigned count{};
template<class F> void Refuses(F call){
  if(static_cast<int>(count++)==selected){call();std::_Exit(86);}
}
}
int main(int argc,char** argv){
  if(argc>1) selected=std::atoi(argv[1]);
  namespace b=VS::Battle;
  using F=decltype(&b::LiveCalculateSpellHealing);
  const std::array<F,5> functions{
    +[](Battle_ActionDefinition* a,Battle_HitResult* s,Battle_HitResult* t,int h,int r)->std::int16_t {
      return b::LiveCalculatePhysicalHitDamage(a,s,t,h,r,3);},
    b::LivePhysicalDamageWithoutDebuffs,b::LiveCalculateSpellHitDamage,
    b::LiveCalculateSpellHealing,b::LivePhysicalDamageWithDebuffs};
  const std::array ids{409,410,411,421,433};
  for(unsigned f=0;f<functions.size();++f){
    auto call=functions[f];
    auto first=count;
    Battle_ActionDefinition action{};
    Battle_HitResult source{},target{};
    Battle_Actor actor{},other{};
    Battle_ActorStats stats{},to{};
    stats.strength=stats.intelligence=100;
    to.strength=to.intelligence=20;
    actor.stats=&stats;other.stats=&to;
    Battle_actors[0]=&actor;Battle_actors[1]=&other;
    stats.actorId=0;to.actorId=1;
    target.target.targetActor=1;
    action.hits[0].affinity=6;
    Refuses([&]{call(nullptr,&source,&target,0,0);});
    Refuses([&]{call(&action,nullptr,&target,0,0);});
    Refuses([&]{call(&action,&source,nullptr,0,0);});
    for(int hit:{-1,2}) Refuses([&]{call(&action,&source,&target,hit,0);});
    for(auto* hit:{&source,&target}){
      auto slot=hit->target.targetActor;
      for(unsigned index:{42U,255U}){
        hit->target.targetActor=static_cast<unsigned char>(index);
        Refuses([&]{call(&action,&source,&target,0,0);});
      }
      hit->target.targetActor=slot;
      auto* saved=Battle_actors[slot];
      Battle_actors[slot]=nullptr;
      Refuses([&]{call(&action,&source,&target,0,0);});
      Battle_actors[slot]=saved;
      auto* saved_stats=saved->stats;
      saved->stats=nullptr;
      Refuses([&]{call(&action,&source,&target,0,0);});
      saved->stats=saved_stats;
    }
    for(int limb:{-128,-1,6,127}){
      target.target.targetLimb=static_cast<signed char>(limb);
      target.targetsPosition=1;
      Refuses([&]{call(&action,&source,&target,0,0);});
    }
    target.target.targetLimb=0;target.targetsPosition=0;
    for(auto* owner:{&stats,&to}){
      auto saved=owner->actorId;
      for(unsigned id:{42U,255U,2U}){
        owner->actorId=static_cast<unsigned char>(id);
        Refuses([&]{call(&action,&source,&target,0,0);});
      }
      owner->actorId=saved;
    }
    if(f==0||f==1||f==4){
      stats.weapon.damageType=4;
      Refuses([&]{call(&action,&source,&target,0,0);});
      stats.weapon.damageType=0;
      actor.weaponDrawn=1;
      stats.weapon.maxPp=1;
      for(unsigned category:{12U,255U}){
        stats.weapon.blade.category=static_cast<unsigned char>(category);
        Refuses([&]{call(&action,&source,&target,0,0);});
      }
      stats.weapon.maxPp=0;
      if(call(&action,&source,&target,0,0)!=100) return 1;
    }
    if(f==3){
      actor.weaponDrawn=0;
      action.hits[0].affinity=0;
      Refuses([&]{call(&action,&source,&target,0,0);});
      actor.weaponDrawn=1;
      Refuses([&]{call(&action,&source,&target,0,0);});
      action.hits[0].affinity=1;
      Refuses([&]{call(&action,&source,&target,0,0);});
      actor.weaponDrawn=0;
      if(call(&action,&source,&target,0,0)!=100) return 2;
      actor.weaponDrawn=1;
      action.hits[0].affinity=6;
      stats.weapon.classAffinityCurrent.affinities[0][5]=40;
      if(call(&action,&source,&target,0,0)!=110) return 3;
    }
    std::printf("F%05d refusals [%u,%u)\n",ids[f],first,count);
  }
  if(Battle_criticalHitOccurred!=-7) return 4;
  std::printf("%u diagnostic refusals; defined/unused paths pass\n",count);
}
