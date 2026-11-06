// Hand-maintained (tools/port/owned.csv).
#include "Menu/Setup/Combination.hpp"  // NOLINT(build/include): typed API

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Items/Inventory.hpp"
#include "Menu/Setup/Assembly.hpp"

namespace VS::Menu::Setup {
namespace {
using Framework::MenuRow;
using Framework::PackXy;
using Framework::RowPlacement;
using Framework::RowState;
using Items::ItemCategory;

// _setClassAffinities' factor choice (0-2) by the first and the second
// ingredient's strongest class (or affinity).
using ClassRow = std::array<std::uint8_t, Items::kClassCount>;
using AffinityRow = std::array<std::uint8_t, Items::kAffinityCount>;
constexpr std::array<ClassRow, Items::kClassCount> kClassFactorLookup{{
    {2, 0, 1, 1, 1, 0},
    {0, 2, 0, 1, 1, 1},
    {1, 0, 2, 0, 1, 1},
    {1, 1, 0, 2, 0, 1},
    {1, 1, 1, 0, 2, 0},
    {0, 1, 1, 1, 0, 2},
}};
constexpr std::array<AffinityRow, Items::kAffinityCount> kAffinityFactorLookup{{
    {2, 1, 1, 1, 1, 1, 1},
    {1, 2, 1, 0, 1, 1, 1},
    {1, 1, 2, 1, 0, 1, 1},
    {1, 0, 1, 2, 1, 1, 1},
    {1, 1, 0, 1, 2, 1, 1},
    {1, 1, 1, 1, 1, 2, 0},
    {1, 1, 1, 1, 1, 0, 2},
}};
// The factors are tenths.
constexpr int kFactorDivisor = 10;
// _setTypeValues combines the three physical types after entry 0.
constexpr std::size_t kFirstPhysicalType = 1;
constexpr std::size_t kPhysicalTypeCount = 3;

// The recipe file header's three little-endian words.
constexpr std::size_t kRecipeTableCount = 3;
constexpr std::size_t kWordBytes = 4;
constexpr std::size_t kByteBits = 8;
// BLADE.SYD's material table by both blades' material (from bronze, 3) and
// category, and its result table by both blade ids.
constexpr int kBladeMaterialBase = 3;
constexpr int kBladeFirstMaterialStride = 500;
constexpr int kBladeFirstCategoryStride = 50;
constexpr int kBladeSecondMaterialStride = 10;
constexpr int kBladeIdStride = 96;
// SHIELD.SYD's by both materials and both sub ids.
constexpr int kShieldMaterialStride = 8;
constexpr int kShieldSubIdStride = 17;
// ARMOR.SYD's by both materials and categories (from 2), and both sub ids
// (from 16).
constexpr int kArmorFirstMaterialStride = 128;
constexpr int kArmorFirstCategoryStride = 32;
constexpr int kArmorSecondMaterialStride = 4;
constexpr int kArmorCategoryBase = 2;
constexpr int kArmorSubIdBase = 16;
constexpr int kArmorSubIdStride = 65;
// A combined shield's or armor piece's id follows its sub id.
constexpr int kArmorIdOffset = 126;
// A blade recipe record's bytes (_combineBlades' info).
constexpr std::size_t kBladeModelByte = 1;
constexpr std::size_t kBladeCategoryByte = 2;
constexpr std::size_t kBladeDamageTypeByte = 3;
constexpr std::size_t kBladeCostTypeByte = 4;
constexpr std::size_t kBladeCostByte = 5;
constexpr std::size_t kBladeOpaqueByte = 6;
constexpr std::size_t kBladeStrengthByte = 8;
constexpr std::size_t kBladeIntelligenceByte = 9;
constexpr std::size_t kBladeAgilityByte = 10;
constexpr std::size_t kBladeRangeByte = 12;
constexpr std::size_t kBladeSecondRangeByte = 13;
constexpr std::size_t kBladeThirdRangeByte = 14;
constexpr std::size_t kBladeShapeByte = 15;
// A shield or armor recipe record's bytes (_armorInfo).
constexpr std::size_t kArmorModelByte = 1;
constexpr std::size_t kArmorCategoryByte = 2;
constexpr std::size_t kArmorGemSlotsByte = 3;
constexpr std::size_t kArmorStrengthByte = 4;
constexpr std::size_t kArmorIntelligenceByte = 5;
constexpr std::size_t kArmorAgilityByte = 6;

// The screens' rows: the result (row 0) slides in to (155, 18); ingredient
// slot s (1-2) is row 10 + s at y 18 + 16s, sliding in to x 169.
constexpr RowPlacement kResultRow{
    .row = kTitleRow,
    .x = 155,
    .y = 18,
    .w = 165,
    .background_width = 0,
};
constexpr int kOffscreenX = 320;
constexpr int kIngredientStartX = 334;
constexpr int kIngredientX = 169;
constexpr int kIngredientWidth = 151;
constexpr int kIngredientTopY = 18;
constexpr int kRowPitch = 16;
// The screen's rows: the result, the two ingredients and "OK".
constexpr int kIngredientCount = 2;
constexpr int kOkRow = 3;
constexpr int kBothIngredients = 3;
constexpr int kOkLabelRow = 2;
constexpr std::uint8_t kBlinkStep = 16;
constexpr int kInformationRowState = 16;
// The cursor points at x 155 and y 10 + 16 a row, nudged left for the
// result and up for "OK".
constexpr int kCursorX = 155;
constexpr int kCursorTopY = 10;
constexpr int kResultCursorNudge = 14;
constexpr int kOkCursorRaise = 4;
// The ingredient lists mark parts in use: on an equipped weapon (or
// equipped), or on a carried weapon.
constexpr std::int32_t kInUseEquipped = 0xCA00;
constexpr std::int32_t kInUseCarried = 0xCC00;
constexpr std::uint8_t kEquippedOutset = 1;
constexpr std::uint8_t kCarriedOutset = 2;
constexpr std::uint8_t kShieldRowIcon = 15;
constexpr int kArmorRowIconBase = 14;
constexpr std::uint8_t kAccessoryCategory = 7;
// The equipment details: the stat pages, the panel and the subtypes.
constexpr std::size_t kClassAffinityStatRows = 16;
constexpr std::size_t kAffinityStatBase = 16;
constexpr std::size_t kTypeStatBase = 32;
constexpr std::size_t kPartSlotMask = 7;
constexpr std::size_t kStrengthValue = 4;
constexpr std::size_t kIntelligenceValue = 5;
constexpr std::size_t kAgilityValue = 6;
constexpr int kBladeSubtype = 2;
constexpr int kShieldSubtype = 8;
constexpr int kArmorSubtype = 16;
constexpr int kShowPanel = 1;
constexpr int kHidePanel = 2;
constexpr int kHideStatPages = -1;
constexpr int kHideDpPp = 4;
// The three-choice prompt: its "cancel combine" row and its answers.
constexpr int kCancelRow = 2;
constexpr int kAnswerCombine = 1;
constexpr int kAnswerBack = 2;
constexpr int kAnswerCancel = 3;
// _combineTopMenu: the rows hoist from the top menu's row 5 and its title
// returns on row 15; a category with fewer than two items reads "no items"
// (or "one item", four entries before).
constexpr int kCombineHoistRow = 5;
constexpr int kCombineSubtitleRow = 15;
constexpr int kCategoryRowInfo = 0x23D;
constexpr int kMinimumIngredients = 2;
constexpr int kFewItemsStride = 4;

// _combineBladeMenu's steps (the shield and armor menus share them).
enum CombineStep : std::uint8_t {
  kCombineHeader,
  kCombineRows,
  kCombineChoose,
  kCombinePick,
  kCombineAsk,
  kCombineAnswer,
  kCombineReload,
};
// _selectBlade's steps.
enum IngredientStep : std::uint8_t {
  kIngredientList,
  kIngredientChoose,
  kIngredientChosen,
};
// _combineTopMenu's steps after the choice: one per menu, then the return.
constexpr std::uint8_t kCombineReturn = kCategoryRun + kRecipeKindCount;

// The index of the first strictly greatest value (0 when none beats -128).
std::size_t StrongestIndex(std::span<const std::int8_t> values) {
  std::int8_t best = std::numeric_limits<std::int8_t>::min();
  std::size_t index = 0;
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (best < values[i]) {
      best = values[i];
      index = i;
    }
  }
  return index;
}

std::int8_t ScaledSum(int first, int second, int factor) {
  return static_cast<std::int8_t>(
      std::clamp(((first + second) * factor) / kFactorDivisor,
                 -kCombinedValueLimit, kCombinedValueLimit));
}

// A record's base value plus its material's bonus, kept to a byte.
std::int8_t WithBonus(std::uint8_t base, std::int8_t bonus) {
  return static_cast<std::int8_t>(
      static_cast<std::uint8_t>(base + static_cast<std::uint8_t>(bonus)));
}

std::expected<std::uint8_t, std::string> RecipeEntry(
    std::span<const std::uint8_t> table, int index, std::string_view what) {
  if (index < 0 || static_cast<std::size_t>(index) >= table.size()) {
    return std::unexpected(
        std::format("{} entry {} outside the recipe file", what, index));
  }
  return table[static_cast<std::size_t>(index)];
}

template <typename Record>
std::expected<Record, std::string> RecipeRecord(
    const std::vector<Record>& records, std::size_t index) {
  if (index >= records.size()) {
    return std::unexpected(
        std::format("recipe record {} outside the recipe file", index));
  }
  return records[index];
}

std::expected<MaterialBonus, std::string> BonusOf(
    const MaterialBonuses& bonuses, std::size_t material) {
  if (material >= bonuses.size()) {
    return std::unexpected(
        std::format("combined material {} has no stat bonus", material));
  }
  return bonuses[material];
}

// The combined record's material and sub id (its result record's index),
// from the material and result table entries, and the material's bonus.
struct RecipeResult {
  std::uint8_t material = 0;
  std::uint8_t sub_id = 0;
  MaterialBonus bonus;
};

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): table entries.
std::expected<RecipeResult, std::string> LookUpRecipe(
    const RecipeBook& recipes, RecipeKind kind, const MaterialBonuses& bonuses,
    int material_index, int result_index) {
  if (recipes.kind != kind) {
    return std::unexpected(std::format(
        "recipe file {} decoded where file {} belongs",
        static_cast<int>(recipes.kind) + 1, static_cast<int>(kind) + 1));
  }
  const auto material =
      RecipeEntry(recipes.materials, material_index, "combined material");
  if (!material) {
    return std::unexpected(material.error());
  }
  const auto sub_id = RecipeEntry(recipes.results, result_index, "combination");
  if (!sub_id) {
    return std::unexpected(sub_id.error());
  }
  const auto bonus = BonusOf(bonuses, *material);
  if (!bonus) {
    return std::unexpected(bonus.error());
  }
  return RecipeResult{
      .material = *material, .sub_id = *sub_id, .bonus = *bonus};
}

