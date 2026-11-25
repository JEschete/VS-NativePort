#include "Content/DataStore.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
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

#include "Content/ActorArchetype.hpp"
#include "Content/AnimationSet.hpp"
#include "Content/AssetCatalog.hpp"
#include "Content/AudioBank.hpp"
#include "Content/AudioStream.hpp"
#include "Content/BestiaryCatalog.hpp"
#include "Content/ConversionManifest.hpp"
#include "Content/DataAccess.hpp"
#include "Content/EsqFile.hpp"
#include "Content/EtmFile.hpp"
#include "Content/MovieEncoder.hpp"
#include "Content/SourceStampCache.hpp"
#include "Content/TextCatalogRegistration.hpp"
#include "Content/WorkshopRecipes.hpp"
#include "Content/ZndFile.hpp"
#include "Content/ZoneTables.hpp"
#include "Content/ZudFile.hpp"
#include "Core/Log.hpp"
#include "Core/Sha256.hpp"
#include "Platform/CrashHandler.hpp"
#include "Platform/Paths.hpp"

namespace fs = std::filesystem;

namespace VS::Content {

namespace {

std::shared_ptr<DataStore>& Instance() {
  static std::shared_ptr<DataStore> store;
  return store;
}

// What OpenShared reuses: a store opened for `root` while its manifest files
// still have these sizes and modification times.
struct FileStamp {
  std::optional<std::uintmax_t> size;
  std::optional<fs::file_time_type> modified;
  bool operator==(const FileStamp&) const = default;
};

struct SharedStore {
  fs::path root;
  std::array<FileStamp, 2> stamps;
  std::shared_ptr<DataStore> store;
};

SharedStore& SharedInstance() {
  static SharedStore shared;
  return shared;
}

FileStamp StampOf(const fs::path& path) {
  std::error_code error;
  FileStamp stamp;
  if (const auto size = fs::file_size(path, error); !error) {
    stamp.size = size;
  }
  if (const auto time = fs::last_write_time(path, error); !error) {
    stamp.modified = time;
  }
  return stamp;
}

std::array<FileStamp, 2> StampsOf(const fs::path& root) {
  return {StampOf(root / DataStore::ManifestName),
          StampOf(root / Platform::Utf8Path(kConversionManifestName))};
}

// The last catalog generation issued; handles from earlier catalogs no longer
// resolve.
u32 catalogGeneration = 0;

bool ParseU32(std::string_view s, u32& out) {
  const auto r = std::from_chars(s.data(), s.data() + s.size(), out);
  return r.ec == std::errc() && r.ptr == s.data() + s.size();
}

// A disc file type the runtime reads only through a converter's outputs.
// `prefix` is the path start ("" for any directory). `form` is the files.tsv
// form of its sources: raw sources are under disc\ and are rehashed, while
// interleaved ones (movies, XA) have no disc\ copy to hash, so only their
// outputs' files are required.
struct ConvertedType {
  std::string_view prefix;
  std::string_view extension;
  std::string_view converter;
  std::string_view form;
  bool exact = false;

