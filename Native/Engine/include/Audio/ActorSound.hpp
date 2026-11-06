#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>
#include <optional>
#include <span>

#include "Audio/Sfx.hpp"
#include "Audio/SoundBanks.hpp"

// The battle overlay's actor sounds (BATTLE.PRG 146C.c, 40564.c, 44F14.c):
// an actor's sound is panned and attenuated from its projected sound
// position unless its sound override silences it or fixes its pan or volume,
// and a model's sounds come from its SHP's sound bank. The actor records,
// models, gimmicks, room cells and weather are read through ActorSoundHost
// (ActorSoundLive.cpp binds the battle overlay's records).
namespace VS::Audio {
// The channel groups the actor sounds play on.
inline constexpr std::int32_t kActorSoundGroup = 0x7E;
inline constexpr std::int32_t kModelSoundGroup = 0x180;
// An actor sound override of this kind silences the actor.
inline constexpr std::uint8_t kSilencedActorSound = 2;
// A model at or below this height (y grows downward) makes no sound.
inline constexpr std::int16_t kSilentModelHeight = 0xBB8;
// The model sounds picked at random: an impact plays one of three entries
// from 8 and a step one of four entries from 4.
inline constexpr std::int32_t kRandomImpactSound = 8;
inline constexpr std::int32_t kRandomImpactSoundCount = 3;
inline constexpr std::int32_t kRandomStepSound = 4;
inline constexpr std::int32_t kRandomStepSoundCount = 4;
// The model sound variants start at entry 4.
inline constexpr std::int32_t kModelSoundVariantBase = 4;
// Only these battle actor slots play script sounds.
inline constexpr std::int32_t kScriptSoundActorCount = 16;
// The actor whose surface contacts are silent.
inline constexpr std::int32_t kSilentContactActor = 1;
// Actor animations from 0xCA make no surface contact sound for 18 ids.
inline constexpr std::uint8_t kSilentContactAnimationBase = 0xCA;
inline constexpr std::uint8_t kSilentContactAnimationCount = 18;
// A contact on a cell without a surface.
inline constexpr std::int32_t kDefaultSurfaceSound = 0xD;
// The surface sound bank offsets of a soft and a hard landing, and of a
// contact on a wet surface.
inline constexpr std::int32_t kSoftLandingContact = 0x41;
inline constexpr std::int32_t kHardLandingContact = 0x42;
inline constexpr std::int32_t kSoftLandingSoundOffset = 0x10;
inline constexpr std::int32_t kHardLandingSoundOffset = 0x18;
inline constexpr std::int32_t kWetSurfaceSoundOffset = 8;
// Weather of these kinds at this intensity or more wets the surfaces.
inline constexpr std::int32_t kFirstWetWeather = 1;
inline constexpr std::int32_t kLastWetWeather = 2;
inline constexpr std::int32_t kWetWeatherIntensity = 6;
// The bits of a gimmick's object id and of a cell's surface kind.
inline constexpr std::uint8_t kGimmickObjectMask = 0x7;
inline constexpr std::uint8_t kSurfaceKindMask = 0x1F;

// An actor's sound override (D_800F4538_t.unk17E4): a kind, and a pan and
// volume that replace the projected ones when nonzero.
struct ActorSoundOverride {
  std::uint8_t kind = 0;
  std::uint8_t pan = 0;
  std::uint8_t volume = 0;

  friend bool operator==(const ActorSoundOverride&,
                         const ActorSoundOverride&) = default;
};

// The model state that leaves an actor's sound position stale: the actor is
// projected again unless all of it is clear and its sound is enabled.
struct ActorSoundMotion {
  std::uint8_t action_animation_state = 0;
  bool preview_model = false;
  bool sound_enabled = true;
  std::uint8_t movement_mode = 0;
  std::uint8_t locomotion_state = 0;
  bool motion_locked = false;

  friend bool operator==(const ActorSoundMotion&,
                         const ActorSoundMotion&) = default;
};

struct ActorSoundRecord {
  ActorSoundOverride sound_override;
  ActorSoundMotion motion;

  friend bool operator==(const ActorSoundRecord&,
                         const ActorSoundRecord&) = default;
};

// A model instance making a sound: its height, the actor slot its mix comes
// from and its SHP's sound bank (kNone without an SHP).
struct ActorModelSound {
  std::int16_t height = 0;
  std::uint8_t actor = 0;
  BankBuffer bank = BankBuffer::kNone;

