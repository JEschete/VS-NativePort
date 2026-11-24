// The asset catalog loader (AssetCatalog.hpp).
#include "Content/AssetCatalog.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <limits>
#include <numeric>
#include <optional>
#include <set>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "Content/JsonText.hpp"
#include "Platform/Paths.hpp"
#include "nlohmann/json.hpp"

namespace VS::Content {
namespace {
namespace fs = std::filesystem;
using Json = nlohmann::json;

// The largest file whose whole-sector load size still fits 32 bits.
constexpr std::uint64_t LargestAssetBytes() {
  return std::numeric_limits<std::uint32_t>::max() - (kAssetSectorBytes - 1);
}

bool IsIdCharacter(char value) {
  return (value >= 'a' && value <= 'z') || (value >= '0' && value <= '9') ||
         value == '_' || value == '-';
}

// One catalog file's error reporting: "catalogs/<file>: <pointer>: <rule>".
class Errors {
 public:
  explicit Errors(std::string_view file) : file_(file) {}

  [[nodiscard]] std::unexpected<std::string> Fail(std::string_view pointer,
                                                  std::string_view rule) const {
    return std::unexpected(std::format("{}/{}: {}: {}", kCatalogDirectory,
                                       file_, pointer.empty() ? "/" : pointer,
                                       rule));
  }

