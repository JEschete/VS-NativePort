#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <expected>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Audio/SoundBanks.hpp"
#include "Audio/SoundSlotsLive.hpp"
#include "Content/AssetCatalog.hpp"
#include "Content/AssetCatalogImport.hpp"
#include "Content/AssetHandle.hpp"
#include "Content/CdDrive.hpp"
#include "Content/CdDriveLive.hpp"
#include "Content/DataStore.hpp"
#include "Content/DiscQueue.hpp"
#include "Content/DiscReaderLive.hpp"
#include "State/PlayTime.hpp"
#include "nlohmann/json.hpp"

namespace {
namespace Content = VS::Content;
namespace fs = std::filesystem;
using Json = nlohmann::json;

void RemoveDisplayNames(Json& value) {
  if (!value.is_structured()) return;
  if (value.is_object()) {
    const auto label = value.find("display_name");
    if (label != value.end()) {
      REQUIRE(label->is_string());
      value.erase(label);
    }
  }
  for (Json& child : value) RemoveDisplayNames(child);
}

Json CatalogValues(std::string_view text) {
  Json value = Json::parse(text);
  RemoveDisplayNames(value);
  return value;
}

struct DiscFileSpec {
  std::string_view path;
  std::uint32_t lba;
  std::uint32_t bytes;
};

// Raw files covering every directory the importer's tables use; the sizes
// leave partial last sectors.
constexpr std::array<DiscFileSpec, 7> kFiles = {{
    {.path = "MUSIC/MUSIC000.DAT", .lba = 100, .bytes = 3000},
    {.path = "MUSIC/MUSIC001.DAT", .lba = 102, .bytes = 2048},
    {.path = "SE/EFFECT00.DAT", .lba = 200, .bytes = 100},
    {.path = "SE/SEP00.DAT", .lba = 201, .bytes = 4097},
    {.path = "SOUND/WAVE0000.DAT", .lba = 300, .bytes = 10},
    {.path = "SOUND/WAVE0005.DAT", .lba = 301, .bytes = 20},
    {.path = "SOUND/WAVE0200.DAT", .lba = 302, .bytes = 30},
}};

std::uint8_t FileByte(std::size_t file, std::size_t offset) {
  return static_cast<std::uint8_t>((file * 31U + offset * 7U + 1U) & 0xFFU);
}

std::string IdOf(std::string_view path) {
  return Content::AssetIdForDiscPath(path).value_or("");
}

// The present value; fails the test when there is none.
template <class T>
T Required(const std::optional<T>& value) {
  REQUIRE(value.has_value());
  return value.value_or(T{});
}

std::uint32_t WholeSectors(std::uint32_t bytes) {
  return std::max<std::uint32_t>((bytes + 2047U) / 2048U, 1U) * 2048U;
}

// A temporary data root: the manifest, the raw files under disc\ and, once
// written, the catalogs.
class DataRoot {
 public:
  DataRoot()
      : root_(
            fs::temp_directory_path() /
            ("vs-asset-catalog-" +
             std::to_string(
                 std::chrono::steady_clock::now().time_since_epoch().count()) +
             "-" + std::to_string(++serial))) {
    fs::create_directories(root_ / "disc");
    for (std::size_t index = 0; index < kFiles.size(); ++index) {
      WriteFile(kFiles[index].path, Contents(index));
    }
  }
  ~DataRoot() {
    try {
      std::error_code error;
      fs::remove_all(root_, error);
    } catch (const std::exception& failure) {
      std::fprintf(stderr, "asset catalog fixture left files: %s\n",
                   failure.what());
    }
  }
  DataRoot(const DataRoot&) = delete;
  DataRoot& operator=(const DataRoot&) = delete;
  DataRoot(DataRoot&&) = delete;
  DataRoot& operator=(DataRoot&&) = delete;

  [[nodiscard]] const fs::path& Path() const { return root_; }

  static std::vector<std::uint8_t> Contents(std::size_t file) {
    std::vector<std::uint8_t> bytes(kFiles[file].bytes);
    for (std::size_t offset = 0; offset < bytes.size(); ++offset) {
      bytes[offset] = FileByte(file, offset);
    }
    return bytes;
  }

  void WriteFile(std::string_view disc_path,
                 std::span<const std::uint8_t> bytes) const {
    const fs::path path = root_ / "disc" / fs::path(disc_path);
    fs::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    REQUIRE(output.good());
  }

  void WriteManifest(std::span<const Content::DataStore::Entry> entries) const {
    std::ofstream manifest(root_ / Content::DataStore::ManifestName);
    manifest << "lba\tsize\tform\tpath\n";
    for (const auto& entry : entries) {
      manifest << entry.lba << '\t' << entry.size << '\t' << entry.form << '\t'
               << entry.path << '\n';
    }
    REQUIRE(manifest.good());
  }

  void WriteCatalogs(const Json& assets, const Json& audio) const {
    WriteText(assets.dump(2), Content::kAssetCatalogFile);
    WriteText(audio.dump(2), Content::kAudioCatalogFile);
  }

  void WriteText(std::string_view text, std::string_view file) const {
    const fs::path directory = root_ / fs::path(Content::kCatalogDirectory);
    fs::create_directories(directory);
    std::ofstream output(directory / fs::path(file),
                         std::ios::binary | std::ios::trunc);
    output.write(text.data(), static_cast<std::streamsize>(text.size()));
    REQUIRE(output.good());
  }

