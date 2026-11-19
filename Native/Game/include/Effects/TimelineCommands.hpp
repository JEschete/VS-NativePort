#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>

#include "Effects/Transforms.hpp"

// Typed effect timeline action-control commands (BATTLE.PRG 5BF94.c:8213-8420).
// The endpoint commands read the effect instance's endpoints and select what
// they apply to; the transform and timeline commands update the instance's
// flags; TickActionControlTrack runs a track's due key.
// TimelineCommandsLive.hpp performs them on the effect instance records.
namespace VS::Effects {
// An endpoint on a room tile or room object (Effects_ActionEffectTarget type
// 5).
inline constexpr std::uint16_t kRoomPointEndpoint = 5;
// Reaction sound variants below this play as given; the others pick one of the
// random variants.
inline constexpr std::uint8_t kFixedReactionVariants = 4;
inline constexpr std::int32_t kRandomReactionVariants = 3;

// One draw from the BIOS rand() sequence.
using RandomDraw = std::function<std::int32_t()>;

// Effects_ActionEffectTarget.value.position; `pad` names the actor or room
// object the point belongs to.
struct EndpointPoint {
  std::int16_t x{};
  std::int16_t y{};
  std::int16_t z{};
  std::int16_t pad{};

  friend bool operator==(const EndpointPoint&, const EndpointPoint&) = default;
};

// An action effect endpoint as the commands read it: `actor` is
// value.actor.actor, which overlays the low byte of the point's x.
struct CommandEndpoint {
  std::uint16_t type{};
  std::uint8_t actor{};
  EndpointPoint point{};
};

// The effect instance's endpoints (Effects_EffectInstanceTargets).
struct CommandEndpoints {
  CommandEndpoint source{};
  CommandEndpoint emit_from{};
  CommandEndpoint target{};
  CommandEndpoint emit_to{};
};

// An action impact's arguments: an actor endpoint names the actor alone; any
// other names the actor in the point's pad, with the point.
struct ActionImpact {
  std::int32_t actor{};
  std::optional<EndpointPoint> point;

  friend bool operator==(const ActionImpact&, const ActionImpact&) = default;
};

// A weapon brightness override for an actor; `enabled` is false (and
// `brightness` 0) when the command clears it.
struct WeaponBrightness {
  std::uint8_t actor{};
  bool enabled{};
  std::uint8_t brightness{};

  friend bool operator==(const WeaponBrightness&,
                         const WeaponBrightness&) = default;
};

// A reaction sound for an actor and the random variant the next random pick
// must avoid.
struct ReactionSound {
  std::uint8_t actor{};
  std::uint8_t variant{};
  std::uint8_t last_variant{};

  friend bool operator==(const ReactionSound&, const ReactionSound&) = default;
};

// decomp: func_800D6388 (src/BATTLE/BATTLE.PRG/5BF94.c:8213)
[[nodiscard]] ActionImpact TargetActionImpact(
    const CommandEndpoints& endpoints);
// decomp: func_800D63D0 (src/BATTLE/BATTLE.PRG/5BF94.c:8223)
[[nodiscard]] ActionImpact SourceActionImpact(
    const CommandEndpoints& endpoints);
// decomp: func_800D6418 (src/BATTLE/BATTLE.PRG/5BF94.c:8233): the room tile
// whose trap the target disables.
[[nodiscard]] std::optional<EndpointPoint> TargetRoomTile(
    const CommandEndpoints& endpoints);
// decomp: func_800D6574 (src/BATTLE/BATTLE.PRG/5BF94.c:8259)
[[nodiscard]] std::optional<std::uint8_t> TimelineSourceActorToSignal(
    const CommandEndpoints& endpoints);
// decomp: func_800D65D8 (src/BATTLE/BATTLE.PRG/5BF94.c:8272): a brightness of
// 0 clears the override.
[[nodiscard]] std::optional<WeaponBrightness> SourceWeaponBrightness(
    const CommandEndpoints& endpoints, std::uint8_t brightness);
// decomp: func_800D6628 (src/BATTLE/BATTLE.PRG/5BF94.c:8285): variants from
// kFixedReactionVariants on draw until the pick differs from `last_variant`.
[[nodiscard]] std::optional<ReactionSound> SourceReactionSound(
    const CommandEndpoints& endpoints, std::uint8_t variant,
    std::uint8_t last_variant, const RandomDraw& draw);
// decomp: func_800D66CC (src/BATTLE/BATTLE.PRG/5BF94.c:8301): the room object
// the target unlocks.
[[nodiscard]] std::optional<EndpointPoint> TargetGimmick(
    const CommandEndpoints& endpoints);
// decomp: func_800D6860 (src/BATTLE/BATTLE.PRG/5BF94.c:8335): the actor whose
// defense ability timing window opens.
[[nodiscard]] std::optional<std::uint8_t> DefenseTimingTarget(
    const CommandEndpoints& endpoints);

// LiveEffectTimeline.flags: the timeline stays on its frame.
inline constexpr std::uint8_t kTimelineFrameHeld = 0x1;
// A packed trail style's low decimal digit picks the colour, the rest the
// texture.
inline constexpr std::uint8_t kTrailStyleBase = 10;

// The transform slot a command operand names; nullopt from
// kEffectTransformSlots on (refused: the original indexes past the instance's
// transforms).
[[nodiscard]] std::optional<std::size_t> TransformSlotOf(std::uint8_t operand);

// An effect transform's attachment: an actor and one of its bones.
struct TransformAnchor {
  std::uint8_t actor{};
  std::uint8_t bone{};
};

// A weapon trail drawn from an actor's bone.
struct WeaponTrail {
  std::uint8_t actor{};
  std::uint8_t bone{};
  std::uint8_t color{};
  std::uint8_t texture{};

