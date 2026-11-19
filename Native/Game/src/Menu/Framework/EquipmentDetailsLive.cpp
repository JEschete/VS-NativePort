// Hand-maintained (tools/port/owned.csv).
// Live binding for the equipment details (EquipmentDetails.hpp): defines the
// variables the copied menus share (EquipmentDetailsLiveState.hpp) and the
// screen's statics, and implements the host over the item rows' host,
// Ashley's equipment, the battle actors, the controller, the menu rows and the
// frame's ordering tables.
#include "Menu/Framework/EquipmentDetailsLive.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <span>
#include <string_view>

#include "Audio/SfxLive.hpp"
#include "Battle/AbilityState.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Core/Log.hpp"
#include "Items/EquipmentLive.hpp"
#include "Items/ItemRecords.hpp"
#include "Items/ItemRecordsLive.hpp"
#include "Legacy/Psx.h"
#include "Menu/Framework/EquipmentDetails.hpp"
#include "Menu/Framework/EquipmentDetailsLiveState.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Framework/ItemRowsLiveHost.hpp"
#include "Menu/Framework/Menus.h"
#include "Menu/Sounds.hpp"
#include "Menu/Status/EquipmentStats.hpp"
#include "Render/GameFontLive.hpp"
#include "Render/UiPackets.hpp"
#include "Render/UiPacketsLive.hpp"

namespace VS::Menu::Framework {
namespace {
constexpr std::size_t kArmorStatsLimbCount = 5;

// vs_mainMenu_drawClassAffinityType's page turn (C48.c:950): sound effect
// 0x7E of group 14.
constexpr int kPageSoundGroup = 14;
constexpr int kPageSoundEffect = 0x7E;

// The screen's variables between calls: the shared ones are reloaded from
// EquipmentDetailsLiveState.hpp around each call, the rest were module statics.
EquipmentDetailsState g_state;

void Warn(std::string_view what) {
  VS::Core::LogWarn("menu.framework", "equipment details: {}", what);
}

[[nodiscard]] std::uint8_t Byte(char value) {
  return static_cast<std::uint8_t>(value);
}

void Load() {
  auto& state = g_state;
  std::ranges::copy(equipmentStats, state.stats.begin());
  std::ranges::copy(strIntAgi, state.primary.begin());
  std::ranges::transform(rangeRiskData, state.range_risk.begin(), Byte);
  std::ranges::copy(comparisonStatDeltas, state.comparison_deltas.begin());
  std::ranges::copy(comparisonStatTotals, state.comparison_totals.begin());
  std::ranges::copy(comparisonAccessoryStats,
                    state.comparison_accessory.begin());
  state.subtype = Byte(equipmentSubtype);
  state.item = Byte(currentStatusViewItem);
  state.enabled_pages = Byte(enabledStatPages);
  state.page = Byte(itemStatPage);
  state.selected = selectedStatusViewElement;
  std::ranges::transform(statusViewCursorPositions,
                         state.cursor_positions.begin(),
                         [](int xy) { return static_cast<std::uint32_t>(xy); });
  for (std::size_t i = 0; i < kStatusViewElementCount; ++i) {
    std::ranges::copy(statusViewNavigationMap[i], state.navigation[i].begin());
  }
}

void Store() {
  const auto& state = g_state;
  std::ranges::copy(state.stats, equipmentStats);
  std::ranges::copy(state.primary, strIntAgi);
  std::ranges::transform(
      state.range_risk, rangeRiskData,
      [](std::uint8_t value) { return static_cast<char>(value); });
  std::ranges::copy(state.comparison_deltas, comparisonStatDeltas);
  std::ranges::copy(state.comparison_totals, comparisonStatTotals);
  std::ranges::copy(state.comparison_accessory, comparisonAccessoryStats);
  equipmentSubtype = static_cast<char>(state.subtype);
  currentStatusViewItem = static_cast<char>(state.item);
  enabledStatPages = static_cast<char>(state.enabled_pages);
  itemStatPage = static_cast<char>(state.page);
  selectedStatusViewElement = state.selected;
  std::ranges::transform(state.cursor_positions, statusViewCursorPositions,
                         [](std::uint32_t xy) { return static_cast<int>(xy); });
  for (std::size_t i = 0; i < kStatusViewElementCount; ++i) {
    std::ranges::copy(state.navigation[i], statusViewNavigationMap[i]);
  }
}

// Writes the status view tables' module image values (2D10.c:823,
// 2D10.c:830), as loading the overlay did; the game only reads them.
bool LoadStatusViewTables() noexcept {
  std::ranges::transform(kStatusViewCursorPositions, statusViewCursorPositions,
                         [](std::uint32_t xy) { return static_cast<int>(xy); });
  for (std::size_t i = 0; i < kStatusViewElementCount; ++i) {
    std::ranges::copy(kStatusViewNavigation[i], statusViewNavigationMap[i]);
  }
  return true;
}

// The tables hold their image values before any menu reads them.
[[maybe_unused]] const bool kStatusViewTablesLoaded = LoadStatusViewTables();

// Ashley's statistics; null before they are loaded.
[[nodiscard]] Battle_ActorStats* AshleyStats() {
  return Battle_characterState == nullptr ? nullptr
                                          : Battle_characterState->stats;
}

class LiveEquipmentDetailsHost final : public EquipmentDetailsHost {
 public:
  void Refuse(std::string_view what) override { Warn(what); }

