// The fbt-texture-png converter (EffectTextureConversion.hpp).
#include "Extract/EffectTextureConversion.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <map>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Core/Sha256.hpp"
#include "Extract/ImageFilesWriter.hpp"

namespace VS::Extract {
using Content::ConversionOutput;
using Content::DataStore;
using Content::ImageDependency;
using Content::ImageDepth;
using Content::ImageDocument;
using Content::ImageFile;
using Content::ImageProvenance;
using Content::NativeImage;

namespace {
constexpr std::string_view kRawForm = "raw";
constexpr std::string_view kEffectDirectory = "EFFECT/";
constexpr std::string_view kPurgePath = "EFFECT/EFFPURGE.BIN";
// Package extents count sectors from the first package's.
constexpr std::string_view kPackageBasePath = "EFFECT/E000.P";
constexpr std::string_view kTextureExtension = ".FBT";
constexpr std::string_view kPaletteExtension = ".FBC";
constexpr std::string_view kPackageExtension = ".P";
constexpr std::string_view kSamplerRole = "sampler";
constexpr std::string_view kPreviewPaletteRole = "preview palette";
constexpr std::string_view kBindingRole = "binding";

// EFFPURGE.BIN's tables at their file offsets. Extents are
// vs_LoadFileContext {u16 lba, u16 size} (battleEffect.h:6).
constexpr std::size_t kPurgeBytes = 17264;
constexpr std::size_t kEffectCount = 400;
constexpr std::size_t kTextureFileCount = 755;
// decomp: fbcClutRows (src/EFFECT/EFFPURGE.BIN/0.c:125)
constexpr std::size_t kClutRowsOffset = 0x164C;
// decomp: fbcSlots (src/EFFECT/EFFPURGE.BIN/0.c:141)
constexpr std::size_t kPaletteSlotsOffset = 0x17DC;
// decomp: fbcFiles (src/EFFECT/EFFPURGE.BIN/0.c:160)
constexpr std::size_t kPaletteFilesOffset = 0x196C;
// decomp: fbtCounts (src/EFFECT/EFFPURGE.BIN/0.c:219)
constexpr std::size_t kTextureCountsOffset = 0x1FAC;
// decomp: fbtSlots (src/EFFECT/EFFPURGE.BIN/0.c:235)
constexpr std::size_t kTextureSlotsOffset = 0x213C;
// decomp: fbtFiles (src/EFFECT/EFFPURGE.BIN/0.c:265)
constexpr std::size_t kTextureFilesOffset = 0x2430;
// decomp: pFiles (src/EFFECT/EFFPURGE.BIN/0.c:378)
constexpr std::size_t kPackageFilesOffset = 0x2FFC;
constexpr std::size_t kExtentBytes = 4;
constexpr std::size_t kExtentSizeOffset = sizeof(std::uint16_t);

// An effect texture page fills VRAM texture page 24 + its slot.
constexpr std::uint32_t kFirstEffectPage = 24;
constexpr std::uint32_t kEffectPageCount = 8;
constexpr std::size_t kHalfwordBytes = sizeof(std::uint16_t);
constexpr std::size_t kPageWidth = Content::kTexturePageWidth;
constexpr std::size_t kPageHalfwords = kPageWidth * Content::kTexturePageHeight;
constexpr std::size_t kTextureBytes = kPageHalfwords * kHalfwordBytes;
constexpr unsigned kByteBits = 8;

// A package block starts with {u16 size (with this word), u16 type}; type 0
// ends the package and type 1 holds the texture tables.
constexpr std::size_t kBlockHeaderBytes = 4;
constexpr std::uint32_t kBlockSizeMask = 0xFFFF;
constexpr unsigned kBlockTypeShift = 16;
constexpr std::uint32_t kTextureBlockType = 1;
// A texture block: u16 table count, u16 descriptor offset, ...; descriptors
// run from that offset to the block's end. A descriptor starts with its CLUT
// word, its texture page word and u8 u, v, w and h.
constexpr std::size_t kDescriptorOffsetField = sizeof(std::uint16_t);
constexpr std::size_t kDescriptorBytes = 0x18;
constexpr std::size_t kPageWordField = sizeof(std::uint16_t);
constexpr std::size_t kTexelUField = 4;
constexpr std::size_t kTexelWidthField = 6;
constexpr unsigned kPageDepthShift = 7;
constexpr std::uint32_t kPageDepthMask = 3;
constexpr std::uint32_t kIndexed4Code = 0;
constexpr std::uint32_t kIndexed8Code = 1;
constexpr std::uint32_t kPageColumnMask = 0xF;
constexpr unsigned kPageRowShift = 4;
constexpr std::uint32_t kPageRowMask = 1;
constexpr std::uint32_t kPageTexels = 256;  // u runs 0-255 within a page
constexpr unsigned kClutRowShift = 6;
constexpr std::uint32_t kClutRowMask = 0x1FF;
constexpr std::uint32_t kClutColumnMask = 0x3F;
constexpr std::uint32_t kClutColumnEntries = 16;
// The effect CLUT: rows 240-244 from x 768; an FBC fills rows from
// 240 + its slot.
constexpr std::uint32_t kEffectClutFirstRow = 240;
constexpr std::uint32_t kEffectClutX = 768;
constexpr std::size_t kPaletteRowEntries = 256;
constexpr std::size_t kPaletteRowBytes = kPaletteRowEntries * kHalfwordBytes;

using Bytes = std::span<const std::byte>;

std::uint32_t ReadLittleEndian(Bytes data, std::size_t offset,
                               std::size_t bytes) {
  std::uint32_t value = 0;
  for (std::size_t index = 0; index < bytes; ++index) {
    value |= std::to_integer<std::uint32_t>(data[offset + index])
             << (index * kByteBits);
  }
  return value;
}

std::uint16_t HalfwordAt(Bytes data, std::size_t offset) {
  return static_cast<std::uint16_t>(
      ReadLittleEndian(data, offset, kHalfwordBytes));
}

std::string HashOf(const SourceFile& source) {
  return Core::Sha256Hex(source.bytes);
}

struct FileExtent {
  std::uint32_t lba = 0;  // sectors from the first package's
  std::uint32_t size = 0;
};

struct PurgeTables {
  std::vector<std::uint8_t> clut_rows;
  std::vector<std::uint8_t> palette_slots;
  std::vector<FileExtent> palette_files;
  std::vector<std::uint8_t> texture_counts;
  std::vector<std::uint8_t> texture_slots;
  std::vector<FileExtent> texture_files;
  std::vector<FileExtent> package_files;
};

std::vector<std::uint8_t> TableBytes(Bytes data, std::size_t offset,
                                     std::size_t count) {
  std::vector<std::uint8_t> values;
  values.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    values.push_back(std::to_integer<std::uint8_t>(data[offset + index]));
  }
  return values;
}

std::vector<FileExtent> TableExtents(Bytes data, std::size_t offset,
                                     std::size_t count) {
  std::vector<FileExtent> extents;
  extents.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    const std::size_t at = offset + (index * kExtentBytes);
    extents.push_back({.lba = HalfwordAt(data, at),
                       .size = HalfwordAt(data, at + kExtentSizeOffset)});
  }
  return extents;
}