// _combineShields' and _combineArmor's shared part: the material, ids,
// stats, DP and values; the shield also takes the model and gem slots.
std::expected<Items::InventoryArmor, std::string> BuildCombinedArmorPiece(
    const Items::InventoryArmor& first, const Items::InventoryArmor& second,
    const RecipeBook& recipes, const RecipeResult& recipe) {
  const auto info = RecipeRecord(recipes.armor, recipe.sub_id);
  if (!info) {
    return std::unexpected(info.error());
  }
  Items::InventoryArmor result = first;
  result.material = recipe.material;
  result.sub_id = recipe.sub_id;
  result.id = static_cast<std::uint8_t>(recipe.sub_id + kArmorIdOffset);
  result.category = info->category;
  result.strength = WithBonus(info->strength, recipe.bonus.strength);
  result.intelligence =
      WithBonus(info->intelligence, recipe.bonus.intelligence);
  result.agility = WithBonus(info->agility, recipe.bonus.agility);
  result.max_dp =
      static_cast<std::uint16_t>((first.max_dp + second.max_dp) / 2);
  result.current_dp = result.max_dp;
  const bool different = first.material != second.material;
  CombineClassOrAffinityValues(first.classes, second.classes, result.classes,
                               different, false);
  CombineClassOrAffinityValues(first.affinities, second.affinities,
                               result.affinities, different, true);
  CombinePhysicalTypeValues(first.types, second.types, result.types, different);
  return result;
}

std::uint16_t HalfSum(std::uint16_t first, std::uint16_t second) {
  return static_cast<std::uint16_t>((first + second) >> 1);
}

// The equipment details' class (0-15) and affinity (16-31) entries, each
// table repeating its eight values.
void ShowClassesAndAffinities(Context& context,
                              const Items::PartClasses& classes,
                              const Items::PartAffinities& affinities) {
  for (std::size_t i = 0; i < kClassAffinityStatRows; ++i) {
    context.host.SetEquipmentStat(i, classes[i & kPartSlotMask]);
    context.host.SetEquipmentStat(kAffinityStatBase + i,
                                  affinities[i & kPartSlotMask]);
  }
}

// A shield's or armor piece's values, DP and stats as the details show them.
void ShowArmorValues(Context& context, const Items::InventoryArmor& armor,
                     int subtype) {
  ShowClassesAndAffinities(context, armor.classes, armor.affinities);
  for (std::size_t i = 0; i < armor.types.size(); ++i) {
    context.host.SetEquipmentStat(kTypeStatBase + i, armor.types[i]);
  }
  context.host.SetDpPp(armor.current_dp, armor.max_dp, 0, 0);
  context.host.SetPrimaryStats(armor.strength, armor.intelligence,
                               armor.agility, 1);
  context.host.SetPrimaryStatValue(kStrengthValue, armor.strength);
  context.host.SetPrimaryStatValue(kIntelligenceValue, armor.intelligence);
  context.host.SetPrimaryStatValue(kAgilityValue, armor.agility);
  context.host.SetEquipmentSubtype(subtype);
}

void ClearBladeStats(Context& context) {
  context.host.ResetEquipmentStats();
  context.host.SetRangeAndRisk(0, 0, 0, 1);
}

void ClearSlot(Context& context, ItemCategory category, int slot) {
  const Items::ItemsStatus status =
      Items::Inventory::ClearItemSlot(category, slot, context.host.Inventory());
  if (status != Items::ItemsStatus::kOk) {
    context.host.Refuse(Items::Describe(status));
  }
}

// Whether the current workshop works `material`.
bool WorkshopWorks(Context& context, int material) {
  const std::uint8_t workshop = context.state.combination.workshop;
  if (workshop >= kWorkshopMaterials.size()) {
    context.host.Refuse(std::format("workshop {} outside the table", workshop));
    return false;
  }
  if (material < 0 || material >= static_cast<int>(kByteBits)) {
    return false;
  }
  return ((kWorkshopMaterials[workshop] >> material) & 1) != 0;
}

// The three combination screens' differences.
struct BladeKind {
  using Record = Items::InventoryBlade;
  static constexpr int kItemType = 0;
  static constexpr std::size_t kHeaderText = menu12::kCombineBlade;
  static constexpr int kIcon = kWeaponIcon;
  static constexpr std::size_t kEmptyText = menu12::kBlade;
  static constexpr std::size_t kSelectText = menu12::kSelectBladeToCombine;
  static constexpr std::size_t kInvalidMaterialText =
      menu12::kInvalidBladeMaterial;
  static constexpr ItemCategory kCategory = ItemCategory::kBlade;
  static constexpr std::string_view kName = "blade";
  static constexpr bool kReloads = true;
  static constexpr int kReloadMode = 0;

