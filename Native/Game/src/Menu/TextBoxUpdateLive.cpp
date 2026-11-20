#include "Menu/TextBoxUpdateLive.hpp"

#include <bit>
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <span>

#include "Battle/Hud.hpp"
#include "Battle/HudOrchestrationLive.hpp"
#include "Core/Diagnostics.hpp"
#include "Core/RandomLive.hpp"
#include "Effects/ScreenDistortion/ScreenOverlaysLive.hpp"
#include "Legacy/Psx.hpp"
#include "Menu/Framework/FrameworkLiveState.hpp"
#include "Menu/TextBox.hpp"
#include "Menu/TextBoxFramesLive.hpp"
#include "Menu/TextBoxStateLive.hpp"
#include "Menu/TextBoxTextLive.hpp"
#include "Menu/TextBoxUpdate.hpp"
#include "Render/UiPacketsLive.hpp"
#include "State/Timers.hpp"

using VS::Legacy::AddPrims;
using VS::Legacy::ClearOTag;

namespace VS::Menu {
namespace {
constexpr int kTextOrderingSlot = -6;
constexpr int kTimerVisible = 1;
std::uint64_t refusedUpdates = 0;
static_assert(std::size(textBoxOrderingTables) ==
              kTextBoxOrderingBufferCount * kTextBoxOrderingTags);

void RefuseUpdate() {
  ++refusedUpdates;
  if (std::has_single_bit(refusedUpdates)) {
    VS::Core::Warnf(
        "menu",
        "Text-box update refused an unbound frame or ordering buffer "
        "(count=%" PRIu64 ")",
        refusedUpdates);
  }
}

class LiveUpdateHost final : public TextBoxUpdateHost {
 public:
  explicit LiveUpdateHost(std::uint32_t* destination)
      : destination_(destination) {}

  TextBox ReadBox(std::size_t index) const override {
    return ReadLiveTextBox(index);
  }
  void WriteBox(std::size_t index, const TextBox& box) override {
    WriteLiveTextBox(index, box);
  }
  std::int32_t TickSpeed() const override { return VS::State::tickSpeed; }
  void BlendText(std::size_t index, std::int32_t scale) override {
    BlendLiveTextBoxIntoFramebuffer(&std::span(textBoxes)[index], scale);
  }
  void DrawInformationBackdrop(std::size_t index) override {
    DrawLiveInformationBoxBackdrop(static_cast<std::int32_t>(index));
  }
  void DrawFrame(std::size_t index) override {
    DrawLiveTextBoxFrame(static_cast<std::int32_t>(index));
  }
  void DrawProgressiveText(std::size_t index) override {
    DrawLiveProgressiveTextBoxText(&std::span(textBoxes)[index]);
  }
  void DrawScaledText(std::size_t index, std::int32_t scale) override {
    DrawLiveScaledTextBoxText(&std::span(textBoxes)[index], scale);
  }
  void SubmitOrdering() override {
    AddPrims(destination_, activeTextBoxOrderingTable,
             activeTextBoxOrderingTable + kTextBoxOrderingTags - 1);
  }
  std::uint8_t OrderingBufferIndex() const override {
    return static_cast<std::uint8_t>(textBoxBufferIndex);
  }
  void BeginOrderingBuffer(std::uint8_t index) override {
    textBoxBufferIndex = static_cast<char>(index);
    activeTextBoxOrderingTable =
        std::span(textBoxOrderingTables)
            .subspan(index * kTextBoxOrderingTags, kTextBoxOrderingTags)
            .data();
    ClearOTag(activeTextBoxOrderingTable,
              static_cast<int>(kTextBoxOrderingTags));
  }
  bool TimerVisible() const override {
    return VS::Battle::lowerScreenUiState == kTimerVisible;
  }
  void DrawTimer() override {
    Battle::DrawLivePuzzleTimer(&VS::State::sceneTimer);
  }
  void DrawOverlay() override {
    Effects::ScreenDistortion::DrawLiveScreenOverlay();
  }
  void AdvanceRandomBits() override { Core::NextLiveInterfaceRandomBits(0); }

 private:
  std::uint32_t* destination_;
};
}  // namespace

// decomp: F01392 func_800CB83C (src/BATTLE/BATTLE.PRG/5BF94.c:3936).
void UpdateLiveTextBoxes() {
  auto* destination = Render::LiveFrameOrderingEntry(
      Render::LiveOrderingBase::kScene, kTextOrderingSlot);
  const auto buffer = static_cast<std::uint8_t>(textBoxBufferIndex);
  if (destination == nullptr || buffer >= kTextBoxOrderingBufferCount ||
      activeTextBoxOrderingTable !=
          std::span(textBoxOrderingTables)
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
