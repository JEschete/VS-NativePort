// Hand-maintained (tools/port/owned.csv).
#include "Menu/Setup/SetupLive.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iterator>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "Audio/SfxLive.hpp"
#include "Content/ActorAssetsLive.hpp"
#include "Content/CdDriveLive.hpp"
#include "Core/Log.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Field/RoomLoadingLive.hpp"
#include "Items/EquipmentLive.hpp"
#include "Items/InventoryLive.hpp"
#include "Items/ItemRecordsLive.hpp"
#include "Menu/Framework/ActorPreviewLive.hpp"
#include "Menu/Framework/EquipmentDetailsLive.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Framework/ItemRows.hpp"
#include "Menu/Framework/ItemRowsLiveHost.hpp"
#include "Menu/Setup/Combination.hpp"  // NOLINT(build/include): typed API
#include "Menu/Setup/SetupScreen.hpp"  // NOLINT(build/include): typed API
#include "Menu/Sounds.hpp"
#include "Render/EncodedText.hpp"
#include "Render/EncodedTextLive.hpp"
#include "Render/GameFontLive.hpp"
#include "Render/UiPacketsLive.hpp"

extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/Movement.h"
#include "Audio/Seq/Channel.h"
#include "Battle/TargetingState.h"
#include "Content/DiscLayout.h"
#include "Content/StringTables.h"
#include "DiscQueue.h"
#include "Field/RoomLoadingLiveState.h"
#include "Input/ControllerLiveState.h"
#include "Items/Inventory.h"
#include "Legacy/Psx.h"
#include "Menu/Framework/EquipmentDetailsLiveState.h"
#include "Menu/Framework/Framework.h"
#include "Menu/Framework/FrameworkLiveState.h"
#include "Menu/Framework/ItemRowsLiveState.h"
#include "Menu/Framework/TextBoxes.h"
#include "Menu/Inventory/Inventory.h"
#include "Menu/TextBox.h"
}