  ItemRowsHost& Rows() override { return rows_; }

  std::optional<Items::WeaponLoadout> EquippedWeapon() override {
    const Battle_ActorStats* stats = AshleyStats();
    if (stats == nullptr) {
      return std::nullopt;
    }
    return stats->weapon;
  }

  std::optional<Items::ShieldLoadout> EquippedShield() override {
    const Battle_ActorStats* stats = AshleyStats();
    if (stats == nullptr) {
      return std::nullopt;
    }
    return stats->shield;
  }

  std::optional<Items::ArmorLoadout> EquippedArmor(int limb) override {
    const Battle_ActorStats* stats = AshleyStats();
    if (stats == nullptr || limb < 0 ||
        static_cast<std::size_t>(limb) >= std::size(stats->limbs)) {
      return std::nullopt;
    }
    return stats->limbs[static_cast<std::size_t>(limb)].armor;
  }

  std::optional<Items::ArmorLoadout> InventoryArmorLoadout(
      const Items::InventoryArmor& armor) override {
    VS::Items::ArmorLoadout loadout{};
    Items::LoadLiveInventoryArmorStats(&loadout, &armor);
    return loadout;
  }

  std::optional<ActorComparisonStats> ActorStats(int actor) override {
    if (actor < 0 ||
        static_cast<std::size_t>(actor) >= std::size(Battle_actors)) {
      return std::nullopt;
    }
    const Battle_Actor* record = Battle_actors[static_cast<std::size_t>(actor)];
    if (record == nullptr || record->stats == nullptr) {
      return std::nullopt;
    }
    const Battle_ActorStats& stats = *record->stats;
    return ActorComparisonStats{
        .accessory = {stats.accessory.current_strength,
                      stats.accessory.current_intelligence,
                      stats.accessory.current_agility},
        .base = {stats.strength, stats.intelligence, stats.agility},
        .totals = {stats.totalStrength, stats.totalIntelligence,
                   stats.totalAgility},
        .analyzed = stats.isAnalyzed != 0,
        .current_range = static_cast<std::uint8_t>(stats.currentRange),
        .weapon_range = static_cast<std::uint8_t>(stats.weapon.range.range)};
  }

  Buttons Input() override { return LiveButtons(); }

  std::span<const std::uint8_t> RowAnimationSteps() override {
    return {
        reinterpret_cast<const std::uint8_t*>(Menu_Framework_rowAnimationSteps),
        sizeof Menu_Framework_rowAnimationSteps};
  }

