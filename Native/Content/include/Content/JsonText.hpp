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
  // Fails when the value is not an object or has a member not in `keys`.
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

}  // namespace VS::Content
