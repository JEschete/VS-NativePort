#include "Extract/MaterialConversion.hpp"

#include <algorithm>
#include <bit>
#include <charconv>
#include <cstdint>
#include <format>
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "Content/CatalogTable.hpp"
#include "Content/ConversionManifest.hpp"
#include "Content/JsonText.hpp"
#include "Extract/ConversionManifestWriter.hpp"
#include "Extract/ConverterBatch.hpp"
#include "Platform/Paths.hpp"

namespace VS::Extract {
namespace {
namespace fs = std::filesystem;

constexpr std::string_view kImportSchema = "vs.material-import";
constexpr std::uint32_t kImportVersion = 2;
// Version 1 tables held the archived fields as columns.
constexpr std::uint32_t kColumnsVersion = 1;
constexpr std::size_t kAttackReserved = 13;
constexpr std::size_t kDefenseReserved = 21;
constexpr std::size_t kDpOffset = 22;
constexpr std::size_t kStrengthOffset = 24;
constexpr std::size_t kIntelligenceOffset = 26;
constexpr std::size_t kAgilityOffset = 28;
constexpr std::size_t kAttackOffset = kMaterialClassCount;
constexpr std::size_t kDefenseOffset = kAttackReserved + 1;
constexpr unsigned kByteBits = 8;

struct ArchivedField {
  std::string_view name;
  std::int64_t minimum = 0;
  std::int64_t maximum = 0;
  bool table_column = true;  // a version 1 column; the paddings never were
};

constexpr ArchivedField Modifier(std::string_view name) {
  return {.name = name,
          .minimum = std::numeric_limits<std::int32_t>::min(),
          .maximum = std::numeric_limits<std::int32_t>::max()};
}

constexpr ArchivedField Padding(std::string_view name) {
  return {
      .name = name, .minimum = 0, .maximum = UINT8_MAX, .table_column = false};
}

constexpr ArchivedField Durability(std::string_view name) {
  return {.name = name,
          .minimum = 0,
          .maximum = std::numeric_limits<std::uint32_t>::max()};
}

// decomp: MaterialModifiers (src/SLUS_010.40/modifiers.h:37)
// In record byte order: six class points, the attack affinities and their
// padding byte, the defense affinities and theirs, then the base DP. Air is
// the source's wind.
constexpr std::array kArchivedFields{
    Modifier("class_human"),
    Modifier("class_beast"),
    Modifier("class_undead"),
    Modifier("class_phantom"),
    Modifier("class_dragon"),
    Modifier("class_evil"),
    Modifier("attack_affinity_physical"),
    Modifier("attack_affinity_air"),
    Modifier("attack_affinity_fire"),
    Modifier("attack_affinity_earth"),
    Modifier("attack_affinity_water"),
    Modifier("attack_affinity_light"),
    Modifier("attack_affinity_dark"),
    Padding("attack_padding"),
    Modifier("defense_affinity_physical"),
    Modifier("defense_affinity_air"),
    Modifier("defense_affinity_fire"),
    Modifier("defense_affinity_earth"),
    Modifier("defense_affinity_water"),
    Modifier("defense_affinity_light"),
    Modifier("defense_affinity_dark"),
    Padding("defense_padding"),
    Durability("max_dp"),
};
static_assert(kArchivedFields.size() ==
              kMaterialClassCount + (2 * (kMaterialAffinityCount + 1)) + 1);
using ArchivedValues = std::array<std::int64_t, kArchivedFields.size()>;

ArchivedValues Flatten(const ArchivedMaterial& material) {
  ArchivedValues values{};
  auto out = std::ranges::copy(material.classes, values.begin()).out;
  out = std::ranges::copy(material.attack_affinities, out).out;
  *out++ = std::to_integer<std::int64_t>(material.padding[0]);
  out = std::ranges::copy(material.defense_affinities, out).out;
  *out++ = std::to_integer<std::int64_t>(material.padding[1]);
  *out = material.max_dp;
  return values;
}

// `values` are within their fields' ranges.
ArchivedMaterial Unflatten(const ArchivedValues& values) {
  ArchivedMaterial material;
  auto in = values.begin();
  for (auto& value : material.classes) {
    value = static_cast<std::int32_t>(*in++);
  }
  for (auto& value : material.attack_affinities) {
    value = static_cast<std::int32_t>(*in++);
  }
  material.padding[0] = static_cast<std::byte>(*in++);
  for (auto& value : material.defense_affinities) {
    value = static_cast<std::int32_t>(*in++);
  }
  material.padding[1] = static_cast<std::byte>(*in++);
  material.max_dp = static_cast<std::uint32_t>(*in);
  return material;
}

std::uint16_t Word(std::span<const std::byte> record, std::size_t at) {
  return static_cast<std::uint16_t>(
      std::to_integer<unsigned>(record[at]) |
      (std::to_integer<unsigned>(record[at + 1]) << kByteBits));
}

std::int32_t SignedByte(std::byte value) {
  return std::bit_cast<std::int8_t>(value);
}

std::expected<void, std::string> Append(std::vector<std::byte>& bytes,
                                        std::int64_t value, unsigned width,
                                        bool is_signed,
                                        std::string_view field) {
  const unsigned bits = width * kByteBits;
  const std::int64_t maximum =
      (std::int64_t{1} << (bits - (is_signed ? 1 : 0))) - 1;
  const std::int64_t minimum = is_signed ? -maximum - 1 : 0;
  if (value < minimum || value > maximum) {
    return std::unexpected(std::format(
        "{} cannot be represented by the offline legacy material encoder",
        field));
  }
  auto word = static_cast<std::uint64_t>(value);
  for (unsigned byte = 0; byte < width; ++byte) {
    bytes.push_back(static_cast<std::byte>(word & UINT8_MAX));
    word >>= kByteBits;
  }
  return {};
}

std::expected<void, std::string> EncodeRecord(
    std::vector<std::byte>& bytes, const Content::MaterialDefinition& material,
    const ArchivedMaterial& archived) {
  for (const auto value : archived.classes) {
    if (auto result = Append(bytes, value, 1, true, "class"); !result) {
      return result;
    }
  }
  for (const auto value : archived.attack_affinities) {
    if (auto result = Append(bytes, value, 1, true, "attack affinity");
        !result) {
      return result;
    }
  }
  bytes.push_back(archived.padding[0]);
  for (const auto value : archived.defense_affinities) {
    if (auto result = Append(bytes, value, 1, true, "defense affinity");
        !result) {
      return result;
    }
  }
  bytes.push_back(archived.padding[1]);
  if (auto result = Append(bytes, archived.max_dp, 2, false, "max_dp");
      !result) {
    return result;
  }
  if (auto result = Append(bytes, material.strength, 2, true, "strength");
      !result) {
    return result;
  }
  if (auto result =
          Append(bytes, material.intelligence, 2, true, "intelligence");
      !result) {
    return result;
  }
  return Append(bytes, material.agility, 2, true, "agility");
}

Content::JsonDocument ProvenanceDocument(const ArchivedMaterials& archived,
                                         std::string_view source_sha256) {
  auto rows = Content::JsonDocument::array();
  for (std::size_t slot = 0; slot < archived.size(); ++slot) {
    auto row = Content::JsonDocument::object();
    row["id"] = Content::kMaterialIds[slot];
    const ArchivedValues values = Flatten(archived[slot]);
    for (std::size_t field = 0; field < values.size(); ++field) {
      row[std::string(kArchivedFields[field].name)] = values[field];
    }
    rows.push_back(std::move(row));
  }
  return {{"schema", kImportSchema},
          {"schema_version", kImportVersion},
          {"source", Content::kMaterialSource},
          {"source_sha256", source_sha256},
          {"byte_offset", kMaterialSourceOffset},
          {"record_bytes", kMaterialRecordBytes},
          {"original_records", std::move(rows)}};
}

// The document's eight records under `key`, once it describes this table.
std::expected<const Content::JsonDocument*, std::string> SourceRecords(
    const Content::JsonMembers& members, const Content::JsonSource& source,
    std::string_view key) {
  auto offset = members.Unsigned("byte_offset", kMaterialSourceOffset);
  auto width = members.Unsigned("record_bytes", kMaterialRecordBytes);
  auto rows = members.Get(key);
  if (source.source != Content::kMaterialSource || !offset ||
      *offset != kMaterialSourceOffset || !width ||
      *width != kMaterialRecordBytes || !rows || !(*rows)->is_array() ||
      (*rows)->size() != Content::kMaterialIds.size()) {
    return std::unexpected(
        "materials import provenance: incompatible source layout");
  }
  return *rows;
}

bool IsArchivedField(std::string_view name) {
  return std::ranges::find(kArchivedFields, name, &ArchivedField::name) !=
         kArchivedFields.end();
}

std::expected<ArchivedMaterial, std::string> ParseRecord(
    const Content::JsonDocument& row, std::size_t slot) {
  const std::string where = std::format("original_records[{}]", slot);
  if (!row.is_object()) {
    return std::unexpected(where + ": expected an object");
  }
  for (const auto& item : row.items()) {
    const bool label = item.key() == "display_name" && item.value().is_string();
    if (item.key() != "id" && !label && !IsArchivedField(item.key())) {
      return std::unexpected(
          std::format("{}: unknown member '{}'", where, item.key()));
    }
  }
  const Content::JsonMembers members(row, where);
  auto id = members.String("id");
  if (!id || *id != Content::kMaterialIds[slot]) {
    return std::unexpected(std::format("{}: expected id '{}'", where,
                                       Content::kMaterialIds[slot]));
  }
  ArchivedValues values{};
  for (std::size_t field = 0; field < values.size(); ++field) {
    const ArchivedField& spec = kArchivedFields[field];
    auto value = members.Integer(spec.name, spec.minimum, spec.maximum);
    if (!value) {
      return std::unexpected(value.error());
    }
    values[field] = *value;
  }
  return Unflatten(values);
}

// A version 1 provenance: the paddings, beside the table's columns.
struct ColumnsProvenance {
  std::optional<std::string> display_name;
  std::string source_sha256;
  std::array<MaterialPadding, kImportedMaterialCount> padding{};
};

std::expected<ColumnsProvenance, std::string> ParseColumnsProvenance(
    std::span<const std::byte> text) {
  auto json = Content::ParseJsonText(text, kMaterialImportPath);
  if (!json) {
    return std::unexpected(json.error());
  }
  const Content::JsonMembers members(*json, std::string(kMaterialImportPath));
  if (auto only =
          members.Only({"schema", "schema_version", "source", "source_sha256",
                        "byte_offset", "record_bytes", "reserved"});
      !only) {
    return std::unexpected(only.error());
  }
  auto source = Content::JsonSourceOf(members, kMaterialImportPath,
                                      kImportSchema, kColumnsVersion);
  if (!source) {
    return std::unexpected(source.error());
  }
  auto rows = SourceRecords(members, *source, "reserved");
  if (!rows) {
    return std::unexpected(rows.error());
  }
  ColumnsProvenance provenance{.display_name = std::nullopt,
                               .source_sha256 = source->source_sha256};
  if (members.Find("display_name") != nullptr) {
    auto name = members.String("display_name");
    if (!name) {
      return std::unexpected(name.error());
    }
    provenance.display_name = std::move(*name);
  }
  for (std::size_t slot = 0; slot < provenance.padding.size(); ++slot) {
    const Content::JsonMembers row((**rows)[slot],
                                   std::format("reserved[{}]", slot));
    if (auto only = row.Only({"id", "attack_padding", "defense_padding"});
        !only) {
      return std::unexpected(only.error());
    }
    auto id = row.String("id");
    auto attack = row.Unsigned("attack_padding", UINT8_MAX);
    auto defense = row.Unsigned("defense_padding", UINT8_MAX);
    if (!id || *id != Content::kMaterialIds[slot] || !attack || !defense) {
      return std::unexpected(std::format(
          "reserved[{}]: invalid material identity or padding", slot));
    }
    provenance.padding[slot] = {static_cast<std::byte>(*attack),
                                static_cast<std::byte>(*defense)};
  }
  return provenance;
}

std::optional<std::int64_t> WholeNumber(std::string_view cell,
                                        const ArchivedField& field) {
  std::int64_t value = 0;
  const char* const first = cell.data();
  const char* const last = first + cell.size();
  const auto parsed = std::from_chars(first, last, value);
  if (cell.empty() || parsed.ec != std::errc() || parsed.ptr != last ||
      value < field.minimum || value > field.maximum) {
    return std::nullopt;
  }
  return value;
}

std::optional<std::size_t> ColumnOf(const std::vector<std::string>& header,
                                    std::string_view name) {
  const auto found = std::ranges::find(header, name);
  if (found == header.end()) {
    return std::nullopt;
  }
  return static_cast<std::size_t>(found - header.begin());
}

// The archived fields of a version 1 table's rows, cells as authored, and
// the table without their columns.
struct SplitTable {
  ArchivedMaterials archived{};
  std::string table;
  std::size_t archived_columns = 0;
};

std::expected<SplitTable, std::string> SplitColumns(
    std::string_view text, const ColumnsProvenance& provenance) {
  const std::string_view file = Content::kMaterialTablePath;
  auto records = Content::ParseCsv(text, file);
  if (!records) {
    return std::unexpected(records.error());
  }
  if (records->empty()) {
    return std::unexpected(std::format("{}: missing header", file));
  }
  const std::vector<std::string>& header = records->front().fields;
  const auto id_column = ColumnOf(header, "id");
  if (!id_column) {
    return std::unexpected(std::format("{}: missing column 'id'", file));
  }
  std::array<std::size_t, kArchivedFields.size()> columns{};
  std::vector<std::size_t> removed;
  for (std::size_t field = 0; field < kArchivedFields.size(); ++field) {
    if (!kArchivedFields[field].table_column) {
      continue;
    }
    const auto column = ColumnOf(header, kArchivedFields[field].name);
    if (!column) {
      return std::unexpected(std::format("{}: version 1 column '{}' missing",
                                         file, kArchivedFields[field].name));
    }
    columns[field] = *column;
    removed.push_back(*column);
  }
  SplitTable split;
  std::array<bool, kImportedMaterialCount> seen{};
  for (const Content::CsvRecord& row : std::span(*records).subspan(1)) {
    if (row.fields.size() != header.size()) {
      return std::unexpected(std::format("{}:{}: expected {} fields", file,
                                         row.line, header.size()));
    }
    const auto material =
        std::ranges::find(Content::kMaterialIds, row.fields[*id_column]);
    if (material == Content::kMaterialIds.end()) {
      return std::unexpected(
          Content::CellError(file, row.line, "id", "unsupported material"));
    }
    const auto slot =
        static_cast<std::size_t>(material - Content::kMaterialIds.begin());
    if (seen[slot]) {
      return std::unexpected(
          Content::CellError(file, row.line, "id", "duplicate material"));
    }
    seen[slot] = true;
    ArchivedValues values =
        Flatten(ArchivedMaterial{.padding = provenance.padding[slot]});
    for (std::size_t field = 0; field < values.size(); ++field) {
      const ArchivedField& spec = kArchivedFields[field];
      if (!spec.table_column) {
        continue;
      }
      const auto value = WholeNumber(row.fields[columns[field]], spec);
      if (!value) {
        return std::unexpected(
            Content::CellError(file, row.line, spec.name,
                               std::format("expected an integer from {} to {}",
                                           spec.minimum, spec.maximum)));
      }
      values[field] = *value;
    }
    split.archived[slot] = Unflatten(values);
  }
  if (!std::ranges::all_of(seen, std::identity{})) {
    return std::unexpected(std::format("{}: missing a material row", file));
  }
  std::ranges::sort(removed, std::greater{});
  std::vector<std::vector<std::string>> kept;
  kept.reserve(records->size());
  for (Content::CsvRecord& record : *records) {
    for (const std::size_t column : removed) {
      record.fields.erase(record.fields.begin() +
                          static_cast<std::ptrdiff_t>(column));
    }
    kept.push_back(std::move(record.fields));
  }
  split.table = Content::FormatCsv(kept);
  split.archived_columns = removed.size();
  return split;
}

std::expected<void, std::string> WriteMaterials(
    const fs::path& root, const ImportedMaterials& imported,
    std::string_view hash) {
  const std::string csv = Content::FormatMaterials(imported.definitions);
  auto parsed = Content::ParseMaterials(csv);
  if (!parsed) {
    return std::unexpected(parsed.error());
  }
  if (*parsed != imported.definitions) {
    return std::unexpected("material CSV changed imported definitions");
  }
  const std::string schema =
      Content::FormatTableSchema(Content::MaterialTableSpec());
  const std::string provenance =
      FormatMaterialProvenance(imported.archived, hash);
  const std::array files{
      OutputBytes{Content::kMaterialTablePath, std::as_bytes(std::span(csv))},
      OutputBytes{Content::kMaterialSchemaPath,
                  std::as_bytes(std::span(schema))},
      OutputBytes{kMaterialImportPath, std::as_bytes(std::span(provenance))}};
  return WriteOutputFiles(root, files);
}

}  // namespace

std::expected<ImportedMaterials, std::string> DecodeMaterials(
    std::span<const std::byte> records) {
  if (records.size() != kMaterialImportBytes) {
    return std::unexpected(
        "SLUS_010.40 materials: expected eight 30-byte records");
  }
  ImportedMaterials imported;
  for (std::size_t slot = 0; slot < imported.definitions.size(); ++slot) {
    const auto record =
        records.subspan(slot * kMaterialRecordBytes, kMaterialRecordBytes);
    auto& material = imported.definitions[slot];
    auto& archived = imported.archived[slot];
    material.id = Content::kMaterialIds[slot];
    material.legacy_id = static_cast<std::uint32_t>(slot);
    std::ranges::transform(record.first(kMaterialClassCount),
                           archived.classes.begin(), SignedByte);
    std::ranges::transform(
        record.subspan(kAttackOffset, kMaterialAffinityCount),
        archived.attack_affinities.begin(), SignedByte);
    std::ranges::transform(
        record.subspan(kDefenseOffset, kMaterialAffinityCount),
        archived.defense_affinities.begin(), SignedByte);
    archived.padding = {record[kAttackReserved], record[kDefenseReserved]};
    archived.max_dp = Word(record, kDpOffset);
    material.strength =
        std::bit_cast<std::int16_t>(Word(record, kStrengthOffset));
    material.intelligence =
        std::bit_cast<std::int16_t>(Word(record, kIntelligenceOffset));
    material.agility =
        std::bit_cast<std::int16_t>(Word(record, kAgilityOffset));
  }
  return imported;
}

std::expected<std::vector<std::byte>, std::string> EncodeMaterials(
    const ImportedMaterials& imported) {
  std::vector<std::byte> bytes;
  for (std::size_t slot = 0; slot < imported.definitions.size(); ++slot) {
    const auto& material = imported.definitions[slot];
    if (material.id != Content::kMaterialIds[slot] ||
        material.legacy_id != slot) {
      return std::unexpected(
          "offline material encoder: mismatched identity/kind");
    }
    if (auto result = EncodeRecord(bytes, material, imported.archived[slot]);
        !result) {
      return std::unexpected(material.id + ": " + result.error());
    }
  }
  return bytes;
}

std::string FormatMaterialProvenance(const ArchivedMaterials& archived,
                                     std::string_view source_sha256) {
  return Content::FormatJsonText(ProvenanceDocument(archived, source_sha256)) +
         "\n";
}

std::expected<ArchivedMaterials, std::string> ParseMaterialProvenance(
    std::span<const std::byte> text) {
  auto json = Content::ParseJsonText(text, kMaterialImportPath);
  if (!json) {
    return std::unexpected(json.error());
  }
  const Content::JsonMembers members(*json, std::string(kMaterialImportPath));
  if (auto only =
          members.Only({"schema", "schema_version", "source", "source_sha256",
                        "byte_offset", "record_bytes", "original_records"});
      !only) {
    return std::unexpected(only.error());
  }
  auto source = Content::JsonSourceOf(members, kMaterialImportPath,
                                      kImportSchema, kImportVersion);
  if (!source) {
    return std::unexpected(source.error());
  }
  auto rows = SourceRecords(members, *source, "original_records");
  if (!rows) {
    return std::unexpected(rows.error());
  }
  ArchivedMaterials archived{};
  for (std::size_t slot = 0; slot < archived.size(); ++slot) {
    auto record = ParseRecord((**rows)[slot], slot);
    if (!record) {
      return std::unexpected(record.error());
    }
    archived[slot] = *record;
  }
  return archived;
}

std::expected<MaterialAuthoringReport, std::string> UpgradeMaterialAuthoring(
    const fs::path& root) {
  auto manifest = Content::LoadConversionManifest(root);
  if (!manifest) {
    return std::unexpected(manifest.error());
  }
  if (!*manifest) {
    return MaterialAuthoringReport{};
  }
  const auto version =
      (**manifest).converters.find(Content::kMaterialConverterName);
  if (version == (**manifest).converters.end() ||
      version->second == Content::kMaterialConverterVersion) {
    return MaterialAuthoringReport{};
  }
  if (version->second != kColumnsVersion) {
    return std::unexpected(
        "unsupported material table version for authoring migration");
  }
  const fs::path table = root / Platform::Utf8Path(Content::kMaterialTablePath);
  const fs::path import = root / Platform::Utf8Path(kMaterialImportPath);
  std::error_code error;
  const bool has_table = fs::exists(table, error);
  const bool has_import = !error && fs::exists(import, error);
  if (error) {
    return std::unexpected(Platform::PathText(root) + ": " + error.message());
  }
  // Reconversion writes a missing file afresh.
  if (!has_table || !has_import) {
    return MaterialAuthoringReport{};
  }
  auto columns_text = ReadFileBytes(table);
  if (!columns_text) {
    return std::unexpected(columns_text.error());
  }
  auto provenance_text = ReadFileBytes(import);
  if (!provenance_text) {
    return std::unexpected(provenance_text.error());
  }
  auto provenance = ParseColumnsProvenance(*provenance_text);
  if (!provenance) {
    return std::unexpected(provenance.error());
  }
  auto split = SplitColumns(
      std::string_view(reinterpret_cast<const char*>(columns_text->data()),
                       columns_text->size()),
      *provenance);
  if (!split) {
    return std::unexpected(split.error());
  }
  if (auto parsed = Content::ParseMaterials(split->table); !parsed) {
    return std::unexpected(parsed.error());
  }
  Content::JsonDocument document =
      ProvenanceDocument(split->archived, provenance->source_sha256);
  if (provenance->display_name) {
    auto labelled = Content::JsonDocument::object();
    labelled["display_name"] = *provenance->display_name;
    labelled.update(document);
    document = std::move(labelled);
  }
  const std::string archive = Content::FormatJsonText(document) + "\n";
  const std::string schema =
      Content::FormatTableSchema(Content::MaterialTableSpec());
  const std::array files{
      OutputBytes{Content::kMaterialTablePath,
                  std::as_bytes(std::span(split->table))},
      OutputBytes{Content::kMaterialSchemaPath,
                  std::as_bytes(std::span(schema))},
      OutputBytes{kMaterialImportPath, std::as_bytes(std::span(archive))}};
  if (auto written = WriteOutputFiles(root, files); !written) {
    return std::unexpected(written.error());
  }
  version->second = Content::kMaterialConverterVersion;
  if (auto written = WriteConversionManifest(root, **manifest); !written) {
    return std::unexpected(written.error());
  }
  return MaterialAuthoringReport{.archived_columns = split->archived_columns,
                                 .written = files.size() + 1};
}
std::expected<TableConversionReport, std::string> ConvertDiscMaterials(
    const std::filesystem::path& root, bool force) {
  auto start = StartRun(root);
  if (!start) {
    return std::unexpected(start.error());
  }
  const auto version =
      start->previous.converters.find(Content::kMaterialConverterName);
  if (!force && version != start->previous.converters.end() &&
      version->second == kColumnsVersion) {
    if (auto upgraded = UpgradeMaterialAuthoring(root); !upgraded) {
      return std::unexpected(upgraded.error());
    }
    start = StartRun(root);
    if (!start) {
      return std::unexpected(start.error());
    }
  }
  auto batch = StartBatch(start->previous, Content::kMaterialConverterName,
                          Content::kMaterialConverterVersion, force);
  TableConversionReport report;
  const auto entry = std::ranges::find(start->entries, Content::kMaterialSource,
                                       &Content::DataStore::Entry::path);
  if (entry == start->entries.end()) {
    return report;
  }
  if (entry->form != "raw") {
    return std::unexpected("SLUS_010.40 material import requires a raw source");
  }
  auto source = ReadSource(root, Content::kMaterialSource);
  if (!source) {
    return std::unexpected(source.error());
  }
  if (source->bytes.size() < kMaterialSourceOffset + kMaterialImportBytes) {
    return std::unexpected(
        "SLUS_010.40 is too short for the material table at 0x3F5DC");
  }
  Content::ConversionOutput output{
      .id = std::string(Content::kMaterialOutputId),
      .source = source->path,
      .source_sha256 = HashOf(*source),
      .converter = std::string(Content::kMaterialConverterName),
      .files = {},
      .dependencies = {},
      .note =
          "G01905 only: byte offset 0x3F5DC, 8 records x 30 bytes; not the "
          "whole executable.",
      .alias_of = {}};
  if (const auto* kept =
          batch.reuse ? MatchingOutput(start->previous, root, output) : nullptr;
      kept != nullptr) {
    report.unchanged = 1;
    return report;
  }
  const auto records =
      std::span(source->bytes)
          .subspan(kMaterialSourceOffset, kMaterialImportBytes);
  auto imported = DecodeMaterials(records);
  if (!imported) {
    return std::unexpected(imported.error());
  }
  auto encoded = EncodeMaterials(*imported);
  if (!encoded) {
    return std::unexpected(encoded.error());
  }
  if (!std::ranges::equal(*encoded, records)) {
    return std::unexpected("material import did not preserve source bytes");
  }
  output.files = {std::string(Content::kMaterialTablePath),
                  std::string(Content::kMaterialSchemaPath),
                  std::string(kMaterialImportPath)};
  batch.outputs.push_back(output);
  auto removed = PublishBatch(root, start->previous, batch, [&] {
    return WriteMaterials(root, *imported, output.source_sha256);
  });
  if (!removed) {
    return std::unexpected(removed.error());
  }
  report.converted = 1;
  report.removed = *removed;
  return report;
}

}  // namespace VS::Extract
