// Hand-maintained (tools/port/owned.csv).
// Live binding for the equipment details (EquipmentDetails.hpp): defines the
// variables the copied menus share (EquipmentDetailsLiveState.h) and the
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
#include "Core/Log.hpp"
#include "Items/EquipmentLive.hpp"
#include "Items/ItemRecordsLive.hpp"
#include "Menu/Framework/EquipmentDetails.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Framework/ItemRowsLiveHost.hpp"
#include "Menu/Sounds.hpp"
#include "Render/GameFontLive.hpp"
#include "Render/UiPackets.hpp"
#include "Render/UiPacketsLive.hpp"

extern "C" {
#include "Battle/AbilityState.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Input/ControllerLiveState.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Legacy/Psx.h"
#include "Menu/Framework/EquipmentDetailsLiveState.h"
#include "Menu/Framework/Menus.h"
}

namespace VS::Menu::Framework {
namespace {
static_assert(kMenuComparisonStatCount == kComparisonStatCount);
static_assert(kMenuRangeRiskBytes == kRangeRiskBytes);
static_assert(kMenuEquipmentStatCount == kEquipmentStatCount);
static_assert(kMenuPrimaryStatCount == kPrimaryStatCount);
static_assert(kMenuStatusViewElementCount == kStatusViewElementCount);
static_assert(kMenuStatusViewDirections == kStatusViewDirections);

// vs_mainMenu_drawClassAffinityType's page turn (C48.c:950): sound effect
// 0x7E of group 14.
constexpr int kPageSoundGroup = 14;
constexpr int kPageSoundEffect = 0x7E;

// The screen's variables between calls: the shared ones are reloaded from
// EquipmentDetailsLiveState.h around each call, the rest were module statics.
EquipmentDetailsState g_state;

void Warn(std::string_view what) {
  VS_LOG_WARN("menu.framework", "equipment details: {}", what);
}

[[nodiscard]] std::uint8_t Byte(char value) {
  return static_cast<std::uint8_t>(value);
}

void Load() {
  auto& state = g_state;
  std::ranges::copy(Menu_Framework_equipmentStats, state.stats.begin());
  std::ranges::copy(Menu_Framework_strIntAgi, state.primary.begin());
  std::ranges::transform(Menu_Framework_rangeRiskData, state.range_risk.begin(),
                         Byte);
  std::ranges::copy(Menu_Framework_comparisonStatDeltas,
                    state.comparison_deltas.begin());
  std::ranges::copy(Menu_Framework_comparisonStatTotals,
                    state.comparison_totals.begin());
  std::ranges::copy(Menu_Framework_comparisonAccessoryStats,
                    state.comparison_accessory.begin());
  state.subtype = Byte(Menu_Framework_equipmentSubtype);
  state.item = Byte(Menu_Framework_currentStatusViewItem);
  state.enabled_pages = Byte(Menu_Framework_enabledStatPages);
  state.page = Byte(Menu_Framework_itemStatPage);
  state.selected = Menu_Framework_selectedStatusViewElement;
  std::ranges::transform(Menu_Framework_statusViewCursorPositions,
                         state.cursor_positions.begin(),
                         [](int xy) { return static_cast<std::uint32_t>(xy); });
  for (std::size_t i = 0; i < kStatusViewElementCount; ++i) {
    std::ranges::copy(Menu_Framework_statusViewNavigationMap[i],
                      state.navigation[i].begin());
  }
}

void Store() {
  const auto& state = g_state;
  std::ranges::copy(state.stats, Menu_Framework_equipmentStats);
  std::ranges::copy(state.primary, Menu_Framework_strIntAgi);
  std::ranges::transform(
      state.range_risk, Menu_Framework_rangeRiskData,
      [](std::uint8_t value) { return static_cast<char>(value); });
  std::ranges::copy(state.comparison_deltas,
                    Menu_Framework_comparisonStatDeltas);
  std::ranges::copy(state.comparison_totals,
                    Menu_Framework_comparisonStatTotals);
  std::ranges::copy(state.comparison_accessory,
                    Menu_Framework_comparisonAccessoryStats);
  Menu_Framework_equipmentSubtype = static_cast<char>(state.subtype);
  Menu_Framework_currentStatusViewItem = static_cast<char>(state.item);
  Menu_Framework_enabledStatPages = static_cast<char>(state.enabled_pages);
  Menu_Framework_itemStatPage = static_cast<char>(state.page);
  Menu_Framework_selectedStatusViewElement = state.selected;
  std::ranges::transform(state.cursor_positions,
                         Menu_Framework_statusViewCursorPositions,
                         [](std::uint32_t xy) { return static_cast<int>(xy); });
  for (std::size_t i = 0; i < kStatusViewElementCount; ++i) {
    std::ranges::copy(state.navigation[i],
                      Menu_Framework_statusViewNavigationMap[i]);
  }
}

// Writes the status view tables' module image values (2D10.c:823,
// 2D10.c:830), as loading the overlay did; the game only reads them.
bool LoadStatusViewTables() noexcept {
  std::ranges::transform(kStatusViewCursorPositions,
                         Menu_Framework_statusViewCursorPositions,
                         [](std::uint32_t xy) { return static_cast<int>(xy); });
  for (std::size_t i = 0; i < kStatusViewElementCount; ++i) {
    std::ranges::copy(kStatusViewNavigation[i],
                      Menu_Framework_statusViewNavigationMap[i]);
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
    return Items::Live::Typed(stats->weapon);
  }

  std::optional<Items::ShieldLoadout> EquippedShield() override {
    const Battle_ActorStats* stats = AshleyStats();
    if (stats == nullptr) {
      return std::nullopt;
    }
    return Items::Live::Typed(stats->shield);
  }

  std::optional<Items::ArmorLoadout> EquippedArmor(int limb) override {
    const Battle_ActorStats* stats = AshleyStats();
    if (stats == nullptr || limb < 0 ||
        static_cast<std::size_t>(limb) >= std::size(stats->limbs)) {
      return std::nullopt;
    }
    return Items::Live::Typed(
        stats->limbs[static_cast<std::size_t>(limb)].armor);
  }

  std::optional<Items::ArmorLoadout> InventoryArmorLoadout(
      const Items::InventoryArmor& armor) override {
    Items_InventoryArmor legacy{};
    Items::Live::Commit(legacy, armor);
    Items_ArmorLoadout loadout{};
    Items::LoadLiveInventoryArmorStats(&loadout, &legacy);
    return Items::Live::Typed(loadout);
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
        .accessory = {stats.accessory.currentStr, stats.accessory.currentInt,
                      stats.accessory.currentAgility},
        .base = {stats.strength, stats.intelligence, stats.agility},
        .totals = {stats.totalStrength, stats.totalIntelligence,
                   stats.totalAgility},
        .analyzed = stats.isAnalyzed != 0,
        .current_range = static_cast<std::uint8_t>(stats.currentRange),
        .weapon_range = static_cast<std::uint8_t>(stats.weapon.range.range)};
  }

