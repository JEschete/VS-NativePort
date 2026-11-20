#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <cstdint>
#include <optional>

// The single-target secondary effect requests the battle code builds and hands
// to the secondary effect runtime (func_800CF3F8, BeginSecondaryEffects in
// EffectRuntime.hpp): action hit effects, ground-plane impacts, trap tiles and
// the ability-learned effect (BATTLE.PRG 146C.c). Each request holds one
// target and no other actors. A request defines only the fields its producer
// writes; the others stay zero (the original left them as stack bytes).
namespace VS::Effects {

// LiveActionEffectTarget.type of the endpoints these requests use.
// kUnset is a zero type the producers here never write.
enum class EffectEndpointKind : std::uint16_t {
  kUnset = 0,
  kActor = 4,
  kPosition = 5
};

// An endpoint position (SVECTOR); `w` is its fourth halfword.
struct EffectPoint {
  std::int16_t x{}, y{}, z{}, w{};
  friend bool operator==(const EffectPoint&, const EffectPoint&) = default;
};

// One endpoint of a request. An actor endpoint names the actor, an extra slot
// byte and the part (bone tag); a position endpoint names a point.
struct EffectEndpoint {
  EffectEndpointKind kind{};
  std::uint16_t color_preset{};
  std::uint8_t actor{}, slot{}, part{};
  EffectPoint position{};
  friend bool operator==(const EffectEndpoint&,
                         const EffectEndpoint&) = default;
};

// A secondary request from `source` to its single `target`.
struct SingleTargetEffectRequest {
  std::uint16_t effect_id{};
  EffectEndpoint source{}, target{};
  friend bool operator==(const SingleTargetEffectRequest&,
                         const SingleTargetEffectRequest&) = default;
};

// The parameter every request here begins with (func_800CF3F8's second
// argument).
inline constexpr std::int32_t kSecondaryRequestParameter = 0;

// The effects these producers name.
inline constexpr std::uint16_t kDefaultHitEffect = 1;
inline constexpr std::uint16_t kAbilityLearnedEffect = 20;
inline constexpr std::uint16_t kTrapTileEffect = 33;
inline constexpr std::uint16_t kGroundPlaneImpactEffect = 36;
// Position hits and actor impact kinds 3, 4 and 6 of the defined actions.
inline constexpr std::uint16_t kPositionHitEffect = 0x26;
// Actor impact kind 1 of the defined actions.
inline constexpr std::uint16_t kImpactKindOneEffect = 0x27;

// effectBusyFlags bit that suppresses the trap tile effect (the
// secondary instances' busy bit, SecondaryBusyBits).
inline constexpr std::uint32_t kTrapEffectBusyBit = 2;

// The hit word of a target (VS::Battle::HitTargetWord) and whether the hit
// names a position rather than an actor (Battle_HitResult.targetsPosition !=
// 0).
struct ImpactTarget {
  std::uint8_t actor{};
  std::int8_t limb{};
  std::uint8_t threshold{}, impact{};
  std::int16_t hp{}, mp{};
  bool at_position{};
};

// The current queued action (VS::Battle::actionState->queue[queueIndex & 3]).
struct QueuedEffectAction {
  std::uint16_t action_id{};
  std::uint8_t effect_variant{};
  // primaryTarget.target.target_actor: the request's source actor.
  std::uint8_t source_actor{};
  std::uint16_t target_count{};
};

// A tile a trap fires on (VS::Actors::ActorLocation): its tile coordinates and
// the height of the position on it.
struct TrapTile {
  std::uint8_t tile_x{}, tile_z{};
  std::int16_t height{};
};

// Where the requests go.
class SecondaryEffectSink {
 public:
  virtual ~SecondaryEffectSink() = default;
  // func_800CF3F8: begins the request's secondary instances.
  virtual void Begin(const SingleTargetEffectRequest& request,
                     std::int32_t parameter) = 0;
  // effectBusyFlags.
  [[nodiscard]] virtual std::uint32_t BusyFlags() = 0;

