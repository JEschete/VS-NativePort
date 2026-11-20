// Hand-maintained (tools/port/owned.csv).
// Live binding for the equipment choice list and preview model reload
// (EquipmentRows.cpp): MENU3.PRG's list globals, the game heap, the menu rows,
// the controller, the equipment stat panel, the preview actor and the asset
// queue.
#include "Menu/Inventory/EquipmentRowsLive.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "Actors/ActorRecord.hpp"
#include "Actors/AnimationControlLive.hpp"
#include "Actors/SequencePlaybackLive.hpp"
#include "Actors/SkeletonLiveState.hpp"
#include "Actors/WeaponModel.hpp"
#include "Audio/SfxLive.hpp"
#include "Battle/AbilityState.hpp"
#include "Battle/Actor.hpp"
#include "Content/ActorAssets.hpp"
#include "Content/ActorAssetsLive.hpp"
#include "Core/Log.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Core/Memory/WordCopyLive.hpp"
#include "Input/ControllerLiveState.hpp"
#include "Items/EquipmentLive.hpp"
#include "Items/InventoryLive.hpp"
#include "Items/ItemRecords.hpp"
#include "Legacy/Psx.hpp"
#include "Menu/Container/ItemView.hpp"
#include "Menu/Container/Storage.hpp"
#include "Menu/Framework/ActorPreviewLive.hpp"
#include "Menu/Framework/EquipmentDetailsLive.hpp"
#include "Menu/Framework/EquipmentDetailsLiveState.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Inventory/EquipmentRows.hpp"
#include "Menu/Inventory/Inventory.hpp"
#include "Menu/Sounds.hpp"

