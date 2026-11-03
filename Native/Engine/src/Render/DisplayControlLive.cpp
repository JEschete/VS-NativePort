// Hand-maintained (tools/port/owned.csv).
// Live binding for the display policies (DisplayControl.cpp).
#include "Render/DisplayControlLive.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <span>

#include "Core/RandomLive.hpp"

extern "C" {
#include "Core/Diagnostics.h"
#include "Core/Gte.h"
#include "Input/Buttons.h"
#include "Input/Controller.h"
#include "Input/Controllers.h"
#include "Render/Display.h"
#include "Render/Loading.h"
#include "Render/Palette.h"
#include "State/PlayTime.h"
#include "State/Timers.h"
}

namespace VS::Render {
namespace {
constexpr int kSecondPortShift = 16;
constexpr unsigned kByteBits = 8;
// The loading picture's 16-bit pixels, stored as words.
using LoadingPixels = decltype(Render_loadingScreenPixels);
constexpr std::size_t kPixelBytes = sizeof(std::uint16_t);
constexpr std::size_t kLoadingPixelCount = sizeof(LoadingPixels) / kPixelBytes;

RECT ToRect(const ScreenRect& area) {
  RECT rect{};
  setRECT(&rect, area.x, area.y, area.width, area.height);
  return rect;
}

std::size_t CurrentBuffer() {
  return static_cast<std::size_t>(Render_frameBuf & 1);
}

void PresentEnvironments(std::size_t index) {
  PutDispEnv(&Render_legacyDisplayEnvironments[index]);
  PutDrawEnv(&Render_legacyDrawEnvironments[index]);
}

class LiveDisplayDevice final : public DisplayDevice {
 public:
  void SetGeometryOffset(std::int32_t x, std::int32_t y) override {
    Core_Gte_SetGeomOffset(x, y);
  }
  void SetProjectionDistance(std::int32_t distance) override {
    Core_Gte_SetGeomScreen(distance);
  }
  void ApplyFieldLayout(const FieldDisplayLayout& layout) override {
    for (std::size_t index = 0; index < kFrameBufferCount; ++index) {
      const ScreenRect& draw = layout.draw_areas[index];
      DRAWENV& draw_env = Render_legacyDrawEnvironments[index];
      SetDefDrawEnv(&draw_env, draw.x, draw.y, draw.width, draw.height);
      draw_env.dtd = 1;
      draw_env.dfe = 0;
      draw_env.isbg = 0;
      setRGB0(&draw_env, layout.clear.red, layout.clear.green,
              layout.clear.blue);
      const ScreenRect& shown = layout.display_areas[index];
      DISPENV& display_env = Render_legacyDisplayEnvironments[index];
      SetDefDispEnv(&display_env, shown.x, shown.y, shown.width, shown.height);
      display_env.screen = ToRect(layout.screen);
    }
    Render_legacyViewportLeft = layout.viewport.x;
    Render_legacyViewportTop = layout.viewport.y;
    Render_viewportWidth = layout.viewport.width;
    Render_viewportHeight = static_cast<u_short>(layout.viewport.height);
    PresentEnvironments(CurrentBuffer());
  }
  void ShowDisplayArea(const ScreenRect& area,
                       const ScreenRect& screen) override {
    DISPENV& display_env = Render_legacyDisplayEnvironments[0];
    SetDefDispEnv(&display_env, area.x, area.y, area.width, area.height);
    display_env.screen = ToRect(screen);
    PutDispEnv(&display_env);
  }
  void ClearToBlack(const ScreenRect& area) override {
    RECT rect = ToRect(area);
    ClearImage2(&rect, 0, 0, 0);
  }
  void WaitVerticalBlanks(std::int32_t count) override { VSync(count); }
  void UploadImage(const ScreenRect& area,
                   std::span<const std::uint16_t> pixels) override {
    RECT rect = ToRect(area);
    // LoadImage only reads the pixels; the policy sized the span to the area.
    LoadImage(&rect, reinterpret_cast<u_long*>(
                         const_cast<std::uint16_t*>(pixels.data())));
  }
  void SetDisplayEnabled(bool enabled) override {
    SetDispMask(enabled ? 1 : 0);
  }
};

class LiveFieldFrame final : public FieldFrameHost {
 public:
  explicit LiveFieldFrame(u_long* ordering_table)
      : ordering_table_(ordering_table) {}

