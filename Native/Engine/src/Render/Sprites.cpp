// Hand-maintained (tools/port/owned.csv).
#include "Render/Sprites.hpp"

#include <climits>

namespace VS::Render {
namespace {
constexpr std::uint32_t kLinkMask = 0xFFFFFF;
constexpr int kCommandShift = 24;
constexpr int kTransparencyShift = 7;

void Queue(SpriteQueue& queue, const SpriteRequest& request) {
  queue.Emit(BuildSpritePacket(request, queue.ChainLink()));
}
}  // namespace

SpritePacket BuildSpritePacket(const SpriteRequest& request,
                               std::uint32_t next_link) {
  const auto brightness_word =
      static_cast<std::uint32_t>(request.brightness_transparency);
  const std::uint32_t brightness = brightness_word & UCHAR_MAX;
  const std::uint32_t code =
      kSpriteCommand +
      ((brightness_word >> kTransparencyShift) & kSemiTransparentSpriteFlag);
  SpritePacket packet{};
  packet[kSpriteTagWord] =
      (next_link & kLinkMask) |
      (static_cast<std::uint32_t>(kSpritePacketWords - 1) << kCommandShift);
  packet[kSpriteDrawModeWord] = request.draw_mode;
  packet[kSpriteColorWord] = brightness | (brightness << CHAR_BIT) |
                             (brightness << (2 * CHAR_BIT)) |
                             (code << kCommandShift);
  packet[kSpritePositionWord] =
      static_cast<std::uint32_t>(request.packed_position);
  packet[kSpriteSizeWord] = static_cast<std::uint32_t>(request.packed_size);
  return packet;
}

void QueueDefaultSprite(SpriteQueue& queue, int wh, int xy) {
  Queue(queue, {.draw_mode = kDefaultSpriteDrawMode,
                .brightness_transparency = kDefaultSpriteBrightness,
                .packed_position = xy,
                .packed_size = wh});
}

void QueueSpriteWithTexturePage(SpriteQueue& queue, int brightness_transparency,
                                int xy, int wh) {
  Queue(queue, {.draw_mode = kDefaultSpriteDrawMode,
                .brightness_transparency = brightness_transparency,
                .packed_position = xy,
                .packed_size = wh});
}

void QueueSprite(SpriteQueue& queue, int brightness_transparency, int xy,
                 int wh) {
  Queue(queue, {.draw_mode = 0,
                .brightness_transparency = brightness_transparency,
                .packed_position = xy,
                .packed_size = wh});
}

void LoadFrameViewMatrix(FrameViewTarget& target) {
  target.LoadViewRotation();
  target.LoadViewTranslation();
}
}  // namespace VS::Render
