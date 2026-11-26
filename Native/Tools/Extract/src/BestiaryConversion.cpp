#include "Extract/BestiaryConversion.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <set>
#include <span>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "Content/EnemyPreviews.hpp"
#include "Content/GameTextCodec.hpp"
#include "Core/Sha256.hpp"
#include "Extract/ConverterBatch.hpp"
#include "Extract/SaveTextConversion.hpp"
#include "Extract/TextContribution.hpp"
#include "Platform/Paths.hpp"

namespace VS::Extract {
namespace {
constexpr std::size_t kRecords = 150, kLiveRecords = 78, kStride = 44;
constexpr std::size_t kDescriptions = kRecords * kStride;
constexpr std::size_t kName = 16, kNameBytes = 28;
constexpr std::size_t kTableBytes = kRecords * 2;
constexpr unsigned kByteBits = 8;
constexpr std::size_t kModelOffset = 0, kClassOffset = 2;
constexpr std::size_t kFirstKindOffset = 4, kKindCountOffset = 6;

int Word(std::span<const std::byte> bytes, std::size_t at) {
  return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
      std::to_integer<unsigned>(bytes[at]) |
      (std::to_integer<unsigned>(bytes[at + 1]) << kByteBits)));
}
using source_bytes::Hex;
std::expected<std::vector<std::byte>, std::string> Unhex(std::string_view hex) {
  auto bytes = source_bytes::Unhex(hex);
  if (!bytes) return std::unexpected("bestiary: bad provenance hex");
  return std::move(*bytes);
}
std::expected<std::string, std::string> Name(std::string markup) {
  // F02820 deliberately skips this exact two-byte list indentation.
  constexpr std::string_view kIndent = "{spacing:14}";
  if (!markup.starts_with(kIndent)) {
    return std::unexpected(
        "bestiary name: expected original 14-unit list indent");
  }
  markup.erase(0, kIndent.size());
  // The observed inter-word advance is six nominal units. The shared native
  // font has a six-unit space; no other spacing command is normalized.
  constexpr std::string_view kSpace = "{spacing:6}";
  for (auto at = markup.find(kSpace); at != std::string::npos;
       at = markup.find(kSpace)) {
    markup.replace(at, kSpace.size(), " ");
  }
  if (markup.find('{') != std::string::npos) {
    return std::unexpected("bestiary name: unresolved presentation control");
  }
  return markup;
}
std::string Identity(std::string_view name) {
  std::string id = "bestiary.";
  for (const char c : name) {
    if (c >= 'A' && c <= 'Z') {
      id += static_cast<char>(c - 'A' + 'a');
    } else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
      id += c;
    } else if (c == ' ' || c == '-') {
      id += '-';
    } else {
      return {};
    }
  }
  return id;
}
Content::ImageFile JsonFile(std::string_view path,
                            const Content::JsonDocument& json) {
  const auto text = Content::FormatJsonText(json) + "\n";
  const auto bytes = std::as_bytes(std::span(text));
  return {std::string(path), {bytes.begin(), bytes.end()}};
}
}  // namespace

