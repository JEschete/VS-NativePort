#include <algorithm>
#include <bit>
#include <cstdint>
#include <format>
#include <limits>
#include <string>
#include <string_view>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Save/NativeSnapshot.hpp"
#include "nlohmann/json.hpp"

namespace {
namespace Save = VS::Game::Save;
using Json = nlohmann::json;

void SetSettings(Save::Snapshot& state) {
  state.settings.reserved0Bit0 = 8;
  state.settings.containerEmpty = 1;
  state.settings.abilityTimingDisplay = 1;
  state.settings.armorStatChange = 1;
  state.settings.simpleMap = 3;
  state.settings.monoSound = 1;
  state.settings.mappedChainAbilities[3] = static_cast<char>(255);
  state.settings.slotState = 0xFEDCBA98u;
  state.settings.key = 0xFFFFFFFFu;
  state.settings.cumulativeSaveCount = 9999;
  state.settings.currentGameSaveCount = 9998;
  state.settings.saveFileGeneration = 0x80000000u;
}

void SetAshley(Save::Snapshot& state) {
  state.ashley.currentHp = 250;
  state.ashley.maxHp = 999;
  state.ashley.currentMp = 20;
  state.ashley.maxMp = 50;
  state.ashley.risk = 99;
  state.ashley.statuses = std::bit_cast<std::int32_t>(0x80000001u);
  state.ashley.statusTimers[7] = static_cast<char>(255);
  state.ashley.limbs[5].hp = 120;
  state.ashley.limbs[5].maxHp = 200;
  state.ashley.limbs[5].affinities[7] = 65535;
  state.ashley.cameraPitch = -384;
  state.ashley.cameraDistance = 1536;
}

void SetEquipment(Save::Snapshot& state) {
  auto& blade = state.inventory.blades[15];
  blade.id = 14;
  blade.index = 15;
  blade.strength = -128;
  blade.agility = 127;
  // These plain-char bit fields have signed C++ views; the wire keeps source
  // bits.
  blade.damageType = -1;
  blade.costType = -1;
  blade.reserved10Bit5 = -3;
  blade.opaque12 = 0xBEEF;
  blade.range.attackShape = -1;
  blade.range.attackShapeAngle = -1;
  blade.classes[7] = -128;
  blade.affinities[7] = 127;
  auto& weapon = state.inventory.weapons[7];
  weapon.index = 7;
  weapon.blade = 15;
  weapon.grip = 15;
  weapon.gems[3] = 47;
  std::fill_n(weapon.name, 24, static_cast<char>(0xE7));
  weapon.name[0] = static_cast<char>(0xFA);
  weapon.name[1] = 0;
  weapon.name[23] = static_cast<char>(255);
  state.inventory.grips[15].id = 96;
  state.inventory.armor[15].id = 2;
  state.inventory.armor[15].currentDp = 65535;
  state.inventory.shields[7].base = state.inventory.armor[15];
  state.inventory.shields[7].gems[3] = 47;
  state.inventory.gems[47].id = 282;
  state.inventory.gems[47].setItemIndex = 65535;
  state.inventory.misc[63].id = 323;
  state.inventory.misc[63].count = 99;
  state.inventory_order.misc[63] = 63;
}

void SetProgress(Save::Snapshot& state) {
  state.storage.misc[255].id = 383;
  state.storage.misc[255].count = static_cast<char>(255);
  state.visits.roomFlags[15] = std::bit_cast<std::int32_t>(0x80000000u);
  state.visits.areaFlags[1] = std::bit_cast<std::int32_t>(0x80000000u);
  state.arts.artsLearned[11] = 255;
  state.arts.kills.weaponCategoryKills[9] = 65535;
  state.score.bossTimeTrialScores[7][2].time = 0xFFFFFF;
  state.score.bossTimeTrialScores[7][2].round = 127;
  state.score.bossTimeTrialScores[7][2].difficulty = 1;
  state.score.enemyKillFlags[25] = std::bit_cast<std::int32_t>(0x80000000u);
  state.room_cache_slot = 3;
  state.room_cache[3].zone = 9;
  state.room_cache[3].map = 20;
  state.room_cache[3].next_slot = 7;
  state.room_cache[3].actors[7].spawnSlot = 32;
  state.room_cache[3].actors[7].currentHp = 123;
  state.room_cache[3].actors[7].tileX = 31;
  state.room_cache[3].actors[7].tileZ = 30;
  state.play_ticks = std::uint64_t{60} * 60 * 60;
  state.scene_timer.mode = 2;
  state.scene_timer.frames = 59;
  state.scene_timer.seconds = 29;
}

void SetRandom(Save::Snapshot& state) {
  state.random.bios_state = 0xDEADBEEFu;
  state.random.interface_state = 0xFFFFFFFFu;
  state.random.shuffle_index = 96;
  state.random.movement_state = 0xAABBCCDDu;
  for (std::size_t index = 0; index < state.random.shuffle.size(); ++index) {
    state.random.shuffle[index] = static_cast<std::int32_t>(index) * 337;
  }
}

Save::Document SampleDocument() {
  Save::Document result;
  result.generation = 7;
  result.saved_unix_ms = 1792859988000;
  auto& state = result.state;
  state.location.zone = 9;
  state.location.room = 11;
  state.location.position = {448, -128, 1600};
  state.location.facing = 2048;
  state.location.camera_yaw = 1024;
  state.script_flags[187] = 28;
  state.script_flags.back() = 255;
  state.learned_actions[1] = true;
  state.learned_actions[97] = true;
  state.learned_actions.back() = true;
  SetSettings(state);
  SetAshley(state);
  SetEquipment(state);
  SetProgress(state);
  SetRandom(state);
  return result;
}

std::string HashText(std::string_view text) {
  std::uint64_t hash = 14695981039346656037ull;
  for (const unsigned char byte : text) {
    hash ^= byte;
    hash *= 1099511628211ull;
  }
  return std::format("{:016X}", hash);
}

void Recheck(Json& document) {
  document.erase("checksum");
  document["checksum"] = HashText(document.dump());
}

Json EncodedSample() {
  const auto encoded = Save::Encode(SampleDocument());
  REQUIRE(encoded.has_value());
  return Json::parse(*encoded);
}

void CheckRejected(const Json& document, std::string_view expected) {
  const auto result = Save::Decode(document.dump());
  REQUIRE_FALSE(result.has_value());
  CHECK(result.error().find(expected) != std::string::npos);
}

void CheckInventory(const Save::Snapshot& state) {
  CHECK(state.inventory.blades[15].strength == -128);
  CHECK(state.inventory.blades[15].opaque12 == 0xBEEF);
  CHECK((static_cast<unsigned>(state.inventory.blades[15].damageType) & 3u) ==
        3);
  CHECK((static_cast<unsigned>(state.inventory.blades[15].costType) & 7u) == 7);
  CHECK((static_cast<unsigned>(state.inventory.blades[15].range.attackShape) &
         7u) == 7);
  CHECK(state.inventory.gems[47].id == 282);
  CHECK(state.inventory.gems[47].setItemIndex == 65535);
  CHECK(state.inventory_order.misc[63] == 63);
  CHECK(static_cast<unsigned char>(state.storage.misc[255].count) == 255);
}

void CheckProgress(const Save::Snapshot& state,
                   const Save::Snapshot& original) {
  CHECK(state.script_flags[187] == 28);
  CHECK(state.script_flags.back() == 255);
  CHECK(state.learned_actions[97]);
  CHECK(state.learned_actions.back());
  CHECK(state.location.position == original.location.position);
  CHECK(state.random.shuffle == original.random.shuffle);
  CHECK(state.room_cache[3].actors[7].currentHp == 123);
}

void CheckGlyphNames(const Json& state) {
  const auto& glyphs = state.at("inventory").at("weapons")[7].at("name_glyphs");
  REQUIRE(glyphs.is_array());
  REQUIRE(glyphs.size() == 24);
  CHECK(glyphs[0] == 250);
  CHECK(glyphs[1] == 0);
  CHECK(glyphs[23] == 255);
}

void CheckSourceBits(const Json& state) {
  const auto& blade = state.at("inventory").at("blades")[15];
  CHECK(blade.at("damage_type") == 3);
  CHECK(blade.at("cost_type") == 7);
  CHECK(blade.at("source_flags") == 5);
  CHECK(blade.at("range").at("shape") == 7);
  CHECK(blade.at("range").at("angle") == 31);
  CHECK(state.at("settings").at("ability_timing") == true);
}
}  // namespace

