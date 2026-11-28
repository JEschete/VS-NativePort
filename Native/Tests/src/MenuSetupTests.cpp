// Hand-maintained tests for the typed workshop menu (MENUC.PRG, #276 row
// 49): its cores through a recording host, and the live recipe books.
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Content/DataStore.hpp"
#include "Content/ModuleData.hpp"
#include "Content/WorkshopRecipes.hpp"
#include "Items/ItemRecords.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Framework/ItemRows.hpp"
#include "Menu/Setup/Assembly.hpp"
#include "Menu/Setup/Combination.hpp"
#include "Menu/Setup/SetupLive.hpp"
#include "Menu/Setup/SetupScreen.hpp"
#include "Menu/Sounds.hpp"
#include "Render/EncodedText.hpp"
#include "TestSupport/ChannelWarnings.hpp"
#include "TestSupport/WorkshopRecipeRoot.hpp"

namespace {
namespace fw = VS::Menu::Framework;
namespace items = VS::Items;
namespace setup = VS::Menu::Setup;
namespace Content = VS::Content;
using Strings = std::vector<std::string>;

constexpr std::uint8_t kTerminator = 0xE7;

// Item text the workshop only passes through: every lookup misses.
class StubItemRows final : public fw::ItemRowsHost {
 public:
  void Refuse(std::string_view what) override { refusals.emplace_back(what); }
  std::span<const std::uint8_t> HelpText() override { return {}; }
  std::span<const std::uint8_t> NameText() override { return {}; }
  std::span<const std::uint8_t> WeaponHands() override { return {}; }
  VS::Render::EncodedTextArguments TextArguments() override { return {}; }
  void SetStringArgument(std::size_t /*slot*/,
                         std::span<const std::uint8_t> /*text*/) override {}
  std::optional<items::WeaponLoadout> InventoryWeaponLoadout(
      const items::InventoryWeapon& /*weapon*/) override {
    return std::nullopt;
  }
  std::optional<items::ShieldLoadout> InventoryShieldLoadout(
      const items::InventoryShield& /*shield*/) override {
    return std::nullopt;
  }
  std::optional<items::InventoryWeapon> Weapon(int /*reference*/) override {
    return std::nullopt;
  }
  std::optional<items::InventoryBlade> Blade(int /*reference*/) override {
    return std::nullopt;
  }
  std::optional<items::InventoryGrip> Grip(int /*reference*/) override {
    return std::nullopt;
  }
  std::optional<items::InventoryShield> Shield(int /*reference*/) override {
    return std::nullopt;
  }
  std::optional<items::InventoryArmor> Armor(int /*reference*/) override {
    return std::nullopt;
  }
  std::optional<items::InventoryGem> Gem(int /*reference*/) override {
    return std::nullopt;
  }
  std::span<std::uint8_t> StringBuffer() override { return buffer; }
  void DeactivateRow(int /*row*/) override {}
  fw::MenuRow* SetMenuItem(const fw::RowPlacement& /*placement*/,
                           fw::TextRef /*text*/) override {
    return nullptr;
  }

  Strings refusals;
  std::array<std::uint8_t, fw::kRowDescriptionBufferBytes> buffer{};
};

// Records the refusals, the recipe reads and Ashley's model reloads; the
// drawing and the details panels do nothing.
class FakeHost final : public setup::Host {
 public:
  void Refuse(std::string_view what) override { refusals.emplace_back(what); }
  fw::Buttons Input() override { return {}; }
  void PlaySound(VS::Menu::MenuSound /*sound*/) override {}
  void PlaySoundEffect(int /*effect*/) override {}

  fw::MenuRow& MenuItem(int row) override { return Row(row); }
  fw::MenuRow& SetMenuItem(const fw::RowPlacement& placement,
                           std::span<const std::uint8_t> /*text*/) override {
    return Row(placement.row);
  }
  bool RowsIdle() override { return true; }
  void ClearMenuExcept(int /*row*/) override {}
  void FlyOutRowLeft(int /*row*/) override {}
  void FlyOutRowRight(int /*row*/) override {}
  void FlyOutRowsAndHoistSelection(int /*row*/, int /*offset*/) override {}
  void DeactivateRow(int /*row*/) override {}
  void CreateMenuRows(int /*row_info*/, std::span<const fw::RowText> /*texts*/,
                      std::span<const std::int32_t> /*row_types*/) override {}
  int SelectedRow() override { return setup::kNoRow; }
  int HighlightedRow() override { return 0; }
  int DrawCursor(int /*step*/, std::uint32_t /*position*/) override {
    return 0;
  }
  int DrawMainMenuCursor(int /*step*/, int /*position*/) override { return 0; }
  void DrawConfirmationText(std::uint32_t /*position*/,
                            std::uint32_t /*color*/) override {}
  void SetMenuCommandLabel() override {}
  void SetBackgroundPriority(int /*ot_offset*/, int /*brightness*/) override {}
  void SetCursorColor(int /*color*/) override {}
  int ItemsListRow() override { return 0; }
  std::span<std::int32_t> RowTypes() override { return row_types_; }
  std::span<std::uint8_t> StringBuffer() override { return rows_.buffer; }
  int CursorMemory() override { return 0; }
  void SetCursorMemory(int /*value*/) override {}