std::expected<PurgeTables, std::string> ReadPurgeTables(
    const SourceFile& purge) {
  if (purge.bytes.size() != kPurgeBytes) {
    return std::unexpected(
        std::format("{}: the purge tables take {} bytes; it has {}", purge.path,
                    kPurgeBytes, purge.bytes.size()));
  }
  const Bytes data = purge.bytes;
  PurgeTables tables{
      .clut_rows = TableBytes(data, kClutRowsOffset, kEffectCount),
      .palette_slots = TableBytes(data, kPaletteSlotsOffset, kEffectCount),
      .palette_files = TableExtents(data, kPaletteFilesOffset, kEffectCount),
      .texture_counts = TableBytes(data, kTextureCountsOffset, kEffectCount),
      .texture_slots = TableBytes(data, kTextureSlotsOffset, kTextureFileCount),
      .texture_files =
          TableExtents(data, kTextureFilesOffset, kTextureFileCount),
      .package_files = TableExtents(data, kPackageFilesOffset, kEffectCount)};
  std::size_t textures = 0;
  for (const std::uint8_t count : tables.texture_counts) textures += count;
  if (textures > kTextureFileCount) {
    return std::unexpected(
        std::format("{}: the effects load {} texture pages; the table holds {}",
                    purge.path, textures, kTextureFileCount));
  }
  return tables;
}

