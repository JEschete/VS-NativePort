// The asset catalog importer (AssetCatalogImport.hpp).
#include "Content/AssetCatalogImport.hpp"

#include <algorithm>
#include <array>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "Content/AssetCatalog.hpp"
#include "Content/DataStore.hpp"
#include "Platform/Paths.hpp"
#include "nlohmann/json.hpp"

namespace VS::Content {
namespace {
namespace fs = std::filesystem;
using Json = nlohmann::ordered_json;

// The PS-X EXE header: magic, load address at 0x18, text size at 0x1C; the
// text follows the 2048-byte header.
constexpr std::string_view kExecutableMagic = "PS-X EXE";
constexpr std::size_t kExecutableLoadAddressOffset = 0x18;
constexpr std::size_t kExecutableTextSizeOffset = 0x1C;
constexpr std::size_t kExecutableHeaderBytes = 0x800;

struct AudioTable {
  std::string_view key;
  std::string_view decomp;
  std::uint32_t sectors;
  std::uint32_t sizes;
  std::size_t count;
  std::string_view directory;
};

// decomp: SLUS_010.40 _musicLBAs/_musicFileSizes, _seLBAs/_seFileSizes,
// _soundLBAs/_soundFileSizes (src/SLUS_010.40/main.c data, 0x8004A530 to
// 0x8004B1AF).
constexpr std::array<AudioTable, 3> kAudioTables = {{
    {.key = "music",
     .decomp = "_musicLBAs/_musicFileSizes",
     .sectors = 0x8004A6C0,
     .sizes = 0x8004AD00,
     .count = kMusicAssetCount,
     .directory = "MUSIC/"},
    {.key = "sfx_banks",
     .decomp = "_seLBAs/_seFileSizes",
     .sectors = 0x8004A9E0,
     .sizes = 0x8004B020,
     .count = kSfxBankAssetCount,
     .directory = "SE/"},
    {.key = "wave_banks",
     .decomp = "_soundLBAs/_soundFileSizes",
     .sectors = 0x8004A530,
     .sizes = 0x8004AB70,
     .count = kWaveBankAssetCount,
     .directory = "SOUND/"},
}};

// decomp: SLUS_010.40 _loadMenuSound (src/SLUS_010.40/main.c:10452) reads
// these files by their disc layout constants.
constexpr std::array<std::string_view, 2> kMenuSampleBanks = {
    "SOUND/WAVE0000.DAT",
    "SOUND/WAVE0005.DAT",
};
constexpr std::string_view kMenuEffectSampleBank = "SOUND/WAVE0200.DAT";
constexpr std::string_view kMenuEffectPrograms = "SE/EFFECT00.DAT";

constexpr std::size_t kWordBytes = sizeof(std::uint32_t);

std::uint32_t Word(std::span<const std::uint8_t> bytes, std::size_t offset) {
  std::uint32_t value = 0;
  for (std::size_t index = kWordBytes; index-- > 0;) {
    value = (value << static_cast<unsigned>(CHAR_BIT)) | bytes[offset + index];
  }
  return value;
}

// The executable's loaded image, addressed as the PS1 saw it.
class ExecutableImage {
 public:
  static std::expected<ExecutableImage, std::string> Open(
      std::span<const std::uint8_t> bytes) {
    if (bytes.size() < kExecutableHeaderBytes ||
        !std::equal(kExecutableMagic.begin(), kExecutableMagic.end(),
                    bytes.begin(), [](char expected, std::uint8_t actual) {
                      return static_cast<std::uint8_t>(expected) == actual;
                    })) {
      return std::unexpected(
          std::format("{} is not a PS-X EXE", kOriginalExecutablePath));
    }
    ExecutableImage image;
    image.base_ = Word(bytes, kExecutableLoadAddressOffset);
    const std::uint32_t text = Word(bytes, kExecutableTextSizeOffset);
    if (bytes.size() - kExecutableHeaderBytes < text) {
      return std::unexpected(std::format(
          "{} is truncated: {} text bytes, {} present", kOriginalExecutablePath,
          text, bytes.size() - kExecutableHeaderBytes));
    }
    image.text_ = bytes.subspan(kExecutableHeaderBytes, text);
    return image;
  }

