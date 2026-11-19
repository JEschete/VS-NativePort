// Hand-maintained (tools/port/owned.csv).
#include "Effects/ParticleTextureAnimationLive.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <utility>

#include "Core/Log.hpp"
#include "Effects/EffectResources.hpp"
#include "Effects/Package.hpp"
#include "Effects/PackageExtents.hpp"
#include "Effects/ParticleTextureAnimation.hpp"
#include "Effects/Particles.hpp"
#include "Effects/Runtime.h"
#include "Effects/TextureBindings.hpp"

namespace VS::Effects {
namespace {
using Animation = LiveParticleTextureAnimation;

std::uint64_t refused_calls = 0;
std::uint64_t unbound_frames = 0;

[[nodiscard]] bool ShouldReport(std::uint64_t& count) {
  return std::has_single_bit(++count);
}

void Refuse(std::string_view call, std::string_view reason) {
  if (ShouldReport(refused_calls)) {
    VS::Core::LogWarn("effects",
                      "texture animation: {} refused: {} ({} refusals)", call,
                      reason, refused_calls);
  }
}

// The package whose texture or animation an animation names, for reports.
struct PackageContext {
  std::string_view name;
  unsigned request_effect;
};

[[nodiscard]] PackageContext Context() {
  const bool primary = Effects_package == &Effects_actionPackage;
  return {.name = primary ? "primary" : "secondary",
          .request_effect = primary ? Effects_actionRequest.effectId
                                    : Effects_secondaryRequest.effectId};
}

// The current package's frame records of texture table `table`; null when
// the package lacks the table.
[[nodiscard]] LivePackageTextureFrame* BoundTextureTable(int table) {
  if (Effects_package == nullptr || table < 0 ||
      std::cmp_greater_equal(table, kTextureTableCapacity) ||
      table >= Effects_package->block1TableCount) {
    return nullptr;
  }
  return Effects_package->textureInfoTable[table];
}

[[nodiscard]] TextureAnimationState Load(const Animation& animation) {
  return {.frame_timer = animation.frame_timer,
          .frame_flags = animation.frame_flags,
          .frame_offset_x = animation.frame_offset_x,
          .frame_offset_y = animation.frame_offset_y,
          .cursor = animation.cursor,
          .texture_index = animation.texture_index};
}

void Store(Animation& animation, const TextureAnimationState& state) {
  animation.frame_timer = state.frame_timer;
  animation.frame_flags = state.frame_flags;
  animation.frame_offset_x = state.frame_offset_x;
  animation.frame_offset_y = state.frame_offset_y;
  animation.cursor = state.cursor;
  animation.texture_index = state.texture_index;
}

// Animation `index` of the current package's recorded animation table.
[[nodiscard]] std::optional<std::span<std::byte>> AnimationProgram(int index) {
  if (Effects_package == nullptr) {
    return std::nullopt;
  }
  EffectPackageBlocks blocks{};
  BindParticleAnimationTable(blocks,
                             LiveAnimationTable(Effects_package->block2Data));
  return ParticleAnimationData(blocks, index);
}

void SelectFrame(Animation& animation, std::uint16_t frame) {
  LivePackageTextureFrame* const table =
      BoundTextureTable(animation.texture_index);
  if (table == nullptr) {
    if (ShouldReport(unbound_frames)) {
      const PackageContext context = Context();
      VS::Core::LogWarn(
          "effects",
          "texture animation: frame {} of unbound table {} of {} "
          "kept the previous frame (context={} request_effect={}; "
          "{} reports)",
          frame, animation.texture_index,
          Effects_package == nullptr ? 0 : Effects_package->block1TableCount,
          context.name, context.request_effect, unbound_frames);
    }
    return;
  }
  animation.texture_info =
      &std::span(table, static_cast<std::size_t>(frame) + 1).back();
}
}  // namespace

// decomp: BATTLE.PRG func_800D6CCC (src/BATTLE/BATTLE.PRG/5BF94.c:8501)
void LiveClearParticleTextureState(Animation* animation) {
  if (animation == nullptr) {
    Refuse("clear", "no animation");
    return;
  }
  TextureAnimationState state = Load(*animation);
  ClearTextureAnimation(state);
  // The program and frame pointers, which the state does not carry, too.
  *animation = {};
  Store(*animation, state);
}

// decomp: BATTLE.PRG func_800D6CF0 (src/BATTLE/BATTLE.PRG/5BF94.c:8513)
void LiveInitializeParticleTextureAnimation(Animation* animation,
                                            int texture_index,
                                            int program_index) {
  if (animation == nullptr) {
    Refuse("initialize", "no animation");
    return;
  }
  if (BoundTextureTable(texture_index) == nullptr) {
    const PackageContext context = Context();
    VS::Core::LogDebug(
        "effects",
        "texture animation: unbound texture={} tables={} program={} "
        "context={} request_effect={}",
        texture_index,
        Effects_package == nullptr ? 0 : Effects_package->block1TableCount,
        program_index, context.name, context.request_effect);
  }
  TextureAnimationState state = Load(*animation);
  RestartTextureAnimation(state, static_cast<std::uint8_t>(texture_index));
  Store(*animation, state);
  const std::optional<std::span<std::byte>> program =
      AnimationProgram(program_index);
  animation->program =
      program ? reinterpret_cast<u_char*>(program->data()) : nullptr;
  if (!program) {
    Refuse("initialize", "program index outside the animation table");
  }
}

// decomp: BATTLE.PRG func_800D6D24 (src/BATTLE/BATTLE.PRG/5BF94.c:8521)
void LiveAdvanceParticleTextureAnimation(Animation* animation) {
  if (animation == nullptr) {
    Refuse("advance", "no animation");
    return;
  }
  TextureAnimationState state = Load(*animation);
  const TextureAnimationAdvance advance =
      AdvanceTextureAnimation(state, LiveAnimationProgram(animation->program));
  Store(*animation, state);
  if (advance.step == TextureAnimationStep::kRefused) {
    Refuse("advance",
           "program ends, repeats without a frame or holds an unknown "
           "command");
  } else if (advance.step == TextureAnimationStep::kFrame) {
    SelectFrame(*animation, advance.frame);
  }
}
}  // namespace VS::Effects
