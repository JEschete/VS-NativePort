#include <algorithm>
#include <array>
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

#include "Content/ActorArchetype.hpp"
#include "Content/ActorAssetsLive.hpp"
#include "Content/AnimationSet.hpp"
#include "Content/CdDrive.hpp"
#include "Content/ConversionManifest.hpp"
#include "Content/ConvertedModels.hpp"
#include "Content/DataStore.hpp"
#include "Content/EsqFile.hpp"
#include "Content/EtmFile.hpp"
#include "Content/ImageFiles.hpp"
#include "Content/JsonText.hpp"
#include "Content/NativeImage.hpp"
#include "Content/SeqFile.hpp"
#include "Content/ShpFile.hpp"
#include "Content/ShpModelFiles.hpp"
#include "Content/ZudFile.hpp"
#include "Extract/ConversionManifestWriter.hpp"
#include "Extract/EsqConversion.hpp"
#include "Extract/EtmConversion.hpp"
#include "Extract/ImageFilesWriter.hpp"
#include "Extract/ModelAuthoring.hpp"
#include "Extract/ShpModelConversion.hpp"
#include "Extract/ZudConversion.hpp"
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
  // Body part 0 holds the triangle and body part 7 both styled polygons.
  shp.limb_ranges.front() = 1;
  shp.limb_ranges.back() = 2;
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
  // A split quad uncoloured; a coloured quad cannot split, so it draws both
  // sides at depth offset 7, adding.
  const std::uint8_t styled_quad_flags = colored ? 0x3D : 3;
  const std::uint8_t triangle_extra = colored ? 0 : 9;
  shp.parts.at(0) = {Polygon(
      {.corners = 3, .flags = kFlags, .extra = triangle_extra, .first = 0})};
  shp.parts.at(1) = {
      Polygon({.corners = 4, .flags = 1, .extra = 2, .first = 1})};
  shp.parts.at(2) = {
      Polygon(
          {.corners = 4, .flags = styled_quad_flags, .extra = 4, .first = 2}),
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
                   .events = {},
                   .sequence_bank_ids = {}});
    CheckOwnTextureOutput(model.output);
    CheckOwnTextureFiles(model, shp, bytes);
  }
}

// Two triangles, a quad and three styled polygons.
constexpr content::ShpPolygonCounts kEntryCounts{2, 1, 3};

struct EntryForm {
  content::ShpPolygonEntry entry;
  bool colored = false;
  std::string_view json;
};

TEST_CASE("Polygon entries name the style the game draws each with",
          "[content-models][shp]") {
  const std::array forms{
      EntryForm{.entry = {.order = 0, .corners = 3, .flags = 4, .extra = 0},
                .colored = false,
                .json = R"({"order":0,"corners":3})"},
      EntryForm{
          .entry = {.order = 2, .corners = 4, .flags = 0x75, .extra = 9},
          .colored = false,
          .json =
              R"({"order":2,"corners":4,"unread":{"flags":117,"extra":9}})"},
      EntryForm{.entry = {.order = 3, .corners = 3, .flags = 0, .extra = 0},
                .colored = false,
                .json = R"({"order":3,"corners":3,"style":{"sides":"one",
                  "depth_offset":-5,"semi_transparent":false,
                  "blend":"average"}})"},
      EntryForm{.entry = {.order = 4, .corners = 4, .flags = 0x3D, .extra = 4},
                .colored = true,
                .json = R"({"order":4,"corners":4,"style":{"sides":"both",
                  "depth_offset":7,"semi_transparent":true,"blend":"add"},
                  "unread":{"extra":4}})"},
      EntryForm{.entry = {.order = 5, .corners = 4, .flags = 0x5B, .extra = 0},
                .colored = false,
                .json = R"({"order":5,"corners":4,"style":{"sides":"split",
                  "semi_transparent":true,"blend":"subtract"},
                  "unread":{"depth_bits":2}})"},
  };
  for (const EntryForm& form : forms) {
    CAPTURE(form.json);
    const content::JsonDocument expected =
        content::JsonDocument::parse(form.json);
    const auto json =
        content::ShpPolygonEntryJson(form.entry, kEntryCounts, form.colored);
    INFO(ErrorOf(json));
    REQUIRE(json.has_value());
    CHECK(*json == expected);
    const auto read =
        content::ShpPolygonEntryOf(expected, "e", kEntryCounts, form.colored);
    INFO(ErrorOf(read));
    REQUIRE(read.has_value());
    CHECK(*read == form.entry);
  }
  CHECK(content::ShpMaterialName(0x00) == "one_sided_opaque");
  CHECK(content::ShpMaterialName(0x20) == "one_sided_opaque_add");
  CHECK(content::ShpMaterialName(0x31) == "double_sided_add");
}

struct RefusedEntry {
  content::ShpPolygonEntry entry;
  bool colored = false;
  std::string_view message;
};

TEST_CASE("Polygon entries refuse flags no style holds",
          "[content-models][shp]") {
  constexpr std::string_view kNoStyle = "which no style holds";
  const std::array refused{
      RefusedEntry{.entry = {.order = 3, .corners = 3, .flags = 0x80},
                   .colored = false,
                   .message = kNoStyle},
      RefusedEntry{.entry = {.order = 3, .corners = 3, .flags = 0x02},
                   .colored = false,
                   .message = kNoStyle},
      RefusedEntry{.entry = {.order = 3, .corners = 3, .flags = 0x03},
                   .colored = false,
                   .message = kNoStyle},
      RefusedEntry{.entry = {.order = 4, .corners = 4, .flags = 0x03},
                   .colored = true,
                   .message = kNoStyle},
      RefusedEntry{.entry = {.order = 6, .corners = 3},
                   .colored = false,
                   .message = "past the 6 polygons"},
      RefusedEntry{.entry = {.order = 0, .corners = 4},
                   .colored = false,
                   .message = "has 4 corners"},
  };
  for (const RefusedEntry& entry : refused) {
    CAPTURE(entry.entry.order, entry.entry.flags);
    const auto json =
        content::ShpPolygonEntryJson(entry.entry, kEntryCounts, entry.colored);
    REQUIRE_FALSE(json.has_value());
    CHECK(json.error().contains(entry.message));
  }
}

struct RefusedForm {
  std::string_view json;
  bool colored = false;
  std::string_view message;
};

TEST_CASE("Named polygon entries refuse a style their polygon cannot have",
          "[content-models][shp]") {
  const std::array refused{
      RefusedForm{.json = R"({"order":0,"corners":3,"style":{"sides":"one",
                    "depth_offset":-5,"semi_transparent":false,
                    "blend":"average"}})",
                  .colored = false,
                  .message = "e.style must be left out"},
      RefusedForm{.json = R"({"order":3,"corners":3})",
                  .colored = false,
                  .message = "e.style is missing"},
      RefusedForm{.json = R"({"order":5,"corners":4,"style":{"sides":"split",
                    "depth_offset":-1,"semi_transparent":false,
                    "blend":"average"}})",
                  .colored = false,
                  .message = "depth_offset must be left out"},
      RefusedForm{.json = R"({"order":5,"corners":4,"style":{"sides":"split",
                    "semi_transparent":false,"blend":"average"}})",
                  .colored = true,
                  .message = "only a quad of an uncoloured model"},
      RefusedForm{.json = R"({"order":3,"corners":3,"style":{"sides":"one",
                    "depth_offset":0,"semi_transparent":false,
                    "blend":"average"}})",
                  .colored = false,
                  .message = "must be -5, -1, 3 or 7"},
      RefusedForm{.json = R"({"order":3,"corners":3,"style":{"sides":"one",
                    "depth_offset":-5,"semi_transparent":false,
                    "blend":"mix"}})",
                  .colored = false,
                  .message = R"(must be "average")"},
      RefusedForm{.json = R"({"order":3,"corners":3,"style":{"sides":"one",
                    "depth_offset":-5,"semi_transparent":false,
                    "blend":"average"},"unread":{"flags":4}})",
                  .colored = false,
                  .message = "unknown member 'flags'"},
      RefusedForm{.json = R"({"order":0,"corners":3,
                    "unread":{"depth_bits":1}})",
                  .colored = false,
                  .message = "unknown member 'depth_bits'"},
  };
  for (const RefusedForm& form : refused) {
    CAPTURE(form.json);
    const auto read =
        content::ShpPolygonEntryOf(content::JsonDocument::parse(form.json), "e",
                                   kEntryCounts, form.colored);
    REQUIRE_FALSE(read.has_value());
    CHECK(read.error().contains(form.message));
  }
}

