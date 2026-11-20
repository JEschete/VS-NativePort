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

#include "Actors/ActorRecord.hpp"
#include "Audio/SfxLive.hpp"
#include "Battle/TargetingState.hpp"
#include "Content/ActorAssetsLive.hpp"
#include "Content/CdDriveLive.hpp"
#include "Content/DiscLayout.h"
#include "Content/DiscQueue.hpp"
#include "Content/StringTables.hpp"
#include "Core/Log.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Field/RoomLoadingLive.hpp"
#include "Field/RoomLoadingLiveState.hpp"
#include "Input/ControllerLiveState.hpp"
#include "Items/EquipmentLive.hpp"
#include "Items/InventoryLive.hpp"
#include "Items/ItemRecordsLive.hpp"
#include "Items/Materials.hpp"
#include "Legacy/Psx.hpp"
#include "Menu/Framework/ActorPreviewLive.hpp"
#include "Menu/Framework/EquipmentDetailsLive.hpp"
#include "Menu/Framework/EquipmentDetailsLiveState.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Framework/FrameworkLiveState.hpp"
#include "Menu/Framework/ItemRows.hpp"
#include "Menu/Framework/ItemRowsLiveHost.hpp"
#include "Menu/Framework/ItemRowsLiveState.hpp"
#include "Menu/Framework/TextBoxes.hpp"
#include "Menu/Setup/Combination.hpp"  // NOLINT(build/include): typed API
#include "Menu/Setup/SetupScreen.hpp"  // NOLINT(build/include): typed API
#include "Menu/Sounds.hpp"
#include "Menu/TextBox.hpp"
#include "Menu/TextBoxStateLive.hpp"
#include "Render/EncodedText.hpp"
#include "Render/EncodedTextLive.hpp"
#include "Render/GameFontLive.hpp"
#include "Render/UiPacketsLive.hpp"

namespace menu12_text = VS::Content::text::menu12_bin;