namespace VS::Menu::Setup {
namespace {
// MENU12.BIN's texts and offset-table entries the typed menu names.
static_assert(menu12::kSetup == std::size_t{Text_Menu_MENU12_BIN_Offset_setup});
static_assert(menu12::kAttachGems ==
              std::size_t{Text_Menu_MENU12_BIN_Offset_attachGems});
static_assert(menu12::kDisassemble ==
              std::size_t{Text_Menu_MENU12_BIN_Offset_disassemble});
static_assert(menu12::kRename ==
              std::size_t{Text_Menu_MENU12_BIN_Offset_rename});
static_assert(menu12::kCombine ==
              std::size_t{Text_Menu_MENU12_BIN_Offset_combine});
static_assert(menu12::kAssemble ==
              std::size_t{Text_Menu_MENU12_BIN_Offset_assemble});
static_assert(menu12::kInvalidGrip ==
              std::size_t{Text_Menu_MENU12_BIN_Offset_invalidGrip});
static_assert(menu12::kInvalidBlade ==
              std::size_t{Text_Menu_MENU12_BIN_Offset_invalidBlade});
static_assert(menu12::kInvalidBladeMaterial ==
              std::size_t{Text_Menu_MENU12_BIN_Offset_invalidBladeMaterial});
static_assert(menu12::kInvalidShieldMaterial ==
              std::size_t{Text_Menu_MENU12_BIN_Offset_invalidShieldMaterial});
static_assert(menu12::kInvalidArmorMaterial ==
              std::size_t{Text_Menu_MENU12_BIN_Offset_invalidArmorMaterial});
static_assert(menu12::kChooseWeaponRename ==
              std::size_t{Text_Menu_MENU12_BIN_Offset_chooseWeaponRename});
static_assert(menu12::kRenamePrompt ==
              std::size_t{Text_Menu_MENU12_BIN_Offset_renamePrompt});
static_assert(menu12::kNoGems ==
              std::size_t{Text_Menu_MENU12_BIN_Offset_noGems});
static_assert(menu12::kChooseWeaponDisassemble ==
              std::size_t{Text_Menu_MENU12_BIN_Offset_chooseWeaponDisassemble});
static_assert(menu12::kChooseShieldDisassemble ==
              std::size_t{Text_Menu_MENU12_BIN_Offset_chooseShieldDisassemble});
static_assert(menu12::kRepairConfirm ==
              std::size_t{Text_Menu_MENU12_BIN_Offset_repairConfirm});
static_assert(menu12::kRepairComplete ==
              std::size_t{Text_Menu_MENU12_BIN_Offset_repairComplete});
static_assert(menu12::kNotInWorkshop ==
              std::size_t{Text_Menu_MENU12_BIN_Offset_notInWorkshop});
static_assert(menu12::kWeaponsFull ==
              std::size_t{Text_Menu_MENU12_BIN_Offset_weaponsFull});
static_assert(menu12::kNoWeapons ==
              std::size_t{Text_Menu_MENU12_BIN_Offset_noWeapons});
static_assert(menu12::kNoItems ==
              std::size_t{Text_Menu_MENU12_BIN_Offset_noItems});
static_assert(menu12::kSelectGem ==
              std::size_t{Text_Menu_MENU12_BIN_Offset_selectGem});
static_assert(menu12::kPressOk ==
              std::size_t{Text_Menu_MENU12_BIN_Offset_pressOk});
static_assert(menu12::kWeapon ==
              std::size_t{Text_Menu_MENU12_BIN_Offset_weapon});
static_assert(menu12::kBlades ==
              std::size_t{Text_Menu_MENU12_BIN_Offset_blades});
static_assert(menu12::kBladesDesc ==
              std::size_t{Text_Menu_MENU12_BIN_Offset_bladesDesc});
static_assert(menu12::kGrips == std::size_t{Text_Menu_MENU12_BIN_Offset_grips});
static_assert(menu12::kGripsDesc ==
              std::size_t{Text_Menu_MENU12_BIN_Offset_gripsDesc});
static_assert(menu12::kGems == std::size_t{Text_Menu_MENU12_BIN_Offset_gems});
static_assert(menu12::kGemsDesc ==
              std::size_t{Text_Menu_MENU12_BIN_Offset_gemsDesc});
static_assert(menu12::kAttachmentConfirm ==
              std::size_t{Text_Menu_MENU12_BIN_Offset_attachmentConfirm});
static_assert(menu12::kBlade == std::size_t{Text_Menu_MENU12_BIN_Offset_blade});
static_assert(menu12::kShield ==
              std::size_t{Text_Menu_MENU12_BIN_Offset_shield});
static_assert(menu12::kArmor == std::size_t{Text_Menu_MENU12_BIN_Offset_armor});
static_assert(menu12::kSelectBladeToCombine ==
              std::size_t{Text_Menu_MENU12_BIN_Offset_selectBladeToCombine});
static_assert(menu12::kSelectShield ==
              std::size_t{Text_Menu_MENU12_BIN_Offset_selectShield});
static_assert(menu12::kSelectArmor ==
              std::size_t{Text_Menu_MENU12_BIN_Offset_selectArmor});
static_assert(menu12::kCombineBlade ==
              std::size_t{Text_Menu_MENU12_BIN_Offset_combineBlade});
static_assert(menu12::kCombineShield ==
              std::size_t{Text_Menu_MENU12_BIN_Offset_combineShield});
static_assert(menu12::kCombineArmor ==
              std::size_t{Text_Menu_MENU12_BIN_Offset_combineArmor});
static_assert(menu12::kCancelCombine ==
              std::size_t{Text_Menu_MENU12_BIN_Offset_cancelCombine});
static_assert(menu12::kCancelCombineConfirm ==
              std::size_t{Text_Menu_MENU12_BIN_Offset_cancelCombineConfirm});
static_assert(menu12::kAssembleWeaponsIndex ==
              Text_Menu_MENU12_BIN_Index_assembleWeapons);
static_assert(menu12::kAssembleWeaponsDescIndex ==
              Text_Menu_MENU12_BIN_Index_assembleWeaponsDesc);
static_assert(menu12::kAssembleIndex == Text_Menu_MENU12_BIN_Index_assemble);
static_assert(menu12::kWeaponsIndex == Text_Menu_MENU12_BIN_Index_weapons);
static_assert(menu12::kAttachWeaponIndex ==
              Text_Menu_MENU12_BIN_Index_attachWeapon);
static_assert(menu12::kOptionYesIndex == Text_Menu_MENU12_BIN_Index_optionYes);
static_assert(menu12::kDisassembleWeaponDescIndex ==
              Text_Menu_MENU12_BIN_Index_disassembleWeaponDesc);
static_assert(menu12::kDisassembleWeaponConfirmIndex ==
              Text_Menu_MENU12_BIN_Index_disassembleWeaponConfirm);
static_assert(menu12::kDisassembleWeaponIndex ==
              Text_Menu_MENU12_BIN_Index_disassembleWeapon);
static_assert(menu12::kNoItemsIndex == Text_Menu_MENU12_BIN_Index_noItems);
static_assert(menu12::kSelectBladeToAssembleIndex ==
              Text_Menu_MENU12_BIN_Index_selectBladeToAssemble);
static_assert(menu12::kSelectGemIndex == Text_Menu_MENU12_BIN_Index_selectGem);
static_assert(menu12::kPressOkIndex == Text_Menu_MENU12_BIN_Index_pressOk);
static_assert(menu12::kWeaponGemSlotsIndex ==
              Text_Menu_MENU12_BIN_Index_weaponGemSlots);
static_assert(menu12::kWeaponNoGemSlotsIndex ==
              Text_Menu_MENU12_BIN_Index_weaponNoGemSlots);
static_assert(menu12::kBladeIndex == Text_Menu_MENU12_BIN_Index_blade);
static_assert(menu12::kCombineBladesIndex ==
              Text_Menu_MENU12_BIN_Index_combineBlades);
static_assert(menu12::kNewBladeIndex == Text_Menu_MENU12_BIN_Index_newBlade);
static_assert(Framework::kMenu12Bytes == VS_MENU12_BIN_SIZE);

// _loadSyd's files (168.c:2658): BLADE.SYD, SHIELD.SYD and ARMOR.SYD.
struct RecipeFile {
  int lba;
  int size;
};
constexpr std::array<RecipeFile, kRecipeKindCount> kRecipeFiles{{
    {.lba = VS_BLADE_SYD_LBA, .size = VS_BLADE_SYD_SIZE},
    {.lba = VS_SHIELD_SYD_LBA, .size = VS_SHIELD_SYD_SIZE},
    {.lba = VS_ARMOR_SYD_LBA, .size = VS_ARMOR_SYD_SIZE},
}};
// Audio_Seq_unusedMaterialModifiers' strength, intelligence and agility
// bytes of a material's record.
constexpr std::size_t kStrengthModifier = 24;
constexpr std::size_t kIntelligenceModifier = 26;
constexpr std::size_t kAgilityModifier = 28;
static_assert(std::size(Audio_Seq_unusedMaterialModifiers) == kMaterialCount);
static_assert(std::size(Audio_Seq_unusedMaterialModifiers[0]) >
              kAgilityModifier);
// The information text box _repairMenu and the prompts close.
constexpr std::size_t kInformationTextBox = 7;
// _drawOk's "OK" goes 0xC bytes (three entries) before the scene table.
constexpr int kConfirmationOrderingSlot = -3;
// The action restriction the workshop lifts when it returns.
constexpr int kWorkshopRestriction = 0x200;

State g_state{};

// _sydData's block, its read and the file it decoded into.
struct RecipeLoad {
  void* data = nullptr;
  std::size_t size = 0;
  RecipeKind kind = RecipeKind::kBlade;
  Content_DiscQueueSlot* request = nullptr;
  std::optional<RecipeBook> book;
};
RecipeLoad g_recipes{};

void FreeRecipeBlock() {
  if (g_recipes.data != nullptr) {
    Core::Memory::FreeLiveFromHighEnd(g_recipes.data);
  }
  g_recipes.data = nullptr;
  g_recipes.size = 0;
  g_recipes.book.reset();
}

// A row text the framework copies while it builds the rows: the row
// builders trust kTrustedEncodedTextBytes from each pointer, so each text is
// a terminator-padded copy that long.
using TrustedText = std::array<char, Render::kTrustedEncodedTextBytes>;
void CopyTrusted(TrustedText& copy, std::span<const std::uint8_t> text) {
  copy.fill(static_cast<char>(Render::kTextTerminator));
  const std::size_t bytes = std::min(text.size(), copy.size() - 1);
  std::memcpy(copy.data(), text.data(), bytes);
}

class LiveHost final : public Host {
 public:
  LiveHost() { Reload(); }