 private:
  static inline int serial = 0;
  fs::path root_;
};

std::vector<Content::DataStore::Entry> RawEntries() {
  std::vector<Content::DataStore::Entry> entries;
  for (const DiscFileSpec& file : kFiles) {
    entries.push_back({.path = std::string(file.path),
                       .lba = file.lba,
                       .size = file.bytes,
                       .form = "raw"});
  }
  return entries;
}

Json AssetsJson() {
  Json assets = Json::array();
  for (const DiscFileSpec& file : kFiles) {
    assets.push_back(
        {{"id", IdOf(file.path)},
         {"file", "disc/" + std::string(file.path)},
         {"bytes", file.bytes},
         {"provenance",
          {{"disc_path", std::string(file.path)}, {"lba", file.lba}}}});
  }
  return {{"schema", "vs.assets"}, {"schema_version", 1}, {"assets", assets}};
}

Json Table(std::size_t count,
           const std::function<std::string_view(std::size_t)>& path_of) {
  Json rows = Json::array();
  for (std::size_t index = 0; index < count; ++index) {
    rows.push_back({{"legacyId", index}, {"asset", IdOf(path_of(index))}});
  }
  return rows;
}

std::string_view MusicPath(std::size_t index) {
  return index % 2 == 0 ? "MUSIC/MUSIC000.DAT" : "MUSIC/MUSIC001.DAT";
}

Json AudioJson() {
  return {
      {"schema", "vs.audio"},
      {"schema_version", 1},
      {"music", Table(Content::kMusicAssetCount, MusicPath)},
      {"sfx_banks", Table(Content::kSfxBankAssetCount,
                          [](std::size_t) { return "SE/SEP00.DAT"; })},
      {"wave_banks", Table(Content::kWaveBankAssetCount,
                           [](std::size_t) { return "SOUND/WAVE0000.DAT"; })},
      {"menu",
       {{"sample_banks",
         {IdOf("SOUND/WAVE0000.DAT"), IdOf("SOUND/WAVE0005.DAT")}},
        {"effect_sample_bank", IdOf("SOUND/WAVE0200.DAT")},
        {"effect_programs", IdOf("SE/EFFECT00.DAT")}}}};
}

std::expected<Content::AssetCatalog, std::string> Parse(const DataRoot& root,
                                                        const Json& assets,
                                                        const Json& audio) {
  return Content::AssetCatalog::Parse(assets.dump(), audio.dump(), root.Path(),
                                      1);
}

TEST_CASE(
    "Asset compatibility aliases share canonical handles and audio references",
    "[asset-catalog][far-authoring]") {
  DataRoot root;
  auto assets = AssetsJson();
  assets["schema_version"] = 2;
  const auto canonical = IdOf(kFiles.front().path);
  assets["aliases"] = {{"asset.compat.music", canonical}};
  auto audio = AudioJson();
  audio["music"][0]["asset"] = "asset.compat.music";
  const auto catalog = Parse(root, assets, audio);
  REQUIRE(catalog.has_value());
  CHECK(catalog->Find("asset.compat.music") == catalog->Find(canonical));
  CHECK(catalog->Audio().music.front() == Required(catalog->Find(canonical)));
  CHECK(catalog->Size() == kFiles.size());
}

TEST_CASE(
    "Asset aliases reject collisions, chains and unversioned declarations",
    "[asset-catalog][far-authoring]") {
  DataRoot root;
  auto assets = AssetsJson();
  assets["schema_version"] = 2;
  const auto canonical = IdOf(kFiles.front().path);
  SECTION("canonical collision") {
    assets["aliases"] = {{canonical, canonical}};
  }
  SECTION("unresolved target") {
    assets["aliases"] = {{"asset.compat.test", "asset.missing"}};
  }
  SECTION("alias chain") {
    assets["aliases"] = {{"asset.compat.a", canonical},
                         {"asset.compat.b", "asset.compat.a"}};
  }
  SECTION("version one") {
    assets["schema_version"] = 1;
    assets["aliases"] = Json::object();
  }
  CHECK_FALSE(Parse(root, assets, AudioJson()));
}

TEST_CASE("Duplicate asset alias keys are not silently replaced",
          "[asset-catalog][far-authoring]") {
  DataRoot root;
  auto assets = AssetsJson();
  assets["schema_version"] = 2;
  auto text = assets.dump();
  text.pop_back();
  text += ",\"aliases\":{\"asset.compat.same\":\"" + IdOf(kFiles[0].path) +
          "\",\"asset.compat.same\":\"" + IdOf(kFiles[1].path) + "\"}}";
  const auto parsed =
      Content::AssetCatalog::Parse(text, AudioJson().dump(), root.Path(), 1);
  REQUIRE_FALSE(parsed);
  CHECK(parsed.error().find("duplicate JSON object key") != std::string::npos);
}

// A data root opened as the current store with valid catalogs.
struct OpenCatalog {
  Content::DataStore::ScopedOverride restore;
  DataRoot root;

  OpenCatalog() {
    root.WriteManifest(RawEntries());
    root.WriteCatalogs(AssetsJson(), AudioJson());
    Reopen();
  }

  void Reopen() const {
    const auto opened = Content::DataStore::Open(root.Path());
    REQUIRE(opened.has_value());
    REQUIRE(Content::DataStore::Get()->Catalog() != nullptr);
  }

  [[nodiscard]] static Content::AssetHandle Handle(std::string_view path) {
    const auto handle = Content::DataStore::Get()->Catalog()->Find(IdOf(path));
    REQUIRE(handle.has_value());
    return handle.value_or(Content::AssetHandle{});
  }

  [[nodiscard]] static Content::DiscFileExtent Extent(std::string_view path) {
    const Content::AssetHandle handle = Handle(path);
    const Content::AssetRecord* record =
        Content::DataStore::Get()->Catalog()->Record(handle);
    REQUIRE(record != nullptr);
    return {.sector = 0,
            .bytes = static_cast<std::int32_t>(
                record != nullptr ? record->LoadBytes() : 0),
            .asset = handle};
  }
};

// File `file`'s bytes followed by zeros to its whole-sector load size.
std::vector<std::uint8_t> Loaded(std::size_t file) {
  std::vector<std::uint8_t> bytes = DataRoot::Contents(file);
  bytes.resize(WholeSectors(kFiles[file].bytes), 0);
  return bytes;
}

// Saves the live request array, counts and drive record, leaving the drive
// idle and every request free.
struct LiveQueueScope {
  std::array<Content::DiscQueueSlot, Content::kDiscRequestCapacity>
      saved_slots{};
  Content::DiscRequestCounts saved_counts = Content::discRequestCounts;
  Content::DiscDrive saved_drive = Content::discDriveState;
  VS::State::PlayTime saved_play_time = VS::State::playTime;
  decltype(Content::legacyDiscReadBuffer) saved_read_buffer =
      Content::legacyDiscReadBuffer;

