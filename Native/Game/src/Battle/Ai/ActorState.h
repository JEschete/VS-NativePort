// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"


#include "Actors/ActorModelSlot.hpp"
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/ModelInstance.h"
#include "Actors/Movement.h"
#include "Actors/Shadow.h"
#include "Actors/TextureAnimation.h"
#include "Battle/Actor.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/AiViews.h"
#include "Battle/Ai/Perception.h"
#include "Render/Palette.hpp"

extern unsigned char Battle_Ai_kSettingsBits30Values[1284];
extern unsigned int Battle_Ai_kPerceptionRangesSquared[320];
extern Battle_Ai_WanderTiming Battle_Ai_kWanderTimingPresets[208];
extern int (*Battle_Ai_decisionCallbacks[9])(Battle_Ai_ActorAiState*);
extern unsigned short Battle_Ai_viewAngles[45][2];
extern unsigned short Battle_Ai_settingsBits8Values[41][2];
extern unsigned char Battle_Ai_facingDelayScales[148];
extern Battle_Ai_ActorAiState* Battle_Ai_actorAiStates[16];
extern Battle_Ai_RoomAiState* Battle_Ai_roomAiState;
extern void* Battle_Ai_roomHasWaypoints;
extern int Battle_Ai_attributeActorMask;
extern const unsigned char Battle_Ai_kFlags34Bit14Values[4];
