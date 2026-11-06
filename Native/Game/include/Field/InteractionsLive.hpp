#pragma once

#include "Actors/Location.h"

extern "C++" {
namespace VS::Field {
bool CheckAshleyTileTriggersLive(int actor_id, Actors_ActorLocation& location);
}
}