  LiveQueueScope() {
    saved_slots = Content::discRequestSlots;
    Content::InitializeLiveDiscQueue();
    Content::discDriveState.state = Content::DiscDriveState::kIdle;
  }
  ~LiveQueueScope() {
    Content::ResetLiveDiscDrive();
    Content::discRequestSlots = saved_slots;
    Content::discRequestCounts = saved_counts;
    Content::discDriveState = saved_drive;
    VS::State::playTime = saved_play_time;
    Content::legacyDiscReadBuffer = saved_read_buffer;
  }
  LiveQueueScope(const LiveQueueScope&) = delete;
  LiveQueueScope& operator=(const LiveQueueScope&) = delete;
  LiveQueueScope(LiveQueueScope&&) = delete;
  LiveQueueScope& operator=(LiveQueueScope&&) = delete;
};

Content::DiscRequestState RunQueue(const Content::DiscQueueSlot* request) {
  for (int frame = 0; frame < 16; ++frame) {
    if (request->state == Content::DiscRequestState::kLoaded ||
        request->state == Content::DiscRequestState::kError) {
      break;
    }
    Content::UpdateLiveDiscQueue();
  }
  return request->state;
}

// Queues one read of `extent` into `buffer` and returns how it ended.
Content::DiscRequestState ReadExtent(const Content::DiscFileExtent& extent,
                                     std::vector<std::uint8_t>& buffer) {
  Content::DiscQueueSlot* request = Content::AllocateLiveDiscRequest(extent);
  REQUIRE(request != nullptr);
  Content::EnqueueLiveDiscRequest(request, buffer.data());
  const Content::DiscRequestState state = RunQueue(request);
  Content::ReleaseLiveDiscRequest(request);
  return state;
}

std::span<std::byte> Bytes(std::vector<std::uint8_t>& buffer) {
  return std::as_writable_bytes(std::span(buffer));
}

// The repository's extracted data, when present.
std::optional<fs::path> RepositoryData() {
  for (auto dir = fs::current_path();
       dir.has_parent_path() && dir != dir.parent_path();
       dir = dir.parent_path()) {
    const fs::path data = dir / "Native" / "data";
    if (fs::exists(data / Content::DataStore::ManifestName)) return data;
  }
  return std::nullopt;
}

std::string ReadAll(const fs::path& path) {
  std::ifstream input(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(input),
          std::istreambuf_iterator<char>()};
}

// A PS-X EXE whose text holds the three audio tables at their original
// addresses, each entry naming `sector_of(table, index)` and the whole-sector
// size of the file there.
constexpr std::uint32_t kExecutableBase = 0x8004A000;
constexpr std::uint32_t kExecutableText = 0x1200;

void PutWord(std::vector<std::uint8_t>& bytes, std::size_t offset,
             std::uint32_t value) {
  for (std::size_t index = 0; index < 4; ++index) {
    bytes[offset + index] = static_cast<std::uint8_t>(value >> (index * 8U));
  }
}

struct TableLayout {
  std::uint32_t sectors;
  std::uint32_t sizes;
  std::size_t count;
};
constexpr std::array<TableLayout, 3> kTables = {{
    {.sectors = 0x8004A6C0, .sizes = 0x8004AD00, .count = 200},
    {.sectors = 0x8004A9E0, .sizes = 0x8004B020, .count = 100},
    {.sectors = 0x8004A530, .sizes = 0x8004AB70, .count = 100},
}};

std::uint32_t SpecSize(std::uint32_t lba) {
  const auto found = std::ranges::find(kFiles, lba, &DiscFileSpec::lba);
  return found == kFiles.end() ? 2048U : WholeSectors(found->bytes);
}

// The sector each table entry names: music alternates its two files, the
// sound effect and wave tables repeat one.
std::uint32_t TableSector(std::size_t table, std::size_t index) {
  switch (table) {
    case 0:
      return index % 2 == 0 ? 100U : 102U;
    case 1:
      return 201U;
    default:
      return 300U;
  }
}

std::vector<std::uint8_t> Executable() {
  std::vector<std::uint8_t> bytes(0x800 + kExecutableText, 0);
  std::memcpy(bytes.data(), "PS-X EXE", 8);
  PutWord(bytes, 0x18, kExecutableBase);
  PutWord(bytes, 0x1C, kExecutableText);
  for (std::size_t table = 0; table < kTables.size(); ++table) {
    for (std::size_t index = 0; index < kTables[table].count; ++index) {
      const std::uint32_t lba = TableSector(table, index);
      const std::size_t entry = index * 4;
      PutWord(bytes, 0x800 + kTables[table].sectors - kExecutableBase + entry,
              lba);
      PutWord(bytes, 0x800 + kTables[table].sizes - kExecutableBase + entry,
              SpecSize(lba));
    }
  }
  return bytes;
}

void SetEntry(std::vector<std::uint8_t>& exe, std::size_t table,
              std::size_t index, std::uint32_t lba, std::uint32_t bytes) {
  PutWord(exe, 0x800 + kTables[table].sectors - kExecutableBase + index * 4,
          lba);
  PutWord(exe, 0x800 + kTables[table].sizes - kExecutableBase + index * 4,
          bytes);
}

// The error importing `files` with `exe`; fails the test if the import works.
std::string ImportError(const std::vector<std::uint8_t>& exe,
                        std::span<const Content::DataStore::Entry> files) {
  const auto texts = Content::ImportAssetCatalogs(files, exe);
  REQUIRE_FALSE(texts.has_value());
  return texts.has_value() ? std::string() : texts.error();
}

// Writes `exe` and a manifest naming it beside the raw files; returns the
// manifest's entry count.
std::size_t PrepareImport(const DataRoot& root,
                          std::span<const std::uint8_t> exe) {
  root.WriteFile(Content::kOriginalExecutablePath, exe);
  std::vector<Content::DataStore::Entry> entries = RawEntries();
  entries.push_back({.path = std::string(Content::kOriginalExecutablePath),
                     .lba = 24,
                     .size = static_cast<std::uint32_t>(exe.size()),
                     .form = "raw"});
  root.WriteManifest(entries);
  return entries.size();
}

std::uint32_t GetWord(std::span<const std::uint8_t> bytes, std::size_t offset) {
  std::uint32_t value = 0;
  for (std::size_t index = 0; index < 4; ++index) {
    value |= static_cast<std::uint32_t>(bytes[offset + index]) << (index * 8U);
  }
  return value;
}

// The repository's extracted data with its manifest and executable.
struct ExtractedData {
  fs::path root;
  std::vector<Content::DataStore::Entry> entries;
  std::vector<std::uint8_t> exe;
};

// The extracted data with catalogs, or why a test of it is skipped.
std::expected<ExtractedData, std::string> Extracted() {
  const auto data = RepositoryData();
  if (!data) return std::unexpected("Extracted Native/data is unavailable");
  if (!fs::exists(*data / "catalogs" / "assets.json")) {
    return std::unexpected(
        "Native/data has no catalogs; run VS.Extract catalogs Native\\data");
  }
  auto entries = Content::DataStore::LoadManifest(*data);
  REQUIRE(entries.has_value());
  const std::string exe = ReadAll(*data / "disc" / "SLUS_010.40");
  return ExtractedData{.root = *data,
                       .entries = std::move(*entries),
                       .exe = {exe.begin(), exe.end()}};
}

// Checks every entry of `table` is the file the executable's table at
// `layout` names, at the size that table loads.
void CheckTableMatchesExecutable(const Content::AssetCatalog& catalog,
                                 std::span<const Content::AssetHandle> table,
                                 const TableLayout& layout,
                                 std::span<const std::uint8_t> exe) {
  const std::uint32_t base = GetWord(exe, 0x18);
  for (std::size_t index = 0; index < table.size(); ++index) {
    const auto offset = static_cast<std::uint32_t>(index * 4);
    const Content::AssetRecord* record = catalog.Record(table[index]);
    REQUIRE(record != nullptr);
    CHECK(record->provenance.lba ==
          GetWord(exe, 0x800 + (layout.sectors + offset - base)));
    CHECK(record->LoadBytes() ==
          GetWord(exe, 0x800 + (layout.sizes + offset - base)));
  }
}
}  // namespace

