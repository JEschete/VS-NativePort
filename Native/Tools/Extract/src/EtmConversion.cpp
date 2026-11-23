// The etm-png converter (EtmConversion.hpp).
#include "Extract/EtmConversion.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <format>
#include <functional>
#include <iterator>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Content/ConversionManifest.hpp"
#include "Content/DataStore.hpp"
#include "Content/EsqFile.hpp"
#include "Content/EtmFile.hpp"
#include "Content/ImageFiles.hpp"
#include "Content/NativeImage.hpp"
#include "Content/SeqFile.hpp"
#include "Content/ShpFile.hpp"
#include "Extract/ConverterBatch.hpp"
#include "Extract/EsqConversion.hpp"
#include "Extract/ImageConversion.hpp"
#include "Extract/ImageFilesWriter.hpp"
#include "Extract/ShpModelConversion.hpp"

namespace VS::Extract {
using Content::ConversionOutput;
using Content::DataStore;
using Content::EsqEntry;
using Content::EsqFile;
using Content::EtmSheet;
using Content::ImageDependency;
using Content::ImageDepth;
using Content::ImageDocument;
using Content::ImageFile;
using Content::NativeImage;
using Content::SourceDependency;

namespace {
namespace fs = std::filesystem;
using Check = std::expected<void, std::string>;
template <typename T>
using Result = std::expected<T, std::string>;

constexpr std::string_view kRawForm = "raw";
constexpr std::string_view kActorTextureIdPrefix = "actor-texture";
// What each input gives a region.
constexpr std::string_view kTableRole = "table";
constexpr std::string_view kBindingRole = "binding";
constexpr std::string_view kPreviewPaletteRole = "preview palette";
constexpr std::uint32_t kByteBits = 8;
constexpr std::size_t kHalfwordBytes = 2;

// BATTLE.PRG's sheet and set file tables, by set number (ActorAssets.cpp
// StartEffectTextureRead and SequenceFile, which index both by the request's
// model): each file's first sector (a halfword from the first sector of its
// kind) and its sector count (a byte).
struct FileTable {
  std::size_t offsets_at = 0;
  std::size_t sectors_at = 0;
  std::uint32_t first_sector = 0;
  std::string_view kind;
};
// decomp: etmLbaOffsets (src/BATTLE/BATTLE.PRG/30DB0.c:64)
constexpr std::size_t kSheetOffsetsAt = 0x803B0;
// decomp: etmFileSectorSizes (src/BATTLE/BATTLE.PRG/30DB0.c:65)
constexpr std::size_t kSheetSectorsAt = 0x80490;
// decomp: D_800E8A60 (D_800E8A60@BATTLE/BATTLE.PRG)
constexpr std::size_t kSetOffsetsAt = 0x80260;
// decomp: D_800E8B40 (D_800E8B40@BATTLE/BATTLE.PRG)
constexpr std::size_t kSetSectorsAt = 0x80340;
constexpr std::uint32_t kSheetFirstSector = 104351;
constexpr std::uint32_t kSetFirstSector = 0x19797;
constexpr std::size_t kTableEntries = 112;
static_assert(kSheetSectorsAt + kTableEntries <= Content::kSeqEventLengthsAt);

using SectorFiles = std::map<std::uint32_t, const DataStore::Entry*>;

// What the sheets convert from: every ETM files.tsv lists, BATTLE.PRG, and
// where the listed sheets and sets start. Sets and models are read as the
// sheets need them.
// NOLINTNEXTLINE(bugprone-exception-escape): std::map's noexcept move.
struct SheetSources {
  std::vector<SourceFile> sheets;
  std::optional<SourceFile> program;
  std::set<std::string, std::less<>> listed;
  SectorFiles sheet_sectors;
  SectorFiles set_sectors;
};

Result<SheetSources> ReadSheetSources(
    const fs::path& root, const std::vector<DataStore::Entry>& entries) {
  SheetSources sources;
  for (const DataStore::Entry& entry : entries) {
    if (entry.form != kRawForm) continue;
    sources.listed.insert(entry.path);
    if (entry.path.ends_with(Content::kEsqExtension)) {
      sources.set_sectors.emplace(entry.lba, &entry);
      continue;
    }
    const bool sheet = entry.path.ends_with(Content::kEtmExtension);
    if (!sheet && entry.path != kBattleProgramPath) continue;
    auto source = ReadSource(root, entry.path);
    if (!source) return std::unexpected(source.error());
    if (sheet) {
      sources.sheet_sectors.emplace(entry.lba, &entry);
      sources.sheets.push_back(std::move(*source));
    } else {
      sources.program = std::move(*source);
    }
  }
  return sources;
}

// Listed files, each read once.
class SourceCache {
 public:
  SourceCache(fs::path root, const SheetSources& sources)
      : root_(std::move(root)), sources_(&sources) {}

