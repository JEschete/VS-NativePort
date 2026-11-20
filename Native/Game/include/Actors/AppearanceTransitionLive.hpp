#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Actors/AppearanceTransition.hpp"

namespace VS::Actors {
struct ActorRecord;
}  // namespace VS::Actors
namespace VS::Field {
struct GimmickInstance;
}  // namespace VS::Field

// Bindings from actor records and field gimmicks to the typed palette pulse
// and appearance transitions (AppearanceTransition.cpp). Each reads the
// global tick speed, uploads the CLUT rows and, for actors, the actor's
// equipment palettes; actors outside the equipment table have no equipment.
namespace VS::Actors {
void UpdateLiveActorPalettePulse(VS::Actors::ActorRecord& actor);
void UpdateLiveGimmickPalettePulse(VS::Field::GimmickInstance& object);
void UpdateLiveActorAppearanceTransition(VS::Actors::ActorRecord& actor);
void UpdateLiveGimmickAppearanceTransition(VS::Field::GimmickInstance& object);
}  // namespace VS::Actors
