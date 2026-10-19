#pragma once
// Hand-maintained (tools/port/owned.csv).
#ifndef VS_OWNED_FIELD_COLLISIONSAMPLING_H
#define VS_OWNED_FIELD_COLLISIONSAMPLING_H
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/ModelInstance.h"
#include "Actors/Movement.h"
#include "Actors/Shadow.h"
#include "Actors/Skeleton.h"
#include "Actors/TextureAnimation.h"
#include "Field/Mpd.h"
#include "Field/Platforms.h"
#include "Field/Zone.h"
#include "Legacy/Psx.h"
#include "Render/Palette.h"

u_char Field_SampleCollisionHeightCode(u_int, int, int);

#endif  // VS_OWNED_FIELD_COLLISIONSAMPLING_H
