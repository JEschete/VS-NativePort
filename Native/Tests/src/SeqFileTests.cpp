#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <functional>
#include <initializer_list>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Content/ActorAnimation.hpp"
#include "Content/JsonText.hpp"
#include "Content/SeqFile.hpp"
#include "Content/ShpFile.hpp"
#include "TestSupport/ModelFixtures.hpp"

namespace {

namespace fs = std::filesystem;
namespace content = VS::Content;
using Bytes = std::vector<std::byte>;
using Lengths = std::array<std::uint8_t, content::kSeqEventLengthCount>;
using Axes = std::array<std::optional<std::int16_t>, content::kSeqAxes>;
using Tallies = std::map<std::string, std::size_t, std::less<>>;
using VS::test_support::ErrorOf;
using VS::test_support::ExtractedModels;
using VS::test_support::ReadModel;

constexpr std::nullopt_t kNo = std::nullopt;
constexpr std::size_t kSampleBones = 2;
// The sample bank's layout: three 18-byte clip headers, then a 4-entry map.
constexpr std::size_t kClip0 = 8;
constexpr std::size_t kClip1 = 26;
constexpr std::size_t kMapAt = 62;
constexpr std::size_t kMotionAt = 66;

// A made-up stride table: each handled opcode takes 1 + op % 3 bytes.
Lengths TestLengths() {
  Lengths lengths{};
  for (std::size_t op = 0; op < lengths.size(); ++op) {
    lengths.at(op) = static_cast<std::uint8_t>(1 + op % 3);
  }
  return lengths;
}

Bytes Of(std::initializer_list<int> values) {
  Bytes bytes;
  for (const int value : values) {
    bytes.push_back(static_cast<std::byte>(value));
  }
  return bytes;
}

content::SeqCommand Command(std::uint16_t frames, Axes axes) {
  return {.frames = frames, .axes = axes};
}

content::SeqTrack Track(content::SeqStart start,
                        std::array<std::int16_t, content::kSeqAxes> initial,
                        std::vector<content::SeqCommand> commands) {
  return {.start = start,
          .initial = initial,
          .commands = std::move(commands),
          .terminated = true};
}

content::SeqTrack Vector(std::array<std::int16_t, content::kSeqAxes> initial,
                         std::vector<content::SeqCommand> commands) {
  return Track(content::SeqStart::kVector, initial, std::move(commands));
}

// Elements 0-2 are vector tracks, 3 and 4 the two scale forms, 5 an event
// script, 6 a travel track, 7 a track of commands alone and 8 unread bytes.
std::vector<content::SeqElement> SampleMotion() {
  using content::SeqStart;
  return {
      Vector({0, -4, 8190},
             {Command(3, {1, kNo, -2}), Command(40, {kNo, 7, kNo})}),
      Vector({2, 0, 0}, {Command(1, {300, kNo, kNo})}),
      Vector({-8192, 0, 16382}, {}),
      Track(SeqStart::kPackedScale, {64, 64, -16384}, {Command(2, {1, 1, 1})}),
      Track(SeqStart::kRawScale, {64, 0, 255}, {}),
      content::SeqEvents{.events = {{.frame = 0, .op = 1, .arguments = {9}},
                                    {.frame = 300,
                                     .op = 41,
                                     .arguments = {1, 2, 3, 4, 5, 6, 7}},
                                    {.frame = 5, .op = 0, .arguments = {}}}},
      Vector({10, 20, 30}, {Command(2, {kNo, kNo, kNo})}),
      Track(SeqStart::kNone, {},
            {Command(2, {-1, kNo, kNo}), Command(1, {kNo, 4000, kNo})}),
      content::SeqUnread{.bytes = Of({0xAB, 0xCD})},
  };
}

// An own clip reading every kind of element, a clip inheriting its keys that
// shares commands and reads an empty track, and one that plays scale tracks.
content::SeqBank SampleBank() {
  using content::SeqEmpty;
  using content::SeqEntry;
  using content::SeqOwn;
  content::SeqBank bank;
  bank.motion = SampleMotion();
  bank.clips = {
      {.length = 50,
       .base = std::nullopt,
       .flags = 3,
       .events = 5,
       .root = SeqOwn{.element = 0},
       .travel = 6,
       .rotations = {SeqOwn{.element = 1}, SeqOwn{.element = 2}},
       .scales = std::vector<content::SeqRef>{SeqOwn{.element = 3},
                                              SeqOwn{.element = 4}}},
      {.length = 4,
       .base = 0,
       .flags = 0,
       .events = std::nullopt,
       .root = SeqEntry{.element = 7, .command = 0},
       .travel = std::nullopt,
       .rotations = {SeqEntry{.element = 0, .command = 1}, SeqEmpty{.hint = 0}},
       .scales = std::nullopt},
      {.length = 1,
       .base = 0,
       .flags = 2,
       .events = std::nullopt,
       .root = SeqEntry{.element = 2, .command = 0},
       .travel = std::nullopt,
       .rotations = {SeqEntry{.element = 1, .command = 0},
                     SeqEntry{.element = 1, .command = 1}},
       .scales =
           std::vector<content::SeqRef>{SeqEntry{.element = 3, .command = 0},
                                        SeqEntry{.element = 7, .command = 2}}},
  };
  bank.map = {0, std::nullopt, 1, 2};
  return bank;
}

Bytes Encoded(const content::SeqBank& bank) {
  const auto bytes = content::EncodeSeqBank(bank, kSampleBones, TestLengths());
  INFO(ErrorOf(bytes));
  REQUIRE(bytes.has_value());
  return *bytes;
}

std::string EncodeError(const content::SeqBank& bank,
                        std::size_t bones = kSampleBones) {
  const auto bytes = content::EncodeSeqBank(bank, bones, TestLengths());
  REQUIRE_FALSE(bytes.has_value());
  return bytes.error();
}

std::string DecodeError(const Bytes& bytes) {
  const auto bank = content::DecodeSeqBank(bytes, kSampleBones, TestLengths());
  REQUIRE_FALSE(bank.has_value());
  return bank.error();
}

void CheckContains(const std::string& error, std::string_view part) {
  INFO(error);
  CHECK(error.find(part) != std::string::npos);
}

// A boneless bank whose one clip plays a track of `command` alone.
content::SeqBank LoneCommand(const content::SeqCommand& command) {
  content::SeqBank bank;
  bank.motion = {Vector({}, {command})};
  bank.clips = {{.length = 1,
                 .base = std::nullopt,
                 .flags = 0,
                 .events = std::nullopt,
                 .root = content::SeqOwn{.element = 0},
                 .travel = std::nullopt,
                 .rotations = {},
                 .scales = std::nullopt}};
  bank.map = {0};
  return bank;
}

// The bytes a lone command encodes to.
Bytes CommandBytes(const content::SeqCommand& command) {
  constexpr std::size_t kCommandAt = 25;  // header, clip, map and key
  const auto bytes =
      content::EncodeSeqBank(LoneCommand(command), 0, TestLengths());
  INFO(ErrorOf(bytes));
  REQUIRE(bytes.has_value());
  REQUIRE(bytes->size() > kCommandAt);
  CHECK(bytes->back() == std::byte{0});
  return {bytes->begin() + kCommandAt, bytes->end() - 1};
}

std::string CommandError(const content::SeqCommand& command) {
  const auto bytes =
      content::EncodeSeqBank(LoneCommand(command), 0, TestLengths());
  REQUIRE_FALSE(bytes.has_value());
  return bytes.error();
}

TEST_CASE("SEQ banks round-trip every element and reference",
          "[content-models][seq]") {
  const content::SeqBank bank = SampleBank();
  const Bytes bytes = Encoded(bank);
  const auto decoded =
      content::DecodeSeqBank(bytes, kSampleBones, TestLengths());
  INFO(ErrorOf(decoded));
  REQUIRE(decoded.has_value());
  CHECK(*decoded == bank);
  CHECK(Encoded(*decoded) == bytes);
}

TEST_CASE("Decoded SEQ tracks grow and shrink without breaking references",
          "[content-models][seq]") {
  content::SeqBank edited = SampleBank();
  std::get<content::SeqTrack>(edited.motion.at(0))
      .commands.push_back(Command(1, {kNo, kNo, 3}));
  std::get<content::SeqTrack>(edited.motion.at(6)).commands.clear();
  const Bytes bytes = Encoded(edited);
  CHECK(bytes.size() == Encoded(SampleBank()).size() + 2 - 3);
  const auto decoded =
      content::DecodeSeqBank(bytes, kSampleBones, TestLengths());
  INFO(ErrorOf(decoded));
  REQUIRE(decoded.has_value());
  CHECK(*decoded == edited);
}

TEST_CASE("SEQ files carry the map length, bones and rounded size",
          "[content-models][seq]") {
  const content::SeqFile file{.bones = kSampleBones, .bank = SampleBank()};
  const auto bytes = content::EncodeSeqFile(file, TestLengths());
  INFO(ErrorOf(bytes));
  REQUIRE(bytes.has_value());
  const std::size_t bank = Encoded(file.bank).size();
  CHECK(bytes->size() == 8 + bank);
  CHECK(bytes->at(0) == std::byte{4});
  CHECK(bytes->at(2) == std::byte{kSampleBones});
  CHECK(std::to_integer<std::size_t>(bytes->at(4)) == (bank + 3) / 4 * 4);
  const auto decoded = content::DecodeSeqFile(*bytes, TestLengths());
  INFO(ErrorOf(decoded));
  REQUIRE(decoded.has_value());
  CHECK(*decoded == file);
  SECTION("pad byte") {
    Bytes bad = *bytes;
    bad.at(1) = std::byte{1};
    CheckContains(ErrorOf(content::DecodeSeqFile(bad, TestLengths())),
                  "byte 0x1: the byte after the map length");
  }
  SECTION("size") {
    Bytes bad = *bytes;
    bad.at(4) = std::byte{0};
    CheckContains(ErrorOf(content::DecodeSeqFile(bad, TestLengths())),
                  "the size field says 0 bytes");
  }
  SECTION("map length") {
    Bytes bad = *bytes;
    bad.at(0) = std::byte{3};
    CheckContains(ErrorOf(content::DecodeSeqFile(bad, TestLengths())),
                  "the map length byte says 3");
  }
}

TEST_CASE("SEQ commands take the one form the encoder writes",
          "[content-models][seq]") {
  // Byte deltas, with a long run in an extra byte.
  CHECK(CommandBytes(Command(3, {1, kNo, -2})) == Of({0xA2, 0x01, 0xFE}));
  CHECK(CommandBytes(Command(40, {kNo, 7, kNo})) == Of({0x5F, 0x08, 0x07}));
  CHECK(CommandBytes(Command(287, {1, kNo, kNo})) == Of({0x9F, 0xFF, 0x01}));
  // Word deltas: every present axis sets its byte bit, the first word axis
  // rides in the packed word's top 13 bits, later ones follow as words, then
  // the byte axes.
  CHECK(CommandBytes(Command(2, {200, -300, 100})) ==
        Of({0x1D, 0x06, 0x46, 0xFE, 0xD4, 0x64}));
  CHECK(CommandBytes(Command(5, {kNo, kNo, kNo})) ==
        Of({0x03, 0x01, 0x00, 0x00}));
  // A first word axis past 13 bits makes an earlier axis carry the word.
  CHECK(CommandBytes(Command(2, {5, 5000, kNo})) ==
        Of({0x19, 0x00, 0x2E, 0x13, 0x88}));
  // A word clears its axis's byte bit, so a 1-frame command of words alone
  // still starts with a nonzero byte.
  CHECK(CommandBytes(Command(1, {300, kNo, kNo})) == Of({0x10, 0x09, 0x64}));
  CHECK(CommandBytes(Command(1, {kNo, 4000, kNo})) == Of({0x08, 0x7D, 0x02}));
}

TEST_CASE("SEQ encoding refuses commands the game cannot read",
          "[content-models][seq]") {
  CheckContains(CommandError(Command(1, {kNo, kNo, kNo})),
                "a 1-frame command must change an axis");
  CheckContains(CommandError(Command(2, {5000, kNo, kNo})),
                "the x delta 5000 needs more than 13 bits");
  CheckContains(CommandError(Command(0, {1, kNo, kNo})), "0 frames do not fit");
  CheckContains(CommandError(Command(288, {1, kNo, kNo})),
                "288 frames do not fit a command of byte deltas");
  CheckContains(CommandError(Command(260, {kNo, kNo, 300})),
                "260 frames do not fit a command of word deltas");
}

TEST_CASE("SEQ encoding refuses what the bank cannot hold",
          "[content-models][seq]") {
  content::SeqBank bank = SampleBank();
  auto track = [&bank](std::size_t element) -> content::SeqTrack& {
    return std::get<content::SeqTrack>(bank.motion.at(element));
  };
  SECTION("odd key") {
    track(0).initial.at(1) = -3;
    CheckContains(EncodeError(bank), "motion element 0: the y key -3 is odd");
  }
  SECTION("raw scale read as packed") {
    track(4).initial = {0xFE, 1, 0};
    CheckContains(EncodeError(bank), "FE 01 would read as a packed scale");
  }
  SECTION("playback past an unterminated track") {
    track(0).terminated = false;
    CheckContains(EncodeError(bank),
                  "clip 0's root: motion element 0 has no terminator");
  }
  SECTION("flags") {
    bank.clips.at(1).flags = 4;
    CheckContains(EncodeError(bank), "clip 1: flags 0x4 hold bits");
  }
  SECTION("inherited base") {
    bank.clips.at(1).base = 2;
    CheckContains(EncodeError(bank), "clip 1: base 2 is not an own clip");
  }
  SECTION("events") {
    bank.clips.at(0).events = 0;
    CheckContains(EncodeError(bank), "which is not an event script");
  }
  SECTION("script end") {
    std::get<content::SeqEvents>(bank.motion.at(5)).events.back().op = 1;
    CheckContains(EncodeError(bank), "the last event's op 1 does not end");
  }
  SECTION("event arguments") {
    std::get<content::SeqEvents>(bank.motion.at(5)).events.front().arguments =
        {};
    CheckContains(EncodeError(bank), "has 0 argument bytes, not 1");
  }
  SECTION("missing scales") {
    bank.clips.at(0).scales.reset();
    CheckContains(EncodeError(bank), "its flags read scales, but it has none");
  }
  SECTION("key read from commands") {
    bank.clips.at(0).rotations.at(0) =
        content::SeqEntry{.element = 1, .command = 0};
    CheckContains(EncodeError(bank), "bone 0's rotation reads a key");
  }
  SECTION("bones") {
    CheckContains(EncodeError(bank, content::kSeqMaxBones + 1),
                  "42 bones is more than the 41");
  }
}

TEST_CASE("SEQ decoding names the bytes it rejects", "[content-models][seq]") {
  const Bytes good = Encoded(SampleBank());
  Bytes bytes = good;
  SECTION("flags") {
    bytes.at(kClip0 + 3) = std::byte{7};
    CheckContains(DecodeError(bytes), "byte 0xB: clip 0's flags 0x7");
  }
  SECTION("base") {
    bytes.at(kClip1 + 2) = std::byte{2};
    CheckContains(DecodeError(bytes), "clip 1's base 2 is not an own clip");
  }
  SECTION("map") {
    bytes.at(kMapAt + 1) = std::byte{9};
    CheckContains(DecodeError(bytes), "byte 0x3F: the map names clip 9 of 3");
  }
  SECTION("unread scale word") {
    bytes.at(kClip1 + 14) = std::byte{1};
    CheckContains(DecodeError(bytes), "bone 0's scale word is not zero");
  }
  SECTION("doubled key") {
    bytes.at(kMotionAt) = std::byte{0x40};
    CheckContains(DecodeError(bytes), "byte 0x42: a key's top two bits differ");
  }
  SECTION("element read two ways") {
    bytes.at(kClip0 + 8) = bytes.at(kClip0 + 4);
    bytes.at(kClip0 + 9) = bytes.at(kClip0 + 5);
    CheckContains(DecodeError(bytes),
                  "read as both an event script and a pose key");
  }
  SECTION("command form") {
    const Bytes command = Of({0x10, 0x09, 0x64});
    const auto found = std::ranges::search(bytes, command);
    REQUIRE_FALSE(found.empty());
    const auto at = static_cast<std::size_t>(found.begin() - bytes.begin());
    bytes.at(at + 1) = std::byte{0x00};
    bytes.at(at + 2) = std::byte{0x0C};
    CheckContains(DecodeError(bytes),
                  "a command is not in the form the encoder writes");
  }
}

// The sample bank as an actor-animation document.
content::ActorAnimationDocument SampleDocument() {
  return {.source = "OBJ/SAMPLE.SEQ",
          .source_sha256 =
              "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934c"
              "a495991b7852b855",
          .file = {.bones = kSampleBones, .bank = SampleBank()}};
}

content::JsonDocument SampleFixture() {
  constexpr std::string_view kFixture =
      "Native/Tests/data/animation/"
      "sample-bank.json";
  auto directory = fs::current_path();
  while (!fs::exists(directory / kFixture) &&
         directory != directory.parent_path()) {
    directory = directory.parent_path();
  }
  const auto json =
      content::ParseJsonText(ReadModel(directory / kFixture), kFixture);
  INFO(ErrorOf(json));
  REQUIRE(json.has_value());
  return *json;
}

content::JsonDocument Reparsed(const content::JsonDocument& json) {
  const std::string text = content::FormatJsonText(json);
  const auto parsed =
      content::ParseJsonText(std::as_bytes(std::span(text)), "text");
  INFO(ErrorOf(parsed));
  REQUIRE(parsed.has_value());
  return *parsed;
}

TEST_CASE("The actor-animation form writes and reads the sample bank",
          "[content-models][seq]") {
  const content::ActorAnimationDocument document = SampleDocument();
  const content::JsonDocument json = content::ActorAnimationJson(document);
  INFO(content::FormatJsonText(json));
  CHECK(json == SampleFixture());
  const auto read =
      content::ActorAnimationOf(Reparsed(json), "sample-bank.json");
  INFO(ErrorOf(read));
  REQUIRE(read.has_value());
  CHECK(*read == document);
  CHECK(Encoded(read->file.bank) == Encoded(SampleBank()));
}

TEST_CASE("The actor-animation reader names the member at fault",
          "[content-models][seq]") {
  using content::JsonDocument;
  struct Edit {
    std::string_view pointer;
    JsonDocument value;
    std::string_view error;
  };
  const std::vector<Edit> edits{
      {"/schema_version", 2, "s is vs.actor-animation version 2; this build"},
      {"/source_sha256", "ABC", "s.source_sha256 must be a lowercase SHA-256"},
      {"/bones", 42, "s.bones must be a whole number from 1 to 41"},
      {"/bank/map/1", 255,
       "s.bank.map[1] must be a whole number from 0 to "
       "254 or null"},
      {"/bank/clips/0/base", -1,
       "s.bank.clips[0].base must be a whole number "
       "from 0 to 254 or null"},
      {"/bank/clips/0/extra", 1, "s.bank.clips[0] has an unknown member"},
      {"/bank/clips/1/root", JsonDocument::parse(R"({"track": 7})"),
       "s.bank.clips[1].root.command is missing"},
      {"/bank/clips/2/scales", 1,
       "s.bank.clips[2].scales must be an array "
       "or null"},
      {"/bank/motion/0/commands/4", 0,
       "s.bank.motion[0].commands[4] must be "
       "a whole number from 1 to 65535"},
      {"/bank/motion/1/commands/1", 1.5,
       "s.bank.motion[1].commands[1] must "
       "be a whole number"},
      {"/bank/motion/1/commands", JsonDocument::parse("[1, 300, null]"),
       "s.bank.motion[1].commands must hold frames, x, y, z"},
      {"/bank/motion/1/raw_scale", JsonDocument::parse("[1, 2, 3]"),
       "s.bank.motion[1] has more than one initial key"},
      {"/bank/motion/3/packed_scale/2", -16385,
       "s.bank.motion[3].packed_scale[2] must be a whole number from -16384"},
      {"/bank/motion/4/raw_scale/1", 256,
       "s.bank.motion[4].raw_scale[1] must be a whole number from 0 to 255"},
      {"/bank/motion/5/events/2", JsonDocument::parse("[5]"),
       "s.bank.motion[5].events[2] must be an array of a frame, an op"},
      {"/bank/motion/8/unread", "ABCD",
       "s.bank.motion[8].unread must be bytes in lowercase hexadecimal"},
      {"/bank/motion/8/unread", "abc",
       "s.bank.motion[8].unread must be bytes in lowercase hexadecimal"},
  };
  for (const Edit& edit : edits) {
    CAPTURE(edit.pointer);
    JsonDocument json = content::ActorAnimationJson(SampleDocument());
    json[JsonDocument::json_pointer(std::string(edit.pointer))] = edit.value;
    const auto read = content::ActorAnimationOf(json, "s");
    REQUIRE_FALSE(read.has_value());
    CheckContains(read.error(), edit.error);
  }
}

// The game's event stride table, from BATTLE.PRG beside the models.
Lengths GameLengths(const fs::path& models) {
  const Bytes prg = ReadModel(models.parent_path() / "BATTLE" / "BATTLE.PRG");
  REQUIRE(prg.size() >=
          content::kSeqEventLengthsAt + content::kSeqEventLengthCount);
  Lengths lengths{};
  for (std::size_t op = 0; op < lengths.size(); ++op) {
    lengths.at(op) =
        std::to_integer<std::uint8_t>(prg.at(content::kSeqEventLengthsAt + op));
  }
  return lengths;
}

void CountRef(const content::SeqRef& ref, Tallies& tallies) {
  if (std::holds_alternative<content::SeqEmpty>(ref)) {
    ++tallies["empty refs"];
  } else if (const auto* entry = std::get_if<content::SeqEntry>(&ref);
             entry != nullptr && entry->command != 0) {
    ++tallies["mid-track refs"];
  }
}

void Count(const content::SeqBank& bank, Tallies& tallies) {
  constexpr std::array<std::string_view, 3> kElements{"tracks", "scripts",
                                                      "unread"};
  for (const content::SeqClip& clip : bank.clips) {
    ++tallies[clip.base ? "inherited clips" : "own clips"];
    CountRef(clip.root, tallies);
    for (const content::SeqRef& ref : clip.rotations) CountRef(ref, tallies);
    for (const content::SeqRef& ref :
         clip.scales.value_or(std::vector<content::SeqRef>{})) {
      CountRef(ref, tallies);
    }
  }
  for (const content::SeqElement& element : bank.motion) {
    ++tallies[std::string(kElements.at(element.index()))];
    const auto* track = std::get_if<content::SeqTrack>(&element);
    if (track == nullptr) continue;
    tallies["commands"] += track->commands.size();
    if (!track->terminated) ++tallies["unterminated"];
  }
}

std::string Describe(const Tallies& tallies) {
  std::string text;
  for (const auto& [name, count] : tallies) {
    text += std::format("{}: {}\n", name, count);
  }
  return text;
}

// Checks `bank` reads back unchanged from its actor-animation JSON text.
void CheckJsonRebuilds(const content::SeqBank& bank) {
  const auto read =
      content::SeqBankOf(Reparsed(content::SeqBankJson(bank)), "bank");
  INFO(ErrorOf(read));
  REQUIRE(read.has_value());
  CHECK(*read == bank);
}

// Decodes `bytes` as a bank and checks it rebuilds them; false on failure.
bool CheckBankRebuilds(std::span<const std::byte> bytes, std::size_t bones,
                       const Lengths& lengths, Tallies& tallies) {
  const auto decoded = content::DecodeSeqBank(bytes, bones, lengths);
  if (!decoded) {
    FAIL_CHECK(decoded.error());
    return false;
  }
  const auto rebuilt = content::EncodeSeqBank(*decoded, bones, lengths);
  INFO(ErrorOf(rebuilt));
  REQUIRE(rebuilt.has_value());
  CHECK(std::ranges::equal(*rebuilt, bytes));
  CheckJsonRebuilds(*decoded);
  Count(*decoded, tallies);
  return true;
}

TEST_CASE("Extracted SEQ files decode and rebuild their exact bytes",
          "[content-models][seq][extract]") {
  const auto models = ExtractedModels();
  if (!models) SKIP("Extracted Native/data is unavailable");
  const Lengths lengths = GameLengths(*models);
  Tallies tallies;
  for (const auto& entry : fs::directory_iterator(*models)) {
    if (entry.path().extension() != ".SEQ") continue;
    CAPTURE(entry.path().filename().string());
    const Bytes bytes = ReadModel(entry.path());
    const auto decoded = content::DecodeSeqFile(bytes, lengths);
    if (!decoded) {
      FAIL_CHECK(decoded.error());
      continue;
    }
    const auto rebuilt = content::EncodeSeqFile(*decoded, lengths);
    INFO(ErrorOf(rebuilt));
    REQUIRE(rebuilt.has_value());
    CHECK(*rebuilt == bytes);
    CheckJsonRebuilds(decoded->bank);
    Count(decoded->bank, tallies);
    ++tallies["files"];
  }
  // Tracks often run on into the next one; the game stops at the clip's
  // length, so most need no terminator.
  const Tallies expected{{"commands", 2416326},
                         {"empty refs", 6391},
                         {"files", 700},
                         {"inherited clips", 1935},
                         {"mid-track refs", 26566},
                         {"own clips", 12556},
                         {"scripts", 9297},
                         {"tracks", 262258},
                         {"unterminated", 148454}};
  INFO(Describe(tallies));
  CHECK(tallies == expected);
}

TEST_CASE("Banks embedded in extracted SHP files rebuild their exact bytes",
          "[content-models][seq][extract]") {
  const auto models = ExtractedModels();
  if (!models) SKIP("Extracted Native/data is unavailable");
  const Lengths lengths = GameLengths(*models);
  Tallies tallies;
  for (const auto& entry : fs::directory_iterator(*models)) {
    if (entry.path().extension() != ".SHP") continue;
    CAPTURE(entry.path().filename().string());
    const Bytes bytes = ReadModel(entry.path());
    const auto source = content::PeekShpTexture(bytes);
    REQUIRE(source.has_value());
    std::optional<bool> lender;
    if (source->lender) {
      lender = VS::test_support::LenderColored(*models, *source->lender);
    }
    const auto shp = content::DecodeShp(bytes, lender);
    INFO(ErrorOf(shp));
    REQUIRE(shp.has_value());
    if (CheckBankRebuilds(shp->sequence, shp->bones.size(), lengths, tallies)) {
      ++tallies["files"];
    }
  }
  // The embedded banks hold only each bone's initial key.
  const Tallies expected{{"commands", 0},  {"files", 202},   {"own clips", 202},
                         {"scripts", 140}, {"tracks", 4791}, {"unread", 115}};
  INFO(Describe(tallies));
  CHECK(tallies == expected);
}

}  // namespace
