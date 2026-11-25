// Converted zone loading (ConvertedZones.hpp).
#include "Content/ConvertedZones.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "Content/ActionCatalog.hpp"
#include "Content/ConversionManifest.hpp"
#include "Content/ConvertedImages.hpp"
#include "Content/DataStore.hpp"
#include "Content/ImageFiles.hpp"
#include "Content/NativeImage.hpp"
#include "Content/TimFile.hpp"
#include "Content/ZndFile.hpp"
#include "Content/ZoneTables.hpp"
#include "Core/Log.hpp"
#include "Platform/Paths.hpp"

namespace VS::Content {
namespace {

namespace fs = std::filesystem;

constexpr std::string_view kImageIdKind = "image";
constexpr std::string_view kZoneTextureDirectory = "textures/zone";

std::string Upper(std::string_view text) {
  std::string upper(text);
  std::ranges::transform(upper, upper.begin(), [](char value) {
    return value >= 'a' && value <= 'z' ? static_cast<char>(value - 'a' + 'A')
                                        : value;
  });
  return upper;
}

// The zone file's output from the znd-tables converter.
std::expected<ConvertedSource, std::string> ZoneSource(
    const DataStore& store, const DataStore::Entry& entry) {
  return FindConvertedSource(
      store, {.lba = entry.lba,
              .rounded_size = std::uint64_t{entry.SectorCount()} *
                              DataStore::kDataSectorBytes,
              .extension = kZndExtension,
              .converter = kZndTableConverterName});
}

std::expected<std::vector<std::byte>, std::string> TextureBytes(
    const DataStore& store, std::string_view image_id) {
  auto location = ZoneTextureLocationOf(image_id);
  if (!location) return std::unexpected(location.error());
  auto document = DecodeImageFiles(
      location->stem,
      DirectoryFileReader(store.Root() /
                          Platform::Utf8Path(location->directory)));
  if (!document) return std::unexpected(document.error());
  const DataStore::Entry* const zone = store.Find(location->source);
  if (zone == nullptr) {
    return std::unexpected(
        std::format("{}: its zone {} is not in the "
                    "extracted data",
                    image_id, location->source));
  }
  auto source = ZoneSource(store, *zone);
  if (!source) return std::unexpected(source.error());
  if (document->id != image_id ||
      document->provenance.source != location->source ||
      document->provenance.source_sha256 != source->conversion->source_sha256) {
    return std::unexpected(
        std::format("{} does not match its conversion manifest", image_id));
  }
  if (!document->provenance.tim) {
    return std::unexpected(
        std::format("{} records no source TIM to rebuild", image_id));
  }
  auto tim = TimFromImage(document->image, *document->provenance.tim);
  if (!tim)
    return std::unexpected(std::format("{}: {}", image_id, tim.error()));
  auto bytes = EncodeTim(*tim);
  if (!bytes) {
    return std::unexpected(std::format("{}: {}", image_id, bytes.error()));
  }
  return bytes;
}

std::expected<std::string, std::string> ReadText(const fs::path& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return std::unexpected(
        std::format("cannot read {}", Platform::PathText(path)));
  }
  return std::string(std::istreambuf_iterator<char>(file), {});
}

// A table file as it was when the tables were read.
struct FileStamp {
  fs::file_time_type written;
  std::uintmax_t size = 0;

