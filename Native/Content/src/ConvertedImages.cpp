// Converted images the runtime loads (ConvertedImages.hpp).
#include "Content/ConvertedImages.hpp"

#include <algorithm>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <format>
#include <string>
#include <string_view>
#include <vector>

#include "Content/ConversionManifest.hpp"
#include "Content/DataStore.hpp"
#include "Content/ImageFiles.hpp"
#include "Content/NativeImage.hpp"
#include "Platform/Paths.hpp"

namespace VS::Content {
namespace {

struct ImageFileLocation {
  std::filesystem::path directory;
  std::string stem;
};

// The directory and stem of the sidecar and PNG an output lists.
std::expected<ImageFileLocation, std::string> ImageFilesOf(
    const std::filesystem::path& root, const ConversionOutput& output) {
  const std::vector<std::string>& files = output.files;
  std::string sidecar;
  for (const std::string& file : files) {
    if (!file.ends_with(kImageSidecarSuffix)) continue;
    if (!sidecar.empty()) {
      return std::unexpected(
          std::format("{} has multiple image sidecars", output.id));
    }
    sidecar = file;
  }
  if (sidecar.empty()) {
    return std::unexpected(
        std::format("{} has no native image sidecar", output.id));
  }
  const std::string stem_path =
      sidecar.substr(0, sidecar.size() - kImageSidecarSuffix.size());
  if (std::ranges::find(files, stem_path + std::string(kImagePngSuffix)) ==
      files.end()) {
    return std::unexpected(
        std::format("{} has no native PNG image", output.id));
  }
  const std::size_t separator = stem_path.rfind('/');
  ImageFileLocation location{
      .directory = root,
      .stem =
          stem_path.substr(separator == std::string::npos ? 0 : separator + 1)};
  if (separator != std::string::npos) {
    location.directory /= Platform::Utf8Path(stem_path.substr(0, separator));
  }
  return location;
}

std::expected<ImageDocument, std::string> ReadImageOutput(
    const std::filesystem::path& root, const ConversionOutput& conversion,
    const ConversionOutput& files) {
  auto location = ImageFilesOf(root, files);
  if (!location) return std::unexpected(location.error());
  auto document = LoadImageFiles(location->directory, location->stem);
  if (!document) {
    return std::unexpected(
        std::format("{}: {}", conversion.id, document.error()));
  }
  if (document->id != files.id || document->provenance.source != files.source ||
      document->provenance.source_sha256 != conversion.source_sha256 ||
      document->unreferenced != files.unreferenced) {
    return std::unexpected(std::format(
        "{} does not match its conversion manifest", conversion.id));
  }
  if (auto valid = ValidateImage(document->image); !valid) {
    return std::unexpected(std::format("{}: {}", conversion.id, valid.error()));
  }
  return document;
}

}  // namespace

std::expected<ConvertedSource, std::string> FindConvertedSource(
    const DataStore& store, const ConvertedRead& read) {
  const DataStore::Entry* const entry = store.FindBySector(read.lba);
  const std::uint64_t rounded_size =
      entry == nullptr ? 0
                       : static_cast<std::uint64_t>(entry->SectorCount()) *
                             DataStore::kDataSectorBytes;
  if (entry == nullptr || entry->lba != read.lba || entry->form != "raw" ||
      !entry->path.ends_with(read.extension) ||
      rounded_size != read.rounded_size) {
    return std::unexpected(std::format(
        "no extracted {} file matches LBA {} and sector-rounded size {}",
        read.extension, read.lba, read.rounded_size));
  }
  const ConversionManifest* const manifest = store.Conversions();
  if (manifest == nullptr) {
    return std::unexpected("no conversion manifest is loaded");
  }
  const ConversionOutput* conversion = nullptr;
  for (const ConversionOutput& output : manifest->outputs) {
    if (output.converter != read.converter || output.source != entry->path) {
      continue;
    }
    if (conversion != nullptr) {
      return std::unexpected(std::format("multiple {} outputs name {}",
                                         read.converter, entry->path));
    }
    conversion = &output;
  }
  if (conversion == nullptr) {
    return std::unexpected(std::format("no {} output is registered for {}",
                                       read.converter, entry->path));
  }
  const ConversionOutput* const files = manifest->FilesOf(*conversion);
  if (files == nullptr) {
    return std::unexpected(std::format("{}: alias target {} is not registered",
                                       entry->path, conversion->alias_of));
  }
  return ConvertedSource{
      .entry = entry, .conversion = conversion, .files = files};
}

std::expected<ImageDocument, std::string> LoadConvertedImage(
    const DataStore& store, const ConvertedSource& source) {
  auto document =
      ReadImageOutput(store.Root(), *source.conversion, *source.files);
  if (!document) return document;
  if (document->unreferenced) {
    return std::unexpected(std::format(
        "{} does not match its conversion manifest", source.entry->path));
  }
  return document;
}

std::expected<ImageDocument, std::string> LoadConvertedImageById(
    const std::filesystem::path& root, const ConversionManifest& manifest,
    std::string_view image_id) {
  if (!IsContentId(image_id)) {
    return std::unexpected(
        std::format("invalid native image ID '{}'", image_id));
  }
  const auto* output = manifest.Find(image_id);
  if (output == nullptr) {
    return std::unexpected(
        std::format("native image '{}' is not registered", image_id));
  }
  const auto* files = manifest.FilesOf(*output);
  if (files == nullptr) {
    return std::unexpected(
        std::format("native image '{}' has no alias target", image_id));
  }
  return ReadImageOutput(root, *output, *files);
}

}  // namespace VS::Content
