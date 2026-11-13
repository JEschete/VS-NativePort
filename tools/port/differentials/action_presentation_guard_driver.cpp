// Actual checked entrypoints; external scene services are controlled.
#include <array>
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <string>
#include "Battle/ActionPresentationLive.hpp"
#include "Battle/Ai/ReactionDelay.hpp"
extern "C" {
#include "ActorAssets.h"
#include "Actors/Location.h"
#include "Audio/Seq/Channel.h"
#include "Battle/AbilityState.h"
#include "Battle/Action.h"
#include "Battle/ActionFlow.h"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Effects/Runtime.h"
#include "Input/ControllerLiveState.h"
#include "Menu/Framework/ActionSelection.h"
#include "State/Progress.h"
#include "State/Timers.h"
Battle_Actor* Battle_actors[42]{};
Battle_Actor* Battle_characterState{};
Battle_ActionState* Battle_actionState{};
Menu_Framework_ActionSelection* Battle_actionSelection{};
decltype(Battle_actionDefinitions) Battle_actionDefinitions{};
decltype(Audio_Seq_unusedScoredata) Audio_Seq_unusedScoredata{};
decltype(Input_settings) Input_settings{};
unsigned Battle_actionFlowMode{};
char Battle_spellCommitFlag{};
int State_tickSpeed{};
}
namespace {
Battle_ActionState state{},other{};
Battle_Actor actor{},character{};
Battle_ActorStats stats{},player_stats{};
Menu_Framework_ActionSelection selection{};
struct Guarded {
  unsigned before=0x12345678;
  Effects_ActionEffectRequest request{};
  unsigned after=0x87654321;
} guarded;
unsigned dispatches{},resolved{},poses{},phase_callbacks{},assets{};
bool overflow{},swap_pose{},swap_resolve{};
Content_ActorAssetRequest asset{};
void check_canaries(){
  if(guarded.before!=0x12345678||guarded.after!=0x87654321)std::_Exit(94);
}
void pose(){
  ++poses;
  if(swap_pose){Battle_actionState=&other;state.queue[0].primaryTarget.target.targetActor=2;}
}
}
extern "C" {
void Core_Errorf(const char*,const char* message,...){
  check_canaries();
  if(overflow&&(dispatches!=0||Battle_actionFlowMode==5))std::_Exit(93);
  std::fprintf(stderr,"%s\n",message);std::_Exit(85);
}
void Menu_ReleaseSceneNotification(){}
void Effects_ClearActionEffectMode(){++phase_callbacks;if(Battle_actionFlowMode!=6)std::_Exit(92);Battle_actionFlowMode=91;}
void Battle_ClearSphereTargets(){}
void Battle_ClearPrimarySphereVolume(){}
void Battle_ClearSecondarySphereVolume(){}
void Battle_FreeSphere(){}
void Battle_BeginPostActionRecovery(int){}
void Battle_SetActorMarkersHidden(int){}
void Battle_SetHudHidden(int){}
void Menu_ShowSceneNotification(int,int,int){}
void Battle_PlayMeleeAttackAnimation(int,SVECTOR*,int){pose();}
void Battle_PlaySpellCastAnimation(int,SVECTOR*,int){pose();}
void Battle_PlayBreakArtAnimation(int,SVECTOR*,int){pose();}
void Battle_PlaySpecialActionAnimation(int,SVECTOR*,int){pose();}
void Battle_PlayChainAbilityAnimation(int,SVECTOR*,int,int){pose();}
void Battle_PlayDefenseAbilityAnimation(int,SVECTOR*,int){pose();}
int Effects_BeginActionEffect(Effects_ActionEffectRequest* request,int,int){
  ++dispatches;guarded.request=*request;check_canaries();return 0;
}
}
namespace VS::State { void SetLiveScriptFlag(int,int){} }
namespace VS::Actors {
int LiveAnimationBattleWindowFrame(int actor){return swap_pose?actor+65535:2;}
void RestoreLiveActorIdlePose(int){}
void FreezeLiveActorAnimations(){}
int LiveActorPlacement(int,Actors_ActorLocationQuery* out){out->location.position={1,2,3,4};return 0;}
int FindLiveActorBoneTag(int actor,int limb,int kind){
  if(kind!=2)std::_Exit(90);
  return limb==255?0:actor*8+limb;
}
}
namespace VS::Audio {
void LiveStopSfx(int,int){}
void LivePlayWorldSfx(int,int,SVECTOR*){}
}
namespace VS::Field {
void LiveStorePreferredActionCameraDistance(){}
int SetLiveMinimapVisible(int){return 0;}
void LiveSetRoomAnimationsPaused(int){}
}
namespace VS::Menu::Framework { void ReleaseLiveMenuRows(){} }
namespace VS::Items { int ConsumeLiveMiscItem(int){return 0;} }
namespace VS::Render {
void SetLiveClutAnimationEnabled(int){}
void TransformLiveSceneColors(int,int,short,short,short){}
}
namespace VS::Content {
int QueueLiveActorAssetRequest(const Content_ActorAssetRequest* request){asset=*request;++assets;return 0;}
}
namespace VS::Battle {
int LiveActionTargetDistance(){return 449;}
void LiveReturnToBattleActionMenu(){}
void LiveCollectEnvironmentActionTargets(){}
void LiveSetAllActionTargets(){}
void LiveSetSingleActionTarget(){}
void ResolveLiveActionTargets(int action,Battle_ActionRequest* result,Battle_ActionRequest* source,int execute){
  if(action!=Battle_actionState->selection.actionId||result!=Battle_actionState->queue||
     source!=&Battle_actionState->selection||execute!=1)std::_Exit(95);
  ++resolved;
  if(swap_resolve){Battle_actionState=&other;other.selection.actionId=184;}
}
}
namespace VS::Battle::Ai {
struct Delays final:ActorReactionDelays {bool SetMinimumActionDelay(int,unsigned char)override{return true;}};
ActorReactionDelays& BattleActorReactionDelays(){static Delays delays;return delays;}
void ResetActorAiReactionDelay(ActorReactionDelays&,int){}
void DisengageLiveActorById(int){}
}
int main(int argc,char** argv){
  if(argc!=2)return 2;
  const std::string mode=argv[1];
  namespace b=VS::Battle;
  Battle_actionState=&state;Battle_actionSelection=&selection;
  Battle_characterState=&character;character.stats=&player_stats;
  actor.stats=&stats;actor.allocationKind=1;actor.commandArg0=0x10002;
  Battle_actors[0]=&character;Battle_actors[1]=&actor;Battle_actors[2]=&actor;
  character.allocationKind=1;character.equippedWeaponCategory=10;
  stats.defaultTargetLimb=4;stats.limbActions[2][1].effectModelId=9;
  Input_settings.mappedChainAbilities[0]=22;
  auto& q=state.queue[0];q.actionId=60;Battle_actionDefinitions[60].returnsToMenuOnCancel=1;
  if(mode=="player-status"){q.primaryTarget.target.targetActor=1;stats.statuses=0x8000;}
  if(mode=="bystanders"||mode=="duplicate-default"||mode=="mixed-world"){
    q.targetCount=1;q.targets[0].target.targetActor=1;
    if(mode!="bystanders"){
      q.targetCount=3;q.targets[1].target.targetActor=1;
      q.targets[2].target.targetActor=2;q.targets[2].target.targetLimb=-1;
    }
    if(mode=="mixed-world"){
      q.targetCount=4;q.targets[3]=q.targets[2];
      q.targets[2].targetsPosition=1;q.targets[2].target.targetActor=2;
    }
    b::BuildLiveActionPresentationRequest(&q,&guarded.request);
    const auto& result=guarded.request;
    if(mode=="bystanders")return result.otherCount==1&&result.others[0]==2?0:21;
    if(mode=="duplicate-default")return result.count==2&&result.targets[0].value.actor.part==0&&
      result.targets[1].value.actor.actor==2&&result.targets[1].value.actor.part==20&&result.otherCount==0?0:22;
    return result.count==3&&result.targets[1].type==5&&result.targets[2].type==4&&
      result.targets[2].value.actor.actor==2?0:23;
  }
  if(mode=="phase"){Battle_actionState=nullptr;b::AdvanceLiveActionEffectPhase();return phase_callbacks==1&&Battle_actionFlowMode==91?0:3;}
  if(mode=="state")Battle_actionState=nullptr;
  if(mode=="character")Battle_characterState=nullptr;
  if(mode=="player-stats")character.stats=nullptr;
  if(mode=="definition")q.actionId=256;
  if(mode=="asset-limb"||mode=="asset-choice"||mode=="asset-valid"){
    q.actionId=141;q.primaryTarget.target.targetActor=1;Battle_actionDefinitions[141].returnsToMenuOnCancel=1;
    actor.equippedWeaponCategory=255;
    if(mode=="asset-limb")actor.commandArg0=0x10006;
    if(mode=="asset-choice")actor.commandArg0=0x40002;
  }
  if(mode.starts_with("chain-")||mode=="defense-slot-unused"||mode=="pose-swap"){
    q.actionId=22;
    if(mode=="pose-swap"){swap_pose=true;other.selection.focusPosition={1,2,3,4};}
    else{
      state.queueIndex=1;state.queue[1]=q;
      state.chainAbilitySlot=mode=="chain-zero"?0:mode=="chain-four"?4:mode=="chain-negative"?-1:1;
      if(mode=="defense-slot-unused")state.queue[1].actionId=40;
    }
    b::StartLiveActionAnimation();
    return poses==1&&Battle_actionFlowMode==7&&(!swap_pose||other.abilityWindow==2)?0:4;
  }
  if(mode=="category"||mode=="category-unused"||mode=="commit-swap"){
    state.selection.actionId=1;character.equippedWeaponCategory=11;
    if(mode=="category-unused")state.selection.targets[0].targetsPosition=1;
    if(mode=="commit-swap"){swap_resolve=true;character.equippedWeaponCategory=10;}
    b::CommitLiveSelectedAction(0);
    return resolved==1&&Battle_actionFlowMode==4&&(!swap_resolve||Audio_Seq_unusedScoredata.weaponAttacks[0]==1)?0:5;
  }
  if(mode=="null-request")b::BuildLiveActionPresentationRequest(&q,nullptr);
  if(mode=="null-action")b::BuildLiveActionPresentationRequest(nullptr,&guarded.request);
  q.targetCount=mode=="unique17"||mode=="duplicate17"||mode=="boundary-duplicate"?17:mode=="duplicate255"?255:16;
  for(unsigned i=0;i<q.targetCount;++i){
    q.targets[i].targetsPosition=1;q.targets[i].target.targetActor=static_cast<unsigned char>(i+2);
    if(mode.starts_with("duplicate")){
      q.targets[i].targetsPosition=0;q.targets[i].target.targetActor=1;q.targets[i].target.targetLimb=3;
    }
  }
  if(mode=="boundary-duplicate"){
    for(unsigned i=15;i<17;++i){
      q.targets[i].targetsPosition=0;q.targets[i].target.targetActor=1;
      q.targets[i].target.targetLimb=3;
    }
  }
  if(mode=="default-stats"||mode=="default-slot"){
    q.targetCount=1;q.targets[0].targetsPosition=0;q.targets[0].target.targetLimb=-1;
    if(mode=="default-stats")actor.stats=nullptr;
    else q.targets[0].target.targetActor=42;
  }
  if(mode=="lazy-world"){actor.stats=nullptr;Battle_characterState=nullptr;b::BuildLiveActionPresentationRequest(&q,&guarded.request);return guarded.request.count==16?0:6;}
  overflow=mode=="unique17";
  b::StartLiveActionPresentation();
  check_canaries();
  if(dispatches!=1||Battle_actionFlowMode!=5)return 7;
  if(mode=="boundary-duplicate"&&(guarded.request.count!=16||guarded.request.targets[15].value.actor.part!=0))return 11;
  if(mode=="player-status"&&state.abilityInputAttempted!=0)return 10;
  if(mode.starts_with("duplicate")&&(guarded.request.count!=1||guarded.request.targets[0].value.actor.part!=0))return 8;
  if(mode=="asset-valid"){
    std::printf("limbActions[2][1] nibble9 -> native field %d -> request model %d; expected9\n",
      stats.limbActions[2][1].effectModelId,asset.modelId);
    if(assets!=1||asset.modelId!=9||asset.subKind!=5||asset.targetSlot!=1||
       static_cast<unsigned char>(asset.material)!=255)return 9;
  }
  std::printf("%s pass\n",mode.c_str());
}
