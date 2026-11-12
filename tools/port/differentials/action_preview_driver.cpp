#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "Battle/ActionPreview.hpp"
namespace b = VS::Battle;
enum Index { F, LANE, SEED_BYTE, EXECUTE, SOURCE_KIND, COUNT, CATEGORY, PRE0, PRE1, STAT,
  DAMAGE, AFFINITY, DRAWN, BLADE, WEAPON_DAMAGE, CANCEL, SPAWN, QUEUE, TRACE,
  CHECK, ROLL, MUTATE, TARGET, LIMB, EFFECTS, THRESHOLD, IMPACT, FLAGS,
  X0, Y0, Z0, T0, X1, Y1, Z1, T1 };
using Vector=std::array<std::uint32_t,36>;
struct Actor final:b::ResolutionActor {
  unsigned id{};
  const Vector& v;
  Actor(unsigned i,const Vector& input):id(i),v(input){}
  std::uint8_t Slot() override{return 2;}
  std::uint8_t DefaultLimb() override{return id+2;}
  bool Cancels() override{return id==2?!v[CANCEL]:v[CANCEL]!=0;}
  bool HasBlade() override{return v[BLADE]!=0;}
  std::uint8_t DamageType() override{return v[WEAPON_DAMAGE];}
  std::array<std::int16_t,7> Affinities() override{
    std::array<std::int16_t,7> a{0,-1,4,4,-32768,2,9},out{};
    for(unsigned j=0;j<7;++j)out[j]=a[(j+v[LANE])%7];
    return out;
  }
};
struct Definition final:b::ResolutionDefinition {
  const Vector& v;
  bool self{};
  explicit Definition(const Vector& input):v(input){}
  b::ResolutionDescriptor Hit(int hit) override{
    if(hit==1&&self)return {3,0,0,0};
    return {static_cast<std::uint8_t>(v[PRE0+hit]),static_cast<std::uint8_t>(v[STAT]),
      static_cast<std::uint8_t>(v[DAMAGE]),static_cast<std::uint8_t>(v[AFFINITY])};
  }
  bool Redirects() override{return v[FLAGS]!=0;}
  std::uint8_t Category() override{return v[CATEGORY];}
  b::PreviewVolume Range() override{return {255,1,0,{255,64,128},0,{}};}
  b::PreviewVolume Area() override{return {255,1,0,{7,8,9},255,{}};}
};
struct World final:b::ActionPreviewWorld {
  const Vector& v;
  ::Definition definition;
  std::array<::Actor,3> actors;
  b::ActionRequest result{},source{};
  std::uint32_t prerequisite=456,previous=123,last=789,queue;
  std::vector<std::uint32_t> calls;
  bool swapped{};
  World(const Vector& input):v(input),definition(v),actors{::Actor(0,v),::Actor(1,v),::Actor(2,v)},queue(v[QUEUE]){}
  template<class... T>void Event(T... values){(calls.push_back(static_cast<std::uint32_t>(values)),...);}
  b::ResolutionDefinition* Definition(std::uint32_t) override{return &definition;}
  std::optional<b::ResolutionActor*> Actor(int slot) override{
    if(slot<0||slot>=16)return std::nullopt;
    return &actors[swapped&&slot<2?2:std::min(slot,2)];
  }
  std::optional<std::uint32_t> WeaponDrawn(int slot) override{return slot==2?v[DRAWN]:v[DRAWN]^1;}
  std::optional<std::uint8_t> SpawnId(int) override{return v[SPAWN];}
  std::optional<b::ActionPoint> SourceOrigin(int slot) override{
    Event(1,slot,0,0);
    if(v[MUTATE]==1&&v[F]==3)source.count=2;
    return b::ActionPoint{1,2,3,4};
  }
  b::ActionPoint Point(unsigned offset){
    std::array<std::uint16_t,4> p{};
    for(unsigned i=0;i<4;++i)p[i]=v[offset+i];
    return std::bit_cast<b::ActionPoint>(p);
  }
  std::optional<b::ActionPoint> TargetPosition(int slot,int limb) override{
    Event(2,slot,limb);return Point(X1);
  }
  std::optional<b::ActionPoint> LimbPosition(int slot,int limb) override{
    Event(20,slot,limb,2);
    if((limb+v[LANE])%3==1)return std::nullopt;
    return b::ActionPoint{static_cast<std::int16_t>(limb),0,0,0};
  }
  std::optional<b::ActionPoint> PreviewPosition(bool target) override{return Point(target?X1:X0);}
  bool Contains(const b::PreviewVolume& volume,b::ActionPoint point) override{
    auto words=std::bit_cast<std::array<std::uint32_t,4>>(volume);
    Event(21,words[0],words[1],words[2],words[3],point.x);
    return (point.x+v[LANE])%4!=2;
  }
  std::optional<b::ActionPoint> Trace(b::ActionPoint,b::ActionPoint,int slot) override{
    Event(3,slot);
    if(v[TRACE]==0xFFFFFFFF)return std::nullopt;
    return b::ActionPoint{11,22,33,std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(v[TRACE]))};
  }
  void ResetHitState() override{last=previous=prerequisite=0;}
  void Prerequisite(std::uint32_t value) override{prerequisite=value;}
  void PreviousHit(std::uint32_t value) override{previous=value;}
  std::optional<std::uint32_t> QueueIndex() override{return queue;}
  std::uint32_t Check(b::ResolutionDefinition&,b::ActionHit& target,int hit) override{
    Event(40+hit,target.actor);
    if(v[MUTATE]==4)result.count=0;
    return v[CHECK];
  }
  std::uint32_t Roll(b::ResolutionDefinition&,b::ActionHit&,b::ActionHit& target,int hit,int execute) override{
    Event(50+hit,target.actor,prerequisite,previous,execute);
    target.threshold=77;return v[ROLL];
  }
  void Effect(b::ResolutionDefinition&,b::ActionHit&,b::ActionHit& target,int execute,int hit) override{
    Event(60+hit,target.actor,execute,prerequisite,previous,last);
    target.hp=10+hit;target.effects|=1;last=100+hit;
    if(v[MUTATE]==2&&hit==0){result.count=1;definition.self=true;swapped=true;}
    if(v[MUTATE]==3){target.actor=2;target.limb=5;swapped=true;}
    if(v[MUTATE]==5)queue=0;
  }
  void Injuries(b::ActionHit& target) override{Event(70,target.actor,target.impact);}
  bool Recovery(std::uint32_t action,b::ResolutionActor& actor) override{
    Event(80,action,static_cast<::Actor&>(actor).id,1);
    if(v[MUTATE]==6)swapped=true;
    return true;
  }
  std::optional<std::uint32_t> Cost(std::uint32_t action,b::ResolutionActor* actor,int update) override{
    Event(90,action,static_cast<::Actor*>(actor)->id,update);return 0xAB123456;
  }
  bool Resolve(std::uint32_t action,b::ActionRequest& out,b::ActionRequest& in,int execute) override{
    Event(100,action,execute,in.count);
    return ActionPreviewWorld::Resolve(action,out,in,execute);
  }
};
std::uint32_t Hash(const b::ActionRequest& request,bool preview){
  auto bytes=std::bit_cast<std::array<std::uint8_t,sizeof(b::ActionRequest)>>(request);
  std::uint32_t h=2166136261;
  for(unsigned i=0;i<0x84C;++i){
    unsigned at=i<0x844?i:static_cast<unsigned>(offsetof(b::ActionRequest,focus))+i-0x844;
    const auto value=preview&&(i==5||i>=0x844)?0:bytes[at];
    h=(h^value)*16777619;
  }
  return h;
}
void Setup(World& w){
  const auto& v=w.v;
  std::array<std::uint8_t,sizeof(b::ActionRequest)> bytes;
  bytes.fill(static_cast<std::uint8_t>(v[SEED_BYTE]));
  w.source=w.result=std::bit_cast<b::ActionRequest>(bytes);
  w.source.primary.actor=0;w.source.primary.limb=v[LIMB];
  w.source.primary.position_kind=std::bit_cast<int>(v[SOURCE_KIND]);
  w.source.count=v[COUNT];
  for(unsigned i=0;i<30;++i){
    auto& hit=w.source.targets[i];
    hit.actor=i%2+1;hit.limb=(i+3)%6;
    hit.position_kind=std::array<int,5>{0,1,5,6,-1}[i%5];
  }
  auto& hit=w.result.primary;
  hit.position_kind=std::bit_cast<int>(v[SOURCE_KIND]);
  hit.threshold=v[THRESHOLD];hit.impact=v[IMPACT];
  hit.remove_statuses=v[LANE]%2;hit.inflict_statuses=v[LANE]/2%2;hit.effects=v[EFFECTS];
}
int main(){
  std::string line;
  while(std::getline(std::cin,line)){
    std::istringstream in(line);
    Vector v{};
    for(auto& word:v)in>>word;
    if(!in)return 2;
    World w(v);Setup(w);
    std::uint32_t returned=0;
    switch(v[F]){
      case 0:b::ClearActionRequest(w.result);break;
      case 1:b::InitializeActionHit(w.result.primary);break;
      case 2:b::FinalizeActionHit(w.result.primary);break;
      case 3:if(!b::ResolveActionTargets(23,w.result,w.source,v[EXECUTE],w))return 3;break;
      case 4:{auto r=b::PreviewActionAgainstLimb(23,0,v[TARGET],v[LIMB],w.result,w);
        if(!r)return 4;returned=r.value();break;}
      case 5:{auto r=b::PreviewActionAgainstActor(23,0,v[TARGET],v[LIMB],w.result,w);
        if(!r)return 5;returned=r.value();break;}
    }
    std::cout<<returned<<' '<<Hash(w.result,v[F]>=4)<<' '<<Hash(w.source,false)<<' '
      <<w.prerequisite<<' '<<w.previous<<' '<<w.last<<' '<<w.calls.size();
    for(auto word:w.calls)std::cout<<' '<<word;
    std::cout<<'\n';
  }
}
