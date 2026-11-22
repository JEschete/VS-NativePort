#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <map>
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
#include "Content/ImageFiles.hpp"
#include "Content/JsonText.hpp"
#include "Content/ShpFile.hpp"
#include "Content/ShpModelFiles.hpp"
#include "Extract/ConverterBatch.hpp"
#include "Extract/ImageConversion.hpp"
#include "Extract/ShpModelConversion.hpp"
#include "Extract/WorkshopRecipeConversion.hpp"

namespace {

namespace fs = std::filesystem;
namespace content = VS::Content;
namespace extract = VS::Extract;
using Bytes = std::vector<std::byte>;

constexpr std::uint8_t kRootParent = 0x2F;
constexpr std::int32_t kSampleLender = 0x4F;
constexpr std::size_t kGroupsWordAt = content::kShpModelOffset + 0x34;
constexpr std::size_t kVerticesWordAt = content::kShpModelOffset + 0x38;
constexpr std::size_t kPolygonsWordAt = content::kShpModelOffset + 0x3C;
constexpr std::size_t kVertexPadAt = 6;
constexpr std::size_t kFlatIndicesAt = 4;
constexpr std::size_t kColoredCommandAt = 11;
constexpr std::byte kBadByte{0x7F};

std::uint32_t Read32(std::span<const std::byte> bytes, std::size_t at) {
  std::uint32_t value = 0;
  std::memcpy(&value, &bytes[at], sizeof value);
  return value;
}

// A file offset from a model section offset word.
std::size_t SectionAt(std::span<const std::byte> bytes, std::size_t word_at) {
  return content::kShpModelOffset + Read32(bytes, word_at);
}

struct PolygonSpec {
  std::uint8_t corners = 3;
  std::uint8_t flags = 0;
  std::uint8_t extra = 0;
  std::uint16_t first = 0;  // the first corner's vertex
};

content::ShpPolygon Polygon(const PolygonSpec& spec) {
  content::ShpPolygon polygon;
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

// A model of every part with fields set to values the encoder must keep.
content::ShpFile SampleShp(bool colored) {
  content::ShpFile shp;
  for (std::size_t i = 0; i < shp.layout.size(); ++i) {
    shp.layout.at(i) = static_cast<std::byte>(i);
  }
  shp.limb_ranges.front() = 0x0102;
  shp.limb_ranges.back() = 0xFFFF;
  shp.bones = {{.length = 0,
                .parent = kRootParent,
                .reserved5 = 0,
                .tag = 0,
                .limb_ids = 0,
                .auxiliary_tag = 0,
                .reserved9 = 0,
                .joint_init = 0,
                .pose_lock_slot = 0,
                .reserved13 = {}},
               {.length = -50,
                .parent = 0,
                .reserved5 = 1,
                .tag = 2,
                .limb_ids = 3,
                .auxiliary_tag = 4,
                .reserved9 = 5,
                .joint_init = 0x1234,
                .pose_lock_slot = 6,
                .reserved13 = {7, 8, 9}}};
  shp.groups = {{.bone = 1, .end = 3}, {.bone = 0, .end = 6}};
  shp.vertices = {{.x = 1, .y = -2, .z = 3},   {.x = -4, .y = 5, .z = -6},
                  {.x = 7, .y = 8, .z = 9},    {.x = -10, .y = 11, .z = 12},
                  {.x = 13, .y = 14, .z = 15}, {.x = 16, .y = 17, .z = -18}};
  constexpr std::uint8_t kFlags = 0x75;
  const std::uint8_t triangle_extra = colored ? 0 : 9;
  shp.parts.at(0) = {Polygon(
      {.corners = 3, .flags = kFlags, .extra = triangle_extra, .first = 0})};
  shp.parts.at(1) = {
      Polygon({.corners = 4, .flags = 1, .extra = 2, .first = 1})};
  shp.parts.at(2) = {
      Polygon({.corners = 4, .flags = 3, .extra = 4, .first = 2}),
      Polygon({.corners = 3, .flags = 0, .extra = triangle_extra, .first = 3})};
  if (colored) {
    std::uint32_t color = 0x102030;
    for (auto& part : shp.parts) {
      for (content::ShpPolygon& polygon : part) {
        for (std::size_t corner = 0; corner < polygon.corners; ++corner) {
          polygon.colors.at(corner) = color++;
        }
      }
    }
  }
  shp.sound = {std::byte{1}, std::byte{2}, std::byte{3}};
  shp.sequence = {std::byte{0xAA}, std::byte{0xBB}, std::byte{0xCC},
                  std::byte{0xDD}};
  shp.colored = colored;
  content::ShpTexture texture{
      .flags = colored ? content::kShp4BitTexture : content::kShp8BitTexture,
      .width_byte = 2,
      .height_byte = 1,
      .palette_words = 4,
      .palettes = {},
      .pixels = {}};
  for (std::uint16_t word = 0; word < 2U * texture.palette_words; ++word) {
    texture.palettes.push_back(static_cast<std::uint16_t>(0x8000U | word));
  }
  const std::size_t pixels = std::size_t{texture.RowBytes()} * texture.Height();
  for (std::size_t pixel = 0; pixel < pixels; ++pixel) {
    texture.pixels.push_back(static_cast<std::uint8_t>(pixel));
  }
  shp.texture = std::move(texture);
  return shp;
}

content::ShpFile BorrowingShp() {
  content::ShpFile shp = SampleShp(false);
  shp.texture.reset();
  shp.lender = kSampleLender;
  return shp;
}

Bytes Encoded(const content::ShpFile& shp) {
  auto bytes = content::EncodeShp(shp);
  INFO((bytes ? std::string{} : bytes.error()));
  REQUIRE(bytes.has_value());
  return *std::move(bytes);
}

void CheckDecodeFails(std::span<const std::byte> bytes,
                      std::string_view message) {
  const auto decoded = content::DecodeShp(bytes);
  REQUIRE_FALSE(decoded.has_value());
  INFO(decoded.error());
  CHECK(decoded.error().find(message) != std::string::npos);
}

void CheckEncodeFails(const content::ShpFile& shp, std::string_view message) {
  const auto encoded = content::EncodeShp(shp);
  REQUIRE_FALSE(encoded.has_value());
  INFO(encoded.error());
  CHECK(encoded.error().find(message) != std::string::npos);
}

TEST_CASE("SHP files round-trip every field", "[content-models][shp]") {
  for (const bool colored : {false, true}) {
    CAPTURE(colored);
    const content::ShpFile shp = SampleShp(colored);
    const Bytes bytes = Encoded(shp);
    const auto decoded = content::DecodeShp(bytes);
    INFO((decoded ? std::string{} : decoded.error()));
    REQUIRE(decoded.has_value());
    CHECK(*decoded == shp);
    const auto source = content::PeekShpTexture(bytes);
    REQUIRE(source.has_value());
    CHECK(source->flags ==
          (colored ? content::kShp4BitTexture : content::kShp8BitTexture));
    CHECK_FALSE(source->lender.has_value());
  }
}

TEST_CASE("A borrowed SHP texture needs its lender's layout",
          "[content-models][shp]") {
  const content::ShpFile shp = BorrowingShp();
  const Bytes bytes = Encoded(shp);
  const auto source = content::PeekShpTexture(bytes);
  REQUIRE(source.has_value());
  CHECK(source->lender == kSampleLender);
  CHECK_FALSE(source->flags.has_value());
  CheckDecodeFails(bytes, "model 79's texture");
  const auto decoded = content::DecodeShp(bytes, false);
  REQUIRE(decoded.has_value());
  CHECK(*decoded == shp);
}

TEST_CASE("SHP decoding names the bytes it rejects", "[content-models][shp]") {
  const Bytes flat = Encoded(SampleShp(false));
  SECTION("magic") {
    Bytes bytes = flat;
    bytes.at(1) = kBadByte;
    CheckDecodeFails(bytes, "magic");
  }
  SECTION("truncated") {
    CheckDecodeFails(std::span(flat).first(flat.size() - 1), "remain");
    CheckDecodeFails(std::span(flat).first(content::kShpModelOffset - 1),
                     "SHP header");
  }
  SECTION("trailing byte") {
    Bytes bytes = flat;
    bytes.push_back(std::byte{0});
    CheckDecodeFails(bytes, "remain");
  }
  SECTION("section offsets") {
    Bytes bytes = flat;
    bytes.at(kGroupsWordAt) = kBadByte;
    CheckDecodeFails(bytes, "the groups start");
  }
  SECTION("vertex pad") {
    Bytes bytes = flat;
    bytes.at(SectionAt(flat, kVerticesWordAt) + kVertexPadAt) = std::byte{1};
    CheckDecodeFails(bytes, "pad halfword");
  }
  SECTION("polygon command") {
    Bytes bytes = flat;
    bytes.at(SectionAt(flat, kPolygonsWordAt)) = kBadByte;
    CheckDecodeFails(bytes, "flat polygon command");
  }
  SECTION("polygon index") {
    Bytes bytes = flat;
    bytes.at(SectionAt(flat, kPolygonsWordAt) + kFlatIndicesAt) = std::byte{1};
    CheckDecodeFails(bytes, "names no vertex");
  }
  SECTION("coloured command") {
    Bytes bytes = Encoded(SampleShp(true));
    bytes.at(SectionAt(bytes, kPolygonsWordAt) + kColoredCommandAt) = kBadByte;
    CheckDecodeFails(bytes, "coloured polygon command");
  }
  SECTION("group order") {
    content::ShpFile shp = SampleShp(false);
    Bytes bytes = Encoded(shp);
    const std::size_t groups = SectionAt(bytes, kGroupsWordAt);
    bytes.at(groups + 2 + 4) = std::byte{3};  // the second group's end
    CheckDecodeFails(bytes, "not after vertex 3");
  }
}

TEST_CASE("SHP encoding refuses what the file cannot hold",
          "[content-models][shp]") {
  SECTION("coloured triangle extra") {
    content::ShpFile shp = SampleShp(true);
    shp.parts.at(0).front().extra = 1;
    CheckEncodeFails(shp, "no extra byte");
  }
  SECTION("flat colour") {
    content::ShpFile shp = SampleShp(false);
    shp.parts.at(1).front().colors.front() = 1;
    CheckEncodeFails(shp, "cannot hold");
  }
  SECTION("unused corner") {
    content::ShpFile shp = SampleShp(false);
    shp.parts.at(0).front().vertices.back() = 1;
    CheckEncodeFails(shp, "cannot hold");
  }
  SECTION("wrong part") {
    content::ShpFile shp = SampleShp(false);
    shp.parts.at(0).push_back(shp.parts.at(1).front());
    CheckEncodeFails(shp, "wrong part");
  }
  SECTION("texture and lender") {
    content::ShpFile shp = SampleShp(false);
    shp.lender = kSampleLender;
    CheckEncodeFails(shp, "either its own texture or a lender");
  }
  SECTION("layout and texture flags") {
    content::ShpFile shp = SampleShp(false);
    shp.colored = true;
    CheckEncodeFails(shp, "disagrees");
  }
  SECTION("uncovered vertices") {
    content::ShpFile shp = SampleShp(false);
    shp.vertices.emplace_back();
    CheckEncodeFails(shp, "cover 6 of 7");
  }
  SECTION("pixel count") {
    content::ShpFile shp = SampleShp(false);
    if (shp.texture) shp.texture->pixels.pop_back();
    CheckEncodeFails(shp, "do not match its header");
  }
}

std::optional<fs::path> ExtractedModels() {
  for (auto directory = fs::current_path();
       directory.has_parent_path() && directory != directory.parent_path();
       directory = directory.parent_path()) {
    const fs::path models = directory / "Native" / "data" / "disc" / "OBJ";
    if (fs::exists(models)) return models;
  }
  return std::nullopt;
}

Bytes ReadModel(const fs::path& path) {
  auto bytes = VS::Extract::ReadFileBytes(path);
  REQUIRE(bytes.has_value());
  return *std::move(bytes);
}

using Tallies = std::map<std::string, std::size_t, std::less<>>;

// The layout a borrowed texture's lender sets, tallying the texture's kind.
std::optional<bool> LenderColored(const fs::path& models,
                                  const content::ShpTextureSource& source,
                                  Tallies& tallies) {
  if (!source.lender) {
    ++tallies[std::format("flags {:02X}", source.flags.value_or(0))];
    return std::nullopt;
  }
  const Bytes lender =
      ReadModel(models / std::format("{:02X}.SHP", *source.lender));
  const auto lent = content::PeekShpTexture(lender);
  REQUIRE(lent.has_value());
  REQUIRE(lent->flags.has_value());
  ++tallies["borrowed"];
  return lent->flags == content::kShp4BitTexture;
}

void CheckRebuilds(const fs::path& path, const fs::path& models,
                   Tallies& tallies) {
  CAPTURE(path.filename().string());
  const Bytes bytes = ReadModel(path);
  const auto source = content::PeekShpTexture(bytes);
  INFO((source ? std::string{} : source.error()));
  REQUIRE(source.has_value());
  const auto decoded =
      content::DecodeShp(bytes, LenderColored(models, *source, tallies));
  INFO((decoded ? std::string{} : decoded.error()));
  REQUIRE(decoded.has_value());
  const auto rebuilt = content::EncodeShp(*decoded);
  INFO((rebuilt ? std::string{} : rebuilt.error()));
  REQUIRE(rebuilt.has_value());
  CHECK(*rebuilt == bytes);
}

TEST_CASE("Extracted SHP files decode and rebuild their exact bytes",
          "[content-models][shp][extract]") {
  const auto models = ExtractedModels();
  if (!models) SKIP("Extracted Native/data is unavailable");
  Tallies tallies;
  for (const auto& entry : fs::directory_iterator(*models)) {
    if (entry.path().extension() == ".SHP") {
      CheckRebuilds(entry.path(), *models, tallies);
    }
  }
  const Tallies expected{{"borrowed", 10}, {"flags 01", 166}, {"flags 10", 26}};
  CHECK(tallies == expected);
}

constexpr std::size_t kExtractedModelCount = 202;
constexpr std::uint32_t kFirstTestLba = 100;  // ModelRoot's first file
constexpr std::uint64_t kSectorBytes = content::DataStore::kDataSectorBytes;
constexpr std::uint64_t kEditedRadius = 333;
constexpr std::uint64_t kPastHalfword = 0x10000;

std::string ErrorOf(const auto& result) {
  return result ? std::string{} : result.error();
}

extract::ConvertedModel Converted(const extract::ShpConversion& conversion) {
  auto converted = extract::ConvertShpModel(conversion);
  INFO(ErrorOf(converted));
  REQUIRE(converted.has_value());
  return *std::move(converted);
}

content::ImageFileReader ReaderOf(
    const std::vector<content::ImageFile>& files) {
  return [&files](const std::string& name) -> content::ImageFileBytes {
    const auto file = std::ranges::find(files, name, &content::ImageFile::name);
    if (file == files.end()) return std::nullopt;
    return file->bytes;
  };
}

content::JsonDocument ParseGltf(std::span<const std::byte> bytes) {
  auto gltf = content::ParseJsonText(bytes, "model.gltf");
  INFO(ErrorOf(gltf));
  REQUIRE(gltf.has_value());
  return *std::move(gltf);
}

// The manifest entry of OBJ\4F.SHP converted with its own texture.
void CheckOwnTextureOutput(const content::ConversionOutput& output) {
  CHECK(output.id == "model.obj.4f");
  CHECK(output.source == "OBJ/4F.SHP");
  CHECK(output.converter == content::kShpModelConverterName);
  CHECK(output.dependencies.empty());
  const std::vector<std::string> files{
      "models/characters/4F.gltf", "models/characters/4F.bin",
      "models/characters/4F.png", "models/characters/4F.image.json",
      "models/characters/4F.palettes.png"};
  CHECK(output.files == files);
}

// `model`'s files read back to `shp` and rebuild `bytes`.
void CheckOwnTextureFiles(const extract::ConvertedModel& model,
                          const content::ShpFile& shp, const Bytes& bytes) {
  const auto document =
      content::DecodeShpModelFiles("4F", ReaderOf(model.files));
  INFO(ErrorOf(document));
  REQUIRE(document.has_value());
  CHECK(document->shp == shp);
  CHECK(document->image_id == "image.obj.4f");
  CHECK_FALSE(document->lender_texture.has_value());
  const auto rebuilt = content::EncodeShp(document->shp);
  REQUIRE(rebuilt.has_value());
  CHECK(*rebuilt == bytes);
}

TEST_CASE("An SHP model converts to glTF files that rebuild its bytes",
          "[content-models][shp]") {
  for (const bool colored : {false, true}) {
    CAPTURE(colored);
    const content::ShpFile shp = SampleShp(colored);
    const Bytes bytes = Encoded(shp);
    const auto model = Converted(
        {.model = {.path = "OBJ/4F.SHP", .bytes = bytes}, .lender = {}});
    CheckOwnTextureOutput(model.output);
    CheckOwnTextureFiles(model, shp, bytes);
  }
}

// The borrower's files: its glTF and buffer, showing the lender's image, and
// its manifest dependency on the lender.
void CheckBorrowerOutput(const extract::ConvertedModel& model,
                         const extract::SourceFile& lender) {
  const std::vector<std::string> files{"models/characters/50.gltf",
                                       "models/characters/50.bin"};
  CHECK(model.output.files == files);
  REQUIRE(model.output.dependencies.size() == 1);
  CHECK(model.output.dependencies.front().source == lender.path);
  CHECK(model.output.dependencies.front().source_sha256 ==
        Converted({.model = lender, .lender = {}}).output.source_sha256);
  REQUIRE_FALSE(model.files.empty());
  const auto gltf = ParseGltf(model.files.front().bytes);
  CHECK(gltf["images"] ==
        content::JsonDocument::parse(R"([{"uri":"4F.png"}])"));
}

// The borrower reads back with the lender's texture size and no image.
void CheckBorrowerDocument(const extract::ConvertedModel& model,
                           const content::ShpFile& lender) {
  const auto document =
      content::DecodeShpModelFiles("50", ReaderOf(model.files));
  INFO(ErrorOf(document));
  REQUIRE(document.has_value());
  CHECK(document->shp == BorrowingShp());
  CHECK(document->image_id.empty());
  REQUIRE(lender.texture.has_value());
  if (!lender.texture) return;
  const content::ShpTextureSize lent{.width = lender.texture->Width(),
                                     .height = lender.texture->Height()};
  CHECK(document->lender_texture == lent);
}

TEST_CASE("A converted model that borrows a texture depends on its lender",
          "[content-models][shp]") {
  const content::ShpFile lender_shp = SampleShp(false);
  const extract::SourceFile lender{.path = "OBJ/4F.SHP",
                                   .bytes = Encoded(lender_shp)};
  const extract::SourceFile borrower{.path = "OBJ/50.SHP",
                                     .bytes = Encoded(BorrowingShp())};
  const auto model = Converted({.model = borrower, .lender = lender});
  CheckBorrowerOutput(model, lender);
  CheckBorrowerDocument(model, lender_shp);
  const auto missing =
      extract::ConvertShpModel({.model = borrower, .lender = {}});
  REQUIRE_FALSE(missing.has_value());
  CHECK(missing.error().find("OBJ/4F.SHP, which was not given") !=
        std::string::npos);
}

// A temporary data root whose files.tsv lists `sources` as raw files from
// sector kFirstTestLba on.
class ModelRoot {
 public:
  explicit ModelRoot(const std::vector<extract::SourceFile>& sources)
      : root_(fs::temp_directory_path() /
              std::format(
                  "vs-shp-models-{}-{}",
                  std::chrono::steady_clock::now().time_since_epoch().count(),
                  sequence++)) {
    fs::create_directories(root_ / "disc");
    std::ofstream manifest(root_ / content::DataStore::ManifestName);
    manifest << "lba\tsize\tform\tpath\n";
    std::uint32_t lba = kFirstTestLba;
    for (const extract::SourceFile& source : sources) {
      const fs::path file = root_ / "disc" / fs::path(source.path);
      fs::create_directories(file.parent_path());
      std::ofstream output(file, std::ios::binary | std::ios::trunc);
      output.write(reinterpret_cast<const char*>(source.bytes.data()),
                   static_cast<std::streamsize>(source.bytes.size()));
      REQUIRE(output.good());
      manifest << lba << '\t' << source.bytes.size() << "\traw\t" << source.path
               << '\n';
      const std::uint64_t sectors =
          (source.bytes.size() + kSectorBytes - 1) / kSectorBytes;
      reads_.push_back({.lba = lba, .rounded_size = sectors * kSectorBytes});
      lba += static_cast<std::uint32_t>(sectors);
    }
    REQUIRE(manifest.good());
  }
  // NOLINTNEXTLINE(bugprone-exception-escape): errors use the error_code API.
  ~ModelRoot() {
    std::error_code error;
    fs::remove_all(root_, error);
    if (error) {
      std::fprintf(stderr, "SHP model fixture left files: %s\n",
                   error.message().c_str());
    }
  }
  ModelRoot(const ModelRoot&) = delete;
  ModelRoot& operator=(const ModelRoot&) = delete;
  ModelRoot(ModelRoot&&) = delete;
  ModelRoot& operator=(ModelRoot&&) = delete;

  [[nodiscard]] const fs::path& Root() const { return root_; }
  // The read of the `index`th source.
  [[nodiscard]] const content::ConvertedModelRead& Read(
      std::size_t index) const {
    return reads_.at(index);
  }

  // Runs every required converter; the models' report.
  [[nodiscard]] extract::ModelConversionReport Convert() const {
    const auto images = extract::ConvertDiscImages(root_, false);
    INFO(ErrorOf(images));
    REQUIRE(images.has_value());
    const auto tables = extract::ConvertDiscTables(root_, false);
    INFO(ErrorOf(tables));
    REQUIRE(tables.has_value());
    auto models = extract::ConvertDiscModels(root_, false);
    INFO(ErrorOf(models));
    REQUIRE(models.has_value());
    return *std::move(models);
  }

  // Applies `edit` to `stem`'s converted glTF file.
  void EditGltf(std::string_view stem,
                const std::function<void(content::JsonDocument&)>& edit) const {
    const fs::path path = root_ / "models" / "characters" /
                          std::format("{}{}", stem, content::kGltfSuffix);
    content::JsonDocument gltf = ParseGltf(ReadModel(path));
    edit(gltf);
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << content::FormatJsonText(gltf) << '\n';
    REQUIRE(output.good());
  }

 private:
  static inline unsigned sequence = 0;
  fs::path root_;
  std::vector<content::ConvertedModelRead> reads_;
};

// The model `read` names, loaded from a store opened over `root`.
std::expected<Bytes, std::string> LoadModel(
    const fs::path& root, const content::ConvertedModelRead& read) {
  content::DataStore::ScopedOverride restore;
  if (auto opened = content::DataStore::Open(root); !opened) {
    return std::unexpected(opened.error());
  }
  const content::DataStore* store = content::DataStore::Get();
  if (store == nullptr) return std::unexpected("no store is open");
  if (auto required = store->RequireConversions(); !required) {
    return std::unexpected(required.error());
  }
  return content::LoadConvertedModel(*store, read);
}

void SetCollisionRadius(content::JsonDocument& gltf, std::uint64_t radius) {
  gltf["asset"]["extras"]["vs"]["layout"]["collision_radius"] = radius;
}

// An edit to the model's glTF survives a rerun and changes the bytes loaded.
void CheckEditedModelLoads(const ModelRoot& root, const Bytes& original) {
  root.EditGltf("4F", [](content::JsonDocument& gltf) {
    SetCollisionRadius(gltf, kEditedRadius);
  });
  CHECK(root.Convert().unchanged == 2);
  const auto edited = LoadModel(root.Root(), root.Read(0));
  INFO(ErrorOf(edited));
  REQUIRE(edited.has_value());
  CHECK(*edited != original);
  const auto shp = content::DecodeShp(*edited);
  INFO(ErrorOf(shp));
  REQUIRE(shp.has_value());
  CHECK(content::ShpLayoutJson(shp->layout)["collision_radius"] ==
        kEditedRadius);
}

// A value the file cannot hold, and a read no file matches, are refused.
void CheckRefusedLoads(const ModelRoot& root) {
  root.EditGltf("4F", [](content::JsonDocument& gltf) {
    SetCollisionRadius(gltf, kPastHalfword);
  });
  const auto refused = LoadModel(root.Root(), root.Read(0));
  REQUIRE_FALSE(refused.has_value());
  CHECK(refused.error().find("collision_radius") != std::string::npos);
  const auto unknown = LoadModel(root.Root(), {.lba = 1, .rounded_size = 0});
  REQUIRE_FALSE(unknown.has_value());
  CHECK(unknown.error().find("no extracted .SHP file matches LBA 1") !=
        std::string::npos);
}

TEST_CASE("Converted models load in place of their SHP files, edits included",
          "[content-models][shp]") {
  const Bytes lender = Encoded(SampleShp(false));
  const Bytes borrower = Encoded(BorrowingShp());
  const ModelRoot root({{.path = "OBJ/4F.SHP", .bytes = lender},
                        {.path = "OBJ/50.SHP", .bytes = borrower}});
  CHECK(root.Convert().converted == 2);
  CHECK(LoadModel(root.Root(), root.Read(0)) == lender);
  CHECK(LoadModel(root.Root(), root.Read(1)) == borrower);
  CheckEditedModelLoads(root, lender);
  CheckRefusedLoads(root);
}

TEST_CASE("Extracted SHP models load from their conversions as exact bytes",
          "[content-models][shp][extract]") {
  const auto models = ExtractedModels();
  if (!models) SKIP("Extracted Native/data is unavailable");
  content::DataStore::ScopedOverride restore;
  const auto opened =
      content::DataStore::Open(models->parent_path().parent_path());
  INFO(ErrorOf(opened));
  REQUIRE(opened.has_value());
  const content::DataStore* store = content::DataStore::Get();
  REQUIRE(store != nullptr);
  if (store == nullptr) return;
  std::size_t loaded = 0;
  for (const content::DataStore::Entry& entry : store->Entries()) {
    if (entry.form != "raw" || !entry.path.ends_with(content::kShpExtension)) {
      continue;
    }
    CAPTURE(entry.path);
    const auto bytes = content::LoadConvertedModel(
        *store,
        {.lba = entry.lba, .rounded_size = entry.SectorCount() * kSectorBytes});
    INFO(ErrorOf(bytes));
    REQUIRE(bytes.has_value());
    CHECK(*bytes == ReadModel(store->FileOf(entry)));
    ++loaded;
  }
  CHECK(loaded == kExtractedModelCount);
}

}  // namespace