 private:
  std::string_view file_;
};

std::string Pointer(std::string_view parent, std::string_view key) {
  return std::format("{}/{}", parent, key);
}

std::string Pointer(std::string_view parent, std::size_t index) {
  return std::format("{}/{}", parent, index);
}

std::expected<Json, std::string> ParseText(std::string_view text,
                                           const Errors& errors) {
  std::vector<std::set<std::string>> keys;
  bool duplicate = false;
  const auto check_keys = [&keys, &duplicate](int, Json::parse_event_t event,
                                              Json& item) {
    if (event == Json::parse_event_t::object_start) keys.emplace_back();
    if (event == Json::parse_event_t::key && !keys.empty()) {
      duplicate =
          !keys.back().insert(item.get<std::string>()).second || duplicate;
    }
    if (event == Json::parse_event_t::object_end && !keys.empty()) {
      keys.pop_back();
    }
    return true;
  };
  Json value = Json::parse(text, check_keys, false);
  if (value.is_discarded()) return errors.Fail("", "not valid JSON");
  if (duplicate) return errors.Fail("", "duplicate JSON object key");
  return value;
}

// Checks `object` is an object holding exactly `keys`.
std::expected<void, std::string> RequireKeys(
    const Json& object, std::string_view pointer,
    std::span<const std::string_view> keys, const Errors& errors) {
  if (!object.is_object()) return errors.Fail(pointer, "must be an object");
  for (const auto& [key, value] : object.items()) {
    if (key == kDisplayName) {
      if (!value.is_string()) {
        return errors.Fail(Pointer(pointer, key), "must be a string");
      }
      continue;
    }
    if (std::ranges::find(keys, key) == keys.end()) {
      return errors.Fail(Pointer(pointer, key), "unknown key");
    }
  }
  for (const std::string_view key : keys) {
    if (!object.contains(key)) {
      return errors.Fail(Pointer(pointer, key), "missing key");
    }
  }
  return {};
}

std::expected<void, std::string> RequireSchema(const Json& root,
                                               std::string_view schema,
                                               int version,
                                               const Errors& errors,
                                               int minimum_version = 0) {
  const Json& name = root.at("schema");
  if (!name.is_string() || name.get<std::string>() != schema) {
    return errors.Fail("/schema", std::format("must be \"{}\"", schema));
  }
  const Json& found = root.at("schema_version");
  if (!found.is_number_integer()) {
    return errors.Fail("/schema_version", "must be an integer");
  }
  if (minimum_version == 0) minimum_version = version;
  if (found.get<std::int64_t>() < minimum_version ||
      found.get<std::int64_t>() > version) {
    return errors.Fail(
        "/schema_version",
        std::format("unsupported version {} (this build reads {})",
                    found.dump(), version));
  }
  return {};
}

std::expected<std::uint32_t, std::string> RequireU32(const Json& value,
                                                     std::string_view pointer,
                                                     const Errors& errors) {
  if (!value.is_number_unsigned() ||
      value.get<std::uint64_t>() > std::numeric_limits<std::uint32_t>::max()) {
    return errors.Fail(pointer, "must be an unsigned 32-bit integer");
  }
  return static_cast<std::uint32_t>(value.get<std::uint64_t>());
}

std::expected<std::string, std::string> RequireString(const Json& value,
                                                      std::string_view pointer,
                                                      const Errors& errors) {
  if (!value.is_string() || value.get_ref<const std::string&>().empty()) {
    return errors.Fail(pointer, "must be a non-empty string");
  }
  return value.get<std::string>();
}

// A '/'-separated path below the data root: no drive, root, backslash, empty
// segment, "." or "..".
bool IsContainedRelativePath(std::string_view path) {
  if (path.empty() || path.find('\\') != std::string_view::npos ||
      path.find(':') != std::string_view::npos) {
    return false;
  }
  std::size_t start = 0;
  while (start <= path.size()) {
    const std::size_t end = std::min(path.find('/', start), path.size());
    const std::string_view segment = path.substr(start, end - start);
    if (segment.empty() || segment == "." || segment == "..") return false;
    start = end + 1;
  }
  return true;
}

std::expected<AssetRecord, std::string> ParseRecord(const Json& entry,
                                                    std::string_view pointer,
                                                    const fs::path& root,
                                                    const Errors& errors) {
  static constexpr std::array<std::string_view, 4> kRecordKeys = {
      "id", "file", "bytes", "provenance"};
  static constexpr std::array<std::string_view, 2> kProvenanceKeys = {
      "disc_path", "lba"};
  if (auto keys = RequireKeys(entry, pointer, kRecordKeys, errors); !keys) {
    return std::unexpected(keys.error());
  }
  AssetRecord record;
  auto id = RequireString(entry.at("id"), Pointer(pointer, "id"), errors);
  if (!id) return std::unexpected(id.error());
  if (!IsValidAssetId(*id)) {
    return errors.Fail(Pointer(pointer, "id"),
                       std::format("invalid asset id \"{}\"", *id));
  }
  record.id = std::move(*id);
  auto file = RequireString(entry.at("file"), Pointer(pointer, "file"), errors);
  if (!file) return std::unexpected(file.error());
  if (!IsContainedRelativePath(*file)) {
    return errors.Fail(
        Pointer(pointer, "file"),
        std::format("\"{}\" is not a '/'-separated path inside the data root",
                    *file));
  }
  record.file = std::move(*file);
  const auto bytes =
      RequireU32(entry.at("bytes"), Pointer(pointer, "bytes"), errors);
  if (!bytes) return std::unexpected(bytes.error());
  if (*bytes > LargestAssetBytes()) {
    return errors.Fail(Pointer(pointer, "bytes"),
                       "exceeds the largest loadable asset");
  }
  record.bytes = *bytes;
  const std::string provenance = Pointer(pointer, "provenance");
  const Json& source = entry.at("provenance");
  if (auto keys = RequireKeys(source, provenance, kProvenanceKeys, errors);
      !keys) {
    return std::unexpected(keys.error());
  }
  auto disc_path = RequireString(source.at("disc_path"),
                                 Pointer(provenance, "disc_path"), errors);
  if (!disc_path) return std::unexpected(disc_path.error());
  record.provenance.disc_path = std::move(*disc_path);
  const auto lba =
      RequireU32(source.at("lba"), Pointer(provenance, "lba"), errors);
  if (!lba) return std::unexpected(lba.error());
  record.provenance.lba = *lba;

  const fs::path path = root / Platform::Utf8Path(record.file);
  std::error_code error;
  const std::uintmax_t size = fs::file_size(path, error);
  if (error) {
    return errors.Fail(Pointer(pointer, "file"),
                       std::format("{} is missing or unreadable ({})",
                                   Platform::PathText(path), error.message()));
  }
  if (size != record.bytes) {
    return errors.Fail(
        Pointer(pointer, "bytes"),
        std::format("{} holds {} bytes, the catalog says {}",
                    Platform::PathText(path), size, record.bytes));
  }
  return record;
}

std::expected<std::string, std::string> ReadText(const fs::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    return std::unexpected("cannot read " + Platform::PathText(path) +
                           " (run VS.Extract extract <disc> <dir>, or "
                           "VS.Extract catalogs <dir>)");
  }
  std::ostringstream text;
  text << input.rdbuf();
  return std::move(text).str();
}

struct ParsedAssets {
  std::vector<AssetRecord> records;
  std::vector<std::pair<std::string, std::string>> aliases;
};

std::expected<ParsedAssets, std::string> ParseAssetRecords(
    std::string_view assets_json, const fs::path& root, const Errors& errors) {
  auto assets = ParseText(assets_json, errors);
  if (!assets) return std::unexpected(assets.error());
  Json aliases = Json::object();
  const bool has_aliases = assets->is_object() && assets->contains("aliases");
  if (has_aliases) {
    aliases = assets->at("aliases");
    assets->erase("aliases");
  }
  static constexpr std::array<std::string_view, 3> kAssetKeys = {
      "schema", "schema_version", "assets"};
  if (auto keys = RequireKeys(*assets, "", kAssetKeys, errors); !keys) {
    return std::unexpected(keys.error());
  }
  if (auto schema = RequireSchema(*assets, kAssetCatalogSchema,
                                  kAssetCatalogSchemaVersion, errors,
                                  kOldestAssetCatalogSchemaVersion);
      !schema) {
    return std::unexpected(schema.error());
  }
  const Json& entries = assets->at("assets");
  if (!entries.is_array()) return errors.Fail("/assets", "must be an array");
  if (entries.size() > std::numeric_limits<std::uint32_t>::max()) {
    return errors.Fail("/assets", "too many assets");
  }
  ParsedAssets parsed;
  parsed.records.reserve(entries.size());
  for (std::size_t index = 0; index < entries.size(); ++index) {
    auto record =
        ParseRecord(entries[index], Pointer("/assets", index), root, errors);
    if (!record) return std::unexpected(record.error());
    parsed.records.push_back(std::move(*record));
  }
  if (!aliases.is_object()) return errors.Fail("/aliases", "must be an object");
  if (has_aliases &&
      assets->at("schema_version") == kOldestAssetCatalogSchemaVersion) {
    return errors.Fail("/aliases",
                       "compatibility aliases require asset schema version 2");
  }
  for (const auto& [id, target] : aliases.items()) {
    if (!IsValidAssetId(id) || !target.is_string() ||
        !IsValidAssetId(target.get_ref<const std::string&>())) {
      return errors.Fail(Pointer("/aliases", id),
                         "must map a valid asset ID to a canonical asset ID");
    }
    parsed.aliases.emplace_back(id, target.get<std::string>());
  }
  return parsed;
}

// Record indexes ordered by id. A repeated id is reported at its later entry.
std::expected<std::vector<std::uint32_t>, std::string> SortById(
    const std::vector<AssetRecord>& records, const Errors& errors) {
  std::vector<std::uint32_t> by_id(records.size());
  std::iota(by_id.begin(), by_id.end(), 0U);
  std::ranges::sort(by_id, {}, [&records](std::uint32_t index) {
    return std::string_view(records[index].id);
  });
  for (std::size_t index = 1; index < by_id.size(); ++index) {
    const AssetRecord& previous = records[by_id[index - 1]];
    if (previous.id == records[by_id[index]].id) {
      const std::uint32_t duplicate = std::max(by_id[index - 1], by_id[index]);
      return errors.Fail(Pointer(Pointer("/assets", duplicate), "id"),
                         std::format("duplicate asset id \"{}\"", previous.id));
    }
  }
  return by_id;
}

std::expected<AssetHandle, std::string> ResolveAsset(
    const AssetCatalog& catalog, const Json& value, std::string_view pointer,
    const Errors& errors) {
  if (!value.is_string()) return errors.Fail(pointer, "must be an asset id");
  const auto handle = catalog.Find(value.get_ref<const std::string&>());
  if (!handle) {
    return errors.Fail(pointer,
                       std::format("unresolved asset \"{}\"",
                                   value.get_ref<const std::string&>()));
  }
  return *handle;
}

// One table of `out.size()` {"legacyId", "asset"} entries in legacy id order.
std::expected<void, std::string> ParseAudioTable(const AssetCatalog& catalog,
                                                 const Json& audio,
                                                 std::string_view key,
                                                 std::span<AssetHandle> out,
                                                 const Errors& errors) {
  static constexpr std::array<std::string_view, 2> kEntryKeys = {"legacyId",
                                                                 "asset"};
  const std::string pointer = Pointer("", key);
  const Json& entries = audio.at(key);
  if (!entries.is_array() || entries.size() != out.size()) {
    return errors.Fail(
        pointer, std::format("must be an array of {} entries", out.size()));
  }
  for (std::size_t index = 0; index < out.size(); ++index) {
    const std::string at = Pointer(pointer, index);
    const Json& entry = entries[index];
    if (auto keys = RequireKeys(entry, at, kEntryKeys, errors); !keys) {
      return keys;
    }
    const Json& legacy = entry.at("legacyId");
    if (!legacy.is_number_unsigned() || legacy.get<std::uint64_t>() != index) {
      return errors.Fail(Pointer(at, "legacyId"),
                         std::format("must be {}", index));
    }
    auto handle =
        ResolveAsset(catalog, entry.at("asset"), Pointer(at, "asset"), errors);
    if (!handle) return std::unexpected(handle.error());
    out[index] = *handle;
  }
  return {};
}

std::expected<MenuSoundAssets, std::string> ParseMenuSounds(
    const AssetCatalog& catalog, const Json& menu, const Errors& errors) {
  static constexpr std::array<std::string_view, 3> kMenuKeys = {
      "sample_banks", "effect_sample_bank", "effect_programs"};
  if (auto keys = RequireKeys(menu, "/menu", kMenuKeys, errors); !keys) {
    return std::unexpected(keys.error());
  }
  MenuSoundAssets assets;
  const Json& samples = menu.at("sample_banks");
  if (!samples.is_array() || samples.size() != assets.sample_banks.size()) {
    return errors.Fail("/menu/sample_banks",
                       std::format("must be an array of {} asset ids",
                                   assets.sample_banks.size()));
  }
  for (std::size_t index = 0; index < assets.sample_banks.size(); ++index) {
    auto handle = ResolveAsset(catalog, samples[index],
                               Pointer("/menu/sample_banks", index), errors);
    if (!handle) return std::unexpected(handle.error());
    assets.sample_banks.at(index) = *handle;
  }
  auto effect_bank = ResolveAsset(catalog, menu.at("effect_sample_bank"),
                                  "/menu/effect_sample_bank", errors);
  if (!effect_bank) return std::unexpected(effect_bank.error());
  assets.effect_sample_bank = *effect_bank;
  auto programs = ResolveAsset(catalog, menu.at("effect_programs"),
                               "/menu/effect_programs", errors);
  if (!programs) return std::unexpected(programs.error());
  assets.effect_programs = *programs;
  return assets;
}

// audio.json, resolved against the catalog's records.
std::expected<AudioCatalog, std::string> ParseAudioCatalog(
    std::string_view audio_json, const AssetCatalog& catalog) {
  const Errors errors(kAudioCatalogFile);
  auto audio = ParseText(audio_json, errors);
  if (!audio) return std::unexpected(audio.error());
  static constexpr std::array<std::string_view, 6> kAudioKeys = {
      "schema", "schema_version", "music", "sfx_banks", "wave_banks", "menu"};
  if (auto keys = RequireKeys(*audio, "", kAudioKeys, errors); !keys) {
    return std::unexpected(keys.error());
  }
  if (auto schema = RequireSchema(*audio, kAudioCatalogSchema,
                                  kAudioCatalogSchemaVersion, errors);
      !schema) {
    return std::unexpected(schema.error());
  }
  AudioCatalog tables;
  if (auto music =
          ParseAudioTable(catalog, *audio, "music", tables.music, errors);
      !music) {
    return std::unexpected(music.error());
  }
  if (auto sfx = ParseAudioTable(catalog, *audio, "sfx_banks", tables.sfx_banks,
                                 errors);
      !sfx) {
    return std::unexpected(sfx.error());
  }
  if (auto wave = ParseAudioTable(catalog, *audio, "wave_banks",
                                  tables.wave_banks, errors);
      !wave) {
    return std::unexpected(wave.error());
  }
  auto menu = ParseMenuSounds(catalog, audio->at("menu"), errors);
  if (!menu) return std::unexpected(menu.error());
  tables.menu = *menu;
  return tables;
}
}  // namespace