TEST_CASE("Asset ids name disc paths in lowercase with their extension",
          "[content][asset-catalog]") {
  CHECK(Content::AssetIdForDiscPath("MUSIC/MUSIC000.DAT") ==
        "asset.music.music000.dat");
  CHECK(Content::AssetIdForDiscPath("SLUS_010.40") == "asset.slus_010.40");
  CHECK_FALSE(Content::AssetIdForDiscPath("A B/C.DAT").has_value());
  CHECK_FALSE(Content::AssetIdForDiscPath("MAP//X.MPD").has_value());
  CHECK(Content::IsValidAssetId("asset.x_y-z.0"));
  for (const std::string_view id :
       {"asset.", "asset..x", "asset.x.", "Asset.x", "asset.X", "x.y"}) {
    INFO(id);
    CHECK_FALSE(Content::IsValidAssetId(id));
  }
}

TEST_CASE("Asset records load whole sectors", "[content][asset-catalog]") {
  for (const auto& [bytes, load] : std::array<std::array<std::uint32_t, 2>, 5>{
           {{0, 2048},
            {1, 2048},
            {2048, 2048},
            {2049, 4096},
            {0xFFFFF800U, 0xFFFFF800U}}}) {
    Content::AssetRecord record;
    record.bytes = bytes;
    CHECK(record.LoadBytes() == load);
  }
}

TEST_CASE("A valid asset catalog resolves ids to records",
          "[content][asset-catalog]") {
  const DataRoot root;
  const auto catalog = Parse(root, AssetsJson(), AudioJson());
  REQUIRE(catalog.has_value());
  CHECK(catalog->Size() == kFiles.size());
  const Content::AssetRecord* record =
      catalog->Record(Required(catalog->Find("asset.music.music000.dat")));
  REQUIRE(record != nullptr);
  CHECK(record->bytes == 3000);
  CHECK(record->LoadBytes() == 4096);
  CHECK(record->provenance.lba == 100);
  CHECK(record->provenance.disc_path == "MUSIC/MUSIC000.DAT");
  CHECK(catalog->FileOf(*record) ==
        root.Path() / "disc" / "MUSIC" / "MUSIC000.DAT");
}

TEST_CASE("Asset catalogs refuse unknown ids and handles they did not issue",
          "[content][asset-catalog]") {
  const DataRoot root;
  const auto catalog = Parse(root, AssetsJson(), AudioJson());
  REQUIRE(catalog.has_value());
  const Content::AssetHandle music =
      Required(catalog->Find("asset.music.music000.dat"));
  CHECK_FALSE(catalog->Find("asset.music.music002.dat").has_value());
  CHECK(catalog->Record({}) == nullptr);
  CHECK(catalog->Record({.index = music.index, .generation = 2}) == nullptr);
  CHECK(catalog->Record({.index = 99, .generation = 1}) == nullptr);
}

TEST_CASE("A valid asset catalog resolves the audio tables and menu files",
          "[content][asset-catalog]") {
  const DataRoot root;
  const auto catalog = Parse(root, AssetsJson(), AudioJson());
  REQUIRE(catalog.has_value());
  const Content::AudioCatalog& audio = catalog->Audio();
  CHECK(audio.music[0] == catalog->Find("asset.music.music000.dat"));
  CHECK(audio.music[1] == catalog->Find("asset.music.music001.dat"));
  CHECK(audio.sfx_banks[99] == catalog->Find("asset.se.sep00.dat"));
  CHECK(audio.menu.sample_banks[1] ==
        catalog->Find("asset.sound.wave0005.dat"));
  CHECK(audio.menu.effect_programs == catalog->Find("asset.se.effect00.dat"));
}