  void PlaySound(MenuSound sound) override {
    switch (sound) {
      case MenuSound::kChange:
        VS::Menu::PlayChangeSound();
        break;
      case MenuSound::kSelect:
        VS::Menu::PlaySelectSound();
        break;
      case MenuSound::kLeave:
        VS::Menu::PlayLeaveSound();
        break;
      case MenuSound::kInvalid:
        VS::Menu::PlayInvalidSound();
        break;
      case MenuSound::kOpen:
        VS::Menu::PlayOpenSound();
        break;
    }
  }

  void PlayPageSound() override {
    Audio::LivePlaySfxCentered(kPageSoundEffect, kPageSoundGroup);
  }

  void SetRowSelected(int row, bool selected) override {
    LiveMenuRow(row).selected = selected ? 1 : 0;
  }

  void SetInformationMessage(TextRef text) override {
    const auto bytes = rows_.TextBytes(text);
    if (bytes.empty()) {
      Warn("an information message outside its text");
      return;
    }
    SetLiveInformationMessage(
        reinterpret_cast<char*>(const_cast<std::uint8_t*>(bytes.data())));
  }

  int DrawCursor(std::uint32_t step, std::uint32_t position) override {
    return DrawLiveCursor(step, static_cast<int>(position));
  }

  Render::PacketQueue& SceneQueue(int slot) override {
    u_long* const chain = Entry(slot);
    queue_.emplace(chain);
    return *queue_;
  }

  std::uint32_t DrawComparedInteger(int value, std::uint32_t xy, int threshold,
                                    int slot) override {
    u_long* const chain = Entry(slot);
    if (chain == nullptr) {
      return xy;
    }
    return static_cast<std::uint32_t>(
        DrawLiveComparedInteger(value, static_cast<int>(xy), threshold, chain));
  }

  void DrawColoredInteger(int value, std::uint32_t xy, std::uint32_t color,
                          int slot) override {
    if (u_long* const chain = Entry(slot)) {
      DrawLiveColoredInteger(value, static_cast<int>(xy),
                             static_cast<int>(color), chain);
    }
  }

  void DrawNeutralInteger(int value, std::uint32_t xy, int slot) override {
    if (u_long* const chain = Entry(slot)) {
      DrawLiveNeutralInteger(value, static_cast<int>(xy), chain);
    }
  }

  void DrawCompactText(const char* text, std::uint32_t xy, std::uint32_t color,
                       int slot) override {
    if (u_long* const chain = Entry(slot)) {
      Render::DrawLiveCompactTextColor(text, static_cast<int>(xy),
                                       static_cast<int>(color), chain);
    }
  }

  std::uint32_t DrawRightAlignedInteger(int font, std::uint32_t xy, int value,
                                        int slot) override {
    u_long* const chain = Entry(slot);
    if (chain == nullptr) {
      return xy;
    }
    return static_cast<std::uint32_t>(Render::DrawLiveIntegerRightAligned(
        font, static_cast<int>(xy), value, chain));
  }

  void DrawStatGauge(int color, int fill_width, std::uint32_t xy,
                     int slot) override {
    if (u_long* const chain = Entry(slot)) {
      Render::DrawLiveStatGauge(color, fill_width, chain, static_cast<int>(xy));
    }
  }

  void DrawButtonPromptBackground(int x, int y, int w, int h) override {
    DrawLiveButtonPromptBackground(x, y, w, h);
  }

  void DrawButtonIcon(int button, int x, int y, int slot) override {
    if (u_long* const chain = Entry(slot)) {
      DrawLiveButtonIcon(button, x, y, chain);
    }
  }

  void SetFontStyle(int palette_offset) override {
    Render::SetLiveMenuFontStyle(palette_offset);
  }

  int DrawGlyph(std::uint8_t glyph, int x, int y, int slot) override {
    u_long* const chain = Entry(slot);
    if (chain == nullptr) {
      return x;
    }
    return Render::DrawLiveVariableWidthGlyph(glyph, x, y, chain);
  }