  // Writes the inventory and its display order back.
  void Commit() {
    Items::Live::Commit(Audio_Seq_unusedInventory, inventory_);
    Items::Live::Commit(Audio_Seq_unusedInventoryIndices, indices_);
  }

  void Refuse(std::string_view what) override {
    VS_LOG_WARN("menu.setup", "{}", what);
  }
  Framework::Buttons Input() override {
    return {.pressed = Input_buttonsPressed.allButtons,
            .repeat = static_cast<std::uint32_t>(Input_buttonRepeat),
            .held = static_cast<std::uint32_t>(Input_buttonsState)};
  }
  void PlaySound(MenuSound sound) override { PlayMenuSound(sound); }
  void PlaySoundEffect(int effect) override {
    Audio::LivePlaySfxCentered(kMenuSoundChannelGroup, effect);
  }

  Framework::MenuRow& MenuItem(int row) override {
    return Framework::LiveMenuRow(row);
  }
  Framework::MenuRow& SetMenuItem(const Framework::RowPlacement& placement,
                                  std::span<const std::uint8_t> text) override {
    return Framework::SetLiveMenuItemText(placement, text);
  }
  bool RowsIdle() override { return Framework::AreLiveMenuRowsIdle() != 0; }
  void ClearMenuExcept(int row) override {
    Framework::ClearLiveMenuExcept(row);
  }
  void FlyOutRowLeft(int row) override { Framework::FlyOutLiveRowLeft(row); }
  void FlyOutRowRight(int row) override { Framework::FlyOutLiveRowRight(row); }
  void FlyOutRowsAndHoistSelection(int row, int offset) override {
    Framework::FlyOutLiveRowsAndHoistSelection(row, offset);
  }
  void DeactivateRow(int row) override { Framework::DeactivateLiveRow(row); }
  void CreateMenuRows(int row_info, std::span<const Framework::RowText> texts,
                      std::span<const std::int32_t> row_types) override {
    if (row_types.size() != texts.size()) {
      Refuse("workshop rows without one type a row");
      return;
    }
    std::vector<TrustedText> copies(texts.size() * 2);
    std::vector<char*> pointers(copies.size(), nullptr);
    for (std::size_t row = 0; row < texts.size(); ++row) {
      const std::array<std::span<const std::uint8_t>, 2> parts{
          texts[row].name, texts[row].description};
      for (std::size_t part = 0; part < parts.size(); ++part) {
        const std::size_t index = (row * 2) + part;
        if (parts[part].empty()) {
          continue;
        }
        CopyTrusted(copies[index], parts[part]);
        pointers[index] = copies[index].data();
      }
    }
    std::vector<int> types(row_types.begin(), row_types.end());
    Framework::CreateLiveMenuRows(static_cast<int>(texts.size()), row_info,
                                  pointers.data(), types.data());
  }
  int SelectedRow() override { return Framework::LiveSelectedRow(); }
  int HighlightedRow() override { return Framework::LiveHighlightedRow(); }
  int DrawCursor(int step, std::uint32_t position) override {
    return Framework::DrawLiveCursor(static_cast<u_int>(step),
                                     static_cast<int>(position));
  }
  int DrawMainMenuCursor(int step, int position) override {
    return Framework::DrawLiveMainMenuCursor(step, position);
  }
  void DrawConfirmationText(std::uint32_t position,
                            std::uint32_t color) override {
    u_long* const chain = Render::LiveFrameOrderingEntry(
        Render::LiveOrderingBase::kScene, kConfirmationOrderingSlot);
    if (chain == nullptr) {
      Refuse("workshop OK label outside the frame's ordering table");
      return;
    }
    Render::DrawLiveCompactTextColor("OK", static_cast<int>(position),
                                     static_cast<int>(color), chain);
  }
  void SetMenuCommandLabel() override {
    Framework::SetLiveCommandLabel(Menu_Framework_MenuCommand_Menu);
  }
  void SetBackgroundPriority(int ot_offset, int brightness) override {
    Framework::SetLiveBackgroundPriority(ot_offset, brightness);
  }
  void SetCursorColor(int color) override {
    Menu_Framework_cursorColor = static_cast<char>(color);
  }
  int ItemsListRow() override { return Menu_Framework_itemsListRow; }
  std::span<std::int32_t> RowTypes() override {
    if (Menu_Framework_rowTypeBuf == nullptr) {
      return {};
    }
    return {Menu_Framework_rowTypeBuf, Framework::kRowTypeCount};
  }
  std::span<std::uint8_t> StringBuffer() override {
    if (Menu_Framework_stringBuf == nullptr) {
      return {};
    }
    return {reinterpret_cast<std::uint8_t*>(Menu_Framework_stringBuf),
            Framework::kStringBufferBytes};
  }
  int CursorMemory() override { return Input_settings.cursorMemory; }
  void SetCursorMemory(int value) override {
    Input_settings.cursorMemory = static_cast<char>(value);
  }

