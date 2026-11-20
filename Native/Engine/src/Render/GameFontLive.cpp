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

#include "Actors/TextureAnimationLive.hpp"
#include "Audio/SfxLive.hpp"
#include "Battle/TargetingState.hpp"
#include "Core/Diagnostics.hpp"
#include "Input/ControllerLiveState.hpp"
#include "Legacy/Psx.hpp"
#include "Menu/Framework/TextBoxes.hpp"
#include "Menu/TextBox.hpp"
#include "Render/Display.hpp"
#include "Render/DisplayControl.hpp"
#include "Render/EncodedText.hpp"
#include "Render/EncodedTextLive.hpp"
#include "Render/Frame.hpp"
#include "Render/GameFont.hpp"
#include "Render/Render.hpp"
#include "Render/ScaledFontLive.hpp"
#include "Render/Text/CompactFont.hpp"
#include "Render/Text/VariableFont.hpp"
#include "Render/TextBoxText.hpp"
#include "Render/UiPacketsLive.hpp"
#include "Render/UiPrimitives.hpp"
#include "State/Timers.hpp"

using VS::Legacy::LoadImage;
using VS::Legacy::RECT;
using VS::Legacy::setRECT;
using VS::Legacy::StoreImage;

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

void ReportTextBox(const VS::Menu::TextBoxState* box, const char* problem) {
  ++text_box_problems;
  // The first and then every power-of-two report, so a per-frame problem
  // cannot flood the log.
  if (std::has_single_bit(text_box_problems)) {
    VS::Core::Errorf("text", "text box %p %s (%" PRIu64 " text box problems)",
                     static_cast<const void*>(box), problem, text_box_problems);
  }
}

// The box's layers: its index in textBoxes selects them (the original
// kept the index in D_800F4E80 while drawing the box).
std::uint32_t* TextBoxLayers(const VS::Menu::TextBoxState* box) {
  const std::span boxes(VS::Menu::textBoxes);
  for (std::size_t index = 0; index < boxes.size(); ++index) {
    if (&boxes[index] == box) {
      return VS::Menu::activeTextBoxOrderingTable != nullptr
                 ? VS::Menu::activeTextBoxOrderingTable +
                       (index * kTextBoxLayers)
                 : nullptr;
    }
  }
  return nullptr;
}

TextBoxText ViewOf(const VS::Menu::TextBoxState& box) {
  TextBoxText view;
  view.text = encoded_text_live::TrustedText(box.text);
  view.kind = static_cast<std::uint8_t>(box.flags.kind);
  view.arrow_frame = static_cast<std::uint8_t>(box.flags.arrow_frame);
  view.speaker_texture = static_cast<std::uint8_t>(box.flags.speaker_texture);
  view.done = box.flags.done != 0;
  view.speed = box.speed;
  view.previous_chunks = box.previous_chunks;
  view.x_indent = box.x_indent;
  view.y_indent = box.y_indent;
  view.x = box.x;
  view.y = box.y;
  view.frame_x = box.frame_x;
  view.frame_y = box.frame_y;
  view.brightness = box.brightness;
  view.chars_per_line = box.chars_per_line;
  view.line_count = box.line_count;
  view.speaker_pause = box.speaker_pause;
  return view;
}

void StoreProgress(VS::Menu::TextBoxState& box, const TextBoxText& view) {
  box.flags.arrow_frame = view.arrow_frame;
  box.flags.done = view.done ? 1U : 0U;
  box.speed = view.speed;
  box.previous_chunks = view.previous_chunks;
  box.speaker_pause = view.speaker_pause;
}

// The next page starts in the box's own text or in a referenced string; a
// printed integer holds no page break.
char* PageStart(const VS::Menu::TextBoxState& box, const TextPosition& page) {
  char* text = nullptr;
  if (page.source == kTextBoxString) {
    text = box.text;
  } else if (page.source >= 0 && page.source < kPrintedIntegerText) {
    text = std::span(VS::Battle::stringContext
                         .strings)[static_cast<std::size_t>(page.source)];
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
  return {.widths = compactGlyphWidths,
          .left_trim = compactGlyphLeftTrim,
          .indices = compactGlyphIndices};
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
    StoreImage(&rect, reinterpret_cast<std::uint32_t*>(pixels.data()));
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
    LoadImage(&rect, reinterpret_cast<std::uint32_t*>(
                         const_cast<std::uint16_t*>(pixels.data())));
  }
};
}  // namespace

