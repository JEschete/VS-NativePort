#include "Content/NativeText.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Content/AssetCatalog.hpp"
#include "Content/ConversionManifest.hpp"
#include "Content/JsonText.hpp"

namespace VS::Content {
namespace {
constexpr std::size_t kTextLimit = 65536;
constexpr std::size_t kJsonLimit = 4ull * 1024 * 1024;
constexpr int kMaximumMetric = 4096;
constexpr std::uint32_t kAsciiMask = 0x7F;
constexpr std::uint32_t kFirstPrintable = 0x20;
constexpr std::uint32_t kContinuation = 0x80;
constexpr std::uint32_t kContinuationMask = 0xC0;
constexpr std::uint32_t kFirstMultibyteLead = 0xC2;
constexpr std::uint32_t kLastMultibyteLead = 0xF4;
constexpr std::uint32_t kPayloadMask = 0x3F;
constexpr unsigned kPayloadBits = 6;
constexpr std::uint32_t kLastScalar = 0x10FFFF;
constexpr std::uint32_t kSurrogateFirst = 0xD800;
constexpr std::uint32_t kSurrogateLast = 0xDFFF;
constexpr std::string_view kArgumentPrefix = "value:";

std::expected<JsonDocument, std::string> ParseDocument(
    std::span<const std::byte> bytes, std::string_view name) {
  if (bytes.empty() || bytes.size() > kJsonLimit) {
    return std::unexpected(std::string(name) +
                           ": JSON resource limit exceeded");
  }
  std::vector<std::set<std::string>> objects;
  bool duplicate = false;
  const auto callback = [&](int, JsonDocument::parse_event_t event,
                            JsonDocument& value) {
    if (event == JsonDocument::parse_event_t::object_start) {
      objects.emplace_back();
    }
    if (event == JsonDocument::parse_event_t::key && !objects.empty()) {
      duplicate =
          !objects.back().insert(value.get<std::string>()).second || duplicate;
    }
    if (event == JsonDocument::parse_event_t::object_end) objects.pop_back();
    return true;
  };
  const auto* first = reinterpret_cast<const char*>(bytes.data());
  auto json = JsonDocument::parse(first, first + bytes.size(), callback, false);
  if (json.is_discarded() || duplicate) {
    return std::unexpected(std::string(name) +
                           ": invalid JSON or duplicate member");
  }
  return json;
}

bool Name(std::string_view name) {
  return !name.empty() && name.front() >= 'a' && name.front() <= 'z' &&
         name.back() != '.' && name.find("..") == std::string_view::npos &&
         std::ranges::all_of(name, [](char c) {
           return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                  c == '_' || c == '-' || c == '.';
         });
}

std::expected<std::size_t, std::string> Scalar(std::string_view text,
                                               std::size_t at,
                                               std::string_view id) {
  const auto first = static_cast<unsigned char>(text[at]);
  const std::size_t count = first < 0x80   ? 1
                            : first < 0xE0 ? 2
                            : first < 0xF0 ? 3
                                           : 4;
  std::uint32_t value =
      first & (count == 1 ? kAsciiMask : (kAsciiMask >> count));
  bool valid = first >= kFirstPrintable && first != kAsciiMask &&
               (first < kContinuation ||
                (first >= kFirstMultibyteLead && first <= kLastMultibyteLead));
  valid = valid && count <= text.size() - at;
  for (std::size_t i = 1; valid && i < count; ++i) {
    const auto byte = static_cast<unsigned char>(text[at + i]);
    valid = (byte & kContinuationMask) == kContinuation;
    value = (value << kPayloadBits) | (byte & kPayloadMask);
  }
  constexpr std::array<std::uint32_t, 4> kMinimum{0, 0x80, 0x800, 0x10000};
  if (!valid || value < kMinimum[count - 1] || value > kLastScalar ||
      (value >= kSurrogateFirst && value <= kSurrogateLast)) {
    return std::unexpected(
        std::format("{}: text byte {}: invalid Unicode scalar", id, at));
  }
  return count;
}

bool Symbol(std::string_view token) {
  constexpr std::array kNames{"btn:triangle", "btn:square", "btn:circle",
                              "btn:cross",    "arrow:left", "arrow:right",
                              "arrow:up",     "arrow:down", "music",
                              "level",        "star",       "block"};
  return std::ranges::find(kNames, token) != kNames.end();
}

std::expected<void, std::string> AppendToken(NativeText& result,
                                             std::string_view token,
                                             std::string_view id,
                                             std::size_t at) {
  if (Symbol(token)) {
    result.push_back({TextTokenKind::kGlyph, std::format("{{{}}}", token)});
    return {};
  }
  if (token.starts_with(kArgumentPrefix) &&
      Name(token.substr(kArgumentPrefix.size()))) {
    result.push_back({TextTokenKind::kArgument,
                      std::string(token.substr(kArgumentPrefix.size()))});
    return {};
  }
  return std::unexpected(
      std::format("{}: text byte {}: unsupported instant-text control {{{}}}",
                  id, at, token));
}

std::expected<void, std::string> Schema(const JsonMembers& members,
                                        std::string_view expected) {
  auto schema = members.String("schema");
  auto version = members.Integer("schema_version", 1, 1);
  if (!schema) return std::unexpected(schema.error());
  if (!version) return std::unexpected(version.error());
  if (*schema != expected) {
    return std::unexpected("expected " + std::string(expected));
  }
  return {};
}

std::expected<GlyphMetric, std::string> Metric(const JsonDocument& json,
                                               const NativeImage& atlas,
                                               std::string_view identity) {
  const JsonMembers row(json, "font glyph " + std::string(identity));
  if (auto only = row.Only({"x", "y", "width", "height", "advance", "visible"});
      !only) {
    return std::unexpected(only.error());
  }
  auto x = row.Integer("x", 0, atlas.width);
  auto y = row.Integer("y", 0, atlas.height);
  auto width = row.Integer("width", 1, atlas.width);
  auto height = row.Integer("height", 1, atlas.height);
  auto advance = row.Integer("advance", 1, kMaximumMetric);
  auto visible = row.Boolean("visible");
  if (!x || !y || !width || !height || !advance || !visible ||
      *x + *width > atlas.width || *y + *height > atlas.height) {
    return std::unexpected("font glyph " + std::string(identity) +
                           ": invalid metrics or atlas rectangle");
  }
  return GlyphMetric{static_cast<int>(*x),       static_cast<int>(*y),
                     static_cast<int>(*width),   static_cast<int>(*height),
                     static_cast<int>(*advance), *visible};
}
}  // namespace

std::expected<NativeText, std::string> ParseNativeText(std::string_view markup,
                                                       std::string_view id) {
  if (markup.size() > kTextLimit) {
    return std::unexpected(std::string(id) + ": text resource limit exceeded");
  }
  NativeText result;
  for (std::size_t at = 0; at < markup.size();) {
    const char c = markup[at];
    if (c == '\n') {
      result.push_back({TextTokenKind::kNewline, {}});
      ++at;
    } else if ((c == '{' || c == '}') && at + 1 < markup.size() &&
               markup[at + 1] == c) {
      result.push_back({TextTokenKind::kGlyph, std::string(1, c)});
      at += 2;
    } else if (c == '{') {
      const auto end = markup.find('}', at + 1);
      if (end == std::string_view::npos) {
        return std::unexpected(
            std::format("{}: text byte {}: unclosed control", id, at));
      }
      auto appended =
          AppendToken(result, markup.substr(at + 1, end - at - 1), id, at);
      if (!appended) return std::unexpected(appended.error());
      at = end + 1;
    } else if (c == '}') {
      return std::unexpected(
          std::format("{}: text byte {}: unmatched brace", id, at));
    } else {
      auto count = Scalar(markup, at, id);
      if (!count) return std::unexpected(count.error());
      result.push_back(
          {TextTokenKind::kGlyph, std::string(markup.substr(at, *count))});
      at += *count;
    }
  }
  return result;
}

std::expected<NativeText, std::string> BindNativeText(
    std::span<const TextToken> text, const TextArguments& arguments,
    std::string_view id) {
  if (text.size() > kTextLimit) {
    return std::unexpected(std::string(id) + ": text resource limit exceeded");
  }
  NativeText result;
  for (const auto& token : text) {
    if (result.size() >= kTextLimit) {
      return std::unexpected(std::string(id) +
                             ": bound text resource limit exceeded");
    }
    if (token.kind != TextTokenKind::kArgument) {
      result.push_back(token);
      continue;
    }
    const auto found = arguments.find(token.identity);
    if (found == arguments.end()) {
      return std::unexpected(std::string(id) + ": missing argument " +
                             token.identity);
    }
    const std::string value =
        std::holds_alternative<std::string>(found->second)
            ? std::get<std::string>(found->second)
            : std::to_string(std::get<std::int64_t>(found->second));
    if (value.size() > kTextLimit) {
      return std::unexpected(std::string(id) +
                             ": argument resource limit exceeded");
    }
    // Arguments are plain, single-line Unicode; braces cannot inject
    // controls and external line endings cannot change confirmation layout.
    for (std::size_t at = 0; at < value.size();) {
      if (result.size() >= kTextLimit) {
        return std::unexpected(std::string(id) +
                               ": bound text resource limit exceeded");
      }
      if (value[at] == '\n' || value[at] == '\r' || value[at] == '\t') {
        result.push_back({TextTokenKind::kGlyph, " "});
        ++at;
        continue;
      }
      auto count = Scalar(value, at, id);
      if (!count) return std::unexpected(count.error());
      result.push_back({TextTokenKind::kGlyph, value.substr(at, *count)});
      at += *count;
      if (result.size() > kTextLimit) {
        return std::unexpected(std::string(id) +
                               ": bound text resource limit exceeded");
      }
    }
  }
  return result;
}

std::expected<NativeTextCatalog, std::string> ParseNativeStrings(
    std::span<const std::byte> json) {
  auto parsed = ParseDocument(json, kSaveTextPath);
  if (!parsed) return std::unexpected(parsed.error());
  const JsonMembers members(*parsed, std::string(kSaveTextPath));
  if (auto only = members.Only(
          {"schema", "schema_version", "font", "scale", "strings"});
      !only) {
    return std::unexpected(only.error());
  }
  if (auto schema = Schema(members, "vs.strings"); !schema) {
    return std::unexpected(schema.error());
  }
  auto font = members.String("font");
  auto scale = members.Get("scale");
  auto strings = members.Get("strings");
  if (!font || !font->starts_with("font.") || !IsContentId(*font) || !scale ||
      !(**scale).is_number() || !strings || !(**strings).is_object()) {
    return std::unexpected("strings: invalid font, scale or string object");
  }
  NativeTextCatalog catalog;
  catalog.font = *font;
  catalog.scale = (**scale).get<double>();
  if (!std::isfinite(catalog.scale) || catalog.scale < kMinimumTextScale ||
      catalog.scale > kMaximumTextScale) {
    return std::unexpected("strings: scale must be in 0.5..4");
  }
  for (const auto& [id, value] : (**strings).items()) {
    if (!id.starts_with("text.") || !Name(id) || !value.is_string()) {
      return std::unexpected(id + ": expected semantic text ID and string");
    }
    auto text = ParseNativeText(value.get_ref<const std::string&>(), id);
    if (!text) return std::unexpected(text.error());
    catalog.strings.emplace(id, std::move(*text));
  }
  return catalog;
}

std::expected<std::span<const TextToken>, std::string> NativeTextCatalog::Find(
    std::string_view id) const {
  const auto found = strings.find(id);
  if (found == strings.end()) {
    return std::unexpected(std::string(id) + ": missing string ID");
  }
  return std::span<const TextToken>(found->second);
}

std::expected<NativeFont, std::string> ParseNativeFont(
    std::span<const std::byte> json, const NativeImage& atlas) {
  auto parsed = ParseDocument(json, kSaveFontPath);
  if (!parsed) return std::unexpected(parsed.error());
  const JsonMembers members(*parsed, std::string(kSaveFontPath));
  if (auto only = members.Only(
          {"schema", "schema_version", "id", "image", "line_height", "glyphs"});
      !only) {
    return std::unexpected(only.error());
  }
  if (auto schema = Schema(members, "vs.font"); !schema) {
    return std::unexpected(schema.error());
  }
  auto id = members.String("id");
  auto image = members.String("image");
  auto line = members.Integer("line_height", 1, kMaximumMetric);
  auto glyphs = members.Get("glyphs");
  if (!id || !id->starts_with("font.") || !IsContentId(*id) || !image ||
      !image->starts_with("image.") || !IsContentId(*image) || !line ||
      !glyphs || !(**glyphs).is_object()) {
    return std::unexpected(
        "font: invalid identity, image reference or metrics");
  }
  NativeFont font{*id, *image, static_cast<int>(*line), {}};
  for (const auto& [identity, value] : (**glyphs).items()) {
    const auto escaped = identity == "{"   ? "{{"
                         : identity == "}" ? "}}"
                                           : identity;
    auto token = ParseNativeText(escaped, identity);
    if (!token || token->size() != 1 ||
        token->front().kind != TextTokenKind::kGlyph) {
      return std::unexpected("font: invalid glyph identity " + identity);
    }
    auto metric = Metric(value, atlas, identity);
    if (!metric) return std::unexpected(metric.error());
    font.glyphs.emplace(identity, *metric);
  }
  return font;
}
}  // namespace VS::Content
