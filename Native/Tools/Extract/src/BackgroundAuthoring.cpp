#include "Extract/BackgroundAuthoring.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "Content/AssetCatalog.hpp"
#include "Content/BackgroundAssets.hpp"
#include "Content/BackgroundLayout.hpp"
#include "Content/ConvertedImages.hpp"
#include "Content/JsonText.hpp"
#include "Extract/ConversionManifestWriter.hpp"
#include "Extract/ImageConversion.hpp"
#include "Platform/Paths.hpp"

namespace VS::Extract {
namespace {
namespace fs = std::filesystem;
using Bytes = std::vector<std::byte>;
using Json = Content::JsonDocument;
using Output = Content::ConversionOutput;
using Identity = Content::BackgroundAssetIdentity;

struct Migration {
  fs::path root;
  Content::ConversionManifest before;
  Content::ConversionManifest after;
  std::map<std::string, Bytes> files;
  std::map<std::string, std::string> renamed_paths;
  BackgroundAuthoringReport report;
};

Bytes TextBytes(const std::string& text) {
  const auto bytes = std::as_bytes(std::span(text));
  return {bytes.begin(), bytes.end()};
}

std::expected<Json, std::string> ReadJson(const fs::path& root,
                                          std::string_view path) {
  auto bytes = ReadFileBytes(root / Platform::Utf8Path(path));
  if (!bytes) return std::unexpected(bytes.error());
  return Content::ParseJsonText(*bytes, path);
}

std::expected<void, std::string> Stage(Migration& migration,
                                       const std::string& old_path,
                                       const std::string& new_path,
                                       Bytes bytes) {
  const auto target = migration.root / Platform::Utf8Path(new_path);
  std::error_code error;
  const bool present = fs::exists(target, error);
  if (error) return std::unexpected(new_path + ": " + error.message());
  if (present) {
    auto existing = ReadFileBytes(target);
    if (!existing) return std::unexpected(existing.error());
    if (*existing == bytes) {
      if (old_path != new_path) migration.renamed_paths[old_path] = new_path;
      return {};
    }
    if (old_path != new_path) {
      return std::unexpected(new_path +
                             ": semantic destination conflicts with existing "
                             "authored content; nothing was published");
    }
  }
  if (const auto found = migration.files.find(new_path);
      found != migration.files.end() && found->second != bytes) {
    return std::unexpected(
        new_path + ": multiple background outputs claim different bytes");
  }
  migration.files[new_path] = std::move(bytes);
  if (old_path != new_path) migration.renamed_paths[old_path] = new_path;
  return {};
}

std::expected<void, std::string> StageJson(Migration& migration,
                                           const std::string& old_path,
                                           const std::string& new_path,
                                           const Json& json) {
  return Stage(migration, old_path, new_path,
               TextBytes(Content::FormatJsonText(json) + "\n"));
}

Output AliasOf(const Output& canonical, const std::string& id) {
  Output alias = canonical;
  alias.id = id;
  alias.files.clear();
  alias.alias_of = canonical.id;
  alias.note = "Compatibility alias of " + canonical.id;
  return alias;
}

bool StockId(std::string_view id) {
  for (const auto& asset : Content::BackgroundAssets()) {
    if (id == Content::BackgroundLayoutId(asset) ||
        id == Content::LegacyBackgroundLayoutId(asset) ||
        id == Content::BackgroundImageId(asset) ||
        id == Content::LegacyBackgroundImageId(asset)) {
      return true;
    }
  }
  return false;
}

std::expected<const Output*, std::string> ExistingOutput(
    const Migration& migration, const std::string& old_id,
    const std::string& new_id, const std::string& source,
    std::string_view converter) {
  const auto* old = migration.before.Find(old_id);
  const auto* current = migration.before.Find(new_id);
  if (current != nullptr &&
      (!current->alias_of.empty() || current->source != source ||
       current->converter != converter)) {
    return std::unexpected(new_id +
                           ": semantic ID is already owned by another asset");
  }
  if (old != nullptr && (old->source != source || old->converter != converter ||
                         (!old->alias_of.empty() && old->alias_of != new_id))) {
    return std::unexpected(old_id +
                           ": compatibility ID has a conflicting owner");
  }
  if (old != nullptr && old->alias_of.empty() && current != nullptr) {
    return std::unexpected(source +
                           ": both legacy and semantic primary outputs exist");
  }
  if (current != nullptr) return current;
  if (old != nullptr) {
    if (!old->alias_of.empty()) {
      return std::unexpected(old_id + ": missing semantic alias target");
    }
    return old;
  }
  for (const auto& output : migration.before.outputs) {
    if (output.source == source && output.converter == converter &&
        output.alias_of.empty()) {
      return std::unexpected(source +
                             ": custom primary identity requires an explicit "
                             "compatibility mapping");
    }
  }
  return nullptr;
}

void ReplaceIdentity(Migration& migration, Output canonical,
                     const std::string& old_id) {
  const auto id = canonical.id;
  std::erase_if(migration.after.outputs, [&](const Output& output) {
    return output.id == old_id || output.id == id;
  });
  migration.after.outputs.push_back(AliasOf(canonical, old_id));
  migration.after.outputs.push_back(std::move(canonical));
}

std::expected<void, std::string> MigrateImage(Migration& migration,
                                              const Identity& asset) {
  const auto source = "BG/" + std::string(asset.source_stem) + ".TIM";
  const auto old_id = Content::LegacyBackgroundImageId(asset);
  const auto id = Content::BackgroundImageId(asset);
  auto existing =
      ExistingOutput(migration, old_id, id, source, kTimConverterName);
  if (!existing) return std::unexpected(existing.error());
  if (*existing == nullptr) return {};
  const auto& output = **existing;
  const bool legacy_identity =
      output.id != id ||
      migration.before.converters.at(std::string(kTimConverterName)) < 2;
  if (!legacy_identity) {
    ReplaceIdentity(migration, output, old_id);
    return {};
  }
  auto image = Content::LoadConvertedImageById(migration.root, migration.before,
                                               output.id);
  if (!image) return std::unexpected(image.error());
  std::string sidecar;
  for (const auto& file : output.files) {
    if (file.ends_with(Content::kImageSidecarSuffix)) sidecar = file;
  }
  if (sidecar.empty()) return std::unexpected(output.id + ": no image sidecar");
  const auto old_stem =
      sidecar.substr(0, sidecar.size() - Content::kImageSidecarSuffix.size());
  const auto legacy_stem = "textures/bg/" + std::string(asset.source_stem);
  const auto new_stem = old_stem == legacy_stem
                            ? Content::BackgroundImageStemPath(asset)
                            : old_stem;
  Output replacement = output;
  replacement.id = id;
  replacement.files.clear();
  for (const auto& file : output.files) {
    if (!file.starts_with(old_stem + ".")) {
      return std::unexpected(file +
                             ": image companion ownership is unresolved");
    }
    const auto target = new_stem + file.substr(old_stem.size());
    if (file == sidecar) {
      auto json = ReadJson(migration.root, file);
      if (!json) return std::unexpected(json.error());
      const auto original_json = *json;
      (*json)["id"] = id;
      const auto legacy_label = "Image " + source + " - texture " + old_id;
      const auto transitional_label = "Image " + source + " - texture " + id;
      if (legacy_identity &&
          (!json->contains(Content::kDisplayName) ||
           (*json)[Content::kDisplayName] == legacy_label ||
           (*json)[Content::kDisplayName] == transitional_label)) {
        (*json)[Content::kDisplayName] =
            "Background image: " + std::string(asset.display_name);
      }
      if (file != target || *json != original_json) {
        if (auto staged = StageJson(migration, file, target, *json); !staged) {
          return staged;
        }
      }
    } else {
      auto bytes = ReadFileBytes(migration.root / Platform::Utf8Path(file));
      if (!bytes) return std::unexpected(bytes.error());
      if (auto staged = Stage(migration, file, target, std::move(*bytes));
          !staged) {
        return staged;
      }
    }
    replacement.files.push_back(target);
  }
  if (output.id != id || output.files != replacement.files) {
    ++migration.report.identities;
  }
  ReplaceIdentity(migration, std::move(replacement), old_id);
  return {};
}

std::expected<void, std::string> MigrateLayout(Migration& migration,
                                               const Identity& asset) {
  const auto source = "BG/" + std::string(asset.source_stem) + ".FAR";
  const auto old_id = Content::LegacyBackgroundLayoutId(asset);
  const auto id = Content::BackgroundLayoutId(asset);
  auto existing =
      ExistingOutput(migration, old_id, id, source, Content::kFarConverterName);
  if (!existing) return std::unexpected(existing.error());
  if (*existing == nullptr) return {};
  const auto& output = **existing;
  const bool legacy_identity =
      output.id != id || migration.before.converters.at(
                             std::string(Content::kFarConverterName)) < 2;
  if (!legacy_identity) {
    ReplaceIdentity(migration, output, old_id);
    return {};
  }
  if (output.files.size() != 1) {
    return std::unexpected(output.id + ": expected one layout file");
  }
  const auto& old_path = output.files.front();
  auto bytes = ReadFileBytes(migration.root / Platform::Utf8Path(old_path));
  if (!bytes) return std::unexpected(bytes.error());
  auto before = Content::ReadBackgroundLayout(*bytes, old_path);
  if (!before) return std::unexpected(before.error());
  if (before->id != output.id || before->source != output.source ||
      before->source_sha256 != output.source_sha256 ||
      before->unreferenced != output.unreferenced ||
      output.dependencies !=
          std::vector<Content::SourceDependency>{
              {before->image_source, before->image_source_sha256}}) {
    return std::unexpected(
        old_path +
        ": authored layout identity/provenance does not match the manifest");
  }
  auto json = Content::ParseJsonText(*bytes, old_path);
  if (!json) return std::unexpected(json.error());
  const auto original_json = *json;
  (*json)["schema_version"] = Content::kBackgroundLayoutVersion;
  if (!json->contains("cell_size_units")) {
    (*json)["cell_size_units"] = Content::kBackgroundDefaultCellUnits;
  }
  (*json)["id"] = id;
  const auto image_id =
      legacy_identity ? Content::CanonicalBackgroundReference(before->image_id)
                      : before->image_id;
  (*json)["image_id"] = image_id;
  if (legacy_identity && !json->contains(Content::kDisplayName)) {
    (*json)[Content::kDisplayName] =
        "Background layout: " + std::string(asset.display_name);
  }
  const auto text = Content::FormatJsonText(*json);
  auto after =
      Content::ReadBackgroundLayout(std::as_bytes(std::span(text)), old_path);
  if (!after) return std::unexpected(after.error());
  before->id = id;
  before->image_id = image_id;
  if (*before != *after) {
    return std::unexpected(old_path +
                           ": migration changed authored layout values");
  }
  const auto legacy_path =
      "backgrounds/" + std::string(asset.source_stem) + ".background.json";
  const auto target =
      old_path == legacy_path ? Content::BackgroundLayoutPath(asset) : old_path;
  if (old_path != target || *json != original_json) {
    if (auto staged = StageJson(migration, old_path, target, *json); !staged) {
      return staged;
    }
  }
  Output replacement = output;
  replacement.id = id;
  replacement.files = {target};
  replacement.note =
      "Typed camera-wrapped background: " + std::string(asset.display_name);
  if (output.id != id || output.files != replacement.files) {
    ++migration.report.identities;
  }
  ReplaceIdentity(migration, std::move(replacement), old_id);
  return {};
}

Content::ImageFileReader CandidateFiles(const Migration& migration) {
  return [&migration](const std::string& path) -> std::optional<Bytes> {
    const auto pending = migration.files.find(path);
    if (pending != migration.files.end()) return pending->second;
    return Content::DirectoryFileReader(migration.root)(path);
  };
}

std::expected<void, std::string> ValidateCandidates(
    const Migration& migration) {
  const auto reader = CandidateFiles(migration);
  for (const auto& asset : Content::BackgroundAssets()) {
    const auto* image_output =
        migration.after.Find(Content::BackgroundImageId(asset));
    if (image_output != nullptr) {
      auto after = Content::LoadConvertedImageById(migration.after,
                                                   image_output->id, reader);
      if (!after) return std::unexpected(after.error());
      const auto* old =
          migration.before.Find(Content::LegacyBackgroundImageId(asset));
      const auto* canonical = migration.before.Find(image_output->id);
      const auto* original = canonical != nullptr ? canonical : old;
      if (original != nullptr) {
        auto before = Content::LoadConvertedImageById(
            migration.root, migration.before, original->id);
        if (!before) return std::unexpected(before.error());
        before->id = after->id;
        if (*before != *after) {
          return std::unexpected(image_output->id +
                                 ": migration changed native image values");
        }
      }
    }
    const auto* layout_output =
        migration.after.Find(Content::BackgroundLayoutId(asset));
    if (layout_output == nullptr) continue;
    const auto& path = layout_output->files.front();
    auto bytes = reader(path);
    if (!bytes) return std::unexpected("cannot read staged layout " + path);
    auto layout = Content::ReadBackgroundLayout(*bytes, path);
    if (!layout) return std::unexpected(layout.error());
    auto image = Content::LoadConvertedImageById(migration.after,
                                                 layout->image_id, reader);
    if (!image) return std::unexpected(image.error());
    if (auto valid = Content::ValidateBackgroundImage(*layout, *image);
        !valid) {
      return valid;
    }
  }
  return {};
}

std::expected<std::set<std::string>, std::string> CatalogIdentities(
    Json& json, const std::string& path) {
  if (!json.is_object() || !json.contains("assets") ||
      !json["assets"].is_array()) {
    return std::unexpected(path + ": malformed asset catalog");
  }
  if (!json.contains("schema") ||
      json["schema"] != std::string(Content::kAssetCatalogSchema) ||
      !json.contains("schema_version") ||
      (json["schema_version"] != 1 && json["schema_version"] != 2)) {
    return std::unexpected(path + ": unsupported asset catalog schema/version");
  }
  if (json["schema_version"] == 1 && json.contains("aliases")) {
    return std::unexpected(path +
                           ": version 1 cannot declare compatibility aliases");
  }
  if (!json.contains("aliases")) json["aliases"] = Json::object();
  if (!json["aliases"].is_object()) {
    return std::unexpected(path + ": aliases must be an object");
  }
  std::set<std::string> ids;
  for (const auto& record : json["assets"]) {
    if (!record.contains("id") || !record["id"].is_string()) {
      return std::unexpected(path + ": asset record has no ID");
    }
    if (!ids.insert(record["id"].get<std::string>()).second) {
      return std::unexpected(path + ": duplicate asset ID");
    }
  }
  return ids;
}

std::expected<void, std::string> MigrateCatalog(Migration& migration) {
  const auto path = std::string(Content::kCatalogDirectory) + "/" +
                    std::string(Content::kAssetCatalogFile);
  std::error_code error;
  if (!fs::exists(migration.root / Platform::Utf8Path(path), error)) {
    if (error) return std::unexpected(path + ": " + error.message());
    return {};
  }
  auto json = ReadJson(migration.root, path);
  if (!json) return std::unexpected(json.error());
  const Json before = *json;
  const auto ids = CatalogIdentities(*json, path);
  if (!ids) return std::unexpected(ids.error());
  auto& aliases = (*json)["aliases"];
  bool changed = false;
  for (auto& record : (*json)["assets"]) {
    if (record.contains("file") && record["file"].is_string() &&
        (migration.renamed_paths.contains(record["file"].get<std::string>()) ||
         migration.files.contains(record["file"].get<std::string>()))) {
      return std::unexpected(
          path +
          ": a custom file record references a renamed native image; migrate "
          "that file reference explicitly");
    }
    if (!record.contains("provenance") || !record["provenance"].is_object() ||
        !record["provenance"].contains("disc_path") ||
        !record["provenance"]["disc_path"].is_string()) {
      continue;
    }
    const auto source = record["provenance"]["disc_path"].get<std::string>();
    const auto canonical = Content::BackgroundSourceAssetId(source);
    if (!canonical) continue;
    const auto legacy = Content::AssetIdForDiscPath(source);
    if (!legacy) {
      return std::unexpected(path + ": invalid background source path");
    }
    const auto id = record["id"].get<std::string>();
    if (id != *legacy && id != *canonical) continue;
    if (id != *canonical && ids->contains(*canonical)) {
      return std::unexpected(
          path + ": semantic background source ID is already present");
    }
    if (aliases.contains(*legacy) && aliases[*legacy] != *canonical) {
      return std::unexpected(
          path + ": background source alias has a conflicting target");
    }
    for (auto& alias : aliases.items()) {
      if (alias.value() == *legacy) alias.value() = *canonical;
    }
    record["id"] = *canonical;
    aliases[*legacy] = *canonical;
    changed = true;
  }
  if (!changed) return {};
  (*json)["schema_version"] = Content::kAssetCatalogSchemaVersion;
  const auto text = Content::FormatJsonText(*json) + "\n";
  auto audio = ReadFileBytes(
      migration.root /
      Platform::Utf8Path(std::string(Content::kCatalogDirectory) + "/" +
                         std::string(Content::kAudioCatalogFile)));
  if (!audio) return std::unexpected(audio.error());
  const std::string audio_text(reinterpret_cast<const char*>(audio->data()),
                               audio->size());
  auto valid =
      Content::AssetCatalog::Parse(text, audio_text, migration.root, 1);
  if (!valid) return std::unexpected(valid.error());
  if (*json == before) return {};
  return Stage(migration, path, path, TextBytes(text));
}

std::expected<void, std::string> Prepare(Migration& migration) {
  for (const auto& asset : Content::BackgroundAssets()) {
    if (auto result = MigrateImage(migration, asset); !result) {
      return result;
    }
    if (auto result = MigrateLayout(migration, asset); !result) {
      return result;
    }
  }
  for (auto& output : migration.after.outputs) {
    output.alias_of = Content::CanonicalBackgroundReference(output.alias_of);
    for (auto& file : output.files) {
      if (const auto found = migration.renamed_paths.find(file);
          found != migration.renamed_paths.end()) {
        file = found->second;
      }
    }
  }
  for (const auto name : {kTimConverterName, Content::kFarConverterName}) {
    const auto found = migration.after.converters.find(name);
    if (found == migration.after.converters.end()) continue;
    if (found->second < 1 || found->second > 2) {
      return std::unexpected(std::string(name) +
                             ": unsupported authoring migration version");
    }
    found->second = 2;
  }
  std::ranges::sort(migration.after.outputs, {}, &Output::id);
  if (auto valid = Content::ValidateConversionManifest(migration.after);
      !valid) {
    return valid;
  }
  if (!migration.files.empty() || migration.report.identities != 0) {
    if (auto valid = ValidateCandidates(migration); !valid) {
      return valid;
    }
  }
  return MigrateCatalog(migration);
}

}  // namespace

std::expected<BackgroundAuthoringReport, std::string>
UpgradeBackgroundAuthoring(const fs::path& root) {
  auto previous = Content::LoadConversionManifest(root);
  if (!previous) return std::unexpected(previous.error());
  if (!*previous) return BackgroundAuthoringReport{};
  Migration migration{.root = root,
                      .before = **previous,
                      .after = **previous,
                      .files = {},
                      .renamed_paths = {},
                      .report = {}};
  if (auto prepared = Prepare(migration); !prepared) {
    return std::unexpected(prepared.error());
  }
  std::vector<OutputBytes> files;
  for (const auto& [name, bytes] : migration.files) {
    files.push_back({name, bytes});
  }
  if (auto written = WriteOutputFiles(root, files); !written) {
    return std::unexpected(written.error());
  }
  migration.report.files = files.size();
  if (migration.after != migration.before) {
    if (auto written = WriteConversionManifest(root, migration.after);
        !written) {
      return std::unexpected(written.error());
    }
  }
  for (const auto& [old_path, new_path] : migration.renamed_paths) {
    (void)new_path;
    std::error_code error;
    if (fs::remove(root / Platform::Utf8Path(old_path), error)) {
      ++migration.report.removed;
    }
    if (error) return std::unexpected(old_path + ": " + error.message());
  }
  return migration.report;
}

std::expected<void, std::string> KeepBackgroundCompatibility(
    ConverterBatch& batch, const Content::ConversionManifest& previous) {
  const auto canonical_outputs = batch.outputs;
  for (const auto& output : canonical_outputs) {
    const auto* asset = Content::BackgroundAssetForSource(output.source);
    if (asset == nullptr) continue;
    const bool layout = output.converter == Content::kFarConverterName;
    const auto expected = layout ? Content::BackgroundLayoutId(*asset)
                                 : Content::BackgroundImageId(*asset);
    if (output.id != expected || !output.alias_of.empty()) {
      return std::unexpected(
          output.id +
          ": background converter did not produce a canonical identity");
    }
    batch.outputs.push_back(
        AliasOf(output, layout ? Content::LegacyBackgroundLayoutId(*asset)
                               : Content::LegacyBackgroundImageId(*asset)));
  }
  for (auto output : previous.outputs) {
    if (output.converter != batch.name || StockId(output.id) ||
        Content::BackgroundAssetForSource(output.source) == nullptr) {
      continue;
    }
    if (std::ranges::any_of(batch.outputs, [&](const auto& item) {
          return item.id == output.id;
        })) {
      continue;
    }
    output.alias_of = Content::CanonicalBackgroundReference(output.alias_of);
    batch.outputs.push_back(std::move(output));
  }
  return {};
}

}  // namespace VS::Extract
