#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>
#include <optional>

#include "Render/EncodedText.hpp"
#include "Render/GameFont.hpp"
#include "Render/TextBoxText.hpp"

// Live binding for GameFont.hpp and TextBoxText.hpp: the font tables (module
// data), the font style the menus and text boxes share, the current frame's
// primitive buffer and ordering tables and VRAM (GameFontLive.cpp). The caller
// supplies the text box's text, its four ordering-table layers, the frame's
// input and the encoded-text arguments; Game binds the dialogue text boxes
// (Menu/TextBoxTextLive.hpp).
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

// The text drawers over a text box's text with the live font style and
// tables. The progressive and scaled drawers draw in front of entry 3 of the
// four text box layers at `layers`; null layers are refused, reported and
// counted.
//
// BATTLE.PRG _printVariableWidthFont through DrawProgressiveTextBoxText;
// nullopt when refused.
[[nodiscard]] std::optional<ProgressiveTextResult> DrawLiveProgressiveText(
    TextBoxText& box, const TextBoxInput& input,
    const EncodedTextArguments& arguments, std::uint32_t* layers);
// BATTLE.PRG _printFixedWidthFont through DrawScaledTextBoxText.
void DrawLiveScaledText(const TextBoxText& box, int scale,
                        std::uint32_t* layers);
// BATTLE.PRG _renderTextImmediate through BlendTextBoxIntoFramebuffer, into
// the drawn framebuffer (frameBuf) in VRAM.
void BlendLiveTextIntoFramebuffer(TextBoxText& box, int glyph_alpha,
                                  bool skip_pressed);

// Text draws refused since start-up: null layers, or a VRAM area larger than
// its pixels.
[[nodiscard]] std::uint64_t RefusedLiveTextCount() noexcept;
}  // namespace VS::Render
