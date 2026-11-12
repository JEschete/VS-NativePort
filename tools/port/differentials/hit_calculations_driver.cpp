// Test-only projection of guarded original memory; gem algorithms are real.
#include "Battle/HitCalculations.hpp"

#include <array>
#include <bit>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {
namespace battle = VS::Battle;
constexpr unsigned kTable=0x800F1928, kActors=0x80500000, kStats=0x80600000;
constexpr unsigned kAction=0x80700000, kSource=0x80700100, kTarget=0x80700200;
constexpr std::array<unsigned,10> kAddresses{kTable,kActors,kActors+0x100,kActors+0x200,
    kStats,kStats+0x1000,kAction,kSource,kTarget,0x800F1A04};
constexpr std::array<unsigned,10> kSizes{200,112,112,112,2436,2436,84,100,100,36};
using Memory=std::array<unsigned,5728/4>;
struct State {
  unsigned function{},action{},source{},target{},hit{},execute{},result{},draw{},mutation{};
  Memory memory{};
  std::vector<std::array<unsigned,5>> events;
  unsigned Offset(unsigned address) const {
    unsigned offset=0;
    for(unsigned i=0;i<kAddresses.size();++i){
      const auto start=kAddresses[i]-16;
      if(address>=start&&address<start+kSizes[i])return offset+address-start;
      offset+=kSizes[i];
    }
    std::abort();
  }
  template<class T> T Read(unsigned address) const {
    T value{};
    std::memcpy(&value,reinterpret_cast<const char*>(memory.data())+Offset(address),sizeof(T));
    return value;
  }
  template<class T> void Write(unsigned address,T value){
    std::memcpy(reinterpret_cast<char*>(memory.data())+Offset(address),&value,sizeof(T));
  }
  void Mutate(bool random) {
    if(function<3){
      if(mutation&1){
        for(unsigned owner=0;owner<2;++owner){
          const auto at=kStats+owner*0x1000;
          Write<std::uint16_t>(at+0x2A,60000);
          Write<std::uint8_t>(at+0xCA+0x60,2);
          Write<std::uint8_t>(at+0x232+0x60,6);
          Write<std::uint8_t>(at+0x957,41);
        }
        Write<unsigned>(kActors+0x220,1);
        Write<unsigned>(kAction,(Read<unsigned>(kAction)&~0xE0000U)|0x20000);
      }
      if(mutation&2){
        Write<unsigned>(kActors+0x3C,kStats+0x1000);
        Write<unsigned>(kActors+0x13C,kStats);
        Write<std::uint8_t>(kSource,1);
        Write<std::uint8_t>(kTarget,0);
      }
      return;
    }
    if(!random){
      if(mutation&1){
        Write<std::uint8_t>(kTarget,0);
        Write<unsigned>(kTarget+0x40,1);
        Write<unsigned>(kStats+0x948,0xFFFFFFFF);
        Write<unsigned>(kAction,(Read<unsigned>(kAction)&~0xE0000U)|0x20000);
      }
      if(mutation&2)Write<unsigned>(0x800F1A00,0);
      Write<unsigned>(0x800F1A08,0x12345678);
    }else if(mutation&4){
      Write<std::uint8_t>(kSource,0);
      Write<unsigned>(kSource+0x40,0);
      Write<std::uint8_t>(kTarget+2,77);
      Write<unsigned>(0x800F1A00,0);
    }
  }
  int Random(unsigned kind,int bound){
    events.push_back({kind,static_cast<unsigned>(bound),Read<std::uint8_t>(kTarget+2),0,0});
    Mutate(true);
    return std::bit_cast<int>(draw);
  }
  unsigned Stats(unsigned slot) const {
    if(slot>=42)return 0;
    const auto actor=Read<unsigned>(kTable+slot*4);
    return actor?Read<unsigned>(actor+0x3C):0;
  }
};
struct Gems final:battle::HitModifierWorld {
  State& s;
  explicit Gems(State& state):s(state){}
  battle::HitModifierStats At(unsigned at){
    battle::HitModifierStats stats;
    stats.actor_id=s.Read<std::uint8_t>(at+0x957);
    for(unsigned i=0;i<3;++i){
      stats.weapon_gems[i]=s.Read<std::uint8_t>(at+0xCA+0x30*i);
      stats.shield_gems[i]=s.Read<std::uint8_t>(at+0x232+0x30*i);
    }
    return stats;
  }
  std::optional<battle::HitModifierStats> Stats(std::uint8_t slot)override{
    const auto at=s.Stats(slot);
    return at?std::optional{At(at)}:std::nullopt;
  }
  std::optional<unsigned> WeaponDrawn(std::uint8_t slot)override{
    if(slot>=42)return std::nullopt;
    const auto actor=s.Read<unsigned>(kTable+slot*4);
    return actor?std::optional{s.Read<unsigned>(actor+0x20)}:std::nullopt;
  }
  std::optional<battle::HitModifierAction> Action()override{
    if(!s.action)return std::nullopt;
    return battle::HitModifierAction{s.Read<std::uint8_t>(s.action),
      static_cast<std::uint8_t>((s.Read<unsigned>(s.action)>>17)&7),
      {static_cast<std::uint8_t>(s.Read<unsigned>(s.action+0x14)&127),
       static_cast<std::uint8_t>(s.Read<unsigned>(s.action+0x18)&127)}};
  }
};
struct Access final:battle::HitAttributeAccess {
  State& s;
  Gems& gems;
  unsigned at;
  Access(State& state,Gems& world,unsigned address):s(state),gems(world),at(address){}
  battle::HitAttributes Attributes(bool intelligence){
    battle::HitAttributes a;
    a.base=s.Read<std::uint16_t>(at+(intelligence?0x26:0x2A));
    a.accessory=s.Read<std::int16_t>(at+(intelligence?0x36A:0x36C));
    a.weapon=s.Read<std::int16_t>(at+(intelligence?0x158:0x15C));
    a.shield=s.Read<std::int16_t>(at+(intelligence?0x2BC:0x2C0));
    for(unsigned i=0;i<6;++i)a.armor[i]=s.Read<std::int16_t>(at+(intelligence?0x3F0:0x3F4)+0xDC*i);
    return a;
  }
  battle::HitAttributeStats Read()override{
    battle::HitAttributeStats a;
    a.actor_id=s.Read<std::uint8_t>(at+0x957);
    a.risk=s.Read<std::uint16_t>(at+0x20);
    a.hp=s.Read<std::int16_t>(at+0x18);
    a.max_hp=s.Read<std::int16_t>(at+0x1A);
    a.agility=Attributes(false);a.intelligence=Attributes(true);
    for(unsigned i=0;i<6;++i)a.defense[i]=s.Read<std::int8_t>(at+0x39C+0xDC*i);
    return a;
  }
  std::optional<std::int16_t> AttackBonus()override{
    const auto stats=gems.At(at);
    return battle::AttackGemHitBonus(&stats,gems);
  }
  std::optional<std::int16_t> DefenseBonus(int hit)override{
    const auto stats=gems.At(at);
    return battle::DefenseGemHitBonus(&stats,hit,gems);
  }
};
struct Attributes final:battle::HitAttributeWorld {
  State& s;
  Gems gems;
  std::array<std::optional<Access>,2> owners{};
  unsigned count{};
  explicit Attributes(State& state):s(state),gems(s){}
  battle::HitAttributeAccess* Stats(std::uint8_t slot)override{
    const auto at=s.Stats(slot);
    return at?&owners.at(count++).emplace(s,gems,at):nullptr;
  }
  std::optional<unsigned> WeaponDrawn(std::uint8_t slot)override{return gems.WeaponDrawn(slot);}
  std::optional<std::uint8_t> Cost()override{
    return s.action?std::optional{s.Read<std::uint8_t>(s.action+3)}:std::nullopt;
  }
  int RandomAveraged(int bound)override{return s.Random(1,bound);}
};
struct Dispatch final:battle::ActionCalculationWorld {
  State& s;
  explicit Dispatch(State& state):s(state){}
  std::optional<battle::CalculationTarget> Hit(unsigned at){
    if(!at)return std::nullopt;
    return battle::CalculationTarget{s.Read<std::uint8_t>(at),s.Read<int>(at+0x40),s.Read<std::uint8_t>(at+2)};
  }
  std::optional<battle::CalculationTarget> Target()override{return Hit(s.target);}
  std::optional<battle::CalculationTarget> Source()override{return Hit(s.source);}
  std::optional<battle::CalculationActor> Actor(std::uint8_t slot)override{
    const auto at=s.Stats(slot);
    if(!at)return std::nullopt;
    return battle::CalculationActor{s.Read<std::uint8_t>(at+0x957),s.Read<int>(at+0x948)};
  }
  std::optional<std::uint8_t> ActionType()override{
    if(!s.action)return std::nullopt;
    return (s.Read<unsigned>(s.action)>>17)&7;
  }
  std::optional<int> Calculate(unsigned kind,int hit,int execute){
    if(!s.action||hit<0||hit>=2)return std::nullopt;
    const auto word=s.Read<unsigned>(s.action+0x14+hit*4);
    const auto selector=kind==3?(word>>13)&7:(word>>16)&63;
    if(selector==0||selector>=(kind==3?8U:27U))return std::nullopt;
    s.events.push_back({kind,selector,static_cast<unsigned>(hit),static_cast<unsigned>(execute),s.Read<std::uint8_t>(kTarget+2)});
    s.Mutate(false);
    return std::bit_cast<int>(s.result);
  }
  std::optional<int> HitCalculator(int hit,int execute)override{return Calculate(3,hit,execute);}
  std::optional<std::int16_t> StatCalculator(int hit,int execute)override{
    const auto value=Calculate(4,hit,execute);
    return value?std::optional{static_cast<std::int16_t>(value.value())}:std::nullopt;
  }
  unsigned Prerequisite()override{return s.Read<unsigned>(0x800F1A00);}
  void StoreThreshold(std::uint8_t value)override{s.Write(s.target+2,value);}
  void StoreLastValue(unsigned value)override{s.Write(0x800F1A08,value);}
  int RandomBounded(int bound)override{return s.Random(2,bound);}
};
}  // namespace
int main(){
  std::string line;
  while(std::getline(std::cin,line)){
    State s;
    std::istringstream in(line);
    in>>s.function>>s.action>>s.source>>s.target>>s.hit>>s.execute>>s.result>>s.draw>>s.mutation;
    for(auto& word:s.memory)in>>word;
    if(!in||s.function>4)return 2;
    std::optional<int> result;
    const int hit=std::bit_cast<int>(s.hit),execute=std::bit_cast<int>(s.execute);
    if(s.function<3){
      Attributes world(s);
      battle::HitModifierTarget from{s.Read<std::uint8_t>(s.source),s.Read<std::int8_t>(s.source+1),s.Read<int>(s.source+0x40)};
      battle::HitModifierTarget to{s.Read<std::uint8_t>(s.target),s.Read<std::int8_t>(s.target+1),s.Read<int>(s.target+0x40)};
      const std::array functions{battle::AgilityHitChance,battle::IntelligenceHitChance,battle::CostAdjustedAgilityHitChance};
      result=functions[s.function](&from,&to,hit,execute,world);
    }else{
      Dispatch world(s);
      if(s.function==3)result=battle::RollActionHit(hit,execute,world);
      else result=battle::CalculateActionStatChange(hit,execute,world);
    }
    std::cout<<result.has_value()<<' '<<static_cast<unsigned>(result.value_or(0));
    for(const auto word:s.memory)std::cout<<' '<<word;
    std::cout<<' '<<s.events.size();
    for(const auto& event:s.events)for(auto value:event)std::cout<<' '<<value;
    std::cout<<'\n';
  }
}
