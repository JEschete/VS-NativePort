#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Actors/AppearanceTransition.hpp"

struct Actors_ActorRecord;
struct Field_GimmickInstance;

// Bindings from actor records and field gimmicks to the typed palette pulse
// and appearance transitions (AppearanceTransition.cpp). Each reads the
// global tick speed, uploads the CLUT rows and, for actors, the actor's
// equipment palettes; actors outside the equipment table have no equipment.
namespace VS::Actors {
void UpdateLiveActorPalettePulse(Actors_ActorRecord& actor);
void UpdateLiveGimmickPalettePulse(Field_GimmickInstance& object);
void UpdateLiveActorAppearanceTransition(Actors_ActorRecord& actor);
void UpdateLiveGimmickAppearanceTransition(Field_GimmickInstance& object);
}  // namespace VS::Actors
