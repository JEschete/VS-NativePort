#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <optional>

#include "Audio/Sfx.hpp"

struct Actors_ActorRecord;
struct Actors_ModelInstance;
struct Field_GimmickInstance;

// Live bindings for ActorSound.hpp over the battle overlay's actor records
// (D_800F4538), battle actors (vs_battle_actors), gimmicks (D_800F45E0), the
// room's cells and weather, and the sound effect front end. Generated and
// owned callers keep the original call shapes (copy_fixups.py
// OWNED_CALL_BINDINGS). A missing record, model, sound bank or gimmick, an
// actor index outside the records or a null output is refused with a
// diagnostic.
namespace VS::Audio {
// The actor's pan and volume (ActorSoundPanAndVolume), or nothing for an
// actor without a record, which is reported.
[[nodiscard]] std::optional<SfxMix> LiveActorSoundMix(int actor_index);
void LiveActorSoundPanAndVolume(int actor_index, int* output_pan,
                                int* output_volume);
void LivePlayActorModelSound(const Actors_ModelInstance* model_instance,
                             int sound_index);
void LivePlayActorModelSoundVariant(int actor_index, int variant_index);
void LivePlayActorSoundPair(const Actors_ActorRecord* source_actor,
                            int first_effect_id, int second_effect_id);
void LivePlayGimmickSoundPair(const Field_GimmickInstance* gimmick,
                              int first_effect_id, int second_effect_id);
void LivePlayActorSurfaceContactSound(int actor_index,
                                      const Actors_ActorRecord* actor,
                                      int contact_sound_code);
void LivePlayActorScriptSound(int actor_index, int sound_index);
}  // namespace VS::Audio