namespace VS::Menu::Inventory {
namespace {
using Content::ActorAssetRequest;
using Content::AssetAddress;
using Framework::ItemText;
using Framework::MenuRow;
using Framework::RowPlacement;

constexpr std::size_t kItemTextBytes = 128;
static_assert(sizeof(ItemText) == kItemTextBytes,
              "_itemsText's records are 128 bytes");

constexpr std::uint32_t kRefusalLogLimit = 16;
constexpr std::uint32_t kRefusalLogInterval = 1024;
// vs_main_playSfxDefault's menu sound group (16C.c:191).
constexpr int kMenuSoundGroup = 0x7E;
// VS::Actors::actorRecords' preview actor (vs_battle_actors[1]).
constexpr int kPreviewActor = 1;
constexpr int kAdvanceAllBones = 1;
constexpr int kWeaponDrawTransition = 1;
std::uint64_t g_refusals = 0;
// decomp: _processEquipSubMenu's cursor static (16C.c:113).
std::uint8_t g_cursor_animation = 0;

void Refuse(std::string_view reason) {
  ++g_refusals;
  if (g_refusals <= kRefusalLogLimit || g_refusals % kRefusalLogInterval == 0) {
    VS::Core::LogWarn("menu.inventory", "equipment rows: {} (refusal {})",
                      reason, g_refusals);
  }
}

[[nodiscard]] std::span<const std::uint8_t> CallerText(
    const std::uint16_t* text, std::size_t bytes) {
  if (text == nullptr) {
    return {};
  }
  return {reinterpret_cast<const std::uint8_t*>(text), bytes};
}

// itemsText's first `count` records, inside the live game heap payload that
// holds them.
[[nodiscard]] std::optional<std::span<ItemText>> LiveText(ItemText* text,
                                                          std::size_t count) {
  if (reinterpret_cast<std::uintptr_t>(text) % alignof(ItemText) != 0) {
    return std::nullopt;
  }
  const auto payload = Core::Memory::LivePayloadContaining(text);
  if (!payload) {
    return std::nullopt;
  }
  const auto* begin = reinterpret_cast<const std::byte*>(text);
  const std::byte* end = payload->data() + payload->size();
  if (begin < payload->data() || begin >= end ||
      static_cast<std::size_t>(end - begin) / sizeof(ItemText) < count) {
    return std::nullopt;
  }
  return std::span<ItemText>(text, count);
}

[[nodiscard]] EquipmentRowsState LoadScalars() {
  EquipmentRowsState state;
  state.selected_row = VS::Menu::Inventory::selectedItemRow;
  state.step = VS::Menu::Inventory::equipListStep;
  state.row_count = VS::Menu::Inventory::menuRowCount;
  state.first_row = VS::Menu::Inventory::itemsFirstRow;
  state.equipped_item = VS::Menu::Inventory::equippedItem;
  state.cursor_animation = g_cursor_animation;
  return state;
}

[[nodiscard]] std::optional<EquipmentRowsState> LoadState() {
  EquipmentRowsState state = LoadScalars();
  if (VS::Menu::Inventory::itemsText != nullptr) {
    const auto text = LiveText(VS::Menu::Inventory::itemsText, state.row_count);
    if (!text) {
      Refuse("the list text is outside a live game heap payload");
      return std::nullopt;
    }
    state.text = *text;
  }
  return state;
}

void StoreState(const EquipmentRowsState& state) {
  VS::Menu::Inventory::itemsText = state.text.data();
  VS::Menu::Inventory::selectedItemRow = state.selected_row;
  VS::Menu::Inventory::equipListStep = state.step;
  VS::Menu::Inventory::menuRowCount = state.row_count;
  VS::Menu::Inventory::itemsFirstRow = state.first_row;
  VS::Menu::Inventory::equippedItem = state.equipped_item;
  g_cursor_animation = state.cursor_animation;
}

class LiveRowsHost final : public EquipmentRowsHost {
 public:
  std::span<ItemText> AllocateText(std::size_t count) override {
    void* text =
        Core::Memory::AllocateLiveFromHighEnd(count * sizeof(ItemText));
    if (text == nullptr) {
      return {};
    }
    return {static_cast<ItemText*>(text), count};
  }
  void FreeText(std::span<ItemText> text) override {
    Core::Memory::FreeLiveFromHighEnd(text.data());
  }
  MenuRow& SetRow(const RowPlacement& placement,
                  std::span<const std::uint8_t> text) override {
    return Framework::SetLiveMenuItemText(placement, text);
  }
  MenuRow& Row(int row) override { return Framework::LiveMenuRow(row); }
  void ShowInformation(std::span<const std::uint8_t> message) override {
    Framework::SetLiveInformationMessageText(message);
  }
  bool RowsIdle() override { return Framework::AreLiveMenuRowsIdle() != 0; }
  void PlayLeaveSound() override { Menu::PlayLeaveSound(); }
  void PlayInvalidSound() override { Menu::PlayInvalidSound(); }
  void PlayChangeSound() override { Menu::PlayChangeSound(); }
  void PlayConfirmSound(std::int32_t effect) override {
    Audio::LivePlaySfxCentered(kMenuSoundGroup, effect);
  }
  std::int32_t DrawCursor(std::uint8_t animation,
                          std::int32_t position) override {
    return Framework::DrawLiveCursor(animation, position);
  }
};

// vs_mainMenu_armor: Ashley's armor or the open container's.
[[nodiscard]] std::span<const VS::Items::InventoryArmor> BoundArmor() {
  const VS::Items::InventoryArmor* table = VS::Menu::Framework::armor;
  if (table == nullptr) {
    return {};
  }
  if (table == VS::Items::inventory.armor.data()) {
    return VS::Items::inventory.armor;
  }
  if (VS::Menu::container::containerData != nullptr &&
      table == VS::Menu::container::containerData->items.armor.data()) {
    return VS::Menu::container::containerData->items.armor;
  }
  return {};
}

[[nodiscard]] std::uint16_t Unsigned(std::uint16_t value) { return value; }

class LiveAccessoryHost final : public AccessoryPreviewHost {
 public:
  bool HasAccessory(std::int32_t slot) override {
    return slot >= 0 && static_cast<std::size_t>(slot) < armor_.size();
  }
  void ResetStats() override { Framework::ResetLiveEquipmentStats(); }
  AccessoryPreview LoadAccessory(std::int32_t slot) override {
    VS::Items::AccessorySpawnLoadout loadout{};
    Items::LoadLiveInventoryAccessoryStats(
        &loadout, &armor_[static_cast<std::size_t>(slot)]);
    AccessoryPreview stats;
    stats.strength = loadout.current_strength;
    stats.intelligence = loadout.current_intelligence;
    stats.agility = loadout.current_agility;
    for (std::size_t i = 0; i < stats.types.size(); ++i) {
      stats.types[i] = Unsigned(loadout.types[i]);
    }
    for (std::size_t i = 0; i < stats.classes.size(); ++i) {
      stats.classes[i] = Unsigned(loadout.classes[i]);
      stats.affinities[i] = Unsigned(loadout.affinities[i]);
    }
    return stats;
  }
  void StoreStatColumns(const AccessoryStatColumns& columns) override {
    for (std::size_t i = 0; i < kAccessoryStatColumn; ++i) {
      VS::Menu::Framework::equipmentStats[i] = columns.classes[i];
      VS::Menu::Framework::equipmentStats[i + kAccessoryStatColumn] =
          columns.affinities[i];
      VS::Menu::Framework::equipmentStats[i + 2 * kAccessoryStatColumn] =
          columns.types[i];
    }
  }
  void SetPrimaryStats(std::int32_t strength, std::int32_t intelligence,
                       std::int32_t agility, std::int32_t known) override {
    Framework::SetLivePrimaryStats(strength, intelligence, agility, known);
  }
  void SetStatusItem(std::uint8_t subtype, std::uint8_t item) override {
    VS::Menu::Framework::equipmentSubtype = static_cast<char>(subtype);
    VS::Menu::Framework::currentStatusViewItem = static_cast<char>(item);
  }
  void SetStatPages(std::int32_t pages) override {
    Framework::SetLiveEnabledStatPages(pages);
  }