  // The file `path`, which `user` needs.
  Result<const SourceFile*> Get(std::string_view path, std::string_view user) {
    if (const auto found = files_.find(path); found != files_.end()) {
      return &found->second;
    }
    if (!sources_->listed.contains(path)) {
      return std::unexpected(std::format(
          "{} needs {}, which files.tsv does not list", user, path));
    }
    auto source = ReadSource(root_, path);
    if (!source) return std::unexpected(source.error());
    return &files_.emplace(std::string(path), std::move(*source)).first->second;
  }

 private:
  fs::path root_;
  const SheetSources* sources_;
  std::map<std::string, SourceFile, std::less<>> files_;
};

std::uint32_t ProgramByte(const SourceFile& program, std::size_t at) {
  return std::to_integer<std::uint32_t>(program.bytes[at]);
}

// The listed file entry `index` of `table` names, or nullptr when the entry
// has no sectors.
Result<const DataStore::Entry*> TableFileOf(const SourceFile& program,
                                            const FileTable& table,
                                            std::size_t index,
                                            const SectorFiles& files) {
  const std::uint32_t sectors = ProgramByte(program, table.sectors_at + index);
  if (sectors == 0) return nullptr;
  const std::size_t at = table.offsets_at + (index * kHalfwordBytes);
  const std::uint32_t lba = table.first_sector + ProgramByte(program, at) +
                            (ProgramByte(program, at + 1) << kByteBits);
  const auto found = files.find(lba);
  if (found == files.end() || found->second->SectorCount() != sectors) {
    return std::unexpected(
        std::format("{}: entry {} names {} sectors at LBA {}, which is no {} "
                    "files.tsv lists",
                    program.path, index, sectors, lba, table.kind));
  }
  return found->second;
}

// Each sheet's set: the one the same table entry names. Every sheet must be
// named by one entry, and that entry must name a set.
Result<std::map<std::string, std::string, std::less<>>> SheetSets(
    const SourceFile& program, const SheetSources& sources) {
  constexpr FileTable kSheetTable{.offsets_at = kSheetOffsetsAt,
                                  .sectors_at = kSheetSectorsAt,
                                  .first_sector = kSheetFirstSector,
                                  .kind = Content::kEtmExtension};
  constexpr FileTable kSetTable{.offsets_at = kSetOffsetsAt,
                                .sectors_at = kSetSectorsAt,
                                .first_sector = kSetFirstSector,
                                .kind = Content::kEsqExtension};
  std::map<std::string, std::string, std::less<>> sets;
  for (std::size_t index = 0; index < kTableEntries; ++index) {
    auto sheet =
        TableFileOf(program, kSheetTable, index, sources.sheet_sectors);
    if (!sheet) return std::unexpected(sheet.error());
    if (*sheet == nullptr) continue;
    auto set = TableFileOf(program, kSetTable, index, sources.set_sectors);
    if (!set) return std::unexpected(set.error());
    if (*set == nullptr) {
      return std::unexpected(
          std::format("{}: entry {} names the sheet {} but no set",
                      program.path, index, (*sheet)->path));
    }
    if (!sets.emplace((*sheet)->path, (*set)->path).second) {
      return std::unexpected(
          std::format("{}: {} is named by more than one "
                      "entry, so its regions are ambiguous",
                      program.path, (*sheet)->path));
    }
  }
  for (const SourceFile& sheet : sources.sheets) {
    if (!sets.contains(sheet.path)) {
      return std::unexpected(
          std::format("{} is named by no entry of {}'s sheet table", sheet.path,
                      program.path));
    }
  }
  return sets;
}

ImageDependency DependencyOf(const SourceFile& source, std::string_view role) {
  return {.source = source.path,
          .source_sha256 = HashOf(source),
          .role = std::string(role)};
}

// The depth a model draws its region at and the palette its preview shows:
// the first palette row of the model that owns its texture.
struct ModelTexture {
  ImageDepth depth = ImageDepth::kIndexed8;
  std::string owner;          // the SHP holding the texture
  std::string palette_image;  // the owner's texture image id
  std::vector<std::uint16_t> palette;
  std::vector<ImageDependency> dependencies;  // the model and the owner
};

// A model's region of a sheet: from its texture row, `lines` long.
struct SheetRegion {
  std::int32_t model = 0;
  std::size_t row = 0;
  std::size_t lines = 0;
  ModelTexture texture;
};

struct SheetConversion {
  SourceFile sheet;
  std::string sheet_sha256;
  std::vector<SheetRegion> regions;            // by row
  std::vector<ImageDependency> shared;         // BATTLE.PRG and the set
  std::vector<SourceDependency> dependencies;  // every input, by source
};

// The regions the set's texture rows cut the sheet into, by row: each from
// its row to the next, the first from line 0 and the last to the end.
Result<std::vector<SheetRegion>> RegionsOf(const SourceFile& sheet,
                                           std::size_t height,
                                           const SourceFile& set,
                                           const EsqFile& file) {
  std::vector<SheetRegion> regions;
  for (const EsqEntry& entry : file.entries) {
    if (!entry.texture_row) continue;
    regions.push_back({.model = static_cast<std::int16_t>(entry.selector),
                       .row = *entry.texture_row,
                       .lines = 0,
                       .texture = {}});
  }
  std::ranges::sort(regions, {}, &SheetRegion::row);
  if (regions.empty() || regions.front().row != 0) {
    return std::unexpected(std::format(
        "{}: {} gives no model line 0, so no model draws the sheet's first "
        "lines",
        sheet.path, set.path));
  }
  std::set<std::int32_t> models;
  for (std::size_t index = 0; index < regions.size(); ++index) {
    SheetRegion& region = regions[index];
    if (!models.insert(region.model).second) {
      return std::unexpected(
          std::format("{}: {} gives model {} two texture "
                      "rows",
                      sheet.path, set.path, region.model));
    }
    const bool last = index + 1 == regions.size();
    const std::size_t end = last ? height : regions[index + 1].row;
    if (!last && end == region.row) {
      return std::unexpected(std::format(
          "{}: {} gives models {} and {} the same line {}", sheet.path,
          set.path, region.model, regions[index + 1].model, region.row));
    }
    if (region.row >= height) {
      return std::unexpected(std::format(
          "{}: {} gives model {} line {}, past the sheet's {} lines",
          sheet.path, set.path, region.model, region.row, height));
    }
    region.lines = end - region.row;
  }
  return regions;
}

// Every input of the sheet's regions, once each, by source.
std::vector<SourceDependency> DependenciesOf(
    const SheetConversion& conversion) {
  std::map<std::string_view, std::string_view> inputs;
  for (const ImageDependency& dependency : conversion.shared) {
    inputs.emplace(dependency.source, dependency.source_sha256);
  }
  for (const SheetRegion& region : conversion.regions) {
    for (const ImageDependency& dependency : region.texture.dependencies) {
      inputs.emplace(dependency.source, dependency.source_sha256);
    }
  }
  std::vector<SourceDependency> dependencies;
  for (const auto& [source, hash] : inputs) {
    dependencies.push_back(
        {.source = std::string(source), .source_sha256 = std::string(hash)});
  }
  return dependencies;
}

// Plans the sheets, reading the sets and models they need.
class SheetPlanner {
 public:
  SheetPlanner(const SourceFile& program, SeqEventTable events,
               SourceCache& cache)
      : table_(DependencyOf(program, kTableRole)),
        events_(std::move(events)),
        cache_(&cache) {}