std::expected<ImportedBestiary, std::string> DecodeBestiary(
    std::span<const std::byte> bytes, std::string_view hash) {
  if (bytes.size() < kDescriptions + kTableBytes ||
      Word(bytes, kDescriptions) != static_cast<int>(kRecords)) {
    return std::unexpected(
        "MON.BIN: truncated records or invalid description table");
  }
  ImportedBestiary imported{{},
                            Content::JsonDocument::object(),
                            {{"schema", "vs.bestiary-import"},
                             {"schema_version", 1},
                             {"source", Content::kBestiarySource},
                             {"source_sha256", hash},
                             {"records", Content::JsonDocument::array()},
                             {"description_table_hex",
                              Hex(bytes.subspan(kDescriptions, kTableBytes))}}};
  imported.strings["text.bestiary.locked.name"] = "NONE";
  imported.strings["text.bestiary.locked.description"] =
      "No defeat recorded for this entry.";
  std::set<std::string> identities;
  for (std::size_t index = 0; index < kRecords; ++index) {
    const auto record = bytes.subspan(index * kStride, kStride);
    const auto start =
        static_cast<std::uint16_t>(Word(bytes, kDescriptions + index * 2)) *
            std::size_t{2} +
        kDescriptions;
    const auto end = index + 1 == kRecords
                         ? bytes.size()
                         : static_cast<std::uint16_t>(
                               Word(bytes, kDescriptions + (index + 1) * 2)) *
                                   std::size_t{2} +
                               kDescriptions;
    if (start < kDescriptions + kTableBytes || end <= start ||
        end > bytes.size()) {
      return std::unexpected(
          std::format("MON.BIN record {}: invalid description extent", index));
    }
    const auto description = bytes.subspan(start, end - start);
    auto row = Content::JsonDocument{{"legacy_id", index},
                                     {"record_hex", Hex(record)},
                                     {"description_offset", start},
                                     {"description_hex", Hex(description)}};
    if (index >= kLiveRecords) {
      // Source list/selection/prev-next consumers are explicitly bounded at78.
      // These records cannot reach preview, regardless of their stored fields.
      row["role"] = "outside-original-encyclopedia-list";
      imported.provenance["records"].push_back(std::move(row));
      continue;
    }
    const std::string context = std::format("MON.BIN record {}", index);
    if (!Content::IsEnemyPreviewCode(Word(record, kModelOffset)) ||
        Word(record, kClassOffset) < 0 ||
        Word(record, kClassOffset) >
            static_cast<int>(Content::BestiaryClass::kEvil) ||
        Word(record, kFirstKindOffset) < 0 ||
        Word(record, kKindCountOffset) <= 0 ||
        Word(record, kFirstKindOffset) + Word(record, kKindCountOffset) >
            static_cast<int>(Content::kBestiaryFlaggedKinds)) {
      return std::unexpected(context + ": invalid preview/class/defeat range");
    }
    auto name =
        Content::DecodeGameText(record.subspan(kName, kNameBytes), context);
    auto desc = Content::DecodeGameText(description, context);
    if (!name) return std::unexpected(name.error());
    if (!desc) return std::unexpected(desc.error());
    auto readable = Name(name->markup);
    if (!readable) return std::unexpected(context + ": " + readable.error());
    const auto id = Identity(*readable);
    if (id.empty() || !identities.insert(id).second) {
      return std::unexpected(context + ": missing/colliding semantic name");
    }
    Content::BestiaryDefinition definition{
        id,
        static_cast<std::int32_t>(index),
        static_cast<std::uint32_t>(index + 1),
        static_cast<Content::BestiaryClass>(Word(record, kClassOffset)),
        static_cast<std::uint32_t>(Word(record, kModelOffset)),
        Content::BestiaryUnlock::kDefeatedKindRange,
        static_cast<std::uint32_t>(Word(record, kFirstKindOffset)),
        static_cast<std::uint32_t>(Word(record, kKindCountOffset)),
        "text." + id + ".name",
        "text." + id + ".description"};
    imported.strings[definition.name] = *readable;
    imported.strings[definition.description] = desc->markup;
    row["role"] = "visible-definition";
    row["id"] = id;
    row["name_markup"] = name->markup;
    row["name_consumed"] = name->consumed;
    row["description_markup"] = desc->markup;
    row["description_consumed"] = desc->consumed;
    imported.definitions.push_back(std::move(definition));
    imported.provenance["records"].push_back(std::move(row));
  }
  return imported;
}