  static CombinationMenuState& Menu(CombinationState& state) {
    return state.blade_menu;
  }
  static IngredientSelectionState& Selection(CombinationState& state) {
    return state.blade_selection;
  }
  static std::array<std::uint8_t, 2>& Ids(CombinationState& state) {
    return state.blade_ids;
  }
  static Record& Buffer(CombinationState& state) { return state.blade; }
  static auto& Table(Items::InventoryContents& inventory) {
    return inventory.blades;
  }
  static std::span<const std::uint8_t> Order(Items::InventoryIndices& order) {
    return order.blades;
  }
  static int Id(const Record& record) { return record.id; }
  static int Material(const Record& record) { return record.material; }
  static bool Listable(const Record& /*record*/) { return true; }
  static bool InUse(Context& /*context*/, const Record& record) {
    return record.assembled_weapon_index != 0;
  }
  static std::int32_t InUseFlags(Context& context, const Record& record) {
    if (record.assembled_weapon_index == 0) {
      return 0;
    }
    const Items::InventoryWeapon* weapon =
        InventorySlot(context, context.host.Inventory().weapons,
                      record.assembled_weapon_index, "blade's weapon");
    if (weapon == nullptr) {
      return 0;
    }
    return weapon->is_equipped == 0 ? kInUseCarried : kInUseEquipped;
  }
  static Framework::ItemRowText Format(Context& context, const Record& record,
                                       std::span<std::uint8_t> buffer) {
    return Framework::FormatBladeRow(context.host.ItemRows(), record, buffer);
  }
  static void SetRow(Context& context, MenuRow& row, const Record& record) {
    SetCombinationBladeRow(context, row, record);
  }
  static void ShowListed(Context& context, int id) {
    if (id != 0) {
      context.host.SetBladeStats(id);
    } else {
      ClearBladeStats(context);
    }
  }
  static void Preview(Context& context, int selection) {
    PreviewBladeCombination(context, selection);
  }
  static int Select(Context& context, int slot) {
    return SelectCombinationBlade(context, slot);
  }
  static std::expected<Record, std::string> Build(
      const Record& first, const Record& second, const RecipeBook& recipes,
      const MaterialBonuses& bonuses) {
    return BuildCombinedBlade(first, second, recipes, bonuses);
  }
  static void Store(Record& buffer, const Record& built) { buffer = built; }
  static void ClearInUse(Record& record) { record.assembled_weapon_index = 0; }
  // Takes the blade off its weapon; true when Ashley's weapon came off.
  static bool Consume(Context& context, const Record& record, int /*id*/) {
    const int weapon = record.assembled_weapon_index;
    if (weapon == 0) {
      return false;
    }
    const Items::InventoryWeapon* owner = InventorySlot(
        context, context.host.Inventory().weapons, weapon, "blade's weapon");
    const bool equipped = owner != nullptr && owner->is_equipped != 0;
    if (equipped) {
      context.host.UnequipWeapon();
    }
    DisassembleWeapon(context, weapon - 1);
    return equipped;
  }
};

struct ShieldKind {
  using Record = Items::InventoryShield;
  static constexpr int kItemType = 1;
  static constexpr std::size_t kHeaderText = menu12::kCombineShield;
  static constexpr int kIcon = kShieldIcon;
  static constexpr std::size_t kEmptyText = menu12::kShield;
  static constexpr std::size_t kSelectText = menu12::kSelectShield;
  static constexpr std::size_t kInvalidMaterialText =
      menu12::kInvalidShieldMaterial;
  static constexpr ItemCategory kCategory = ItemCategory::kShield;
  static constexpr std::string_view kName = "shield";
  static constexpr bool kReloads = true;
  static constexpr int kReloadMode = 1;

  static CombinationMenuState& Menu(CombinationState& state) {
    return state.shield_menu;
  }
  static IngredientSelectionState& Selection(CombinationState& state) {
    return state.shield_selection;
  }
  static std::array<std::uint8_t, 2>& Ids(CombinationState& state) {
    return state.shield_ids;
  }
  static Record& Buffer(CombinationState& state) { return state.shield; }
  static auto& Table(Items::InventoryContents& inventory) {
    return inventory.shields;
  }
  static std::span<const std::uint8_t> Order(Items::InventoryIndices& order) {
    return order.shields;
  }
  static int Id(const Record& record) { return record.base.id; }
  static int Material(const Record& record) { return record.base.material; }
  static bool Listable(const Record& /*record*/) { return true; }
  // Equipped, or holding a gem in one of its slots.
  static bool InUse(Context& context, const Record& record) {
    std::size_t slots = record.base.gem_slots;
    if (slots > record.gems.size()) {
      context.host.Refuse(std::format("shield with {} gem slots", slots));
      slots = record.gems.size();
    }
    return record.is_equipped != 0 ||
           std::ranges::any_of(std::span(record.gems).first(slots),
                               [](std::uint8_t gem) { return gem != 0; });
  }
  static std::int32_t InUseFlags(Context& /*context*/, const Record& record) {
    return record.is_equipped != 0 ? kInUseEquipped : 0;
  }
  static Framework::ItemRowText Format(Context& context, const Record& record,
                                       std::span<std::uint8_t> buffer) {
    return SetShieldRowWithoutGems(context, record, buffer);
  }
  static void SetRow(Context& /*context*/, MenuRow& row, const Record& record) {
    SetCombinationShieldRow(row, record);
  }
  static void ShowListed(Context& context, int id) {
    const Record* shield =
        id != 0 ? InventorySlot(context, context.host.Inventory().shields, id,
                                kName)
                : nullptr;
    if (shield != nullptr) {
      SetShieldBaseStats(context, *shield);
    } else {
      context.host.ResetEquipmentStats();
    }
  }
  static void Preview(Context& context, int selection) {
    PreviewShieldCombination(context, selection);
  }
  static int Select(Context& context, int slot) {
    return SelectCombinationShield(context, slot);
  }
  static std::expected<Items::InventoryArmor, std::string> Build(
      const Record& first, const Record& second, const RecipeBook& recipes,
      const MaterialBonuses& bonuses) {
    return BuildCombinedShield(first.base, second.base, recipes, bonuses);
  }
  // The preview keeps the rest of the shield empty.
  static void Store(Record& buffer, const Items::InventoryArmor& built) {
    buffer.base = built;
  }
  static void ClearInUse(Record& record) { record.is_equipped = 0; }
  // Takes the shield off Ashley and its gems out; true when it was equipped.
  static bool Consume(Context& context, const Record& record, int id) {
    const bool equipped = record.is_equipped != 0;
    if (equipped) {
      context.host.UnequipShield();
    }
    DetachShieldGems(context, id - 1);
    return equipped;
  }
};

struct ArmorKind {
  using Record = Items::InventoryArmor;
  static constexpr int kItemType = 2;
  static constexpr std::size_t kHeaderText = menu12::kCombineArmor;
  static constexpr int kIcon = kArmorIcon;
  static constexpr std::size_t kEmptyText = menu12::kArmor;
  static constexpr std::size_t kSelectText = menu12::kSelectArmor;
  static constexpr std::size_t kInvalidMaterialText =
      menu12::kInvalidArmorMaterial;
  static constexpr ItemCategory kCategory = ItemCategory::kArmor;
  static constexpr std::string_view kName = "armor";
  static constexpr bool kReloads = false;

  static CombinationMenuState& Menu(CombinationState& state) {
    return state.armor_menu;
  }
  static IngredientSelectionState& Selection(CombinationState& state) {
    return state.armor_selection;
  }
  static std::array<std::uint8_t, 2>& Ids(CombinationState& state) {
    return state.armor_ids;
  }
  static Record& Buffer(CombinationState& state) { return state.armor; }
  static auto& Table(Items::InventoryContents& inventory) {
    return inventory.armor;
  }
  static std::span<const std::uint8_t> Order(Items::InventoryIndices& order) {
    return order.armor;
  }
  static int Id(const Record& record) { return record.id; }
  static int Material(const Record& record) { return record.material; }
  // Accessories share the table but do not combine.
  static bool Listable(const Record& record) {
    return record.category != kAccessoryCategory;
  }
  static bool InUse(Context& /*context*/, const Record& record) {
    return record.limb != 0;
  }
  static std::int32_t InUseFlags(Context& /*context*/, const Record& record) {
    return record.limb != 0 ? kInUseEquipped : 0;
  }
  static Framework::ItemRowText Format(Context& context, const Record& record,
                                       std::span<std::uint8_t> buffer) {
    return Framework::FormatInventoryArmorRow(context.host.ItemRows(), record,
                                              buffer);
  }
  static void SetRow(Context& /*context*/, MenuRow& row, const Record& record) {
    SetCombinationArmorRow(row, record);
  }
  static void ShowListed(Context& context, int id) {
    if (id != 0) {
      context.host.SetArmorStats(id);
    } else {
      context.host.ResetEquipmentStats();
    }
  }
  static void Preview(Context& context, int selection) {
    PreviewArmorCombination(context, selection);
  }
  static int Select(Context& context, int slot) {
    return SelectCombinationArmor(context, slot);
  }
  static std::expected<Record, std::string> Build(
      const Record& first, const Record& second, const RecipeBook& recipes,
      const MaterialBonuses& bonuses) {
    return BuildCombinedArmor(first, second, recipes, bonuses);
  }
  static void Store(Record& buffer, const Record& built) { buffer = built; }
  static void ClearInUse(Record& record) { record.limb = 0; }
  // Takes the piece off Ashley's body part; Ashley's model stays.
  static bool Consume(Context& context, const Record& record, int id) {
    if (record.limb != 0) {
      context.host.EquipArmor(record.limb - 1, std::nullopt);
      Record* worn =
          InventorySlot(context, context.host.Inventory().armor, id, kName);
      if (worn != nullptr) {
        worn->limb = 0;
      }
    }
    return false;
  }
};