  void InitInformationTextBox(int /*header*/) override {}
  void InitializeInformationBox() override {}
  void DismissInformationBox() override {}
  void CloseInformationTextBox() override {}
  void SetInformationMessage(std::span<const std::uint8_t> /*text*/) override {}
  void UpdateInformationVisibility(int /*row*/, int /*state*/) override {}
  void ReturnFromSetup(int /*return_state*/) override {}
  void OpenMenu(int /*menu*/) override {}

  void ResetEquipmentStats() override {}
  void SetWeaponStats(int /*weapon*/) override {}
  void SetBladeStats(int /*blade*/) override {}
  void SetGripStats(int /*grip*/) override {}
  void SetShieldStats(int /*shield*/) override {}
  void SetArmorStats(int /*armor*/) override {}
  void SetGemStats(int /*gem*/) override {}
  void SetEnabledStatPages(int /*pages*/) override {}
  void UpdateEquipmentStatPages(int /*command*/) override {}
  void SetRangeAndRisk(int /*range*/, int /*risk*/, int /*unused*/,
                       int /*shown*/) override {}
  void SetPrimaryStats(int /*strength*/, int /*intelligence*/, int /*agility*/,
                       int /*shown*/) override {}
  void UpdatePrimaryStatPanel(int /*command*/) override {}
  void SetDpPp(int /*current_dp*/, int /*max_dp*/, int /*current_pp*/,
               int /*max_pp*/) override {}
  void UpdateDpPpBars(int /*flags*/) override {}
  void SetEquipmentSubtype(int /*subtype*/) override {}
  void SetEquipmentStat(std::size_t /*index*/, int /*value*/) override {}
  void SetPrimaryStatValue(std::size_t /*index*/, int /*value*/) override {}

  std::span<const std::uint8_t> Menu12() override { return {}; }
  fw::ItemRowsHost& ItemRows() override { return rows_; }
  void SetTextInteger(std::size_t /*slot*/, int /*value*/) override {}

  items::InventoryContents& Inventory() override { return inventory; }
  items::InventoryIndices& Indices() override { return indices; }
  void UnequipWeapon() override {}
  void UnequipShield() override {}
  void EquipWeapon(std::size_t /*slot*/) override {}
  void EquipShield(std::size_t /*slot*/) override {}
  void EquipArmor(int /*body_part*/,
                  std::optional<std::size_t> /*slot*/) override {}
  std::expected<setup::MaterialBonuses, std::string> Materials() override {
    if (!material_error.empty()) return std::unexpected(material_error);
    return setup::MaterialBonuses{};
  }
  int CurrentWorkshop() override { return 0; }

  void StartRecipeLoad(setup::RecipeKind kind) override {
    started.push_back(kind);
  }
  std::optional<bool> RecipeLoaded() override { return loaded; }
  void ReleaseRecipeRequest() override { ++released; }
  const setup::RecipeBook* Recipes() override { return recipes; }
  void FreeRecipes() override {}

  std::optional<int> AshleySequenceModel() override { return ashley_model; }
  void ReleaseWeaponModel(int slot) override {
    released_models.push_back(slot);
  }
  std::optional<Content::ActorAssetTicket> QueueWeaponAnimations(
      int sequence_model) override {
    queued_model = sequence_model;
    if (!queue_asset) return std::nullopt;
    return static_cast<Content::ActorAssetTicket>(next_ticket++);
  }
  Content::ActorAssetOutcome AssetOutcome(
      Content::ActorAssetTicket /*ticket*/) override {
    return asset_outcome;
  }
  void ForgetAsset(Content::ActorAssetTicket ticket) override {
    forgotten_assets.push_back(ticket);
  }
  void ResetEquipmentPose() override { ++pose_resets; }
  int EquipmentModelLoadStep() override { return load_step; }
  void SetEquipmentModelLoadStep(int step) override { load_step = step; }