  friend bool operator==(const ActorModelSound&,
                         const ActorModelSound&) = default;
};

// An actor touching its surface: its animation, whether it stands on a
// gimmick, its tile, its contact layer (0 for its tile's layer) and model.
struct ActorContactSource {
  std::uint8_t animation_id = 0;
  bool on_gimmick = false;
  std::uint8_t tile_x = 0;
  std::uint8_t tile_layer = 0;
  std::uint8_t tile_z = 0;
  std::uint8_t contact_layer = 0;
  ActorModelSound model;
};

struct ActorSoundPairIds {
  std::int32_t first = 0;
  // No second sound when 0.
  std::int32_t second = 0;
};

struct RoomWeather {
  std::int32_t kind = 0;
  std::int32_t intensity = 0;
};

enum class ActorSoundStatus : std::uint8_t {
  kDone,
  kNoActor,
  kNoModel,
  kNoSoundBank,
  kNoGimmick,
  kNoSurfaceSound,
};

class ActorSoundHost {
 public:
  ActorSoundHost() = default;
  ActorSoundHost(const ActorSoundHost&) = delete;
  ActorSoundHost& operator=(const ActorSoundHost&) = delete;
  ActorSoundHost(ActorSoundHost&&) = delete;
  ActorSoundHost& operator=(ActorSoundHost&&) = delete;
  virtual ~ActorSoundHost() = default;

  // Actor records (D_800F4538): empty past them or for an empty record.
  virtual std::optional<ActorSoundRecord> ActorSound(std::int32_t actor) = 0;
  virtual std::optional<ActorModelSound> ActorModel(std::int32_t actor) = 0;
  // func_800A1280: projects the actor's sound position to the screen.
  virtual void ProjectSoundPosition(std::int32_t actor) = 0;
  // The screen position (x, y) and depth (z) projected last.
  virtual SfxPoint SoundPosition(std::int32_t actor) = 0;
  // Battle actors (vs_battle_actors).
  virtual bool ScriptActorPresent(std::int32_t actor) = 0;
  virtual std::optional<ActorModelSound> ScriptActorModel(
      std::int32_t actor) = 0;
  // The object id of the top idle gimmick on the tile (func_800A91DC);
  // empty when its slot is empty.
  virtual std::optional<std::uint8_t> TopGimmickObject(std::uint8_t tile_x,
                                                       std::uint8_t tile_z) = 0;
  // The surface kind of the room cell; empty without one.
  virtual std::optional<std::uint8_t> TileSurfaceKind(std::uint8_t tile_x,
                                                      std::uint8_t tile_z,
                                                      std::uint8_t layer) = 0;
  // The surface sounds of the gimmick objects (D_800E91A4) and of the cell
  // surface kinds (D_800E9278).
  virtual std::span<const std::uint8_t> ObjectSurfaceSounds() = 0;
  virtual std::span<const std::uint8_t> TileSurfaceSounds() = 0;
  // func_8008EB04.
  virtual RoomWeather Weather() = 0;
  // vs_main_getRand and rand.
  virtual std::int32_t BoundedRandom(std::int32_t bound) = 0;
  virtual std::int32_t Random() = 0;
  // vs_main_playSfx, func_800461CC and vs_main_panSfx.
  virtual void PlaySfx(std::int32_t group, std::int32_t effect_id,
                       const SfxMix& mix) = 0;
  virtual void PlayBankSfx(std::int32_t group, BankBuffer bank,
                           std::int32_t index, const SfxMix& mix) = 0;
  virtual void PlayWorldSfx(std::int32_t group, std::int32_t effect_id,
                            const SfxPoint& point) = 0;
};

// func_8006CD60: one of the bank's three impact sounds from entry 8.
void PlayRandomActorImpactSound(ActorSoundHost& host, BankBuffer bank,
                                const SfxMix& mix);
// func_800AD62C: the actor's sound pan and volume; empty without its record.
std::optional<SfxMix> ActorSoundPanAndVolume(ActorSoundHost& host,
                                             std::int32_t actor);
// func_800AE4FC: plays the model's sound `sound_index` at its actor's mix.
ActorSoundStatus PlayActorModelSound(ActorSoundHost& host,
                                     const ActorModelSound& model,
                                     std::int32_t sound_index);
// func_800AE68C: plays the actor model's sound variant.
ActorSoundStatus PlayActorModelSoundVariant(ActorSoundHost& host,
                                            std::int32_t actor,
                                            std::int32_t variant);
// func_800AE6C0: plays one or two sounds at the actor's mix.
ActorSoundStatus PlayActorSoundPair(ActorSoundHost& host, std::uint8_t actor,
                                    const ActorSoundPairIds& sounds);
// func_800AE7D8: plays one or two sounds at the gimmick's position.
void PlayGimmickSoundPair(ActorSoundHost& host, const SfxPoint& position,
                          const ActorSoundPairIds& sounds);
// func_800AE828: plays the sound of the actor's surface.
ActorSoundStatus PlayActorSurfaceContactSound(ActorSoundHost& host,
                                              std::int32_t actor_index,
                                              const ActorContactSource& actor,
                                              std::int32_t contact_code);
// func_8007C8A4: plays the battle actor model's sound.
ActorSoundStatus PlayActorScriptSound(ActorSoundHost& host, std::int32_t actor,
                                      std::int32_t sound_index);
}  // namespace VS::Audio
