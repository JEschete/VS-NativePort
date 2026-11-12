// Real checked-live entry points: diagnostics terminate with 85, never a
// fabricated packed result. Empty/natural 0/0 is tested outside MIPS equality.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

#include "Battle/ActionCostsLive.hpp"
extern "C" {
#include "Battle/Action.h"
#include "Battle/ActionFlow.h"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
Battle_Actor* Battle_actors[42]{};
Battle_ActionState* Battle_actionState{};
Battle_ActionDefinition Battle_actionDefinitions[256]{};
void Core_Errorf(const char*,const char* message,...) {
  std::fprintf(stderr,"%s\n",message);
  std::_Exit(85);
}
}
namespace {
int selected=-1;
unsigned count{};
template<class F>void Refuses(F call){
  if(static_cast<int>(count++)==selected){call();std::_Exit(86);}
}
}
int main(int argc,char** argv){
  if(argc>1)selected=std::atoi(argv[1]);
  namespace b=VS::Battle;
  Battle_ActorStats stats{};
  Battle_Actor actor{};
  Battle_ActionState state{};
  Battle_actors[0]=&actor;
  Battle_actors[41]=&actor;
  actor.stats=&stats;
  actor.weaponDrawn=2;
  Battle_actionState=&state;
  auto& action=Battle_actionDefinitions[1];
  action.type=6;action.unlocked=1;
  unsigned first=count;
  for(unsigned id:{256U,0xFFFFFFFFU})
    Refuses([&]{(void)b::LiveActionCostAndAvailability(id,&stats,0);});
  for(unsigned kind:{1U,2U,3U,4U,6U}){
    action.type=static_cast<unsigned char>(kind);
    Refuses([&]{(void)b::LiveActionCostAndAvailability(1,nullptr,0);});
  }
  action.type=6;
  for(unsigned type:{0U,5U,6U,7U,255U}){
    stats.weapon.actionType=static_cast<unsigned char>(type);
    for(unsigned cost:{0U,1U,255U}){
      stats.weapon.risk=static_cast<char>(cost);
      if(type==0&&cost==0)continue;
      Refuses([&]{(void)b::LiveActionCostAndAvailability(1,&stats,0);});
    }
  }
  stats.weapon.actionType=2;
  Battle_actionState=nullptr;
  Refuses([&]{(void)b::LiveActionCostAndAvailability(1,&stats,0);});
  stats.weapon.actionType=0;stats.weapon.risk=0;
  stats.currentHp=stats.currentMp=-32768;stats.risk=65535;
  if(b::LiveActionCostAndAvailability(1,&stats,-1)!=0x01000000)return 1;
  if(stats.currentHp!=-32768||stats.currentMp!=-32768||stats.risk!=65535)return 2;
  action.type=5;action.cost=0;
  if(b::LiveActionCostAndAvailability(1,nullptr,-1)!=0x01050000)return 3;
  std::printf("F00510 refusals [%u,%u)\n",first,count);first=count;
  action.type=6;
  for(int slot:{-2147483647-1,-1,42,255,256}){
    Refuses([&]{b::LivePreviewActorActionCost(1,slot);});
  }
  Refuses([&]{b::LivePreviewActorActionCost(-1,0);});
  Refuses([&]{b::LivePreviewActorActionCost(256,0);});
  Battle_actors[0]=nullptr;
  Refuses([&]{b::LivePreviewActorActionCost(1,0);});
  Battle_actors[0]=&actor;actor.stats=nullptr;
  Refuses([&]{b::LivePreviewActorActionCost(1,0);});
  actor.stats=&stats;
  stats.weapon.actionType=5;
  Refuses([&]{b::LivePreviewActorActionCost(1,0);});
  stats.weapon.actionType=2;
  Refuses([&]{b::LivePreviewActorActionCost(1,0);});
  stats.weapon.actionType=0;
  b::LivePreviewActorActionCost(1,41);
  action.type=5;actor.stats=nullptr;
  b::LivePreviewActorActionCost(1,0);
  actor.stats=&stats;action.type=6;
  std::printf("F00511 refusals [%u,%u)\n",first,count);first=count;
  for(unsigned id:{0U,21U,54U,255U}){
    Refuses([&]{b::LiveAddActionRecoveryDelay(id,nullptr,-1);});
  }
  for(unsigned id:{256U,0xFFFFFFFFU}){
    Refuses([&]{b::LiveAddActionRecoveryDelay(id,&stats,1);});
  }
  for(unsigned id:{22U,53U}){
    b::LiveAddActionRecoveryDelay(id,nullptr,-1);
    b::LiveAddActionRecoveryDelay(id,reinterpret_cast<Battle_ActorStats*>(1),1);
  }
  b::LiveAddActionRecoveryDelay(0xFFFFFFFF,reinterpret_cast<Battle_ActorStats*>(1),0);
  std::printf("F00512 refusals [%u,%u)\n",first,count);first=count;
  for(int id:{1,140,141,223,224}){
    auto saved=Battle_actionDefinitions[id];
    Battle_actionDefinitions[id]=action;
    for(int slot:{-1,42,255,256})
      Refuses([&]{(void)b::LiveActorActionRestrictionFlags(slot,id);});
    Battle_actors[0]=nullptr;
    Refuses([&]{(void)b::LiveActorActionRestrictionFlags(0,id);});
    Battle_actors[0]=&actor;actor.stats=nullptr;
    Refuses([&]{(void)b::LiveActorActionRestrictionFlags(0,id);});
    actor.stats=&stats;
    stats.weapon.actionType=5;
    Refuses([&]{(void)b::LiveActorActionRestrictionFlags(0,id);});
    stats.weapon.actionType=2;
    Refuses([&]{(void)b::LiveActorActionRestrictionFlags(0,id);});
    stats.weapon.actionType=0;
    if(b::LiveActorActionRestrictionFlags(0,id)!=0)return 4;
    Battle_actionDefinitions[id]=saved;
  }
  for(int id:{-1,256})Refuses([&]{(void)b::LiveActorActionRestrictionFlags(0,id);});
  std::printf("F00548 refusals [%u,%u)\n",first,count);
  std::printf("%u diagnostic refusals; native0/0 and unused-pointer controls pass\n",count);
}
