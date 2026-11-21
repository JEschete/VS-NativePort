// Hand-maintained (tools/port/owned.csv).
#include "Audio/ActorSoundLive.hpp"

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <span>

#include "Actors/ActorRecord.hpp"
#include "Actors/ModelInstance.hpp"
#include "Actors/ShpModel.hpp"
#include "Actors/SkeletonLive.hpp"
#include "Audio/ActorSound.hpp"
#include "Audio/SeqRecordsLive.hpp"
#include "Audio/Sfx.hpp"
#include "Audio/SfxLive.hpp"
#include "Audio/SoundBanks.hpp"
#include "Audio/SoundSlotsLive.hpp"
#include "Audio/SurfaceSounds.hpp"
#include "Battle/Actor.hpp"
#include "Battle/Damage.hpp"
#include "Core/RandomLive.hpp"
#include "Effects/WeatherPoolLive.hpp"
#include "Field/Collision.hpp"
#include "Field/CollisionMapLive.hpp"
#include "Field/GimmickPlacementLive.hpp"
#include "Field/Gimmicks.hpp"
#include "Field/Mpd.hpp"
#include "Legacy/Psx.hpp"

using VS::Core::Gte::SVECTOR;

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
static_assert(std::size(VS::Actors::actorRecords) == kActorRecordCount);
static_assert(std::size(VS::Battle::actors) == kBattleActorCount);
static_assert(std::size(VS::Field::gimmickInstances) == kGimmickCount);
static_assert(std::size(VS::Audio::objectSurfaceSoundIds) ==
              kObjectSurfaceSoundBytes);
static_assert(std::size(VS::Field::tileSurfaceIds) == kTileSurfaceSoundBytes);
static_assert(sizeof(VS::Actors::ModelEffectSlot) == kModelEffectSlotBytes);
static_assert(offsetof(VS::Actors::ModelEffectSlot, kind) == 0);
static_assert(offsetof(VS::Actors::ModelEffectSlot, level) == 1);
static_assert(offsetof(VS::Actors::ModelEffectSlot, flags) == 2);
static_assert(sizeof(VS::Actors::ActorRecord::animation_id) == 1);
static_assert(sizeof(VS::Actors::ActorRecord::contact_kind) == 1);
static_assert(sizeof(VS::Actors::ModelInstance::slot) == 1);
static_assert(sizeof(VS::Actors::ModelInstance::current_tile_x) == 1);
static_assert(offsetof(VS::Actors::ModelInstance, current_tile_layer) ==
              offsetof(VS::Actors::ModelInstance, current_tile_x) + 1);
static_assert(offsetof(VS::Actors::ModelInstance, current_tile_z) ==
              offsetof(VS::Actors::ModelInstance, current_tile_x) + 2);
static_assert(sizeof(SVECTOR) == kPositionVectorBytes);
static_assert(offsetof(VS::Actors::ShpModel, sound_section) ==
              sizeof(VS::Actors::ShpModel::header));
static_assert(sizeof(VS::Actors::ShpModel::sound_section) ==
              kSoundSectionBytes);
static_assert(offsetof(VS::Field::GimmickInstance, position_x) ==
              kGimmickPositionOffset);
static_assert(offsetof(VS::Field::GimmickInstance, position_y) ==
              kGimmickPositionOffset + sizeof(std::int16_t));