// The preview record: empty, the one ingredient, or (both chosen) the
// combined record; its in-use mark cleared. Returns which ingredients are
// chosen (bit 0 the first, bit 1 the second).
template <typename Kind>
int BuildPreview(Context& context) {
  CombinationState& combination = context.state.combination;
  const std::array<std::uint8_t, 2>& ids = Kind::Ids(combination);
  const int mask = (ids[0] != 0 ? 1 : 0) + (ids[1] != 0 ? 2 : 0);
  typename Kind::Record& buffer = Kind::Buffer(combination);
  auto& table = Kind::Table(context.host.Inventory());
  buffer = {};
  combination.refused = false;
  if (mask == kBothIngredients) {
    const auto* first = InventorySlot(context, table, ids[0], Kind::kName);
    const auto* second = InventorySlot(context, table, ids[1], Kind::kName);
    const RecipeBook* recipes = context.host.Recipes();
    if (first == nullptr || second == nullptr) {
      combination.refused = true;
    } else if (recipes == nullptr) {
      context.host.Refuse("combination without a loaded recipe file");
      combination.refused = true;
    } else if (auto built = Kind::Build(*first, *second, *recipes,
                                        context.host.Materials())) {
      Kind::Store(buffer, *built);
    } else {
      context.host.Refuse(built.error());
      combination.refused = true;
    }
  } else if (mask != 0) {
    const auto* record =
        InventorySlot(context, table, ids[0] + ids[1], Kind::kName);
    if (record != nullptr) {
      buffer = *record;
    } else {
      combination.refused = true;
    }
  }
  Kind::ClearInUse(buffer);
  return mask;
}

// The ingredient list: row 0 holds the slot's ingredient (or its
// placeholder), then the other listable records in display order, those
// the workshop cannot work unselectable.
template <typename Kind>
void ListIngredients(Context& context, int current) {
  CombinationState& combination = context.state.combination;
  const std::array<std::uint8_t, 2>& ids = Kind::Ids(combination);
  auto& table = Kind::Table(context.host.Inventory());
  PartListRows rows;
  const typename Kind::Record* record =
      current != 0 ? InventorySlot(context, table, current, Kind::kName)
                   : nullptr;
  if (record == nullptr) {
    AddPlaceholderRow(context, rows, Kind::kEmptyText, Kind::kSelectText);
  } else {
    AddPartRow(context, rows,
               Kind::Format(context, *record, NextPartBuffer(rows)), current);
  }
  for (const std::uint8_t id : Kind::Order(context.host.Indices())) {
    if (id == 0 || id == ids[0] || id == ids[1]) {
      continue;
    }
    const typename Kind::Record* listed =
        InventorySlot(context, table, id, Kind::kName);
    if (listed == nullptr || !Kind::Listable(*listed)) {
      continue;
    }
    const std::span<std::uint8_t> buffer = NextPartBuffer(rows);
    if (buffer.empty() ||
        !AddPartRow(context, rows, Kind::Format(context, *listed, buffer),
                    id)) {
      break;
    }
    if (!WorkshopWorks(context, Kind::Material(*listed))) {
      MarkLastPartInvalid(context, rows, Kind::kInvalidMaterialText);
    }
    rows.types[rows.count - 1] |= Kind::InUseFlags(context, *listed);
  }
  ShowPartList(context, rows);
}

// _selectBlade, _selectShields and _initUiArmor: a slot (1-2) opens the
// list; then 0 while choosing, the chosen record's 1-based slot, or < 0.
template <typename Kind>
int SelectIngredient(Context& context, int slot) {
  CombinationState& combination = context.state.combination;
  IngredientSelectionState& selection = Kind::Selection(combination);
  if (slot != 0) {
    selection.slot = static_cast<std::uint8_t>(slot);
    AnimateItemHeader(context, slot + kDetailRow);
    selection.step = kIngredientList;
    return 0;
  }
  switch (selection.step) {
    case kIngredientList: {
      if (!context.host.RowsIdle()) {
        break;
      }
      const std::array<std::uint8_t, 2>& ids = Kind::Ids(combination);
      int current = 0;
      if (CheckReference(context, selection.slot, ids.size(),
                         "ingredient slot")) {
        current = ids[static_cast<std::size_t>(selection.slot - 1)];
      }
      ListIngredients<Kind>(context, current);
      selection.step = kIngredientChoose;
      break;
    }
    case kIngredientChoose:
      Kind::ShowListed(context, ListedPart(context));
      return ChoosePart(context, selection.selected_row, selection.step);
    case kIngredientChosen:
      if (context.host.RowsIdle()) {
        return selection.selected_row;
      }
      break;
    default:
      break;
  }
  return 0;
}

RowPlacement IngredientPlacement(int slot, int x) {
  return {.row = slot + kDetailRow,
          .x = x,
          .y = (slot * kRowPitch) + kIngredientTopY,
          .w = kIngredientWidth,
          .background_width = 0};
}

// The ingredient record in slot `slot` (1-2); null when empty or refused.
template <typename Kind>
const typename Kind::Record* Ingredient(Context& context, int slot) {
  const int id =
      Kind::Ids(context.state.combination)[static_cast<std::size_t>(slot - 1)];
  if (id == 0) {
    return nullptr;
  }
  return InventorySlot(context, Kind::Table(context.host.Inventory()), id,
                       Kind::kName);
}

// Steps 0-1: the result row and the two ingredient rows slide in.
template <typename Kind>
void ShowCombinationRows(Context& context) {
  CombinationState& combination = context.state.combination;
  const typename Kind::Record& buffer = Kind::Buffer(combination);
  const int result_id = Kind::Id(buffer);
  MenuRow& result =
      AnimateCombinationResult(context, Kind::kItemType, result_id);
  if (result_id != 0) {
    Kind::SetRow(context, result, buffer);
  }
  for (int slot = 1; slot <= kIngredientCount; ++slot) {
    const typename Kind::Record* record = Ingredient<Kind>(context, slot);
    MenuRow& row = context.host.SetMenuItem(
        IngredientPlacement(slot, kIngredientStartX),
        record == nullptr ? Menu12Text(context, Kind::kEmptyText)
                          : ItemName(context, Kind::Id(*record)));
    row.state = RowState::kSlideX;
    row.target0 = kIngredientX;
    if (record == nullptr) {
      row.font_color = 1;
    } else {
      Kind::SetRow(context, row, *record);
    }
  }
}

// Cross on the screen: the last chosen ingredient goes back (or, with none,
// the screen closes).
template <typename Kind>
int ReturnIngredient(Context& context) {
  CombinationState& combination = context.state.combination;
  CombinationMenuState& menu = Kind::Menu(combination);
  const int slot = PopSlotSelection(context, kSlotHistoryCount);
  if (slot == 0) {
    return LeaveItemSelection(context, true);
  }
  if (slot > kIngredientCount) {
    context.host.Refuse(std::format("ingredient slot {} returned", slot));
    return LeaveItemSelection(context, true);
  }
  context.host.PlaySound(MenuSound::kLeave);
  menu.row = static_cast<std::uint8_t>(slot);
  Kind::Ids(combination)[static_cast<std::size_t>(slot - 1)] = 0;
  Kind::Preview(context, slot);
  MenuRow& row =
      context.host.SetMenuItem(IngredientPlacement(slot, kIngredientX),
                               Menu12Text(context, Kind::kEmptyText));
  row.selected = 1;
  row.font_color = 1;
  const typename Kind::Record& buffer = Kind::Buffer(combination);
  const int result_id = Kind::Id(buffer);
  MenuRow& result =
      SetCombinationResultRow(context, Kind::kItemType, result_id);
  if (result_id != 0) {
    Kind::SetRow(context, result, buffer);
  }
  return 0;
}

