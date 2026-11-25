#include "Extract/ZoneAuthoring.hpp"

#include <cstddef>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

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
      return std::unexpected("missing zone table " + std::string(path));
    }
    return found->second;
  };
}

std::expected<void, std::string> PrepareFiles(const fs::path& root,
                                              Texts& pending,
                                              ZoneAuthoringReport& report) {
  Texts original;
  Texts normalized;
  for (const auto table : Content::kZoneTables) {
    const auto path = Content::ZoneTablePath(table);
    auto text = Read(root / Platform::Utf8Path(path));
    if (!text) return std::unexpected(text.error());
    // The split is checked to be lossless; the records check below compares
    // its result with the normalized tables.
    auto split = Content::SplitPackedZoneColumns(table, *text, path);
    if (!split) return std::unexpected(split.error());
    report.split_columns += split->split_columns;
    auto rewrite = Content::NormalizeTableText(Content::ZoneTableSpec(table),
                                               split->text, path);
    if (!rewrite) return std::unexpected(rewrite.error());
    original.emplace(path, split->text);
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
  auto before = Content::ReadZoneTables(Reader(original));
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
      // shape_bits; both upgrade in place.
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
