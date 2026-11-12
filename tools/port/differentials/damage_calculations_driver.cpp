// Record projection only; calls the production typed core and real gem policy.
#include "Battle/DamageCalculations.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>
namespace {
namespace b=VS::Battle;
constexpr unsigned Table=0x800F1928, Bonus=0x800E82F4, Critical=0x800F1A0C;
constexpr unsigned Action=0x80500000, Source=0x80501000, Target=0x80502000;
constexpr unsigned Actors=0x80600000, kStats=0x80700000;
constexpr std::array<std::array<unsigned,2>,8> Blocks{{
  {Table-16,200},{Bonus-16,56},{Critical-16,36},{Action-16,84},
  {Source-16,100},{Target-16,100},{Actors-16,0x430},{kStats-16,0x1984}}};
struct Memory {
  std::array<std::vector<unsigned>,8> blocks;
  template<class T> T Read(unsigned address) const {
    for (unsigned i=0;i<Blocks.size();++i) if(address>=Blocks[i][0] && address+sizeof(T)<=Blocks[i][0]+Blocks[i][1]) {
      T value;
      std::memcpy(&value,reinterpret_cast<const std::byte*>(blocks[i].data())+address-Blocks[i][0],sizeof(T));
      return value;
    }
    std::abort();
  }
  template<class T> void Write(unsigned address,T value) {
    for (unsigned i=0;i<Blocks.size();++i) if(address>=Blocks[i][0] && address+sizeof(T)<=Blocks[i][0]+Blocks[i][1]) {
      std::memcpy(reinterpret_cast<std::byte*>(blocks[i].data())+address-Blocks[i][0],&value,sizeof(T));
      return;
    }
    std::abort();
  }
  unsigned Crc() const {
    unsigned crc=~0U;
    for (const auto& block:blocks) for(auto word:block) for(int byte=0;byte<4;++byte) {
      crc^=(word>>(8*byte))&255;
      for(int bit=0;bit<8;++bit) crc=(crc>>1)^(0xEDB88320U&(0U-(crc&1)));
    }
    return ~crc;
  }
};
struct World;
struct Owner final:b::DamageOwner {
  World& world; unsigned address;
  Owner(World& w,unsigned a):world(w),address(a){}
  b::DamageStats Read() override;
  std::optional<b::DamageLimb> Limb(int limb) override;
  std::uint32_t Statuses() override;
  std::uint16_t Risk() override;
  std::uint8_t EnemyClass() override;
  void GrowAttacker(int c,int a) override;
  void GrowDefender(int c,int a,int l) override;
};
struct World final:b::DamageWorld {
  Memory memory;
  unsigned draw{}, mutation{}, source{}, target{};
  std::vector<unsigned> events;
  std::array<std::optional<Owner>,2> owners;
  unsigned count{};
  void Mutate(int stage) {
    if ((stage==1||stage==2)&&(mutation&1)) {
      memory.Write<unsigned>(source+0x948,2);
      memory.Write<std::uint16_t>(source+0x20,stage==2?65535:0);
      memory.Write<unsigned>(Action,(memory.Read<unsigned>(Action)&~255U)|((mutation&16)?6:5));
    }
    if(stage==1&&(mutation&2)) {
      memory.Write<unsigned>(Actors+0x3C,kStats+0x1000);
      memory.Write<unsigned>(Actors+0x13C,kStats);
      memory.Write<std::uint8_t>(Source,255);
      memory.Write<std::uint8_t>(Target,255);
      memory.Write<std::uint8_t>(Target+1,128);
    }
    if(stage==1&&(mutation&8)) memory.Write<unsigned>(Target+0x40,1);
    if(stage==3&&(mutation&4)) {
      memory.Write<std::uint8_t>(source+0x37,7);
      memory.Write<std::uint16_t>(source+0x368,12345);
    }
    if(stage==4) memory.Write<std::uint16_t>(target+0x36A,23456);
  }
  b::DamageOwner* Stats(std::uint8_t slot) override {
    if(slot>=42||count>=owners.size()) return nullptr;
    const auto actor=memory.Read<unsigned>(Table+4*slot);
    return &owners[count++].emplace(*this,memory.Read<unsigned>(actor+0x3C));
  }
  std::optional<std::uint32_t> WeaponDrawn(std::uint8_t slot) override {
    return memory.Read<unsigned>(memory.Read<unsigned>(Table+4*slot)+0x20);
  }
  std::optional<b::DamageHit> Hit(int hit) override {
    auto word=memory.Read<unsigned>(Action+0x14+4*hit);
    return b::DamageHit{static_cast<std::uint8_t>((word>>27)&3),
      static_cast<std::uint8_t>(word>>29),static_cast<std::uint8_t>((word>>22)&31)};
  }
  std::optional<std::uint8_t> ActionType() override {return (memory.Read<unsigned>(Action)>>17)&7;}
  std::optional<std::uint8_t> ActionId() override {return memory.Read<std::uint8_t>(Action);}
  std::optional<std::uint16_t> BladeBonus(std::uint8_t c) override {return memory.Read<std::uint16_t>(Bonus+2*c);}
  std::int32_t TargetPosition() override {return memory.Read<std::int32_t>(Target+0x40);}
  int Random(int bound,bool averaged) {
    events.insert(events.end(),{averaged?2U:1U,static_cast<unsigned>(bound),memory.Read<unsigned>(Critical),0,0,0});
    Mutate(bound==100?2:1);
    return draw%bound;
  }
  std::int32_t RandomBounded(int bound) override {return Random(bound,false);}
  std::int32_t RandomAveraged(int bound) override {return Random(bound,true);}
  void SetCriticalHit() override {memory.Write<unsigned>(Critical,1);}
  void Grow(unsigned address,int c,int a,int l,bool defender) {
    events.insert(events.end(),{defender?4U:3U,Action,address,static_cast<unsigned>(c),
                               static_cast<unsigned>(a),static_cast<unsigned>(l)});
    Mutate(defender?4:3);
  }
};
b::DamageStats Owner::Read() {
  const auto& m=world.memory;
  return {m.Read<std::uint8_t>(address+0x957),static_cast<std::uint8_t>(m.Read<std::uint8_t>(address+0x37)&7),
    m.Read<std::uint16_t>(address+0x22),m.Read<std::uint16_t>(address+0x26),m.Read<std::uint16_t>(address+0x20),
    m.Read<VS::Items::WeaponLoadout>(address+0x3C),m.Read<VS::Items::ShieldLoadout>(address+0x1D4),
    m.Read<VS::Items::AccessorySpawnLoadout>(address+0x338)};
}
std::optional<b::DamageLimb> Owner::Limb(int limb) {
  const auto& m=world.memory;
  const auto base=address+0xDC*limb;
  return b::DamageLimb{m.Read<std::array<std::int16_t,4>>(base+0x3A0),
    m.Read<VS::Items::AffinityTotals>(base+0x3A8),m.Read<VS::Items::ArmorLoadout>(base+0x3B8)};
}
std::uint32_t Owner::Statuses(){return world.memory.Read<unsigned>(address+0x948);}
std::uint16_t Owner::Risk(){return world.memory.Read<std::uint16_t>(address+0x20);}
std::uint8_t Owner::EnemyClass(){return world.memory.Read<std::uint8_t>(address+0x37)&7;}
void Owner::GrowAttacker(int c,int a){world.Grow(address,c,a,0,false);}
void Owner::GrowDefender(int c,int a,int l){world.Grow(address,c,a,l,true);}
}
int main(){
  std::string line;
  while(std::getline(std::cin,line)){
    World w;
    std::istringstream input(line);
    unsigned function{},hit{},random{},statuses{};
    input>>function>>hit>>random>>statuses>>w.draw>>w.mutation;
    for(unsigned i=0;i<Blocks.size();++i){
      w.memory.blocks[i].resize(Blocks[i][1]/4);
      for(auto& word:w.memory.blocks[i]) input>>word;
    }
    w.source=w.memory.Read<unsigned>(Actors+0x3C);
    w.target=w.memory.Read<unsigned>(Actors+0x13C);
    b::DamageTarget source{w.memory.Read<std::uint8_t>(Source),w.memory.Read<std::int8_t>(Source+1)};
    b::DamageTarget target{w.memory.Read<std::uint8_t>(Target),w.memory.Read<std::int8_t>(Target+1)};
    std::optional<std::int16_t> result;
    switch(function){
      case 0:result=b::CalculatePhysicalHitDamage(&source,&target,hit,random,statuses,w);break;
      case 1:result=b::PhysicalDamageWithoutDebuffs(&source,&target,hit,random,w);break;
      case 2:result=b::CalculateSpellHitDamage(&source,&target,hit,random,w);break;
      case 3:result=b::CalculateSpellHealing(&source,&target,hit,random,w);break;
      case 4:result=b::PhysicalDamageWithDebuffs(&source,&target,hit,random,w);break;
      default:std::abort();
    }
    if(!result) std::abort();
    std::cout<<static_cast<unsigned>(result.value())<<' '<<w.memory.Crc();
    for(auto value:w.events) std::cout<<' '<<value;
    std::cout<<'\n';
  }
}