  void InitInformationTextBox(int header) override {
    Menu_InitInformationTextBox(
        static_cast<enum Menu_InformationBoxHeader>(header));
  }
  void InitializeInformationBox() override {
    Framework::InitializeLiveInformationBox();
  }
  void DismissInformationBox() override {
    Framework::DismissLiveInformationBox();
  }
  void CloseInformationTextBox() override {
    Menu_textBoxes[kInformationTextBox].state = 0;
  }
  void SetInformationMessage(std::span<const std::uint8_t> text) override {
    TrustedText copy{};
    CopyTrusted(copy, text);
    Framework::SetLiveInformationMessage(copy.data());
  }
  void UpdateInformationVisibility(int row, int state) override {
    Framework::UpdateLiveInformationVisibility(row, state);
  }

  void ReturnFromSetup(int return_state) override {
    Menu_Framework_menuState.currentMenu = Menu_Framework_menuState.returnState;
    Menu_Framework_menuState.returnState = static_cast<char>(return_state);
    Menu_Framework_actionRestrictions &= ~kWorkshopRestriction;
  }
  void OpenMenu(int menu) override {
    Menu_Framework_menuState.currentMenu = static_cast<char>(menu);
  }

  void ResetEquipmentStats() override { Framework::ResetLiveEquipmentStats(); }
  void SetWeaponStats(int weapon) override {
    Commit();
    Framework::SetLiveWeaponStats(weapon);
  }
  void SetBladeStats(int blade) override {
    Commit();
    Framework::SetLiveBladeStats(blade);
  }
  void SetGripStats(int grip) override {
    Commit();
    Framework::SetLiveGripStats(grip);
  }
  void SetShieldStats(int shield) override {
    Commit();
    Framework::SetLiveShieldStats(shield);
  }
  void SetArmorStats(int armor) override {
    Commit();
    Framework::SetLiveArmorStats(armor);
  }
  void SetGemStats(int gem) override {
    Commit();
    Framework::SetLiveGemStats(gem);
  }
  void SetEnabledStatPages(int pages) override {
    Framework::SetLiveEnabledStatPages(pages);
  }
  void UpdateEquipmentStatPages(int command) override {
    Framework::UpdateLiveEquipmentStatPages(command);
  }
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
  void SetRangeAndRisk(int range, int risk, int unused, int shown) override {
    Framework::SetLiveRangeAndRisk(range, risk, unused, shown);
  }
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
  void SetPrimaryStats(int strength, int intelligence, int agility,
                       int shown) override {
    Framework::SetLivePrimaryStats(strength, intelligence, agility, shown);
  }
  void UpdatePrimaryStatPanel(int command) override {
    Framework::UpdateLivePrimaryStatPanel(command);
  }
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
  void SetDpPp(int current_dp, int max_dp, int current_pp,
               int max_pp) override {
    Framework::SetLiveDpPp(current_dp, max_dp, current_pp, max_pp);
  }
  void UpdateDpPpBars(int flags) override {
    Framework::UpdateLiveDpPpBars(flags);
  }
  void SetEquipmentSubtype(int subtype) override {
    Menu_Framework_equipmentSubtype = static_cast<char>(subtype);
  }
  void SetEquipmentStat(std::size_t index, int value) override {
    if (index >= std::size(Menu_Framework_equipmentStats)) {
      Refuse("workshop equipment stat outside the details table");
      return;
    }
    Menu_Framework_equipmentStats[index] = static_cast<std::int16_t>(value);
  }
  void SetPrimaryStatValue(std::size_t index, int value) override {
    if (index >= std::size(Menu_Framework_strIntAgi)) {
      Refuse("workshop primary stat outside the details table");
      return;
    }
    Menu_Framework_strIntAgi[index] = static_cast<std::int16_t>(value);
  }

