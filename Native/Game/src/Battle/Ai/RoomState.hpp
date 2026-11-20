#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Actors/ActorAllocation.hpp"
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/ModelInstance.h"
#include "Actors/Skeleton.h"
#include "Battle/Actor.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/AiViews.h"
#include "Battle/Ai/Navigation.hpp"
#include "Battle/Ai/Perception.hpp"
#include "Field/Gimmicks.h"
#include "Items/ItemRecords.hpp"
#include "Legacy/Psx.h"
#include "Render/Palette.hpp"

namespace VS::Battle::Ai {
void InvalidateSessionBindings();
}  // namespace VS::Battle::Ai