TEST_CASE("Native snapshots encode semantic state rather than native objects",
          "[native-snapshot]") {
  CHECK(HashText("hello") == "A430D84680AABD0B");
  const auto original = SampleDocument();
  const auto encoded = Save::Encode(original);
  REQUIRE(encoded.has_value());
  const auto decoded = Save::Decode(*encoded);
  REQUIRE(decoded.has_value());
  const auto repeated = Save::Encode(*decoded);
  REQUIRE(repeated.has_value());
  CHECK(*repeated == *encoded);
  CheckInventory(decoded->state);
  CheckProgress(decoded->state, original.state);
}

TEST_CASE("Native save identifiers and glyph names have fixed wire shapes",
          "[native-snapshot]") {
  const auto document = EncodedSample();
  const auto& state = document.at("state");
  CHECK(state.at("location").at("zone") == "zone/9");
  CHECK(state.at("location").at("room") == "room/11");
  CHECK(state.at("inventory").at("blades")[15].at("definition") == "blade/14");
  CHECK(state.at("inventory").at("gems")[47].at("definition") == "gem/282");
  CHECK(state.at("learned_actions") ==
        Json::array({"action/1", "action/97", "action/255"}));
  CHECK(state.at("visits").at("maps") == Json::array({"map/511"}));
  CHECK(state.at("visits").at("areas") == Json::array({"area/63"}));
  CheckGlyphNames(state);
  CheckSourceBits(state);
}