// The listed effect files by the extents EFFPURGE.BIN gives them.
class EffectFiles {
 public:
  EffectFiles() noexcept(false) = default;
  EffectFiles(EffectFiles&&) noexcept(false) = default;

  static std::expected<EffectFiles, std::string> Index(
      std::span<const DataStore::Entry> entries) {
    EffectFiles files;
    const DataStore::Entry* base = nullptr;
    for (const DataStore::Entry& entry : entries) {
      if (entry.form != kRawForm || !entry.path.starts_with(kEffectDirectory)) {
        continue;
      }
      if (entry.path == kPackageBasePath) base = &entry;
      files.by_lba_.emplace(entry.lba, &entry);
    }
    if (base == nullptr) {
      return std::unexpected(
          std::format("{} is not in files.tsv, so no effect extent resolves",
                      kPackageBasePath));
    }
    files.base_ = base->lba;
    return files;
  }

  // The one listed file of `extension` the extent names: its first sector
  // and its sector-rounded size.
  [[nodiscard]] std::expected<std::string, std::string> Find(
      FileExtent extent, std::string_view extension,
      std::string_view what) const {
    const DataStore::Entry* found = nullptr;
    const auto [first, last] = by_lba_.equal_range(base_ + extent.lba);
    for (auto at = first; at != last; ++at) {
      const DataStore::Entry& entry = *at->second;
      if (entry.path.ends_with(extension) &&
          std::uint64_t{entry.SectorCount()} * DataStore::kDataSectorBytes ==
              extent.size) {
        if (found != nullptr) {
          return std::unexpected(std::format("{} names both {} and {}", what,
                                             found->path, entry.path));
        }
        found = &entry;
      }
    }
    if (found == nullptr) {
      return std::unexpected(
          std::format("{} (sector {} + {}, {} bytes) names no listed {} file",
                      what, base_, extent.lba, extent.size, extension));
    }
    return found->path;
  }

 private:
  std::uint32_t base_ = 0;
  std::multimap<std::uint32_t, const DataStore::Entry*> by_lba_;
};

// Disc files read once each.
class SourceCache {
 public:
  explicit SourceCache(const SourceReader& read) : read_(&read) {}

  std::expected<const SourceFile*, std::string> Get(std::string_view path) {
    const auto found = files_.find(path);
    if (found != files_.end()) return &found->second;
    auto source = (*read_)(path);
    if (!source) return std::unexpected(source.error());
    if (source->path != path) {
      return std::unexpected(
          std::format("reading {} gave {}", path, source->path));
    }
    return &files_.emplace(std::string(path), std::move(*source)).first->second;
  }

 private:
  const SourceReader* read_;
  std::map<std::string, SourceFile, std::less<>> files_;
};

struct BoundTexture {
  std::string path;
  std::uint32_t slot = 0;
};

// The files one effect loads with its texture pages.
struct EffectBinding {
  std::uint32_t effect = 0;
  std::string package;
  std::string palette;
  std::uint32_t palette_slot = 0;
  std::vector<BoundTexture> textures;
};

// decomp: BATTLE.PRG func_800CE9B0 (src/BATTLE/BATTLE.PRG/5BF94.c:4735)
// loads no textures for an effect other than 0 whose package extent is at
// sector 0, so an effect with pages must name its package and palette.
std::expected<void, std::string> BindFiles(const PurgeTables& tables,
                                           const EffectFiles& files,
                                           EffectBinding& binding) {
  const std::uint32_t effect = binding.effect;
  const FileExtent package = tables.package_files[effect];
  if (effect != 0 && package.lba == 0) {
    return std::unexpected(std::format(
        "effect {} lists texture pages but has no package, so it never loads "
        "them",
        effect));
  }
  auto package_path =
      files.Find(package, kPackageExtension,
                 std::format("effect {}'s package extent", effect));
  if (!package_path) return std::unexpected(package_path.error());
  if (tables.clut_rows[effect] == 0) {
    return std::unexpected(std::format(
        "effect {} lists texture pages but loads no FBC palette", effect));
  }
  auto palette_path =
      files.Find(tables.palette_files[effect], kPaletteExtension,
                 std::format("effect {}'s FBC extent", effect));
  if (!palette_path) return std::unexpected(palette_path.error());
  binding.package = std::move(*package_path);
  binding.palette = std::move(*palette_path);
  binding.palette_slot = tables.palette_slots[effect];
  return {};
}

