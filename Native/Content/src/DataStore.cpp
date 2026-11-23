#include "Content/DataStore.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstring>
#include <expected>
#include <format>
#include <fstream>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "Content/AssetCatalog.hpp"
#include "Content/ConversionManifest.hpp"
#include "Content/DataAccess.hpp"
#include "Content/WorkshopRecipes.hpp"
#include "Core/Log.hpp"
#include "Core/Sha256.hpp"
#include "Platform/CrashHandler.hpp"
#include "Platform/Paths.hpp"

namespace fs = std::filesystem;

namespace VS::Content {

namespace {

std::unique_ptr<DataStore>& Instance() {
  static std::unique_ptr<DataStore> store;
  return store;
}

// The last catalog generation issued; handles from earlier catalogs no longer
// resolve.
u32 catalogGeneration = 0;

bool ParseU32(std::string_view s, u32& out) {
  const auto r = std::from_chars(s.data(), s.data() + s.size(), out);
  return r.ec == std::errc() && r.ptr == s.data() + s.size();
}

// A disc file type the runtime reads only through a converter's outputs.
struct ConvertedType {
  std::string_view extension;
  std::string_view converter;
};

constexpr std::array kConvertedTypes{
    ConvertedType{.extension = ".TIM", .converter = "tim-png"},
    ConvertedType{.extension = ".FBC", .converter = "fbc-palette-png"},
    ConvertedType{.extension = ".FBT", .converter = "fbt-texture-png"},
    ConvertedType{.extension = ".SYD", .converter = kSydRecipeConverterName},
    ConvertedType{.extension = ".SHP", .converter = "shp-gltf"},
    ConvertedType{.extension = ".SEQ", .converter = "seq-gltf"},
    ConvertedType{.extension = ".WEP", .converter = "wep-gltf"},
};

// The fix each stale or missing output error names.
constexpr std::string_view kRerun = " (run VS.Extract extract <disc> <dir>)";

// The SHA-256 of each extracted file one check has read: many outputs share
// a dependency such as EFFECT/EFFPURGE.BIN.
using SourceHashes = std::unordered_map<std::string, std::string>;

// Whether `entry`'s bytes still hash to `sha256`.
std::expected<bool, std::string> HashMatches(const DataStore& store,
                                             SourceHashes& hashes,
                                             const DataStore::Entry& entry,
                                             std::string_view sha256) {
  auto found = hashes.find(entry.path);
  if (found == hashes.end()) {
    const auto bytes = store.ReadAll(entry);
    if (!bytes) return std::unexpected(bytes.error());
    found = hashes
                .emplace(entry.path,
                         Core::Sha256Hex(std::as_bytes(std::span(*bytes))))
                .first;
  }
  return found->second == sha256;
}

// Whether every file `conversion` lists exists under the data root.
std::expected<void, std::string> RequireOutputFiles(
    const DataStore& store, const ConversionOutput& conversion) {
  for (const std::string& file : conversion.files) {
    const fs::path path = store.Root() / Platform::Utf8Path(file);
    std::error_code error;
    const bool regular = fs::is_regular_file(path, error);
    if (error && error != std::errc::no_such_file_or_directory) {
      return std::unexpected("cannot inspect " + Platform::PathText(path) +
                             ": " + error.message());
    }
    if (!regular) {
      return std::unexpected(std::format("missing {} output {}{}",
                                         conversion.converter,
                                         Platform::PathText(path), kRerun));
    }
  }
  return {};
}

// Whether the output `type`'s converter made from `entry` is current with its
// source and dependencies, and the output holding its files has every file.
std::expected<void, std::string> RequireConvertedSource(
    const DataStore& store, const ConversionManifest& manifest,
    SourceHashes& hashes, const DataStore::Entry& entry,
    const ConversionOutput& conversion) {
  const auto current =
      HashMatches(store, hashes, entry, conversion.source_sha256);
  if (!current) return std::unexpected(current.error());
  if (!*current) {
    return std::unexpected(std::format("stale {} source hash for {}{}",
                                       conversion.converter, entry.path,
                                       kRerun));
  }
  for (const SourceDependency& dependency : conversion.dependencies) {
    const DataStore::Entry* dependency_entry = store.Find(dependency.source);
    if (dependency_entry == nullptr) {
      return std::unexpected(std::format("missing {} dependency {} for {}",
                                         conversion.converter,
                                         dependency.source, entry.path));
    }
    const auto matches =
        HashMatches(store, hashes, *dependency_entry, dependency.source_sha256);
    if (!matches) return std::unexpected(matches.error());
    if (!*matches) {
      return std::unexpected(
          std::format("stale {} dependency {} for {}{}", conversion.converter,
                      dependency.source, entry.path, kRerun));
    }
  }
  const ConversionOutput* const files = manifest.FilesOf(conversion);
  if (files == nullptr) {
    return std::unexpected(std::format("{}: alias target {} is not registered",
                                       entry.path, conversion.alias_of));
  }
  return RequireOutputFiles(store, *files);
}

using OutputMap = std::unordered_map<std::string_view, const ConversionOutput*>;

// Each output of `type`'s converter by source path, checking that it names a
// raw file of `type` and that no source has two outputs.
std::expected<OutputMap, std::string> OutputsBySource(
    const DataStore& store, const ConversionManifest& manifest,
    const ConvertedType& type) {
  OutputMap outputs;
  for (const ConversionOutput& output : manifest.outputs) {
    if (output.converter != type.converter) continue;
    const DataStore::Entry* source = store.Find(output.source);
    if (source == nullptr || source->form != "raw" ||
        !output.source.ends_with(type.extension)) {
      return std::unexpected(
          std::format("{} output names a missing or non-{} source {}",
                      type.converter, type.extension, output.source));
    }
    if (!outputs.emplace(output.source, &output).second) {
      return std::unexpected(std::format("multiple {} outputs name {}",
                                         type.converter, output.source));
    }
  }
  return outputs;
}

// Whether every raw file of `type` has its current converted output, and
// every output of its converter names such a file.
std::expected<void, std::string> RequireConvertedType(
    const DataStore& store, const ConversionManifest& manifest,
    SourceHashes& hashes, const ConvertedType& type) {
  const auto outputs = OutputsBySource(store, manifest, type);
  if (!outputs) return std::unexpected(outputs.error());
  for (const DataStore::Entry& entry : store.Entries()) {
    if (!entry.path.ends_with(type.extension)) continue;
    if (entry.form != "raw") {
      return std::unexpected(
          std::format("{} source is not raw: {}", type.converter, entry.path));
    }
    const auto found = outputs->find(entry.path);
    if (found == outputs->end()) {
      return std::unexpected(std::format("no {} output for {}{}",
                                         type.converter, entry.path, kRerun));
    }
    if (auto required = RequireConvertedSource(store, manifest, hashes, entry,
                                               *found->second);
        !required) {
      return required;
    }
  }
  return {};
}

// The workshop recipe tables, when `manifest` lists their outputs.
std::expected<std::optional<WorkshopRecipeCatalog>, std::string>
LoadListedRecipes(const fs::path& root,
                  const std::optional<ConversionManifest>& manifest) {
  if (!manifest || std::ranges::none_of(
                       manifest->outputs, [](const ConversionOutput& output) {
                         return output.converter == kSydRecipeConverterName;
                       })) {
    return std::nullopt;
  }
  auto recipes = LoadWorkshopRecipes(root);
  if (!recipes) {
    return std::unexpected(recipes.error() + " (re-run VS.Extract tables " +
                           Platform::PathText(root) + ")");
  }
  return std::move(*recipes);
}

}  // namespace

DataStore::ScopedOverride::ScopedOverride() {
  m_previous_ = std::move(Instance());
}

DataStore::ScopedOverride::~ScopedOverride() { Restore(); }

void DataStore::ScopedOverride::Restore() noexcept {
  if (!m_active_) return;
  Instance() = std::move(m_previous_);
  m_active_ = false;
}

u32 DataStore::Entry::SectorCount() const {
  const u64 rounded =
      (static_cast<u64>(size) + kDataSectorBytes - 1) / kDataSectorBytes;
  return static_cast<u32>(std::max<u64>(rounded, 1));
}

bool DataStore::Entry::ContainsSector(u32 sector) const {
  return sector >= lba &&
         static_cast<u64>(sector) < static_cast<u64>(lba) + SectorCount();
}

std::expected<std::vector<DataStore::Entry>, std::string>
DataStore::LoadManifest(const fs::path& root) {
  enum : usize {
    kLbaColumn,
    kSizeColumn,
    kFormColumn,
    kPathColumn,
    kColumnCount
  };
  std::ifstream in(root / ManifestName);
  if (!in) {
    return std::unexpected("no extracted data at " +
                           Platform::PathText(root / ManifestName) +
                           " (run VS.Extract extract <disc> <dir>)");
  }
  std::vector<Entry> entries;
  std::string line;
  std::getline(in, line);  // header
  while (std::getline(in, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    std::string_view v(line);
    Entry e;
    std::string_view cols[kColumnCount];
    for (usize column = 0; column < kColumnCount; ++column) {
      const usize tab = column < kPathColumn ? v.find('\t') : v.size();
      if (tab == std::string_view::npos) {
        return std::unexpected("bad line in " +
                               Platform::PathText(root / ManifestName) + ": " +
                               line);
      }
      cols[column] = v.substr(0, tab);
      v = column < kPathColumn ? v.substr(tab + 1) : std::string_view{};
    }
    if (!ParseU32(cols[kLbaColumn], e.lba) ||
        !ParseU32(cols[kSizeColumn], e.size)) {
      return std::unexpected("bad line in " +
                             Platform::PathText(root / ManifestName) + ": " +
                             line);
    }
    e.form = cols[kFormColumn];
    e.path = cols[kPathColumn];
    entries.push_back(std::move(e));
  }
  std::ranges::sort(entries, {}, &Entry::lba);
  return entries;
}

std::expected<void, std::string> DataStore::Open(const fs::path& root) {
  auto entries = LoadManifest(root);
  if (!entries) return std::unexpected(entries.error());
  auto store = std::make_unique<DataStore>();
  store->m_root_ = root;
  store->m_entries_ = std::move(*entries);
  auto conversions = LoadConversionManifest(root);
  if (!conversions) return std::unexpected(conversions.error());
  store->m_conversion_manifest_ = std::move(*conversions);
  auto recipes = LoadListedRecipes(root, store->m_conversion_manifest_);
  if (!recipes) return std::unexpected(recipes.error());
  store->m_workshop_recipes_ = std::move(*recipes);
  // catalogs\ also holds the tier-1 tables (catalogs\menu\); either asset
  // catalog file means the asset catalog was written and must load.
  std::error_code error;
  const fs::path catalogs = root / Platform::Utf8Path(kCatalogDirectory);
  const bool cataloged =
      fs::exists(catalogs / Platform::Utf8Path(kAssetCatalogFile), error) ||
      (!error &&
       fs::exists(catalogs / Platform::Utf8Path(kAudioCatalogFile), error));
  if (cataloged) {
    if (catalogGeneration == std::numeric_limits<u32>::max()) {
      return std::unexpected("asset catalog generations exhausted");
    }
    auto catalog = AssetCatalog::Load(root, ++catalogGeneration);
    if (!catalog) {
      return std::unexpected(catalog.error() + " (re-run VS.Extract catalogs " +
                             Platform::PathText(root) + ")");
    }
    store->m_catalog_ = std::move(*catalog);
  } else if (error) {
    return std::unexpected("cannot inspect " + Platform::PathText(catalogs) +
                           ": " + error.message());
  }
  VS::Core::LogInfo("content", "extracted data: {} files, {} assets from {}",
                    store->m_entries_.size(),
                    store->m_catalog_ ? store->m_catalog_->Size() : 0,
                    Platform::PathText(root));
  Instance() = std::move(store);
  return {};
}

std::expected<void, std::string> DataStore::RequireCatalog() const {
  if (m_catalog_) return {};
  return std::unexpected(
      "no asset catalog in " +
      Platform::PathText(m_root_ / Platform::Utf8Path(kCatalogDirectory)) +
      " (run VS.Extract catalogs " + Platform::PathText(m_root_) +
      ", or prepare the disc again with VS.Launcher)");
}

std::expected<void, std::string> DataStore::RequireConversions() const {
  if (!m_conversion_manifest_) {
    return std::unexpected(
        "no conversion manifest in " +
        Platform::PathText(m_root_ /
                           Platform::Utf8Path(kConversionManifestName)) +
        " (run VS.Extract extract <disc> <dir>)");
  }
  if (auto required =
          CheckRequiredConverters(*m_conversion_manifest_, kRequiredConverters);
      !required) {
    return required;
  }
  SourceHashes hashes;
  for (const ConvertedType& type : kConvertedTypes) {
    if (auto required =
            RequireConvertedType(*this, *m_conversion_manifest_, hashes, type);
        !required) {
      return required;
    }
  }
  return {};
}

std::expected<void, std::string> DataStore::ReadAsset(AssetHandle asset,
                                                      std::span<u8> out) const {
  if (!m_catalog_) {
    return std::unexpected("asset read refused: no asset catalog is loaded");
  }
  const AssetRecord* record = m_catalog_->Record(asset);
  if (record == nullptr) {
    return std::unexpected(std::format(
        "asset read refused: handle {}/{} was not issued by the open catalog "
        "(generation {})",
        asset.index, asset.generation, m_catalog_->Generation()));
  }
  if (out.size() > record->LoadBytes()) {
    return std::unexpected(
        std::format("asset read refused: {} bytes exceed {}'s {}-byte load",
                    out.size(), record->id, record->LoadBytes()));
  }
  const usize bytes = std::min<usize>(out.size(), record->bytes);
  const fs::path path = m_catalog_->FileOf(*record);
  std::ifstream file(path, std::ios::binary);
  file.read(reinterpret_cast<char*>(out.data()),
            static_cast<std::streamsize>(bytes));
  if (!file || static_cast<usize>(file.gcount()) != bytes) {
    return std::unexpected(std::format("cannot read {} bytes of {} ({})", bytes,
                                       Platform::PathText(path), record->id));
  }
  std::fill(out.begin() + static_cast<std::ptrdiff_t>(bytes), out.end(), u8{0});
  return {};
}

const DataStore* DataStore::Get() { return Instance().get(); }

const DataStore::Entry* DataStore::Find(std::string_view path) const {
  for (const Entry& e : m_entries_) {
    if (e.path == path) {
      return &e;
    }
  }
  return nullptr;
}

const DataStore::Entry* DataStore::FindBySector(u32 lba) const {
  auto it = std::ranges::upper_bound(m_entries_, lba, {}, &Entry::lba);
  if (it == m_entries_.begin()) {
    return nullptr;
  }
  --it;
  return it->ContainsSector(lba) ? &*it : nullptr;
}

fs::path DataStore::FileOf(const Entry& e) const {
  // a converted file keeps the disc name with its form as the extension
  // (MOV/TITLE.STR.mkv)
  const fs::path disc = m_root_ / "disc" / fs::path(e.path);
  fs::path result = disc;
  if (e.form != "raw") result += "." + e.form;
  return result;
}

bool DataStore::Read(const Entry& e, u64 offset, std::span<u8> out) const {
  std::ifstream f(FileOf(e), std::ios::binary);
  if (!f) {
    return false;
  }
  f.seekg(static_cast<std::streamoff>(offset));
  f.read(reinterpret_cast<char*>(out.data()),
         static_cast<std::streamsize>(out.size()));
  return static_cast<usize>(f.gcount()) == out.size();
}

std::expected<std::vector<u8>, std::string> DataStore::ReadAll(
    const Entry& e) const {
  std::vector<u8> data(e.size);
  if (!Read(e, 0, data)) {
    return std::unexpected("cannot read " + Platform::PathText(FileOf(e)));
  }
  return data;
}

}  // namespace VS::Content

int VS::Content::ReadDiscBytes(unsigned lba, unsigned bytes, void* dest) {
  using VS::u32;
  using VS::u64;
  using VS::u8;
  using VS::usize;
  using VS::Content::DataStore;
  const DataStore* store = DataStore::Get();
  if (store == nullptr) {
    return 0;
  }
  // A read runs on through consecutive files, as the drive read sectors
  // (MCDATA.BIN is read together with the MCMAN.BIN after it). Bytes past a
  // file's end inside its last sector, and sectors no file holds, read as 0.
  static std::unordered_set<std::string> logged;
  auto* out = static_cast<u8*>(dest);
  u32 sector = lba;
  usize remaining = bytes;
  while (remaining != 0) {
    const DataStore::Entry* e = store->FindBySector(sector);
    if (e == nullptr) {
      VS::Core::LogError(
          "content",
          "no extracted file holds sector {} (read of {} bytes at sector {})",
          sector, bytes, lba);
      VS::Platform::LogCallStack("bad disc read");
      std::memset(out, 0, remaining);
      return 0;
    }
    const u64 offset =
        static_cast<u64>(sector - e->lba) * DataStore::kDataSectorBytes;
    const u32 sectors = e->SectorCount();
    const usize span = std::min<usize>(
        remaining, (static_cast<u64>(e->lba) + sectors - sector) *
                       DataStore::kDataSectorBytes);
    const usize n = static_cast<usize>(
        std::min<u64>(span, offset < e->size ? e->size - offset : 0));
    if (n != 0 && !store->Read(*e, offset, std::span(out, n))) {
      VS::Core::LogError("content", "read failed: {} at {} ({} bytes)", e->path,
                         offset, n);
      return 0;
    }
    std::memset(out + n, 0, span - n);
    const auto level = logged.insert(e->path).second
                           ? VS::Core::LogLevel::Info
                           : VS::Core::LogLevel::Debug;
    VS::Core::Log(level, "content", "disc read: {} (sector {}, {} bytes, {})",
                  e->path, sector, span, e->form);
    out += span;
    remaining -= span;
    sector += static_cast<u32>(span / DataStore::kDataSectorBytes);
  }
  return 1;
}
