// Hand-maintained (tools/port/owned.csv).
#include "Effects/SecondaryEffectRequestsLive.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>

#include "Actors/Location.hpp"
#include "Actors/SkeletonLive.hpp"
#include "Battle/Action.hpp"
#include "Battle/ActionFlow.hpp"
#include "Battle/ActionOutcomesLive.hpp"
#include "Battle/ActionTable.hpp"
#include "Battle/Hit.hpp"
#include "Core/Log.hpp"
#include "Effects/ActionRequest.hpp"
#include "Effects/EffectRuntimeLive.hpp"
#include "Effects/Runtime.hpp"
#include "Effects/SecondaryEffectRequests.hpp"
#include "Legacy/Psx.hpp"

using VS::Battle::ActionRequestRecord;

namespace VS::Effects {
namespace {
std::uint64_t refusals = 0;

void Refuse(const char* what) {
  if (std::has_single_bit(++refusals)) {
    VS::Core::LogWarn("effects",
                      "Secondary effect request refused: {} ({} refusals)",
                      what, refusals);
  }
}

[[nodiscard]] ImpactTarget LiveImpactTarget(const VS::Battle::HitResult& hit) {
  return {.actor = hit.target.target_actor,
          .limb = hit.target.target_limb,
          .threshold = hit.target.hit_threshold,
          .impact = hit.target.impact_kind,
          .hp = hit.target.hp,
          .mp = hit.target.mp,
          .at_position = hit.targets_position != 0};
}

[[nodiscard]] LiveActionEffectTarget LiveEndpoint(
    const EffectEndpoint& endpoint) {
  LiveActionEffectTarget target{};
  target.type = static_cast<std::uint16_t>(endpoint.kind);
  target.color_preset = endpoint.color_preset;
  if (endpoint.kind == EffectEndpointKind::kActor) {
    target.SetActor({.actor = endpoint.actor,
                     .event_actor_slot = endpoint.slot,
                     .part = endpoint.part,
                     .reserved3 = 0});
  } else {
    target.position.vx = endpoint.position.x;
    target.position.vy = endpoint.position.y;
    target.position.vz = endpoint.position.z;
    target.position.pad = endpoint.position.w;
  }
  return target;
}

class LiveWorld final : public ActionImpactWorld {
 public:
  void Begin(const SingleTargetEffectRequest& request,
             std::int32_t parameter) override {
    LiveActionEffectRequest live{};
    live.effect_id = request.effect_id;
    live.count = 1;
    live.other_count = 0;
    live.source = LiveEndpoint(request.source);
    live.targets[0] = LiveEndpoint(request.target);
    static_cast<void>(LiveBeginSecondaryEffects(&live, parameter));
  }
  std::uint32_t BusyFlags() override {
    return static_cast<std::uint32_t>(effectBusyFlags);
  }

  std::optional<QueuedEffectAction> QueuedAction() override {
    const ActionRequestRecord* queued = Queued();
    if (queued == nullptr) return std::nullopt;
    return QueuedEffectAction{
        .action_id = queued->action_id,
        .effect_variant = static_cast<std::uint8_t>(queued->effect_variant),
        .source_actor = queued->primary_target.target.target_actor,
        .target_count = queued->target_count};
  }
  std::optional<ImpactTarget> QueuedTarget(std::size_t index) override {
    const ActionRequestRecord* queued = Queued();
    if (queued == nullptr) return std::nullopt;
    if (index >= std::size(queued->targets)) {
      Refuse("queued target past the target array");
      return std::nullopt;
    }
    return LiveImpactTarget(queued->targets[index]);
  }
  std::uint8_t HitPrerequisite(std::uint16_t action) override {
    if (action >= std::size(VS::Battle::actionDefinitions)) return 0;
    return static_cast<std::uint8_t>(
        VS::Battle::actionDefinitions[action].hits[0].prerequisite_function_id);
  }
  std::int32_t BoneTag(std::uint8_t actor, std::int8_t limb,
                       std::int32_t kind) override {
    return Actors::FindLiveActorBoneTag(actor, limb, kind);
  }
  void PlayWeaponHitSounds(std::int32_t attacker) override {
    static_cast<void>(Battle::PlayLiveWeaponHitSounds(attacker));
  }

 private:
  static const ActionRequestRecord* Queued() {
    if (VS::Battle::actionState == nullptr) {
      Refuse("no action state");
      return nullptr;
    }
    constexpr std::uint32_t kQueueMask = 3;
    return &VS::Battle::actionState
                ->queue[VS::Battle::actionState->queue_index & kQueueMask];
  }
};
}  // namespace

// decomp: func_8006C4C4 (src/BATTLE/BATTLE.PRG/146C.c:1927)
void LiveQueueActionHitEffect(int effect_id, VS::Battle::HitResult* hit_result,
                              int target_limb) {
  if (hit_result == nullptr) {
    Refuse("null hit");
    return;
  }
  LiveWorld world;
  static_cast<void>(QueueActionHitEffect(
      static_cast<std::uint16_t>(effect_id), LiveImpactTarget(*hit_result),
      static_cast<std::uint8_t>(target_limb), world));
}

// decomp: func_8006C84C (src/BATTLE/BATTLE.PRG/146C.c:2044)
int LiveQueueActionImpactEffects(int attacker_index) {
  LiveWorld world;
  return QueueActionImpactEffects(attacker_index, world);
}

// decomp: func_8006F450 (src/BATTLE/BATTLE.PRG/146C.c:3171)
void LiveSpawnGroundPlaneImpactEffect(
    std::uint16_t* world_position_components) {
  if (world_position_components == nullptr) {
    Refuse("null ground position");
    return;
  }
  constexpr std::size_t kZ = 2;
  LiveWorld sink;
  SpawnGroundPlaneImpactEffect(
      static_cast<std::int16_t>(world_position_components[0]),
      static_cast<std::int16_t>(world_position_components[kZ]), sink);
}

// decomp: func_8006F4B0 (src/BATTLE/BATTLE.PRG/146C.c:3187)
void LiveSpawnTrapTileEffect(VS::Actors::ActorLocation* actor_location) {
  if (actor_location == nullptr) {
    Refuse("null trap location");
    return;
  }
  LiveWorld sink;
  static_cast<void>(SpawnTrapTileEffect(
      {.tile_x =
           static_cast<std::uint8_t>(actor_location->packed_tile.target.tile_x),
       .tile_z =
           static_cast<std::uint8_t>(actor_location->packed_tile.target.tile_z),
       .height = actor_location->position.vy},
      sink));
}

// decomp: func_8006F848 (src/BATTLE/BATTLE.PRG/146C.c:3298)
void LiveSpawnAbilityLearnedEffect() {
  LiveWorld sink;
  SpawnAbilityLearnedEffect(sink);
}
}  // namespace VS::Effects