TEST_CASE("Native save corruption and unsupported formats fail explicitly",
          "[native-snapshot]") {
  auto document = EncodedSample();
  SECTION("integrity") {
    document["state"]["script_flags"][187] = 29;
    CheckRejected(document, "/checksum");
  }
  SECTION("future version") {
    document["version"] = 2;
    CheckRejected(document, "/version");
  }
  SECTION("foreign source") {
    document["source_game"] = "unsupported-source";
    CheckRejected(document, "source game");
  }
  SECTION("unknown envelope field") {
    document["raw_native_memory"] = Json::array();
    CheckRejected(document, "envelope");
  }
}

TEST_CASE("Native save scalars reject booleans fractions and signed overflow",
          "[native-snapshot]") {
  auto document = EncodedSample();
  const auto invalid =
      GENERATE(Json(true), Json(1.5), Json(-1),
               Json(std::numeric_limits<std::uint64_t>::max()));
  document["state"]["script_flags"][0] = invalid;
  Recheck(document);
  CheckRejected(document, "/state/script_flags/0");
}

TEST_CASE("Native unsigned generations cannot wrap negative JSON integers",
          "[native-snapshot]") {
  auto document = EncodedSample();
  const auto key = GENERATE("generation", "saved_unix_ms");
  document[key] = -1;
  Recheck(document);
  CheckRejected(document, std::format("/{}: integer", key));
}

