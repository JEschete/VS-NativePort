#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

#include "Render/UiTexturedQuads.hpp"

namespace VS::Battle {
struct FloatingLabelAppearance {
  std::uint8_t elapsed_frames = 0;
  std::uint16_t value = 0;
  std::uint8_t raw_mode = 0;
  std::uint8_t text_length = 0;
};

struct FloatingLabelScreenPoint {
  std::int32_t x = 0;
  std::int32_t y = 0;
};

struct FloatingLabelAtlasRect {
  std::int16_t u = 0;
  std::int16_t v = 0;
  std::int16_t width = 0;
  std::int16_t height = 0;
};

using FloatingLabelQuad = Render::UiTexturedQuad;

class FloatingLabelRenderWorld {
 public:
  virtual ~FloatingLabelRenderWorld() = default;
  virtual std::optional<FloatingLabelAtlasRect> ValueCaption(
      std::uint8_t raw_mode) = 0;
  virtual std::optional<std::int16_t> LockedHalfHeight(std::int32_t step) = 0;
  virtual std::optional<FloatingLabelAtlasRect> LockedPiece(
      std::size_t piece) = 0;
  virtual bool DrawQuad(const FloatingLabelQuad& quad) = 0;
  virtual bool DrawText(std::string_view text, FloatingLabelScreenPoint origin,
                        std::uint32_t color) = 0;
};

// Each draw commits in order. A refused later draw leaves earlier draws intact.
// decomp: F00723 func_80094E18 (src/BATTLE/BATTLE.PRG/2842C.c:2138).
std::optional<std::int32_t> DrawValueLabelCaption(
    FloatingLabelRenderWorld& world, FloatingLabelScreenPoint origin,
    const FloatingLabelAppearance& label);
// decomp: F00724 func_8009506C (src/BATTLE/BATTLE.PRG/2842C.c:2167).
bool DrawLockedFloatingLabel(FloatingLabelRenderWorld& world,
                             FloatingLabelScreenPoint origin,
                             const FloatingLabelAppearance& label);
// decomp: F00726 func_800957D0 (src/BATTLE/BATTLE.PRG/2842C.c:2312).
bool DrawStatusRemovalMarker(FloatingLabelRenderWorld& world,
                             FloatingLabelScreenPoint origin,
                             std::int32_t animation_step);
// decomp: F00727 func_800958A4 (src/BATTLE/BATTLE.PRG/2842C.c:2332).
bool DrawAbilityTimingResult(FloatingLabelRenderWorld& world,
                             FloatingLabelScreenPoint origin,
                             const FloatingLabelAppearance& label);
// decomp: F00728 func_80095A4C (src/BATTLE/BATTLE.PRG/2842C.c:2359).
bool DrawChainCount(FloatingLabelRenderWorld& world,
                    FloatingLabelScreenPoint origin,
                    const FloatingLabelAppearance& label);
// The parent updater, not this leaf, owns cue visibility and countdown.
// decomp: F00741 func_800970BC (src/BATTLE/BATTLE.PRG/2842C.c:2864).
bool DrawAbilityTimingCue(FloatingLabelRenderWorld& world,
                          FloatingLabelScreenPoint origin);
}  // namespace VS::Battle