namespace {
struct OriginalRow {
  std::vector<std::byte> record;
  std::vector<std::byte> description;
};

std::expected<void, std::string> EncodeOriginalText(
    const Content::JsonMembers& row, std::string_view key,
    std::string_view consumed_key, std::span<std::byte> destination) {
  auto markup = row.String(key);
  auto consumed = row.Unsigned(consumed_key, destination.size());
  if (!markup) return std::unexpected(markup.error());
  if (!consumed) return std::unexpected(consumed.error());
  auto encoded = Content::EncodeGameText(*markup, row.Where(key));
  if (!encoded) return std::unexpected(encoded.error());
  if (encoded->size() != *consumed) {
    return std::unexpected(row.Where(key) + ": inconsistent consumed extent");
  }
  std::ranges::copy(*encoded, destination.begin());
  return {};
}

std::expected<OriginalRow, std::string> ReadOriginalRow(
    const Content::JsonDocument& json, std::size_t index,
    std::size_t description_offset,
    const Content::BestiaryCatalog& definitions) {
  const Content::JsonMembers row(
      json, std::format("bestiary import record {}", index));
  auto legacy = row.Unsigned("legacy_id", kRecords - 1);
  auto record_hex = row.String("record_hex");
  auto description_hex = row.String("description_hex");
  auto offset = row.Unsigned("description_offset", UINT64_MAX);
  auto role = row.String("role");
  const auto expected_role = index < kLiveRecords
                                 ? "visible-definition"
                                 : "outside-original-encyclopedia-list";
  if (!legacy || *legacy != index || !record_hex || !description_hex ||
      !offset || *offset != description_offset || !role ||
      *role != expected_role) {
    return std::unexpected(std::format(
        "bestiary import record {}: invalid identity/extent/role", index));
  }
  auto record = Unhex(*record_hex);
  auto description = Unhex(*description_hex);
  if (!record || record->size() != kStride || !description ||
      description->empty()) {
    return std::unexpected(
        std::format("bestiary import record {}: malformed bytes", index));
  }
  if (index < kLiveRecords) {
    auto id = row.String("id");
    if (!id || *id != definitions[index].id) {
      return std::unexpected("bestiary: mismatched provenance definition ID");
    }
    if (auto name =
            EncodeOriginalText(row, "name_markup", "name_consumed",
                               std::span(*record).subspan(kName, kNameBytes));
        !name) {
      return std::unexpected(name.error());
    }
    if (auto text = EncodeOriginalText(row, "description_markup",
                                       "description_consumed", *description);
        !text) {
      return std::unexpected(text.error());
    }
  }
  return OriginalRow{std::move(*record), std::move(*description)};
}
}  // namespace

std::expected<std::vector<std::byte>, std::string> ReconstructBestiary(
    const ImportedBestiary& imported) {
  const Content::JsonMembers provenance(imported.provenance,
                                        std::string(kBestiaryImportPath));
  auto source = Content::JsonSourceOf(provenance, kBestiaryImportPath,
                                      "vs.bestiary-import", 1);
  auto rows = provenance.Get("records");
  auto table_hex = provenance.String("description_table_hex");
  if (!source || source->source != Content::kBestiarySource || !rows ||
      !(*rows)->is_array() || (*rows)->size() != kRecords || !table_hex ||
      imported.definitions.size() != kLiveRecords) {
    return std::unexpected("bestiary: incompatible import provenance");
  }
  std::vector<std::byte> records, descriptions;
  for (std::size_t i = 0; i < kRecords; ++i) {
    auto row = ReadOriginalRow(
        (**rows)[i], i, kDescriptions + kTableBytes + descriptions.size(),
        imported.definitions);
    if (!row) return std::unexpected(row.error());
    records.insert(records.end(), row->record.begin(), row->record.end());
    descriptions.insert(descriptions.end(), row->description.begin(),
                        row->description.end());
  }
  auto table = Unhex(*table_hex);
  if (!table || table->size() != kTableBytes) {
    return std::unexpected("bestiary: invalid original description table");
  }
  records.insert(records.end(), table->begin(), table->end());
  records.insert(records.end(), descriptions.begin(), descriptions.end());
  if (Core::Sha256Hex(records) != source->source_sha256) {
    return std::unexpected("bestiary: original reconstruction hash mismatch");
  }
  auto decoded = DecodeBestiary(records, Core::Sha256Hex(records));
  if (!decoded || decoded->definitions != imported.definitions ||
      decoded->strings != imported.strings) {
    return std::unexpected(
        "bestiary: original definitions/text do not match provenance");
  }
  return records;
}

