#include <lodepng.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
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
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include "Content/ImageFiles.hpp"
#include "Content/NativeImage.hpp"
#include "Content/TimFile.hpp"
#include "Core/Sha256.hpp"
#include "Extract/ImageFilesWriter.hpp"
#include "Extract/TimImage.hpp"
#include "Render/DrawPolygon.hpp"
#include "Render/PageSpace.hpp"
#include "nlohmann/json.hpp"

namespace {
namespace Content = VS::Content;
namespace Extract = VS::Extract;
namespace fs = std::filesystem;
using Content::ImageDepth;
using Bytes = std::vector<std::byte>;
using Words = std::vector<std::uint16_t>;
using FileMap = std::map<std::string, Bytes>;

struct BlockSpec {
  Content::TimRect rect;
  Words words;
};

void Put16(Bytes& out, std::uint16_t value) {
  out.push_back(static_cast<std::byte>(value & 0xFFU));
  out.push_back(static_cast<std::byte>(value >> 8U));
}

void Put32(Bytes& out, std::uint32_t value) {
  Put16(out, static_cast<std::uint16_t>(value & 0xFFFFU));
  Put16(out, static_cast<std::uint16_t>(value >> 16U));
}

void PutBlock(Bytes& out, const BlockSpec& block) {
  Put32(out, static_cast<std::uint32_t>(12 + (block.words.size() * 2)));
  Put16(out, static_cast<std::uint16_t>(block.rect.x));
  Put16(out, static_cast<std::uint16_t>(block.rect.y));
  Put16(out, block.rect.width);
  Put16(out, block.rect.height);
  for (const std::uint16_t word : block.words) Put16(out, word);
}

// A TIM written field by field, independently of EncodeTim.
Bytes TimBytes(std::uint32_t id, std::uint32_t flags,
               const std::optional<BlockSpec>& clut, const BlockSpec& image) {
  Bytes out;
  Put32(out, id);
  Put32(out, flags);
  if (clut) PutBlock(out, *clut);
  PutBlock(out, image);
  return out;
}

// The optional's value; a test fails with an exception when it is empty.
template <class Optional>
auto& Present(Optional& value) {
  if (!value.has_value()) throw std::bad_optional_access();
  return *value;
}

template <class T>
T Take(std::expected<T, std::string> value) {
  if (!value) FAIL(value.error());
  return std::move(*value);
}

Content::TimFile TimOf(const Bytes& bytes) {
  auto decoded = Content::DecodeTim(bytes);
  if (!decoded) FAIL(decoded.error().message);
  CHECK(decoded->bytes == bytes.size());
  return std::move(decoded->tim);
}

// Two 16-entry palettes: row 0 has STP on some colours, row 1 on every one.
Bytes FourBitTwoPalettes() {
  const Words row0{0x0000, 0x8000, 0x001F, 0x801F, 0x03E0, 0x7C00,
                   0x7FFF, 0xFFFF, 0x1234, 0x9234, 0x0421, 0x0842,
                   0x0C63, 0x1084, 0x14A5, 0x18C6};
  Words clut = row0;
  for (const std::uint16_t word : row0) {
    clut.push_back(word == 0 ? 0 : static_cast<std::uint16_t>(word | 0x8000));
  }
  return TimBytes(0x10, 0x8, BlockSpec{{0, 480, 16, 2}, clut},
                  BlockSpec{{768, 0, 2, 2}, {0x3210, 0x7654, 0xBA98, 0xFEDC}});
}

// 256 entries, every opaque colour with STP, and 0x0000 / 0x8000.
Bytes EightBitAllStp() {
  Words clut(256);
  for (std::size_t index = 2; index < clut.size(); ++index) {
    clut[index] =
        static_cast<std::uint16_t>(((index * 0x123U) & 0x7FFFU) | 0x8000U);
  }
  clut[1] = 0x8000;
  return TimBytes(0x10, 0x9, BlockSpec{{0, 481, 256, 1}, clut},
                  BlockSpec{{512, 0, 4, 2},
                            {0x0100, 0xFF02, 0x7F80, 0x0001, 0x1110, 0x3322,
                             0x5544, 0xFE00}});
}

Bytes EightBitShortPalette() {
  return TimBytes(0x10, 0x9,
                  BlockSpec{{0, 482, 16, 1},
                            {0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005,
                             0x0006, 0x0007, 0x0008, 0x0009, 0x000A, 0x000B,
                             0x000C, 0x000D, 0x000E, 0x800F}},
                  BlockSpec{{512, 8, 2, 1}, {0x0F00, 0x0E01}});
}

Bytes DirectMixedStp() {
  return TimBytes(0x10, 0x2, std::nullopt,
                  BlockSpec{{640, 0, 4, 2},
                            {0x0000, 0x8000, 0x001F, 0x801F, 0x7FFF, 0x83E0,
                             0x7C00, 0xFC00}});
}

Bytes DirectNoStp() {
  return TimBytes(0x10, 0x2, std::nullopt,
                  BlockSpec{{640, 8, 2, 2}, {0x0000, 0x8000, 0x001F, 0x03E0}});
}

Bytes DirectAllStp() {
  return TimBytes(0x10, 0x2, std::nullopt,
                  BlockSpec{{640, 16, 2, 2}, {0x801F, 0x8000, 0x0000, 0xFFFF}});
}

// A field background as the disc has it: id 0x110 and a 16-bit mode whose
// halfwords pack 8-bit indices.
Bytes PackedBackground() {
  Words clut(256);
  for (std::size_t index = 0; index < clut.size(); ++index) {
    clut[index] = static_cast<std::uint16_t>(index * 0x81U);
  }
  return TimBytes(0x110, 0xA, BlockSpec{{128, 0, 256, 1}, clut},
                  BlockSpec{{320, 0, 2, 2}, {0x0201, 0x0403, 0xFF00, 0x7F80}});
}

struct Sample {
  std::string_view stem;
  Bytes bytes;
  ImageDepth depth;
  std::vector<std::string> files;
};

std::vector<Sample> Samples() {
  return {
      {"FOUR",
       FourBitTwoPalettes(),
       ImageDepth::kIndexed4,
       {"FOUR.png", "FOUR.image.json", "FOUR.palettes.png"}},
      {"EIGHT",
       EightBitAllStp(),
       ImageDepth::kIndexed8,
       {"EIGHT.png", "EIGHT.image.json"}},
      {"SHORT",
       EightBitShortPalette(),
       ImageDepth::kIndexed8,
       {"SHORT.png", "SHORT.image.json"}},
      {"MIXED",
       DirectMixedStp(),
       ImageDepth::kDirect16,
       {"MIXED.png", "MIXED.image.json", "MIXED.stp.png"}},
      {"PLAIN",
       DirectNoStp(),
       ImageDepth::kDirect16,
       {"PLAIN.png", "PLAIN.image.json"}},
      {"ALL",
       DirectAllStp(),
       ImageDepth::kDirect16,
       {"ALL.png", "ALL.image.json"}},
      {"BG",
       PackedBackground(),
       ImageDepth::kIndexed8,
       {"BG.png", "BG.image.json"}},
  };
}

Content::ImageDocument DocumentOf(const Bytes& bytes, ImageDepth depth) {
  const Content::TimFile tim = TimOf(bytes);
  return {.id = "image.test.sample",
          .image = Take(Extract::ImageFromTim(tim, depth)),
          .material = Content::ImageMaterial{},
          .unreferenced = false,
          .provenance = {.source = "TEST/SAMPLE.TIM",
                         .source_sha256 = VS::Core::Sha256Hex(bytes),
                         .offset = 0,
                         .tim = VS::Content::OriginOf(tim),
                         .texture_page = std::nullopt,
                         .preview_palette = std::nullopt,
                         .dependencies = {},
                         .note = {}}};
}

FileMap MapOf(const std::vector<Content::ImageFile>& files) {
  FileMap map;
  for (const Content::ImageFile& file : files) map[file.name] = file.bytes;
  return map;
}

Content::ImageFileReader ReaderOf(const FileMap& files) {
  return [&files](const std::string& name) -> std::optional<Bytes> {
    const auto found = files.find(name);
    if (found == files.end()) return std::nullopt;
    return found->second;
  };
}

std::vector<unsigned char> Raw(const Bytes& bytes) {
  std::vector<unsigned char> raw(bytes.size());
  for (std::size_t index = 0; index < bytes.size(); ++index) {
    raw[index] = std::to_integer<unsigned char>(bytes[index]);
  }
  return raw;
}

Bytes Wrap(const std::vector<unsigned char>& raw) {
  Bytes bytes(raw.size());
  for (std::size_t index = 0; index < raw.size(); ++index) {
    bytes[index] = static_cast<std::byte>(raw[index]);
  }
  return bytes;
}

// An indexed PNG re-saved with `edit` applied to its palette (RGBA bytes) and
// raw packed indices, as an image editor keeping indexed mode would.
Bytes EditIndexedPng(const Bytes& png,
                     const std::function<void(std::vector<unsigned char>&,
                                              unsigned char*)>& edit) {
  lodepng::State state;
  state.decoder.color_convert = 0;
  std::vector<unsigned char> raw;
  unsigned width = 0;
  unsigned height = 0;
  REQUIRE(lodepng::decode(raw, width, height, state, Raw(png)) == 0);
  edit(raw, state.info_png.color.palette);
  REQUIRE(lodepng_color_mode_copy(&state.info_raw, &state.info_png.color) == 0);
  state.encoder.auto_convert = 0;
  std::vector<unsigned char> out;
  REQUIRE(lodepng::encode(out, raw, width, height, state) == 0);
  return Wrap(out);
}

// A PNG re-saved as RGBA with `edit` applied to its RGBA pixels.
Bytes EditRgbaPng(
    const Bytes& png,
    const std::function<void(std::vector<unsigned char>&)>& edit) {
  std::vector<unsigned char> rgba;
  unsigned width = 0;
  unsigned height = 0;
  REQUIRE(lodepng::decode(rgba, width, height, Raw(png)) == 0);
  edit(rgba);
  lodepng::State state;
  state.encoder.auto_convert = 0;
  state.info_png.color.colortype = LCT_RGBA;
  state.info_png.color.bitdepth = 8;
  std::vector<unsigned char> out;
  REQUIRE(lodepng::encode(out, rgba, width, height, state) == 0);
  return Wrap(out);
}

std::string DecodeError(std::string_view stem, const FileMap& files) {
  auto decoded = Content::DecodeImageFiles(stem, ReaderOf(files));
  REQUIRE_FALSE(decoded.has_value());
  return decoded.error();
}

nlohmann::ordered_json SidecarOf(const FileMap& files,
                                 const std::string& name) {
  const std::vector<unsigned char> text = Raw(files.at(name));
  return nlohmann::ordered_json::parse(text.begin(), text.end());
}

Bytes SidecarBytes(const nlohmann::ordered_json& json) {
  const std::string text = json.dump(2);
  Bytes bytes(text.size());
  for (std::size_t index = 0; index < text.size(); ++index) {
    bytes[index] = static_cast<std::byte>(text[index]);
  }
  return bytes;
}

struct PngShape {
  LodePNGColorType type = LCT_RGBA;
  unsigned bit_depth = 0;
};

PngShape ShapeOf(const Bytes& png) {
  lodepng::State state;
  unsigned width = 0;
  unsigned height = 0;
  const std::vector<unsigned char> raw = Raw(png);
  REQUIRE(lodepng_inspect(&width, &height, &state, raw.data(), raw.size()) ==
          0);
  return {state.info_png.color.colortype, state.info_png.color.bitdepth};
}
}  // namespace