// The information box's text for row `row`: the record's description, the
// "press OK" text or the screen's prompt.
template <typename Kind>
std::span<const std::uint8_t> CombinationMessage(Context& context, int row) {
  CombinationState& combination = context.state.combination;
  std::span<const std::uint8_t> message =
      Menu12Text(context, Kind::kSelectText);
  const typename Kind::Record* record = nullptr;
  if (row == 0) {
    if (Kind::Id(Kind::Buffer(combination)) != 0) {
      record = &Kind::Buffer(combination);
    }
  } else if (row == kOkRow) {
    return Menu12Text(context, menu12::kPressOk);
  } else if (row <= kIngredientCount) {
    record = Ingredient<Kind>(context, row);
  }
  if (record != nullptr) {
    const std::span<std::uint8_t> buffer = context.host.StringBuffer();
    const Framework::ItemRowText text = Kind::Format(context, *record, buffer);
    if (text.description) {
      message = RowTextBytes(context, text.description, buffer);
    }
  }
  return message;
}

// The screen's rows' marks, the information box and the cursor.
template <typename Kind>
void ShowCombinationChoice(Context& context, int row) {
  CombinationState& combination = context.state.combination;
  CombinationMenuState& menu = Kind::Menu(combination);
  const std::array<std::uint8_t, 2>& ids = Kind::Ids(combination);
  const std::span<std::int32_t> types = context.host.RowTypes();
  if (types.size() <= static_cast<std::size_t>(kOkRow)) {
    context.host.Refuse("row type buffer shorter than the combination rows");
  } else {
    types[0] = Kind::Id(Kind::Buffer(combination)) < 1 ? 1 : 0;
    types[kOkRow] = 1;
  }
  for (int slot = 1; slot <= kIngredientCount; ++slot) {
    context.host.MenuItem(slot + kDetailRow).selected =
        static_cast<std::uint8_t>(slot == menu.row);
    if (types.size() > static_cast<std::size_t>(kOkRow)) {
      types[static_cast<std::size_t>(slot)] =
          ids[static_cast<std::size_t>(slot - 1)] == 0 ? 1 : 0;
    }
  }
  context.host.UpdateInformationVisibility(row, kInformationRowState);
  context.host.SetInformationMessage(CombinationMessage<Kind>(context, row));
  const int x = kCursorX - (row == 0 ? kResultCursorNudge : 0);
  const int y =
      (row * kRowPitch) + kCursorTopY - (row == kOkRow ? kOkCursorRaise : 0);
  menu.cursor = static_cast<std::uint8_t>(
      context.host.DrawCursor(menu.cursor, PackXy(x, y)));
}

// Step 2: the pad on the screen.
template <typename Kind>
int ChooseCombinationRow(Context& context, bool ready) {
  CombinationMenuState& menu = Kind::Menu(context.state.combination);
  const Framework::Buttons buttons = context.host.Input();
  if (Pressed(buttons, pad::kTriangle)) {
    return LeaveItemSelection(context, true);
  }
  if (ready) {
    DrawConfirmationLabel(context, kOkLabelRow, menu.ok_blink);
  }
  if (Pressed(buttons, pad::kCross)) {
    return ReturnIngredient<Kind>(context);
  }
  if (ready && Pressed(buttons, pad::kStart)) {
    context.host.PlaySound(MenuSound::kSelect);
    context.host.MenuItem(menu.row + kDetailRow).selected = 0;
    menu.row = kOkRow;
    menu.step = kCombineAsk;
    Kind::Preview(context, kOkRow);
    return 0;
  }
  int row = menu.row;
  if (Pressed(buttons, pad::kCircle)) {
    if (row == 0) {
      context.host.PlaySound(MenuSound::kInvalid);
    } else {
      context.host.PlaySound(MenuSound::kSelect);
      if (row == kOkRow) {
        menu.step = kCombineAsk;
      } else {
        context.host.ClearMenuExcept(kNoRow);
        Kind::Select(context, row);
        menu.step = kCombinePick;
      }
      return 0;
    }
  }
  const int last = (ready ? 1 : 0) + kIngredientCount;
  if (Repeated(buttons, pad::kUp)) {
    row = row == 0 ? last : row - 1;
  }
  if (Repeated(buttons, pad::kDown)) {
    row = row == last ? 0 : row + 1;
  }
  if (row != menu.row) {
    context.host.PlaySound(MenuSound::kChange);
    menu.row = static_cast<std::uint8_t>(row);
    Kind::Preview(context, row);
  }
  ShowCombinationChoice<Kind>(context, row);
  return 0;
}

// Step 3: the list's choice fills (or, chosen again, empties) the slot.
template <typename Kind>
int PickIngredient(Context& context) {
  CombinationState& combination = context.state.combination;
  CombinationMenuState& menu = Kind::Menu(combination);
  const int chosen = Kind::Select(context, 0);
  if (chosen == 0) {
    return 0;
  }
  if (chosen == -2) {
    return LeaveItemSelection(context, true);
  }
  const int slot = menu.row - 1;
  if (chosen > 0 && (slot < 0 || slot >= kIngredientCount)) {
    context.host.Refuse(std::format("ingredient row {} chosen", menu.row));
  } else if (chosen > 0) {
    std::uint8_t& id = Kind::Ids(combination)[static_cast<std::size_t>(slot)];
    if (chosen == id) {
      id = 0;
      PopSlotSelection(context, slot);
    } else {
      id = static_cast<std::uint8_t>(chosen);
      RecordSlotSelection(context, slot);
      ++menu.row;
      if (slot == 1 && Kind::Ids(combination)[0] == 0) {
        menu.row = static_cast<std::uint8_t>(slot);
      }
    }
    Kind::Preview(context, menu.row);
  }
  menu.step = kCombineRows;
  return 0;
}

// Step 4: the prompt, warning when an ingredient is in use.
template <typename Kind>
void AskCombination(Context& context) {
  CombinationMenuState& menu = Kind::Menu(context.state.combination);
  context.host.UpdatePrimaryStatPanel(kHidePanel);
  UpdateThreeChoicePrompt(context, kCancelRow);
  bool in_use = false;
  for (int slot = 1; slot <= kIngredientCount; ++slot) {
    const typename Kind::Record* record = Ingredient<Kind>(context, slot);
    in_use = (record != nullptr && Kind::InUse(context, *record)) || in_use;
  }
  context.host.SetInformationMessage(
      Menu12Text(context, in_use ? menu12::kCancelCombineConfirm
                                 : menu12::kAttachmentConfirm));
  menu.step = kCombineAnswer;
}

// The combination itself: both ingredients leave Ashley's equipment and the
// inventory, and the preview record takes the first one's slot. 1 once
// done, 0 while Ashley's model reloads.
template <typename Kind>
int CombineIngredients(Context& context) {
  CombinationState& combination = context.state.combination;
  CombinationMenuState& menu = Kind::Menu(combination);
  const std::array<std::uint8_t, 2> ids = Kind::Ids(combination);
  const auto capacity = Kind::Table(context.host.Inventory()).size();
  if (combination.refused ||
      !CheckReference(context, ids[0], capacity, Kind::kName) ||
      !CheckReference(context, ids[1], capacity, Kind::kName)) {
    context.host.Refuse("combination without a recipe result");
    return LeaveItemSelection(context, true);
  }
  context.host.PlaySoundEffect(sfx::kCombine);
  context.host.InitializeInformationBox();
  context.host.UpdateEquipmentStatPages(kHideStatPages);
  context.host.UpdateDpPpBars(kHideDpPp);
  context.host.ClearMenuExcept(kNoRow);
  context.host.FlyOutRowLeft(kTitleRow);
  for (const std::uint8_t id : ids) {
    const typename Kind::Record record =
        Kind::Table(context.host.Inventory())[id - 1U];
    if (Kind::Consume(context, record, id)) {
      menu.step = kCombineReload;
    }
    ClearSlot(context, Kind::kCategory, id);
  }
  typename Kind::Record& combined =
      Kind::Table(context.host.Inventory())[ids[0] - 1U];
  combined = Kind::Buffer(combination);
  combined.index = ids[0];
  const Items::ItemsStatus status = Items::Inventory::RebuildDisplayIndices(
      Kind::kCategory, context.host.Indices(), context.host.Inventory());
  if (status != Items::ItemsStatus::kOk) {
    context.host.Refuse(Items::Describe(status));
  }
  return menu.step != kCombineReload ? 1 : 0;
}