TEST_CASE("Asset catalogs reject invalid documents with the rule and pointer",
          "[content][asset-catalog]") {
  struct Case {
    std::string_view name;
    std::function<void(Json&, Json&)> edit;
    std::string_view error;
  };
  const std::vector<Case> cases = {
      {"schema name", [](Json& a, Json&) { a["schema"] = "vs.other"; },
       "assets.json: /schema: must be \"vs.assets\""},
      {"asset version", [](Json& a, Json&) { a["schema_version"] = 3; },
       "assets.json: /schema_version: unsupported version 3"},
      {"audio version", [](Json&, Json& b) { b["schema_version"] = 0; },
       "audio.json: /schema_version: unsupported version 0"},
      {"unknown key", [](Json& a, Json&) { a["extra"] = 1; },
       "assets.json: /extra: unknown key"},
      {"unknown record key", [](Json& a, Json&) { a["assets"][0]["lba"] = 1; },
       "/assets/0/lba: unknown key"},
      {"invalid id",
       [](Json& a, Json&) { a["assets"][0]["id"] = "asset.Music"; },
       "/assets/0/id: invalid asset id"},
      {"duplicate id",
       [](Json& a, Json&) { a["assets"][3]["id"] = a["assets"][1]["id"]; },
       "/assets/3/id: duplicate asset id \"asset.music.music001.dat\""},
      {"escaping path",
       [](Json& a, Json&) { a["assets"][0]["file"] = "disc/../../x.dat"; },
       "/assets/0/file: \"disc/../../x.dat\" is not"},
      {"backslash path",
       [](Json& a, Json&) {
         a["assets"][0]["file"] = "disc\\MUSIC\\MUSIC000.DAT";
       },
       "/assets/0/file:"},
      {"missing file",
       [](Json& a, Json&) { a["assets"][0]["file"] = "disc/MUSIC/NONE.DAT"; },
       "is missing or unreadable"},
      {"size mismatch", [](Json& a, Json&) { a["assets"][0]["bytes"] = 2999; },
       "holds 3000 bytes, the catalog says 2999"},
      {"negative size", [](Json& a, Json&) { a["assets"][0]["bytes"] = -1; },
       "/assets/0/bytes: must be an unsigned 32-bit integer"},
      {"unresolved reference",
       [](Json&, Json& b) { b["music"][5]["asset"] = "asset.music.none.dat"; },
       "audio.json: /music/5/asset: unresolved asset \"asset.music.none.dat\""},
      {"short table", [](Json&, Json& b) { b["sfx_banks"].erase(99); },
       "/sfx_banks: must be an array of 100 entries"},
      {"table order",
       [](Json&, Json& b) { b["wave_banks"][3]["legacyId"] = 4; },
       "/wave_banks/3/legacyId: must be 3"},
      {"missing menu file",
       [](Json&, Json& b) { b["menu"].erase("effect_programs"); },
       "/menu/effect_programs: missing key"},
      {"menu reference",
       [](Json&, Json& b) { b["menu"]["sample_banks"][1] = 7; },
       "/menu/sample_banks/1: must be an asset id"},
  };
  const DataRoot root;
  for (const Case& test : cases) {
    INFO(test.name);
    Json assets = AssetsJson();
    Json audio = AudioJson();
    test.edit(assets, audio);
    const auto parsed = Parse(root, assets, audio);
    REQUIRE_FALSE(parsed.has_value());
    CHECK(parsed.error().find(test.error) != std::string::npos);
  }
  CHECK_FALSE(
      Content::AssetCatalog::Parse("{", AudioJson().dump(), root.Path(), 1)
          .has_value());
  const auto zero = Content::AssetCatalog::Parse(
      AssetsJson().dump(), AudioJson().dump(), root.Path(), 0);
  REQUIRE_FALSE(zero.has_value());
  CHECK(zero.error().find("generation 0") != std::string::npos);
}

TEST_CASE("Data opens without a catalog but fails the startup requirement",
          "[content][asset-catalog]") {
  const Content::DataStore::ScopedOverride restore;
  const DataRoot root;
  root.WriteManifest(RawEntries());
  REQUIRE(Content::DataStore::Open(root.Path()).has_value());
  const Content::DataStore* store = Content::DataStore::Get();
  REQUIRE(store != nullptr);
  CHECK(store->Catalog() == nullptr);
  const auto required = store->RequireCatalog();
  REQUIRE_FALSE(required.has_value());
  CHECK(required.error().find("VS.Extract catalogs") != std::string::npos);
  std::array<std::uint8_t, 2048> buffer{};
  CHECK_FALSE(
      store->ReadAsset({.index = 0, .generation = 1}, buffer).has_value());
  CHECK_FALSE(Content::BeginLiveAssetRead({}, 0, buffer.data()));
}

TEST_CASE("An invalid catalog fails opening the data",
          "[content][asset-catalog]") {
  const Content::DataStore::ScopedOverride restore;
  const DataRoot root;
  root.WriteManifest(RawEntries());
  Json audio = AudioJson();
  audio["schema_version"] = 2;
  root.WriteCatalogs(AssetsJson(), audio);
  const auto opened = Content::DataStore::Open(root.Path());
  REQUIRE_FALSE(opened.has_value());
  CHECK(opened.error().find("unsupported version 2") != std::string::npos);
  CHECK(opened.error().find("re-run VS.Extract catalogs") != std::string::npos);
  root.WriteText("{}", Content::kAudioCatalogFile);
  fs::remove(root.Path() / "catalogs" / "assets.json");
  const auto missing = Content::DataStore::Open(root.Path());
  REQUIRE_FALSE(missing.has_value());
  CHECK(missing.error().find("cannot read") != std::string::npos);
}