bool IsValidAssetId(std::string_view id) noexcept {
  constexpr std::string_view kPrefix = "asset.";
  if (!id.starts_with(kPrefix) || id.size() == kPrefix.size()) return false;
  bool segment_empty = true;
  for (const char value : id.substr(kPrefix.size())) {
    if (value == '.') {
      if (segment_empty) return false;
      segment_empty = true;
    } else if (IsIdCharacter(value)) {
      segment_empty = false;
    } else {
      return false;
    }
  }
  return !segment_empty;
}

std::optional<std::string> AssetIdForDiscPath(std::string_view disc_path) {
  std::string id = "asset.";
  for (const char value : disc_path) {
    if (value == '/') {
      id += '.';
    } else if (value >= 'A' && value <= 'Z') {
      id += static_cast<char>(value - 'A' + 'a');
    } else {
      id += value;
    }
  }
  if (!IsValidAssetId(id)) return std::nullopt;
  return id;
}

std::uint32_t AssetRecord::LoadBytes() const noexcept {
  const std::uint64_t sectors = std::max<std::uint64_t>(
      (static_cast<std::uint64_t>(bytes) + kAssetSectorBytes - 1) /
          kAssetSectorBytes,
      1);
  return static_cast<std::uint32_t>(sectors * kAssetSectorBytes);
}

