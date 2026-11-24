#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Battle/AbilitiesLive.hpp"
#include "Battle/AbilityState.hpp"
#include "Battle/Actor.hpp"
#include "Battle/LearningLive.hpp"
#include "Content/CatalogDatabase.hpp"
#include "Extract/ConverterBatch.hpp"
#include "Extract/LearningConversion.hpp"
#include "Menu/AbilityFoundationWorld.hpp"
#include "Menu/AbilityTerminalWorld.hpp"
#include "Menu/BattleAbilitiesUnlockLive.hpp"
#include "State/Progress.hpp"
#include "TestSupport/LearningRoot.hpp"

namespace {
namespace content = VS::Content;
namespace extract = VS::Extract;
namespace battle = VS::Battle;
namespace menu = VS::Menu::BattleAbilities;
using VS::test_support::LearningRoot;

template <typename T>
T Value(std::expected<T, std::string> result) {
  if (!result) FAIL(result.error());
  return std::move(*result);
}
std::string Read(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) FAIL("missing " + path.string());
  return {std::istreambuf_iterator<char>(file),
          std::istreambuf_iterator<char>()};
}
void Source(LearningRoot& root) {
  std::string bytes(
      extract::kLearningSourceOffset + extract::kLearningImportBytes, '\0');
  for (std::size_t at = extract::kLearningSourceOffset; at < bytes.size();
       ++at) {
    bytes[at] = static_cast<char>(at % 128);
  }
  root.Write("disc/SLUS_010.40", bytes);
  root.Write(content::DataStore::ManifestName,
             "lba\tsize\tform\tpath\n1\t" + std::to_string(bytes.size()) +
                 "\traw\tSLUS_010.40\n");
}

struct LiveProgress {
  VS::State::ArtsStatus saved = VS::State::artsStatus;
  battle::Actor* previous = battle::characterState;
  battle::Actor actor{};
  LiveProgress() {
    VS::State::artsStatus = {};
    battle::characterState = &actor;
    actor.equipped_weapon_category = 1;
  }
  ~LiveProgress() {
    battle::characterState = previous;
    VS::State::artsStatus = saved;
  }
};
struct CatalogPoints : VS::Test::ability_terminal::PointsWorld {
  std::optional<int> Requirement(int index) const override {
    return battle::LiveBattleAbilityRequirement(index);
  }
};
struct CatalogUnlock : VS::Test::ability_foundation::World {
  std::optional<int> BreakArtRequirement(int category,
                                         int tier) const override {
    return battle::LiveBreakArtRequirement(category, tier);
  }
  std::optional<int> BattleAbilityRequirement(int tier) const override {
    return battle::LiveBattleAbilityRequirement(tier);
  }
};
}  // namespace

TEST_CASE(
    "Learning import separates 40 plus 22 requirements from neighboring bytes",
    "[learning-catalog][content-tables]") {
  std::array<std::byte, extract::kLearningImportBytes> bytes{};
  for (std::size_t at = 0; at < bytes.size(); ++at) {
    bytes[at] = static_cast<std::byte>((at * 37) % 256);
  }
  const auto imported = Value(extract::DecodeLearning(bytes));
  CHECK(imported.definitions.break_arts.size() == 40);
  CHECK(imported.definitions.battle_abilities.size() == 22);
  const auto provenance =
      extract::FormatLearningProvenance(imported, std::string(64, 'a'));
  const auto restored = Value(
      extract::ParseLearningProvenance(std::as_bytes(std::span(provenance))));
  CHECK(Value(extract::EncodeLearning(restored)) ==
        std::vector<std::byte>(bytes.begin(), bytes.end()));
  CHECK_FALSE(
      extract::DecodeLearning(std::span(bytes).first(bytes.size() - 1)));
}

TEST_CASE(
    "Learning native definitions validate semantic action category and choice "
    "references",
    "[learning-catalog]") {
  LearningRoot root;
  root.Publish();
  const auto patch =
      GENERATE("id,required_weapon_kills\nlearning.whistle-sting,-1\n",
               "id,required_weapon_kills\nlearning.whistle-sting,2147483648\n",
               "id,action\nlearning.whistle-sting,lotus-palm\n",
               "id,weapon_category\nlearning.whistle-sting,none\n",
               "id,tier\nlearning.whistle-sting,fourth\n",
               "id,required_weapon_kills\nlearning.unknown,1\n");
  root.Write(
      "catalog-layers.json",
      R"({"schema":"vs.catalog-layers","schema_version":1,"layers":["mod"]})");
  root.Write("mod/catalogs/battle/break-art-learning.csv", patch);
  const auto opened = content::DataStore::Open(root.Root());
  REQUIRE_FALSE(opened);
  CHECK(opened.error().contains("column"));
  CHECK(opened.error().contains("break-art-learning.csv"));
}

