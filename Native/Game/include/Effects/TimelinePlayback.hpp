#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Effects/TimelineCommands.hpp"

// Typed effect timeline playback (BATTLE.PRG 5BF94.c:7885-8460). An effect's
// timeline holds up to 32 active tracks; each frame the dispatcher ticks every
// active track by kind and retires those that finish, then moves the timeline
// to its next frame unless the frame is held. The particle, sound, renderer,
// vibration and camera shake tracks act when the timeline reaches their key;
// the child effect track spawns one child per action target, then a final
// child once the others finish. TimelinePlaybackLive.hpp runs them on the
// effect instance records.
namespace VS::Effects {
// Effects_TimelineTrackHeader.kind.
enum class TimelineTrackKind : std::uint8_t {
  kParticleEmission = 1,
  kSound = 2,
  kChildEffects = 3,
  kRenderer = 4,
  kVibration = 5,
  kSourceActorColour = 6,
  kTargetActorColour = 7,
  kOtherActorColour = 8,
  kSceneColour = 9,
  kBackgroundColour = 10,
  kActionControl = 12,
  kCameraShake = 13,
};

// The kind a track header names; nullopt for the kinds the dispatcher has no
// tick for (0, 11 and from 14).
[[nodiscard]] std::optional<TimelineTrackKind> TimelineTrackKindOf(
    std::uint8_t kind);

// Effects_EffectTimeline.flags: the timeline has child effect tracks, so the
// target colour tracks shift every action target.
inline constexpr std::uint8_t kTimelineHasChildTracks = 0x2;

// What a track tick asks of its track.
enum class TrackStep : std::uint8_t {
  kContinue,  // keep the current key: the tick returns 1
  kNextKey,   // advance to the next key: the tick returns the advance's result
  kEnd,       // the track has finished: the tick returns 0
};

// A particle track's frames sampled for the current key and the frames the
// key has left.
struct TrackTimers {
  std::int16_t curve_frame{};
  std::int16_t key_timer{};

  friend bool operator==(const TrackTimers&, const TrackTimers&) = default;
};

// Effects_ParticleTrackKey's emission: the particle event (bits 20-25), its
// transparency override (26-31) and variant (16-19).
struct ParticleEmission {
  std::uint8_t event{};
  std::uint8_t transparency{};
  std::uint8_t variant{};

  friend bool operator==(const ParticleEmission&,
                         const ParticleEmission&) = default;
};

[[nodiscard]] ParticleEmission ParticleEmissionOf(std::uint32_t key);

// A transform's world translation.
struct Translation {
  std::int32_t x{};
  std::int32_t y{};
  std::int32_t z{};
};

// Where a positioned sound plays: the low halves of the translation.
struct SoundPoint {
  std::int16_t x{};
  std::int16_t y{};
  std::int16_t z{};

  friend bool operator==(const SoundPoint&, const SoundPoint&) = default;
};

// Effects_RendererTrackKey: the due frame (bits 0-8), the parameter's low
// byte (9-17), the renderer (18-20) and the parameter's next six bits (21-26)
// in `timing`; the parameter's high half follows.
struct RendererKey {
  std::uint32_t timing{};
  std::uint16_t parameter_high{};
};

// LiveStartCameraShakeChannels' arguments.
struct ShakeChannels {
  std::uint8_t mask{};
  std::uint8_t shape{};
  std::uint8_t amplitude{};
  bool step_uses_curve{};
  std::uint8_t step{};
  bool random_uses_curve{};
  std::uint8_t random{};

  friend bool operator==(const ShakeChannels&, const ShakeChannels&) = default;
};

// Effects_ShakeChannelKey's second word.
[[nodiscard]] ShakeChannels ShakeChannelsOf(std::uint32_t channels);

// Effects_ChildTrackKey: the child's actor for each event actor slot, the
// final child's actor, the frame the children start and the frames between
// them.
inline constexpr std::size_t kChildTrackActors = 6;
struct ChildTrackKey {
  std::array<std::uint8_t, kChildTrackActors> actors{};
  std::uint8_t final_actor{};
  std::uint8_t frame{};
  std::uint8_t delay{};
};

// The key byte an action target's event actor slot names: the six actors,
// then the final actor, frame and delay; nullopt past the key (refused: the
// original reads the bytes after it).
[[nodiscard]] std::optional<std::uint8_t> EventActorOf(const ChildTrackKey& key,
                                                       std::uint8_t slot);

// An action target as the child effect track reads it.
struct ChildTarget {
  std::uint16_t type{};
  std::uint8_t event_actor_slot{};
};

// A child effect track's frames to the next child, the next target and the
// parent's children still running (Effects_EffectInstance.childCount, five
// bits).
struct ChildTrackState {
  std::int16_t delay{};
  std::int16_t index{};
  std::uint8_t child_count{};

