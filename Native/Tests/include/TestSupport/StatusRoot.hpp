#pragma once

#include <string>

#include "Content/StatusCatalog.hpp"
#include "Extract/ConversionManifestWriter.hpp"
#include "TestSupport/ScratchDataRoot.hpp"

namespace VS::test_support {
class StatusRoot : public ScratchDataRoot {
 public:
  StatusRoot() : ScratchDataRoot("test-status") {
    definitions.timings = {15, 20, 15, 15, 2, 5};
    Publish();
  }

  Content::StatusCatalog definitions;
  void Publish() const {
    Write(Content::DataStore::ManifestName, "lba\tsize\tform\tpath\n");
    Write(Content::kStatusRulesPath, Content::FormatStatusRules(definitions));
    Write(Content::kStatusTimingsPath,
          Content::FormatStatusTimings(definitions));
    Write("catalogs/battle/status-rules.schema.json",
          Content::FormatTableSchema(Content::StatusRulesSpec()));
    Write("catalogs/battle/status-timings.schema.json",
          Content::FormatTableSchema(Content::StatusTimingsSpec()));
    Content::ConversionManifest manifest;
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
    const auto written = Extract::WriteConversionManifest(Root(), manifest);
    if (!written) FAIL(written.error());
    Open();
  }
};
}  // namespace VS::test_support
