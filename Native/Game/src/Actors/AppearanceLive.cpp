// Hand-maintained (tools/port/owned.csv).
// Live binding for the appearance requests (Appearance.hpp): the actor
// records, the weapon models of each actor slot and the field gimmicks that
// share the actor ids.
#include "Actors/AppearanceLive.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>

#include "Actors/ActorRecord.hpp"
#include "Actors/ActorTableLive.hpp"
#include "Actors/Appearance.hpp"
#include "Actors/AppearanceLiveState.hpp"
#include "Actors/SequencePlaybackLive.hpp"
#include "Actors/SkeletonLiveState.hpp"
#include "Actors/WeaponModel.hpp"
#include "Core/Diagnostics.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Field/GimmickLookup.hpp"
#include "Field/Gimmicks.hpp"
#include "Render/Palette.hpp"
#include "Render/PaletteAnimationLive.hpp"

using VS::Core::Gte::VECTOR;

// decomp: D_800F4438 (D_800F4438@BATTLE/BATTLE.PRG)
VECTOR VS::Actors::roomActorScale;

namespace VS::Actors {
namespace {
using Core::Math::StoreVector;
using Core::Math::VectorOf;

static_assert(std::size(VS::Actors::actorRecords) == kActorTableSize);
static_assert(std::size(weaponModels) % kWeaponsPerActor == 0);
static_assert(sizeof(VS::Render::ColorCode) == sizeof(Render::ColorCode));

// The actor CLUT rows start at this row plus the actor's slot.
constexpr unsigned kActorClutRowBase = 0x16;
// Gimmick palettes are 16-color runs of this row, one per gimmick slot.
constexpr unsigned kGimmickClutRow = 0x1F;
constexpr unsigned kWeaponTableSize = std::size(weaponModels);
constexpr unsigned kWeaponSlots = kWeaponTableSize / kWeaponsPerActor;
constexpr int kNoActorStatus = static_cast<int>(AppearanceStatus::kNoModel);

[[nodiscard]] VS::Actors::WeaponModel** LiveWeaponPair(unsigned slot,
                                                       const char* operation) {
  if (slot >= kWeaponSlots) {
    VS::Core::Warnf("actors", "%s refused: weapon slot %u is outside the table",
                    operation, slot);
    return nullptr;
  }
  return &weaponModels[slot * kWeaponsPerActor];
}

[[nodiscard]] Render::ColorCode ToColor(const VS::Render::ColorCode& live) {
  return {live.red, live.green, live.blue, live.code};
}

[[nodiscard]] VS::Render::ColorCode ToLiveColor(
    const Render::ColorCode& color) {
  return {color.red, color.green, color.blue, color.code};
}

[[nodiscard]] ActorColors LoadColors(const VS::Actors::ActorRecord& actor) {
  return {.fade = {actor.fade_red, actor.fade_green, actor.fade_blue},
          .custom_tint = actor.custom_tint != 0,
          .tint = ToColor(actor.instance.tint)};
}

void StoreColors(const ActorColors& colors, VS::Actors::ActorRecord& actor) {
  actor.fade_red = colors.fade[0];
  actor.fade_green = colors.fade[1];
  actor.fade_blue = colors.fade[2];
  actor.custom_tint = colors.custom_tint ? 1 : 0;
  actor.instance.tint = ToLiveColor(colors.tint);
}

[[nodiscard]] PalettePulseRequest LoadPulse(
    const VS::Actors::ModelEffectSlot& slot) {
  return {.kind = slot.kind,
          .pulse = {.level = slot.level, .flags = slot.flags}};
}

void StorePulse(const PalettePulseRequest& request,
                VS::Actors::ModelEffectSlot& slot) {
  slot.kind = request.kind;
  slot.level = request.pulse.level;
  slot.flags = request.pulse.flags;
}

// The palette pulse of the actor, or else the gimmick, that `actor_id`
// names, and the upload of its base palettes.
struct LivePulseModel {
  VS::Actors::ActorRecord* actor = nullptr;
  VS::Field::GimmickInstance* gimmick = nullptr;

  [[nodiscard]] VS::Actors::ModelEffectSlot* Slot() const {
    if (actor != nullptr) return &actor->instance.palette_pulse;
    return gimmick != nullptr ? &gimmick->palette_pulse : nullptr;
  }

