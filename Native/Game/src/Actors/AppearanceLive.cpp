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

#include "Actors/ActorTableLive.hpp"
#include "Actors/Appearance.hpp"
#include "Actors/AppearanceLiveState.h"
#include "Actors/SequencePlaybackLive.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Render/PaletteAnimationLive.hpp"

extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/Movement.h"
#include "Actors/Shadow.h"
#include "Actors/SkeletonLiveState.h"
#include "Actors/WeaponModel.h"
#include "Core/Diagnostics.h"
#include "Field/GimmickLookup.h"
#include "Field/Gimmicks.h"
#include "Render/Palette.h"

// decomp: D_800F4438 (D_800F4438@BATTLE/BATTLE.PRG)
VECTOR Actors_roomActorScale;
}

namespace VS::Actors {
namespace {
using Core::Math::StoreVector;
using Core::Math::VectorOf;

static_assert(std::size(Actors_actorRecords) == kActorTableSize);
static_assert(std::size(Actors_weaponModels) % kWeaponsPerActor == 0);
static_assert(sizeof(Render_ColorCode) == sizeof(Render::ColorCode));

// The actor CLUT rows start at this row plus the actor's slot.
constexpr unsigned kActorClutRowBase = 0x16;
// Gimmick palettes are 16-color runs of this row, one per gimmick slot.
constexpr unsigned kGimmickClutRow = 0x1F;
constexpr unsigned kWeaponTableSize = std::size(Actors_weaponModels);
constexpr unsigned kWeaponSlots = kWeaponTableSize / kWeaponsPerActor;
constexpr int kNoActorStatus = static_cast<int>(AppearanceStatus::kNoModel);

[[nodiscard]] Actors_WeaponModel** LiveWeaponPair(unsigned slot,
                                                  const char* operation) {
  if (slot >= kWeaponSlots) {
    Core_Warnf("actors", "%s refused: weapon slot %u is outside the table",
               operation, slot);
    return nullptr;
  }
  return &Actors_weaponModels[slot * kWeaponsPerActor];
}

[[nodiscard]] Render::ColorCode ToColor(const Render_ColorCode& live) {
  return {live.red, live.green, live.blue, live.code};
}

[[nodiscard]] Render_ColorCode ToLiveColor(const Render::ColorCode& color) {
  return {color.red, color.green, color.blue, color.code};
}

[[nodiscard]] ActorColors LoadColors(const Actors_ActorRecord& actor) {
  return {.fade = {actor.fadeRed, actor.fadeGreen, actor.fadeBlue},
          .custom_tint = actor.customTint != 0,
          .tint = ToColor(actor.instance.tint)};
}

void StoreColors(const ActorColors& colors, Actors_ActorRecord& actor) {
  actor.fadeRed = colors.fade[0];
  actor.fadeGreen = colors.fade[1];
  actor.fadeBlue = colors.fade[2];
  actor.customTint = colors.custom_tint ? 1 : 0;
  actor.instance.tint = ToLiveColor(colors.tint);
}

[[nodiscard]] PalettePulseRequest LoadPulse(
    const Actors_ModelEffectSlot& slot) {
  return {.kind = slot.kind,
          .pulse = {.level = slot.level, .flags = slot.flags}};
}

void StorePulse(const PalettePulseRequest& request,
                Actors_ModelEffectSlot& slot) {
  slot.kind = request.kind;
  slot.level = request.pulse.level;
  slot.flags = request.pulse.flags;
}

// The palette pulse of the actor, or else the gimmick, that `actor_id`
// names, and the upload of its base palettes.
struct LivePulseModel {
  Actors_ActorRecord* actor = nullptr;
  Field_GimmickInstance* gimmick = nullptr;

  [[nodiscard]] Actors_ModelEffectSlot* Slot() const {
    if (actor != nullptr) return &actor->instance.palettePulse;
    return gimmick != nullptr ? &gimmick->palettePulse : nullptr;
  }

