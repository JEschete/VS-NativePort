// Hand-maintained (tools/port/owned.csv).
#include "Render/UiPackets.hpp"

#include "Core/Math/ScalarMath.hpp"

namespace VS::Render {
namespace {
constexpr std::uint32_t kTagByteMask = 0xFF000000;
constexpr std::uint32_t kNibbleMask = 0xF;
constexpr int kNibbleBits = 4;

constexpr std::uint32_t Tag(std::uint32_t next_link, std::size_t words) {
  return (next_link & kPacketLinkMask) |
         (static_cast<std::uint32_t>(words) << kPacketLengthShift);
}

// Glyph sizes (w | h << 16) by font, packed a nibble apart; the shift uses
// only its low five bits, as srav does.
constexpr std::uint32_t kGlyphSizes = 0x05CA0576;
constexpr std::uint32_t kGlyphSizeMask = 0xF000F;
constexpr std::uint32_t kGlyphSizeShift = 4;
constexpr std::uint32_t kShiftAmountMask = 31;
constexpr std::uint32_t kGlyphFontRowFlag = 2;
constexpr int kHalfwordBits = 16;
constexpr std::uint32_t kLowHalfMask = 0xFFFF;
// u 32, v 11 with the CLUT at (832, 223).
constexpr std::uint32_t kGlyphTexture = 0x37F40B20;
constexpr int kSeparatorFont = 2;
constexpr std::int32_t kGlyphUBase = 64;
constexpr std::int32_t kGlyphWidthBase = 3;
constexpr std::int32_t kGlyphUOffset = 192;
constexpr std::int32_t kDigitAdvance = 5;

// The gauge: a dark frame tile, the draw mode for the semi-transparent
// gradient, and the Gouraud bar between the palette colour and the fill.
constexpr std::uint32_t kGaugeFrameTag = 0x03000000;
constexpr std::uint32_t kGaugeFullTag = 0x0D000000;
constexpr std::uint32_t kGaugeFrameColor = 0x60402800;
constexpr std::uint32_t kGaugeFrameSize = 0x50042;
constexpr std::uint32_t kGaugeDitheredDrawMode = 0xE1000200;
constexpr std::uint32_t kGaugeGouraudCommand = 0x38000000;
constexpr std::uint32_t kGaugeBarOffset = 0x10001;
constexpr std::uint32_t kGaugeBarHeight = 0x30000;
constexpr std::uint32_t kGaugeResetDrawMode = 0xE1000000;
constexpr std::uint32_t kGaugeLength = 0x40;
constexpr int kGaugeShift = 6;
constexpr std::uint32_t kGaugeRedFull = 0xFF;
constexpr std::uint32_t kGaugeGreenFull = 0xF0;
constexpr std::uint32_t kGaugeBlueFull = 0x9E;
constexpr int kGreenShift = 8;
constexpr int kBlueShift = 16;
constexpr std::uint32_t kByteMask = 0xFF;

enum StatGaugeWord : std::uint8_t {
  kGaugeTag,
  kGaugeFrameColorWord,
  kGaugeFramePosition,
  kGaugeFrameSizeWord,
  kGaugeDrawMode,
  kGaugeTopLeftColor,
  kGaugeTopLeft,
  kGaugeTopRightColor,
  kGaugeTopRight,
  kGaugeBottomLeftColor,
  kGaugeBottomLeft,
  kGaugeBottomRightColor,
  kGaugeBottomRight,
  kGaugeResetMode,
};

constexpr int kBlendChannels = 3;
constexpr std::uint32_t kBlendSteps = 8;
constexpr int kBlendShift = 10;
constexpr int kBlendChannelShift = 16;
constexpr int kBlendStepShift = 3;

// The texture move: a cache flush, then the VRAM-to-VRAM copy command.
constexpr std::uint32_t kMoveCacheFlush = 0x01000000;
constexpr std::uint32_t kMoveCommand = 0x80000000;

// The texture-page draw mode; the sprite command (0x64) and its neutral grey.
constexpr std::uint32_t kTexturePageCommand = 0xE1000000;
constexpr std::uint32_t kTexturePageMask = 0x9FF;
constexpr std::uint32_t kTexturePageDither = 0x200;
constexpr std::uint32_t kImmediateSpriteCommand = 0x64000000;
constexpr std::int32_t kNeutralBrightness = 0x80;
constexpr std::int32_t kMaxBrightness = 0xFF;
constexpr std::uint32_t kGreyChannels = 0x010101;

constexpr std::uint32_t PackPair(std::int32_t low, std::int32_t high) noexcept {
  return (static_cast<std::uint32_t>(high) << kHalfwordBits) |
         (static_cast<std::uint32_t>(low) & kLowHalfMask);
}

std::uint32_t GaugeChannel(std::uint32_t full, std::uint32_t base,
                           std::uint32_t fill) {
  return (fill * full + base * (kGaugeLength - fill)) >> kGaugeShift;
}
}  // namespace

TilePacket BuildTilePacket(const TileRequest& request,
                           std::uint32_t next_link) noexcept {
  return {Tag(next_link, kTilePacketWords - 1), request.color_command,
          request.position, request.size};
}

bool InsertThreeWordPrimitive(PacketQueue& queue, const TileRequest& request) {
  return queue.Emit(BuildTilePacket(request, queue.ChainLink()));
}

DrawModePacket BuildDrawModePacket(std::uint32_t draw_mode,
                                   std::uint32_t next_link) noexcept {
  return {Tag(next_link, kDrawModePacketWords - 1), draw_mode};
}

bool InsertDrawModeCommand(PacketQueue& queue, std::uint32_t draw_mode) {
  return queue.Emit(BuildDrawModePacket(draw_mode, queue.ChainLink()));
}

SpritePacket BuildNumericGlyphPacket(const NumericGlyph& glyph,
                                     std::uint32_t next_link) {
  using Core::Math::WrappingAdd;
  using Core::Math::WrappingMultiply;
  using Core::Math::WrappingSubtract;
  const auto font = static_cast<std::uint32_t>(glyph.font);
  const std::uint32_t row_offset = (font & kGlyphFontRowFlag) << kHalfwordBits;
  const std::uint32_t size =
      (kGlyphSizes >> ((font * kGlyphSizeShift) & kShiftAmountMask)) &
      kGlyphSizeMask;
  SpritePacket packet = BuildSpritePacket(
      {.draw_mode = kDefaultSpriteDrawMode,
       .brightness_transparency = kDefaultSpriteBrightness,
       .packed_position =
           WrappingAdd(glyph.position, static_cast<std::int32_t>(row_offset)),
       .packed_size = static_cast<std::int32_t>(size)},
      next_link);
  packet[kSpriteTextureWord] = kGlyphTexture;
  if (glyph.font != kSeparatorFont) {
    const std::int32_t u = WrappingSubtract(
        WrappingMultiply(
            WrappingAdd(WrappingMultiply(glyph.digit, 2), kGlyphUBase),
            WrappingAdd(glyph.font, kGlyphWidthBase)),
        kGlyphUOffset);
    packet[kSpriteTextureWord] = (kGlyphTexture & ~kLowHalfMask) |
                                 (static_cast<std::uint32_t>(u) & kLowHalfMask);
  }
  return packet;
}

bool DrawNumericGlyph(PacketQueue& queue, const NumericGlyph& glyph) {
  return queue.Emit(BuildNumericGlyphPacket(glyph, queue.ChainLink()));
}

RightAlignedNumber DrawIntegerRightAligned(PacketQueue& queue,
                                           const NumberRequest& request) {
  using Core::Math::WrappingSubtract;
  std::int32_t value = request.value;
  std::int32_t position = request.position;
  for (int digit = 0; digit < kMaxDecimalDigits; ++digit) {
    value = Core::Math::SplitDecimalDigit(value);
    DrawNumericGlyph(queue,
                     {.font = request.font,
                      .position = position,
                      .digit = static_cast<std::int32_t>(
                          static_cast<std::uint32_t>(value) & kNibbleMask)});
    position = WrappingSubtract(WrappingSubtract(position, kDigitAdvance),
                                request.font);
    value >>= kNibbleBits;
    if (value == 0) {
      return {.next_position = position, .truncated = false};
    }
  }
  return {.next_position = position, .truncated = true};
}

std::optional<StatGaugePacket> BuildStatGaugePacket(
    std::span<const std::uint32_t> palette, const StatGaugeRequest& request,
    std::uint32_t next_link) noexcept {
  if (request.color_index >= palette.size()) {
    return std::nullopt;
  }
  const std::uint32_t base = palette[request.color_index];
  const auto fill = static_cast<std::uint32_t>(request.fill_width);
  const std::uint32_t fill_color =
      GaugeChannel(kGaugeRedFull, base & kByteMask, fill) |
      (GaugeChannel(kGaugeGreenFull, (base >> kGreenShift) & kByteMask, fill)
       << kGreenShift) |
      // The original left the blue source unmasked.
      (GaugeChannel(kGaugeBlueFull, base >> kBlueShift, fill) << kBlueShift);
  const auto position = static_cast<std::uint32_t>(request.position);
  const std::uint32_t top_left = position + kGaugeBarOffset;
  const std::uint32_t bottom_left = top_left + kGaugeBarHeight;
  StatGaugePacket packet{};
  packet[kGaugeTag] = (next_link & kPacketLinkMask) |
                      (fill == 0 ? kGaugeFrameTag : kGaugeFullTag);
  packet[kGaugeFrameColorWord] = kGaugeFrameColor;
  packet[kGaugeFramePosition] = position;
  packet[kGaugeFrameSizeWord] = kGaugeFrameSize;
  packet[kGaugeDrawMode] = kGaugeDitheredDrawMode;
  packet[kGaugeTopLeftColor] = fill_color | kGaugeGouraudCommand;
  packet[kGaugeTopLeft] = top_left;
  packet[kGaugeTopRightColor] = base;
  packet[kGaugeTopRight] = top_left + fill;
  packet[kGaugeBottomLeftColor] = fill_color;
  packet[kGaugeBottomLeft] = bottom_left;
  packet[kGaugeBottomRightColor] = base;
  packet[kGaugeBottomRight] = bottom_left + fill;
  packet[kGaugeResetMode] = kGaugeResetDrawMode;
  return packet;
}

bool DrawStatGauge(PacketQueue& queue, std::span<const std::uint32_t> palette,
                   const StatGaugeRequest& request) {
  const std::optional<StatGaugePacket> packet =
      BuildStatGaugePacket(palette, request, queue.ChainLink());
  return packet.has_value() && queue.Emit(*packet);
}

std::uint32_t BlendRgbWithBrightness(std::uint32_t color0, std::uint32_t color1,
                                     std::int32_t brightness,
                                     std::int32_t blend_factor) noexcept {
  const auto factor = static_cast<std::uint32_t>(blend_factor);
  const auto scale = static_cast<std::uint32_t>(brightness);
  std::uint32_t blended = 0;
  for (int channel = 0; channel < kBlendChannels; ++channel) {
    const std::uint32_t weighted =
        (((color0 >> kBlendChannelShift) & kByteMask) * (kBlendSteps - factor) +
         ((color1 >> kBlendChannelShift) & kByteMask) * factor) *
        scale;
    blended = (blended << kGreenShift) + (weighted >> kBlendShift);
    color0 <<= kGreenShift;
    color1 <<= kGreenShift;
  }
  return blended;
}

SpliceResult SplicePrimitiveList(PrimitiveChain& chain,
                                 const ChainBucket& bucket,
                                 std::uint32_t primitive) {
  const std::optional<std::uint32_t> primitive_tag = chain.TagWord(primitive);
  if (!primitive_tag.has_value()) {
    return SpliceResult::kMissingPrimitive;
  }
  std::uint32_t current = bucket.head;
  for (std::uint32_t step = 0; step < kMaxChainSteps; ++step) {
    const std::optional<std::uint32_t> tag = chain.TagWord(current);
    if (!tag.has_value()) {
      return SpliceResult::kBrokenChain;
    }
    if ((*tag & kPacketLinkMask) == bucket.next) {
      chain.SetTagWord(current, (*tag & kTagByteMask) | primitive);
      chain.SetTagWord(primitive,
                       (*primitive_tag & kTagByteMask) | bucket.next);
      return SpliceResult::kSpliced;
    }
    current = *tag & kPacketLinkMask;
  }
  return SpliceResult::kBrokenChain;
}

SpliceResult AppendOverlayPrimitive(PrimitiveChain& chain,
                                    std::uint32_t primitive) {
  const std::optional<ChainBucket> bucket = chain.OverlayBucket();
  if (!bucket.has_value()) {
    return SpliceResult::kNoOrderingTable;
  }
  return SplicePrimitiveList(chain, *bucket, primitive);
}

std::optional<ScreenRect> ImageRectangle(std::int32_t position,
                                         std::int32_t size) noexcept {
  using Core::Math::WrapSigned16;
  const auto packed_position = static_cast<std::uint32_t>(position);
  const auto packed_size = static_cast<std::uint32_t>(size);
  const ScreenRect area{WrapSigned16(packed_position),
                        WrapSigned16(packed_position >> kHalfwordBits),
                        WrapSigned16(packed_size),
                        WrapSigned16(packed_size >> kHalfwordBits)};
  if (area.width <= 0 || area.height <= 0) {
    return std::nullopt;
  }
  return area;
}

bool UploadImageRectangle(ImageUploadTarget& target, std::int32_t position,
                          std::int32_t size) {
  const std::optional<ScreenRect> area = ImageRectangle(position, size);
  if (!area.has_value()) {
    return false;
  }
  target.Upload(*area);
  return true;
}

bool ReadImageRectangle(ImageReadTarget& target, std::int32_t position,
                        std::int32_t size) {
  const std::optional<ScreenRect> area = ImageRectangle(position, size);
  if (!area.has_value()) {
    return false;
  }
  target.Read(*area);
  return true;
}

TextureMovePacket BuildTextureMovePacket(const ScreenRect& source,
                                         std::int32_t x,
                                         std::int32_t y) noexcept {
  return {Tag(0, kTextureMovePacketWords - 1),
          kMoveCacheFlush,
          kMoveCommand,
          PackPair(source.x, source.y),
          PackPair(x, y),
          PackPair(source.width, source.height)};
}

bool QueueTextureRectangleMove(OverlayPacketQueue& queue,
                               const ScreenRect& source, std::int32_t x,
                               std::int32_t y) {
  return queue.AppendOverlay(BuildTextureMovePacket(source, x, y));
}

bool InsertTexturePageCommand(PacketQueue& queue, std::uint32_t texture_page) {
  return InsertDrawModeCommand(queue, kTexturePageCommand | kTexturePageDither |
                                          (texture_page & kTexturePageMask));
}

std::optional<ImmediateSpritePacket> BuildImmediateSpritePacket(
    const ImmediateSpriteRequest& request) noexcept {
  const auto page = static_cast<std::uint32_t>(request.texture_page);
  const std::int32_t brightness =
      kNeutralBrightness - Core::Math::WrapSigned16(page >> kHalfwordBits);
  if (brightness < 0 || brightness > kMaxBrightness) {
    return std::nullopt;
  }
  return ImmediateSpritePacket{
      Tag(0, kImmediateSpriteWords - 1),
      kTexturePageCommand | (page & kTexturePageMask),
      kImmediateSpriteCommand |
          (static_cast<std::uint32_t>(brightness) * kGreyChannels),
      static_cast<std::uint32_t>(request.position),
      static_cast<std::uint32_t>(request.uv_clut),
      static_cast<std::uint32_t>(request.size)};
}

bool DrawImmediateSprite(ImmediatePacketTarget& target,
                         const ImmediateSpriteRequest& request) {
  const std::optional<ImmediateSpritePacket> packet =
      BuildImmediateSpritePacket(request);
  if (!packet.has_value()) {
    return false;
  }
  target.Submit(*packet);
  return true;
}

std::uint32_t BlendRgb(std::uint32_t color1, std::uint32_t color2,
                       std::uint32_t factor) noexcept {
  std::uint32_t blended = 0;
  for (int channel = 0; channel < kBlendChannels; ++channel) {
    const std::uint32_t weighted =
        ((color1 >> kBlendChannelShift) & kByteMask) * (kBlendSteps - factor) +
        ((color2 >> kBlendChannelShift) & kByteMask) * factor;
    blended = (blended << kGreenShift) + (weighted >> kBlendStepShift);
    color1 <<= kGreenShift;
    color2 <<= kGreenShift;
  }
  return blended;
}
}  // namespace VS::Render
