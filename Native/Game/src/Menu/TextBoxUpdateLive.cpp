#include "Menu/TextBoxUpdateLive.hpp"

#include <bit>
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <span>

#include "Battle/HudOrchestrationLive.hpp"
#include "Core/RandomLive.hpp"
#include "Effects/ScreenDistortion/ScreenOverlaysLive.hpp"
#include "Menu/TextBoxFramesLive.hpp"
#include "Menu/TextBoxStateLive.hpp"
#include "Menu/TextBoxUpdate.hpp"
#include "Render/GameFontLive.hpp"
#include "Render/UiPacketsLive.hpp"

extern "C" {
#include "Battle/Hud.h"
#include "Core/Diagnostics.h"
#include "Menu/Framework/FrameworkLiveState.h"
#include "Menu/TextBox.h"
#include "State/Timers.h"
}

namespace VS::Menu {
namespace {
constexpr int kTextOrderingSlot = -6;
constexpr int kTimerVisible = 1;
std::uint64_t refused_updates = 0;
static_assert(std::size(Menu_textBoxOrderingTables) ==
              kTextBoxOrderingBufferCount * kTextBoxOrderingTags);

void RefuseUpdate() {
  ++refused_updates;
  if (std::has_single_bit(refused_updates)) {
    Core_Warnf("menu",
               "Text-box update refused an unbound frame or ordering buffer "
               "(count=%" PRIu64 ")",
               refused_updates);
  }
}

class LiveUpdateHost final : public TextBoxUpdateHost {
 public:
  explicit LiveUpdateHost(u_long* destination) : destination_(destination) {}

  TextBox ReadBox(std::size_t index) const override {
    return ReadLiveTextBox(index);
  }
  void WriteBox(std::size_t index, const TextBox& box) override {
    WriteLiveTextBox(index, box);
  }
  std::int32_t TickSpeed() const override { return State_tickSpeed; }
  void BlendText(std::size_t index, std::int32_t scale) override {
    Render::BlendLiveTextBoxIntoFramebuffer(&std::span(Menu_textBoxes)[index],
                                            scale);
  }
  void DrawInformationBackdrop(std::size_t index) override {
    DrawLiveInformationBoxBackdrop(static_cast<std::int32_t>(index));
  }
  void DrawFrame(std::size_t index) override {
    DrawLiveTextBoxFrame(static_cast<std::int32_t>(index));
  }
  void DrawProgressiveText(std::size_t index) override {
    Render::DrawLiveProgressiveTextBoxText(&std::span(Menu_textBoxes)[index]);
  }
  void DrawScaledText(std::size_t index, std::int32_t scale) override {
    Render::DrawLiveScaledTextBoxText(&std::span(Menu_textBoxes)[index], scale);
  }
  void SubmitOrdering() override {
    AddPrims(destination_, Menu_activeTextBoxOrderingTable,
             Menu_activeTextBoxOrderingTable + kTextBoxOrderingTags - 1);
  }
  std::uint8_t OrderingBufferIndex() const override {
    return static_cast<std::uint8_t>(Menu_textBoxBufferIndex);
  }
  void BeginOrderingBuffer(std::uint8_t index) override {
    Menu_textBoxBufferIndex = static_cast<char>(index);
    Menu_activeTextBoxOrderingTable =
        std::span(Menu_textBoxOrderingTables)
            .subspan(index * kTextBoxOrderingTags, kTextBoxOrderingTags)
            .data();
    ClearOTag(Menu_activeTextBoxOrderingTable,
              static_cast<int>(kTextBoxOrderingTags));
  }
  bool TimerVisible() const override {
    return Battle_lowerScreenUiState == kTimerVisible;
  }
  void DrawTimer() override { Battle::DrawLivePuzzleTimer(&State_sceneTimer); }
  void DrawOverlay() override {
    Effects::ScreenDistortion::DrawLiveScreenOverlay();
  }
  void AdvanceRandomBits() override { Core::NextLiveInterfaceRandomBits(0); }

 private:
  u_long* destination_;
};
}  // namespace

// decomp: F01392 func_800CB83C (src/BATTLE/BATTLE.PRG/5BF94.c:3936).
void UpdateLiveTextBoxes() {
  auto* destination = Render::LiveFrameOrderingEntry(
      Render::LiveOrderingBase::kScene, kTextOrderingSlot);
  const auto buffer = static_cast<std::uint8_t>(Menu_textBoxBufferIndex);
  if (destination == nullptr || buffer >= kTextBoxOrderingBufferCount ||
      Menu_activeTextBoxOrderingTable !=
          std::span(Menu_textBoxOrderingTables)
              .subspan(buffer * kTextBoxOrderingTags, kTextBoxOrderingTags)
              .data()) {
    RefuseUpdate();
    return;
  }
  LiveUpdateHost host(destination);
  if (UpdateTextBoxes(host) != TextBoxUpdateStatus::kComplete) {
    RefuseUpdate();
  }
}
}  // namespace VS::Menu
