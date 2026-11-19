#include "Effects/Templates/RepeatingRingsLive.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "Core/Log.hpp"
#include "Effects/Package.hpp"
#include "Effects/PackageExtents.hpp"
#include "Effects/Particles.hpp"
#include "Effects/Plugins/Plg077.hpp"
#include "Effects/Plugins/Plg162.hpp"
#include "Effects/Plugins/Plg236.hpp"
#include "Effects/Plugins/Plg263.hpp"
#include "Effects/Plugins/Plg300.hpp"
#include "Effects/Plugins/PluginData.hpp"
#include "Effects/Runtime.h"
#include "Effects/Templates/RepeatingRings.hpp"
#include "Effects/Templates/TemplateLiveState.hpp"
#include "Effects/TransformsLiveState.hpp"
#include "Legacy/Psx.h"

namespace VS::Effects::Templates {
namespace {
std::uint64_t refused_rings = 0;

[[nodiscard]] bool Refuse(std::string_view reason) {
  if (std::has_single_bit(++refused_rings)) {
    VS::Core::LogWarn("effects",
                      "closed repeating ring refused: {} ({} refusals)", reason,
                      refused_rings);
  }
  return false;
}

[[nodiscard]] bool BoundCurve(std::uint8_t curve, std::size_t count) {
  return curve == 0 ||
         (curve <= count && Effects_package->curve_block != nullptr &&
          Effects_package->curves[curve - 1] != nullptr &&
          Effects_package->curve_block->Size(curve - 1) != 0);
}

[[nodiscard]] bool BoundTick(const LiveParticleDefinition& definition) {
  if (Effects_currentInstance == nullptr ||
      definition.transform_index >=
          std::span(Effects_currentInstance->transforms).size()) {
    return Refuse("missing current instance or unavailable transform");
  }
  const std::array curves{definition.inner_extent_curve,
                          definition.position_curve,
                          definition.extent_curve,
                          definition.force_curve,
                          definition.acceleration_curve,
                          definition.direction_curve,
                          definition.speed_curve,
                          definition.r_curve,
                          definition.g_curve,
                          definition.b_curve};
  const auto count = LiveEffectCurveCount();
  if (!std::ranges::all_of(curves, [count](std::uint8_t curve) {
        return BoundCurve(curve, count);
      })) {
    return Refuse("unbound or empty current-package curve");
  }
  return true;
}

[[nodiscard]] bool Ready(const Effects_RendererSlot* slot, RingCommand command,
                         std::int32_t packed) {
  if (slot == nullptr) return Refuse("missing renderer slot");
  if (command == RingCommand::kStop ||
      (command != RingCommand::kCreate && command != RingCommand::kTick &&
       command != RingCommand::kExpire)) {
    return true;
  }
  if (command == RingCommand::kCreate) {
    if (slot->rendererState != nullptr) {
      return Refuse("create requires an empty renderer slot");
    }
  } else if (slot->rendererState == nullptr) {
    return Refuse("tick or expire without a created state");
  }
  if (command == RingCommand::kExpire) return true;
  const auto index =
      command == RingCommand::kCreate
          ? PlanRing(packed).definition_index
          : static_cast<const LiveRingState*>(slot->rendererState)->effect;
  if (Effects_package == nullptr ||
      Effects_package->particle_definitions == nullptr ||
      index >= LiveParticleDefinitionCount()) {
    return Refuse("definition outside the current bound package");
  }
  return command != RingCommand::kTick ||
         BoundTick(Effects_package->particle_definitions->Definitions()[index]);
}

[[nodiscard]] std::array<RingPaletteEntry, kRingPaletteEntries> Palette(
    const plugins::RingPaletteBytes& live) {
  std::array<RingPaletteEntry, kRingPaletteEntries> palette{};
  for (std::size_t index = 0; index < palette.size(); ++index) {
    palette.at(index).rows = {live[index][0], live[index][1], live[index][2]};
  }
  return palette;
}

int Run(Effects_RendererSlot* slot, unsigned int command, int packed,
        const plugins::RingPaletteBytes& palette, LiveRingInstance instance) {
  const auto operation = static_cast<RingCommand>(command);
  if (!Ready(slot, operation, packed)) return 0;
  const auto alive =
      RunLiveRing(slot, command, packed, Palette(palette), instance);
  if (operation == RingCommand::kCreate && slot->rendererState == nullptr) {
    static_cast<void>(Refuse("state allocation did not create a ring"));
    return 0;
  }
  return alive;
}
}  // namespace

// F01920, PLG052.BIN func_800F9800, src/EFFECT/PLG052.BIN/0.c:3 INCLUDE_ASM.
// The original 800FA770 palette equals PLG077's 800FD9B8 palette byte for
// byte; only its native data binding is shared, not the original verification.
int UpdateLivePlg052RepeatingRing(Effects_RendererSlot* slot,
                                  unsigned int command, int packed) {
  return Run(slot, command, packed, Effects::Plg077::ringColorFactors,
             &Plg052::RepeatingRing);
}

// F01998, PLG077.BIN func_800FA324, src/EFFECT/PLG077.BIN/18.c:37.
int UpdateLivePlg077RepeatingRing(Effects_RendererSlot* slot,
                                  unsigned int command, int packed) {
  return Run(slot, command, packed, Effects::Plg077::ringColorFactors,
             &Plg077::RepeatingRing);
}

// F02209, PLG162.BIN func_800F9800, src/EFFECT/PLG162.BIN/0.c:32.
int UpdateLivePlg162TransposedRing(Effects_RendererSlot* slot,
                                   unsigned int command, int packed) {
  return Run(slot, command, packed, Effects::Plg162::ringColorFactors,
             &Plg162::TransposedRing);
}

// F02289, PLG236.BIN func_800FB62C, src/EFFECT/PLG236.BIN/0.c:41.
int UpdateLivePlg236TransposedRing(Effects_RendererSlot* slot,
                                   unsigned int command, int packed) {
  return Run(slot, command, packed, Effects::Plg236::ringColorFactors,
             &Plg236::TransposedRing);
}

// F02323, PLG263.BIN func_800FB0CC, src/EFFECT/PLG263.BIN/110.c:40.
int UpdateLivePlg263TransposedRing(Effects_RendererSlot* slot,
                                   unsigned int command, int packed) {
  return Run(slot, command, packed, Effects::Plg263::ringColorFactors2,
             &Plg263::TransposedRing);
}

// F02397, PLG300.BIN func_800FC228, src/EFFECT/PLG300.BIN/110.c:48.
int UpdateLivePlg300RepeatingRing(Effects_RendererSlot* slot,
                                  unsigned int command, int packed) {
  return Run(slot, command, packed, Effects::Plg300::ringColorFactors,
             &Plg300::RepeatingRing);
}

// F02398, PLG300.BIN func_800FD198, src/EFFECT/PLG300.BIN/110.c:355.
int UpdateLivePlg300ForegroundRepeatingRing(Effects_RendererSlot* slot,
                                            unsigned int command, int packed) {
  return Run(slot, command, packed, Effects::Plg300::ringColorFactors2,
             &Plg300::ForegroundRepeatingRing);
}
}  // namespace VS::Effects::Templates