  void ReloadBasePalettes() const {
    if (actor != nullptr) {
      Render::LoadLiveClutColors(actor->base_palette,
                                 actor->instance.slot + kActorClutRowBase, 0,
                                 kActorPaletteColors);
    } else {
      Render::LoadLiveClutColors(gimmick->base_palette.data(), kGimmickClutRow,
                                 gimmick->slot * kSmallPaletteColors,
                                 kSmallPaletteColors);
    }
  }
};

[[nodiscard]] LivePulseModel FindPulseModel(int actor_id,
                                            const char* operation) {
  if (VS::Actors::ActorRecord* actor = LiveActorRecord(actor_id, operation)) {
    return {.actor = actor};
  }
  return {.gimmick = VS::Field::FindGimmickForActorSlot(actor_id)};
}

class LiveTransitionActors final : public AppearanceTransitionActors {
 public:
  [[nodiscard]] bool Exists(int actor_id) const override {
    return IsActorTableId(actor_id) &&
           VS::Actors::actorRecords[actor_id] != nullptr;
  }
  void SetTransitionState(int actor_id, int state) override {
    VS::Actors::actorRecords[actor_id]->instance.action_animation_state =
        static_cast<unsigned>(state);
  }
  [[nodiscard]] int CompanionSlot(int actor_id) const override {
    return VS::Actors::actorRecords[actor_id]->companion_slot;
  }
};
}  // namespace

WeaponAppearance LoadLiveWeaponAppearance(
    const VS::Actors::WeaponModel& weapon) {
  return {.attached = weapon.attachment_bone != 0,
          .shown = weapon.shown != 0,
          .brightness_override = weapon.brightness_override != 0,
          .brightness = weapon.brightness,
          .blend_level = weapon.blend_level,
          .blend_target = weapon.blend_target,
          .blend_frames = weapon.blend_frames};
}

void StoreLiveWeaponAppearance(const WeaponAppearance& appearance,
                               VS::Actors::WeaponModel& weapon) {
  weapon.shown = appearance.shown ? 1 : 0;
  weapon.brightness_override = appearance.brightness_override ? 1 : 0;
  weapon.brightness = appearance.brightness;
  weapon.blend_level = appearance.blend_level;
  weapon.blend_target = appearance.blend_target;
  weapon.blend_frames = appearance.blend_frames;
}

void SetLiveActorPaletteFade(int actor_id, int fade_parameter) {
  VS::Actors::ActorRecord* actor =
      LiveActorRecord(actor_id, "SetActorPaletteFade");
  if (actor == nullptr) return;
  PaletteFade fade{.active = actor->palette_fade_active != 0,
                   .reverse = actor->palette_fade_reverse != 0,
                   .phase = actor->palette_fade_state[0],
                   .rate = actor->palette_fade_state[1]};
  SetActorPaletteFade(fade, fade_parameter);
  actor->palette_fade_active = fade.active ? 1 : 0;
  actor->palette_fade_reverse = fade.reverse ? 1 : 0;
  actor->palette_fade_state[0] = fade.phase;
  actor->palette_fade_state[1] = fade.rate;
}

void ResetLiveActorWeaponVisibilityBlend(int actor_id) {
  constexpr const char* kOperation = "ResetActorWeaponVisibilityBlend";
  const VS::Actors::ActorRecord* actor = LiveActorRecord(actor_id, kOperation);
  if (actor == nullptr) return;
  VS::Actors::WeaponModel** pair =
      LiveWeaponPair(actor->instance.slot, kOperation);
  if (pair == nullptr) return;
  std::array<std::optional<WeaponAppearance>, kWeaponsPerActor> weapons{};
  for (std::size_t i = 0; i < kWeaponsPerActor; ++i) {
    if (pair[i] != nullptr) weapons[i] = LoadLiveWeaponAppearance(*pair[i]);
  }
  ResetActorWeaponVisibilityBlend(weapons);
  for (std::size_t i = 0; i < kWeaponsPerActor; ++i) {
    if (pair[i] != nullptr) StoreLiveWeaponAppearance(*weapons[i], *pair[i]);
  }
}

void SetLiveWeaponBrightnessOverride(int actor_id, int override_enabled,
                                     int brightness) {
  VS::Actors::WeaponModel** pair = LiveWeaponPair(
      static_cast<unsigned>(actor_id), "SetWeaponBrightnessOverride");
  if (pair == nullptr || pair[0] == nullptr) return;
  WeaponAppearance weapon = LoadLiveWeaponAppearance(*pair[0]);
  // The original stored both into a bit and a byte, keeping the low bits.
  SetWeaponBrightnessOverride(weapon, (override_enabled & 1) != 0,
                              static_cast<std::uint8_t>(brightness));
  StoreLiveWeaponAppearance(weapon, *pair[0]);
}

void SetLiveRoomActorScale(const VECTOR* room_scale) {
  if (room_scale == nullptr) {
    WarnActorRefused("SetRoomActorScale", "null scale");
    return;
  }
  Core::Math::Vec3 stored = VectorOf(roomActorScale);
  const std::int16_t actor_scale =
      SetRoomActorScale(stored, VectorOf(*room_scale));
  StoreVector(roomActorScale, stored);
  roomActorScale.pad = room_scale->pad;
  for (VS::Actors::ActorRecord* actor : VS::Actors::actorRecords) {
    if (actor != nullptr) actor->scale = actor_scale;
  }
}

int EnableLiveModelPalettePulse(int actor_id, int pulse_flags, int pulse_slot) {
  const LivePulseModel model =
      FindPulseModel(actor_id, "EnableModelPalettePulse");
  VS::Actors::ModelEffectSlot* slot = model.Slot();
  if (slot == nullptr) return kNoActorStatus;
  PalettePulseRequest request = LoadPulse(*slot);
  const PalettePulseChange change =
      EnableModelPalettePulse(request, pulse_flags, pulse_slot);
  StorePulse(request, *slot);
  if (change.restarted) model.ReloadBasePalettes();
  return static_cast<int>(change.status);
}

int DisableLiveModelPalettePulse(int actor_id, int pulse_slot) {
  VS::Actors::ModelEffectSlot* slot =
      FindPulseModel(actor_id, "DisableModelPalettePulse").Slot();
  if (slot == nullptr) return kNoActorStatus;
  PalettePulseRequest request = LoadPulse(*slot);
  const AppearanceStatus status = DisableModelPalettePulse(request, pulse_slot);
  StorePulse(request, *slot);
  return static_cast<int>(status);
}

void BeginLiveActorShadowBlend(VS::Actors::ActorRecord* actor,
                               int blend_frames) {
  if (actor == nullptr) {
    WarnActorRefused("BeginActorShadowBlend", "null actor");
    return;
  }
  ShadowBlend shadow = LoadLiveShadowBlend(actor->shadow_blend);
  BeginActorShadowBlend(shadow,
                        {.expand = actor->shadow_radius_expand_rate,
                         .shrink = actor->shadow_radius_shrink_rate},
                        blend_frames);
  StoreLiveShadowBlend(shadow, actor->shadow_blend);
}

int LiveActorFadeParameters(int actor_id,
                            VS::Render::ColorCode* fade_color_out) {
  constexpr const char* kOperation = "ActorFadeParameters";
  const VS::Actors::ActorRecord* actor = LiveActorRecord(actor_id, kOperation);
  if (actor == nullptr) return kNoActorStatus;
  if (fade_color_out == nullptr) {
    WarnActorRefused(kOperation, "null color");
    return kNoActorStatus;
  }
  Render::ColorCode fade = ToColor(*fade_color_out);
  ActorFadeParameters(LoadColors(*actor), fade);
  *fade_color_out = ToLiveColor(fade);
  return static_cast<int>(AppearanceStatus::kOk);
}

int SetLiveActorFadeParameters(int actor_id,
                               const VS::Render::ColorCode* fade_color) {
  constexpr const char* kOperation = "SetActorFadeParameters";
  VS::Actors::ActorRecord* actor = LiveActorRecord(actor_id, kOperation);
  if (actor == nullptr) return kNoActorStatus;
  if (fade_color == nullptr) {
    WarnActorRefused(kOperation, "null color");
    return kNoActorStatus;
  }
  ActorColors colors = LoadColors(*actor);
  SetActorFadeParameters(colors, ToColor(*fade_color));
  StoreColors(colors, *actor);
  return static_cast<int>(AppearanceStatus::kOk);
}

int LiveActorTint(int actor_id, VS::Render::ColorCode* tint_out) {
  constexpr const char* kOperation = "ActorTint";
  const VS::Actors::ActorRecord* actor = LiveActorRecord(actor_id, kOperation);
  if (actor == nullptr) return kNoActorStatus;
  if (tint_out == nullptr) {
    WarnActorRefused(kOperation, "null color");
    return kNoActorStatus;
  }
  *tint_out = ToLiveColor(ActorTint(LoadColors(*actor)));
  return static_cast<int>(AppearanceStatus::kOk);
}

int SetLiveActorTint(int actor_id, const VS::Render::ColorCode* tint) {
  VS::Actors::ActorRecord* actor = LiveActorRecord(actor_id, "SetActorTint");
  if (actor == nullptr) return kNoActorStatus;
  ActorColors colors = LoadColors(*actor);
  SetActorTint(colors, tint != nullptr
                           ? std::optional<Render::ColorCode>(ToColor(*tint))
                           : std::nullopt);
  StoreColors(colors, *actor);
  return static_cast<int>(AppearanceStatus::kOk);
}

void StartLiveActorAppearanceTransition(int actor_id, int transition_kind) {
  LiveTransitionActors actors;
  if (!StartActorAppearanceTransition(actors, actor_id, transition_kind)) {
    VS::Core::Warnf(
        "actors",
        "StartActorAppearanceTransition refused: the chain from actor "
        "%d names a missing actor or repeats",
        actor_id);
  }
}
}  // namespace VS::Actors