namespace VS::Menu::Setup {
namespace {
// MENU12.BIN's texts and offset-table entries the typed menu names.
static_assert(menu12::kSetup == std::size_t{menu12_text::offset::kSetup});
static_assert(menu12::kAttachGems ==
              std::size_t{menu12_text::offset::kAttachGems});
static_assert(menu12::kDisassemble ==
              std::size_t{menu12_text::offset::kDisassemble});
static_assert(menu12::kRename == std::size_t{menu12_text::offset::kRename});
static_assert(menu12::kCombine == std::size_t{menu12_text::offset::kCombine});
static_assert(menu12::kAssemble == std::size_t{menu12_text::offset::kAssemble});
static_assert(menu12::kInvalidGrip ==
              std::size_t{menu12_text::offset::kInvalidGrip});
static_assert(menu12::kInvalidBlade ==
              std::size_t{menu12_text::offset::kInvalidBlade});
static_assert(menu12::kInvalidBladeMaterial ==
              std::size_t{menu12_text::offset::kInvalidBladeMaterial});
static_assert(menu12::kInvalidShieldMaterial ==
              std::size_t{menu12_text::offset::kInvalidShieldMaterial});
static_assert(menu12::kInvalidArmorMaterial ==
              std::size_t{menu12_text::offset::kInvalidArmorMaterial});
static_assert(menu12::kChooseWeaponRename ==
              std::size_t{menu12_text::offset::kChooseWeaponRename});
static_assert(menu12::kRenamePrompt ==
              std::size_t{menu12_text::offset::kRenamePrompt});
static_assert(menu12::kNoGems == std::size_t{menu12_text::offset::kNoGems});
static_assert(menu12::kChooseWeaponDisassemble ==
              std::size_t{menu12_text::offset::kChooseWeaponDisassemble});
static_assert(menu12::kChooseShieldDisassemble ==
              std::size_t{menu12_text::offset::kChooseShieldDisassemble});
static_assert(menu12::kRepairConfirm ==
              std::size_t{menu12_text::offset::kRepairConfirm});
static_assert(menu12::kRepairComplete ==
              std::size_t{menu12_text::offset::kRepairComplete});
static_assert(menu12::kNotInWorkshop ==
              std::size_t{menu12_text::offset::kNotInWorkshop});
static_assert(menu12::kWeaponsFull ==
              std::size_t{menu12_text::offset::kWeaponsFull});
static_assert(menu12::kNoWeapons ==
              std::size_t{menu12_text::offset::kNoWeapons});
static_assert(menu12::kNoItems == std::size_t{menu12_text::offset::kNoItems});
static_assert(menu12::kSelectGem ==
              std::size_t{menu12_text::offset::kSelectGem});
static_assert(menu12::kPressOk == std::size_t{menu12_text::offset::kPressOk});
static_assert(menu12::kWeapon == std::size_t{menu12_text::offset::kWeapon});
static_assert(menu12::kBlades == std::size_t{menu12_text::offset::kBlades});
static_assert(menu12::kBladesDesc ==
              std::size_t{menu12_text::offset::kBladesDesc});
static_assert(menu12::kGrips == std::size_t{menu12_text::offset::kGrips});
static_assert(menu12::kGripsDesc ==
              std::size_t{menu12_text::offset::kGripsDesc});
static_assert(menu12::kGems == std::size_t{menu12_text::offset::kGems});
static_assert(menu12::kGemsDesc == std::size_t{menu12_text::offset::kGemsDesc});
static_assert(menu12::kAttachmentConfirm ==
              std::size_t{menu12_text::offset::kAttachmentConfirm});
static_assert(menu12::kBlade == std::size_t{menu12_text::offset::kBlade});
static_assert(menu12::kShield == std::size_t{menu12_text::offset::kShield});
static_assert(menu12::kArmor == std::size_t{menu12_text::offset::kArmor});
static_assert(menu12::kSelectBladeToCombine ==
              std::size_t{menu12_text::offset::kSelectBladeToCombine});
static_assert(menu12::kSelectShield ==
              std::size_t{menu12_text::offset::kSelectShield});
static_assert(menu12::kSelectArmor ==
              std::size_t{menu12_text::offset::kSelectArmor});
static_assert(menu12::kCombineBlade ==
              std::size_t{menu12_text::offset::kCombineBlade});
static_assert(menu12::kCombineShield ==
              std::size_t{menu12_text::offset::kCombineShield});
static_assert(menu12::kCombineArmor ==
              std::size_t{menu12_text::offset::kCombineArmor});
static_assert(menu12::kCancelCombine ==
              std::size_t{menu12_text::offset::kCancelCombine});
static_assert(menu12::kCancelCombineConfirm ==
              std::size_t{menu12_text::offset::kCancelCombineConfirm});
static_assert(menu12::kAssembleWeaponsIndex ==
              menu12_text::index::kAssembleWeapons);
static_assert(menu12::kAssembleWeaponsDescIndex ==
              menu12_text::index::kAssembleWeaponsDesc);
static_assert(menu12::kAssembleIndex == menu12_text::index::kAssemble);
static_assert(menu12::kWeaponsIndex == menu12_text::index::kWeapons);
static_assert(menu12::kAttachWeaponIndex == menu12_text::index::kAttachWeapon);
static_assert(menu12::kOptionYesIndex == menu12_text::index::kOptionYes);
static_assert(menu12::kDisassembleWeaponDescIndex ==
              menu12_text::index::kDisassembleWeaponDesc);
static_assert(menu12::kDisassembleWeaponConfirmIndex ==
              menu12_text::index::kDisassembleWeaponConfirm);
static_assert(menu12::kDisassembleWeaponIndex ==
              menu12_text::index::kDisassembleWeapon);
static_assert(menu12::kNoItemsIndex == menu12_text::index::kNoItems);
static_assert(menu12::kSelectBladeToAssembleIndex ==
              menu12_text::index::kSelectBladeToAssemble);
static_assert(menu12::kSelectGemIndex == menu12_text::index::kSelectGem);
static_assert(menu12::kPressOkIndex == menu12_text::index::kPressOk);
static_assert(menu12::kWeaponGemSlotsIndex ==
              menu12_text::index::kWeaponGemSlots);
static_assert(menu12::kWeaponNoGemSlotsIndex ==
              menu12_text::index::kWeaponNoGemSlots);
static_assert(menu12::kBladeIndex == menu12_text::index::kBlade);
static_assert(menu12::kCombineBladesIndex ==
              menu12_text::index::kCombineBlades);
static_assert(menu12::kNewBladeIndex == menu12_text::index::kNewBlade);
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
// VS::Items::materialModifiers' strength, intelligence and agility
// bytes of a material's record.
constexpr std::size_t kStrengthModifier = 24;
constexpr std::size_t kIntelligenceModifier = 26;
constexpr std::size_t kAgilityModifier = 28;
static_assert(std::size(VS::Items::materialModifiers) == kMaterialCount);
static_assert(std::size(VS::Items::materialModifiers[0]) > kAgilityModifier);
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
  VS::Content::DiscQueueSlot* request = nullptr;
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
    VS::Items::inventory = inventory_;
    VS::Items::inventoryIndices = indices_;
  }

