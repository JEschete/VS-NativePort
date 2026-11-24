// Hand-maintained (tools/port/owned.csv).
// Live binding for the display policies (DisplayControl.cpp).
#include "Render/DisplayControlLive.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <span>

#include "Core/Diagnostics.hpp"
#include "Core/Gte.hpp"
#include "Core/RandomLive.hpp"
#include "Input/Buttons.hpp"
#include "Input/ControllerLive.hpp"
#include "Input/ControllerLiveState.hpp"
#include "Input/Controllers.hpp"
#include "Platform/LogicalTicks.hpp"
#include "Render/Display.hpp"
#include "Render/Loading.hpp"
#include "Render/NativeTextDraw.hpp"
#include "Render/PaletteAnimationLive.hpp"
#include "State/GameClockLive.hpp"
#include "State/PlayTime.hpp"
#include "State/Timers.hpp"

using VS::Core::Gte::SetGeomOffset;
using VS::Core::Gte::SetGeomScreen;

using VS::Legacy::ClearImage2;
using VS::Legacy::DISPENV;
using VS::Legacy::DRAWENV;
using VS::Legacy::DrawOTag;
using VS::Legacy::LoadImage;
using VS::Legacy::PutDispEnv;
using VS::Legacy::PutDrawEnv;
using VS::Legacy::RECT;
using VS::Legacy::SetDefDispEnv;
using VS::Legacy::SetDefDrawEnv;
using VS::Legacy::SetDispMask;
using VS::Legacy::setRECT;
using VS::Legacy::setRGB0;

namespace VS::Render {
namespace {
constexpr int kSecondPortShift = 16;
constexpr unsigned kByteBits = 8;
// The loading picture's 16-bit pixels, stored as words.
using LoadingPixels = decltype(VS::Render::loadingScreenPixels);
constexpr std::size_t kPixelBytes = sizeof(std::uint16_t);
constexpr std::size_t kLoadingPixelCount = sizeof(LoadingPixels) / kPixelBytes;

RECT ToRect(const ScreenRect& area) {
  RECT rect{};
  setRECT(&rect, area.x, area.y, area.width, area.height);
  return rect;
}

std::size_t CurrentBuffer() { return static_cast<std::size_t>(frameBuf & 1); }

void PresentEnvironments(std::size_t index) {
  PutDispEnv(&legacyDisplayEnvironments[index]);
  PutDrawEnv(&legacyDrawEnvironments[index]);
}

class LiveDisplayDevice final : public DisplayDevice {
 public:
  void SetGeometryOffset(std::int32_t x, std::int32_t y) override {
    SetGeomOffset(x, y);
  }
  void SetProjectionDistance(std::int32_t distance) override {
    SetGeomScreen(distance);
  }
  void ApplyFieldLayout(const FieldDisplayLayout& layout) override {
    for (std::size_t index = 0; index < kFrameBufferCount; ++index) {
      const ScreenRect& draw = layout.draw_areas[index];
      DRAWENV& draw_env = legacyDrawEnvironments[index];
      SetDefDrawEnv(&draw_env, draw.x, draw.y, draw.width, draw.height);
      draw_env.dtd = 1;
      draw_env.dfe = 0;
      draw_env.isbg = 0;
      setRGB0(&draw_env, layout.clear.red, layout.clear.green,
              layout.clear.blue);
      const ScreenRect& shown = layout.display_areas[index];
      DISPENV& display_env = legacyDisplayEnvironments[index];
      SetDefDispEnv(&display_env, shown.x, shown.y, shown.width, shown.height);
      display_env.screen = ToRect(layout.screen);
    }
    legacyViewportLeft = layout.viewport.x;
    legacyViewportTop = layout.viewport.y;
    viewportWidth = layout.viewport.width;
    viewportHeight = static_cast<std::uint16_t>(layout.viewport.height);
    PresentEnvironments(CurrentBuffer());
  }
  void ShowDisplayArea(const ScreenRect& area,
                       const ScreenRect& screen) override {
    DISPENV& display_env = legacyDisplayEnvironments[0];
    SetDefDispEnv(&display_env, area.x, area.y, area.width, area.height);
    display_env.screen = ToRect(screen);
    PutDispEnv(&display_env);
  }
  void ClearToBlack(const ScreenRect& area) override {
    RECT rect = ToRect(area);
    ClearImage2(&rect, 0, 0, 0);
  }
  void WaitTicks(std::int32_t count) override {
    VS::Platform::WaitTicks(count);
  }
  void UploadImage(const ScreenRect& area,
                   std::span<const std::uint16_t> pixels) override {
    RECT rect = ToRect(area);
    // LoadImage only reads the pixels; the policy sized the span to the area.
    LoadImage(&rect, reinterpret_cast<std::uint32_t*>(
                         const_cast<std::uint16_t*>(pixels.data())));
  }
  void SetDisplayEnabled(bool enabled) override {
    SetDispMask(enabled ? 1 : 0);
  }
};

class LiveFieldFrame final : public FieldFrameHost {
 public:
  explicit LiveFieldFrame(std::uint32_t* ordering_table)
      : ordering_table_(ordering_table) {}