// decomp: EFFPURGE.BIN configureFb (src/EFFECT/EFFPURGE.BIN/0.c:944): an
// effect's pages follow every earlier effect's in fbtFiles and fbtSlots.
std::expected<std::vector<EffectBinding>, std::string> BindEffects(
    const PurgeTables& tables, const EffectFiles& files) {
  std::vector<EffectBinding> bindings;
  std::map<std::string, std::uint32_t, std::less<>> owners;
  std::size_t first = 0;
  for (std::uint32_t effect = 0; effect < kEffectCount; ++effect) {
    const std::size_t count = tables.texture_counts[effect];
    if (count == 0) continue;
    EffectBinding binding{.effect = effect,
                          .package = {},
                          .palette = {},
                          .palette_slot = 0,
                          .textures = {}};
    std::uint32_t slots = 0;  // one bit per loaded slot
    for (std::size_t index = first; index < first + count; ++index) {
      const std::uint32_t slot = tables.texture_slots[index];
      if (slot >= kEffectPageCount || ((slots >> slot) & 1U) != 0) {
        return std::unexpected(std::format(
            "effect {} loads a texture page into slot {}; slots are 0-{} and "
            "each takes one page",
            effect, slot, kEffectPageCount - 1));
      }
      slots |= 1U << slot;
      auto path = files.Find(tables.texture_files[index], kTextureExtension,
                             std::format("texture page {}'s extent", index));
      if (!path) return std::unexpected(path.error());
      if (const auto [owner, added] = owners.try_emplace(*path, effect);
          !added) {
        return std::unexpected(
            std::format("{} is loaded by effects {} and {}; one effect must "
                        "own each page",
                        *path, owner->second, effect));
      }
      binding.textures.push_back({.path = std::move(*path), .slot = slot});
    }
    first += count;
    if (auto bound = BindFiles(tables, files, binding); !bound) {
      return std::unexpected(bound.error());
    }
    bindings.push_back(std::move(binding));
  }
  return bindings;
}

// The payload of the package's last texture block, which is the one the
// runtime binds; empty when it has none.
// decomp: BATTLE.PRG func_800D8060 (src/BATTLE/BATTLE.PRG/6E4CC.c:375)
std::expected<Bytes, std::string> TextureBlockOf(const SourceFile& package) {
  const Bytes data = package.bytes;
  Bytes texture_block;
  std::size_t offset = 0;
  while (true) {
    if (data.size() - offset < kBlockHeaderBytes) {
      return std::unexpected(
          std::format("{}: the package ends at byte {} without its end block",
                      package.path, offset));
    }
    const std::uint32_t meta =
        ReadLittleEndian(data, offset, kBlockHeaderBytes);
    const std::uint32_t type = meta >> kBlockTypeShift;
    if (type == 0) return texture_block;
    const std::size_t size = meta & kBlockSizeMask;
    if (size < kBlockHeaderBytes || size > data.size() - offset) {
      return std::unexpected(
          std::format("{}: the block at byte {} takes {} bytes; {} remain",
                      package.path, offset, size, data.size() - offset));
    }
    if (type == kTextureBlockType) {
      texture_block =
          data.subspan(offset + kBlockHeaderBytes, size - kBlockHeaderBytes);
    }
    offset += size;
  }
}

// What one page's sampling descriptors fix.
struct PageSamples {
  PageSamples() noexcept(false) = default;
  PageSamples(PageSamples&&) noexcept(false) = default;
  std::optional<ImageDepth> depth;
  // How many descriptors sample each (FBC row, first entry).
  std::map<std::pair<std::uint32_t, std::uint32_t>, std::size_t> palettes;
};
using EffectSamples = std::array<PageSamples, kEffectPageCount>;

// One descriptor's fields that place its texels.
struct Descriptor {
  std::size_t index = 0;
  std::uint16_t clut = 0;
  std::uint16_t page_word = 0;
  std::uint32_t u = 0;
  std::uint32_t width = 0;
};

