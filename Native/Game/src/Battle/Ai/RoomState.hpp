#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Actors/ActorAllocation.hpp"
#include "Actors/ActorRecord.hpp"
#include "Actors/Animation.hpp"
#include "Actors/ModelInstance.hpp"
#include "Actors/ShpModel.hpp"
#include "Battle/Actor.hpp"
#include "Battle/Ai/AiState.hpp"
#include "Battle/Ai/Navigation.hpp"
#include "Battle/Ai/Perception.hpp"
#include "Field/Gimmicks.hpp"
#include "Items/ItemRecords.hpp"
#include "Legacy/Psx.h"
#include "Render/Palette.hpp"

namespace VS::Battle::Ai {
void InvalidateSessionBindings();
}  // namespace VS::Battle::Ai
