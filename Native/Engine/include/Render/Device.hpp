#pragma once

#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "Core/Types.hpp"

struct SDL_Window;
struct SDL_GPUDevice;
struct SDL_GPUTexture;
struct SDL_GPUTransferBuffer;

namespace VS::Render {

// The swapchain clear color PresentClear draws (#454).
struct SwapchainClearColor {
  float r = 0.0f;
  float g = 0.0f;
  float b = 0.0f;
  float a = 1.0f;
};

// SDL GPU debug mode (driver validation layers), as SDL reported it when the
// device was created. Validation changes driver behaviour and cost, so a
// Release timing or capture taken with it does not measure the shipping
// configuration (issue #348).
enum class GpuValidation : std::uint8_t {
  kOff,          // not requested
  kOn,           // SDL enabled the validation layers
  kPartial,      // SDL enabled only some of them
  kUnavailable,  // requested, but SDL found no validation layers
  kUnreported,   // requested; SDL did not report the result
};

// Debug builds request validation by default and Release builds do not; the
// last of --gpu-validation / --no-gpu-validation in argv overrides that.
bool GpuValidationByDefault();
bool ChooseGpuValidation(int argc, const char* const* argv);
GpuValidation ClassifyGpuValidation(bool requested,
                                    std::span<const std::string> sdl_reports);
std::string_view GpuValidationName(GpuValidation validation);

// SDL3 GPU device bound to the main window (plan D: SDL3 GPU now, optional
// D3D12 later). Phase 1a only clears the swapchain; the PS1-semantics renderer
// (RenderQueue, VRAM model, semi-transparency modes) is built on top of this in
// 1b.
class Device {
 public:
  static constexpr float kDefaultDisplayAspect = 4.0f / 3.0f;
  Device() = default;
  ~Device();
  Device(const Device&) = delete;
  Device& operator=(const Device&) = delete;

  // `validation` requests SDL GPU debug mode; Validation() reports the result.
  std::expected<void, std::string> Init(SDL_Window* window, bool validation);
  void Shutdown();

  // Acquires the swapchain, clears it and presents. Returns false if the frame
  // was skipped (e.g. minimized window).
  bool PresentClear(const SwapchainClearColor& color);

  // Shows an RGBA8 image (0xAABBGGRR) letterboxed at the given display aspect
  // (4:3 for the PS1 TV picture).
  bool PresentImage(const u32* rgba, i32 width, i32 height,
                    float aspect = kDefaultDisplayAspect);

  static std::optional<u32> FrameByteCount(i32 width, i32 height);

  const char* BackendName() const;
  GpuValidation Validation() const { return m_validation_; }

 private:
  bool EnsureFrameTexture(i32 width, i32 height);

  SDL_Window* m_window_ = nullptr;
  SDL_GPUDevice* m_device_ = nullptr;
  SDL_GPUTexture* m_frame_ = nullptr;
  SDL_GPUTransferBuffer* m_upload_ = nullptr;
  i32 m_frame_w_ = 0, m_frame_h_ = 0;
  GpuValidation m_validation_ = GpuValidation::kOff;
};

}  // namespace VS::Render
