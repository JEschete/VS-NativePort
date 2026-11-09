// Hand-maintained (tools/port/owned.csv).
#include "Audio/ActorSoundLive.hpp"

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <span>

#include "Actors/SkeletonLive.hpp"
#include "Audio/ActorSound.hpp"
#include "Audio/SeqRecordsLive.hpp"
#include "Audio/Sfx.hpp"
#include "Audio/SfxLive.hpp"
#include "Audio/SoundBanks.hpp"
#include "Audio/SoundSlotsLive.hpp"
#include "Core/RandomLive.hpp"
#include "Field/CollisionMapLive.hpp"
#include "Field/GimmickPlacementLive.hpp"

extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/ModelInstance.h"
#include "Actors/Movement.h"
#include "Actors/Skeleton.h"
#include "Audio/ActorSound.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Core/BiosRand.h"
#include "Effects/Weather.h"
#include "Field/Collision.h"
#include "Field/Gimmicks.h"
#include "Field/Mpd.h"
#include "Legacy/Psx.h"
}

namespace VS::Audio {
namespace {
constexpr std::size_t kActorRecordCount = 17;
constexpr std::size_t kBattleActorCount = 42;
constexpr std::size_t kGimmickCount = 16;
constexpr std::size_t kObjectSurfaceSoundBytes = 2956;
constexpr std::size_t kTileSurfaceSoundBytes = 2744;
// func_800A1280's tag for the actor's sound position.
constexpr int kSoundPositionTag = 0xFF;
// func_800A91DC only reports idle gimmicks for a surface.
constexpr int kRequireIdleGimmick = 1;
// The original's gimmick position (D_800F45E0_t +0x1C).
constexpr std::size_t kGimmickPositionOffset = 0x1C;
// The legacy layouts the original reads: a model effect slot's bytes, a
// position vector's and the SHP model's sound section word.
constexpr std::size_t kModelEffectSlotBytes = 4;
constexpr std::size_t kPositionVectorBytes = 8;
constexpr std::size_t kSoundSectionBytes = 4;

// The record tables and the fields the original reads (lbu, lh) as typed.
static_assert(std::size(Actors_actorRecords) == kActorRecordCount);
static_assert(std::size(Battle_actors) == kBattleActorCount);
static_assert(std::size(Field_gimmickInstances) == kGimmickCount);
static_assert(std::size(Audio_objectSurfaceSoundIds) ==
              kObjectSurfaceSoundBytes);
static_assert(std::size(Field_kTileSurfaceIds) == kTileSurfaceSoundBytes);
static_assert(sizeof(Actors_ModelEffectSlot) == kModelEffectSlotBytes);
static_assert(offsetof(Actors_ModelEffectSlot, kind) == 0);
static_assert(offsetof(Actors_ModelEffectSlot, level) == 1);
static_assert(offsetof(Actors_ModelEffectSlot, flags) == 2);
static_assert(sizeof(Actors_ActorRecord::animationId) == 1);
static_assert(sizeof(Actors_ActorRecord::contactKind) == 1);
static_assert(sizeof(Actors_ModelInstance::slot) == 1);
static_assert(sizeof(Actors_ModelInstance::currentTileX) == 1);
static_assert(offsetof(Actors_ModelInstance, currentTileLayer) ==
              offsetof(Actors_ModelInstance, currentTileX) + 1);
static_assert(offsetof(Actors_ModelInstance, currentTileZ) ==
              offsetof(Actors_ModelInstance, currentTileX) + 2);
static_assert(sizeof(SVECTOR) == kPositionVectorBytes);
static_assert(offsetof(Actors_ShpModel, soundSection) ==
              sizeof(Actors_ShpModel::header));
static_assert(sizeof(Actors_ShpModel::soundSection) == kSoundSectionBytes);
static_assert(offsetof(Field_GimmickInstance, positionX) ==
              kGimmickPositionOffset);
static_assert(offsetof(Field_GimmickInstance, positionY) ==
              kGimmickPositionOffset + sizeof(std::int16_t));
static_assert(offsetof(Field_GimmickInstance, positionZ) ==
              kGimmickPositionOffset + 2 * sizeof(std::int16_t));

const char* RefusalReason(ActorSoundStatus status) {
  switch (status) {
    case ActorSoundStatus::kNoActor:
      return "actor record is missing or out of range";
    case ActorSoundStatus::kNoModel:
      return "battle actor has no model";
    case ActorSoundStatus::kNoSoundBank:
      return "actor model has no SHP sound bank";
    case ActorSoundStatus::kNoGimmick:
      return "no gimmick in the top gimmick slot";
    case ActorSoundStatus::kNoSurfaceSound:
      return "surface sound id is outside its table";
    case ActorSoundStatus::kDone:
      break;
  }
  return "refused";
}

void Report(const char* operation, ActorSoundStatus status) {
  if (status != ActorSoundStatus::kDone) {
    Seq::ReportRefusal(operation, RefusalReason(status));
  }
}

// The SHP's sound section (VS_ONDISC(model, soundSection, void*)).
BankBuffer SoundBank(const Actors_ShpModel* shp) {
  if (shp == nullptr) return BankBuffer::kNone;
  return LiveBankBuffer(reinterpret_cast<const std::byte*>(shp) +
                        shp->soundSection);
}

ActorModelSound ModelSound(const Actors_ModelInstance& instance) {
  return {.height = instance.position.vy,
          .actor = instance.slot,
          .bank = SoundBank(instance.model)};
}

Actors_ActorRecord* Record(std::int32_t actor) {
  if (actor < 0 || static_cast<std::size_t>(actor) >= kActorRecordCount) {
    return nullptr;
  }
  return Actors_actorRecords[actor];
}

Battle_Actor* BattleActor(std::int32_t actor) {
  if (actor < 0 || static_cast<std::size_t>(actor) >= kBattleActorCount) {
    return nullptr;
  }
  return Battle_actors[actor];
}

class LiveActorSoundHost final : public ActorSoundHost {
 public:
  std::optional<ActorSoundRecord> ActorSound(std::int32_t actor) override {
    const Actors_ActorRecord* record = Record(actor);
    if (record == nullptr) return std::nullopt;
    const Actors_ModelInstance& instance = record->instance;
    return ActorSoundRecord{
        .sound_override = {.kind = record->soundEffect.kind,
                           .pan = record->soundEffect.level,
                           .volume = record->soundEffect.flags},
        .motion = {.action_animation_state = instance.actionAnimationState,
                   .preview_model = instance.isPreviewModel != 0,
                   .sound_enabled = instance.soundEnabled != 0,
                   .movement_mode = instance.movementMode,
                   .locomotion_state = instance.locomotionState,
                   .motion_locked = instance.motionLocked != 0}};
  }

