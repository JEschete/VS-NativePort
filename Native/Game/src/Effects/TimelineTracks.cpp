// Hand-maintained (tools/port/owned.csv).
#include "Effects/TimelineTracks.hpp"

#include <cstdint>
#include <optional>
#include <span>

#include "Effects/TimelineCommands.hpp"
#include "Effects/Transforms.hpp"

namespace VS::Effects {
namespace {
constexpr int kParticleTimerShift = 8;
constexpr std::uint32_t kParticleTimerMask = 0xFF;
constexpr int kParticleParameterShift = 20;
constexpr std::uint32_t kParticleParameterMask = 0x3F;
constexpr int kShakeCurveShift = 4;
constexpr std::uint32_t kShakeCurveMask = 0x1F;

[[nodiscard]] constexpr TimelineKeySetup SetupOf(std::uint8_t kind) {
  switch (kind) {
    case kParticleEmissionTrack:
      return TimelineKeySetup::kParticleEmission;
    case kCameraShakeTrack:
      return TimelineKeySetup::kCameraShake;
    default:
      return TimelineKeySetup::kNone;
  }
}

[[nodiscard]] constexpr std::int16_t Offset(std::uint8_t channel) {
  return static_cast<std::int16_t>(channel - kNeutralColourChannel);
}

// Shifts a visible actor endpoint's colours; other endpoints are skipped.
void ShiftEndpoint(const CommandEndpoint& endpoint, const ColourShift& shift,
                   ColourTrackHost& host) {
  if (endpoint.type != kActorPartEndpoint) return;
  if (host.ActorVisible(endpoint.actor)) {
    host.ShiftActorColours(endpoint.actor, shift);
  }
}
}  // namespace

// decomp: func_800D5780 (src/BATTLE/BATTLE.PRG/5BF94.c:6927)
std::optional<TimelineKeyAdvance> AdvanceTimelineKey(std::int16_t key_index,
                                                     std::uint8_t key_count,
                                                     std::uint8_t kind) {
  const int next = key_index + 1;
  if (next >= key_count) return std::nullopt;
  return TimelineKeyAdvance{.key_index = static_cast<std::int16_t>(next),
                            .setup = SetupOf(kind)};
}

// decomp: func_800D5700 (src/BATTLE/BATTLE.PRG/5BF94.c:6914)
ParticleKeyTiming ParticleKeyTimingOf(std::uint32_t key) {
  return {.timer = static_cast<std::int16_t>((key >> kParticleTimerShift) &
                                             kParticleTimerMask),
          .parameter = static_cast<std::int32_t>(
              (key >> kParticleParameterShift) & kParticleParameterMask)};
}

// decomp: func_800D5738 (src/BATTLE/BATTLE.PRG/5BF94.c:6921)
std::int16_t ShakeKeyTimer(std::uint32_t parameters,
                           const CurveDuration& duration) {
  const auto curve = static_cast<std::uint8_t>(
      (parameters >> kShakeCurveShift) & kShakeCurveMask);
  return static_cast<std::int16_t>(duration(curve));
}

ColourShift ColourShiftOf(const ColourKey& key) {
  return {.mode = key.mode,
          .duration = key.duration,
          .red = Offset(key.red),
          .green = Offset(key.green),
          .blue = Offset(key.blue)};
}

// decomp: func_800D5F8C (src/BATTLE/BATTLE.PRG/5BF94.c:7122)
bool TickSourceActorColourTrack(const ColourKey& key,
                                std::int16_t timeline_frame,
                                const CommandEndpoint& emit_from,
                                ColourTrackHost& host) {
  if (key.frame != timeline_frame) return true;
  ShiftEndpoint(emit_from, ColourShiftOf(key), host);
  return host.AdvanceKey();
}

// decomp: func_800D6048 (src/BATTLE/BATTLE.PRG/5BF94.c:7138)
bool TickTargetActorColourTrack(const ColourKey& key,
                                std::int16_t timeline_frame,
                                std::span<const CommandEndpoint> action_targets,
                                const CommandEndpoint& emit_to,
                                bool all_targets, ColourTrackHost& host) {
  if (key.frame != timeline_frame) return true;
  const ColourShift shift = ColourShiftOf(key);
  if (all_targets) {
    for (const CommandEndpoint& target : action_targets) {
      ShiftEndpoint(target, shift, host);
    }
  } else {
    ShiftEndpoint(emit_to, shift, host);
  }
  return host.AdvanceKey();
}

// decomp: func_800D61AC (src/BATTLE/BATTLE.PRG/5BF94.c:7166)
bool TickOtherActorColourTrack(const ColourKey& key,
                               std::int16_t timeline_frame,
                               std::span<const std::uint8_t> other_actors,
                               ColourTrackHost& host) {
  if (key.frame != timeline_frame) return true;
  const ColourShift shift = ColourShiftOf(key);
  for (const std::uint8_t actor : other_actors) {
    if (host.ActorVisible(actor)) host.ShiftActorColours(actor, shift);
  }
  return host.AdvanceKey();
}

// decomp: func_800D6298 (src/BATTLE/BATTLE.PRG/5BF94.c:7183)
bool TickSceneColourTrack(const ColourKey& key, std::int16_t timeline_frame,
                          ColourTrackHost& host) {
  if (key.frame != timeline_frame) return true;
  host.ShiftSceneColours(ColourShiftOf(key));
  return host.AdvanceKey();
}

// decomp: func_800D6310 (src/BATTLE/BATTLE.PRG/5BF94.c:7195)
bool TickBackgroundColourTrack(const ColourKey& key,
                               std::int16_t timeline_frame,
                               ColourTrackHost& host) {
  if (key.frame != timeline_frame) return true;
  host.ShiftBackgroundColours(ColourShiftOf(key));
  return host.AdvanceKey();
}
}  // namespace VS::Effects
