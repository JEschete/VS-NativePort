// The tim-png and fbc-palette-png converters, and the batch that runs them with
// fbt-texture-png (ImageConversion.hpp).
#include "Extract/ImageConversion.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <ranges>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "Content/BackgroundAssets.hpp"
#include "Content/DataStore.hpp"
#include "Content/JsonText.hpp"
#include "Content/TimFile.hpp"
#include "Extract/BackgroundAuthoring.hpp"
#include "Extract/ConverterBatch.hpp"
#include "Extract/EffectTextureConversion.hpp"
#include "Platform/Paths.hpp"

namespace VS::Extract {
using Content::ConversionManifest;
using Content::ConversionOutput;
using Content::DataStore;
using Content::DecodeImageFiles;
using Content::DecodeTim;
using Content::ImageDepth;
using Content::ImageDocument;
using Content::ImageFile;
using Content::ImageMaterial;
using Content::ImageProvenance;
using Content::IsContentId;
using Content::IsDataRelativePath;
using Content::kImagePngSuffix;
using Content::SemiTransparency;
using Content::SourceDependency;
using Content::TimFile;
using Content::TimPixelMode;
using Content::ZeroWord;

namespace {
namespace fs = std::filesystem;

constexpr std::string_view kTexturesDirectory = "textures";
constexpr std::string_view kRootDirectory = "root";
constexpr std::string_view kImageIdPrefix = "image";
constexpr std::string_view kRawForm = "raw";
constexpr std::string_view kTimExtension = ".TIM";
constexpr std::string_view kFbcExtension = ".FBC";
constexpr std::string_view kLayoutExtension = ".FAR";
constexpr std::string_view kBackgroundDirectory = "BG";
constexpr std::string_view kLayoutRole = "layout";
constexpr unsigned kByteBits = 8;
constexpr std::size_t kFbcPaletteEntries = 256;
constexpr std::size_t kFbcWordBytes = sizeof(std::uint16_t);
constexpr std::size_t kFbcRowBytes = kFbcPaletteEntries * kFbcWordBytes;
constexpr std::size_t kMaxFbcPaletteRows = 5;
constexpr std::string_view kFbcPaletteDirectory = "palettes";

// vs_battle_backgroundLayout (2842C.h:12): {int width, height, unk8}, then
// width x height tiles. A tile (2842C.h:7) is a RECT {x, y, w, h} of int16
// texture coordinates and four CVECTOR corners {r, g, b, cd}.
constexpr std::size_t kLayoutHeaderBytes = 12;
constexpr std::size_t kLayoutHeightOffset = 4;
constexpr std::size_t kTileBytes = 24;
constexpr std::size_t kTileWidthOffset = 4;
constexpr std::size_t kTileHeightOffset = 6;
constexpr std::size_t kTileRectBytes = 8;
constexpr std::size_t kCornerBytes = 4;
constexpr std::size_t kCornerCodeOffset = 3;
constexpr std::size_t kDepthCorner = 1;
constexpr auto kDepthOffset = kDepthCorner * kCornerBytes + kCornerCodeOffset;
constexpr auto kTileDepthCodeOffset = kTileRectBytes + kDepthOffset;

// The texture-page depth bits 7-8 hold 1: 8-bit indices.
constexpr std::uint8_t kEightBitCode = 1;

constexpr auto kBackgroundNote = "16-bit TIM; FAR uses 8-bit indices";

// A standalone TIM nothing on the disc reads, and how that is known.
struct UnreferencedImage {
  std::string_view path;
  std::string_view note;
};
constexpr std::array kUnreferencedImages{
    UnreferencedImage{
        .path = "DBGFNT.TIM",
        .note = "A 4-bit debug font with two palettes. No program, zone or "
                "room names DBGFNT or holds its sector and size, so nothing "
                "on the disc loads it."},
};

std::uint32_t ReadLittleEndian(std::span<const std::byte> data,
                               std::size_t offset, std::size_t bytes) {
  std::uint32_t value = 0;
  for (std::size_t index = 0; index < bytes; ++index) {
    value |= std::to_integer<std::uint32_t>(data[offset + index])
             << (index * kByteBits);
  }
  return value;
}

std::string Lowercase(std::string_view text) {
  std::string lower(text);
  std::ranges::transform(lower, lower.begin(), [](char value) {
    return value >= 'A' && value <= 'Z' ? static_cast<char>(value - 'A' + 'a')
                                        : value;
  });
  return lower;
}

// A disc path's directories and its file name without the extension.
struct DiscPathParts {
  std::vector<std::string> directories;  // lowercase
  std::string stem;
};

std::expected<DiscPathParts, std::string> SplitDiscPath(std::string_view path) {
  if (!IsDataRelativePath(path)) {
    return std::unexpected(
        std::format("'{}' is not a '/'-separated disc path", path));
  }
  DiscPathParts parts;
  const std::size_t name_start = path.rfind('/');
  const std::string_view name =
      name_start == std::string_view::npos ? path : path.substr(name_start + 1);
  if (name_start != std::string_view::npos) {
    for (const auto part : std::views::split(path.substr(0, name_start), '/')) {
      parts.directories.push_back(
          Lowercase(std::string_view(part.begin(), part.end())));
    }
  }
  parts.stem = std::string(name.substr(0, name.rfind('.')));
  if (parts.stem.empty()) {
    return std::unexpected(std::format("'{}' names no file stem", path));
  }
  return parts;
}

std::string JoinDirectories(const DiscPathParts& parts, char separator) {
  if (parts.directories.empty()) return std::string(kRootDirectory);
  std::string joined;
  for (const std::string& directory : parts.directories) {
    if (!joined.empty()) joined += separator;
    joined += directory;
  }
  return joined;
}

std::expected<ImageLocation, std::string> FbcPaletteLocationForDiscPath(
    std::string_view path) {
  auto location = ImageLocationForDiscPath(path);
  if (!location) return std::unexpected(location.error());
  location->directory += std::format("/{}", kFbcPaletteDirectory);
  return location;
}

std::vector<SourceDependency> DependenciesOf(const TimConversion& conversion) {
  if (!conversion.layout) return {};
  return {{.source = conversion.layout->path,
           .source_sha256 = HashOf(*conversion.layout)}};
}

// The depth the image is drawn at: its layout's, else the TIM's own mode.
std::expected<ImageDepth, std::string> DepthOf(const TimConversion& conversion,
                                               const TimFile& tim) {
  if (conversion.layout) {
    auto depth = BackgroundLayoutDepth(conversion.layout->bytes);
    if (!depth) {
      return std::unexpected(
          std::format("{}: {}", conversion.layout->path, depth.error()));
    }
    return *depth;
  }
  switch (tim.Mode()) {
    case TimPixelMode::kIndexed4:
      return ImageDepth::kIndexed4;
    case TimPixelMode::kIndexed8:
      return ImageDepth::kIndexed8;
    case TimPixelMode::kDirect15:
      return ImageDepth::kDirect16;
    case TimPixelMode::kDirect24:
      break;
  }
  return std::unexpected(std::format(
      "{}: 24-bit TIMs have no native image depth", conversion.tim.path));
}

ImageProvenance ProvenanceOf(const TimConversion& conversion,
                             const TimFile& tim) {
  ImageProvenance provenance{.source = conversion.tim.path,
                             .source_sha256 = HashOf(conversion.tim),
                             .offset = 0,
                             .tim = Content::OriginOf(tim),
                             .texture_page = std::nullopt,
                             .preview_palette = std::nullopt,
                             .dependencies = {},
                             .note = conversion.note};
  for (const SourceDependency& dependency : DependenciesOf(conversion)) {
    provenance.dependencies.push_back(
        {.source = dependency.source,
         .source_sha256 = dependency.source_sha256,
         .role = std::string(kLayoutRole)});
  }
  return provenance;
}

// The files must decode to the same document and rebuild the source bytes.
std::expected<void, std::string> VerifyRoundTrip(
    const ImageDocument& document, std::string_view stem,
    const std::vector<ImageFile>& files, const SourceFile& source) {
  auto decoded = DecodeConvertedFiles(document, stem, files, source);
  if (!decoded) return std::unexpected(decoded.error());
  if (!decoded->provenance.tim) {
    return std::unexpected(std::format(
        "{}: the converted files decode to a different image", source.path));
  }
  auto tim = Content::TimFromImage(decoded->image, *decoded->provenance.tim);
  if (!tim) {
    return std::unexpected(std::format("{}: {}", source.path, tim.error()));
  }
  auto bytes = Content::EncodeTim(*tim);
  if (!bytes) {
    return std::unexpected(std::format("{}: {}", source.path, bytes.error()));
  }
  if (*bytes != source.bytes) {
    return std::unexpected(
        std::format("{}: the converted image does not rebuild the source TIM",
                    source.path));
  }
  return {};
}

// The palette files must decode to the same document and rebuild every
// source colour word.
std::expected<void, std::string> VerifyFbcRoundTrip(
    const ImageDocument& document, std::string_view stem,
    const std::vector<ImageFile>& files, const SourceFile& source) {
  auto decoded = DecodeConvertedFiles(document, stem, files, source);
  if (!decoded) return std::unexpected(decoded.error());
  const std::vector<std::uint16_t> words = Content::PackedWords(decoded->image);
  if (words.size() * kFbcWordBytes != source.bytes.size()) {
    return std::unexpected(std::format(
        "{}: the palette image has a different byte length", source.path));
  }
  for (std::size_t index = 0; index < words.size(); ++index) {
    if (words[index] !=
        ReadLittleEndian(source.bytes, index * kFbcWordBytes, kFbcWordBytes)) {
      return std::unexpected(
          std::format("{}: palette word {} does not rebuild the source",
                      source.path, index));
    }
  }
  return {};
}

std::expected<TimConversion, std::string> PlanBackground(
    const fs::path& root, const std::vector<DataStore::Entry>& entries,
    SourceFile tim, std::string_view stem) {
  if (Content::BackgroundAssetForSource(tim.path) == nullptr) {
    return std::unexpected(tim.path +
                           ": background artwork identity is unresolved");
  }
  const std::string layout_path =
      std::format("{}/{}{}", kBackgroundDirectory, stem, kLayoutExtension);
  const bool listed = std::ranges::any_of(
      entries, [&layout_path](const DataStore::Entry& entry) {
        return entry.form == kRawForm && entry.path == layout_path;
      });
  if (!listed) {
    return std::unexpected(
        std::format("{}: its background layout {} is not in files.tsv",
                    tim.path, layout_path));
  }
  auto layout = ReadSource(root, layout_path);
  if (!layout) return std::unexpected(layout.error());
  // decomp: BATTLE.PRG func_8009147C (src/BATTLE/BATTLE.PRG/2842C.c:500-510)
  // draws each tile as an opaque POLY_GT4 (setPolyGT4 without SetSemiTrans),
  // so the STP bit never blends, and textured primitives skip the word 0.
  return TimConversion{
      .tim = std::move(tim),
      .layout = std::move(*layout),
      .material = ImageMaterial{.zero_word = ZeroWord::kTransparent,
                                .semi_transparency = SemiTransparency::kOff},
      .unreferenced = false,
      .note = std::string(kBackgroundNote)};
}

std::expected<TimConversion, std::string> PlanConversion(
    const fs::path& root, const std::vector<DataStore::Entry>& entries,
    std::string_view path) {
  auto tim = ReadSource(root, path);
  if (!tim) return std::unexpected(tim.error());
  const std::size_t name_start = path.rfind('/');
  if (name_start != std::string_view::npos &&
      path.substr(0, name_start) == kBackgroundDirectory) {
    const std::string_view name = path.substr(name_start + 1);
    return PlanBackground(root, entries, std::move(*tim),
                          name.substr(0, name.size() - kTimExtension.size()));
  }
  const auto unreferenced =
      std::ranges::find(kUnreferencedImages, path, &UnreferencedImage::path);
  if (unreferenced == kUnreferencedImages.end()) {
    return std::unexpected(std::format(
        "{}: no consumer or unreferenced entry is known for this TIM", path));
  }
  return TimConversion{.tim = std::move(*tim),
                       .layout = std::nullopt,
                       .material = std::nullopt,
                       .unreferenced = true,
                       .note = std::string(unreferenced->note)};
}

std::expected<std::vector<TimConversion>, std::string> PlanConversions(
    const fs::path& root, const std::vector<DataStore::Entry>& entries) {
  std::vector<TimConversion> conversions;
  for (const DataStore::Entry& entry : entries) {
    if (entry.form != kRawForm || !entry.path.ends_with(kTimExtension)) {
      continue;
    }
    auto conversion = PlanConversion(root, entries, entry.path);
    if (!conversion) return std::unexpected(conversion.error());
    conversions.push_back(std::move(*conversion));
  }
  return conversions;
}

std::expected<std::vector<SourceFile>, std::string> PlanFbcConversions(
    const fs::path& root, const std::vector<DataStore::Entry>& entries) {
  std::vector<SourceFile> sources;
  for (const DataStore::Entry& entry : entries) {
    if (entry.form != kRawForm || !entry.path.ends_with(kFbcExtension)) {
      continue;
    }
    auto source = ReadSource(root, entry.path);
    if (!source) return std::unexpected(source.error());
    sources.push_back(std::move(*source));
  }
  return sources;
}

// The manifest's output for `conversion` when its inputs and flags are the
// ones recorded and its files exist.
const ConversionOutput* UnchangedOutput(const ConversionManifest& manifest,
                                        const fs::path& root,
                                        const TimConversion& conversion) {
  const auto id = ImageIdForDiscPath(conversion.tim.path);
  if (!id) return nullptr;
  const ConversionOutput* output = manifest.Find(*id);
  if (output == nullptr || output->converter != kTimConverterName ||
      output->source != conversion.tim.path ||
      output->source_sha256 != HashOf(conversion.tim) ||
      output->dependencies != DependenciesOf(conversion) ||
      output->unreferenced != conversion.unreferenced ||
      output->note != conversion.note) {
    return nullptr;
  }
  return PresentOutput(root, output);
}

// The manifest's output for the FBC `source` when its hash is the one
// recorded and its files exist.
const ConversionOutput* UnchangedFbcOutput(const ConversionManifest& manifest,
                                           const fs::path& root,
                                           const SourceFile& source) {
  const auto id = FbcPaletteIdForDiscPath(source.path);
  if (!id) return nullptr;
  const ConversionOutput* output = manifest.Find(*id);
  if (output == nullptr || output->converter != kFbcPaletteConverterName ||
      output->source != source.path ||
      output->source_sha256 != HashOf(source) ||
      !output->dependencies.empty() || output->unreferenced ||
      !output->note.empty()) {
    return nullptr;
  }
  return PresentOutput(root, output);
}

// The images a run converted, which nothing has written yet.
struct ConversionRun {
  ImageConversionReport report;
  std::vector<ConvertedImage> converted;
};

// Records `kept` in `batch`, or else the image `convert` makes.
template <typename Convert>
std::expected<void, std::string> Record(ConversionRun& run,
                                        ConverterBatch& batch,
                                        const ConversionOutput* kept,
                                        Convert convert) {
  if (kept != nullptr) {
    batch.outputs.push_back(*kept);
    ++run.report.unchanged;
    return {};
  }
  auto image = convert();
  if (!image) return std::unexpected(image.error());
  batch.outputs.push_back(image->output);
  run.converted.push_back(std::move(*image));
  return {};
}

std::expected<ConverterBatch, std::string> ConvertTims(
    const fs::path& root, const ConversionManifest& previous,
    const std::vector<DataStore::Entry>& entries, bool force,
    ConversionRun& run) {
  auto conversions = PlanConversions(root, entries);
  if (!conversions) return std::unexpected(conversions.error());
  ConverterBatch batch =
      StartBatch(previous, kTimConverterName, kTimConverterVersion, force);
  for (const TimConversion& conversion : *conversions) {
    const ConversionOutput* kept =
        batch.reuse ? UnchangedOutput(previous, root, conversion) : nullptr;
    if (auto recorded = Record(
            run, batch, kept, [&conversion] { return ConvertTim(conversion); });
        !recorded) {
      return std::unexpected(recorded.error());
    }
  }
  if (auto aliases = KeepBackgroundCompatibility(batch, previous); !aliases) {
    return std::unexpected(aliases.error());
  }
  return batch;
}

// The manifest's output for `conversion` when every input and flag is the one
// recorded and its files exist.
const ConversionOutput* UnchangedFbtOutput(const ConversionManifest& manifest,
                                           const fs::path& root,
                                           const FbtConversion& conversion) {
  auto expected = FbtOutputOf(conversion);
  if (!expected) return nullptr;
  return MatchingOutput(manifest, root, *expected);
}

std::expected<ConverterBatch, std::string> ConvertFbtTextures(
    const fs::path& root, const ConversionManifest& previous,
    const std::vector<DataStore::Entry>& entries, bool force,
    ConversionRun& run) {
  auto plan = PlanFbtConversions(entries, [&root](std::string_view path) {
    return ReadSource(root, path);
  });
  if (!plan) return std::unexpected(plan.error());
  ConverterBatch batch = StartBatch(previous, kFbtTextureConverterName,
                                    kFbtTextureConverterVersion, force);
  for (const FbtConversion& conversion : plan->conversions) {
    const ConversionOutput* kept =
        batch.reuse ? UnchangedFbtOutput(previous, root, conversion) : nullptr;
    if (auto recorded =
            Record(run, batch, kept,
                   [&conversion] { return ConvertFbtTexture(conversion); });
        !recorded) {
      return std::unexpected(recorded.error());
    }
  }
  for (ConversionOutput& alias : plan->aliases) {
    batch.outputs.push_back(std::move(alias));
    ++run.report.aliased;
  }
  return batch;
}

std::expected<ConverterBatch, std::string> ConvertFbcPalettes(
    const fs::path& root, const ConversionManifest& previous,
    const std::vector<DataStore::Entry>& entries, bool force,
    ConversionRun& run) {
  auto sources = PlanFbcConversions(root, entries);
  if (!sources) return std::unexpected(sources.error());
  ConverterBatch batch = StartBatch(previous, kFbcPaletteConverterName,
                                    kFbcPaletteConverterVersion, force);
  for (const SourceFile& source : *sources) {
    const ConversionOutput* kept =
        batch.reuse ? UnchangedFbcOutput(previous, root, source) : nullptr;
    if (auto recorded = Record(run, batch, kept,
                               [&source] { return ConvertFbcPalette(source); });
        !recorded) {
      return std::unexpected(recorded.error());
    }
  }
  return batch;
}

// Writes the converted images, then records the batches (PublishBatches), and
// returns how many stale files it removed.
std::expected<std::size_t, std::string> Publish(
    const fs::path& root, const ConversionManifest& previous,
    const ConversionRun& run, std::span<const ConverterBatch> batches) {
  for (const ConvertedImage& image : run.converted) {
    if (auto written = WriteImageFiles(
            root / Platform::Utf8Path(image.location.directory), image.files);
        !written) {
      return std::unexpected(written.error());
    }
  }
  return PublishBatches(root, previous, batches);
}

}  // namespace

std::expected<ImageDocument, std::string> DecodeConvertedFiles(
    const ImageDocument& document, std::string_view stem,
    const std::vector<ImageFile>& files, const SourceFile& source) {
  auto decoded = DecodeImageFiles(stem, Content::FileListReader(files));
  if (!decoded) {
    return std::unexpected(
        std::format("{}: the converted files do not decode: {}", source.path,
                    decoded.error()));
  }
  if (*decoded != document) {
    return std::unexpected(std::format(
        "{}: the converted files decode to a different image", source.path));
  }
  return decoded;
}

ConvertedImage ListConvertedImage(ConversionOutput output,
                                  ImageLocation location,
                                  std::vector<ImageFile> files) {
  for (const ImageFile& file : files) {
    output.files.push_back(std::format("{}/{}", location.directory, file.name));
  }
  return ConvertedImage{.output = std::move(output),
                        .location = std::move(location),
                        .files = std::move(files)};
}

std::expected<std::string, std::string> ImageIdForDiscPath(
    std::string_view disc_path) {
  if (const auto* background = Content::BackgroundAssetForSource(disc_path)) {
    return Content::BackgroundImageId(*background);
  }
  auto parts = SplitDiscPath(disc_path);
  if (!parts) return std::unexpected(parts.error());
  std::string id =
      std::format("{}.{}.{}", kImageIdPrefix, JoinDirectories(*parts, '.'),
                  Lowercase(parts->stem));
  if (!IsContentId(id)) {
    return std::unexpected(std::format(
        "'{}' gives the image id '{}', which is not valid", disc_path, id));
  }
  return id;
}

std::expected<ImageLocation, std::string> ImageLocationForDiscPath(
    std::string_view disc_path) {
  if (const auto* background = Content::BackgroundAssetForSource(disc_path)) {
    return ImageLocation{.directory = "textures/bg",
                         .stem = std::string(background->slug)};
  }
  auto parts = SplitDiscPath(disc_path);
  if (!parts) return std::unexpected(parts.error());
  ImageLocation location{.directory = std::format("{}/{}", kTexturesDirectory,
                                                  JoinDirectories(*parts, '/')),
                         .stem = parts->stem};
  if (!IsDataRelativePath(std::format("{}/{}{}", location.directory,
                                      location.stem, kImagePngSuffix))) {
    return std::unexpected(std::format(
        "'{}' gives no valid image location under the data root", disc_path));
  }
  return location;
}

std::expected<std::string, std::string> FbcPaletteIdForDiscPath(
    std::string_view disc_path) {
  auto id = ImageIdForDiscPath(disc_path);
  if (!id) return std::unexpected(id.error());
  id->append(".palette");
  if (!IsContentId(*id)) {
    return std::unexpected(
        std::format("'{}' gives no valid FBC palette id", disc_path));
  }
  return id;
}

std::expected<ImageDepth, std::string> BackgroundLayoutDepth(
    std::span<const std::byte> layout) {
  if (layout.size() < kLayoutHeaderBytes) {
    return std::unexpected(
        std::format("a background layout needs a {}-byte header; it has {} "
                    "bytes",
                    kLayoutHeaderBytes, layout.size()));
  }
  const auto width = static_cast<std::int32_t>(
      ReadLittleEndian(layout, 0, sizeof(std::int32_t)));
  const auto height = static_cast<std::int32_t>(
      ReadLittleEndian(layout, kLayoutHeightOffset, sizeof(std::int32_t)));
  if (width <= 0 || height <= 0) {
    return std::unexpected(std::format(
        "the layout is {}x{} tiles; both must be positive", width, height));
  }
  const std::uint64_t tiles =
      static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height);
  if (layout.size() != kLayoutHeaderBytes + (tiles * kTileBytes)) {
    return std::unexpected(
        std::format("a {}x{} layout takes {} bytes; it has {}", width, height,
                    kLayoutHeaderBytes + (tiles * kTileBytes), layout.size()));
  }
  bool drawn = false;
  for (std::size_t tile = 0; tile < tiles; ++tile) {
    const std::size_t offset = kLayoutHeaderBytes + (tile * kTileBytes);
    // decomp: BATTLE.PRG func_8009147C (src/BATTLE/BATTLE.PRG/2842C.c:491)
    // skips a tile without width and height.
    if (ReadLittleEndian(layout, offset + kTileWidthOffset,
                         sizeof(std::int16_t)) == 0 &&
        ReadLittleEndian(layout, offset + kTileHeightOffset,
                         sizeof(std::int16_t)) == 0) {
      continue;
    }
    // decomp: BATTLE.PRG func_8009147C (src/BATTLE/BATTLE.PRG/2842C.c:509)
    // draws with the texture page
    // page | (colors[1].cd << 7). The page is the event script's byte operand
    // (vs_battle_script_74, src/BATTLE/BATTLE.PRG/4DEA8.c:2813), which can set
    // depth bit 7 but not bit 8, so only code 1 (8-bit) fixes the depth without
    // the script.
    const auto code =
        std::to_integer<std::uint8_t>(layout[offset + kTileDepthCodeOffset]);
    if (code != kEightBitCode) {
      return std::unexpected(std::format(
          "tile {} has depth code {}; only code {} (8-bit) fixes the depth the "
          "texture is drawn at",
          tile, code, kEightBitCode));
    }
    drawn = true;
  }
  if (!drawn) {
    return std::unexpected("the layout draws no tiles, so it fixes no depth");
  }
  return ImageDepth::kIndexed8;
}