  std::optional<ActorModelSound> ActorModel(std::int32_t actor) override {
    const Actors_ActorRecord* record = Record(actor);
    if (record == nullptr) return std::nullopt;
    return ModelSound(record->instance);
  }

  void ProjectSoundPosition(std::int32_t actor) override {
    Actors_ActorRecord* record = Record(actor);
    if (record == nullptr) return;
    Actors::ProjectLiveActorTaggedBone(actor, kSoundPositionTag,
                                       &record->soundPosition, 0);
  }

  SfxPoint SoundPosition(std::int32_t actor) override {
    const Actors_ActorRecord* record = Record(actor);
    if (record == nullptr) return {};
    const SVECTOR& position = record->soundPosition;
    return {.x = position.vx, .y = position.vy, .z = position.vz};
  }

  bool ScriptActorPresent(std::int32_t actor) override {
    return BattleActor(actor) != nullptr;
  }

  std::optional<ActorModelSound> ScriptActorModel(std::int32_t actor) override {
    const Battle_Actor* battle_actor = BattleActor(actor);
    if (battle_actor == nullptr || battle_actor->model == nullptr) {
      return std::nullopt;
    }
    return ModelSound(*battle_actor->model);
  }

  std::optional<std::uint8_t> TopGimmickObject(std::uint8_t tile_x,
                                               std::uint8_t tile_z) override {
    const auto index = Field::FindLiveTopGimmickAtTile({tile_x, tile_z},
                                                       kRequireIdleGimmick != 0)
                           .value_or(0);
    const auto gimmick = Field::LiveGimmickQueryState(index);
    if (!gimmick) return std::nullopt;
    return gimmick->object_id;
  }

  std::optional<std::uint8_t> TileSurfaceKind(std::uint8_t tile_x,
                                              std::uint8_t tile_z,
                                              std::uint8_t layer) override {
    const Field_MpdTileCell* cell =
        Field::LiveCollisionCellAt(tile_x, tile_z, layer);
    if (cell == nullptr) return std::nullopt;
    return static_cast<std::uint8_t>(cell->surfaceKind);
  }

  std::span<const std::uint8_t> ObjectSurfaceSounds() override {
    return Audio_objectSurfaceSoundIds;
  }

  std::span<const std::uint8_t> TileSurfaceSounds() override {
    return Field_kTileSurfaceIds;
  }

  RoomWeather Weather() override {
    RoomWeather weather;
    Effects_RoomWeatherState(&weather.kind, &weather.intensity);
    return weather;
  }

  std::int32_t BoundedRandom(std::int32_t bound) override {
    return Core::NextLiveRandomBounded(bound);
  }

  std::int32_t Random() override { return Core_Random_Rand(); }

  void PlaySfx(std::int32_t group, std::int32_t effect_id,
               const SfxMix& mix) override {
    LivePlaySfx(group, effect_id, mix.pan, mix.volume);
  }

