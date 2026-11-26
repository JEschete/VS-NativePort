#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>

namespace VS::Extract {

// The shp-gltf and vs.shp-model, and the wep-gltf and vs.wep-model, versions
// that wrote polygon entries as [ordinal, corners, flags, extra] arrays,
// polygon_counts as an array and limb_ranges as 24 numbers.
inline constexpr std::uint32_t kNumberedPolygonShpVersion = 3;
inline constexpr std::uint32_t kNumberedPolygonWepVersion = 1;

struct ModelAuthoringReport {
  std::size_t models = 0;    // model payloads moved to the current version
  std::size_t polygons = 0;  // polygon entries rewritten in their named form
  std::size_t written = 0;   // files written, with the manifest
};

// Rewrites the polygon entries, polygon counts, body part polygons and
// preview material names of existing character, weapon and ZUD body model
// outputs in their named form (Content/ShpModelFiles.hpp) without
// reconverting source assets or replacing authored values. Each polygon
// entry and payload header is checked to read back as the same values; the
// converter versions are published last, and a file whose payload is already
// current is left as it is, so an interrupted run resumes. A material whose
// name is not the numbered flags_<hex> form keeps it.
[[nodiscard]] std::expected<ModelAuthoringReport, std::string>
UpgradeModelAuthoring(const std::filesystem::path& root);

}  // namespace VS::Extract
