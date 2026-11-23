#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Content/CdDrive.hpp"
#include "Content/DataStore.hpp"
#include "Content/ZndFile.hpp"
#include "Content/ZoneTables.hpp"

// Converted zones the room loader reads in place of MAP\ZONE*.ZND
// (ASSET_PIPELINE.md §6.9, #517): the zone and enemy tables (ZoneTables.hpp)
// and each zone's texture images, rebuilt into the zone file's exact bytes.
// The rows name files by id; the rebuild takes each file's sector and read
// size from the extracted disc files as they are now, so a room or archetype
// that changes size moves with its file. Loading never converts.
namespace VS::Content {

// A zone texture's image: image.map.zone009.00 is the first TIM of
// MAP/ZONE009.ZND, written as textures/zone/ZONE009/00.png and its sidecar.
[[nodiscard]] std::string ZoneTextureId(std::string_view zone_id,
                                        std::size_t index);
struct ZoneTextureLocation {
  std::string directory;  // textures/zone/ZONE009
  std::string stem;       // 00
  std::string source;     // the zone file, MAP/ZONE009.ZND

  bool operator==(const ZoneTextureLocation&) const = default;
};
[[nodiscard]] std::expected<ZoneTextureLocation, std::string>
ZoneTextureLocationOf(std::string_view image_id);

// How a rebuild finds what a zone's rows name.
struct ZoneFileResolver {
  // The first sector and read size (whole sectors) of the extracted file.
  std::function<std::expected<ZndFileEntry, std::string>(
      std::string_view disc_path)>
      file;
  // The TIM bytes of a zone texture image.
  std::function<std::expected<std::vector<std::byte>, std::string>(
      std::string_view image_id)>
      texture;
};

// The zone file `zone` describes: its rooms and archetypes by their files'
// extents, its templates, music and textures.
[[nodiscard]] std::expected<ZndFile, std::string> ZndFileOf(
    const ZoneRecord& zone, const ZoneFileResolver& resolve);

// The resolver the runtime uses: extracted files from `store`, and texture
// images from their files, each checked against its zone's manifest output.
[[nodiscard]] ZoneFileResolver StoreZoneResolver(const DataStore& store);

// The zone file's bytes rebuilt from the znd-tables output of the extracted
// zone file starting at `lba`. The tables are read once while their files are
// unchanged.
[[nodiscard]] std::expected<std::vector<std::byte>, std::string>
LoadConvertedZone(const DataStore& store, std::uint32_t lba);

// Whether a read of `file` names an extracted MAP\ZONE*.ZND, which loads from
// its conversion (ReadLiveConvertedZone) instead of the disc.
[[nodiscard]] bool IsLiveConvertedZone(DiscFileExtent file);

// Native (#517): fills `destination` with the converted zone a read of `file`
// names and returns whether it loaded; a zone that does not load or fit
// leaves zeros, which the loader refuses as a malformed zone. Nothing when
// `file` is not an extracted MAP\ZONE*.ZND, which the caller reads from the
// disc.
[[nodiscard]] std::optional<bool> ReadLiveConvertedZone(
    DiscFileExtent file, std::span<std::byte> destination);

}  // namespace VS::Content
