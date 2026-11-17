#include <array>
#include <cstdint>

#include "Menu/Status/TerminalControllers.hpp"

// NOLINTNEXTLINE(readability-identifier-naming): the catalog's component.
namespace VS::Menu::Status {
namespace {
namespace math = Core::Math;
constexpr std::uint32_t kGradientWords = 13;
constexpr std::uint32_t kSpriteWords = 8;
constexpr int kModelSlideCenter = 128;
constexpr int kSlideScale = 8;
constexpr int kOrbitScale = 10;
constexpr int kHeightScale = 2048;
constexpr int kVerticalOrbitScale = 4;
constexpr int kCameraClip = 4096;
}  // namespace

// decomp: _fadeScreenLeft (src/MENU/MENU4.PRG/120.c:2081)
bool DrawModelSlideFade(Render::PacketQueue& queue, bool enabled,
                        bool framebuffer) {
  if (!enabled) {
    return true;
  }
  const std::array packet{
      Render::PackTagWord(kGradientWords, queue.ChainLink()),
      0xE1000000U,
      0x3C000000U,
      0U,
      0U,
      0x00808080U,
      96U,
      framebuffer ? 0x01000060U : 0x01050060U,
      0U,
      0x00F00000U,
      0x0000F000U,
      0x00808080U,
      0x00F00060U,
      0x0000F060U};
  return queue.Emit(packet);
}

// decomp: _drawMenuBackground (src/MENU/MENU4.PRG/120.c:2121)
bool DrawBackground(Render::PacketQueue& queue, bool framebuffer) {
  const std::array gradient{
      Render::PackTagWord(kGradientWords, queue.ChainLink()),
      0x3E101010U,
      64U,
      0x38F00040U,
      0x00909090U,
      160U,
      0x00BA00A0U,
      0x00101010U,
      0x00F00040U,
      0x0000F040U,
      0x00909090U,
      0x00F000A0U,
      0x0000F0A0U,
      0xE1000200U};
  if (!queue.Emit(gradient)) {
    return false;
  }
  const std::array image{Render::PackTagWord(kSpriteWords, queue.ChainLink()),
                         0xE10000BBU,
                         0x64808080U,
                         160U,
                         0x38F00020U,
                         0x00F00060U,
                         0x60000000U,
                         256U,
                         0x00F00040U};
  if (!queue.Emit(image)) {
    return false;
  }
  const std::array framebuffer_gradient{
      Render::PackTagWord(kGradientWords, queue.ChainLink()),
      0xE1000000U,
      0x3C808080U,
      64U,
      64U,
      0U,
      160U,
      framebuffer ? 0x010000A0U : 0x010500A0U,
      0x00808080U,
      0x00F00040U,
      0x0000F040U,
      0U,
      0x00F000A0U,
      0x0000F0A0U};
  return queue.Emit(framebuffer_gradient);
}

// decomp: _drawScreen (src/MENU/MENU4.PRG/120.c:2190)
TerminalResult DrawFrame(TerminalFrameHost& host) {
  const auto model = host.ModelCamera();
  if (!model) {
    return std::unexpected(TerminalError::kMissingActor);
  }
  auto& frame = host.Frame();
  auto& camera = host.Camera();
  const int slide =
      math::WrappingSubtract(kModelSlideCenter, frame.animation_step);
  const int yaw_sine = math::Sine(frame.yaw);
  const int yaw_cosine = math::Cosine(frame.yaw);
  const int pitch_sine = math::Sine(frame.pitch);
  const int pitch_cosine = math::Cosine(frame.pitch);
  camera.look_at.x = math::WrappingMultiply(
      math::WrappingMultiply(math::WrappingNegate(yaw_sine), slide) >>
          kSlideScale,
      model->distance);
  camera.look_at.y = -static_cast<int>(model->height) * kHeightScale;
  camera.look_at.z = math::WrappingMultiply(
      math::WrappingMultiply(yaw_cosine, slide) >> kSlideScale,
      model->distance);
  camera.position.x = math::WrappingAdd(
      camera.look_at.x,
      math::WrappingMultiply(
          math::WrappingMultiply(yaw_cosine, pitch_cosine) >> kOrbitScale,
          model->distance));
  camera.position.y = math::WrappingSubtract(
      camera.look_at.y, math::WrappingMultiply(
                            math::WrappingMultiply(pitch_sine, model->distance),
                            kVerticalOrbitScale));
  camera.position.z = math::WrappingAdd(
      camera.look_at.z,
      math::WrappingMultiply(
          math::WrappingMultiply(yaw_sine, pitch_cosine) >> kOrbitScale,
          model->distance));
  camera.angles = {};
  camera.far_clip = kCameraClip;
  if (!host.BuildCamera()) {
    return std::unexpected(TerminalError::kUnavailableService);
  }
  if (frame.model_delay != 0) {
    --frame.model_delay;
  } else if (!host.DrawPreview()) {
    return std::unexpected(TerminalError::kUnavailableService);
  }
  if (!(host.Background() && host.Fade() && host.LimbPanel() &&
        host.Vitals())) {
    return std::unexpected(TerminalError::kUnavailableService);
  }
  return 0;
}
}  // namespace VS::Menu::Status
