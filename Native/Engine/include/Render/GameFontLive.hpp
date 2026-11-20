#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

#include "Legacy/Psx.hpp"
#include "Render/GameFont.hpp"

namespace VS::Menu {
struct TextBoxState;
}  // namespace VS::Menu

// Live binding for GameFont.hpp and TextBoxText.hpp: the font tables (module
// data), the font style the menus and text boxes share, the current frame's
// primitive buffer and ordering tables, the dialogue text boxes
// (textBoxes), the first controller, the text sound and the speaker's
// texture animation (GameFontLive.cpp).
namespace VS::Render {
// BATTLE.PRG _fontTable, D_800F4CB8 and _fontBrightness: the style later
// glyphs draw with.
[[nodiscard]] FontStyle& LiveFontStyle() noexcept;

// BATTLE.PRG vs_battle_renderTextRawColor: in front of `chain`, or of the
// frame's overlay ordering table when it is null.
void DrawLiveCompactTextColor(const char* text, int xy, int color,
                              std::uint32_t* chain);
// BATTLE.PRG vs_battle_renderTextRaw: in gray.
void DrawLiveCompactText(const char* text, int xy, void* chain);
// BATTLE.PRG vs_battle_printVariableWidthFontChar: in front of entry 3 of the
// four text box layers at `layers`.
int DrawLiveVariableWidthGlyph(std::uint32_t glyph, int x, int y,
                               std::uint32_t* layers);
// BATTLE.PRG vs_battle_setFontStyle.
void SetLiveMenuFontStyle(int palette_offset);

// The text box drawers. The box's text is trusted for
// kTrustedEncodedTextBytes. The progressive and scaled drawers draw into the
// box's layers of the active text box ordering table, so a box outside
// textBoxes is refused and reported.
void DrawLiveProgressiveTextBoxText(VS::Menu::TextBoxState* box);
void DrawLiveScaledTextBoxText(VS::Menu::TextBoxState* box, int scale);
void BlendLiveTextBoxIntoFramebuffer(VS::Menu::TextBoxState* box,
                                     int glyph_alpha);

// Text box draws refused, or that referenced a missing string or overran,
// since start-up.
[[nodiscard]] std::uint64_t RefusedLiveTextBoxCount() noexcept;
}  // namespace VS::Render
