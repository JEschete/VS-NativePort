#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "Items/ItemRecords.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Framework/ItemRows.hpp"
#include "Render/UiPackets.hpp"

// MAINMENU.PRG's equipment details (2D10.c, C48.c): the stats an item shows
// (class, affinity and type totals, DP/PP, strength/intelligence/agility,
// range and risk), the detail screen's element selection and its help texts,
// the class/affinity/type stat page, the primary stat panel and the DP/PP
// bars. The functions keep their state in an EquipmentDetailsState and reach
// the item rows, Ashley's equipment, the battle actors, the input, the menu
// rows and the draws through an EquipmentDetailsHost
// (EquipmentDetailsLive.hpp binds both).
namespace VS::Menu::Framework {
// vs_mainMenu_equipmentSubtype's bits.
namespace equipment_subtype {
inline constexpr std::uint8_t kWeapon = 1;
inline constexpr std::uint8_t kBlade = 2;
inline constexpr std::uint8_t kGrip = 4;
inline constexpr std::uint8_t kShield = 8;
inline constexpr std::uint8_t kArmor = 16;
inline constexpr std::uint8_t kAccessory = 32;
inline constexpr std::uint8_t kGem = 64;
}  // namespace equipment_subtype

// vs_mainMenu_statPage and vs_mainMenu_statPageEnabled (58EC.h:337,
// C48.h:77).
namespace stat_page {
inline constexpr std::uint8_t kClass = 0;
inline constexpr std::uint8_t kAffinity = 1;
inline constexpr std::uint8_t kType = 2;
inline constexpr std::uint8_t kNone = 3;
inline constexpr std::uint8_t kClassEnabled = 1;
inline constexpr std::uint8_t kAffinityEnabled = 2;
inline constexpr std::uint8_t kTypeEnabled = 4;
inline constexpr std::uint8_t kAllEnabled = 7;
}  // namespace stat_page

// vs_mainMenu_renderDpPpBars' flags (C48.h:97, C48.h:104): the target in
// bits 0-1 (DP, PP), the state from bit 2 (slide in, slide out, static).
namespace dp_pp_bars {
inline constexpr int kDp = 1;
inline constexpr int kPp = 2;
inline constexpr int kBoth = 3;
inline constexpr int kSlideOut = 1 << 2;
inline constexpr int kStatic = 2 << 2;
}  // namespace dp_pp_bars

inline constexpr std::size_t kEquipmentStatCount = 64;
inline constexpr std::size_t kPrimaryStatCount = 8;
inline constexpr std::size_t kComparisonStatCount = 4;
inline constexpr std::size_t kRangeRiskBytes = 8;
inline constexpr std::size_t kStatusViewElementCount = 21;
inline constexpr std::size_t kStatusViewDirections = 4;
// Strength, intelligence and agility.
inline constexpr std::size_t kPrimaryStats = 3;
// The current and maximum DP and PP.
inline constexpr std::size_t kDpPpValues = 4;
// The detail screen's first item row (row 0 is the item itself).
inline constexpr int kFirstDetailElement = 9;

// A value per detail screen element.
template <typename T>
using StatusViewTable = std::array<T, kStatusViewElementCount>;
// The element up, down, left and right of an element.
using StatusViewMoves = std::array<std::uint8_t, kStatusViewDirections>;
using StatusViewCursorPositions = StatusViewTable<std::uint32_t>;
using StatusViewNavigation = StatusViewTable<StatusViewMoves>;
// The setters' values: range, risk, an unread byte and whether they show;
// strength, intelligence, agility and whether they are known; the current
// and maximum DP and PP.
using RangeRiskValues = std::array<int, kRangeRiskBytes / 2>;
using PrimaryStatValues = std::array<int, kPrimaryStats + 1>;
using DpPpValues = std::array<int, kDpPpValues>;

// decomp: vs_mainMenu_statusViewCursorPositions' module image values
// (2D10.c:830).
inline constexpr StatusViewCursorPositions kStatusViewCursorPositions{
    PackXy(66, 28),   PackXy(-4, 28),   PackXy(0, 50),    PackXy(0, 66),
    PackXy(0, 82),    PackXy(0, 98),    PackXy(0, 114),   PackXy(0, 130),
    PackXy(0, 146),   PackXy(141, 10),  PackXy(148, 26),  PackXy(148, 42),
    PackXy(155, 58),  PackXy(155, 74),  PackXy(155, 90),  PackXy(180, 24),
    PackXy(192, 126), PackXy(192, 136), PackXy(152, 146), PackXy(152, 156),
    PackXy(152, 166),
};

// decomp: vs_mainMenu_statusViewNavigationMap's module image values
// (2D10.c:823): the element up, down, left and right of each element.
inline constexpr StatusViewNavigation kStatusViewNavigation{{
    {0, 2, 1, 10},   {1, 2, 1, 0},    {1, 3, 2, 11},   {2, 4, 3, 12},
    {3, 5, 4, 13},   {4, 6, 5, 14},   {5, 7, 6, 16},   {6, 8, 7, 17},
    {7, 8, 8, 18},   {9, 10, 0, 9},   {9, 11, 0, 10},  {10, 12, 2, 11},
    {11, 13, 3, 12}, {12, 14, 4, 13}, {13, 15, 5, 14}, {14, 16, 5, 15},
    {15, 17, 6, 16}, {16, 18, 7, 17}, {17, 19, 8, 18}, {18, 20, 8, 19},
    {19, 20, 8, 20},
}};

// The equipment details' variables. The first group is shared with the
// copied menus (EquipmentDetailsLiveState.h); the rest were module statics.
struct EquipmentDetailsState {
  // vs_mainMenu_equipmentStats: the class (0-15) and affinity (16-31)
  // totals, the types (32-35) and the damage type (36); entries 48-62 hold a
  // comparison and 63 enables it.
  std::array<std::int16_t, kEquipmentStatCount> stats{};
  // vs_mainMenu_strIntAgi: strength, intelligence, agility and whether the
  // values are known, then the totals they compare with.
  std::array<std::int16_t, kPrimaryStatCount> primary{};
  // _rangeRiskData: range, risk, an unread byte and whether they show.
  std::array<std::uint8_t, kRangeRiskBytes> range_risk{};
  // D_80102488: the actor's base minus total strength, intelligence and
  // agility, then the actor id.
  std::array<std::int16_t, kComparisonStatCount> comparison_deltas{};
  // D_80102490 and D_80102498.
  std::array<std::int16_t, kComparisonStatCount> comparison_totals{};
  std::array<std::int16_t, kComparisonStatCount> comparison_accessory{};
  std::uint8_t subtype = 0;
  // vs_mainMenu_currentStatusViewItem: the 1-based item shown.
  std::uint8_t item = 0;
  std::uint8_t enabled_pages = 0;
  std::uint8_t page = 0;
  // vs_mainMenu_selectedStatusViewElement.
  std::uint8_t selected = 0;
  StatusViewCursorPositions cursor_positions = kStatusViewCursorPositions;
  StatusViewNavigation navigation = kStatusViewNavigation;

