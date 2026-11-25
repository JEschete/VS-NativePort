#include "Extract/ActionCategoryConversion.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <string>
#include <vector>

#include "Content/JsonText.hpp"
#include "Extract/ConverterBatch.hpp"

namespace VS::Extract {
namespace {
constexpr std::string_view kSchema = "vs.action-category-import";
constexpr std::string_view kTableSchema{
    "catalogs/battle/action-categories.schema.json",
};
constexpr std::size_t kTableBytes = Content::kActionCategoryCount;
// The source span holds six sixteen-byte tables back to back: the original
// has five per-category tables and a zero gap between the area and
// includes-user tables.
constexpr std::size_t kNeedsTable = 0;
constexpr std::size_t kAreaTable = 1;
constexpr std::size_t kGapTable = 2;
constexpr std::size_t kIncludesTable = 3;
constexpr std::size_t kLimbsTable = 4;
constexpr std::size_t kMaskTable = 5;
constexpr std::size_t kSourceTables = 6;
static_assert(kSourceTables * kTableBytes == kActionCategorySourceBytes);

std::uint8_t At(std::span<const std::byte> bytes, std::size_t table,
                std::size_t category) {
  return std::to_integer<std::uint8_t>(bytes[table * kTableBytes + category]);
}

std::uint32_t KnownTargetKinds() {
  std::uint32_t known = 0;
  for (const auto& kind : Content::ActionTargetKinds()) {
    known |= static_cast<std::uint32_t>(kind.code);
  }
  return known;
}

std::expected<void, std::string> Write(const std::filesystem::path& root,
                                       const ImportedActionCategories& imported,
                                       std::string_view hash, bool preserve) {
  const auto categories = Content::FormatActionCategories(imported.definitions);
  const auto schema =
      Content::FormatTableSchema(Content::ActionCategoriesSpec());
  const auto provenance =
      FormatActionCategoryProvenance(imported.originals, hash);
  const std::array files{
      OutputBytes{Content::kActionCategoriesPath,
                  std::as_bytes(std::span(categories))},
      OutputBytes{kTableSchema, std::as_bytes(std::span(schema))},
      OutputBytes{kActionCategoryImportPath,
                  std::as_bytes(std::span(provenance))}};
  std::vector<OutputBytes> pending;
  for (const auto& file : files) {
    if (!preserve || !std::filesystem::exists(root / file.name)) {
      pending.push_back(file);
    }
  }
  return WriteOutputFiles(root, pending);
}
}  // namespace

// decomp: D_800E8184 (D_800E8184@BATTLE/BATTLE.PRG)
std::expected<ActionCategorySourceRecords, std::string>
DecodeActionCategorySource(std::span<const std::byte> bytes) {
  if (bytes.size() != kActionCategorySourceBytes) {
    return std::unexpected(
        "action category source: expected six sixteen-byte tables");
  }
  for (std::size_t category = 0; category < kTableBytes; ++category) {
    if (At(bytes, kGapTable, category) != 0) {
      return std::unexpected(
          "action category source: the gap between the area and "
          "includes-user tables must be zero");
    }
  }
  ActionCategorySourceRecords records;
  for (std::size_t category = 0; category < records.size(); ++category) {
    records[category] = {
        At(bytes, kNeedsTable, category), At(bytes, kAreaTable, category),
        At(bytes, kIncludesTable, category), At(bytes, kLimbsTable, category),
        At(bytes, kMaskTable, category)};
  }
  return records;
}
std::vector<std::byte> ReconstructActionCategorySource(
    const ActionCategorySourceRecords& records) {
  std::vector<std::byte> bytes(kActionCategorySourceBytes);
  const auto put = [&](std::size_t table, std::size_t category,
                       std::uint8_t value) {
    bytes[table * kTableBytes + category] = static_cast<std::byte>(value);
  };
  for (std::size_t category = 0; category < records.size(); ++category) {
    const auto& record = records[category];
    put(kNeedsTable, category, record.needs_target_selection);
    put(kAreaTable, category, record.area_target);
    put(kIncludesTable, category, record.includes_acting_actor);
    put(kLimbsTable, category, record.targets_body_parts);
    put(kMaskTable, category, record.target_actor_mask);
  }
  return bytes;
}
std::expected<ImportedActionCategories, std::string> DecodeActionCategories(
    std::span<const std::byte> bytes) {
  auto records = DecodeActionCategorySource(bytes);
  if (!records) return std::unexpected(records.error());
  const auto known = KnownTargetKinds();
  ImportedActionCategories imported{.originals = *records, .definitions = {}};
  for (std::size_t category = 0; category < records->size(); ++category) {
    const auto& record = (*records)[category];
    const std::array flags{record.needs_target_selection, record.area_target,
                           record.includes_acting_actor,
                           record.targets_body_parts};
    const auto not_boolean = [](auto flag) { return flag > 1; };
    if (std::ranges::any_of(flags, not_boolean) ||
        (static_cast<std::uint32_t>(record.target_actor_mask) & ~known) != 0) {
      return std::unexpected(
          std::format("action category {}: a flag byte is not 0 or 1, or the "
                      "target mask has a bit with no target kind name",
                      category));
    }
    imported.definitions.rules[category] = {
        .needs_target_selection = record.needs_target_selection != 0,
        .area_target = record.area_target != 0,
        .includes_acting_actor = record.includes_acting_actor != 0,
        .targets_body_parts = record.targets_body_parts != 0,
        .target_kinds = record.target_actor_mask};
  }
  auto checked = Content::ParseActionCategoryCatalog(
      Content::FormatActionCategories(imported.definitions));
  if (!checked) return std::unexpected(checked.error());
  return imported;
}
std::string FormatActionCategoryProvenance(
    const ActionCategorySourceRecords& records,
    std::string_view source_sha256) {
  auto rows = Content::JsonDocument::array();
  for (std::size_t index = 0; index < records.size(); ++index) {
    const auto& row = records[index];
    rows.push_back({{"legacy_category", index},
                    {"id", Content::ActionCategoryIdentity(index)},
                    {"needs_target_selection_byte", row.needs_target_selection},
                    {"area_target_byte", row.area_target},
                    {"includes_acting_actor_byte", row.includes_acting_actor},
                    {"targets_body_parts_byte", row.targets_body_parts},
                    {"target_actor_mask_byte", row.target_actor_mask}});
  }
  return Content::FormatJsonText({{"schema", kSchema},
                                  {"schema_version", 1},
                                  {"source", Content::kActionCategorySource},
                                  {"source_sha256", source_sha256},
                                  {"byte_offset", kActionCategorySourceOffset},
                                  {"original_records", rows}}) +
         "\n";
}
std::expected<ActionCategorySourceRecords, std::string>
ParseActionCategoryProvenance(std::span<const std::byte> bytes) {
  auto json = Content::ParseJsonText(bytes, kActionCategoryImportPath);
  if (!json) return std::unexpected(json.error());
  const Content::JsonMembers members(*json,
                                     std::string(kActionCategoryImportPath));
  auto only =
      members.Only({"schema", "schema_version", "source", "source_sha256",
                    "byte_offset", "original_records"});
  if (!only) return std::unexpected(only.error());
  auto source =
      Content::JsonSourceOf(members, kActionCategoryImportPath, kSchema, 1);
  auto offset = members.Unsigned("byte_offset", kActionCategorySourceOffset);
  auto rows = members.Get("original_records");
  if (!source || source->source != Content::kActionCategorySource || !offset ||
      *offset != kActionCategorySourceOffset || !rows || !(*rows)->is_array() ||
      (*rows)->size() != Content::kActionCategoryCount) {
    return std::unexpected(
        "action category provenance: incompatible source extent");
  }
  ActionCategorySourceRecords records;
  for (std::size_t index = 0; index < records.size(); ++index) {
    Content::JsonMembers row(
        (**rows)[index], std::format("action category original row {}", index));
    auto ordinal = row.Unsigned("legacy_category", records.size() - 1);
    auto id = row.String("id");
    auto needs = row.Unsigned("needs_target_selection_byte", UINT8_MAX);
    auto area = row.Unsigned("area_target_byte", UINT8_MAX);
    auto user = row.Unsigned("includes_acting_actor_byte", UINT8_MAX);
    auto limbs = row.Unsigned("targets_body_parts_byte", UINT8_MAX);
    auto mask = row.Unsigned("target_actor_mask_byte", UINT8_MAX);
    if (!ordinal || *ordinal != index || !id ||
        *id != Content::ActionCategoryIdentity(index) || !needs || !area ||
        !user || !limbs || !mask) {
      return std::unexpected("action category provenance: invalid source row");
    }
    records[index] = {
        static_cast<std::uint8_t>(*needs), static_cast<std::uint8_t>(*area),
        static_cast<std::uint8_t>(*user), static_cast<std::uint8_t>(*limbs),
        static_cast<std::uint8_t>(*mask)};
  }
  return records;
}

std::expected<TableConversionReport, std::string> ConvertDiscActionCategories(
    const std::filesystem::path& root, bool force) {
  auto start = StartRun(root);
  if (!start) return std::unexpected(start.error());
  auto batch =
      StartBatch(start->previous, Content::kActionCategoryConverter.name,
                 Content::kActionCategoryConverter.version, force);
  const auto entry =
      std::ranges::find(start->entries, Content::kActionCategorySource,
                        &Content::DataStore::Entry::path);
  if (entry == start->entries.end()) return TableConversionReport{};
  if (entry->form != "raw") {
    return std::unexpected("action category import requires raw BATTLE.PRG");
  }
  auto source = ReadSource(root, Content::kActionCategorySource);
  if (!source) return std::unexpected(source.error());
  if (source->bytes.size() <
      kActionCategorySourceOffset + kActionCategorySourceBytes) {
    return std::unexpected(
        "BATTLE.PRG is too short for the action category tables at 0x7F984");
  }
  Content::ConversionOutput output{
      .id = std::string(Content::kActionCategoryOutputId),
      .source = source->path,
      .source_sha256 = HashOf(*source),
      .converter = std::string(Content::kActionCategoryConverter.name),
      .files = {},
      .dependencies = {},
      .note =
          "G00019-G00023 only: the five sixteen-entry per-category tables at "
          "0x7F984 (needs selection, area, includes user, limbs, target "
          "mask). The zero gap and every source byte are preserved offline. "
          "Not whole BATTLE.PRG conversion.",
      .alias_of = {}};
  if (batch.reuse && MatchingOutput(start->previous, root, output) != nullptr) {
    return TableConversionReport{.unchanged = 1};
  }
  auto original =
      std::span(source->bytes)
          .subspan(kActionCategorySourceOffset, kActionCategorySourceBytes);
  auto imported = DecodeActionCategories(original);
  if (!imported) return std::unexpected(imported.error());
  if (!std::ranges::equal(ReconstructActionCategorySource(imported->originals),
                          original)) {
    return std::unexpected("action category exact source preservation failed");
  }
  output.files = {std::string(Content::kActionCategoriesPath),
                  std::string(kTableSchema),
                  std::string(kActionCategoryImportPath)};
  auto published = PublishAuthoredBatch(
      start->previous, root, batch, output, force,
      [&] { return Write(root, *imported, output.source_sha256, !force); });
  if (!published) return std::unexpected(published.error());
  return TableConversionReport{.converted = 1, .removed = *published};
}
}  // namespace VS::Extract