// The FBC row and first entry a descriptor's palette reference reads.
std::expected<std::pair<std::uint32_t, std::uint32_t>, std::string> PaletteOf(
    const Descriptor& descriptor, ImageDepth depth,
    const EffectBinding& binding, std::size_t palette_rows) {
  const std::uint32_t row = (descriptor.clut >> kClutRowShift) & kClutRowMask;
  const std::uint32_t x =
      (descriptor.clut & kClutColumnMask) * kClutColumnEntries;
  const std::uint32_t first_row = kEffectClutFirstRow + binding.palette_slot;
  if (row < first_row || row - first_row >= palette_rows || x < kEffectClutX ||
      x - kEffectClutX + Content::PaletteCapacity(depth) > kPaletteRowEntries) {
    return std::unexpected(std::format(
        "{}: texture descriptor {} samples CLUT ({}, {}), which {}'s {} rows "
        "at row {} do not hold",
        binding.package, descriptor.index, x, row, binding.palette,
        palette_rows, first_row));
  }
  return std::pair{row - first_row, x - kEffectClutX};
}

// The indexed depth of a texture page word's depth code; nothing for direct
// colour.
std::optional<ImageDepth> IndexedDepthOf(std::uint32_t code) {
  if (code == kIndexed4Code) return ImageDepth::kIndexed4;
  if (code == kIndexed8Code) return ImageDepth::kIndexed8;
  return std::nullopt;
}

std::expected<void, std::string> SampleDescriptor(const Descriptor& descriptor,
                                                  const EffectBinding& binding,
                                                  std::size_t palette_rows,
                                                  EffectSamples& samples) {
  const std::optional<ImageDepth> indexed = IndexedDepthOf(
      (descriptor.page_word >> kPageDepthShift) & kPageDepthMask);
  const std::uint32_t page =
      (descriptor.page_word & kPageColumnMask) +
      (Content::kTexturePagesPerRow *
       ((descriptor.page_word >> kPageRowShift) & kPageRowMask));
  const std::uint32_t width = std::max(descriptor.width, 1U);
  if (descriptor.u + width > kPageTexels) {
    return std::unexpected(std::format(
        "{}: texture descriptor {} reads texels {}-{}, past its page's {}",
        binding.package, descriptor.index, descriptor.u,
        descriptor.u + width - 1, kPageTexels));
  }
  const std::uint32_t per_halfword =
      Content::PixelsPerHalfword(indexed.value_or(ImageDepth::kDirect16));
  const std::uint32_t first =
      page + (descriptor.u / per_halfword / Content::kTexturePageWidth);
  const std::uint32_t last = page + ((descriptor.u + width - 1) / per_halfword /
                                     Content::kTexturePageWidth);
  if (last >= Content::kTexturePageCount) {
    return std::unexpected(std::format(
        "{}: texture descriptor {} reads past VRAM texture page {}",
        binding.package, descriptor.index, Content::kTexturePageCount - 1));
  }
  for (std::uint32_t sampled = first; sampled <= last; ++sampled) {
    const auto bound = std::ranges::find_if(
        binding.textures, [sampled](const BoundTexture& texture) {
          return kFirstEffectPage + texture.slot == sampled;
        });
    if (bound == binding.textures.end()) continue;
    if (!indexed) {
      return std::unexpected(std::format(
          "{}: texture descriptor {} reads {} as direct colour; effect pages "
          "hold palette indices",
          binding.package, descriptor.index, bound->path));
    }
    const ImageDepth depth = *indexed;
    PageSamples& page_samples = samples.at(bound->slot);
    if (page_samples.depth && *page_samples.depth != depth) {
      return std::unexpected(std::format(
          "{}: texture descriptor {} reads {} at {} bits; an earlier one reads "
          "it at {}",
          binding.package, descriptor.index, bound->path,
          static_cast<int>(depth), static_cast<int>(*page_samples.depth)));
    }
    page_samples.depth = depth;
    auto palette = PaletteOf(descriptor, depth, binding, palette_rows);
    if (!palette) return std::unexpected(palette.error());
    ++page_samples.palettes[*palette];
  }
  return {};
}

