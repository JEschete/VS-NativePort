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
#include "Actors/Shadow.h"
#include "Actors/Skeleton.h"
#include "Actors/TextureAnimation.h"
#include "Render/Palette.h"

extern VECTOR Actors_roomActorScale;
void Actors_SetActorPaletteFade(int actor_id, int fade_parameter);
void Actors_ResetActorWeaponVisibilityBlend(int actor_id);
void Actors_SetWeaponBrightnessOverride(int actor_id, int override_enabled, int brightness);
void Actors_SetRoomActorScale(VECTOR* room_scale);
int Actors_EnableModelPalettePulse(int actor_id, int pulse_flags, int pulse_slot);
int Actors_DisableModelPalettePulse(int actor_id, int pulse_slot);
void Actors_BeginActorShadowBlend(Actors_ActorRecord* actor, int blend_frames);
int Actors_ActorFadeParameters(int actor_id, Render_ColorCode* fade_color_out);
int Actors_SetActorFadeParameters(int actor_id, Render_ColorCode* fade_color);
int Actors_ActorTint(int actor_id, Render_ColorCode* tint_out);
int Actors_SetActorTint(int actor_id, Render_ColorCode* tint);
void Actors_StartActorAppearanceTransition(int actor_id, int transition_kind);

#endif // VS_ACTORS_APPEARANCE_H_DECLS