// A model header of kEntryCounts, the body parts `body_parts` and the
// sample's bones.
content::JsonDocument HeaderPayload(std::string_view body_parts) {
  content::JsonDocument payload = content::JsonDocument::object();
  payload["polygon_counts"] = content::ShpPolygonCountsJson(kEntryCounts);
  payload["body_part_polygons"] = content::JsonDocument::parse(body_parts);
  payload["bones"] = content::ShpBonesJson(SampleShp(false));
  return payload;
}

TEST_CASE("Body parts list their polygons once each, within the counts",
          "[content-models][shp]") {
  content::ShpLimbRanges ranges{};
  ranges.at(0) = 2;  // both triangles in body part 0
  ranges.at((2 * content::kShpBodyPartCount) + 7) = 1;  // a styled one in 7
  const content::JsonDocument listed = content::ShpBodyPartPolygonsJson(ranges);
  CHECK(listed == content::JsonDocument::parse(
                      R"([{"body_part":0,"triangles":2,"quads":0,"styled":0},
                          {"body_part":7,"triangles":0,"quads":0,
                           "styled":1}])"));
  content::ShpModel model;
  const auto counts = content::ReadShpModelHeader(
      content::JsonMembers(HeaderPayload(listed.dump()), "vs"), model);
  INFO(ErrorOf(counts));
  REQUIRE(counts.has_value());
  CHECK(*counts == kEntryCounts);
  CHECK(model.limb_ranges == ranges);

  const std::array<std::pair<std::string_view, std::string_view>, 3> refused{{
      {R"([{"body_part":0,"triangles":2,"quads":0,"styled":0},
           {"body_part":1,"triangles":1,"quads":0,"styled":0}])",
       "gives the body parts 3 triangles; polygon_counts.triangles is 2"},
      {R"([{"body_part":1,"triangles":0,"quads":1,"styled":0},
           {"body_part":1,"triangles":0,"quads":0,"styled":1}])",
       "must be past the body part before it"},
      {R"([{"body_part":8,"triangles":0,"quads":0,"styled":0}])", "body_part"},
  }};
  for (const auto& [body_parts, message] : refused) {
    CAPTURE(body_parts);
    const auto read = content::ReadShpModelHeader(
        content::JsonMembers(HeaderPayload(body_parts), "vs"), model);
    REQUIRE_FALSE(read.has_value());
    CHECK(read.error().contains(message));
  }
}

// `entries`, a primitive's polygons, as shp-gltf 3 numbered them:
// [order, corners, flags, extra].
content::JsonDocument NumberedEntries(const content::JsonDocument& entries,
                                      const content::ShpPolygonCounts& counts,
                                      bool colored) {
  content::JsonDocument numbered = content::JsonDocument::array();
  for (const content::JsonDocument& json : entries) {
    const auto entry = VS::test_support::Required(
        content::ShpPolygonEntryOf(json, "e", counts, colored));
    numbered.push_back(content::JsonDocument::array(
        {entry.order, entry.corners, entry.flags, entry.extra}));
  }
  return numbered;
}

// `gltf` as shp-gltf 3 wrote it: numbered polygon entries, polygon counts and
// limb ranges, and materials named flags_<hex> by their primitive flags.
content::JsonDocument NumberedModel(content::JsonDocument gltf) {
  content::JsonDocument& payload = gltf["asset"]["extras"]["vs"];
  content::ShpModel model;
  const auto counts = VS::test_support::Required(
      content::ReadShpModelHeader(content::JsonMembers(payload, "vs"), model));
  const bool colored = payload["colored"].get<bool>();
  content::JsonDocument numbered = content::JsonDocument::object();
  for (const auto& [key, value] : payload.items()) {
    numbered[key == "body_part_polygons" ? "limb_ranges" : key] = value;
  }
  numbered["schema_version"] = extract::kNumberedPolygonShpVersion;
  numbered.erase("sequence_bank_ids");
  if (numbered.contains("sequences") && numbered["sequences"].is_array()) {
    for (content::JsonDocument& bank : numbered["sequences"]) {
      if (bank.is_object() && bank.contains("animation")) {
        bank = std::move(bank["animation"]);
      }
    }
  }
  numbered["polygon_counts"] = counts;
  numbered["limb_ranges"] = model.limb_ranges;
  payload = std::move(numbered);
  for (content::JsonDocument& mesh : gltf["meshes"]) {
    for (content::JsonDocument& primitive : mesh["primitives"]) {
      content::JsonDocument& polygons = primitive["extras"]["vs"]["polygons"];
      polygons = NumberedEntries(polygons, counts, colored);
    }
  }
  std::map<std::string, std::string, std::less<>> legacy;
  for (unsigned flags = 0; flags <= content::kShpPrimitiveFlags; ++flags) {
    if ((flags & ~unsigned{content::kShpPrimitiveFlags}) != 0) continue;
    legacy[content::ShpMaterialName(static_cast<std::uint8_t>(flags))] =
        std::format("flags_{:02x}", flags);
  }
  for (content::JsonDocument& material : gltf["materials"]) {
    material["name"] = legacy.at(material["name"].get<std::string>());
  }
  return gltf;
}

fs::path SampleGltf(const ModelRoot& root) {
  return root.Root() / fs::path(content::kShpModelDirectory) / "4F.gltf";
}

// Records shp-gltf at `version` in `root`'s manifest.
void RecordShpVersion(const fs::path& root, std::uint32_t version) {
  auto manifest =
      VS::test_support::Required(content::LoadConversionManifest(root));
  REQUIRE(manifest.has_value());
  if (!manifest) return;
  manifest->converters[std::string(content::kShpModelConverterName)] = version;
  REQUIRE(extract::WriteConversionManifest(root, *manifest).has_value());
}

std::optional<std::uint32_t> RecordedShpVersion(const fs::path& root) {
  const auto manifest =
      VS::test_support::Required(content::LoadConversionManifest(root));
  if (!manifest) return std::nullopt;
  const auto found = manifest->converters.find(content::kShpModelConverterName);
  if (found == manifest->converters.end()) return std::nullopt;
  return found->second;
}

// Upgrades `root`'s numbered sample model, which reads as `named` once named.
void CheckUpgradedModel(const ModelRoot& root,
                        const content::JsonDocument& named) {
  const auto report =
      VS::test_support::Required(extract::UpgradeModelAuthoring(root.Root()));
  CHECK(report.models == 1);
  CHECK(report.polygons == 4);
  CHECK(report.written == 2);
  CHECK(ParseGltf(ReadModel(SampleGltf(root))) == named);
  CHECK(RecordedShpVersion(root.Root()) == content::kShpModelConverterVersion);
}

