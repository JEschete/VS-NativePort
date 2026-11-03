// Hand-maintained (tools/port/owned.csv).
#include "Render/DisplayControl.hpp"

#include <cstddef>
#include <cstdint>

#include "Core/Math/ScalarMath.hpp"

namespace VS::Render {
namespace {
// The field draws 16 lines fewer than the frame height.
constexpr std::int32_t kDisplayTopMargin = 16;
constexpr std::int32_t kFieldCenterX = 160;
constexpr std::int32_t kFieldCenterY = 112;
constexpr std::int32_t kFrameStepTicks = 2;
// The NTSC window: 256 wide (scaled to the display), from line 8 for 224.
constexpr ScreenRect kScreenWindow{0, 8, 256, 224};
constexpr ScreenRect kWholeVram{0, 0, 1024, 512};
constexpr std::int32_t kLoadingFrameWidth = 320;
constexpr std::int32_t kLoadingFrameHeight = 240;
constexpr std::int32_t kLoadingVisibleHeight = 224;
constexpr std::int32_t kLoadingSettleBlanks = 2;

std::int16_t Halfword(std::int32_t value) {
  return Core::Math::WrapSigned16(static_cast<std::uint32_t>(value));
}

std::int32_t DrawableHeight(std::int32_t frame_height) {
  return Core::Math::WrappingSubtract(frame_height, kDisplayTopMargin);
}

// The pixels the image's size covers, or an empty span when the size is not
// positive or exceeds the pixels supplied.
std::span<const std::uint16_t> ImagePixels(const LoadingImage& image) {
  if (image.width <= 0 || image.height <= 0) {
    return {};
  }
  const auto count = static_cast<std::size_t>(image.width) *
                     static_cast<std::size_t>(image.height);
  if (count > image.pixels.size()) {
    return {};
  }
  return image.pixels.first(count);
}

void WaitForFrameStep(FieldFrameHost& host) {
  while (true) {
    host.UpdatePlayTime(kFrameStepTicks);
    const std::uint32_t pressed = host.PollSecondControllerPressed();
    if ((pressed & kFrameStepToggleButton) != 0) {
      host.ToggleFrameStepPause();
    }
    if ((pressed & kFrameStepAdvanceButton) != 0 || !host.FrameStepPaused()) {
      return;
    }
  }
}
}  // namespace

FieldDisplayLayout FieldLayoutFor(
    const FieldDisplaySettings& settings) noexcept {
  const std::int16_t width = Halfword(settings.frame_width);
  const std::int16_t height = Halfword(DrawableHeight(settings.frame_height));
  FieldDisplayLayout layout{};
  layout.draw_areas = {ScreenRect{0, 0, width, height},
                       ScreenRect{width, 0, width, height}};
  layout.display_areas = {ScreenRect{width, 0, width, height},
                          ScreenRect{0, 0, width, height}};
  layout.screen = kScreenWindow;
  layout.viewport = {0, 0, width, height};
  layout.clear = settings.clear;
  return layout;
}

void ConfigureFieldDisplay(DisplayDevice& device,
                           const FieldDisplaySettings& settings) {
  const std::int32_t drawable = DrawableHeight(settings.frame_height);
  device.SetGeometryOffset(settings.frame_width / 2,
                           drawable / 2 + kDisplayTopMargin);
  device.SetProjectionDistance(settings.projection_distance);
  device.ApplyFieldLayout(FieldLayoutFor(settings));
}

void ShowDisplay(DisplayDevice& device) { device.SetDisplayEnabled(true); }

void HideDisplay(DisplayDevice& device) { device.SetDisplayEnabled(false); }

void EnableDisplay(DisplayDevice& device) { ShowDisplay(device); }

void DisableDisplay(DisplayDevice& device) { HideDisplay(device); }

std::int32_t PresentFieldFrame(FieldFrameHost& host) {
  const int buffer = host.FrameBuffer() == 0 ? 1 : 0;
  host.SelectFrameBuffer(buffer);
  host.AdvanceRandom();
  host.SetGeometryOffset(kFieldCenterX, kFieldCenterY);
  const std::int32_t elapsed = host.UpdatePlayTime(host.TickSpeed());
  // The pause flag is only changed inside this wait, so it is a debug hook.
  if (host.FrameStepPaused()) {
    WaitForFrameStep(host);
  }
  host.PresentFrameBuffer(buffer);
  host.CommitPalettes();
  host.DrawOrderingTable();
  return elapsed;
}

ScreenRect LoadingImageArea(const LoadingImage& image) noexcept {
  using Core::Math::WrappingSubtract;
  return {Halfword(WrappingSubtract(kLoadingFrameWidth, image.width) / 2),
          Halfword(WrappingSubtract(kLoadingVisibleHeight, image.height) / 2),
          Halfword(image.width), Halfword(image.height)};
}

void InitializeLoadingDisplay(DisplayDevice& device, std::int32_t width,
                              std::int32_t height) {
  device.ShowDisplayArea(
      ScreenRect{0, 0, Halfword(width), Halfword(DrawableHeight(height))},
      kScreenWindow);
}

bool ShowLoadingScreen(DisplayDevice& device, const LoadingImage& image) {
  InitializeLoadingDisplay(device, kLoadingFrameWidth, kLoadingFrameHeight);
  device.ClearToBlack(kWholeVram);
  device.WaitVerticalBlanks(kLoadingSettleBlanks);
  const std::span<const std::uint16_t> pixels = ImagePixels(image);
  const bool uploaded = !pixels.empty();
  if (uploaded) {
    ScreenRect area = LoadingImageArea(image);
    device.UploadImage(area, pixels);
    area.x = Halfword(area.x + kLoadingFrameWidth);
    device.UploadImage(area, pixels);
  }
  device.SetDisplayEnabled(true);
  return uploaded;
}

bool UploadLoadingScreenBackBuffer(DisplayDevice& device,
                                   const LoadingImage& image,
                                   int frame_buffer) {
  const std::span<const std::uint16_t> pixels = ImagePixels(image);
  if (pixels.empty()) {
    return false;
  }
  ScreenRect area = LoadingImageArea(image);
  if (frame_buffer == 0) {
    area.x = Halfword(area.x + kLoadingFrameWidth);
  }
  device.UploadImage(area, pixels);
  return true;
}
}  // namespace VS::Render