 private:
  // The scene table's entry `slot`; a draw outside the frame's table is
  // refused (a null chain would send some draws to another table).
  [[nodiscard]] u_long* Entry(int slot) {
    u_long* const chain =
        Render::LiveFrameOrderingEntry(Render::LiveOrderingBase::kScene, slot);
    if (chain == nullptr) {
      Warn("a draw outside the frame's ordering table");
    }
    return chain;
  }

  LiveItemRowsHost rows_;
  std::optional<Render::LivePacketQueue> queue_;
};

// Runs `body` over the loaded state and the Live host, then stores the state.
template <typename Body>
void LiveCall(Body body) {
  Load();
  LiveEquipmentDetailsHost host;
  EquipmentDetailsContext ctx{.state = g_state, .host = host};
  body(ctx);
  Store();
}
}  // namespace

void ResetLiveEquipmentStats() {
  LiveCall(
      [](EquipmentDetailsContext& ctx) { ResetEquipmentStats(ctx.state); });
}

void SetLiveWeaponStats(int weapon_index) {
  LiveCall([weapon_index](EquipmentDetailsContext& ctx) {
    SetWeaponStats(ctx, weapon_index);
  });
}

void SetLiveBladeStats(int blade_index) {
  LiveCall([blade_index](EquipmentDetailsContext& ctx) {
    SetBladeStats(ctx, blade_index);
  });
}

void SetLiveGripStats(int grip_index) {
  LiveCall([grip_index](EquipmentDetailsContext& ctx) {
    SetGripStats(ctx, grip_index);
  });
}

void SetLiveShieldStats(int shield_index) {
  LiveCall([shield_index](EquipmentDetailsContext& ctx) {
    SetShieldStats(ctx, shield_index);
  });
}

void SetLiveArmorStats(int armor_index) {
  LiveCall([armor_index](EquipmentDetailsContext& ctx) {
    SetArmorStats(ctx, armor_index);
  });
}

void SetLiveGemStats(int gem_index) {
  LiveCall([gem_index](EquipmentDetailsContext& ctx) {
    SetGemStats(ctx, gem_index);
  });
}

void ResetLiveItemDetailCursor() {
  LiveCall(
      [](EquipmentDetailsContext& ctx) { ResetItemDetailCursor(ctx.state); });
}

void UpdateLiveItemDetails() {
  LiveCall([](EquipmentDetailsContext& ctx) { UpdateItemDetails(ctx); });
}

void DrawLiveEquipmentStatPage(int x_offset) {
  LiveCall([x_offset](EquipmentDetailsContext& ctx) {
    DrawEquipmentStatPage(ctx, x_offset);
  });
}

void SetLiveEnabledStatPages(int enabled) {
  LiveCall([enabled](EquipmentDetailsContext& ctx) {
    SetEnabledStatPages(ctx, enabled);
  });
}

void UpdateLiveEquipmentStatPages(int command) {
  LiveCall([command](EquipmentDetailsContext& ctx) {
    UpdateEquipmentStatPages(ctx, command);
  });
}

void SetLiveRangeAndRisk(int range, int risk, int unused, int shown) {
  LiveCall([&](EquipmentDetailsContext& ctx) {
    SetRangeAndRisk(ctx.state, {range, risk, unused, shown});
  });
}

void SetLivePrimaryStats(int strength, int intelligence, int agility,
                         int known) {
  LiveCall([&](EquipmentDetailsContext& ctx) {
    SetPrimaryStats(ctx.state, {strength, intelligence, agility, known});
  });
}

void SetLiveActorComparisonStats(int actor) {
  LiveCall([actor](EquipmentDetailsContext& ctx) {
    SetActorComparisonStats(ctx, actor);
  });
}

void UpdateLivePrimaryStatPanel(int animate) {
  LiveCall([animate](EquipmentDetailsContext& ctx) {
    UpdatePrimaryStatPanel(ctx, animate);
  });
}

void SetLiveDpPp(int current_dp, int max_dp, int current_pp, int max_pp) {
  LiveCall([&](EquipmentDetailsContext& ctx) {
    SetDpPp(ctx.state, {current_dp, max_dp, current_pp, max_pp});
  });
}

void UpdateLiveDpPpBars(int flags) {
  LiveCall(
      [flags](EquipmentDetailsContext& ctx) { UpdateDpPpBars(ctx, flags); });
}

void PopulateLiveWeaponStats(const VS::Items::WeaponLoadout* weapon) {
  if (weapon == nullptr) {
    Warn("missing weapon loadout for status details");
    return;
  }
  const Battle_ActorStats* const ashley = AshleyStats();
  const bool equipped = ashley != nullptr && weapon == &ashley->weapon;
  LiveCall([&](EquipmentDetailsContext& ctx) {
    const auto effects =
        VS::Menu::Status::PopulateWeaponStats(ctx.state, *weapon, equipped);
    SetEnabledStatPages(ctx, effects.enabled_pages);
  });
}

void PopulateLiveShieldStats(const VS::Items::ShieldLoadout* shield) {
  if (shield == nullptr) {
    Warn("missing shield loadout for status details");
    return;
  }
  const Battle_ActorStats* const ashley = AshleyStats();
  const bool equipped = ashley != nullptr && shield == &ashley->shield;
  LiveCall([&](EquipmentDetailsContext& ctx) {
    const auto effects =
        VS::Menu::Status::PopulateShieldStats(ctx.state, *shield, equipped);
    SetEnabledStatPages(ctx, effects.enabled_pages);
  });
}

void PopulateLiveArmorStats(const VS::Items::ArmorLoadout* armor) {
  if (armor == nullptr) {
    Warn("missing armor loadout for status details");
    return;
  }
  const Battle_ActorStats* const ashley = AshleyStats();
  bool equipped = false;
  if (ashley != nullptr) {
    for (std::size_t limb = 0; limb < kArmorStatsLimbCount; ++limb) {
      equipped |= armor == &ashley->limbs[limb].armor;
    }
  }
  LiveCall([&](EquipmentDetailsContext& ctx) {
    const auto effects =
        VS::Menu::Status::PopulateArmorStats(ctx.state, *armor, equipped);
    SetEnabledStatPages(ctx, effects.enabled_pages);
  });
}

void PopulateLiveAccessoryStats(
    const VS::Items::AccessorySpawnLoadout* accessory) {
  if (accessory == nullptr) {
    Warn("missing accessory loadout for status details");
    return;
  }
  LiveCall([&](EquipmentDetailsContext& ctx) {
    const auto effects =
        VS::Menu::Status::PopulateAccessoryStats(ctx.state, *accessory);
    SetEnabledStatPages(ctx, effects.enabled_pages);
  });
}

void PopulateLiveBladeStats(const VS::Items::WeaponLoadout* weapon) {
  if (weapon == nullptr) {
    Warn("missing weapon loadout for blade details");
    return;
  }
  LiveCall([&](EquipmentDetailsContext& ctx) {
    const auto effects = VS::Menu::Status::PopulateBladeStats(
        ctx.state, weapon->blade, weapon->current_pp, weapon->max_pp);
    SetEnabledStatPages(ctx, effects.enabled_pages);
    UpdateDpPpBars(ctx, effects.dp_pp_bars);
  });
}

void PopulateLiveGripStats(const VS::Items::EquipmentPart* grip) {
  if (grip == nullptr) {
    Warn("missing grip stats for status details");
    return;
  }
  LiveCall([&](EquipmentDetailsContext& ctx) {
    const auto effects = VS::Menu::Status::PopulateGripStats(ctx.state, *grip);
    SetEnabledStatPages(ctx, effects.enabled_pages);
    UpdateDpPpBars(ctx, effects.dp_pp_bars);
  });
}

void PopulateLiveGemStats(const VS::Items::EquipmentPart* gem) {
  if (gem == nullptr) {
    Warn("missing gem stats for status details");
    return;
  }
  LiveCall([&](EquipmentDetailsContext& ctx) {
    const auto effects = VS::Menu::Status::PopulateGemStats(ctx.state, *gem);
    SetEnabledStatPages(ctx, effects.enabled_pages);
    UpdateDpPpBars(ctx, effects.dp_pp_bars);
  });
}

void ResetLiveEquipmentDetailsStatics() {
  const EquipmentDetailsState initial;
  auto& state = g_state;
  state.current_dp = initial.current_dp;
  state.max_dp = initial.max_dp;
  state.current_pp = initial.current_pp;
  state.max_pp = initial.max_pp;
  state.element15_enabled = initial.element15_enabled;
  state.bars_animation = initial.bars_animation;
  state.bars_state = initial.bars_state;
  state.bars_target = initial.bars_target;
  state.cursor_step = initial.cursor_step;
  state.previous_item = initial.previous_item;
  state.panel_direction = initial.panel_direction;
  state.panel_slide = initial.panel_slide;
  state.primary_slide = initial.primary_slide;
  state.primary_animation = initial.primary_animation;
  LoadStatusViewTables();
}
}  // namespace VS::Menu::Framework

