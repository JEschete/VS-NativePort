#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Content/ConvertedModels.hpp"
#include "Content/DataStore.hpp"
#include "Content/JsonText.hpp"
#include "Content/SeqFile.hpp"
#include "Content/ShpFile.hpp"
#include "Content/ShpModelFiles.hpp"
#include "Extract/ConverterBatch.hpp"
#include "Extract/EsqConversion.hpp"
#include "Extract/EtmConversion.hpp"
#include "Extract/ImageConversion.hpp"
#include "Extract/ShpModelConversion.hpp"
#include "Extract/WepModelConversion.hpp"
#include "Extract/WorkshopRecipeConversion.hpp"
#include "Extract/ZndConversion.hpp"
#include "Extract/ZudConversion.hpp"
#include "TestSupport/AudioConversionFixture.hpp"

// Shared fixtures for the converted character, weapon, animation, actor
// texture and archetype tests (ShpFileTests.cpp, WepFileTests.cpp,
// SeqFileTests.cpp, ZudFileTests.cpp).
namespace VS::test_support {

struct PolygonSpec {
  std::uint8_t corners = 3;
  std::uint8_t flags = 0;
  std::uint8_t extra = 0;
  std::uint16_t first = 0;  // the first corner's vertex
};

// A polygon over consecutive vertices from spec.first, with distinct UVs.
[[nodiscard]] inline Content::ShpPolygon Polygon(const PolygonSpec& spec) {
  Content::ShpPolygon polygon;
  polygon.corners = spec.corners;
  polygon.flags = spec.flags;
  polygon.extra = spec.extra;
  for (std::uint16_t corner = 0; corner < spec.corners; ++corner) {
    polygon.vertices.at(corner) =
        static_cast<std::uint16_t>(spec.first + corner);
    polygon.uvs.at(corner) = static_cast<std::uint16_t>(0x0101U * (corner + 1));
  }
  return polygon;
}

[[nodiscard]] std::string ErrorOf(const auto& result) {
  return result ? std::string{} : result.error();
}

// The value of a result that must succeed; the error fails the test.
[[nodiscard]] auto Required(auto result) {
  INFO(ErrorOf(result));
  REQUIRE(result.has_value());
  return *std::move(result);
}

// The little-endian 32-bit word at `at`.
[[nodiscard]] inline std::uint32_t LittleWord(std::span<const std::byte> bytes,
                                              std::size_t at) {
  std::uint32_t word = 0;
  for (std::size_t index = 0; index < 4; ++index) {
    word |= std::to_integer<std::uint32_t>(bytes[at + index]) << (8U * index);
  }
  return word;
}

// Native/data/disc/OBJ, found from the working directory up.
[[nodiscard]] inline std::optional<std::filesystem::path> ExtractedModels() {
  for (auto directory = std::filesystem::current_path();
       directory.has_parent_path() && directory != directory.parent_path();
       directory = directory.parent_path()) {
    const std::filesystem::path models =
        directory / "Native" / "data" / "disc" / "OBJ";
    if (std::filesystem::exists(models)) return models;
  }
  return std::nullopt;
}

[[nodiscard]] inline std::vector<std::byte> ReadModel(
    const std::filesystem::path& path) {
  auto bytes = Extract::ReadFileBytes(path);
  INFO(ErrorOf(bytes));
  REQUIRE(bytes.has_value());
  return *std::move(bytes);
}

using EventLengths = std::array<std::uint8_t, Content::kSeqEventLengthCount>;

// The game's event stride table, from the extracted BATTLE.PRG beside
// `models`.
[[nodiscard]] inline EventLengths ExtractedEventLengths(
    const std::filesystem::path& models) {
  const std::vector<std::byte> program =
      ReadModel(models.parent_path() / "BATTLE" / "BATTLE.PRG");
  REQUIRE(program.size() >=
          Content::kSeqEventLengthsAt + Content::kSeqEventLengthCount);
  EventLengths lengths{};
  for (std::size_t op = 0; op < lengths.size(); ++op) {
    lengths.at(op) = std::to_integer<std::uint8_t>(
        program.at(Content::kSeqEventLengthsAt + op));
  }
  return lengths;
}

// Whether the extracted model `lender` in `models` is coloured, which a model
// that borrows its texture needs to decode.
[[nodiscard]] inline bool LenderColored(const std::filesystem::path& models,
                                        std::int32_t lender) {
  const std::vector<std::byte> bytes =
      ReadModel(models / std::format("{:02X}.SHP", lender));
  const auto lent = Content::PeekShpTexture(bytes);
  INFO(ErrorOf(lent));
  REQUIRE(lent.has_value());
  REQUIRE(lent->flags.has_value());
  return lent->flags == Content::kShp4BitTexture;
}

[[nodiscard]] inline Content::JsonDocument ParseGltf(
    std::span<const std::byte> bytes) {
  auto gltf = Content::ParseJsonText(bytes, "model.gltf");
  INFO(ErrorOf(gltf));
  REQUIRE(gltf.has_value());
  return *std::move(gltf);
}

// What each model converter reported.
struct ModelReports {
  Extract::ModelConversionReport characters;
  Extract::ModelConversionReport weapons;
  Extract::ModelConversionReport animation_sets;
  Extract::ModelConversionReport actor_textures;
  Extract::ModelConversionReport archetypes;
  Extract::ZoneConversionReport zones;
};

// A temporary data root whose files.tsv lists `sources` as raw files from
// sector `first_lba` (kFirstLba unless given) on.
class ModelRoot {
 public:
  static constexpr std::uint32_t kFirstLba = 100;
  // Wide enough for a file's sector-rounded size.
  static constexpr std::uint64_t kSectorBytes =
      Content::DataStore::kDataSectorBytes;

