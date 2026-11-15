// Hand-maintained (tools/port/owned.csv).
#include "Effects/ScreenDistortion/ScreenOverlays.hpp"

#include <algorithm>
#include <array>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <span>

#include "Core/Math/ScalarMath.hpp"
#include "Render/Sprites.hpp"

namespace VS::Effects::ScreenDistortion {
namespace {
using Core::Math::WrappingAdd;
using Core::Math::WrappingMultiply;

constexpr int kHalfwordShift = 16;

// Links `packet` (its first word is the tag) in front of the queue's chain.
void LinkPacket(Render::PacketQueue& queue, std::span<std::uint32_t> packet) {
  packet[0] = (queue.ChainLink() & Render::kPacketLinkMask) |
              (static_cast<std::uint32_t>(packet.size() - 1)
               << Render::kPacketLengthShift);
  static_cast<void>(queue.Emit(packet));
}

constexpr std::uint32_t Pack(std::int32_t low, std::int32_t high) {
  return (static_cast<std::uint32_t>(low) & UINT16_MAX) |
         (static_cast<std::uint32_t>(high) << kHalfwordShift);
}

// Feedback tiles (0.c:22). The brightness/transparency words select a 0x60
// brightness semi-transparent (0x160) or opaque (0x60) sprite.
constexpr int kBlendedSprite = 0x160;
constexpr int kOpaqueSprite = 0x60;
constexpr std::uint32_t kDitheredDrawMode = 0xE1000200;
constexpr std::uint32_t kQuarterBlendPages = 0x170;
constexpr std::uint32_t kHalfBlendPages = 0x110;
constexpr std::int32_t kCaptureX = 0x2C0;
constexpr std::int32_t kPageMask = 0x3FF;
constexpr int kPageShift = 6;
constexpr std::int32_t kTopRow = 192;
constexpr std::int32_t kRowStep = 32;
constexpr std::int32_t kLastStrip = 0x100;
constexpr std::int32_t kStripStep = 64;
constexpr std::int32_t kEdgeColumn = 0x40;
constexpr std::int32_t kEdgeStep = 0x20;
constexpr std::int32_t kLastTileColumn = 0x120;
constexpr std::int32_t kTileStep = 32;
constexpr std::int32_t kTilePasses = 3;
constexpr std::int32_t kTileColumnPageMask = 0x1C0;
constexpr std::int32_t kTileTextureXMask = 0x20;
constexpr std::int32_t kInset = 2;
constexpr std::int32_t kEdgeInset = 0x1E;
constexpr std::int32_t kEdgeTextureInset = 0x1C;
constexpr std::uint32_t kEdgeColumnSize = 0x00040040;
constexpr std::uint32_t kEdgeStripSize = 0x001C0004;
constexpr std::uint32_t kInnerTileSize = 0x001C001C;
constexpr std::uint32_t kFullTileSize = 0x00200020;
constexpr std::int32_t kFeedbackLayerBias = 3;

// The capture packet: drawing area (704, 256)-(1023, 479), offset (704, 256),
// a black 320x224 rectangle, then mask bit set.
constexpr std::array<std::uint32_t, 7> kCapturePayload{{
    0xE30402C0,
    0xE4077FFF,
    0xE50802C0,
    0x60000000,
    0,
    0x00E00140,
    0xE6000001,
}};
// Packet sizes including the ordering-tag word.
constexpr std::size_t kRestoreWords = 5;
constexpr std::size_t kCornerTriangleWords = 7;
constexpr std::size_t kRimQuadWords = 10;
constexpr std::int32_t kSecondBufferX = 0x140;
constexpr std::int32_t kDisplayRight = 0x13F;
constexpr std::uint32_t kDrawAreaTopLeft = 0xE3000000;
constexpr std::uint32_t kDrawAreaBottomRight = 0xE4037C00;
constexpr std::uint32_t kDrawOffset = 0xE5000000;
constexpr std::uint32_t kMaskOff = 0xE6000000;

std::uint32_t PageBits(std::int32_t x) {
  return static_cast<std::uint32_t>(((x + kCaptureX) & kPageMask) >>
                                    kPageShift);
}

void QueueTile(Render::PacketQueue& queue, int sprite, std::uint32_t xy,
               std::uint32_t wh, std::uint32_t draw_mode,
               std::uint32_t texture) {
  Render::SpritePacket packet =
      Render::BuildSpritePacket({.draw_mode = draw_mode,
                                 .brightness_transparency = sprite,
                                 .packed_position = static_cast<int>(xy),
                                 .packed_size = static_cast<int>(wh)},
                                queue.ChainLink());
  packet[Render::kSpriteTextureWord] = texture;
  static_cast<void>(queue.Emit(packet));
}

// Vignette (5E8.c). Colours and commands of the corner triangles (0x32,
// shaded semi-transparent) and rim quads (0x3A).
constexpr std::int32_t kOuterRadius = 0x80;
constexpr std::int32_t kInnerRadius = 0x60;
constexpr std::int32_t kCornerTriangles = 4;
constexpr std::int32_t kRimQuads = 0x40;
constexpr std::uint32_t kCornerColorCommand = 0x32FFFFFF;
constexpr std::uint32_t kRimGrey = 0xA0A0A0;
constexpr std::uint32_t kRimDrawMode = 0xE1000220;
constexpr std::uint32_t kRimColorCommand = 0x3AA0A0A0;
struct VignetteCorner {
  std::uint32_t xy;
  std::int32_t first_step;
};
constexpr std::array<VignetteCorner, 4> kVignetteCorners{{
    {.xy = 0x00E00140, .first_step = 6},
    {.xy = 0x00E00000, .first_step = 0x16},
    {.xy = 0, .first_step = 0x26},
    {.xy = 0x140, .first_step = 0x36},
}};

constexpr std::int32_t kScallopBias = 8;
constexpr std::int32_t kScallopMask = 0xF;
constexpr std::int32_t kScallopBase = 0x40;
constexpr int kStepAngleShift = 6;
constexpr std::int32_t kWidening = 0xB;
constexpr int kWidenedShift = 0xF;
constexpr int kQ12Shift = 0xC;
constexpr int kScallopShift = 6;
constexpr std::int32_t kCentreX = 0xA0;
constexpr std::int32_t kCentreY = 0x70;
}  // namespace

// decomp: GIM/SCREFF2.PRG func_800F9800 (0.c:8).
void SetFeedbackLayer(ScreenOverlayControls& controls,
                      std::int32_t ordering_layer,
                      FeedbackTexturePages& pages) {
  controls.feedback_layer = static_cast<std::uint16_t>(ordering_layer);
  if (controls.feedback_pages_reserved == 0) {
    if (ordering_layer != 0) {
      controls.feedback_pages_reserved = 1;
      pages.Reserve(kFeedbackTexturePages);
    }
  } else if (ordering_layer == 0) {
    controls.feedback_pages_reserved = 0;
    pages.Retire(kFeedbackTexturePages);
  }
}

RadialWarpSettings RadialWarpFromWords(std::int32_t scale_word,
                                       std::int32_t growth_word) noexcept {
  return {.scale = static_cast<std::uint16_t>(scale_word),
          .growth = Core::Math::WrapSigned16(
              static_cast<std::uint32_t>(growth_word))};
}

// decomp: GIM/SCREFF2.PRG func_800F9BC0 (0.c:114).
void SetRadialWarp(ScreenOverlayControls& controls,
                   RadialWarpSettings settings) noexcept {
  controls.radial_warp = settings;
}

// decomp: GIM/SCREFF2.PRG func_800F9DE8 (5E8.c:8).
void SetVignetteEnabled(ScreenOverlayControls& controls,
                        std::int32_t enabled) noexcept {
  controls.vignette_enabled = enabled;
}

std::int32_t FeedbackOrderingSlot(std::uint16_t feedback_layer) noexcept {
  return std::int32_t{feedback_layer} - kFeedbackLayerBias;
}

// decomp: GIM/SCREFF2.PRG func_800F986C (0.c:22).
void DrawFeedbackTiles(std::int32_t frame_buffer,
                       Render::PacketQueue& scene_head,
                       Render::PacketQueue& feedback_entry) {
  // The right-hand edge strips of each 64-pixel page column.
  for (std::int32_t y = kTopRow; y >= 0; y -= kRowStep) {
    for (std::int32_t strip = kLastStrip; strip >= 0; strip -= kStripStep) {
      const std::uint32_t draw_mode =
          kDitheredDrawMode | kQuarterBlendPages | PageBits(strip);
      for (std::int32_t x = kEdgeColumn; x >= 0; x -= kEdgeStep) {
        if (x == kEdgeColumn) {
          QueueTile(
              scene_head, kBlendedSprite, Pack(strip + kInset, y + kEdgeInset),
              kEdgeColumnSize, draw_mode,
              static_cast<std::uint32_t>(y + kEdgeTextureInset) << CHAR_BIT);
        } else {
          QueueTile(scene_head, kBlendedSprite,
                    Pack(strip + x + kEdgeInset, y + kInset), kEdgeStripSize,
                    draw_mode,
                    static_cast<std::uint32_t>(x + kEdgeTextureInset) |
                        (static_cast<std::uint32_t>(y) << CHAR_BIT));
        }
      }
    }
  }

  // Each 32x32 tile: inset and outset blended copies, then an opaque one.
  for (std::int32_t y = kTopRow; y >= 0; y -= kRowStep) {
    for (std::int32_t x = kLastTileColumn; x >= 0; x -= kTileStep) {
      const std::uint32_t pages = PageBits(x & kTileColumnPageMask);
      const std::uint32_t texture =
          static_cast<std::uint32_t>(x & kTileTextureXMask) |
          (static_cast<std::uint32_t>(y) << CHAR_BIT);
      for (std::int32_t pass = 0; pass < kTilePasses; ++pass) {
        const std::uint32_t draw_mode =
            kDitheredDrawMode | pages |
            (pass == 0 ? kQuarterBlendPages : kHalfBlendPages);
        if (pass == 0) {
          QueueTile(scene_head, kBlendedSprite, Pack(x + kInset, y + kInset),
                    kInnerTileSize, draw_mode, texture);
        } else if (pass == 1) {
          QueueTile(scene_head, kBlendedSprite, Pack(x - kInset, y - kInset),
                    kFullTileSize, draw_mode, texture);
        } else {
          QueueTile(scene_head, kOpaqueSprite, Pack(x, y), kFullTileSize,
                    draw_mode, texture);
        }
      }
    }
  }

  std::array<std::uint32_t, kCapturePayload.size() + 1> capture{};
  std::ranges::copy(kCapturePayload, capture.begin() + 1);
  LinkPacket(feedback_entry, capture);

  const std::uint32_t x = frame_buffer == 0 ? kSecondBufferX : 0;
  std::array<std::uint32_t, kRestoreWords> restore{
      0, x | kDrawAreaTopLeft, (x + kDisplayRight) | kDrawAreaBottomRight,
      x | kDrawOffset, kMaskOff};
  LinkPacket(scene_head, restore);
}

// decomp: GIM/SCREFF2.PRG func_800F9DF8 (5E8.c:10).
std::uint32_t EllipsePoint(std::int32_t angular_step,
                           std::int32_t radius) noexcept {
  std::int32_t scallop =
      kScallopBias - (WrappingAdd(angular_step, kScallopBias) & kScallopMask);
  if (scallop < 0) {
    scallop = -scallop;
  }
  scallop += kScallopBase;
  const std::int32_t angle =
      WrappingMultiply(angular_step, 1 << kStepAngleShift);
  const std::int32_t cosine = Core::Math::Cosine(angle);
  const std::int32_t sine = Core::Math::Sine(angle);
  const std::int32_t x = WrappingAdd(
      WrappingMultiply(
          WrappingMultiply(WrappingMultiply(cosine, radius), kWidening) >>
              kWidenedShift,
          scallop) >>
          kScallopShift,
      kCentreX);
  const std::int32_t y = WrappingAdd(
      WrappingMultiply(WrappingMultiply(sine, radius) >> kQ12Shift, scallop) >>
          kScallopShift,
      kCentreY);
  return (static_cast<std::uint32_t>(x) & UINT16_MAX) |
         (static_cast<std::uint32_t>(y) << kHalfwordShift);
}

// decomp: GIM/SCREFF2.PRG func_800F9EBC (5E8.c:23).
void AppendVignetteCorner(std::uint32_t packed_corner_xy,
                          std::int32_t first_angular_step,
                          Render::PacketQueue& overlay) {
  // The original tests the bound after each triangle, against a wrapping end.
  const std::int32_t end = WrappingAdd(first_angular_step, kCornerTriangles);
  std::int32_t step = first_angular_step;
  do {
    const std::int32_t next = WrappingAdd(step, 1);
    std::array<std::uint32_t, kCornerTriangleWords> triangle{
        0,
        kCornerColorCommand,
        packed_corner_xy,
        kRimGrey,
        EllipsePoint(next, kOuterRadius),
        kRimGrey,
        EllipsePoint(step, kOuterRadius)};
    LinkPacket(overlay, triangle);
    step = next;
  } while (step < end);
}

// decomp: GIM/SCREFF2.PRG func_800F9FB8 (5E8.c:41).
void DrawVignette(Render::PacketQueue& overlay) {
  for (const VignetteCorner& corner : kVignetteCorners) {
    AppendVignetteCorner(corner.xy, corner.first_step, overlay);
  }
  for (std::int32_t step = 0; step < kRimQuads; ++step) {
    std::array<std::uint32_t, kRimQuadWords> quad{
        0,
        kRimDrawMode,
        kRimColorCommand,
        EllipsePoint(step + 1, kOuterRadius),
        kRimGrey,
        EllipsePoint(step, kOuterRadius),
        0,
        EllipsePoint(step + 1, kInnerRadius),
        0,
        EllipsePoint(step, kInnerRadius)};
    LinkPacket(overlay, quad);
  }
}
}  // namespace VS::Effects::ScreenDistortion