static_assert(offsetof(VS::Field::GimmickInstance, position_z) ==
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

// The SHP's sound section: sound_section is a byte offset from the model.
BankBuffer SoundBank(const VS::Actors::ShpModel* shp) {
  if (shp == nullptr) return BankBuffer::kNone;
  return LiveBankBuffer(reinterpret_cast<const std::byte*>(shp) +
                        shp->sound_section);
}

ActorModelSound ModelSound(const VS::Actors::ModelInstance& instance) {
  return {.height = instance.position.vy,
          .actor = instance.slot,
          .bank = SoundBank(instance.model)};
}

VS::Actors::ActorRecord* Record(std::int32_t actor) {
  if (actor < 0 || static_cast<std::size_t>(actor) >= kActorRecordCount) {
    return nullptr;
  }
  return VS::Actors::actorRecords[actor];
}

VS::Battle::Actor* BattleActor(std::int32_t actor) {
  if (actor < 0 || static_cast<std::size_t>(actor) >= kBattleActorCount) {
    return nullptr;
  }
  return VS::Battle::actors[actor];
}

class LiveActorSoundHost final : public ActorSoundHost {
 public:
  std::optional<ActorSoundRecord> ActorSound(std::int32_t actor) override {
    const VS::Actors::ActorRecord* record = Record(actor);
    if (record == nullptr) return std::nullopt;
    const VS::Actors::ModelInstance& instance = record->instance;
    return ActorSoundRecord{
        .sound_override = {.kind = record->sound_effect.kind,
                           .pan = record->sound_effect.level,
                           .volume = record->sound_effect.flags},
        .motion = {.action_animation_state = instance.action_animation_state,
                   .is_culled = instance.is_culled != 0,
                   .sound_enabled = instance.sound_enabled != 0,
                   .movement_mode = instance.movement_mode,
                   .locomotion_state = instance.locomotion_state,
                   .motion_locked = instance.motion_locked != 0}};
  }

  std::optional<ActorModelSound> ActorModel(std::int32_t actor) override {
    const VS::Actors::ActorRecord* record = Record(actor);
    if (record == nullptr) return std::nullopt;
    return ModelSound(record->instance);
  }

  void ProjectSoundPosition(std::int32_t actor) override {
    VS::Actors::ActorRecord* record = Record(actor);
    if (record == nullptr) return;
    Actors::ProjectLiveActorTaggedBone(actor, kSoundPositionTag,
                                       &record->sound_position, 0);
  }

  SfxPoint SoundPosition(std::int32_t actor) override {
    const VS::Actors::ActorRecord* record = Record(actor);
    if (record == nullptr) return {};
    const SVECTOR& position = record->sound_position;
    return {.x = position.vx, .y = position.vy, .z = position.vz};
  }

  bool ScriptActorPresent(std::int32_t actor) override {
    return BattleActor(actor) != nullptr;
  }

  std::optional<ActorModelSound> ScriptActorModel(std::int32_t actor) override {
    const VS::Battle::Actor* battle_actor = BattleActor(actor);
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
    const VS::Field::MpdTileCell* cell =
        Field::LiveCollisionCellAt(tile_x, tile_z, layer);
    if (cell == nullptr) return std::nullopt;
    return static_cast<std::uint8_t>(cell->surface_kind);
  }

  std::span<const std::uint8_t> ObjectSurfaceSounds() override {
    return VS::Audio::objectSurfaceSoundIds;
  }

  std::span<const std::uint8_t> TileSurfaceSounds() override {
    return VS::Field::tileSurfaceIds;
  }

  RoomWeather Weather() override {
    RoomWeather weather;
    Effects::LiveRoomWeatherState(&weather.kind, &weather.intensity);
    return weather;
  }

  std::int32_t BoundedRandom(std::int32_t bound) override {
    return Core::NextLiveRandomBounded(bound);
  }

  std::int32_t Random() override { return VS::Core::NextBiosRand(); }

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

// decomp: BATTLE.PRG func_800AD62C (src/BATTLE/BATTLE.PRG/40564.c:2209)
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
void LivePlayActorModelSound(const VS::Actors::ModelInstance* model_instance,
                             int sound_index) {
  if (model_instance == nullptr) {
    Seq::ReportRefusal("PlayActorModelSound", "no model instance");
    return;
  }
  LiveActorSoundHost host;
  Report("PlayActorModelSound",
         PlayActorModelSound(host, ModelSound(*model_instance), sound_index));
}

// decomp: BATTLE.PRG func_800AE68C (src/BATTLE/BATTLE.PRG/44F14.c:667)
void LivePlayActorModelSoundVariant(int actor_index, int variant_index) {
  LiveActorSoundHost host;
  Report("PlayActorModelSoundVariant",
         PlayActorModelSoundVariant(host, actor_index, variant_index));
}

// decomp: BATTLE.PRG func_800AE6C0 (src/BATTLE/BATTLE.PRG/44F14.c:690)
void LivePlayActorSoundPair(const VS::Actors::ActorRecord* source_actor,
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

// decomp: BATTLE.PRG func_800AE7D8 (src/BATTLE/BATTLE.PRG/44F14.c:707)
void LivePlayGimmickSoundPair(const VS::Field::GimmickInstance* gimmick,
                              int first_effect_id, int second_effect_id) {
  if (gimmick == nullptr) {
    Seq::ReportRefusal("PlayGimmickSoundPair", "no gimmick");
    return;
  }
  LiveActorSoundHost host;
  PlayGimmickSoundPair(host,
                       {.x = gimmick->position_x,
                        .y = gimmick->position_y,
                        .z = gimmick->position_z},
                       {.first = first_effect_id, .second = second_effect_id});
}

// decomp: BATTLE.PRG func_800AE828 (src/BATTLE/BATTLE.PRG/44F14.c:739)
void LivePlayActorSurfaceContactSound(int actor_index,
                                      const VS::Actors::ActorRecord* actor,
                                      int contact_sound_code) {
  if (actor == nullptr) {
    Seq::ReportRefusal("PlayActorSurfaceContactSound", "no actor");
    return;
  }
  const VS::Actors::ModelInstance& instance = actor->instance;
  const ActorContactSource source{.animation_id = actor->animation_id,
                                  .on_gimmick = actor->support_gimmick != 0,
                                  .tile_x = instance.current_tile_x,
                                  .tile_layer = instance.current_tile_layer,
                                  .tile_z = instance.current_tile_z,
                                  .contact_layer = actor->contact_kind,
                                  .model = ModelSound(instance)};
  LiveActorSoundHost host;
  Report("PlayActorSurfaceContactSound",
         PlayActorSurfaceContactSound(host, actor_index, source,
                                      contact_sound_code));
}

// decomp: BATTLE.PRG func_8007C8A4 (src/BATTLE/BATTLE.PRG/146C.c:8872)
void LivePlayActorScriptSound(int actor_index, int sound_index) {
  LiveActorSoundHost host;
  Report("PlayActorScriptSound",
         PlayActorScriptSound(host, actor_index, sound_index));
}
}  // namespace VS::Audio
