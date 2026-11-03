// Hand-maintained (tools/port/owned.csv).
// Live binding for the menu and dialogue fonts (GameFont.cpp,
// TextBoxText.cpp): the font tables, the shared font style, the frame's
// primitive buffer and VRAM, the dialogue text boxes, the first controller,
// the text sound and the speaker's texture animation.
#include "Render/GameFontLive.hpp"

#include <bit>
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string_view>

#include "Legacy/Psx.h"
#include "Render/DisplayControl.hpp"
#include "Render/EncodedText.hpp"
#include "Render/EncodedTextLive.hpp"
#include "Render/GameFont.hpp"
#include "Render/ScaledFontLive.hpp"
#include "Render/TextBoxText.hpp"
#include "Render/UiPacketsLive.hpp"

extern "C" {
#include "Actors/TextureAnimation.h"
#include "Audio/Sfx.h"
#include "Battle/Targeting.h"
#include "Core/Diagnostics.h"
#include "Input/ControllerLiveState.h"
#include "Menu/Framework/TextBoxes.h"
#include "Menu/TextBox.h"
#include "Render/Display.h"
#include "Render/Frame.h"
#include "Render/Render.h"
#include "Render/Text/CompactFont.h"
#include "Render/Text/VariableFont.h"
#include "Render/UiPrimitives.h"
#include "State/Timers.h"
}

namespace VS::Render {
namespace {
// Each text box draws into four entries of the active text box ordering
// table (D_800F51B8); its glyphs go in front of the last.
constexpr std::size_t kTextBoxLayers = 4;
constexpr std::size_t kTextForegroundLayer = 3;
constexpr std::size_t kTrustedCompactTextBytes = 0x1000;
constexpr int kTextSoundGroup = 0x7E;
constexpr int kFramebufferWidth = 320;

FontStyle font_style;
std::uint64_t text_box_problems = 0;

void ReportTextBox(const Menu_TextBoxState* box, const char* problem) {
  ++text_box_problems;
  // The first and then every power-of-two report, so a per-frame problem
  // cannot flood the log.
  if (std::has_single_bit(text_box_problems)) {
    Core_Errorf("text", "text box %p %s (%" PRIu64 " text box problems)",
                static_cast<const void*>(box), problem, text_box_problems);
  }
}

// The box's layers: its index in Menu_textBoxes selects them (the original
// kept the index in D_800F4E80 while Menu_UpdateTextBoxes drew the box).
u_long* TextBoxLayers(const Menu_TextBoxState* box) {
  const std::span boxes(Menu_textBoxes);
  for (std::size_t index = 0; index < boxes.size(); ++index) {
    if (&boxes[index] == box) {
      return Menu_activeTextBoxOrderingTable != nullptr
                 ? Menu_activeTextBoxOrderingTable + (index * kTextBoxLayers)
                 : nullptr;
    }
  }
  return nullptr;
}

// Menu_TextBoxState's encoded text field, whose name cpplint takes for
// std::string.
char* BoxText(const Menu_TextBoxState& box) {
  return box.string;  // NOLINT(build/include_what_you_use)
}

void SetBoxText(Menu_TextBoxState& box, char* text) {
  box.string = text;  // NOLINT(build/include_what_you_use)
}

TextBoxText ViewOf(const Menu_TextBoxState& box) {
  TextBoxText view;
  view.text = encoded_text_live::TrustedText(BoxText(box));
  view.kind = static_cast<std::uint8_t>(box.flags.kind);
  view.arrow_frame = static_cast<std::uint8_t>(box.flags.arrowAnimFrame);
  view.speaker_texture = static_cast<std::uint8_t>(box.flags.speakerTexture);
  view.done = box.flags.done != 0;
  view.speed = box.speed;
  view.previous_chunks = box.prevChunksPrinted;
  view.x_indent = box.xIndent;
  view.y_indent = box.yIndent;
  view.x = box.x;
  view.y = box.y;
  view.frame_x = box.frameX;
  view.frame_y = box.frameY;
  view.brightness = box.brightness;
  view.chars_per_line = static_cast<std::int8_t>(box.charsPerLine);
  view.line_count = static_cast<std::int8_t>(box.lineCount);
  view.speaker_pause = box.opaque2C;
  return view;
}

void StoreProgress(Menu_TextBoxState& box, const TextBoxText& view) {
  box.flags.arrowAnimFrame = view.arrow_frame;
  box.flags.done = view.done ? 1U : 0U;
  box.speed = view.speed;
  box.prevChunksPrinted = view.previous_chunks;
  box.opaque2C = view.speaker_pause;
}

// The next page starts in the box's own text or in a referenced string; a
// printed integer holds no page break.
char* PageStart(const Menu_TextBoxState& box, const TextPosition& page) {
  char* text = nullptr;
  if (page.source == kTextBoxString) {
    text = BoxText(box);
  } else if (page.source >= 0 && page.source < kPrintedIntegerText) {
    text = std::span(
        Battle_stringContext.strings)[static_cast<std::size_t>(page.source)];
  }
  return text != nullptr ? text + page.offset : nullptr;
}

std::string_view CompactText(const char* text) {
  if (text == nullptr) {
    return {};
  }
  const void* end = std::memchr(text, '\0', kTrustedCompactTextBytes);
  return {text, end != nullptr ? static_cast<std::size_t>(
                                     static_cast<const char*>(end) - text)
                               : kTrustedCompactTextBytes};
}

CompactFontTables LiveCompactFontTables() {
  return {.widths = Render_compactGlyphWidths,
          .left_trim = Render_compactGlyphLeftTrim,
          .indices = Render_compactGlyphIndices};
}

[[nodiscard]] bool Covers(const ScreenRect& area, std::size_t pixels) {
  return area.width >= 0 && area.height >= 0 &&
         pixels >= static_cast<std::size_t>(area.width) *
                       static_cast<std::size_t>(area.height);
}

// The framebuffer area around a text box, read and written through the GPU.
class LiveVramPixels final : public VramPixelTarget {
 public:
  void Read(const ScreenRect& area, std::span<std::uint16_t> pixels) override {
    if (!Covers(area, pixels.size())) {
      ReportTextBox(nullptr, "framebuffer read has too few pixels");
      return;
    }
    RECT rect{};
    setRECT(&rect, area.x, area.y, area.width, area.height);
    StoreImage(&rect, reinterpret_cast<u_long*>(pixels.data()));
  }