 protected:
  SecondaryEffectSink() = default;
  SecondaryEffectSink(const SecondaryEffectSink&) = default;
  SecondaryEffectSink& operator=(const SecondaryEffectSink&) = default;
  SecondaryEffectSink(SecondaryEffectSink&&) = default;
  SecondaryEffectSink& operator=(SecondaryEffectSink&&) = default;
};

// The queued action, its targets and the skeleton and sound services the
// action hit effects use.
class ActionImpactWorld : public SecondaryEffectSink {
 public:
  // The current queued action; nullopt when there is no action state.
  [[nodiscard]] virtual std::optional<QueuedEffectAction> QueuedAction() = 0;
  // The queued action's target `index`; nullopt past the action's targets.
  [[nodiscard]] virtual std::optional<ImpactTarget> QueuedTarget(
      std::size_t index) = 0;
  // hits[0].prerequisiteFunctionId of the action's definition.
  [[nodiscard]] virtual std::uint8_t HitPrerequisite(std::uint16_t action) = 0;
  // func_800A152C: the bone tag of `actor`'s `limb` of `kind`.
  [[nodiscard]] virtual std::int32_t BoneTag(std::uint8_t actor,
                                             std::int8_t limb,
                                             std::int32_t kind) = 0;
  // func_8006C5AC: the attacker's weapon hit sounds at every hit actor.
  virtual void PlayWeaponHitSounds(std::int32_t attacker) = 0;
};

// The bone tag kind a hit effect's actor endpoint uses.
inline constexpr std::int32_t kHitEffectBoneTagKind = 2;
// Actions below this have a definition whose prerequisite chooses the effect.
inline constexpr std::uint16_t kImpactDefinedActions = 0x28;
// Actions below this use the default hit effect for plain impacts.
inline constexpr std::uint16_t kDefaultHitActions = 0x16;
// The hit prerequisite that keeps the action's own effect.
inline constexpr std::uint8_t kOwnEffectPrerequisite = 3;

// The effect and limb an impact's hit effect uses.
struct ImpactEffectChoice {
  std::uint16_t effect_id{};
  std::uint8_t limb{};
};

// func_8006C4C4 (146C.c:1927): the hit effect request from the queued
// action's source actor to `target`: at its actor's limb bone tag with
// `limb` as the slot byte, or at the position its hit word holds. The target
// carries the action's effect variant as its color preset. `bone_tag` is the
// actor bone tag the request names (unused for a position).
[[nodiscard]] SingleTargetEffectRequest ActionHitEffectRequest(
    std::uint16_t effect_id, const QueuedEffectAction& action,
    const ImpactTarget& target, std::uint8_t limb, std::int32_t bone_tag);

// func_8006C4C4 (146C.c:1927): begins the hit effect for `target`. Refused
// (false) when there is no queued action.
bool QueueActionHitEffect(std::uint16_t effect_id, const ImpactTarget& target,
                          std::uint8_t limb, ActionImpactWorld& world);

// The hit effect func_8006C84C chooses for a target of `action`; nullopt for
// an actor impact kind without one (7 and above).
[[nodiscard]] std::optional<ImpactEffectChoice> ChooseImpactEffect(
    std::uint16_t action, const ImpactTarget& target, ActionImpactWorld& world);

// func_8006C84C (146C.c:2044): begins each queued target's hit effect and
// plays the attacker's weapon hit sounds once per target. Returns 1. A
// missing queued action or a target past the action's targets ends the walk.
std::int32_t QueueActionImpactEffects(std::int32_t attacker,
                                      ActionImpactWorld& world);

// func_8006F450 (146C.c:3171): effect 36 at (x, 0, z).
[[nodiscard]] SingleTargetEffectRequest GroundPlaneImpactEffectRequest(
    std::int16_t x, std::int16_t z);
void SpawnGroundPlaneImpactEffect(std::int16_t x, std::int16_t z,
                                  SecondaryEffectSink& sink);

// func_8006F4B0 (146C.c:3187): effect 33 at the centre of the trap's tile.
[[nodiscard]] SingleTargetEffectRequest TrapTileEffectRequest(
    const TrapTile& tile);
// Begins it unless the secondary busy bit is set; returns whether it began.
bool SpawnTrapTileEffect(const TrapTile& tile, SecondaryEffectSink& sink);

// func_8006F848 (146C.c:3298): effect 20 on the player's part 0xFD. Its
// source is a position endpoint the original left unwritten (here the
// origin).
[[nodiscard]] SingleTargetEffectRequest AbilityLearnedEffectRequest();
void SpawnAbilityLearnedEffect(SecondaryEffectSink& sink);
}  // namespace VS::Effects