TEST_CASE("Native reference kinds extents and uniqueness are validated",
          "[native-snapshot]") {
  auto document = EncodedSample();
  SECTION("reference kind") {
    document["state"]["inventory"]["blades"][0]["definition"] = "gem/14";
  }
  SECTION("reference overflow") {
    document["state"]["inventory"]["blades"][0]["definition"] = "blade/256";
  }
  SECTION("reference traversal") {
    document["state"]["location"]["zone"] = "zone/../9";
  }
  SECTION("array extent") {
    document["state"]["inventory"]["weapons"].erase(0);
  }
  SECTION("missing field") { document["state"]["ashley"].erase("hp"); }
  SECTION("unknown field") {
    document["state"]["ashley"]["native_pointer"] = 1234;
  }
  SECTION("duplicate action") {
    document["state"]["learned_actions"].push_back("action/1");
  }
  Recheck(document);
  CheckRejected(document, "/state/");
}

TEST_CASE("Native bit fields reject values that would silently truncate",
          "[native-snapshot]") {
  auto document = EncodedSample();
  document["state"]["inventory"]["blades"][15]["damage_type"] = 4;
  Recheck(document);
  CheckRejected(document, "bit field");
}

TEST_CASE("Native save parsing rejects duplicate keys deep JSON and truncation",
          "[native-snapshot]") {
  auto document = EncodedSample();
  auto text = document.dump();
  text.insert(1, "\"generation\":7,");
  const auto duplicate = Save::Decode(text);
  REQUIRE_FALSE(duplicate.has_value());
  CHECK(duplicate.error().find("duplicate") != std::string::npos);
  const auto deep =
      Save::Decode(std::string(70, '[') + "0" + std::string(70, ']'));
  REQUIRE_FALSE(deep.has_value());
  CHECK(deep.error().find("nesting") != std::string::npos);
  CHECK_FALSE(Save::Decode(document.dump().substr(0, 100)));
}

TEST_CASE("Native save schema omits padding and transient load markers",
          "[native-snapshot]") {
  auto original = SampleDocument();
  original.state.ashley.isPending = 1;
  original.state.inventory.blades[15].reserved11 = 123;
  original.state.score.reserved10 = 0x12345678;
  const auto encoded = Save::Encode(original);
  REQUIRE(encoded.has_value());
  const auto decoded = Save::Decode(*encoded);
  REQUIRE(decoded.has_value());
  CHECK(decoded->state.ashley.isPending == 0);
  CHECK(decoded->state.inventory.blades[15].reserved11 == 0);
  CHECK(decoded->state.score.reserved10 == 0);
}

TEST_CASE("Native save generation location and time domains are bounded",
          "[native-snapshot]") {
  auto document = SampleDocument();
  const int invalid = GENERATE(0, 1, 2, 3, 4);
  switch (invalid) {
    case 0:
      document.generation = 0;
      break;
    case 1:
      document.saved_unix_ms = std::numeric_limits<std::uint64_t>::max();
      break;
    case 2:
      document.state.location.zone = 0;
      break;
    case 3:
      document.state.location.room = 256;
      break;
    case 4:
      document.state.location.facing = -1;
      break;
    default:
      FAIL("Unknown generated location/time case");
  }
  const auto encoded = Save::Encode(document);
  REQUIRE_FALSE(encoded.has_value());
  CHECK_FALSE(encoded.error().empty());
}

TEST_CASE("Native save cache and random indices cannot escape native arrays",
          "[native-snapshot]") {
  auto document = SampleDocument();
  const int invalid = GENERATE(0, 1, 2, 3, 4);
  switch (invalid) {
    case 0:
      document.state.room_cache_slot = 4;
      break;
    case 1:
      document.state.room_cache[0].next_slot = 8;
      break;
    case 2:
      document.state.random.shuffle_index = 97;
      break;
    case 3:
      document.state.random.shuffle[0] = -1;
      break;
    case 4:
      document.state.random.shuffle[96] = 32768;
      break;
    default:
      FAIL("Unknown generated cache/random case");
  }
  const auto encoded = Save::Encode(document);
  REQUIRE_FALSE(encoded.has_value());
  CHECK_FALSE(encoded.error().empty());
}