  [[nodiscard]] bool Matches(std::string_view path) const {
    if (exact) return path == prefix;
    return path.starts_with(prefix) && path.ends_with(extension);
  }
};

constexpr std::string_view kRawForm = "raw";

constexpr std::array kConvertedTypes{
    ConvertedType{"", ".TIM", "tim-png", kRawForm},
    ConvertedType{"", ".FBC", "fbc-palette-png", kRawForm},
    ConvertedType{"", ".FBT", "fbt-texture-png", kRawForm},
    ConvertedType{"", ".SYD", kSydRecipeConverterName, kRawForm},
    ConvertedType{kMaterialSource, "", kMaterialConverterName, kRawForm, true},
    ConvertedType{kLearningSource, "", kLearningConverter.name, kRawForm, true},
    ConvertedType{kStatusSource, "", kStatusConverter.name, kRawForm, true},
    ConvertedType{kActionCategorySource, "", kActionCategoryConverter.name,
                  kRawForm, true},
    ConvertedType{kBestiarySource, "", kBestiaryConverter, kRawForm, true},
    ConvertedType{"", ".SHP", "shp-gltf", kRawForm},
    ConvertedType{"", ".SEQ", "seq-gltf", kRawForm},
    ConvertedType{"", ".WEP", "wep-gltf", kRawForm},
    ConvertedType{"", kEsqExtension, kEsqAnimationSetConverterName, kRawForm},
    ConvertedType{"", kEtmExtension, kEtmTextureConverterName, kRawForm},
    ConvertedType{"", kZudExtension, kZudArchetypeConverterName, kRawForm},
    ConvertedType{"", kZndExtension, kZndTableConverterName, kRawForm},
    ConvertedType{kWaveBankPrefix, kWaveBankExtension, kWaveBankConverterName,
                  kRawForm},
    ConvertedType{"", kXaExtension, kXaFlacConverterName, "stream"},
    ConvertedType{"", kMovieExtension, kStrMkvConverterName, kMovieForm},
};

// The fix each stale or missing output error names.
constexpr std::string_view kRerun = " (run VS.Extract extract <disc> <dir>)";

std::string_view RerunFor(std::string_view converter) {
  if (converter == kBestiaryConverter) {
    return " (run VS.Extract bestiary <dir>)";
  }
  if (converter == kSaveTextConverter) return " (run VS.Extract text <dir>)";
  if (converter == kStatusConverter.name) {
    return " (run VS.Extract status <dir>)";
  }
  if (converter == kActionCategoryConverter.name) {
    return " (run VS.Extract action-categories <dir>)";
  }
  return converter == kMaterialConverterName ? " (run VS.Extract tables <dir>)"
                                             : kRerun;
}

// Whether `entry` is an empty placeholder: no bytes, or one zero byte (the
// unused SOUND, MUSIC and SE slots), which no converter reads.
std::expected<bool, std::string> IsPlaceholder(const DataStore& store,
                                               const DataStore::Entry& entry) {
  if (entry.form != kRawForm || entry.size > 1) return false;
  if (entry.size == 0) return true;
  std::array<u8, 1> byte{};
  if (!store.Read(entry, 0, byte)) {
    return std::unexpected("cannot read " +
                           Platform::PathText(store.FileOf(entry)));
  }
  return byte[0] == 0;
}

// The SHA-256 of each extracted file one check has read: many outputs share
// a dependency such as EFFECT/EFFPURGE.BIN.
struct SourceHashes {
  std::unordered_map<std::string, std::string> values;
  SourceStampCache& cache;
  SourceVerification verification;
  SourceVerificationStats& statistics;
};

// Whether `entry`'s bytes still hash to `sha256`.
std::expected<bool, std::string> HashMatches(const DataStore& store,
                                             SourceHashes& hashes,
                                             const DataStore::Entry& entry,
                                             std::string_view sha256) {
  const auto found = hashes.values.find(entry.path);
  if (found != hashes.values.end()) return found->second == sha256;
  const auto path = store.FileOf(entry);
  const auto before = ReadSourceFileStamp(path);
  if (!before) return std::unexpected(before.error());
  if (before->size != entry.size) {
    return std::unexpected(
        std::format("{}: source size is {}, expected {} from files.tsv",
                    Platform::PathText(path), before->size, entry.size));
  }
  if (hashes.verification == SourceVerification::kCached &&
      hashes.cache.Use(entry.path, *before, sha256)) {
    hashes.values.emplace(entry.path, sha256);
    ++hashes.statistics.cache_hits;
    return true;
  }
  const auto bytes = store.ReadAll(entry);
  if (!bytes) return std::unexpected(bytes.error());
  const auto actual = Core::Sha256Hex(std::as_bytes(std::span(*bytes)));
  ++hashes.statistics.files_hashed;
  hashes.statistics.bytes_hashed += bytes->size();
  const auto after = ReadSourceFileStamp(path);
  if (!after) return std::unexpected(after.error());
  if (*before != *after) {
    return std::unexpected(Platform::PathText(path) +
                           ": source changed during verification; retry");
  }
  hashes.values.emplace(entry.path, actual);
  if (actual != sha256) return false;
  hashes.cache.Remember(entry.path, *after, actual);
  return true;
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
      return std::unexpected(std::format(
          "missing {} output {}{}", conversion.converter,
          Platform::PathText(path), RerunFor(conversion.converter)));
    }
  }
  return {};
}

