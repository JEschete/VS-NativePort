#include "Menu/TextBoxTextLive.hpp"

#include <bit>
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Actors/TextureAnimationLive.hpp"
#include "Audio/SfxLive.hpp"
#include "Battle/StringContextLive.hpp"
#include "Battle/TargetingState.hpp"
#include "Core/Diagnostics.hpp"
#include "Input/Buttons.hpp"
#include "Input/ControllerLiveState.hpp"
#include "Input/Pad.hpp"
#include "Menu/Framework/TextBoxes.hpp"
#include "Menu/TextBox.hpp"
#include "Render/EncodedTextLive.hpp"
#include "Render/GameFontLive.hpp"
#include "Render/TextBoxText.hpp"
#include "State/Timers.hpp"

namespace VS::Menu {
namespace {
// Each text box draws into four entries of the active text box ordering
// table (D_800F51B8).
constexpr std::size_t kTextBoxLayers = 4;
constexpr int kTextSoundGroup = 0x7E;

std::uint64_t textBoxProblems = 0;

void ReportTextBox(const TextBoxState* box, const char* problem) {
  ++textBoxProblems;
  // The first and then every power-of-two report, so a per-frame problem
  // cannot flood the log.
  if (std::has_single_bit(textBoxProblems)) {
    VS::Core::Errorf("text", "text box %p %s (%" PRIu64 " text box problems)",
                     static_cast<const void*>(box), problem, textBoxProblems);
  }
}

// The box's layers: its index in textBoxes selects them (the original
// kept the index in D_800F4E80 while drawing the box).
std::uint32_t* TextBoxLayers(const TextBoxState* box) {
  const std::span boxes(textBoxes);
  for (std::size_t index = 0; index < boxes.size(); ++index) {
    if (&boxes[index] == box) {
      return activeTextBoxOrderingTable != nullptr
                 ? activeTextBoxOrderingTable + (index * kTextBoxLayers)
                 : nullptr;
    }
  }
  return nullptr;
}

Render::TextBoxText ViewOf(const TextBoxState& box) {
  Render::TextBoxText view;
  view.text = Render::encoded_text_live::TrustedText(box.text);
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

void StoreProgress(TextBoxState& box, const Render::TextBoxText& view) {
  box.flags.arrow_frame = view.arrow_frame;
  box.flags.done = view.done ? 1U : 0U;
  box.speed = view.speed;
  box.previous_chunks = view.previous_chunks;
  box.speaker_pause = view.speaker_pause;
}

// The next page starts in the box's own text or in a referenced string; a
// printed integer holds no page break.
char* PageStart(const TextBoxState& box, const Render::TextPosition& page) {
  char* text = nullptr;
  if (page.source == Render::kTextBoxString) {
    text = box.text;
  } else if (page.source >= 0 && page.source < Render::kPrintedIntegerText) {
    text = std::span(VS::Battle::stringContext
                         .strings)[static_cast<std::size_t>(page.source)];
  }
  return text != nullptr ? text + page.offset : nullptr;
}
}  // namespace

std::uint64_t RefusedLiveTextBoxCount() noexcept { return textBoxProblems; }

void DrawLiveProgressiveTextBoxText(TextBoxState* box) {
  if (box == nullptr || box->speed == 0) {
    return;
  }
  std::uint32_t* const layers = TextBoxLayers(box);
  if (layers == nullptr) {
    ReportTextBox(box, "is not a dialogue text box or has no ordering table");
    return;
  }
  Render::TextBoxText view = ViewOf(*box);
  const Render::TextBoxInput input{
      .confirm_pressed =
          VS::Input::LowButtonByte(VS::Input::buttonsPressed) != 0,
      .tick_speed = VS::State::tickSpeed};
  const std::optional<Render::ProgressiveTextResult> drawn =
      Render::DrawLiveProgressiveText(
          view, input, VS::Battle::LiveEncodedTextArguments(), layers);
  if (!drawn) {
    return;
  }
  const Render::ProgressiveTextResult& result = *drawn;
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

void DrawLiveScaledTextBoxText(TextBoxState* box, int scale) {
  if (box == nullptr || box->speed == 0) {
    return;
  }
  std::uint32_t* const layers = TextBoxLayers(box);
  if (layers == nullptr) {
    ReportTextBox(box, "is not a dialogue text box or has no ordering table");
    return;
  }
  Render::DrawLiveScaledText(ViewOf(*box), scale, layers);
}

void BlendLiveTextBoxIntoFramebuffer(TextBoxState* box, int glyph_alpha) {
  if (box == nullptr) {
    ReportTextBox(box, "is null");
    return;
  }
  Render::TextBoxText view = ViewOf(*box);
  Render::BlendLiveTextIntoFramebuffer(
      view, glyph_alpha, (VS::Input::buttonsPressed & VS::Input::Circle) != 0);
  box->speed = view.speed;
}
}  // namespace VS::Menu