// decomp: BATTLE.PRG parsePfileBlock1 (src/BATTLE/BATTLE.PRG/6E4CC.c:141)
std::expected<EffectSamples, std::string> SampleEffect(
    const EffectBinding& binding, const SourceFile& package,
    std::size_t palette_rows) {
  EffectSamples samples;
  auto block = TextureBlockOf(package);
  if (!block) return std::unexpected(block.error());
  if (block->empty()) return samples;
  if (block->size() < kDescriptorOffsetField + kHalfwordBytes) {
    return std::unexpected(
        std::format("{}: the texture block has no header", package.path));
  }
  const std::size_t descriptors = HalfwordAt(*block, kDescriptorOffsetField);
  if (descriptors > block->size() ||
      (block->size() - descriptors) % kDescriptorBytes != 0) {
    return std::unexpected(std::format(
        "{}: texture descriptors from byte {} do not fill the {}-byte block",
        package.path, descriptors, block->size()));
  }
  for (std::size_t offset = descriptors, index = 0; offset < block->size();
       offset += kDescriptorBytes, ++index) {
    const Descriptor descriptor{
        .index = index,
        .clut = HalfwordAt(*block, offset),
        .page_word = HalfwordAt(*block, offset + kPageWordField),
        .u = std::to_integer<std::uint32_t>((*block)[offset + kTexelUField]),
        .width = std::to_integer<std::uint32_t>(
            (*block)[offset + kTexelWidthField])};
    if (auto sampled =
            SampleDescriptor(descriptor, binding, palette_rows, samples);
        !sampled) {
      return std::unexpected(sampled.error());
    }
  }
  return samples;
}

// The (row, entry) most descriptors sample, the lowest on a tie; (0, 0)
// when none does.
std::pair<std::uint32_t, std::uint32_t> PreviewOf(const PageSamples& samples) {
  std::pair<std::uint32_t, std::uint32_t> preview{};
  std::size_t most = 0;
  for (const auto& [palette, count] : samples.palettes) {
    if (count > most) {
      preview = palette;
      most = count;
    }
  }
  return preview;
}

ImageDependency DependencyOf(const SourceFile& source, std::string_view role) {
  return {.source = source.path,
          .source_sha256 = HashOf(source),
          .role = std::string(role)};
}

// The conversions of one effect's pages.
std::expected<void, std::string> PlanEffect(const EffectBinding& binding,
                                            const SourceFile& purge,
                                            SourceCache& sources,
                                            std::vector<FbtConversion>& plan) {
  auto package = sources.Get(binding.package);
  if (!package) return std::unexpected(package.error());
  auto palette = sources.Get(binding.palette);
  if (!palette) return std::unexpected(palette.error());
  const std::size_t palette_rows = (*palette)->bytes.size() / kPaletteRowBytes;
  auto samples = SampleEffect(binding, **package, palette_rows);
  if (!samples) return std::unexpected(samples.error());
  std::vector<ImageDependency> dependencies{
      DependencyOf(**package, kSamplerRole),
      DependencyOf(**palette, kPreviewPaletteRole),
      DependencyOf(purge, kBindingRole)};
  std::ranges::sort(dependencies, {}, &ImageDependency::source);
  for (const BoundTexture& texture : binding.textures) {
    auto source = sources.Get(texture.path);
    if (!source) return std::unexpected(source.error());
    const PageSamples& page = samples->at(texture.slot);
    const auto [row, entry] = PreviewOf(page);
    plan.push_back({.texture = **source,
                    .effect = binding.effect,
                    .slot = texture.slot,
                    .depth = page.depth,
                    .palette = **palette,
                    .palette_row = row,
                    .palette_entry = entry,
                    .dependencies = dependencies});
  }
  return {};
}

std::string NoteOf(const FbtConversion& conversion) {
  if (conversion.depth) return {};
  return std::format(
      "No texture descriptor of effect {} samples this page, so its depth is "
      "unknown; it converts at 8 bits.",
      conversion.effect);
}

ImageProvenance ProvenanceOf(const FbtConversion& conversion,
                             std::string palette_id) {
  const std::uint32_t page = kFirstEffectPage + conversion.slot;
  return {
      .source = conversion.texture.path,
      .source_sha256 = HashOf(conversion.texture),
      .offset = 0,
      .tim = std::nullopt,
      .texture_page =
          Content::TexturePagePlacement{.effect = conversion.effect,
                                        .slot = conversion.slot,
                                        .page = page,
                                        .vram = Content::TexturePageRect(page)},
      .preview_palette =
          Content::PreviewPalette{.image = std::move(palette_id),
                                  .row = conversion.palette_row,
                                  .entry = conversion.palette_entry},
      .dependencies = conversion.dependencies,
      .note = NoteOf(conversion)};
}