TEST_CASE("A TIM decodes every word", "[content-images][tim]") {
  const Bytes bytes = FourBitTwoPalettes();
  Bytes trailing = bytes;
  trailing.resize(bytes.size() + 4, std::byte{0xEE});
  auto decoded = Content::DecodeTim(trailing);
  REQUIRE(decoded.has_value());
  CHECK(decoded->bytes == bytes.size());
  const Content::TimFile& tim = decoded->tim;
  CHECK(tim.Mode() == Content::TimPixelMode::kIndexed4);
  CHECK(tim.HasClut());
  const auto& clut = Present(tim.clut);
  CHECK(clut.rect == Content::TimRect{0, 480, 16, 2});
  CHECK(clut.words.size() == 32);
  CHECK(tim.image.rect == Content::TimRect{768, 0, 2, 2});
  CHECK(tim.image.words == Words{0x3210, 0x7654, 0xBA98, 0xFEDC});
}

TEST_CASE("A TIM re-encodes to the same bytes", "[content-images][tim]") {
  const Bytes bytes = FourBitTwoPalettes();
  CHECK(Take(VS::Content::EncodeTim(TimOf(bytes))) == bytes);
  // The id's version byte is kept, not normalized.
  const Bytes background = PackedBackground();
  CHECK(TimOf(background).id == 0x110);
  CHECK(Take(VS::Content::EncodeTim(TimOf(background))) == background);
}

