#include "Extract/ZoneAuthoring.hpp"

#include <algorithm>
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

std::expected<bool, std::string> Exists(const fs::path& file) {
  std::error_code error;
  const bool exists = fs::exists(file, error);
  if (error) {
    return std::unexpected(Platform::PathText(file) + ": " + error.message());
  }
  return exists;
}

// The table's text. Version 12 has no enemy-kinds table: when the enemies'
// migration made one (`made`), it is the table, and is written.
std::expected<std::string, std::string> TableText(const fs::path& root,
                                                  Content::ZoneTable table,
                                                  const std::string& made,
                                                  Texts& pending) {
  const auto path = Content::ZoneTablePath(table);
  const fs::path file = root / Platform::Utf8Path(path);
  if (table != Content::ZoneTable::kKinds || made.empty()) return Read(file);
  auto exists = Exists(file);
  if (!exists) return std::unexpected(exists.error());
  if (*exists) {
    return std::unexpected(path +
                           " exists, but the enemies table names kinds by "
                           "number (sub_type)");
  }
  pending.emplace(path, made);
  return made;
}

// Writes the table's schema when it is missing or differs.
std::expected<void, std::string> PrepareSchema(const fs::path& root,
                                               Content::ZoneTable table,
                                               Texts& pending) {
  const auto path = Content::ZoneTableSchemaPath(table);
  const fs::path file = root / Platform::Utf8Path(path);
  const auto expected =
      Content::FormatTableSchema(Content::ZoneTableSpec(table));
  auto exists = Exists(file);
  if (!exists) return std::unexpected(exists.error());
  if (*exists) {
    auto schema = Read(file);
    if (!schema) return std::unexpected(schema.error());
    if (*schema == expected) return {};
  }
  pending.insert_or_assign(path, expected);
  return {};
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
  std::string kinds;  // the enemy-kinds table the enemies' migration made
  for (const auto table : Content::kZoneTables) {
    const auto path = Content::ZoneTablePath(table);
    auto text = TableText(root, table, kinds, pending);
    if (!text) return std::unexpected(text.error());
    // The migration is checked to be lossless; the records check below
    // compares its result with the normalized tables.
    auto migrated =
        Content::MigrateZoneColumns(table, *text, path, read_original);
    if (!migrated) return std::unexpected(migrated.error());
    if (!migrated->kinds.empty()) kinds = migrated->kinds;
    report.split_columns += migrated->split_columns;
    report.retired_columns += migrated->retired_columns;
    report.named_values += migrated->named_values;
    report.resigned_values += migrated->resigned_values;
    report.decoded_texts += migrated->decoded_texts;
    auto rewrite = Content::NormalizeTableText(Content::ZoneTableSpec(table),
                                               migrated->text, path);
    if (!rewrite) return std::unexpected(rewrite.error());
    original.emplace(path, migrated->text);
    normalized.emplace(path, rewrite->text);
    if (*text != rewrite->text) pending.insert_or_assign(path, rewrite->text);
    report.renamed_columns += rewrite->renamed_columns;
    report.named_values += rewrite->named_values;
    if (auto schema = PrepareSchema(root, table, pending); !schema) {
      return std::unexpected(schema.error());
    }
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
    auto exists =
        Exists(root / Platform::Utf8Path(Content::ZoneTablePath(table)));
    if (!exists) return std::unexpected(exists.error());
    any = any || *exists;
  }
  return any;
}

// Lists every zone table and schema in each zone output that lists files, as
// ConvertDiscZones does; whether a file was missing (a table version 13
// added). Unlisted files would be removed as stale.
bool ListTableFiles(Content::ConversionManifest& manifest) {
  std::vector<std::string> tables;
  for (const auto table : Content::kZoneTables) {
    tables.push_back(Content::ZoneTablePath(table));
    tables.push_back(Content::ZoneTableSchemaPath(table));
  }
  bool added = false;
  for (Content::ConversionOutput& output : manifest.outputs) {
    if (output.converter != Content::kZndTableConverterName ||
        output.files.empty()) {
      continue;
    }
    for (const std::string& file : tables) {
      if (std::ranges::find(output.files, file) != output.files.end()) continue;
      output.files.push_back(file);
      added = true;
    }
    std::ranges::sort(output.files);
  }
  return added;
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
      // the zud_index column, version 9 the packed ai_settings, version 10
      // the class and affinity storage columns, version 11 hexadecimal
      // names, version 12 numeric sub_type kinds without the enemy-kinds
      // table, version 13 numeric gem effects and version 14 numeric part
      // categories; all upgrade in place.
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
  const bool files_added = *manifest && ListTableFiles(**manifest);
  if ((version_changed || files_added) && *manifest) {
    if (auto written = WriteConversionManifest(root, **manifest); !written) {
      return std::unexpected(written.error());
    }
    ++report.written;
  }
  return report;
}

}  // namespace VS::Extract