TEST_CASE("Authoring names existing polygon entries in place",
          "[content-models][shp]") {
  for (const bool colored : {false, true}) {
    CAPTURE(colored);
    const Bytes bytes = Encoded(SampleShp(colored));
    const ModelRoot root({{.path = "OBJ/4F.SHP", .bytes = bytes}});
    CHECK(root.Convert().characters.converted == 1);
    const content::JsonDocument named = ParseGltf(ReadModel(SampleGltf(root)));
    root.EditGltf(content::kShpModelDirectory, "4F",
                  [](content::JsonDocument& gltf) {
                    gltf = NumberedModel(std::move(gltf));
                  });
    RecordShpVersion(root.Root(), extract::kNumberedPolygonShpVersion);

    CheckUpgradedModel(root, named);
    CHECK(
        VS::test_support::Required(extract::UpgradeModelAuthoring(root.Root()))
            .written == 0);
    CHECK(root.Convert().characters.unchanged == 1);
    CHECK(VS::test_support::Required(LoadModel(root.Root(), root.Read(0),
                                               content::LoadConvertedModel)) ==
          bytes);
  }
}

TEST_CASE("Authoring leaves a model whose polygons no style holds",
          "[content-models][shp]") {
  const ModelRoot root(
      {{.path = "OBJ/4F.SHP", .bytes = Encoded(SampleShp(false))}});
  static_cast<void>(root.Convert());
  constexpr std::size_t kStyledTriangle = 3;
  constexpr std::size_t kFlagsAt = 2;
  root.EditGltf(content::kShpModelDirectory, "4F",
                [](content::JsonDocument& gltf) {
                  gltf = NumberedModel(std::move(gltf));
                  for (auto& primitive : gltf["meshes"][0]["primitives"]) {
                    for (auto& entry : primitive["extras"]["vs"]["polygons"]) {
                      if (entry[0] == kStyledTriangle) entry[kFlagsAt] = 0x80;
                    }
                  }
                });
  RecordShpVersion(root.Root(), extract::kNumberedPolygonShpVersion);
  const content::JsonDocument before = ParseGltf(ReadModel(SampleGltf(root)));

  const auto refused = extract::UpgradeModelAuthoring(root.Root());
  REQUIRE_FALSE(refused.has_value());
  CHECK(refused.error().contains("polygon 3 has flags 0x80"));
  CHECK(ParseGltf(ReadModel(SampleGltf(root))) == before);
  CHECK(RecordedShpVersion(root.Root()) == extract::kNumberedPolygonShpVersion);
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
        Converted({.model = lender,
                   .lender = {},
                   .sequences = {},
                   .events = {},
                   .sequence_bank_ids = {}})
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
  const auto model = Converted({.model = borrower,
                                .lender = lender,
                                .sequences = {},
                                .events = {},
                                .sequence_bank_ids = {}});
  CheckBorrowerOutput(model, lender);
  CheckBorrowerDocument(model, lender_shp);
  const auto missing = extract::ConvertShpModel({.model = borrower,
                                                 .lender = {},
                                                 .sequences = {},
                                                 .events = {},
                                                 .sequence_bank_ids = {}});
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
       .events = {},
       .sequence_bank_ids = {}});
  REQUIRE(model.output.dependencies.size() == 1);
  CHECK(model.output.dependencies.front().source == kBankPath);
  CheckBankOutput(model);
  const auto document = content::DecodeShpModelFiles(
      "4F", content::FileListReader(model.files), {});
  INFO(ErrorOf(document));
  REQUIRE(document.has_value());
  REQUIRE(document->sequences.size() == 1);
  CHECK(document->sequences.front().animation.source == kBankPath);
  const content::SeqFile sample{.bones = kSampleBones, .bank = SampleBank()};
  CHECK(document->sequences.front().animation.file == sample);
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
                      .at("animation")
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
  const auto gltf = ParseGltf(ReadModel(SampleGltf(root)));
  CHECK(gltf["asset"]["extras"]["vs"]["sequence_bank_ids"]["battle"][0] ==
        "animation.obj.4f_bt1");
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

TEST_CASE("SHP layout references must resolve to owned sequence banks",
          "[content-models][shp][seq]") {
  const ModelRoot root(
      {{.path = std::string(kBankPath), .bytes = SampleSeq()},
       {.path = "OBJ/4F.SHP", .bytes = OwningShp(ModelRoot::kFirstLba)}});
  CHECK(root.Convert().characters.converted == 1);
  root.EditGltf(
      content::kShpModelDirectory, "4F", [](content::JsonDocument& gltf) {
        gltf["asset"]["extras"]["vs"]["sequence_bank_ids"]["battle"][0] =
            "animation.obj.missing";
      });

  const auto loaded = LoadShp(root.Root(), root.Read(1));
  REQUIRE_FALSE(loaded.has_value());
  CHECK(loaded.error().contains("references missing sequence bank"));
}

TEST_CASE("Authoring adds stable sequence-bank IDs to existing model files",
          "[content-models][shp][seq]") {
  const Bytes bank = SampleSeq();
  const ModelRoot root(
      {{.path = std::string(kBankPath), .bytes = bank},
       {.path = "OBJ/4F.SHP", .bytes = OwningShp(ModelRoot::kFirstLba)}});
  CHECK(root.Convert().characters.converted == 1);
  content::JsonDocument expected = ParseGltf(ReadModel(SampleGltf(root)));
  auto& payload = expected["asset"]["extras"]["vs"];
  payload["sequences"][0]["animation"]["bank"]["clips"][0]["length"] = 1000;

  root.EditGltf(
      content::kShpModelDirectory, "4F", [](content::JsonDocument& gltf) {
        auto& legacy = gltf["asset"]["extras"]["vs"];
        legacy["sequences"][0]["animation"]["bank"]["clips"][0]["length"] =
            1000;
        legacy["schema_version"] = content::kShpModelSchemaVersion - 1;
        content::JsonDocument sequences = content::JsonDocument::array();
        for (auto& bank : legacy["sequences"]) {
          sequences.push_back(std::move(bank["animation"]));
        }
        legacy["sequences"] = std::move(sequences);
        legacy.erase("sequence_bank_ids");
      });
  RecordShpVersion(root.Root(), content::kShpModelConverterVersion - 1);

  const auto upgraded = extract::UpgradeModelAuthoring(root.Root());
  INFO(ErrorOf(upgraded));
  REQUIRE(upgraded.has_value());
  CHECK(upgraded->models == 1);
  CHECK(upgraded->polygons == 0);
  CHECK(upgraded->written == 2);
  CHECK(ParseGltf(ReadModel(SampleGltf(root))) == expected);
  CHECK(RecordedShpVersion(root.Root()) == content::kShpModelConverterVersion);
  const auto rerun = extract::UpgradeModelAuthoring(root.Root());
  INFO(ErrorOf(rerun));
  REQUIRE(rerun.has_value());
  CHECK(rerun->models == 0);
  CHECK(rerun->written == 0);
  CheckEditedBankLoads(root);
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
       "BATTLE/BATTLE.PRG is too short for its file tables"},
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

// --- ETM actor effect texture sheets.

