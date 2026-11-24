#include "Extract/TextContribution.hpp"

#include <algorithm>
#include <set>
#include <span>
#include <string>
#include <utility>

#include "Content/NativeText.hpp"
#include "Platform/Paths.hpp"

namespace VS::Extract {
namespace {
using Content::JsonDocument;
constexpr std::size_t kSourceHashCharacters = 64;
struct ContributionState {
  JsonDocument catalog;
  JsonDocument owners;
  bool upgrading = false;
};

std::expected<JsonDocument, std::string> ReadJson(
    const std::filesystem::path& root, std::string_view path) {
  auto bytes = ReadFileBytes(root / Platform::Utf8Path(path));
  if (!bytes) return std::unexpected(bytes.error());
  return Content::ParseJsonText(*bytes, path);
}

std::expected<void, std::string> ValidateOwners(const JsonDocument& owners) {
  if (!owners.is_object()) {
    return std::unexpected(
        "text/strings.import.json: owners must be an object");
  }
  std::set<std::string> ids;
  for (const auto& item : owners.items()) {
    const Content::JsonMembers owner(item.value(), item.key());
    auto source = owner.String("source");
    auto hash = owner.String("source_sha256");
    auto keys = owner.Get("ids");
    if (!source || !hash || !keys || !(*keys)->is_array()) {
      return std::unexpected(item.key() + ": invalid text ownership record");
    }
    if (source->empty() || hash->size() != kSourceHashCharacters ||
        !std::ranges::all_of(*hash, [](char c) {
          return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
        })) {
      return std::unexpected(item.key() + ": invalid contributor source/hash");
    }
    for (const auto& id : **keys) {
      if (!id.is_string() || !ids.insert(id.get<std::string>()).second) {
        return std::unexpected(item.key() +
                               ": duplicate or invalid owned text ID");
      }
    }
  }
  return {};
}

std::expected<ContributionState, std::string> LoadState(
    const std::filesystem::path& root,
    const Content::ConversionManifest& previous) {
  const auto* existing = previous.Find(kTextCatalogId);
  ContributionState state{
      {{"schema", "vs.strings"},
       {"schema_version", 1},
       {"font", Content::kSaveFontId},
       {"scale", 1},
       {"strings", JsonDocument::object()}},
      JsonDocument::object(),
      existing == nullptr && previous.Find(Content::kSaveTextId) != nullptr};
  auto claims = Content::CheckNativeTextCatalogOwnership(
      previous, state.upgrading ? Content::kSaveTextId : kTextCatalogId);
  if (!claims) return std::unexpected(claims.error());
  if (existing != nullptr || state.upgrading) {
    auto old = ReadJson(root, Content::kSaveTextPath);
    if (!old) return std::unexpected(old.error());
    const auto serialized = Content::FormatJsonText(*old);
    auto valid =
        Content::ParseNativeStrings(std::as_bytes(std::span(serialized)));
    if (!valid) return std::unexpected(valid.error());
    state.catalog = std::move(*old);
  }
  if (existing != nullptr) {
    auto ownership = ReadJson(root, kTextOwnershipPath);
    if (!ownership) return std::unexpected(ownership.error());
    const Content::JsonMembers members(*ownership,
                                       std::string(kTextOwnershipPath));
    auto schema = members.String("schema");
    auto version = members.Integer("schema_version", 1, 1);
    auto owners = members.Get("owners");
    if (!schema || *schema != "vs.text-contributions" || !version || !owners) {
      return std::unexpected("text/strings.import.json: invalid ownership");
    }
    if (auto valid = ValidateOwners(**owners); !valid) {
      return std::unexpected(valid.error());
    }
    state.owners = **owners;
  }
  return state;
}

std::expected<void, std::string> Merge(ContributionState& state,
                                       const TextContribution& contribution,
                                       bool force) {
  std::set<std::string> owned;
  bool unchanged = false;
  if (state.owners.contains(contribution.owner)) {
    const auto& owner = state.owners.at(contribution.owner);
    for (const auto& id : owner["ids"]) owned.insert(id.get<std::string>());
    unchanged = owner["source"] == contribution.source.source &&
                owner["source_sha256"] == contribution.source.source_sha256;
  }
  auto& strings = state.catalog["strings"];
  if (!contribution.strings.is_object()) {
    return std::unexpected("text contribution: expected a string map");
  }
  JsonDocument ids = JsonDocument::array();
  for (const auto& item : contribution.strings.items()) {
    if (!item.key().starts_with(contribution.prefix) ||
        !item.value().is_string()) {
      return std::unexpected(item.key() + ": invalid contributor ID/value");
    }
    const bool adopted = state.upgrading && contribution.owner == "save" &&
                         item.key().starts_with("text.save.");
    if (strings.contains(item.key()) && !owned.contains(item.key()) &&
        !adopted) {
      return std::unexpected(item.key() + ": text contribution ID collision");
    }
    if (force || (!unchanged && !adopted)) strings[item.key()] = item.value();
    ids.push_back(item.key());
  }
  if (force || !unchanged) {
    for (const auto& id : owned) {
      if (!contribution.strings.contains(id)) strings.erase(id);
    }
  }
  state.owners[contribution.owner] = {
      {"source", contribution.source.source},
      {"source_sha256", contribution.source.source_sha256},
      {"ids", ids}};
  const auto serialized = Content::FormatJsonText(state.catalog);
  auto valid =
      Content::ParseNativeStrings(std::as_bytes(std::span(serialized)));
  if (!valid) return std::unexpected(valid.error());
  return {};
}

Content::ConversionOutput Registration(const JsonDocument& owners) {
  Content::ConversionOutput output{
      .id = std::string(kTextCatalogId),
      .source = {},
      .source_sha256 = {},
      .converter = std::string(kTextCatalogConverter),
      .files = {std::string(Content::kSaveTextPath),
                std::string(kTextOwnershipPath)},
      .dependencies = {},
      .note =
          "Shared native text; per-ID contributor ownership is recorded in "
          "strings.import.json. Authored values live only in strings.en.json.",
      .alias_of = {}};
  for (const auto& item : owners.items()) {
    const auto source = item.value().at("source").get<std::string>();
    const auto hash = item.value().at("source_sha256").get<std::string>();
    if (output.source.empty()) {
      output.source = source;
      output.source_sha256 = hash;
    } else {
      output.dependencies.push_back({source, hash});
    }
  }
  return output;
}

void AddChangedFile(PreparedTextContribution& prepared,
                    const std::filesystem::path& root, std::string_view path,
                    const JsonDocument& json) {
  auto old = ReadJson(root, path);
  if (old && *old == json) return;
  const auto text = Content::FormatJsonText(json) + "\n";
  const auto bytes = std::as_bytes(std::span(text));
  prepared.files.push_back({std::string(path), {bytes.begin(), bytes.end()}});
}
}  // namespace

std::expected<PreparedTextContribution, std::string> PrepareTextContribution(
    const std::filesystem::path& root,
    const Content::ConversionManifest& previous,
    const TextContribution& contribution, bool force) {
  auto state = LoadState(root, previous);
  if (!state) return std::unexpected(state.error());
  if (auto merged = Merge(*state, contribution, force); !merged) {
    return std::unexpected(merged.error());
  }
  PreparedTextContribution prepared{
      StartBatch(previous, kTextCatalogConverter, 1, force), {}};
  prepared.batch.outputs.push_back(Registration(state->owners));
  AddChangedFile(prepared, root, Content::kSaveTextPath, state->catalog);
  AddChangedFile(prepared, root, kTextOwnershipPath,
                 {{"schema", "vs.text-contributions"},
                  {"schema_version", 1},
                  {"owners", state->owners}});
  return prepared;
}
}  // namespace VS::Extract