  std::span<const std::uint8_t> Menu12() override {
    if (Menu_Framework_menu12Text == nullptr) {
      return {};
    }
    return {reinterpret_cast<const std::uint8_t*>(Menu_Framework_menu12Text),
            Framework::kMenu12Bytes};
  }
  Framework::ItemRowsHost& ItemRows() override {
    Commit();
    return rows_;
  }
  void SetTextInteger(std::size_t slot, int value) override {
    if (slot >= std::size(Battle_stringContext.integers)) {
      Refuse("workshop text integer outside the string context");
      return;
    }
    Battle_stringContext.integers[slot] = value;
  }

  Items::InventoryContents& Inventory() override { return inventory_; }
  Items::InventoryIndices& Indices() override { return indices_; }
  void UnequipWeapon() override {
    Commit();
    Items::Inventory::UnequipLiveWeapon();
    Reload();
  }
  void UnequipShield() override {
    Commit();
    Items::Inventory::UnequipLiveShield();
    Reload();
  }
  void EquipWeapon(std::size_t slot) override {
    if (slot >= std::size(Audio_Seq_unusedInventory.weapons)) {
      Refuse("equipped weapon outside the inventory");
      return;
    }
    Commit();
    Items::EquipLiveAshleyWeapon(&Audio_Seq_unusedInventory.weapons[slot]);
    Reload();
  }
  void EquipShield(std::size_t slot) override {
    if (slot >= std::size(Audio_Seq_unusedInventory.shields)) {
      Refuse("equipped shield outside the inventory");
      return;
    }
    Commit();
    Items::EquipLiveAshleyShield(&Audio_Seq_unusedInventory.shields[slot]);
    Reload();
  }
  void EquipArmor(int body_part, std::optional<std::size_t> slot) override {
    if (slot && *slot >= std::size(Audio_Seq_unusedInventory.armor)) {
      Refuse("equipped armor outside the inventory");
      return;
    }
    Commit();
    Items::EquipLiveAshleyArmor(
        body_part, slot ? &Audio_Seq_unusedInventory.armor[*slot] : nullptr);
    Reload();
  }
  MaterialBonuses Materials() override {
    MaterialBonuses bonuses{};
    for (std::size_t material = 0; material < bonuses.size(); ++material) {
      const auto& modifiers = Audio_Seq_unusedMaterialModifiers[material];
      bonuses[material] = {
          .strength = static_cast<std::int8_t>(modifiers[kStrengthModifier]),
          .intelligence =
              static_cast<std::int8_t>(modifiers[kIntelligenceModifier]),
          .agility = static_cast<std::int8_t>(modifiers[kAgilityModifier])};
    }
    return bonuses;
  }
  int CurrentWorkshop() override {
    return VS::Field::LiveIsCurrentRoomWorkshop();
  }