TEST_CASE("Malformed TIMs are rejected with the field at fault",
          "[content-images][tim]") {
  struct Case {
    std::string_view name;
    std::function<void(Bytes&)> damage;
    Content::TimError error;
    std::size_t offset;
  };
  const auto set16 = [](Bytes& bytes, std::size_t at, std::uint16_t value) {
    bytes[at] = static_cast<std::byte>(value & 0xFFU);
    bytes[at + 1] = static_cast<std::byte>(value >> 8U);
  };
  // The CLUT block starts at byte 8 (16x2 words: 76 bytes), the image at 84.
  const std::vector<Case> cases{
      {"header", [](Bytes& b) { b.resize(5); },
       Content::TimError::kTruncatedHeader, 0},
      {"id", [](Bytes& b) { b[0] = std::byte{0x11}; },
       Content::TimError::kBadId, 0},
      {"flags", [](Bytes& b) { b[4] = std::byte{0x18}; },
       Content::TimError::kUnknownFlags, 4},
      {"mode", [](Bytes& b) { b[4] = std::byte{0x0C}; },
       Content::TimError::kUnsupportedMode, 4},
      {"clut header", [](Bytes& b) { b.resize(14); },
       Content::TimError::kTruncatedBlock, 8},
      {"clut pixels", [](Bytes& b) { b.resize(40); },
       Content::TimError::kTruncatedBlock, 20},
      {"clut length", [](Bytes& b) { b[8] = std::byte{77}; },
       Content::TimError::kBlockLength, 8},
      {"clut empty", [&](Bytes& b) { set16(b, 16, 0); },
       Content::TimError::kEmptyRect, 16},
      {"image outside", [&](Bytes& b) { set16(b, 88, 1023); },
       Content::TimError::kOutsideVram, 88},
      {"image negative", [&](Bytes& b) { set16(b, 90, 0xFFFF); },
       Content::TimError::kOutsideVram, 88},
      {"image pixels", [](Bytes& b) { b.pop_back(); },
       Content::TimError::kTruncatedBlock, 96},
  };
  for (const Case& test : cases) {
    CAPTURE(test.name);
    Bytes bytes = FourBitTwoPalettes();
    test.damage(bytes);
    auto decoded = Content::DecodeTim(bytes);
    REQUIRE_FALSE(decoded.has_value());
    CHECK(decoded.error().error == test.error);
    CHECK(decoded.error().offset == test.offset);
    CHECK_FALSE(decoded.error().message.empty());
  }
}