TEST_CASE(
    "Learning named weapon edits use axe-mace great-axe and staff progress "
    "categories",
    "[learning-catalog][catalog-layers]") {
  LearningRoot root;
  root.Publish();
  root.Write(
      "catalog-layers.json",
      R"({"schema":"vs.catalog-layers","schema_version":1,"layers":["mod"]})");
  root.Write("mod/catalogs/battle/break-art-learning.csv",
             "id,weapon_category,required_weapon_kills\n"
             "learning.mistral-edge,axe-mace,70004\n"
             "learning.bear-claw,great-axe,70005\n"
             "learning.sirocco,staff,70006\n");
  root.Open();
  CHECK(battle::LiveBreakArtRequirement(4, 0) == 70004);
  CHECK(battle::LiveBreakArtRequirement(5, 0) == 70005);
  CHECK(battle::LiveBreakArtRequirement(6, 0) == 70006);
}

TEST_CASE(
    "Learning definitions refuse missing and duplicate required identities",
    "[learning-catalog]") {
  LearningRoot root;
  const bool duplicate = GENERATE(false, true);
  if (duplicate) {
    root.definitions.battle_abilities.back().id =
        root.definitions.battle_abilities.front().id;
  } else {
    root.definitions.battle_abilities.pop_back();
  }
  const auto parsed =
      content::ParseLearning(content::FormatBreakArtLearning(root.definitions),
                             content::FormatAbilityLearning(root.definitions));
  REQUIRE_FALSE(parsed);
  CHECK(parsed.error().contains("battle-ability-learning.csv"));
}

TEST_CASE(
    "Learning patch and mod thresholds reach live pending comparisons and next "
    "points",
    "[learning-catalog][catalog-layers]") {
  LearningRoot root;
  root.Publish();
  const auto base = Read(root.Root() / content::kAbilityLearningPath);
  root.Write(
      "catalog-layers.json",
      R"({"schema":"vs.catalog-layers","schema_version":1,"layers":["patch","mod"]})");
  root.Write("patch/catalogs/battle/battle-ability-learning.csv",
             "id,required_ability_points\nlearning.first-battle-ability-choice,"
             "60000\n");
  root.Write("mod/catalogs/battle/battle-ability-learning.csv",
             "id,required_ability_points\nlearning.first-battle-ability-choice,"
             "70000\n");
  root.Write("mod/catalogs/battle/break-art-learning.csv",
             "id,required_weapon_kills\nlearning.whistle-sting,70001\n");
  root.Open();
  LiveProgress progress;
  VS::State::artsStatus.kills.total_kills = UINT16_MAX;
  VS::State::artsStatus.kills.weapon_category_kills[1] = UINT16_MAX;
  CHECK(battle::LivePendingBattleAbilityUnlocks() == 0);
  CHECK(battle::LiveBreakArtRequirement(1, 0) == 70001);
  CHECK(menu::LiveUpdateBreakArtUnlock(1) == 1);
  CHECK(menu::LiveUpdateAbilityUnlock(1) == 1);
  CatalogPoints points;
  points.kills = UINT16_MAX;
  menu::DrawNextUnlockPoints(0, points);
  CHECK(points.texts == std::vector<std::string>{"NEXT", "#4465PT"});
  CHECK(Read(root.Root() / content::kAbilityLearningPath) == base);
  root.Open();
  CHECK(battle::LiveBattleAbilityRequirement(0) == 70000);
}

TEST_CASE(
    "Learning missing catalogs cannot grant actions or manufacture zero point "
    "displays",
    "[learning-catalog]") {
  content::DataStore::ScopedOverride empty;
  LiveProgress progress;
  CHECK(battle::LivePendingBattleAbilityUnlocks() == 0);
  CatalogUnlock world;
  menu::BreakArtUnlockState arts;
  menu::AbilityUnlockState abilities;
  CHECK(menu::UpdateBreakArtUnlock(arts, world, world, true) == 1);
  CHECK(menu::UpdateAbilityUnlock(abilities, world, world, true) == 1);
  CHECK(world.unlocked == 0);
  CHECK(world.learned[1] == 0);
  CatalogPoints points;
  menu::DrawNextUnlockPoints(0, points);
  CHECK(points.texts.empty());
  CHECK(points.refusals.size() == 1);
}

