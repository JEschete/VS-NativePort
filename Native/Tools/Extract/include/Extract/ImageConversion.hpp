#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Content/ConversionManifest.hpp"
#include "Content/ImageFiles.hpp"
#include "Content/NativeImage.hpp"
#include "Extract/ConversionManifestWriter.hpp"
#include "Extract/ImageFilesWriter.hpp"
#include "Extract/TimImage.hpp"
#include "Extract/TimWriter.hpp"

// The tim-png and fbc-palette-png converters (ASSET_PIPELINE.md §6.4, #208,
// #215): standalone TIMs and effect FBC CLUTs become native images under
// textures\, recorded in manifest.json. A background TIM's header is not
// authoritative (it declares 16 bits with a CLUT); its layout
// (BG\<name>.FAR) fixes the depth its tiles draw it at, and is recorded as a
// dependency. TIMs inside other files (zones, DIS, programs) belong to their
// containers' converters, which share DecodeTim.
namespace VS::Extract {
namespace Content = VS::Content;

inline constexpr std::string_view kTimConverterName = "tim-png";
inline constexpr std::uint32_t kTimConverterVersion = 1;
inline constexpr std::string_view kFbcPaletteConverterName = "fbc-palette-png";
inline constexpr std::uint32_t kFbcPaletteConverterVersion = 1;

// Where an image lives under the data root: textures/<directory>, lowercase,
// and the source file's stem.
struct ImageLocation {
  std::string directory;  // '/'-separated, e.g. textures/bg
  std::string stem;       // e.g. 001OP01A

  bool operator==(const ImageLocation&) const = default;
};

// image.<directories>.<stem>, lowercase; a file in the disc's root directory
// is image.root.<stem> (DBGFNT.TIM -> image.root.dbgfnt).
[[nodiscard]] std::expected<std::string, std::string> ImageIdForDiscPath(
    std::string_view disc_path);
// textures/<directories> (or textures/root) and the stem.
[[nodiscard]] std::expected<ImageLocation, std::string>
ImageLocationForDiscPath(std::string_view disc_path);

// The depth a background layout's tiles draw their texture at. Every drawn
// tile must give the same depth; the error names the tile.
[[nodiscard]] std::expected<Content::ImageDepth, std::string>
BackgroundLayoutDepth(std::span<const std::byte> layout);

struct SourceFile {
  std::string path;  // disc path, '/'-separated
  std::vector<std::byte> bytes;
};

struct TimConversion {
  SourceFile tim;
  std::optional<SourceFile> layout;  // fixes the depth when present
  std::optional<Content::ImageMaterial> material;
  bool unreferenced = false;
  std::string note;
};

struct ConvertedImage {
  Content::ConversionOutput output;
  ImageLocation location;
  std::vector<Content::ImageFile> files;
};

// The image files of one TIM and its manifest entry. The TIM must decode
// strictly with no bytes after it, and the result must rebuild the exact
// source bytes.
[[nodiscard]] std::expected<ConvertedImage, std::string> ConvertTim(
    const TimConversion& conversion);

// Converts one raw FBC file: 256 little-endian 16-bit colour words per row.
[[nodiscard]] std::expected<ConvertedImage, std::string> ConvertFbcPalette(
    const SourceFile& source);

struct ImageConversionReport {
  std::size_t converted = 0;
  std::size_t unchanged = 0;  // sources, dependencies and versions match
  std::size_t removed = 0;    // files no current output lists
};

// Converts every standalone TIM and effect FBC files.tsv lists under `root`,
// then records them in root\manifest.json. An output whose source,
// dependencies and converter version are unchanged and whose files exist is
// kept as it is (so edits to it survive) unless `force`. Every image converts
// before anything is written; the manifest is written after the images, and
// files no output lists any more are removed last.
[[nodiscard]] std::expected<ImageConversionReport, std::string>
ConvertDiscImages(const std::filesystem::path& root, bool force);

}  // namespace VS::Extract
