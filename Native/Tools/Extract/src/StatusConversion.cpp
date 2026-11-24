#include "Extract/StatusConversion.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <string>
#include <vector>

#include "Content/JsonText.hpp"
#include "Extract/ConverterBatch.hpp"

namespace VS::Extract {
namespace {
constexpr std::string_view kSchema = "vs.status-import";
constexpr std::string_view kRulesSchema{
    "catalogs/battle/status-rules.schema.json",
};
constexpr std::string_view kTimingsSchema{
    "catalogs/battle/status-timings.schema.json",
};
constexpr unsigned kByteBits = 8;
constexpr std::size_t kRecordBytes = 16;
// Source record layout: effect word, duration and interval halfwords, then the
// replaces and blocked-by words.
constexpr std::size_t kWordBytes = 4;
constexpr std::size_t kHalfBytes = 2;
constexpr std::size_t kDurationOffset = 4;
constexpr std::size_t kIntervalOffset = 6;
constexpr std::size_t kReplacesOffset = 8;
constexpr std::size_t kBlockedByOffset = 12;
// Legacy rules whose words feed each shared timing role.
constexpr std::size_t kAttributeRule = 5;
constexpr std::size_t kMovementRule = 11;
constexpr std::size_t kParalysisRule = 13;
constexpr std::size_t kPoisonRule = 14;
constexpr std::size_t kRegenerationRule = 17;
constexpr std::size_t kEquipmentRule = 19;

std::uint32_t Read(std::span<const std::byte> bytes, std::size_t offset,
                   std::size_t width) {
  std::uint32_t value = 0;
  for (std::size_t index = 0; index < width; ++index) {
    value |= std::to_integer<std::uint32_t>(bytes[offset + index])
             << (index * kByteBits);
  }
  return value;
}
void Append(std::vector<std::byte>& bytes, std::uint32_t value,
            std::size_t width) {
  for (std::size_t index = 0; index < width; ++index) {
    bytes.push_back(static_cast<std::byte>(value & UINT8_MAX));
    value >>= kByteBits;
  }
}

std::expected<void, std::string> Write(const std::filesystem::path& root,
                                       const ImportedStatus& imported,
                                       std::string_view hash, bool preserve) {
  const auto rules = Content::FormatStatusRules(imported.definitions);
  const auto timings = Content::FormatStatusTimings(imported.definitions);
  const auto rules_schema =
      Content::FormatTableSchema(Content::StatusRulesSpec());
  const auto timings_schema =
      Content::FormatTableSchema(Content::StatusTimingsSpec());
  const auto provenance = FormatStatusProvenance(imported.originals, hash);
  const std::array files{
      OutputBytes{Content::kStatusRulesPath, std::as_bytes(std::span(rules))},
      OutputBytes{Content::kStatusTimingsPath,
                  std::as_bytes(std::span(timings))},
      OutputBytes{kRulesSchema, std::as_bytes(std::span(rules_schema))},
      OutputBytes{kTimingsSchema, std::as_bytes(std::span(timings_schema))},
      OutputBytes{kStatusImportPath, std::as_bytes(std::span(provenance))}};
  std::vector<OutputBytes> pending;
  for (const auto& file : files) {
    if (!preserve || !std::filesystem::exists(root / file.name)) {
      pending.push_back(file);
    }
  }
  return WriteOutputFiles(root, pending);
}
}  // namespace

// decomp: vs_main_statusEffectParams (src/SLUS_010.40/main.c:524)
std::expected<StatusSourceRecords, std::string> DecodeStatusSource(
    std::span<const std::byte> bytes) {
  if (bytes.size() != kStatusSourceBytes) {
    return std::unexpected("status source: expected 32 sixteen-byte records");
  }
  StatusSourceRecords records;
  for (std::size_t index = 0; index < records.size(); ++index) {
    const auto at = index * kRecordBytes;
    records[index] = {Read(bytes, at, kWordBytes),
                      static_cast<std::uint16_t>(
                          Read(bytes, at + kDurationOffset, kHalfBytes)),
                      static_cast<std::uint16_t>(
                          Read(bytes, at + kIntervalOffset, kHalfBytes)),
                      Read(bytes, at + kReplacesOffset, kWordBytes),
                      Read(bytes, at + kBlockedByOffset, kWordBytes)};
  }
  return records;
}
std::vector<std::byte> ReconstructStatusSource(
    const StatusSourceRecords& records) {
  std::vector<std::byte> bytes;
  for (const auto& record : records) {
    Append(bytes, record.effect_channels, kWordBytes);
    Append(bytes, record.duration_word, kHalfBytes);
    Append(bytes, record.interval_word, kHalfBytes);
    Append(bytes, record.replaces, kWordBytes);
    Append(bytes, record.blocked_by, kWordBytes);
  }
  return bytes;
}
std::expected<ImportedStatus, std::string> DecodeStatus(
    std::span<const std::byte> bytes) {
  auto records = DecodeStatusSource(bytes);
  if (!records) return std::unexpected(records.error());
  ImportedStatus imported{.originals = *records, .definitions = {}};
  for (std::size_t index = 0; index < records->size(); ++index) {
    const auto& record = (*records)[index];
    imported.definitions.rules[index] = {record.effect_channels,
                                         record.replaces, record.blocked_by};
  }
  // LBU at 80086FD8/6FF0/700C/7024/703C; AI reads interval LHU.
  // Preserve every source word offline, but refuse an interval whose actor
  // and AI projections disagree rather than silently choosing one.
  if ((*records)[kPoisonRule].interval_word > UINT8_MAX ||
      (*records)[kRegenerationRule].interval_word > UINT8_MAX) {
    return std::unexpected(
        "status interval exceeds actor capacity: AI halfword and actor byte "
        "would disagree; originals remain reconstructible offline");
  }
  imported.definitions.timings = {
      static_cast<std::uint8_t>((*records)[kAttributeRule].duration_word),
      static_cast<std::uint8_t>((*records)[kMovementRule].duration_word),
      static_cast<std::uint8_t>((*records)[kEquipmentRule].duration_word),
      static_cast<std::uint8_t>((*records)[kParalysisRule].duration_word),
      static_cast<std::uint8_t>((*records)[kPoisonRule].interval_word),
      static_cast<std::uint8_t>((*records)[kRegenerationRule].interval_word)};
  auto checked = Content::ParseStatusCatalog(
      Content::FormatStatusRules(imported.definitions),
      Content::FormatStatusTimings(imported.definitions));
  if (!checked) return std::unexpected(checked.error());
  return imported;
}
std::string FormatStatusProvenance(const StatusSourceRecords& records,
                                   std::string_view source_sha256) {
  auto rows = Content::JsonDocument::array();
  for (std::size_t index = 0; index < records.size(); ++index) {
    const auto& row = records[index];
    rows.push_back({{"legacy_rule", index},
                    {"id", Content::StatusRuleIdentity(index)},
                    {"effect_channels_word", row.effect_channels},
                    {"duration_word", row.duration_word},
                    {"interval_word", row.interval_word},
                    {"replaces_word", row.replaces},
                    {"blocked_by_word", row.blocked_by}});
  }
  return Content::FormatJsonText({{"schema", kSchema},
                                  {"schema_version", 1},
                                  {"source", Content::kStatusSource},
                                  {"source_sha256", source_sha256},
                                  {"byte_offset", kStatusSourceOffset},
                                  {"original_records", rows}}) +
         "\n";
}
std::expected<StatusSourceRecords, std::string> ParseStatusProvenance(
    std::span<const std::byte> bytes) {
  auto json = Content::ParseJsonText(bytes, kStatusImportPath);
  if (!json) return std::unexpected(json.error());
  const Content::JsonMembers members(*json, std::string(kStatusImportPath));
  auto only =
      members.Only({"schema", "schema_version", "source", "source_sha256",
                    "byte_offset", "original_records"});
  if (!only) return std::unexpected(only.error());
  auto source = Content::JsonSourceOf(members, kStatusImportPath, kSchema, 1);
  auto offset = members.Unsigned("byte_offset", kStatusSourceOffset);
  auto rows = members.Get("original_records");
  if (!source || source->source != Content::kStatusSource || !offset ||
      *offset != kStatusSourceOffset || !rows || !(*rows)->is_array() ||
      (*rows)->size() != Content::kStatusRuleCount) {
    return std::unexpected("status provenance: incompatible source extent");
  }
  StatusSourceRecords records;
  for (std::size_t index = 0; index < records.size(); ++index) {
    Content::JsonMembers row((**rows)[index],
                             std::format("status original row {}", index));
    auto ordinal = row.Unsigned("legacy_rule", records.size() - 1);
    auto id = row.String("id");
    auto flags = row.Unsigned("effect_channels_word", UINT32_MAX);
    auto duration = row.Unsigned("duration_word", UINT16_MAX);
    auto interval = row.Unsigned("interval_word", UINT16_MAX);
    auto replaces = row.Unsigned("replaces_word", UINT32_MAX);
    auto blocked = row.Unsigned("blocked_by_word", UINT32_MAX);
    if (!ordinal || *ordinal != index || !id ||
        *id != Content::StatusRuleIdentity(index) || !flags || !duration ||
        !interval || !replaces || !blocked) {
      return std::unexpected("status provenance: invalid source record");
    }
    records[index] = {static_cast<std::uint32_t>(*flags),
                      static_cast<std::uint16_t>(*duration),
                      static_cast<std::uint16_t>(*interval),
                      static_cast<std::uint32_t>(*replaces),
                      static_cast<std::uint32_t>(*blocked)};
  }
  return records;
}

std::expected<TableConversionReport, std::string> ConvertDiscStatus(
    const std::filesystem::path& root, bool force) {
  auto start = StartRun(root);
  if (!start) return std::unexpected(start.error());
  auto batch = StartBatch(start->previous, Content::kStatusConverter.name,
                          Content::kStatusConverter.version, force);
  const auto entry = std::ranges::find(start->entries, Content::kStatusSource,
                                       &Content::DataStore::Entry::path);
  if (entry == start->entries.end()) return TableConversionReport{};
  if (entry->form != "raw") {
    return std::unexpected("status import requires raw SLUS");
  }
  auto source = ReadSource(root, Content::kStatusSource);
  if (!source) return std::unexpected(source.error());
  if (source->bytes.size() < kStatusSourceOffset + kStatusSourceBytes) {
    return std::unexpected(
        "SLUS_010.40 is too short for status rules at 0x3F6CC");
  }
  Content::ConversionOutput output{
      .id = std::string(Content::kStatusOutputId),
      .source = source->path,
      .source_sha256 = HashOf(*source),
      .converter = std::string(Content::kStatusConverter.name),
      .files = {},
      .dependencies = {},
      .note =
          "G01906 only: 32 sixteen-byte rules at 0x3F6CC. Shared timing "
          "roles use four duration bytes and two intervals; unused timing "
          "words and all masks preserved offline. Not whole SLUS conversion.",
      .alias_of = {}};
  if (batch.reuse && MatchingOutput(start->previous, root, output) != nullptr) {
    return TableConversionReport{.unchanged = 1};
  }
  auto original =
      std::span(source->bytes).subspan(kStatusSourceOffset, kStatusSourceBytes);
  auto imported = DecodeStatus(original);
  if (!imported) return std::unexpected(imported.error());
  if (!std::ranges::equal(ReconstructStatusSource(imported->originals),
                          original)) {
    return std::unexpected("status exact source preservation failed");
  }
  output.files = {std::string(Content::kStatusRulesPath),
                  std::string(Content::kStatusTimingsPath),
                  std::string(kRulesSchema), std::string(kTimingsSchema),
                  std::string(kStatusImportPath)};
  const auto owned =
      CheckAuthoringOwnership(start->previous, root, output, force);
  if (!owned) return std::unexpected(owned.error());
  batch.outputs.push_back(output);
  auto published = PublishBatch(root, start->previous, batch, [&] {
    return Write(root, *imported, output.source_sha256, !force);
  });
  if (!published) return std::unexpected(published.error());
  return TableConversionReport{.converted = 1, .removed = *published};
}
}  // namespace VS::Extract
