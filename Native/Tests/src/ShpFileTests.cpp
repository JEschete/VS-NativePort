#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <ios>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Content/AnimationSet.hpp"
#include "Content/ConversionManifest.hpp"
#include "Content/ConvertedModels.hpp"
#include "Content/DataStore.hpp"
#include "Content/EsqFile.hpp"
#include "Content/ImageFiles.hpp"
#include "Content/JsonText.hpp"
#include "Content/SeqFile.hpp"
#include "Content/ShpFile.hpp"
#include "Content/ShpModelFiles.hpp"
#include "Extract/EsqConversion.hpp"
#include "Extract/ShpModelConversion.hpp"
#include "TestSupport/ModelFixtures.hpp"

namespace {

namespace fs = std::filesystem;
namespace content = VS::Content;
namespace extract = VS::Extract;
using Bytes = std::vector<std::byte>;
using VS::test_support::ErrorOf;
using VS::test_support::ExtractedModels;
using VS::test_support::LoadModel;
using VS::test_support::ModelRoot;
using VS::test_support::ParseGltf;
using VS::test_support::Polygon;
using VS::test_support::ReadModel;

constexpr std::uint8_t kRootParent = 0x2F;
constexpr std::int32_t kSampleLender = 0x4F;
constexpr std::uint16_t kSampleBones = 2;
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

// An eventless bank for the sample model's two bones: one clip whose root and
// rotations read tracks of their own.
content::SeqBank SampleBank() {
  const auto track = [](std::int16_t key) {
    return content::SeqTrack{
        .start = content::SeqStart::kVector,
        .initial = {key, 0, 0},
        .commands = {{.frames = 2, .axes = {key, std::nullopt, std::nullopt}}},
        .terminated = true};
  };
  return {.clips = {{.length = 2,
                     .base = std::nullopt,
                     .flags = 0,
                     .events = std::nullopt,
                     .root = content::SeqOwn{.element = 0},
                     .travel = std::nullopt,
                     .rotations = {content::SeqOwn{.element = 1},
                                   content::SeqOwn{.element = 2}},
                     .scales = std::nullopt}},
          .map = {0},
          .motion = {track(0), track(2), track(4)}};
}

Bytes SampleBankBytes() {
  auto bytes = content::EncodeSeqBank(SampleBank(), kSampleBones, {});
  INFO(ErrorOf(bytes));
  REQUIRE(bytes.has_value());
  return *std::move(bytes);
}

// A SEQ file of the sample bank, whose clip lasts `length` frames.
Bytes SampleSeq(std::uint16_t length = 2) {
  content::SeqFile file{.bones = kSampleBones, .bank = SampleBank()};
  file.bank.clips.front().length = length;
  auto bytes = content::EncodeSeqFile(file, {});
  INFO(ErrorOf(bytes));
  REQUIRE(bytes.has_value());
  return *std::move(bytes);
}

// Points the model's first battle sequence at `sectors` sectors from `lba`
// and its other sequence fields at none.
void NameBattleSequence(content::ShpFile& shp, std::uint32_t lba,
                        std::uint32_t sectors) {
  constexpr std::size_t kFirst = 0;
  content::JsonDocument layout = content::ShpLayoutJson(shp.layout);
  for (const std::string_view sizes :
       {"battle_sequence_sizes", "special_sequence_sizes"}) {
    for (content::JsonDocument& size : layout.at(std::string(sizes))) size = 0;
  }
  layout.at("battle_sequence_lbas").at(kFirst) = lba;
  layout.at("battle_sequence_sizes").at(kFirst) =
      sectors * ModelRoot::kSectorBytes;
  auto edited = content::ShpLayoutOf(layout);
  INFO(ErrorOf(edited));
  REQUIRE(edited.has_value());
  shp.layout = *edited;
}

// A model of every part with fields set to values the encoder must keep. No
// sequence field names a bank.
content::ShpFile SampleShp(bool colored) {
  content::ShpFile shp;
  for (std::size_t i = 0; i < shp.layout.size(); ++i) {
    shp.layout.at(i) = static_cast<std::byte>(i);
  }
  NameBattleSequence(shp, 0, 0);
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
  shp.sequence = SampleBankBytes();
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

using Tallies = std::map<std::string, std::size_t, std::less<>>;

// The layout a borrowed texture's lender sets, tallying the texture's kind.
std::optional<bool> LenderColored(const fs::path& models,
                                  const content::ShpTextureSource& source,
                                  Tallies& tallies) {
  if (!source.lender) {
    ++tallies[std::format("flags {:02X}", source.flags.value_or(0))];
    return std::nullopt;
  }
  ++tallies["borrowed"];
  return VS::test_support::LenderColored(models, *source.lender);
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
constexpr std::uint64_t kEditedRadius = 333;
constexpr std::uint64_t kPastHalfword = 0x10000;

extract::ConvertedModel Converted(const extract::ShpConversion& conversion) {
  auto converted = extract::ConvertShpModel(conversion);
  INFO(ErrorOf(converted));
  REQUIRE(converted.has_value());
  return *std::move(converted);
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
  const auto document = content::DecodeShpModelFiles(
      "4F", content::FileListReader(model.files), {});
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
    const auto model =
        Converted({.model = {.path = "OBJ/4F.SHP", .bytes = bytes},
                   .lender = {},
                   .sequences = {},
                   .events = {}});
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
  CHECK(
      model.output.dependencies.front().source_sha256 ==
      Converted({.model = lender, .lender = {}, .sequences = {}, .events = {}})
          .output.source_sha256);
  REQUIRE_FALSE(model.files.empty());
  const auto gltf = ParseGltf(model.files.front().bytes);
  CHECK(gltf["images"] ==
        content::JsonDocument::parse(R"([{"uri":"4F.png"}])"));
}

// The borrower reads back with the lender's texture size and no image.
void CheckBorrowerDocument(const extract::ConvertedModel& model,
                           const content::ShpFile& lender) {
  const auto document = content::DecodeShpModelFiles(
      "50", content::FileListReader(model.files), {});
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
  const auto model = Converted(
      {.model = borrower, .lender = lender, .sequences = {}, .events = {}});
  CheckBorrowerOutput(model, lender);
  CheckBorrowerDocument(model, lender_shp);
  const auto missing = extract::ConvertShpModel(
      {.model = borrower, .lender = {}, .sequences = {}, .events = {}});
  REQUIRE_FALSE(missing.has_value());
  CHECK(missing.error().find("OBJ/4F.SHP, which was not given") !=
        std::string::npos);
}

auto LoadShp(const fs::path& root, const content::ConvertedModelRead& read) {
  return LoadModel(root, read, content::LoadConvertedModel);
}

void SetCollisionRadius(content::JsonDocument& gltf, std::uint64_t radius) {
  gltf["asset"]["extras"]["vs"]["layout"]["collision_radius"] = radius;
}

// An edit to the model's glTF survives a rerun and changes the bytes loaded.
void CheckEditedModelLoads(const ModelRoot& root, const Bytes& original) {
  root.EditGltf(content::kShpModelDirectory, "4F",
                [](content::JsonDocument& gltf) {
                  SetCollisionRadius(gltf, kEditedRadius);
                });
  CHECK(root.Convert().characters.unchanged == 2);
  const auto edited = LoadShp(root.Root(), root.Read(0));
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
  root.EditGltf(content::kShpModelDirectory, "4F",
                [](content::JsonDocument& gltf) {
                  SetCollisionRadius(gltf, kPastHalfword);
                });
  const auto refused = LoadShp(root.Root(), root.Read(0));
  REQUIRE_FALSE(refused.has_value());
  CHECK(refused.error().find("collision_radius") != std::string::npos);
  const auto unknown =
      LoadShp(root.Root(), {.lba = 1, .rounded_size = 0, .event_lengths = {}});
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
  CHECK(root.Convert().characters.converted == 2);
  CHECK(LoadShp(root.Root(), root.Read(0)) == lender);
  CHECK(LoadShp(root.Root(), root.Read(1)) == borrower);
  CheckEditedModelLoads(root, lender);
  CheckRefusedLoads(root);
}

constexpr std::string_view kBankPath = "OBJ/4F_BT1.SEQ";

// The sample model, whose first battle sequence names the one-sector bank at
// `lba`.
Bytes OwningShp(std::uint32_t lba) {
  content::ShpFile shp = SampleShp(false);
  NameBattleSequence(shp, lba, 1);
  return Encoded(shp);
}

// The output that registers `model`'s one SEQ bank, kBankPath: its owner's
// glTF and buffer, at the hash the model output records.
void CheckBankOutput(const extract::ConvertedModel& model) {
  REQUIRE(model.companions.size() == 1);
  const content::ConversionOutput& companion = model.companions.front();
  CHECK(companion.id == "animation.obj.4f_bt1");
  CHECK(companion.source == kBankPath);
  CHECK(companion.converter == content::kSeqAnimationConverterName);
  const std::vector<std::string> files{"models/characters/4F.gltf",
                                       "models/characters/4F.bin"};
  CHECK(companion.files == files);
  const std::vector<content::SourceDependency> owner{
      {.source = "OBJ/4F.SHP", .source_sha256 = model.output.source_sha256}};
  CHECK(companion.dependencies == owner);
}

TEST_CASE("A model's glTF holds the SEQ banks it owns",
          "[content-models][shp][seq]") {
  const extract::SourceFile bank{.path = std::string(kBankPath),
                                 .bytes = SampleSeq()};
  const auto model = Converted(
      {.model = {.path = "OBJ/4F.SHP", .bytes = Encoded(SampleShp(false))},
       .lender = {},
       .sequences = {bank},
       .events = {}});
  REQUIRE(model.output.dependencies.size() == 1);
  CHECK(model.output.dependencies.front().source == kBankPath);
  CheckBankOutput(model);
  const auto document = content::DecodeShpModelFiles(
      "4F", content::FileListReader(model.files), {});
  INFO(ErrorOf(document));
  REQUIRE(document.has_value());
  REQUIRE(document->sequences.size() == 1);
  CHECK(document->sequences.front().source == kBankPath);
  const content::SeqFile sample{.bones = kSampleBones, .bank = SampleBank()};
  CHECK(document->sequences.front().file == sample);
}

auto LoadSeq(const fs::path& root, const content::ConvertedModelRead& read) {
  return LoadModel(root, read, content::LoadConvertedSequence);
}

// An edit to the bank in its owner's glTF survives a rerun and changes the
// bytes it loads as.
void CheckEditedBankLoads(const ModelRoot& root) {
  static constexpr std::size_t kFirst = 0;
  static constexpr std::uint16_t kEditedLength = 1000;
  root.EditGltf(content::kShpModelDirectory, "4F",
                [](content::JsonDocument& gltf) {
                  gltf.at("asset")
                      .at("extras")
                      .at("vs")
                      .at("sequences")
                      .at(kFirst)
                      .at("bank")
                      .at("clips")
                      .at(kFirst)
                      .at("length") = kEditedLength;
                });
  CHECK(root.Convert().characters.unchanged == 1);
  CHECK(LoadSeq(root.Root(), root.Read(0)) == SampleSeq(kEditedLength));
}

void WriteFile(const fs::path& path, std::span<const std::byte> bytes) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
  REQUIRE(output.good());
}

// Takes the bank off the disc: files.tsv stops listing it and its owner's
// field stops naming it.
void DropBank(const ModelRoot& root) {
  const fs::path manifest = root.Root() / content::DataStore::ManifestName;
  std::string kept;
  std::ifstream input(manifest);
  for (std::string line; std::getline(input, line);) {
    if (!line.contains(kBankPath)) kept += line + '\n';
  }
  input.close();
  WriteFile(manifest, std::as_bytes(std::span(kept)));
  REQUIRE(fs::remove(root.Root() / "disc" / fs::path(kBankPath)));
  WriteFile(root.Root() / "disc" / "OBJ" / "4F.SHP", Encoded(SampleShp(false)));
}

TEST_CASE("SEQ banks load from their owner's converted glTF",
          "[content-models][shp][seq]") {
  const Bytes bank = SampleSeq();
  // The bank is the first source, so it starts at kFirstLba.
  const Bytes owner = OwningShp(ModelRoot::kFirstLba);
  const ModelRoot root({{.path = std::string(kBankPath), .bytes = bank},
                        {.path = "OBJ/4F.SHP", .bytes = owner}});
  CHECK(root.Convert().characters.converted == 1);
  CHECK(LoadSeq(root.Root(), root.Read(0)) == bank);
  CHECK(LoadShp(root.Root(), root.Read(1)) == owner);
  const extract::ModelConversionReport rerun = root.Convert().characters;
  CHECK(rerun.unchanged == 1);
  CHECK(rerun.removed == 0);
  CHECK(LoadSeq(root.Root(), root.Read(0)) == bank);
  CheckEditedBankLoads(root);
  // The model's files outlive the bank output that also listed them.
  DropBank(root);
  const extract::ModelConversionReport dropped = root.Convert().characters;
  CHECK(dropped.converted == 1);
  CHECK(dropped.removed == 0);
  CHECK(LoadShp(root.Root(), root.Read(1)) == Encoded(SampleShp(false)));
}

TEST_CASE("Every SEQ bank needs the one model that loads it",
          "[content-models][shp][seq]") {
  const auto refusal = [](const std::vector<extract::SourceFile>& sources) {
    const ModelRoot root(sources);
    const auto converted = extract::ConvertDiscModels(root.Root(), false);
    REQUIRE_FALSE(converted.has_value());
    return converted.error();
  };
  const extract::SourceFile bank{.path = std::string(kBankPath),
                                 .bytes = SampleSeq()};
  const Bytes owner = OwningShp(ModelRoot::kFirstLba);
  CHECK(refusal({bank,
                 {.path = "OBJ/4F.SHP", .bytes = Encoded(SampleShp(false))}}) ==
        "OBJ/4F_BT1.SEQ is loaded by no model");
  CHECK(refusal({bank,
                 {.path = "OBJ/4F.SHP", .bytes = owner},
                 {.path = "OBJ/50.SHP", .bytes = owner}}) ==
        "OBJ/4F_BT1.SEQ is loaded by both OBJ/4F.SHP and OBJ/50.SHP");
  CHECK(refusal({{.path = "OBJ/4F.SHP", .bytes = owner}}) ==
        "OBJ/4F.SHP: battle_sequence_lbas[0] names 1 sectors at LBA 100, "
        "which is no SEQ files.tsv lists");
}

// --- ESQ shared animation sets.

constexpr std::string_view kSetPath = "OBJ/E001.ESQ";
constexpr std::string_view kSetFile = "E001.json";
// BATTLE.PRG's set tables (EsqConversion.cpp): the kind of each model number,
// then each set's first sector (from kSetFirstSector) and sector count.
constexpr std::size_t kKindTableAt = 0x80160;
constexpr std::size_t kSetSectorsAt = 0x80340;
constexpr std::uint32_t kSetFirstSector = 0x19797;
constexpr std::size_t kSampleSet = 1;
constexpr std::int32_t kSampleBorrower = 0x50;
constexpr std::uint8_t kBorrowerKind = 0x30;
constexpr std::uint8_t kLenderRow = 5;
constexpr std::uint8_t kEditedRow = 6;

content::SeqFile SetAnimation() {
  return {.bones = kSampleBones, .bank = SampleBank()};
}

// The sample bank for the borrower's kind, then for the lender's model number
// with a texture row.
content::EsqFile SampleSet() {
  return {.entries = {{.selector = kBorrowerKind,
                       .texture_row = std::nullopt,
                       .animation = SetAnimation()},
                      {.selector = static_cast<std::uint16_t>(kSampleLender),
                       .texture_row = kLenderRow,
                       .animation = SetAnimation()}}};
}

Bytes EncodedSet(const content::EsqFile& set) {
  auto bytes = content::EncodeEsqFile(set, {});
  INFO(ErrorOf(bytes));
  REQUIRE(bytes.has_value());
  return *std::move(bytes);
}

// A BATTLE.PRG of the tables alone: set kSampleSet is `sectors` sectors from
// the first set sector, and the borrower is of kBorrowerKind.
Bytes SetProgram(std::uint8_t sectors = 1) {
  Bytes program(content::kSeqEventLengthsAt + content::kSeqEventLengthCount);
  program.at(kKindTableAt + kSampleBorrower) = std::byte{kBorrowerKind};
  program.at(kSetSectorsAt + kSampleSet) = std::byte{sectors};
  return program;
}

// The set, first so that it starts at the first set sector, the lender, the
// borrower and `program`.
std::vector<extract::SourceFile> SetSources(const Bytes& set,
                                            Bytes program = SetProgram()) {
  return {{.path = std::string(kSetPath), .bytes = set},
          {.path = "OBJ/4F.SHP", .bytes = Encoded(SampleShp(false))},
          {.path = "OBJ/50.SHP", .bytes = Encoded(BorrowingShp())},
          {.path = std::string(extract::kBattleProgramPath),
           .bytes = std::move(program)}};
}

auto LoadSet(const fs::path& root, const content::ConvertedModelRead& read) {
  return LoadModel(root, read, content::LoadConvertedAnimationSet);
}

// The set's output: its JSON file, depending on BATTLE.PRG and both models.
void CheckSetOutput(const ModelRoot& root) {
  const auto manifest = content::LoadConversionManifest(root.Root());
  INFO(ErrorOf(manifest));
  if (!manifest || !*manifest) {
    FAIL("no manifest.json");
    return;
  }
  const std::vector<content::ConversionOutput>& outputs = (*manifest)->outputs;
  const auto output =
      std::ranges::find(outputs, kSetPath, &content::ConversionOutput::source);
  REQUIRE(output != outputs.end());
  CHECK(output->id == "animation-set.obj.e001");
  CHECK(output->files ==
        std::vector<std::string>{"models/animation-sets/E001.json"});
  std::vector<std::string> dependencies;
  for (const content::SourceDependency& dependency : output->dependencies) {
    dependencies.push_back(dependency.source);
  }
  CHECK(dependencies == std::vector<std::string>{"BATTLE/BATTLE.PRG",
                                                 "OBJ/4F.SHP", "OBJ/50.SHP"});
}

// Applies `edit` to the set's converted JSON file.
void EditSet(const ModelRoot& root,
             const std::function<void(content::JsonDocument&)>& edit) {
  const fs::path path =
      root.Root() / fs::path(content::kAnimationSetDirectory) / kSetFile;
  auto json = content::ParseJsonText(ReadModel(path), kSetFile);
  INFO(ErrorOf(json));
  REQUIRE(json.has_value());
  edit(*json);
  const std::string text = content::FormatJsonText(*json) + "\n";
  WriteFile(path, std::as_bytes(std::span(text)));
}

// An edit to the set's JSON survives a rerun and changes the bytes it loads
// as.
void CheckEditedSetLoads(const ModelRoot& root) {
  EditSet(root, [](content::JsonDocument& json) {
    json.at("entries").back().at("texture_row") = kEditedRow;
  });
  CHECK(root.Convert().animation_sets.unchanged == 1);
  content::EsqFile edited = SampleSet();
  edited.entries.back().texture_row = kEditedRow;
  CHECK(LoadSet(root.Root(), root.Read(0)) == EncodedSet(edited));
}

// A set the game would misread, one from another source and a read no set
// matches are refused.
void CheckRefusedSetLoads(const ModelRoot& root) {
  EditSet(root, [](content::JsonDocument& json) {
    json.at("entries").front().at("selector") = kSampleLender;
  });
  CHECK(ErrorOf(LoadSet(root.Root(), root.Read(0))).contains("repeats"));
  EditSet(root, [](content::JsonDocument& json) {
    json.at("entries").front().at("selector") = kBorrowerKind;
    json.at("source_sha256") = std::string(64, 'a');
  });
  CHECK(ErrorOf(LoadSet(root.Root(), root.Read(0))) ==
        "OBJ/E001.ESQ does not match its conversion manifest");
  CHECK(ErrorOf(LoadSet(root.Root(),
                        {.lba = 1, .rounded_size = 0, .event_lengths = {}}))
            .contains("no extracted .ESQ file matches LBA 1"));
}

TEST_CASE("ESQ sets convert to JSON that loads as their bytes, edits included",
          "[content-models][esq]") {
  const Bytes set = EncodedSet(SampleSet());
  const ModelRoot root(SetSources(set), kSetFirstSector);
  CHECK(root.Convert().animation_sets.converted == 1);
  CheckSetOutput(root);
  CHECK(LoadSet(root.Root(), root.Read(0)) == set);
  CHECK(root.Convert().animation_sets.unchanged == 1);
  CheckEditedSetLoads(root);
  CheckRefusedSetLoads(root);
}

// Why the animation set converter refuses `sources`.
std::string SetRefusal(const std::vector<extract::SourceFile>& sources) {
  const ModelRoot root(sources, kSetFirstSector);
  return ErrorOf(extract::ConvertDiscAnimationSets(root.Root(), false));
}

// The sources with the sample set changed by `change`.
std::vector<extract::SourceFile> ChangedSet(
    const std::function<void(content::EsqFile&)>& change) {
  content::EsqFile set = SampleSet();
  change(set);
  return SetSources(EncodedSet(set));
}

TEST_CASE("Every ESQ set needs its table entry and the models it binds",
          "[content-models][esq]") {
  struct Refused {
    std::vector<extract::SourceFile> sources;
    std::string_view error;
  };
  const Bytes set = EncodedSet(SampleSet());
  std::vector<extract::SourceFile> unlisted = SetSources(set);
  unlisted.pop_back();
  Bytes trailing = set;
  trailing.resize(trailing.size() + sizeof(std::uint32_t));
  const std::vector<Refused> refusals{
      {unlisted,
       "OBJ/E001.ESQ needs BATTLE/BATTLE.PRG, which files.tsv does not list"},
      {SetSources(set, Bytes(kSetSectorsAt)),
       "BATTLE/BATTLE.PRG is too short for the set tables"},
      {SetSources(set, SetProgram(2)),
       "BATTLE/BATTLE.PRG: set 1 names 2 sectors at LBA 104343, which is no "
       "ESQ files.tsv lists"},
      {SetSources(set, SetProgram(0)),
       "OBJ/E001.ESQ is named by no entry of BATTLE/BATTLE.PRG's set tables"},
      {SetSources(trailing), "OBJ/E001.ESQ: byte 0x"},
      {ChangedSet([](content::EsqFile& changed) {
         changed.entries.front().selector =
             static_cast<std::uint16_t>(kSampleLender);
       }),
       "OBJ/E001.ESQ: entry 1 repeats entry 0's selector 79"},
      {ChangedSet([](content::EsqFile& changed) {
         changed.entries.front().selector = kBorrowerKind + 1;
       }),
       "OBJ/E001.ESQ: entry 0 (selector 49) binds no model files.tsv lists"},
      {ChangedSet([](content::EsqFile& changed) {
         content::SeqFile& animation = changed.entries.front().animation;
         animation.bones = 1;
         animation.bank.clips.front().rotations.pop_back();
         animation.bank.motion.pop_back();
       }),
       "OBJ/E001.ESQ: entry 0 (selector 48) animates 1 bones, but OBJ/50.SHP "
       "has 2"},
      {ChangedSet([](content::EsqFile& changed) {
         changed.entries.front().texture_row = kLenderRow;
       }),
       "OBJ/E001.ESQ: entry 0 gives texture row 5 to selector 48, which is no "
       "model number files.tsv lists"},
  };
  for (const Refused& refused : refusals) {
    const std::string error = SetRefusal(refused.sources);
    INFO(error);
    CHECK(error.contains(refused.error));
  }
}

TEST_CASE("Extracted SHP models load from their conversions as exact bytes",
          "[content-models][shp][extract]") {
  const auto models = ExtractedModels();
  if (!models) SKIP("Extracted Native/data is unavailable");
  CHECK(VS::test_support::CheckExtractedModelsLoad(
            *models, content::kShpExtension, content::LoadConvertedModel,
            VS::test_support::ExtractedEventLengths(*models)) ==
        kExtractedModelCount);
}

}  // namespace
