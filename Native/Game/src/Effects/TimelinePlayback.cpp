// Hand-maintained (tools/port/owned.csv).
#include "Effects/TimelinePlayback.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Audio/SoundBanks.hpp"
#include "Effects/TimelineCommands.hpp"
#include "Effects/Transforms.hpp"

namespace VS::Effects {
namespace {
constexpr std::uint32_t kParticleDueMask = 0xFF;
constexpr int kParticleVariantShift = 16;
constexpr std::uint32_t kParticleVariantMask = 0xF;
constexpr int kParticleEventShift = 20;
constexpr std::uint32_t kParticleEventMask = 0x3F;
constexpr int kParticleTransparencyShift = 26;

constexpr std::uint32_t kDueFrameMask = 0x1FF;

constexpr int kSoundEffectShift = 9;
constexpr std::uint32_t kSoundEffectMask = 0x1FF;
constexpr int kSoundSourceShift = 18;
constexpr std::uint32_t kSoundSourceMask = 0x3;
constexpr std::uint32_t kSoundSharedBank = 0;
constexpr std::uint32_t kSoundStream = 2;
constexpr int kSoundPositionShift = 20;
constexpr std::uint32_t kSoundPositionMask = 0x3;
constexpr int kSoundTransformShift = 22;
constexpr std::uint32_t kSoundTransformMask = 0x3F;

constexpr int kRendererLowShift = 9;
constexpr std::uint32_t kRendererLowMask = 0x1FF;
constexpr int kRendererIndexShift = 18;
constexpr std::uint32_t kRendererIndexMask = 0x7;
constexpr int kRendererMiddleShift = 13;
constexpr std::uint32_t kRendererMiddleMask = 0x3F00;
constexpr int kRendererHighShift = 16;

constexpr int kVibrationPatternShift = 9;
constexpr std::uint32_t kVibrationPatternMask = 0x1F;
constexpr int kVibrationAudienceShift = 14;
constexpr std::size_t kVibrationOffsetsAt = 4;
constexpr std::size_t kVibrationOffsetBytes = 2;
constexpr int kBitsPerByte = 8;
enum class VibrationAudience : std::uint8_t {
  kEveryone,
  kSourcePlayer,
  kTargetPlayer,
  kActionTargetPlayer,
};

constexpr std::uint32_t kShakeMaskBits = 0xF;
constexpr int kShakeShapeShift = 4;
constexpr std::uint32_t kShakeShapeMask = 0x1F;
constexpr int kShakeAmplitudeShift = 9;
constexpr std::uint32_t kShakeAmplitudeMask = 0x7F;
constexpr int kShakeStepCurveShift = 16;
constexpr int kShakeStepShift = 17;
constexpr std::uint32_t kShakeStepMask = 0x7F;
constexpr int kShakeRandomCurveShift = 24;
constexpr int kShakeRandomShift = 25;

constexpr std::uint8_t kChildCountMask = 0x1F;
constexpr std::size_t kFinalActorSlot = kChildTrackActors;
constexpr std::size_t kFrameSlot = kFinalActorSlot + 1;
constexpr std::size_t kDelaySlot = kFrameSlot + 1;
constexpr std::size_t kActivityBitMask = 31;
// The player is actor 0.
constexpr std::uint8_t kPlayerActor = 0;

[[nodiscard]] constexpr std::int16_t Wrapped(int value) {
  return static_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}

[[nodiscard]] constexpr bool IsPlayer(const CommandEndpoint& endpoint) {
  return endpoint.type == kActorPartEndpoint && endpoint.actor == kPlayerActor;
}

[[nodiscard]] bool VibrationAllowed(VibrationAudience audience,
                                    const VibrationEndpoints& endpoints) {
  switch (audience) {
    case VibrationAudience::kEveryone:
      return true;
    case VibrationAudience::kSourcePlayer:
      return IsPlayer(endpoints.source);
    case VibrationAudience::kTargetPlayer:
      return IsPlayer(endpoints.target);
    case VibrationAudience::kActionTargetPlayer:
      for (const CommandEndpoint& target : endpoints.action_targets) {
        if (IsPlayer(target)) return true;
      }
      return false;
  }
  return false;
}

// Spawns a child at `target` and counts it among the parent's children.
void Spawn(std::size_t target, std::optional<std::uint8_t> actor,
           ChildTrackState& state, TimelinePlaybackHost& host) {
  if (host.SpawnChild(target, actor)) {
    state.child_count =
        static_cast<std::uint8_t>((state.child_count + 1) & kChildCountMask);
  }
}
}  // namespace

std::optional<TimelineTrackKind> TimelineTrackKindOf(std::uint8_t kind) {
  constexpr auto kFirst =
      static_cast<std::uint8_t>(TimelineTrackKind::kParticleEmission);
  constexpr auto kLast =
      static_cast<std::uint8_t>(TimelineTrackKind::kCameraShake);
  // Kind 11 has no track.
  constexpr auto kUnused =
      static_cast<std::uint8_t>(TimelineTrackKind::kBackgroundColour) + 1;
  if (kind < kFirst || kind > kLast || kind == kUnused) return std::nullopt;
  return static_cast<TimelineTrackKind>(kind);
}

ParticleEmission ParticleEmissionOf(std::uint32_t key) {
  return {.event = static_cast<std::uint8_t>((key >> kParticleEventShift) &
                                             kParticleEventMask),
          .transparency =
              static_cast<std::uint8_t>(key >> kParticleTransparencyShift),
          .variant = static_cast<std::uint8_t>((key >> kParticleVariantShift) &
                                               kParticleVariantMask)};
}

ShakeChannels ShakeChannelsOf(std::uint32_t channels) {
  return {.mask = static_cast<std::uint8_t>(channels & kShakeMaskBits),
          .shape = static_cast<std::uint8_t>((channels >> kShakeShapeShift) &
                                             kShakeShapeMask),
          .amplitude = static_cast<std::uint8_t>(
              (channels >> kShakeAmplitudeShift) & kShakeAmplitudeMask),
          .step_uses_curve = ((channels >> kShakeStepCurveShift) & 1U) != 0,
          .step = static_cast<std::uint8_t>((channels >> kShakeStepShift) &
                                            kShakeStepMask),
          .random_uses_curve = ((channels >> kShakeRandomCurveShift) & 1U) != 0,
          .random = static_cast<std::uint8_t>(channels >> kShakeRandomShift)};
}

std::optional<std::uint8_t> EventActorOf(const ChildTrackKey& key,
                                         std::uint8_t slot) {
  if (slot < kChildTrackActors) return key.actors.at(slot);
  switch (slot) {
    case kFinalActorSlot:
      return key.final_actor;
    case kFrameSlot:
      return key.frame;
    case kDelaySlot:
      return key.delay;
    default:
      return std::nullopt;
  }
}

std::optional<std::size_t> VibrationPatternOffset(
    std::span<const std::byte> block, std::uint8_t pattern) {
  const std::size_t entry =
      kVibrationOffsetsAt + (std::size_t{pattern} * kVibrationOffsetBytes);
  if (entry > block.size() || block.size() - entry < kVibrationOffsetBytes) {
    return std::nullopt;
  }
  const auto offset = static_cast<std::int16_t>(
      std::to_integer<std::uint16_t>(block[entry]) |
      (std::to_integer<std::uint16_t>(block[entry + 1]) << kBitsPerByte));
  if (offset < 0 || static_cast<std::size_t>(offset) >= block.size()) {
    return std::nullopt;
  }
  return static_cast<std::size_t>(offset);
}

// decomp: func_800D57FC (src/BATTLE/BATTLE.PRG/5BF94.c:7950)
TrackStep TickParticleEmissionTrack(std::uint32_t key,
                                    std::int16_t timeline_frame,
                                    TrackTimers& timers,
                                    TimelinePlaybackHost& host) {
  if (timeline_frame < static_cast<int>(key & kParticleDueMask)) {
    return TrackStep::kContinue;
  }
  host.SampleCurves(timers.curve_frame);
  host.EmitParticles(ParticleEmissionOf(key));
  timers.curve_frame = Wrapped(timers.curve_frame + 1);
  if (!host.FrameHeld()) timers.key_timer = Wrapped(timers.key_timer - 1);
  if (timers.key_timer != 0) return TrackStep::kContinue;
  timers.curve_frame = 0;
  return TrackStep::kNextKey;
}

// decomp: func_800D5904 (src/BATTLE/BATTLE.PRG/5BF94.c:7974)
TrackStep TickSoundTrack(std::uint32_t key, std::int16_t timeline_frame,
                         std::int32_t stream_handle,
                         TimelinePlaybackHost& host) {
  if (static_cast<int>(key & kDueFrameMask) != timeline_frame) {
    return TrackStep::kContinue;
  }
  const std::uint32_t source = (key >> kSoundSourceShift) & kSoundSourceMask;
  if (source == kSoundStream) {
    if (stream_handle != 0) host.PlayStream(stream_handle);
    return TrackStep::kNextKey;
  }
  const std::int32_t group = source == kSoundSharedBank
                                 ? Audio::kSharedEffectTags
                                 : Audio::kRoomSfxBankEffectTags;
  const auto effect =
      static_cast<std::uint16_t>((key >> kSoundEffectShift) & kSoundEffectMask);
  if (((key >> kSoundPositionShift) & kSoundPositionMask) == 0) {
    host.PlaySfx(group, effect);
    return TrackStep::kNextKey;
  }
  const auto transform = static_cast<std::uint8_t>(
      (key >> kSoundTransformShift) & kSoundTransformMask);
  if (const auto translation = host.TransformTranslation(transform)) {
    host.PlayWorldSfx(group, effect,
                      {.x = Wrapped(translation->x),
                       .y = Wrapped(translation->y),
                       .z = Wrapped(translation->z)});
  }
  return TrackStep::kNextKey;
}

// decomp: func_800D5A98 (src/BATTLE/BATTLE.PRG/5BF94.c:8005)
TrackStep TickChildEffectTrack(const ChildTrackKey& key,
                               std::int16_t timeline_frame,
                               std::span<const ChildTarget> targets,
                               ChildTrackState& state,
                               TimelinePlaybackHost& host) {
  if (timeline_frame < key.frame) return TrackStep::kContinue;
  if (state.index < 0) return TrackStep::kEnd;
  const auto count = static_cast<int>(targets.size());
  if (state.index < count) {
    while (state.delay == 0) {
      const auto target = static_cast<std::size_t>(state.index);
      const ChildTarget& endpoint = targets[target];
      const auto actor = endpoint.type == kActorPartEndpoint
                             ? EventActorOf(key, endpoint.event_actor_slot)
                             : std::optional<std::uint8_t>{key.actors.front()};
      Spawn(target, actor, state, host);
      state.index = Wrapped(state.index + 1);
      if (state.index == count) break;
      state.delay = Wrapped(state.delay + key.delay);
    }
    if (state.delay != 0) state.delay = Wrapped(state.delay - 1);
    return TrackStep::kContinue;
  }
  if (state.index != count) return TrackStep::kEnd;
  if (state.child_count == 0) {
    Spawn(0, key.final_actor, state, host);
    state.index = Wrapped(state.index + 1);
  }
  return TrackStep::kContinue;
}

// decomp: func_800D5D74 (src/BATTLE/BATTLE.PRG/5BF94.c:8069)
TrackStep TickRendererTrack(const RendererKey& key, std::int16_t timeline_frame,
                            TimelinePlaybackHost& host) {
  const std::uint32_t timing = key.timing;
  if (static_cast<int>(timing & kDueFrameMask) != timeline_frame) {
    return TrackStep::kContinue;
  }
  // The parameter's low and middle parts overlap at bit 8 and are added, as
  // in the original.
  const std::uint32_t parameters =
      (((timing >> kRendererLowShift) & kRendererLowMask) +
       ((timing >> kRendererMiddleShift) & kRendererMiddleMask)) |
      (static_cast<std::uint32_t>(key.parameter_high) << kRendererHighShift);
  host.AttachRenderer(static_cast<std::uint8_t>(
                          (timing >> kRendererIndexShift) & kRendererIndexMask),
                      static_cast<std::int32_t>(parameters));
  return TrackStep::kNextKey;
}

// decomp: func_800D5E00 (src/BATTLE/BATTLE.PRG/5BF94.c:8081)
TrackStep TickVibrationTrack(std::uint16_t key, std::int16_t timeline_frame,
                             const VibrationEndpoints& endpoints,
                             TimelinePlaybackHost& host) {
  if (static_cast<int>(key & kDueFrameMask) != timeline_frame) {
    return TrackStep::kContinue;
  }
  const auto audience =
      static_cast<VibrationAudience>(key >> kVibrationAudienceShift);
  if (VibrationAllowed(audience, endpoints)) {
    host.Vibrate(static_cast<std::uint8_t>((key >> kVibrationPatternShift) &
                                           kVibrationPatternMask));
  }
  return TrackStep::kNextKey;
}

// decomp: func_800D6A18 (src/BATTLE/BATTLE.PRG/5BF94.c:8422)
TrackStep TickCameraShakeTrack(std::uint16_t key_timing,
                               std::uint32_t first_key_channels,
                               std::int16_t timeline_frame,
                               std::int16_t& key_timer,
                               TimelinePlaybackHost& host) {
  const auto due = static_cast<int>(key_timing & kDueFrameMask);
  if (timeline_frame < due) return TrackStep::kContinue;
  if (timeline_frame == due) {
    host.ShakeCamera(ShakeChannelsOf(first_key_channels));
  }
  key_timer = Wrapped(key_timer - 1);
  return key_timer != 0 ? TrackStep::kContinue : TrackStep::kNextKey;
}

// decomp: func_800D6AEC (src/BATTLE/BATTLE.PRG/5BF94.c:8442)
void TickEffectTimeline(TimelineProgress& progress, TimelineRunner& runner) {
  for (std::size_t track = 0; track < progress.track_count; ++track) {
    const std::uint32_t bit = 1U << (track & kActivityBitMask);
    if ((progress.active_tracks & bit) == 0) continue;
    const std::uint8_t raw_kind = runner.KindOf(track);
    const auto kind = TimelineTrackKindOf(raw_kind);
    bool running = false;
    if (kind) {
      const bool all_targets = *kind == TimelineTrackKind::kTargetActorColour &&
                               (runner.Flags() & kTimelineHasChildTracks) != 0;
      running = runner.Tick(track, *kind, all_targets);
    } else {
      runner.UnknownKind(track, raw_kind);
    }
    if (!running) progress.active_tracks &= ~bit;
  }
  if ((runner.Flags() & kTimelineFrameHeld) == 0) {
    progress.frame = Wrapped(progress.frame + 1);
  }
}

// decomp: func_800D55A4 (src/BATTLE/BATTLE.PRG/5BF94.c:7885)
LoadedTimeline LoadEffectTimeline(std::uint8_t track_count,
                                  TimelineLoadHost& host) {
  LoadedTimeline loaded{.track_count = track_count};
  bool child_tracks = false;
  for (std::size_t track = 0; track < track_count; ++track) {
    const auto header = host.TrackHeader(track);
    if (!header) {
      loaded.track_count = static_cast<std::uint8_t>(track);
      break;
    }
    if (static_cast<std::uint8_t>(*header) ==
        static_cast<std::uint8_t>(TimelineTrackKind::kChildEffects)) {
      child_tracks = true;
    }
    host.StartTrack(track, {.header = *header});
    loaded.active_tracks |= 1U << (track & kActivityBitMask);
  }
  loaded.flags = child_tracks ? kTimelineHasChildTracks : 0;
  return loaded;
}
}  // namespace VS::Effects