TEST_CASE("Every colour word survives the editor colour round trip",
          "[content-images][colour]") {
  bool channels_exact = true;
  for (unsigned channel = 0; channel < 32; ++channel) {
    const auto five = static_cast<std::uint8_t>(channel);
    channels_exact =
        channels_exact &&
        Content::NarrowChannel(Content::ExpandChannel(five)) == five;
  }
  CHECK(channels_exact);
  bool all_exact = true;
  for (unsigned value = 0; value <= 0xFFFFU; ++value) {
    const auto word = static_cast<std::uint16_t>(value);
    const std::optional<std::uint16_t> back = Content::WordOfColor(
        Content::ColorOfWord(word), Content::CarriesStp(word));
    all_exact = all_exact && back == word;
  }
  CHECK(all_exact);
}

TEST_CASE("Editor colours follow the hardware's transparency rules",
          "[content-images][colour]") {
  CHECK(Content::ColorOfWord(0x0000) == Content::ImageColor{0, 0, 0, 0});
  CHECK(Content::ColorOfWord(0x8000) == Content::ImageColor{0, 0, 0, 255});
  CHECK_FALSE(Content::CarriesStp(0x8000));
  CHECK(Content::CarriesStp(0x801F));
  // Transparent whatever its colour; black is never transparent.
  CHECK(Content::WordOfColor({200, 10, 30, 0}, true) == 0x0000);
  CHECK(Content::WordOfColor({0, 0, 0, 255}, false) == 0x8000);
  CHECK_FALSE(Content::WordOfColor({10, 10, 10, 128}, false).has_value());
}

TEST_CASE("Indexed TIM pixels unpack low pixel first",
          "[content-images][tim]") {
  const Content::TimFile four = TimOf(FourBitTwoPalettes());
  const Content::NativeImage image =
      Take(Extract::ImageFromTim(four, ImageDepth::kIndexed4));
  CHECK(image.width == 8);
  CHECK(image.height == 2);
  CHECK(image.indices == std::vector<std::uint8_t>{0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
                                                   10, 11, 12, 13, 14, 15});
  CHECK(image.palettes.size() == 2);
  CHECK(Content::PackedWords(image) == four.image.words);
}