  // _currentDp, _maxDp (in hundreds, rounded up), _currentPp and _maxPp.
  std::uint16_t current_dp = 0;
  std::uint16_t max_dp = 0;
  std::uint16_t current_pp = 0;
  std::uint16_t max_pp = 0;
  // _uIElement15Enabled: never set by the game.
  std::uint8_t element15_enabled = 0;
  // dpPpBarAnimState and vs_mainMenu_renderDpPpBars' state and target.
  std::uint16_t bars_animation = 0;
  std::uint8_t bars_state = 1;
  std::uint8_t bars_target = 0;
  // vs_mainMenu_renderStatusView's cursor step and previous item.
  std::uint8_t cursor_step = 0;
  std::uint8_t previous_item = 0;
  // vs_mainMenu_drawClassAffinityType's direction and slide step.
  std::int32_t panel_direction = 0;
  std::uint8_t panel_slide = 0;
  // vs_mainMenu_renderEquipStats' slide step and animation.
  std::uint8_t primary_slide = 0;
  std::uint8_t primary_animation = 0;
};

// What the comparison reads of a battle actor's statistics.
struct ActorComparisonStats {
  // The accessory's current strength, intelligence and agility.
  std::array<std::int16_t, kPrimaryStats> accessory{};
  // Strength, intelligence and agility, and their equipment totals.
  std::array<std::uint16_t, kPrimaryStats> base{};
  std::array<std::uint16_t, kPrimaryStats> totals{};
  bool analyzed = false;
  // The actor's current range and its weapon's.
  std::uint8_t current_range = 0;
  std::uint8_t weapon_range = 0;
};

// vs_getRGB888 colours of the stat texts.
inline constexpr std::uint32_t kStatTextGray = 0x808080;
inline constexpr std::uint32_t kStatTextDim = 0x404040;
inline constexpr std::uint32_t kStatTextGain = 0x804020;
inline constexpr std::uint32_t kStatTextLoss = 0x204080;

// The ordering-table entries the draws link into, from the scene table.
namespace detail_layer {
inline constexpr int kStatPage = -3;
inline constexpr int kButtonIcon = -4;
inline constexpr int kPrimaryPanel = -6;
inline constexpr int kHeaderGlyphs = -7;
}  // namespace detail_layer

class EquipmentDetailsHost {
 public:
  virtual ~EquipmentDetailsHost() = default;
  virtual void Refuse(std::string_view what) = 0;
  // The item tables, texts and string buffer the detail rows format with.
  [[nodiscard]] virtual ItemRowsHost& Rows() = 0;
  // Ashley's equipped weapon, shield and limb armour; nothing before her
  // statistics are loaded or for a limb past five.
  [[nodiscard]] virtual std::optional<Items::WeaponLoadout>
  EquippedWeapon() = 0;
  [[nodiscard]] virtual std::optional<Items::ShieldLoadout>
  EquippedShield() = 0;
  [[nodiscard]] virtual std::optional<Items::ArmorLoadout> EquippedArmor(
      int limb) = 0;
  // vs_battle_applyArmor over an inventory record.
  [[nodiscard]] virtual std::optional<Items::ArmorLoadout>
  InventoryArmorLoadout(const Items::InventoryArmor& armor) = 0;
  // Battle actor `actor`'s statistics; nothing for an absent actor.
  [[nodiscard]] virtual std::optional<ActorComparisonStats> ActorStats(
      int actor) = 0;
  [[nodiscard]] virtual Buttons Input() = 0;
  [[nodiscard]] virtual std::span<const std::uint8_t> RowAnimationSteps() = 0;
  virtual void PlaySound(MenuSound sound) = 0;
  // The stat page turn's sound effect.
  virtual void PlayPageSound() = 0;
  virtual void SetRowSelected(int row, bool selected) = 0;
  virtual void SetInformationMessage(TextRef text) = 0;
  // Returns the cursor's next animation step.
  virtual int DrawCursor(std::uint32_t step, std::uint32_t position) = 0;
  // Packets linked in front of the scene table's entry `slot`.
  [[nodiscard]] virtual Render::PacketQueue& SceneQueue(int slot) = 0;
  // The number draws return the position after the number.
  virtual std::uint32_t DrawComparedInteger(int value, std::uint32_t xy,
                                            int threshold, int slot) = 0;
  virtual void DrawColoredInteger(int value, std::uint32_t xy,
                                  std::uint32_t color, int slot) = 0;
  virtual void DrawNeutralInteger(int value, std::uint32_t xy, int slot) = 0;
  // `text` is a NUL-terminated literal.
  virtual void DrawCompactText(const char* text, std::uint32_t xy,
                               std::uint32_t color, int slot) = 0;
  virtual std::uint32_t DrawRightAlignedInteger(int font, std::uint32_t xy,
                                                int value, int slot) = 0;
  virtual void DrawStatGauge(int color, int fill_width, std::uint32_t xy,
                             int slot) = 0;
  virtual void DrawButtonPromptBackground(int x, int y, int w, int h) = 0;
  virtual void DrawButtonIcon(int button, int x, int y, int slot) = 0;
  virtual void SetFontStyle(int palette_offset) = 0;
  // Returns the x after the glyph.
  virtual int DrawGlyph(std::uint8_t glyph, int x, int y, int slot) = 0;
};

struct EquipmentDetailsContext {
  EquipmentDetailsState& state;
  EquipmentDetailsHost& host;
};

// MAINMENU.PRG 2D10.c: the stats of the 1-based item of each table (0 for
// none where the original allowed it).
void ResetEquipmentStats(EquipmentDetailsState& state);
void SetWeaponStats(EquipmentDetailsContext& ctx, int weapon);
void SetBladeStats(EquipmentDetailsContext& ctx, int blade);
void SetGripStats(EquipmentDetailsContext& ctx, int grip);
void SetShieldStats(EquipmentDetailsContext& ctx, int shield);
void SetArmorStats(EquipmentDetailsContext& ctx, int armor);
void SetGemStats(EquipmentDetailsContext& ctx, int gem);

// The detail rows' descriptions: the weapon's (row 0) and its parts' (rows
// 1-5), the blade's, grip's, shield's, armour's and gem's; nothing when the
// record is missing or its row sets no description.
std::optional<TextRef> SelectWeaponDetailRow(EquipmentDetailsContext& ctx,
                                             int row);
std::optional<TextRef> BladeDetailDescription(EquipmentDetailsContext& ctx);
std::optional<TextRef> GripDetailDescription(EquipmentDetailsContext& ctx);
std::optional<TextRef> SelectShieldDetailRow(EquipmentDetailsContext& ctx,
                                             int row);
std::optional<TextRef> ArmorDetailDescription(EquipmentDetailsContext& ctx);
std::optional<TextRef> GemDetailDescription(EquipmentDetailsContext& ctx);

void ResetItemDetailCursor(EquipmentDetailsState& state);
[[nodiscard]] bool IsItemDetailElementReachable(EquipmentDetailsContext& ctx,
                                                int element);
// One frame of the detail screen: the selection, the stats it shows, its
// help text and the cursor.
void UpdateItemDetails(EquipmentDetailsContext& ctx);

// MAINMENU.PRG C48.c.
[[nodiscard]] std::uint16_t OffsetStatBarCoordinate(int panel_x,
                                                    int scanline_offset,
                                                    int edge_base_x);
void DrawEquipmentStatPage(EquipmentDetailsContext& ctx, int x_offset);
[[nodiscard]] std::optional<std::uint8_t> NextEnabledStatPage(
    const EquipmentDetailsState& state, int page);
void SetEnabledStatPages(EquipmentDetailsContext& ctx, int enabled);
// A positive command opens the stat page panel with those pages enabled, a
// negative one closes it; 0 animates and draws it.
void UpdateEquipmentStatPages(EquipmentDetailsContext& ctx, int command);
void SetRangeAndRisk(EquipmentDetailsState& state,
                     const RangeRiskValues& range_risk);
void SetPrimaryStats(EquipmentDetailsState& state,
                     const PrimaryStatValues& primary);
void SetActorComparisonStats(EquipmentDetailsContext& ctx, int actor);
// A nonzero `animate` starts the panel's slide (1 in, otherwise out); 0
// animates and draws it.
void UpdatePrimaryStatPanel(EquipmentDetailsContext& ctx, int animate);
void SetDpPp(EquipmentDetailsState& state, const DpPpValues& dp_pp);
// Nonzero flags set the bars' state and target; 0 animates and draws them.
void UpdateDpPpBars(EquipmentDetailsContext& ctx, int flags);
}  // namespace VS::Menu::Framework
