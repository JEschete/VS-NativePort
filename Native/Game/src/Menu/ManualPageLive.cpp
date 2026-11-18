// Hand-maintained (tools/port/owned.csv).
// Live binding for the manual page's frame (ManualPage.cpp): the help text
// file on the game heap, the back buffer's draw environment, the page arrow
// counter, last frame's buttons and the current frame's primitive buffer.
#include "Menu/ManualPageLive.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "Core/Log.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Input/ControllerLiveState.hpp"
#include "Legacy/Psx.h"
#include "Menu/Manual.h"
#include "Menu/ManualIllustrationLive.hpp"
#include "Menu/ManualPage.hpp"
#include "Render/Display.h"
#include "Render/DisplayControl.hpp"
#include "Render/SpritesLive.hpp"
#include "Render/UiPackets.hpp"
#include "Render/UiPacketsLive.hpp"

namespace VS::Menu::manual {
namespace {
constexpr std::uint32_t kRefusalLogLimit = 16;
constexpr std::uint32_t kRefusalLogInterval = 1024;
constexpr std::size_t kTagWords = 1;
constexpr std::size_t kDrawAreaWords = 2;
static_assert(sizeof(u_long) == sizeof(std::uint32_t));
static_assert(sizeof(DR_AREA) == (kTagWords + kDrawAreaWords) * sizeof(u_long));
std::uint64_t g_refusals = 0;

void Refuse(std::string_view reason) {
  ++g_refusals;
  if (g_refusals <= kRefusalLogLimit || g_refusals % kRefusalLogInterval == 0) {
    VS_LOG_WARN("menu.manual", "{} (refusal {})", reason, g_refusals);
  }
}

u_long* SlotEntry(std::int32_t slot) {
  return Render::LiveFrameOrderingEntry(Render::LiveOrderingBase::kScene, slot);
}

// Links the tag and `body` in front of ordering-table entry `slot`.
template <std::size_t Words>
void EmitPacket(std::int32_t slot,
                const std::array<std::uint32_t, Words>& body) {
  Render::LivePacketQueue queue(SlotEntry(slot));
  std::array<std::uint32_t, Words + kTagWords> packet{};
  packet[0] = Render::PackTagWord(Words, queue.ChainLink());
  std::ranges::copy(body, packet.begin() + kTagWords);
  queue.Emit(packet);
}

// Queues each primitive into the current frame.
class LivePrimitiveTarget final : public PagePrimitiveTarget {
 public:
  void DrawGradientQuad(std::int32_t slot, const GradientQuad& quad) override {
    EmitPacket(slot, PackGradientQuad(quad));
  }
  void DrawTexturedQuad(std::int32_t slot, const TexturedQuad& quad) override {
    EmitPacket(slot, PackTexturedQuad(quad));
  }
  void DrawTile(std::int32_t slot, const PageTile& tile) override {
    EmitPacket(slot, PackPageTile(tile));
  }
  void DrawGradientLine(std::int32_t slot, const GradientLine& line) override {
    EmitPacket(slot, PackGradientLine(line));
  }
  void DrawFlatQuad(std::int32_t slot, const FlatQuad& quad) override {
    EmitPacket(slot, PackFlatQuad(quad));
  }
  void SetTexturePage(std::int32_t slot, std::uint16_t texture_page) override {
    EmitPacket(slot, PackTexturePage(texture_page));
  }
};

// Tallies a call's packets and checks each slot, so a call that does not
// fit is refused before anything is linked.
class BudgetTarget final : public PagePrimitiveTarget {
 public:
  void DrawGradientQuad(std::int32_t slot, const GradientQuad&) override {
    Add(slot, kGradientQuadWords);
  }
  void DrawTexturedQuad(std::int32_t slot, const TexturedQuad&) override {
    Add(slot, kTexturedQuadWords);
  }
  void DrawTile(std::int32_t slot, const PageTile&) override {
    Add(slot, kPageTileWords);
  }
  void DrawGradientLine(std::int32_t slot, const GradientLine&) override {
    Add(slot, kGradientLineWords);
  }
  void DrawFlatQuad(std::int32_t slot, const FlatQuad&) override {
    Add(slot, kFlatQuadWords);
  }
  void SetTexturePage(std::int32_t slot, std::uint16_t) override {
    Add(slot, kTexturePageWords);
  }

  [[nodiscard]] bool SlotsValid() const { return slots_valid_; }
  [[nodiscard]] std::size_t Words() const { return words_; }

 private:
  void Add(std::int32_t slot, std::size_t words) {
    words_ += kTagWords + words;
    if (SlotEntry(slot) == nullptr) {
      slots_valid_ = false;
    }
  }