  void Upload(const ScreenRect& area,
              std::span<const std::uint16_t> pixels) override {
    if (!Covers(area, pixels.size())) {
      ReportTextBox(nullptr, "framebuffer upload has too few pixels");
      return;
    }
    RECT rect{};
    setRECT(&rect, area.x, area.y, area.width, area.height);
    // LoadImage only reads the pixels.
    LoadImage(&rect, reinterpret_cast<u_long*>(
                         const_cast<std::uint16_t*>(pixels.data())));
  }
};
}  // namespace

FontStyle& LiveFontStyle() noexcept { return font_style; }

std::uint64_t RefusedLiveTextBoxCount() noexcept { return text_box_problems; }

void DrawLiveCompactTextColor(const char* text, int xy, int color,
                              u_long* chain) {
  LivePacketQueue queue(
      chain != nullptr
          ? chain
          : static_cast<u_long*>(Render_frameScratch.overlayOrderingTable));
  DrawCompactText(queue, LiveCompactFontTables(), CompactText(text), xy,
                  static_cast<std::uint32_t>(color));
}

void DrawLiveCompactText(const char* text, int xy, void* chain) {
  DrawLiveCompactTextColor(text, xy, static_cast<int>(kCompactTextGray),
                           static_cast<u_long*>(chain));
}

int DrawLiveVariableWidthGlyph(u_int glyph, int x, int y, u_long* layers) {
  LivePacketQueue queue(layers != nullptr ? layers + kTextForegroundLayer
                                          : nullptr);
  return DrawVariableWidthGlyph(queue, font_style,
                                LiveVariableFontGlyphWidths(), glyph, x, y);
}

void SetLiveMenuFontStyle(int palette_offset) {
  SetMenuFontStyle(font_style, static_cast<std::uint8_t>(palette_offset));
}

void DrawLiveProgressiveTextBoxText(Menu_TextBoxState* box) {
  if (box == nullptr || box->speed == 0) {
    return;
  }
  u_long* const layers = TextBoxLayers(box);
  if (layers == nullptr) {
    ReportTextBox(box, "is not a dialogue text box or has no ordering table");
    return;
  }
  TextBoxText view = ViewOf(*box);
  const TextBoxInput input{
      .confirm_pressed = Input_buttonsPressed.ports[0].lowByte != 0,
      .tick_speed = State_tickSpeed};
  LivePacketQueue glyphs(layers + kTextForegroundLayer);
  const ProgressiveTextResult result = DrawProgressiveTextBoxText(
      view, input, font_style, LiveVariableFontGlyphWidths(),
      LiveEncodedTextArguments(), glyphs);
  StoreProgress(*box, view);
  if (result.next_page) {
    if (char* const page = PageStart(*box, *result.next_page)) {
      SetBoxText(*box, page);
    } else {
      ReportTextBox(box, "broke its page inside a printed integer");
    }
  }
  if (result.sound != 0) {
    Audio_PlaySfxCentered(kTextSoundGroup, result.sound);
  }
  if (result.reset_speaker) {
    Actors_SetActorTextureAnimation(view.speaker_texture, 0, 0, 0);
  }
  if (result.missing_strings != 0) {
    ReportTextBox(box, "referenced a string that is not set");
  }
  if (result.overran) {
    ReportTextBox(box, "stopped at the step limit");
  }
}

void DrawLiveScaledTextBoxText(Menu_TextBoxState* box, int scale) {
  if (box == nullptr || box->speed == 0) {
    return;
  }
  u_long* const layers = TextBoxLayers(box);
  if (layers == nullptr) {
    ReportTextBox(box, "is not a dialogue text box or has no ordering table");
    return;
  }
  LiveScaledGlyphTarget target(layers + kTextForegroundLayer);
  DrawScaledTextBoxText(ViewOf(*box), scale, font_style,
                        LiveVariableFontGlyphWidths(), target);
}

void BlendLiveTextBoxIntoFramebuffer(Menu_TextBoxState* box, int glyph_alpha) {
  if (box == nullptr) {
    ReportTextBox(box, "is null");
    return;
  }
  TextBoxText view = ViewOf(*box);
  const FramebufferTextBlend blend{
      .alpha = glyph_alpha,
      .framebuffer_x = Render_frameBuf * kFramebufferWidth,
      .skip_pressed = (Input_buttonsPressed.allButtons & PADRright) != 0};
  const VariableFontTables font{.glyph_widths = LiveVariableFontGlyphWidths(),
                                .pixel_expansion = Render_kGlyphPixelExpansion};
  LiveVramPixels vram;
  BlendTextBoxIntoFramebuffer(view, blend, font_style, font, vram);
  box->speed = view.speed;
}
}  // namespace VS::Render
