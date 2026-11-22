// The syd-recipe-csv converter (WorkshopRecipeConversion.hpp).
#include "Extract/WorkshopRecipeConversion.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Content/DataStore.hpp"
#include "Platform/Paths.hpp"

namespace VS::Extract {
using Content::BladeResult;
using Content::ConversionManifest;
using Content::ConversionOutput;
using Content::DataStore;
using Content::kSydRecipeConverterName;
using Content::kSydRecipeConverterVersion;
using Content::kWorkshopKinds;
using Content::kWorkshopTableDirectory;
using Content::kWorkshopTableParts;
using Content::LayoutOf;
using Content::MaterialRule;
using Content::PieceResult;
using Content::WorkshopKind;
using Content::WorkshopLayout;
using Content::WorkshopPair;
using Content::WorkshopRecipeTable;
using Content::WorkshopStats;
using Content::WorkshopTablePart;

namespace {
namespace fs = std::filesystem;

constexpr std::string_view kRawForm = "raw";
constexpr std::string_view kSydExtension = ".SYD";
constexpr std::string_view kOutputIdPrefix = "catalog.menu.combination";
constexpr std::size_t kHeaderWords = 3;
constexpr std::size_t kWordBytes = 4;
constexpr std::size_t kHeaderBytes = kHeaderWords * kWordBytes;
constexpr unsigned kByteBits = 8;
constexpr std::uint8_t kLastMaterial = 7;

// Blade record bytes (BLADE.SYD, _combineBlades' info).
enum BladeByte : std::uint8_t {
  kBladeIdByte,
  kBladeModelByte,
  kBladeCategoryByte,
  kBladeDamageTypeByte,
  kBladeCostTypeByte,
  kBladeCostByte,
  kBladeRecoveryDelayByte,
  kBladeRaw7Byte,
  kBladeStrengthByte,
  kBladeIntelligenceByte,
  kBladeAgilityByte,
  kBladeRaw11Byte,
  kBladeRangeByte,
  kBladeRangeYByte,
  kBladeRangeZByte,
  kBladeShapeByte,
};
// Shield and armor record bytes (_armorInfo).
enum PieceByte : std::uint8_t {
  kPieceIdByte,
  kPieceModelByte,
  kPieceCategoryByte,
  kPieceGemSlotsByte,
  kPieceStrengthByte,
  kPieceIntelligenceByte,
  kPieceAgilityByte,
  kPieceRaw7Byte,
};
// The shape byte: the shape in bits 0-2, the angle in bits 3-7.
constexpr unsigned kAngleShift = 3;
constexpr std::uint8_t kShapeMask = 0x7;

struct SydLayout {
  std::array<std::size_t, kHeaderWords> offsets{};
  std::size_t records = 0;
  std::size_t size = 0;
};

constexpr std::size_t AlignWord(std::size_t value) {
  return (value + kWordBytes - 1) / kWordBytes * kWordBytes;
}

SydLayout SydLayoutOf(const WorkshopLayout& layout) {
  SydLayout syd;
  syd.offsets[0] = kHeaderBytes;
  syd.offsets[1] =
      AlignWord(kHeaderBytes + (layout.pair_side * layout.pair_side));
  syd.offsets[2] = syd.offsets[1] + layout.material_entries;
  syd.records = layout.first_pair_id + layout.pair_side;
  syd.size = syd.offsets[2] + (syd.records * layout.record_bytes);
  return syd;
}

std::uint8_t ByteAt(std::span<const std::byte> file, std::size_t offset) {
  return std::to_integer<std::uint8_t>(file[offset]);
}

std::int8_t SignedAt(std::span<const std::byte> file, std::size_t offset) {
  return static_cast<std::int8_t>(ByteAt(file, offset));
}

std::uint32_t WordAt(std::span<const std::byte> file, std::size_t offset) {
  std::uint32_t value = 0;
  for (std::size_t index = 0; index < kWordBytes; ++index) {
    value |= static_cast<std::uint32_t>(ByteAt(file, offset + index))
             << (index * kByteBits);
  }
  return value;
}

std::string ResultId(const WorkshopLayout& layout, std::size_t legacy) {
  return std::format("{}-{:03}", layout.name, legacy);
}

std::expected<void, std::string> CheckFrame(const WorkshopLayout& layout,
                                            const SydLayout& syd,
                                            std::span<const std::byte> file) {
  if (file.size() != syd.size) {
    return std::unexpected(std::format("{}: has {} bytes; its layout takes {}",
                                       layout.source, file.size(), syd.size));
  }
  for (std::size_t word = 0; word < kHeaderWords; ++word) {
    const std::uint32_t offset = WordAt(file, word * kWordBytes);
    if (offset != syd.offsets.at(word)) {
      return std::unexpected(
          std::format("{}: table {} starts at {}; its layout puts it at {}",
                      layout.source, word, offset, syd.offsets.at(word)));
    }
  }
  for (std::size_t at = kHeaderBytes + (layout.pair_side * layout.pair_side);
       at < syd.offsets[1]; ++at) {
    if (ByteAt(file, at) != 0) {
      return std::unexpected(std::format("{}: padding byte {} is {}, not 0",
                                         layout.source, at, ByteAt(file, at)));
    }
  }
  return {};
}

BladeResult BladeAt(const WorkshopLayout& layout,
                    std::span<const std::byte> record, std::size_t legacy) {
  const std::uint8_t shape = ByteAt(record, kBladeShapeByte);
  return {.id = ResultId(layout, legacy),
          .legacy_id = static_cast<std::uint32_t>(legacy),
          .model = ByteAt(record, kBladeModelByte),
          .category = ByteAt(record, kBladeCategoryByte),
          .damage_type = ByteAt(record, kBladeDamageTypeByte),
          .cost_type = ByteAt(record, kBladeCostTypeByte),
          .cost = ByteAt(record, kBladeCostByte),
          .recovery_delay = ByteAt(record, kBladeRecoveryDelayByte),
          .stats = {.strength = SignedAt(record, kBladeStrengthByte),
                    .intelligence = SignedAt(record, kBladeIntelligenceByte),
                    .agility = SignedAt(record, kBladeAgilityByte)},
          .range = ByteAt(record, kBladeRangeByte),
          .range_y = ByteAt(record, kBladeRangeYByte),
          .range_z = ByteAt(record, kBladeRangeZByte),
          .attack_shape = static_cast<std::uint8_t>(shape & kShapeMask),
          .attack_angle = static_cast<std::uint8_t>(shape >> kAngleShift),
          .raw_byte7 = ByteAt(record, kBladeRaw7Byte),
          .raw_byte11 = ByteAt(record, kBladeRaw11Byte)};
}

PieceResult PieceAt(const WorkshopLayout& layout,
                    std::span<const std::byte> record, std::size_t legacy) {
  return {.id = ResultId(layout, legacy),
          .legacy_id = static_cast<std::uint32_t>(legacy),
          .model = ByteAt(record, kPieceModelByte),
          .category = ByteAt(record, kPieceCategoryByte),
          .gem_slots = ByteAt(record, kPieceGemSlotsByte),
          .stats = {.strength = SignedAt(record, kPieceStrengthByte),
                    .intelligence = SignedAt(record, kPieceIntelligenceByte),
                    .agility = SignedAt(record, kPieceAgilityByte)},
          .raw_byte7 = ByteAt(record, kPieceRaw7Byte)};
}

std::expected<void, std::string> DecodeRecords(const WorkshopLayout& layout,
                                               const SydLayout& syd,
                                               std::span<const std::byte> file,
                                               WorkshopRecipeTable& table) {
  for (std::size_t legacy = 0; legacy < syd.records; ++legacy) {
    const std::span<const std::byte> record = file.subspan(
        syd.offsets[2] + (legacy * layout.record_bytes), layout.record_bytes);
    if (ByteAt(record, 0) != legacy) {
      return std::unexpected(std::format("{}: record {} starts with {}",
                                         layout.source, legacy,
                                         ByteAt(record, 0)));
    }
    if (table.kind == WorkshopKind::kBlade) {
      table.blades.push_back(BladeAt(layout, record, legacy));
    } else {
      table.pieces.push_back(PieceAt(layout, record, legacy));
    }
  }
  return {};
}

std::expected<void, std::string> DecodePairs(const WorkshopLayout& layout,
                                             const SydLayout& syd,
                                             std::span<const std::byte> file,
                                             WorkshopRecipeTable& table) {
  for (std::size_t first = 0; first < layout.pair_side; ++first) {
    for (std::size_t second = 0; second < layout.pair_side; ++second) {
      const std::uint8_t result =
          ByteAt(file, syd.offsets[0] + (first * layout.pair_side) + second);
      if (result == 0) continue;
      if (result >= syd.records) {
        return std::unexpected(
            std::format("{}: pair ({}, {}) names record {} of {}",
                        layout.source, first + layout.first_pair_id,
                        second + layout.first_pair_id, result, syd.records));
      }
      table.pairs.push_back(
          {.first = ResultId(layout, first + layout.first_pair_id),
           .second = ResultId(layout, second + layout.first_pair_id),
           .result = ResultId(layout, result)});
    }
  }
  return {};
}

// Every category a material entry ranges over: one, 0, without categories.
std::uint8_t CategoryCount(const WorkshopLayout& layout) {
  return layout.by_category ? layout.category_count : 1;
}

// The ingredients of material combination `ordinal`, counting the second
// category fastest, then the second material, the first category and the
// first material (the rows' order).
MaterialRule MaterialKeyAt(const WorkshopLayout& layout, std::size_t ordinal) {
  const std::size_t categories = CategoryCount(layout);
  const std::size_t second_category = ordinal % categories;
  ordinal /= categories;
  const std::size_t second_material = ordinal % layout.material_count;
  ordinal /= layout.material_count;
  const std::size_t first_category = ordinal % categories;
  const std::size_t first_material = ordinal / categories;
  const auto material = [&layout](std::size_t step) {
    return static_cast<std::uint8_t>(layout.material_base + step);
  };
  const auto category = [&layout](std::size_t step) {
    return static_cast<std::uint8_t>(
        layout.by_category ? layout.category_base + step : 0);
  };
  return {.first_material = material(first_material),
          .first_category = category(first_category),
          .second_material = material(second_material),
          .second_category = category(second_category),
          .material = 0};
}

std::expected<void, std::string> DecodeMaterials(
    const WorkshopLayout& layout, const SydLayout& syd,
    std::span<const std::byte> file, WorkshopRecipeTable& table) {
  const std::size_t categories = CategoryCount(layout);
  const std::size_t combinations = std::size_t{layout.material_count} *
                                   categories * layout.material_count *
                                   categories;
  if (combinations != layout.material_entries) {
    return std::unexpected(
        std::format("{}: the material layout covers {} of {} entries",
                    layout.source, combinations, layout.material_entries));
  }
  const std::span<const std::byte> entries =
      file.subspan(syd.offsets[1], layout.material_entries);
  for (std::size_t ordinal = 0; ordinal < combinations; ++ordinal) {
    MaterialRule rule = MaterialKeyAt(layout, ordinal);
    const std::size_t index = Content::MaterialIndex(layout, rule);
    rule.material = ByteAt(entries, index);
    if (rule.material == 0) continue;
    if (rule.material > kLastMaterial) {
      return std::unexpected(std::format("{}: material entry {} is {}, past {}",
                                         layout.source, index, rule.material,
                                         kLastMaterial));
    }
    table.materials.push_back(rule);
  }
  return {};
}

void PutWord(std::vector<std::byte>& file, std::size_t offset,
             std::size_t value) {
  for (std::size_t index = 0; index < kWordBytes; ++index) {
    file[offset + index] = static_cast<std::byte>(
        static_cast<std::uint8_t>(value >> (index * kByteBits)));
  }
}

// Stores value's byte (a signed value as its two's complement byte).
template <typename Value>
void PutByte(std::span<std::byte> record, std::size_t at, Value value) {
  record[at] = static_cast<std::byte>(static_cast<std::uint8_t>(value));
}

void PutBlade(std::span<std::byte> record, const BladeResult& blade) {
  PutByte(record, kBladeIdByte, blade.legacy_id);
  PutByte(record, kBladeModelByte, blade.model);
  PutByte(record, kBladeCategoryByte, blade.category);
  PutByte(record, kBladeDamageTypeByte, blade.damage_type);
  PutByte(record, kBladeCostTypeByte, blade.cost_type);
  PutByte(record, kBladeCostByte, blade.cost);
  PutByte(record, kBladeRecoveryDelayByte, blade.recovery_delay);
  PutByte(record, kBladeRaw7Byte, blade.raw_byte7);
  PutByte(record, kBladeStrengthByte, blade.stats.strength);
  PutByte(record, kBladeIntelligenceByte, blade.stats.intelligence);
  PutByte(record, kBladeAgilityByte, blade.stats.agility);
  PutByte(record, kBladeRaw11Byte, blade.raw_byte11);
  PutByte(record, kBladeRangeByte, blade.range);
  PutByte(record, kBladeRangeYByte, blade.range_y);
  PutByte(record, kBladeRangeZByte, blade.range_z);
  PutByte(record, kBladeShapeByte,
          blade.attack_shape | (blade.attack_angle << kAngleShift));
}

void PutPiece(std::span<std::byte> record, const PieceResult& piece) {
  PutByte(record, kPieceIdByte, piece.legacy_id);
  PutByte(record, kPieceModelByte, piece.model);
  PutByte(record, kPieceCategoryByte, piece.category);
  PutByte(record, kPieceGemSlotsByte, piece.gem_slots);
  PutByte(record, kPieceStrengthByte, piece.stats.strength);
  PutByte(record, kPieceIntelligenceByte, piece.stats.intelligence);
  PutByte(record, kPieceAgilityByte, piece.stats.agility);
  PutByte(record, kPieceRaw7Byte, piece.raw_byte7);
}

std::string OutputIdOf(WorkshopKind kind) {
  return std::format("{}-{}", kOutputIdPrefix, LayoutOf(kind).name);
}

std::vector<std::string> OutputFilesOf(WorkshopKind kind) {
  std::vector<std::string> files;
  for (const WorkshopTablePart part : kWorkshopTableParts) {
    files.push_back(Content::WorkshopTablePath(kind, part));
    files.push_back(Content::WorkshopSchemaPath(kind, part));
  }
  return files;
}

// The file name of `path` under catalogs/menu.
std::string FileNameOf(std::string_view path) {
  return std::string(path.substr(kWorkshopTableDirectory.size() + 1));
}

// The manifest's output for `source` when its hash is the one recorded and
// its files are the expected ones and exist.
const ConversionOutput* UnchangedOutput(const ConversionManifest& manifest,
                                        const fs::path& root,
                                        const SourceFile& source,
                                        WorkshopKind kind) {
  const ConversionOutput* output = manifest.Find(OutputIdOf(kind));
  if (output == nullptr || output->converter != kSydRecipeConverterName ||
      output->source != source.path ||
      output->source_sha256 != HashOf(source) ||
      !output->dependencies.empty() || output->files != OutputFilesOf(kind)) {
    return nullptr;
  }
  return PresentOutput(root, output);
}

std::expected<void, std::string> WriteTables(
    const fs::path& root, const std::vector<ConvertedTables>& converted) {
  std::vector<OutputBytes> files;
  for (const ConvertedTables& tables : converted) {
    for (const TableFile& file : tables.files) {
      files.push_back(
          {.name = file.name, .bytes = std::as_bytes(std::span(file.text))});
    }
  }
  if (files.empty()) return {};
  return WriteOutputFiles(root / Platform::Utf8Path(kWorkshopTableDirectory),
                          files);
}

}  // namespace

std::optional<WorkshopKind> WorkshopKindForDiscPath(
    std::string_view disc_path) {
  for (const WorkshopKind kind : kWorkshopKinds) {
    if (LayoutOf(kind).source == disc_path) return kind;
  }
  return std::nullopt;
}

std::expected<WorkshopRecipeTable, std::string> DecodeSydRecipes(
    WorkshopKind kind, std::span<const std::byte> file) {
  const WorkshopLayout& layout = LayoutOf(kind);
  const SydLayout syd = SydLayoutOf(layout);
  if (auto framed = CheckFrame(layout, syd, file); !framed) {
    return std::unexpected(framed.error());
  }
  WorkshopRecipeTable table{};
  table.kind = kind;
  if (auto records = DecodeRecords(layout, syd, file, table); !records) {
    return std::unexpected(records.error());
  }
  if (auto pairs = DecodePairs(layout, syd, file, table); !pairs) {
    return std::unexpected(pairs.error());
  }
  if (auto materials = DecodeMaterials(layout, syd, file, table); !materials) {
    return std::unexpected(materials.error());
  }
  return table;
}

std::expected<std::vector<std::byte>, std::string> EncodeSydRecipes(
    const WorkshopRecipeTable& table) {
  const WorkshopLayout& layout = LayoutOf(table.kind);
  auto matrices = Content::DenseWorkshopTables(table);
  if (!matrices) return std::unexpected(matrices.error());
  SydLayout syd = SydLayoutOf(layout);
  syd.records = table.kind == WorkshopKind::kBlade ? table.blades.size()
                                                   : table.pieces.size();
  syd.size = syd.offsets[2] + (syd.records * layout.record_bytes);
  std::vector<std::byte> file(syd.size);
  for (std::size_t word = 0; word < kHeaderWords; ++word) {
    PutWord(file, word * kWordBytes, syd.offsets.at(word));
  }
  const std::span<std::byte> bytes(file);
  const auto as_byte = [](std::uint8_t value) { return std::byte{value}; };
  std::ranges::transform(matrices->results,
                         bytes.subspan(syd.offsets[0]).begin(), as_byte);
  std::ranges::transform(matrices->materials,
                         bytes.subspan(syd.offsets[1]).begin(), as_byte);
  const std::span<std::byte> records = bytes.subspan(syd.offsets[2]);
  for (std::size_t index = 0; index < table.blades.size(); ++index) {
    PutBlade(records.subspan(index * layout.record_bytes, layout.record_bytes),
             table.blades[index]);
  }
  for (std::size_t index = 0; index < table.pieces.size(); ++index) {
    PutPiece(records.subspan(index * layout.record_bytes, layout.record_bytes),
             table.pieces[index]);
  }
  return file;
}

std::expected<ConvertedTables, std::string> ConvertSydRecipes(
    const SourceFile& source) {
  const auto kind = WorkshopKindForDiscPath(source.path);
  if (!kind) {
    return std::unexpected(std::format(
        "{}: no workshop recipe layout is known for this file", source.path));
  }
  auto decoded = DecodeSydRecipes(*kind, source.bytes);
  if (!decoded) return std::unexpected(decoded.error());
  const Content::WorkshopTexts texts = Content::FormatWorkshopTables(*decoded);
  auto parsed = Content::ParseWorkshopTables(*kind, texts);
  if (!parsed) {
    return std::unexpected(
        std::format("{}: the converted tables do not read back: {}",
                    source.path, parsed.error()));
  }
  if (*parsed != *decoded) {
    return std::unexpected(
        std::format("{}: the converted tables read back to different recipes",
                    source.path));
  }
  auto encoded = EncodeSydRecipes(*parsed);
  if (!encoded) {
    return std::unexpected(std::format("{}: {}", source.path, encoded.error()));
  }
  if (*encoded != source.bytes) {
    return std::unexpected(
        std::format("{}: the converted tables do not rebuild the source bytes",
                    source.path));
  }
  ConvertedTables converted{
      .output = {.id = OutputIdOf(*kind),
                 .source = source.path,
                 .source_sha256 = HashOf(source),
                 .converter = std::string(kSydRecipeConverterName),
                 .files = OutputFilesOf(*kind),
                 .dependencies = {},
                 .unreferenced = false,
                 .note = {},
                 .alias_of = {}},
      .files = {}};
  for (const WorkshopTablePart part : kWorkshopTableParts) {
    const auto index = static_cast<std::size_t>(part);
    converted.files.push_back(
        {.name = FileNameOf(Content::WorkshopTablePath(*kind, part)),
         .text = texts.at(index)});
    converted.files.push_back(
        {.name = FileNameOf(Content::WorkshopSchemaPath(*kind, part)),
         .text = Content::FormatTableSchema(
             Content::WorkshopTableSpec(*kind, part))});
  }
  return converted;
}

std::expected<TableConversionReport, std::string> ConvertDiscTables(
    const fs::path& root, bool force) {
  auto entries = DataStore::LoadManifest(root);
  if (!entries) return std::unexpected(entries.error());
  auto loaded = Content::LoadConversionManifest(root);
  if (!loaded) return std::unexpected(loaded.error());
  const ConversionManifest previous = loaded->value_or(ConversionManifest{});
  ConverterBatch batch = StartBatch(previous, kSydRecipeConverterName,
                                    kSydRecipeConverterVersion, force);
  TableConversionReport report;
  std::vector<ConvertedTables> converted;
  for (const DataStore::Entry& entry : *entries) {
    if (entry.form != kRawForm || !entry.path.ends_with(kSydExtension)) {
      continue;
    }
    auto source = ReadSource(root, entry.path);
    if (!source) return std::unexpected(source.error());
    const auto kind = WorkshopKindForDiscPath(source->path);
    const ConversionOutput* kept =
        batch.reuse && kind ? UnchangedOutput(previous, root, *source, *kind)
                            : nullptr;
    if (kept != nullptr) {
      batch.outputs.push_back(*kept);
      ++report.unchanged;
      continue;
    }
    auto tables = ConvertSydRecipes(*source);
    if (!tables) return std::unexpected(tables.error());
    batch.outputs.push_back(tables->output);
    converted.push_back(std::move(*tables));
  }
  if (auto written = WriteTables(root, converted); !written) {
    return std::unexpected(written.error());
  }
  auto removed = PublishBatches(root, previous, std::span(&batch, 1));
  if (!removed) return std::unexpected(removed.error());
  report.converted = converted.size();
  report.removed = *removed;
  return report;
}

}  // namespace VS::Extract