std::expected<void, std::string> ConvertBestiary(
    const std::filesystem::path& root, bool force) {
  auto start = StartRun(root);
  if (!start) return std::unexpected(start.error());
  const auto entry = std::ranges::find(start->entries, Content::kBestiarySource,
                                       &Content::DataStore::Entry::path);
  if (entry == start->entries.end()) return {};
  if (entry->form != "raw") {
    return std::unexpected(
        "SMALL/MON.BIN bestiary import requires a raw source");
  }
  const auto source_bytes = entry->size;
  // Bootstrap the shared native font and save contributor, without replacing
  // that owner's authored edits when this converter is forced.
  if (auto save = ConvertSaveText(root, false); !save) {
    return save;
  }
  start = StartRun(root);
  if (!start) return std::unexpected(start.error());
  auto source = ReadSource(root, Content::kBestiarySource);
  if (!source) return std::unexpected(source.error());
  if (source->bytes.size() != source_bytes) {
    return std::unexpected(
        "SMALL/MON.BIN physical size differs from files.tsv");
  }
  auto imported = DecodeBestiary(source->bytes, HashOf(*source));
  if (!imported) return std::unexpected(imported.error());
  auto exact = ReconstructBestiary(*imported);
  if (!exact || *exact != source->bytes) {
    return std::unexpected(exact ? "bestiary: round trip mismatch"
                                 : exact.error());
  }
  auto text = PrepareTextContribution(root, start->previous,
                                      {"bestiary",
                                       "text.bestiary.",
                                       {source->path, HashOf(*source)},
                                       imported->strings},
                                      force);
  if (!text) return std::unexpected(text.error());
  auto batch = StartBatch(start->previous, Content::kBestiaryConverter,
                          Content::kBestiaryConverterVersion, force);
  Content::ConversionOutput output{
      .id = std::string(Content::kBestiaryId),
      .source = source->path,
      .source_sha256 = HashOf(*source),
      .converter = std::string(Content::kBestiaryConverter),
      .files = {},
      .dependencies = {},
      .note =
          "78 reachable definitions; all150records and150descriptions "
          "preserved offline, including72out-of-list records. No runtime MON "
          "bytes.",
      .alias_of = {}};
  const auto* old =
      batch.reuse ? MatchingOutput(start->previous, root, output) : nullptr;
  if (old != nullptr) {
    batch.outputs.push_back(*old);
  } else {
    output.files = {std::string(Content::kBestiaryPath),
                    std::string(Content::kBestiarySchemaPath),
                    std::string(kBestiaryImportPath)};
    batch.outputs.push_back(output);
    const auto csv = Content::FormatBestiary(imported->definitions);
    auto parsed = Content::ParseBestiary(csv);
    if (!parsed || *parsed != imported->definitions) {
      return std::unexpected(parsed ? "bestiary CSV changed definitions"
                                    : parsed.error());
    }
    const auto schema =
        Content::FormatTableSchema(Content::BestiaryTableSpec());
    for (const auto& [path, value] :
         {std::pair{Content::kBestiaryPath, csv},
          std::pair{Content::kBestiarySchemaPath, schema}}) {
      const auto bytes = std::as_bytes(std::span(value));
      text->files.push_back({std::string(path), {bytes.begin(), bytes.end()}});
    }
    text->files.push_back(JsonFile(kBestiaryImportPath, imported->provenance));
  }
  if (text->files.empty() && old != nullptr) return {};
  std::vector<OutputBytes> files;
  for (const auto& file : text->files) files.push_back({file.name, file.bytes});
  if (auto written = WriteOutputFiles(root, files); !written) {
    return written;
  }
  const std::array batches{batch, text->batch};
  auto published = PublishBatches(root, start->previous, batches);
  if (!published) return std::unexpected(published.error());
  return {};
}