std::expected<ConvertedImage, std::string> ConvertTim(
    const TimConversion& conversion) {
  const SourceFile& source = conversion.tim;
  auto decoded = DecodeTim(source.bytes);
  if (!decoded) {
    return std::unexpected(
        std::format("{}: {}", source.path, decoded.error().message));
  }
  if (decoded->bytes != source.bytes.size()) {
    return std::unexpected(std::format("{}: {} bytes follow the TIM",
                                       source.path,
                                       source.bytes.size() - decoded->bytes));
  }
  auto depth = DepthOf(conversion, decoded->tim);
  if (!depth) return std::unexpected(depth.error());
  auto image = ImageFromTim(decoded->tim, *depth);
  if (!image) {
    return std::unexpected(std::format("{}: {}", source.path, image.error()));
  }
  auto id = ImageIdForDiscPath(source.path);
  if (!id) return std::unexpected(id.error());
  auto location = ImageLocationForDiscPath(source.path);
  if (!location) return std::unexpected(location.error());
  const ImageDocument document{
      .id = *id,
      .image = std::move(*image),
      .material = conversion.material,
      .unreferenced = conversion.unreferenced,
      .provenance = ProvenanceOf(conversion, decoded->tim)};
  auto files = EncodeImageFiles(document, location->stem);
  if (!files) {
    return std::unexpected(std::format("{}: {}", source.path, files.error()));
  }
  if (const auto* background = Content::BackgroundAssetForSource(source.path)) {
    for (auto& file : *files) {
      if (!file.name.ends_with(Content::kImageSidecarSuffix)) continue;
      auto json = Content::ParseJsonText(file.bytes, file.name);
      if (!json) return std::unexpected(json.error());
      (*json)[Content::kDisplayName] =
          "Background image: " + std::string(background->display_name);
      const auto text = Content::FormatJsonText(*json) + "\n";
      const auto bytes = std::as_bytes(std::span(text));
      file.bytes.assign(bytes.begin(), bytes.end());
    }
  }
  if (auto verified = VerifyRoundTrip(document, location->stem, *files, source);
      !verified) {
    return std::unexpected(verified.error());
  }
  return ListConvertedImage({.id = *id,
                             .source = source.path,
                             .source_sha256 = document.provenance.source_sha256,
                             .converter = std::string(kTimConverterName),
                             .files = {},
                             .dependencies = DependenciesOf(conversion),
                             .unreferenced = conversion.unreferenced,
                             .note = conversion.note,
                             .alias_of = {}},
                            std::move(*location), std::move(*files));
}