  bool operator==(const FileStamp&) const = default;
};
// The six tables', then the action catalog's, which limbs name actions from.
using TableStamps = std::array<FileStamp, kZoneTableCount + 1>;

std::optional<TableStamps> StampsOf(const fs::path& root) {
  TableStamps stamps{};
  for (const ZoneTable table : kZoneTables) {
    const fs::path path = root / Platform::Utf8Path(ZoneTablePath(table));
    FileStamp& stamp = stamps.at(static_cast<std::size_t>(table));
    std::error_code error;
    stamp.written = fs::last_write_time(path, error);
    if (!error) stamp.size = fs::file_size(path, error);
    if (error) return std::nullopt;
  }
  // Tables whose limbs name no action read without a catalog, so a missing
  // one stamps as empty.
  const fs::path actions = root / Platform::Utf8Path(kActionsPath);
  FileStamp& stamp = stamps.back();
  std::error_code error;
  stamp.written = fs::last_write_time(actions, error);
  if (!error) stamp.size = fs::file_size(actions, error);
  if (error) stamp = {};
  return stamps;
}

// The zone tables last read, kept while their files are unchanged: a zone
// change reads them, and a later one usually finds them unchanged.
class ZoneTableCache {
 public:
  std::expected<std::shared_ptr<const std::vector<ZoneRecord>>, std::string>
  Load(const fs::path& root) {
    const std::optional<TableStamps> stamps = StampsOf(root);
    const std::scoped_lock lock(mutex_);
    if (stamps && zones_ && root == root_ && *stamps == stamps_) {
      return zones_;
    }
    auto zones = ReadZoneTables([&root](std::string_view path)
                                    -> std::expected<std::string, std::string> {
      return ReadText(root / Platform::Utf8Path(path));
    });
    if (!zones) return std::unexpected(zones.error());
    auto shared =
        std::make_shared<const std::vector<ZoneRecord>>(std::move(*zones));
    if (stamps) {
      root_ = root;
      stamps_ = *stamps;
      zones_ = shared;
    }
    return shared;
  }

 private:
  std::mutex mutex_;
  fs::path root_;
  TableStamps stamps_{};
  std::shared_ptr<const std::vector<ZoneRecord>> zones_;
};

ZoneTableCache& Tables() {
  static ZoneTableCache cache;
  return cache;
}

}  // namespace

std::string ZoneTextureId(std::string_view zone_id, std::size_t index) {
  const std::string_view rest = zone_id.starts_with(kZoneIdKind)
                                    ? zone_id.substr(kZoneIdKind.size())
                                    : zone_id;
  return std::format("{}{}.{:02}", kImageIdKind, rest, index);
}

std::expected<ZoneTextureLocation, std::string> ZoneTextureLocationOf(
    std::string_view image_id) {
  const std::size_t last = image_id.rfind('.');
  if (!image_id.starts_with(kImageIdKind) || last == std::string_view::npos ||
      last + 1 == image_id.size() ||
      !std::ranges::all_of(image_id.substr(last + 1), [](char value) {
        return value >= '0' && value <= '9';
      })) {
    return std::unexpected(
        std::format("'{}' is not a zone texture id", image_id));
  }
  const std::string zone_id = std::format(
      "{}{}", kZoneIdKind,
      image_id.substr(kImageIdKind.size(), last - kImageIdKind.size()));
  auto source = DiscPathOfFileId(kZoneIdKind, zone_id, kZndExtension);
  if (!source) {
    return std::unexpected(
        std::format("'{}' is not a zone texture id", image_id));
  }
  const std::string_view path = *source;
  const std::size_t slash = path.rfind('/');
  const std::string_view stem =
      path.substr(slash + 1, path.size() - slash - 1 - kZndExtension.size());
  return ZoneTextureLocation{
      .directory = std::format("{}/{}", kZoneTextureDirectory, Upper(stem)),
      .stem = std::string(image_id.substr(last + 1)),
      .source = *std::move(source)};
}

std::expected<ZndFile, std::string> ZndFileOf(const ZoneRecord& zone,
                                              const ZoneFileResolver& resolve) {
  ZndFile file;
  file.music_id = zone.music_id;
  for (std::size_t slot = 0; slot < zone.rooms.size(); ++slot) {
    const std::string& room = zone.rooms[slot];
    if (room.empty()) {
      file.rooms.push_back({});
      continue;
    }
    auto path = DiscPathOfFileId(kRoomIdKind, room, kRoomExtension);
    if (!path) {
      return std::unexpected(
          std::format("{} room slot {}: {}", zone.id, slot, path.error()));
    }
    auto entry = resolve.file(*path);
    if (!entry) {
      return std::unexpected(
          std::format("{} room slot {}: {}", zone.id, slot, entry.error()));
    }
    file.rooms.push_back(*entry);
  }
  for (const ZoneEnemyRecord& enemy : zone.enemies) {
    auto path = DiscPathOfFileId(kArchetypeIdKind, enemy.archetype, ".ZUD");
    if (!path)
      return std::unexpected(std::format("{}: {}", enemy.id, path.error()));
    auto entry = resolve.file(*path);
    if (!entry) {
      return std::unexpected(std::format("{}: {}", enemy.id, entry.error()));
    }
    file.enemies.push_back({.zud = *entry, .data = enemy.data});
  }
  if (!zone.textures.empty()) {
    std::vector<std::vector<std::byte>> textures;
    for (const std::string& image : zone.textures) {
      auto bytes = resolve.texture(image);
      if (!bytes) {
        return std::unexpected(std::format("{}: {}", zone.id, bytes.error()));
      }
      textures.push_back(*std::move(bytes));
    }
    file.textures = std::move(textures);
  }
  return file;
}