  std::size_t words_ = 0;
  bool slots_valid_ = true;
};

// Runs `draw` against a budget, then, when every packet fits, against the
// frame. `draw` returns the core's result.
template <typename Draw>
void DrawChecked(std::string_view refused, Draw draw) {
  BudgetTarget budget;
  if (draw(budget) == PageDrawResult::kRefused) {
    Refuse(refused);
    return;
  }
  if (!budget.SlotsValid()) {
    Refuse("Manual page has no frame ordering-table entry");
    return;
  }
  if (Render::ReserveLivePrimitiveWords(budget.Words()) == nullptr) {
    Refuse("Manual page does not fit the primitive buffer");
    return;
  }
  LivePrimitiveTarget target;
  draw(target);
}

PageArrowState ArrowState() {
  return {.animation_state = Menu_Manual_pageArrowAnimState,
          .square_held = (VS::Input::previousButtons & PADRleft) != 0};
}

// The help text from its first halfword to the end of its heap payload.
std::optional<std::span<const std::int16_t>> HelpText() {
  const auto* first = reinterpret_cast<const std::byte*>(Menu_Manual_helpText);
  const auto payload = Core::Memory::LivePayloadContaining(first);
  if (first == nullptr || !payload.has_value() || first < payload->data() ||
      first >= payload->data() + payload->size()) {
    return std::nullopt;
  }
  const auto skipped = static_cast<std::size_t>(first - payload->data());
  return std::span(reinterpret_cast<const std::int16_t*>(first),
                   (payload->size() - skipped) / sizeof(std::int16_t));
}

class LiveContentTarget final : public PageContentTarget {
 public:
  explicit LiveContentTarget(std::int16_t* help_text) : help_text_(help_text) {}

  void SetDrawArea(const Render::ScreenRect& area) override {
    RECT rect{};
    setRECT(&rect, area.x, area.y, area.width, area.height);
    DR_AREA packet{};
    ::SetDrawArea(&packet, &rect);
    if (SlotEntry(kPageContentSlot) == nullptr) {
      Refuse("Manual page content has no frame ordering-table entry");
      return;
    }
    EmitPacket(kPageContentSlot,
               std::array<std::uint32_t, kDrawAreaWords>{
                   static_cast<std::uint32_t>(packet.code[0]),
                   static_cast<std::uint32_t>(packet.code[1])});
  }
  void DrawDiagramLines() override { LiveDrawDiagramLines(); }
  void DrawIllustration(std::size_t halfword_offset) override {
    // DrawPageContent only hands over offsets inside the help text.
    LiveDrawIllustration(help_text_ + halfword_offset);
  }

 private:
  std::int16_t* help_text_;
};
}  // namespace

// decomp: MENUE.PRG _drawContent (494.c:577)
void LiveDrawPageContent() {
  const auto help_text = HelpText();
  if (!help_text) {
    Refuse("Manual help text is outside the game heap");
    return;
  }
  const Render::ScreenRect clip{
      .x = Render_legacyDrawEnvironments[(Render_frameBuf + 1) & 1].clip.x,
      .y = Render_legacyDrawEnvironments[(Render_frameBuf + 1) & 1].clip.y,
      .width = Render_legacyDrawEnvironments[(Render_frameBuf + 1) & 1].clip.w,
      .height =
          Render_legacyDrawEnvironments[(Render_frameBuf + 1) & 1].clip.h};
  LiveContentTarget target(
      reinterpret_cast<std::int16_t*>(Menu_Manual_helpText));
  if (DrawPageContent(*help_text, clip, target) == PageDrawResult::kRefused) {
    Refuse("Manual illustration list lies past its help text");
  }
}

// decomp: MENUE.PRG _setPageBg (494.c:716)
void LiveDrawPageBackground(int x, int y, int w, int h, int color) {
  DrawChecked("Manual page background colour is outside its palette",
              [&](PagePrimitiveTarget& target) {
                return DrawPageBackground(
                    {.x = x, .y = y, .width = w, .height = h}, color, target);
              });
}

// decomp: MENUE.PRG _fadeMenuUpper (494.c:740)
void LiveDrawUpperScrollFade() {
  const PageArrowState arrow = ArrowState();
  DrawChecked("Manual upper scroll fade refused",
              [&](PagePrimitiveTarget& target) {
                DrawUpperScrollFade(arrow, target);
                return PageDrawResult::kDrawn;
              });
}

// decomp: MENUE.PRG _fadeMenuLower (494.c:791)
void LiveDrawLowerScrollFade() {
  const PageArrowState arrow = ArrowState();
  DrawChecked("Manual lower scroll fade refused",
              [&](PagePrimitiveTarget& target) {
                DrawLowerScrollFade(arrow, target);
                return PageDrawResult::kDrawn;
              });
}

// decomp: MENUE.PRG _drawPaginationArrow (494.c:844)
void LiveDrawPageArrow(enum Menu_Manual_ArrowType arrow_type) {
  if (arrow_type != Menu_Manual_ArrowType_Up &&
      arrow_type != Menu_Manual_ArrowType_Down) {
    Refuse("Manual page arrow type is unknown");
    return;
  }
  const PageArrow arrow = arrow_type == Menu_Manual_ArrowType_Up
                              ? PageArrow::kUp
                              : PageArrow::kDown;
  const PageArrowState state = ArrowState();
  DrawChecked("Manual page arrow refused", [&](PagePrimitiveTarget& target) {
    DrawPageArrow(arrow, state, target);
    return PageDrawResult::kDrawn;
  });
}

// decomp: MENUE.PRG _drawControlsBg (494.c:962)
void LiveDrawControlsBackground(int x, int y, int w, int h) {
  DrawChecked("Manual control hint box is taller than VRAM",
              [&](PagePrimitiveTarget& target) {
                return DrawControlsBackground(
                    {.x = x, .y = y, .width = w, .height = h}, target);
              });
}

std::uint64_t LivePageRefusalCount() { return g_refusals; }
}  // namespace VS::Menu::manual
