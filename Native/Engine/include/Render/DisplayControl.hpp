#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

// Field display setup, frame presentation and the boot loading screen. The
// policies describe display areas, uploads and waits as values and hand them
// to a DisplayDevice or FieldFrameHost, so they reach no GPU environment,
// frame-buffer global or module address directly.
namespace VS::Render {
// A VRAM or screen rectangle (RECT).
struct ScreenRect {
  std::int16_t x = 0;
  std::int16_t y = 0;
  std::int16_t width = 0;
  std::int16_t height = 0;

  friend bool operator==(const ScreenRect&, const ScreenRect&) = default;
};

struct ClearColor {
  std::uint8_t red = 0;
  std::uint8_t green = 0;
  std::uint8_t blue = 0;

  friend bool operator==(const ClearColor&, const ClearColor&) = default;
};

inline constexpr std::size_t kFrameBufferCount = 2;

// The double-buffered field display. Buffer i draws into draw_areas[i] while
// display_areas[i] is shown. Dithering is on, drawing to the displayed area
// is off and the draw area is not cleared on each frame.
struct FieldDisplayLayout {
  std::array<ScreenRect, kFrameBufferCount> draw_areas{};
  std::array<ScreenRect, kFrameBufferCount> display_areas{};
  // The visible window within each display area.
  ScreenRect screen{};
  ScreenRect viewport{};
  ClearColor clear{};

  friend bool operator==(const FieldDisplayLayout&,
                         const FieldDisplayLayout&) = default;
};

struct FieldDisplaySettings {
  std::int32_t frame_width = 0;
  std::int32_t frame_height = 0;
  std::int32_t projection_distance = 0;
  ClearColor clear{};
};

class DisplayDevice {
 public:
  virtual ~DisplayDevice() = default;
  virtual void SetGeometryOffset(std::int32_t x, std::int32_t y) = 0;
  virtual void SetProjectionDistance(std::int32_t distance) = 0;
  // Records both buffers' environments and presents the current buffer.
  virtual void ApplyFieldLayout(const FieldDisplayLayout& layout) = 0;
  // Shows `area` (the first buffer) through the `screen` window.
  virtual void ShowDisplayArea(const ScreenRect& area,
                               const ScreenRect& screen) = 0;
  virtual void ClearToBlack(const ScreenRect& area) = 0;
  virtual void WaitVerticalBlanks(std::int32_t count) = 0;
  virtual void UploadImage(const ScreenRect& area,
                           std::span<const std::uint16_t> pixels) = 0;
  virtual void SetDisplayEnabled(bool enabled) = 0;
};

// The display layout for a field frame of the given size.
[[nodiscard]] FieldDisplayLayout FieldLayoutFor(
    const FieldDisplaySettings& settings) noexcept;

// BATTLE.PRG func_800760CC (146C.c:5959).
void ConfigureFieldDisplay(DisplayDevice& device,
                           const FieldDisplaySettings& settings);
// BATTLE.PRG func_80089CE4 (146C.c:15250).
void ShowDisplay(DisplayDevice& device);
// BATTLE.PRG func_80089D04 (146C.c:15252).
void HideDisplay(DisplayDevice& device);
// BATTLE.PRG func_8007C404 (146C.c:8770): ShowDisplay.
void EnableDisplay(DisplayDevice& device);
// BATTLE.PRG func_8007C424 (146C.c:8772): HideDisplay.
void DisableDisplay(DisplayDevice& device);

// Frame presentation for the field loop.
class FieldFrameHost {
 public:
  virtual ~FieldFrameHost() = default;
  [[nodiscard]] virtual int FrameBuffer() const = 0;
  virtual void SelectFrameBuffer(int index) = 0;
  virtual void AdvanceRandom() = 0;
  virtual void SetGeometryOffset(std::int32_t x, std::int32_t y) = 0;
  [[nodiscard]] virtual std::int32_t TickSpeed() const = 0;
  // Waits `ticks` vertical blanks, advances play time and loading; returns
  // the elapsed ticks.
  virtual std::int32_t UpdatePlayTime(std::int32_t ticks) = 0;
  [[nodiscard]] virtual bool FrameStepPaused() const = 0;
  virtual void ToggleFrameStepPause() = 0;
  // Reads both controllers; the second controller's newly pressed buttons.
  virtual std::uint32_t PollSecondControllerPressed() = 0;
  virtual void PresentFrameBuffer(int index) = 0;
  virtual void CommitPalettes() = 0;
  virtual void DrawOrderingTable() = 0;
};

// Second-controller Start toggles the debug frame-step pause; Select steps
// one frame.
inline constexpr std::uint32_t kFrameStepToggleButton = 0x800;
inline constexpr std::uint32_t kFrameStepAdvanceButton = 0x100;

// BATTLE.PRG func_8007629C (146C.c:5988): flips the frame buffer and submits
// the frame; returns the elapsed ticks.
std::int32_t PresentFieldFrame(FieldFrameHost& host);

// The boot loading image: its size and 16-bit pixels.
struct LoadingImage {
  std::int32_t width = 0;
  std::int32_t height = 0;
  std::span<const std::uint16_t> pixels;
};

// Where the loading image sits in the 320x224 front buffer; the back buffer
// copy is one frame width (320) to the right.
[[nodiscard]] ScreenRect LoadingImageArea(const LoadingImage& image) noexcept;

// SLUS_010.40 _initScreen (main.c:8069).
void InitializeLoadingDisplay(DisplayDevice& device, std::int32_t width,
                              std::int32_t height);
// SLUS_010.40 _displayLoadingScreen (main.c:8081): clears VRAM and shows the
// loading image in both buffers. An image whose size exceeds its pixels is
// not uploaded; returns whether it was.
bool ShowLoadingScreen(DisplayDevice& device, const LoadingImage& image);
// SLUS_010.40 _bufferLoadingScreen (main.c:8102): the loading image in the
// buffer not being shown. Returns whether it was uploaded.
bool UploadLoadingScreenBackBuffer(DisplayDevice& device,
                                   const LoadingImage& image, int frame_buffer);
}  // namespace VS::Render
