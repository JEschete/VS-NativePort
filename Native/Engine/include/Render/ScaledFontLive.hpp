#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Legacy/Psx.hpp"
#include "Render/ScaledFont.hpp"

namespace VS::Render {
// Glyph quads queued into the current frame's primitive buffer in front of a
// text box layer; a quad that does not fit is dropped and reported
// (ScaledFontLive.cpp).
class LiveScaledGlyphTarget final : public ScaledGlyphTarget {
 public:
  // Null when the box has no layer to draw into.
  explicit LiveScaledGlyphTarget(std::uint32_t* layer) : layer_(layer) {}

  [[nodiscard]] bool CanDraw() const override { return layer_ != nullptr; }
  void Draw(const ScaledGlyphQuad& quad) override;

 private:
  std::uint32_t* layer_;
};
}  // namespace VS::Render
