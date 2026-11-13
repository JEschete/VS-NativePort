// Native policies against explicit, controlled scene services.
#include "Battle/ActionCameraSelection.hpp"

#include <array>
#include <bit>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {
namespace b = VS::Battle;
using Word = std::uint32_t;
constexpr Word kTag=0x800A152C,kBone=0x800A1AF8,kStep=0x8006DEFC,kTrack=0x8006DB98;
constexpr Word kMenu=0x800C58F8,kArea=0x800C4794,kCommit=0x80070F28,kFilter=0x8007053C;
constexpr Word kInitialize=0x80072BA8,kClear=0x800C16DC,kPrimary=0x800C06E0;
constexpr Word kSecondary=0x800C0738,kFree=0x800C05B4,kReturn=0x800704D8;
constexpr Word kApply=0x80041534,kPresent=0x8007138C,kNone=0xFFFFFFFF;
int Signed(Word value){return std::bit_cast<std::int32_t>(value);}
Word Bits(int value){return static_cast<Word>(value);}
Word Short(short value){return static_cast<std::uint16_t>(value);}
b::ActionPoint ReadPoint(std::istream& in){
  std::array<Word,4> values{};for(auto& x:values)in>>x;
  return {static_cast<short>(values[0]),static_cast<short>(values[1]),
          static_cast<short>(values[2]),static_cast<short>(values[3])};
}
void EmitPoint(std::vector<Word>& out,b::ActionPoint value){
  out.insert(out.end(),{Short(value.x),Short(value.y),Short(value.z),Short(value.tag)});
}
struct Queue final:b::ActionCameraRequest {
  Word primary{},count{},kind{};
  b::ActionPoint target{},focus{};
  std::uint8_t PrimaryActor()const override{
    return static_cast<std::uint8_t>(primary);
  }
  b::ActionHit FirstTarget()const override{
    b::ActionHit hit{};hit.SetPosition(target);hit.position_kind=Signed(kind);return hit;
  }
  std::uint16_t Count()const override{return static_cast<std::uint16_t>(count>>16);}
  b::ActionPoint Focus()const override{return focus;}
};
struct World;
struct State final:b::ActionSelectionState {
  World* world{};Word owner{},index{},selecting{},action{},primary{},preview{},zoom{};
  b::ActionPoint area{};std::array<Queue,4> queues{};
  bool Selecting()const override{return selecting!=0;}
  std::uint8_t SourceActor()const override{return static_cast<std::uint8_t>(primary);}
  std::uint8_t Frames()const override{return static_cast<std::uint8_t>(preview>>16);}
  void SetFrames(std::uint8_t value)override{preview=(preview&0xFF00FFFF)|(static_cast<Word>(value)<<16);}
  std::uint8_t Preview()const override{return static_cast<std::uint8_t>(preview>>8);}
  std::uint8_t Selected()const override{return static_cast<std::uint8_t>(preview);}
  void SetSelected(std::uint8_t value)override{preview=(preview&0xFFFFFF00)|value;}
  std::uint16_t ActionId()const override{return static_cast<std::uint16_t>(action);}
  int UpdateMenu()override;
  int UpdateArea()override;
};
struct World final:b::ActionCameraWorld,b::ActionSelectionWorld {
  Word function{},lane{},mutation{},actor{},limb{},execute{},bone{},menu_return{},tracking_return{};
  Word current{},mode{},projection{},distance{},yaw{};
  b::ActionPoint source{},target{},output{};
  VS::Core::Math::Vec3 displacement{};
  Word alt_mode{},alt_projection{},alt_distance{},alt_yaw{},menu_preview{},flag_mode{};
  std::array<State,2> states{};
  Word initial_index{},positions{},refused{};
  std::vector<Word> calls;
  template<typename...Args>void Event(Word address,Args...args){
    calls.push_back(address);(calls.push_back(static_cast<Word>(args)),...);
    for(std::size_t i=sizeof...(args);i<7;++i)calls.push_back(0);
  }
  void SetPreview(State& state,Word value){state.preview=(state.preview&0xFFFF00FF)|((value&255)<<8);}
  void Mutate(Word address){
    if(address==kTag&&function==0&&mutation==6)current=1;
    if(address==kBone&&positions==1&&(function==1||function==2||function==3||function==7)){
      auto& q=states[0].queues[initial_index];
      if(mutation==1){current=1;states[0].index=(initial_index+1)&3;q.primary=(q.primary&0xFFFFFF00)|255;}
      else if(mutation==2){q.count&=65535;q.primary=(q.primary&0xFFFFFF00)|255;q.focus=target;}
      else if(mutation>=3&&mutation<=5){
        q.count=(q.count&65535)|((mutation==3?65535u:1u)<<16);
        q.kind=mutation==3?kNone:0;
        if(mutation==3)q.target=target;
        else q.target.x=static_cast<short>(mutation==4?65535:0x8080);
      }
    }
    if(function==7){
      if(address==kStep&&(mutation==6||mutation==7||mutation==8||mutation==11)){
        if(mutation==6){current=1;states[1].zoom=(states[1].zoom&65535)|(3u<<16);}
        if(mutation==6||mutation==7){projection=alt_projection;distance=alt_distance;}
        if(mutation==6||mutation==8||mutation==11){mode=alt_mode;yaw=alt_yaw;}
      }
      if(address==kApply&&mutation==9){projection=alt_projection;distance=alt_distance;mode=alt_mode;}
      if(address==kTrack&&mutation==10){mode=alt_mode;yaw=alt_yaw;current=1;}
    }
    if(address==kMenu||address==kArea){
      if(mutation==1||mutation==2||mutation==5||mutation==6){
        current=mutation==6&&menu_return==0?kNone:1;
        if(mutation==1)SetPreview(states[1],menu_preview^127);
        else if(mutation==2){SetPreview(states[1],255);states[1].action=(states[1].action&0xFFFF0000)|(lane*17&255);}
        else if(mutation==5){SetPreview(states[1],0);states[1].selecting=0;}
      }
      if(mutation==3)states[0].SetSelected(199);
      if(mutation==4){SetPreview(states[0],255);states[0].action=(states[0].action&0xFFFF0000)|(lane*7&255);}
    }
    if(mutation==7&&(address==kCommit||address==kFilter||address==kInitialize||
      address==kClear||address==kPrimary||address==kSecondary||address==kFree||address==kReturn))current=1;
  }
  b::ActionCameraRequest* Queued()override{
    if(current>=2||states[current].index>=4){++refused;return nullptr;}
    return &states[current].queues[states[current].index];
  }
  int BoneTag(int a,int l)override{
    Event(kTag,Bits(a),Bits(l),2,bone);Mutate(kTag);return Signed(bone);
  }
  bool BonePosition(int a,int selected,b::ActionPoint& out)override{
    const auto point=positions++%2==0?source:target;
    Event(kBone,Bits(a),Bits(selected),0,Short(point.x),Short(point.y),Short(point.z),0);
    out.x=point.x;out.y=point.y;out.z=point.z;Mutate(kBone);return true;
  }
  void StepCamera(b::ActionPoint point)override{
    Event(kStep,Short(point.x),Short(point.y),Short(point.z),1,40,tracking_return);Mutate(kStep);
  }
  VS::Core::Math::Vec3 TrackingDisplacement(b::ActionPoint point)override{
    Event(kTrack,Short(point.x),Short(point.y),Short(point.z),1,40,tracking_return);
    Mutate(kTrack);return displacement;
  }
  std::int32_t CameraMode()override{return Signed(mode);}
  std::optional<std::int16_t> Zoom()override{
    if(current>=2){++refused;return std::nullopt;}
    return static_cast<short>(states[current].zoom>>16);
  }
  std::int32_t YawDelta()override{return Signed(yaw);}
  std::int32_t Distance()override{return Signed(distance);}
  void SetDistance(std::int32_t value)override{distance=Bits(value);}
  std::int32_t Projection()override{return Signed(projection);}
  void SetProjection(std::int32_t value)override{projection=Bits(value);}
  void ApplyProjection(std::int32_t value)override{
    Event(kApply,Bits(value),mode,distance);Mutate(kApply);
  }
  void StartPresentation()override{Event(kPresent);Mutate(kPresent);}
  b::ActionSelectionState* Current()override{
    if(current>=2){++refused;return nullptr;}return &states[current];
  }
  void Commit(int value)override{
    Event(kCommit,value,current,current<2?states[current].Selected():kNone,
          current<2?states[current].ActionId():kNone);Mutate(kCommit);
  }
  void FilterArea()override{Event(kFilter);Mutate(kFilter);}
  std::optional<bool> NeedsTargeting(std::uint16_t id)override{
    if(id>=256){++refused;return std::nullopt;}
    const Word category=(id+lane)%16;return (category*5+category/4+flag_mode)%4!=0;
  }
  void InitializeTargeting(std::uint16_t id)override{Event(kInitialize,id);Mutate(kInitialize);}
  void ClearSphereTargets()override{Event(kClear);Mutate(kClear);}
  void ClearPrimaryVolume()override{Event(kPrimary);Mutate(kPrimary);}
  void ClearSecondaryVolume()override{Event(kSecondary);Mutate(kSecondary);}
  void FreeSphere()override{Event(kFree);Mutate(kFree);}
  void ReturnToMenu()override{Event(kReturn);Mutate(kReturn);}
  void Read(std::istream& in){
    in>>function>>lane>>mutation>>actor>>limb>>execute>>bone>>menu_return>>tracking_return;
    in>>current>>mode>>projection>>distance>>yaw;
    source=ReadPoint(in);target=ReadPoint(in);output=target;
    Word x{},y{},z{};in>>x>>y>>z;displacement={Signed(x),Signed(y),Signed(z)};
    in>>alt_mode>>alt_projection>>alt_distance>>alt_yaw>>menu_preview>>flag_mode;
    for(Word owner=0;owner<2;++owner){
      auto& s=states[owner];s.world=this;s.owner=owner;
      in>>s.index>>s.selecting>>s.action>>s.primary>>s.preview>>s.zoom;
      s.area=ReadPoint(in);
      Queue q;in>>q.primary>>q.count>>q.kind;q.target=ReadPoint(in);q.focus=ReadPoint(in);
      for(auto& value:s.queues)value=q;
    }
    initial_index=states[0].index;
  }
  void Run(){
    b::ActionPoint* out=&output;
    if((function==0||function==2)&&lane%3==0)out=&states[0].queues[initial_index].focus;
    else if((function==0||function==2)&&lane%3==1)out=&states[0].area;
    bool ok=true;Word result{};
    switch(function){
      case 0:ok=b::ActionTargetLimbPosition(Signed(actor),Signed(limb),*out,*this);break;
      case 1:{const auto value=b::ActionTargetDistance(*this);ok=value.has_value();if(ok)result=Bits(value.value());break;}
      case 2:ok=b::ActionCameraMidpoint(*out,*this);break;
      case 3:ok=b::StepCameraTowardActionMidpoint(Signed(execute),*this);break;
      case 4:ok=b::UpdateInitialTargetSelection(*this);break;
      case 5:ok=b::UpdateActionAreaSelection(*this);break;
      case 6:ok=b::UpdateFinalTargetSelection(*this);break;
      case 7:ok=b::UpdateActionCameraZoom(*this);break;
      default:ok=false;
    }
    std::vector<Word> values{ok?refused:1,result};
    EmitPoint(values,*out);
    values.insert(values.end(),{current,mode,projection,distance,yaw});
    for(const auto& s:states){
      values.insert(values.end(),{s.index,s.selecting,s.action,s.primary,s.preview,s.zoom});
      EmitPoint(values,s.area);
      for(const auto& q:s.queues){
        values.insert(values.end(),{q.primary,q.count,q.kind});EmitPoint(values,q.target);EmitPoint(values,q.focus);
      }
    }
    values.push_back(static_cast<Word>(calls.size()));values.insert(values.end(),calls.begin(),calls.end());
    for(const auto value:values)std::cout<<value<<' ';std::cout<<'\n';
  }
};
int State::UpdateMenu(){
  world->Event(kMenu,owner,Preview());world->SetPreview(*this,world->menu_preview);
  world->Mutate(kMenu);return Signed(world->menu_return);
}
int State::UpdateArea(){
  world->Event(kArea,owner,Short(area.x),Short(area.y),Short(area.z),Short(area.tag));
  area=world->target;area.tag=1;world->Mutate(kArea);return Signed(world->menu_return);
}
}  // namespace
int main(){
  std::string line;
  while(std::getline(std::cin,line)){
    if(line.empty())continue;std::istringstream input(line);World world;world.Read(input);
    if(!input)return 2;world.Run();
  }
}