// The page's indices at its depth with the preview palette row.
std::expected<NativeImage, std::string> ImageOf(
    const FbtConversion& conversion) {
  const SourceFile& source = conversion.texture;
  const ImageDepth depth = conversion.depth.value_or(ImageDepth::kIndexed8);
  if (!Content::IsIndexed(depth)) {
    return std::unexpected(std::format(
        "{}: an effect texture page holds palette indices", source.path));
  }
  const std::size_t capacity = Content::PaletteCapacity(depth);
  const Bytes palette = conversion.palette.bytes;
  if (conversion.palette_row >= palette.size() / kPaletteRowBytes ||
      conversion.palette_entry + capacity > kPaletteRowEntries) {
    return std::unexpected(
        std::format("{}: {} has no {}-entry palette at row {}, entry {}",
                    source.path, conversion.palette.path, capacity,
                    conversion.palette_row, conversion.palette_entry));
  }
  std::vector<std::uint16_t> preview;
  preview.reserve(capacity);
  const std::size_t palette_start =
      (conversion.palette_row * kPaletteRowBytes) +
      (conversion.palette_entry * kHalfwordBytes);
  for (std::size_t entry = 0; entry < capacity; ++entry) {
    preview.push_back(
        HalfwordAt(palette, palette_start + (entry * kHalfwordBytes)));
  }
  const std::uint32_t per_halfword = Content::PixelsPerHalfword(depth);
  const std::uint32_t bits = Content::BitsPerPixel(depth);
  const std::uint32_t mask = (1U << bits) - 1U;
  NativeImage image{.depth = depth,
                    .width = Content::kTexturePageWidth * per_halfword,
                    .height = Content::kTexturePageHeight,
                    .indices = {},
                    .texels = {},
                    .palettes = {std::move(preview)}};
  image.indices.reserve(kPageHalfwords * per_halfword);
  for (std::size_t offset = 0; offset < source.bytes.size();
       offset += kHalfwordBytes) {
    const std::uint16_t word = HalfwordAt(source.bytes, offset);
    for (std::uint32_t pixel = 0; pixel < per_halfword; ++pixel) {
      image.indices.push_back(
          static_cast<std::uint8_t>((word >> (pixel * bits)) & mask));
    }
  }
  if (auto valid = Content::ValidateImage(image); !valid) {
    return std::unexpected(std::format("{}: {}", source.path, valid.error()));
  }
  return image;
}

// The files must decode to the same document and pack back into every
// source halfword.
std::expected<void, std::string> VerifyRoundTrip(
    const ImageDocument& document, std::string_view stem,
    const std::vector<ImageFile>& files, const SourceFile& source) {
  auto decoded = DecodeConvertedFiles(document, stem, files, source);
  if (!decoded) return std::unexpected(decoded.error());
  const std::vector<std::uint16_t> words = Content::PackedWords(decoded->image);
  if (words.size() * kHalfwordBytes != source.bytes.size()) {
    return std::unexpected(std::format(
        "{}: the image packs into a different byte length", source.path));
  }
  for (std::size_t index = 0; index < words.size(); ++index) {
    if (words[index] != HalfwordAt(source.bytes, index * kHalfwordBytes)) {
      return std::unexpected(std::format(
          "{}: halfword {} does not rebuild the source", source.path, index));
    }
  }
  return {};
}

// The conversions of every page EFFPURGE.BIN binds, sorted by path.
std::expected<std::vector<FbtConversion>, std::string> PlanAll(
    std::span<const DataStore::Entry> entries, SourceCache& sources) {
  auto purge = sources.Get(kPurgePath);
  if (!purge) return std::unexpected(purge.error());
  auto tables = ReadPurgeTables(**purge);
  if (!tables) return std::unexpected(tables.error());
  auto files = EffectFiles::Index(entries);
  if (!files) return std::unexpected(files.error());
  auto bindings = BindEffects(*tables, *files);
  if (!bindings) return std::unexpected(bindings.error());
  std::vector<FbtConversion> conversions;
  for (const EffectBinding& binding : *bindings) {
    if (auto planned = PlanEffect(binding, **purge, sources, conversions);
        !planned) {
      return std::unexpected(planned.error());
    }
  }
  std::ranges::sort(conversions, {}, [](const FbtConversion& conversion) {
    return conversion.texture.path;
  });
  return conversions;
}
}  // namespace