TEST_CASE(
    "Learning choice thresholds are cumulative without a fabricated monotonic "
    "restriction",
    "[learning-catalog]") {
  LearningRoot root;
  root.definitions.battle_abilities[0].required_ability_points = 200;
  root.definitions.battle_abilities[1].required_ability_points = 10;
  root.Publish();
  CatalogUnlock world;
  world.unlocked = 1;
  world.total_kills = 10;
  menu::AbilityUnlockState state;
  CHECK(menu::UpdateAbilityUnlock(state, world, world, true) == 0);
  CHECK(world.unlocked == 2);
  CatalogPoints points;
  root.Write(
      "catalog-layers.json",
      R"({"schema":"vs.catalog-layers","schema_version":1,"layers":["mod"]})");
  root.Write("mod/catalogs/battle/battle-ability-learning.csv",
             "id,required_ability_points\nlearning.first-battle-ability-choice,"
             "2147483647\n");
  root.Open();
  menu::DrawNextUnlockPoints(0, points);
  CHECK(points.texts == std::vector<std::string>{"#NEXT 2147483647PT"});
}

TEST_CASE(
    "Learning conversion preserves authored edits repairs missing schemas and "
    "refuses foreign files",
    "[learning-catalog]") {
  LearningRoot root;
  Source(root);
  CHECK(Value(extract::ConvertDiscLearning(root.Root(), false)).converted == 1);
  const auto file = root.Root() / content::kBreakArtLearningPath;
  const auto edited = content::FormatBreakArtLearning(root.definitions);
  root.Write(content::kBreakArtLearningPath, edited);
  const auto stamp = std::filesystem::last_write_time(file);
  std::filesystem::remove(root.Root() /
                          "catalogs\\battle\\break-art-learning.schema.json");
  CHECK(Value(extract::ConvertDiscLearning(root.Root(), false)).converted == 1);
  CHECK(Read(file) == edited);
  CHECK(std::filesystem::last_write_time(file) == stamp);
  CHECK(Value(extract::ConvertDiscLearning(root.Root(), false)).unchanged == 1);
  LearningRoot foreign;
  Source(foreign);
  foreign.Write(content::kBreakArtLearningPath, "foreign");
  CHECK_FALSE(extract::ConvertDiscLearning(foreign.Root(), true));
}

TEST_CASE("Committed learning schemas match both native authoring contracts",
          "[learning-catalog][content-tables]") {
  const std::filesystem::path root(
      "Native\\Content\\schemas\\catalogs\\battle");
  auto art = Read(root / "break-art-learning.schema.json");
  auto ability = Read(root / "battle-ability-learning.schema.json");
  std::erase(art, '\r');
  std::erase(ability, '\r');
  CHECK(art == content::FormatTableSchema(content::BreakArtLearningSpec()));
  CHECK(ability == content::FormatTableSchema(content::AbilityLearningSpec()));
}

TEST_CASE(
    "Learning configured executable reloads exactly with independent imported "
    "provenance",
    "[learning-catalog][content-tables]") {
  const std::filesystem::path root("Native\\data");
  const auto path = root / "disc" / content::kLearningSource;
  if (!std::filesystem::is_regular_file(path)) {
    SKIP("configured extracted executable is unavailable");
  }
  const auto source = Value(extract::ReadFileBytes(path));
  REQUIRE(source.size() >=
          extract::kLearningSourceOffset + extract::kLearningImportBytes);
  const auto section = std::span(source).subspan(extract::kLearningSourceOffset,
                                                 extract::kLearningImportBytes);
  const auto imported = Value(extract::DecodeLearning(section));
  CHECK(std::ranges::equal(Value(extract::EncodeLearning(imported)), section));
  const auto text =
      Value(extract::ReadFileBytes(root / extract::kLearningImportPath));
  const auto restored = Value(extract::ParseLearningProvenance(text));
  CHECK(std::ranges::equal(Value(extract::EncodeLearning(restored)), section));
}
