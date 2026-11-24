#pragma once

#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>

#include <catch2/catch_test_macros.hpp>

#include "Content/DataStore.hpp"
#include "Content/JsonText.hpp"
#include "Content/LearningCatalog.hpp"
#include "Extract/ConversionManifestWriter.hpp"
#include "Platform/Paths.hpp"

namespace VS::test_support {
class LearningRoot {
 public:
  LearningRoot()
      : root_("build\\test-learning\\" +
              std::format(
                  "{}-{}",
                  std::chrono::steady_clock::now().time_since_epoch().count(),
                  sequence++)) {
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
  // NOLINTNEXTLINE(bugprone-exception-escape): error_code cleanup.
  ~LearningRoot() {
    restore_.Restore();
    std::error_code error;
    std::filesystem::remove_all(root_, error);
  }
  LearningRoot(const LearningRoot&) = delete;
  LearningRoot& operator=(const LearningRoot&) = delete;
  LearningRoot(LearningRoot&&) = delete;
  LearningRoot& operator=(LearningRoot&&) = delete;

  Content::LearningCatalog definitions;
  [[nodiscard]] const std::filesystem::path& Root() const { return root_; }
  void Write(std::string_view path, std::string_view text) const {
    const auto file = root_ / Platform::Utf8Path(path);
    std::filesystem::create_directories(file.parent_path());
    std::ofstream stream(file, std::ios::binary | std::ios::trunc);
    stream << text;
    REQUIRE(stream.good());
  }
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
    auto written = Extract::WriteConversionManifest(root_, manifest);
    if (!written) FAIL(written.error());
    Open();
  }
  void Open() const {
    auto opened = Content::DataStore::Open(root_);
    if (!opened) FAIL(opened.error());
  }

 private:
  Content::DataStore::ScopedOverride restore_;
  std::filesystem::path root_;
  static inline unsigned sequence = 0;
};
}  // namespace VS::test_support