std::expected<ConvertedImage, std::string> ConvertFbcPalette(
    const SourceFile& source) {
  if (!source.path.ends_with(kFbcExtension)) {
    return std::unexpected(
        std::format("{}: an FBC palette source must end in .FBC", source.path));
  }
  if (source.bytes.empty() || source.bytes.size() % kFbcRowBytes != 0) {
    return std::unexpected(std::format(
        "{}: an FBC palette must contain whole {}-byte rows; it has {} bytes",
        source.path, kFbcRowBytes, source.bytes.size()));
  }
  const std::size_t rows = source.bytes.size() / kFbcRowBytes;
  if (rows > kMaxFbcPaletteRows) {
    return std::unexpected(std::format(
        "{}: an FBC palette has {} rows; the effect CLUT has at most {}",
        source.path, rows, kMaxFbcPaletteRows));
  }

  std::vector<std::uint16_t> words;
  words.reserve(rows * kFbcPaletteEntries);
  for (std::size_t offset = 0; offset < source.bytes.size();
       offset += kFbcWordBytes) {
    words.push_back(static_cast<std::uint16_t>(
        ReadLittleEndian(source.bytes, offset, kFbcWordBytes)));
  }
  Content::NativeImage image{
      .depth = ImageDepth::kDirect16,
      .width = static_cast<std::uint32_t>(kFbcPaletteEntries),
      .height = static_cast<std::uint32_t>(rows),
      .indices = {},
      .texels = std::move(words),
      .palettes = {}};
  if (auto valid = Content::ValidateImage(image); !valid) {
    return std::unexpected(std::format("{}: {}", source.path, valid.error()));
  }

  auto id = FbcPaletteIdForDiscPath(source.path);
  if (!id) return std::unexpected(id.error());
  auto location = FbcPaletteLocationForDiscPath(source.path);
  if (!location) return std::unexpected(location.error());
  const ImageDocument document{
      .id = *id,
      .image = std::move(image),
      .material = std::nullopt,
      .unreferenced = false,
      .provenance = {.source = source.path,
                     .source_sha256 = HashOf(source),
                     .offset = 0,
                     .tim = std::nullopt,
                     .texture_page = std::nullopt,
                     .preview_palette = std::nullopt,
                     .dependencies = {},
                     .note = "FBC CLUT rows retain source order."}};
  auto files = EncodeImageFiles(document, location->stem);
  if (!files) {
    return std::unexpected(std::format("{}: {}", source.path, files.error()));
  }
  if (auto verified =
          VerifyFbcRoundTrip(document, location->stem, *files, source);
      !verified) {
    return std::unexpected(verified.error());
  }
  return ListConvertedImage({.id = *id,
                             .source = source.path,
                             .source_sha256 = document.provenance.source_sha256,
                             .converter = std::string(kFbcPaletteConverterName),
                             .files = {},
                             .dependencies = {},
                             .unreferenced = false,
                             .note = {},
                             .alias_of = {}},
                            std::move(*location), std::move(*files));
}

std::expected<ImageConversionReport, std::string> ConvertDiscImages(
    const fs::path& root, bool force) {
  if (auto migration = UpgradeBackgroundAuthoring(root); !migration) {
    return std::unexpected(migration.error());
  }
  auto start = StartRun(root);
  if (!start) return std::unexpected(start.error());
  const ConversionManifest& previous = start->previous;
  const std::vector<DataStore::Entry>& entries = start->entries;
  // Every image converts before anything is written.
  ConversionRun run;
  auto tims = ConvertTims(root, previous, entries, force, run);
  if (!tims) return std::unexpected(tims.error());
  auto palettes = ConvertFbcPalettes(root, previous, entries, force, run);
  if (!palettes) return std::unexpected(palettes.error());
  auto textures = ConvertFbtTextures(root, previous, entries, force, run);
  if (!textures) return std::unexpected(textures.error());
  const std::array batches{std::move(*tims), std::move(*palettes),
                           std::move(*textures)};
  auto removed = Publish(root, previous, run, batches);
  if (!removed) return std::unexpected(removed.error());
  run.report.converted = run.converted.size();
  run.report.removed = *removed;
  return run.report;
}

}  // namespace VS::Extract
