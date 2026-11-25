#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <string>
#include <vector>

#include "Content/ActionCatalog.hpp"
#include "Extract/ActionConversion.hpp"
#include "Extract/ConversionManifestWriter.hpp"
#include "TestSupport/ActionCategoryRoot.hpp"
#include "TestSupport/ScratchDataRoot.hpp"

namespace VS::test_support {
// A valid 256-action table whose every field varies with the action's index,
// with the offline bits the catalog does not keep. Names are letters only so
// they are distinct; action 0 is the untitled placeholder.
inline Extract::ImportedActions SyntheticActions() {
  constexpr std::size_t kLetters = 26;
  constexpr std::size_t kKinds = 8;
  constexpr std::size_t kCategories = 16;
  constexpr std::size_t kZooms = 4;
  constexpr std::size_t kShapes = 8;
  constexpr std::size_t kAngles = 32;
  constexpr std::size_t kHighHitEffect = 100;
  constexpr std::size_t kPrerequisites = 40;
  constexpr std::size_t kHitFunctions = 8;
  constexpr std::size_t kStatCalculators = 27;
  constexpr std::size_t kDamageTypes = 4;
  constexpr std::size_t kAffinities = 8;
  constexpr std::size_t kSecondHitEvery = 5;
  constexpr std::size_t kNoHitEvery = 3;
  constexpr std::size_t kAiWordMask = 0xFFF;
  constexpr unsigned kAiShift = 4;
  constexpr std::size_t kLevelNamed = 7;
  constexpr std::uint32_t kReservedStep = 0x01010101;

  Extract::ImportedActions imported;
  std::array<std::string, Content::kActionCount> names;
  for (std::size_t index = 0; index < names.size(); ++index) {
    names[index] = index == 0 ? "untitled" : "Move ";
    if (index != 0) {
      names[index] += static_cast<char>('A' + index / kLetters);
      names[index] += static_cast<char>('a' + index % kLetters);
    }
  }
  names[kLevelNamed] = "Rend {level}";
  const auto identities = Content::ActionIdentities(names);
  for (std::size_t index = 0; index < names.size(); ++index) {
    const auto byte = [index](std::size_t step) {
      return static_cast<std::uint8_t>(index * step);
    };
    auto& rule = imported.definitions.actions[index];
    rule.id = identities[index];
    rule.name = names[index];
    rule.kind = static_cast<std::uint8_t>(index % kKinds);
    rule.category = static_cast<std::uint8_t>(index % kCategories);
    rule.cost = byte(1);
    rule.recovery_delay = byte(3);
    rule.camera_zoom = static_cast<std::uint8_t>(index % kZooms);
    rule.effect_id = byte(5);
    rule.behavior =
        static_cast<std::uint32_t>(((index * 0x11) & kAiWordMask) << kAiShift) |
        ((index & 1) != 0 ? Content::kBehaviorMenuAction : 0) |
        ((index & 2) != 0 ? Content::kBehaviorHitsFirstObstruction : 0) |
        ((index & 4) != 0 ? Content::kBehaviorStartsUnlocked : 0);
    rule.range = {{byte(1), byte(2), byte(3)},
                  static_cast<std::uint8_t>(index % kShapes),
                  static_cast<std::uint8_t>(index % kAngles)};
    rule.area = {{byte(7), byte(11), byte(13)},
                 static_cast<std::uint8_t>((index + 1) % kShapes),
                 static_cast<std::uint8_t>((index + 5) % kAngles)};
    const auto hit = [index](std::size_t shift) {
      const auto at = index + shift;
      return Content::ActionHit{
          .effect = static_cast<std::uint8_t>(1 + at % kHighHitEffect),
          .prerequisite = static_cast<std::uint8_t>(at % kPrerequisites),
          .hit_function = static_cast<std::uint8_t>(at % kHitFunctions),
          .stat_calculator = static_cast<std::uint8_t>(at % kStatCalculators),
          .stat_factor = static_cast<std::uint8_t>(at % kAngles),
          .damage_type = static_cast<std::uint8_t>(at % kDamageTypes),
          .affinity = static_cast<std::uint8_t>(at % kAffinities)};
    };
    if (index % kNoHitEvery != 0) rule.hits[0] = hit(0);
    if (index % kSecondHitEvery == 0) rule.hits[1] = hit(kLetters);

    const auto text = Content::EncodeActionName(rule.name);
    if (!text) FAIL(text.error());
    const auto terminator =
        std::ranges::find(*text, std::byte{0xE7}) - text->begin() + 1;
    auto& original = imported.originals[index];
    original.unused_ai_bits = static_cast<std::uint8_t>(index & 0xF);
    original.reserved_d_bit0 = static_cast<std::uint8_t>((index >> 1) & 0xF);
    original.reserved10 = std::bit_cast<std::int32_t>(
        static_cast<std::uint32_t>(index) * kReservedStep);
    for (auto at = static_cast<std::size_t>(terminator);
         at < Content::kActionNameBytes; ++at) {
      original.name_tail.push_back(
          static_cast<std::byte>((index * 7 + at) & 0x7F));
    }
  }
  return imported;
}

// The SLUS_010.40 stand-in that holds a synthetic action section.
inline std::string SyntheticActionExecutable() {
  const auto records = Extract::ReconstructActionSource(SyntheticActions());
  if (!records) FAIL(records.error());
  std::string bytes(Extract::kActionSourceOffset, '\0');
  for (const auto byte : *records) bytes += static_cast<char>(byte);
  return bytes;
}

// Publishes the action tables under `root` and registers their conversion; the
// action categories they name must be published too.
inline void AddActionCatalog(const std::filesystem::path& root,
                             const Content::ActionCatalog& definitions,
                             Content::ConversionManifest& manifest) {
  WriteTestFile(root, Content::kActionsPath,
                Content::FormatActions(definitions));
  WriteTestFile(root, Content::kActionHitsPath,
                Content::FormatActionHits(definitions));
  WriteTestFile(root, "catalogs/battle/actions.schema.json",
                Content::FormatTableSchema(Content::ActionsSpec()));
  WriteTestFile(root, "catalogs/battle/action-hits.schema.json",
                Content::FormatTableSchema(Content::ActionHitsSpec()));
  manifest.converters.emplace(Content::kActionCatalogConverter.name,
                              Content::kActionCatalogConverter.version);
  manifest.outputs.push_back(
      {.id = std::string(Content::kActionOutputId),
       .source = std::string(Content::kActionSource),
       .source_sha256 = std::string(64, 'a'),
       .converter = std::string(Content::kActionCatalogConverter.name),
       .files = {std::string(Content::kActionsPath),
                 std::string(Content::kActionHitsPath)},
       .dependencies = {},
       .note = {},
       .alias_of = {}});
}

// The active store with the action categories and the synthetic actions.
class ActionRoot : public ScratchDataRoot {
 public:
  ActionRoot()
      : ScratchDataRoot("test-action"),
        definitions(SyntheticActions().definitions) {
    Publish();
  }

  Content::ActionCatalog definitions;
  void Publish() const {
    Write(Content::DataStore::ManifestName, "lba\tsize\tform\tpath\n");
    Content::ConversionManifest manifest;
    AddActionCategoryCatalog(Root(), OriginalActionCategories(), manifest);
    AddActionCatalog(Root(), definitions, manifest);
    const auto written = Extract::WriteConversionManifest(Root(), manifest);
    if (!written) FAIL(written.error());
    Open();
  }
};
}  // namespace VS::test_support