// Step 5: the prompt's answer.
template <typename Kind>
int AnswerCombination(Context& context) {
  CombinationMenuState& menu = Kind::Menu(context.state.combination);
  switch (UpdateThreeChoicePrompt(context, 0)) {
    case kAnswerCombine:
      return CombineIngredients<Kind>(context);
    case kAnswerBack:
      context.host.PlaySound(MenuSound::kLeave);
      context.host.UpdatePrimaryStatPanel(kShowPanel);
      menu.step = kCombineChoose;
      break;
    case kAnswerCancel:
      return LeaveItemSelection(context, true);
    default:
      break;
  }
  return 0;
}

// _combineBladeMenu, _combineShieldMenu and _combineArmorMenu.
template <typename Kind>
int UpdateCombinationMenu(Context& context, bool initialize) {
  CombinationState& combination = context.state.combination;
  CombinationMenuState& menu = Kind::Menu(combination);
  std::array<std::uint8_t, 2>& ids = Kind::Ids(combination);
  if (initialize) {
    RecordSlotSelection(context, kSlotHistoryCount);
    ids = {};
    Kind::Preview(context, 0);
    menu.row = 1;
    menu.step = kCombineHeader;
    return 0;
  }
  const bool ready = ids[0] != 0 && ids[1] != 0;
  menu.ok_blink = static_cast<std::uint8_t>(menu.ok_blink + kBlinkStep);
  if (menu.step != kCombineChoose) {
    menu.ok_blink = 0;
  }
  switch (menu.step) {
    case kCombineHeader:
      if (!context.host.RowsIdle()) {
        break;
      }
      InitializeEquipmentHeader(context, Kind::kHeaderText, Kind::kIcon);
      [[fallthrough]];
    case kCombineRows:
      ShowCombinationRows<Kind>(context);
      menu.step = kCombineChoose;
      break;
    case kCombineChoose:
      if (context.host.RowsIdle()) {
        return ChooseCombinationRow<Kind>(context, ready);
      }
      menu.ok_blink = 0;
      break;
    case kCombinePick:
      return PickIngredient<Kind>(context);
    case kCombineAsk:
      AskCombination<Kind>(context);
      break;
    case kCombineAnswer:
      return AnswerCombination<Kind>(context);
    case kCombineReload:
      if constexpr (Kind::kReloads) {
        if (ReloadAshleyEquipment(context, Kind::kReloadMode) == 0) {
          return -1;
        }
      }
      break;
    default:
      break;
  }
  return 0;
}

using CombinationMenu = int (*)(Context&, bool);
constexpr std::array<CombinationMenu, kRecipeKindCount> kCombinationMenus{
    UpdateBladeCombination,
    UpdateShieldCombination,
    UpdateArmorCombination,
};

void CreateCombinationRows(Context& context, CategoryState& menu) {
  std::array<Framework::RowText, kRecipeKindCount> texts{};
  std::array<std::int32_t, kRecipeKindCount> types{};
  for (std::size_t i = 0; i < kRecipeKindCount; ++i) {
    const int kind = static_cast<int>(i);
    const int count = CountCombinationIngredients(context, kind);
    texts[i].name = Menu12Entry(context, kind + menu12::kBladeIndex);
    texts[i].description =
        count < kMinimumIngredients
            ? Menu12Entry(context,
                          menu12::kNoItemsIndex - (count * kFewItemsStride))
            : Menu12Entry(context, kind + menu12::kCombineBladesIndex);
    types[i] = count < kMinimumIngredients ? 1 : 0;
  }
  CreateCategoryRows(context, menu, kCategoryRowInfo, texts, types);
}

int ChooseCombination(Context& context, CategoryState& menu) {
  context.host.UpdateInformationVisibility(context.host.HighlightedRow(),
                                           context.host.ItemsListRow());
  const int selected = context.host.SelectedRow();
  const int choice = selected + 1;
  if (choice == 0) {
    return 0;
  }
  if (choice < 0) {
    context.host.ClearMenuExcept(kTitleRow);
    return choice;
  }
  if (static_cast<std::size_t>(selected) >= kCombinationMenus.size()) {
    context.host.Refuse(std::format("combination row {} chosen", selected));
    return 0;
  }
  context.host.ClearMenuExcept(kNoRow);
  kCombinationMenus[static_cast<std::size_t>(selected)](context, true);
  LoadCombinationRecipes(context, choice);
  menu.step = static_cast<std::uint8_t>(selected + kCategoryRun);
  return 0;
}

int RunCombination(Context& context, CategoryState& menu) {
  if (LoadCombinationRecipes(context, 0) == 0) {
    return 0;
  }
  const int result =
      kCombinationMenus[static_cast<std::size_t>(menu.step - kCategoryRun)](
          context, false);
  if (result == 0) {
    return 0;
  }
  context.host.FreeRecipes();
  if (result == -1) {
    menu.step = kCombineReturn;
    return 0;
  }
  return result;
}
}  // namespace

BladeRecipe DecodeBladeRecipe(
    std::span<const std::uint8_t, kBladeRecipeBytes> record) {
  return {.model = record[kBladeModelByte],
          .category = record[kBladeCategoryByte],
          .damage_type = record[kBladeDamageTypeByte],
          .cost_type = record[kBladeCostTypeByte],
          .cost = record[kBladeCostByte],
          .opaque = record[kBladeOpaqueByte],
          .strength = record[kBladeStrengthByte],
          .intelligence = record[kBladeIntelligenceByte],
          .agility = record[kBladeAgilityByte],
          .range = {.range = record[kBladeRangeByte],
                    .second_range = record[kBladeSecondRangeByte],
                    .third_range = record[kBladeThirdRangeByte],
                    .shape_bits = record[kBladeShapeByte]}};
}

ArmorRecipe DecodeArmorRecipe(
    std::span<const std::uint8_t, kArmorRecipeBytes> record) {
  return {.model = record[kArmorModelByte],
          .category = record[kArmorCategoryByte],
          .gem_slots = record[kArmorGemSlotsByte],
          .strength = record[kArmorStrengthByte],
          .intelligence = record[kArmorIntelligenceByte],
          .agility = record[kArmorAgilityByte]};
}

// decomp: MENUC.PRG _setClassAffinities (src/MENU/MENUC.PRG/168.c:4759)
void CombineClassOrAffinityValues(const Items::PartClasses& first,
                                  const Items::PartClasses& second,
                                  Items::PartClasses& result,
                                  bool materials_different, bool affinities) {
  const std::size_t count =
      affinities ? Items::kAffinityCount : Items::kClassCount;
  const std::span<const std::int8_t> ones = std::span(first).first(count);
  const std::span<const std::int8_t> twos = std::span(second).first(count);
  const std::size_t first_strongest = StrongestIndex(ones);
  const std::size_t second_strongest = StrongestIndex(twos);
  const std::int8_t greatest =
      std::max(std::ranges::max(ones), std::ranges::max(twos));
  const std::int8_t least =
      std::min(std::ranges::min(ones), std::ranges::min(twos));
  const int lookup =
      affinities ? kAffinityFactorLookup[first_strongest][second_strongest]
                 : kClassFactorLookup[first_strongest][second_strongest];
  const auto& factors = materials_different ? kDifferentMaterialStatFactors
                                            : kSameMaterialStatFactors;
  for (std::size_t i = 0; i < count; ++i) {
    if (ones[i] == greatest || twos[i] == greatest) {
      result[i] = greatest;
    } else if (ones[i] == least || twos[i] == least) {
      result[i] = least;
    } else {
      result[i] = ScaledSum(ones[i], twos[i],
                            factors[static_cast<std::size_t>(lookup)]);
    }
  }
}

// decomp: MENUC.PRG _setTypeValues (src/MENU/MENUC.PRG/168.c:4864)
void CombinePhysicalTypeValues(const Items::PartTypes& first,
                               const Items::PartTypes& second,
                               Items::PartTypes& result,
                               bool materials_different) {
  const std::size_t first_strongest = StrongestIndex(
      std::span(first).subspan(kFirstPhysicalType, kPhysicalTypeCount));
  const std::size_t second_strongest = StrongestIndex(
      std::span(second).subspan(kFirstPhysicalType, kPhysicalTypeCount));
  const auto& factors = materials_different ? kDifferentMaterialTypeFactors
                                            : kSameMaterialTypeFactors;
  const int factor = factors[first_strongest == second_strongest ? 1U : 0U];
  for (std::size_t i = kFirstPhysicalType;
       i < kFirstPhysicalType + kPhysicalTypeCount; ++i) {
    result[i] = ScaledSum(first[i], second[i], factor);
  }
}

