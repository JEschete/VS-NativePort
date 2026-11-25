#include "Extract/WorkshopAuthoring.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <format>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "Content/CatalogTable.hpp"
#include "Content/ConversionManifest.hpp"
#include "Content/JsonText.hpp"
#include "Content/WorkshopRecipes.hpp"
#include "Extract/AssetLabels.hpp"
#include "Extract/ConversionManifestWriter.hpp"
#include "Extract/ConverterBatch.hpp"
#include "Extract/WorkshopRecipeConversion.hpp"
#include "Platform/Paths.hpp"

namespace VS::Extract {
namespace {
namespace fs = std::filesystem;
using Check = std::expected<void, std::string>;
using Aliases = std::map<std::string, std::string, std::less<>>;
using Content::WorkshopKind;
using Content::WorkshopTablePart;
constexpr std::uint32_t kEquipmentItemBase = 126;
constexpr std::uint8_t kShieldCategory = 1;
constexpr unsigned char kLastAscii = 0x7F;
constexpr std::string_view kImportSchema = "vs.workshop-import";
// Version 1 had no reserved_bytes.
constexpr std::uint64_t kImportVersion = 2;
constexpr unsigned kHexRadix = 16;
constexpr std::size_t kHexDigits = 2;
constexpr std::string_view kRawColumnPrefix = "raw.byte";

// An import file as read: its provenance and its text, if it exists.
// NOLINTNEXTLINE(bugprone-exception-escape): std::map's noexcept move.
struct StoredImport {
  WorkshopImport rows;
  std::optional<std::string> text;
};

std::size_t RecordCount(const Content::WorkshopRecipeTable& table) {
  return table.kind == WorkshopKind::kBlade ? table.blades.size()
                                            : table.pieces.size();
}

std::expected<std::vector<std::byte>, std::string> ImportBytes(
    std::string_view text, std::size_t width, std::string_view where) {
  if (text.size() != width * kHexDigits) {
    return std::unexpected(
        std::format("{}: wrong imported record size", where));
  }
  if (text.find_first_not_of("0123456789abcdef") != std::string_view::npos) {
    return std::unexpected(
        std::format("{}: imported bytes must be lowercase hexadecimal", where));
  }
  std::vector<std::byte> record;
  for (std::size_t at = 0; at < text.size(); at += kHexDigits) {
    unsigned value = 0;
    const char* start = text.data() + at;
    const auto parsed =
        std::from_chars(start, start + kHexDigits, value, kHexRadix);
    if (parsed.ec != std::errc() || parsed.ptr != start + kHexDigits) {
      return std::unexpected(
          std::format("{}: invalid imported hex byte", where));
    }
    record.push_back(static_cast<std::byte>(value));
  }
  return record;
}

std::expected<std::uint64_t, std::string> ImportHeader(
    const Content::JsonMembers& members,
    const Content::WorkshopLayout& layout) {
  auto schema = members.String("schema");
  if (!schema) return std::unexpected(schema.error());
  auto version = members.Unsigned("schema_version", kImportVersion);
  if (!version) return std::unexpected(version.error());
  auto source = members.String("source");
  if (!source) return std::unexpected(source.error());
  auto width = members.Unsigned("record_bytes", layout.record_bytes);
  if (!width) return std::unexpected(width.error());
  if (*schema != kImportSchema || *version == 0 || *source != layout.source ||
      *width != layout.record_bytes) {
    return std::unexpected("incompatible workshop import metadata");
  }
  return *version;
}

Check ReadUnusedResults(const Content::JsonMembers& members,
                        const Content::WorkshopLayout& layout,
                        const std::string& relative, WorkshopImport& imported) {
  auto rows = members.Get("unused_results");
  if (!rows) return std::unexpected(rows.error());
  if (!(*rows)->is_array()) {
    return std::unexpected(relative + ": unused_results must be an array");
  }
  for (const auto& row : **rows) {
    const Content::JsonMembers item(row, relative + ".unused_results");
    if (auto only = item.Only({"legacy_id", "bytes"}); !only) {
      return std::unexpected(only.error());
    }
    auto legacy = item.Unsigned("legacy_id", UINT8_MAX);
    if (!legacy) return std::unexpected(legacy.error());
    auto text = item.String("bytes");
    if (!text) return std::unexpected(text.error());
    auto record = ImportBytes(*text, layout.record_bytes, relative);
    if (!record) return std::unexpected(record.error());
    if (std::to_integer<std::uint32_t>(record->front()) != *legacy ||
        !imported.unused_results
             .emplace(static_cast<std::uint32_t>(*legacy), std::move(*record))
             .second) {
      return std::unexpected(relative +
                             ": duplicate or mismatched imported id");
    }
  }
  return {};
}

bool SameByte(const ReservedRecordByte& left, const ReservedRecordByte& right) {
  return left.legacy_id == right.legacy_id && left.offset == right.offset;
}

Check ReadReservedBytes(const Content::JsonDocument& rows, WorkshopKind kind,
                        const std::string& relative, WorkshopImport& imported) {
  if (!rows.is_array()) {
    return std::unexpected(relative + ": reserved_bytes must be an array");
  }
  const std::span<const std::uint8_t> offsets = ReservedRecordOffsets(kind);
  for (const auto& row : rows) {
    const Content::JsonMembers item(row, relative + ".reserved_bytes");
    if (auto only = item.Only({"legacy_id", "byte", "value"}); !only) {
      return std::unexpected(only.error());
    }
    auto legacy = item.Unsigned("legacy_id", UINT8_MAX);
    if (!legacy) return std::unexpected(legacy.error());
    auto offset = item.Unsigned("byte", UINT8_MAX);
    if (!offset) return std::unexpected(offset.error());
    auto value = item.Unsigned("value", UINT8_MAX);
    if (!value) return std::unexpected(value.error());
    if (*value == 0 || std::ranges::find(offsets, *offset) == offsets.end()) {
      return std::unexpected(
          std::format("{}: byte {} of record {} is not a non-zero reserved "
                      "record byte",
                      relative, *offset, *legacy));
    }
    imported.reserved_bytes.push_back(
        {.legacy_id = static_cast<std::uint32_t>(*legacy),
         .offset = static_cast<std::uint8_t>(*offset),
         .value = static_cast<std::uint8_t>(*value)});
  }
  std::ranges::sort(imported.reserved_bytes);
  if (std::ranges::adjacent_find(imported.reserved_bytes, SameByte) !=
      imported.reserved_bytes.end()) {
    return std::unexpected(relative + ": a reserved byte is listed twice");
  }
  return {};
}

std::expected<StoredImport, std::string> ReadImport(const fs::path& root,
                                                    WorkshopKind kind) {
  const std::string relative = WorkshopImportPath(kind);
  const fs::path path = root / Platform::Utf8Path(relative);
  std::error_code error;
  const bool present = fs::is_regular_file(path, error);
  if (error && error != std::errc::no_such_file_or_directory) {
    return std::unexpected(std::format("{}: {}", relative, error.message()));
  }
  if (!present) return StoredImport{};
  auto bytes = ReadFileBytes(path);
  if (!bytes) return std::unexpected(bytes.error());
  auto json = Content::ParseJsonText(*bytes, relative);
  if (!json) return std::unexpected(json.error());
  const Content::JsonMembers members(*json, relative);
  if (auto only =
          members.Only({"schema", "schema_version", "source", "record_bytes",
                        "unused_results", "reserved_bytes"});
      !only) {
    return std::unexpected(only.error());
  }
  const auto& layout = Content::LayoutOf(kind);
  auto version = ImportHeader(members, layout);
  if (!version) {
    return std::unexpected(std::format("{}: {}", relative, version.error()));
  }
  StoredImport stored{
      .rows = {},
      .text = std::string(reinterpret_cast<const char*>(bytes->data()),
                          bytes->size())};
  if (auto unused = ReadUnusedResults(members, layout, relative, stored.rows);
      !unused) {
    return std::unexpected(unused.error());
  }
  if (const Content::JsonDocument* reserved = members.Find("reserved_bytes")) {
    if (*version < kImportVersion) {
      return std::unexpected(relative +
                             ": reserved_bytes needs schema_version 2");
    }
    if (auto read = ReadReservedBytes(*reserved, kind, relative, stored.rows);
        !read) {
      return std::unexpected(read.error());
    }
  }
  return stored;
}

std::expected<std::vector<std::byte>, std::string> WithImport(
    const Content::WorkshopRecipeTable& table, const WorkshopImport& imported) {
  auto bytes = EncodeSydRecipes(table, imported.reserved_bytes);
  if (!bytes) return std::unexpected(bytes.error());
  std::size_t count = RecordCount(table);
  for (const auto& [legacy, record] : imported.unused_results) {
    // An explicitly authored definition activates its former unused slot.
    if (legacy < count) continue;
    if (legacy != count) {
      return std::unexpected(
          "import-only recipe records leave a legacy-id gap");
    }
    bytes->insert(bytes->end(), record.begin(), record.end());
    ++count;
  }
  return bytes;
}

std::expected<std::string, std::string> Slug(std::string_view name) {
  if (name.find("[glyph ") != std::string_view::npos ||
      name.find("[control ") != std::string_view::npos) {
    return std::unexpected("the name contains unresolved text codes");
  }
  std::string slug;
  bool separated = false;
  for (const char character : name) {
    const auto byte = static_cast<unsigned char>(character);
    if (byte > kLastAscii) {
      return std::unexpected("the name needs an explicit ASCII semantic id");
    }
    const char letter = character >= 'A' && character <= 'Z'
                            ? static_cast<char>(character - 'A' + 'a')
                            : character;
    if ((letter >= 'a' && letter <= 'z') || (letter >= '0' && letter <= '9')) {
      if (separated && !slug.empty()) slug += '-';
      slug += letter;
      separated = false;
    } else if (letter != '\'') {
      separated = true;
    }
  }
  if (slug.empty()) return std::unexpected("the item has no readable name");
  return slug;
}

struct Naming {
  const fs::path& root;
  std::optional<std::vector<std::string>> items;

