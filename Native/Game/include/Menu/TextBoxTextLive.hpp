#pragma once

#include <cstdint>

namespace VS::Menu {
struct TextBoxState;

// The dialogue text boxes' text drawers (TextBoxTextLive.cpp): each binds a
// box of textBoxes, its four layers of the active text box ordering table,
// the first controller, the tick speed and the battle string context to the
// Render text drawers (Render/GameFontLive.hpp). The box's text is trusted
// for Render::kTrustedEncodedTextBytes. A box outside textBoxes, or a frame
// without a text box ordering table, is refused and reported.

// decomp: F01334 printVariableWidthFont
// (src/BATTLE/BATTLE.PRG/5BF94.c:1253).
void DrawLiveProgressiveTextBoxText(TextBoxState* box);
// decomp: F01335 printFixedWidthFont (src/BATTLE/BATTLE.PRG/5BF94.c:1643).
void DrawLiveScaledTextBoxText(TextBoxState* box, int scale);
// decomp: F01337 renderTextImmediate (src/BATTLE/BATTLE.PRG/5BF94.c:1783).
void BlendLiveTextBoxIntoFramebuffer(TextBoxState* box, int glyph_alpha);

// Text box draws refused, or that referenced a missing string, broke a page
// inside a printed integer or overran, since start-up.
[[nodiscard]] std::uint64_t RefusedLiveTextBoxCount() noexcept;
}  // namespace VS::Menu