constexpr std::string_view kSheetPath = "OBJ/E001.ETM";
constexpr std::string_view kSheetDirectory = "E001";
// BATTLE.PRG's sheet tables and the set offsets beside the set sector counts
// (EtmConversion.cpp): each entry's first sector, from the first sector of
// its kind, and its sector count.
constexpr std::size_t kSheetOffsetsAt = 0x803B0;
constexpr std::size_t kSheetSectorsAt = 0x80490;
constexpr std::size_t kSetOffsetsAt = 0x80260;
constexpr std::uint32_t kSheetFirstSector = 104351;
// The sheet is one sector, so the set follows it.
constexpr std::uint32_t kSetOffset = kSheetFirstSector + 1 - kSetFirstSector;
constexpr std::int32_t kFourBitModel = 0x51;
// The lender's two lines, then the 4-bit model's, then the borrower's.
constexpr std::size_t kSheetLines = 4;
constexpr std::uint8_t kFourBitRow = 2;
constexpr std::uint8_t kBorrowerRow = 3;
// SampleShp's palette rows hold 4 entries; the borrower's line uses index 9.
constexpr std::size_t kSamplePaletteEntries = 4;
constexpr std::uint16_t kPastPaletteIndex = 9;
constexpr unsigned kNibbleBits = 4;
constexpr unsigned kByteBits = 8;
constexpr std::size_t kExtractedSheetCount = 80;
// Each of the sample's three regions is a PNG and its sidecar.
constexpr std::size_t kSheetRegionFiles = 6;

// Indices within SampleShp's palette, at each line's model's depth.
content::EtmSheet SampleSheet() {
  content::EtmSheet sheet;
  for (std::size_t line = 0; line < kSheetLines; ++line) {
    const unsigned bits = line == kFourBitRow ? kNibbleBits : kByteBits;
    for (std::size_t word = 0; word < content::kEtmLineWords; ++word) {
      std::size_t value = 0;
      for (unsigned pixel = 0; pixel * bits < content::kHalfwordBits; ++pixel) {
        value |= ((word + line + pixel) % kSamplePaletteEntries)
                 << (pixel * bits);
      }
      sheet.words.push_back(static_cast<std::uint16_t>(value));
    }
  }
  sheet.words.at(kBorrowerRow * content::kEtmLineWords) =
      static_cast<std::uint16_t>(kPastPaletteIndex << kByteBits);
  return sheet;
}

Bytes EncodedSheet(const content::EtmSheet& sheet) {
  auto bytes = content::EncodeEtmFile(sheet);
  INFO(ErrorOf(bytes));
  REQUIRE(bytes.has_value());
  return *std::move(bytes);
}

// The borrower's, lender's and 4-bit model's texture rows, out of row order.
content::EsqFile SheetSet() {
  const auto entry = [](std::int32_t model, std::uint8_t row) {
    return content::EsqEntry{.selector = static_cast<std::uint16_t>(model),
                             .texture_row = row,
                             .animation = SetAnimation()};
  };
  return {.entries = {entry(kSampleBorrower, kBorrowerRow),
                      entry(kSampleLender, 0),
                      entry(kFourBitModel, kFourBitRow)}};
}

struct SheetSectors {
  std::uint8_t sheet = 1;
  std::uint8_t set = 1;
};

// A BATTLE.PRG of the tables alone: entry kSampleSet names the sheet at the
// first sheet sector and the set after it.
Bytes SheetProgram(SheetSectors sectors = {}) {
  Bytes program(content::kSeqEventLengthsAt + content::kSeqEventLengthCount);
  program.at(kSheetSectorsAt + kSampleSet) = std::byte{sectors.sheet};
  program.at(kSetSectorsAt + kSampleSet) = std::byte{sectors.set};
  const std::size_t set_offset_at = kSetOffsetsAt + (kSampleSet * 2);
  program.at(set_offset_at) = static_cast<std::byte>(kSetOffset);
  program.at(set_offset_at + 1) =
      static_cast<std::byte>(kSetOffset >> kByteBits);
  CHECK(program.at(kSheetOffsetsAt + (kSampleSet * 2)) == std::byte{0});
  return program;
}

// The sheet, first so that it starts at the first sheet sector, the set, the
// three models and `program`.
std::vector<extract::SourceFile> SheetSources(
    const Bytes& sheet, const content::EsqFile& set = SheetSet(),
    Bytes program = SheetProgram()) {
  const Bytes set_bytes = EncodedSet(set);
  REQUIRE(sheet.size() <= ModelRoot::kSectorBytes);
  REQUIRE(set_bytes.size() <= ModelRoot::kSectorBytes);
  return {{.path = std::string(kSheetPath), .bytes = sheet},
          {.path = std::string(kSetPath), .bytes = set_bytes},
          {.path = "OBJ/4F.SHP", .bytes = Encoded(SampleShp(false))},
          {.path = "OBJ/50.SHP", .bytes = Encoded(BorrowingShp())},
          {.path = "OBJ/51.SHP", .bytes = Encoded(SampleShp(true))},
          {.path = std::string(extract::kBattleProgramPath),
           .bytes = std::move(program)}};
}

auto LoadSheet(const fs::path& root, const content::ConvertedModelRead& read) {
  return LoadModel(root, read, content::LoadConvertedActorTexture);
}

fs::path SheetDirectory(const ModelRoot& root) {
  return root.Root() / fs::path(content::kActorTextureDirectory) /
         kSheetDirectory;
}

content::ImageDocument LoadRegion(const ModelRoot& root,
                                  std::string_view model) {
  auto document = content::LoadImageFiles(SheetDirectory(root), model);
  INFO(ErrorOf(document));
  REQUIRE(document.has_value());
  return *std::move(document);
}

// Applies `edit` to the region image of `model` and writes its files again.
void EditRegion(const ModelRoot& root, std::string_view model,
                const std::function<void(content::ImageDocument&)>& edit) {
  content::ImageDocument document = LoadRegion(root, model);
  edit(document);
  const auto files = extract::EncodeImageFiles(document, model);
  INFO(ErrorOf(files));
  REQUIRE(files.has_value());
  for (const content::ImageFile& file : *files) {
    WriteFile(SheetDirectory(root) / file.name, file.bytes);
  }
}

std::vector<std::string> RolesOf(const content::ImageDocument& document) {
  std::vector<std::string> roles;
  for (const content::ImageDependency& dependency :
       document.provenance.dependencies) {
    roles.push_back(std::format("{} {}", dependency.source, dependency.role));
  }
  return roles;
}

// The sheet's output: a folder of region images, depending on BATTLE.PRG, the
// set and the models.
void CheckSheetOutput(const ModelRoot& root) {
  const auto manifest = content::LoadConversionManifest(root.Root());
  INFO(ErrorOf(manifest));
  if (!manifest || !*manifest) {
    FAIL("no manifest.json");
    return;
  }
  const std::vector<content::ConversionOutput>& outputs = (*manifest)->outputs;
  const auto output = std::ranges::find(outputs, kSheetPath,
                                        &content::ConversionOutput::source);
  REQUIRE(output != outputs.end());
  CHECK(output->id == "actor-texture.obj.e001");
  std::vector<std::string> files = output->files;
  std::ranges::sort(files);
  CHECK(files == std::vector<std::string>{"textures/actors/E001/4F.image.json",
                                          "textures/actors/E001/4F.png",
                                          "textures/actors/E001/50.image.json",
                                          "textures/actors/E001/50.png",
                                          "textures/actors/E001/51.image.json",
                                          "textures/actors/E001/51.png"});
  std::vector<std::string> dependencies;
  for (const content::SourceDependency& dependency : output->dependencies) {
    dependencies.push_back(dependency.source);
  }
  CHECK(dependencies == std::vector<std::string>{"BATTLE/BATTLE.PRG",
                                                 "OBJ/4F.SHP", "OBJ/50.SHP",
                                                 "OBJ/51.SHP", "OBJ/E001.ESQ"});
}