  void ReloadBasePalettes() const {
    if (actor != nullptr) {
      Render::LoadLiveClutColors(actor->basePalette,
                                 actor->instance.slot + kActorClutRowBase, 0,
                                 kActorPaletteColors);
    } else {
      Render::LoadLiveClutColors(gimmick->basePalette, kGimmickClutRow,
                                 gimmick->slot * kSmallPaletteColors,
                                 kSmallPaletteColors);
    }
  }
};

[[nodiscard]] LivePulseModel FindPulseModel(int actor_id,
                                            const char* operation) {
  if (Actors_ActorRecord* actor = LiveActorRecord(actor_id, operation)) {
    return {.actor = actor};
  }
  return {.gimmick = Field_FindGimmickForActorSlot(actor_id)};
}

class LiveTransitionActors final : public AppearanceTransitionActors {
 public:
  [[nodiscard]] bool Exists(int actor_id) const override {
    return IsActorTableId(actor_id) && Actors_actorRecords[actor_id] != nullptr;
  }
  void SetTransitionState(int actor_id, int state) override {
    Actors_actorRecords[actor_id]->instance.actionAnimationState =
        static_cast<unsigned>(state);
  }
  [[nodiscard]] int CompanionSlot(int actor_id) const override {
    return Actors_actorRecords[actor_id]->companionSlot;
  }
};
}  // namespace

WeaponAppearance LoadLiveWeaponAppearance(const Actors_WeaponModel& weapon) {
  return {.attached = weapon.attachmentBone != 0,
          .shown = weapon.reserved08Bit4 != 0,
          .brightness_override = weapon.reserved08Bit6 != 0,
          .brightness = weapon.reserved5C0,
          .blend_level = weapon.reserved11,
          .blend_target = weapon.reserved12,
          .blend_frames = weapon.reserved13};
}

void StoreLiveWeaponAppearance(const WeaponAppearance& appearance,
                               Actors_WeaponModel& weapon) {
  weapon.reserved08Bit4 = appearance.shown ? 1 : 0;
  weapon.reserved08Bit6 = appearance.brightness_override ? 1 : 0;
  weapon.reserved5C0 = appearance.brightness;
  weapon.reserved11 = appearance.blend_level;
  weapon.reserved12 = appearance.blend_target;
  weapon.reserved13 = appearance.blend_frames;
}

void SetLiveActorPaletteFade(int actor_id, int fade_parameter) {
  Actors_ActorRecord* actor = LiveActorRecord(actor_id, "SetActorPaletteFade");
  if (actor == nullptr) return;
  PaletteFade fade{.active = actor->paletteFadeActive != 0,
                   .reverse = actor->paletteFadeReverse != 0,
                   .phase = actor->paletteFadeState[0],
                   .rate = actor->paletteFadeState[1]};
  SetActorPaletteFade(fade, fade_parameter);
  actor->paletteFadeActive = fade.active ? 1 : 0;
  actor->paletteFadeReverse = fade.reverse ? 1 : 0;
  actor->paletteFadeState[0] = fade.phase;
  actor->paletteFadeState[1] = fade.rate;
}

void ResetLiveActorWeaponVisibilityBlend(int actor_id) {
  constexpr const char* kOperation = "ResetActorWeaponVisibilityBlend";
  const Actors_ActorRecord* actor = LiveActorRecord(actor_id, kOperation);
  if (actor == nullptr) return;
  Actors_WeaponModel** pair = LiveWeaponPair(actor->instance.slot, kOperation);
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
  Actors_WeaponModel** pair = LiveWeaponPair(static_cast<unsigned>(actor_id),
                                             "SetWeaponBrightnessOverride");
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
  Core::Math::Vec3 stored = VectorOf(Actors_roomActorScale);
  const std::int16_t actor_scale =
      SetRoomActorScale(stored, VectorOf(*room_scale));
  StoreVector(Actors_roomActorScale, stored);
  Actors_roomActorScale.pad = room_scale->pad;
  for (Actors_ActorRecord* actor : Actors_actorRecords) {
    if (actor != nullptr) actor->scale = actor_scale;
  }
}

int EnableLiveModelPalettePulse(int actor_id, int pulse_flags, int pulse_slot) {
  const LivePulseModel model =
      FindPulseModel(actor_id, "EnableModelPalettePulse");
  Actors_ModelEffectSlot* slot = model.Slot();
  if (slot == nullptr) return kNoActorStatus;
  PalettePulseRequest request = LoadPulse(*slot);
  const PalettePulseChange change =
      EnableModelPalettePulse(request, pulse_flags, pulse_slot);
  StorePulse(request, *slot);
  if (change.restarted) model.ReloadBasePalettes();
  return static_cast<int>(change.status);
}

int DisableLiveModelPalettePulse(int actor_id, int pulse_slot) {
  Actors_ModelEffectSlot* slot =
      FindPulseModel(actor_id, "DisableModelPalettePulse").Slot();
  if (slot == nullptr) return kNoActorStatus;
  PalettePulseRequest request = LoadPulse(*slot);
  const AppearanceStatus status = DisableModelPalettePulse(request, pulse_slot);
  StorePulse(request, *slot);
  return static_cast<int>(status);
}

void BeginLiveActorShadowBlend(Actors_ActorRecord* actor, int blend_frames) {
  if (actor == nullptr) {
    WarnActorRefused("BeginActorShadowBlend", "null actor");
    return;
  }
  ShadowBlend shadow = LoadLiveShadowBlend(actor->shadowBlend);
  BeginActorShadowBlend(shadow,
                        {.expand = actor->shadowRadiusExpandRate,
                         .shrink = actor->shadowRadiusShrinkRate},
                        blend_frames);
  StoreLiveShadowBlend(shadow, actor->shadowBlend);
}

int LiveActorFadeParameters(int actor_id, Render_ColorCode* fade_color_out) {
  constexpr const char* kOperation = "ActorFadeParameters";
  const Actors_ActorRecord* actor = LiveActorRecord(actor_id, kOperation);
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
                               const Render_ColorCode* fade_color) {
  constexpr const char* kOperation = "SetActorFadeParameters";
  Actors_ActorRecord* actor = LiveActorRecord(actor_id, kOperation);
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

int LiveActorTint(int actor_id, Render_ColorCode* tint_out) {
  constexpr const char* kOperation = "ActorTint";
  const Actors_ActorRecord* actor = LiveActorRecord(actor_id, kOperation);
  if (actor == nullptr) return kNoActorStatus;
  if (tint_out == nullptr) {
    WarnActorRefused(kOperation, "null color");
    return kNoActorStatus;
  }
  *tint_out = ToLiveColor(ActorTint(LoadColors(*actor)));
  return static_cast<int>(AppearanceStatus::kOk);
}

int SetLiveActorTint(int actor_id, const Render_ColorCode* tint) {
  Actors_ActorRecord* actor = LiveActorRecord(actor_id, "SetActorTint");
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
    Core_Warnf("actors",
               "StartActorAppearanceTransition refused: the chain from actor "
               "%d names a missing actor or repeats",
               actor_id);
  }
}
}  // namespace VS::Actors