TEST_CASE("A packed background converts at the depth its tiles draw",
          "[content-images][tim]") {
  const Content::TimFile background = TimOf(PackedBackground());
  const Content::NativeImage eight =
      Take(Extract::ImageFromTim(background, ImageDepth::kIndexed8));
  CHECK(eight.width == 4);
  CHECK(eight.indices ==
        std::vector<std::uint8_t>{1, 2, 3, 4, 0x00, 0xFF, 0x80, 0x7F});
  CHECK(eight.palettes.size() == 1);
  CHECK(eight.PaletteEntries() == 256);
  const Content::TimFile rebuilt =
      Take(VS::Content::TimFromImage(eight, VS::Content::OriginOf(background)));
  CHECK(rebuilt == background);
}

TEST_CASE("Image conversion rejects what it cannot keep",
          "[content-images][tim]") {
  const Content::TimFile direct = TimOf(DirectMixedStp());
  CHECK_FALSE(Extract::ImageFromTim(direct, ImageDepth::kIndexed8).has_value());
  const Content::TimFile four = TimOf(FourBitTwoPalettes());
  CHECK_FALSE(Extract::ImageFromTim(four, ImageDepth::kDirect16).has_value());
  CHECK_FALSE(Extract::ImageFromTim(four, ImageDepth::kIndexed8).has_value());

  // An index past a short palette names the pixel.
  Content::TimFile short_palette = TimOf(EightBitShortPalette());
  short_palette.image.words[1] = 0x2001;
  auto invalid = Extract::ImageFromTim(short_palette, ImageDepth::kIndexed8);
  REQUIRE_FALSE(invalid.has_value());
  CHECK(invalid.error().find("pixel (3, 0)") != std::string::npos);

  // CLUT words that do not divide into whole palettes.
  Content::TimFile ragged = TimOf(EightBitAllStp());
  Present(ragged.clut).words.resize(300);
  CHECK_FALSE(Extract::ImageFromTim(ragged, ImageDepth::kIndexed8).has_value());

  Content::TimFile wide = TimOf(DirectNoStp());
  wide.flags = 0x3;
  CHECK_FALSE(Extract::ImageFromTim(wide, ImageDepth::kDirect16).has_value());
}

TEST_CASE("An image of an unknown depth is rejected, not divided by",
          "[content-images][tim]") {
  // This deliberately checks rejection of an invalid on-disk enum value.
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  const auto unknown = static_cast<ImageDepth>(0);
  STATIC_CHECK(Content::PixelsPerHalfword(unknown) == 1);
  STATIC_CHECK_FALSE(Content::IsIndexed(unknown));
  const Content::NativeImage image{.depth = unknown,
                                   .width = 2,
                                   .height = 1,
                                   .indices = {},
                                   .texels = {0x001F, 0x8000},
                                   .palettes = {}};
  const auto valid = Content::ValidateImage(image);
  REQUIRE_FALSE(valid.has_value());
  CHECK(valid.error().find("depth 0") != std::string::npos);
}

TEST_CASE("Image files reproduce the TIM exactly", "[content-images][files]") {
  const std::vector<Sample> samples = Samples();
  const auto index = GENERATE(range(std::size_t{0}, std::size_t{7}));
  REQUIRE(index < samples.size());
  const Sample& sample = samples[index];
  CAPTURE(sample.stem);
  const Content::ImageDocument document =
      DocumentOf(sample.bytes, sample.depth);
  const std::vector<Content::ImageFile> files =
      Take(Extract::EncodeImageFiles(document, sample.stem));
  std::vector<std::string> names;
  for (const Content::ImageFile& file : files) names.push_back(file.name);
  CHECK(names == sample.files);
  CHECK(Take(Extract::EncodeImageFiles(document, sample.stem)) == files);

  const FileMap map = MapOf(files);
  const Content::ImageDocument loaded =
      Take(Content::DecodeImageFiles(sample.stem, ReaderOf(map)));
  CHECK(loaded == document);
  REQUIRE(loaded.provenance.tim.has_value());
  const Content::TimFile rebuilt = Take(
      VS::Content::TimFromImage(loaded.image, Present(loaded.provenance.tim)));
  CHECK(Take(VS::Content::EncodeTim(rebuilt)) == sample.bytes);
}

