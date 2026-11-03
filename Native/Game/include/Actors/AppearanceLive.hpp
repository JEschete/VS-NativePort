#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Actors/Appearance.hpp"
#include "Legacy/Psx.h"

struct Actors_ActorRecord;
struct Actors_WeaponModel;
struct Render_ColorCode;

// Bindings from the actor records, weapon models and field gimmicks to the
// typed appearance requests (Appearance.cpp), in the original argument order
// for the generated callers (tools/port/copy_fixups.py OWNED_CALL_BINDINGS).
// An actor id outside the actor table is reported and treated as an empty
// slot; the status results are AppearanceStatus values.
namespace VS::Actors {
// The weapon state the appearance requests use, and its store.
[[nodiscard]] WeaponAppearance LoadLiveWeaponAppearance(
    const Actors_WeaponModel& weapon);
void StoreLiveWeaponAppearance(const WeaponAppearance& appearance,
                               Actors_WeaponModel& weapon);

void SetLiveActorPaletteFade(int actor_id, int fade_parameter);
void ResetLiveActorWeaponVisibilityBlend(int actor_id);
// `actor_id` names the slot whose first weapon model is changed.
void SetLiveWeaponBrightnessOverride(int actor_id, int override_enabled,
                                     int brightness);
void SetLiveRoomActorScale(const VECTOR* room_scale);
// `actor_id` names an actor, or else the gimmick in that slot.
int EnableLiveModelPalettePulse(int actor_id, int pulse_flags, int pulse_slot);
int DisableLiveModelPalettePulse(int actor_id, int pulse_slot);
void BeginLiveActorShadowBlend(Actors_ActorRecord* actor, int blend_frames);
int LiveActorFadeParameters(int actor_id, Render_ColorCode* fade_color_out);
int SetLiveActorFadeParameters(int actor_id,
                               const Render_ColorCode* fade_color);
int LiveActorTint(int actor_id, Render_ColorCode* tint_out);
// A null tint returns the actor to its own color.
int SetLiveActorTint(int actor_id, const Render_ColorCode* tint);
void StartLiveActorAppearanceTransition(int actor_id, int transition_kind);
}  // namespace VS::Actors
