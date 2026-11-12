// Live boundaries, actual stat/status policies, controlled external services.
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <type_traits>
#include <string>
#include "Battle/ActionEffectDispatchLive.hpp"
#include "Battle/DamageRecoveryEffectsLive.hpp"
#include "Battle/HitCalculationsLive.hpp"
extern "C" {
#include "Audio/Seq/Channel.h"
#include "Battle/AbilityState.h"
#include "Battle/Action.h"
#include "Battle/ActionFlow.h"
#include "Battle/ActionResolution.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Battle/Hit.h"
#include "Battle/HitChance.h"
#include "Battle/StatusEffects.h"
#include "State/Progress.h"
Battle_Actor* Battle_actors[42]{};
Battle_Actor* Battle_characterState{};
Battle_ActionState* Battle_actionState{};
decltype(Battle_limbInjuryStatusIds) Battle_limbInjuryStatusIds{};
decltype(Battle_statusEffectParams) Battle_statusEffectParams{};
decltype(Battle_actionEffectMediators) Battle_actionEffectMediators{};
decltype(Battle_hitFunctions) Battle_hitFunctions{};
decltype(Battle_statCalculators) Battle_statCalculators{};
decltype(Battle_breakArtPointRequirements) Battle_breakArtPointRequirements{};
decltype(Audio_Seq_unusedArtsStatus) Audio_Seq_unusedArtsStatus{};
unsigned Battle_lastActionStatValue{},Battle_hitPrerequisiteResult{},Battle_previousHitResult{};
int Battle_criticalHitOccurred{};
void Core_Errorf(const char*,const char* message,...){std::fprintf(stderr,"%s\n",message);std::_Exit(85);}
void Core_Debugf(const char*,const char*,...){}
}
namespace VS::Core {
int NextLiveRandomBounded(int){return 0;}
int NextLiveRandomAveraged(int){return 0;}
}
namespace VS::Items {
void ConsumeLiveHitEquipmentDp(const Battle_ActionDefinition*,Battle_ActorStats*,Battle_ActorStats*,int){std::abort();}
void AccumulateLiveHitEquipmentPp(const Battle_ActionDefinition*,Battle_ActorStats*,Battle_ActorStats*,int){std::abort();}
}
namespace VS::Battle {
short AttackLiveGemHitBonus(Battle_ActionDefinition*,Battle_ActorStats*){std::abort();}
short DefenseLiveGemHitBonus(Battle_ActionDefinition*,Battle_ActorStats*,int){std::abort();}
void LiveDistributeHitLimbDamage(Battle_ActionDefinition*,Battle_HitResult*,std::int16_t){std::abort();}
void LiveDistributeLimbRecovery(Battle_ActionDefinition*,Battle_HitResult*,std::int16_t){std::abort();}
}
namespace {
Battle_ActionDefinition action{};
Battle_HitResult source{},target{};
Battle_Actor actor{},character{};
Battle_ActorStats stats{};
unsigned calls{},script{},flag=0x1000A;
int execution=-1;
}
namespace VS::State {
unsigned char LiveScriptFlag(int id){
  if(id!=0x82||actor.killCounted!=1)std::_Exit(91);
  ++script;
  Audio_Seq_unusedArtsStatus.artsLearned[9]=3;
  Audio_Seq_unusedArtsStatus.kills.weaponCategoryKills[9]=65534;
  Battle_breakArtPointRequirements[9][3]=65535;
  Battle_characterState=nullptr;
  return static_cast<unsigned char>(flag);
}
}
short Calculate(Battle_ActionDefinition* a,Battle_HitResult* s,Battle_HitResult* t,int hit,int ex){
  if(a!=&action||s!=&source||t!=&target||hit!=1||ex!=execution)std::_Exit(90);
  ++calls;target.weaponDpChange=32767;target.weaponPpChange=-32768;target.riskChange=9;
  return -1;
}
void Effect(Battle_ActionDefinition* a,Battle_HitResult* s,Battle_HitResult* t,int hit,int ex){
  if(a!=&action||(s!=&source&&s!=nullptr)||t!=&target||hit!=1||ex!=execution)std::_Exit(92);
  ++calls;
}
using Mediator=std::remove_extent_t<decltype(Battle_actionEffectMediators)>;
static_assert(std::is_same_v<Mediator,decltype(&VS::Battle::RejectLiveEffectSelector)>);
#if defined(PROBE_WRONG_ABI)
int Wrong(Battle_ActionDefinition*,Battle_HitResult*,Battle_HitResult*,int,int);
Mediator wrong=&Wrong;
#endif
int main(int argc,char**argv){
  if(argc!=2)return 2;
  const std::string mode=argv[1];
  namespace b=VS::Battle;
  actor.stats=&stats;Battle_actors[0]=Battle_actors[41]=&actor;
  Battle_characterState=&character;character.equippedWeaponCategory=29;
  target.target.targetActor=41;stats.actorId=41;stats.currentHp=target.target.hp=2;
  action.type=2;action.hits[1].effectId=1;action.hits[1].statCalculatorId=1;
  Battle_actionEffectMediators[1]=Effect;Battle_statCalculators[1]=Calculate;
  for(auto& limb:stats.limbs)limb.hp=2;
  for(unsigned i=0;i<4;++i)Battle_limbInjuryStatusIds[i]=i+1;
  if(mode=="target")b::ApplyLiveActionHitEffect(nullptr,nullptr,nullptr,0,-1);
  if(mode=="slot")target.target.targetActor=42;
  if(mode=="actor")Battle_actors[41]=nullptr;
  if(mode=="stats")actor.stats=nullptr;
  if(mode=="action")b::ApplyLiveActionHitEffect(nullptr,nullptr,&target,0,-1);
  if(mode=="source")b::ApplyLiveActionHitEffect(&action,nullptr,&target,-1,1);
  if(mode=="hit-neg")b::ApplyLiveActionHitEffect(&action,&source,&target,0,-1);
  if(mode=="hit-two")b::ApplyLiveActionHitEffect(&action,&source,&target,0,2);
  if(mode=="effect")action.hits[1].effectId=70;
  if(mode=="null-effect")Battle_actionEffectMediators[1]=nullptr;
  if(mode=="character")Battle_characterState=nullptr;
  if(mode=="F00503"||mode=="F00504"||mode=="F00505"){
    const unsigned index=67+std::stoul(mode.substr(1))-503;
    action.hits[1].effectId=index;Battle_actionEffectMediators[index]=b::RejectLiveEffectSelector;
  }
  if(mode.starts_with("injury")){
    if(mode=="injury-target")b::DeriveLiveLimbInjuryStatuses(nullptr);
    if(mode=="injury-mobility0")stats.mobilityLimb0=6;
    if(mode=="injury-mobility1")stats.mobilityLimb1=255;
    if(mode=="injury-rule"){target.target.targetActor=0;Battle_limbInjuryStatusIds[0]=32;}
    if(mode=="injury-zero"){stats.mobilityLimb0=0;stats.mobilityLimb1=0;}
    b::DeriveLiveLimbInjuryStatuses(&target);
  }else if(mode=="F00500"||mode=="F00501"||mode=="F00502"){
    const std::array<Mediator,3> functions{b::QueueLiveWeaponPpRecovery,b::QueueLiveWeaponDpRecovery,b::QueueLiveRiskRecovery};
    const auto i=std::stoul(mode.substr(1))-500;
    Battle_actionEffectMediators[1]=functions[i];
    b::ApplyLiveActionHitEffect(&action,&source,&target,execution,1);
    if(calls!=1||target.weaponPpChange!=(i==0?32767:-32768)||
       target.weaponDpChange!=(i==1?32766:32767)||target.riskChange!=(i==2?8:9))return 3;
  }else if(mode=="exempt"){
    stats.actorId=128;b::ApplyLiveActionHitEffect(nullptr,nullptr,&target,0,-1);
    if(target.target.hp!=1001||target.hpEffect!=1)return 4;
  }else {
    if(mode=="flag-low")flag=0x10000;
    b::ApplyLiveActionHitEffect(&action,&source,&target,execution,1);
    if(calls!=1||script!=1||Audio_Seq_unusedArtsStatus.kills.weaponCategoryKills[9]!=(flag==0x10000?65534:65535))return 5;
  }
  std::printf("%s pass\n",mode.c_str());
}
