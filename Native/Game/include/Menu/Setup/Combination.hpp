#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <vector>

#include "Items/ItemRecords.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Framework/ItemRows.hpp"
#include "Menu/Setup/SetupScreen.hpp"

// MENUC.PRG's blade, shield and armor combination (168.c:2658-5100): the
// recipe files (BLADE.SYD, SHIELD.SYD, ARMOR.SYD), the combined records'
// formulas, the ingredient lists and the three combination screens.
namespace VS::Menu::Setup {
// _workshopMaterials: per workshop, the materials (one bit each) it works.
inline constexpr std::array<std::uint8_t, 7> kWorkshopMaterials{
    0, 0xE, 0x18, 0x38, 0xC0, 0x3E, 0xFE,
};
// _setClassAffinities' factors in tenths, by the factor lookup (0-2), for
// ingredients of different and of the same material.
inline constexpr std::array<std::int16_t, 3> kDifferentMaterialStatFactors{
    3,
    5,
    7,
};
inline constexpr std::array<std::int16_t, 3> kSameMaterialStatFactors{4, 6, 8};
// _setTypeValues' factors, by whether both ingredients peak in one type.
inline constexpr std::array<std::int16_t, 2> kDifferentMaterialTypeFactors{
    4,
    6,
};
inline constexpr std::array<std::int16_t, 2> kSameMaterialTypeFactors{6, 8};
// The combined values stay within +-100.
inline constexpr int kCombinedValueLimit = 100;

// A recipe file starts with three little-endian byte offsets: the result
// table, the material table and the result records (16 bytes a blade's, 8 a
// shield's or armor piece's).
inline constexpr std::size_t kRecipeHeaderBytes = 12;
inline constexpr std::size_t kBladeRecipeBytes = 16;
inline constexpr std::size_t kArmorRecipeBytes = 8;

// A BLADE.SYD result record (_combineBlades' info): the combined blade's
// model, category, damage and cost types, cost, unknown byte 6, base
// strength, intelligence and agility and its ranges and shape.
struct BladeRecipe {
  std::uint8_t model = 0;
  std::uint8_t category = 0;
  std::uint8_t damage_type = 0;
  std::uint8_t cost_type = 0;
  std::uint8_t cost = 0;
  std::uint8_t opaque = 0;
  std::uint8_t strength = 0;
  std::uint8_t intelligence = 0;
  std::uint8_t agility = 0;
  Items::WeaponRange range;
};
// A SHIELD.SYD or ARMOR.SYD result record (_armorInfo): the model, the
// category, the gem slots and the base strength, intelligence and agility.
struct ArmorRecipe {
  std::uint8_t model = 0;
  std::uint8_t category = 0;
  std::uint8_t gem_slots = 0;
  std::uint8_t strength = 0;
  std::uint8_t intelligence = 0;
  std::uint8_t agility = 0;
};
[[nodiscard]] BladeRecipe DecodeBladeRecipe(
    std::span<const std::uint8_t, kBladeRecipeBytes> record);
[[nodiscard]] ArmorRecipe DecodeArmorRecipe(
    std::span<const std::uint8_t, kArmorRecipeBytes> record);

// A decoded recipe file: the result record of each ingredient pair (by
// their ids), the combined material of each material pair (by both
// materials, and the categories for blades and armor) and the result records
// (blades for BLADE.SYD, shield or armor pieces for the others). Each table
// runs from its header offset to the next table's (the last to the end of
// the loaded block).
struct RecipeBook {
  RecipeKind kind = RecipeKind::kBlade;
  std::vector<std::uint8_t> results;
  std::vector<std::uint8_t> materials;
  std::vector<BladeRecipe> blades;
  std::vector<ArmorRecipe> armor;
};
// Decodes a loaded recipe file of `kind`, or says why its header does not
// hold three tables inside it.
[[nodiscard]] std::expected<RecipeBook, std::string> DecodeRecipeFile(
    RecipeKind kind, std::span<const std::uint8_t> file);

// Combines six classes (or seven affinities): a value either ingredient
// shares with the pair's greatest or least stays; the others take the sum
// scaled by the factor their strongest entries pick, within +-100.
void CombineClassOrAffinityValues(const Items::PartClasses& first,
                                  const Items::PartClasses& second,
                                  Items::PartClasses& result,
                                  bool materials_different, bool affinities);
// Combines the three physical types (entries 1-3; entry 0 stays).
void CombinePhysicalTypeValues(const Items::PartTypes& first,
                               const Items::PartTypes& second,
                               Items::PartTypes& result,
                               bool materials_different);
// The combined record from the decoded recipe file `recipes` (the first
// ingredient with the recipe's id, material, base stats and the combined
// values), or why the recipe lookup failed.
[[nodiscard]] std::expected<Items::InventoryBlade, std::string>
BuildCombinedBlade(const Items::InventoryBlade& first,
                   const Items::InventoryBlade& second,
                   const RecipeBook& recipes, const MaterialBonuses& bonuses);
[[nodiscard]] std::expected<Items::InventoryArmor, std::string>
BuildCombinedShield(const Items::InventoryArmor& first,
                    const Items::InventoryArmor& second,
                    const RecipeBook& recipes, const MaterialBonuses& bonuses);
[[nodiscard]] std::expected<Items::InventoryArmor, std::string>
BuildCombinedArmor(const Items::InventoryArmor& first,
                   const Items::InventoryArmor& second,
                   const RecipeBook& recipes, const MaterialBonuses& bonuses);

// Starts loading recipe file `id` (1-3), or (0) polls it: 1 once loaded or
// when nothing loads.
int LoadCombinationRecipes(Context& context, int id);
// The result row (row 0): the item's name, or "new blade/shield/armor".
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
Framework::MenuRow& SetCombinationResultRow(Context& context, int item_type,
                                            int item_id);
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
Framework::MenuRow& AnimateCombinationResult(Context& context, int item_type,
                                             int item_id);
void PreviewBladeCombination(Context& context, int selection);
int SelectCombinationBlade(Context& context, int slot);
void SetCombinationBladeRow(Context& context, Framework::MenuRow& row,
                            const Items::InventoryBlade& blade);
int UpdateBladeCombination(Context& context, bool initialize);
void SetShieldBaseStats(Context& context, const Items::InventoryShield& shield);
void PreviewShieldCombination(Context& context, int selection);
// The shield's row as it reads without its gems.
[[nodiscard]] Framework::ItemRowText SetShieldRowWithoutGems(
    Context& context, const Items::InventoryShield& shield,
    std::span<std::uint8_t> buffer);
int SelectCombinationShield(Context& context, int slot);
void SetCombinationShieldRow(Framework::MenuRow& row,
                             const Items::InventoryShield& shield);
int UpdateShieldCombination(Context& context, bool initialize);
void PreviewArmorCombination(Context& context, int selection);
int SelectCombinationArmor(Context& context, int slot);
void SetCombinationArmorRow(Framework::MenuRow& row,
                            const Items::InventoryArmor& armor);
int UpdateArmorCombination(Context& context, bool initialize);
// How many blades (0), shields (1) or armor pieces other than accessories
// (2) Ashley carries.
[[nodiscard]] int CountCombinationIngredients(Context& context, int category);
int UpdateCombinationCategory(Context& context, bool initialize);
}  // namespace VS::Menu::Setup