  std::expected<std::string, std::string> Item(std::uint32_t id) {
    if (!items) {
      auto loaded = ReadItemDisplayNames(root);
      if (!loaded) return std::unexpected(loaded.error());
      items = std::move(*loaded);
    }
    if (id >= items->size() || items->at(id).empty()) {
      return std::unexpected(std::format("item {} has no readable name", id));
    }
    return items->at(id);
  }
};

Check AddAlias(Aliases& aliases, std::string alias, const std::string& id) {
  const auto [found, added] = aliases.emplace(std::move(alias), id);
  if (!added && found->second != id) {
    return std::unexpected(
        std::format("ambiguous legacy alias '{}'", found->first));
  }
  return {};
}

std::optional<std::string_view> BladeCategory(std::uint8_t category) {
  // ItemRows.cpp: category - 1 + itemhelp_bin::index::kDagger.
  // StringTables.hpp offsets 9870..9907 name these nine source strings.
  constexpr std::array<std::string_view, 10> kCategories{
      "",          "dagger", "sword",      "great-sword", "axe-mace",
      "great-axe", "staff",  "heavy-mace", "polearm",     "crossbow"};
  if (category == 0 || category >= kCategories.size()) return std::nullopt;
  return kCategories[category];
}

template <typename Record>
std::expected<std::string, std::string> ProposedId(const Record& record,
                                                   WorkshopKind kind,
                                                   Naming& names) {
  const auto& layout = Content::LayoutOf(kind);
  if (record.id != Content::LegacyWorkshopResultId(layout, record.legacy_id)) {
    return record.id;
  }
  const std::string_view prefix =
      kind == WorkshopKind::kArmor && record.category == kShieldCategory
          ? "shield"
          : layout.name;
  if (record.legacy_id == 0) return std::format("{}.none", prefix);
  auto name =
      names.Item(record.legacy_id +
                 (kind == WorkshopKind::kBlade ? 0 : kEquipmentItemBase));
  if (!name) {
    return std::unexpected(std::format("{}: {}", record.id, name.error()));
  }
  if (*name == "untitled") {
    return std::unexpected(std::format(
        "{}: an edited or referenced untitled record needs an explicit "
        "semantic id",
        record.id));
  }
  auto slug = Slug(*name);
  if (!slug) {
    return std::unexpected(std::format("{}: {}", record.id, slug.error()));
  }
  return std::format("{}.{}", prefix, *slug);
}

template <typename Record>
Check NameRecords(std::vector<Record>& records, WorkshopKind kind,
                  Naming& names, Aliases& aliases,
                  WorkshopAuthoringReport& report) {
  const auto& layout = Content::LayoutOf(kind);
  std::set<std::string> assigned;
  std::vector<std::string> proposed;
  std::map<std::string, std::size_t> counts;
  for (const Record& record : records) {
    auto id = ProposedId(record, kind, names);
    if (!id) return std::unexpected(id.error());
    ++counts[*id];
    proposed.push_back(std::move(*id));
  }
  for (std::size_t index = 0; index < records.size(); ++index) {
    Record& record = records[index];
    const std::string previous = record.id;
    const std::string legacy =
        Content::LegacyWorkshopResultId(layout, record.legacy_id);
    if (previous == legacy) {
      record.id = proposed[index];
      if (counts.at(record.id) > 1 && kind == WorkshopKind::kBlade) {
        if (auto category = BladeCategory(record.category); category) {
          record.id += "." + std::string(*category);
        }
      }
      ++report.renamed;
    }
    if (!assigned.insert(record.id).second) {
      return std::unexpected(std::format(
          "{}: semantic id '{}' collides; assign an explicit variant id",
          layout.source, record.id));
    }
    if (auto alias = AddAlias(aliases, previous, record.id); !alias) {
      return alias;
    }
    if (auto alias = AddAlias(aliases, legacy, record.id); !alias) {
      return alias;
    }
  }
  return {};
}

Check RenamePairs(Content::WorkshopRecipeTable& table, const Aliases& aliases) {
  for (auto& pair : table.pairs) {
    for (std::string* reference : {&pair.first, &pair.second, &pair.result}) {
      const auto found = aliases.find(*reference);
      if (found == aliases.end()) {
        return std::unexpected(
            std::format("unresolved recipe reference '{}'", *reference));
      }
      *reference = found->second;
    }
  }
  return {};
}

std::expected<std::set<std::string>, std::string> ProtectedCaptions(
    const std::string& text, std::string_view file) {
  auto records = Content::ParseCsv(text, file);
  if (!records) return std::unexpected(records.error());
  if (records->empty()) {
    return std::unexpected(std::string(file) + ": no header");
  }
  const auto& header = records->front().fields;
  const auto label = std::ranges::find(header, Content::kDisplayName);
  std::set<std::string> protected_ids;
  if (label == header.end()) return protected_ids;
  const auto id = std::ranges::find(header, "id");
  if (id == header.end()) return std::unexpected(std::string(file) + ": no id");
  const auto name_index = static_cast<std::size_t>(label - header.begin());
  const auto id_index = static_cast<std::size_t>(id - header.begin());
  for (std::size_t row = 1; row < records->size(); ++row) {
    const auto& cells = records->at(row).fields;
    if (cells.at(name_index) != "untitled") {
      protected_ids.insert(cells.at(id_index));
    }
  }
  return protected_ids;
}

bool StockPlaceholder(const Content::BladeResult& blade) {
  Content::BladeResult expected{};
  expected.id = blade.id;
  expected.legacy_id = blade.legacy_id;
  expected.damage_type = 1;
  expected.cost_type = 2;
  expected.attack_shape = 1;
  return blade == expected;
}

std::expected<std::set<std::string>, std::string> QuarantineUnused(
    Content::WorkshopRecipeTable& table, const std::string& results,
    Naming& names, WorkshopImport& imported) {
  std::set<std::string> removed;
  if (table.kind != WorkshopKind::kBlade) return removed;
  const auto& layout = Content::LayoutOf(table.kind);
  auto captions = ProtectedCaptions(
      results,
      Content::WorkshopTablePath(table.kind, WorkshopTablePart::kResults));
  if (!captions) return std::unexpected(captions.error());
  auto source = EncodeSydRecipes(table, imported.reserved_bytes);
  if (!source) return std::unexpected(source.error());
  const std::size_t records_at =
      source->size() - table.blades.size() * layout.record_bytes;
  std::set<std::string> used;
  for (const auto& pair : table.pairs) {
    used.insert(pair.first);
    used.insert(pair.second);
    used.insert(pair.result);
  }
  while (table.blades.size() > 1) {
    const auto& last = table.blades.back();
    if (last.id != Content::LegacyWorkshopResultId(layout, last.legacy_id) ||
        !StockPlaceholder(last) || used.contains(last.id) ||
        captions->contains(last.id)) {
      break;
    }
    auto name = names.Item(last.legacy_id);
    if (!name) return std::unexpected(name.error());
    if (*name != "untitled") break;
    const auto bytes = std::span(*source).subspan(
        records_at + last.legacy_id * layout.record_bytes, layout.record_bytes);
    const std::vector<std::byte> record(bytes.begin(), bytes.end());
    const auto [found, added] =
        imported.unused_results.emplace(last.legacy_id, record);
    if (!added && found->second != record) {
      return std::unexpected("conflicting unused recipe import record");
    }
    // The archived record holds its reserved bytes itself.
    std::erase_if(imported.reserved_bytes,
                  [&last](const ReservedRecordByte& entry) {
                    return entry.legacy_id == last.legacy_id;
                  });
    removed.insert(last.id);
    table.blades.pop_back();
  }
  return removed;
}

struct Rewrite {
  std::string text;
  std::size_t cells = 0;
};

std::expected<Rewrite, std::string> RewriteReferences(
    const std::string& text, std::string_view file, const Aliases& aliases,
    std::span<const std::string_view> columns,
    const std::set<std::string>& removed) {
  auto records = Content::ParseCsv(text, file);
  if (!records) return std::unexpected(records.error());
  if (records->empty()) {
    return std::unexpected(std::format("{} has no header", file));
  }
  std::vector<std::size_t> indices;
  const auto& header = records->front().fields;
  for (const auto name : columns) {
    const auto found = std::ranges::find(header, name);
    if (found == header.end()) {
      return std::unexpected(std::format("{}: no column '{}'", file, name));
    }
    indices.push_back(static_cast<std::size_t>(found - header.begin()));
  }
  std::size_t changed = 0;
  std::vector<std::vector<std::string>> rows{header};
  for (std::size_t row = 1; row < records->size(); ++row) {
    auto cells = records->at(row).fields;
    if (removed.contains(cells.at(indices.front()))) {
      ++changed;
      continue;
    }
    for (const auto index : indices) {
      std::string& cell = cells.at(index);
      const auto id = aliases.find(cell);
      if (id == aliases.end()) {
        return std::unexpected(
            Content::CellError(file, records->at(row).line, header.at(index),
                               std::format("unresolved id '{}'", cell)));
      }
      if (cell != id->second) {
        cell = id->second;
        ++changed;
      }
    }
    rows.push_back(std::move(cells));
  }
  if (changed == 0) return Rewrite{.text = text, .cells = 0};
  return Rewrite{.text = Content::FormatCsv(rows), .cells = changed};
}

std::expected<Content::WorkshopTexts, std::string> ReadTables(
    const fs::path& root, WorkshopKind kind) {
  Content::WorkshopTexts texts;
  for (const auto part : Content::kWorkshopTableParts) {
    auto bytes = ReadFileBytes(
        root / Platform::Utf8Path(Content::WorkshopTablePath(kind, part)));
    if (!bytes) return std::unexpected(bytes.error());
    if (bytes->empty()) {
      return std::unexpected(
          std::format("{} is empty", Content::WorkshopTablePath(kind, part)));
    }
    texts.at(static_cast<std::size_t>(part)) = std::string(
        reinterpret_cast<const char*>(bytes->data()), bytes->size());
  }
  return texts;
}

std::expected<Rewrite, std::string> NormalizeEnums(
    const Content::TableSpec& spec, const std::string& text,
    std::string_view file) {
  auto normalized = Content::NormalizeTableText(spec, text, file);
  if (!normalized) return std::unexpected(normalized.error());
  return Rewrite{.text = std::move(normalized->text),
                 .cells = normalized->named_values};
}

using PendingFiles = std::vector<std::pair<std::string, std::string>>;

std::expected<bool, std::string> RecordImports(
    const fs::path& root, Content::ConversionManifest& manifest) {
  bool changed = false;
  for (auto& output : manifest.outputs) {
    if (output.converter != Content::kSydRecipeConverterName) continue;
    const auto kind = WorkshopKindForDiscPath(output.source);
    if (!kind) continue;
    const auto path = WorkshopImportPath(*kind);
    std::error_code error;
    const bool present =
        fs::is_regular_file(root / Platform::Utf8Path(path), error);
    if (error && error != std::errc::no_such_file_or_directory) {
      return std::unexpected(std::format("{}: {}", path, error.message()));
    }
    if (present &&
        std::ranges::find(output.files, path) == output.files.end()) {
      output.files.push_back(path);
      changed = true;
    }
  }
  if (changed) {
    if (auto written = WriteConversionManifest(root, manifest); !written) {
      return std::unexpected(written.error());
    }
  }
  return changed;
}

std::optional<std::uint32_t> WholeNumber(std::string_view text,
                                         std::uint32_t max) {
  std::uint32_t value = 0;
  const char* const first = text.data();
  const char* const last = first + text.size();
  const auto parsed = std::from_chars(first, last, value);
  if (text.empty() || parsed.ec != std::errc() || parsed.ptr != last ||
      value > max) {
    return std::nullopt;
  }
  return value;
}

// The reserved record byte a former raw.byte<N> results column held.
std::optional<std::uint8_t> RawColumnOffset(WorkshopKind kind,
                                            std::string_view column) {
  if (!column.starts_with(kRawColumnPrefix)) return std::nullopt;
  for (const std::uint8_t offset : ReservedRecordOffsets(kind)) {
    if (column.substr(kRawColumnPrefix.size()) == std::to_string(offset)) {
      return offset;
    }
  }
  return std::nullopt;
}

using RawColumns = std::vector<std::pair<std::size_t, std::uint8_t>>;

std::expected<std::size_t, std::string> MoveRawCells(
    const Content::CsvRecord& record, const std::vector<std::string>& header,
    std::size_t legacy_index, const RawColumns& raw, std::string_view path,
    WorkshopImport& imported) {
  const auto legacy = WholeNumber(record.fields.at(legacy_index), UINT8_MAX);
  if (!legacy) {
    return std::unexpected(Content::CellError(
        path, record.line, header.at(legacy_index), "must be a record number"));
  }
  std::size_t moved = 0;
  for (const auto& [index, offset] : raw) {
    const auto value = WholeNumber(record.fields.at(index), UINT8_MAX);
    if (!value) {
      return std::unexpected(Content::CellError(
          path, record.line, header.at(index), "must be a byte value 0-255"));
    }
    if (*value == 0) continue;
    const ReservedRecordByte entry{.legacy_id = *legacy,
                                   .offset = offset,
                                   .value = static_cast<std::uint8_t>(*value)};
    const auto existing = std::ranges::find_if(
        imported.reserved_bytes, [&entry](const ReservedRecordByte& other) {
          return SameByte(entry, other);
        });
    if (existing != imported.reserved_bytes.end()) {
      if (existing->value == entry.value) continue;
      return std::unexpected(
          Content::CellError(path, record.line, header.at(index),
                             "differs from the import file's reserved byte"));
    }
    imported.reserved_bytes.push_back(entry);
    ++moved;
  }
  return moved;
}

// Moves the non-zero values of former raw.byte<N> results columns (record
// bytes no reader uses) to the import's reserved bytes and drops the columns.
std::expected<std::size_t, std::string> MoveRawColumns(
    WorkshopKind kind, std::string& results, WorkshopImport& imported) {
  const std::string path =
      Content::WorkshopTablePath(kind, WorkshopTablePart::kResults);
  auto records = Content::ParseCsv(results, path);
  if (!records) return std::unexpected(records.error());
  if (records->empty()) return 0;
  const std::vector<std::string>& header = records->front().fields;
  RawColumns raw;
  for (std::size_t index = 0; index < header.size(); ++index) {
    if (const auto offset = RawColumnOffset(kind, header[index])) {
      raw.emplace_back(index, *offset);
    }
  }
  if (raw.empty()) return 0;
  const auto legacy = std::ranges::find(header, "legacy_id");
  if (legacy == header.end()) {
    return std::unexpected(path + ": no legacy_id column");
  }
  const auto legacy_index = static_cast<std::size_t>(legacy - header.begin());
  const auto is_raw = [&raw](std::size_t index) {
    return std::ranges::any_of(
        raw, [index](const auto& column) { return column.first == index; });
  };
  std::size_t moved = 0;
  std::vector<std::vector<std::string>> rows;
  for (std::size_t row = 0; row < records->size(); ++row) {
    const Content::CsvRecord& record = records->at(row);
    if (record.fields.size() != header.size()) {
      return std::unexpected(
          std::format("{}:{}: has {} fields; the header has {}", path,
                      record.line, record.fields.size(), header.size()));
    }
    if (row > 0) {
      auto added =
          MoveRawCells(record, header, legacy_index, raw, path, imported);
      if (!added) return std::unexpected(added.error());
      moved += *added;
    }
    std::vector<std::string> cells;
    for (std::size_t index = 0; index < record.fields.size(); ++index) {
      if (!is_raw(index)) cells.push_back(record.fields[index]);
    }
    rows.push_back(std::move(cells));
  }
  results = Content::FormatCsv(rows);
  std::ranges::sort(imported.reserved_bytes);
  return moved;
}

// Queues the import file when its provenance is needed or it exists and its
// text differs (an older version or changed rows).
void QueueImport(WorkshopKind kind, const StoredImport& stored,
                 PendingFiles& files) {
  const bool needed = !stored.rows.unused_results.empty() ||
                      !stored.rows.reserved_bytes.empty();
  if (!needed && !stored.text) return;
  std::string text = FormatWorkshopImport(kind, stored.rows);
  if (stored.text != text) {
    files.emplace_back(WorkshopImportPath(kind), std::move(text));
  }
}

// Queues each table schema that is missing or differs from its spec.
Check QueueSchemas(const fs::path& root, WorkshopKind kind,
                   PendingFiles& files) {
  for (const auto part : Content::kWorkshopTableParts) {
    const std::string path = Content::WorkshopSchemaPath(kind, part);
    std::string expected =
        Content::FormatTableSchema(Content::WorkshopTableSpec(kind, part));
    const fs::path file = root / Platform::Utf8Path(path);
    std::error_code error;
    const bool present = fs::is_regular_file(file, error);
    if (error && error != std::errc::no_such_file_or_directory) {
      return std::unexpected(std::format("{}: {}", path, error.message()));
    }
    if (present) {
      auto bytes = ReadFileBytes(file);
      if (!bytes) return std::unexpected(bytes.error());
      if (std::string_view(reinterpret_cast<const char*>(bytes->data()),
                           bytes->size()) == expected) {
        continue;
      }
    }
    files.emplace_back(path, std::move(expected));
  }
  return {};
}

// Queues each table whose renamed references or named values differ from
// the file on disk.
Check QueueTables(WorkshopKind kind, const Content::WorkshopTexts& texts,
                  const Content::WorkshopTexts& on_disk, const Aliases& aliases,
                  const std::set<std::string>& removed, PendingFiles& files,
                  WorkshopAuthoringReport& report) {
  constexpr std::array<std::string_view, 1> kResultColumns{"id"};
  constexpr std::array<std::string_view, 3> kPairColumns{"first", "second",
                                                         "result"};
  const std::set<std::string> no_removed;
  for (const auto part : Content::kWorkshopTableParts) {
    const std::string path = Content::WorkshopTablePath(kind, part);
    const auto index = static_cast<std::size_t>(part);
    std::string candidate = texts.at(index);
    if (part != WorkshopTablePart::kMaterials) {
      auto rewritten = RewriteReferences(
          candidate, path, aliases,
          part == WorkshopTablePart::kResults
              ? std::span<const std::string_view>(kResultColumns)
              : std::span<const std::string_view>(kPairColumns),
          part == WorkshopTablePart::kResults ? removed : no_removed);
      if (!rewritten) return std::unexpected(rewritten.error());
      if (part == WorkshopTablePart::kPairs) {
        report.references += rewritten->cells;
      }
      candidate = std::move(rewritten->text);
    }
    auto normalized =
        NormalizeEnums(Content::WorkshopTableSpec(kind, part), candidate, path);
    if (!normalized) return std::unexpected(normalized.error());
    report.named_values += normalized->cells;
    if (normalized->text != on_disk.at(index)) {
      files.emplace_back(path, std::move(normalized->text));
    }
  }
  return {};
}

Check UpgradeTables(const fs::path& root, WorkshopKind kind, Naming& names,
                    Content::WorkshopRecipeCatalog& catalog,
                    PendingFiles& files, WorkshopAuthoringReport& report) {
  auto texts = ReadTables(root, kind);
  if (!texts) return std::unexpected(texts.error());
  const Content::WorkshopTexts on_disk = *texts;
  auto imported = ReadImport(root, kind);
  if (!imported) return std::unexpected(imported.error());
  auto& results =
      texts->at(static_cast<std::size_t>(WorkshopTablePart::kResults));
  auto moved = MoveRawColumns(kind, results, imported->rows);
  if (!moved) return std::unexpected(moved.error());
  report.reserved += *moved;
  auto table = Content::ParseWorkshopTables(kind, *texts);
  if (!table) return std::unexpected(table.error());
  auto original = WithImport(*table, imported->rows);
  if (!original) return std::unexpected(original.error());
  auto removed = QuarantineUnused(*table, results, names, imported->rows);
  if (!removed) return std::unexpected(removed.error());
  report.quarantined += removed->size();
  // Save import-only bytes before removing their rows from authoring files.
  QueueImport(kind, *imported, files);
  Aliases aliases;
  auto named = kind == WorkshopKind::kBlade
                   ? NameRecords(table->blades, kind, names, aliases, report)
                   : NameRecords(table->pieces, kind, names, aliases, report);
  if (!named) return named;
  if (auto pairs = RenamePairs(*table, aliases); !pairs) {
    return pairs;
  }
  auto rebuilt = WithImport(*table, imported->rows);
  if (!rebuilt) return std::unexpected(rebuilt.error());
  if (*original != *rebuilt) {
    return std::unexpected(
        std::format("{}: identity migration changed recipe data",
                    Content::LayoutOf(kind).source));
  }
  if (auto queued =
          QueueTables(kind, *texts, on_disk, aliases, *removed, files, report);
      !queued) {
    return queued;
  }
  if (auto schemas = QueueSchemas(root, kind, files); !schemas) {
    return schemas;
  }
  catalog.tables.at(static_cast<std::size_t>(kind)) = std::move(*table);
  return {};
}

}  // namespace

std::string WorkshopImportPath(WorkshopKind kind) {
  return std::format("catalogs/menu/import/combination-{}.json",
                     Content::LayoutOf(kind).name);
}

std::expected<std::vector<std::byte>, std::string> EncodeWorkshopAuthoring(
    const fs::path& root, const Content::WorkshopRecipeTable& table) {
  auto imported = ReadImport(root, table.kind);
  if (!imported) return std::unexpected(imported.error());
  return WithImport(table, imported->rows);
}

std::string FormatWorkshopImport(WorkshopKind kind,
                                 const WorkshopImport& imported) {
  Content::JsonDocument records = Content::JsonDocument::array();
  for (const auto& [legacy, bytes] : imported.unused_results) {
    std::string hex;
    for (const auto byte : bytes) {
      hex += std::format("{:02x}", std::to_integer<unsigned>(byte));
    }
    records.push_back({{"legacy_id", legacy}, {"bytes", std::move(hex)}});
  }
  Content::JsonDocument reserved = Content::JsonDocument::array();
  for (const ReservedRecordByte& entry : imported.reserved_bytes) {
    reserved.push_back({{"legacy_id", entry.legacy_id},
                        {"byte", entry.offset},
                        {"value", entry.value}});
  }
  const auto& layout = Content::LayoutOf(kind);
  return Content::FormatJsonText({{"schema", kImportSchema},
                                  {"schema_version", kImportVersion},
                                  {"source", layout.source},
                                  {"record_bytes", layout.record_bytes},
                                  {"unused_results", std::move(records)},
                                  {"reserved_bytes", std::move(reserved)}}) +
         "\n";
}

Check CheckWorkshopImports(const fs::path& root,
                           const Content::ConversionManifest& manifest) {
  for (const auto& output : manifest.outputs) {
    if (output.converter != Content::kSydRecipeConverterName) continue;
    const auto kind = WorkshopKindForDiscPath(output.source);
    if (!kind) continue;
    const auto path = WorkshopImportPath(*kind);
    if (std::ranges::find(output.files, path) == output.files.end()) continue;
    std::error_code error;
    if (!fs::is_regular_file(root / Platform::Utf8Path(path), error)) {
      return std::unexpected(std::format(
          "{}: missing import provenance; restore it before upgrading "
          "authoring, or explicitly reconvert with --force",
          path));
    }
  }
  return {};
}

std::expected<WorkshopAuthoringReport, std::string> UpgradeWorkshopAuthoring(
    const fs::path& root) {
  std::error_code root_error;
  if (!fs::is_directory(root, root_error)) {
    return std::unexpected(std::format(
        "{}: {}", Platform::PathText(root),
        root_error ? root_error.message() : "not an asset directory"));
  }
  auto manifest = Content::LoadConversionManifest(root);
  if (!manifest) return std::unexpected(manifest.error());
  if (manifest->has_value()) {
    if (auto imports = CheckWorkshopImports(root, **manifest); !imports) {
      return std::unexpected(imports.error());
    }
  }
  Naming names{.root = root, .items = std::nullopt};
  Content::WorkshopRecipeCatalog catalog;
  PendingFiles files;
  WorkshopAuthoringReport report;
  for (const auto kind : Content::kWorkshopKinds) {
    const auto path = root / Platform::Utf8Path(Content::WorkshopTablePath(
                                 kind, WorkshopTablePart::kResults));
    std::error_code error;
    const bool present = fs::is_regular_file(path, error);
    if (error && error != std::errc::no_such_file_or_directory) {
      return std::unexpected(
          std::format("{}: {}", Platform::PathText(path), error.message()));
    }
    if (!present) continue;
    if (auto upgraded =
            UpgradeTables(root, kind, names, catalog, files, report);
        !upgraded) {
      return std::unexpected(upgraded.error());
    }
  }
  if (auto shared = Content::CheckSharedWorkshopResults(catalog); !shared) {
    return std::unexpected(shared.error());
  }
  std::vector<OutputBytes> outputs;
  for (const auto& [path, text] : files) {
    outputs.push_back({.name = path, .bytes = std::as_bytes(std::span(text))});
  }
  if (!outputs.empty()) {
    if (auto written = WriteOutputFiles(root, outputs); !written) {
      return std::unexpected(written.error());
    }
  }
  report.written = files.size();
  if (manifest->has_value()) {
    auto recorded = RecordImports(root, **manifest);
    if (!recorded) return std::unexpected(recorded.error());
    if (*recorded) ++report.written;
  }
  return report;
}

}  // namespace VS::Extract
