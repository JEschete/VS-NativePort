// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_ACTORS_APPEARANCE_H_TYPES
#define VS_ACTORS_APPEARANCE_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_ACTORS_APPEARANCE_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_ACTORS_APPEARANCE_H_DECLS)
#define VS_ACTORS_APPEARANCE_H_DECLS
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/ModelInstance.h"
#include "Actors/Movement.h"
#include "Actors/Palette.h"
#include "Actors/Shadow.h"
#include "Actors/Skeleton.h"
#include "Actors/TextureAnimation.h"
#include "Render/Palette.h"

extern VECTOR Actors_roomActorScale;
void Actors_SetActorPaletteFade(int arg0, int arg1);
void Actors_ResetActorWeaponVisibilityBlend(int arg0);
void Actors_SetWeaponBrightnessOverride(int arg0, int arg1, int arg2);
void Actors_SetRoomActorScale(VECTOR* arg0);
int Actors_EnableModelPalettePulse(int arg0, int arg1, int arg2);
int Actors_DisableModelPalettePulse(int arg0, int arg1);
void Actors_BeginActorShadowBlend(Actors_ActorRecord* arg0, int arg1);
int Actors_ActorFadeParameters(int arg0, Render_ColorCode* arg1);
int Actors_SetActorFadeParameters(int arg0, Render_ColorCode* arg1);
int Actors_ActorTint(int arg0, Render_ColorCode* arg1);
int Actors_SetActorTint(int arg0, Render_ColorCode* arg1);
void Actors_UpdateActorPalettePulse(Actors_ActorRecord* actor, Actors_PalettePulseView* object, int arg2);
void Actors_StartActorAppearanceTransition(int arg0, int arg1);

#endif // VS_ACTORS_APPEARANCE_H_DECLS