  std::expected<std::uint32_t, std::string> WordAt(
      std::uint32_t address) const {
    if (address < base_ || text_.size() < kWordBytes ||
        address - base_ > text_.size() - kWordBytes ||
        address % kWordBytes != 0) {
      return std::unexpected(
          std::format("address {:#010x} is outside {}'s image", address,
                      kOriginalExecutablePath));
    }
    return Word(text_, address - base_);
  }

 private:
  std::uint32_t base_ = 0;
  std::span<const std::uint8_t> text_;
};

std::uint32_t WholeSectorBytes(std::uint32_t bytes) {
  AssetRecord record;
  record.bytes = bytes;
  return record.LoadBytes();
}

class Importer {
 public:
  explicit Importer(std::span<const DataStore::Entry> entries)
      : entries_(entries) {}

  std::expected<Json, std::string> Assets() const {
    Json assets = Json::array();
    for (const DataStore::Entry& entry : entries_) {
      if (entry.form != "raw") continue;
      const auto id = AssetIdForDiscPath(entry.path);
      if (!id) {
        return std::unexpected(std::format(
            "{} cannot be named as an asset (ids use [A-Za-z0-9_-] segments)",
            entry.path));
      }
      assets.push_back(
          {{"id", *id},
           {"file", "disc/" + entry.path},
           {"bytes", entry.size},
           {"provenance", {{"disc_path", entry.path}, {"lba", entry.lba}}}});
    }
    return assets;
  }

  std::expected<std::string, std::string> AssetNamed(
      std::string_view path) const {
    const auto found =
        std::ranges::find_if(entries_, [path](const DataStore::Entry& entry) {
          return entry.path == path && entry.form == "raw";
        });
    if (found == entries_.end()) {
      return std::unexpected(std::format("{} was not extracted", path));
    }
    return AssetIdOf(*found);
  }

  std::expected<Json, std::string> Table(const ExecutableImage& image,
                                         const AudioTable& table) const {
    Json entries = Json::array();
    for (std::size_t index = 0; index < table.count; ++index) {
      const auto offset = static_cast<std::uint32_t>(index * 4);
      const auto sector = image.WordAt(table.sectors + offset);
      if (!sector) return std::unexpected(sector.error());
      const auto bytes = image.WordAt(table.sizes + offset);
      if (!bytes) return std::unexpected(bytes.error());
      const auto entry = EntryAt(*sector);
      if (entry == nullptr) {
        return std::unexpected(
            std::format("{}[{}]: no extracted file starts at sector {}",
                        table.decomp, index, *sector));
      }
      if (!entry->path.starts_with(table.directory)) {
        return std::unexpected(std::format("{}[{}]: {} is not under {}",
                                           table.decomp, index, entry->path,
                                           table.directory));
      }
      if (WholeSectorBytes(entry->size) != *bytes) {
        return std::unexpected(std::format(
            "{}[{}]: the table loads {} bytes, {} holds {} ({} whole-sector)",
            table.decomp, index, *bytes, entry->path, entry->size,
            WholeSectorBytes(entry->size)));
      }
      auto id = AssetIdOf(*entry);
      if (!id) return std::unexpected(id.error());
      entries.push_back({{"legacyId", index}, {"asset", *id}});
    }
    return entries;
  }

 private:
  const DataStore::Entry* EntryAt(std::uint32_t sector) const {
    const auto found =
        std::ranges::find_if(entries_, [sector](const DataStore::Entry& entry) {
          return entry.lba == sector && entry.form == "raw";
        });
    return found == entries_.end() ? nullptr : &*found;
  }

  static std::expected<std::string, std::string> AssetIdOf(
      const DataStore::Entry& entry) {
    auto id = AssetIdForDiscPath(entry.path);
    if (!id) {
      return std::unexpected(
          std::format("{} cannot be named as an asset", entry.path));
    }
    return *id;
  }