  [[nodiscard]] int FrameBuffer() const override { return Render_frameBuf; }
  void SelectFrameBuffer(int index) override { Render_frameBuf = index; }
  void AdvanceRandom() override { VS::Core::NextLiveRandomBounded(0); }
  void SetGeometryOffset(std::int32_t x, std::int32_t y) override {
    Core_Gte_SetGeomOffset(x, y);
  }
  [[nodiscard]] std::int32_t TickSpeed() const override {
    return State_tickSpeed;
  }
  std::int32_t UpdatePlayTime(std::int32_t ticks) override {
    return State_UpdatePlayTimeAndLoading(ticks);
  }
  [[nodiscard]] bool FrameStepPaused() const override {
    return Render_isFrameStepPaused != 0;
  }
  void ToggleFrameStepPause() override { Render_isFrameStepPaused ^= 1; }
  // The original polled both pads inline (146C.c:5998-6008).
  std::uint32_t PollSecondControllerPressed() override {
    const auto first = static_cast<std::uint16_t>(
        Input_ReadControllerPort(0, Input_legacyPadResponseBytes[0]));
    Input_buttonsState = first;
    const auto second = static_cast<std::uint32_t>(Input_ReadControllerPort(
        kSecondPortShift, Input_legacyPadResponseBytes[1]));
    const std::uint32_t state = first | (second << kSecondPortShift);
    Input_buttonsState = static_cast<int>(state);
    Input_UpdateControllerConnection(0, Input_legacyPadResponseBytes[0]);
    Input_UpdateControllerConnection(kSecondPortShift,
                                     Input_legacyPadResponseBytes[1]);
    const auto previous = static_cast<std::uint32_t>(Input_previousButtons);
    const std::uint32_t pressed = ~previous & state;
    Input_buttonsPressed.allButtons = pressed;
    Input_releasedButtons = static_cast<int>(previous & ~state);
    Input_previousButtons = Input_buttonsState;
    return pressed >> kSecondPortShift;
  }
  void PresentFrameBuffer(int index) override {
    PresentEnvironments(static_cast<std::size_t>(index & 1));
  }
  void CommitPalettes() override { Render_CommitClutBuffer(); }
  void DrawOrderingTable() override { DrawOTag(ordering_table_); }

 private:
  u_long* ordering_table_;
};

std::int32_t HeaderSize(char low, char high) {
  // Read unsigned; the original's signed chars agree for the shipped
  // 126x20 image.
  return static_cast<std::int32_t>(
      static_cast<unsigned char>(low) |
      (static_cast<unsigned>(static_cast<unsigned char>(high)) << kByteBits));
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

int PresentLiveFieldFrame(u_long* ordering_table) {
  LiveFieldFrame host(ordering_table);
  return PresentFieldFrame(host);
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
  static_assert(sizeof(Render_loadingScreenPixels) % sizeof(std::uint16_t) ==
                0);
  std::array<std::uint16_t, kLoadingPixelCount> pixels{};
  std::memcpy(pixels.data(), Render_loadingScreenPixels,
              sizeof(Render_loadingScreenPixels));
  const Render_LegacyImageSize& header = Render_legacyLoadingImageHeader;
  const LoadingImage image{
      .width = HeaderSize(header.widthLow, header.widthHigh),
      .height = HeaderSize(header.heightLow, header.heightHigh),
      .pixels = pixels,
  };
  LiveDisplayDevice device;
  if (!ShowLoadingScreen(device, image)) {
    Core_Errorf("render",
                "loading image %dx%d exceeds its %zu pixels; not uploaded",
                image.width, image.height, pixels.size());
  }
}
}  // namespace VS::Render