// decomp: MENUC.PRG _combineBlades (src/MENU/MENUC.PRG/168.c:4934)
std::expected<Items::InventoryBlade, std::string> BuildCombinedBlade(
    const Items::InventoryBlade& first, const Items::InventoryBlade& second,
    const RecipeBook& recipes, const MaterialBonuses& bonuses) {
  const int material_index =
      ((first.material - kBladeMaterialBase) * kBladeFirstMaterialStride) +
      (first.category * kBladeFirstCategoryStride) +
      ((second.material - kBladeMaterialBase) * kBladeSecondMaterialStride) +
      second.category;
  const auto recipe =
      LookUpRecipe(recipes, RecipeKind::kBlade, bonuses, material_index,
                   (first.id * kBladeIdStride) + second.id);
  if (!recipe) {
    return std::unexpected(recipe.error());
  }
  const auto info = RecipeRecord(recipes.blades, recipe->sub_id);
  if (!info) {
    return std::unexpected(info.error());
  }
  Items::InventoryBlade result = first;
  result.material = recipe->material;
  result.sub_id = recipe->sub_id;
  result.id = recipe->sub_id;
  result.weapon_model_id = info->model;
  result.category = info->category;
  result.cost = info->cost;
  // The blade's type byte keeps its bits above the damage and cost types.
  constexpr std::uint8_t kBladeTypeBits =
      Items::kBladeDamageTypeMask |
      (Items::kBladeCostTypeMask << Items::kBladeCostTypeShift);
  result.type_bits = static_cast<std::uint8_t>(
      (first.type_bits & ~kBladeTypeBits) |
      (info->damage_type & Items::kBladeDamageTypeMask) |
      ((info->cost_type & Items::kBladeCostTypeMask)
       << Items::kBladeCostTypeShift));
  result.opaque12 = info->opaque;
  result.range = info->range;
  result.strength = WithBonus(info->strength, recipe->bonus.strength);
  result.intelligence =
      WithBonus(info->intelligence, recipe->bonus.intelligence);
  result.agility = WithBonus(info->agility, recipe->bonus.agility);
  result.max_dp = HalfSum(first.max_dp, second.max_dp);
  result.current_dp = result.max_dp;
  result.max_pp = HalfSum(first.max_pp, second.max_pp);
  result.current_pp = HalfSum(first.current_pp, second.current_pp);
  const bool different = first.material != second.material;
  CombineClassOrAffinityValues(first.classes, second.classes, result.classes,
                               different, false);
  CombineClassOrAffinityValues(first.affinities, second.affinities,
                               result.affinities, different, true);
  return result;
}

// decomp: MENUC.PRG _combineShields (src/MENU/MENUC.PRG/168.c:5012)
std::expected<Items::InventoryArmor, std::string> BuildCombinedShield(
    const Items::InventoryArmor& first, const Items::InventoryArmor& second,
    const RecipeBook& recipes, const MaterialBonuses& bonuses) {
  const auto recipe =
      LookUpRecipe(recipes, RecipeKind::kShield, bonuses,
                   (first.material * kShieldMaterialStride) + second.material,
                   (first.sub_id * kShieldSubIdStride) + second.sub_id);
  if (!recipe) {
    return std::unexpected(recipe.error());
  }
  auto result = BuildCombinedArmorPiece(first, second, recipes, *recipe);
  if (result) {
    const ArmorRecipe& info = recipes.armor[recipe->sub_id];
    result->weapon_model_id = info.model;
    result->gem_slots = info.gem_slots;
    result->max_pp = HalfSum(first.max_pp, second.max_pp);
    result->current_pp = HalfSum(first.current_pp, second.current_pp);
  }
  return result;
}

// decomp: MENUC.PRG _combineArmor (src/MENU/MENUC.PRG/168.c:5061)
std::expected<Items::InventoryArmor, std::string> BuildCombinedArmor(
    const Items::InventoryArmor& first, const Items::InventoryArmor& second,
    const RecipeBook& recipes, const MaterialBonuses& bonuses) {
  const int material_index =
      (first.material * kArmorFirstMaterialStride) +
      ((first.category - kArmorCategoryBase) * kArmorFirstCategoryStride) +
      (second.material * kArmorSecondMaterialStride) +
      (second.category - kArmorCategoryBase);
  const int result_index =
      ((first.sub_id - kArmorSubIdBase) * kArmorSubIdStride) +
      (second.sub_id - kArmorSubIdBase);
  const auto recipe = LookUpRecipe(recipes, RecipeKind::kArmor, bonuses,
                                   material_index, result_index);
  if (!recipe) {
    return std::unexpected(recipe.error());
  }
  return BuildCombinedArmorPiece(first, second, recipes, *recipe);
}

std::expected<RecipeBook, std::string> DecodeRecipeFile(
    RecipeKind kind, std::span<const std::uint8_t> file) {
  if (file.size() < kRecipeHeaderBytes) {
    return std::unexpected(
        std::format("recipe file of {} bytes has no header", file.size()));
  }
  std::array<std::size_t, kRecipeTableCount> offsets{};
  for (std::size_t table = 0; table < kRecipeTableCount; ++table) {
    for (std::size_t byte = 0; byte < kWordBytes; ++byte) {
      offsets[table] |=
          static_cast<std::size_t>(file[(table * kWordBytes) + byte])
          << (byte * kByteBits);
    }
    if (offsets[table] > file.size()) {
      return std::unexpected(
          std::format("recipe table {} at {} outside the {}-byte file", table,
                      offsets[table], file.size()));
    }
  }
  // A table ends where the next table after it starts, or with the file.
  const auto table_bytes = [&](std::size_t table) {
    std::size_t end = file.size();
    for (const std::size_t offset : offsets) {
      if (offset > offsets[table]) {
        end = std::min(end, offset);
      }
    }
    return file.subspan(offsets[table], end - offsets[table]);
  };
  const std::span<const std::uint8_t> results = table_bytes(0);
  const std::span<const std::uint8_t> materials = table_bytes(1);
  const std::span<const std::uint8_t> records = table_bytes(2);
  RecipeBook book{};
  book.kind = kind;
  book.results.assign(results.begin(), results.end());
  book.materials.assign(materials.begin(), materials.end());
  if (kind == RecipeKind::kBlade) {
    for (std::size_t at = 0; at + kBladeRecipeBytes <= records.size();
         at += kBladeRecipeBytes) {
      book.blades.push_back(
          DecodeBladeRecipe(records.subspan(at).first<kBladeRecipeBytes>()));
    }
  } else {
    for (std::size_t at = 0; at + kArmorRecipeBytes <= records.size();
         at += kArmorRecipeBytes) {
      book.armor.push_back(
          DecodeArmorRecipe(records.subspan(at).first<kArmorRecipeBytes>()));
    }
  }
  return book;
}

// decomp: MENUC.PRG _loadSyd (src/MENU/MENUC.PRG/168.c:2658)
int LoadCombinationRecipes(Context& context, int id) {
  CombinationState& combination = context.state.combination;
  if (id != 0) {
    if (id < 1 || id > static_cast<int>(kRecipeKindCount)) {
      context.host.Refuse(std::format("recipe file {} outside 1-3", id));
      return 0;
    }
    context.host.StartRecipeLoad(static_cast<RecipeKind>(id - 1));
    combination.recipes_loading = true;
    return 0;
  }
  if (!combination.recipes_loading) {
    return 1;
  }
  const std::optional<bool> loaded = context.host.RecipeLoaded();
  if (!loaded) {
    context.host.Refuse("recipe file loading without a pending read");
    combination.recipes_loading = false;
    return 1;
  }
  if (*loaded) {
    context.host.ReleaseRecipeRequest();
    combination.recipes_loading = false;
    return 1;
  }
  return 0;
}

// decomp: MENUC.PRG _setItemRow (src/MENU/MENUC.PRG/168.c:2696)
MenuRow& SetCombinationResultRow(Context& context, int item_type, int item_id) {
  MenuRow& row = context.host.SetMenuItem(
      kResultRow, item_id == 0
                      ? Menu12Entry(context, item_type + menu12::kNewBladeIndex)
                      : ItemName(context, item_id));
  row.selected = 1;
  return row;
}

