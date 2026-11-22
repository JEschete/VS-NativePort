// The wep-gltf converter (WepModelConversion.hpp).
#include "Extract/WepModelConversion.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <format>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Content/ConversionManifest.hpp"
#include "Content/DataStore.hpp"
#include "Content/WepFile.hpp"
#include "Content/WepModelFiles.hpp"
#include "Extract/ImageConversion.hpp"
#include "Extract/ImageFilesWriter.hpp"

namespace VS::Extract {
using Content::ConversionOutput;
using Content::ImageDocument;
using Content::ImageFile;
using Content::WepFile;
using Content::WepModelDocument;

namespace {
namespace fs = std::filesystem;
using Check = std::expected<void, std::string>;
template <typename T>
using Result = std::expected<T, std::string>;

constexpr std::string_view kRawForm = "raw";
constexpr std::string_view kModelIdPrefix = "model";
constexpr std::string_view kGenerator = "VS.Extract wep-gltf 1";
// The image's provenance offset is the texture's flags byte, after its size
// field, as an SHP texture's is; the flags, width, height and palette words
// bytes start it.
constexpr std::size_t kTextureHeaderBytes = 4;
constexpr std::size_t kRotationBytes = 4 * sizeof(std::int16_t);

// The disc file's stem: OBJ/01.WEP -> 01.
Result<std::string> StemOf(std::string_view disc_path) {
  auto stem = DiscStemOf(disc_path, Content::kWepExtension);
  if (!stem) {
    return std::unexpected(
        std::format("'{}' is not a WEP weapon model", disc_path));
  }
  return std::move(*stem);
}

std::size_t TextureOffset(const WepFile& wep, std::size_t file_bytes) {
  const Content::WepTexture& texture = wep.texture;
  const std::size_t colors =
      texture.HandleWords() +
      (Content::kWepMaterialCount * texture.MaterialWords());
  return file_bytes - kTextureHeaderBytes - (colors * sizeof(std::uint16_t)) -
         texture.pixels.size() - (wep.rotations.size() * kRotationBytes);
}

Result<ImageDocument> TextureImageOf(const WepModelDocument& document) {
  auto encoded = Content::EncodeWep(document.wep);
  if (!encoded) return std::unexpected(encoded.error());
  return ImageDocument{
      .id = document.image_id,
      .image = Content::WepTextureImage(document.wep.texture),
      .material = std::nullopt,
      .unreferenced = false,
      .provenance = {.source = document.source,
                     .source_sha256 = document.source_sha256,
                     .offset = TextureOffset(document.wep, encoded->size()),
                     .tim = std::nullopt,
                     .texture_page = std::nullopt,
                     .preview_palette = std::nullopt,
                     .dependencies = {},
                     .note = {}}};
}

// The texture through palette row `row` alone, as the PNG named `stem`.
Result<ImageFile> PreviewOf(const ImageDocument& image, std::size_t row,
                            std::string_view stem) {
  ImageDocument preview = image;
  preview.image.palettes = {image.image.palettes.at(row)};
  auto files = EncodeImageFiles(preview, stem);
  if (!files) return std::unexpected(files.error());
  const std::string name = std::format("{}{}", stem, Content::kImagePngSuffix);
  const auto png = std::ranges::find(*files, name, &ImageFile::name);
  if (png == files->end()) {
    return std::unexpected(std::format("{} was not encoded", name));
  }
  return std::move(*png);
}

// The texture's files, then a preview for each material after the first.
Result<std::vector<ImageFile>> TextureFiles(
    const WepModelDocument& document, std::string_view stem,
    std::span<const std::string> preview_stems) {
  auto image = TextureImageOf(document);
  if (!image) return std::unexpected(image.error());
  auto files = EncodeImageFiles(*image, stem);
  if (!files) return std::unexpected(files.error());
  for (std::size_t row = 1; row < preview_stems.size(); ++row) {
    auto preview = PreviewOf(*image, row, preview_stems[row]);
    if (!preview) return std::unexpected(preview.error());
    files->push_back(std::move(*preview));
  }
  return files;
}

// The files must read back to `document` and rebuild the source bytes.
Check VerifyWeapon(const WepModelDocument& document, std::string_view stem,
                   const std::vector<ImageFile>& files,
                   const SourceFile& source) {
  auto decoded =
      Content::DecodeWepModelFiles(stem, Content::FileListReader(files));
  if (!decoded) {
    return std::unexpected(
        std::format("{}: the converted weapon does not read back: {}",
                    source.path, decoded.error()));
  }
  if (*decoded != document) {
    return std::unexpected(
        std::format("{}: the converted weapon reads back to a different weapon",
                    source.path));
  }
  auto encoded = Content::EncodeWep(decoded->wep);
  if (!encoded || *encoded != source.bytes) {
    return std::unexpected(std::format(
        "{}: the converted weapon does not rebuild the source bytes",
        source.path));
  }
  return {};
}

// The manifest entry `source` makes, without its files.
Result<ConversionOutput> WeaponOutputOf(const SourceFile& source) {
  auto id = WepModelIdForDiscPath(source.path);
  if (!id) return std::unexpected(id.error());
  return ConversionOutput{
      .id = std::move(*id),
      .source = source.path,
      .source_sha256 = HashOf(source),
      .converter = std::string(Content::kWepModelConverterName),
      .files = {},
      .dependencies = {},
      .unreferenced = false,
      .note = {},
      .alias_of = {}};
}

Result<WepModelDocument> DocumentOf(const SourceFile& source) {
  auto wep = Content::DecodeWep(source.bytes);
  if (!wep) {
    return std::unexpected(std::format("{}: {}", source.path, wep.error()));
  }
  auto id = WepModelIdForDiscPath(source.path);
  if (!id) return std::unexpected(id.error());
  auto image = WepImageIdForDiscPath(source.path);
  if (!image) return std::unexpected(image.error());
  return WepModelDocument{.id = std::move(*id),
                          .source = source.path,
                          .source_sha256 = HashOf(source),
                          .image_id = std::move(*image),
                          .wep = std::move(*wep)};
}

}  // namespace

std::expected<std::string, std::string> WepImageIdForDiscPath(
    std::string_view disc_path) {
  if (auto stem = StemOf(disc_path); !stem) {
    return std::unexpected(stem.error());
  }
  auto id = ImageIdForDiscPath(disc_path);
  if (!id) return std::unexpected(id.error());
  id->append(Content::kWepIdSuffix);
  if (!Content::IsContentId(*id)) {
    return std::unexpected(
        std::format("'{}' gives no valid weapon image id", disc_path));
  }
  return id;
}

std::expected<std::string, std::string> WepModelIdForDiscPath(
    std::string_view disc_path) {
  auto image = WepImageIdForDiscPath(disc_path);
  if (!image) return std::unexpected(image.error());
  // image.<directories>.<stem>.wep -> model.<directories>.<stem>.wep
  std::string id =
      std::format("{}{}", kModelIdPrefix,
                  std::string_view(*image).substr(image->find('.')));
  if (!Content::IsContentId(id)) {
    return std::unexpected(
        std::format("'{}' gives no valid weapon model id", disc_path));
  }
  return id;
}

std::expected<std::vector<ImageFile>, std::string> EncodeWepModelFiles(
    const WepModelDocument& document, std::string_view stem) {
  const WepFile& wep = document.wep;
  // Variant 0 shows the texture itself; the others show their previews.
  std::array<std::string, Content::kWepMaterialCount> preview_stems;
  std::array<std::string, Content::kWepMaterialCount> image_uris;
  std::array<ModelMaterialVariant, Content::kWepMaterialCount> variants;
  for (std::size_t row = 0; row < variants.size(); ++row) {
    const std::string_view name = Content::kWepMaterialNames.at(row);
    preview_stems.at(row) = row == 0 ? std::string(stem)
                                     : std::format("{}.{}{}", stem, name,
                                                   Content::kWepPreviewSuffix);
    image_uris.at(row) =
        std::format("{}{}", preview_stems.at(row), Content::kImagePngSuffix);
    variants.at(row) = {.name = name, .image_uri = image_uris.at(row)};
  }
  const ModelGltfSpec spec{.stem = stem,
                           .image_uri = image_uris[0],
                           .generator = kGenerator,
                           .uv_scale = {.width = wep.texture.Width(),
                                        .height = Content::kWepTextureRows},
                           .skeleton = Content::WepSkeletonGltf(wep, 0),
                           .payload =
                               [&document](const ModelViewAdder& /*add_view*/) {
                                 return Content::WepModelPayload(document);
                               },
                           .variants = variants};
  auto files = EncodeModelGltf(wep, spec);
  if (!files) {
    return std::unexpected(
        std::format("{}: {}", document.source, files.error()));
  }
  auto texture = TextureFiles(document, stem, preview_stems);
  if (!texture) {
    return std::unexpected(
        std::format("{}: {}", document.source, texture.error()));
  }
  std::ranges::move(*texture, std::back_inserter(*files));
  return files;
}

std::expected<ConvertedModel, std::string> ConvertWepModel(
    const SourceFile& source) {
  auto document = DocumentOf(source);
  if (!document) return std::unexpected(document.error());
  auto stem = StemOf(source.path);
  if (!stem) return std::unexpected(stem.error());
  auto files = EncodeWepModelFiles(*document, *stem);
  if (!files) return std::unexpected(files.error());
  if (auto verified = VerifyWeapon(*document, *stem, *files, source);
      !verified) {
    return std::unexpected(verified.error());
  }
  auto output = WeaponOutputOf(source);
  if (!output) return std::unexpected(output.error());
  return ConvertedModelOf(std::move(*output), std::move(*files),
                          Content::kWepModelDirectory);
}

std::expected<ModelConversionReport, std::string> ConvertDiscWeapons(
    const fs::path& root, bool force) {
  auto start = StartRun(root);
  if (!start) return std::unexpected(start.error());
  std::vector<ModelJob> jobs;
  for (const Content::DataStore::Entry& entry : start->entries) {
    if (entry.form != kRawForm ||
        !entry.path.ends_with(Content::kWepExtension)) {
      continue;
    }
    auto source = ReadSource(root, entry.path);
    if (!source) return std::unexpected(source.error());
    auto expected = WeaponOutputOf(*source);
    if (!expected) return std::unexpected(expected.error());
    jobs.push_back({.expected = std::move(*expected),
                    .convert = [source = std::move(*source)] {
                      return ConvertWepModel(source);
                    }});
  }
  return RunModelConverter(root, *start,
                           {.name = Content::kWepModelConverterName,
                            .version = Content::kWepModelConverterVersion,
                            .directory = Content::kWepModelDirectory,
                            .force = force},
                           jobs);
}

}  // namespace VS::Extract