  Result<SheetConversion> Plan(const SourceFile& sheet,
                               std::string_view set_path) {
    auto decoded = Content::DecodeEtmFile(sheet.bytes);
    if (!decoded) {
      return std::unexpected(
          std::format("{}: {}", sheet.path, decoded.error()));
    }
    auto set = cache_->Get(set_path, sheet.path);
    if (!set) return std::unexpected(set.error());
    auto file = Content::DecodeEsqFile((*set)->bytes, events_.lengths);
    if (!file) {
      return std::unexpected(std::format("{}: {}", set_path, file.error()));
    }
    auto regions = RegionsOf(sheet, decoded->Height(), **set, *file);
    if (!regions) return std::unexpected(regions.error());
    for (SheetRegion& region : *regions) {
      auto texture = TextureOf(region.model, set_path);
      if (!texture) return std::unexpected(texture.error());
      region.texture = **texture;
    }
    SheetConversion conversion{
        .sheet = sheet,
        .sheet_sha256 = HashOf(sheet),
        .regions = std::move(*regions),
        .shared = {table_, DependencyOf(**set, kBindingRole)},
        .dependencies = {}};
    conversion.dependencies = DependenciesOf(conversion);
    return conversion;
  }

 private:
  // A model's own texture gives its depth and palette; a model that borrows
  // one draws at 8 bits (ActorAssets.cpp BorrowActorTexture) and previews
  // with its lender's palette.
  Result<const ModelTexture*> TextureOf(std::int32_t model,
                                        std::string_view user) {
    if (const auto found = textures_.find(model); found != textures_.end()) {
      return &found->second;
    }
    const std::string path = ShpLenderPath(model);
    auto source = cache_->Get(path, user);
    if (!source) return std::unexpected(source.error());
    auto peek = Content::PeekShpTexture((*source)->bytes);
    if (!peek) {
      return std::unexpected(std::format("{}: {}", path, peek.error()));
    }
    ModelTexture texture;
    texture.dependencies.push_back(
        DependencyOf(**source, Content::kEtmRegionModelRole));
    const SourceFile* owner = *source;
    if (peek->lender) {
      auto lender = cache_->Get(ShpLenderPath(*peek->lender), path);
      if (!lender) return std::unexpected(lender.error());
      owner = *lender;
      texture.dependencies.push_back(DependencyOf(*owner, kPreviewPaletteRole));
    }
    auto shp = Content::DecodeShp(owner->bytes);
    if (!shp) {
      return std::unexpected(std::format("{}: {}", owner->path, shp.error()));
    }
    if (!shp->texture) {
      return std::unexpected(
          std::format("{} borrows the texture of {}, which has none of its own",
                      path, owner->path));
    }
    const Content::ShpTexture& own = *shp->texture;
    texture.depth = peek->lender || own.Is8Bit() ? ImageDepth::kIndexed8
                                                 : ImageDepth::kIndexed4;
    const std::size_t entries =
        std::min<std::size_t>({own.palette_words, own.palettes.size(),
                               Content::PaletteCapacity(texture.depth)});
    texture.palette.assign(
        own.palettes.begin(),
        own.palettes.begin() + static_cast<std::ptrdiff_t>(entries));
    auto image = ImageIdForDiscPath(owner->path);
    if (!image) return std::unexpected(image.error());
    texture.owner = owner->path;
    texture.palette_image = std::move(*image);
    std::ranges::sort(texture.dependencies, {}, &ImageDependency::source);
    return &textures_.emplace(model, std::move(texture)).first->second;
  }