  friend bool operator==(const WeaponTrail&, const WeaponTrail&) = default;
};

// The effect resources, trails and room objects the commands reach.
class TimelineEffectHost {
 public:
  virtual ~TimelineEffectHost() = default;
  // LiveSaveEffectResourceContext: the next calls use the action's effect
  // resources.
  virtual void SaveResourceContext() = 0;
  // LiveRestoreEffectResourceContext.
  virtual void RestoreResourceContext() = 0;
  // LiveSpawnBoneTrail.
  virtual void SpawnBoneTrail(const WeaponTrail& trail) = 0;
  // LiveFadeBoneTrails: every weapon trail fades out.
  virtual void FadeBoneTrails() = 0;
  // FreezeLiveRoomGimmicksTemporarily.
  virtual void FreezeRoomObjects() = 0;

 protected:
  TimelineEffectHost() = default;
  TimelineEffectHost(const TimelineEffectHost&) = default;
  TimelineEffectHost& operator=(const TimelineEffectHost&) = default;
  TimelineEffectHost(TimelineEffectHost&&) = default;
  TimelineEffectHost& operator=(TimelineEffectHost&&) = default;
};

// decomp: func_800D6448 (src/BATTLE/BATTLE.PRG/5BF94.c:8240): spawns the
// trail of `packed_style` from the anchor with the action's resources.
void SpawnTransformWeaponTrail(TransformAnchor anchor,
                               std::uint8_t packed_style,
                               TimelineEffectHost& host);
// decomp: func_800D64E4 (src/BATTLE/BATTLE.PRG/5BF94.c:8247)
void FadeActionWeaponTrails(TimelineEffectHost& host);
// decomp: func_800D6508 (src/BATTLE/BATTLE.PRG/5BF94.c:8249): a transform's
// flags with its frame advancing again.
[[nodiscard]] std::uint16_t ResumeTransformMotion(std::uint16_t flags);
// decomp: func_800D6538 (src/BATTLE/BATTLE.PRG/5BF94.c:8255): the timeline
// flags holding its frame.
[[nodiscard]] std::uint8_t HoldTimelineFrame(std::uint8_t flags);
// decomp: func_800D6554 (src/BATTLE/BATTLE.PRG/5BF94.c:8257)
void PauseMovingRoomObjects(TimelineEffectHost& host);
// decomp: func_800D65A8 (src/BATTLE/BATTLE.PRG/5BF94.c:8266): a transform's
// flags with its attachment and target to resolve again.
[[nodiscard]] std::uint16_t InvalidateTransformAttachments(std::uint16_t flags);

// An action-control key (func_800D6894_t, 5BF94.c:84): `timing` holds the frame
// the key is due in bits 0-8 and its command in bits 9-15.
struct ActionControlKey {
  std::uint16_t timing{};
  std::uint8_t operand0{};
  std::uint8_t operand1{};
};
inline constexpr std::uint16_t kActionControlFrameMask = 0x1FF;
inline constexpr int kActionControlCommandShift = 9;

// The action-control commands, by key command number; the numbers from
// kActionControlCommands on do nothing.
enum class ActionControlCommand : std::uint8_t {
  kTargetImpact,
  kSourceImpact,
  kTargetRoomTile,
  kTransformWeaponTrail,
  kFadeWeaponTrails,
  kResumeTransformMotion,
  kHoldTimelineFrame,
  kPauseRoomObjects,
  kSignalSourceActor,
  kInvalidateTransformAttachments,
  kSourceWeaponBrightness,
  kSourceReactionSound,
  kTargetGimmick,
  kFocusCamera,
  kRestoreCamera,
  kDefenseTiming,
};
inline constexpr std::uint8_t kActionControlCommands = 16;

// A due key's command and its operands.
struct ActionControl {
  ActionControlCommand command{};
  std::uint8_t operand0{};
  std::uint8_t operand1{};

  friend bool operator==(const ActionControl&, const ActionControl&) = default;
};

// What an action-control track drives.
class ActionControlHost {
 public:
  virtual ~ActionControlHost() = default;
  // Performs one command on the effect instance.
  virtual void Perform(const ActionControl& control) = 0;
  // LiveAdvanceEffectTimelineKey: false once the track has no more keys.
  [[nodiscard]] virtual bool AdvanceKey() = 0;

 protected:
  ActionControlHost() = default;
  ActionControlHost(const ActionControlHost&) = default;
  ActionControlHost& operator=(const ActionControlHost&) = default;
  ActionControlHost(ActionControlHost&&) = default;
  ActionControlHost& operator=(ActionControlHost&&) = default;
};

// decomp: func_800D6894 (src/BATTLE/BATTLE.PRG/5BF94.c:8342): once the
// timeline reaches the key's frame, performs its command and advances the
// track. False once the track has no more keys.
[[nodiscard]] bool TickActionControlTrack(const ActionControlKey& key,
                                          std::int16_t timeline_frame,
                                          ActionControlHost& host);
}  // namespace VS::Effects
