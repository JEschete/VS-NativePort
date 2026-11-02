#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>

// Textured sprite packets for the HUD, menus and fonts. The policy builds the
// six-word GPU packet (tag, draw mode, colour and command, position, texture,
// size) and hands it to a SpriteQueue, so it reaches no global render state.
namespace VS::Render {
inline constexpr std::size_t kSpritePacketWords = 6;
using SpritePacket = std::array<std::uint32_t, kSpritePacketWords>;

enum SpritePacketWord : std::uint8_t {
  kSpriteTagWord,
  kSpriteDrawModeWord,
  kSpriteColorWord,
  kSpritePositionWord,
  kSpriteTextureWord,
  kSpriteSizeWord,
};

// Texture page 0x0C with dithering enabled.
inline constexpr std::uint32_t kDefaultSpriteDrawMode = 0xE100000Cu;
inline constexpr int kDefaultSpriteBrightness = 0x80;
// Code 0x64 is the opaque textured sprite; bit 8 of the brightness word makes
// it the semi-transparent 0x66.
inline constexpr std::uint32_t kSpriteCommand = 0x64;
inline constexpr std::uint32_t kSemiTransparentSpriteFlag = 2;

struct SpriteRequest {
  std::uint32_t draw_mode = 0;
  int brightness_transparency = 0;
  int packed_position = 0;
  int packed_size = 0;
};

// The packet linked in front of `next_link` (the low 24 bits of the chain
// head). The texture word is left zero for the caller to fill.
[[nodiscard]] SpritePacket BuildSpritePacket(const SpriteRequest& request,
                                             std::uint32_t next_link);

class SpriteQueue {
 public:
  virtual ~SpriteQueue() = default;
  // The chain head the packet is linked in front of.
  [[nodiscard]] virtual std::uint32_t ChainLink() const = 0;
  // Stores the packet and points the chain head at it.
  virtual void Emit(const SpritePacket& packet) = 0;
};

// BATTLE.PRG vs_battle_setSpriteDefault (573B8.c:270): the default draw mode
// and brightness.
void QueueDefaultSprite(SpriteQueue& queue, int wh, int xy);
// BATTLE.PRG vs_battle_setSpriteDefaultTexPage (573B8.c:275).
void QueueSpriteWithTexturePage(SpriteQueue& queue, int brightness_transparency,
                                int xy, int wh);
// BATTLE.PRG vs_battle_setSprite (573B8.c:281): no draw-mode change.
void QueueSprite(SpriteQueue& queue, int brightness_transparency, int xy,
                 int wh);

class FrameViewTarget {
 public:
  virtual ~FrameViewTarget() = default;
  virtual void LoadViewRotation() = 0;
  virtual void LoadViewTranslation() = 0;
};

// BATTLE.PRG func_800C02A8 (573B8.c:315): the frame's view matrix becomes the
// GTE rotation and translation.
void LoadFrameViewMatrix(FrameViewTarget& target);
}  // namespace VS::Render
