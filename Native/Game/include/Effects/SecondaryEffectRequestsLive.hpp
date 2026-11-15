#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

struct Actors_ActorLocation;
struct Battle_HitResult;

// Live binding for the secondary hit and spatial effect requests
// (SecondaryEffectRequests.hpp) over Battle_actionState, the action
// definitions, Effects_effectBusyFlags, the skeleton's bone tags, the weapon
// hit sounds and LiveBeginSecondaryEffects. The entries keep the original
// signatures for the generated callers and the module's function pointers. A
// null hit, location or position and a missing action state are refused with
// a diagnostic, as is a queued target past the request's target array.
namespace VS::Effects {
// func_8006C4C4 (146C.c:1927).
void LiveQueueActionHitEffect(int effect_id, Battle_HitResult* hit_result,
                              int target_limb);
// func_8006C84C (146C.c:2044): returns 1.
int LiveQueueActionImpactEffects(int attacker_index);
// func_8006F450 (146C.c:3171): the x and z of an SVECTOR's halfwords 0 and 2.
void LiveSpawnGroundPlaneImpactEffect(std::uint16_t* world_position_components);
// func_8006F4B0 (146C.c:3187).
void LiveSpawnTrapTileEffect(Actors_ActorLocation* actor_location);
// func_8006F848 (146C.c:3298).
void LiveSpawnAbilityLearnedEffect();
}  // namespace VS::Effects
