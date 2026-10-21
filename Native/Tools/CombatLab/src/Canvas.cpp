#include "CombatLab/Canvas.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <stdexcept>
#include <utility>
#include <vector>

#include "Content/DataStore.hpp"
#include "Legacy/Psx.h"

extern "C" {
#include "Render/Text/CompactFont.h"
#include "SystemResources.h"
}

namespace VS::combat_lab {
namespace {
constexpr std::uint32_t kBackground = 0xFF211D19;
constexpr std::uint32_t kBorder = 0xFF897C61;
constexpr int kBorderInset = 8;
constexpr int kBorderThickness = 2;
constexpr int kPixelsPerWord = 4;
constexpr int kGlyphHeight = 10;
constexpr int kGlyphAtlasTop = 76;
constexpr int kPageWidthWords = 1024;
constexpr int kPageHeight = 512;
constexpr int kFontMinimumHeight = 96;
constexpr int kDefaultAdvance = 4;
constexpr int kNibbleBits = 4;
constexpr int kNibbleMask = 15;
constexpr int kGlyphColumnMask = 0xFE;
constexpr int kFontPageX = 768;
constexpr int kInterfacePageX = 832;
constexpr int kInterfaceWidthWords = 64;
constexpr int kFontPaletteX = 864;
constexpr int kFontPaletteY = 223;
constexpr int kColorChannels = 3;
constexpr int kColorBits = 5;
constexpr int kColorMask = 31;
constexpr int kByteBits = 8;
constexpr int kByteMask = 255;
constexpr std::uint32_t kAlphaMask = 0xFF000000;

template <typename T>
T Read(std::span<const std::uint8_t> bytes, std::size_t offset) {
  if (offset > bytes.size() || sizeof(T) > bytes.size() - offset) {
    throw std::runtime_error("truncated SYSTEM.DAT font header");
  }
  T value{};
  std::memcpy(&value, bytes.data() + offset, sizeof(T));
  return value;
}

std::vector<std::uint8_t> ReadSystemFont() {
  const auto* store = Content::DataStore::Get();
  const auto* entry = store ? store->Find("BATTLE/SYSTEM.DAT") : nullptr;
  if (entry == nullptr) throw std::runtime_error("SYSTEM.DAT is missing");
  auto bytes = store->ReadAll(*entry);
  if (!bytes) throw std::runtime_error(bytes.error());
  return std::move(*bytes);
}

std::uint32_t TintFontColor(std::uint16_t shade, std::uint32_t color) {
  std::uint32_t result = color & kAlphaMask;
  for (int channel = 0; channel < kColorChannels; ++channel) {
    const auto intensity = (shade >> (channel * kColorBits)) & kColorMask;
    const auto tint = (color >> (channel * kByteBits)) & kByteMask;
    result |= ((intensity * tint) / kColorMask) << (channel * kByteBits);
  }
  return result;
}
}  // namespace

Canvas::Canvas() : Canvas(ReadSystemFont()) {}

Canvas::Canvas(std::span<const std::uint8_t> system_data)
    : pixels_(std::size_t{kWidth} * kHeight) {
  const auto header = Read<Content_SystemDatHeader>(system_data, 0);
  const auto rect = Read<RECT>(system_data, header.image0Offset);
  font_width_ = rect.w;
  font_height_ = rect.h;
  if (rect.x != kFontPageX || rect.y != 0 ||
      font_width_ < kInterfaceWidthWords || font_width_ > kPageWidthWords ||
      font_height_ < kFontMinimumHeight || font_height_ > kPageHeight) {
    throw std::runtime_error("invalid SYSTEM.DAT font dimensions");
  }
  const auto offset = std::size_t{header.image0Offset} + sizeof(RECT);
  const auto length = static_cast<std::size_t>(font_width_) * font_height_ *
                      sizeof(std::uint16_t);
  if (offset > system_data.size() || length > system_data.size() - offset) {
    throw std::runtime_error("SYSTEM.DAT font image is outside its file");
  }
  font_.resize(length / sizeof(std::uint16_t));
  std::memcpy(font_.data(), system_data.data() + offset, length);
  // BATTLE/5BF94.c:3484 uploads the first interface block to (832,0).
  // Compact text uses its CLUT at (864,223), not SYSTEM.DAT paletteOffset.
  const auto palette_offset =
      std::size_t{header.interfaceDataOffset} +
      (kFontPaletteY * kInterfaceWidthWords + kFontPaletteX - kInterfacePageX) *
          sizeof(std::uint16_t);
  font_palette_ = Read<decltype(font_palette_)>(system_data, palette_offset);
}

void Canvas::Clear() {
  std::ranges::fill(pixels_, kBackground);
  Rectangle(kBorderInset, kBorderInset, kWidth - 2 * kBorderInset,
            kBorderThickness, kBorder);
  Rectangle(kBorderInset, kHeight - kBorderInset - kBorderThickness,
            kWidth - 2 * kBorderInset, kBorderThickness, kBorder);
}

void Canvas::Rectangle(int x, int y, int width, int height,
                       std::uint32_t color) {
  const int left = std::clamp(x, 0, kWidth);
  const int top = std::clamp(y, 0, kHeight);
  const int right = std::clamp(x + width, 0, kWidth);
  const int bottom = std::clamp(y + height, 0, kHeight);
  for (int row = top; row < bottom; ++row) {
    const auto offset = static_cast<std::ptrdiff_t>(row) * kWidth;
    std::fill(pixels_.begin() + offset + left, pixels_.begin() + offset + right,
              color);
  }
}

void Canvas::Text(int x, int y, std::string_view text, std::uint32_t color) {
  // The atlas and tables are the production compact font, not a substitute.
  // Provenance: BATTLE/5BF94.c:1345, vs_battle_renderTextRawColor.
  for (const unsigned char character : text) {
    const int upper = std::toupper(character);
    const int index = upper >= ' ' && upper <= 'Z' ? upper - ' ' : '?' - ' ';
    const int width =
        (Render_compactGlyphWidths[index / 2] >> ((index % 2) * kNibbleBits)) &
        kNibbleMask;
    const int glyph = Render_compactGlyphIndices[index];
    const int u = glyph & kGlyphColumnMask;
    const int v = (glyph & 1) * kGlyphHeight + kGlyphAtlasTop;
    for (int row = 0; row < kGlyphHeight; ++row) {
      for (int column = 0; column < width; ++column) {
        const int source_x = (u + column) / kPixelsPerWord;
        if (source_x >= font_width_ || v + row >= font_height_) continue;
        const auto word = font_[(v + row) * font_width_ + source_x];
        const auto pixel =
            (word >> (((u + column) % kPixelsPerWord) * kNibbleBits)) &
            kNibbleMask;
        const auto shade = font_palette_[pixel];
        if (shade != 0) {
          Rectangle(x + column, y + row, 1, 1, TintFontColor(shade, color));
        }
      }
    }
    x += width == 0 ? kDefaultAdvance : width;
    if (x >= kWidth) break;
  }
}

}  // namespace VS::combat_lab