std::expected<ImportedBestiary, std::string> ParseBestiaryProvenance(
    std::span<const std::byte> bytes) {
  auto json = Content::ParseJsonText(bytes, kBestiaryImportPath);
  if (!json) return std::unexpected(json.error());
  const Content::JsonMembers members(*json, std::string(kBestiaryImportPath));
  auto source = Content::JsonSourceOf(members, kBestiaryImportPath,
                                      "vs.bestiary-import", 1);
  auto rows = members.Get("records");
  auto table_hex = members.String("description_table_hex");
  if (!source || source->source != Content::kBestiarySource || !rows ||
      !(*rows)->is_array() || (*rows)->size() != kRecords || !table_hex) {
    return std::unexpected("bestiary: incompatible import provenance");
  }
  std::vector<std::byte> records, descriptions;
  for (const auto& row : **rows) {
    const Content::JsonMembers fields(row, "bestiary import record");
    auto record_hex = fields.String("record_hex");
    auto description_hex = fields.String("description_hex");
    if (!record_hex || !description_hex) {
      return std::unexpected("bestiary: missing original record/description");
    }
    auto record = Unhex(*record_hex);
    auto description = Unhex(*description_hex);
    if (!record || record->size() != kStride || !description) {
      return std::unexpected("bestiary: malformed original bytes");
    }
    records.insert(records.end(), record->begin(), record->end());
    descriptions.insert(descriptions.end(), description->begin(),
                        description->end());
  }
  auto table = Unhex(*table_hex);
  if (!table || table->size() != kTableBytes) {
    return std::unexpected("bestiary: malformed original offset table");
  }
  records.insert(records.end(), table->begin(), table->end());
  records.insert(records.end(), descriptions.begin(), descriptions.end());
  auto original = DecodeBestiary(records, source->source_sha256);
  if (!original) return std::unexpected(original.error());
  original->provenance = std::move(*json);
  if (auto checked = ReconstructBestiary(*original); !checked) {
    return std::unexpected(checked.error());
  }
  return original;
}

std::expected<BestiaryAuthoringReport, std::string> UpgradeBestiaryAuthoring(
    const std::filesystem::path& root) {
  BestiaryAuthoringReport report;
  const auto table = root / Platform::Utf8Path(Content::kBestiaryPath);
  std::error_code error;
  const bool exists = std::filesystem::exists(table, error);
  if (error) {
    return std::unexpected(Platform::PathText(table) + ": " + error.message());
  }
  if (!exists) return report;
  auto bytes = ReadFileBytes(table);
  if (!bytes) return std::unexpected(bytes.error());
  const std::string text(reinterpret_cast<const char*>(bytes->data()),
                         bytes->size());
  const auto& spec = Content::BestiaryTableSpec();
  auto rewrite =
      Content::NormalizeTableText(spec, text, Content::kBestiaryPath);
  if (!rewrite) return std::unexpected(rewrite.error());
  report.renamed_columns = rewrite->renamed_columns;
  report.named_values = rewrite->named_values;
  const auto before = Content::ParseBestiary(text);
  if (!before) return std::unexpected(before.error());
  const auto after = Content::ParseBestiary(rewrite->text);
  if (!after) return std::unexpected(after.error());
  if (*after != *before) {
    return std::unexpected("bestiary authoring migration changed definitions");
  }
  const auto schema = Content::FormatTableSchema(spec);
  const auto schema_bytes = std::as_bytes(std::span(schema));
  const auto old_schema =
      ReadFileBytes(root / Platform::Utf8Path(Content::kBestiarySchemaPath));
  std::vector<OutputBytes> outputs;
  if (rewrite->text != text) {
    outputs.push_back({.name = Content::kBestiaryPath,
                       .bytes = std::as_bytes(std::span(rewrite->text))});
  }
  if (!old_schema || !std::ranges::equal(*old_schema, schema_bytes)) {
    outputs.push_back(
        {.name = Content::kBestiarySchemaPath, .bytes = schema_bytes});
  }
  if (!outputs.empty()) {
    if (auto written = WriteOutputFiles(root, outputs); !written) {
      return std::unexpected(written.error());
    }
  }
  report.written = outputs.size();
  return report;
}
}  // namespace VS::Extract
