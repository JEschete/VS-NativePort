#include "Extract/ZoneAuthoring.hpp"

#include <cstddef>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "Content/ActionCatalog.hpp"
#include "Content/CatalogTable.hpp"
#include "Content/ConversionManifest.hpp"
#include "Content/ZoneTables.hpp"
#include "Extract/ConversionManifestWriter.hpp"
#include "Extract/ConverterBatch.hpp"
#include "Platform/Paths.hpp"

namespace VS::Extract {
namespace {
namespace fs = std::filesystem;
using Texts = std::map<std::string, std::string>;

std::expected<std::string, std::string> Read(const fs::path& file) {
  auto bytes = ReadFileBytes(file);
  if (!bytes) return std::unexpected(bytes.error());
  if (bytes->empty()) {
    return std::unexpected("empty zone table file " + Platform::PathText(file));
  }
  return std::string(reinterpret_cast<const char*>(bytes->data()),
                     bytes->size());
}

Content::ZoneTableReader Reader(const Texts& texts) {
  return [&texts](
             std::string_view path) -> std::expected<std::string, std::string> {
    const auto found = texts.find(std::string(path));
    if (found == texts.end()) {
      return std::unexpected("missing " + std::string(path));
    }
    return found->second;
  };
}

std::expected<void, std::string> PrepareFiles(const fs::path& root,
                                              Texts& pending,
                                              ZoneAuthoringReport& report) {
  Texts original;
  Texts normalized;
  // The limbs name their actions from the action catalog, which upgrades
  // before the zones.
  const fs::path catalog = root / Platform::Utf8Path(Content::kActionsPath);
  std::error_code error;
  if (fs::exists(catalog, error)) {
    auto text = Read(catalog);
    if (!text) return std::unexpected(text.error());
    original.emplace(Content::kActionsPath, *text);
    normalized.emplace(Content::kActionsPath, *std::move(text));
  } else if (error) {
    return std::unexpected(Platform::PathText(catalog) + ": " +
                           error.message());
  }
  const Content::ZoneTableReader read_original = Reader(original);
  for (const auto table : Content::kZoneTables) {
    const auto path = Content::ZoneTablePath(table);
    auto text = Read(root / Platform::Utf8Path(path));
    if (!text) return std::unexpected(text.error());
    // The migration is checked to be lossless; the records check below
    // compares its result with the normalized tables.
    auto migrated =
        Content::MigrateZoneColumns(table, *text, path, read_original);
    if (!migrated) return std::unexpected(migrated.error());
    report.split_columns += migrated->split_columns;
    report.retired_columns += migrated->retired_columns;
    report.named_values += migrated->named_values;
    report.resigned_values += migrated->resigned_values;
    auto rewrite = Content::NormalizeTableText(Content::ZoneTableSpec(table),
                                               migrated->text, path);
    if (!rewrite) return std::unexpected(rewrite.error());
    original.emplace(path, migrated->text);
    normalized.emplace(path, rewrite->text);
    if (*text != rewrite->text) pending.emplace(path, rewrite->text);
    report.renamed_columns += rewrite->renamed_columns;
    report.named_values += rewrite->named_values;
    const auto schema_path = Content::ZoneTableSchemaPath(table);
    auto schema = Read(root / Platform::Utf8Path(schema_path));
    if (!schema) return std::unexpected(schema.error());
    const auto expected =
        Content::FormatTableSchema(Content::ZoneTableSpec(table));
    if (*schema != expected) pending.emplace(schema_path, expected);
  }
  auto before = Content::ReadZoneTables(read_original);
  if (!before) return std::unexpected(before.error());
  auto after = Content::ReadZoneTables(Reader(normalized));
  if (!after) return std::unexpected(after.error());
  if (*before != *after) {
    return std::unexpected("zone authoring migration changed native records");
  }
  return {};
}

std::expected<bool, std::string> HasTables(const fs::path& root) {
  bool any = false;
  for (const auto table : Content::kZoneTables) {
    const auto path = root / Platform::Utf8Path(Content::ZoneTablePath(table));
    std::error_code error;
    const bool exists = fs::exists(path, error);
    if (error) {
      return std::unexpected(Platform::PathText(path) + ": " + error.message());
    }
    any = any || exists;
  }
  return any;
}
}  // namespace

std::expected<ZoneAuthoringReport, std::string> UpgradeZoneAuthoring(
    const fs::path& root) {
  auto present = HasTables(root);
  if (!present) return std::unexpected(present.error());
  if (!*present) return ZoneAuthoringReport{};
  auto manifest = Content::LoadConversionManifest(root);
  if (!manifest) return std::unexpected(manifest.error());
  bool version_changed = false;
  if (*manifest) {
    auto found = (**manifest).converters.find(Content::kZndTableConverterName);
    if (found != (**manifest).converters.end()) {
      // Version 1 tables have numeric names and values, version 2 the packed
      // shape_bits, version 3 numeric status_immunities, version 4 the
      // reserved bytes' raw_<offset> columns, version 5 numeric action ids
      // and the action_N_index columns, version 6 the equipment index
      // columns, version 7 unsigned agility_defense_bonus cells, version 8
      // the zud_index column, version 9 the packed ai_settings and version
      // 10 the class and affinity storage columns; all upgrade in place.
      if (found->second < 1 ||
          found->second > Content::kZndTableConverterVersion) {
        return std::unexpected(
            "unsupported zone table version for authoring migration");
      }
      version_changed = found->second != Content::kZndTableConverterVersion;
      found->second = Content::kZndTableConverterVersion;
    }
  }
  ZoneAuthoringReport report;
  Texts pending;
  if (auto prepared = PrepareFiles(root, pending, report); !prepared) {
    return std::unexpected(prepared.error());
  }
  std::vector<OutputBytes> outputs;
  for (const auto& [path, text] : pending) {
    outputs.push_back({.name = path, .bytes = std::as_bytes(std::span(text))});
  }
  if (!outputs.empty()) {
    if (auto written = WriteOutputFiles(root, outputs); !written) {
      return std::unexpected(written.error());
    }
  }
  report.written = outputs.size();
  if (version_changed && *manifest) {
    if (auto written = WriteConversionManifest(root, **manifest); !written) {
      return std::unexpected(written.error());
    }
    ++report.written;
  }
  return report;
}

}  // namespace VS::Extract