namespace {
FileMap FourFiles() {
  return MapOf(Take(Extract::EncodeImageFiles(
      DocumentOf(FourBitTwoPalettes(), ImageDepth::kIndexed4), "FOUR")));
}
}  // namespace

TEST_CASE("Indexed image files keep the true depth",
          "[content-images][files]") {
  const FileMap four = FourFiles();
  CHECK(ShapeOf(four.at("FOUR.png")).type == LCT_PALETTE);
  CHECK(ShapeOf(four.at("FOUR.png")).bit_depth == 4);
  CHECK(ShapeOf(four.at("FOUR.palettes.png")).type == LCT_RGBA);

  const FileMap eight = MapOf(Take(Extract::EncodeImageFiles(
      DocumentOf(EightBitAllStp(), ImageDepth::kIndexed8), "EIGHT")));
  CHECK(ShapeOf(eight.at("EIGHT.png")).bit_depth == 8);
  CHECK(SidecarOf(eight, "EIGHT.image.json")["palette"]["stp"][0] == "all");
}

TEST_CASE("An indexed sidecar records the palette rows and their STP bits",
          "[content-images][files]") {
  const auto sidecar = SidecarOf(FourFiles(), "FOUR.image.json");
  CHECK(sidecar["palette"]["entries"] == 16);
  CHECK(sidecar["palette"]["count"] == 2);
  CHECK(sidecar["palette"]["stp"][0] ==
        nlohmann::ordered_json::array({3, 7, 9}));
  CHECK(sidecar["palette"]["stp"][1] == "all");
}

TEST_CASE("A sidecar carries only what PNG cannot", "[content-images][files]") {
  const auto sidecar = SidecarOf(FourFiles(), "FOUR.image.json");
  CHECK(sidecar["schema"] == "vs.image");
  CHECK(sidecar["depth"] == 4);
  CHECK_FALSE(sidecar.contains("stp"));
  CHECK_FALSE(sidecar.contains("unreferenced"));
  CHECK(sidecar["provenance"]["tim"]["clut"] ==
        nlohmann::ordered_json::array({0, 480, 16, 2}));
}

TEST_CASE("A direct image has an STP mask only when its bits are mixed",
          "[content-images][files]") {
  const FileMap mixed = MapOf(Take(Extract::EncodeImageFiles(
      DocumentOf(DirectMixedStp(), ImageDepth::kDirect16), "MIXED")));
  CHECK(ShapeOf(mixed.at("MIXED.png")).type == LCT_RGBA);
  CHECK(ShapeOf(mixed.at("MIXED.stp.png")).type == LCT_GREY);
  CHECK(ShapeOf(mixed.at("MIXED.stp.png")).bit_depth == 1);
  CHECK(SidecarOf(mixed, "MIXED.image.json")["stp"] == "mask");
  CHECK_FALSE(SidecarOf(mixed, "MIXED.image.json").contains("palette"));
}

TEST_CASE("Edited image files change the words the game uses",
          "[content-images][files]") {
  FileMap files = MapOf(Take(Extract::EncodeImageFiles(
      DocumentOf(EightBitShortPalette(), ImageDepth::kIndexed8), "SHORT")));
  files["SHORT.png"] = EditIndexedPng(
      files.at("SHORT.png"),
      [](std::vector<unsigned char>& raw, unsigned char* palette) {
        // Entry 15 (STP) becomes pure green; pixel 0 uses entry 2.
        palette[(15 * 4) + 0] = 0;
        palette[(15 * 4) + 1] = 255;
        palette[(15 * 4) + 2] = 0;
        raw[0] = 2;
      });
  const Content::ImageDocument edited =
      Take(Content::DecodeImageFiles("SHORT", ReaderOf(files)));
  CHECK(edited.image.palettes[0][15] == 0x83E0);
  CHECK(edited.image.indices[0] == 2);
  CHECK(Content::PackedWords(edited.image)[0] == 0x0F02);

  // A transparent entry ignores its STP; a black one is always 0x8000.
  files["SHORT.png"] =
      EditIndexedPng(files.at("SHORT.png"),
                     [](std::vector<unsigned char>&, unsigned char* palette) {
                       palette[(15 * 4) + 3] = 0;
                       palette[(14 * 4) + 0] = 0;
                       palette[(14 * 4) + 1] = 0;
                       palette[(14 * 4) + 2] = 0;
                     });
  const Content::ImageDocument cleared =
      Take(Content::DecodeImageFiles("SHORT", ReaderOf(files)));
  CHECK(cleared.image.palettes[0][15] == 0x0000);
  CHECK(cleared.image.palettes[0][14] == 0x8000);
}

