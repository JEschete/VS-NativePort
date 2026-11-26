#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Content/ActorAnimation.hpp"
#include "Content/ActorArchetype.hpp"
#include "Content/ConvertedModels.hpp"
#include "Content/JsonText.hpp"
#include "Content/SeqFile.hpp"
#include "Content/ZudFile.hpp"
#include "TestSupport/ModelFixtures.hpp"

namespace {

namespace fs = std::filesystem;
namespace content = VS::Content;
using Bytes = std::vector<std::byte>;
using VS::test_support::ErrorOf;
using VS::test_support::ExtractedModels;
using VS::test_support::LittleWord;
using VS::test_support::ReadModel;

constexpr std::size_t kExtractedZudCount = 455;
// The weapon section's offset word in the header.
constexpr std::size_t kWeaponOffsetAt = 0x10;

// Identity bytes 1 to 7, then the pad byte; section i holds i + 1 bytes of
// value i + 1, except an empty shield.
content::ZudFile SampleZud() {
  content::ZudFile zud;
  for (std::size_t index = 0; index + 1 < zud.identity.size(); ++index) {
    zud.identity.at(index) = static_cast<std::uint8_t>(index + 1);
  }
  for (std::size_t index = 0; index < zud.sections.size(); ++index) {
    zud.sections.at(index).assign(index + 1, static_cast<std::byte>(index + 1));
  }
  zud.sections.at(static_cast<std::size_t>(content::ZudSection::kShield))
      .clear();
  return zud;
}

Bytes Encoded(const content::ZudFile& zud) {
  return VS::test_support::Required(content::EncodeZudFile(zud));
}

TEST_CASE("ZUD files round-trip their identity and packed sections",
          "[content-models][zud]") {
  const content::ZudFile zud = SampleZud();
  const Bytes bytes = Encoded(zud);
  REQUIRE(bytes.size() == content::kZudHeaderBytes + 12);
  CHECK(std::to_integer<int>(bytes.at(6)) == 7);
  CHECK(std::to_integer<int>(bytes.at(7)) == 0);
  // Offset and size words: the empty shield sits where the next one starts.
  const std::vector<std::uint32_t> words{0x30, 1,    0x31, 2,    0x33,
                                         0,    0x33, 4,    0x37, 5};
  for (std::size_t word = 0; word < words.size(); ++word) {
    CAPTURE(word);
    CHECK(LittleWord(bytes, content::kZudIdentityBytes + (4 * word)) ==
          words.at(word));
  }
  CHECK(content::DecodeZudFile(bytes) == zud);
  const content::ZudFile empty;
  const Bytes header = Encoded(empty);
  CHECK(header.size() == content::kZudHeaderBytes);
  CHECK(content::DecodeZudFile(header) == empty);
}

TEST_CASE("ZUD decoding names the bytes it rejects", "[content-models][zud]") {
  struct Refused {
    Bytes bytes;
    std::string_view error;
  };
  const Bytes bytes = Encoded(SampleZud());
  Bytes short_header(bytes.begin(),
                     bytes.begin() + content::kZudHeaderBytes - 1);
  Bytes cut = bytes;
  cut.pop_back();
  Bytes trailing = bytes;
  trailing.push_back(std::byte{0});
  Bytes gap = bytes;
  gap.at(kWeaponOffsetAt) = std::byte{0x32};
  const std::vector<Refused> refusals{
      {short_header, "47 bytes are too short for the 48-byte header"},
      {cut,
       "the battle animation section's 5 bytes at 0x37 run past the 59-byte "
       "file"},
      {trailing, "1 bytes follow the battle animation section"},
      {gap,
       "the weapon section starts at 0x32, not 0x31 where the one before it "
       "ends"},
  };
  for (const Refused& refused : refusals) {
    CAPTURE(refused.error);
    CHECK(ErrorOf(content::DecodeZudFile(refused.bytes)) == refused.error);
  }
}

// The animation fixture `name` in Native/Tests/data/animation.
content::JsonDocument Fixture(std::string_view name) {
  const std::string fixture =
      std::format("Native/Tests/data/animation/{}", name);
  auto directory = fs::current_path();
  while (!fs::exists(directory / fixture) &&
         directory != directory.parent_path()) {
    directory = directory.parent_path();
  }
  const auto json =
      content::ParseJsonText(ReadModel(directory / fixture), fixture);
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

// An archetype naming a converted body, weapon and common bank, with the
// SEQ tests' sample bank as a battle bank of its own.
content::ActorArchetypeDocument SampleDocument() {
  const auto bank = content::ActorAnimationOf(Fixture("sample-bank.json"),
                                              "sample-bank.json");
  INFO(ErrorOf(bank));
  REQUIRE(bank.has_value());
  return {.source = "MAP/Z001U00.ZUD",
          .source_sha256 =
              "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934c"
              "a495991b7852b855",
          .header = {.character_id = 0x4F,
                     .weapon = 1,
                     .battle_sequence_model = 0x4F,
                     .weapon_material = 3,
                     .shield = 0,
                     .shield_material = 0,
                     .default_material = 5},
          .body = content::ArchetypeReference{.id = "model.obj.4f"},
          .weapon = content::ArchetypeReference{.id = "model.obj.01.wep"},
          .shield = std::nullopt,
          .common_animation =
              content::ArchetypeReference{.id = "animation.obj.4f_com"},
          .battle_animation = bank->file};
}

TEST_CASE("The actor-archetype form writes and reads the sample archetype",
          "[content-models][zud]") {
  content::ActorArchetypeDocument document = SampleDocument();
  const content::JsonDocument json = content::ActorArchetypeJson(document);
  INFO(content::FormatJsonText(json));
  CHECK(json == Fixture("sample-archetype.json"));
  const auto read =
      content::ActorArchetypeOf(Reparsed(json), "sample-archetype.json");
  INFO(ErrorOf(read));
  REQUIRE(read.has_value());
  CHECK(*read == document);
  // A body of its own and no banks.
  document.body = content::ArchetypeOwnModel{.file = "Z049U06.gltf"};
  document.common_animation = std::monostate{};
  document.battle_animation = std::monostate{};
  const auto own = content::ActorArchetypeOf(
      Reparsed(content::ActorArchetypeJson(document)), "own");
  INFO(ErrorOf(own));
  REQUIRE(own.has_value());
  CHECK(*own == document);
}

TEST_CASE("The actor-archetype reader names the member at fault",
          "[content-models][zud]") {
  using content::JsonDocument;
  struct Edit {
    std::string_view pointer;
    JsonDocument value;
    std::string_view error;
  };
  const std::vector<Edit> edits{
      {"/schema_version", 3, "s is vs.actor-archetype version 3; this build"},
      {"/source_sha256", "ABC", "s.source_sha256 must be a lowercase SHA-256"},
      {"/header/shield", 256,
       "s.header.shield must be a whole number from 0 to 255"},
      {"/header/extra", 1, "s.header has an unknown member"},
      {"/body", JsonDocument::parse(R"({"model": "Model.OBJ.4F"})"),
       "s.body.model \"Model.OBJ.4F\" is not a content id"},
      {"/body", JsonDocument::parse(R"({"file": "../4F.gltf"})"),
       "s.body.file \"../4F.gltf\" must be a .gltf file name"},
      {"/body", JsonDocument::parse(R"({"file": "Z.gltf", "model": "m"})"),
       "s.body has an unknown member"},
      {"/weapon", JsonDocument::parse(R"({"animation": "m"})"),
       "s.weapon has an unknown member"},
      {"/shield", 1, "s.shield must be an object"},
      {"/common_animation", JsonDocument::parse(R"({"animation": 7})"),
       "s.common_animation.animation must be a string"},
      {"/battle_animation/bones", 42,
       "s.battle_animation.bones must be a whole number from 1 to 41"},
      {"/battle_animation/bank/map/1", 255,
       "s.battle_animation.bank.map[1] must be a whole number"},
      {"/extra", 1, "s has an unknown member"},
  };
  for (const Edit& edit : edits) {
    CAPTURE(edit.pointer);
    JsonDocument json = content::ActorArchetypeJson(SampleDocument());
    json[JsonDocument::json_pointer(std::string(edit.pointer))] = edit.value;
    const auto read = content::ActorArchetypeOf(json, "s");
    REQUIRE_FALSE(read.has_value());
    INFO(read.error());
    CHECK(read.error().contains(edit.error));
  }
}

TEST_CASE("Extracted ZUD files decode and rebuild their exact bytes",
          "[content-models][zud][extract]") {
  const auto models = ExtractedModels();
  if (!models) SKIP("Extracted Native/data is unavailable");
  std::size_t files = 0;
  for (const auto& entry :
       fs::directory_iterator(models->parent_path() / "MAP")) {
    if (entry.path().extension() != fs::path(content::kZudExtension)) {
      continue;
    }
    CAPTURE(entry.path().filename().string());
    const Bytes bytes = ReadModel(entry.path());
    const auto zud = content::DecodeZudFile(bytes);
    INFO(ErrorOf(zud));
    REQUIRE(zud.has_value());
    CHECK(zud->identity.back() == 0);
    CHECK(Encoded(*zud) == bytes);
    ++files;
  }
  CHECK(files == kExtractedZudCount);
}

TEST_CASE("Extracted ZUD archetypes load from their JSON as exact bytes",
          "[content-models][zud][extract]") {
  const auto models = ExtractedModels();
  if (!models) SKIP("Extracted Native/data is unavailable");
  CHECK(VS::test_support::CheckExtractedModelsLoad(
            *models, content::kZudExtension, content::LoadConvertedArchetype,
            VS::test_support::ExtractedEventLengths(*models)) ==
        kExtractedZudCount);
}

}  // namespace