  friend bool operator==(const ChildTrackState&,
                         const ChildTrackState&) = default;
};

// The endpoints a vibration key may require to be the player.
struct VibrationEndpoints {
  CommandEndpoint source{};
  CommandEndpoint target{};
  std::span<const CommandEndpoint> action_targets{};
};

// The offset of vibration pattern `pattern` in the package's block 11: a
// count word, then each pattern's signed offset from the block. nullopt
// (refused) when the entry or the pattern lies outside the block.
[[nodiscard]] std::optional<std::size_t> VibrationPatternOffset(
    std::span<const std::byte> block, std::uint8_t pattern);

// What the timeline tracks play.
class TimelinePlaybackHost {
 public:
  virtual ~TimelinePlaybackHost() = default;
  // Effects_EffectTimeline.flags has kTimelineFrameHeld.
  [[nodiscard]] virtual bool FrameHeld() = 0;
  // LiveSampleAllEffectCurves.
  virtual void SampleCurves(std::int16_t curve_frame) = 0;
  // Effects_EmitParticleEvent, without a parent plugin.
  virtual void EmitParticles(const ParticleEmission& emission) = 0;
  // LivePlayCutsceneStreamSlotCentered.
  virtual void PlayStream(std::int32_t handle) = 0;
  // LivePlaySfxCentered.
  virtual void PlaySfx(std::int32_t group, std::uint16_t effect) = 0;
  // The world translation of the instance's transform; nullopt (refused)
  // past the instance's transforms.
  [[nodiscard]] virtual std::optional<Translation> TransformTranslation(
      std::uint8_t transform) = 0;
  // LivePlayWorldSfx.
  virtual void PlayWorldSfx(std::int32_t group, std::uint16_t effect,
                            const SoundPoint& point) = 0;
  // Effects_AttachEffectRenderer with Effects_effectRenderers[renderer].
  virtual void AttachRenderer(std::uint8_t renderer,
                              std::int32_t parameters) = 0;
  // StartLiveVibrationPattern with the package's vibration pattern.
  virtual void Vibrate(std::uint8_t pattern) = 0;
  // LiveStartCameraShakeChannels.
  virtual void ShakeCamera(const ShakeChannels& channels) = 0;
  // Spawns a child effect at action target `target` with `actor` in its
  // second register (left unset when nullopt); false (refused) when no child
  // was created.
  [[nodiscard]] virtual bool SpawnChild(std::size_t target,
                                        std::optional<std::uint8_t> actor) = 0;

 protected:
  TimelinePlaybackHost() = default;
  TimelinePlaybackHost(const TimelinePlaybackHost&) = default;
  TimelinePlaybackHost& operator=(const TimelinePlaybackHost&) = default;
  TimelinePlaybackHost(TimelinePlaybackHost&&) = default;
  TimelinePlaybackHost& operator=(TimelinePlaybackHost&&) = default;
};

// decomp: func_800D57FC (src/BATTLE/BATTLE.PRG/5BF94.c:7950): from its due
// frame (bits 0-7) on, samples the curves and emits the key's particles each
// frame, counting down the key's frames unless the timeline frame is held.
[[nodiscard]] TrackStep TickParticleEmissionTrack(std::uint32_t key,
                                                  std::int16_t timeline_frame,
                                                  TrackTimers& timers,
                                                  TimelinePlaybackHost& host);

// decomp: func_800D5904 (src/BATTLE/BATTLE.PRG/5BF94.c:7974): on its due frame
// (bits 0-8) plays the effect (9-17) from the shared (bits 18-19 zero) or room
// sound banks, centred (20-21 zero) or at a transform (22-27); source 2 plays
// the package's stream instead.
[[nodiscard]] TrackStep TickSoundTrack(std::uint32_t key,
                                       std::int16_t timeline_frame,
                                       std::int32_t stream_handle,
                                       TimelinePlaybackHost& host);

// decomp: func_800D5A98 (src/BATTLE/BATTLE.PRG/5BF94.c:8005): from the key's
// frame on, spawns a child at each action target, `delay` frames apart, then
// a final child at the first target once the parent has no children running.
// A negative index (never set by the original) ends the track.
[[nodiscard]] TrackStep TickChildEffectTrack(
    const ChildTrackKey& key, std::int16_t timeline_frame,
    std::span<const ChildTarget> targets, ChildTrackState& state,
    TimelinePlaybackHost& host);

// decomp: func_800D5D74 (src/BATTLE/BATTLE.PRG/5BF94.c:8069): attaches the
// key's renderer on its due frame.
[[nodiscard]] TrackStep TickRendererTrack(const RendererKey& key,
                                          std::int16_t timeline_frame,
                                          TimelinePlaybackHost& host);

// decomp: func_800D5E00 (src/BATTLE/BATTLE.PRG/5BF94.c:8081): on its due frame
// (bits 0-8) starts the vibration pattern (9-13) for everyone (bits 14-15
// zero) or only when the source (1), the target (2) or an action target (3)
// is the player.
[[nodiscard]] TrackStep TickVibrationTrack(std::uint16_t key,
                                           std::int16_t timeline_frame,
                                           const VibrationEndpoints& endpoints,
                                           TimelinePlaybackHost& host);

// decomp: func_800D6A18 (src/BATTLE/BATTLE.PRG/5BF94.c:8422): from its due
// frame (bits 0-8 of the key's first halfword) on, counts down the key's
// frames; on the due frame it starts the shake channels of the track's first
// key, whichever key is current, as the original does.
[[nodiscard]] TrackStep TickCameraShakeTrack(std::uint16_t key_timing,
                                             std::uint32_t first_key_channels,
                                             std::int16_t timeline_frame,
                                             std::int16_t& key_timer,
                                             TimelinePlaybackHost& host);

// The dispatcher's view of Effects_EffectTimeline.
struct TimelineProgress {
  std::uint8_t track_count{};
  std::int16_t frame{};
  std::uint32_t active_tracks{};

