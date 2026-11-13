// Real Live bindings; only external actor/menu/camera services are controlled.
#include <array>
#include <bit>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>
#include <vector>

#include "Actors/SkeletonLive.hpp"
#include "Battle/ActionCameraSelectionLive.hpp"
#include "Battle/ActionPresentationLive.hpp"
#include "Battle/TargetingLive.hpp"
#include "Field/CameraLive.hpp"
#include "Render/CameraViewLive.hpp"
extern "C" {
#include "Battle/Action.h"
#include "Battle/ActionFlow.h"
#include "Battle/ActionTable.h"
#include "Battle/Sphere.h"
#include "Battle/TargetingState.h"
#include "Field/CameraLiveState.h"
#include "Render/Camera.h"
Battle_ActionState* Battle_actionState{};
decltype(Battle_actionDefinitions) Battle_actionDefinitions{};
decltype(Battle_categoryNeedsTargetSelection) Battle_categoryNeedsTargetSelection{};
decltype(Field_sphericalCamera) Field_sphericalCamera{};
int Render_projectionDistance{};
}
namespace {
namespace b=VS::Battle;
using Point=std::array<std::int16_t,4>;
std::string mode;
Battle_ActionState state{},other{};
std::vector<int> events;
int positions{},steps{},tracks{},starts{},logs{};
int actor_arg{},limb_arg{},bone_arg{},tag_calls{},menu_result=1;
SVECTOR last_step{},last_track{};
bool missing_position{};
void Check(bool valid,int code){if(!valid)std::_Exit(code);}
void SetWorldTarget(Battle_ActionRequest& q,Point point){
  q.targetCount=65535;q.targets[0].targetsPosition=-1;
  q.targets[0].target=std::bit_cast<decltype(q.targets[0].target)>(point);
}
}
extern "C" {
void Core_Errorf(const char*,const char* message,...){
  std::fprintf(stderr,"%s\n",message);std::_Exit(85);
}
void Core_Debugf(const char*,const char*,...){++logs;}
void Battle_ClearSphereTargets(){events.push_back(1);}
void Battle_ClearPrimarySphereVolume(){events.push_back(2);}
void Battle_ClearSecondarySphereVolume(){events.push_back(3);}
void Battle_FreeSphere(){events.push_back(4);}
}
namespace VS::Actors {
int FindLiveActorBoneTag(int actor,int limb,int kind){
  ++tag_calls;actor_arg=actor;limb_arg=limb;Check(kind==2,90);return -32768;
}
int LiveActorBonePosition(int actor,int bone,SVECTOR* out,int endpoint){
  actor_arg=actor;bone_arg=bone;Check(endpoint==0&&out!=nullptr,91);
  if(missing_position)return -1;
  const bool first=positions++%2==0;
  out->vx=first?0:3;out->vy=first?0:4;out->vz=0;
  if(positions==1&&(mode=="cached-distance"||mode=="cached-midpoint")){
    Battle_actionState=&other;
    state.queueIndex=1;state.queue[3].primaryTarget.target.targetActor=255;
    SetWorldTarget(state.queue[3],{3,4,0,77});
    other.queue[0].focusPosition={99,98,97,96};
  }
  if(positions==1&&mode=="second-root"){
    state.queue[3].primaryTarget.target.targetActor=255;
  }
  return 0;
}
}
namespace VS::Battle {
int LiveUpdateTargetSelectionMenu(std::uint8_t* output){
  events.push_back(10);Check(output==&state.cameraPreview,92);
  *output=17;Battle_actionState=&other;
  other.cameraPreview=(mode=="menu-noncancel"||mode=="noncancel-unused")?7:255;
  other.selection.actionId=(mode=="cancel-invalid"||mode=="noncancel-unused")?65535:255;
  if(mode=="menu-false"){other.selectedPreview=199;return 0;}
  return menu_result;
}
int LiveUpdateSphereTargetPosition(SVECTOR* output){
  events.push_back(11);Check(output==&state.areaCenter,93);
  output->vx=-7;output->pad=1;Battle_actionState=nullptr;
  return mode=="area-false"?0:-1;
}
void CommitLiveSelectedAction(int selection_mode){
  events.push_back(20+selection_mode);
}
void LiveFilterAreaActionTargets(){events.push_back(30);}
void LiveInitializeActionTargeting(int action){events.push_back(40);Check(action==255,94);}
void LiveReturnToBattleActionMenu(){events.push_back(5);}
void StartLiveActionPresentation(){++starts;events.push_back(50);}
}
namespace VS::Field {
int LiveStepCameraLookAtTowardTarget(const SVECTOR* point,int clamp,int speed){
  ++steps;last_step=*point;Check(clamp==1&&speed==40,95);
  if(mode=="camera-mode-change")Field_sphericalCamera.delta.mode=2;
  if(mode=="camera-cached-zoom"){
    Battle_actionState=&other;other.cameraZoomState=2;Render_projectionDistance=704;
    Field_sphericalCamera.values.distance=1600;
  }
  if(mode=="camera-zoom-out")Field_sphericalCamera.values.distance=2200;
  return 0;
}
int LiveComputeCameraTrackingStep(const SVECTOR* point,VECTOR* step,VECTOR* displacement,int clamp,int speed){
  ++tracks;last_track=*point;Check(clamp==1&&speed==40&&step!=displacement,96);
  step->vx=91;step->vy=92;step->vz=93;
  displacement->vx=0;displacement->vy=0;displacement->vz=mode=="camera-not-ready"?1:0;
  if(mode=="camera-current-yaw")Field_sphericalCamera.delta.yaw=-1;
  return 0;
}
}
namespace VS::Render {
void SetLiveCameraProjectionDistance(int projection){
  events.push_back(60);Check(projection==Render_projectionDistance,97);
  if(mode=="camera-cached-zoom")Field_sphericalCamera.values.distance=1600;
  if(mode=="camera-current-projection"){
    Render_projectionDistance=767;Field_sphericalCamera.values.distance=1600;
  }
}
}
int main(int argc,char** argv){
  if(argc!=2)return 2;
  mode=argv[1];Battle_actionState=&state;state.queueIndex=3;
  state.queue[3].primaryTarget.target.targetActor=7;
  state.queue[3].focusPosition={0,0,0,77};
  Field_sphericalCamera.delta.mode=1;
  Field_sphericalCamera.values.distance=1536;Render_projectionDistance=704;
  other.queueIndex=0;
  Battle_actionDefinitions[255].category=15;
  Battle_categoryNeedsTargetSelection[15]=static_cast<char>(128);
  if(mode=="inactive"){
    Battle_actionState=nullptr;
    for(int value:{0,2,4,-1}){
      Field_sphericalCamera.delta.mode=value;b::LiveUpdateActionCameraZoom();
    }
    return logs==4&&steps==0&&tracks==0&&starts==0?0:10;
  }
  if(mode=="target-full"||mode=="target-sentinel"||mode=="target-null"||mode=="target-missing"){
    SVECTOR output{7,8,9,99};missing_position=mode=="target-missing";
    const int limb=mode=="target-sentinel"?-1:0x10002;
    b::LiveActionTargetLimbPosition(0x10001,limb,mode=="target-null"?nullptr:&output);
    Check(actor_arg==0x10001&&output.pad==99,11);
    if(mode=="target-sentinel")return tag_calls==0&&bone_arg==0?0:12;
    return tag_calls==1&&limb_arg==0x10002&&bone_arg==-32768?0:13;
  }
  if(mode=="distance-state"||mode=="midpoint-state"||mode=="step-state"||
     mode=="initial-state"||mode=="area-state"||mode=="final-state"||mode=="camera-state")
    Battle_actionState=nullptr;
  if(mode=="queue-four")state.queueIndex=4;
  if(mode=="queue-max")state.queueIndex=0xFFFFFFFF;
  if(mode=="queue-step")state.queueIndex=4;
  if(mode=="position-missing")missing_position=true;
  if(mode=="negative-horizontal"){
    SetWorldTarget(state.queue[3],{-32768,0,-32768,71});
  }
  if(mode=="negative-vertical"){
    // (-32768)^2 + approximately sqrt(32768^2)^2 wraps negative.
    SetWorldTarget(state.queue[3],{0,-32768,-32768,71});
  }
  if(mode=="distance-state"||mode=="queue-four"||mode=="queue-max"||
     mode=="position-missing"||mode=="negative-horizontal"||mode=="negative-vertical"){
    b::LiveActionTargetDistance();return 14;
  }
  if(mode=="midpoint-state"||mode=="midpoint-null"){
    SVECTOR output{};b::LiveActionCameraMidpoint(mode=="midpoint-null"?nullptr:&output);return 15;
  }
  if(mode=="step-state"||mode=="queue-step"){
    b::LiveStepCameraTowardActionMidpoint(0);return 16;
  }
  if(mode=="initial-state"){b::LiveUpdateInitialTargetSelection();return 17;}
  if(mode=="area-state"){b::LiveUpdateActionAreaSelection();return 18;}
  if(mode=="final-state"){b::LiveUpdateFinalTargetSelection();return 19;}
  if(mode=="camera-state"){b::LiveUpdateActionCameraZoom();return 20;}
  if(mode=="cached-distance"||mode=="second-root"){
    const int result=b::LiveActionTargetDistance();
    Check(result==5&&positions==2-(mode=="cached-distance"),21);
    return mode!="second-root"||actor_arg==255?0:22;
  }
  if(mode=="cached-midpoint"){
    auto& output=state.queue[3].focusPosition;b::LiveActionCameraMidpoint(&output);
    return std::bit_cast<Point>(output)==Point{1,2,0,77}&&positions==1?0:23;
  }
  if(mode=="enemy-initial"||mode=="enemy-area"||mode=="enemy-final"){
    state.isPlayerAction=0x10000;state.selection.primaryTarget.target.targetActor=255;
    state.selectionFrames=30;state.selectedPreview=99;state.selection.actionId=65535;
    if(mode=="enemy-initial")b::LiveUpdateInitialTargetSelection();
    else if(mode=="enemy-area")b::LiveUpdateActionAreaSelection();
    else b::LiveUpdateFinalTargetSelection();
    const int expected=mode=="enemy-initial"?21:mode=="enemy-area"?30:22;
    return events==std::vector<int>{expected}&&
      static_cast<unsigned char>(state.selectionFrames)==31&&
      state.selectedPreview==(mode=="enemy-final"?0:99)?0:24;
  }
  if(mode.starts_with("menu-")||mode=="cancel-invalid"||mode=="noncancel-unused"){
    state.isPlayerAction=1;state.selection.primaryTarget.target.targetActor=0;
    menu_result=-1;
    if(mode=="menu-cleanup")Battle_categoryNeedsTargetSelection[15]=0;
    if(mode=="cancel-invalid"||mode=="noncancel-unused"){
      // The callback deliberately selects the current owner's action.
      other.selection.actionId=65535;
      Battle_actionDefinitions[255].category=15;
    }
    if(mode=="menu-initial"||mode=="menu-false"){
      b::LiveUpdateInitialTargetSelection();
      return mode=="menu-false"?
        (events==std::vector<int>{10}&&other.selectedPreview==199?0:25):
        (events==std::vector<int>{10,21}&&other.selectedPreview==255&&state.cameraPreview==17?0:26);
    }
    b::LiveUpdateFinalTargetSelection();
    if(mode=="menu-cleanup")return events==std::vector<int>{10,1,2,3,4,5}?0:27;
    if(mode=="menu-noncancel"||mode=="noncancel-unused")return events==std::vector<int>{10,22}&&other.selectedPreview==7?0:28;
    return events==std::vector<int>{10,40}&&other.selectedPreview==255?0:29;
  }
  if(mode=="area-false"||mode=="area-nonzero"){
    state.isPlayerAction=1;b::LiveUpdateActionAreaSelection();
    return state.areaCenter.vx==-7&&state.areaCenter.pad==1&&
      events==(mode=="area-false"?std::vector<int>{11}:std::vector<int>{11,30})?0:30;
  }
  if(mode.starts_with("camera-")){
    if(mode=="camera-other"){
      state.cameraZoomState=-1;state.queueIndex=0xFFFFFFFF;
      b::LiveUpdateActionCameraZoom();return starts==1&&positions==0?0:31;
    }
    if(mode=="camera-cached-zoom"||mode=="camera-wrap"||mode=="camera-current-projection"){
      state.cameraZoomState=1;
      if(mode=="camera-wrap")Render_projectionDistance=std::numeric_limits<int>::max();
    }
    if(mode=="camera-zoom-out")state.cameraZoomState=2;
    b::LiveUpdateActionCameraZoom();
    if(mode=="camera-not-ready"||mode=="camera-current-yaw"||mode=="camera-mode-change")
      return steps==1&&tracks==1&&starts==0&&positions==2?0:32;
    if(mode=="camera-wrap")return Render_projectionDistance==std::numeric_limits<int>::min()+63&&tracks==0?0:33;
    if(mode=="camera-current-projection")
      return Render_projectionDistance==767&&Field_sphericalCamera.values.distance==1536&&tracks==0?0:34;
    if(mode=="camera-cached-zoom")
      return events==std::vector<int>{60,50}&&starts==1&&tracks==1&&
        Field_sphericalCamera.values.distance==1536&&Render_projectionDistance==768?0:35;
    if(mode=="camera-zoom-out")return starts==1&&Field_sphericalCamera.values.distance==2304?0:36;
    return starts==1&&tracks==1&&steps==1?0:37;
  }
  return 3;
}
