#include "Extract/ActionConversion.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "Content/CatalogTable.hpp"
#include "Content/GameTextCodec.hpp"
#include "Content/JsonText.hpp"
#include "Extract/ConverterBatch.hpp"
#include "Platform/Paths.hpp"

namespace VS::Extract {
namespace {
constexpr std::string_view kSchema = "vs.action-import";
constexpr std::string_view kActionsSchema{
    "catalogs/battle/actions.schema.json",
};
constexpr std::string_view kHitsSchema{
    "catalogs/battle/action-hits.schema.json",
};
constexpr std::string_view kSpacing = "{spacing:6}";
constexpr std::string_view kLevelToken = "{level}";
constexpr std::byte kTerminator{0xE7};
constexpr std::size_t kWordBytes = 4;
constexpr std::size_t kHalfBytes = 2;

// Source record layout; see G01904.
constexpr std::size_t kIdOffset = 0;
constexpr std::size_t kEffectOffset = 1;
constexpr std::size_t kPackedOffset = 2;
constexpr std::size_t kCostOffset = 3;
constexpr std::size_t kRangeOffset = 4;
constexpr std::size_t kRangeShapeOffset = 7;
constexpr std::size_t kAreaOffset = 8;
constexpr std::size_t kAreaShapeOffset = 11;
constexpr std::size_t kRecoveryOffset = 12;
constexpr std::size_t kFlagsOffset = 13;
constexpr std::size_t kAiOffset = 14;
constexpr std::size_t kReservedOffset = 16;
constexpr std::size_t kHitOffset = 20;
constexpr std::size_t kNameOffset = 28;
static_assert(kNameOffset + Content::kActionNameBytes == kActionRecordBytes);

struct BitField {
  unsigned shift;
  unsigned bits;
};
constexpr std::uint32_t Mask(BitField field) {
  return (std::uint32_t{1} << field.bits) - 1;
}
constexpr std::uint32_t Get(std::uint32_t word, BitField field) {
  return (word >> field.shift) & Mask(field);
}
constexpr std::uint32_t Place(std::uint32_t value, BitField field) {
  return (value & Mask(field)) << field.shift;
}
// The packed type and category byte.
constexpr BitField kMenuAction{0, 1};
constexpr BitField kKind{1, 3};
constexpr BitField kCategory{4, 4};
// The shape bytes of both volumes.
constexpr BitField kShape{0, 3};
constexpr BitField kAngle{3, 5};
// The flags byte.
constexpr BitField kReservedD{0, 4};
constexpr BitField kCameraZoom{4, 2};
constexpr BitField kHitsFirstObstruction{6, 1};
constexpr BitField kStartsUnlocked{7, 1};
// The hit words, least significant field first.
constexpr BitField kHitEffect{0, 7};
constexpr BitField kHitPrerequisite{7, 6};
constexpr BitField kHitFunction{13, 3};
constexpr BitField kHitStatCalculator{16, 6};
constexpr BitField kHitStatFactor{22, 5};
constexpr BitField kHitDamageType{27, 2};
constexpr BitField kHitAffinity{29, 3};
constexpr std::uint32_t kUnusedAiMask = 0xF;
static_assert((Content::kBehaviorAiMask & kUnusedAiMask) == 0);

using source_bytes::Append;
using source_bytes::Hex;
using source_bytes::Read;

struct DecodedName {
  std::string name;
  std::size_t consumed = 0;
};
std::expected<DecodedName, std::string> DecodeName(
    std::span<const std::byte> bytes, std::size_t index) {
  auto decoded =
      Content::DecodeGameText(bytes, std::format("action {} name", index));
  if (!decoded) return std::unexpected(decoded.error());
  if (!decoded->ignored_font_bits.empty()) {
    return std::unexpected(
        std::format("action {} name: a move-down control carries bits the "
                    "catalog does not keep",
                    index));
  }
  auto name = std::move(decoded->markup);
  // The shared native font has a six-unit space; the original names spell
  // every space as that spacing control.
  for (auto at = name.find(kSpacing); at != std::string::npos;
       at = name.find(kSpacing)) {
    name.replace(at, kSpacing.size(), " ");
  }
  auto probe = name;
  for (auto at = probe.find(kLevelToken); at != std::string::npos;
       at = probe.find(kLevelToken)) {
    probe.erase(at, kLevelToken.size());
  }
  if (probe.find_first_of("{}") != std::string::npos) {
    return std::unexpected(
        std::format("action {} name: unresolved presentation control", index));
  }
  return DecodedName{std::move(name), decoded->consumed};
}

std::uint32_t HitWord(const Content::ActionHit& hit) {
  return Place(hit.effect, kHitEffect) |
         Place(hit.prerequisite, kHitPrerequisite) |
         Place(hit.hit_function, kHitFunction) |
         Place(hit.stat_calculator, kHitStatCalculator) |
         Place(hit.stat_factor, kHitStatFactor) |
         Place(hit.damage_type, kHitDamageType) |
         Place(hit.affinity, kHitAffinity);
}
std::optional<Content::ActionHit> HitOf(std::uint32_t word) {
  if (word == 0) return std::nullopt;
  const auto field = [word](BitField where) {
    return static_cast<std::uint8_t>(Get(word, where));
  };
  return Content::ActionHit{.effect = field(kHitEffect),
                            .prerequisite = field(kHitPrerequisite),
                            .hit_function = field(kHitFunction),
                            .stat_calculator = field(kHitStatCalculator),
                            .stat_factor = field(kHitStatFactor),
                            .damage_type = field(kHitDamageType),
                            .affinity = field(kHitAffinity)};
}
Content::ActionVolume VolumeOf(std::span<const std::byte> record,
                               std::size_t extent, std::size_t shape) {
  const auto packed = Read(record, shape, 1);
  Content::ActionVolume volume;
  for (std::size_t axis = 0; axis < Content::kActionVolumeAxes; ++axis) {
    volume.extent[axis] =
        static_cast<std::uint8_t>(Read(record, extent + axis, 1));
  }
  volume.shape = static_cast<std::uint8_t>(Get(packed, kShape));
  volume.angle = static_cast<std::uint8_t>(Get(packed, kAngle));
  return volume;
}
void AppendVolume(std::vector<std::byte>& bytes,
                  const Content::ActionVolume& volume) {
  for (const auto extent : volume.extent) Append(bytes, extent, 1);
}
std::uint32_t PackedShape(const Content::ActionVolume& volume) {
  return Place(volume.shape, kShape) | Place(volume.angle, kAngle);
}

std::expected<void, std::string> Write(const std::filesystem::path& root,
                                       const ImportedActions& imported,
                                       std::string_view hash, bool preserve) {
  const auto actions = Content::FormatActions(imported.definitions);
  const auto hits = Content::FormatActionHits(imported.definitions);
  const auto actions_schema =
      Content::FormatTableSchema(Content::ActionsSpec());
  const auto hits_schema =
      Content::FormatTableSchema(Content::ActionHitsSpec());
  const auto provenance = FormatActionProvenance(imported, hash);
  const std::array files{
      OutputBytes{Content::kActionsPath, std::as_bytes(std::span(actions))},
      OutputBytes{Content::kActionHitsPath, std::as_bytes(std::span(hits))},
      OutputBytes{kActionsSchema, std::as_bytes(std::span(actions_schema))},
      OutputBytes{kHitsSchema, std::as_bytes(std::span(hits_schema))},
      OutputBytes{kActionImportPath, std::as_bytes(std::span(provenance))}};
  std::vector<OutputBytes> pending;
  for (const auto& file : files) {
    if (!preserve || !std::filesystem::exists(root / file.name)) {
      pending.push_back(file);
    }
  }
  return WriteOutputFiles(root, pending);
}

// The file's text, or nothing when it does not exist.
std::expected<std::optional<std::string>, std::string> ReadOptionalText(
    const std::filesystem::path& file) {
  std::error_code error;
  const bool exists = std::filesystem::exists(file, error);
  if (error) {
    return std::unexpected(Platform::PathText(file) + ": " + error.message());
  }
  if (!exists) return std::optional<std::string>{};
  auto bytes = ReadFileBytes(file);
  if (!bytes) return std::unexpected(bytes.error());
  return std::optional<std::string>{
      std::in_place, reinterpret_cast<const char*>(bytes->data()),
      bytes->size()};
}
}  // namespace

// decomp: VS::Battle::actionDefinitions (G01904 at SLUS_010.40 0x3C1DC)
std::expected<ImportedActions, std::string> DecodeActions(
    std::span<const std::byte> bytes) {
  if (bytes.size() != kActionSourceBytes) {
    return std::unexpected("action source: expected 256 records of 52 bytes");
  }
  ImportedActions imported;
  std::array<std::string, Content::kActionCount> names;
  for (std::size_t index = 0; index < Content::kActionCount; ++index) {
    const auto record =
        bytes.subspan(index * kActionRecordBytes, kActionRecordBytes);
    if (Read(record, kIdOffset, 1) != index) {
      return std::unexpected(
          std::format("action {}: the record's own index disagrees", index));
    }
    auto name = DecodeName(record.subspan(kNameOffset), index);
    if (!name) return std::unexpected(name.error());
    names[index] = name->name;
    const auto packed = Read(record, kPackedOffset, 1);
    const auto flags = Read(record, kFlagsOffset, 1);
    const auto ai = Read(record, kAiOffset, kHalfBytes);
    auto& rule = imported.definitions.actions[index];
    rule.name = std::move(name->name);
    rule.kind = static_cast<std::uint8_t>(Get(packed, kKind));
    rule.category = static_cast<std::uint8_t>(Get(packed, kCategory));
    rule.cost = static_cast<std::uint8_t>(Read(record, kCostOffset, 1));
    rule.recovery_delay =
        static_cast<std::uint8_t>(Read(record, kRecoveryOffset, 1));
    rule.camera_zoom = static_cast<std::uint8_t>(Get(flags, kCameraZoom));
    rule.effect_package =
        static_cast<std::uint8_t>(Read(record, kEffectOffset, 1));
    rule.behavior =
        (ai & Content::kBehaviorAiMask) |
        (Get(packed, kMenuAction) != 0 ? Content::kBehaviorMenuAction : 0) |
        (Get(flags, kHitsFirstObstruction) != 0
             ? Content::kBehaviorHitsFirstObstruction
             : 0) |
        (Get(flags, kStartsUnlocked) != 0 ? Content::kBehaviorStartsUnlocked
                                          : 0);
    rule.range = VolumeOf(record, kRangeOffset, kRangeShapeOffset);
    rule.area = VolumeOf(record, kAreaOffset, kAreaShapeOffset);
    for (std::size_t slot = 0; slot < Content::kActionHitSlots; ++slot) {
      rule.hits[slot] =
          HitOf(Read(record, kHitOffset + slot * kWordBytes, kWordBytes));
    }
    auto& original = imported.originals[index];
    original.unused_ai_bits = static_cast<std::uint8_t>(ai & kUnusedAiMask);
    original.reserved_d_bit0 =
        static_cast<std::uint8_t>(Get(flags, kReservedD));
    original.reserved10 =
        std::bit_cast<std::int32_t>(Read(record, kReservedOffset, kWordBytes));
    const auto tail = record.subspan(kNameOffset + name->consumed);
    original.name_tail.assign(tail.begin(), tail.end());
  }
  const auto identities = Content::ActionIdentities(names);
  for (std::size_t index = 0; index < Content::kActionCount; ++index) {
    imported.definitions.actions[index].id = identities[index];
  }
  // The published tables must read back to what was decoded; this also
  // names the file, row and column of any value the schema cannot hold.
  auto checked = Content::ParseActionCatalog(
      Content::FormatActions(imported.definitions),
      Content::FormatActionHits(imported.definitions));
  if (!checked) return std::unexpected(checked.error());
  if (!(*checked == imported.definitions)) {
    return std::unexpected("action tables do not read back as decoded");
  }
  return imported;
}

std::expected<std::vector<std::byte>, std::string> ReconstructActionSource(
    const ImportedActions& imported) {
  std::vector<std::byte> bytes;
  bytes.reserve(kActionSourceBytes);
  for (std::size_t index = 0; index < Content::kActionCount; ++index) {
    const auto& rule = imported.definitions.actions[index];
    const auto& original = imported.originals[index];
    const auto name = Content::EncodeActionName(rule.name);
    if (!name) return std::unexpected(name.error());
    const auto end = std::ranges::find(*name, kTerminator);
    const auto text = static_cast<std::size_t>(end - name->begin()) + 1;
    if (end == name->end() ||
        text + original.name_tail.size() != Content::kActionNameBytes) {
      return std::unexpected(std::format(
          "action {}: the name and its preserved tail do not fill the field",
          index));
    }
    Append(bytes, static_cast<std::uint32_t>(index), 1);
    Append(bytes, rule.effect_package, 1);
    Append(bytes,
           Place((rule.behavior & Content::kBehaviorMenuAction) != 0 ? 1 : 0,
                 kMenuAction) |
               Place(rule.kind, kKind) | Place(rule.category, kCategory),
           1);
    Append(bytes, rule.cost, 1);
    AppendVolume(bytes, rule.range);
    Append(bytes, PackedShape(rule.range), 1);
    AppendVolume(bytes, rule.area);
    Append(bytes, PackedShape(rule.area), 1);
    Append(bytes, rule.recovery_delay, 1);
    Append(
        bytes,
        Place(original.reserved_d_bit0, kReservedD) |
            Place(rule.camera_zoom, kCameraZoom) |
            Place((rule.behavior & Content::kBehaviorHitsFirstObstruction) != 0
                      ? 1
                      : 0,
                  kHitsFirstObstruction) |
            Place(
                (rule.behavior & Content::kBehaviorStartsUnlocked) != 0 ? 1 : 0,
                kStartsUnlocked),
        1);
    Append(bytes,
           (rule.behavior & Content::kBehaviorAiMask) | original.unused_ai_bits,
           kHalfBytes);
    Append(bytes, std::bit_cast<std::uint32_t>(original.reserved10),
           kWordBytes);
    for (const auto& hit : rule.hits) {
      Append(bytes, hit ? HitWord(*hit) : 0, kWordBytes);
    }
    bytes.insert(bytes.end(), name->begin(),
                 name->begin() + static_cast<std::ptrdiff_t>(text));
    bytes.insert(bytes.end(), original.name_tail.begin(),
                 original.name_tail.end());
  }
  return bytes;
}

std::string FormatActionProvenance(const ImportedActions& imported,
                                   std::string_view source_sha256) {
  auto rows = Content::JsonDocument::array();
  for (std::size_t index = 0; index < imported.originals.size(); ++index) {
    const auto& row = imported.originals[index];
    rows.push_back({{"legacy_action", index},
                    {"id", imported.definitions.actions[index].id},
                    {"unused_ai_bits", row.unused_ai_bits},
                    {"reserved_d_bits", row.reserved_d_bit0},
                    {"reserved10", row.reserved10},
                    {"name_tail_hex", Hex(row.name_tail)}});
  }
  return Content::FormatJsonText({{"schema", kSchema},
                                  {"schema_version", 1},
                                  {"source", Content::kActionSource},
                                  {"source_sha256", source_sha256},
                                  {"byte_offset", kActionSourceOffset},
                                  {"original_records", rows}}) +
         "\n";
}

std::expected<ActionSourceRecords, std::string> ParseActionProvenance(
    std::span<const std::byte> bytes) {
  auto json = Content::ParseJsonText(bytes, kActionImportPath);
  if (!json) return std::unexpected(json.error());
  const Content::JsonMembers members(*json, std::string(kActionImportPath));
  auto only =
      members.Only({"schema", "schema_version", "source", "source_sha256",
                    "byte_offset", "original_records"});
  if (!only) return std::unexpected(only.error());
  auto source = Content::JsonSourceOf(members, kActionImportPath, kSchema, 1);
  auto offset = members.Unsigned("byte_offset", kActionSourceOffset);
  auto rows = members.Get("original_records");
  if (!source || source->source != Content::kActionSource || !offset ||
      *offset != kActionSourceOffset || !rows || !(*rows)->is_array() ||
      (*rows)->size() != Content::kActionCount) {
    return std::unexpected("action provenance: incompatible source extent");
  }
  ActionSourceRecords records;
  for (std::size_t index = 0; index < records.size(); ++index) {
    Content::JsonMembers row((**rows)[index],
                             std::format("action original row {}", index));
    auto ordinal = row.Unsigned("legacy_action", records.size() - 1);
    auto id = row.String("id");
    auto ai = row.Unsigned("unused_ai_bits", kUnusedAiMask);
    auto reserved_d = row.Unsigned("reserved_d_bits", Mask(kReservedD));
    auto reserved = row.Integer("reserved10", INT32_MIN, INT32_MAX);
    auto tail = row.String("name_tail_hex");
    if (!ordinal || *ordinal != index || !id || !ai || !reserved_d ||
        !reserved || !tail) {
      return std::unexpected("action provenance: invalid source record");
    }
    auto name_tail = source_bytes::Unhex(*tail);
    if (!name_tail) {
      return std::unexpected("action provenance: bad name tail hex");
    }
    records[index] = {
        static_cast<std::uint8_t>(*ai), static_cast<std::uint8_t>(*reserved_d),
        static_cast<std::int32_t>(*reserved), std::move(*name_tail)};
  }
  return records;
}

std::expected<TableConversionReport, std::string> ConvertDiscActions(
    const std::filesystem::path& root, bool force) {
  auto start = StartRun(root);
  if (!start) return std::unexpected(start.error());
  auto batch =
      StartBatch(start->previous, Content::kActionCatalogConverter.name,
                 Content::kActionCatalogConverter.version, force);
  const auto entry = std::ranges::find(start->entries, Content::kActionSource,
                                       &Content::DataStore::Entry::path);
  if (entry == start->entries.end()) return TableConversionReport{};
  if (entry->form != "raw") {
    return std::unexpected("action import requires raw SLUS");
  }
  auto source = ReadSource(root, Content::kActionSource);
  if (!source) return std::unexpected(source.error());
  if (source->bytes.size() < kActionSourceOffset + kActionSourceBytes) {
    return std::unexpected(
        "SLUS_010.40 is too short for the action table at 0x3C1DC");
  }
  Content::ConversionOutput output{
      .id = std::string(Content::kActionOutputId),
      .source = source->path,
      .source_sha256 = HashOf(*source),
      .converter = std::string(Content::kActionCatalogConverter.name),
      .files = {},
      .dependencies = {},
      .note =
          "G01904 only: 256 fifty-two-byte action records at 0x3C1DC. Names "
          "become readable text; the AI flag bits, record bits and reserved "
          "word no code reads, and the bytes after each name's terminator, "
          "are preserved offline. Not whole SLUS conversion.",
      .alias_of = {}};
  if (batch.reuse && MatchingOutput(start->previous, root, output) != nullptr) {
    return TableConversionReport{.unchanged = 1};
  }
  const auto original =
      std::span(source->bytes).subspan(kActionSourceOffset, kActionSourceBytes);
  auto imported = DecodeActions(original);
  if (!imported) return std::unexpected(imported.error());
  auto rebuilt = ReconstructActionSource(*imported);
  if (!rebuilt) return std::unexpected(rebuilt.error());
  if (!std::ranges::equal(*rebuilt, original)) {
    return std::unexpected("action exact source preservation failed");
  }
  output.files = {std::string(Content::kActionsPath),
                  std::string(Content::kActionHitsPath),
                  std::string(kActionsSchema), std::string(kHitsSchema),
                  std::string(kActionImportPath)};
  auto published = PublishAuthoredBatch(
      start->previous, root, batch, output, force,
      [&] { return Write(root, *imported, output.source_sha256, !force); });
  if (!published) return std::unexpected(published.error());
  return TableConversionReport{.converted = 1, .removed = *published};
}

std::expected<ActionAuthoringReport, std::string> UpgradeActionAuthoring(
    const std::filesystem::path& root) {
  struct Table {
    const Content::TableSpec* spec;
    std::string_view path;
    std::string_view schema;
  };
  const std::array tables{
      Table{&Content::ActionsSpec(), Content::kActionsPath, kActionsSchema},
      Table{&Content::ActionHitsSpec(), Content::kActionHitsPath, kHitsSchema},
  };
  std::array<std::optional<std::string>, tables.size()> texts;
  for (std::size_t index = 0; index < tables.size(); ++index) {
    auto text = ReadOptionalText(root / Platform::Utf8Path(tables[index].path));
    if (!text) return std::unexpected(text.error());
    texts[index] = std::move(*text);
  }
  ActionAuthoringReport report;
  if (!texts[0] && !texts[1]) return report;
  if (!texts[0] || !texts[1]) {
    return std::unexpected(
        "action authoring needs both actions.csv and action-hits.csv (run "
        "VS.Extract actions <dir>)");
  }
  std::array<std::string, tables.size()> normalized;
  std::vector<std::pair<std::string_view, std::string>> pending;
  for (std::size_t index = 0; index < tables.size(); ++index) {
    const auto& table = tables[index];
    auto rewrite =
        Content::NormalizeTableText(*table.spec, *texts[index], table.path);
    if (!rewrite) return std::unexpected(rewrite.error());
    report.renamed_columns += rewrite->renamed_columns;
    report.named_values += rewrite->named_values;
    normalized[index] = std::move(rewrite->text);
    if (normalized[index] != *texts[index]) {
      pending.emplace_back(table.path, normalized[index]);
    }
    auto schema = ReadOptionalText(root / Platform::Utf8Path(table.schema));
    if (!schema) return std::unexpected(schema.error());
    auto expected = Content::FormatTableSchema(*table.spec);
    if (*schema != expected) pending.emplace_back(table.schema, expected);
  }
  const auto before = Content::ParseActionCatalog(*texts[0], *texts[1]);
  if (!before) return std::unexpected(before.error());
  const auto after = Content::ParseActionCatalog(normalized[0], normalized[1]);
  if (!after) return std::unexpected(after.error());
  if (*after != *before) {
    return std::unexpected("action authoring migration changed action rules");
  }
  auto written = WriteTextFiles(root, pending);
  if (!written) return std::unexpected(written.error());
  report.written = *written;
  return report;
}
}  // namespace VS::Extract