  std::span<const DataStore::Entry> entries_;
};

// Top-level keys one per line, arrays one entry per line, so a re-import
// diffs by entry.
std::string Lines(const Json& object) {
  std::string text = "{\n";
  bool first = true;
  for (const auto& [name, value] : object.items()) {
    text += first ? "  " : ",\n  ";
    first = false;
    text += Json(name).dump() + ": ";
    if (!value.is_array()) {
      text += value.dump();
      continue;
    }
    text += "[\n";
    for (std::size_t index = 0; index < value.size(); ++index) {
      text += "    " + value[index].dump();
      text += index + 1 == value.size() ? "\n" : ",\n";
    }
    text += "  ]";
  }
  return text + "\n}\n";
}

std::expected<void, std::string> ReplaceFile(const fs::path& path,
                                             std::string_view text) {
  fs::path temporary = path;
  temporary += ".tmp";
  {
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    output.write(text.data(), static_cast<std::streamsize>(text.size()));
    output.close();
    if (!output) {
      return std::unexpected("cannot write " + Platform::PathText(temporary));
    }
  }
  std::error_code error;
  fs::rename(temporary, path, error);
  if (error) {
    return std::unexpected("cannot replace " + Platform::PathText(path) + ": " +
                           error.message());
  }
  return {};
}
}  // namespace

std::expected<AssetCatalogTexts, std::string> ImportAssetCatalogs(
    std::span<const DataStore::Entry> entries,
    std::span<const std::uint8_t> executable) {
  const auto image = ExecutableImage::Open(executable);
  if (!image) return std::unexpected(image.error());
  const Importer importer(entries);
  auto assets = importer.Assets();
  if (!assets) return std::unexpected(assets.error());

  Json audio = {{"schema", std::string(kAudioCatalogSchema)},
                {"schema_version", kAudioCatalogSchemaVersion}};
  for (const AudioTable& table : kAudioTables) {
    auto rows = importer.Table(*image, table);
    if (!rows) return std::unexpected(rows.error());
    audio[std::string(table.key)] = std::move(*rows);
  }
  Json menu = Json::object();
  Json samples = Json::array();
  for (const std::string_view path : kMenuSampleBanks) {
    auto id = importer.AssetNamed(path);
    if (!id) return std::unexpected(id.error());
    samples.push_back(*id);
  }
  menu["sample_banks"] = std::move(samples);
  auto effect_bank = importer.AssetNamed(kMenuEffectSampleBank);
  if (!effect_bank) return std::unexpected(effect_bank.error());
  menu["effect_sample_bank"] = *effect_bank;
  auto programs = importer.AssetNamed(kMenuEffectPrograms);
  if (!programs) return std::unexpected(programs.error());
  menu["effect_programs"] = *programs;
  audio["menu"] = std::move(menu);

  Json catalog = {{"schema", std::string(kAssetCatalogSchema)},
                  {"schema_version", kAssetCatalogSchemaVersion},
                  {"assets", std::move(*assets)}};
  return AssetCatalogTexts{.assets = Lines(catalog), .audio = Lines(audio)};
}

std::expected<std::size_t, std::string> WriteAssetCatalogs(
    const fs::path& root) {
  const auto entries = DataStore::LoadManifest(root);
  if (!entries) return std::unexpected(entries.error());
  const auto executable = std::ranges::find(*entries, kOriginalExecutablePath,
                                            &DataStore::Entry::path);
  if (executable == entries->end() || executable->form != "raw") {
    return std::unexpected(std::format("{} was not extracted to {}",
                                       kOriginalExecutablePath,
                                       Platform::PathText(root)));
  }
  const fs::path executable_path =
      root / "disc" / Platform::Utf8Path(kOriginalExecutablePath);
  std::ifstream input(executable_path, std::ios::binary);
  const std::vector<std::uint8_t> image{std::istreambuf_iterator<char>(input),
                                        std::istreambuf_iterator<char>()};
  if (!input.eof() && !input) {
    return std::unexpected("cannot read " +
                           Platform::PathText(executable_path));
  }
  auto texts = ImportAssetCatalogs(*entries, image);
  if (!texts) return std::unexpected(texts.error());
  // The importer's output must load before it replaces anything.
  const auto checked =
      AssetCatalog::Parse(texts->assets, texts->audio, root, 1);
  if (!checked) {
    return std::unexpected("imported catalogs do not load: " + checked.error());
  }
  const fs::path directory = root / Platform::Utf8Path(kCatalogDirectory);
  std::error_code error;
  fs::create_directories(directory, error);
  if (error) {
    return std::unexpected("cannot create " + Platform::PathText(directory) +
                           ": " + error.message());
  }
  if (auto written = ReplaceFile(
          directory / Platform::Utf8Path(kAssetCatalogFile), texts->assets);
      !written) {
    return std::unexpected(written.error());
  }
  if (auto written = ReplaceFile(
          directory / Platform::Utf8Path(kAudioCatalogFile), texts->audio);
      !written) {
    return std::unexpected(written.error());
  }
  return checked->Size();
}

}  // namespace VS::Content
