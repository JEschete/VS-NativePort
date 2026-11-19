#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>
#include <functional>
#include <optional>
#include <span>

#include "Effects/TimelineCommands.hpp"

// Typed effect timeline keys and colour tracks (BATTLE.PRG 5BF94.c:7920-8211).
// A track steps through its keys as the timeline reaches them; the particle
// emission and camera shake tracks set up per-key timing as they do. The
// colour tracks shift an actor's, the scene's or the background's colours
// when their key is due. TimelineTracksLive.hpp runs them on the effect
// instance records.
namespace VS::Effects {
// LiveTimelineTrackHeader.kind values whose keys set up timing.
inline constexpr std::uint8_t kParticleEmissionTrack = 1;
inline constexpr std::uint8_t kCameraShakeTrack = 13;

// The per-key timing a track's kind sets up for its new key.
enum class TimelineKeySetup : std::uint8_t {
  kNone,
  kParticleEmission,
  kCameraShake,
};

// The key a track moved to and the setup its kind needs.
struct TimelineKeyAdvance {
  std::int16_t key_index{};
  TimelineKeySetup setup{};

  friend bool operator==(const TimelineKeyAdvance&,
                         const TimelineKeyAdvance&) = default;
};

// decomp: func_800D5780 (src/BATTLE/BATTLE.PRG/5BF94.c:7933): nullopt once the
// track has no more keys.
[[nodiscard]] std::optional<TimelineKeyAdvance> AdvanceTimelineKey(
    std::int16_t key_index, std::uint8_t key_count, std::uint8_t kind);

// A particle emission key's (func_800D57FC_t) timing: the frames the key lasts
// (bits 8-15) and its emission parameter (bits 20-25).
struct ParticleKeyTiming {
  std::int16_t timer{};
  std::int32_t parameter{};

  friend bool operator==(const ParticleKeyTiming&,
                         const ParticleKeyTiming&) = default;
};

// decomp: func_800D5700 (src/BATTLE/BATTLE.PRG/5BF94.c:7920)
[[nodiscard]] ParticleKeyTiming ParticleKeyTimingOf(std::uint32_t key);

// The frames a segmented effect curve lasts (LiveSegmentedEffectCurveDuration).
using CurveDuration = std::function<auto(std::uint8_t curve)->std::int32_t>;

// decomp: func_800D5738 (src/BATTLE/BATTLE.PRG/5BF94.c:7927): a camera shake
// key lasts as long as the curve in bits 4-8 of its second word; the timer
// keeps the duration's low 16 bits.
[[nodiscard]] std::int16_t ShakeKeyTimer(std::uint32_t parameters,
                                         const CurveDuration& duration);

// A colour track key (func_800D6310_t): a colour shift due on `frame`. Each
// channel is centred on kNeutralColourChannel.
struct ColourKey {
  std::int16_t frame{};
  std::int16_t duration{};
  std::uint8_t red{};
  std::uint8_t green{};
  std::uint8_t blue{};
  std::uint8_t mode{};
};
inline constexpr std::uint8_t kNeutralColourChannel = 0x80;

// A colour shift as the palette animator takes it: signed channel offsets.
struct ColourShift {
  std::uint8_t mode{};
  std::int16_t duration{};
  std::int16_t red{};
  std::int16_t green{};
  std::int16_t blue{};

  friend bool operator==(const ColourShift&, const ColourShift&) = default;
};

[[nodiscard]] ColourShift ColourShiftOf(const ColourKey& key);

// What the colour tracks shift and the track they advance.
class ColourTrackHost {
 public:
  virtual ~ColourTrackHost() = default;
  // Actors::StateFlag::kVisible in the actor's state flags.
  [[nodiscard]] virtual bool ActorVisible(std::uint8_t actor) = 0;
  // Render::TransformLiveActorColors.
  virtual void ShiftActorColours(std::uint8_t actor,
                                 const ColourShift& shift) = 0;
  // Render::TransformLiveSceneColors.
  virtual void ShiftSceneColours(const ColourShift& shift) = 0;
  // Render::TransformLiveBackgroundColors.
  virtual void ShiftBackgroundColours(const ColourShift& shift) = 0;
  // LiveAdvanceEffectTimelineKey: false once the track has no more keys.
  [[nodiscard]] virtual bool AdvanceKey() = 0;

 protected:
  ColourTrackHost() = default;
  ColourTrackHost(const ColourTrackHost&) = default;
  ColourTrackHost& operator=(const ColourTrackHost&) = default;
  ColourTrackHost(ColourTrackHost&&) = default;
  ColourTrackHost& operator=(ColourTrackHost&&) = default;
};

// Each colour track waits (true) until the timeline reaches its key's frame,
// then shifts the colours it names and advances the track: false once the
// track has no more keys. Only visible actors are shifted.

// decomp: func_800D5F8C (src/BATTLE/BATTLE.PRG/5BF94.c:8128): the emitter
// actor's colours.
[[nodiscard]] bool TickSourceActorColourTrack(const ColourKey& key,
                                              std::int16_t timeline_frame,
                                              const CommandEndpoint& emit_from,
                                              ColourTrackHost& host);
// decomp: func_800D6048 (src/BATTLE/BATTLE.PRG/5BF94.c:8144): every action
// target actor's colours when `all_targets`, otherwise the receiver's.
[[nodiscard]] bool TickTargetActorColourTrack(
    const ColourKey& key, std::int16_t timeline_frame,
    std::span<const CommandEndpoint> action_targets,
    const CommandEndpoint& emit_to, bool all_targets, ColourTrackHost& host);
// decomp: func_800D61AC (src/BATTLE/BATTLE.PRG/5BF94.c:8172): the colours of
// the action's other actors.
[[nodiscard]] bool TickOtherActorColourTrack(
    const ColourKey& key, std::int16_t timeline_frame,
    std::span<const std::uint8_t> other_actors, ColourTrackHost& host);
// decomp: func_800D6298 (src/BATTLE/BATTLE.PRG/5BF94.c:8189)
[[nodiscard]] bool TickSceneColourTrack(const ColourKey& key,
                                        std::int16_t timeline_frame,
                                        ColourTrackHost& host);
// decomp: func_800D6310 (src/BATTLE/BATTLE.PRG/5BF94.c:8201)
[[nodiscard]] bool TickBackgroundColourTrack(const ColourKey& key,
                                             std::int16_t timeline_frame,
                                             ColourTrackHost& host);
}  // namespace VS::Effects