TEST_CASE("Image files that disagree or cannot be drawn are rejected",
          "[content-images][files]") {
  const FileMap four = MapOf(Take(Extract::EncodeImageFiles(
      DocumentOf(FourBitTwoPalettes(), ImageDepth::kIndexed4), "FOUR")));
  const auto rejects = [](const FileMap& files, std::string_view stem,
                          std::string_view text) {
    const std::string error = DecodeError(stem, files);
    CAPTURE(error);
    CHECK(error.find(text) != std::string::npos);
  };

  FileMap files = four;
  files["FOUR.palettes.png"] = EditRgbaPng(
      four.at("FOUR.palettes.png"),
      [](std::vector<unsigned char>& rgba) { rgba[(2 * 4) + 1] = 99; });
  rejects(files, "FOUR", "row 0 entry 2");

  files = four;
  files["FOUR.png"] = EditIndexedPng(
      four.at("FOUR.png"), [](std::vector<unsigned char>&,
                              unsigned char* palette) { palette[7] = 128; });
  rejects(files, "FOUR", "alpha 128");

  files = four;
  files["FOUR.png"] =
      EditRgbaPng(four.at("FOUR.png"), [](std::vector<unsigned char>&) {});
  rejects(files, "FOUR", "indexed");

  files = four;
  files.erase("FOUR.palettes.png");
  rejects(files, "FOUR", "FOUR.palettes.png is missing");

  auto sidecar = SidecarOf(four, "FOUR.image.json");
  sidecar["palette"]["colour"] = 1;
  files = four;
  files["FOUR.image.json"] = SidecarBytes(sidecar);
  rejects(files, "FOUR", "unknown member 'colour'");

  sidecar = SidecarOf(four, "FOUR.image.json");
  sidecar["width"] = 16;
  files = four;
  files["FOUR.image.json"] = SidecarBytes(sidecar);
  rejects(files, "FOUR", "FOUR.png is 8x2; the sidecar says 16x2");

  sidecar = SidecarOf(four, "FOUR.image.json");
  sidecar["palette"]["stp"][0] = nlohmann::ordered_json::array({16});
  files = four;
  files["FOUR.image.json"] = SidecarBytes(sidecar);
  rejects(files, "FOUR", "palette indices run from 0 to 15");

  sidecar = SidecarOf(four, "FOUR.image.json");
  sidecar["palette"]["entries"] = 15;
  files = four;
  files["FOUR.image.json"] = SidecarBytes(sidecar);
  rejects(files, "FOUR", "the PNG palette has 16 entries; the sidecar says 15");

  const FileMap mixed = MapOf(Take(Extract::EncodeImageFiles(
      DocumentOf(DirectMixedStp(), ImageDepth::kDirect16), "MIXED")));
  files = mixed;
  files["MIXED.stp.png"] = EditRgbaPng(mixed.at("MIXED.stp.png"),
                                       [](std::vector<unsigned char>& rgba) {
                                         rgba[4] = 128;
                                         rgba[5] = 128;
                                         rgba[6] = 128;
                                       });
  rejects(files, "MIXED", "pixel (1, 0) must be black");
  files = mixed;
  files["MIXED.png"] = EditRgbaPng(
      mixed.at("MIXED.png"),
      [](std::vector<unsigned char>& rgba) { rgba[(5 * 4) + 3] = 1; });
  rejects(files, "MIXED", "pixel (1, 1) has alpha 1");
}

