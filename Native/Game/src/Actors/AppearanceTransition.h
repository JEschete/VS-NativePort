#pragma once
// Hand-maintained (tools/port/owned.csv).
#ifndef VS_OWNED_ACTORS_APPEARANCETRANSITION_H
#define VS_OWNED_ACTORS_APPEARANCETRANSITION_H
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/ModelInstance.h"
#include "Actors/Movement.h"
#include "Actors/Palette.h"
#include "Actors/Shadow.h"
#include "Actors/Skeleton.h"
#include "Actors/TextureAnimation.h"
#include "Field/Gimmicks.h"
#include "Legacy/Psx.h"
#include "Render/Palette.h"

void Actors_UpdateActorAppearanceTransition(Actors_ActorRecord* actor);
void Field_UpdateGimmickAppearanceTransition(Field_GimmickInstance* object);

#endif  // VS_OWNED_ACTORS_APPEARANCETRANSITION_H
