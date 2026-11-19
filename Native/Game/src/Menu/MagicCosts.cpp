// Hand-maintained (tools/port/owned.csv).
#include "Menu/MagicCosts.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string_view>

#include "Core/Math/ScalarMath.hpp"

namespace VS::Menu::Magic {
namespace {
constexpr std::uint32_t kActionRestrictionMask = 0xB7;
constexpr std::uint32_t kCostDisabledFlag = 1U << 1;
constexpr std::int32_t kTeleportRowInfo = (2 << 8) | 11;
constexpr std::int32_t kRowDisabled = 1;
constexpr std::int32_t kRowCurrentSavePoint = 4;
constexpr std::uint8_t kSavePointUndiscovered = 0;
constexpr std::uint8_t kSavePointDisabled = 2;
constexpr int kVisibleRows = 8;
constexpr std::uint8_t kCenteredCursorRow = 3;
constexpr int kCostLabelIndex = 1;
constexpr int kCostValueIndex = 0;
constexpr int kCostLabelX = 8;
constexpr int kCostValueX = 72;
constexpr int kVariableCostXStep = 4;
constexpr int kFlyOutRow = 4;
constexpr int kFlyOutOffset = 1;
constexpr int kNoMenuRow = 40;
constexpr int kKeepHeaderRow = 0;
constexpr int kMenuCommandNone = 0;
constexpr int kMenuCommandMenu = 2;
constexpr int kMenuSelectionBack = -1;
constexpr std::int32_t kSelectionPending = 0;
constexpr std::size_t kFormatBufferSize = 16;
constexpr std::size_t kVariableCostStart = 13;
constexpr int kPackedDigitBits = 4;
constexpr int kPackedDigitMask = 0xF;
constexpr std::uint32_t kLocationMask = 0x1FF;
constexpr std::uint32_t kTileMask = 0x1F;
constexpr int kTileXShift = 16;
constexpr int kTileZShift = 24;
constexpr int kPackedTileXShift = 9;
constexpr int kPackedTileZShift = 14;
constexpr int kPackedCostShift = 19;
constexpr std::uint32_t kRoomIdMask = 0xFFFFU;
constexpr MenuItem kHeaderItem{
    .row = 0,
    .x = 320,
    .y = 18,
    .width = 0x8C,
    .background_width = 8,
    .text_offset = 0,
    .state = 2,
    .target_position = 180,
    .selected = 1,
};

// decomp: _savePointData (src/MENU/MENU0.PRG/84.c:95).
constexpr std::array<std::uint32_t, kSavePointCount> kOriginalSavePointWords{
    0x0305001B, 0x0801000E, 0x04030199, 0x0C0C001C, 0x070C0028, 0x040200CD,
    0x0F070035, 0x08060093, 0x060000CE, 0x09080106, 0x09080111, 0x0C030098,
    0x090800E0, 0x060900C8, 0x050800CA, 0x050F00CC, 0x0109009B, 0x0A07007C,
    0x000400CF, 0x080800EC, 0x020400F1, 0x0705011D, 0x06080120, 0x04080139,
    0x04080147, 0x0904014F, 0x010300A2, 0x150C00A3, 0x030200D0, 0x060600D1,
    0x1007008B, 0x010400AB, 0x05020055, 0x07060063, 0x0A0400D2, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
};

// decomp: _getTeleportCost savePointCosts (src/MENU/MENU0.PRG/84.c:126). The
// MP each save-point interval adds; the last entry closes the table.
constexpr std::array<std::uint8_t, kSavePointCount> kSavePointIntervalCosts{
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0,
};

constexpr SavePoint Decode(std::uint32_t word) noexcept {
  const std::uint32_t destination = word + 1U;
  return {
      .room_id = static_cast<std::uint16_t>(word & kRoomIdMask),
      .location = static_cast<std::uint16_t>(destination & kLocationMask),
      .tile_x =
          static_cast<std::uint8_t>((destination >> kTileXShift) & kTileMask),
      .tile_z =
          static_cast<std::uint8_t>((destination >> kTileZShift) & kTileMask),
  };
}

constexpr SavePointTable DecodeTable() noexcept {
  SavePointTable table{};
  for (std::size_t index = 0; index < kSavePointCount; ++index) {
    table[index] = Decode(kOriginalSavePointWords[index]);
  }
  return table;
}

constexpr SavePointTable kOriginalSavePoints = DecodeTable();

// _setMPCost/_setMPCostDirect's right-aligned "#<digits>" text; a variable
// cost interleaves '[' and ends in "[-".
std::string_view FormatCost(int cost, bool variable,
                            std::array<char, kFormatBufferSize>& buffer) {
  std::size_t index = buffer.size() - 1;
  buffer[index] = '\0';
  if (variable) {
    index = kVariableCostStart;
    buffer[kVariableCostStart + 1] = '-';
    buffer[kVariableCostStart] = '[';
  }
  int remaining = cost;
  do {
    const int packed = VS::Core::Math::SplitDecimalDigit(remaining);
    buffer[--index] = static_cast<char>((packed & kPackedDigitMask) + '0');
    remaining = packed >> kPackedDigitBits;
    if (variable) {
      buffer[--index] = '[';
    }
  } while (remaining != 0);
  buffer[--index] = '#';
  return {buffer.data() + index, buffer.size() - index - 1};
}

bool ReadString(Host& host, std::span<const std::uint16_t> strings,
                std::size_t index, std::uint16_t& offset) {
  if (index >= strings.size() || strings[index] >= strings.size()) {
    host.Refuse("Magic teleport string offset exceeds its halfword table");
    return false;
  }
  offset = strings[index];
  return true;
}

// The teleport rows for the discovered save points; false (after a refusal)
// when the string or state tables cannot describe them.
bool BuildTeleportRows(TeleportMenuState& state, Host& host,
                       std::array<TeleportRow, kSavePointCount>& rows,
                       std::size_t& row_count, int& nearest_row) {
  const std::span<const std::uint8_t> states = host.SavePointStates();
  if (states.size() < kSavePointCount) {
    host.Refuse("Magic teleport save-point states are unavailable");
    return false;
  }
  const std::span<const std::uint16_t> strings = host.TeleportationStrings();
  if (host.DisabledDescriptionOffset() >= strings.size()) {
    host.Refuse("Magic teleport disabled description exceeds its table");
    return false;
  }
  const SavePointTable& save_points = host.SavePoints();
  const int room_id = host.CurrentRoomId();
  row_count = 0;
  for (std::size_t index = 0; index < kSavePointCount; ++index) {
    const std::uint8_t discovered = states[index];
    if (discovered == kSavePointUndiscovered) {
      continue;
    }
    const std::size_t name_index = host.LocationsIndex() + index * 2;
    TeleportRow& row = rows[row_count];
    if (!ReadString(host, strings, name_index, row.name_offset)) {
      return false;
    }
    if (discovered == kSavePointDisabled) {
      row.description_offset = host.DisabledDescriptionOffset();
    } else if (!ReadString(host, strings, name_index + 1,
                           row.description_offset)) {
      return false;
    }
    const std::optional<int> cost =
        TeleportMpCost(static_cast<int>(index), room_id, save_points);
    const auto mp_cost = static_cast<std::uint16_t>(cost.value_or(0));
    state.mp_costs[row_count] = mp_cost;
    row.type = static_cast<std::int32_t>(discovered == kSavePointDisabled) |
               static_cast<std::int32_t>(state.current_mp < mp_cost);
    if (mp_cost == kTeleportBaseMpCost) {
      row.type |= kRowCurrentSavePoint;
      nearest_row = static_cast<int>(row_count);
    }
    state.save_points[row_count] = static_cast<std::uint8_t>(index);
    ++row_count;
  }
  return true;
}

void RememberTeleportCursor(int row_count, int nearest_row, Host& host) {
  if (row_count <= kVisibleRows || nearest_row < kVisibleRows) {
    host.SetTeleportCursorMemory(static_cast<std::uint8_t>(nearest_row), 0);
  } else if (nearest_row >= row_count - (kVisibleRows - 1)) {
    host.SetTeleportCursorMemory(
        static_cast<std::uint8_t>(nearest_row - (row_count - kVisibleRows)),
        static_cast<std::uint8_t>(row_count - kVisibleRows));
  } else {
    host.SetTeleportCursorMemory(
        kCenteredCursorRow,
        static_cast<std::uint8_t>(nearest_row - kCenteredCursorRow));
  }
}

void OpenTeleportList(TeleportMenuState& state, Host& host) {
  if (!host.RowsIdle()) {
    return;
  }
  std::array<TeleportRow, kSavePointCount> rows{};
  std::size_t row_count = 0;
  // When no row costs 15, the original reads nearestSavePoint from an
  // uninitialized stack slot (sp+0x250, read at 0x8010403C), so hardware uses
  // stale stack data. Row 0 is a defined choice (#399).
  int nearest_row = 0;
  if (!BuildTeleportRows(state, host, rows, row_count, nearest_row)) {
    return;
  }
  if ((host.ActionRestrictions() & kActionRestrictionMask) != 0) {
    for (TeleportRow& row : rows) {
      row.type |= kRowDisabled;
    }
  }
  RememberTeleportCursor(static_cast<int>(row_count), nearest_row, host);
  const std::uint8_t cursor_memory = host.CursorMemorySetting();
  host.SetCursorMemorySetting(1);
  host.CreateRows(kTeleportRowInfo,
                  std::span<const TeleportRow>(rows.data(), row_count));
  state.step = TeleportStep::kHandleInput;
  host.SetCursorMemorySetting(cursor_memory);
}

void HandleTeleportInput(TeleportMenuState& state, Host& host) {
  state.selected_row = VS::Core::Math::WrappingAdd(host.SelectedRow(), 1);
  if (state.selected_row != kSelectionPending) {
    host.SetDisplayActionCost(0);
    if (state.selected_row == kMenuSelectionBack) {
      host.ClearRowsExcept(kKeepHeaderRow);
    } else {
      host.ClearRowsExcept(kNoMenuRow);
      host.DismissInformationBox();
      host.SetCommandLabel(kMenuCommandNone);
    }
    state.step = TeleportStep::kReturnIfReady;
    return;
  }
  const int highlighted = host.HighlightedRow();
  if (highlighted < 0 || highlighted >= static_cast<int>(kSavePointCount)) {
    host.Refuse("Magic teleport highlighted row exceeds the row table");
    return;
  }
  const int cost = state.mp_costs[static_cast<std::size_t>(highlighted)];
  SetTeleportMpCost(cost, static_cast<int>(state.current_mp < cost), host);
}

TeleportMenuResult ReturnTeleportSelection(const TeleportMenuState& state,
                                           Host& host) {
  if (!host.RowsIdle()) {
    return {};
  }
  if (state.selected_row <= 0) {
    return {.selection = state.selected_row, .destination = std::nullopt};
  }
  if (state.selected_row > static_cast<int>(kSavePointCount)) {
    host.Refuse("Magic teleport selection exceeds the row table");
    return {.selection = kMenuSelectionBack, .destination = std::nullopt};
  }
  const auto row = static_cast<std::size_t>(state.selected_row - 1);
  const std::uint8_t save_point = state.save_points[row];
  if (save_point >= kSavePointCount) {
    host.Refuse("Magic teleport save point exceeds the save-point table");
    return {.selection = kMenuSelectionBack, .destination = std::nullopt};
  }
  const SavePoint& target = host.SavePoints()[save_point];
  return {
      .selection = state.selected_row,
      .destination =
          TeleportDestination{
              .save_point = save_point,
              .location = target.location,
              .tile_x = target.tile_x,
              .tile_z = target.tile_z,
              .mp_cost = state.mp_costs[row],
          },
  };
}
}  // namespace

SavePoint DecodeSavePoint(std::uint32_t word) noexcept { return Decode(word); }

const SavePointTable& OriginalSavePoints() noexcept {
  return kOriginalSavePoints;
}

// decomp: MENU0.PRG _teleportMenu result packing
// (src/MENU/MENU0.PRG/15AC.c:130).
std::int32_t PackTeleportDestination(
    const TeleportDestination& destination) noexcept {
  const std::uint32_t packed =
      (destination.location & kLocationMask) |
      ((destination.tile_x & kTileMask) << kPackedTileXShift) |
      ((destination.tile_z & kTileMask) << kPackedTileZShift) |
      (static_cast<std::uint32_t>(destination.mp_cost) << kPackedCostShift);
  return static_cast<std::int32_t>(packed);
}

// decomp: D_800F18EC_t executeAbility bits
// (src/BATTLE/BATTLE.PRG/5BF94.h:160).
TeleportDestination UnpackTeleportDestination(std::int32_t packed) noexcept {
  const auto word = static_cast<std::uint32_t>(packed);
  return TeleportDestination{
      .save_point = 0,
      .location = static_cast<std::uint16_t>(word & kLocationMask),
      .tile_x =
          static_cast<std::uint8_t>((word >> kPackedTileXShift) & kTileMask),
      .tile_z =
          static_cast<std::uint8_t>((word >> kPackedTileZShift) & kTileMask),
      .mp_cost = static_cast<std::uint16_t>(word >> kPackedCostShift),
  };
}

// decomp: MENU0.PRG _setMPCost (src/MENU/MENU0.PRG/84.c:18).
void SetSpellMpCost(int signed_action_id, Host& host) {
  if (signed_action_id == std::numeric_limits<int>::min()) {
    host.Refuse("Magic spell action id cannot be negated");
    return;
  }
  const bool variable = signed_action_id < 0;
  const int action_id = variable ? -signed_action_id : signed_action_id;
  if (action_id >= static_cast<int>(kActionCount)) {
    host.Refuse("Magic spell action id exceeds the action table");
    return;
  }
  const std::optional<std::uint32_t> flags =
      host.ActionRestrictionFlags(action_id);
  const std::optional<std::uint8_t> cost = host.ActionCost(action_id);
  if (!flags || !cost) {
    host.Refuse("Magic spell cost or restrictions are unavailable");
    return;
  }
  const int disabled = static_cast<int>((*flags & kCostDisabledFlag) >> 1);
  host.SetActionCostText(kCostLabelIndex, "MP", kCostLabelX, disabled);
  std::array<char, kFormatBufferSize> buffer{};
  host.SetActionCostText(
      kCostValueIndex, FormatCost(*cost, variable, buffer),
      (static_cast<int>(variable) * kVariableCostXStep) + kCostValueX,
      disabled);
}

// decomp: MENU0.PRG _setMPCostDirect (src/MENU/MENU0.PRG/84.c:64).
void SetTeleportMpCost(int mp_cost, int disabled, Host& host) {
  if (mp_cost < 0 || mp_cost > kMaxFormattedMpCost) {
    host.Refuse("Magic teleport MP cost is outside the formatted range");
    return;
  }
  host.SetActionCostText(kCostLabelIndex, "MP", kCostLabelX, disabled);
  std::array<char, kFormatBufferSize> buffer{};
  host.SetActionCostText(kCostValueIndex, FormatCost(mp_cost, false, buffer),
                         kCostValueX, disabled);
}

// decomp: MENU0.PRG _getTeleportCost (src/MENU/MENU0.PRG/84.c:117).
std::optional<int> TeleportMpCost(int target_save_point, int current_room_id,
                                  const SavePointTable& save_points) {
  if (target_save_point < 0 ||
      target_save_point > static_cast<int>(kSavePointCount)) {
    return std::nullopt;
  }
  int current = static_cast<int>(kSavePointCount);
  for (std::size_t index = 0; index < kSavePointCount; ++index) {
    if (static_cast<int>(save_points[index].room_id) == current_room_id) {
      current = static_cast<int>(index);
      break;
    }
  }
  const int first = target_save_point < current ? target_save_point : current;
  const int last = target_save_point < current ? current : target_save_point;
  int cost = kTeleportBaseMpCost;
  for (int index = first; index < last; ++index) {
    cost += kSavePointIntervalCosts[static_cast<std::size_t>(index)];
  }
  return cost;
}

// decomp: MENU0.PRG _teleportMenu (src/MENU/MENU0.PRG/15AC.c:22).
TeleportMenuResult UpdateTeleportMenu(int init, TeleportMenuState& state,
                                      Host& host) {
  if (init != 0) {
    host.FlyOutRowsAndHoistSelection(kFlyOutRow, kFlyOutOffset);
    state.step = TeleportStep::kInit;
    const std::optional<std::int16_t> mp = host.CurrentMp();
    if (!mp) {
      host.Refuse("Magic teleport current MP is unavailable");
    }
    state.current_mp = mp.value_or(0);
    return {};
  }
  switch (state.step) {
    case TeleportStep::kInit:
      OpenTeleportList(state, host);
      return {};
    case TeleportStep::kHandleInput:
      HandleTeleportInput(state, host);
      return {};
    case TeleportStep::kReturnIfReady:
      return ReturnTeleportSelection(state, host);
  }
  return {};
}

// decomp: MENU0.PRG _setMenuHeader (src/MENU/MENU0.PRG/15AC.c:148).
void CreateHeader(Host& host) {
  const std::span<const std::uint16_t> strings = host.BattleMenuStrings();
  const std::size_t index = host.MagicHeaderIndex();
  if (index >= strings.size() || strings[index] >= strings.size()) {
    host.Refuse("Magic header string offset exceeds the battle menu strings");
    return;
  }
  MenuItem item = kHeaderItem;
  item.text_offset = strings[index];
  host.SetMenuItem(item);
  host.SetCommandLabel(kMenuCommandMenu);
  host.InitializeInformationBox();
}
}  // namespace VS::Menu::Magic
