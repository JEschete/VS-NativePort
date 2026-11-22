// Converted character models the runtime loads (ConvertedModels.hpp).
#include "Content/ConvertedModels.hpp"

#include <cstddef>
#include <expected>
#include <filesystem>
#include <format>
#include <string>
#include <string_view>
#include <vector>

#include "Content/ConvertedImages.hpp"
#include "Content/DataStore.hpp"
#include "Content/ShpFile.hpp"
#include "Content/ShpModelFiles.hpp"
#include "Platform/Paths.hpp"

namespace VS::Content {
namespace {

struct ModelFileLocation {
  std::filesystem::path directory;
  std::string stem;
};

// The directory and stem of the one glTF file an output lists.
std::expected<ModelFileLocation, std::string> ModelFilesOf(
    const DataStore& store, const ConvertedSource& source) {
  std::string_view gltf;
  for (const std::string& file : source.files->files) {
    if (!file.ends_with(kGltfSuffix)) continue;
    if (!gltf.empty()) {
      return std::unexpected(
          std::format("{} has multiple glTF models", source.entry->path));
    }
    gltf = file;
  }
  if (gltf.empty()) {
    return std::unexpected(
        std::format("{} has no glTF model", source.entry->path));
  }
  const std::string_view stem_path =
      gltf.substr(0, gltf.size() - kGltfSuffix.size());
  const std::size_t separator = stem_path.rfind('/');
  ModelFileLocation location{
      .directory = store.Root(),
      .stem = std::string(stem_path.substr(
          separator == std::string_view::npos ? 0 : separator + 1))};
  if (separator != std::string_view::npos) {
    location.directory /= Platform::Utf8Path(stem_path.substr(0, separator));
  }
  return location;
}

}  // namespace

std::expected<std::vector<std::byte>, std::string> LoadConvertedModel(
    const DataStore& store, const ConvertedModelRead& read) {
  auto found =
      FindConvertedSource(store, {.lba = read.lba,
                                  .rounded_size = read.rounded_size,
                                  .extension = kShpExtension,
                                  .converter = kShpModelConverterName});
  if (!found) return std::unexpected(found.error());
  const ConvertedSource& source = *found;
  auto location = ModelFilesOf(store, source);
  if (!location) return std::unexpected(location.error());
  auto document = LoadShpModelFiles(location->directory, location->stem);
  if (!document) {
    return std::unexpected(
        std::format("{}: {}", source.entry->path, document.error()));
  }
  if (document->id != source.files->id ||
      document->source != source.files->source ||
      document->source_sha256 != source.conversion->source_sha256) {
    return std::unexpected(std::format(
        "{} does not match its conversion manifest", source.entry->path));
  }
  auto bytes = EncodeShp(document->shp);
  if (!bytes) {
    return std::unexpected(
        std::format("{}: {}", source.entry->path, bytes.error()));
  }
  return bytes;
}

}  // namespace VS::Content