  Strings refusals;
  items::InventoryContents inventory{};
  items::InventoryIndices indices{};
  std::vector<setup::RecipeKind> started;
  std::optional<bool> loaded;
  int released = 0;
  std::optional<int> ashley_model;
  std::vector<int> released_models;
  int queued_model = 0;
  bool queue_asset = true;
  Content::ActorAssetOutcome asset_outcome =
      Content::ActorAssetOutcome::kPending;
  std::uint32_t next_ticket = 1;
  std::vector<Content::ActorAssetTicket> forgotten_assets;
  int pose_resets = 0;
  int load_step = 0;
  std::string material_error;
  const setup::RecipeBook* recipes = nullptr;

 private:
  static constexpr std::size_t kMenuRows = 64;
  static constexpr std::size_t kRowTypes = 64;

  fw::MenuRow& Row(int row) {
    return menu_rows_.at(static_cast<std::size_t>(row));
  }

  StubItemRows rows_;
  std::array<fw::MenuRow, kMenuRows> menu_rows_{};
  std::array<std::int32_t, kRowTypes> row_types_{};
};

// A fresh State over a FakeHost.
struct Workshop {
  FakeHost host;
  setup::State state{};
  setup::Context context{.state = state, .host = host};
};

// SHIELD.SYD's pieces as tables: sub ids 0 and 1 make record 1 (model 7,
// category 2, one gem slot), and materials 0 and 1 combine into material 3.
constexpr std::uint8_t kCombinedMaterial = 3;
constexpr std::uint8_t kBonuslessMaterial = 9;
constexpr std::uint8_t kRecordModel = 7;
constexpr std::uint8_t kRecordCategory = 2;
constexpr std::uint8_t kRecordGemSlots = 1;
constexpr std::uint8_t kArmorIdOffset = 126;
constexpr std::size_t kShieldPairSide = 17;
constexpr std::uint16_t kShieldMaterialEntries = 64;

VS::Content::WorkshopRecipeTable ShieldTable() {
  constexpr auto kShield = VS::Content::WorkshopKind::kShield;
  auto table = VS::test_support::FullWorkshopTable(kShield);
  VS::Content::PieceResult& record = table.pieces.at(1);
  record.model = kRecordModel;
  record.category = kRecordCategory;
  record.gem_slots = kRecordGemSlots;
  table.pairs.push_back(
      {.first = VS::test_support::WorkshopResultId(kShield, 0),
       .second = VS::test_support::WorkshopResultId(kShield, 1),
       .result = VS::test_support::WorkshopResultId(kShield, 1)});
  table.materials.push_back({.first_material = 0,
                             .first_category = 0,
                             .second_material = 1,
                             .second_category = 0,
                             .material = kCombinedMaterial});
  return table;
}

setup::RecipeBook ShieldBook() {
  auto book = setup::BuildRecipeBook(ShieldTable());
  INFO(book.error_or(""));
  REQUIRE(book.has_value());
  return std::move(*book);
}

// Why BuildRecipeBook refuses `table`, or empty.
std::string BookRefusal(const VS::Content::WorkshopRecipeTable& table) {
  const auto book = setup::BuildRecipeBook(table);
  return book ? std::string{} : book.error();
}

// Shields of materials 0 and `material`, sub ids 0 and `sub_id`; the error
// when the recipe lookup refuses them.
std::string ShieldRefusal(const setup::RecipeBook& book, std::uint16_t material,
                          std::uint8_t sub_id) {
  items::InventoryArmor first{};
  items::InventoryArmor second{};
  second.material = material;
  second.sub_id = sub_id;
  const auto built = setup::BuildCombinedShield(first, second, book, {});
  return built ? std::string{} : built.error();
}

TEST_CASE("Workshop recipe books hold the recipe tables' matrices",
          "[menu][menu-setup]") {
  const setup::RecipeBook shield = ShieldBook();
  CHECK(shield.kind == setup::RecipeKind::kShield);
  CHECK(shield.results.size() == kShieldPairSide * kShieldPairSide);
  CHECK(shield.results[1] == 1);
  CHECK(shield.materials.size() == kShieldMaterialEntries);
  CHECK(shield.materials[1] == kCombinedMaterial);
  CHECK(shield.armor.size() == kShieldPairSide);
  CHECK(shield.blades.empty());
}

TEST_CASE("Workshop recipe books refuse tables that break their layout",
          "[menu][menu-setup]") {
  VS::Content::WorkshopRecipeTable mixed = ShieldTable();
  mixed.blades.emplace_back().id = "blade-000";
  CHECK(BookRefusal(mixed) == "shield tables hold another kind's results");
  VS::Content::WorkshopRecipeTable gap = ShieldTable();
  gap.pieces.erase(gap.pieces.begin());
  CHECK(BookRefusal(gap) ==
        "result shield-001 has legacy_id 1 at index 0; results run from 0 in "
        "order");
  VS::Content::WorkshopRecipeTable unnamed = ShieldTable();
  unnamed.pairs[0].result = "shield-999";
  CHECK(BookRefusal(unnamed) == "pair names no result 'shield-999'");
  VS::Content::WorkshopRecipeTable empty_result = ShieldTable();
  empty_result.pairs[0].result = "shield-000";
  CHECK(BookRefusal(empty_result) ==
        "pair (shield-000, shield-001) repeats or makes legacy id 0");
  VS::Content::WorkshopRecipeTable past_material = ShieldTable();
  past_material.materials[0].material = kBonuslessMaterial;
  CHECK(BookRefusal(past_material) ==
        "material rule (0, 0, 1, 0) is out of range or repeats");
}

TEST_CASE("Workshop recipe lookups refuse entries outside the recipe tables",
          "[menu][menu-setup]") {
  const setup::RecipeBook shield = ShieldBook();
  CHECK(ShieldRefusal(shield, kShieldMaterialEntries, 1) ==
        "combined material entry 64 outside the recipe tables");
  setup::RecipeBook short_results = shield;
  short_results.results.resize(2);
  CHECK(ShieldRefusal(short_results, 1, 2) ==
        "combination entry 2 outside the recipe tables");
  setup::RecipeBook bonusless = shield;
  bonusless.materials[1] = kBonuslessMaterial;
  CHECK(ShieldRefusal(bonusless, 1, 1) ==
        "combined material 9 has no stat bonus");
  setup::RecipeBook short_records = shield;
  short_records.armor.resize(1);
  CHECK(ShieldRefusal(short_records, 1, 1) ==
        "recipe record 1 outside the recipe tables");
  setup::RecipeBook armor = shield;
  armor.kind = setup::RecipeKind::kArmor;
  CHECK(ShieldRefusal(armor, 1, 1) ==
        "recipe book 3 loaded where book 2 belongs");
}

TEST_CASE("Workshop shield combination builds the looked-up recipe record",
          "[menu][menu-setup]") {
  const setup::RecipeBook shield = ShieldBook();
  items::InventoryArmor first{};
  items::InventoryArmor second{};
  second.material = 1;
  second.sub_id = 1;
  const auto built = setup::BuildCombinedShield(first, second, shield, {});
  REQUIRE(built.has_value());
  CHECK(built->id == 1 + kArmorIdOffset);
  CHECK(built->material == kCombinedMaterial);
  CHECK(built->weapon_model_id == kRecordModel);
  CHECK(built->category == kRecordCategory);
  CHECK(built->gem_slots == kRecordGemSlots);
}

TEST_CASE("Workshop previews propagate material catalog failures and recover",
          "[menu][menu-setup][material-catalog]") {
  Workshop shop;
  const auto recipes = ShieldBook();
  shop.host.recipes = &recipes;
  shop.host.inventory.shields[0].base.id = 126;
  shop.host.inventory.shields[1].base.id = 127;
  shop.host.inventory.shields[1].base.sub_id = 1;
  shop.host.inventory.shields[1].base.material = 1;
  shop.state.combination.shield_ids = {1, 2};
  shop.host.material_error = "invalid native material catalog";
  setup::PreviewShieldCombination(shop.context, 0);
  CHECK(shop.state.combination.refused);
  CHECK(shop.state.combination.shield.base.id == 0);
  CHECK(shop.host.refusals == Strings{shop.host.material_error});
  shop.host.material_error.clear();
  setup::PreviewShieldCombination(shop.context, 0);
  CHECK_FALSE(shop.state.combination.refused);
  CHECK(shop.state.combination.shield.base.id == 127);
}

TEST_CASE("Workshop slot history pops the latest selection first",
          "[menu][menu-setup]") {
  constexpr int kLatest = static_cast<int>(setup::kSlotHistoryCount);
  Workshop shop;
  for (const int slot : {2, 0, 4}) {
    setup::RecordSlotSelection(shop.context, slot);
  }
  CHECK(setup::PopSlotSelection(shop.context, kLatest) == 5);
  // Clearing slot 0 directly leaves slot 2 the latest.
  CHECK(setup::PopSlotSelection(shop.context, 0) == 0);
  CHECK(setup::PopSlotSelection(shop.context, kLatest) == 3);
  // An empty history pops nothing, without a refusal.
  CHECK(setup::PopSlotSelection(shop.context, kLatest) == 0);
  CHECK(shop.host.refusals.empty());

  setup::RecordSlotSelection(shop.context, 1);
  setup::RecordSlotSelection(shop.context, kLatest);
  CHECK(shop.state.slot_history ==
        std::array<std::uint32_t, setup::kSlotHistoryCount>{});
  setup::RecordSlotSelection(shop.context, -1);
  CHECK(setup::PopSlotSelection(shop.context, -1) == 0);
  CHECK(shop.host.refusals ==
        Strings{"selection history slot -1", "selection history slot -1"});
}

// Weapon 1 holds blade 1 and grip 1 (one gem slot, holding gem 1); blade 2
// lies loose.
void StockWeapon(FakeHost& host) {
  items::InventoryContents& inventory = host.inventory;
  inventory.weapons[0].index = 1;
  inventory.weapons[0].blade = 1;
  inventory.weapons[0].grip = 1;
  inventory.weapons[0].gems[0] = 1;
  for (std::uint8_t slot = 0; slot < 2; ++slot) {
    inventory.blades[slot].id = 1;
    inventory.blades[slot].category = 1;
    inventory.blades[slot].index = static_cast<std::uint8_t>(slot + 1);
    host.indices.blades[slot] = static_cast<std::uint8_t>(slot + 1);
  }
  inventory.blades[0].assembled_weapon_index = 1;
  inventory.grips[0].id = 1;
  inventory.grips[0].gem_slots = 1;
  inventory.grips[0].assembled_weapon_index = 1;
  inventory.grips[0].index = 1;
  inventory.gems[0].id = 1;
  inventory.gems[0].set_item_index = 1;
  inventory.gems[0].index = 1;
  host.indices.weapons[0] = 1;
  host.indices.grips[0] = 1;
  host.indices.gems[0] = 1;
}

TEST_CASE("Workshop assembly moves the chosen parts onto the weapon",
          "[menu][menu-setup]") {
  Workshop shop;
  StockWeapon(shop.host);
  setup::AssembleWeapon(shop.context, 1, 0, {1, 0, 0, 0});
  const items::InventoryContents& inventory = shop.host.inventory;
  CHECK(inventory.weapons[0].blade == 2);
  CHECK(inventory.weapons[0].grip == 1);
  CHECK(inventory.blades[0].assembled_weapon_index == 0);
  CHECK(inventory.blades[1].assembled_weapon_index == 1);
  CHECK(inventory.gems[0].set_item_index == 1);
  CHECK(static_cast<std::uint8_t>(inventory.weapons[0].name[0]) == kTerminator);
  CHECK(shop.host.refusals.empty());
}

TEST_CASE("Workshop assembly refuses a stale blade selection",
          "[menu][menu-setup]") {
  // A popped gem selection after the blade was deselected leaves blade -1;
  // the original wrote before the blade table and kept the old parts linked.
  Workshop shop;
  StockWeapon(shop.host);
  const items::InventoryContents before = shop.host.inventory;
  setup::AssembleWeapon(shop.context, -1, 0, {});
  CHECK(shop.host.inventory == before);
  CHECK(shop.host.refusals ==
        Strings{"assembled blade reference 0 outside the 16-slot table"});
}

TEST_CASE("Workshop gem slots past the four a record holds are refused",
          "[menu][menu-setup]") {
  // The original looped over every recorded gem slot, past the four gem
  // bytes of the weapon or shield; only the four it holds are cleared.
  constexpr std::uint8_t kPastRecordSlots = items::kShieldGemSlots + 1;
  Workshop shop;
  StockWeapon(shop.host);
  items::InventoryContents& inventory = shop.host.inventory;
  inventory.grips[0].gem_slots = kPastRecordSlots;
  setup::DisassembleWeapon(shop.context, 0);
  CHECK(inventory.weapons[0].blade == 0);
  CHECK(inventory.gems[0].set_item_index == 0);

  inventory.shields[0].base.gem_slots = kPastRecordSlots;
  inventory.shields[0].gems = {1, 2, 3, 4};
  for (std::size_t gem = 0; gem < items::kShieldGemSlots; ++gem) {
    inventory.gems[gem].set_item_index = 1;
  }
  setup::DetachShieldGems(shop.context, 0);
  CHECK(inventory.shields[0].gems == setup::GemIds{});
  CHECK(std::ranges::none_of(
      std::span(inventory.gems).first(items::kShieldGemSlots),
      [](const items::InventoryGem& gem) { return gem.set_item_index != 0; }));
  const std::string refusal = "5 gem slots past the four a record holds";
  CHECK(shop.host.refusals == Strings{refusal, refusal});
}

TEST_CASE("Workshop grip checks refuse categories outside the tables",
          "[menu][menu-setup]") {
  constexpr std::uint8_t kGripKinds = 5;
  constexpr std::uint8_t kCategoryBits = 32;
  Workshop shop;
  items::InventoryBlade blade{};
  blade.category = 1;
  items::InventoryGrip grip{};
  grip.category = 1;
  CHECK(setup::IsGripCompatible(shop.context, &blade, &grip));
  CHECK(setup::IsGripCompatible(shop.context, nullptr, &grip));
  CHECK(setup::IsGripCompatible(shop.context, &blade, nullptr));
  grip.category = 2;
  CHECK_FALSE(setup::IsGripCompatible(shop.context, &blade, &grip));
  CHECK(shop.host.refusals.empty());

  grip.category = kGripKinds;
  CHECK_FALSE(setup::IsGripCompatible(shop.context, &blade, &grip));
  grip.category = 1;
  blade.category = kCategoryBits;
  CHECK_FALSE(setup::IsGripCompatible(shop.context, &blade, &grip));
  CHECK(shop.host.refusals ==
        Strings{"grip category 5 outside the 5 kinds",
                "blade category 32 past the category bits"});
}

TEST_CASE("Workshop item lists keep at most forty-nine entries",
          "[menu][menu-setup]") {
  constexpr std::int32_t kRowType = 0x1000;
  constexpr std::size_t kOverCapacity = setup::kItemListCapacity + 1;
  const std::array<std::uint8_t, 3> name{0x0A, 0x0B, kTerminator};
  const std::vector<fw::RowText> texts(
      kOverCapacity, fw::RowText{.name = name, .description = {}});
  const std::vector<std::int32_t> types(kOverCapacity, kRowType);
  Workshop shop;
  setup::BuildItemList(shop.context, texts, types);
  const setup::ItemListState& list = shop.state.item_list;
  CHECK(list.count == setup::kItemListCapacity);
  CHECK(list.open);
  CHECK(list.items.back().name[0] == name[0]);
  CHECK(list.items[0].flags == kRowType);
  CHECK(list.items[0].description[0] == kTerminator);

  setup::BuildItemList(shop.context, std::span(texts).first(2),
                       std::span(types).first(1));
  CHECK(list.count == 2);
  CHECK(list.items[1].flags == 0);
  CHECK(shop.host.refusals ==
        Strings{"item list of 50 entries past 49",
                "item list with fewer row types than texts"});
}

TEST_CASE("Workshop recipe loading polls the host's read",
          "[menu][menu-setup]") {
  constexpr int kPoll = 0;
  constexpr int kShieldFile = 2;
  Workshop shop;
  CHECK(setup::LoadCombinationRecipes(shop.context, 4) == 0);
  CHECK(setup::LoadCombinationRecipes(shop.context, kShieldFile) == 0);
  CHECK(shop.host.started ==
        std::vector<setup::RecipeKind>{setup::RecipeKind::kShield});
  shop.host.loaded = false;
  CHECK(setup::LoadCombinationRecipes(shop.context, kPoll) == 0);
  shop.host.loaded = true;
  CHECK(setup::LoadCombinationRecipes(shop.context, kPoll) == 1);
  CHECK(shop.host.released == 1);
  // Nothing loading: done at once.
  CHECK(setup::LoadCombinationRecipes(shop.context, kPoll) == 1);

  shop.state.combination.recipes_loading = true;
  shop.host.loaded.reset();
  CHECK(setup::LoadCombinationRecipes(shop.context, kPoll) == 1);
  CHECK(shop.host.refusals ==
        Strings{"recipe book 4 outside 1-3",
                "recipe book loading without a pending load"});
}

TEST_CASE("Workshop weapon reloads refuse a missing Ashley actor",
          "[menu][menu-setup]") {
  constexpr int kWeaponReload = 0;
  Workshop shop;
  CHECK(setup::ReloadAshleyEquipment(shop.context, kWeaponReload) == 0);
  CHECK(shop.host.refusals ==
        Strings{"no Ashley actor to reload the weapon onto"});
}

TEST_CASE("Workshop weapon reload waits on its animation ticket",
          "[menu][menu-setup]") {
  constexpr int kWeaponReload = 0;
  constexpr int kFieldModel = 3;
  constexpr int kMenuModel = 10;
  Workshop shop;
  shop.host.ashley_model = kFieldModel;
  CHECK(setup::ReloadAshleyEquipment(shop.context, kWeaponReload) == 1);
  CHECK(shop.host.queued_model == kMenuModel);
  CHECK(setup::ReloadAshleyEquipment(shop.context, kWeaponReload) == 1);
  shop.host.asset_outcome = Content::ActorAssetOutcome::kLoaded;
  CHECK(setup::ReloadAshleyEquipment(shop.context, kWeaponReload) == 0);
  CHECK(shop.host.load_step == 0);
  CHECK(shop.host.forgotten_assets.size() == 1);
}

TEST_CASE("Workshop shield reload keeps its original release and pose behavior",
          "[menu][menu-setup]") {
  constexpr int kShieldReload = 1;
  Workshop shop;
  CHECK(setup::ReloadAshleyEquipment(shop.context, kShieldReload) == 0);
  CHECK(shop.host.released_models == std::vector<int>{1});
  CHECK(shop.host.pose_resets == 1);
}

TEST_CASE("Workshop weapon reload refuses a failed animation ticket",
          "[menu][menu-setup]") {
  constexpr int kWeaponReload = 0;
  constexpr int kFieldModel = 3;
  Workshop shop;
  shop.host.ashley_model = kFieldModel;
  CHECK(setup::ReloadAshleyEquipment(shop.context, kWeaponReload) == 1);
  shop.host.asset_outcome = Content::ActorAssetOutcome::kFailed;
  CHECK(setup::ReloadAshleyEquipment(shop.context, kWeaponReload) == 0);
  CHECK(shop.host.load_step == 0);
  CHECK(shop.host.pose_resets == 1);
  CHECK(shop.host.forgotten_assets.size() == 1);
  CHECK(shop.host.refusals ==
        Strings{"Ashley's weapon animation request failed"});
}

TEST_CASE("Workshop weapon reload refuses an unsubmitted animation ticket",
          "[menu][menu-setup]") {
  constexpr int kWeaponReload = 0;
  constexpr int kFieldModel = 3;
  Workshop shop;
  shop.host.ashley_model = kFieldModel;
  shop.host.queue_asset = false;
  CHECK(setup::ReloadAshleyEquipment(shop.context, kWeaponReload) == 0);
  CHECK(shop.host.load_step == 0);
  CHECK(shop.host.pose_resets == 1);
  CHECK(shop.host.refusals ==
        Strings{"Ashley's weapon animation request failed"});
}

TEST_CASE("Workshop ingredient counts leave accessories out",
          "[menu][menu-setup]") {
  constexpr std::uint8_t kBodyArmor = 2;
  constexpr std::uint8_t kAccessory = 7;
  Workshop shop;
  items::InventoryContents& inventory = shop.host.inventory;
  inventory.blades[0].id = 1;
  inventory.blades[3].id = 1;
  inventory.shields[1].base.id = 1;
  inventory.armor[0].id = 1;
  inventory.armor[0].category = kBodyArmor;
  inventory.armor[1].id = 1;
  inventory.armor[1].category = kAccessory;
  CHECK(setup::CountCombinationIngredients(shop.context, 0) == 2);
  CHECK(setup::CountCombinationIngredients(shop.context, 1) == 1);
  CHECK(setup::CountCombinationIngredients(shop.context, 2) == 1);
  CHECK(setup::CountCombinationIngredients(shop.context, 3) == 0);
  CHECK(shop.host.refusals == Strings{"combination category 3 outside 0-2"});
}

// The live workshop over its own data store (none until Open); its recipe
// books and state are reset when it goes.
class LiveRecipes {
 public:
  LiveRecipes() = default;
  LiveRecipes(const LiveRecipes&) = delete;
  LiveRecipes& operator=(const LiveRecipes&) = delete;
  LiveRecipes(LiveRecipes&&) = delete;
  LiveRecipes& operator=(LiveRecipes&&) = delete;
  ~LiveRecipes() {
    setup::ResetLiveSetupStatics();
    setup::LiveSetupState() = saved_state_;
  }

