// Native side: the same owned queue/stat/limb/equipment algorithms as production.
#include "Battle/DamageRecoveryEffects.hpp"
#include "Battle/DamagePolicies.hpp"
#include "Battle/HitCalculations.hpp"
#include "Items/EquipmentWear.hpp"
#include <array>
#include <bit>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
namespace {
namespace b = VS::Battle;
namespace items = VS::Items;
constexpr unsigned kSource=0x80500000,kTarget=0x80501000,kAction=0x80502000,kStats=0x80700000;
constexpr std::array<unsigned,11> offsets{0x1C,0x14C,0x150,0x2B0,0x2B4,0x3E8,0x4C4,0x5A0,0x67C,0x758,0x834};
template<std::size_t N>struct Block {
  std::array<std::uint32_t,N/4> words{};
  unsigned get(unsigned offset,unsigned size=4) const {
    unsigned result{}; std::memcpy(&result,reinterpret_cast<const char*>(words.data())+offset,size);
    return result;
  }
  void put(unsigned offset,unsigned value,unsigned size=4) {
    std::memcpy(reinterpret_cast<char*>(words.data())+offset,&value,size);
  }
  template<class T>T read(unsigned offset) const {
    T value; std::memcpy(&value,reinterpret_cast<const char*>(words.data())+offset,sizeof(T));return value;
  }
  template<class T>void write(unsigned offset,const T& value) {
    std::memcpy(reinterpret_cast<char*>(words.data())+offset,&value,sizeof(T));
  }
};
short low(unsigned value){return std::bit_cast<short>(static_cast<unsigned short>(value));}
unsigned wide(short value){return static_cast<unsigned>(static_cast<int>(value));}
struct Queue final:b::DamageRecoveryQueue,b::ActionCalculationWorld {
  unsigned function{},lane{},amount{},execute{},mode{},critical{},mutate{},category{},type{};
  unsigned last=0xAABBCCDD, global_critical=99, source_owner{},target_owner{};
  std::array<unsigned,3> bindings{0,1,2};
  Block<100> source,target,changed;
  std::array<Block<0x960>,3> stats{};
  std::array<short,8> armor{113,113,113,113,113,113,113,113};
  std::vector<unsigned> events;
  template<class...T>void Event(T... values){(events.push_back(static_cast<unsigned>(values)),...);}
  unsigned source_address() const{return lane%7==0?kTarget:kSource;}
  unsigned owner(const Block<100>& hit) const {
    const auto slot=hit.get(16,1);return bindings[slot==40?2:slot];
  }
  void CaptureDamageOwners() override {
    source_owner=owner(lane%7==0?target:source);CaptureTargetOwner();
  }
  void CaptureTargetOwner() override {target_owner=owner(target);}
  void ClearCriticalHit() override {global_critical=0;}
  bool CriticalHit() override {return global_critical!=0;}
  std::int16_t Calculate() override {
    Event(1,kAction,source_address(),kTarget,1,execute,global_critical);
    return b::CalculateActionStatChange(1,std::bit_cast<int>(execute),*this).value();
  }
  static std::pair<unsigned,unsigned> field(b::ActionResultField f) {
    switch(f) {
      case b::ActionResultField::kHp:return {4,0};
      case b::ActionResultField::kMp:return {6,2};
      case b::ActionResultField::kWeaponPp:return {0x3A,20};
      case b::ActionResultField::kShieldPp:return {0x3E,24};
      default:std::abort();
    }
  }
  b::ActionResultChange ReadChange(b::ActionResultField f) override {
    const auto [offset,shift]=field(f);
    return {low(target.get(16+offset,2)),static_cast<std::uint8_t>((target.get(44)>>shift)&3)};
  }
  void WriteChange(b::ActionResultField f,b::ActionResultChange change) override {
    const auto [offset,shift]=field(f);
    target.put(16+offset,static_cast<unsigned short>(change.amount),2);
    target.put(44,(target.get(44)&~(3U<<shift))|(change.mode<<shift));
  }
  bool TargetsPosition() override {return target.get(80)!=0;}
  std::array<b::StatFactorVital,6> vitals() {
    std::array<b::StatFactorVital,6> result{};
    const auto& actor=stats[owner(target)];
    for(unsigned i=0;i<6;++i)result[i]={low(actor.get(0x398+i*0xDC,2)),low(actor.get(0x39A+i*0xDC,2))};
    return result;
  }
  std::array<short,6> limbs() {
    std::array<short,6> result{};
    for(unsigned i=0;i<6;++i)result[i]=low(target.get(48+i*4,2));return result;
  }
  void store_limbs(const std::array<short,6>& values) {
    for(unsigned i=0;i<6;++i)target.put(48+i*4,static_cast<unsigned short>(values[i]),2);
  }
  void DistributeDamage(std::int16_t value) override {
    Event(3,kAction,kTarget,wide(value),target.get(20,2),target.get(44));
    auto values=limbs();const auto actor=owner(target);const auto selected=target.get(17,1);
    std::array<std::uint8_t,6> splash{};
    for(unsigned i=0;i<6;++i)splash[i]=stats[actor].get(0x920+selected*6+i,1);
    if(!b::DistributeHitLimbDamage(category,selected,value,vitals(),splash,values,armor))std::abort();
    store_limbs(values);
  }
  void RecoverLimbs(std::int16_t value) override {
    Event(4,kAction,kTarget,wide(value),target.get(20,2),target.get(44));
    auto values=limbs();b::DistributeLimbRecovery(value,vitals(),values,armor);store_limbs(values);
  }
  std::uint32_t TargetRemovalStatuses() override {
    return stats[target_owner].get(0x957,1)==128?0:stats[target_owner].get(0x948)&0x8000;
  }
  void QueueRemovalStatuses(std::uint32_t value) override {target.put(40,target.get(40)|value);}
  items::DefenderEquipment defender() {
    items::DefenderEquipment value{};
    const auto& actor=stats[target_owner];const auto slot=actor.get(0x957,1);
    const auto drawn_owner=slot==40?2:slot;
    value.weapon_drawn=((lane+drawn_owner)&1)!=0;
    value.shield=actor.read<items::ShieldLoadout>(0x1D4);
    for(unsigned i=0;i<6;++i)value.limbs[i].armor=actor.read<items::ArmorLoadout>(0x3B8+i*0xDC);
    return value;
  }
  void commit(const items::WeaponLoadout& weapon,const items::DefenderEquipment& value) {
    stats[source_owner].write(0x3C,weapon);
    stats[target_owner].write(0x1D4,value.shield);
    for(unsigned i=0;i<6;++i)stats[target_owner].write(0x3B8+i*0xDC,value.limbs[i].armor);
  }
  void equipment(short value,bool pp) {
    Event(pp?6:5,kAction,kStats+source_owner*0x1000,kStats+target_owner*0x1000,wide(value),
          stats[source_owner].get(0x150,2),stats[target_owner].get(0x2B4,2));
    auto weapon=stats[source_owner].read<items::WeaponLoadout>(0x3C);
    auto defense=defender(); items::ItemAction action{};action.type=type;
    const auto result=pp?items::AccumulateHitEquipmentPp(action,weapon,defense,value):
      items::ConsumeHitEquipmentDp(action,weapon,defense,std::span<const short,6>(armor.data(),6),value);
    if(result!=items::ItemsStatus::kOk)std::abort();commit(weapon,defense);
  }
  void ConsumeEquipmentDp(std::int16_t value) override {equipment(value,false);}
  void AccumulateEquipmentPp(std::int16_t value) override {equipment(value,true);}
  std::int16_t TargetMp() override{return low(stats[target_owner].get(0x1C,2));}
  void StoreLastValue(std::int16_t value) override{last=wide(value);}
  bool HasWeapon() override{return stats[target_owner].get(0x54,2)!=0;}
  bool HasShield() override{return stats[target_owner].get(0x1EC,2)!=0;}
  std::uint16_t WeaponPp() override{return stats[target_owner].get(0x14C,2);}
  std::uint16_t ShieldPp() override{return stats[target_owner].get(0x2B0,2);}
  std::uint8_t SelectedPrimaryHpMode() override{return mode;}
  std::optional<b::CalculationTarget> Target() override{
    return b::CalculationTarget{static_cast<std::uint8_t>(target.get(16,1)),std::bit_cast<int>(target.get(80)),0};
  }
  std::optional<b::CalculationTarget> Source() override{return {};}
  std::optional<b::CalculationActor> Actor(std::uint8_t slot) override{
    const auto& s=stats[bindings[slot==40?2:slot]];
    return b::CalculationActor{static_cast<std::uint8_t>(s.get(0x957,1)),std::bit_cast<int>(s.get(0x948))};
  }
  std::optional<std::uint8_t> ActionType() override{return {};}
  std::optional<int> HitCalculator(int,int) override{return {};}
  std::optional<std::int16_t> StatCalculator(int,int) override{
    Event(2,kAction,source_address(),kTarget,1,execute,global_critical);
    if(mutate) {
      target=changed;bindings[0]=bindings[1]=2;
      stats[1].put(0x1C,static_cast<unsigned>(-19),2);
      stats[1].put(0x948,mutate==1?0x8000:0x4000);
      if(execute==0)stats[1].put(0x957,mutate==2?128:1,1);
    }
    global_critical=critical;return low(amount);
  }
  std::uint32_t Prerequisite() override{return 0;}
  void StoreThreshold(std::uint8_t) override{}
  void StoreLastValue(std::uint32_t value) override{last=value;}
  std::int32_t RandomBounded(int) override{return 0;}
};
}
int main(){
  std::string line;
  while(std::getline(std::cin,line)){
    std::istringstream in(line);Queue q;
    in>>q.function>>q.lane>>q.amount>>q.execute>>q.mode>>q.critical>>q.mutate>>q.category>>q.type;
    for(auto* b:{&q.source,&q.target,&q.changed})for(auto& value:b->words)in>>value;
    for(auto& b:q.stats)for(auto& value:b.words)in>>value;
    if(!in)return 2;
    switch(q.function){
      case 0:b::QueueHitDamageAndEquipmentWear(q,q.execute!=0);break;
      case 1:b::QueueMpDrain(q);break;
      case 2:b::QueueWeaponPpDrain(q);break;
      case 3:b::QueueShieldPpDrain(q);break;
      case 4:b::QueueHpRecovery(q);break;
      case 5:b::QueueMpRecovery(q);break;
      default:return 3;
    }
    for(const auto* b:{&q.source,&q.target})for(auto value:b->words)std::cout<<value<<' ';
    for(auto value:std::bit_cast<std::array<unsigned,4>>(q.armor))std::cout<<value<<' ';
    std::cout<<q.last<<' '<<q.global_critical<<' ';
    for(const auto& b:q.stats)for(auto offset:offsets)std::cout<<b.get(offset,2)<<' ';
    std::cout<<q.events.size();for(auto value:q.events)std::cout<<' '<<value;std::cout<<'\n';
  }
}