namespace {
constexpr std::uint16_t kCompositePage = 0x88;  // x 512, 8-bit, average blend
constexpr std::uint16_t kCompositePalette = 480U << 6U;

VS::Render::DrawCorner Corner(std::int16_t x, std::int16_t y, std::uint8_t u,
                              std::uint8_t v) {
  VS::Render::DrawCorner corner;
  corner.x = x;
  corner.y = y;
  corner.u = u;
  corner.v = v;
  return corner;
}

// The four texels drawn as one raw-textured row over a known background.
std::array<std::uint16_t, 4> Composite(const Words& pixels, const Words& clut,
                                       bool semi) {
  VS::Render::PageSpace pages;
  pages.SetDrawArea(0, 0, VS::Render::PageSpace::Width - 1,
                    VS::Render::PageSpace::Height - 1);
  pages.SetDrawOffset(0, 0);
  pages.Fill({0, 400, 16, 4}, 128, 64, 32);
  pages.Upload({512, 0, 2, 1}, pixels.data());
  pages.Upload({0, 480, 256, 1}, clut.data());
  VS::Render::DrawPolygon quad;
  quad.material = {.gouraud = false,
                   .textured = true,
                   .semi_transparent = semi,
                   .raw_texture = true,
                   .page = {kCompositePage},
                   .palette = {kCompositePalette}};
  quad.corner_count = VS::Render::kDrawQuadCorners;
  quad.corners = {Corner(0, 400, 0, 0), Corner(4, 400, 4, 0),
                  Corner(0, 401, 0, 1), Corner(4, 401, 4, 1)};
  REQUIRE(pages.Draw(quad));
  return {pages.At(0, 400), pages.At(1, 400), pages.At(2, 400),
          pages.At(3, 400)};
}
}  // namespace

namespace {
struct CompositePair {
  std::array<std::uint16_t, 4> original;
  std::array<std::uint16_t, 4> converted;
};

// The TIM's row drawn from its own words and from its converted image files.
CompositePair CompositeBoth(bool semi) {
  Words clut(256);
  clut[1] = 0x8000;  // opaque black
  clut[2] = 0x001F;  // red, opaque
  clut[3] = 0x83E0;  // green, semi-transparent
  const Bytes bytes = TimBytes(0x10, 0x9, BlockSpec{{0, 480, 256, 1}, clut},
                               BlockSpec{{512, 0, 2, 1}, {0x0100, 0x0302}});
  const Content::TimFile tim = TimOf(bytes);
  const FileMap files = MapOf(Take(Extract::EncodeImageFiles(
      DocumentOf(bytes, ImageDepth::kIndexed8), "COMPOSITE")));
  const Content::ImageDocument loaded =
      Take(Content::DecodeImageFiles("COMPOSITE", ReaderOf(files)));
  return {.original = Composite(tim.image.words, Present(tim.clut).words, semi),
          .converted = Composite(Content::PackedWords(loaded.image),
                                 Content::PaletteWords(loaded.image), semi)};
}
}  // namespace

TEST_CASE("Converted images composite over a background like the original",
          "[content-images][render]") {
  const bool semi = GENERATE(false, true);
  CAPTURE(semi);
  const CompositePair pair = CompositeBoth(semi);
  CHECK(pair.converted == pair.original);
}

TEST_CASE("Converted images skip 0x0000 and blend only STP texels",
          "[content-images][render]") {
  VS::Render::PageSpace background;
  background.Fill({0, 0, 1, 1}, 128, 64, 32);
  const std::uint16_t behind = background.At(0, 0);
  CHECK(CompositeBoth(false).converted ==
        std::array<std::uint16_t, 4>{behind, 0x8000, 0x001F, 0x83E0});
  const std::array<std::uint16_t, 4> blended = CompositeBoth(true).converted;
  CHECK(blended[0] == behind);  // 0x0000 is not drawn
  CHECK(blended[1] != 0x8000);
  CHECK(blended[2] == 0x001F);  // no STP: never blended
  CHECK(blended[3] != 0x83E0);
}

TEST_CASE("Image files are staged on disk and load back",
          "[content-images][files]") {
  const fs::path directory =
      fs::temp_directory_path() / "vs-native-image-files-test";
  std::error_code ignored;
  fs::remove_all(directory, ignored);
  const Content::ImageDocument document =
      DocumentOf(FourBitTwoPalettes(), ImageDepth::kIndexed4);
  const auto files = Take(Extract::EncodeImageFiles(document, "FOUR"));
  REQUIRE(Extract::WriteImageFiles(directory, files).has_value());
  std::size_t count = 0;
  for (const auto& entry : fs::directory_iterator(directory)) {
    CHECK(entry.path().extension() != ".tmp");
    ++count;
  }
  CHECK(count == files.size());
  CHECK(Take(Content::LoadImageFiles(directory, "FOUR")) == document);
  auto missing = Content::LoadImageFiles(directory, "NONE");
  REQUIRE_FALSE(missing.has_value());
  CHECK(missing.error() == "NONE.image.json is missing");
  fs::remove_all(directory, ignored);
}