  friend bool operator==(const TimelineProgress&,
                         const TimelineProgress&) = default;
};

// The timeline's tracks as the dispatcher ticks them.
class TimelineRunner {
 public:
  virtual ~TimelineRunner() = default;
  // Effects_EffectTimeline.flags, which a track tick may change.
  [[nodiscard]] virtual std::uint8_t Flags() = 0;
  // Effects_TimelineTrackHeader.kind of track `track`.
  [[nodiscard]] virtual std::uint8_t KindOf(std::size_t track) = 0;
  // Ticks track `track`; false once it has finished. `all_targets` is the
  // target colour track's argument.
  [[nodiscard]] virtual bool Tick(std::size_t track, TimelineTrackKind kind,
                                  bool all_targets) = 0;
  // A track of a kind without a tick (refused: the original reuses the
  // previous track's result); the track is retired.
  virtual void UnknownKind(std::size_t track, std::uint8_t kind) = 0;

 protected:
  TimelineRunner() = default;
  TimelineRunner(const TimelineRunner&) = default;
  TimelineRunner& operator=(const TimelineRunner&) = default;
  TimelineRunner(TimelineRunner&&) = default;
  TimelineRunner& operator=(TimelineRunner&&) = default;
};

// decomp: func_800D6AEC (src/BATTLE/BATTLE.PRG/5BF94.c:8442): ticks each
// active track and retires those that finish; tracks from the 33rd share the
// activity bits of the first 32, as in the original.
void TickEffectTimeline(TimelineProgress& progress, TimelineRunner& runner);

// A track as the timeline load starts it, before its first key.
struct TimelineTrackStart {
  std::uint16_t header{};
  std::int16_t key_index{-1};
  TrackTimers timers{};
  std::int32_t key_parameter{};
};

// The loaded timeline's tracks, flags and active tracks.
struct LoadedTimeline {
  std::uint8_t track_count{};
  std::uint8_t flags{};
  std::uint32_t active_tracks{};

  friend bool operator==(const LoadedTimeline&,
                         const LoadedTimeline&) = default;
};

// The timeline section's tracks as the load reads them.
class TimelineLoadHost {
 public:
  virtual ~TimelineLoadHost() = default;
  // Track `track`'s header (kind in the low byte, key count in the high);
  // nullopt (refused) when its key data is missing.
  [[nodiscard]] virtual std::optional<std::uint16_t> TrackHeader(
      std::size_t track) = 0;
  // Starts track `track` and advances it to its first key.
  virtual void StartTrack(std::size_t track,
                          const TimelineTrackStart& start) = 0;

 protected:
  TimelineLoadHost() = default;
  TimelineLoadHost(const TimelineLoadHost&) = default;
  TimelineLoadHost& operator=(const TimelineLoadHost&) = default;
  TimelineLoadHost(TimelineLoadHost&&) = default;
  TimelineLoadHost& operator=(TimelineLoadHost&&) = default;
};

// decomp: func_800D55A4 (src/BATTLE/BATTLE.PRG/5BF94.c:7885): starts the
// section's tracks, each active; a track whose data is missing ends the
// timeline there.
[[nodiscard]] LoadedTimeline LoadEffectTimeline(std::uint8_t track_count,
                                                TimelineLoadHost& host);
}  // namespace VS::Effects