// decomp: D_80102488 (MENU/MAINMENU.PRG C48.c)
std::int16_t VS::Menu::Framework::comparisonStatDeltas
    [VS::Menu::Framework::kComparisonStatCount];
// decomp: D_80102490 (MENU/MAINMENU.PRG C48.c)
std::int16_t VS::Menu::Framework::comparisonStatTotals
    [VS::Menu::Framework::kComparisonStatCount];
// decomp: D_80102498 (MENU/MAINMENU.PRG C48.c)
std::int16_t VS::Menu::Framework::comparisonAccessoryStats
    [VS::Menu::Framework::kComparisonStatCount];
// decomp: _rangeRiskData (MENU/MAINMENU.PRG C48.c)
char VS::Menu::Framework::rangeRiskData[VS::Menu::Framework::kRangeRiskBytes];
// decomp: vs_mainMenu_weapons .. vs_mainMenu_gems (MENU/MAINMENU.PRG)
VS::Items::InventoryWeapon* VS::Menu::Framework::weapons;
VS::Items::InventoryBlade* VS::Menu::Framework::blades;
VS::Items::InventoryGrip* VS::Menu::Framework::grips;
VS::Items::InventoryShield* VS::Menu::Framework::shields;
VS::Items::InventoryArmor* VS::Menu::Framework::armor;
VS::Items::InventoryGem* VS::Menu::Framework::gems;
// decomp: vs_mainMenu_currentStatusViewItem (MENU/MAINMENU.PRG)
char VS::Menu::Framework::currentStatusViewItem;
// decomp: vs_mainMenu_enabledStatPages (MENU/MAINMENU.PRG)
char VS::Menu::Framework::enabledStatPages;
// decomp: vs_mainMenu_equipmentStats (MENU/MAINMENU.PRG)
std::int16_t VS::Menu::Framework::equipmentStats
    [VS::Menu::Framework::kEquipmentStatCount];
// decomp: vs_mainMenu_equipmentSubtype (MENU/MAINMENU.PRG)
char VS::Menu::Framework::equipmentSubtype;
// decomp: vs_mainMenu_itemStatPage (MENU/MAINMENU.PRG)
char VS::Menu::Framework::itemStatPage;
// decomp: vs_mainMenu_selectedStatusViewElement (MENU/MAINMENU.PRG)
u_char VS::Menu::Framework::selectedStatusViewElement;
// decomp: vs_mainMenu_statusViewCursorPositions (MENU/MAINMENU.PRG
// 2D10.c:830): the module image values.
int VS::Menu::Framework::statusViewCursorPositions
    [VS::Menu::Framework::kStatusViewElementCount];
// decomp: vs_mainMenu_statusViewNavigationMap (MENU/MAINMENU.PRG
// 2D10.c:823): the module image values.
u_char VS::Menu::Framework::statusViewNavigationMap
    [VS::Menu::Framework::kStatusViewElementCount]
    [VS::Menu::Framework::kStatusViewDirections];
// decomp: vs_mainMenu_strIntAgi (MENU/MAINMENU.PRG)
std::int16_t
    VS::Menu::Framework::strIntAgi[VS::Menu::Framework::kPrimaryStatCount];
