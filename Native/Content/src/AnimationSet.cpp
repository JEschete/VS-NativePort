// The vs.animation-set JSON form of an ESQ animation set (AnimationSet.hpp).
#include "Content/AnimationSet.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "Content/ActorAnimation.hpp"
#include "Content/EsqFile.hpp"
#include "Content/JsonText.hpp"
#include "Content/SeqFile.hpp"

namespace VS::Content {
namespace {
using Json = JsonDocument;
template <typename T>
using Result = std::expected<T, std::string>;

Json EntryJson(const EsqEntry& entry) {
  Json json = Json::object();
  json["selector"] = entry.selector;
  json["texture_row"] =
      entry.texture_row ? Json(*entry.texture_row) : Json(nullptr);
  json["bones"] = entry.animation.bones;
  json["bank"] = SeqBankJson(entry.animation.bank);
  return json;
}

Result<std::optional<std::uint8_t>> TextureRowOf(const JsonMembers& members) {
  auto value = members.Get("texture_row");
  if (!value) return std::unexpected(value.error());
  if ((*value)->is_null()) return std::nullopt;
  auto row = members.Integer("texture_row", 0, kEsqMaxTextureRow);
  if (!row) {
    return std::unexpected(
        std::format("{} must be null or a whole number "
                    "from 0 to {}",
                    members.Where("texture_row"), kEsqMaxTextureRow));
  }
  return static_cast<std::uint8_t>(*row);
}

Result<EsqEntry> EntryOf(const Json& json, const std::string& where) {
  const JsonMembers members(json, where);
  if (auto only = members.Only({"selector", "texture_row", "bones", "bank"});
      !only) {
    return std::unexpected(only.error());
  }
  EsqEntry entry;
  auto selector = members.Integer("selector", 0, kEsqMaxSelector);
  if (!selector) return std::unexpected(selector.error());
  entry.selector = static_cast<std::uint16_t>(*selector);
  auto row = TextureRowOf(members);
  if (!row) return std::unexpected(row.error());
  entry.texture_row = *row;
  auto bones =
      members.Integer("bones", 1, static_cast<std::int64_t>(kSeqMaxBones));
  if (!bones) return std::unexpected(bones.error());
  entry.animation.bones = static_cast<std::uint16_t>(*bones);
  auto bank = members.Get("bank").and_then([&](const Json* value) {
    return SeqBankOf(*value, members.Where("bank"));
  });
  if (!bank) return std::unexpected(bank.error());
  entry.animation.bank = std::move(*bank);
  return entry;
}

Result<EsqFile> EntriesOf(const JsonMembers& members) {
  auto list = members.Get("entries");
  if (!list) return std::unexpected(list.error());
  const std::string where = members.Where("entries");
  if (!(*list)->is_array() || (*list)->size() > kEsqMaxEntries) {
    return std::unexpected(std::format(
        "{} must be an array of at most {} entries", where, kEsqMaxEntries));
  }
  EsqFile file;
  for (std::size_t index = 0; index < (*list)->size(); ++index) {
    const std::string at = std::format("{}[{}]", where, index);
    auto entry = EntryOf((**list)[index], at);
    if (!entry) return std::unexpected(entry.error());
    for (std::size_t earlier = 0; earlier < file.entries.size(); ++earlier) {
      if (file.entries[earlier].selector == entry->selector) {
        return std::unexpected(std::format(
            "{}.selector {} repeats {}[{}]'s, so the game would never use it",
            at, entry->selector, where, earlier));
      }
    }
    file.entries.push_back(std::move(*entry));
  }
  return file;
}

}  // namespace

JsonDocument AnimationSetJson(const AnimationSetDocument& document) {
  Json entries = Json::array();
  for (const EsqEntry& entry : document.file.entries) {
    entries.push_back(EntryJson(entry));
  }
  Json json = Json::object();
  json["schema"] = kAnimationSetSchema;
  json["schema_version"] = kAnimationSetSchemaVersion;
  json["source"] = document.source;
  json["source_sha256"] = document.source_sha256;
  json["entries"] = std::move(entries);
  return json;
}

std::expected<AnimationSetDocument, std::string> AnimationSetOf(
    const JsonDocument& json, std::string_view where) {
  const JsonMembers members(json, std::string(where));
  if (auto only = members.Only(
          {"schema", "schema_version", "source", "source_sha256", "entries"});
      !only) {
    return std::unexpected(only.error());
  }
  auto source = JsonSourceOf(members, where, kAnimationSetSchema,
                             kAnimationSetSchemaVersion);
  if (!source) return std::unexpected(source.error());
  AnimationSetDocument document;
  document.source = std::move(source->source);
  document.source_sha256 = std::move(source->source_sha256);
  auto file = EntriesOf(members);
  if (!file) return std::unexpected(file.error());
  document.file = std::move(*file);
  return document;
}

}  // namespace VS::Content
