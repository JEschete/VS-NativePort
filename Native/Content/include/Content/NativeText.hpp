#pragma once

#include <cstdint>
#include <expected>
#include <functional>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "Content/ImageFiles.hpp"

namespace VS::Content {
inline constexpr std::string_view kSaveTextId = "text.ui.save";
inline constexpr std::string_view kSaveFontId = "font.ui.save";
inline constexpr std::string_view kSaveFontImageId = "image.ui.save-font";
inline constexpr std::string_view kSaveTextPath = "text/strings.en.json";
inline constexpr std::string_view kSaveFontPath = "text/save.font.json";
inline constexpr std::string_view kSaveTextConverter = "save-text-json";
inline constexpr double kMinimumTextScale = 0.5;
inline constexpr double kMaximumTextScale = 4;

enum class TextTokenKind { kGlyph, kNewline, kArgument };
struct TextToken {
  TextTokenKind kind = TextTokenKind::kGlyph;
  // A Unicode scalar or a named symbol, not an encoded byte or atlas index.
  std::string identity;
  bool operator==(const TextToken&) const = default;
};
using NativeText = std::vector<TextToken>;
using TextArgument = std::variant<std::string, std::int64_t>;
using TextArguments = std::map<std::string, TextArgument, std::less<>>;

// This instant-information slice accepts Unicode, escaped braces, newline,
// named buttons/arrows and {value:role}. Other controls fail explicitly.
[[nodiscard]] std::expected<NativeText, std::string> ParseNativeText(
    std::string_view markup, std::string_view id);
[[nodiscard]] std::expected<NativeText, std::string> BindNativeText(
    std::span<const TextToken> text, const TextArguments& arguments,
    std::string_view id);

struct GlyphMetric {
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;
  int advance = 0;
  bool visible = true;
};
// NOLINTNEXTLINE(bugprone-exception-escape): std::map's noexcept move.
struct NativeFont {
  std::string id;
  std::string image;
  int line_height = 0;
  std::map<std::string, GlyphMetric, std::less<>> glyphs;
};
[[nodiscard]] std::expected<NativeFont, std::string> ParseNativeFont(
    std::span<const std::byte> json, const NativeImage& atlas);

// NOLINTNEXTLINE(bugprone-exception-escape): std::map's noexcept move.
struct NativeTextCatalog {
  std::string font;
  double scale = 1;
  std::map<std::string, NativeText, std::less<>> strings;
  [[nodiscard]] std::expected<std::span<const TextToken>, std::string> Find(
      std::string_view id) const;
};
[[nodiscard]] std::expected<NativeTextCatalog, std::string> ParseNativeStrings(
    std::span<const std::byte> json);

class DataStore;
// NOLINTNEXTLINE(bugprone-exception-escape): std::map's noexcept move.
struct NativeTextAssets {
  NativeTextCatalog catalog;
  NativeFont font;
  ImageDocument atlas;
};
// Loaded transactionally with DataStore; overlays reuse catalog-layers.json.
[[nodiscard]] std::expected<NativeTextAssets, std::string> LoadSaveTextAssets(
    const DataStore& store);
}  // namespace VS::Content