  void Refuse(std::string_view what) override {
    VS::Core::LogWarn("menu.setup", "{}", what);
  }
  Framework::Buttons Input() override { return Framework::LiveButtons(); }
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
    return Framework::DrawLiveCursor(static_cast<std::uint32_t>(step),
                                     static_cast<int>(position));
  }
  int DrawMainMenuCursor(int step, int position) override {
    return Framework::DrawLiveMainMenuCursor(step, position);
  }
  void DrawConfirmationText(std::uint32_t position,
                            std::uint32_t color) override {
    std::uint32_t* const chain = Render::LiveFrameOrderingEntry(
        Render::LiveOrderingBase::kScene, kConfirmationOrderingSlot);
    if (chain == nullptr) {
      Refuse("workshop OK label outside the frame's ordering table");
      return;
    }
    Render::DrawLiveCompactTextColor("OK", static_cast<int>(position),
                                     static_cast<int>(color), chain);
  }
  void SetMenuCommandLabel() override {
    Framework::SetLiveCommandLabel(Framework::MenuCommand::kMenu);
  }
  void SetBackgroundPriority(int ot_offset, int brightness) override {
    Framework::SetLiveBackgroundPriority(ot_offset, brightness);
  }
  void SetCursorColor(int color) override {
    VS::Menu::Framework::cursorColor = static_cast<char>(color);
  }
  int ItemsListRow() override { return VS::Menu::Framework::itemsListRow; }
  std::span<std::int32_t> RowTypes() override {
    if (VS::Menu::Framework::rowTypeBuf == nullptr) {
      return {};
    }
    return {VS::Menu::Framework::rowTypeBuf, Framework::kRowTypeCount};
  }
  std::span<std::uint8_t> StringBuffer() override {
    if (VS::Menu::Framework::stringBuf == nullptr) {
      return {};
    }
    return {reinterpret_cast<std::uint8_t*>(VS::Menu::Framework::stringBuf),
            Framework::kStringBufferBytes};
  }
  int CursorMemory() override { return VS::Input::settings.cursor_memory; }
  void SetCursorMemory(int value) override {
    VS::Input::settings.cursor_memory = static_cast<char>(value);
  }

  void InitInformationTextBox(int header) override {
    VS::Menu::InitLiveInformationTextBox(
        static_cast<InformationHeader>(header));
  }
  void InitializeInformationBox() override {
    Framework::InitializeLiveInformationBox();
  }
  void DismissInformationBox() override {
    Framework::DismissLiveInformationBox();
  }
  void CloseInformationTextBox() override {
    textBoxes[kInformationTextBox].state = 0;
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
    VS::Menu::Framework::menuState.control.current_menu =
        VS::Menu::Framework::menuState.control.return_state;
    VS::Menu::Framework::menuState.control.return_state =
        static_cast<std::uint8_t>(return_state);
    VS::Menu::Framework::actionRestrictions &= ~kWorkshopRestriction;
  }
  void OpenMenu(int menu) override {
    VS::Menu::Framework::menuState.control.current_menu =
        static_cast<std::uint8_t>(menu);
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
    VS::Menu::Framework::equipmentSubtype = static_cast<char>(subtype);
  }
  void SetEquipmentStat(std::size_t index, int value) override {
    if (index >= std::size(VS::Menu::Framework::equipmentStats)) {
      Refuse("workshop equipment stat outside the details table");
      return;
    }
    VS::Menu::Framework::equipmentStats[index] =
        static_cast<std::int16_t>(value);
  }
  void SetPrimaryStatValue(std::size_t index, int value) override {
    if (index >= std::size(VS::Menu::Framework::strIntAgi)) {
      Refuse("workshop primary stat outside the details table");
      return;
    }
    VS::Menu::Framework::strIntAgi[index] = static_cast<std::int16_t>(value);
  }

  std::span<const std::uint8_t> Menu12() override {
    if (VS::Menu::Framework::menu12Text == nullptr) {
      return {};
    }
    return {
        reinterpret_cast<const std::uint8_t*>(VS::Menu::Framework::menu12Text),
        Framework::kMenu12Bytes};
  }
  Framework::ItemRowsHost& ItemRows() override {
    Commit();
    return rows_;
  }
  void SetTextInteger(std::size_t slot, int value) override {
    if (slot >= std::size(VS::Battle::stringContext.integers)) {
      Refuse("workshop text integer outside the string context");
      return;
    }
    VS::Battle::stringContext.integers[slot] = value;
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
    if (slot >= std::size(VS::Items::inventory.weapons)) {
      Refuse("equipped weapon outside the inventory");
      return;
    }
    Commit();
    Items::EquipLiveAshleyWeapon(&VS::Items::inventory.weapons[slot]);
    Reload();
  }
  void EquipShield(std::size_t slot) override {
    if (slot >= std::size(VS::Items::inventory.shields)) {
      Refuse("equipped shield outside the inventory");
      return;
    }
    Commit();
    Items::EquipLiveAshleyShield(&VS::Items::inventory.shields[slot]);
    Reload();
  }
  void EquipArmor(int body_part, std::optional<std::size_t> slot) override {
    if (slot && *slot >= std::size(VS::Items::inventory.armor)) {
      Refuse("equipped armor outside the inventory");
      return;
    }
    Commit();
    Items::EquipLiveAshleyArmor(
        body_part, slot ? &VS::Items::inventory.armor[*slot] : nullptr);
    Reload();
  }
  MaterialBonuses Materials() override {
    MaterialBonuses bonuses{};
    for (std::size_t material = 0; material < bonuses.size(); ++material) {
      const auto& modifiers = VS::Items::materialModifiers[material];
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
    return g_recipes.request->state == VS::Content::DiscRequestState::kLoaded;
  }
  void ReleaseRecipeRequest() override;
  const RecipeBook* Recipes() override {
    return g_recipes.book ? &*g_recipes.book : nullptr;
  }
  void FreeRecipes() override { FreeRecipeBlock(); }

  std::optional<int> AshleySequenceModel() override {
    const VS::Actors::ActorRecord* const ashley = VS::Actors::actorRecords[0];
    if (ashley == nullptr) {
      return std::nullopt;
    }
    return ashley->sequence_model_id;
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
    return VS::Content::objectDataLoadStep;
  }
  void SetEquipmentModelLoadStep(int step) override {
    VS::Content::objectDataLoadStep = static_cast<std::uint8_t>(step);
  }

 private:
  void Reload() {
    inventory_ = VS::Items::inventory;
    indices_ = VS::Items::inventoryIndices;
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
  VS::Content::DiscFile request{};
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
    VS::Core::LogWarn("menu.setup",
                      "workshop reset while its recipe read was in flight; the "
                      "block stays until the read finishes");
    return;
  }
  // The reload dropped _sydData without freeing its block; the native port
  // frees it so repeated reloads do not exhaust the high end.
  FreeRecipeBlock();
}
}  // namespace VS::Menu::Setup
