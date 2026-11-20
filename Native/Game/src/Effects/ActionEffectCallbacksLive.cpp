// Hand-maintained (tools/port/owned.csv).
#include "Effects/ActionEffectCallbacksLive.hpp"

#include <cstddef>
#include <cstdint>

#include "Actors/AnimationControlLive.hpp"
#include "Battle/HudStateLive.hpp"
#include "Effects/ActionEffectCallbacks.hpp"
#include "Effects/ActionRequest.hpp"
#include "Effects/EffectRuntimeLive.hpp"
#include "Effects/Runtime.hpp"
#include "Field/GameModeLive.hpp"
#include "Field/Interactions.hpp"
#include "Field/MapLive.hpp"
#include "Legacy/Psx.hpp"
#include "Render/PaletteAnimationLive.hpp"
#include "State/Timers.hpp"

namespace VS::Effects {
namespace {
[[nodiscard]] LiveActionEffectTarget LiveEndpoint(
    const EffectEndpoint& endpoint) {
  LiveActionEffectTarget target{};
  target.type = static_cast<std::uint16_t>(endpoint.kind);
  target.color_preset = endpoint.color_preset;
  if (endpoint.kind == EffectEndpointKind::kPosition) {
    target.position.vx = endpoint.position.x;
    target.position.vy = endpoint.position.y;
    target.position.vz = endpoint.position.z;
    target.position.pad = endpoint.position.w;
  } else {
    target.SetActor({.actor = endpoint.actor,
                     .event_actor_slot = endpoint.slot,
                     .part = endpoint.part,
                     .reserved3 = 0});
  }
  return target;
}

class LiveHost final : public ActionEffectCallbackHost {
 public:
  void BeginActionEffect(const PrimaryEffectRequest& request,
                         std::int32_t action_variant,
                         std::int32_t variant_argument) override {
    static_cast<void>(BeginLivePrimaryEffectRequest(request, action_variant,
                                                    variant_argument));
  }
  void RequestActorAnimationAdvance(std::int32_t actor) override {
    Actors::RequestLiveActorAnimationAdvance(actor);
  }
  void TransformActorColors(const ActorColorTransform& transform) override {
    Render::TransformLiveActorColors(
        static_cast<int>(transform.mode), transform.duration, transform.actor,
        transform.red_offset, transform.green_offset, transform.blue_offset);
  }
  std::int32_t TintedActor() override { return tintedActor; }
  void SetTintedActor(std::int32_t actor) override { tintedActor = actor; }
  void SetFieldControlMode(Field::FieldControlMode mode) override {
    VS::Field::controlMode = static_cast<std::uint32_t>(mode);
  }
  void SetTickSpeed(std::int32_t tick_speed) override {
    VS::State::tickSpeed = tick_speed;
  }
  void SetHudVisibility(Battle::HudVisibility visibility) override {
    Battle::SetLiveHudHidden(static_cast<int>(visibility));
  }
  void SetMinimapVisible(bool visible) override {
    static_cast<void>(Field::SetLiveMinimapVisible(visible ? 1 : 0));
  }
  void EnterFieldExplorationMode() override {
    Field::LiveEnterFieldExplorationMode();
  }
  std::uint32_t EffectBusyFlags() override {
    return static_cast<std::uint32_t>(effectBusyFlags);
  }
};
}  // namespace

int BeginLivePrimaryEffectRequest(const PrimaryEffectRequest& request,
                                  int action_variant, int variant_argument) {
  LiveActionEffectRequest live{};
  live.effect_id = request.effect_id;
  live.count = 1;
  live.other_count = request.other_count;
  live.source = LiveEndpoint(request.source);
  live.targets[0] = LiveEndpoint(request.target);
  for (std::size_t i = 0; i < kPrimaryEffectOthers; ++i) {
    live.others[i] = request.others[i];
  }
  live.position.vx = request.position.x;
  live.position.vy = request.position.y;
  live.position.vz = request.position.z;
  live.position.pad = request.position.w;
  return LiveBeginActionEffect(&live, action_variant, variant_argument);
}

// decomp: BATTLE.PRG func_8006CDB8 (F00064,
// src/BATTLE/BATTLE.PRG/146C.c:2149, 0x8006CDB8-0x8006CDD8)
void LiveSignalActorEffectAnimation(int actor_index) {
  LiveHost host;
  SignalActorEffectAnimation(actor_index, host);
}

// decomp: BATTLE.PRG func_8006D028 (F00068,
// src/BATTLE/BATTLE.PRG/146C.c:2211, 0x8006D028-0x8006D064)
void LiveTintActionActorRed(int actor_index, int /*unused*/) {
  LiveHost host;
  TintActionActorRed(actor_index, host);
}

// decomp: BATTLE.PRG func_8006D064 (F00069,
// src/BATTLE/BATTLE.PRG/146C.c:2217, 0x8006D064-0x8006D0A4)
void LiveRestoreActionActorTint() {
  LiveHost host;
  RestoreActionActorTint(host);
}

// decomp: BATTLE.PRG func_800733F8 (F00119,
// src/BATTLE/BATTLE.PRG/146C.c:4613, 0x800733F8-0x80073484)
void LiveBeginAshleyStandaloneEffect(int effect_id) {
  LiveHost host;
  BeginAshleyStandaloneEffect(static_cast<std::uint16_t>(effect_id), host);
}

// decomp: BATTLE.PRG func_80073484 (F00120,
// src/BATTLE/BATTLE.PRG/146C.c:4638, 0x80073484-0x800734C0)
void LiveEndAshleyStandaloneEffect() {
  LiveHost host;
  EndAshleyStandaloneEffect(host);
}

// decomp: BATTLE.PRG func_80074294 (F00143,
// src/BATTLE/BATTLE.PRG/146C.c:5064, 0x80074294-0x800742A4)
int LiveActionEffectBusyFlags() {
  LiveHost host;
  return static_cast<int>(ActionEffectBusyFlags(host));
}
}  // namespace VS::Effects
