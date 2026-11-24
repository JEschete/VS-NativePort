#pragma once

#include <string>

#include "Content/JsonText.hpp"
#include "Content/LearningCatalog.hpp"
#include "Extract/ConversionManifestWriter.hpp"
#include "TestSupport/ScratchDataRoot.hpp"

namespace VS::test_support {
class LearningRoot : public ScratchDataRoot {
 public:
  LearningRoot() : ScratchDataRoot("test-learning") {
    for (int category = 0; category < 10; ++category) {
      for (int tier = 1; tier <= 4; ++tier) {
        definitions.break_arts.push_back(
            {Content::BreakArtLearningId(category, tier), category, tier,
             Content::BreakArtLearningAction(category, tier), tier * 100});
      }
    }
    for (int order = 1; order <= 22; ++order) {
      definitions.battle_abilities.push_back(
          {Content::AbilityLearningId(order), order, order * 100});
    }
  }

  Content::LearningCatalog definitions;
  void Publish() const {
    Write(Content::DataStore::ManifestName, "lba\tsize\tform\tpath\n");
    Write(Content::kBreakArtLearningPath,
          Content::FormatBreakArtLearning(definitions));
    Write(Content::kAbilityLearningPath,
          Content::FormatAbilityLearning(definitions));
    Write("catalogs/battle/break-art-learning.schema.json",
          Content::FormatTableSchema(Content::BreakArtLearningSpec()));
    Write("catalogs/battle/battle-ability-learning.schema.json",
          Content::FormatTableSchema(Content::AbilityLearningSpec()));
    Content::ConversionManifest manifest;
    manifest.converters.emplace(Content::kLearningConverter.name,
                                Content::kLearningConverter.version);
    manifest.outputs.push_back(
        {.id = std::string(Content::kLearningOutputId),
         .source = std::string(Content::kLearningSource),
         .source_sha256 = std::string(64, 'a'),
         .converter = std::string(Content::kLearningConverter.name),
         .files = {std::string(Content::kBreakArtLearningPath),
                   std::string(Content::kAbilityLearningPath)},
         .dependencies = {},
         .note = {},
         .alias_of = {}});
    auto written = Extract::WriteConversionManifest(Root(), manifest);
    if (!written) FAIL(written.error());
    Open();
  }
};
}  // namespace VS::test_support
