// Hand-maintained (tools/port/owned.csv).
// Live binding for the scene's animated colours (PaletteAnimation.cpp).
#include "Render/PaletteAnimationLive.hpp"

#include <bit>
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

#include "Actors/AppearanceLive.hpp"
#include "Core/Math/ScalarMath.hpp"

extern "C" {
#include "Core/Diagnostics.h"
#include "Field/Background.h"
#include "Field/RoomRendering.h"
#include "Render/Palette.h"
}

namespace VS::Render {
namespace {
namespace Math = VS::Core::Math;

constexpr const char* kChannel = "render";
constexpr int kModeLimit = std::numeric_limits<std::uint8_t>::max();
static_assert(sizeof(ClutRowColors) == kClutColorCount * sizeof(std::uint16_t),
              "the animated rows upload as one contiguous block");

PaletteAnimation live_animation;
std::uint64_t refused_requests = 0;
std::uint32_t reported_steps = 0;

void ReportRefusal(const char* what) {
  if (std::has_single_bit(++refused_requests)) {
    Core_Errorf(kChannel, "palette request refused: %s (%" PRIu64 " refused)",
                what, refused_requests);
  }
}

// The animator holds a channel whose delta-table row or frame is outside its
// table instead of reading past it; each such step counts as a refusal.
void ReportHeldSteps() {
  while (reported_steps < live_animation.refused_steps) {
    ++reported_steps;
    ReportRefusal("delta-table step outside its table");
  }
}

void Upload(std::uint32_t x, std::uint32_t y, std::size_t width,
            std::size_t height, const std::uint16_t* pixels) {
  RECT rect{};
  setRECT(&rect, static_cast<std::int16_t>(x), static_cast<std::int16_t>(y),
          static_cast<std::int16_t>(width), static_cast<std::int16_t>(height));
  // LoadImage only reads the pixels; the core sized the span to the area.
  LoadImage(&rect,
            reinterpret_cast<u_long*>(const_cast<std::uint16_t*>(pixels)));
}

Render_ColorCode CodeOf(const TintCode& tint) {
  return {.red = tint[0], .green = tint[1], .blue = tint[2], .code = 0};
}

class LivePaletteHost final : public PaletteHost {
 public:
  void UploadClutColors(std::uint32_t row, std::uint32_t first_color,
                        std::span<const std::uint16_t> colors) override {
    if (!colors.empty()) {
      Upload(kClutVramX + first_color, kClutVramY + row, colors.size(), 1,
             colors.data());
    }
  }
  void UploadAnimatedClutRows(
      std::span<const ClutRowColors, kAnimatedClutRowCount> rows) override {
    Upload(kClutVramX, kClutVramY, kClutColorCount, kAnimatedClutRowCount,
           rows.front().data());
  }
  void SetRoomBackgroundColors(const BackgroundColors& colors) override {
    BackgroundColors words = colors;
    Field_SetRoomBackgroundColors(words.data());
  }
  void SetRoomTint(const TintCode& tint, std::int32_t blend) override {
    Render_ColorCode code = CodeOf(tint);
    Field_SetRoomTint(&code, blend);
  }
  void SetActorTint(std::uint32_t actor, const TintCode& tint) override {
    Render_ColorCode code = CodeOf(tint);
    (void)VS::Actors::SetLiveActorFadeParameters(static_cast<int>(actor),
                                                 &code);
  }
};

LivePaletteHost live_host;

struct Request {
  ColorTransform mode = ColorTransform::kOffsetCurrent;
  std::int16_t duration = 0;
};

// The callers pass the mode and duration as words; the original kept the
// mode as a byte and divided by the duration's halfword-sized range, so wider
// values are refused rather than truncated.
bool ReadRequest(int mode, int duration, Request& request) {
  if (mode < 0 || mode > kModeLimit) {
    ReportRefusal("colour transform mode outside a byte");
    return false;
  }
  if (duration < std::numeric_limits<std::int16_t>::min() ||
      duration > std::numeric_limits<std::int16_t>::max()) {
    ReportRefusal("colour transform duration outside a halfword");
    return false;
  }
  request = {.mode = static_cast<ColorTransform>(mode),
             .duration = static_cast<std::int16_t>(duration)};
  return true;
}
}  // namespace

bool LoadLiveClutRow(std::uint32_t row, std::uint32_t first_color,
                     std::span<const std::uint16_t> colors) {
  if (!LoadClutColors(live_animation, live_host, row, first_color, colors)) {
    ReportRefusal("CLUT row or colour span outside VRAM or its row");
    return false;
  }
  return true;
}

void LoadLiveClutColors(const u_short* colors, u_int row, u_int first_color,
                        u_int count) {
  if (colors == nullptr && count != 0) {
    ReportRefusal("null CLUT colours");
    return;
  }
  (void)LoadLiveClutRow(row, first_color,
                        std::span<const std::uint16_t>(colors, count));
}

void CommitLiveClutRows() { CommitClutRows(live_animation, live_host); }

void InitializeLiveClutAnimation() { InitializeClutAnimation(live_animation); }

void UpdateLiveBackgroundGradientTransition() {
  UpdateBackgroundGradientTransition(live_animation, live_host);
}

void UpdateLiveModelTintTransitions() {
  UpdateModelTintTransitions(live_animation, live_host);
}

void UpdateLiveClutTransitions() {
  UpdateClutTransitions(live_animation);
  ReportHeldSteps();
}

void CaptureLiveRoomBackgroundColors() {
  const int* room = Field_RoomBackgroundColors();
  if (room == nullptr) {
    ReportRefusal("null room background colours");
    return;
  }
  BackgroundColors colors{};
  for (std::size_t index = 0; index < colors.size(); ++index) {
    colors[index] = room[index];
  }
  CaptureRoomBackgroundColors(live_animation, colors);
}

void SetLiveClutAnimationEnabled(int enabled) {
  SetClutAnimationEnabled(live_animation, enabled);
}

void TransformLiveRoomPalettes(int mode, int duration, int red_offset,
                               std::int16_t green_offset,
                               std::int16_t blue_offset) {
  Request request;
  if (ReadRequest(mode, duration, request)) {
    TransformRoomPalettes(
        live_animation, request.mode, request.duration,
        {Math::WrapSigned16(static_cast<std::uint32_t>(red_offset)),
         green_offset, blue_offset});
  }
}

void TransformLiveSceneColors(int mode, int duration, std::int16_t red_offset,
                              std::int16_t green_offset,
                              std::int16_t blue_offset) {
  Request request;
  if (ReadRequest(mode, duration, request)) {
    TransformSceneColors(live_animation, live_host, request.mode,
                         request.duration,
                         {red_offset, green_offset, blue_offset});
  }
}

void TransformLiveActorColors(int mode, int duration, int actor,
                              std::int16_t red_offset,
                              std::int16_t green_offset,
                              std::int16_t blue_offset) {
  if (actor < 0) {
    ReportRefusal("negative actor for a colour transform");
    return;
  }
  Request request;
  if (ReadRequest(mode, duration, request)) {
    TransformActorColors(live_animation, live_host, request.mode,
                         request.duration, static_cast<std::uint32_t>(actor),
                         {red_offset, green_offset, blue_offset});
  }
}

void TransformLiveBackgroundColors(int mode, int duration,
                                   std::int16_t red_offset,
                                   std::int16_t green_offset,
                                   std::int16_t blue_offset) {
  Request request;
  if (ReadRequest(mode, duration, request)) {
    TransformBackgroundColors(live_animation, live_host, request.mode,
                              request.duration,
                              {red_offset, green_offset, blue_offset});
  }
}

const PaletteAnimation& LivePaletteAnimation() noexcept {
  return live_animation;
}

std::uint64_t RefusedLivePaletteRequestCount() noexcept {
  return refused_requests;
}
}  // namespace VS::Render