  [[nodiscard]] int FrameBuffer() const override { return frameBuf; }
  void SelectFrameBuffer(int index) override { frameBuf = index; }
  void AdvanceRandom() override { VS::Core::NextLiveRandomBounded(0); }
  void SetGeometryOffset(std::int32_t x, std::int32_t y) override {
    SetGeomOffset(x, y);
  }
  [[nodiscard]] std::int32_t TickSpeed() const override {
    return VS::State::tickSpeed;
  }
  void UpdatePlayTime(std::int32_t ticks) override {
    VS::State::UpdateLivePlayTimeAndLoading(ticks);
  }
  [[nodiscard]] bool FrameStepPaused() const override {
    return isFrameStepPaused != 0;
  }
  void ToggleFrameStepPause() override { isFrameStepPaused ^= 1; }
  // The original polled both pads inline (146C.c:5998-6008).
  std::uint32_t PollSecondControllerPressed() override {
    const auto first =
        static_cast<std::uint16_t>(VS::Input::ReadLiveControllerPort(0));
    VS::Input::buttonsState = first;
    const auto second = static_cast<std::uint32_t>(
        VS::Input::ReadLiveControllerPort(kSecondPortShift));
    const std::uint32_t state = first | (second << kSecondPortShift);
    VS::Input::buttonsState = static_cast<int>(state);
    VS::Input::UpdateLiveControllerConnection(0);
    VS::Input::UpdateLiveControllerConnection(kSecondPortShift);
    const auto previous =
        static_cast<std::uint32_t>(VS::Input::previousButtons);
    const std::uint32_t pressed = ~previous & state;
    VS::Input::buttonsPressed = pressed;
    VS::Input::releasedButtons = static_cast<int>(previous & ~state);
    VS::Input::previousButtons = VS::Input::buttonsState;
    return pressed >> kSecondPortShift;
  }
  void PresentFrameBuffer(int index) override {
    PresentEnvironments(static_cast<std::size_t>(index & 1));
  }
  void CommitPalettes() override { CommitLiveClutRows(); }
  void DrawOrderingTable() override {
    DrawOTag(ordering_table_);
    FlushLiveNativeText();
  }

 private:
  std::uint32_t* ordering_table_;
};

std::int32_t HeaderSize(std::uint8_t low, std::uint8_t high) {
  // Read unsigned; the original's signed chars agree for the shipped
  // 126x20 image.
  return static_cast<std::int32_t>(static_cast<unsigned>(low) |
                                   (static_cast<unsigned>(high) << kByteBits));
}
}  // namespace

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): original call shape.
void ConfigureLiveFieldDisplay(int frame_width, int frame_height,
                               int projection_distance, int clear_red,
                               int clear_green, int clear_blue) {
  LiveDisplayDevice device;
  ConfigureFieldDisplay(
      device,
      FieldDisplaySettings{.frame_width = frame_width,
                           .frame_height = frame_height,
                           .projection_distance = projection_distance,
                           .clear = {static_cast<std::uint8_t>(clear_red),
                                     static_cast<std::uint8_t>(clear_green),
                                     static_cast<std::uint8_t>(clear_blue)}});
}

void PresentLiveFieldFrame(std::uint32_t* ordering_table) {
  LiveFieldFrame host(ordering_table);
  PresentFieldFrame(host);
}

void EnableLiveDisplay() {
  LiveDisplayDevice device;
  EnableDisplay(device);
}

void DisableLiveDisplay() {
  LiveDisplayDevice device;
  DisableDisplay(device);
}

void ShowLiveDisplay() {
  LiveDisplayDevice device;
  ShowDisplay(device);
}

void HideLiveDisplay() {
  LiveDisplayDevice device;
  HideDisplay(device);
}

void ShowLiveLoadingScreen() {
  static_assert(sizeof(loadingScreenPixels) % sizeof(std::uint16_t) == 0);
  std::array<std::uint16_t, kLoadingPixelCount> pixels{};
  std::memcpy(pixels.data(), loadingScreenPixels, sizeof(loadingScreenPixels));
  const LegacyImageSize& header = legacyLoadingImageHeader;
  const LoadingImage image{
      .width = HeaderSize(header.width_low, header.width_high),
      .height = HeaderSize(header.height_low, header.height_high),
      .pixels = pixels,
  };
  LiveDisplayDevice device;
  if (!ShowLoadingScreen(device, image)) {
    VS::Core::Errorf("render",
                     "loading image %dx%d exceeds its %zu pixels; not uploaded",
                     image.width, image.height, pixels.size());
  }
}
}  // namespace VS::Render