std::expected<AssetCatalog, std::string> AssetCatalog::Parse(
    std::string_view assets_json, std::string_view audio_json,
    const fs::path& root, std::uint32_t generation) {
  if (generation == 0) {
    return std::unexpected("asset catalog refused: generation 0 is no catalog");
  }
  AssetCatalog catalog;
  catalog.root_ = root;
  catalog.generation_ = generation;

  const Errors asset_errors(kAssetCatalogFile);
  auto records = ParseAssetRecords(assets_json, root, asset_errors);
  if (!records) return std::unexpected(records.error());
  catalog.records_ = std::move(records->records);
  auto by_id = SortById(catalog.records_, asset_errors);
  if (!by_id) return std::unexpected(by_id.error());
  catalog.by_id_ = std::move(*by_id);
  std::vector<std::pair<std::string, std::uint32_t>> aliases;
  for (const auto& [id, target] : records->aliases) {
    if (catalog.Find(id)) {
      return asset_errors.Fail(Pointer("/aliases", id),
                               "alias shadows a canonical asset ID");
    }
    const auto handle = catalog.Find(target);
    if (!handle) {
      return asset_errors.Fail(Pointer("/aliases", id),
                               "alias target must be a canonical asset ID");
    }
    aliases.emplace_back(id, handle->index);
  }
  std::ranges::sort(aliases);
  catalog.aliases_ = std::move(aliases);

  auto audio = ParseAudioCatalog(audio_json, catalog);
  if (!audio) return std::unexpected(audio.error());
  catalog.audio_ = *audio;
  return catalog;
}