ZoneFileResolver StoreZoneResolver(const DataStore& store) {
  return {.file = [&store](std::string_view path)
              -> std::expected<ZndFileEntry, std::string> {
            const DataStore::Entry* const entry = store.Find(path);
            if (entry == nullptr) {
              return std::unexpected(
                  std::format("{} is not in the extracted data", path));
            }
            return ZndFileEntry{
                .sector = static_cast<std::int32_t>(entry->lba),
                .bytes = static_cast<std::int32_t>(
                    entry->SectorCount() * DataStore::kDataSectorBytes)};
          },
          .texture =
              [&store](std::string_view image_id) {
                return TextureBytes(store, image_id);
              }};
}

std::expected<std::vector<std::byte>, std::string> LoadConvertedZone(
    const DataStore& store, std::uint32_t lba) {
  const DataStore::Entry* const entry = store.FindBySector(lba);
  if (entry == nullptr || entry->lba != lba ||
      !entry->path.ends_with(kZndExtension)) {
    return std::unexpected(
        std::format("sector {} starts no extracted zone file", lba));
  }
  auto source = ZoneSource(store, *entry);
  if (!source) return std::unexpected(source.error());
  auto id = DiscFileId(kZoneIdKind, entry->path);
  if (!id) return std::unexpected(id.error());
  auto zones = Tables().Load(store.Root());
  if (!zones) return std::unexpected(zones.error());
  const auto zone = std::ranges::find((*zones)->begin(), (*zones)->end(), *id,
                                      &ZoneRecord::id);
  if (zone == (*zones)->end()) {
    return std::unexpected(std::format("{}: no row of {} names {}", entry->path,
                                       ZoneTablePath(ZoneTable::kZones), *id));
  }
  auto file = ZndFileOf(*zone, StoreZoneResolver(store));
  if (!file) return std::unexpected(file.error());
  auto bytes = EncodeZndFile(*file);
  if (!bytes) {
    return std::unexpected(std::format("{}: {}", entry->path, bytes.error()));
  }
  return bytes;
}

bool IsLiveConvertedZone(DiscFileExtent file) {
  const DataStore* const store = DataStore::Get();
  if (store == nullptr || file.sector <= 0) return false;
  const auto sector = static_cast<std::uint32_t>(file.sector);
  const DataStore::Entry* const entry = store->FindBySector(sector);
  return entry != nullptr && entry->lba == sector &&
         entry->path.ends_with(kZndExtension);
}

std::optional<bool> ReadLiveConvertedZone(DiscFileExtent file,
                                          std::span<std::byte> destination) {
  if (!IsLiveConvertedZone(file)) return std::nullopt;
  const DataStore* const store = DataStore::Get();
  const auto sector = static_cast<std::uint32_t>(file.sector);
  std::ranges::fill(destination, std::byte{0});
  auto bytes = LoadConvertedZone(*store, sector);
  if (!bytes) {
    VS::Core::LogError("content", "zone at LBA {}: {}", sector, bytes.error());
    return false;
  }
  if (bytes->size() > destination.size()) {
    VS::Core::LogError(
        "content",
        "zone at LBA {}: its {} rebuilt bytes exceed the {}-byte "
        "read",
        sector, bytes->size(), destination.size());
    return false;
  }
  std::ranges::copy(*bytes, destination.begin());
  VS::Core::LogDebug("content", "zone at LBA {}: {} converted bytes", sector,
                     bytes->size());
  return true;
}

}  // namespace VS::Content
