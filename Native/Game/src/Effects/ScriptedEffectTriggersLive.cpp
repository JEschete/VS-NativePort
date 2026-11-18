// Hand-maintained (tools/port/owned.csv).
#include "Effects/ScriptedEffectTriggersLive.hpp"

#include <cstddef>
#include <cstdint>
#include <iterator>

#include "Battle/Damage.h"
#include "Effects/ActionEffectCallbacksLive.hpp"
#include "Effects/EffectRuntimeLive.hpp"
#include "Effects/PrimaryEffectLive.hpp"
#include "Effects/ScriptedEffectTriggers.hpp"

namespace VS::Effects {
namespace {
class LiveHost final : public ScriptedEffectTriggerHost {
 public:
  void BeginActionEffect(const PrimaryEffectRequest& request,
                         std::int32_t action_variant,
                         std::int32_t variant_argument) override {
    static_cast<void>(BeginLivePrimaryEffectRequest(request, action_variant,
                                                    variant_argument));
  }
  bool ActorLoaded(std::size_t slot) override {
    return slot < std::size(Battle_actors) && Battle_actors[slot] != nullptr;
  }
  void StartEffectTrack(std::int32_t track) override {
    static_cast<void>(LiveStartEffectTrack(track));
  }
  std::int32_t PrimaryEffectPhase() override {
    return LivePrimaryEffectPhase();
  }
  std::int32_t CanStartEffectTrack() override {
    return LiveCanStartEffectTrack();
  }
};
}  // namespace

// decomp: BATTLE.PRG func_8007BE20 (F00238,
// src/BATTLE/BATTLE.PRG/146C.c:8581, 0x8007BE20-0x8007BEC4)
void LiveStartScriptedActorEffect(int effect_id, int target_actor_index) {
  LiveHost host;
  StartScriptedActorEffect(static_cast<std::uint16_t>(effect_id),
                           target_actor_index, host);
}

// decomp: BATTLE.PRG func_8007BEC4 (F00239,
// src/BATTLE/BATTLE.PRG/146C.c:8611, 0x8007BEC4-0x8007BEE4)
void LiveReleaseScriptedEffectTrigger(int track_index) {
  LiveHost host;
  ReleaseScriptedEffectTrigger(track_index, host);
}

// decomp: BATTLE.PRG func_8007BEE4 (F00240,
// src/BATTLE/BATTLE.PRG/146C.c:8613, 0x8007BEE4-0x8007BF04)
int LiveScriptedEffectPlaybackState() {
  LiveHost host;
  return ScriptedEffectPlaybackState(host);
}

// decomp: BATTLE.PRG func_8007BF0C (F00242,
// src/BATTLE/BATTLE.PRG/146C.c:8617, 0x8007BF0C-0x8007BF2C)
int LiveIsScriptedEffectTriggerReady() {
  LiveHost host;
  return IsScriptedEffectTriggerReady(host) ? 1 : 0;
}
}  // namespace VS::Effects
