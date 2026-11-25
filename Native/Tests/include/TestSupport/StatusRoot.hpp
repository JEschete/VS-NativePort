#pragma once

#include <string>

#include "Content/StatusCatalog.hpp"
#include "Extract/ConversionManifestWriter.hpp"
#include "TestSupport/ScratchDataRoot.hpp"

namespace VS::test_support {
// Publishes the status tables under `root` and registers their conversion.
inline void AddStatusCatalog(const std::filesystem::path& root,
                             const Content::StatusCatalog& definitions,
                             Content::ConversionManifest& manifest) {
  WriteTestFile(root, Content::kStatusRulesPath,
                Content::FormatStatusRules(definitions));
  WriteTestFile(root, Content::kStatusTimingsPath,
                Content::FormatStatusTimings(definitions));
  WriteTestFile(root, "catalogs/battle/status-rules.schema.json",
                Content::FormatTableSchema(Content::StatusRulesSpec()));
  WriteTestFile(root, "catalogs/battle/status-timings.schema.json",
                Content::FormatTableSchema(Content::StatusTimingsSpec()));
  manifest.converters.emplace(Content::kStatusConverter.name,
                              Content::kStatusConverter.version);
  manifest.outputs.push_back(
      {.id = std::string(Content::kStatusOutputId),
       .source = std::string(Content::kStatusSource),
       .source_sha256 = std::string(64, 'a'),
       .converter = std::string(Content::kStatusConverter.name),
       .files = {std::string(Content::kStatusRulesPath),
                 std::string(Content::kStatusTimingsPath)},
       .dependencies = {},
       .note = {},
       .alias_of = {}});
}

class StatusRoot : public ScratchDataRoot {
 public:
  StatusRoot() : ScratchDataRoot("test-status") {
    definitions.timings = {15, 20, 15, 15, 2, 5};
    Publish();
  }

  Content::StatusCatalog definitions;
  void Publish() const {
    Write(Content::DataStore::ManifestName, "lba\tsize\tform\tpath\n");
    Content::ConversionManifest manifest;
    AddStatusCatalog(Root(), definitions, manifest);
    const auto written = Extract::WriteConversionManifest(Root(), manifest);
    if (!written) FAIL(written.error());
    Open();
  }
};
}  // namespace VS::test_support