TEST_CASE("Asset reads fill whole sectors and refuse stale or oversized reads",
          "[content][asset-catalog]") {
  const OpenCatalog data;
  const Content::AssetHandle music = OpenCatalog::Handle("MUSIC/MUSIC000.DAT");
  std::vector<std::uint8_t> buffer(4096, 0xCD);
  REQUIRE(Content::DataStore::Get()->ReadAsset(music, buffer).has_value());
  CHECK(buffer == Loaded(0));
  std::vector<std::uint8_t> oversized(4097, 0xCD);
  const auto refused = Content::DataStore::Get()->ReadAsset(music, oversized);
  REQUIRE_FALSE(refused.has_value());
  CHECK(refused.error().find("exceed") != std::string::npos);
  CHECK(std::ranges::all_of(oversized,
                            [](std::uint8_t value) { return value == 0xCD; }));
}

TEST_CASE("Asset reads refuse handles from before the data reopened",
          "[content][asset-catalog]") {
  const OpenCatalog data;
  const Content::AssetHandle music = OpenCatalog::Handle("MUSIC/MUSIC000.DAT");
  std::vector<std::uint8_t> buffer(4096, 0xCD);
  data.Reopen();
  const auto stale = Content::DataStore::Get()->ReadAsset(music, buffer);
  REQUIRE_FALSE(stale.has_value());
  CHECK(stale.error().find("was not issued") != std::string::npos);
  const Content::AssetHandle reopened =
      OpenCatalog::Handle("MUSIC/MUSIC000.DAT");
  CHECK(reopened.generation > music.generation);
  CHECK(Content::DataStore::Get()->ReadAsset(reopened, buffer).has_value());
}

TEST_CASE("The importer names every raw file and the executable's tables",
          "[content][asset-catalog][asset-import]") {
  const DataRoot root;
  std::vector<Content::DataStore::Entry> entries = RawEntries();
  entries.push_back(
      {.path = "MOV/TITLE.STR", .lba = 400, .size = 10, .form = "mkv"});
  const auto texts = Content::ImportAssetCatalogs(entries, Executable());
  REQUIRE(texts.has_value());
  const auto catalog =
      Content::AssetCatalog::Parse(texts->assets, texts->audio, root.Path(), 1);
  REQUIRE(catalog.has_value());
  CHECK(catalog->Size() == kFiles.size());
  CHECK_FALSE(catalog->Find("asset.mov.title.str").has_value());
  const Content::AudioCatalog& audio = catalog->Audio();
  CHECK(audio.music[0] == catalog->Find("asset.music.music000.dat"));
  CHECK(audio.music[199] == catalog->Find("asset.music.music001.dat"));
  CHECK(audio.sfx_banks[0] == catalog->Find("asset.se.sep00.dat"));
  CHECK(audio.wave_banks[42] == catalog->Find("asset.sound.wave0000.dat"));
  CHECK(audio.menu.effect_sample_bank ==
        catalog->Find("asset.sound.wave0200.dat"));
}

TEST_CASE("Imported audio catalogs keep one table entry per line",
          "[content][asset-catalog][asset-import]") {
  const auto texts = Content::ImportAssetCatalogs(RawEntries(), Executable());
  REQUIRE(texts.has_value());
  // A re-import then diffs by entry.
  CHECK(
      texts->audio.find(
          "\n    {\"legacyId\":0,\"asset\":\"asset.music.music000.dat\"},\n") !=
      std::string::npos);
}

TEST_CASE("The importer refuses an executable that cannot hold the tables",
          "[content][asset-catalog][asset-import]") {
  const std::vector<Content::DataStore::Entry> entries = RawEntries();
  SECTION("not an executable") {
    std::vector<std::uint8_t> exe = Executable();
    exe[0] = 'X';
    CHECK(ImportError(exe, entries).find("is not a PS-X EXE") !=
          std::string::npos);
  }
  SECTION("truncated text") {
    std::vector<std::uint8_t> exe = Executable();
    exe.resize(0x900);
    CHECK(ImportError(exe, entries).find("is truncated") != std::string::npos);
  }
  SECTION("a table outside the text") {
    std::vector<std::uint8_t> exe = Executable();
    PutWord(exe, 0x1C, 0x100);
    exe.resize(0x900);
    CHECK(ImportError(exe, entries).find("outside SLUS_010.40's image") !=
          std::string::npos);
  }
}

TEST_CASE("The importer refuses tables that do not match the extracted files",
          "[content][asset-catalog][asset-import]") {
  const std::vector<Content::DataStore::Entry> entries = RawEntries();
  SECTION("no file at the sector") {
    std::vector<std::uint8_t> exe = Executable();
    SetEntry(exe, 0, 7, 999, 2048);
    CHECK(
        ImportError(exe, entries)
            .find("_musicLBAs/_musicFileSizes[7]: no extracted file starts at "
                  "sector 999") != std::string::npos);
  }
  SECTION("a file outside the table's directory") {
    std::vector<std::uint8_t> exe = Executable();
    SetEntry(exe, 2, 2, 100, 4096);
    CHECK(ImportError(exe, entries)
              .find("MUSIC/MUSIC000.DAT is not under SOUND/") !=
          std::string::npos);
  }
  SECTION("a size that is not the file's whole sectors") {
    std::vector<std::uint8_t> exe = Executable();
    SetEntry(exe, 1, 4, 201, 4096);
    CHECK(ImportError(exe, entries)
              .find("_seLBAs/_seFileSizes[4]: the table loads "
                    "4096 bytes") != std::string::npos);
  }
  SECTION("a missing menu file") {
    std::vector<Content::DataStore::Entry> files = entries;
    std::erase_if(files, [](const Content::DataStore::Entry& entry) {
      return entry.path == "SOUND/WAVE0005.DAT";
    });
    CHECK(ImportError(Executable(), files)
              .find("SOUND/WAVE0005.DAT was not extracted") !=
          std::string::npos);
  }
}

