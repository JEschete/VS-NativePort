#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>
#include <string_view>

#include "Content/ConversionManifest.hpp"
#include "Content/DataStore.hpp"
#include "Content/ImageFiles.hpp"

// Converted images the runtime loads in place of a disc file (ASSET_PIPELINE.md
// §6.4): the extracted file a read names, the manifest output a converter made
// from it, and the native image files that output lists. Loading never
// converts; a missing or mismatched output is an error naming the file.
namespace VS::Content {

// An extracted file and the one output a converter made from it.
struct ConvertedSource {
  const DataStore::Entry* entry = nullptr;
  const ConversionOutput* conversion = nullptr;
  // The output whose files hold the image: `conversion`, or the output it is
  // an alias of when another disc file has the same bytes.
  const ConversionOutput* files = nullptr;
};

// What a read of the extracted file names: its first sector and its size
// rounded up to whole sectors, as the disc read requested it.
struct ConvertedRead {
  std::uint32_t lba = 0;
  std::uint64_t rounded_size = 0;
  std::string_view extension;  // e.g. ".TIM"
  std::string_view converter;  // e.g. "tim-png"
};

// The extracted file `read` names and the sole output of `read.converter`
// made from it.
[[nodiscard]] std::expected<ConvertedSource, std::string> FindConvertedSource(
    const DataStore& store, const ConvertedRead& read);

// The native image `source`'s output wrote, checked against the manifest: its
// id, source path and hash, a referenced image and a valid one. An alias
// loads its target's image, which names the target's id and source.
[[nodiscard]] std::expected<ImageDocument, std::string> LoadConvertedImage(
    const DataStore& store, const ConvertedSource& source);

// An explicit authored reference can select an otherwise unreferenced image.
// Aliases return their canonical image document; the manifest validates the
// requested identity separately from that document's identity.
[[nodiscard]] std::expected<ImageDocument, std::string> LoadConvertedImageById(
    const std::filesystem::path& root, const ConversionManifest& manifest,
    std::string_view image_id);

}  // namespace VS::Content
