#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Render/DisplayControl.hpp"
#include "Render/Sprites.hpp"

// The battle HUD's GPU packets (tiles, draw-mode commands, digit glyphs, stat
// gauges), the overlay primitive splice and VRAM image uploads. The policies
// build packets as words and hand them to a PacketQueue, walk primitive
// chains by handle through a PrimitiveChain and upload through an
// ImageUploadTarget; they reach no global render state (UiPacketsLive.cpp).
namespace VS::Render {
inline constexpr std::uint32_t kPacketLinkMask = 0xFFFFFF;
inline constexpr int kPacketLengthShift = 24;

class PacketQueue {
 public:
  virtual ~PacketQueue() = default;
  // The chain head the packet is linked in front of.
  [[nodiscard]] virtual std::uint32_t ChainLink() const = 0;
  // Stores the packet and points the chain head at it; false when the packet
  // was dropped (no chain or no room).
  virtual bool Emit(std::span<const std::uint32_t> packet) = 0;
};

// A flat rectangle: its colour-and-command word, packed position and size.
struct TileRequest {
  std::uint32_t color_command = 0;
  std::uint32_t position = 0;
  std::uint32_t size = 0;
};

inline constexpr std::size_t kTilePacketWords = 4;
using TilePacket = std::array<std::uint32_t, kTilePacketWords>;

[[nodiscard]] TilePacket BuildTilePacket(const TileRequest& request,
                                         std::uint32_t next_link) noexcept;

// BATTLE.PRG vs_battle_addTile (5BF94.c:4503).
bool InsertThreeWordPrimitive(PacketQueue& queue, const TileRequest& request);

inline constexpr std::size_t kDrawModePacketWords = 2;
using DrawModePacket = std::array<std::uint32_t, kDrawModePacketWords>;

// One command word: the tag's length is 1. The original's vs_getTag(sizeof
// (int)) gave length 2 on a 64-bit host (#279).
[[nodiscard]] DrawModePacket BuildDrawModePacket(
    std::uint32_t draw_mode, std::uint32_t next_link) noexcept;

// BATTLE.PRG vs_battle_insertTpage (5BF94.c:4514).
bool InsertDrawModeCommand(PacketQueue& queue, std::uint32_t draw_mode);

// Font 2 is the separator ('/'); fonts 0 and 1 draw digits 0-9.
struct NumericGlyph {
  int font = 0;
  std::int32_t position = 0;
  std::int32_t digit = 0;
};

// The sprite for one glyph of the HUD number font (CLUT 832,223).
[[nodiscard]] SpritePacket BuildNumericGlyphPacket(const NumericGlyph& glyph,
                                                   std::uint32_t next_link);

// BATTLE.PRG _renderDigit (5BF94.c:3021).
bool DrawNumericGlyph(PacketQueue& queue, const NumericGlyph& glyph);

// A non-negative int32 has at most ten decimal digits.
inline constexpr int kMaxDecimalDigits = 10;

struct NumberRequest {
  int font = 0;
  std::int32_t position = 0;
  std::int32_t value = 0;
};

struct RightAlignedNumber {
  // The position left of the last digit drawn.
  std::int32_t next_position = 0;
  // The original never terminated for a negative value or one of
  // 1342177280 or more (whose shifted quotient wraps negative); ten digits
  // are drawn instead.
  bool truncated = false;
};

// BATTLE.PRG vs_battle_renderValue (5BF94.c:3033): digits right to left, each
// 5 + font pixels left of the previous.
RightAlignedNumber DrawIntegerRightAligned(PacketQueue& queue,
                                           const NumberRequest& request);

struct StatGaugeRequest {
  std::uint32_t color_index = 0;
  // Filled pixels of the 64-pixel bar.
  std::int32_t fill_width = 0;
  std::int32_t position = 0;
};

inline constexpr std::size_t kStatGaugePacketWords = 14;
using StatGaugePacket = std::array<std::uint32_t, kStatGaugePacketWords>;

// The gauge frame and gradient, or nothing for a colour index outside the
// palette (the original read past its four entries).
[[nodiscard]] std::optional<StatGaugePacket> BuildStatGaugePacket(
    std::span<const std::uint32_t> palette, const StatGaugeRequest& request,
    std::uint32_t next_link) noexcept;

// BATTLE.PRG vs_battle_renderStatBar (5BF94.c:3054).
bool DrawStatGauge(PacketQueue& queue, std::span<const std::uint32_t> palette,
                   const StatGaugeRequest& request);

// BATTLE.PRG _lerpColor (5BF94.c:2704): each byte of the 0xBBGGRR colours
// blended by blend_factor / 8 and scaled by brightness / 128. A channel past
// 255 carries into the next byte, as in the original.
[[nodiscard]] std::uint32_t BlendRgbWithBrightness(
    std::uint32_t color0, std::uint32_t color1, std::int32_t brightness,
    std::int32_t blend_factor) noexcept;

// An ordering-table bucket: its head entry and the entry the bucket's chain
// ends at.
struct ChainBucket {
  std::uint32_t head = 0;
  std::uint32_t next = 0;
};

// Packet tag words by handle (the low 24 bits of a tag link to the next).
class PrimitiveChain {
 public:
  virtual ~PrimitiveChain() = default;
  [[nodiscard]] virtual std::optional<std::uint32_t> TagWord(
      std::uint32_t handle) const = 0;
  virtual void SetTagWord(std::uint32_t handle, std::uint32_t word) = 0;
  // The overlay bucket of the current ordering table, if one is bound.
  [[nodiscard]] virtual std::optional<ChainBucket> OverlayBucket() const = 0;
};

enum class SpliceResult : std::uint8_t {
  kSpliced,
  // A link left the chain or the walk exceeded kMaxChainSteps.
  kBrokenChain,
  kMissingPrimitive,
  kNoOrderingTable,
};

inline constexpr std::uint32_t kMaxChainSteps = 1U << 16;

// BATTLE.PRG func_800C64D0 (5BF94.c:1332): links `primitive` last in the
// bucket's chain. Nothing is written unless the result is kSpliced.
SpliceResult SplicePrimitiveList(PrimitiveChain& chain,
                                 const ChainBucket& bucket,
                                 std::uint32_t primitive);

// BATTLE.PRG func_8007A824 (146C.c:8055): splices into the overlay bucket.
SpliceResult AppendOverlayPrimitive(PrimitiveChain& chain,
                                    std::uint32_t primitive);

class ImageUploadTarget {
 public:
  virtual ~ImageUploadTarget() = default;
  // Copies the target's pixels into the VRAM area.
  virtual void Upload(const ScreenRect& area) = 0;
};

// The packed x | y << 16 position and w | h << 16 size as a VRAM area, or
// nothing when either size is not positive.
[[nodiscard]] std::optional<ScreenRect> ImageRectangle(
    std::int32_t position, std::int32_t size) noexcept;

// BATTLE.PRG vs_battle_renderImage (5BF94.c:4531). The original kept the
// rectangle in an eight-entry ring for its asynchronous LoadImage; the native
// upload is synchronous.
bool UploadImageRectangle(ImageUploadTarget& target, std::int32_t position,
                          std::int32_t size);
}  // namespace VS::Render
