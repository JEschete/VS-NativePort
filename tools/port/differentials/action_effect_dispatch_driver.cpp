// Original-layout transport only; production policies perform all decisions.
#include "Battle/ActionEffectDispatch.hpp"
#include "Battle/StatusQueue.hpp"
#include <array>
#include <bit>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
extern "C" void Core_Debugf(const char*,const char*,...) {}
namespace {
namespace b=VS::Battle;
constexpr unsigned kSource=0x80500000,kTarget=0x80501000,kAction=0x80502000;
template<std::size_t N>struct Block {
  std::array<unsigned,N/4> words{};
  unsigned get(unsigned off,unsigned n=4)const{
    unsigned result{};std::memcpy(&result,reinterpret_cast<const char*>(words.data())+off,n);return result;
  }
  void put(unsigned off,unsigned value,unsigned n=4){
    std::memcpy(reinterpret_cast<char*>(words.data())+off,&value,n);
  }
  void load(std::istream& in){for(auto& v:words)in>>v;}
};
short low(unsigned v){return std::bit_cast<short>(static_cast<unsigned short>(v));}
struct World final:b::LimbInjuryQueue,b::ActionEffectWorld {
  unsigned fn{},lane{},execute{},mutation{},flag{},category{},calls{},owner{};
  Block<100> source,target;
  Block<52> action;
  std::array<Block<0x960>,3> stats{};
  Block<512> rules;
  Block<4> ids;
  Block<32> arts;
  Block<80> thresholds;
  std::array<unsigned,3> counted{},bindings{0,1,2};
  unsigned last=0xFEED1234;
  std::vector<unsigned> events;
  unsigned slot()const {auto s=target.get(16,1);return s==40?2:s;}
  Block<0x960>& current(){return stats[bindings[slot()]];}
  template<class...T>void event(T...v){(events.push_back(static_cast<unsigned>(v)),...);}
  void mutate(){
    ++calls;if(!mutation)return;
    target.put(16,std::array<unsigned,3>{0,1,40}[mutation-1],1);
    bindings[0]=2;action.put(0,(mutation+1)<<17);
    stats[0].put(0x948,calls%2?0xFFFFFFFF:0);
    target.put(44,(target.get(44)&~3U)|calls%4);
    ids.put(calls%4,(calls+5)%32,1);
    stats[0].put(0x398+(calls%4)*0xDC,2,2);
    rules.put(16*((calls+5)%32),1<<calls);
    if(mutation==3)target.put(80,calls%2);
  }
  std::uint8_t MobilityLimb(std::size_t i)override{return stats[owner].get(0x34+i,1);}
  std::int16_t CurrentHp(std::uint8_t limb)override{return low(stats[owner].get(0x398+limb*0xDC,2));}
  b::ActionResultChange PendingHp(std::uint8_t limb)override{
    return {low(target.get(48+limb*4,2)),static_cast<std::uint8_t>(target.get(44)&3)};
  }
  std::uint32_t ActiveStatuses()override{return stats[owner].get(0x948);}
  bool PlayerTarget()override{return target.get(16,1)==0;}
  std::uint8_t InjuryRule(std::size_t limb)override{return ids.get(limb,1);}
  std::uint32_t RuleMask(std::uint8_t rule)override{return rules.get(rule*16);}
  void QueueStatus(std::uint8_t rule,bool remove)override{
    event(remove?4:3,rule,kTarget);mutate();
    b::StatusRule r{rules.get(rule*16),rules.get(rule*16+4),rules.get(rule*16+8)};
    auto& s=current();
    b::StatusTarget t{s.get(0x948),s.get(0x944),s.get(0x957,1)==128};
    b::QueuedStatuses q{target.get(36),target.get(40),static_cast<b::StatusFeedback>((target.get(44)>>16)&3)};
    if(remove)b::QueueStatusRemovalMask(r,t,q);else b::QueueApplicableStatusChanges(r,t,q);
    target.put(36,q.added);target.put(40,q.removed);
  }
  bool TargetPosition()override{return target.get(80)!=0;}
  std::uint8_t TargetActorId()override{return current().get(0x957,1);}
  std::uint32_t TargetStatuses()override{return current().get(0x948);}
  std::uint8_t ActionType()override{return (action.get(0)>>17)&7;}
  void QueueExemptDamage()override{
    target.put(44,(target.get(44)&~3U)|1);target.put(20,target.get(20,2)+999,2);
  }
  void ClearLastValue()override{last=0;}
  void Dispatch(int hit,int ex)override{
    event(1,kAction,kSource,kTarget,hit,ex,action.get(20+hit*4)&127);mutate();target.put(20,target.get(20,2)+3,2);
  }
  void QueueStatusRemoval(std::uint32_t mask)override{target.put(40,target.get(40)|mask);}
  bool SourcePosition()override{return source.get(80)!=0;}
  std::uint8_t SourceSlot()override{return source.get(16,1);}
  std::int16_t TargetHp()override{return low(current().get(0x18,2));}
  std::int16_t PendingHp()override{return low(target.get(20,2));}
  std::uint8_t TargetSlot()override{return target.get(16,1);}
  bool KillCounted()override{return counted[bindings[slot()]]!=0;}
  void MarkKillCounted()override{counted[bindings[slot()]]=1;}
  std::uint8_t WeaponCategory()override{return category;}
  std::uint32_t ProgressFlag()override{
    event(2,0x82,counted[0],counted[1],counted[2]);
    if(mutation)for(unsigned i=0;i<10;++i){arts.put(i,(lane+i)%5,1);arts.put(12+i*2,(lane+i)%3,2);}
    return flag;
  }
  std::uint8_t LearnedArts(std::uint8_t c)override{return arts.get(c,1);}
  std::uint16_t WeaponKills(std::uint8_t c)override{return arts.get(12+c*2,2);}
  std::uint16_t RequiredKills(std::uint8_t c,std::uint8_t n)override{return thresholds.get(c*8+n*2,2);}
  void SetWeaponKills(std::uint8_t c,std::uint16_t n)override{arts.put(12+c*2,n,2);}
  void load(std::istream& in){
    in>>fn>>lane>>execute>>mutation>>flag>>category;
    source.load(in);target.load(in);action.load(in);
    for(auto& s:stats)s.load(in);rules.load(in);ids.load(in);arts.load(in);thresholds.load(in);
    counted.fill(lane%17==0?1:0);owner=slot();
  }
  void print(){
    for(auto n:target.words)std::cout<<n<<' ';std::cout<<last<<' ';
    for(auto n:counted)std::cout<<n<<' ';for(auto n:arts.words)std::cout<<n<<' ';
    std::cout<<events.size();for(auto n:events)std::cout<<' '<<n;std::cout<<'\n';
  }
};
}
int main(){
  std::string line;while(std::getline(std::cin,line)){
    std::istringstream in(line);World w;w.load(in);if(!in)return 2;
    if(w.fn==0)b::DeriveLimbInjuryStatuses(w);
    else b::ApplyActionHitEffect(w,std::bit_cast<int>(w.execute),w.lane%2);
    w.print();
  }
}
