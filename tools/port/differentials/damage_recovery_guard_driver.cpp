// Checked live boundaries with controlled callbacks; numerical helpers are
// exercised by damage_recovery.py and the production Catch2 integration tests.
#include <array>
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <type_traits>
#include "Battle/DamageRecoveryEffectsLive.hpp"
extern "C" {
#include "Battle/ActionFlow.h"
#include "Battle/ActionResolution.h"
#include "Battle/Damage.h"
Battle_Actor* Battle_actors[42]{};
Battle_ActionState* Battle_actionState{};
int Battle_criticalHitOccurred{};
u_int Battle_lastActionStatValue{};
}
namespace {
using Handler=std::remove_extent_t<decltype(Battle_actionEffectMediators)>;
constexpr std::array<Handler,6> handlers{
  VS::Battle::QueueLiveHitDamageAndEquipmentWear,VS::Battle::QueueLiveMpDrain,
  VS::Battle::QueueLiveWeaponPpDrain,VS::Battle::QueueLiveShieldPpDrain,
  VS::Battle::QueueLiveHpRecovery,VS::Battle::QueueLiveMpRecovery};
struct Guarded {
  std::array<unsigned,4> before{1,2,3,4};
  Battle_HitResult hit{};
  std::array<unsigned,4> after{5,6,7,8};
};
Battle_ActionDefinition action{};
Battle_ActorStats source_stats{},target_stats{},other_stats{};
Battle_Actor source_actor{},target_actor{},other_actor{};
Battle_HitResult *from{},*to{};
unsigned calls{},damage{},recovery{},dp{},pp{},function{};
int execute{};
bool mutate{},alias{};
}
extern "C" void Core_Errorf(const char*,const char*,...){std::_Exit(calls?86:85);}
namespace VS::Battle {
std::int16_t LiveCalculateActionStatChange(Battle_ActionDefinition* a,
    Battle_HitResult* s,Battle_HitResult* t,int hit,int ex) {
  if(a!=&action||s!=from||t!=to||hit!=1||ex!=execute)std::_Exit(90);
  if(function==0&&Battle_criticalHitOccurred!=0)std::_Exit(91);
  ++calls;
  if(mutate&&to!=nullptr){
    to->target.hp=32767;to->target.mp=32767;
    to->hpEffect=2;to->mpEffect=3;to->removeStatuses=0xFEDCBA98;
    to->target.targetActor=40;
    source_actor.stats=target_actor.stats=&other_stats;
    target_stats.currentMp=-19;
    Battle_criticalHitOccurred=-1;
  }
  return -19;
}
void LiveDistributeHitLimbDamage(Battle_ActionDefinition* a,Battle_HitResult* t,std::int16_t amount){
  if(a!=&action||t!=to||amount!=-19||to->target.hp!=32748||to->hpEffect!=3)std::_Exit(92);
  ++damage;
  target_stats.statuses=0x8000;
  to->inflictStatuses=0x20;
}
void LiveDistributeLimbRecovery(Battle_ActionDefinition* a,Battle_HitResult* t,std::int16_t amount){
  if(a!=&action||t!=to||amount!=-19||to->target.hp!=32748||to->hpEffect!=2)std::_Exit(93);
  ++recovery;
}
}
namespace VS::Items {
void ConsumeLiveHitEquipmentDp(const Battle_ActionDefinition* a,Battle_ActorStats* s,
    Battle_ActorStats* t,int amount){
  if(a!=&action||s!=(alias?&target_stats:&source_stats)||t!=&target_stats||amount!=-19||damage!=1)std::_Exit(94);
  ++dp;s->weapon.currentDp=57;t->shield.currentDp=31;
}
void AccumulateLiveHitEquipmentPp(const Battle_ActionDefinition* a,Battle_ActorStats* s,
    Battle_ActorStats* t,int amount){
  if(a!=&action||s!=(alias?&target_stats:&source_stats)||t!=&target_stats||amount!=-19||
      dp!=1||s->weapon.currentDp!=57||t->shield.currentDp!=31)std::_Exit(95);
  ++pp;
}
}
#if defined(PROBE_WRONG_ABI)
int Wrong(Battle_ActionDefinition*,Battle_HitResult*,Battle_HitResult*,int,int);
Handler wrong=&Wrong;
#endif
int main(int argc,char** argv){
  function=argc>1?std::atoi(argv[1]):0;
  const char* mode=argc>2?argv[2]:"mutation";
  auto is=[mode](const char* name){return std::strcmp(mode,name)==0;};
  Battle_ActionState state{};
  Guarded source{},target{};
  from=&source.hit;to=&target.hit;
  source_actor.stats=&source_stats;target_actor.stats=&target_stats;other_actor.stats=&other_stats;
  Battle_actors[0]=&source_actor;Battle_actors[41]=&target_actor;Battle_actors[40]=&other_actor;
  to->target.targetActor=41;target_stats.actorId=41;
  Battle_actionState=&state;state.selection.primaryTarget.hpEffect=1;
  target_stats.weapon.blade.id=1;target_stats.shield.base.id=1;
  target_stats.weapon.currentPp=target_stats.shield.currentPp=65535;
  execute=-1;mutate=true;
  if(is("source"))from=nullptr;
  if(is("target"))to=nullptr;
  if(is("slot"))target.hit.target.targetActor=42;
  if(is("actor"))Battle_actors[41]=nullptr;
  if(is("stats")||is("inactive-stats")||is("position-stats"))target_actor.stats=nullptr;
  if(is("source-stats")||is("preview"))source_actor.stats=nullptr;
  if(is("preview"))execute=0;
  if(is("position-stats"))target.hit.targetsPosition=1;
  if(is("state"))Battle_actionState=nullptr;
  if(is("inactive-stats"))state.selection.primaryTarget.hpEffect=2;
  if(is("alias")){alias=true;from=to;}
  handlers.at(function)(&action,from,to,1,execute);
  if(target.before!=std::array<unsigned,4>{1,2,3,4}||target.after!=std::array<unsigned,4>{5,6,7,8})return 3;
  if(function==0&&!is("position-stats")&&(to->inflictStatuses!=0x8020||damage!=1||pp!=(execute!=0)))return 4;
  if(function==4&&recovery!=1)return 5;
  if(function==1&&(to->target.mp!=32748||Battle_lastActionStatValue!=0))return 6;
  std::printf("F%05u %s pass: calls%u damage%u recovery%u DP%u PP%u\n",494+function,mode,calls,damage,recovery,dp,pp);
}
