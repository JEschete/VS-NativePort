#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Content/DataStore.hpp"
#include "Content/GameTextCodec.hpp"
#include "Content/JsonText.hpp"
#include "Content/NativeText.hpp"
#include "Extract/SaveTextConversion.hpp"
#include "Extract/TextContribution.hpp"
#include "Render/NativeTextDraw.hpp"
#include "Render/NativeTextLayout.hpp"
#include "Save/NativeFileText.hpp"

namespace {
namespace text = VS::Content;
namespace render = VS::Render;
std::span<const std::byte> Bytes(std::string_view value) {
  return std::as_bytes(std::span(value));
}
text::NativeFont Font() {
  text::NativeFont font{"font.test", "image.test", 12, {}};
  for (char c = ' '; c <= '~'; ++c) {
    font.glyphs.emplace(std::string(1, c),
                        text::GlyphMetric{0, 0, 12, 12, 6, c != ' '});
  }
  return font;
}
class Target final : public render::ImmediatePacketTarget {
 public:
  std::size_t count = 0;
  void Submit(std::span<const std::uint32_t>) override { ++count; }
};
}  // namespace

TEST_CASE("Native text parses Unicode and typed instant controls",
          "[native-text]") {
  auto parsed = text::ParseNativeText("A\xC3\xA9{{}}\n{btn:circle}{value:slot}",
                                      "text.test");
  REQUIRE(parsed);
  CHECK(parsed->size() == 7);
  CHECK((*parsed)[1].identity == "\xC3\xA9");
  CHECK((*parsed)[4].kind == text::TextTokenKind::kNewline);
  CHECK((*parsed)[5].identity == "{btn:circle}");
  CHECK((*parsed)[6].kind == text::TextTokenKind::kArgument);
}

TEST_CASE("Native text rejects unsupported meanings and malformed Unicode",
          "[native-text]") {
  for (const auto input : {"{wait:30}", "{confirm}", "{font:unknown}",
                           "{value:}", "{", "}", "\xC0\xAF", "\xED\xA0\x80"}) {
    auto result = text::ParseNativeText(input, "text.invalid");
    INFO(input);
    REQUIRE_FALSE(result);
    CHECK(result.error().find("text.invalid: text byte 0") !=
          std::string::npos);
  }
}

namespace {
struct ImportedTextFixture {
  text::DataStore::ScopedOverride restore;
  std::filesystem::path root;
  ImportedTextFixture() {
    static std::atomic<unsigned> counter = 0;
    root = std::filesystem::current_path() / "build" / "quality" /
           ("native-text-" +
            std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count()) +
            "-" + std::to_string(counter++));
    std::vector<std::byte> mcman(1600);
    const auto yes = text::EncodeGameText("{spacing:16}YES", "text.test.yes");
    const auto no = text::EncodeGameText("{spacing:16}NO", "text.test.no");
    REQUIRE(yes);
    REQUIRE(no);
    std::ranges::copy(*yes, mcman.begin() + 1558);
    std::ranges::copy(*no, mcman.begin() + 1564);
    Write("disc/MENU/MCMAN.BIN", mcman);
    Write("disc/TITLE/TITLE.PRG", std::vector<std::byte>(0x54A88));
    Write("files.tsv", Bytes("lba\tsize\tform\tpath\n100\t1600\traw\tMENU/"
                             "MCMAN.BIN\n200\t346760\traw\tTITLE/TITLE.PRG\n"));
    Catalogs();
    auto converted = VS::Extract::ConvertSaveText(root, false);
    INFO((converted ? "" : converted.error()));
    REQUIRE(converted);
  }
  ~ImportedTextFixture() {
    try {
      VS::Game::Save::ResetNativeFileText();
      restore.Restore();
      std::error_code error;
      std::filesystem::remove_all(root, error);
      if (error) std::abort();
    } catch (...) {
      std::abort();
    }
  }
  void Write(std::string_view file, std::span<const std::byte> bytes) const {
    const auto path = root / file;
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    REQUIRE(out);
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
    REQUIRE(out);
  }
  std::string Read(std::string_view file) const {
    std::ifstream input(root / file, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), {}};
  }
  void Catalogs() const {
    text::JsonDocument assets{{"schema", "vs.assets"},
                              {"schema_version", 1},
                              {"assets", text::JsonDocument::array()}};
    for (const auto& [path, size] :
         std::array{std::pair{"MENU/MCMAN.BIN", 1600},
                    std::pair{"TITLE/TITLE.PRG", 0x54A88}}) {
      assets["assets"].push_back(
          {{"id", text::AssetIdForDiscPath(path).value_or("")},
           {"file", std::string("disc/") + path},
           {"bytes", size},
           {"provenance",
            {{"disc_path", path}, {"lba", path[0] == 'M' ? 100 : 200}}}});
    }
    const auto table = [](int count) {
      auto rows = text::JsonDocument::array();
      for (int i = 0; i < count; ++i) {
        rows.push_back({{"legacyId", i}, {"asset", "asset.title.title.prg"}});
      }
      return rows;
    };
    text::JsonDocument audio{
        {"schema", "vs.audio"},
        {"schema_version", 1},
        {"music", table(200)},
        {"sfx_banks", table(100)},
        {"wave_banks", table(100)},
        {"menu",
         {{"sample_banks", {"asset.title.title.prg", "asset.title.title.prg"}},
          {"effect_sample_bank", "asset.title.title.prg"},
          {"effect_programs", "asset.title.title.prg"}}}};
    Write("catalogs/assets.json", Bytes(text::FormatJsonText(assets)));
    Write("catalogs/audio.json", Bytes(text::FormatJsonText(audio)));
  }
};
}  // namespace