  Buttons Input() override {
    return {.pressed = Input_buttonsPressed.allButtons,
            .repeat = static_cast<std::uint32_t>(Input_buttonRepeat),
            .held = static_cast<std::uint32_t>(Input_buttonsState)};
  }

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

extern "C" {
// decomp: D_80102488 (MENU/MAINMENU.PRG C48.c)
std::int16_t Menu_Framework_comparisonStatDeltas[kMenuComparisonStatCount];
// decomp: D_80102490 (MENU/MAINMENU.PRG C48.c)
std::int16_t Menu_Framework_comparisonStatTotals[kMenuComparisonStatCount];
// decomp: D_80102498 (MENU/MAINMENU.PRG C48.c)
std::int16_t Menu_Framework_comparisonAccessoryStats[kMenuComparisonStatCount];
// decomp: _rangeRiskData (MENU/MAINMENU.PRG C48.c)
char Menu_Framework_rangeRiskData[kMenuRangeRiskBytes];
// decomp: vs_mainMenu_weapons .. vs_mainMenu_gems (MENU/MAINMENU.PRG)
Items_InventoryWeapon* Menu_Framework_weapons;
Items_InventoryBlade* Menu_Framework_blades;
Items_InventoryGrip* Menu_Framework_grips;
Items_InventoryShield* Menu_Framework_shields;
Items_InventoryArmor* Menu_Framework_armor;
Items_InventoryGem* Menu_Framework_gems;
// decomp: vs_mainMenu_currentStatusViewItem (MENU/MAINMENU.PRG)
char Menu_Framework_currentStatusViewItem;
// decomp: vs_mainMenu_enabledStatPages (MENU/MAINMENU.PRG)
char Menu_Framework_enabledStatPages;
// decomp: vs_mainMenu_equipmentStats (MENU/MAINMENU.PRG)
std::int16_t Menu_Framework_equipmentStats[kMenuEquipmentStatCount];
// decomp: vs_mainMenu_equipmentSubtype (MENU/MAINMENU.PRG)
char Menu_Framework_equipmentSubtype;
// decomp: vs_mainMenu_itemStatPage (MENU/MAINMENU.PRG)
char Menu_Framework_itemStatPage;
// decomp: vs_mainMenu_selectedStatusViewElement (MENU/MAINMENU.PRG)
u_char Menu_Framework_selectedStatusViewElement;
// decomp: vs_mainMenu_statusViewCursorPositions (MENU/MAINMENU.PRG
// 2D10.c:830): the module image values.
int Menu_Framework_statusViewCursorPositions[kMenuStatusViewElementCount];
// decomp: vs_mainMenu_statusViewNavigationMap (MENU/MAINMENU.PRG
// 2D10.c:823): the module image values.
u_char Menu_Framework_statusViewNavigationMap[kMenuStatusViewElementCount]
                                             [kMenuStatusViewDirections];
// decomp: vs_mainMenu_strIntAgi (MENU/MAINMENU.PRG)
std::int16_t Menu_Framework_strIntAgi[kMenuPrimaryStatCount];
}