TEST_CASE("Writing catalogs replaces them stably and the data opens with them",
          "[content][asset-catalog][asset-import]") {
  const Content::DataStore::ScopedOverride restore;
  const DataRoot root;
  const std::size_t entries = PrepareImport(root, Executable());
  const auto written = Content::WriteAssetCatalogs(root.Path());
  REQUIRE(written.has_value());
  CHECK(*written == entries);
  const fs::path assets = root.Path() / "catalogs" / "assets.json";
  const fs::path audio = root.Path() / "catalogs" / "audio.json";
  const std::string first_assets = ReadAll(assets);
  const std::string first_audio = ReadAll(audio);
  REQUIRE(Content::WriteAssetCatalogs(root.Path()).has_value());
  CHECK(ReadAll(assets) == first_assets);
  CHECK(ReadAll(audio) == first_audio);
  CHECK_FALSE(fs::exists(root.Path() / "catalogs" / "assets.json.tmp"));
  REQUIRE(Content::DataStore::Open(root.Path()).has_value());
  REQUIRE(Content::DataStore::Get()->RequireCatalog().has_value());
}

TEST_CASE("A failed import leaves the catalogs that loaded",
          "[content][asset-catalog][asset-import]") {
  const DataRoot root;
  const std::vector<std::uint8_t> exe = Executable();
  PrepareImport(root, exe);
  REQUIRE(Content::WriteAssetCatalogs(root.Path()).has_value());
  const fs::path assets = root.Path() / "catalogs" / "assets.json";
  const std::string first_assets = ReadAll(assets);
  std::vector<std::uint8_t> broken = exe;
  SetEntry(broken, 0, 0, 999, 2048);
  root.WriteFile(Content::kOriginalExecutablePath, broken);
  CHECK_FALSE(Content::WriteAssetCatalogs(root.Path()).has_value());
  CHECK(ReadAll(assets) == first_assets);
}

TEST_CASE("Queued asset reads load the asset and the slot then reads sectors",
          "[content][asset-catalog][disc-queue]") {
  const OpenCatalog data;
  const LiveQueueScope queue;
  const Content::DiscFileExtent music =
      OpenCatalog::Extent("MUSIC/MUSIC000.DAT");
  REQUIRE(music.bytes == 4096);

  std::vector<std::uint8_t> loaded(4096, 0xCD);
  Content::DiscQueueSlot* request = Content::AllocateLiveDiscRequest(music);
  REQUIRE(request != nullptr);
  // The legacy record keeps no sector; the queue keeps the asset.
  CHECK(request->file.lba == 0);
  CHECK(request->file.size == 4096);
  Content::EnqueueLiveDiscRequest(request, loaded.data());
  CHECK(RunQueue(request) == Content::DiscRequestState::kLoaded);
  CHECK(loaded == Loaded(0));
  Content::ReleaseLiveDiscRequest(request);

  // A sector read in the slot the asset used reads the sector, not the asset.
  std::vector<std::uint8_t> sector(2048, 0xCD);
  CHECK(ReadExtent(Content::DiscFileExtent{.sector = 200, .bytes = 2048},
                   sector) == Content::DiscRequestState::kLoaded);
  CHECK(sector == Loaded(2));
}

TEST_CASE("Queued reads refuse an extent naming both an asset and a sector",
          "[content][asset-catalog][disc-queue]") {
  const OpenCatalog data;
  const LiveQueueScope queue;
  std::vector<std::uint8_t> ambiguous(4096, 0xCD);
  Content::DiscFileExtent both = OpenCatalog::Extent("MUSIC/MUSIC000.DAT");
  both.sector = 100;
  CHECK(ReadExtent(both, ambiguous) == Content::DiscRequestState::kError);
  CHECK(std::ranges::all_of(ambiguous,
                            [](std::uint8_t value) { return value == 0xCD; }));
}

TEST_CASE("Asset reads from an earlier catalog fail after the data reopens",
          "[content][asset-catalog][disc-queue]") {
  const OpenCatalog data;
  const LiveQueueScope queue;
  const Content::DiscFileExtent stale = OpenCatalog::Extent("SE/SEP00.DAT");
  data.Reopen();
  std::vector<std::uint8_t> buffer(8192, 0xCD);
  Content::DiscQueueSlot* request = Content::AllocateLiveDiscRequest(stale);
  REQUIRE(request != nullptr);
  Content::EnqueueLiveDiscRequest(request, buffer.data());
  CHECK(RunQueue(request) == Content::DiscRequestState::kError);
  CHECK(std::ranges::all_of(buffer,
                            [](std::uint8_t value) { return value == 0xCD; }));
  Content::ReleaseLiveDiscRequest(request);
  Content::ClearLiveDiscReadError();
  CHECK(Content::ReadLiveDiscFileBlocking(stale, buffer.data()) == 0);
  CHECK(std::ranges::all_of(buffer,
                            [](std::uint8_t value) { return value == 0xCD; }));
  Content::ClearLiveDiscReadError();
  CHECK(Content::ReadLiveDiscFileBlocking(OpenCatalog::Extent("SE/SEP00.DAT"),
                                          buffer.data()) == 1);
  buffer.resize(6144);
  CHECK(buffer == Loaded(3));
}

TEST_CASE("Refused-queue and blocking asset reads deliver the asset",
          "[content][asset-catalog][disc-queue]") {
  const OpenCatalog data;
  const LiveQueueScope queue;
  const Content::DiscFileExtent bank = OpenCatalog::Extent("SE/SEP00.DAT");
  REQUIRE(bank.bytes == 6144);
  std::vector<std::uint8_t> refused(6144, 0xCD);
  Content::DiscQueueSlot* record =
      Content::CompleteLiveRefusedDiscRequest(bank, refused.data());
  REQUIRE(record != nullptr);
  CHECK(record->state == Content::DiscRequestState::kLoaded);
  CHECK(record->file.lba == 0);
  CHECK(refused == Loaded(3));
  Content::ReleaseLiveDiscRequest(record);

  std::vector<std::uint8_t> blocking(2048, 0xCD);
  CHECK(Content::ReadLiveDiscFileBlocking(
            OpenCatalog::Extent("SE/EFFECT00.DAT"), blocking.data()) == 1);
  CHECK(blocking == Loaded(2));
  std::vector<std::uint8_t> direct(2048, 0xCD);
  CHECK(Content::ReadLiveRefusedDiscFile(OpenCatalog::Extent("SE/EFFECT00.DAT"),
                                         direct.data()) == 1);
  CHECK(direct == Loaded(2));
}