// Each region is its model's lines at its depth, previewed with its model's
// own palette.
void CheckOwnedRegionImages(const ModelRoot& root) {
  const content::ImageDocument lender = LoadRegion(root, "4F");
  CHECK(lender.id == "image.obj.e001.4f");
  CHECK(lender.image.depth == content::ImageDepth::kIndexed8);
  CHECK(lender.image.height == kFourBitRow);
  CHECK(lender.provenance.offset == content::kEtmHeaderBytes);
  CHECK(lender.provenance.note.empty());
  CHECK(RolesOf(lender) == std::vector<std::string>{"BATTLE/BATTLE.PRG table",
                                                    "OBJ/4F.SHP texture",
                                                    "OBJ/E001.ESQ binding"});
  const content::ImageDocument four_bit = LoadRegion(root, "51");
  CHECK(four_bit.image.depth == content::ImageDepth::kIndexed4);
  CHECK(four_bit.image.width ==
        content::kEtmLineWords *
            content::PixelsPerHalfword(content::ImageDepth::kIndexed4));
  CHECK(four_bit.provenance.preview_palette ==
        content::PreviewPalette{.image = "image.obj.51", .row = 0, .entry = 0});
}

// A model borrowing another's texture is previewed with the lender's palette,
// padded past its end with a note.
void CheckBorrowedRegionImage(const ModelRoot& root) {
  const content::ImageDocument borrower = LoadRegion(root, "50");
  CHECK(borrower.image.depth == content::ImageDepth::kIndexed8);
  CHECK(borrower.provenance.offset ==
        content::kEtmHeaderBytes + (kBorrowerRow * content::kEtmLineBytes));
  CHECK(borrower.provenance.preview_palette ==
        content::PreviewPalette{.image = "image.obj.4f", .row = 0, .entry = 0});
  CHECK(borrower.image.PaletteEntries() == kPastPaletteIndex + 1U);
  CHECK(borrower.provenance.note.contains(
      "past the 4 entries of OBJ/4F.SHP's palette"));
  CHECK(RolesOf(borrower) ==
        std::vector<std::string>{"BATTLE/BATTLE.PRG table",
                                 "OBJ/4F.SHP preview palette",
                                 "OBJ/50.SHP texture", "OBJ/E001.ESQ binding"});
}

// An edit to a region's PNG survives a rerun and changes the bytes the sheet
// loads as.
void CheckEditedSheetLoads(const ModelRoot& root) {
  EditRegion(root, "4F", [](content::ImageDocument& document) {
    document.image.indices.front() = 1;
  });
  CHECK(root.Convert().actor_textures.unchanged == 1);
  content::EtmSheet edited = SampleSheet();
  edited.words.front() = static_cast<std::uint16_t>(edited.words.front() | 1U);
  CHECK(LoadSheet(root.Root(), root.Read(0)) == EncodedSheet(edited));
}

// Regions that no longer follow one another, one from another source, one at
// a depth its model does not draw and a read no sheet matches are refused.
void CheckRefusedSheetLoads(const ModelRoot& root) {
  const content::ImageDocument four_bit = LoadRegion(root, "51");
  EditRegion(root, "51", [](content::ImageDocument& document) {
    document.image.depth = content::ImageDepth::kIndexed8;
    document.image.width /= 2;
    document.image.indices.resize(document.image.width);
  });
  CHECK(ErrorOf(LoadSheet(root.Root(), root.Read(0))) ==
        "image.obj.e001.51 is 8 bits deep, but its model draws its texture at "
        "4 bits");
  EditRegion(root, "51",
             [&](content::ImageDocument& document) { document = four_bit; });
  EditRegion(root, "4F", [](content::ImageDocument& document) {
    document.image.height = 1;
    document.image.indices.resize(document.image.width);
  });
  CHECK(ErrorOf(LoadSheet(root.Root(), root.Read(0))) ==
        "image.obj.e001.51 starts at byte 0x104; the regions before it end at "
        "0x84");
  EditRegion(root, "51", [](content::ImageDocument& document) {
    document.provenance.source_sha256 = std::string(64, 'a');
  });
  CHECK(ErrorOf(LoadSheet(root.Root(), root.Read(0))) ==
        "OBJ/E001.ETM does not match its conversion manifest");
  CHECK(ErrorOf(LoadSheet(root.Root(),
                          {.lba = 1, .rounded_size = 0, .event_lengths = {}}))
            .contains("no extracted .ETM file matches LBA 1"));
}

TEST_CASE("ETM sheets decode and encode strictly", "[content-models][etm]") {
  const content::EtmSheet sheet = SampleSheet();
  const Bytes bytes = EncodedSheet(sheet);
  CHECK(bytes.size() ==
        content::kEtmHeaderBytes + (kSheetLines * content::kEtmLineBytes));
  CHECK(content::DecodeEtmFile(bytes) == sheet);
  Bytes marker = bytes;
  marker.front() = std::byte{0};
  CHECK(ErrorOf(content::DecodeEtmFile(marker)) ==
        "the marker is 0x3000, not 0x3045");
  Bytes tall = bytes;
  tall.at(2) = std::byte{1};
  tall.at(3) = std::byte{1};
  CHECK(ErrorOf(content::DecodeEtmFile(tall)) ==
        "the sheet is 257 lines high; it must be 1 to 256");
  CHECK(ErrorOf(content::DecodeEtmFile(std::span(bytes).first(3))) ==
        "the file is 3 bytes; the header is 4");
  CHECK(ErrorOf(
            content::DecodeEtmFile(std::span(bytes).first(bytes.size() - 1))) ==
        "the file is 515 bytes; a sheet 4 lines high is 516");
  content::EtmSheet ragged = sheet;
  ragged.words.pop_back();
  CHECK(ErrorOf(content::EncodeEtmFile(ragged)) ==
        "the sheet holds 255 halfwords, which is not whole lines of 64");
  CHECK(ErrorOf(content::EncodeEtmFile({})) ==
        "the sheet is 0 lines high; it must be 1 to 256");
}

// Every file under `directory`, by its path relative to it.
std::map<std::string, Bytes> FilesUnder(const fs::path& directory) {
  std::map<std::string, Bytes> files;
  for (const auto& entry : fs::recursive_directory_iterator(directory)) {
    if (!entry.is_regular_file()) continue;
    files.emplace(fs::relative(entry.path(), directory).generic_string(),
                  ReadModel(entry.path()));
  }
  return files;
}

TEST_CASE(
    "ETM sheets convert to region PNGs that load as their bytes, edits "
    "included",
    "[content-models][etm]") {
  const Bytes sheet = EncodedSheet(SampleSheet());
  const ModelRoot root(SheetSources(sheet), kSheetFirstSector);
  CHECK(root.Convert().actor_textures.converted == 1);
  CheckSheetOutput(root);
  CheckOwnedRegionImages(root);
  CheckBorrowedRegionImage(root);
  CHECK(LoadSheet(root.Root(), root.Read(0)) == sheet);
  const ModelRoot again(SheetSources(sheet), kSheetFirstSector);
  CHECK(again.Convert().actor_textures.converted == 1);
  const std::map<std::string, Bytes> files = FilesUnder(SheetDirectory(root));
  CHECK(files.size() == kSheetRegionFiles);
  CHECK(FilesUnder(SheetDirectory(again)) == files);
  CHECK(root.Convert().actor_textures.unchanged == 1);
  CheckEditedSheetLoads(root);
  CheckRefusedSheetLoads(root);
}

// Why the actor texture converter refuses `sources`.
std::string SheetRefusal(const std::vector<extract::SourceFile>& sources) {
  const ModelRoot root(sources, kSheetFirstSector);
  return ErrorOf(extract::ConvertDiscActorTextures(root.Root(), false));
}