  ImageDependency table_;  // BATTLE.PRG, which pairs sheets with sets
  SeqEventTable events_;
  SourceCache* cache_;
  std::map<std::int32_t, ModelTexture> textures_;
};

// The region's lines at its model's depth, previewed with the owner's palette
// row, extended with transparent entries to the highest index the lines use.
Result<NativeImage> ImageOf(const SheetConversion& conversion,
                            const EtmSheet& sheet, const SheetRegion& region) {
  const ImageDepth depth = region.texture.depth;
  const std::uint32_t per_halfword = Content::PixelsPerHalfword(depth);
  const std::uint32_t bits = Content::BitsPerPixel(depth);
  const std::uint32_t mask = (1U << bits) - 1U;
  NativeImage image{
      .depth = depth,
      .width =
          static_cast<std::uint32_t>(Content::kEtmLineWords) * per_halfword,
      .height = static_cast<std::uint32_t>(region.lines),
      .indices = {},
      .texels = {},
      .palettes = {}};
  const auto words = std::span(sheet.words)
                         .subspan(region.row * Content::kEtmLineWords,
                                  region.lines * Content::kEtmLineWords);
  image.indices.reserve(words.size() * per_halfword);
  for (const std::uint16_t word : words) {
    for (std::uint32_t pixel = 0; pixel < per_halfword; ++pixel) {
      image.indices.push_back(
          static_cast<std::uint8_t>((word >> (pixel * bits)) & mask));
    }
  }
  const std::size_t used =
      image.indices.empty()
          ? 0
          : std::size_t{*std::ranges::max_element(image.indices)} + 1;
  std::vector<std::uint16_t> palette = region.texture.palette;
  if (palette.size() < used) palette.resize(used, Content::kTransparentWord);
  image.palettes.push_back(std::move(palette));
  if (auto valid = Content::ValidateImage(image); !valid) {
    return std::unexpected(
        std::format("{}: {}", conversion.sheet.path, valid.error()));
  }
  return image;
}

// Indices past the owner's palette draw from the rest of the model's CLUT row,
// which the game fills at run time (ActorAssets.cpp uploads a weapon's CLUT
// from column kWeaponClutFirstColumn of the actor rows).
std::string NoteOf(const SheetRegion& region, const NativeImage& image) {
  const std::size_t own = region.texture.palette.size();
  if (image.PaletteEntries() <= own) return {};
  return std::format(
      "The lines use indices past the {} entries of {}'s palette, which the "
      "game draws from the rest of the model's CLUT row as filled at run time "
      "(a weapon's palette, for example); the preview shows them transparent.",
      own, region.texture.owner);
}

std::vector<ImageDependency> RegionDependencies(
    const SheetConversion& conversion, const SheetRegion& region) {
  std::vector<ImageDependency> dependencies = conversion.shared;
  std::ranges::copy(region.texture.dependencies,
                    std::back_inserter(dependencies));
  std::ranges::sort(dependencies, {}, &ImageDependency::source);
  return dependencies;
}

Result<ImageDocument> RegionDocument(const SheetConversion& conversion,
                                     const EtmSheet& sheet,
                                     const SheetRegion& region) {
  auto sheet_id = ImageIdForDiscPath(conversion.sheet.path);
  if (!sheet_id) return std::unexpected(sheet_id.error());
  auto image = ImageOf(conversion, sheet, region);
  if (!image) return std::unexpected(image.error());
  std::string note = NoteOf(region, *image);
  return ImageDocument{
      .id = std::format("{}.{:02x}", *sheet_id, region.model),
      .image = std::move(*image),
      .material = std::nullopt,
      .unreferenced = false,
      .provenance = {
          .source = conversion.sheet.path,
          .source_sha256 = conversion.sheet_sha256,
          .offset =
              Content::kEtmHeaderBytes + (region.row * Content::kEtmLineBytes),
          .tim = std::nullopt,
          .texture_page = std::nullopt,
          .preview_palette =
              Content::PreviewPalette{
                  .image = region.texture.palette_image, .row = 0, .entry = 0},
          .dependencies = RegionDependencies(conversion, region),
          .note = std::move(note)}};
}

Result<ConversionOutput> SheetOutputOf(const SheetConversion& conversion) {
  auto id = ActorTextureIdForDiscPath(conversion.sheet.path);
  if (!id) return std::unexpected(id.error());
  return ConversionOutput{
      .id = std::move(*id),
      .source = conversion.sheet.path,
      .source_sha256 = conversion.sheet_sha256,
      .converter = std::string(Content::kEtmTextureConverterName),
      .files = {},
      .dependencies = conversion.dependencies,
      .unreferenced = false,
      .note = {},
      .alias_of = {}};
}

// The regions' images, read back from their files, must rebuild the source
// bytes.
Check VerifySheet(const SheetConversion& conversion,
                  std::span<const ImageDocument> decoded) {
  const SourceFile& source = conversion.sheet;
  auto sheet = Content::EtmSheetOfImages(decoded, source.path);
  if (!sheet) {
    return std::unexpected(std::format("{}: {}", source.path, sheet.error()));
  }
  auto bytes = Content::EncodeEtmFile(*sheet);
  if (!bytes || *bytes != source.bytes) {
    return std::unexpected(
        std::format("{}: the converted regions do not rebuild the source bytes",
                    source.path));
  }
  return {};
}

// Each region's files are <sheet>/<model>.*, so a sheet's regions share a
// directory: the image files take the model as their stem, and the sheet's
// directory is added to their names once they read back.
Result<ConvertedModel> ConvertSheet(const SheetConversion& conversion) {
  const SourceFile& source = conversion.sheet;
  auto sheet = Content::DecodeEtmFile(source.bytes);
  if (!sheet) {
    return std::unexpected(std::format("{}: {}", source.path, sheet.error()));
  }
  const auto directory = DiscStemOf(source.path, Content::kEtmExtension);
  if (!directory) {
    return std::unexpected(
        std::format("'{}' is not an ETM effect texture sheet", source.path));
  }
  std::vector<ImageDocument> decoded;
  std::vector<ImageFile> files;
  for (const SheetRegion& region : conversion.regions) {
    auto document = RegionDocument(conversion, *sheet, region);
    if (!document) return std::unexpected(document.error());
    const std::string stem = std::format("{:02X}", region.model);
    auto encoded = EncodeImageFiles(*document, stem);
    if (!encoded) {
      return std::unexpected(
          std::format("{}: {}", source.path, encoded.error()));
    }
    auto read = DecodeConvertedFiles(*document, stem, *encoded, source);
    if (!read) return std::unexpected(read.error());
    decoded.push_back(std::move(*read));
    for (ImageFile& file : *encoded) {
      file.name = std::format("{}/{}", *directory, file.name);
      files.push_back(std::move(file));
    }
  }
  if (auto verified = VerifySheet(conversion, decoded); !verified) {
    return std::unexpected(verified.error());
  }
  auto output = SheetOutputOf(conversion);
  if (!output) return std::unexpected(output.error());
  return ConvertedModelOf(std::move(*output), std::move(files),
                          Content::kActorTextureDirectory);
}

}  // namespace

std::expected<std::string, std::string> ActorTextureIdForDiscPath(
    std::string_view disc_path) {
  if (!DiscStemOf(disc_path, Content::kEtmExtension)) {
    return std::unexpected(
        std::format("'{}' is not an ETM effect texture sheet", disc_path));
  }
  auto id = ImageIdForDiscPath(disc_path);
  if (!id) return std::unexpected(id.error());
  // image.<directories>.<stem> -> actor-texture.<directories>.<stem>
  id->replace(0, id->find('.'), kActorTextureIdPrefix);
  if (!Content::IsContentId(*id)) {
    return std::unexpected(
        std::format("'{}' gives the actor texture id '{}', which is not valid",
                    disc_path, *id));
  }
  return id;
}

std::expected<ModelConversionReport, std::string> ConvertDiscActorTextures(
    const fs::path& root, bool force) {
  auto start = StartRun(root);
  if (!start) return std::unexpected(start.error());
  auto sources = ReadSheetSources(root, start->entries);
  if (!sources) return std::unexpected(sources.error());
  std::vector<ModelJob> jobs;
  if (!sources->sheets.empty()) {
    if (!sources->program) {
      return std::unexpected(
          std::format("{} needs {}, which files.tsv does not list",
                      sources->sheets.front().path, kBattleProgramPath));
    }
    const SourceFile& program = *sources->program;
    auto events = ProgramEventTable(program);
    if (!events) return std::unexpected(events.error());
    auto sets = SheetSets(program, *sources);
    if (!sets) return std::unexpected(sets.error());
    SourceCache cache(root, *sources);
    SheetPlanner planner(program, std::move(*events), cache);
    for (const SourceFile& sheet : sources->sheets) {
      const auto set = sets->find(sheet.path);
      if (set == sets->end()) continue;
      auto conversion = planner.Plan(sheet, set->second);
      if (!conversion) return std::unexpected(conversion.error());
      auto expected = SheetOutputOf(*conversion);
      if (!expected) return std::unexpected(expected.error());
      jobs.push_back({.expected = std::move(*expected),
                      .companions = {},
                      .convert = [plan = std::move(*conversion)] {
                        return ConvertSheet(plan);
                      }});
    }
  }
  return RunModelConverter(root, *start,
                           {.name = Content::kEtmTextureConverterName,
                            .version = Content::kEtmTextureConverterVersion,
                            .directory = Content::kActorTextureDirectory,
                            .force = force,
                            .companion = std::nullopt},
                           jobs);
}

}  // namespace VS::Extract
