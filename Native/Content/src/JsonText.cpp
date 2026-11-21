// Strict, stable JSON text (JsonText.hpp).
#include "Content/JsonText.hpp"

#include <algorithm>
#include <cstddef>
#include <format>
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

JsonMembers::JsonMembers(const JsonDocument& object, std::string where)
    : object_(&object), where_(std::move(where)) {}

std::string JsonMembers::Where(std::string_view key) const {
  return std::format("{}.{}", where_, key);
}

std::expected<void, std::string> JsonMembers::Only(
    std::initializer_list<std::string_view> keys) const {
  if (!object_->is_object()) {
    return std::unexpected(std::format("{} must be an object", where_));
  }
  for (const auto& [key, value] : object_->items()) {
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

}  // namespace VS::Content
