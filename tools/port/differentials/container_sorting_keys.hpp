#pragma once
#include "Items/ItemRecords.hpp"
#include "Menu/Framework/Framework.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace VS::Menu::Framework::sorting_test {
class KeyHost final : public Host {
public:
  void Refuse(std::string_view what) override {
    throw std::logic_error("unexpected sort-key framework service");
  }
  std::span<MenuRow> Rows() override {
    throw std::logic_error("unexpected sort-key framework service");
  }
  std::span<std::uint8_t> Strings() override {
    throw std::logic_error("unexpected sort-key framework service");
  }
  std::span<std::int32_t> RowTypes() override {
    throw std::logic_error("unexpected sort-key framework service");
  }
  std::span<std::uint8_t> MenuMemory() override {
    throw std::logic_error("unexpected sort-key framework service");
  }
  bool AllocateWorkspace() override {
    throw std::logic_error("unexpected sort-key framework service");
  }
  void ReleaseWorkspace() override {
    throw std::logic_error("unexpected sort-key framework service");
  }
  void ForgetWorkspace() override {
    throw std::logic_error("unexpected sort-key framework service");
  }
  std::span<ItemText> Items(ItemList list) override {
    throw std::logic_error("unexpected sort-key framework service");
  }
  bool HasItems(ItemList list) override {
    throw std::logic_error("unexpected sort-key framework service");
  }
  std::span<ItemText> AllocateItems(ItemList list, int count) override {
    throw std::logic_error("unexpected sort-key framework service");
  }
  void FreeItems(ItemList list) override {
    throw std::logic_error("unexpected sort-key framework service");
  }
  Buttons Input() override {
    throw std::logic_error("unexpected sort-key framework service");
  }
  Settings MenuSettings() override {
    throw std::logic_error("unexpected sort-key framework service");
  }
  void SetMenuFlags(std::uint32_t flags) override {
    throw std::logic_error("unexpected sort-key framework service");
  }
  void PlaySound(MenuSound sound) override {
    throw std::logic_error("unexpected sort-key framework service");
  }
  void InitInformationBox(InformationHeader header) override {
    throw std::logic_error("unexpected sort-key framework service");
  }
  void SetInformationBoxState(std::uint8_t state) override {
    throw std::logic_error("unexpected sort-key framework service");
  }
  void SetInformationText(std::span<const std::uint8_t> text) override {
    throw std::logic_error("unexpected sort-key framework service");
  }
  void ShowInformationMessage(std::span<const std::uint8_t> text) override {
    throw std::logic_error("unexpected sort-key framework service");
  }
  void DismissInformationBox() override {
    throw std::logic_error("unexpected sort-key framework service");
  }
  void SetStringArgument(std::span<const std::uint8_t> text) override {
    throw std::logic_error("unexpected sort-key framework service");
  }
  int CurrentSceneId() override {
    throw std::logic_error("unexpected sort-key framework service");
  }
  std::optional<RestrictionInputs> Restrictions() override {
    throw std::logic_error("unexpected sort-key framework service");
  }
  bool IsActionUnlocked(int action) override {
    throw std::logic_error("unexpected sort-key framework service");
  }
  StoryFlags Story() override {
    throw std::logic_error("unexpected sort-key framework service");
  }
  bool HasUnlockedSpellClass(int spell_class) override {
    throw std::logic_error("unexpected sort-key framework service");
  }
  bool HasUnlockedWeaponBreakArt() override {
    throw std::logic_error("unexpected sort-key framework service");
  }
  bool HasUnlockedBattleAbility(int ability) override {
    throw std::logic_error("unexpected sort-key framework service");
  }
  std::span<const std::uint8_t> AshleyName() override {
    throw std::logic_error("unexpected sort-key framework service");
  }
  const char *LimbName(int limb) override {
    throw std::logic_error("unexpected sort-key framework service");
  }
  void BeginOverlayLoad(int sub_menu, bool main_menu_program) override {
    throw std::logic_error("unexpected sort-key framework service");
  }
  bool IsOverlayLoaded() override {
    throw std::logic_error("unexpected sort-key framework service");
  }
  void FinishOverlayLoad() override {
    throw std::logic_error("unexpected sort-key framework service");
  }
  void ForgetOverlayLoad() override {
    throw std::logic_error("unexpected sort-key framework service");
  }
  void ResetMainMenuOverlay() override {
    throw std::logic_error("unexpected sort-key framework service");
  }
  bool ReleaseBackgroundBuffer() override {
    throw std::logic_error("unexpected sort-key framework service");
  }
  void StoreEquippedStats() override {
    throw std::logic_error("unexpected sort-key framework service");
  }
  std::span<std::uint32_t> AllocateBackgroundBuffer() override {
    throw std::logic_error("unexpected sort-key framework service");
  }
  void ForgetBackgroundBuffer() override {
    throw std::logic_error("unexpected sort-key framework service");
  }
  std::span<const std::uint8_t> BackgroundSource() override {
    throw std::logic_error("unexpected sort-key framework service");
  }
  void UpdateMiscItemShortcut(int mode) override {
    throw std::logic_error("unexpected sort-key framework service");
  }
  int RunSubmenu(Submenu submenu, int state_index) override {
    throw std::logic_error("unexpected sort-key framework service");
  }
  void BindInventoryViews() override {
    throw std::logic_error("unexpected sort-key framework service");
  }
  void UpdateStatPanels() override {
    throw std::logic_error("unexpected sort-key framework service");
  }
  void ReserveMenuTexturePages() override {
    throw std::logic_error("unexpected sort-key framework service");
  }
  void RetireMenuTexturePages() override {
    throw std::logic_error("unexpected sort-key framework service");
  }
  void UploadImage(std::uint32_t position, std::span<const std::byte> pixels,
                   std::uint32_t size) override {
    throw std::logic_error("unexpected sort-key framework service");
  }
  void ClearImage(const ImageRect &rect) override {
    throw std::logic_error("unexpected sort-key framework service");
  }
  void LoadBackgroundClut(std::span<const std::uint8_t> colors) override {
    throw std::logic_error("unexpected sort-key framework service");
  }
  void ResetBattleInterface() override {
    throw std::logic_error("unexpected sort-key framework service");
  }
  void SetBackgroundRaised(bool raised) override {
    throw std::logic_error("unexpected sort-key framework service");
  }
  Render::PacketQueue &Queue(Layer layer) override {
    throw std::logic_error("unexpected sort-key framework service");
  }
  Render::FontStyle &FontStyle() override {
    throw std::logic_error("unexpected sort-key framework service");
  }
  void DrawCompactText(Layer layer, std::string_view text,
                       std::uint32_t position, std::uint32_t color) override {
    throw std::logic_error("unexpected sort-key framework service");
  }
  int FrameBufferIndex() override {
    throw std::logic_error("unexpected sort-key framework service");
  }
  bool TabArrowsFrozen() override {
    throw std::logic_error("unexpected sort-key framework service");
  }
  AshleyPreviewState AshleyPreview() override {
    throw std::logic_error("unexpected sort-key framework service");
  }
  void SetAshleyPreview(const AshleyPreviewState &preview) override {
    throw std::logic_error("unexpected sort-key framework service");
  }
  std::optional<AshleyMetrics> Ashley() override {
    throw std::logic_error("unexpected sort-key framework service");
  }
  void BeginAshleyPreview() override {
    throw std::logic_error("unexpected sort-key framework service");
  }
  void LoadActorPreview() override {
    throw std::logic_error("unexpected sort-key framework service");
  }
  void ReleaseActorPreview() override {
    throw std::logic_error("unexpected sort-key framework service");
  }
  void EndAshleyPreview() override {
    throw std::logic_error("unexpected sort-key framework service");
  }
  void DrawAshleyPreview(const Core::Math::Vec3 &look_at,
                         const Core::Math::Vec3 &position) override {
    throw std::logic_error("unexpected sort-key framework service");
  }
  void UpdateActorAssetQueue() override {
    throw std::logic_error("unexpected sort-key framework service");
  }
  int FindFreeActorAssetRequest() override {
    throw std::logic_error("unexpected sort-key framework service");
  }
};
} // namespace VS::Menu::Framework::sorting_test