TEST_CASE("Cancelled and reset asset reads write nothing",
          "[content][asset-catalog][disc-cancel]") {
  const OpenCatalog data;
  const LiveQueueScope queue;
  const Content::DiscFileExtent music =
      OpenCatalog::Extent("MUSIC/MUSIC001.DAT");
  for (const bool reset : {false, true}) {
    INFO((reset ? "reset" : "cancel"));
    std::vector<std::uint8_t> buffer(2048, 0xCD);
    REQUIRE(Content::BeginLiveAssetRead(music.asset, 2048, buffer.data()));
    if (reset) {
      Content::ResetLiveDiscDrive();
    } else {
      REQUIRE(Content::CancelLiveDiscWrites(Bytes(buffer)));
    }
    Content::UpdateLiveDiscCommands();
    CHECK(std::ranges::all_of(
        buffer, [](std::uint8_t value) { return value == 0xCD; }));
    CHECK(Content::LiveDiscState() == Content::DiscDriveState::kIdle);
    // A sector read after it reads the sector, not the dropped asset.
    std::vector<std::uint8_t> after(2, 0xCD);
    REQUIRE(Content::BeginLiveDiscRead(100, 2, after.data()));
    Content::UpdateLiveDiscCommands();
    CHECK(Content::LiveDiscReadCompleted(Content::LiveDiscReadGeneration()));
    CHECK(after == std::vector<std::uint8_t>{FileByte(0, 0), FileByte(0, 1)});
  }
}

TEST_CASE("Live audio files are unavailable without open data",
          "[content][asset-catalog][audio]") {
  const Content::DataStore::ScopedOverride none;
  CHECK_FALSE(VS::Audio::LiveAudioFile(VS::Audio::AudioFileTable::kMusic, 0)
                  .has_value());
  CHECK_FALSE(VS::Audio::LiveMenuSoundFile(VS::Audio::MenuSoundFile::kEffect00)
                  .has_value());
}

TEST_CASE("Live audio table files name catalog assets at their load size",
          "[content][asset-catalog][audio]") {
  using VS::Audio::AudioFileTable;
  const OpenCatalog data;
  const Content::DiscFileExtent music =
      Required(VS::Audio::LiveAudioFile(AudioFileTable::kMusic, 1));
  CHECK(music.sector == 0);
  CHECK(music.bytes == 2048);
  CHECK(music.asset == OpenCatalog::Handle("MUSIC/MUSIC001.DAT"));
  CHECK(
      Required(VS::Audio::LiveAudioFile(AudioFileTable::kSfxBanks, 99)).bytes ==
      6144);
  CHECK_FALSE(VS::Audio::LiveAudioFile(AudioFileTable::kMusic, -1).has_value());
  CHECK_FALSE(
      VS::Audio::LiveAudioFile(AudioFileTable::kMusic, 200).has_value());
  CHECK_FALSE(
      VS::Audio::LiveAudioFile(AudioFileTable::kWaveBanks, 100).has_value());
}

TEST_CASE("Live menu sound files name catalog assets at their load size",
          "[content][asset-catalog][audio]") {
  const OpenCatalog data;
  const Content::DiscFileExtent programs = Required(
      VS::Audio::LiveMenuSoundFile(VS::Audio::MenuSoundFile::kEffect00));
  CHECK(programs.asset == OpenCatalog::Handle("SE/EFFECT00.DAT"));
  CHECK(programs.bytes == 2048);
  const Content::DiscFileExtent wave = Required(
      VS::Audio::LiveMenuSoundFile(VS::Audio::MenuSoundFile::kWave0005));
  CHECK(wave.asset == OpenCatalog::Handle("SOUND/WAVE0005.DAT"));
}

TEST_CASE("Extracted catalogs match a fresh import of the user's disc",
          "[content][asset-catalog][disc]") {
  const auto data = Extracted();
  if (!data) SKIP(data.error());
  const auto texts = Content::ImportAssetCatalogs(data->entries, data->exe);
  REQUIRE(texts.has_value());
  CHECK(CatalogValues(texts->assets) ==
        CatalogValues(ReadAll(data->root / "catalogs" / "assets.json")));
  CHECK(CatalogValues(texts->audio) ==
        CatalogValues(ReadAll(data->root / "catalogs" / "audio.json")));
  const auto catalog = Content::AssetCatalog::Load(data->root, 1);
  REQUIRE(catalog.has_value());
  const auto raw = std::ranges::count(data->entries, std::string("raw"),
                                      &Content::DataStore::Entry::form);
  CHECK(catalog->Size() == static_cast<std::size_t>(raw));
}

TEST_CASE("Extracted audio tables name the files the executable's tables load",
          "[content][asset-catalog][disc]") {
  const auto data = Extracted();
  if (!data) SKIP(data.error());
  const auto catalog = Content::AssetCatalog::Load(data->root, 1);
  REQUIRE(catalog.has_value());
  const Content::AudioCatalog& audio = catalog->Audio();
  const std::array<std::span<const Content::AssetHandle>, 3> tables = {
      audio.music, audio.sfx_banks, audio.wave_banks};
  for (std::size_t table = 0; table < tables.size(); ++table) {
    CheckTableMatchesExecutable(*catalog, tables[table], kTables[table],
                                data->exe);
  }
  const Content::AssetRecord* programs =
      catalog->Record(audio.menu.effect_programs);
  REQUIRE(programs != nullptr);
  CHECK(programs->provenance.disc_path == "SE/EFFECT00.DAT");
}