 private:
  std::span<const VS::Items::InventoryArmor> armor_ = BoundArmor();
};

[[nodiscard]] AssetAddress Address(const void* pointer) {
  return static_cast<AssetAddress>(reinterpret_cast<std::uintptr_t>(pointer));
}

[[nodiscard]] VS::Actors::WeaponModel* WeaponModel(int slot) {
  if (slot < 0 || slot >= VS::Actors::kWeaponModelCount) {
    return nullptr;
  }
  return VS::Actors::weaponModels[slot];
}

// The original kept vs_battle_actors[1]'s pointer from entry, across the
// preview's release and clone (68C4.c:16).
class LiveModelHost final : public EquipmentModelHost {
 public:
  bool HasPreviewActor() override { return actor_ != nullptr; }
  std::uint8_t PreviewSequenceModel() override {
    return actor_->sequence_model_id;
  }
  std::uint8_t PreviewAnimation() override { return actor_->animation_id; }
  bool HasWeaponModel(int slot) override {
    return WeaponModel(slot) != nullptr;
  }
  std::optional<AssetAddress> EquipmentModelSlot(int index) override {
    if (VS::Battle::characterState == nullptr || index < 0 ||
        static_cast<std::size_t>(index) >=
            std::size(VS::Battle::characterState->equipment_models)) {
      return std::nullopt;
    }
    return Address(VS::Battle::characterState
                       ->equipment_models[static_cast<std::size_t>(index)]);
  }
  void ReleaseWeaponModel(int slot) override {
    static_cast<void>(Content::ReleaseLiveWeaponModel(slot));
  }
  void QueueWeaponAnimations(std::int32_t sequence_model) override {
    Framework::QueueLiveAshleyWeaponAnimations(sequence_model);
  }
  void QueueAsset(const ActorAssetRequest& request) override {
    static_cast<void>(Content::QueueLiveActorAsset(request));
  }
  void SetWeaponDrawn(bool drawn) override {
    Actors::SetLiveActorWeaponDrawn(kPreviewActor, drawn ? 1 : 0,
                                    kWeaponDrawTransition);
  }
  bool AssetQueueBusy() override {
    return Content::UpdateLiveActorAssetQueue() != 0;
  }
  void ResetEquipmentPose() override {
    Framework::ResetLiveAshleyEquipmentPose();
  }
  void ReleasePreview() override { Framework::ReleaseLiveActorPreview(); }
  void ClonePreview(int source_slot) override {
    Framework::CloneLiveActorPreview(source_slot);
  }
  void ResetPreviewPose() override { Framework::ResetLiveActorPreviewPose(); }
  void SheathePreviewWeapon() override { actor_->instance.weapon_drawn = 0; }
  void PlayPreviewAnimation(int animation) override {
    static_cast<void>(
        Actors::PlayLiveActorAnimation(kPreviewActor, animation, 0, 0));
  }
  void AdvancePreviewSkeleton() override {
    Actors::AdvanceLiveSkeletalAnimation(*actor_, kAdvanceAllBones);
  }
  void CopyPreviewPoseToBlend() override {
    Core::Memory::CopyLiveAlignedWords(&actor_->blend_pose, &actor_->pose,
                                       sizeof actor_->blend_pose);
  }
  void ClearWeaponModelFlag(int slot) override {
    if (VS::Actors::WeaponModel* model = WeaponModel(slot)) {
      model->blend_level = 0;
    }
  }