TEST_CASE(
    "Native string arguments are literal and numbers are not byte limited",
    "[native-text]") {
  auto parsed =
      text::ParseNativeText("{value:file} {value:number}", "text.test");
  REQUIRE(parsed);
  text::TextArguments args{
      {"file", std::string("{btn:circle}\nname")},
      {"number", std::numeric_limits<std::int64_t>::max()}};
  auto bound = text::BindNativeText(*parsed, args, "text.test");
  REQUIRE(bound);
  std::string value;
  for (const auto& token : *bound) {
    CHECK(token.kind == text::TextTokenKind::kGlyph);
    value += token.identity;
  }
  CHECK(value == "{btn:circle} name 9223372036854775807");
  CHECK_FALSE(text::BindNativeText(*parsed, {}, "text.test"));
  args["file"] = std::string(65536, '\n');
  CHECK_FALSE(text::BindNativeText(*parsed, args, "text.test"));
}

TEST_CASE("Native text catalogs validate IDs duplicates and references",
          "[native-text]") {
  const std::string valid =
      R"({"schema":"vs.strings","schema_version":1,"font":"font.test","scale":1.25,"strings":{"text.save.confirm.load":"Load a longer message"}})";
  auto catalog = text::ParseNativeStrings(Bytes(valid));
  REQUIRE(catalog);
  CHECK(catalog->Find("text.save.confirm.load"));
  CHECK_FALSE(catalog->Find("text.missing"));
  CHECK_FALSE(text::ParseNativeStrings(Bytes(
      R"({"schema":"vs.strings","schema_version":1,"font":"font.test","scale":1,"strings":{"text.a":"A","text.a":"B"}})")));
  CHECK_FALSE(text::ParseNativeStrings(Bytes(
      R"({"schema":"vs.strings","schema_version":1,"font":"table-offset","scale":1,"strings":{"32":"A"}})")));
}

TEST_CASE("Native font metrics reject invalid atlas and glyph definitions",
          "[native-text]") {
  text::NativeImage image;
  image.width = 16;
  image.height = 16;
  text::JsonDocument font{{"schema", "vs.font"},
                          {"schema_version", 1},
                          {"id", "font.test"},
                          {"image", "image.test"},
                          {"line_height", 12},
                          {"glyphs",
                           {{"A",
                             {{"x", 0},
                              {"y", 0},
                              {"width", 12},
                              {"height", 12},
                              {"advance", 6},
                              {"visible", true}}}}}};
  CHECK(text::ParseNativeFont(Bytes(text::FormatJsonText(font)), image));
  font["glyphs"]["A"]["x"] = 10;
  CHECK_FALSE(text::ParseNativeFont(Bytes(text::FormatJsonText(font)), image));
  font["glyphs"]["A"]["x"] = 0;
  font["glyphs"]["A"]["advance"] = -1;
  CHECK_FALSE(text::ParseNativeFont(Bytes(text::FormatJsonText(font)), image));
}

TEST_CASE("Native layout wraps longer scaled text without truncating cautions",
          "[native-text]") {
  auto text_value = text::ParseNativeText(
      "Longer native save confirmation text beyond twenty four bytes",
      "text.long");
  REQUIRE(text_value);
  const auto font = Font();
  auto layout = render::LayoutNativeText(*text_value, font, {304, 30, 1.25});
  REQUIRE(layout);
  CHECK(layout->height == 30);
  CHECK(layout->glyphs.size() > 24);
  CHECK_FALSE(render::LayoutNativeText(*text_value, font, {20, 12, 1}));
  CHECK_FALSE(render::LayoutNativeText(
      *text_value, font, {304, 30, std::numeric_limits<double>::infinity()}));
  CHECK(render::LayoutNativeText(*text_value, font, {304, 30, 1.25})
            ->glyphs.size() == layout->glyphs.size());
}

