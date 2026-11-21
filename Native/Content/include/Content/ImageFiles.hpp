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

struct ImageProvenance {
  std::string source;  // the disc path, '/'-separated
  std::string source_sha256;
  std::uint64_t offset = 0;  // where the image's source bytes start
  std::optional<TimOrigin> tim;
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
[[nodiscard]] std::string_view ZeroWordName(ZeroWord value) noexcept;
[[nodiscard]] std::string_view SemiTransparencyName(
    SemiTransparency value) noexcept;

}  // namespace VS::Content