 private:
  VS::Actors::ActorRecord* actor_ = VS::Actors::actorRecords[kPreviewActor];
};
}  // namespace

void LiveCreateEquipmentRows(int count, std::uint16_t** menu_text,
                             int* row_types) {
  if (count < 1 || static_cast<std::size_t>(count) > kEquipmentRowCapacity) {
    Refuse("a row count outside the list's capacity");
    return;
  }
  if (menu_text == nullptr || row_types == nullptr) {
    Refuse("rows without their texts or flags");
    return;
  }
  const auto rows_count = static_cast<std::size_t>(count);
  std::array<EquipmentRowSource, kEquipmentRowCapacity> rows{};
  for (std::size_t i = 0; i < rows_count; ++i) {
    rows[i].name = CallerText(menu_text[2 * i], kEquipmentNameBytes);
    rows[i].description =
        CallerText(menu_text[2 * i + 1], kEquipmentDescriptionBytes);
    rows[i].flags = row_types[i];
  }
  EquipmentRowsState state = LoadScalars();
  LiveRowsHost host;
  if (!CreateEquipmentRows(state, host, std::span(rows.data(), rows_count))) {
    Refuse("the list text could not be allocated");
  }
  StoreState(state);
}

int LiveSelectedEquipmentRow() {
  EquipmentRowsState state = LoadScalars();
  // Only whether the text is held matters here, not its records.
  state.text = std::span<ItemText>(VS::Menu::Inventory::itemsText, 0);
  return SelectedEquipmentRow(state);
}

void UpdateLiveEquipmentChoices(int has_sfx) {
  auto state = LoadState();
  if (!state) {
    return;
  }
  LiveRowsHost host;
  const EquipmentChoiceInput input{
      VS::Input::buttonsPressed,
      static_cast<std::uint32_t>(VS::Input::buttonRepeat)};
  if (!UpdateEquipmentChoices(*state, host, input, has_sfx)) {
    Refuse("a list step, cursor or scroll position outside the list");
    return;
  }
  StoreState(*state);
}

void SetLiveAccessoryPreviewStats(int accessory_id) {
  LiveAccessoryHost host;
  if (!SetAccessoryPreviewStats(host, accessory_id)) {
    Refuse("an accessory outside the bound armor table");
  }
}

int ReloadLiveEquipmentModels(int category, int wep_id, int material) {
  auto step = VS::Content::objectDataLoadStep;
  LiveModelHost host;
  const ModelReload result =
      ReloadEquipmentModels(step, host, {category, wep_id, material});
  VS::Content::objectDataLoadStep = step;
  if (result == ModelReload::kRefused) {
    Refuse("a model reload without the preview actor or Ashley's model slots");
    return 0;
  }
  return result == ModelReload::kLoading ? 1 : 0;
}

int LiveIsTwoHandedWeapon(int category) {
  return IsTwoHandedWeapon(category) ? 1 : 0;
}

void QueueLiveEquipmentModel(int wep_id, int material) {
  LiveModelHost host;
  if (!QueueEquipmentModel(host, wep_id, material)) {
    Refuse("an equipment model without Ashley's model slots");
  }
}

void ResetEquipmentRowsLive() { g_cursor_animation = 0; }

std::uint64_t LiveEquipmentRowsRefusalCount() { return g_refusals; }
}  // namespace VS::Menu::Inventory