TEST_CASE("Native layout refuses unbound or absent glyphs and owns geometry",
          "[native-text]") {
  auto value = text::ParseNativeText("AAA", "text.test");
  REQUIRE(value);
  auto font = Font();
  auto layout = render::LayoutNativeText(*value, font, {304, 30, 1});
  REQUIRE(layout);
  font.glyphs.clear();
  value->clear();
  CHECK(layout->glyphs.size() == 3);
  CHECK(layout->glyphs[2].x == 12);
  const std::array tokens{
      text::TextToken{text::TextTokenKind::kArgument, "name"}};
  CHECK_FALSE(render::LayoutNativeText(tokens, font, {304, 30, 1}));
}

TEST_CASE("Native word wrapping includes the last glyph paint extent",
          "[native-text]") {
  const auto value = text::ParseNativeText("AA BB", "text.test");
  REQUIRE(value);
  const auto layout = render::LayoutNativeText(*value, Font(), {30, 24, 1});
  REQUIRE(layout);
  REQUIRE(layout->glyphs.size() == 4);
  CHECK(layout->glyphs[2].x == 0);
  CHECK(layout->glyphs[2].y == 12);
  CHECK(layout->width == 18);
}

TEST_CASE("Native text backend checks complete draws before emitting packets",
          "[native-text]") {
  text::NativeImage atlas;
  atlas.depth = text::ImageDepth::kIndexed4;
  atlas.width = 16;
  atlas.height = 16;
  atlas.indices.resize(256);
  atlas.palettes.emplace_back(16);
  render::TextLayout layout{
      {{text::GlyphMetric{0, 0, 12, 12, 6, true}, 0, 0, 15, 15}}, 15, 15};
  const std::array draws{render::NativeTextDraw{&layout, 4, 4}};
  Target target;
  REQUIRE(render::DrawNativeText(target, draws, atlas));
  CHECK(target.count == 1);
  layout.glyphs.push_back(
      {text::GlyphMetric{15, 0, 12, 12, 6, true}, 0, 0, 15, 15});
  CHECK_FALSE(render::DrawNativeText(target, draws, atlas));
  CHECK(target.count == 1);
}

TEST_CASE("Save text conversion preserves edits and its manifest on repeat",
          "[native-text]") {
  ImportedTextFixture fixture;
  auto strings = text::JsonDocument::parse(fixture.Read(text::kSaveTextPath));
  strings["strings"]["text.save.confirm.load"] =
      "Longer native confirmation beyond twenty four bytes";
  fixture.Write(text::kSaveTextPath, Bytes(text::FormatJsonText(strings)));
  const auto before = fixture.Read(text::kSaveTextPath);
  const auto manifest = fixture.Read("manifest.json");
  const auto stamp =
      std::filesystem::last_write_time(fixture.root / text::kSaveTextPath);
  REQUIRE(VS::Extract::ConvertSaveText(fixture.root, false));
  CHECK(fixture.Read(text::kSaveTextPath) == before);
  CHECK(fixture.Read("manifest.json") == manifest);
  CHECK(std::filesystem::last_write_time(fixture.root / text::kSaveTextPath) ==
        stamp);
  CHECK(strings["strings"]["text.save.choice.yes"] == "YES");
}

TEST_CASE("Save text layers load transactionally and views own reset snapshots",
          "[native-text]") {
  ImportedTextFixture fixture;
  fixture.Write(
      "mod/text/strings.en.json",
      Bytes(
          R"({"schema":"vs.strings","schema_version":1,"font":"font.ui.save","scale":1.25,"strings":{"text.save.confirm.load":"Load this longer native save confirmation after checking it"}})"));
  fixture.Write(
      "catalog-layers.json",
      Bytes(
          R"({"schema":"vs.catalog-layers","schema_version":1,"layers":["mod"]})"));
  const auto opened = text::DataStore::Open(fixture.root);
  INFO((opened ? "" : opened.error()));
  REQUIRE(opened);
  const auto* original = text::DataStore::Get();
  REQUIRE(original != nullptr);
  REQUIRE(original->SaveText() != nullptr);
  CHECK(original->SaveText()->catalog.scale == 1.25);
  CHECK_NOTHROW(VS::Game::Save::BeginNativeFileText());
  fixture.Write(
      "mod/text/strings.en.json",
      Bytes(
          R"({"schema":"vs.strings","schema_version":1,"font":"font.wrong","scale":1,"strings":{}})"));
  CHECK_FALSE(text::DataStore::Open(fixture.root));
  CHECK(text::DataStore::Get() == original);
  CHECK_NOTHROW(VS::Game::Save::SelectNativeFileText("text.save.confirm.load"));
  VS::Game::Save::ResetNativeFileText();
  CHECK_NOTHROW(VS::Game::Save::BeginNativeFileText());
}

