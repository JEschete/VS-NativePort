#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Content/ActorAssets.hpp"
#include "Menu/Framework/Framework.hpp"

// The equipment menus' choice list (MENU3.PRG 16C.c:39-336): the item text
// records it owns, its rows, cursor and scrolling, the accessory preview's
// stat columns, and the preview model reload (68C4.c:12-127).
namespace VS::Menu::Inventory {
// The first menu row the list owns; rows kEquipmentFirstRow + 0..5 show it.
inline constexpr int kEquipmentFirstRow = 20;
inline constexpr int kEquipmentVisibleRows = 6;
// The callers list an equipment category's slots and a "none" entry; the
// largest (armor) lists 17. A wider list is refused, not truncated.
inline constexpr std::size_t kEquipmentRowCapacity = 64;
// The 13 encoded name halfwords and the 46 description halfwords the list
// copies from its callers' strings.
inline constexpr std::size_t kEquipmentNameBytes = 26;
inline constexpr std::size_t kEquipmentDescriptionBytes = 92;

// _processEquipSubMenu's state byte (16C.c:111): 0-5 slide a row in,
// 16 waits for the rows, 17 takes input.
inline constexpr std::uint8_t kEquipmentRowsWaiting = 16;
inline constexpr std::uint8_t kEquipmentRowsInput = 17;
// _selectedItemRow after Triangle and Cross (16C.c:174, 210).
inline constexpr std::int32_t kEquipmentChoiceBack = -3;
inline constexpr std::int32_t kEquipmentChoiceCancel = -2;

// The globals 16C.c shares with its callers: _itemsText (empty for NULL; at
// least row_count records otherwise), _selectedItemRow, state,
// _menuRowCount, _itemsFirstRow and _equippedItem, plus the cursor byte
// _processEquipSubMenu keeps in a static.
struct EquipmentRowsState {
  std::span<Framework::ItemText> text;
  std::int32_t selected_row = 0;
  std::uint8_t step = 0;
  std::uint8_t row_count = 0;
  std::uint8_t first_row = 0;
  std::uint8_t equipped_item = 0;
  std::uint8_t cursor_animation = 0;
};

// One caller row: the name and description strings (empty for NULL) and the
// row's flags word.
struct EquipmentRowSource {
  std::span<const std::uint8_t> name;
  std::span<const std::uint8_t> description;
  std::int32_t flags = 0;
};

// The pad words _processEquipSubMenu reads.
struct EquipmentChoiceInput {
  std::uint32_t pressed = 0;
  std::uint32_t repeat = 0;
};

class EquipmentRowsHost {
 public:
  virtual ~EquipmentRowsHost() = default;
  // `count` records from the game heap's high end; fewer is a failure.
  [[nodiscard]] virtual std::span<Framework::ItemText> AllocateText(
      std::size_t count) = 0;
  virtual void FreeText(std::span<Framework::ItemText> text) = 0;
  [[nodiscard]] virtual Framework::MenuRow& SetRow(
      const Framework::RowPlacement& placement,
      std::span<const std::uint8_t> text) = 0;
  [[nodiscard]] virtual Framework::MenuRow& Row(int row) = 0;
  virtual void ShowInformation(std::span<const std::uint8_t> message) = 0;
  [[nodiscard]] virtual bool RowsIdle() = 0;
  virtual void PlayLeaveSound() = 0;
  virtual void PlayInvalidSound() = 0;
  virtual void PlayChangeSound() = 0;
  // vs_main_playSfxDefault(0x7E, effect).
  virtual void PlayConfirmSound(std::int32_t effect) = 0;
  [[nodiscard]] virtual std::int32_t DrawCursor(std::uint8_t animation,
                                                std::int32_t position) = 0;
};

// decomp: _populateMenuRows (16C.c:39). False (nothing changed) for no rows,
// more than kEquipmentRowCapacity or a string shorter than its copy; false
// with an empty text after a short allocation.
[[nodiscard]] bool CreateEquipmentRows(
    EquipmentRowsState& state, EquipmentRowsHost& host,
    std::span<const EquipmentRowSource> rows);
// decomp: _getSelectedItemRow (16C.c:101). -1 while the list is open.
[[nodiscard]] std::int32_t SelectedEquipmentRow(
    const EquipmentRowsState& state);
// decomp: _processEquipSubMenu (16C.c:111). False (nothing changed) for a
// text, row or scroll position the list does not hold.
[[nodiscard]] bool UpdateEquipmentChoices(EquipmentRowsState& state,
                                          EquipmentRowsHost& host,
                                          const EquipmentChoiceInput& input,
                                          std::int32_t has_sfx);

// vs_battle_uiAccessory's stats (BATTLE.PRG 146C.h:424).
inline constexpr std::size_t kAccessoryTypes = 4;
inline constexpr std::size_t kAccessoryClasses = 8;
struct AccessoryPreview {
  std::int16_t strength = 0;
  std::int16_t intelligence = 0;
  std::int16_t agility = 0;
  std::array<std::uint16_t, kAccessoryTypes> types{};
  std::array<std::uint16_t, kAccessoryClasses> classes{};
  std::array<std::uint16_t, kAccessoryClasses> affinities{};
};

// vs_mainMenu_equipmentStats' class, affinity and type columns.
inline constexpr std::size_t kAccessoryStatColumn = 16;
struct AccessoryStatColumns {
  std::array<std::int16_t, kAccessoryStatColumn> classes{};
  std::array<std::int16_t, kAccessoryStatColumn> affinities{};
  std::array<std::int16_t, kAccessoryStatColumn> types{};
};

// _populateUiAccessory's status panel subtype (16C.c:333) and stat pages.
inline constexpr std::uint8_t kAccessorySubtype = 32;
inline constexpr std::int32_t kAccessoryStatPages = 7;
inline constexpr std::int32_t kAccessoryPrimaryStatsKnown = 2;

class AccessoryPreviewHost {
 public:
  virtual ~AccessoryPreviewHost() = default;
  // Whether the 0-based armor slot lies inside the bound armor table.
  [[nodiscard]] virtual bool HasAccessory(std::int32_t slot) = 0;
  virtual void ResetStats() = 0;
  // vs_battle_applyAccessory over a slot HasAccessory accepted.
  [[nodiscard]] virtual AccessoryPreview LoadAccessory(std::int32_t slot) = 0;
  virtual void StoreStatColumns(const AccessoryStatColumns& columns) = 0;
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
  virtual void SetPrimaryStats(std::int32_t strength, std::int32_t intelligence,
                               std::int32_t agility, std::int32_t known) = 0;
  virtual void SetStatusItem(std::uint8_t subtype, std::uint8_t item) = 0;
  virtual void SetStatPages(std::int32_t pages) = 0;
};

// decomp: _populateUiAccessory (16C.c:311). `accessory` is the 1-based armor
// slot or 0; false (nothing changed) for a slot outside the armor table.
[[nodiscard]] bool SetAccessoryPreviewStats(AccessoryPreviewHost& host,
                                            std::int32_t accessory);

// _loadObjectData's arguments (68C4.c:12): category 0 equips a shield.
struct EquipmentModelChoice {
  std::int32_t category = 0;
  std::int32_t model_id = 0;
  std::int32_t material = 0;
};

enum class ModelReload : std::uint8_t { kDone, kLoading, kRefused };

class EquipmentModelHost {
 public:
  virtual ~EquipmentModelHost() = default;
  [[nodiscard]] virtual bool HasPreviewActor() = 0;
  [[nodiscard]] virtual std::uint8_t PreviewSequenceModel() = 0;
  [[nodiscard]] virtual std::uint8_t PreviewAnimation() = 0;
  [[nodiscard]] virtual bool HasWeaponModel(int slot) = 0;
  // Battle_characterState->equipmentModels[index]; nullopt without one.
  [[nodiscard]] virtual std::optional<Content::AssetAddress> EquipmentModelSlot(
      int index) = 0;
  virtual void ReleaseWeaponModel(int slot) = 0;
  virtual void QueueWeaponAnimations(std::int32_t sequence_model) = 0;
  virtual void QueueAsset(const Content::ActorAssetRequest& request) = 0;
  virtual void SetWeaponDrawn(bool drawn) = 0;
  [[nodiscard]] virtual bool AssetQueueBusy() = 0;
  virtual void ResetEquipmentPose() = 0;
  virtual void ReleasePreview() = 0;
  virtual void ClonePreview(int source_slot) = 0;
  virtual void ResetPreviewPose() = 0;
  virtual void SheathePreviewWeapon() = 0;
  virtual void PlayPreviewAnimation(int animation) = 0;
  virtual void AdvancePreviewSkeleton() = 0;
  virtual void CopyPreviewPoseToBlend() = 0;
  virtual void ClearWeaponModelFlag(int slot) = 0;
};

// The preview's idle animation once the reload settles (68C4.c:79).
inline constexpr std::uint8_t kPreviewSettledAnimation = 101;

// decomp: _loadObjectData (68C4.c:12). `step` is vs_battle_loadObjectDataState.
// Refused without the preview actor or Ashley's model slots.
[[nodiscard]] ModelReload ReloadEquipmentModels(
    std::uint8_t& step, EquipmentModelHost& host,
    const EquipmentModelChoice& choice);
// decomp: func_80109320 (68C4.c:87).
[[nodiscard]] bool IsTwoHandedWeapon(std::int32_t category);
// decomp: _populateDataSlot (68C4.c:100). False without the model slot.
[[nodiscard]] bool QueueEquipmentModel(EquipmentModelHost& host,
                                       std::int32_t model_id,
                                       std::int32_t material);
}  // namespace VS::Menu::Inventory
