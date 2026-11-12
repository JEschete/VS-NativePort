#include <bit>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "Battle/ActionCosts.hpp"

namespace b = VS::Battle;
struct Owner final : b::ActionCostActor {
  b::ActionResources r{};
  std::uint16_t timer{};
  std::uint8_t bonus{}, delay{};
  std::uint32_t status{};
  b::ActionResources Resources() override { return r; }
  void Pay(b::CostResource resource, std::uint16_t value) override {
    switch(resource) {
      case b::CostResource::kMp: r.mp=std::bit_cast<std::int16_t>(value);break;
      case b::CostResource::kHp: r.hp=std::bit_cast<std::int16_t>(value);break;
      case b::CostResource::kRisk: r.risk=value;break;
      case b::CostResource::kWeaponPp:r.weapon_pp=value;break;
      case b::CostResource::kNone:std::abort();
    }
  }
  std::uint16_t RecoveryTimer() override{return timer;}
  void SetRecoveryTimer(std::uint16_t value) override{timer=value;}
  std::uint8_t RecoveryBonus() override{return bonus;}
  std::uint8_t WeaponRecoveryDelay() override{return delay;}
  std::uint32_t Statuses() override{return status;}
};
struct Definition final:b::ActionCostDefinition {
  std::uint8_t type{},cost{},delay{};
  bool unlocked{};
  std::uint8_t Type() override{return type;}
  std::uint8_t Cost() override{return cost;}
  std::uint8_t RecoveryDelay() override{return delay;}
  bool Unlocked() override{return unlocked;}
};
struct World final:b::ActionCostWorld {
  ::Definition definition;
  Owner owner,other;
  std::uint32_t chain{},drawn{};
  bool mutate{},swapped{},null_stats{};
  std::vector<std::uint32_t> calls;
  b::ActionCostDefinition* Definition(std::uint32_t action) override {
    return action<256?&definition:nullptr;
  }
  std::optional<b::ActionCostActor*> Actor(int slot) override {
    if(slot<0||slot>=42)return std::nullopt;
    if(null_stats)return nullptr;
    return swapped?&other:&owner;
  }
  std::optional<std::uint32_t> ChainStep() override{return chain;}
  std::optional<std::uint32_t> WeaponDrawn(int) override {
    return swapped?drawn^2:drawn;
  }
  std::optional<b::ActionCost> Evaluate(std::uint32_t action,b::ActionCostActor* actor,int update) override {
    calls.insert(calls.end(),{action,actor==&owner?0x80500000U:actor==nullptr?0U:0x80501000U,
      static_cast<std::uint32_t>(update)});
    auto result=ActionCostWorld::Evaluate(action,actor,update);
    if(mutate){owner.timer^=1;owner.status^=0x3000;definition.unlocked=!definition.unlocked;swapped=true;}
    return result;
  }
};
int main(){
  std::string line;
  while(std::getline(std::cin,line)){
    std::istringstream in(line);
    std::uint32_t f,action,type,cost,wt,wc,hp,mp,risk,pp,chain,execute,timer,bonus,delay,wd,status,drawn,slot,unlock,mutate,null_stats;
    in>>f>>action>>type>>cost>>wt>>wc>>hp>>mp>>risk>>pp>>chain>>execute>>timer>>bonus>>delay>>wd>>status>>drawn>>slot>>unlock>>mutate>>null_stats;
    if(!in)return 2;
    World w;
    w.owner.r={std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(hp)),
      std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(mp)),
      static_cast<std::uint16_t>(risk),static_cast<std::uint16_t>(pp),
      static_cast<std::uint8_t>(wt),static_cast<std::uint8_t>(wc)};
    w.owner.timer=static_cast<std::uint16_t>(timer);
    w.owner.bonus=static_cast<std::uint8_t>(bonus);
    w.owner.delay=static_cast<std::uint8_t>(wd);
    w.owner.status=status;
    w.definition.type=static_cast<std::uint8_t>(type);
    w.definition.cost=static_cast<std::uint8_t>(cost);
    w.definition.delay=static_cast<std::uint8_t>(delay);
    w.definition.unlocked=unlock!=0;
    w.chain=chain;w.drawn=drawn;w.mutate=mutate!=0;w.null_stats=null_stats!=0;
    std::uint32_t result=0;
    switch(f){
      case 0:{auto c=b::ActionCostAndAvailability(action,w.null_stats?nullptr:&w.owner,std::bit_cast<int>(execute),w);
        if(!c)return 3;result=c.value().Packed();break;}
      case 1:if(!b::PreviewActorActionCost(std::bit_cast<int>(action),static_cast<int>(slot),w))return 4;break;
      case 2:if(!b::AddActionRecoveryDelay(action,action==0xFFFFFFFF?nullptr:&w.owner,std::bit_cast<int>(execute),w))return 5;break;
      case 3:{auto flags=b::ActorActionRestrictionFlags(static_cast<int>(slot),std::bit_cast<int>(action),w);
        if(!flags)return 6;result=flags.value();break;}
    }
    std::cout<<result<<' '<<static_cast<std::uint16_t>(w.owner.r.hp)<<' '
      <<static_cast<std::uint16_t>(w.owner.r.mp)<<' '<<w.owner.r.risk<<' '<<w.owner.r.weapon_pp
      <<' '<<w.owner.timer<<' '<<w.owner.status<<' '<<w.calls.size()/3;
    for(auto word:w.calls)std::cout<<' '<<word;
    std::cout<<'\n';
  }
}