  explicit ModelRoot(const std::vector<Extract::SourceFile>& sources,
                     std::uint32_t first_lba = kFirstLba)
      : root_(std::filesystem::temp_directory_path() /
              std::format(
                  "vs-models-{}-{}",
                  std::chrono::steady_clock::now().time_since_epoch().count(),
                  sequence++)) {
    std::filesystem::create_directories(root_ / "disc");
    std::ofstream manifest(root_ / Content::DataStore::ManifestName);
    manifest << "lba\tsize\tform\tpath\n";
    std::uint32_t lba = first_lba;
    for (const Extract::SourceFile& source : sources) {
      const std::filesystem::path file =
          root_ / "disc" / std::filesystem::path(source.path);
      std::filesystem::create_directories(file.parent_path());
      std::ofstream output(file, std::ios::binary | std::ios::trunc);
      output.write(reinterpret_cast<const char*>(source.bytes.data()),
                   static_cast<std::streamsize>(source.bytes.size()));
      REQUIRE(output.good());
      manifest << lba << '\t' << source.bytes.size() << "\traw\t" << source.path
               << '\n';
      const std::uint64_t sectors =
          (source.bytes.size() + kSectorBytes - 1) / kSectorBytes;
      reads_.push_back({.lba = lba,
                        .rounded_size = sectors * kSectorBytes,
                        .event_lengths = {}});
      lba += static_cast<std::uint32_t>(sectors);
    }
    REQUIRE(manifest.good());
  }
  // NOLINTNEXTLINE(bugprone-exception-escape): errors use the error_code API.
  ~ModelRoot() {
    std::error_code error;
    std::filesystem::remove_all(root_, error);
    if (error) {
      std::fprintf(stderr, "model fixture left files: %s\n",
                   error.message().c_str());
    }
  }
  ModelRoot(const ModelRoot&) = delete;
  ModelRoot& operator=(const ModelRoot&) = delete;
  ModelRoot(ModelRoot&&) = delete;
  ModelRoot& operator=(ModelRoot&&) = delete;

  [[nodiscard]] const std::filesystem::path& Root() const { return root_; }
  // The read of the `index`th source.
  [[nodiscard]] const Content::ConvertedModelRead& Read(
      std::size_t index) const {
    return reads_.at(index);
  }

