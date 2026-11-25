#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include "Content/ActionCategoryCatalog.hpp"
#include "Extract/ConversionManifestWriter.hpp"
#include "TestSupport/ScratchDataRoot.hpp"

namespace VS::test_support {
// The original's sixteen categories (BATTLE.PRG 0x7F984): selection, area,
// includes-user and limb flags, then the target kind mask.
inline Content::ActionCategoryCatalog OriginalActionCategories() {
  struct Row {
    bool selection;
    bool area;
    bool user;
    bool limbs;
    std::uint32_t kinds;
  };
  constexpr std::uint32_t kActors = 0x07;
  constexpr std::array<Row, Content::kActionCategoryCount> kRows{{
      {false, false, false, false, 0},
      {true, false, true, false, kActors},
      {true, true, true, false, kActors},
      {true, false, false, false, kActors},
      {true, true, false, false, kActors},
      {true, false, true, true, kActors},
      {true, true, true, true, kActors},
      {true, false, false, true, kActors},
      {true, true, false, true, kActors},
      {false, false, false, false, 0x20},
      {true, false, false, false, 0x80},
      {false, true, true, true, kActors},
      {false, true, false, true, kActors},
      {false, false, false, false, 0},
      {true, false, false, true, 0x17},
      {false, false, false, false, 0},
  }};
  Content::ActionCategoryCatalog catalog;
  for (std::size_t index = 0; index < kRows.size(); ++index) {
    catalog.rules[index] = {.needs_target_selection = kRows[index].selection,
                            .area_target = kRows[index].area,
                            .includes_acting_actor = kRows[index].user,
                            .targets_body_parts = kRows[index].limbs,
                            .target_kinds = kRows[index].kinds};
  }
  return catalog;
}

// Publishes the category table under `root` and registers its conversion.
inline void AddActionCategoryCatalog(
    const std::filesystem::path& root,
    const Content::ActionCategoryCatalog& rules,
    Content::ConversionManifest& manifest) {
  WriteTestFile(root, Content::kActionCategoriesPath,
                Content::FormatActionCategories(rules));
  WriteTestFile(root, "catalogs/battle/action-categories.schema.json",
                Content::FormatTableSchema(Content::ActionCategoriesSpec()));
  manifest.converters.emplace(Content::kActionCategoryConverter.name,
                              Content::kActionCategoryConverter.version);
  manifest.outputs.push_back(
      {.id = std::string(Content::kActionCategoryOutputId),
       .source = std::string(Content::kActionCategorySource),
       .source_sha256 = std::string(64, 'a'),
       .converter = std::string(Content::kActionCategoryConverter.name),
       .files = {std::string(Content::kActionCategoriesPath)},
       .dependencies = {},
       .note = {},
       .alias_of = {}});
}

class ActionCategoryRoot : public ScratchDataRoot {
 public:
  ActionCategoryRoot()
      : ScratchDataRoot("test-action-category"),
        definitions(OriginalActionCategories()) {
    Publish();
  }

  Content::ActionCategoryCatalog definitions;
  void Publish() const {
    Write(Content::DataStore::ManifestName, "lba\tsize\tform\tpath\n");
    Content::ConversionManifest manifest;
    AddActionCategoryCatalog(Root(), definitions, manifest);
    const auto written = Extract::WriteConversionManifest(Root(), manifest);
    if (!written) FAIL(written.error());
    Open();
  }
};
}  // namespace VS::test_support