namespace {
void Contribute(const ImportedTextFixture& fixture,
                const VS::Extract::TextContribution& contribution,
                bool force = false) {
  auto start = VS::Extract::StartRun(fixture.root);
  REQUIRE(start);
  if (!start) throw std::runtime_error(start.error());
  auto prepared = VS::Extract::PrepareTextContribution(
      fixture.root, start->previous, contribution, force);
  REQUIRE(prepared);
  if (!prepared) throw std::runtime_error(prepared.error());
  std::vector<VS::Extract::OutputBytes> files;
  for (const auto& file : prepared->files) {
    files.push_back({file.name, file.bytes});
  }
  REQUIRE(VS::Extract::WriteOutputFiles(fixture.root, files));
  REQUIRE(VS::Extract::PublishBatches(fixture.root, start->previous,
                                      std::span(&prepared->batch, 1)));
}
}  // namespace

TEST_CASE(
    "Shared text contributions preserve foreign owners edits and stable repeat "
    "timestamps",
    "[native-text][bestiary]") {
  ImportedTextFixture fixture;
  auto strings = text::JsonDocument::parse(fixture.Read(text::kSaveTextPath));
  strings["strings"]["text.save.select"] = "My authored save selection";
  fixture.Write(text::kSaveTextPath, Bytes(text::FormatJsonText(strings)));
  const VS::Extract::TextContribution contribution{
      "bestiary",
      "text.bestiary.",
      {"SMALL/MON.BIN", std::string(64, 'a')},
      {{"text.bestiary.notes.name", "Native field notes"}}};
  Contribute(fixture, contribution);
  CHECK(text::JsonDocument::parse(
            fixture.Read(text::kSaveTextPath))["strings"]["text.save.select"] ==
        "My authored save selection");
  REQUIRE(VS::Extract::ConvertSaveText(fixture.root, true));
  strings = text::JsonDocument::parse(fixture.Read(text::kSaveTextPath));
  CHECK(strings["strings"]["text.bestiary.notes.name"] == "Native field notes");
  const auto before = fixture.Read(text::kSaveTextPath);
  const auto stamp =
      std::filesystem::last_write_time(fixture.root / text::kSaveTextPath);
  Contribute(fixture, contribution);
  CHECK(fixture.Read(text::kSaveTextPath) == before);
  CHECK(std::filesystem::last_write_time(fixture.root / text::kSaveTextPath) ==
        stamp);
}

TEST_CASE(
    "Shared text contributions reject ID collisions and invalid native "
    "controls before publication",
    "[native-text][bestiary]") {
  ImportedTextFixture fixture;
  auto start = VS::Extract::StartRun(fixture.root);
  REQUIRE(start);
  if (!start) throw std::runtime_error(start.error());
  VS::Extract::TextContribution collision{
      "bestiary",
      "text.",
      {"SMALL/MON.BIN", std::string(64, 'a')},
      {{"text.save.select", "Wrong owner"}}};
  CHECK_FALSE(VS::Extract::PrepareTextContribution(
      fixture.root, start->previous, collision, false));
  collision.strings = {{"text.bestiary.notes", "{spacing:40}Opaque"}};
  CHECK_FALSE(VS::Extract::PrepareTextContribution(
      fixture.root, start->previous, collision, false));
  CHECK(text::JsonDocument::parse(fixture.Read(text::kSaveTextPath))["strings"]
            .size() == 27);
}

TEST_CASE(
    "Shared text refuses multiple physical catalog owners in import and "
    "runtime",
    "[native-text][bestiary]") {
  ImportedTextFixture fixture;
  REQUIRE(text::DataStore::Open(fixture.root));
  const auto* original = text::DataStore::Get();
  auto manifest = text::JsonDocument::parse(fixture.Read("manifest.json"));
  auto other = manifest["outputs"].front();
  other["id"] = "text.other-owner";
  other["files"] = {std::string(text::kSaveTextPath)};
  manifest["outputs"].push_back(other);
  fixture.Write("manifest.json", Bytes(text::FormatJsonText(manifest)));
  auto start = VS::Extract::StartRun(fixture.root);
  REQUIRE(start);
  if (!start) throw std::runtime_error(start.error());
  const VS::Extract::TextContribution contribution{
      "bestiary",
      "text.bestiary.",
      {"SMALL/MON.BIN", std::string(64, 'a')},
      {{"text.bestiary.notes", "Notes"}}};
  CHECK_FALSE(VS::Extract::PrepareTextContribution(
      fixture.root, start->previous, contribution, false));
  CHECK_FALSE(text::DataStore::Open(fixture.root));
  CHECK(text::DataStore::Get() == original);
}
