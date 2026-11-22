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
    const DataStore& store, const ConvertedSource& source) {
  const std::vector<std::string>& files = source.files->files;
  std::string sidecar;
  for (const std::string& file : files) {
    if (!file.ends_with(kImageSidecarSuffix)) continue;
    if (!sidecar.empty()) {
      return std::unexpected(
          std::format("{} has multiple image sidecars", source.entry->path));
    }
    sidecar = file;
  }
  if (sidecar.empty()) {
    return std::unexpected(
        std::format("{} has no native image sidecar", source.entry->path));
  }
  const std::string stem_path =
      sidecar.substr(0, sidecar.size() - kImageSidecarSuffix.size());
  if (std::ranges::find(files, stem_path + std::string(kImagePngSuffix)) ==
      files.end()) {
    return std::unexpected(
        std::format("{} has no native PNG image", source.entry->path));
  }
  const std::size_t separator = stem_path.rfind('/');
  ImageFileLocation location{
      .directory = store.Root(),
      .stem =
          stem_path.substr(separator == std::string::npos ? 0 : separator + 1)};
  if (separator != std::string::npos) {
    location.directory /= Platform::Utf8Path(stem_path.substr(0, separator));
  }
  return location;
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
  auto location = ImageFilesOf(store, source);
  if (!location) return std::unexpected(location.error());
  auto document = LoadImageFiles(location->directory, location->stem);
  if (!document) {
    return std::unexpected(
        std::format("{}: {}", source.entry->path, document.error()));
  }
  if (document->id != source.files->id ||
      document->provenance.source != source.files->source ||
      document->provenance.source_sha256 != source.conversion->source_sha256 ||
      document->unreferenced) {
    return std::unexpected(std::format(
        "{} does not match its conversion manifest", source.entry->path));
  }
  if (auto valid = ValidateImage(document->image); !valid) {
    return std::unexpected(
        std::format("{}: {}", source.entry->path, valid.error()));
  }
  return document;
}

}  // namespace VS::Content
