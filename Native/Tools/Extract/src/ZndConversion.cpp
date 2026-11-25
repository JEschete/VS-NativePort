// The znd-tables converter (ZndConversion.hpp).
#include "Extract/ZndConversion.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <format>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Content/ActorArchetype.hpp"
#include "Content/CatalogTable.hpp"
#include "Content/ConversionManifest.hpp"
#include "Content/ConvertedZones.hpp"
#include "Content/DataStore.hpp"
#include "Content/ImageFiles.hpp"
#include "Content/NativeImage.hpp"
#include "Content/TimFile.hpp"
#include "Content/ZndFile.hpp"
#include "Content/ZoneTables.hpp"
#include "Extract/ConverterBatch.hpp"
#include "Extract/ImageConversion.hpp"
#include "Extract/ImageFilesWriter.hpp"
#include "Extract/TimImage.hpp"
#include "Extract/ZoneAuthoring.hpp"
#include "Platform/Paths.hpp"

namespace VS::Extract {
namespace {

namespace fs = std::filesystem;
using Content::ConversionManifest;
using Content::ConversionOutput;
using Content::DataStore;
using Content::ImageDocument;
using Content::ImageFile;
using Content::ZndFileEntry;
using Content::ZoneRecord;
using Content::ZoneTable;

constexpr std::string_view kRawForm = "raw";
constexpr std::size_t kWordBytes = 4;
// The texture section's offset word in the zone header, and the bytes before
// its first TIM: the four table sizes, the count and the TIM's size word.
constexpr std::size_t kTexturesOffsetAt = 0x10;
constexpr std::size_t kTextureTableAt = 4 * kWordBytes;
// The note each zone texture's sidecar carries.
std::string ZoneTextureNote() {
  return "A zone texture table TIM, kept as the 16-bit words the zone "
         "uploads. Room texture pages hold packed 4- or 8-bit indices until "
         "the rooms (#198) bind each page's depth and palette.";
}

std::uint32_t ReadWord(std::span<const std::byte> data, std::size_t at) {
  std::uint32_t word = 0;
  for (std::size_t index = 0; index < kWordBytes; ++index) {
    word |= std::to_integer<std::uint32_t>(data[at + index]) << (8 * index);
  }
  return word;
}

std::uint64_t RoundedSize(const DataStore::Entry& entry) {
  return std::uint64_t{entry.SectorCount()} * DataStore::kDataSectorBytes;
}

// The extracted file a zone reads at `read`: it must start at that sector,
// have `extension`, and be read as its whole sectors.
std::expected<const DataStore::Entry*, std::string> FileRead(
    std::span<const DataStore::Entry> files, const ZndFileEntry& read,
    std::string_view extension) {
  const auto found = std::ranges::find_if(files, [&](const auto& entry) {
    return entry.form == kRawForm &&
           entry.lba == static_cast<std::uint32_t>(read.sector) &&
           entry.path.ends_with(extension);
  });
  if (found == files.end() || read.bytes < 0 ||
      RoundedSize(*found) != static_cast<std::uint64_t>(read.bytes)) {
    return std::unexpected(
        std::format("sector {} ({} bytes) is no extracted {} file's read",
                    read.sector, read.bytes, extension));
  }
  return &*found;
}

std::expected<Content::ImageDepth, std::string> DepthOf(
    const Content::TimFile& tim) {
  switch (tim.Mode()) {
    case Content::TimPixelMode::kIndexed4:
      return Content::ImageDepth::kIndexed4;
    case Content::TimPixelMode::kIndexed8:
      return Content::ImageDepth::kIndexed8;
    case Content::TimPixelMode::kDirect15:
      return Content::ImageDepth::kDirect16;
    case Content::TimPixelMode::kDirect24:
      break;
  }
  return std::unexpected("24-bit TIMs have no native image depth");
}

struct ConvertedTexture {
  std::string id;
  std::vector<ImageFile> files;
};

// The image files of the texture table's TIM `index`, which starts `offset`
// bytes into the zone.
std::expected<ConvertedTexture, std::string> ConvertTexture(
    const SourceFile& zone, std::string_view zone_id, std::size_t index,
    std::span<const std::byte> payload, std::size_t offset) {
  const std::string where = std::format("{} texture {}", zone.path, index);
  auto decoded = Content::DecodeTim(payload);
  if (!decoded) {
    return std::unexpected(
        std::format("{}: {}", where, decoded.error().message));
  }
  if (decoded->bytes != payload.size()) {
    return std::unexpected(std::format("{}: {} bytes follow the TIM", where,
                                       payload.size() - decoded->bytes));
  }
  auto depth = DepthOf(decoded->tim);
  if (!depth)
    return std::unexpected(std::format("{}: {}", where, depth.error()));
  auto image = ImageFromTim(decoded->tim, *depth);
  if (!image)
    return std::unexpected(std::format("{}: {}", where, image.error()));
  const std::string id = Content::ZoneTextureId(zone_id, index);
  auto location = Content::ZoneTextureLocationOf(id);
  if (!location) return std::unexpected(location.error());
  const ImageDocument document{
      .id = id,
      .image = std::move(*image),
      .material = std::nullopt,
      .unreferenced = false,
      .provenance = {.source = zone.path,
                     .source_sha256 = HashOf(zone),
                     .offset = offset,
                     .tim = Content::OriginOf(decoded->tim),
                     .texture_page = std::nullopt,
                     .preview_palette = std::nullopt,
                     .dependencies = {},
                     .note = ZoneTextureNote()}};
  auto files = EncodeImageFiles(document, location->stem);
  if (!files)
    return std::unexpected(std::format("{}: {}", where, files.error()));
  const SourceFile tim{.path = where,
                       .bytes = {payload.begin(), payload.end()}};
  auto read_back = DecodeConvertedFiles(document, location->stem, *files, tim);
  if (!read_back) return std::unexpected(read_back.error());
  return ConvertedTexture{.id = id, .files = std::move(*files)};
}

// The TIM bytes a converted texture's files rebuild.
std::expected<std::vector<std::byte>, std::string> TextureBytesOf(
    const ConvertedTexture& texture) {
  auto location = Content::ZoneTextureLocationOf(texture.id);
  if (!location) return std::unexpected(location.error());
  auto document = Content::DecodeImageFiles(
      location->stem, Content::FileListReader(texture.files));
  if (!document) return std::unexpected(document.error());
  if (!document->provenance.tim) {
    return std::unexpected(std::format("{} records no source TIM", texture.id));
  }
  auto tim = Content::TimFromImage(document->image, *document->provenance.tim);
  if (!tim)
    return std::unexpected(std::format("{}: {}", texture.id, tim.error()));
  return Content::EncodeTim(*tim);
}

std::vector<std::string> TableFiles() {
  std::vector<std::string> files;
  for (const ZoneTable table : Content::kZoneTables) {
    files.push_back(Content::ZoneTablePath(table));
    files.push_back(Content::ZoneTableSchemaPath(table));
  }
  return files;
}

// Whether every zone's output is unchanged and its files exist.
bool AllUnchanged(const ConversionManifest& previous, const fs::path& root,
                  std::span<const SourceFile> zones,
                  std::vector<ConversionOutput>& kept) {
  for (const SourceFile& zone : zones) {
    auto id = Content::DiscFileId(Content::kZoneIdKind, zone.path);
    if (!id) return false;
    const ConversionOutput expected{
        .id = *id,
        .source = zone.path,
        .source_sha256 = HashOf(zone),
        .converter = std::string(Content::kZndTableConverterName),
        .files = {},
        .dependencies = {},
        .unreferenced = false,
        .note = {},
        .alias_of = {}};
    const ConversionOutput* output = MatchingOutput(previous, root, expected);
    if (output == nullptr) return false;
    kept.push_back(*output);
  }
  return true;
}

std::expected<void, std::string> WriteZones(
    const fs::path& root, std::span<const ConvertedZone> zones,
    const std::array<std::string, Content::kZoneTableCount>& tables) {
  // Without zones no output lists the tables, so none are written.
  if (zones.empty()) return {};
  std::map<std::string, std::vector<std::pair<std::string, std::string>>>
      by_directory;
  for (const ZoneTable table : Content::kZoneTables) {
    const std::string csv = Content::ZoneTablePath(table);
    const std::string schema = Content::ZoneTableSchemaPath(table);
    const std::string directory = csv.substr(0, csv.rfind('/'));
    by_directory[directory].emplace_back(
        csv.substr(csv.rfind('/') + 1),
        tables.at(static_cast<std::size_t>(table)));
    by_directory[directory].emplace_back(
        schema.substr(schema.rfind('/') + 1),
        Content::FormatTableSchema(Content::ZoneTableSpec(table)));
  }
  for (const auto& [directory, files] : by_directory) {
    std::vector<OutputBytes> outputs;
    for (const auto& [name, text] : files) {
      outputs.push_back(
          {.name = name,
           .bytes = std::as_bytes(std::span(text.data(), text.size()))});
    }
    if (auto written =
            WriteOutputFiles(root / Platform::Utf8Path(directory), outputs);
        !written) {
      return written;
    }
  }
  for (const ConvertedZone& zone : zones) {
    if (zone.image_files.empty()) continue;
    if (auto written = WriteImageFiles(
            root / Platform::Utf8Path(zone.image_directory), zone.image_files);
        !written) {
      return written;
    }
  }
  return {};
}

}  // namespace

std::expected<ConvertedZone, std::string> ConvertZone(
    const ZoneConversion& conversion) {
  const SourceFile& source = conversion.znd;
  auto zone = Content::DecodeZndFile(source.bytes);
  if (!zone)
    return std::unexpected(std::format("{}: {}", source.path, zone.error()));
  auto id = Content::DiscFileId(Content::kZoneIdKind, source.path);
  if (!id) return std::unexpected(id.error());
  ConvertedZone converted;
  ZoneRecord& record = converted.record;
  record.id = *id;
  record.music_id = zone->music_id;
  for (std::size_t slot = 0; slot < zone->rooms.size(); ++slot) {
    const ZndFileEntry& room = zone->rooms[slot];
    if (room.sector == 0 && room.bytes == 0) {
      record.rooms.emplace_back();
      continue;
    }
    auto file = FileRead(conversion.files, room, Content::kRoomExtension);
    if (!file) {
      return std::unexpected(
          std::format("{}: room slot {}: {}", source.path, slot, file.error()));
    }
    auto room_id = Content::DiscFileId(Content::kRoomIdKind, (*file)->path);
    if (!room_id) return std::unexpected(room_id.error());
    record.rooms.push_back(std::move(*room_id));
  }
  for (std::size_t slot = 0; slot < zone->enemies.size(); ++slot) {
    const Content::ZndEnemy& enemy = zone->enemies[slot];
    auto file = FileRead(conversion.files, enemy.zud, ".ZUD");
    if (!file) {
      return std::unexpected(std::format("{}: enemy {}'s ZUD: {}", source.path,
                                         slot, file.error()));
    }
    auto archetype =
        Content::DiscFileId(Content::kArchetypeIdKind, (*file)->path);
    if (!archetype) return std::unexpected(archetype.error());
    record.enemies.push_back({.id = Content::ZoneEnemyId(record.id, slot),
                              .archetype = std::move(*archetype),
                              .data = enemy.data});
  }
  std::vector<ConvertedTexture> textures;
  if (zone->textures) {
    if (zone->textures->empty()) {
      return std::unexpected(std::format(
          "{}: an empty texture table has no native form", source.path));
    }
    // Each TIM follows the table sizes, the count and its own size word.
    std::size_t offset = ReadWord(source.bytes, kTexturesOffsetAt) +
                         kTextureTableAt + kWordBytes;
    for (std::size_t index = 0; index < zone->textures->size(); ++index) {
      const std::vector<std::byte>& payload = zone->textures->at(index);
      offset += kWordBytes;
      auto texture = ConvertTexture(source, record.id, index, payload, offset);
      if (!texture) return std::unexpected(texture.error());
      record.textures.push_back(texture->id);
      textures.push_back(std::move(*texture));
      offset += payload.size();
    }
  }
  // The rows and images must rebuild the zone byte for byte.
  const Content::ZoneFileResolver resolve{
      .file = [&conversion](std::string_view path)
          -> std::expected<ZndFileEntry, std::string> {
        const auto found =
            std::ranges::find(conversion.files, path, &DataStore::Entry::path);
        if (found == conversion.files.end()) {
          return std::unexpected(std::format("{} is not extracted", path));
        }
        return ZndFileEntry{
            .sector = static_cast<std::int32_t>(found->lba),
            .bytes = static_cast<std::int32_t>(RoundedSize(*found))};
      },
      .texture = [&textures](std::string_view image_id)
          -> std::expected<std::vector<std::byte>, std::string> {
        const auto found =
            std::ranges::find(textures, image_id, &ConvertedTexture::id);
        if (found == textures.end()) {
          return std::unexpected(std::format("no image {}", image_id));
        }
        return TextureBytesOf(*found);
      }};
  auto rebuilt = Content::ZndFileOf(record, resolve);
  if (!rebuilt) return std::unexpected(rebuilt.error());
  auto bytes = Content::EncodeZndFile(*rebuilt);
  if (!bytes)
    return std::unexpected(std::format("{}: {}", source.path, bytes.error()));
  if (*bytes != source.bytes) {
    return std::unexpected(
        std::format("{}: the converted rows and images do not rebuild the zone",
                    source.path));
  }
  auto location =
      textures.empty()
          ? std::expected<Content::ZoneTextureLocation, std::string>{}
          : Content::ZoneTextureLocationOf(textures.front().id);
  if (!location) return std::unexpected(location.error());
  converted.image_directory = location->directory;
  for (ConvertedTexture& texture : textures) {
    for (ImageFile& file : texture.files) {
      converted.output.files.push_back(
          std::format("{}/{}", converted.image_directory, file.name));
      converted.image_files.push_back(std::move(file));
    }
  }
  converted.output.id = record.id;
  converted.output.source = source.path;
  converted.output.source_sha256 = HashOf(source);
  converted.output.converter = std::string(Content::kZndTableConverterName);
  return converted;
}

std::expected<ZoneConversionReport, std::string> ConvertDiscZones(
    const fs::path& root, bool force) {
  auto start = StartRun(root);
  if (!start) return std::unexpected(start.error());
  const auto version =
      start->previous.converters.find(Content::kZndTableConverterName);
  if (!force && version != start->previous.converters.end() &&
      version->second >= 1 &&
      version->second < Content::kZndTableConverterVersion) {
    auto upgraded = UpgradeZoneAuthoring(root);
    if (!upgraded) return std::unexpected(upgraded.error());
    start = StartRun(root);
    if (!start) return std::unexpected(start.error());
  }
  const ConversionManifest& previous = start->previous;
  ConverterBatch batch = StartBatch(previous, Content::kZndTableConverterName,
                                    Content::kZndTableConverterVersion, force);
  std::vector<SourceFile> sources;
  for (const DataStore::Entry& entry : start->entries) {
    if (entry.form != kRawForm ||
        !entry.path.ends_with(Content::kZndExtension)) {
      continue;
    }
    auto source = ReadSource(root, entry.path);
    if (!source) return std::unexpected(source.error());
    sources.push_back(std::move(*source));
  }
  ZoneConversionReport report;
  const bool tables_present =
      std::ranges::all_of(TableFiles(), [&root](const std::string& file) {
        return fs::exists(root / Platform::Utf8Path(file));
      });
  std::vector<ConversionOutput> kept;
  if (batch.reuse && tables_present &&
      AllUnchanged(previous, root, sources, kept)) {
    batch.outputs = std::move(kept);
    report.unchanged = batch.outputs.size();
    auto removed = PublishBatch(root, previous, batch, [] {
      return std::expected<void, std::string>{};
    });
    if (!removed) return std::unexpected(removed.error());
    report.removed = *removed;
    return report;
  }
  std::vector<ConvertedZone> zones;
  for (const SourceFile& source : sources) {
    auto zone = ConvertZone({.znd = source, .files = start->entries});
    if (!zone) return std::unexpected(zone.error());
    for (const Content::ZoneEnemyRecord& enemy : zone->record.enemies) {
      const ConversionOutput* archetype = previous.Find(enemy.archetype);
      if (archetype == nullptr ||
          archetype->converter != Content::kZudArchetypeConverterName) {
        return std::unexpected(std::format(
            "{}: {} loads {}, which the {} converter has not recorded (convert "
            "the models first)",
            source.path, enemy.id, enemy.archetype,
            Content::kZudArchetypeConverterName));
      }
    }
    zones.push_back(std::move(*zone));
  }
  std::vector<ZoneRecord> records;
  for (const ConvertedZone& zone : zones) records.push_back(zone.record);
  std::ranges::sort(records, {}, &ZoneRecord::id);
  const auto tables = Content::FormatZoneTables(records);
  auto read_back = Content::ReadZoneTables(
      [&tables](
          std::string_view path) -> std::expected<std::string, std::string> {
        for (const ZoneTable table : Content::kZoneTables) {
          if (Content::ZoneTablePath(table) == path) {
            return tables.at(static_cast<std::size_t>(table));
          }
        }
        return std::unexpected(std::format("no table {}", path));
      });
  if (!read_back) return std::unexpected(read_back.error());
  if (*read_back != records) {
    return std::unexpected("the zone tables do not read back to their zones");
  }
  const std::vector<std::string> table_files = TableFiles();
  for (ConvertedZone& zone : zones) {
    ConversionOutput output = zone.output;
    output.files.insert(output.files.end(), table_files.begin(),
                        table_files.end());
    std::ranges::sort(output.files);
    batch.outputs.push_back(std::move(output));
  }
  auto removed = PublishBatch(root, previous, batch,
                              [&] { return WriteZones(root, zones, tables); });
  if (!removed) return std::unexpected(removed.error());
  report.converted = zones.size();
  report.removed = *removed;
  return report;
}

}  // namespace VS::Extract