  void StartRecipeLoad(RecipeKind kind) override;
  std::optional<bool> RecipeLoaded() override {
    if (g_recipes.request == nullptr) {
      return std::nullopt;
    }
    return g_recipes.request->state == Content_DiscRequestState_Loaded;
  }
  void ReleaseRecipeRequest() override;
  const RecipeBook* Recipes() override {
    return g_recipes.book ? &*g_recipes.book : nullptr;
  }
  void FreeRecipes() override { FreeRecipeBlock(); }

  std::optional<int> AshleySequenceModel() override {
    const Actors_ActorRecord* const ashley = Actors_actorRecords[0];
    if (ashley == nullptr) {
      return std::nullopt;
    }
    return ashley->sequenceModelId;
  }
  void ReleaseWeaponModel(int slot) override {
    Content::ReleaseLiveWeaponModel(slot);
  }
  void QueueWeaponAnimations(int sequence_model) override {
    Commit();
    Framework::QueueLiveAshleyWeaponAnimations(sequence_model);
  }
  bool ActorAssetsPending() override {
    return Content::UpdateLiveActorAssetQueue() != 0;
  }
  void ResetEquipmentPose() override {
    Commit();
    Framework::ResetLiveAshleyEquipmentPose();
  }
  int EquipmentModelLoadStep() override {
    return Menu_Inventory_equipmentModelLoadStep;
  }
  void SetEquipmentModelLoadStep(int step) override {
    Menu_Inventory_equipmentModelLoadStep = static_cast<char>(step);
  }

