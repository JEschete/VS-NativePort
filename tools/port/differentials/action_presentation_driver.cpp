// Typed policy transport; original instructions own the independent oracle.
#include "TestSupport/ActionPresentationFixture.hpp"
#include <array>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
namespace {
namespace p = VS::test_support::presentation;
namespace b = VS::Battle;
enum Index { F, LANE, ACTION, ENABLED, SOURCE, SOURCE_KIND, INDEX, SLOT, MAPPING,
  ANIMATION, FRAME, PREVIEW, MODE, KIND, ITEM, CATEGORY, ATTACK, HEAL,
  DISTANCE, ZOOM, STATUS, EFFECT, VARIANT, INFO, MUTATE, SHAPE };
using Vector=std::array<unsigned,26>;
void fill(p::World& w,const Vector& v){
  w.presentation.fill(v[ENABLED]!=0);w.effects.fill(v[EFFECT]);w.zooms.fill(v[ZOOM]);
  w.mappings.fill(v[MAPPING]);w.attacks.fill(v[ATTACK]);w.heals=v[HEAL];
  w.item=v[ITEM];w.category=v[CATEGORY];w.selection_kind=std::bit_cast<short>(static_cast<unsigned short>(v[KIND]));
  w.statuses=v[STATUS];w.frame=v[FRAME];w.distance=std::bit_cast<int>(v[DISTANCE]);w.information=v[INFO]!=0;
  for(unsigned i=0;i<16;++i){
    w.defaults[i]=4;w.spawns[i]=v[LANE]%11==0?128:0;
    w.models[i]=(v[LANE]+i)%16;w.materials[i]=(v[LANE]+i*2)%11;
    w.eligible[i]=i==0||i==3||i==15||(i==1&&(v[SHAPE]==1||v[SHAPE]==3))||
      (i==2&&(v[SHAPE]==2||v[SHAPE]==5));
  }
  w.materials[0]=v[CATEGORY];
  for(unsigned s=0;s<2;++s){
    auto& state=w.states[s];state.index=v[INDEX]^(s==1?1:0);state.step=9;
    state.preview=v[PREVIEW];state.zoom=7;state.fields.fill(9);state.fields[1]=v[SLOT];
    for(unsigned q=0;q<5;++q){
      auto& r=q==0?state.selection.data:state.queue[q-1].data;
      r.action=q==0||q==(v[INDEX]&3)+1?v[ACTION]:96;r.affinity=v[VARIANT];
      r.primary.actor=v[SOURCE];r.primary.position_kind=v[SOURCE_KIND];r.animation=v[ANIMATION];
      r.focus={static_cast<short>(10+s),-20,30,40};
      const auto add=[&](unsigned actor,unsigned limb,unsigned kind){
        auto& h=r.targets[r.count++];h.actor=actor;h.limb=limb;h.position_kind=kind;
        if(kind)h.SetPosition({static_cast<short>(actor),-3,17,-99});
      };
      switch(v[SHAPE]){
        case 1:add(1,v[LANE]%6,0);r.targets[1].actor=1;break;
        case 2:add(1,0,0);add(2,255,0);r.targets[2].actor=2;break;
        case 3:add(1,0,0);add(1,2,0);break;
        case 4:for(unsigned i=0;i<16;++i)add(7,0,1);break;
        case 5:add(1,0,0);add(7,0,1);add(2,255,0);break;
        default:if(v[LANE]/6%2)r.targets[0].position_kind=4;break;
      }
    }
  }
}
void mutate(p::World& w,const Vector& v,unsigned address){
  auto& queue=w.states[0].queue[v[INDEX]&3].data;
  const auto mutation=v[MUTATE];
  const bool animation=address==0x8009DD00||address==0x8009E2E0||address==0x8009F298||
    address==0x8009EFEC||address==0x8009EC9C||address==0x8009EE9C;
  if((mutation==1&&address==0x800BEC14)||(mutation==2&&animation)||
     (mutation==3&&address==0x8009F8DC)||(mutation==4&&address==0x800CB50C)){
    w.current=1;if(mutation==2)queue.primary.actor=2;
  }
  if(mutation==5&&address==0x800CF920)w.phase=91;
  if(mutation==6&&address==0x8006EF10){w.current=1;w.Current().preview=255;}
  if(mutation==7&&address==0x800C16DC)w.Current().preview=v[PREVIEW]^255;
  if(mutation==8&&address==0x800CB114){w.current=1;w.Current().selection.data.primary.actor=2;}
  if(mutation==9&&address==0x80085B10){
    w.current=1;auto& s=w.Current().selection.data;s.action=184;s.primary.actor=0;
    s.targets[0].actor=1;s.targets[0].position_kind=0;
  }
  if(mutation==10&&address==0x80077F70){w.current=1;w.Current().selection.data.action=232;w.zooms[232]=1;}
  if(mutation==11&&address==0x8009E5C4){w.current=1;w.Current().selection.data.primary.actor=2;}
  if(mutation==12&&address==0x800A1108){queue.action=141;queue.primary.actor=1;}
  if(mutation==13&&animation){queue.action=224;queue.primary.actor=0;}
  if(mutation==14&&address==0x800CF0E8){queue.action=223;queue.primary.actor=0;w.current=1;}
  if(mutation==15&&address==0x800CB654){w.information=true;queue.action=222;}
  if(mutation==16&&address==0x800CB2B8)w.phase=99;
}
void append(std::vector<unsigned>& out,const b::PresentationTarget& target){
  out.insert(out.end(),{target.kind,target.actor,0,target.part,
    static_cast<unsigned short>(target.position.x),static_cast<unsigned short>(target.position.y),
    static_cast<unsigned short>(target.position.z),static_cast<unsigned short>(target.position.tag)});
}
void print(const p::World& w){
  std::vector<unsigned> out{static_cast<unsigned>(w.current),w.phase,w.speed,
    static_cast<unsigned short>(w.selection_kind),w.spell_commit,w.heals};
  for(const auto& s:w.states){
    out.insert(out.end(),{s.index,s.step,s.preview,s.zoom});
    for(auto f:s.fields)out.push_back(f);
    out.insert(out.end(),{s.selection.Id(),s.selection.Primary().actor});
    for(const auto& q:s.queue)out.insert(out.end(),{q.Id(),q.Primary().actor});
  }
  for(auto n:w.attacks)out.push_back(n);
  std::vector<unsigned> request;
  if(std::ranges::find(w.events,0x800CF0E8)!=w.events.end()){
    const auto& r=w.dispatched;request={r.effect,r.count,r.other_count};append(request,r.source);
    for(unsigned i=0;i<r.count;++i)append(request,r.targets[i]);
    for(unsigned i=0;i<r.other_count;++i)request.push_back(r.others[i]);
    request.insert(request.end(),{static_cast<unsigned short>(r.focus.x),
      static_cast<unsigned short>(r.focus.y),static_cast<unsigned short>(r.focus.z)});
  }
  out.push_back(request.size());out.insert(out.end(),request.begin(),request.end());
  out.push_back(w.assets.size()*5);
  for(auto a:w.assets)out.insert(out.end(),{7,a.slot,a.kind,a.model,a.material});
  out.push_back(w.events.size());out.insert(out.end(),w.events.begin(),w.events.end());
  for(auto value:out)std::cout<<value<<' ';std::cout<<'\n';
}
}
int main(){
  std::string line;
  while(std::getline(std::cin,line)){
    Vector v{};std::istringstream in(line);for(auto& x:v)in>>x;if(!in)return 2;
    auto world=std::make_unique<p::World>();fill(*world,v);
    world->callback=[&](unsigned address){mutate(*world,v,address);};
    bool valid=true;
    switch(v[F]){
      case 0:valid=b::StartActionAnimation(*world);break;
      case 1:b::AdvanceActionEffectPhase(*world);break;
      case 2:valid=b::CommitSelectedAction(std::bit_cast<int>(v[MODE]),*world);break;
      case 3:valid=b::StartActionPresentation(*world);break;
    }
    if(!valid)return 3;print(*world);
  }
}
