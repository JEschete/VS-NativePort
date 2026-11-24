#include "Extract/MaterialConversion.hpp"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <format>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "Content/JsonText.hpp"
#include "Extract/ConverterBatch.hpp"
#include "Platform/Paths.hpp"

namespace VS::Extract {
namespace {
constexpr std::string_view kImportSchema = "vs.material-import";
constexpr std::size_t kAttackReserved = 13;
constexpr std::size_t kDefenseReserved = 21;
constexpr std::size_t kDpOffset = 22;
constexpr std::size_t kStrengthOffset = 24;
constexpr std::size_t kIntelligenceOffset = 26;
constexpr std::size_t kAgilityOffset = 28;
constexpr std::size_t kAttackOffset = Content::kMaterialClassCount;
constexpr std::size_t kDefenseOffset = kAttackReserved + 1;
constexpr unsigned kByteBits = 8;

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
    const std::array<std::byte, 2>& reserved) {
  for (const auto value : material.classes) {
    if (auto result = Append(bytes, value, 1, true, "class"); !result) {
      return result;
    }
  }
  for (const auto value : material.attack_affinities) {
    if (auto result = Append(bytes, value, 1, true, "attack affinity");
        !result) {
      return result;
    }
  }
  bytes.push_back(reserved[0]);
  for (const auto value : material.defense_affinities) {
    if (auto result = Append(bytes, value, 1, true, "defense affinity");
        !result) {
      return result;
    }
  }
  bytes.push_back(reserved[1]);
  if (auto result = Append(bytes, material.max_dp, 2, false, "max_dp");
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

std::expected<void, std::string> WriteMaterials(
    const std::filesystem::path& root, const ImportedMaterials& imported,
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
      FormatMaterialProvenance(imported.reserved, hash);
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
    material.id = Content::kMaterialIds[slot];
    material.legacy_id = static_cast<std::uint32_t>(slot);
    std::ranges::transform(record.first(Content::kMaterialClassCount),
                           material.classes.begin(), SignedByte);
    std::ranges::transform(
        record.subspan(kAttackOffset, Content::kMaterialAffinityCount),
        material.attack_affinities.begin(), SignedByte);
    std::ranges::transform(
        record.subspan(kDefenseOffset, Content::kMaterialAffinityCount),
        material.defense_affinities.begin(), SignedByte);
    imported.reserved[slot] = {record[kAttackReserved],
                               record[kDefenseReserved]};
    material.max_dp = Word(record, kDpOffset);
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
    if (auto result = EncodeRecord(bytes, material, imported.reserved[slot]);
        !result) {
      return std::unexpected(material.id + ": " + result.error());
    }
  }
  return bytes;
}

std::string FormatMaterialProvenance(const MaterialReserved& reserved,
                                     std::string_view source_sha256) {
  Content::JsonDocument rows = Content::JsonDocument::array();
  for (std::size_t slot = 0; slot < reserved.size(); ++slot) {
    rows.push_back(
        {{"id", Content::kMaterialIds[slot]},
         {"attack_padding", std::to_integer<unsigned>(reserved[slot][0])},
         {"defense_padding", std::to_integer<unsigned>(reserved[slot][1])}});
  }
  return Content::FormatJsonText({{"schema", kImportSchema},
                                  {"schema_version", 1},
                                  {"source", Content::kMaterialSource},
                                  {"source_sha256", source_sha256},
                                  {"byte_offset", kMaterialSourceOffset},
                                  {"record_bytes", kMaterialRecordBytes},
                                  {"reserved", std::move(rows)}}) +
         "\n";
}

std::expected<MaterialReserved, std::string> ParseMaterialProvenance(
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
  auto source =
      Content::JsonSourceOf(members, kMaterialImportPath, kImportSchema, 1);
  if (!source) {
    return std::unexpected(source.error());
  }
  auto offset = members.Unsigned("byte_offset", kMaterialSourceOffset);
  auto width = members.Unsigned("record_bytes", kMaterialRecordBytes);
  auto rows = members.Get("reserved");
  if (source->source != Content::kMaterialSource || !offset ||
      *offset != kMaterialSourceOffset || !width ||
      *width != kMaterialRecordBytes || !rows || !(*rows)->is_array() ||
      (*rows)->size() != Content::kMaterialIds.size()) {
    return std::unexpected(
        "materials import provenance: incompatible source layout");
  }
  MaterialReserved reserved{};
  for (std::size_t slot = 0; slot < reserved.size(); ++slot) {
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
    reserved[slot] = {static_cast<std::byte>(*attack),
                      static_cast<std::byte>(*defense)};
  }
  return reserved;
}

std::expected<TableConversionReport, std::string> ConvertDiscMaterials(
    const std::filesystem::path& root, bool force) {
  auto start = StartRun(root);
  if (!start) {
    return std::unexpected(start.error());
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