  // Runs every required converter; the models' reports.
  [[nodiscard]] ModelReports Convert() const {
    const auto images = Extract::ConvertDiscImages(root_, false);
    INFO(ErrorOf(images));
    REQUIRE(images.has_value());
    const auto tables = Extract::ConvertDiscTables(root_, false);
    INFO(ErrorOf(tables));
    REQUIRE(tables.has_value());
    auto characters = Extract::ConvertDiscModels(root_, false);
    INFO(ErrorOf(characters));
    REQUIRE(characters.has_value());
    auto weapons = Extract::ConvertDiscWeapons(root_, false);
    INFO(ErrorOf(weapons));
    REQUIRE(weapons.has_value());
    auto sets = Extract::ConvertDiscAnimationSets(root_, false);
    INFO(ErrorOf(sets));
    REQUIRE(sets.has_value());
    auto textures = Extract::ConvertDiscActorTextures(root_, false);
    INFO(ErrorOf(textures));
    REQUIRE(textures.has_value());
    auto archetypes = Extract::ConvertDiscArchetypes(root_, false);
    INFO(ErrorOf(archetypes));
    REQUIRE(archetypes.has_value());
    // The zones name the archetypes, so they convert after them.
    auto zones = Extract::ConvertDiscZones(root_, false);
    INFO(ErrorOf(zones));
    REQUIRE(zones.has_value());
    RecordAudioConversions(root_);
    return {.characters = *std::move(characters),
            .weapons = *std::move(weapons),
            .animation_sets = *std::move(sets),
            .actor_textures = *std::move(textures),
            .archetypes = *std::move(archetypes),
            .zones = *zones};
  }

  // Applies `edit` to the converted glTF file `stem` in `directory`
  // (kShpModelDirectory or kWepModelDirectory).
  void EditGltf(std::string_view directory, std::string_view stem,
                const std::function<void(Content::JsonDocument&)>& edit) const {
    const std::filesystem::path path =
        root_ / std::filesystem::path(directory) /
        std::format("{}{}", stem, Content::kGltfSuffix);
    Content::JsonDocument gltf = ParseGltf(ReadModel(path));
    edit(gltf);
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << Content::FormatJsonText(gltf) << '\n';
    REQUIRE(output.good());
  }

 private:
  static inline unsigned sequence = 0;
  std::filesystem::path root_;
  std::vector<Content::ConvertedModelRead> reads_;
};

using ModelLoader = std::expected<std::vector<std::byte>, std::string> (*)(
    const Content::DataStore&, const Content::ConvertedModelRead&);

// What `load` gives for `read` from a store opened over `root`.
[[nodiscard]] inline std::expected<std::vector<std::byte>, std::string>
LoadModel(const std::filesystem::path& root,
          const Content::ConvertedModelRead& read, ModelLoader load) {
  Content::DataStore::ScopedOverride restore;
  if (auto opened = Content::DataStore::Open(root); !opened) {
    return std::unexpected(opened.error());
  }
  const Content::DataStore* store = Content::DataStore::Get();
  if (store == nullptr) return std::unexpected("no store is open");
  if (auto required = store->RequireConversions(); !required) {
    return std::unexpected(required.error());
  }
  return load(*store, read);
}

// Every raw file in the extracted data ending with `extension`, loaded from
// its conversion with `load` and the event stride table `event_lengths`, is
// the file's exact bytes; the count of them.
[[nodiscard]] inline std::size_t CheckExtractedModelsLoad(
    const std::filesystem::path& models, std::string_view extension,
    ModelLoader load, std::span<const std::uint8_t> event_lengths = {}) {
  Content::DataStore::ScopedOverride restore;
  const auto opened =
      Content::DataStore::Open(models.parent_path().parent_path());
  INFO(ErrorOf(opened));
  REQUIRE(opened.has_value());
  const Content::DataStore* store = Content::DataStore::Get();
  REQUIRE(store != nullptr);
  if (store == nullptr) return 0;
  std::size_t loaded = 0;
  for (const Content::DataStore::Entry& entry : store->Entries()) {
    if (entry.form != "raw" || !entry.path.ends_with(extension)) continue;
    CAPTURE(entry.path);
    const auto bytes = load(
        *store, {.lba = entry.lba,
                 .rounded_size = entry.SectorCount() * ModelRoot::kSectorBytes,
                 .event_lengths = event_lengths});
    INFO(ErrorOf(bytes));
    REQUIRE(bytes.has_value());
    CHECK(*bytes == ReadModel(store->FileOf(entry)));
    ++loaded;
  }
  return loaded;
}

}  // namespace VS::test_support
