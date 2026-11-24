#include "Extract/LearningConversion.hpp"

#include <algorithm>
#include <cstdint>
#include <format>
#include <string>
#include <utility>
#include <vector>

#include "Content/JsonText.hpp"
#include "Extract/ConverterBatch.hpp"

namespace VS::Extract {
namespace {
constexpr std::string_view kImportSchema = "vs.learning-import";
constexpr std::size_t kAbilityOffset{
    kArtRequirementBytes + kUnusedArtActionBytes,
};
constexpr std::size_t kShapesOffset = kAbilityOffset + kAbilityRequirementBytes;
constexpr int kCategories = 10, kTiers = 4, kChoices = 22;
constexpr unsigned kByteBits = 8;
constexpr std::string_view kArtSchema{
    "catalogs/battle/break-art-learning.schema.json",
};
constexpr std::string_view kAbilitySchema{
    "catalogs/battle/battle-ability-learning.schema.json",
};

int Word(std::span<const std::byte> bytes, std::size_t at) {
  return std::to_integer<int>(bytes[at]) |
         (std::to_integer<int>(bytes[at + 1]) << kByteBits);
}
bool Append(std::vector<std::byte>& bytes, std::optional<int> value) {
  if (!value || *value < 0 || *value > UINT16_MAX) return false;
  bytes.push_back(static_cast<std::byte>(*value & UINT8_MAX));
  bytes.push_back(static_cast<std::byte>(*value >> kByteBits));
  return true;
}
Content::JsonDocument Bytes(std::span<const std::byte> bytes) {
  Content::JsonDocument values = Content::JsonDocument::array();
  for (const auto byte : bytes) {
    values.push_back(std::to_integer<unsigned>(byte));
  }
  return values;
}
template <std::size_t Size>
std::expected<void, std::string> ReadBytes(const Content::JsonMembers& members,
                                           std::string_view name,
                                           std::array<std::byte, Size>& bytes) {
  auto values = members.Get(name);
  if (!values || !(*values)->is_array() || (*values)->size() != Size) {
    return std::unexpected(std::string(name) +
                           ": incorrect neighbor byte extent");
  }
  for (std::size_t at = 0; at < Size; ++at) {
    const auto& value = (**values)[at];
    if (!value.is_number_unsigned() ||
        value.template get<std::uint64_t>() > UINT8_MAX) {
      return std::unexpected(std::string(name) + ": invalid provenance byte");
    }
    bytes[at] = static_cast<std::byte>(value.template get<unsigned>());
  }
  return {};
}

std::expected<void, std::string> WriteLearning(
    const std::filesystem::path& root, const ImportedLearning& imported,
    std::string_view hash, bool preserve_existing) {
  const auto arts = Content::FormatBreakArtLearning(imported.definitions);
  const auto abilities = Content::FormatAbilityLearning(imported.definitions);
  auto checked = Content::ParseLearning(arts, abilities);
  if (!checked) return std::unexpected(checked.error());
  const auto art_schema =
      Content::FormatTableSchema(Content::BreakArtLearningSpec());
  const auto ability_schema =
      Content::FormatTableSchema(Content::AbilityLearningSpec());
  const auto provenance = FormatLearningProvenance(imported, hash);
  const std::array files{
      OutputBytes{Content::kBreakArtLearningPath,
                  std::as_bytes(std::span(arts))},
      OutputBytes{Content::kAbilityLearningPath,
                  std::as_bytes(std::span(abilities))},
      OutputBytes{kArtSchema, std::as_bytes(std::span(art_schema))},
      OutputBytes{kAbilitySchema, std::as_bytes(std::span(ability_schema))},
      OutputBytes{kLearningImportPath, std::as_bytes(std::span(provenance))}};
  std::vector<OutputBytes> pending;
  for (const auto& file : files) {
    if (!preserve_existing || !std::filesystem::exists(root / file.name)) {
      pending.push_back(file);
    }
  }
  return WriteOutputFiles(root, pending);
}
}  // namespace

// decomp: vs_main_artsPointsRequirements (src/SLUS_010.40/main.c:722)
// decomp: vs_main_battleAbilitiesPointsRequirements
// (src/SLUS_010.40/main.c:751)
std::expected<ImportedLearning, std::string> DecodeLearning(
    std::span<const std::byte> bytes) {
  if (bytes.size() != kLearningImportBytes) {
    return std::unexpected(
        "learning import: expected 80+40+44+256 source bytes");
  }
  ImportedLearning imported;
  for (int category = 0; category < kCategories; ++category) {
    for (int tier = 1; tier <= kTiers; ++tier) {
      imported.definitions.break_arts.push_back(
          {Content::BreakArtLearningId(category, tier), category, tier,
           Content::BreakArtLearningAction(category, tier),
           Word(bytes,
                (static_cast<std::size_t>(category) * kTiers + tier - 1) * 2)});
    }
  }
  for (int order = 1; order <= kChoices; ++order) {
    imported.definitions.battle_abilities.push_back(
        {Content::AbilityLearningId(order), order,
         Word(bytes,
              kAbilityOffset + static_cast<std::size_t>((order - 1) * 2))});
  }
  std::ranges::copy(bytes.subspan(kArtRequirementBytes, kUnusedArtActionBytes),
                    imported.unused_art_actions.begin());
  std::ranges::copy(bytes.subspan(kShapesOffset),
                    imported.actor_extra_shapes.begin());
  std::ranges::sort(imported.definitions.break_arts, {},
                    &Content::BreakArtLearningDefinition::id);
  return imported;
}

std::expected<std::vector<std::byte>, std::string> EncodeLearning(
    const ImportedLearning& imported) {
  std::vector<std::byte> bytes;
  for (int category = 0; category < kCategories; ++category) {
    for (int tier = 0; tier < kTiers; ++tier) {
      if (!Append(bytes,
                  imported.definitions.BreakArtRequirement(category, tier))) {
        return std::unexpected(
            "offline learning encoder: missing or non-ushort art threshold");
      }
    }
  }
  bytes.insert(bytes.end(), imported.unused_art_actions.begin(),
               imported.unused_art_actions.end());
  for (int index = 0; index < kChoices; ++index) {
    if (!Append(bytes, imported.definitions.AbilityRequirement(index))) {
      return std::unexpected(
          "offline learning encoder: missing or non-ushort ability threshold");
    }
  }
  bytes.insert(bytes.end(), imported.actor_extra_shapes.begin(),
               imported.actor_extra_shapes.end());
  return bytes;
}

std::string FormatLearningProvenance(const ImportedLearning& imported,
                                     std::string_view source_sha256) {
  return Content::FormatJsonText(
             {{"schema", kImportSchema},
              {"schema_version", 1},
              {"source", Content::kLearningSource},
              {"source_sha256", source_sha256},
              {"byte_offset", kLearningSourceOffset},
              {"original_break_art_csv",
               Content::FormatBreakArtLearning(imported.definitions)},
              {"original_battle_ability_csv",
               Content::FormatAbilityLearning(imported.definitions)},
              {"unused_arts_action_ids", Bytes(imported.unused_art_actions)},
              {"actor_extra_shp", Bytes(imported.actor_extra_shapes)}}) +
         "\n";
}

std::expected<ImportedLearning, std::string> ParseLearningProvenance(
    std::span<const std::byte> text) {
  auto json = Content::ParseJsonText(text, kLearningImportPath);
  if (!json) return std::unexpected(json.error());
  const Content::JsonMembers members(*json, std::string(kLearningImportPath));
  auto only = members.Only(
      {"schema", "schema_version", "source", "source_sha256", "byte_offset",
       "original_break_art_csv", "original_battle_ability_csv",
       "unused_arts_action_ids", "actor_extra_shp"});
  if (!only) return std::unexpected(only.error());
  auto source =
      Content::JsonSourceOf(members, kLearningImportPath, kImportSchema, 1);
  auto offset = members.Unsigned("byte_offset", kLearningSourceOffset);
  if (!source || source->source != Content::kLearningSource || !offset ||
      *offset != kLearningSourceOffset) {
    return std::unexpected("learning provenance: incompatible source layout");
  }
  auto arts = members.String("original_break_art_csv");
  auto abilities = members.String("original_battle_ability_csv");
  if (!arts) return std::unexpected(arts.error());
  if (!abilities) return std::unexpected(abilities.error());
  auto definitions = Content::ParseLearning(*arts, *abilities);
  if (!definitions) return std::unexpected(definitions.error());
  ImportedLearning imported{.definitions = std::move(*definitions)};
  auto unused =
      ReadBytes(members, "unused_arts_action_ids", imported.unused_art_actions);
  auto shapes =
      ReadBytes(members, "actor_extra_shp", imported.actor_extra_shapes);
  if (!unused) return std::unexpected(unused.error());
  if (!shapes) return std::unexpected(shapes.error());
  if (auto original = EncodeLearning(imported); !original) {
    return std::unexpected(original.error());
  }
  return imported;
}

std::expected<TableConversionReport, std::string> ConvertDiscLearning(
    const std::filesystem::path& root, bool force) {
  auto start = StartRun(root);
  if (!start) return std::unexpected(start.error());
  auto batch = StartBatch(start->previous, Content::kLearningConverter.name,
                          Content::kLearningConverter.version, force);
  const auto entry = std::ranges::find(start->entries, Content::kLearningSource,
                                       &Content::DataStore::Entry::path);
  if (entry == start->entries.end()) return TableConversionReport{};
  if (entry->form != "raw") {
    return std::unexpected("learning import requires raw SLUS_010.40");
  }
  auto source = ReadSource(root, Content::kLearningSource);
  if (!source) return std::unexpected(source.error());
  if (source->bytes.size() < kLearningSourceOffset + kLearningImportBytes) {
    return std::unexpected(
        "SLUS_010.40 is too short for learning sections at 0x405E4");
  }
  Content::ConversionOutput output{
      .id = std::string(Content::kLearningOutputId),
      .source = source->path,
      .source_sha256 = HashOf(*source),
      .converter = std::string(Content::kLearningConverter.name),
      .files = {},
      .dependencies = {},
      .note =
          "G01916 0x405E4: 40 halfwords (80 bytes); G01917 0x4065C: "
          "22 halfwords (44 bytes). Adjacent unused artsActionIds (40 bytes) "
          "and actorExtraShp (256 bytes) are import-only provenance, "
          "not learning definitions or whole-executable adoption.",
      .alias_of = {}};
  if (batch.reuse && MatchingOutput(start->previous, root, output) != nullptr) {
    return TableConversionReport{.unchanged = 1};
  }
  const auto bytes = std::span(source->bytes)
                         .subspan(kLearningSourceOffset, kLearningImportBytes);
  auto imported = DecodeLearning(bytes);
  if (!imported) return std::unexpected(imported.error());
  auto encoded = EncodeLearning(*imported);
  if (!encoded || !std::ranges::equal(*encoded, bytes)) {
    return std::unexpected("learning import failed exact section preservation");
  }
  output.files = {std::string(Content::kBreakArtLearningPath),
                  std::string(Content::kAbilityLearningPath),
                  std::string(kArtSchema), std::string(kAbilitySchema),
                  std::string(kLearningImportPath)};
  const auto owned =
      CheckAuthoringOwnership(start->previous, root, output, force);
  if (!owned) return std::unexpected(owned.error());
  batch.outputs.push_back(output);
  auto published = PublishBatch(root, start->previous, batch, [&] {
    return WriteLearning(root, *imported, output.source_sha256, !force);
  });
  if (!published) return std::unexpected(published.error());
  return TableConversionReport{.converted = 1, .removed = *published};
}
}  // namespace VS::Extract
