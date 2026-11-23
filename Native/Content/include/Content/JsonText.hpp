#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <initializer_list>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "nlohmann/json.hpp"

// Strict, stable JSON for native content files (ASSET_PIPELINE.md §6.4, §8).
// Writers share one text layout, so the same document gives the same bytes and
// diffs by field; readers share one member check, so a misspelt or unknown
// member is an error instead of being ignored.
namespace VS::Content {

using JsonDocument = nlohmann::ordered_json;

// Optional author-facing metadata, never a runtime identifier or value.
inline constexpr std::string_view kDisplayName = "display_name";

// Objects and arrays that hold objects or arrays get one member per line with
// a two-space indent; arrays of scalars stay on one line. No trailing newline.
[[nodiscard]] std::string FormatJsonText(const JsonDocument& value);

// The JSON document in `text`; the error names the file.
[[nodiscard]] std::expected<JsonDocument, std::string> ParseJsonText(
    std::span<const std::byte> text, std::string_view name);

// Whether `text` is a SHA-256 digest in lowercase hexadecimal.
[[nodiscard]] bool IsLowercaseSha256(std::string_view text) noexcept;

// `value` when it is a whole number an int64 holds; nothing for fractions,
// other types and larger numbers.
[[nodiscard]] std::optional<std::int64_t> JsonWholeNumber(
    const JsonDocument& value) noexcept;

// One JSON object's members; errors name each member by its path.
class JsonMembers {
 public:
  JsonMembers(const JsonDocument& object, std::string where);

  [[nodiscard]] std::string Where(std::string_view key) const;
  // Fails on unknown members, except optional string display_name metadata.
  [[nodiscard]] std::expected<void, std::string> Only(
      std::initializer_list<std::string_view> keys) const;
  [[nodiscard]] const JsonDocument* Find(std::string_view key) const;
  [[nodiscard]] std::expected<const JsonDocument*, std::string> Get(
      std::string_view key) const;
  [[nodiscard]] std::expected<std::uint64_t, std::string> Unsigned(
      std::string_view key, std::uint64_t max) const;
  // A whole number from `min` to `max`.
  [[nodiscard]] std::expected<std::int64_t, std::string> Integer(
      std::string_view key, std::int64_t min, std::int64_t max) const;
  [[nodiscard]] std::expected<std::string, std::string> String(
      std::string_view key) const;
  [[nodiscard]] std::expected<bool, std::string> Boolean(
      std::string_view key) const;

 private:
  const JsonDocument* object_;
  std::string where_;
};

// Where a converted document came from.
struct JsonSource {
  std::string source;  // the disc path
  std::string source_sha256;
};

// The source of the document `members` holds, once its schema and
// schema_version are `schema` and `version`; the error names `where` or the
// member at fault.
[[nodiscard]] std::expected<JsonSource, std::string> JsonSourceOf(
    const JsonMembers& members, std::string_view where, std::string_view schema,
    std::uint32_t version);

}  // namespace VS::Content