  void PlayBankSfx(std::int32_t group, BankBuffer bank, std::int32_t index,
                   const SfxMix& mix) override {
    LivePlayBankSfx(group, LiveBufferPointer(bank),
                    static_cast<unsigned>(index), mix.pan, mix.volume);
  }

  void PlayWorldSfx(std::int32_t group, std::int32_t effect_id,
                    const SfxPoint& point) override {
    SVECTOR position{.vx = point.x, .vy = point.y, .vz = point.z, .pad = 0};
    LivePlayWorldSfx(group, effect_id, &position);
  }
};
}  // namespace

std::optional<SfxMix> LiveActorSoundMix(int actor_index) {
  LiveActorSoundHost host;
  const std::optional<SfxMix> mix = ActorSoundPanAndVolume(host, actor_index);
  if (!mix) Report("ActorSoundPanAndVolume", ActorSoundStatus::kNoActor);
  return mix;
}

// decomp: BATTLE.PRG func_800AD62C (src/BATTLE/BATTLE.PRG/40564.c:2789)
void LiveActorSoundPanAndVolume(int actor_index, int* output_pan,
                                int* output_volume) {
  if (output_pan == nullptr) {
    Seq::ReportRefusal("ActorSoundPanAndVolume", "no pan output");
    return;
  }
  const std::optional<SfxMix> mix = LiveActorSoundMix(actor_index);
  if (!mix) return;
  *output_pan = mix->pan;
  if (output_volume != nullptr) *output_volume = mix->volume;
}

// decomp: BATTLE.PRG func_800AE4FC (src/BATTLE/BATTLE.PRG/44F14.c:637)
void LivePlayActorModelSound(const Actors_ModelInstance* model_instance,
                             int sound_index) {
  if (model_instance == nullptr) {
    Seq::ReportRefusal("PlayActorModelSound", "no model instance");
    return;
  }
  LiveActorSoundHost host;
  Report("PlayActorModelSound",
         PlayActorModelSound(host, ModelSound(*model_instance), sound_index));
}

// decomp: BATTLE.PRG func_800AE68C (src/BATTLE/BATTLE.PRG/44F14.c:685)
void LivePlayActorModelSoundVariant(int actor_index, int variant_index) {
  LiveActorSoundHost host;
  Report("PlayActorModelSoundVariant",
         PlayActorModelSoundVariant(host, actor_index, variant_index));
}

// decomp: BATTLE.PRG func_800AE6C0 (src/BATTLE/BATTLE.PRG/44F14.c:690)
void LivePlayActorSoundPair(const Actors_ActorRecord* source_actor,
                            int first_effect_id, int second_effect_id) {
  if (source_actor == nullptr) {
    Seq::ReportRefusal("PlayActorSoundPair", "no source actor");
    return;
  }
  LiveActorSoundHost host;
  Report("PlayActorSoundPair",
         PlayActorSoundPair(
             host, source_actor->instance.slot,
             {.first = first_effect_id, .second = second_effect_id}));
}

// decomp: BATTLE.PRG func_800AE7D8 (src/BATTLE/BATTLE.PRG/44F14.c:728)
void LivePlayGimmickSoundPair(const Field_GimmickInstance* gimmick,
                              int first_effect_id, int second_effect_id) {
  if (gimmick == nullptr) {
    Seq::ReportRefusal("PlayGimmickSoundPair", "no gimmick");
    return;
  }
  LiveActorSoundHost host;
  PlayGimmickSoundPair(host,
                       {.x = gimmick->positionX,
                        .y = gimmick->positionY,
                        .z = gimmick->positionZ},
                       {.first = first_effect_id, .second = second_effect_id});
}

// decomp: BATTLE.PRG func_800AE828 (src/BATTLE/BATTLE.PRG/44F14.c:739)
void LivePlayActorSurfaceContactSound(int actor_index,
                                      const Actors_ActorRecord* actor,
                                      int contact_sound_code) {
  if (actor == nullptr) {
    Seq::ReportRefusal("PlayActorSurfaceContactSound", "no actor");
    return;
  }
  const Actors_ModelInstance& instance = actor->instance;
  const ActorContactSource source{.animation_id = actor->animationId,
                                  .on_gimmick = actor->supportGimmick != 0,
                                  .tile_x = instance.currentTileX,
                                  .tile_layer = instance.currentTileLayer,
                                  .tile_z = instance.currentTileZ,
                                  .contact_layer = actor->contactKind,
                                  .model = ModelSound(instance)};
  LiveActorSoundHost host;
  Report("PlayActorSurfaceContactSound",
         PlayActorSurfaceContactSound(host, actor_index, source,
                                      contact_sound_code));
}

// decomp: BATTLE.PRG func_8007C8A4 (src/BATTLE/BATTLE.PRG/146C.c:8917)
void LivePlayActorScriptSound(int actor_index, int sound_index) {
  LiveActorSoundHost host;
  Report("PlayActorScriptSound",
         PlayActorScriptSound(host, actor_index, sound_index));
}
}  // namespace VS::Audio