std::expected<ConversionOutput, std::string> FbtOutputOf(
    const FbtConversion& conversion) {
  auto id = ImageIdForDiscPath(conversion.texture.path);
  if (!id) return std::unexpected(id.error());
  ConversionOutput output{.id = std::move(*id),
                          .source = conversion.texture.path,
                          .source_sha256 = HashOf(conversion.texture),
                          .converter = std::string(kFbtTextureConverterName),
                          .files = {},
                          .dependencies = {},
                          .unreferenced = false,
                          .note = NoteOf(conversion),
                          .alias_of = {}};
  for (const ImageDependency& dependency : conversion.dependencies) {
    output.dependencies.push_back({.source = dependency.source,
                                   .source_sha256 = dependency.source_sha256});
  }
  return output;
}

std::expected<ConvertedImage, std::string> ConvertFbtTexture(
    const FbtConversion& conversion) {
  const SourceFile& source = conversion.texture;
  if (!source.path.ends_with(kTextureExtension) ||
      source.bytes.size() != kTextureBytes) {
    return std::unexpected(std::format(
        "{}: an effect texture page is a {}-byte .FBT file; it has {} bytes",
        source.path, kTextureBytes, source.bytes.size()));
  }
  if (conversion.slot >= kEffectPageCount) {
    return std::unexpected(std::format("{}: slot {} is not one of 0-{}",
                                       source.path, conversion.slot,
                                       kEffectPageCount - 1));
  }
  auto image = ImageOf(conversion);
  if (!image) return std::unexpected(image.error());
  auto output = FbtOutputOf(conversion);
  if (!output) return std::unexpected(output.error());
  auto location = ImageLocationForDiscPath(source.path);
  if (!location) return std::unexpected(location.error());
  auto palette_id = FbcPaletteIdForDiscPath(conversion.palette.path);
  if (!palette_id) return std::unexpected(palette_id.error());
  const ImageDocument document{
      .id = output->id,
      .image = std::move(*image),
      .material = std::nullopt,
      .unreferenced = false,
      .provenance = ProvenanceOf(conversion, std::move(*palette_id))};
  auto files = EncodeImageFiles(document, location->stem);
  if (!files) {
    return std::unexpected(std::format("{}: {}", source.path, files.error()));
  }
  if (auto verified = VerifyRoundTrip(document, location->stem, *files, source);
      !verified) {
    return std::unexpected(verified.error());
  }
  return ListConvertedImage(std::move(*output), std::move(*location),
                            std::move(*files));
}

std::expected<FbtConversionPlan, std::string> PlanFbtConversions(
    std::span<const DataStore::Entry> entries, const SourceReader& read) {
  std::set<std::string_view> listed;
  for (const DataStore::Entry& entry : entries) {
    if (entry.form == kRawForm && entry.path.ends_with(kTextureExtension)) {
      listed.insert(entry.path);
    }
  }
  if (listed.empty()) return FbtConversionPlan{};
  SourceCache sources(read);
  auto conversions = PlanAll(entries, sources);
  if (!conversions) return std::unexpected(conversions.error());
  for (const FbtConversion& conversion : *conversions) {
    listed.erase(conversion.texture.path);
  }
  if (!listed.empty()) {
    return std::unexpected(
        std::format("{}: no effect in {} loads this texture page",
                    *listed.begin(), kPurgePath));
  }
  // Pages with the same bytes and depth share the first one's files.
  FbtConversionPlan plan;
  std::map<std::pair<std::string, int>, std::string> canonical;
  for (FbtConversion& conversion : *conversions) {
    auto output = FbtOutputOf(conversion);
    if (!output) return std::unexpected(output.error());
    const int depth =
        conversion.depth ? static_cast<int>(*conversion.depth) : 0;
    const auto [first, added] =
        canonical.try_emplace({output->source_sha256, depth}, output->id);
    if (added) {
      plan.conversions.push_back(std::move(conversion));
    } else {
      output->alias_of = first->second;
      plan.aliases.push_back(std::move(*output));
    }
  }
  return plan;
}

}  // namespace VS::Extract