// Whether the output `type`'s converter made from `entry` is current with its
// source and dependencies, and the output holding its files has every file.
// An interleaved source has no extracted bytes to rehash; its output's files
// are still required.
std::expected<void, std::string> RequireConvertedSource(
    const DataStore& store, const ConversionManifest& manifest,
    SourceHashes& hashes, const DataStore::Entry& entry,
    const ConversionOutput& conversion) {
  if (entry.form == kRawForm) {
    const auto current =
        HashMatches(store, hashes, entry, conversion.source_sha256);
    if (!current) return std::unexpected(current.error());
    if (!*current) {
      return std::unexpected(std::format("stale {} source hash for {}{}",
                                         conversion.converter, entry.path,
                                         RerunFor(conversion.converter)));
    }
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

using SourceOutputs = std::vector<const ConversionOutput*>;
using OutputMap = std::unordered_map<std::string_view, SourceOutputs>;

// Each output of `type`'s converter by source path, checking that it names a
// file of `type` in its form and that aliases share one physical output owner.
std::expected<OutputMap, std::string> OutputsBySource(
    const DataStore& store, const ConversionManifest& manifest,
    const ConvertedType& type) {
  OutputMap outputs;
  for (const ConversionOutput& output : manifest.outputs) {
    if (output.converter != type.converter) continue;
    const DataStore::Entry* source = store.Find(output.source);
    if (source == nullptr || source->form != type.form ||
        !type.Matches(output.source)) {
      return std::unexpected(std::format(
          "{} output names a missing or non-{} {}*{} source {}", type.converter,
          type.form, type.prefix, type.extension, output.source));
    }
    auto& matches = outputs[output.source];
    if (!matches.empty() &&
        (manifest.FilesOf(*matches.front()) != manifest.FilesOf(output) ||
         matches.front()->source_sha256 != output.source_sha256)) {
      return std::unexpected(std::format("multiple {} outputs name {}",
                                         type.converter, output.source));
    }
    matches.push_back(&output);
  }
  return outputs;
}

// Whether every file of `type`, other than a placeholder, has its current
// converted output, and every output of its converter names such a file.
std::expected<void, std::string> RequireConvertedType(
    const DataStore& store, const ConversionManifest& manifest,
    SourceHashes& hashes, const ConvertedType& type) {
  const auto outputs = OutputsBySource(store, manifest, type);
  if (!outputs) return std::unexpected(outputs.error());
  for (const DataStore::Entry& entry : store.Entries()) {
    if (!type.Matches(entry.path)) continue;
    const auto found = outputs->find(entry.path);
    if (found == outputs->end()) {
      const auto placeholder = IsPlaceholder(store, entry);
      if (!placeholder) return std::unexpected(placeholder.error());
      if (*placeholder) continue;
      // An interleaved file is listed as "stream" until it is converted.
      if (entry.form != type.form && type.form == kRawForm) {
        return std::unexpected(std::format("{} source is not raw: {}",
                                           type.converter, entry.path));
      }
      return std::unexpected(std::format("no {} output for {}{}",
                                         type.converter, entry.path,
                                         RerunFor(type.converter)));
    }
    for (const auto* output : found->second) {
      if (auto required =
              RequireConvertedSource(store, manifest, hashes, entry, *output);
          !required) {
        return required;
      }
    }
  }
  return {};
}

struct CatalogRecords {
  std::optional<WorkshopRecipeCatalog> recipes;
  std::optional<MaterialCatalog> materials;
  std::optional<BestiaryCatalog> bestiary;
  std::optional<LearningCatalog> learning;
  std::optional<StatusCatalog> status;
  std::optional<ActionCategoryCatalog> action_categories;
};

struct LoadedCatalogs {
  CatalogDatabase database;
  CatalogRecords records;
};

// The catalog families the conversion manifest publishes.
struct CatalogFamilies {
  bool recipes = false;
  bool materials = false;
  bool bestiary = false;
  bool learning = false;
  bool status = false;
  bool action_categories = false;

  [[nodiscard]] bool Any() const {
    return recipes || materials || bestiary || learning || status ||
           action_categories;
  }
};

std::expected<CatalogRecords, std::string> ReadCatalogRecords(
    const CatalogDatabase& database, const CatalogFamilies& families) {
  CatalogRecords records;
  if (families.action_categories) {
    auto loaded = LoadActionCategoryCatalog(database);
    if (!loaded) return std::unexpected(loaded.error());
    records.action_categories = std::move(*loaded);
  }
  if (families.status) {
    auto loaded = LoadStatusCatalog(database);
    if (!loaded) return std::unexpected(loaded.error());
    records.status = std::move(*loaded);
  }
  if (families.learning) {
    auto loaded = LoadLearning(database);
    if (!loaded) return std::unexpected(loaded.error());
    records.learning = std::move(*loaded);
  }
  if (families.recipes) {
    auto loaded = LoadWorkshopRecipes(database);
    if (!loaded) return std::unexpected(loaded.error());
    records.recipes = std::move(*loaded);
  }
  if (families.materials) {
    auto loaded = LoadMaterials(database);
    if (!loaded) return std::unexpected(loaded.error());
    records.materials = std::move(*loaded);
  }
  if (families.bestiary) {
    auto loaded = LoadBestiary(database);
    if (!loaded) return std::unexpected(loaded.error());
    records.bestiary = std::move(*loaded);
  }
  return records;
}

// One conversion registration: the single output a converter may publish, with
// the converter version the manifest must record for it.
struct CatalogRegistration {
  std::string_view converter;
  std::uint32_t version = 0;
  std::string_view id;
  std::string_view source;
  std::string_view error;
};

bool ValidRegistration(const ConversionManifest& manifest,
                       const ConversionOutput& output,
                       const CatalogRegistration& registration, bool seen) {
  const auto version = manifest.converters.find(registration.converter);
  return !seen && output.id == registration.id &&
         output.source == registration.source && output.alias_of.empty() &&
         version != manifest.converters.end() &&
         version->second == registration.version;
}

// Marks the family published by `output`, or returns its registration error.
std::expected<void, std::string> RegisterFamily(
    const ConversionManifest& manifest, const ConversionOutput& output,
    const CatalogRegistration& registration, bool& family) {
  if (output.converter != registration.converter) return {};
  if (!ValidRegistration(manifest, output, registration, family)) {
    return std::unexpected(std::string(registration.error));
  }
  family = true;
  return {};
}

void AppendSources(std::vector<CatalogTableSource>& sources,
                   std::vector<CatalogTableSource> more) {
  for (auto& source : more) sources.push_back(std::move(source));
}

std::expected<CatalogFamilies, std::string> ReadCatalogFamilies(
    const ConversionManifest& manifest) {
  CatalogFamilies families;
  const std::array registrations{
      std::pair{
          CatalogRegistration{
              kActionCategoryConverter.name, kActionCategoryConverter.version,
              kActionCategoryOutputId, kActionCategorySource,
              "invalid action category conversion registration"},
          &families.action_categories},
      std::pair{
          CatalogRegistration{kStatusConverter.name, kStatusConverter.version,
                              kStatusOutputId, kStatusSource,
                              "invalid status conversion registration"},
          &families.status},
      std::pair{CatalogRegistration{kLearningConverter.name,
                                    kLearningConverter.version,
                                    kLearningOutputId, kLearningSource,
                                    "invalid learning conversion registration "
                                    "(run VS.Extract tables <dir>)"},
                &families.learning},
      std::pair{
          CatalogRegistration{kBestiaryConverter, kBestiaryConverterVersion,
                              kBestiaryId, kBestiarySource,
                              "invalid bestiary conversion registration"},
          &families.bestiary},
      std::pair{
          CatalogRegistration{kMaterialConverterName, kMaterialConverterVersion,
                              kMaterialOutputId, kMaterialSource,
                              "invalid material conversion registration "
                              "(run VS.Extract tables <dir>)"},
          &families.materials}};
  for (const auto& output : manifest.outputs) {
    for (const auto& [registration, family] : registrations) {
      if (auto marked = RegisterFamily(manifest, output, registration, *family);
          !marked) {
        return std::unexpected(marked.error());
      }
    }
    families.recipes |= output.converter == kSydRecipeConverterName;
  }
  return families;
}

// Select only published families: the extractor can open raw/synthetic stores
// before generating their tables. All selected consumers share one cache.
std::expected<std::optional<LoadedCatalogs>, std::string> LoadListedCatalogs(
    const fs::path& root, const std::optional<ConversionManifest>& manifest) {
  if (!manifest) return std::nullopt;
  const auto listed = ReadCatalogFamilies(*manifest);
  if (!listed) return std::unexpected(listed.error());
  const CatalogFamilies& families = *listed;
  if (!families.Any()) return std::nullopt;
  auto sources = families.recipes ? WorkshopCatalogSources()
                                  : std::vector<CatalogTableSource>{};
  if (families.materials) sources.push_back(MaterialCatalogSource());
  if (families.bestiary) sources.push_back(BestiaryCatalogSource());
  if (families.learning) AppendSources(sources, LearningCatalogSources());
  if (families.status) AppendSources(sources, StatusCatalogSources());
  if (families.action_categories) {
    AppendSources(sources, ActionCategoryCatalogSources());
  }
  auto database = CatalogDatabase::Load(root, sources);
  if (!database) {
    return std::unexpected(
        database.error() +
        " (check catalog-layers.json and layer CSVs; for missing base tables, "
        "re-run VS.Extract tables " +
        Platform::PathText(root) + ")");
  }
  auto records = ReadCatalogRecords(*database, families);
  if (!records && database->CacheHit()) {
    Core::LogWarn("content", "compiled catalog rejected; recompiling CSVs: {}",
                  records.error());
    database = CatalogDatabase::Load(root, sources, false);
    if (!database) return std::unexpected(database.error());
    records = ReadCatalogRecords(*database, families);
  }
  if (!records) {
    return std::unexpected(records.error() +
                           " (check catalog-layers.json and layer CSVs; run "
                           "VS.Extract tables <dir> for missing base tables)");
  }
  return LoadedCatalogs{std::move(*database), std::move(*records)};
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
  auto tables = LoadListedCatalogs(root, store->m_conversion_manifest_);
  if (!tables) return std::unexpected(tables.error());
  if (*tables) {
    store->m_workshop_recipes_ = std::move((**tables).records.recipes);
    store->m_materials_ = std::move((**tables).records.materials);
    store->m_learning_ = std::move((**tables).records.learning);
    store->m_status_ = std::move((**tables).records.status);
    store->m_action_categories_ =
        std::move((**tables).records.action_categories);
    store->m_catalog_tables_ = std::move((**tables).database);
  }
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
  if (store->m_conversion_manifest_ &&
      store->m_conversion_manifest_->Find(kSaveTextId) != nullptr) {
    auto text = LoadSaveTextAssets(*store);
    if (!text) return std::unexpected(text.error());
    store->m_save_text_ = std::move(*text);
  }
  if (*tables && (**tables).records.bestiary) {
    if (!store->m_save_text_) {
      return std::unexpected("bestiary: missing native text catalog");
    }
    auto references = ValidateBestiaryTextReferences(
        *store->m_catalog_tables_, store->m_save_text_->catalog);
    if (!references) {
      return std::unexpected(references.error());
    }
  }
  if (store->m_catalog_tables_) {
    if (auto cached = store->m_catalog_tables_->PublishCache(); !cached) {
      Core::LogWarn("content",
                    "catalog validated; cache could not be saved: {}",
                    cached.error());
    }
    for (const auto& conflict : store->m_catalog_tables_->Conflicts()) {
      VS::Core::LogInfo("content", "catalog conflict {} [{}]: {} overrides {}",
                        conflict.table, conflict.key, conflict.winning_source,
                        conflict.previous_source);
    }
  }
  VS::Core::LogInfo("content", "extracted data: {} files, {} assets from {}",
                    store->m_entries_.size(),
                    store->m_catalog_ ? store->m_catalog_->Size() : 0,
                    Platform::PathText(root));
  Instance() = std::move(store);
  return {};
}

std::expected<void, std::string> DataStore::OpenShared(const fs::path& root) {
  SharedStore& shared = SharedInstance();
  const auto stamps = StampsOf(root);
  if (shared.store && shared.root == root && shared.stamps == stamps) {
    Instance() = shared.store;
    return {};
  }
  if (auto opened = Open(root); !opened) {
    return opened;
  }
  shared = {.root = root, .stamps = stamps, .store = Instance()};
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

std::expected<void, std::string> DataStore::RequireConversions(
    SourceVerification verification,
    SourceVerificationStats* statistics) const {
  SourceVerificationStats local;
  auto& measured = statistics != nullptr ? *statistics : local;
  measured = {};
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
    return std::unexpected(required.error() + std::string(kRerun));
  }
  SourceStampCache cache(m_root_);
  if (auto loaded = cache.Load(); !loaded) {
    Core::LogWarn("content", "source stamp cache ignored; checking sources: {}",
                  loaded.error());
  }
  SourceHashes hashes{.values = {},
                      .cache = cache,
                      .verification = verification,
                      .statistics = measured};
  for (const ConvertedType& type : kConvertedTypes) {
    if (auto required =
            RequireConvertedType(*this, *m_conversion_manifest_, hashes, type);
        !required) {
      return required;
    }
  }
  // The text/font converter owns sections of two sources, not one container
  // per output. Keep section ownership distinct from the generic file gate.
  if (Find("MENU/MCMAN.BIN") != nullptr && Find("TITLE/TITLE.PRG") != nullptr) {
    for (const auto id : {kSaveTextId, kSaveFontId, kSaveFontImageId}) {
      const auto* output = m_conversion_manifest_->Find(id);
      const auto* source = output == nullptr ? nullptr : Find(output->source);
      if (source == nullptr || output->converter != kSaveTextConverter) {
        return std::unexpected(
            std::string(id) +
            ": missing native save text; run VS.Extract text <dir>");
      }
      if (auto required = RequireConvertedSource(*this, *m_conversion_manifest_,
                                                 hashes, *source, *output);
          !required) {
        return required;
      }
    }
    if (const auto* output = m_conversion_manifest_->Find(kNativeTextCatalogId);
        output != nullptr) {
      const auto* source = Find(output->source);
      if (source == nullptr ||
          output->converter != kNativeTextCatalogConverter) {
        return std::unexpected("native text: invalid contributor source");
      }
      if (auto required = RequireConvertedSource(*this, *m_conversion_manifest_,
                                                 hashes, *source, *output);
          !required) {
        return required;
      }
    }
  }
  if (auto saved = cache.Publish(); !saved) {
    Core::LogWarn("content", "sources verified; cache could not be saved: {}",
                  saved.error());
  }
  Core::LogDebug(
      "content", "source verification: {} hashes ({} bytes), {} stamp hits",
      measured.files_hashed, measured.bytes_hashed, measured.cache_hits);
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
