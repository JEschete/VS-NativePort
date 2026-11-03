#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

#include "Legacy/Psx.h"
#include "Render/GameFont.hpp"

struct Menu_TextBoxState;

// Live binding for GameFont.hpp and TextBoxText.hpp: the font tables (module
// data), the font style the menus and text boxes share, the current frame's
// primitive buffer and ordering tables, the dialogue text boxes
// (Menu_textBoxes), the first controller, the text sound and the speaker's
// texture animation (GameFontLive.cpp).
namespace VS::Render {
// BATTLE.PRG _fontTable, D_800F4CB8 and _fontBrightness: the style later
// glyphs draw with.
[[nodiscard]] FontStyle& LiveFontStyle() noexcept;

// BATTLE.PRG vs_battle_renderTextRawColor: in front of `chain`, or of the
// frame's overlay ordering table when it is null.
void DrawLiveCompactTextColor(const char* text, int xy, int color,
                              u_long* chain);
// BATTLE.PRG vs_battle_renderTextRaw: in gray.
void DrawLiveCompactText(const char* text, int xy, void* chain);
// BATTLE.PRG vs_battle_printVariableWidthFontChar: in front of entry 3 of the
// four text box layers at `layers`.
int DrawLiveVariableWidthGlyph(u_int glyph, int x, int y, u_long* layers);
// BATTLE.PRG vs_battle_setFontStyle.
void SetLiveMenuFontStyle(int palette_offset);

// The text box drawers. The box's text is trusted for
// kTrustedEncodedTextBytes. The progressive and scaled drawers draw into the
// box's layers of the active text box ordering table, so a box outside
// Menu_textBoxes is refused and reported.
void DrawLiveProgressiveTextBoxText(Menu_TextBoxState* box);
void DrawLiveScaledTextBoxText(Menu_TextBoxState* box, int scale);
void BlendLiveTextBoxIntoFramebuffer(Menu_TextBoxState* box, int glyph_alpha);

// Text box draws refused, or that referenced a missing string or overran,
// since start-up.
[[nodiscard]] std::uint64_t RefusedLiveTextBoxCount() noexcept;
}  // namespace VS::Render
