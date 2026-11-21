#include "Extract/ImageFilesWriter.hpp"

#include <lodepng.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <fstream>
#include <initializer_list>
#include <span>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "Content/ConversionManifest.hpp"
#include "Content/JsonText.hpp"
#include "Extract/TimImage.hpp"
#include "Platform/Paths.hpp"

namespace VS::Extract {
namespace {
namespace fs = std::filesystem;
using Content::ImageColor;
using Content::ImageDependency;
using Content::ImageDocument;
using Content::ImageFile;
using Content::ImageProvenance;
using Content::JsonDocument;
using Json = JsonDocument;
using Bytes = std::vector<std::byte>;
using PngBytes = std::vector<unsigned char>;

constexpr unsigned kByteBits = 8;
constexpr std::size_t kRgbaChannels = 4;
constexpr unsigned kRgbaBitDepth = 8;
constexpr unsigned kMaskBitDepth = 1;
constexpr std::string_view kStpNone = "none";
constexpr std::string_view kStpAll = "all";
constexpr std::string_view kStpMask = "mask";

std::string NameOf(std::string_view stem, std::string_view suffix) {
  return std::string(stem) + std::string(suffix);
}

Bytes ToBytes(std::span<const unsigned char> data) {
  Bytes bytes(data.size());
  std::ranges::transform(data, bytes.begin(), [](unsigned char value) {
    return static_cast<std::byte>(value);
  });
  return bytes;
}

PngBytes ToPngBytes(std::span<const std::byte> data) {
  PngBytes bytes(data.size());
  std::ranges::transform(data, bytes.begin(), [](std::byte value) {
    return std::to_integer<unsigned char>(value);
  });
  return bytes;
}

Bytes TextBytes(std::string_view text) {
  Bytes bytes(text.size());
  std::ranges::transform(text, bytes.begin(), [](char value) {
    return static_cast<std::byte>(value);
  });
  return bytes;
}

std::expected<void, std::string> CheckStem(std::string_view stem) {
  if (stem.empty() || stem == "." || stem == ".." ||
      stem.find_first_of("/\\:") != std::string_view::npos) {
    return std::unexpected(
        std::format("'{}' is not an image file name stem", stem));
  }
  return {};
}

std::expected<void, std::string> CheckId(std::string_view id) {
  if (!Content::IsContentId(id)) {
    return std::unexpected(std::format(
        "image id '{}' must be lowercase letters, digits, '.', '_' or '-'",
        id));
  }
  return {};
}

std::expected<void, std::string> CheckProvenance(
    const ImageProvenance& provenance) {
  if (provenance.source.empty() ||
      !Content::IsLowercaseSha256(provenance.source_sha256)) {
    return std::unexpected(
        "provenance needs a source path and its lowercase SHA-256");
  }
  for (const ImageDependency& dependency : provenance.dependencies) {
    if (dependency.source.empty() || dependency.role.empty() ||
        !Content::IsLowercaseSha256(dependency.source_sha256)) {
      return std::unexpected(std::format(
          "dependency '{}' needs a source path, a role and its lowercase "
          "SHA-256",
          dependency.source));
    }
  }
  return {};
}

enum class StpCoverage : std::uint8_t { kNone, kAll, kSome };

StpCoverage CoverageOf(std::span<const std::uint16_t> words) {
  bool any = false;
  bool every = true;
  for (const std::uint16_t word : words) {
    if ((word & Content::kColorBits) == 0) continue;
    if (Content::CarriesStp(word)) {
      any = true;
    } else {
      every = false;
    }
  }
  if (!any) return StpCoverage::kNone;
  return every ? StpCoverage::kAll : StpCoverage::kSome;
}

Json RowStpJson(std::span<const std::uint16_t> row) {
  switch (CoverageOf(row)) {
    case StpCoverage::kNone:
      return std::string(kStpNone);
    case StpCoverage::kAll:
      return std::string(kStpAll);
    case StpCoverage::kSome:
      break;
  }
  Json indices = Json::array();
  for (std::size_t index = 0; index < row.size(); ++index) {
    if (Content::CarriesStp(row[index])) indices.push_back(index);
  }
  return indices;
}

std::string_view DirectStpName(StpCoverage coverage) {
  switch (coverage) {
    case StpCoverage::kNone:
      return kStpNone;
    case StpCoverage::kAll:
      return kStpAll;
    case StpCoverage::kSome:
      break;
  }
  return kStpMask;
}

Json RectJson(const Content::TimRect& rect) {
  return Json::array({rect.x, rect.y, rect.width, rect.height});
}

Json ProvenanceJson(const ImageProvenance& provenance) {
  Json json;
  json["source"] = provenance.source;
  json["source_sha256"] = provenance.source_sha256;
  json["offset"] = provenance.offset;
  if (provenance.tim) {
    Json tim;
    tim["id"] = provenance.tim->id;
    tim["flags"] = provenance.tim->flags;
    tim["image"] = RectJson(provenance.tim->image);
    if (provenance.tim->clut) tim["clut"] = RectJson(*provenance.tim->clut);
    json["tim"] = std::move(tim);
  }
  if (!provenance.dependencies.empty()) {
    Json dependencies = Json::array();
    for (const ImageDependency& dependency : provenance.dependencies) {
      Json entry;
      entry["source"] = dependency.source;
      entry["source_sha256"] = dependency.source_sha256;
      entry["role"] = dependency.role;
      dependencies.push_back(std::move(entry));
    }
    json["dependencies"] = std::move(dependencies);
  }
  if (!provenance.note.empty()) json["note"] = provenance.note;
  return json;
}

std::string SidecarText(const ImageDocument& document) {
  const Content::NativeImage& image = document.image;
  Json json;
  json["schema"] = std::string(Content::kImageSchema);
  json["schema_version"] = Content::kImageSchemaVersion;
  json["id"] = document.id;
  json["depth"] = static_cast<int>(image.depth);
  json["width"] = image.width;
  json["height"] = image.height;
  if (Content::IsIndexed(image.depth)) {
    Json palette;
    palette["entries"] = image.PaletteEntries();
    palette["count"] = image.palettes.size();
    Json stp = Json::array();
    for (const auto& row : image.palettes) stp.push_back(RowStpJson(row));
    palette["stp"] = std::move(stp);
    json["palette"] = std::move(palette);
  } else {
    json["stp"] = std::string(DirectStpName(CoverageOf(image.texels)));
  }
  if (document.material) {
    Json material;
    material["zero_word"] =
        std::string(Content::ZeroWordName(document.material->zero_word));
    material["semi_transparency"] = std::string(
        Content::SemiTransparencyName(document.material->semi_transparency));
    json["material"] = std::move(material);
  }
  if (document.unreferenced) json["unreferenced"] = true;
  json["provenance"] = ProvenanceJson(document.provenance);
  return Content::FormatJsonText(json) + "\n";
}

std::expected<void, std::string> SetPalette(LodePNGColorMode& mode,
                                            std::span<const ImageColor> colors,
                                            std::string_view name) {
  lodepng_palette_clear(&mode);
  for (const ImageColor& color : colors) {
    if (lodepng_palette_add(&mode, color.red, color.green, color.blue,
                            color.alpha) != 0) {
      return std::unexpected(
          std::format("{}: cannot build the PNG palette", name));
    }
  }
  return {};
}

std::expected<PngBytes, std::string> EncodePng(const PngBytes& raw,
                                               std::uint32_t width,
                                               std::uint32_t height,
                                               lodepng::State& state,
                                               std::string_view name) {
  state.encoder.auto_convert = 0;
  state.encoder.add_id = 0;
  PngBytes png;
  if (const unsigned error = lodepng::encode(png, raw, width, height, state);
      error != 0) {
    return std::unexpected(std::format("{}: cannot encode the PNG: {}", name,
                                       lodepng_error_text(error)));
  }
  return png;
}

void SetMode(LodePNGColorMode& mode, LodePNGColorType type, unsigned depth) {
  mode.colortype = type;
  mode.bitdepth = depth;
}

PngBytes PackBits(std::span<const std::uint8_t> values, unsigned bits) {
  PngBytes raw(((values.size() * bits) + kByteBits - 1) / kByteBits);
  for (std::size_t index = 0; index < values.size(); ++index) {
    const std::size_t bit = index * bits;
    const auto shift =
        static_cast<unsigned>(kByteBits - bits - (bit % kByteBits));
    raw[bit / kByteBits] = static_cast<unsigned char>(
        raw[bit / kByteBits] | (unsigned{values[index]} << shift));
  }
  return raw;
}

PngBytes RgbaOf(std::span<const ImageColor> colors) {
  PngBytes raw;
  raw.reserve(colors.size() * kRgbaChannels);
  for (const ImageColor& color : colors) {
    raw.insert(raw.end(), {color.red, color.green, color.blue, color.alpha});
  }
  return raw;
}

std::vector<ImageColor> ColorsOf(std::span<const std::uint16_t> words) {
  std::vector<ImageColor> colors;
  colors.reserve(words.size());
  std::ranges::transform(words, std::back_inserter(colors),
                         Content::ColorOfWord);
  return colors;
}

std::expected<PngBytes, std::string> EncodeIndexedPng(
    const Content::NativeImage& image, std::string_view name) {
  const auto bits = static_cast<unsigned>(image.depth);
  const std::vector<ImageColor> palette = ColorsOf(image.palettes.front());
  lodepng::State state;
  SetMode(state.info_raw, LCT_PALETTE, bits);
  SetMode(state.info_png.color, LCT_PALETTE, bits);
  if (auto set = SetPalette(state.info_raw, palette, name); !set) {
    return std::unexpected(set.error());
  }
  if (auto set = SetPalette(state.info_png.color, palette, name); !set) {
    return std::unexpected(set.error());
  }
  return EncodePng(PackBits(image.indices, bits), image.width, image.height,
                   state, name);
}

std::expected<PngBytes, std::string> EncodeRgbaPng(
    std::span<const ImageColor> colors, std::uint32_t width,
    std::uint32_t height, std::string_view name) {
  lodepng::State state;
  SetMode(state.info_raw, LCT_RGBA, kRgbaBitDepth);
  SetMode(state.info_png.color, LCT_RGBA, kRgbaBitDepth);
  return EncodePng(RgbaOf(colors), width, height, state, name);
}

std::expected<PngBytes, std::string> EncodeStpMask(
    const Content::NativeImage& image, std::string_view name) {
  std::vector<std::uint8_t> bits(image.texels.size());
  std::ranges::transform(image.texels, bits.begin(), [](std::uint16_t word) {
    return static_cast<std::uint8_t>(Content::CarriesStp(word) ? 1 : 0);
  });
  lodepng::State state;
  SetMode(state.info_raw, LCT_GREY, kMaskBitDepth);
  SetMode(state.info_png.color, LCT_GREY, kMaskBitDepth);
  return EncodePng(PackBits(bits, kMaskBitDepth), image.width, image.height,
                   state, name);
}
}  // namespace

std::expected<std::vector<ImageFile>, std::string> EncodeImageFiles(
    const ImageDocument& document, std::string_view stem) {
  if (auto valid = CheckStem(stem); !valid) {
    return std::unexpected(valid.error());
  }
  if (auto valid = CheckId(document.id); !valid) {
    return std::unexpected(valid.error());
  }
  if (auto valid = CheckProvenance(document.provenance); !valid) {
    return std::unexpected(std::format("{}: {}", document.id, valid.error()));
  }
  const Content::NativeImage& image = document.image;
  if (auto valid = Content::ValidateImage(image); !valid) {
    return std::unexpected(std::format("{}: {}", document.id, valid.error()));
  }
  const std::string png_name = NameOf(stem, Content::kImagePngSuffix);
  auto png = Content::IsIndexed(image.depth)
                 ? EncodeIndexedPng(image, png_name)
                 : EncodeRgbaPng(ColorsOf(image.texels), image.width,
                                 image.height, png_name);
  if (!png) return std::unexpected(png.error());
  std::vector<ImageFile> files;
  files.push_back({.name = png_name, .bytes = ToBytes(*png)});
  files.push_back({.name = NameOf(stem, Content::kImageSidecarSuffix),
                   .bytes = TextBytes(SidecarText(document))});
  if (image.palettes.size() > 1) {
    const std::string name = NameOf(stem, Content::kImagePalettesSuffix);
    auto table =
        EncodeRgbaPng(ColorsOf(Content::PaletteWords(image)),
                      static_cast<std::uint32_t>(image.PaletteEntries()),
                      static_cast<std::uint32_t>(image.palettes.size()), name);
    if (!table) return std::unexpected(table.error());
    files.push_back({.name = name, .bytes = ToBytes(*table)});
  }
  if (!Content::IsIndexed(image.depth) &&
      CoverageOf(image.texels) == StpCoverage::kSome) {
    const std::string name = NameOf(stem, Content::kImageStpSuffix);
    auto mask = EncodeStpMask(image, name);
    if (!mask) return std::unexpected(mask.error());
    files.push_back({.name = name, .bytes = ToBytes(*mask)});
  }
  return files;
}

std::expected<void, std::string> WriteImageFiles(
    const fs::path& directory, const std::vector<ImageFile>& files) {
  std::error_code error;
  fs::create_directories(directory, error);
  if (error) {
    return std::unexpected(std::format("cannot create {}: {}",
                                       Platform::PathText(directory),
                                       error.message()));
  }
  std::vector<fs::path> staged;
  const auto discard = [&staged] {
    std::error_code ignored;
    for (const fs::path& path : staged) fs::remove(path, ignored);
  };
  for (const ImageFile& file : files) {
    if (auto valid = CheckStem(file.name); !valid) {
      discard();
      return std::unexpected(valid.error());
    }
    fs::path temporary = directory / Platform::Utf8Path(file.name + ".tmp");
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    staged.push_back(temporary);
    const PngBytes data = ToPngBytes(file.bytes);
    output.write(reinterpret_cast<const char*>(data.data()),
                 static_cast<std::streamsize>(data.size()));
    output.close();
    if (!output) {
      discard();
      return std::unexpected(
          std::format("cannot write {}", Platform::PathText(temporary)));
    }
  }
  for (std::size_t index = 0; index < files.size(); ++index) {
    fs::rename(staged[index], directory / Platform::Utf8Path(files[index].name),
               error);
    if (error) {
      return std::unexpected(std::format(
          "cannot replace {}: {}",
          Platform::PathText(directory / Platform::Utf8Path(files[index].name)),
          error.message()));
    }
  }
  return {};
}

}  // namespace VS::Extract