// The sources with the sample set changed by `change`.
std::vector<extract::SourceFile> ChangedSheetSet(
    const std::function<void(content::EsqFile&)>& change) {
  content::EsqFile set = SheetSet();
  change(set);
  return SheetSources(EncodedSheet(SampleSheet()), set);
}

TEST_CASE("Every ETM sheet needs its table entry, its set and its models",
          "[content-models][etm]") {
  struct Refused {
    std::vector<extract::SourceFile> sources;
    std::string_view error;
  };
  const Bytes sheet = EncodedSheet(SampleSheet());
  std::vector<extract::SourceFile> unlisted = SheetSources(sheet);
  unlisted.pop_back();
  std::vector<extract::SourceFile> no_model = SheetSources(sheet);
  std::erase_if(no_model, [](const extract::SourceFile& source) {
    return source.path == "OBJ/51.SHP";
  });
  Bytes marker = sheet;
  marker.front() = std::byte{0};
  const std::vector<Refused> refusals{
      {unlisted,
       "OBJ/E001.ETM needs BATTLE/BATTLE.PRG, which files.tsv does not list"},
      {SheetSources(sheet, SheetSet(), Bytes(kSheetSectorsAt)),
       "BATTLE/BATTLE.PRG is too short for its file tables"},
      {SheetSources(sheet, SheetSet(), SheetProgram({.sheet = 2, .set = 1})),
       "BATTLE/BATTLE.PRG: entry 1 names 2 sectors at LBA 104351, which is no "
       ".ETM files.tsv lists"},
      {SheetSources(sheet, SheetSet(), SheetProgram({.sheet = 0, .set = 1})),
       "OBJ/E001.ETM is named by no entry of BATTLE/BATTLE.PRG's sheet table"},
      {SheetSources(sheet, SheetSet(), SheetProgram({.sheet = 1, .set = 0})),
       "BATTLE/BATTLE.PRG: entry 1 names the sheet OBJ/E001.ETM but no set"},
      {SheetSources(marker), "OBJ/E001.ETM: the marker is 0x3000, not 0x3045"},
      {SheetSources(Bytes(sheet.begin(), sheet.end() - 1)),
       "OBJ/E001.ETM: the file is 515 bytes; a sheet 4 lines high is 516"},
      {no_model,
       "OBJ/E001.ESQ needs OBJ/51.SHP, which files.tsv does not list"},
      {ChangedSheetSet([](content::EsqFile& set) {
         set.entries.front().texture_row = kSheetLines;
       }),
       "OBJ/E001.ETM: OBJ/E001.ESQ gives model 80 line 4, past the sheet's 4 "
       "lines"},
      {ChangedSheetSet(
           [](content::EsqFile& set) { set.entries.at(1).texture_row = 1; }),
       "OBJ/E001.ETM: OBJ/E001.ESQ gives no model line 0"},
      {ChangedSheetSet(
           [](content::EsqFile& set) { set.entries.back().texture_row = 0; }),
       "the same line 0"},
  };
  for (const Refused& refused : refusals) {
    const std::string error = SheetRefusal(refused.sources);
    INFO(error);
    CHECK(error.contains(refused.error));
  }
}

TEST_CASE("Extracted ETM files decode and rebuild their exact bytes",
          "[content-models][etm][extract]") {
  const auto models = ExtractedModels();
  if (!models) SKIP("Extracted Native/data is unavailable");
  std::size_t sheets = 0;
  for (const auto& entry : fs::directory_iterator(*models)) {
    if (entry.path().extension() != fs::path(content::kEtmExtension)) continue;
    CAPTURE(entry.path().filename().string());
    const Bytes bytes = ReadModel(entry.path());
    const auto sheet = content::DecodeEtmFile(bytes);
    INFO(ErrorOf(sheet));
    REQUIRE(sheet.has_value());
    CHECK(content::EncodeEtmFile(*sheet) == bytes);
    ++sheets;
  }
  CHECK(sheets == kExtractedSheetCount);
}

TEST_CASE("Extracted ETM sheets load from their region images as exact bytes",
          "[content-models][etm][extract]") {
  const auto models = ExtractedModels();
  if (!models) SKIP("Extracted Native/data is unavailable");
  CHECK(VS::test_support::CheckExtractedModelsLoad(
            *models, content::kEtmExtension, content::LoadConvertedActorTexture,
            VS::test_support::ExtractedEventLengths(*models)) ==
        kExtractedSheetCount);
}

// --- ZUD enemy bundles.

constexpr std::string_view kZudPath = "MAP/Z001U00.ZUD";
constexpr std::string_view kBorrowingZudPath = "MAP/Z001U01.ZUD";
constexpr std::string_view kTexturedZudPath = "MAP/Z001U02.ZUD";
constexpr std::string_view kZudFile = "Z001U00.json";
// The reads of ZudSources' sources: the bank, its owner, then the ZUDs.
constexpr std::size_t kOwnerRead = 1;
constexpr std::size_t kZudRead = 2;
constexpr std::size_t kBorrowingZudRead = 3;
constexpr std::size_t kTexturedZudRead = 4;
constexpr std::size_t kZudCount = 3;
constexpr std::uint16_t kOwnBankLength = 3;
constexpr std::int16_t kMovedVertex = 99;
constexpr std::size_t kShieldMaterialByte = 5;
constexpr std::uint8_t kEditedMaterial = 6;  // silver
constexpr std::size_t kWeaponCategoryByte = 2;

constexpr std::size_t SectionIndex(content::ZudSection section) {
  return static_cast<std::size_t>(section);
}

// A body no OBJ file holds: the sample model moved, borrowing 4F's texture or
// with its own.
Bytes ZudBody(bool borrowing) {
  content::ShpFile shp = borrowing ? BorrowingShp() : SampleShp(false);
  shp.vertices.front().x = kMovedVertex;
  return Encoded(shp);
}

// An unarmed ZUD of 4F's numbers and `body` alone.
content::ZudFile ZudOf(Bytes body) {
  content::ZudFile zud;
  zud.identity.front() = static_cast<std::uint8_t>(kSampleLender);
  zud.identity.at(kWeaponCategoryByte) = content::kBareHandsWeaponCategory;
  zud.sections.at(SectionIndex(content::ZudSection::kBody)) = std::move(body);
  return zud;
}

// OBJ/4F.SHP as its body, a common bank no OBJ file holds and the bank
// OBJ/4F_BT1.SEQ as its battle bank.
content::ZudFile SampleZud() {
  content::ZudFile zud = ZudOf(OwningShp(ModelRoot::kFirstLba));
  zud.sections.at(SectionIndex(content::ZudSection::kCommonAnimation)) =
      SampleSeq(kOwnBankLength);
  zud.sections.at(SectionIndex(content::ZudSection::kBattleAnimation)) =
      SampleSeq();
  return zud;
}

Bytes EncodedZud(const content::ZudFile& zud) {
  auto bytes = content::EncodeZudFile(zud);
  INFO(ErrorOf(bytes));
  REQUIRE(bytes.has_value());
  return *std::move(bytes);
}

// The bank, first so that it starts at kFirstLba where its owner names it,
// the owner, `zuds` and, if `program`, BATTLE.PRG.
std::vector<extract::SourceFile> ZudSources(
    const std::vector<extract::SourceFile>& zuds, bool program = true) {
  std::vector<extract::SourceFile> sources{
      {.path = std::string(kBankPath), .bytes = SampleSeq()},
      {.path = "OBJ/4F.SHP", .bytes = OwningShp(ModelRoot::kFirstLba)}};
  sources.insert(sources.end(), zuds.begin(), zuds.end());
  if (program) {
    sources.push_back({.path = std::string(extract::kBattleProgramPath),
                       .bytes = Bytes(content::kSeqEventLengthsAt +
                                      content::kSeqEventLengthCount)});
  }
  return sources;
}

