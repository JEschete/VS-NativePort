// Hand-maintained (tools/port/owned.csv).
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Audio/ActorSound.hpp"
#include "Audio/Sfx.hpp"
#include "Audio/SoundBanks.hpp"

namespace VS::Audio {
namespace {
constexpr unsigned kScreenYShift = 16;

// The projected position as the word the original reads from it: x in the
// low halfword, y in the high halfword.
std::int32_t ScreenXy(const SfxPoint& position) {
  const auto x = static_cast<std::uint16_t>(position.x);
  const auto y = static_cast<std::uint16_t>(position.y);
  return static_cast<std::int32_t>(
      static_cast<std::uint32_t>(x) |
      (static_cast<std::uint32_t>(y) << kScreenYShift));
}

bool PositionStale(const ActorSoundMotion& motion) {
  return motion.action_animation_state != 0 || motion.preview_model ||
         !motion.sound_enabled || motion.movement_mode != 0 ||
         motion.locomotion_state != 0 || motion.motion_locked;
}

// The original packs vs_main_computeSfxPan's pan into the high halfword and
// its volume into the low one and splits them again; both stay within
// 0..0xFF, so the split returns them unchanged.
SfxMix MixFor(ActorSoundHost& host, std::int32_t actor,
              const ActorSoundRecord& record) {
  if (record.sound_override.kind == kSilencedActorSound) {
    return {.pan = kCenteredSfxPan, .volume = 0};
  }
  if (PositionStale(record.motion)) host.ProjectSoundPosition(actor);
  const SfxPoint position = host.SoundPosition(actor);
  SfxMix mix =
      SfxPanAndVolume({.screen_xy = ScreenXy(position), .depth = position.z});
  if (record.sound_override.pan != 0) mix.pan = record.sound_override.pan;
  if (record.sound_override.volume != 0) {
    mix.volume = record.sound_override.volume;
  }
  return mix;
}

// The surface sound of the actor's tile, before the contact offsets, or the
// refusal that leaves it unknown.
struct SurfaceLookup {
  ActorSoundStatus status = ActorSoundStatus::kDone;
  std::int32_t sound = kDefaultSurfaceSound;
};

SurfaceLookup SoundFrom(std::span<const std::uint8_t> sounds,
                        std::size_t index) {
  if (index >= sounds.size()) {
    return {.status = ActorSoundStatus::kNoSurfaceSound};
  }
  return {.sound = sounds[index]};
}

SurfaceLookup SurfaceSound(ActorSoundHost& host,
                           const ActorContactSource& actor) {
  if (actor.on_gimmick) {
    const std::optional<std::uint8_t> object =
        host.TopGimmickObject(actor.tile_x, actor.tile_z);
    if (!object) return {.status = ActorSoundStatus::kNoGimmick};
    return SoundFrom(host.ObjectSurfaceSounds(), *object & kGimmickObjectMask);
  }
  const std::uint8_t layer =
      actor.contact_layer != 0 ? actor.contact_layer : actor.tile_layer;
  const std::optional<std::uint8_t> surface =
      host.TileSurfaceKind(actor.tile_x, actor.tile_z, layer);
  if (!surface) return {};
  return SoundFrom(host.TileSurfaceSounds(), *surface & kSurfaceKindMask);
}

bool WetWeather(const RoomWeather& weather) {
  return weather.intensity >= kWetWeatherIntensity &&
         weather.kind >= kFirstWetWeather && weather.kind <= kLastWetWeather;
}
}  // namespace

// decomp: BATTLE.PRG func_8006CD60 (src/BATTLE/BATTLE.PRG/146C.c:2144)
void PlayRandomActorImpactSound(ActorSoundHost& host, BankBuffer bank,
                                const SfxMix& mix) {
  host.PlayBankSfx(
      kActorSoundGroup, bank,
      host.BoundedRandom(kRandomImpactSoundCount) + kRandomImpactSound, mix);
}

// decomp: BATTLE.PRG func_800AD62C (src/BATTLE/BATTLE.PRG/40564.c:2789)
std::optional<SfxMix> ActorSoundPanAndVolume(ActorSoundHost& host,
                                             std::int32_t actor) {
  const std::optional<ActorSoundRecord> record = host.ActorSound(actor);
  if (!record) return std::nullopt;
  return MixFor(host, actor, *record);
}

// decomp: BATTLE.PRG func_800AE4FC (src/BATTLE/BATTLE.PRG/44F14.c:637)
ActorSoundStatus PlayActorModelSound(ActorSoundHost& host,
                                     const ActorModelSound& model,
                                     std::int32_t sound_index) {
  if (model.height >= kSilentModelHeight) return ActorSoundStatus::kDone;
  const std::optional<SfxMix> mix = ActorSoundPanAndVolume(host, model.actor);
  if (!mix) return ActorSoundStatus::kNoActor;
  if (mix->volume == 0) return ActorSoundStatus::kDone;
  if (model.bank == BankBuffer::kNone) return ActorSoundStatus::kNoSoundBank;
  if (sound_index == kRandomImpactSound) {
    PlayRandomActorImpactSound(host, model.bank, *mix);
  } else if (sound_index == kRandomStepSound) {
    host.PlayBankSfx(kActorSoundGroup, model.bank,
                     host.Random() % kRandomStepSoundCount + kRandomStepSound,
                     *mix);
  } else {
    host.PlayBankSfx(kModelSoundGroup, model.bank, sound_index, *mix);
  }
  return ActorSoundStatus::kDone;
}

// decomp: BATTLE.PRG func_800AE68C (src/BATTLE/BATTLE.PRG/44F14.c:685)
ActorSoundStatus PlayActorModelSoundVariant(ActorSoundHost& host,
                                            std::int32_t actor,
                                            std::int32_t variant) {
  const std::optional<ActorModelSound> model = host.ActorModel(actor);
  if (!model) return ActorSoundStatus::kNoActor;
  return PlayActorModelSound(host, *model, variant + kModelSoundVariantBase);
}

// decomp: BATTLE.PRG func_800AE6C0 (src/BATTLE/BATTLE.PRG/44F14.c:690)
ActorSoundStatus PlayActorSoundPair(ActorSoundHost& host, std::uint8_t actor,
                                    const ActorSoundPairIds& sounds) {
  const std::optional<SfxMix> mix = ActorSoundPanAndVolume(host, actor);
  if (!mix) return ActorSoundStatus::kNoActor;
  if (mix->volume != 0) {
    host.PlaySfx(kActorSoundGroup, sounds.first, *mix);
    if (sounds.second != 0) host.PlaySfx(kActorSoundGroup, sounds.second, *mix);
  }
  return ActorSoundStatus::kDone;
}

// decomp: BATTLE.PRG func_800AE7D8 (src/BATTLE/BATTLE.PRG/44F14.c:728)
void PlayGimmickSoundPair(ActorSoundHost& host, const SfxPoint& position,
                          const ActorSoundPairIds& sounds) {
  host.PlayWorldSfx(kActorSoundGroup, sounds.first, position);
  if (sounds.second != 0) {
    host.PlayWorldSfx(kActorSoundGroup, sounds.second, position);
  }
}

// decomp: BATTLE.PRG func_800AE828 (src/BATTLE/BATTLE.PRG/44F14.c:739)
ActorSoundStatus PlayActorSurfaceContactSound(ActorSoundHost& host,
                                              std::int32_t actor_index,
                                              const ActorContactSource& actor,
                                              std::int32_t contact_code) {
  // The original compares the byte difference unsigned (sltiu).
  if (static_cast<std::uint8_t>(actor.animation_id -
                                kSilentContactAnimationBase) <
      kSilentContactAnimationCount) {
    return ActorSoundStatus::kDone;
  }
  if (actor_index == kSilentContactActor) return ActorSoundStatus::kDone;
  const SurfaceLookup surface = SurfaceSound(host, actor);
  if (surface.status != ActorSoundStatus::kDone) return surface.status;
  std::int32_t sound = surface.sound;
  if (contact_code == kSoftLandingContact) {
    sound += kSoftLandingSoundOffset;
  } else if (contact_code == kHardLandingContact) {
    sound += kHardLandingSoundOffset;
  } else if (WetWeather(host.Weather())) {
    sound += kWetSurfaceSoundOffset;
  }
  return PlayActorModelSound(host, actor.model, sound);
}

// decomp: BATTLE.PRG func_8007C8A4 (src/BATTLE/BATTLE.PRG/146C.c:8917)
ActorSoundStatus PlayActorScriptSound(ActorSoundHost& host, std::int32_t actor,
                                      std::int32_t sound_index) {
  if (actor < 0) return ActorSoundStatus::kNoActor;
  if (actor >= kScriptSoundActorCount || !host.ScriptActorPresent(actor)) {
    return ActorSoundStatus::kDone;
  }
  const std::optional<ActorModelSound> model = host.ScriptActorModel(actor);
  if (!model) return ActorSoundStatus::kNoModel;
  return PlayActorModelSound(host, *model, sound_index);
}
}  // namespace VS::Audio