std::expected<AssetCatalog, std::string> AssetCatalog::Load(
    const fs::path& root, std::uint32_t generation) {
  const fs::path directory = root / Platform::Utf8Path(kCatalogDirectory);
  auto assets = ReadText(directory / Platform::Utf8Path(kAssetCatalogFile));
  if (!assets) return std::unexpected(assets.error());
  auto audio = ReadText(directory / Platform::Utf8Path(kAudioCatalogFile));
  if (!audio) return std::unexpected(audio.error());
  return Parse(*assets, *audio, root, generation);
}

std::optional<AssetHandle> AssetCatalog::Find(std::string_view id) const {
  const auto found =
      std::ranges::lower_bound(by_id_, id, {}, [this](std::uint32_t index) {
        return std::string_view(records_[index].id);
      });
  if (found != by_id_.end() && records_[*found].id == id) {
    return AssetHandle{.index = *found, .generation = generation_};
  }
  const auto alias = std::ranges::lower_bound(
      aliases_, id, {},
      [](const auto& entry) { return std::string_view(entry.first); });
  if (alias == aliases_.end() || alias->first != id) return std::nullopt;
  return AssetHandle{.index = alias->second, .generation = generation_};
}

const AssetRecord* AssetCatalog::Record(AssetHandle handle) const {
  if (!handle.Valid() || handle.generation != generation_ ||
      handle.index >= records_.size()) {
    return nullptr;
  }
  return &records_[handle.index];
}

fs::path AssetCatalog::FileOf(const AssetRecord& record) const {
  return root_ / Platform::Utf8Path(record.file);
}

}  // namespace VS::Content