auto LoadArchetype(const fs::path& root,
                   const content::ConvertedModelRead& read) {
  return LoadModel(root, read, content::LoadConvertedArchetype);
}

struct ArchetypeOutput {
  std::string_view source;
  std::string_view id;
  std::vector<std::string> files;
  std::vector<std::string> dependencies;
};

void CheckArchetypeOutput(const ModelRoot& root,
                          const ArchetypeOutput& expected) {
  CAPTURE(expected.source);
  const auto manifest = content::LoadConversionManifest(root.Root());
  INFO(ErrorOf(manifest));
  if (!manifest || !*manifest) {
    FAIL("no manifest.json");
    return;
  }
  const std::vector<content::ConversionOutput>& outputs = (*manifest)->outputs;
  const auto output = std::ranges::find(outputs, expected.source,
                                        &content::ConversionOutput::source);
  REQUIRE(output != outputs.end());
  CHECK(output->id == expected.id);
  CHECK(output->converter == content::kZudArchetypeConverterName);
  CHECK(output->files == expected.files);
  std::vector<std::string> dependencies;
  for (const content::SourceDependency& dependency : output->dependencies) {
    dependencies.push_back(dependency.source);
  }
  CHECK(dependencies == expected.dependencies);
}

// Each ZUD's JSON, beside the files of a body of its own; what each depends
// on.
void CheckArchetypeOutputs(const ModelRoot& root) {
  CheckArchetypeOutput(
      root,
      {.source = kZudPath,
       .id = "archetype.map.z001u00",
       .files = {"models/zud/Z001U00.json"},
       .dependencies = {"BATTLE/BATTLE.PRG", "OBJ/4F.SHP", "OBJ/4F_BT1.SEQ"}});
  CheckArchetypeOutput(
      root, {.source = kBorrowingZudPath,
             .id = "archetype.map.z001u01",
             .files = {"models/zud/Z001U01.json", "models/zud/Z001U01.gltf",
                       "models/zud/Z001U01.bin"},
             .dependencies = {"BATTLE/BATTLE.PRG", "OBJ/4F.SHP"}});
  CheckArchetypeOutput(
      root, {.source = kTexturedZudPath,
             .id = "archetype.map.z001u02",
             .files = {"models/zud/Z001U02.json", "models/zud/Z001U02.gltf",
                       "models/zud/Z001U02.bin", "models/zud/Z001U02.png",
                       "models/zud/Z001U02.image.json",
                       "models/zud/Z001U02.palettes.png"},
             .dependencies = {"BATTLE/BATTLE.PRG"}});
}

// The archetype's JSON names its body and battle bank and keeps its common
// bank; the bodies of its own show the lender's image or their own.
void CheckArchetypeFiles(const ModelRoot& root) {
  const fs::path directory =
      root.Root() / fs::path(content::kArchetypeDirectory);
  const auto json =
      content::ParseJsonText(ReadModel(directory / kZudFile), kZudFile);
  INFO(ErrorOf(json));
  REQUIRE(json.has_value());
  CHECK(json->at("body") ==
        content::JsonDocument::parse(R"({"model": "model.obj.4f"})"));
  CHECK(
      json->at("battle_animation") ==
      content::JsonDocument::parse(R"({"animation": "animation.obj.4f_bt1"})"));
  CHECK(json->at("common_animation").at("bones") == kSampleBones);
  CHECK(ParseGltf(ReadModel(directory / "Z001U01.gltf"))["images"] ==
        content::JsonDocument::parse(R"([{"uri": "../characters/4F.png"}])"));
  CHECK(ParseGltf(ReadModel(directory / "Z001U02.gltf"))["images"] ==
        content::JsonDocument::parse(R"([{"uri": "Z001U02.png"}])"));
}

// An edit to the model an archetype names survives a rerun and reaches the
// archetype's bytes.
void CheckEditedPartLoads(const ModelRoot& root, const Bytes& original) {
  root.EditGltf(content::kShpModelDirectory, "4F",
                [](content::JsonDocument& gltf) {
                  SetCollisionRadius(gltf, kEditedRadius);
                });
  const auto rerun = root.Convert();
  CHECK(rerun.characters.unchanged == 1);
  CHECK(rerun.archetypes.unchanged == kZudCount);
  const auto body = LoadShp(root.Root(), root.Read(kOwnerRead));
  INFO(ErrorOf(body));
  REQUIRE(body.has_value());
  CHECK(*body != SampleZud().Section(content::ZudSection::kBody));
  auto edited = content::DecodeZudFile(original);
  REQUIRE(edited.has_value());
  edited->sections.at(SectionIndex(content::ZudSection::kBody)) = *body;
  CHECK(LoadArchetype(root.Root(), root.Read(kZudRead)) == EncodedZud(*edited));
}

// An edit to a body of the archetype's own survives a rerun and changes the
// bytes it loads as.
void CheckEditedOwnBodyLoads(const ModelRoot& root, const Bytes& original) {
  root.EditGltf(content::kArchetypeDirectory, "Z001U01",
                [](content::JsonDocument& gltf) {
                  SetCollisionRadius(gltf, kEditedRadius);
                });
  CHECK(root.Convert().archetypes.unchanged == kZudCount);
  const auto edited = LoadArchetype(root.Root(), root.Read(kBorrowingZudRead));
  INFO(ErrorOf(edited));
  REQUIRE(edited.has_value());
  CHECK(*edited != original);
  const auto zud = content::DecodeZudFile(*edited);
  INFO(ErrorOf(zud));
  REQUIRE(zud.has_value());
  const auto shp =
      content::DecodeShp(zud->Section(content::ZudSection::kBody), false);
  INFO(ErrorOf(shp));
  REQUIRE(shp.has_value());
  CHECK(content::ShpLayoutJson(shp->layout)["collision_radius"] ==
        kEditedRadius);
}

fs::path ArchetypePath(const ModelRoot& root) {
  return root.Root() / fs::path(content::kArchetypeDirectory) / kZudFile;
}

// Applies `edit` to the sample ZUD's converted JSON file.
void EditArchetype(const ModelRoot& root,
                   const std::function<void(content::JsonDocument&)>& edit) {
  auto json = content::ParseJsonText(ReadModel(ArchetypePath(root)), kZudFile);
  INFO(ErrorOf(json));
  REQUIRE(json.has_value());
  edit(*json);
  const std::string text = content::FormatJsonText(*json) + "\n";
  WriteFile(ArchetypePath(root), std::as_bytes(std::span(text)));
}

// An edit to the archetype's header and its own bank survives a rerun and
// changes the bytes it loads as.
void CheckEditedArchetypeLoads(const ModelRoot& root) {
  static constexpr std::uint16_t kEditedLength = 1000;
  const auto loaded = LoadArchetype(root.Root(), root.Read(kZudRead));
  INFO(ErrorOf(loaded));
  REQUIRE(loaded.has_value());
  auto edited = content::DecodeZudFile(*loaded);
  REQUIRE(edited.has_value());
  EditArchetype(root, [](content::JsonDocument& json) {
    json.at("header").at("shield_material") = "silver";
    json.at("common_animation").at("bank").at("clips").front().at("length") =
        kEditedLength;
  });
  CHECK(root.Convert().archetypes.unchanged == kZudCount);
  edited->identity.at(kShieldMaterialByte) = kEditedMaterial;
  edited->sections.at(SectionIndex(content::ZudSection::kCommonAnimation)) =
      SampleSeq(kEditedLength);
  CHECK(LoadArchetype(root.Root(), root.Read(kZudRead)) == EncodedZud(*edited));
}