namespace VS::Menu::container::sort_test {
// Test-only projections of typed records into the existing Framework keys.
// Field mappings follow FrameworkLive.cpp
// WeaponStats/ShieldStats/EquipmentStats; all key selection, negation, folding
// and signed comparisons run production code.
class ProductionKeys {
public:
  template <typename Rows, typename Destination>
  static void Flatten(const Rows &source, Destination &destination) {
    std::size_t next = 0;
    for (const auto &row : source) {
      for (const auto value : row)
        destination[next++] = value;
    }
  }
  std::int32_t Weapon(std::int32_t stat, const Items::WeaponLoadout &item) {
    Framework::SortStats key{.category = item.blade.category,
                             .material = item.blade.material,
                             .range = item.range.range,
                             .damage_type = item.damage_type,
                             .current_dp = item.current_dp,
                             .max_dp = item.max_dp,
                             .current_pp = item.current_pp,
                             .max_pp = item.max_pp,
                             .strength = item.attributes.current_strength,
                             .intelligence =
                                 item.attributes.current_intelligence,
                             .agility = item.attributes.current_agility};
    Flatten(item.class_affinity.classes, key.classes);
    Flatten(item.class_affinity.affinities, key.affinities);
    return Framework::WeaponSortValue(context_, stat, key);
  }
  std::int32_t Shield(std::int32_t stat, const Items::ShieldLoadout &item) {
    Framework::SortStats key{.material = item.base.material,
                             .current_dp = item.current_dp,
                             .max_dp = item.max_dp,
                             .current_pp = item.current_pp,
                             .max_pp = item.max_pp,
                             .strength = item.attributes.current_strength,
                             .intelligence =
                                 item.attributes.current_intelligence,
                             .agility = item.attributes.current_agility};
    for (std::size_t i = 0; i < item.types.size(); ++i)
      key.types[i] = item.types[i];
    Flatten(item.class_affinity.classes, key.classes);
    Flatten(item.class_affinity.affinities, key.affinities);
    return Framework::ShieldSortValue(context_, stat, key);
  }
  std::int32_t Misc(std::int32_t stat, const Items::InventoryMisc &item) {
    return Framework::MiscSortValue(stat, {.id = item.id, .count = item.count});
  }
  std::int32_t Equipment(std::int32_t stat, const Items::EquipmentPart &item) {
    Framework::SortStats key{.category = item.category,
                             .material = item.material,
                             .range = item.range.range,
                             .damage_type = item.damage_type,
                             .current_dp = item.current_dp,
                             .max_dp = item.max_dp,
                             .current_pp = item.current_pp,
                             .max_pp = item.max_pp,
                             .strength = item.strength,
                             .intelligence = item.intelligence,
                             .agility = item.agility};
    for (std::size_t i = 0; i < item.types.size(); ++i)
      key.types[i] = item.types[i];
    for (std::size_t i = 0; i < item.classes.size(); ++i) {
      key.classes[i] = item.classes[i];
      key.classes[item.classes.size() + i] = item.affinities[i];
      key.affinities[i] = item.affinities[i];
    }
    return Framework::EquipmentSortValue(context_, stat, key);
  }

private:
  Framework::sorting_test::KeyHost host_;
  Framework::State state_;
  Framework::Tables tables_;
  Framework::Context context_{
      .state = state_, .host = host_, .tables = tables_};
};
} // namespace VS::Menu::container::sort_test
