#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "Content/NativeImage.hpp"

// The files of one native image (ASSET_PIPELINE.md §6.4, #208):
//   <stem>.png           indexed at the true depth (4 or 8 bits) with palette
//                        row 0 as PLTE/tRNS, or RGBA for 16-bit images
//   <stem>.image.json    the sidecar: what PNG cannot carry (schema vs.image)
//   <stem>.palettes.png  every palette row, one per image row (only when an
//                        image has more than one palette)
//   <stem>.stp.png       a 1-bit STP mask (only for a 16-bit image whose STP
//                        bits are neither none nor all)
// The PNG is authoritative for colour and transparency; the sidecar adds the
// STP bits, the material default and provenance. Decoding rebuilds the exact
// 16-bit words, and rejects what the hardware cannot show (partial alpha) or
// what disagrees (a PNG palette that differs from palette row 0).
namespace VS::Content {

inline constexpr std::string_view kImageSchema = "vs.image";
inline constexpr int kImageSchemaVersion = 1;
inline constexpr std::string_view kImagePngSuffix = ".png";
inline constexpr std::string_view kImageSidecarSuffix = ".image.json";
inline constexpr std::string_view kImagePalettesSuffix = ".palettes.png";
inline constexpr std::string_view kImageStpSuffix = ".stp.png";

// How the consumer draws the word 0x0000: textures skip it; an image shown
// directly (a full-screen picture) shows it black.
enum class ZeroWord : std::uint8_t { kTransparent, kBlack };
// The blend the consumer applies to texels whose STP bit is set.
enum class SemiTransparency : std::uint8_t {
  kOff,
  kAverage,
  kAdd,
  kSubtract,
  kAddQuarter,
};

struct ImageMaterial {
  ZeroWord zero_word = ZeroWord::kTransparent;
  SemiTransparency semi_transparency = SemiTransparency::kOff;

  bool operator==(const ImageMaterial&) const = default;
};

// Another input the image was built from (a layout that fixes its depth, a
// palette source), with the hash the manifest checks.
struct ImageDependency {
  std::string source;
  std::string source_sha256;
  std::string role;

  bool operator==(const ImageDependency&) const = default;
};

// VRAM holds 32 texture pages of 64 x 256 halfwords, 16 across each half of
// the 1024 x 512 frame buffer.
inline constexpr std::uint32_t kTexturePageCount = 32;
inline constexpr std::uint32_t kTexturePagesPerRow = 16;
inline constexpr std::uint16_t kTexturePageWidth = 64;  // halfwords
inline constexpr std::uint16_t kTexturePageHeight = 256;

// The VRAM rectangle of texture page `page` (below kTexturePageCount).
[[nodiscard]] constexpr TimRect TexturePageRect(std::uint32_t page) noexcept {
  return TimRect{.x = static_cast<std::int16_t>((page % kTexturePagesPerRow) *
                                                kTexturePageWidth),
                 .y = static_cast<std::int16_t>((page / kTexturePagesPerRow) *
                                                kTexturePageHeight),
                 .width = kTexturePageWidth,
                 .height = kTexturePageHeight};
}

// Where an effect loads a whole-page texture (EFFECT\*.FBT, #214): the
// effect, the page's slot among its pages, the VRAM texture page and that
// page's rectangle (TexturePageRect).
struct TexturePagePlacement {
  std::uint32_t effect = 0;
  std::uint32_t slot = 0;
  std::uint32_t page = 0;
  TimRect vram;

  bool operator==(const TexturePagePlacement&) const = default;
};

// The palette row an image's PNG shows only for previewing: the image that
// owns the row, the row and its first entry. The runtime draws with the
// palette image, so editing it, not this copy, changes the game.
struct PreviewPalette {
  std::string image;  // content id
  std::uint32_t row = 0;
  std::uint32_t entry = 0;

  bool operator==(const PreviewPalette&) const = default;
};

struct ImageProvenance {
  std::string source;  // the disc path, '/'-separated
  std::string source_sha256;
  std::uint64_t offset = 0;  // where the image's source bytes start
  std::optional<TimOrigin> tim;
  std::optional<TexturePagePlacement> texture_page;
  std::optional<PreviewPalette> preview_palette;
  std::vector<ImageDependency> dependencies;
  std::string note;

  bool operator==(const ImageProvenance&) const = default;
};

struct ImageDocument {
  std::string id;  // stable image id, e.g. image.bg.001op01a
  NativeImage image;
  std::optional<ImageMaterial> material;  // only when the consumer is known
  bool unreferenced = false;
  ImageProvenance provenance;

  bool operator==(const ImageDocument&) const = default;
};

struct ImageFile {
  std::string name;  // <stem><suffix>
  std::vector<std::byte> bytes;

  bool operator==(const ImageFile&) const = default;
};

// The bytes of one file; nothing when it does not exist.
using ImageFileBytes = std::optional<std::vector<std::byte>>;
// Reads one file by name.
using ImageFileReader = std::function<ImageFileBytes(const std::string&)>;

// The document `stem`'s files hold. Errors name the file and the pixel or
// palette entry at fault.
[[nodiscard]] std::expected<ImageDocument, std::string> DecodeImageFiles(
    std::string_view stem, const ImageFileReader& read);

// DecodeImageFiles over the files in `directory`.
[[nodiscard]] std::expected<ImageDocument, std::string> LoadImageFiles(
    const std::filesystem::path& directory, std::string_view stem);
// Reads files by name from `directory`.
[[nodiscard]] ImageFileReader DirectoryFileReader(
    std::filesystem::path directory);
// Reads files by name from `files`, which must outlive the reader.
[[nodiscard]] ImageFileReader FileListReader(
    const std::vector<ImageFile>& files);
// Whether provenance names its source and hash, each dependency's source,
// role and hash, a texture page within VRAM at its own rectangle, and a
// preview palette by a valid image id.
[[nodiscard]] std::expected<void, std::string> ValidateImageProvenance(
    const ImageProvenance& provenance);
[[nodiscard]] std::string_view ZeroWordName(ZeroWord value) noexcept;
[[nodiscard]] std::string_view SemiTransparencyName(
    SemiTransparency value) noexcept;

}  // namespace VS::Content