// A part of the wrong kind or of another source, a stale archetype and a read
// no ZUD matches are refused.
void CheckRefusedArchetypeLoads(const ModelRoot& root) {
  struct Refused {
    std::string_view member;
    content::JsonDocument value;
    std::string_view error;
  };
  using content::JsonDocument;
  const std::vector<Refused> refusals{
      {"/body", JsonDocument::parse(R"({"model": "model.obj.99"})"),
       "Z001U00.json's body names model.obj.99, which is not a"},
      {"/body", JsonDocument::parse(R"({"model": "animation.obj.4f_bt1"})"),
       "Z001U00.json's body names animation.obj.4f_bt1, which is not a"},
      {"/battle_animation",
       JsonDocument::parse(R"({"animation": "model.obj.4f"})"),
       "Z001U00.json's battle animation names model.obj.4f, which is not a"},
      {"/body", JsonDocument::parse(R"({"file": "Z001U01.gltf"})"),
       "its body Z001U01.gltf is from MAP/Z001U01.ZUD, not MAP/Z001U00.ZUD"},
      {"/source_sha256", std::string(64, 'a'),
       "MAP/Z001U00.ZUD does not match its conversion manifest"},
  };
  const Bytes original = ReadModel(ArchetypePath(root));
  for (const Refused& refused : refusals) {
    CAPTURE(refused.member);
    EditArchetype(root, [&refused](JsonDocument& json) {
      json[JsonDocument::json_pointer(std::string(refused.member))] =
          refused.value;
    });
    const std::string error =
        ErrorOf(LoadArchetype(root.Root(), root.Read(kZudRead)));
    INFO(error);
    CHECK(error.contains(refused.error));
    WriteFile(ArchetypePath(root), original);
  }
  CHECK(
      ErrorOf(LoadArchetype(root.Root(),
                            {.lba = 1, .rounded_size = 0, .event_lengths = {}}))
          .contains("no extracted .ZUD file matches LBA 1"));
}

bool AllZero(std::span<const std::byte> bytes) {
  return std::ranges::all_of(
      bytes, [](std::byte byte) { return byte == std::byte{}; });
}

// The room spawn's ZUD read loads a bundle that fits its sector-rounded read,
// and refuses one rebuilt larger, leaving zeros.
void CheckLiveArchetypeReads(const ModelRoot& root) {
  const content::ConvertedModelRead read = root.Read(kZudRead);
  const auto expected = LoadArchetype(root.Root(), read);
  INFO(ErrorOf(expected));
  REQUIRE(expected.has_value());
  const content::DiscFileExtent file{
      .sector = static_cast<std::int32_t>(read.lba),
      .bytes = static_cast<std::int32_t>(read.rounded_size)};
  content::DataStore::ScopedOverride restore;
  REQUIRE(content::DataStore::Open(root.Root()).has_value());
  Bytes loaded(read.rounded_size, std::byte{0xFF});
  REQUIRE(expected->size() <= loaded.size());
  CHECK(content::ReadLiveConvertedArchetype(file, loaded) ==
        std::optional<bool>{true});
  CHECK(Bytes(loaded.begin(), loaded.begin() + std::ssize(*expected)) ==
        *expected);
  CHECK(AllZero(std::span(loaded).subspan(expected->size())));
  const Bytes original = ReadModel(ArchetypePath(root));
  EditArchetype(root, [&read](content::JsonDocument& json) {
    content::JsonDocument unread = content::JsonDocument::object();
    unread["unread"] = std::string(2 * read.rounded_size, 'a');
    json.at("common_animation").at("bank").at("motion").push_back(unread);
  });
  std::ranges::fill(loaded, std::byte{0xFF});
  CHECK(content::ReadLiveConvertedArchetype(file, loaded) ==
        std::optional<bool>{false});
  CHECK(AllZero(loaded));
  WriteFile(ArchetypePath(root), original);
}

TEST_CASE("ZUD bundles convert to archetypes that load as their bytes",
          "[content-models][zud]") {
  const Bytes zud = EncodedZud(SampleZud());
  const Bytes borrowing = EncodedZud(ZudOf(ZudBody(true)));
  const Bytes textured = EncodedZud(ZudOf(ZudBody(false)));
  const ModelRoot root(
      ZudSources({{.path = std::string(kZudPath), .bytes = zud},
                  {.path = std::string(kBorrowingZudPath), .bytes = borrowing},
                  {.path = std::string(kTexturedZudPath), .bytes = textured}}));
  CHECK(root.Convert().archetypes.converted == kZudCount);
  CheckArchetypeOutputs(root);
  CheckArchetypeFiles(root);
  CHECK(LoadArchetype(root.Root(), root.Read(kZudRead)) == zud);
  CHECK(LoadArchetype(root.Root(), root.Read(kBorrowingZudRead)) == borrowing);
  CHECK(LoadArchetype(root.Root(), root.Read(kTexturedZudRead)) == textured);
  const auto rerun = root.Convert().archetypes;
  CHECK(rerun.unchanged == kZudCount);
  CHECK(rerun.removed == 0);
  CheckEditedPartLoads(root, zud);
  CheckEditedOwnBodyLoads(root, borrowing);
  CheckEditedArchetypeLoads(root);
  CheckRefusedArchetypeLoads(root);
  CheckLiveArchetypeReads(root);
}

// Why the archetype converter refuses `sources`, run after the models when
// `models` is set.
std::string ZudRefusal(const std::vector<extract::SourceFile>& sources,
                       bool models) {
  const ModelRoot root(sources);
  if (models) {
    const auto converted = extract::ConvertDiscModels(root.Root(), false);
    INFO(ErrorOf(converted));
    REQUIRE(converted.has_value());
  }
  return ErrorOf(extract::ConvertDiscArchetypes(root.Root(), false));
}

TEST_CASE("Every ZUD bundle needs whole sections and its models converted",
          "[content-models][zud]") {
  struct Refused {
    std::vector<extract::SourceFile> sources;
    std::string_view error;
    bool models = true;
  };
  const auto sources = [](const content::ZudFile& zud, bool program = true) {
    return ZudSources(
        {{.path = std::string(kZudPath), .bytes = EncodedZud(zud)}}, program);
  };
  content::ZudFile padded = SampleZud();
  padded.identity.back() = 1;
  content::ZudFile armed = SampleZud();
  armed.sections.at(SectionIndex(content::ZudSection::kWeapon)) = {
      std::byte{1}};
  content::ZudFile bodiless = SampleZud();
  bodiless.sections.at(SectionIndex(content::ZudSection::kBody)).clear();
  Bytes cut = EncodedZud(SampleZud());
  cut.pop_back();
  const std::vector<Refused> refusals{
      {sources(SampleZud(), false),
       "MAP/Z001U00.ZUD needs BATTLE/BATTLE.PRG, which files.tsv does not "
       "list"},
      {sources(SampleZud()),
       "MAP/Z001U00.ZUD: its body is OBJ/4F.SHP, which has no current", false},
      {sources(padded), "MAP/Z001U00.ZUD: identity byte 7 is 1, not 0"},
      {sources(armed), "MAP/Z001U00.ZUD: its weapon is no WEP files.tsv lists"},
      {sources(bodiless), "MAP/Z001U00.ZUD has no body"},
      {ZudSources({{.path = std::string(kZudPath), .bytes = cut}}),
       "MAP/Z001U00.ZUD: the battle animation section's"},
  };
  for (const Refused& refused : refusals) {
    const std::string error = ZudRefusal(refused.sources, refused.models);
    INFO(error);
    CHECK(error.contains(refused.error));
  }
}

}  // namespace