// decomp: MENUC.PRG _initItemRow (src/MENU/MENUC.PRG/168.c:2710)
MenuRow& AnimateCombinationResult(Context& context, int item_type,
                                  int item_id) {
  MenuRow& row = SetCombinationResultRow(context, item_type, item_id);
  row.state = RowState::kSlideX;
  row.target0 = static_cast<std::int16_t>(kResultRow.x);
  row.x = kOffscreenX;
  return row;
}

// decomp: MENUC.PRG _setCombineBladeUi (src/MENU/MENUC.PRG/168.c:2735)
void PreviewBladeCombination(Context& context, int selection) {
  const int mask = BuildPreview<BladeKind>(context);
  const Items::InventoryBlade& blade = context.state.combination.blade;
  ClearBladeStats(context);
  const auto ingredient = static_cast<unsigned>(selection - 1);
  if (ingredient >= static_cast<unsigned>(kIngredientCount)) {
    if (blade.id != 0) {
      ShowClassesAndAffinities(context, blade.classes, blade.affinities);
      context.host.SetDpPp(blade.current_dp, blade.max_dp, blade.current_pp,
                           blade.max_pp);
      context.host.SetPrimaryStats(blade.strength, blade.intelligence,
                                   blade.agility, 1);
      context.host.SetRangeAndRisk(blade.range.range, blade.cost, 0, 1);
      context.host.SetEquipmentSubtype(kBladeSubtype);
    }
  } else if ((mask & selection) != 0) {
    context.host.SetBladeStats(context.state.combination.blade_ids[ingredient]);
  }
}

// decomp: MENUC.PRG _selectBlade (src/MENU/MENUC.PRG/168.c:2801)
int SelectCombinationBlade(Context& context, int slot) {
  return SelectIngredient<BladeKind>(context, slot);
}

// decomp: MENUC.PRG _setBladeMenuItem (src/MENU/MENUC.PRG/168.c:2919)
void SetCombinationBladeRow(Context& context, MenuRow& row,
                            const Items::InventoryBlade& blade) {
  row.row_icon = blade.category;
  row.material = static_cast<std::uint8_t>(blade.material);
  if (blade.assembled_weapon_index == 0) {
    return;
  }
  const Items::InventoryWeapon* weapon =
      InventorySlot(context, context.host.Inventory().weapons,
                    blade.assembled_weapon_index, "blade's weapon");
  if (weapon != nullptr) {
    row.outset_icon =
        weapon->is_equipped == 0 ? kCarriedOutset : kEquippedOutset;
  }
}

// decomp: MENUC.PRG _combineBladeMenu (src/MENU/MENUC.PRG/168.c:2939)
int UpdateBladeCombination(Context& context, bool initialize) {
  return UpdateCombinationMenu<BladeKind>(context, initialize);
}

// decomp: MENUC.PRG _setShieldUi (src/MENU/MENUC.PRG/168.c:3317)
void SetShieldBaseStats(Context& context,
                        const Items::InventoryShield& shield) {
  context.host.ResetEquipmentStats();
  ShowArmorValues(context, shield.base, kShieldSubtype);
}

// decomp: MENUC.PRG _initCombineShields (src/MENU/MENUC.PRG/168.c:3353)
void PreviewShieldCombination(Context& context, int selection) {
  const int mask = BuildPreview<ShieldKind>(context);
  const Items::InventoryShield& shield = context.state.combination.shield;
  context.host.ResetEquipmentStats();
  const auto ingredient = static_cast<unsigned>(selection - 1);
  if (ingredient >= static_cast<unsigned>(kIngredientCount)) {
    if (shield.base.id != 0) {
      SetShieldBaseStats(context, shield);
    }
  } else if ((mask & selection) != 0) {
    const Items::InventoryShield* listed =
        Ingredient<ShieldKind>(context, selection);
    if (listed != nullptr) {
      SetShieldBaseStats(context, *listed);
    }
  }
}

// decomp: MENUC.PRG _initUiShield (src/MENU/MENUC.PRG/168.c:3404)
Framework::ItemRowText SetShieldRowWithoutGems(
    Context& context, const Items::InventoryShield& shield,
    std::span<std::uint8_t> buffer) {
  Items::InventoryShield bare = shield;
  bare.gems = {};
  return Framework::FormatInventoryShieldRow(context.host.ItemRows(), bare,
                                             buffer);
}

// decomp: MENUC.PRG _selectShields (src/MENU/MENUC.PRG/168.c:3421)
int SelectCombinationShield(Context& context, int slot) {
  return SelectIngredient<ShieldKind>(context, slot);
}

// decomp: MENUC.PRG _setShieldMenuItem (src/MENU/MENUC.PRG/168.c:3548)
void SetCombinationShieldRow(MenuRow& row,
                             const Items::InventoryShield& shield) {
  row.row_icon = kShieldRowIcon;
  row.material = static_cast<std::uint8_t>(shield.base.material);
  row.outset_icon = shield.is_equipped != 0 ? 1 : 0;
}

// decomp: MENUC.PRG _combineShieldMenu (src/MENU/MENUC.PRG/168.c:3562)
int UpdateShieldCombination(Context& context, bool initialize) {
  return UpdateCombinationMenu<ShieldKind>(context, initialize);
}

// decomp: MENUC.PRG _initCombineArmor (src/MENU/MENUC.PRG/168.c:3947)
void PreviewArmorCombination(Context& context, int selection) {
  const int mask = BuildPreview<ArmorKind>(context);
  const Items::InventoryArmor& armor = context.state.combination.armor;
  context.host.ResetEquipmentStats();
  const auto ingredient = static_cast<unsigned>(selection - 1);
  if (ingredient >= static_cast<unsigned>(kIngredientCount)) {
    if (armor.id != 0) {
      ShowArmorValues(context, armor, kArmorSubtype);
    }
  } else if ((mask & selection) != 0) {
    context.host.SetArmorStats(context.state.combination.armor_ids[ingredient]);
  }
}

// decomp: MENUC.PRG _initUiArmor (src/MENU/MENUC.PRG/168.c:4019)
int SelectCombinationArmor(Context& context, int slot) {
  return SelectIngredient<ArmorKind>(context, slot);
}

// decomp: MENUC.PRG _setArmorMenuItem (src/MENU/MENUC.PRG/168.c:4126)
void SetCombinationArmorRow(MenuRow& row, const Items::InventoryArmor& armor) {
  row.row_icon = static_cast<std::uint8_t>(armor.category + kArmorRowIconBase);
  row.material = static_cast<std::uint8_t>(armor.material);
  row.outset_icon = armor.limb != 0 ? 1 : 0;
}

// decomp: MENUC.PRG _combineArmorMenu (src/MENU/MENUC.PRG/168.c:4140)
int UpdateArmorCombination(Context& context, bool initialize) {
  return UpdateCombinationMenu<ArmorKind>(context, initialize);
}

// decomp: MENUC.PRG _countItems (src/MENU/MENUC.PRG/168.c:4428)
int CountCombinationIngredients(Context& context, int category) {
  const Items::InventoryContents& inventory = context.host.Inventory();
  switch (category) {
    case 0:
      return Items::Inventory::CountItems(ItemCategory::kBlade, inventory);
    case 1:
      return Items::Inventory::CountItems(ItemCategory::kShield, inventory);
    case 2:
      return static_cast<int>(std::ranges::count_if(
          inventory.armor, [](const Items::InventoryArmor& armor) {
            return armor.id != 0 && armor.category != kAccessoryCategory;
          }));
    default:
      context.host.Refuse(
          std::format("combination category {} outside 0-2", category));
      return 0;
  }
}

// decomp: MENUC.PRG _combineTopMenu (src/MENU/MENUC.PRG/168.c:4455)
int UpdateCombinationCategory(Context& context, bool initialize) {
  CategoryState& menu = context.state.combination.category;
  if (initialize) {
    menu.keep_cursor = true;
    context.host.FlyOutRowsAndHoistSelection(kCombineHoistRow, 1);
    menu.step = kCategoryRows;
    return 0;
  }
  switch (menu.step) {
    case kCategoryRows:
      if (context.host.RowsIdle()) {
        CreateCombinationRows(context, menu);
      }
      break;
    case kCategoryChoose:
      return ChooseCombination(context, menu);
    case kCombineReturn:
      ReturnToCategory(context, menu, kCombineSubtitleRow, menu12::kCombine);
      break;
    default:
      if (menu.step >= kCategoryRun && menu.step < kCombineReturn) {
        return RunCombination(context, menu);
      }
      break;
  }
  return 0;
}
}  // namespace VS::Menu::Setup
