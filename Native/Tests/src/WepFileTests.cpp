#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <format>
#include <functional>
#include <map>
#include <numbers>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Content/ConvertedModels.hpp"
#include "Content/ImageFiles.hpp"
#include "Content/JsonText.hpp"
#include "Content/NativeImage.hpp"
#include "Content/ShpFile.hpp"
#include "Content/ShpModelFiles.hpp"
#include "Content/WepFile.hpp"
#include "Content/WepModelFiles.hpp"
#include "Extract/ShpModelConversion.hpp"
#include "Extract/WepModelConversion.hpp"
#include "TestSupport/ModelFixtures.hpp"

namespace {

namespace fs = std::filesystem;
namespace content = VS::Content;
namespace extract = VS::Extract;
using Bytes = std::vector<std::byte>;
using Quaternion = std::array<double, 4>;
using VS::test_support::ErrorOf;
using VS::test_support::ExtractedModels;
using VS::test_support::LoadModel;
using VS::test_support::ModelRoot;
using VS::test_support::ParseGltf;
using VS::test_support::Polygon;
using VS::test_support::ReadModel;

constexpr std::uint8_t kRootParent = 0x2F;
constexpr std::uint8_t kSampleWidthByte = 4;  // 8 pixels wide
constexpr std::int16_t kQuarterTurn = 1024;   // 4096ths of a turn
constexpr std::int16_t kReserved6 = 7;
constexpr std::size_t kModelSizeAt = 0x0C;
constexpr std::size_t kModelAt = 0x10;
constexpr std::size_t kBoneCountAt = 4;
constexpr std::size_t kFlagsAt = 4;  // from the texture's size field
constexpr std::size_t kWidthAt = 5;
constexpr std::size_t kHeightAt = 6;
constexpr std::size_t kPaletteWordsAt = 7;
constexpr std::size_t kPastPaletteEntry = 50;
constexpr std::uint16_t kStrayColor = 0x1234;
constexpr std::uint8_t kFarIndex = 60;
constexpr std::size_t kExtractedWeaponCount = 127;
constexpr std::size_t kWeaponBones = 3;
constexpr double kTolerance = 1e-12;
constexpr double kHalfRoot2 = std::numbers::sqrt2 / 2.0;

std::int32_t ReadI32(std::span<const std::byte> bytes, std::size_t at) {
  std::int32_t value = 0;
  std::memcpy(&value, &bytes[at], sizeof value);
  return value;
}

void WriteI32(Bytes& bytes, std::size_t at, std::int32_t value) {
  std::memcpy(&bytes[at], &value, sizeof value);
}

// Where the texture's size field is: after the model section.
std::size_t TextureAt(std::span<const std::byte> bytes) {
  return kModelAt + static_cast<std::size_t>(ReadI32(bytes, kModelSizeAt));
}

content::ShpBone Bone(std::int32_t length, std::uint8_t parent) {
  content::ShpBone bone;
  bone.length = length;
  bone.parent = parent;
  return bone;
}

content::WepTexture SampleTexture() {
  content::WepTexture texture;
  texture.width_byte = kSampleWidthByte;
  texture.palette_words = content::kWepSinglePaletteWords;
  for (std::size_t word = 0; word < texture.HandleWords(); ++word) {
    texture.handle.push_back(static_cast<std::uint16_t>(0x8000U | word));
  }
  for (std::size_t material = 0; material < content::kWepMaterialCount;
       ++material) {
    for (std::size_t word = 0; word < texture.MaterialWords(); ++word) {
      texture.materials.at(material).push_back(
          static_cast<std::uint16_t>(0x4000U | (material << 8U) | word));
    }
  }
  const std::size_t pixels =
      std::size_t{texture.Width()} * content::kWepTextureRows;
  for (std::size_t pixel = 0; pixel < pixels; ++pixel) {
    texture.pixels.push_back(
        static_cast<std::uint8_t>(pixel % texture.palette_words));
  }
  return texture;
}

// A weapon of three bones, two turned, with fields set to values the
// encoder must keep.
content::WepFile SampleWep() {
  content::WepFile wep;
  wep.limb_ranges.front() = 1;  // body part 0 holds the triangle
  wep.bones = {Bone(0, kRootParent), Bone(-100, 0), Bone(-50, 1)};
  wep.bones.at(1).tag = 2;
  wep.bones.at(2).reserved13 = {7, 8, 9};
  wep.groups = {{.bone = 1, .end = 3}, {.bone = 2, .end = 6}};
  wep.vertices = {{.x = 1, .y = -2, .z = 3},   {.x = -4, .y = 5, .z = -6},
                  {.x = 7, .y = 8, .z = 9},    {.x = -10, .y = 11, .z = 12},
                  {.x = 13, .y = 14, .z = 15}, {.x = 16, .y = 17, .z = -18}};
  constexpr std::uint8_t kFlags = 0x75;
  wep.parts.at(0) = {
      Polygon({.corners = 3, .flags = kFlags, .extra = 9, .first = 0})};
  wep.parts.at(1) = {
      Polygon({.corners = 4, .flags = 1, .extra = 2, .first = 1})};
  wep.parts.at(2) = {
      Polygon({.corners = 3, .flags = 0, .extra = 3, .first = 3})};
  wep.texture = SampleTexture();
  wep.rotations = {
      {.x = 0, .y = 0, .z = 0, .reserved6 = kReserved6},
      {.x = 0, .y = 0, .z = kQuarterTurn, .reserved6 = kReserved6},
      {.x = kQuarterTurn, .y = 0, .z = kQuarterTurn, .reserved6 = kReserved6}};
  return wep;
}

Bytes Encoded(const content::WepFile& wep) {
  auto bytes = content::EncodeWep(wep);
  INFO(ErrorOf(bytes));
  REQUIRE(bytes.has_value());
  return *std::move(bytes);
}

void CheckDecodeFails(std::span<const std::byte> bytes,
                      std::string_view message) {
  const auto decoded = content::DecodeWep(bytes);
  REQUIRE_FALSE(decoded.has_value());
  INFO(decoded.error());
  CHECK(decoded.error().find(message) != std::string::npos);
}

void CheckEncodeFails(const content::WepFile& wep, std::string_view message) {
  const auto encoded = content::EncodeWep(wep);
  REQUIRE_FALSE(encoded.has_value());
  INFO(encoded.error());
  CHECK(encoded.error().find(message) != std::string::npos);
}

TEST_CASE("WEP files round-trip every field", "[content-models][wep]") {
  const content::WepFile wep = SampleWep();
  const Bytes bytes = Encoded(wep);
  const auto decoded = content::DecodeWep(bytes);
  INFO(ErrorOf(decoded));
  REQUIRE(decoded.has_value());
  CHECK(*decoded == wep);
  const std::size_t texture = TextureAt(bytes);
  // The size field: 4 + the pixel bytes + the palette words.
  CHECK(ReadI32(bytes, texture) ==
        4 + (kSampleWidthByte * 2 * 32) + content::kWepSinglePaletteWords);
}

TEST_CASE("WEP decoding names the bytes it rejects", "[content-models][wep]") {
  const Bytes valid = Encoded(SampleWep());
  const std::size_t texture = TextureAt(valid);
  const auto with = [&valid](std::size_t at, int value) {
    Bytes bytes = valid;
    bytes.at(at) = static_cast<std::byte>(value);
    return bytes;
  };
  SECTION("bone count") {
    CheckDecodeFails(with(kBoneCountAt, 5), "5 bones exceed the game's 4");
  }
  SECTION("model size") {
    Bytes bytes = valid;
    bytes.resize(kModelAt + content::kWepMaxModelBytes + 0x100);
    WriteI32(bytes, kModelSizeAt,
             static_cast<std::int32_t>(content::kWepMaxModelBytes + 1));
    CheckDecodeFails(bytes, "exceeds the game's 2048-byte");
  }
  SECTION("texture header") {
    CheckDecodeFails(with(texture + kFlagsAt, 0x10), "texture flags 0x10");
    CheckDecodeFails(with(texture + kWidthAt, 0), "the width byte 0");
    CheckDecodeFails(with(texture + kWidthAt, content::kWepMaxWidthByte + 1),
                     "the width byte 65");
    CheckDecodeFails(with(texture + kHeightAt, 15), "the height byte 15");
    CheckDecodeFails(with(texture + kPaletteWordsAt, 50),
                     "50 palette words are neither 48 nor 96");
    CheckDecodeFails(
        with(texture, std::to_integer<std::uint8_t>(valid.at(texture)) + 1),
        "the texture size field");
  }
  SECTION("file end") {
    Bytes longer = valid;
    longer.push_back(std::byte{0});
    CheckDecodeFails(longer, "3 bone rotations need");
    CheckDecodeFails(std::span(valid).first(valid.size() - 1), "remain");
  }
}

TEST_CASE("WEP encoding refuses what the game cannot load",
          "[content-models][wep]") {
  content::WepFile wep = SampleWep();
  SECTION("coloured polygons") {
    wep.colored = true;
    CheckEncodeFails(wep, "weapon polygons are flat");
  }
  SECTION("sound") {
    wep.sound = {std::byte{1}};
    CheckEncodeFails(wep, "no sound section");
  }
  SECTION("bones") {
    wep.bones.push_back(Bone(-1, 2));
    wep.bones.push_back(Bone(-1, 3));
    CheckEncodeFails(wep, "5 bones exceed the game's 4");
  }
  SECTION("rotations") {
    wep.rotations.pop_back();
    CheckEncodeFails(wep, "2 bone rotations for 3 bones");
  }
  SECTION("palettes") {
    wep.texture.handle.pop_back();
    CheckEncodeFails(wep, "the handle palette has 15 colours, not 16");
    wep = SampleWep();
    wep.texture.materials.back().push_back(0);
    CheckEncodeFails(wep, "material palette 6 has 33 colours, not 32");
    wep = SampleWep();
    wep.texture.palette_words = 50;
    CheckEncodeFails(wep, "50 palette words are neither");
  }
  SECTION("texture size") {
    wep.texture.pixels.pop_back();
    CheckEncodeFails(wep, "the texture has 255 pixels, not 8 x 32");
    wep = SampleWep();
    wep.texture.width_byte = 0;
    CheckEncodeFails(wep, "the texture width 0 is not 2 to 128");
  }
  SECTION("model size") {
    wep.vertices.resize(wep.vertices.size() + 256);
    wep.groups.back().end = static_cast<std::uint16_t>(wep.vertices.size());
    CheckEncodeFails(wep, "weapon geometry buffer");
  }
}

void CheckRotation(const content::JsonDocument& node,
                   const Quaternion& expected) {
  REQUIRE(node.contains("rotation"));
  const content::JsonDocument& rotation = node.at("rotation");
  REQUIRE(rotation.size() == expected.size());
  for (std::size_t i = 0; i < expected.size(); ++i) {
    INFO(std::format("component {} of {}", i, rotation.dump()));
    CHECK(std::abs(rotation.at(i).get<double>() - expected.at(i)) < kTolerance);
  }
}

TEST_CASE("A weapon's glTF skeleton turns each bone by its rest rotation",
          "[content-models][wep]") {
  content::WepFile wep = SampleWep();
  // The game ignores bone 0's rotation.
  wep.rotations.front().x = kQuarterTurn;
  const auto node = [&wep](std::size_t bone) {
    return content::WepSkeletonGltf(wep, 0)["nodes"].at(
        content::kShpFirstBoneNode + bone);
  };
  CHECK_FALSE(node(0).contains("rotation"));
  CheckRotation(node(1), {0.0, 0.0, kHalfRoot2, kHalfRoot2});
  // Rz x Ry x Rx: about X, then about Z. The other order gives y = -0.5.
  CheckRotation(node(2), {0.5, 0.5, 0.5, 0.5});
  SECTION("only the angles' low 12 bits turn") {
    wep.rotations.at(1).z = static_cast<std::int16_t>(kQuarterTurn + 4096);
    CheckRotation(node(1), {0.0, 0.0, kHalfRoot2, kHalfRoot2});
    wep.rotations.at(1).z = 4096;
    CHECK_FALSE(node(1).contains("rotation"));
  }
  SECTION("w is not negative") {
    wep.rotations.at(1).z = static_cast<std::int16_t>(3 * kQuarterTurn);
    CheckRotation(node(1), {0.0, 0.0, -kHalfRoot2, kHalfRoot2});
  }
}

// A first pixel past the palette pads each row with transparent words, which
// must stay transparent.
void CheckPaddedPalette(content::WepTexture texture) {
  texture.pixels.front() = kFarIndex;
  content::NativeImage image = content::WepTextureImage(texture);
  CHECK(image.PaletteEntries() == kFarIndex + 1U);
  CHECK(image.palettes.front().back() == 0);
  CHECK(content::WepTextureOf(image, texture.palette_words) == texture);
  image.palettes.at(2).at(kPastPaletteEntry) = kStrayColor;
  const auto stray = content::WepTextureOf(image, texture.palette_words);
  REQUIRE_FALSE(stray.has_value());
  CHECK(stray.error().find("palette row 2 entry 50 is 0x1234") !=
        std::string::npos);
}

void CheckHandleColors(content::NativeImage image, std::uint8_t palette_words) {
  std::uint16_t& color = image.palettes.at(3).front();
  color = static_cast<std::uint16_t>(color ^ 1U);
  const auto handle = content::WepTextureOf(image, palette_words);
  REQUIRE_FALSE(handle.has_value());
  CHECK(handle.error().find("palette row 3 starts with other colours") !=
        std::string::npos);
}

void CheckImageShape(content::NativeImage image, std::uint8_t palette_words) {
  const auto words = content::WepTextureOf(image, 50);
  REQUIRE_FALSE(words.has_value());
  CHECK(words.error().find("texture.palette_words is 50") != std::string::npos);
  image.palettes.pop_back();
  const auto rows = content::WepTextureOf(image, palette_words);
  REQUIRE_FALSE(rows.has_value());
  CHECK(rows.error().find("7 palette rows") != std::string::npos);
}

TEST_CASE("A weapon texture is one palette row per material",
          "[content-models][wep]") {
  const content::WepTexture texture = SampleTexture();
  const content::NativeImage image = content::WepTextureImage(texture);
  REQUIRE(image.palettes.size() == content::kWepMaterialCount);
  CHECK(image.width == 8);
  CHECK(image.height == content::kWepTextureRows);
  std::vector<std::uint16_t> last = texture.handle;
  last.insert(last.end(), texture.materials.back().begin(),
              texture.materials.back().end());
  CHECK(image.palettes.back() == last);
  CHECK(content::WepTextureOf(image, texture.palette_words) == texture);
  SECTION("indices past the palette are padded with transparent words") {
    CheckPaddedPalette(texture);
  }
  SECTION("every row starts with the handle's colours") {
    CheckHandleColors(image, texture.palette_words);
  }
  SECTION("the image's shape") {
    CheckImageShape(image, texture.palette_words);
  }
}

extract::ConvertedModel Converted(const extract::SourceFile& source) {
  auto converted = extract::ConvertWepModel(source);
  INFO(ErrorOf(converted));
  REQUIRE(converted.has_value());
  return *std::move(converted);
}

void CheckWeaponOutput(const extract::ConvertedModel& model) {
  CHECK(model.output.id == "model.obj.01.wep");
  CHECK(model.output.source == "OBJ/01.WEP");
  CHECK(model.output.converter == content::kWepModelConverterName);
  CHECK(model.output.dependencies.empty());
  const std::vector<std::string> files{"models/weapons/01.gltf",
                                       "models/weapons/01.bin",
                                       "models/weapons/01.png",
                                       "models/weapons/01.image.json",
                                       "models/weapons/01.palettes.png",
                                       "models/weapons/01.bronze.preview.png",
                                       "models/weapons/01.hagane.preview.png",
                                       "models/weapons/01.silver.preview.png",
                                       "models/weapons/01.damascus.preview.png",
                                       "models/weapons/01.wood.preview.png",
                                       "models/weapons/01.leather.preview.png"};
  CHECK(model.output.files == files);
}

TEST_CASE("A WEP model converts to glTF files that rebuild its bytes",
          "[content-models][wep]") {
  const content::WepFile wep = SampleWep();
  const Bytes bytes = Encoded(wep);
  const auto model = Converted({.path = "OBJ/01.WEP", .bytes = bytes});
  CheckWeaponOutput(model);
  const auto document =
      content::DecodeWepModelFiles("01", content::FileListReader(model.files));
  INFO(ErrorOf(document));
  REQUIRE(document.has_value());
  CHECK(document->wep == wep);
  CHECK(document->image_id == "image.obj.01.wep");
  CHECK(content::EncodeWep(document->wep) == bytes);
  REQUIRE_FALSE(model.files.empty());
  const auto gltf = ParseGltf(model.files.front().bytes);
  CheckRotation(gltf["nodes"].at(content::kShpFirstBoneNode + 1),
                {0.0, 0.0, kHalfRoot2, kHalfRoot2});
  const auto broken = extract::ConvertWepModel(
      {.path = "OBJ/02.WEP", .bytes = Bytes(bytes.begin(), bytes.end() - 1)});
  REQUIRE_FALSE(broken.has_value());
  CHECK(broken.error().starts_with("OBJ/02.WEP: "));
}

// The colours of a PNG's PLTE chunk, opaque.
std::vector<content::ImageColor> PngPalette(std::span<const std::byte> png) {
  constexpr std::size_t kSignatureBytes = 8;
  constexpr std::size_t kLengthBytes = 4;
  constexpr std::size_t kTypeBytes = 4;
  constexpr std::size_t kCrcBytes = 4;
  constexpr std::size_t kRgbBytes = 3;
  constexpr std::uint8_t kOpaque = 255;
  std::size_t at = kSignatureBytes;
  while (at + kLengthBytes + kTypeBytes <= png.size()) {
    std::size_t length = 0;
    for (std::size_t index = 0; index < kLengthBytes; ++index) {
      length = (length << 8U) | std::to_integer<std::size_t>(png[at + index]);
    }
    std::string type;
    for (std::size_t index = 0; index < kTypeBytes; ++index) {
      type.push_back(static_cast<char>(
          std::to_integer<unsigned char>(png[at + kLengthBytes + index])));
    }
    const std::size_t data = at + kLengthBytes + kTypeBytes;
    if (type == "PLTE" && data + length <= png.size()) {
      std::vector<content::ImageColor> colors;
      for (std::size_t color = data; color + kRgbBytes <= data + length;
           color += kRgbBytes) {
        colors.push_back(
            {.red = std::to_integer<std::uint8_t>(png[color]),
             .green = std::to_integer<std::uint8_t>(png[color + 1]),
             .blue = std::to_integer<std::uint8_t>(png[color + 2]),
             .alpha = kOpaque});
      }
      return colors;
    }
    at = data + length + kCrcBytes;
  }
  return {};
}

constexpr std::string_view kVariantsKey = "KHR_materials_variants";

// The colours preview row `row` shows: the handle's, then the material's.
std::vector<content::ImageColor> RowColors(const content::WepTexture& texture,
                                           std::size_t row) {
  std::vector<content::ImageColor> colors;
  for (const std::uint16_t word : texture.handle) {
    colors.push_back(content::ColorOfWord(word));
  }
  for (const std::uint16_t word : texture.materials.at(row)) {
    colors.push_back(content::ColorOfWord(word));
  }
  return colors;
}

// Each variant is named for its material and shows an image of that row.
void CheckVariantImages(const content::JsonDocument& gltf,
                        const extract::ConvertedModel& model,
                        const content::WepTexture& texture) {
  const std::string key(kVariantsKey);
  const auto& names = gltf.at("extensions").at(key).at("variants");
  const auto& images = gltf.at("images");
  const auto& textures = gltf.at("textures");
  REQUIRE(names.size() == content::kWepMaterialCount);
  REQUIRE(images.size() == content::kWepMaterialCount);
  REQUIRE(textures.size() == content::kWepMaterialCount);
  for (std::size_t row = 0; row < content::kWepMaterialCount; ++row) {
    const std::string material(content::kWepMaterialNames.at(row));
    INFO(material);
    CHECK(names.at(row).at("name") == material);
    const std::string uri =
        row == 0 ? "01.png" : std::format("01.{}.preview.png", material);
    CHECK(images.at(row).at("uri") == uri);
    CHECK(textures.at(row).at("source") == row);
    const auto file =
        std::ranges::find(model.files, uri, &content::ImageFile::name);
    REQUIRE(file != model.files.end());
    CHECK(PngPalette(file->bytes) == RowColors(texture, row));
  }
}

// Every primitive shows variant v with a material of texture v, variant 0
// with its own material.
void CheckVariantMappings(const content::JsonDocument& gltf) {
  const std::string key(kVariantsKey);
  const auto& materials = gltf.at("materials");
  const auto& primitives = gltf.at("meshes").at(0).at("primitives");
  REQUIRE_FALSE(primitives.empty());
  for (const auto& primitive : primitives) {
    const auto& mappings = primitive.at("extensions").at(key).at("mappings");
    REQUIRE(mappings.size() == content::kWepMaterialCount);
    CHECK(mappings.at(0).at("material") == primitive.at("material"));
    for (std::size_t variant = 0; variant < mappings.size(); ++variant) {
      CHECK(mappings.at(variant).at("variants") ==
            content::JsonDocument::array({variant}));
      const auto& shown =
          materials.at(mappings.at(variant).at("material").get<std::size_t>());
      CHECK(
          shown.at("pbrMetallicRoughness").at("baseColorTexture").at("index") ==
          variant);
    }
  }
}

TEST_CASE("A converted weapon shows each blade material as a glTF variant",
          "[content-models][wep]") {
  const content::WepFile wep = SampleWep();
  const auto model = Converted({.path = "OBJ/01.WEP", .bytes = Encoded(wep)});
  REQUIRE_FALSE(model.files.empty());
  const auto gltf = ParseGltf(model.files.front().bytes);
  const auto& used = gltf.at("extensionsUsed");
  CHECK(std::count(used.begin(), used.end(), std::string(kVariantsKey)) == 1);
  CheckVariantImages(gltf, model, wep.texture);
  CheckVariantMappings(gltf);
}

auto LoadWep(const fs::path& root, const content::ConvertedModelRead& read) {
  return LoadModel(root, read, content::LoadConvertedWeapon);
}

void SetReserved6(content::JsonDocument& gltf, std::int64_t value) {
  gltf["asset"]["extras"]["vs"]["bone_rotations"][1]["reserved6"] = value;
}

// An edited bone rotation pad loads; one past a halfword is refused by name.
void CheckReservedEdits(const ModelRoot& root) {
  constexpr std::int64_t kEdited = 9;
  root.EditGltf(
      content::kWepModelDirectory, "01",
      [](content::JsonDocument& gltf) { SetReserved6(gltf, kEdited); });
  CHECK(root.Convert().weapons.unchanged == 1);
  const auto edited = LoadWep(root.Root(), root.Read(0));
  INFO(ErrorOf(edited));
  REQUIRE(edited.has_value());
  const auto wep = content::DecodeWep(*edited);
  INFO(ErrorOf(wep));
  REQUIRE(wep.has_value());
  CHECK(wep->rotations.at(1).reserved6 == kEdited);
  constexpr std::int64_t kPastHalfword = 0x8000;
  root.EditGltf(
      content::kWepModelDirectory, "01",
      [](content::JsonDocument& gltf) { SetReserved6(gltf, kPastHalfword); });
  const auto refused = LoadWep(root.Root(), root.Read(0));
  REQUIRE_FALSE(refused.has_value());
  CHECK(refused.error().find("reserved6") != std::string::npos);
}

TEST_CASE("Converted weapons load in place of their WEP files, edits included",
          "[content-models][wep]") {
  const Bytes original = Encoded(SampleWep());
  const ModelRoot root({{.path = "OBJ/01.WEP", .bytes = original}});
  CHECK(root.Convert().weapons.converted == 1);
  CHECK(LoadWep(root.Root(), root.Read(0)) == original);
  CheckReservedEdits(root);
  const auto unknown =
      LoadWep(root.Root(), {.lba = 1, .rounded_size = 0, .event_lengths = {}});
  REQUIRE_FALSE(unknown.has_value());
  CHECK(unknown.error().find("no extracted .WEP file matches LBA 1") !=
        std::string::npos);
}

using Tallies = std::map<std::string, std::size_t, std::less<>>;

TEST_CASE("Extracted WEP files decode and rebuild their exact bytes",
          "[content-models][wep][extract]") {
  const auto models = ExtractedModels();
  if (!models) SKIP("Extracted Native/data is unavailable");
  Tallies tallies;
  for (const auto& entry : fs::directory_iterator(*models)) {
    if (entry.path().extension() != content::kWepExtension) continue;
    CAPTURE(entry.path().filename().string());
    const Bytes bytes = ReadModel(entry.path());
    const auto decoded = content::DecodeWep(bytes);
    INFO(ErrorOf(decoded));
    REQUIRE(decoded.has_value());
    CHECK(decoded->bones.size() == kWeaponBones);
    ++tallies[std::format("width {} words {}", decoded->texture.Width(),
                          decoded->texture.palette_words)];
    CHECK(content::EncodeWep(*decoded) == bytes);
  }
  const Tallies expected{{"width 128 words 48", 23},
                         {"width 128 words 96", 22},
                         {"width 64 words 48", 81},
                         {"width 64 words 96", 1}};
  CHECK(tallies == expected);
}

TEST_CASE("Extracted WEP models load from their conversions as exact bytes",
          "[content-models][wep][extract]") {
  const auto models = ExtractedModels();
  if (!models) SKIP("Extracted Native/data is unavailable");
  CHECK(VS::test_support::CheckExtractedModelsLoad(
            *models, content::kWepExtension, content::LoadConvertedWeapon) ==
        kExtractedWeaponCount);
}

}  // namespace