FontStyle& LiveFontStyle() noexcept { return font_style; }

std::uint64_t RefusedLiveTextBoxCount() noexcept { return text_box_problems; }

void DrawLiveCompactTextColor(const char* text, int xy, int color,
                              std::uint32_t* chain) {
  LivePacketQueue queue(chain != nullptr
                            ? chain
                            : static_cast<std::uint32_t*>(
                                  FrameScratchpad().overlay_ordering_table));
  DrawCompactText(queue, LiveCompactFontTables(), CompactText(text), xy,
                  static_cast<std::uint32_t>(color));
}

void DrawLiveCompactText(const char* text, int xy, void* chain) {
  DrawLiveCompactTextColor(text, xy, static_cast<int>(kCompactTextGray),
                           static_cast<std::uint32_t*>(chain));
}

int DrawLiveVariableWidthGlyph(std::uint32_t glyph, int x, int y,
                               std::uint32_t* layers) {
  LivePacketQueue queue(layers != nullptr ? layers + kTextForegroundLayer
                                          : nullptr);
  return DrawVariableWidthGlyph(queue, font_style,
                                LiveVariableFontGlyphWidths(), glyph, x, y);
}

void SetLiveMenuFontStyle(int palette_offset) {
  SetMenuFontStyle(font_style, static_cast<std::uint8_t>(palette_offset));
}

void DrawLiveProgressiveTextBoxText(VS::Menu::TextBoxState* box) {
  if (box == nullptr || box->speed == 0) {
    return;
  }
  std::uint32_t* const layers = TextBoxLayers(box);
  if (layers == nullptr) {
    ReportTextBox(box, "is not a dialogue text box or has no ordering table");
    return;
  }
  TextBoxText view = ViewOf(*box);
  const TextBoxInput input{
      .confirm_pressed =
          VS::Input::LowButtonByte(VS::Input::buttonsPressed) != 0,
      .tick_speed = VS::State::tickSpeed};
  LivePacketQueue glyphs(layers + kTextForegroundLayer);
  const ProgressiveTextResult result = DrawProgressiveTextBoxText(
      view, input, font_style, LiveVariableFontGlyphWidths(),
      LiveEncodedTextArguments(), glyphs);
  StoreProgress(*box, view);
  if (result.next_page) {
    if (char* const page = PageStart(*box, *result.next_page)) {
      box->text = page;
    } else {
      ReportTextBox(box, "broke its page inside a printed integer");
    }
  }
  if (result.sound != 0) {
    VS::Audio::LivePlaySfxCentered(kTextSoundGroup, result.sound);
  }
  if (result.reset_speaker) {
    VS::Actors::SetLiveActorTextureAnimation(view.speaker_texture, 0, 0, 0);
  }
  if (result.missing_strings != 0) {
    ReportTextBox(box, "referenced a string that is not set");
  }
  if (result.overran) {
    ReportTextBox(box, "stopped at the step limit");
  }
}

void DrawLiveScaledTextBoxText(VS::Menu::TextBoxState* box, int scale) {
  if (box == nullptr || box->speed == 0) {
    return;
  }
  std::uint32_t* const layers = TextBoxLayers(box);
  if (layers == nullptr) {
    ReportTextBox(box, "is not a dialogue text box or has no ordering table");
    return;
  }
  LiveScaledGlyphTarget target(layers + kTextForegroundLayer);
  DrawScaledTextBoxText(ViewOf(*box), scale, font_style,
                        LiveVariableFontGlyphWidths(), target);
}

void BlendLiveTextBoxIntoFramebuffer(VS::Menu::TextBoxState* box,
                                     int glyph_alpha) {
  if (box == nullptr) {
    ReportTextBox(box, "is null");
    return;
  }
  TextBoxText view = ViewOf(*box);
  const FramebufferTextBlend blend{
      .alpha = glyph_alpha,
      .framebuffer_x = frameBuf * kFramebufferWidth,
      .skip_pressed =
          (VS::Input::buttonsPressed & VS::Legacy::kPadRright) != 0};
  const VariableFontTables font{.glyph_widths = LiveVariableFontGlyphWidths(),
                                .pixel_expansion = kGlyphPixelExpansion};
  LiveVramPixels vram;
  BlendTextBoxIntoFramebuffer(view, blend, font_style, font, vram);
  box->speed = view.speed;
}
}  // namespace VS::Render