  static int Load(int id) {
    return setup::RunLiveSetup([id](setup::Context& context) {
      return setup::LoadCombinationRecipes(context, id);
    });
  }

 private:
  VS::Content::DataStore::ScopedOverride store_;
  setup::State saved_state_ = setup::LiveSetupState();
};

constexpr int kRecipePoll = 0;
constexpr int kShieldRecipeBook = 2;

TEST_CASE("Live workshop recipe loads refuse without recipe tables",
          "[menu][menu-setup]") {
  LiveRecipes live;
  VS::test_support::ChannelWarnings warnings{"menu.setup"};
  CHECK(LiveRecipes::Load(kShieldRecipeBook) == 0);
  CHECK(LiveRecipes::Load(kRecipePoll) == 1);
  CHECK(setup::LiveSetupRecipes() == nullptr);
  CHECK(warnings.messages ==
        Strings{"no workshop recipe tables are loaded (run VS.Extract tables "
                "<datadir>)",
                "recipe book loading without a pending load"});
}

// Checks the live recipe book is ShieldTable's.
void CheckLiveShieldBook() {
  const setup::RecipeBook* const book = setup::LiveSetupRecipes();
  REQUIRE(book != nullptr);
  CHECK(book->kind == setup::RecipeKind::kShield);
  CHECK(book->materials.size() == kShieldMaterialEntries);
  CHECK(book->materials[1] == kCombinedMaterial);
  REQUIRE(book->armor.size() == kShieldPairSide);
  CHECK(book->armor[1].model == kRecordModel);
}

// Converts ShieldTable's recipe files under `root` and opens a data store
// over it.
void OpenShieldRecipes(VS::test_support::WorkshopRecipeRoot& root) {
  root.Table(VS::Content::WorkshopKind::kShield) = ShieldTable();
  root.WriteSources();
  CHECK(root.Convert(false).converted == VS::Content::kWorkshopKindCount);
  const auto opened = VS::Content::DataStore::Open(root.Root());
  INFO(opened.error_or(""));
  REQUIRE(opened.has_value());
}

TEST_CASE("Live workshop recipe loads build the book from the data store",
          "[menu][menu-setup]") {
  VS::test_support::WorkshopRecipeRoot root;
  LiveRecipes live;
  OpenShieldRecipes(root);
  VS::test_support::ChannelWarnings warnings{"menu.setup"};
  CHECK(LiveRecipes::Load(kShieldRecipeBook) == 0);
  // A second load while the first is pending keeps the first.
  CHECK(LiveRecipes::Load(kShieldRecipeBook) == 0);
  CHECK(LiveRecipes::Load(kRecipePoll) == 1);
  CheckLiveShieldBook();
  CHECK(warnings.messages ==
        Strings{"recipe book load while the previous load was pending"});
}

TEST_CASE("Live workshop recipe loads over a held book free it first",
          "[menu][menu-setup]") {
  VS::test_support::WorkshopRecipeRoot root;
  LiveRecipes live;
  OpenShieldRecipes(root);
  CHECK(LiveRecipes::Load(kShieldRecipeBook) == 0);
  CHECK(LiveRecipes::Load(kRecipePoll) == 1);
  REQUIRE(setup::LiveSetupRecipes() != nullptr);
  VS::test_support::ChannelWarnings warnings{"menu.setup"};
  CHECK(LiveRecipes::Load(kShieldRecipeBook) == 0);
  CHECK(setup::LiveSetupRecipes() == nullptr);
  CHECK(LiveRecipes::Load(kRecipePoll) == 1);
  CHECK(setup::LiveSetupRecipes() != nullptr);
  setup::ResetLiveSetupStatics();
  CHECK(setup::LiveSetupRecipes() == nullptr);
  CHECK(warnings.messages ==
        Strings{"recipe book load while the previous book was held; freed "
                "it"});
}

TEST_CASE("MENUC.PRG keeps no module symbol entries", "[menu][menu-setup]") {
  const auto symbols = VS::Content::ModuleSymbols();
  CHECK(std::ranges::none_of(
      symbols, [](const VS::Content::ModuleSymbol& symbol) {
        return std::string_view(symbol.module) == "MENU/MENUC.PRG";
      }));
}
}  // namespace
