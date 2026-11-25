#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Native\data\manifest.json (ASSET_PIPELINE.md §8, #208): what each converter
// made from which disc file. Every output records its stable id, its source
// path and SHA-256, the converter that made it, the files it wrote under the
// data root and every other input it consumed (dependencies, with hashes), so
// a changed source or dependency, or an older converter, rebuilds it. An
// alias output (schema version 2) names a source whose bytes another output
// of its converter already holds: it lists no files and reads its target's.
// The runtime refuses a manifest without the converter versions it reads.
// Not files.tsv (DataStore::ManifestName), which lists the raw disc files.
namespace VS::Content {

inline constexpr std::string_view kConversionManifestName = "manifest.json";
inline constexpr std::uint32_t kConversionManifestSchemaVersion = 2;
// Version 1 manifests have no aliases and still read.
inline constexpr std::uint32_t kOldestConversionManifestSchemaVersion = 1;
// Changes when the extractor's output contract does. Fixed text (no date or
// revision), so a second extraction writes the same bytes.
inline constexpr std::string_view kExtractorVersion = "0.1.0";

struct ConverterVersion {
  std::string_view name;
  std::uint32_t version = 0;
};

// FAR adoption is checked at the background reader, including on older data
// trees that have not yet run the optional backgrounds conversion command.
inline constexpr std::array kBackgroundLayoutConverters{
    ConverterVersion{.name = "far-layout-json", .version = 2},
};

// Optional catalog families validate their registration when present.
inline constexpr std::array kBestiaryCatalogConverters{
    ConverterVersion{.name = "bestiary-csv", .version = 1},
};
inline constexpr std::array kStatusRuleCatalogConverters{
    ConverterVersion{.name = "status-csv", .version = 1},
};
inline constexpr std::array kActionCategoryCatalogConverters{
    ConverterVersion{.name = "action-category-csv", .version = 1},
};
inline constexpr std::array kActionCatalogConverters{
    ConverterVersion{.name = "action-csv", .version = 1},
};

// The converters whose outputs this build reads, at the oldest version it
// accepts; a manifest without them is refused with instructions.
inline constexpr std::array kRequiredConverters{
    ConverterVersion{.name = "tim-png", .version = 2},
    ConverterVersion{.name = "fbc-palette-png", .version = 1},
    ConverterVersion{.name = "fbt-texture-png", .version = 1},
    ConverterVersion{.name = "syd-recipe-csv", .version = 1},
    ConverterVersion{.name = "shp-gltf", .version = 2},
    ConverterVersion{.name = "seq-gltf", .version = 1},
    ConverterVersion{.name = "wep-gltf", .version = 1},
    ConverterVersion{.name = "esq-json", .version = 1},
    ConverterVersion{.name = "etm-png", .version = 1},
    ConverterVersion{.name = "zud-json", .version = 1},
    ConverterVersion{.name = "znd-tables", .version = 3},
    ConverterVersion{.name = "wave-bank", .version = 1},
    ConverterVersion{.name = "xa-flac", .version = 1},
    ConverterVersion{.name = "str-mkv", .version = 1},
};

struct SourceDependency {
  std::string source;  // disc path, '/'-separated
  std::string source_sha256;

  bool operator==(const SourceDependency&) const = default;
};

struct ConversionOutput {
  std::string id;      // stable id, e.g. image.bg.001op01a
  std::string source;  // disc path, '/'-separated
  std::string source_sha256;
  std::string converter;
  std::vector<std::string> files;  // under the data root, '/'-separated
  std::vector<SourceDependency> dependencies;  // sorted by source
  bool unreferenced = false;  // valid content nothing on the disc uses
  std::string note;
  // The output whose files hold this source's bytes; then `files` is empty.
  std::string alias_of;

  bool operator==(const ConversionOutput&) const = default;
};

// NOLINTNEXTLINE(bugprone-exception-escape): std::map's noexcept move.
struct ConversionManifest {
  std::string extractor_version{kExtractorVersion};
  std::map<std::string, std::uint32_t, std::less<>> converters;
  std::vector<ConversionOutput> outputs;  // sorted by id

  [[nodiscard]] const ConversionOutput* Find(std::string_view id) const;
  // The output whose files hold `output`'s content: its alias target, or
  // itself.
  [[nodiscard]] const ConversionOutput* FilesOf(
      const ConversionOutput& output) const;
  bool operator==(const ConversionManifest&) const = default;
};

// Whether `id` is a content id: lowercase letters, digits, '.', '_' and '-'
// (image ids, manifest output ids).
[[nodiscard]] bool IsContentId(std::string_view id) noexcept;
// Whether `path` names a file under the data root: '/'-separated, relative,
// without empty, '.' or '..' parts, backslashes or drive colons.
[[nodiscard]] bool IsDataRelativePath(std::string_view path) noexcept;

// Whether the manifest is complete and consistent: known converters, unique
// valid ids, lowercase SHA-256s, sorted unique dependencies, file paths that
// stay under the data root, and aliases that list no files and name an
// output of their converter with files and the same source hash.
[[nodiscard]] std::expected<void, std::string> ValidateConversionManifest(
    const ConversionManifest& manifest);
// Parses and validates a manifest; unknown members are errors.
[[nodiscard]] std::expected<ConversionManifest, std::string>
ParseConversionManifest(std::span<const std::byte> text, std::string_view name);

// root\manifest.json, or nothing when there is none.
[[nodiscard]] std::expected<std::optional<ConversionManifest>, std::string>
LoadConversionManifest(const std::filesystem::path& root);
// Whether the manifest records each required converter at its version or
// later; the error names the first one missing or too old.
[[nodiscard]] std::expected<void, std::string> CheckRequiredConverters(
    const ConversionManifest& manifest,
    std::span<const ConverterVersion> required);

}  // namespace VS::Content
