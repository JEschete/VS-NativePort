// Strict, stable JSON text (JsonText.hpp).
#include "Content/JsonText.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <optional>
#include <string>
#include <utility>

namespace VS::Content {
namespace {

constexpr std::size_t kIndent = 2;
constexpr std::size_t kSha256HexDigits = 64;

void AppendJson(std::string& text, const JsonDocument& value,
                std::size_t depth) {
  const bool expand =
      value.is_object()
          ? !value.empty()
          : value.is_array() &&
                !std::ranges::all_of(value, [](const JsonDocument& element) {
                  return !element.is_structured();
                });
  if (!expand) {
    if (!value.is_array()) {
      text += value.dump();
      return;
    }
    text += '[';
    for (std::size_t index = 0; index < value.size(); ++index) {
      if (index != 0) text += ", ";
      text += value[index].dump();
    }
    text += ']';
    return;
  }
  text += value.is_object() ? "{\n" : "[\n";
  bool first = true;
  for (const auto& [key, member] : value.items()) {
    if (!first) text += ",\n";
    first = false;
    text.append((depth + 1) * kIndent, ' ');
    if (value.is_object()) text += JsonDocument(key).dump() + ": ";
    AppendJson(text, member, depth + 1);
  }
  text += '\n';
  text.append(depth * kIndent, ' ');
  text += value.is_object() ? '}' : ']';
}

}  // namespace

std::string FormatJsonText(const JsonDocument& value) {
  std::string text;
  AppendJson(text, value, 0);
  return text;
}

std::expected<JsonDocument, std::string> ParseJsonText(
    std::span<const std::byte> text, std::string_view name) {
  const auto* first = reinterpret_cast<const char*>(text.data());
  JsonDocument json =
      JsonDocument::parse(first, first + text.size(), nullptr, false);
  if (json.is_discarded()) {
    return std::unexpected(std::format("{} is not valid JSON", name));
  }
  return json;
}

bool IsLowercaseSha256(std::string_view text) noexcept {
  return text.size() == kSha256HexDigits &&
         std::ranges::all_of(text, [](char value) {
           return (value >= '0' && value <= '9') ||
                  (value >= 'a' && value <= 'f');
         });
}

std::optional<std::int64_t> JsonWholeNumber(
    const JsonDocument& value) noexcept {
  if (const auto* whole =
          value.get_ptr<const JsonDocument::number_unsigned_t*>();
      whole != nullptr) {
    if (*whole >
        static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
      return std::nullopt;
    }
    return static_cast<std::int64_t>(*whole);
  }
  if (const auto* whole =
          value.get_ptr<const JsonDocument::number_integer_t*>();
      whole != nullptr) {
    return *whole;
  }
  return std::nullopt;
}

JsonMembers::JsonMembers(const JsonDocument& object, std::string where)
    : object_(&object), where_(std::move(where)) {}

std::string JsonMembers::Where(std::string_view key) const {
  return std::format("{}.{}", where_, key);
}

std::expected<void, std::string> JsonMembers::Only(
    std::initializer_list<std::string_view> keys) const {
  return Only(std::span(keys.begin(), keys.size()));
}

std::expected<void, std::string> JsonMembers::Only(
    std::span<const std::string_view> keys) const {
  if (!object_->is_object()) {
    return std::unexpected(std::format("{} must be an object", where_));
  }
  for (const auto& [key, value] : object_->items()) {
    if (key == kDisplayName) {
      if (!value.is_string()) {
        return std::unexpected(std::format("{} must be a string", Where(key)));
      }
      continue;
    }
    if (std::ranges::find(keys, std::string_view(key)) == keys.end()) {
      return std::unexpected(
          std::format("{} has an unknown member '{}'", where_, key));
    }
  }
  return {};
}

const JsonDocument* JsonMembers::Find(std::string_view key) const {
  if (!object_->is_object()) return nullptr;
  const auto found = object_->find(std::string(key));
  return found == object_->end() ? nullptr : &*found;
}

std::expected<const JsonDocument*, std::string> JsonMembers::Get(
    std::string_view key) const {
  const JsonDocument* value = Find(key);
  if (value == nullptr) {
    return std::unexpected(std::format("{} is missing", Where(key)));
  }
  return value;
}

std::expected<std::uint64_t, std::string> JsonMembers::Unsigned(
    std::string_view key, std::uint64_t max) const {
  auto value = Get(key);
  if (!value) return std::unexpected(value.error());
  if (!(*value)->is_number_unsigned() || (*value)->get<std::uint64_t>() > max) {
    return std::unexpected(
        std::format("{} must be a whole number from 0 to {}", Where(key), max));
  }
  return (*value)->get<std::uint64_t>();
}

std::expected<std::int64_t, std::string> JsonMembers::Integer(
    std::string_view key, std::int64_t min, std::int64_t max) const {
  auto value = Get(key);
  if (!value) return std::unexpected(value.error());
  const std::optional<std::int64_t> number = JsonWholeNumber(**value);
  if (!number || *number < min || *number > max) {
    return std::unexpected(std::format(
        "{} must be a whole number from {} to {}", Where(key), min, max));
  }
  return *number;
}

std::expected<std::string, std::string> JsonMembers::String(
    std::string_view key) const {
  auto value = Get(key);
  if (!value) return std::unexpected(value.error());
  if (!(*value)->is_string()) {
    return std::unexpected(std::format("{} must be a string", Where(key)));
  }
  return (*value)->get<std::string>();
}

std::expected<bool, std::string> JsonMembers::Boolean(
    std::string_view key) const {
  auto value = Get(key);
  if (!value) return std::unexpected(value.error());
  if (!(*value)->is_boolean()) {
    return std::unexpected(std::format("{} must be true or false", Where(key)));
  }
  return (*value)->get<bool>();
}

std::expected<void, std::string> CheckJsonSchema(const JsonMembers& members,
                                                 std::string_view where,
                                                 std::string_view schema,
                                                 std::uint32_t version) {
  auto named = members.String("schema");
  if (!named) return std::unexpected(named.error());
  auto number = members.Integer("schema_version", 0,
                                std::numeric_limits<std::uint32_t>::max());
  if (!number) return std::unexpected(number.error());
  if (*named != schema || *number != version) {
    return std::unexpected(
        std::format("{} is {} version {}; this build reads {} version {}",
                    where, *named, *number, schema, version));
  }
  return {};
}

std::expected<JsonSource, std::string> JsonSourceOf(const JsonMembers& members,
                                                    std::string_view where,
                                                    std::string_view schema,
                                                    std::uint32_t version) {
  if (auto checked = CheckJsonSchema(members, where, schema, version);
      !checked) {
    return std::unexpected(checked.error());
  }
  JsonSource found;
  auto source = members.String("source");
  if (!source) return std::unexpected(source.error());
  found.source = std::move(*source);
  auto hash = members.String("source_sha256");
  if (!hash) return std::unexpected(hash.error());
  if (!IsLowercaseSha256(*hash)) {
    return std::unexpected(std::format("{} must be a lowercase SHA-256 digest",
                                       members.Where("source_sha256")));
  }
  found.source_sha256 = std::move(*hash);
  return found;
}

}  // namespace VS::Content