 private:
  void Reload() {
    inventory_ = Items::Live::Typed(Audio_Seq_unusedInventory);
    indices_ = Items::Live::Typed(Audio_Seq_unusedInventoryIndices);
  }

  Items::InventoryContents inventory_{};
  Items::InventoryIndices indices_{};
  Framework::LiveItemRowsHost rows_;
};

// decomp: MENUC.PRG _loadSyd (src/MENU/MENUC.PRG/168.c:2658), its request.
void LiveHost::StartRecipeLoad(RecipeKind kind) {
  const auto index = static_cast<std::size_t>(kind);
  if (index >= kRecipeFiles.size()) {
    Refuse("recipe file outside BLADE, SHIELD and ARMOR.SYD");
    return;
  }
  if (g_recipes.request != nullptr) {
    // The original lost the read in flight and its block; the block stays
    // until that read finishes.
    Refuse("recipe file read while the previous read was in flight");
    return;
  }
  if (g_recipes.data != nullptr) {
    // The original replaced _sydData and lost the earlier block.
    Refuse("recipe file read while the previous block was held; freed it");
    FreeRecipeBlock();
  }
  const RecipeFile& file = kRecipeFiles[index];
  const auto size = static_cast<std::size_t>(file.size);
  void* const data = Core::Memory::AllocateLiveFromHighEnd(size);
  if (data == nullptr) {
    Refuse("recipe file does not fit the game heap");
    return;
  }
  g_recipes.data = data;
  g_recipes.size = size;
  g_recipes.kind = kind;
  Content_DiscFile request{};
  request.lba = file.lba;
  request.size = file.size;
  g_recipes.request = Content::AllocateLiveDiscRequest(&request);
  if (g_recipes.request != nullptr) {
    Content::EnqueueLiveDiscRequest(g_recipes.request, data);
  } else {
    g_recipes.request = Content::CompleteLiveRefusedDiscRequest(&request, data);
  }
}

void LiveHost::ReleaseRecipeRequest() {
  Content::ReleaseLiveDiscRequest(g_recipes.request);
  g_recipes.request = nullptr;
  if (g_recipes.data == nullptr) {
    Refuse("recipe file read without its block");
    return;
  }
  const std::span<const std::uint8_t> bytes(
      static_cast<const std::uint8_t*>(g_recipes.data), g_recipes.size);
  auto decoded = DecodeRecipeFile(g_recipes.kind, bytes);
  if (!decoded) {
    Refuse(decoded.error());
    g_recipes.book.reset();
    return;
  }
  g_recipes.book = std::move(*decoded);
}

template <typename Call>
int RunWithLiveHost(const Call& call) {
  LiveHost host;
  Context ctx{.state = g_state, .host = host};
  const int result = call(ctx);
  host.Commit();
  return result;
}
}  // namespace

// decomp: MENUC.PRG vs_menuC_exec (src/MENU/MENUC.PRG/168.c:4550)
int UpdateLiveSetup(std::uint8_t& step) {
  return RunWithLiveHost([&step](Context& ctx) { return Update(ctx, step); });
}

int RunLiveSetup(const std::function<int(Context&)>& call) {
  return RunWithLiveHost(call);
}

State& LiveSetupState() { return g_state; }

const RecipeBook* LiveSetupRecipes() {
  return g_recipes.book ? &*g_recipes.book : nullptr;
}

void ResetLiveSetupStatics() {
  ResetModuleStatics(g_state);
  if (g_recipes.request != nullptr) {
    VS_LOG_WARN("menu.setup",
                "workshop reset while its recipe read was in flight; the "
                "block stays until the read finishes");
    return;
  }
  // The reload dropped _sydData without freeing its block; the native port
  // frees it so repeated reloads do not exhaust the high end.
  FreeRecipeBlock();
}
}  // namespace VS::Menu::Setup
