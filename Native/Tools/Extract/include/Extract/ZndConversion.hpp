#pragma once

#include <cstddef>
#include <expected>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

#include "Content/ConversionManifest.hpp"
#include "Content/DataStore.hpp"
#include "Content/ImageFiles.hpp"
#include "Content/ZoneTables.hpp"
#include "Extract/ConverterBatch.hpp"

// The znd-tables converter (ASSET_PIPELINE.md §6.9, #517): every
// MAP\ZONE*.ZND becomes rows of the zone and enemy tables under
// catalogs\world\ and catalogs\enemies\ (Content/ZoneTables.hpp), and the TIMs
// of its texture table become images under textures\zone\<ZONE>\
// (Content/ConvertedZones.hpp), recorded in manifest.json.
//
// A zone names files by sector and read size; the converter names each by its
// extracted file's id instead (a room's MPD, an enemy's ZUD archetype), and
// refuses a sector that starts no such file or a size that is not its read.
// The tables are shared by every zone, so a run that converts any zone writes
// them all.
namespace VS::Extract {

struct ZoneConversion {
  SourceFile znd;
  // The extracted disc files, which name the sectors the zone reads.
  std::span<const Content::DataStore::Entry> files;
};

struct ConvertedZone {
  Content::ZoneRecord record;
  // The zone's manifest entry, listing its texture images' files; the shared
  // tables are added when the run writes them.
  Content::ConversionOutput output;
  std::string image_directory;  // textures/zone/<ZONE>
  std::vector<Content::ImageFile> image_files;
};

// One zone's rows, images and manifest entry. The zone must decode strictly;
// its rows and images must rebuild the exact source bytes.
[[nodiscard]] std::expected<ConvertedZone, std::string> ConvertZone(
    const ZoneConversion& conversion);

struct ZoneConversionReport {
  std::size_t converted = 0;
  std::size_t unchanged = 0;  // source and converter version match
  std::size_t removed = 0;    // files no current output lists
};

// Converts every ZND files.tsv lists under `root` and records them in
// root\manifest.json. The zud-json outputs of the archetypes the zones name
// must already be recorded. When every zone's source and the converter
// version are unchanged and the files exist, nothing is written (so edits to
// the tables survive) unless `force`. Every zone converts and the tables read
// back to the same rows before anything is written.
[[nodiscard]] std::expected<ZoneConversionReport, std::string> ConvertDiscZones(
    const std::filesystem::path& root, bool force);

}  // namespace VS::Extract
