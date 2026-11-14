// Hand-maintained (tools/port/owned.csv).
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>

#include "Core/Math/ScalarMath.hpp"
#include "Menu/BreakArts.hpp"

namespace VS::Menu::break_arts {
namespace {
constexpr std::uint32_t kActionRestrictionMask = 0x15F;
constexpr std::int32_t kArtRowInfo = 0x200;
constexpr std::int32_t kCategoryRowInfo = 0x216;
constexpr std::int32_t kCategoryIconStep = 0x04000000;
constexpr std::int32_t kCategoryDisabled = 1;
constexpr std::int32_t kCategoryEquipped = 4;
constexpr int kActionNameCount = 256;
constexpr int kActionMenuCommandNone = 0;
constexpr int kActionMenuCommandMenu = 2;
constexpr int kMenuCenterX = 320;
constexpr int kNoMenuRow = 40;
constexpr int kHeaderRow = 1;
constexpr int kTitleRow = 10;
constexpr int kCostLabelIndex = 1;
constexpr int kCostValueIndex = 0;
constexpr int kCostLabelX = 8;
constexpr int kCostValueX = 72;
constexpr int kHeaderX = kMenuCenterX;
constexpr int kHeaderY = 18;
constexpr int kHeaderWidth = 0x8C;
constexpr int kArtTitleY = 34;
constexpr int kArtTitleWidth = 0x7E;
constexpr int kMenuItemBackgroundWidth = 8;
constexpr int kArtTitleAnimationIndex = 10;
constexpr std::uint32_t kNextPointsTextOffset = 96;
constexpr std::size_t kFormatBufferSize = 16;
constexpr std::string_view kPointsSuffix = "PT";
constexpr int kPackedDigitBits = 4;
constexpr int kPackedDigitMask = 0xF;
constexpr int kPackedCoordinateBits = 16;
constexpr int kWeaponTypeMask = 0xF;
constexpr std::uint32_t kPackedCoordinateMask = 0xFFFFU;
constexpr int kForceCursorMemoryFlag = 0x10;
constexpr int kBreakArtExecuteType = 5;
constexpr int kBreakArtSelectionKind = 6;
constexpr int kBattleMenu = 2;
constexpr int kMenuSelectionConfirm = 0;
constexpr int kMenuSelectionQuit = -2;
constexpr int kMenuSelectionBack = 0xFFFF;

bool ValidMenuOffset(Host& host, std::uint16_t offset) {
  const std::span<const std::uint16_t> strings = host.MenuStrings();
  if (offset < strings.size()) {
    return true;
  }
  host.Refuse("Break Arts menu string offset exceeds its halfword table");
  return false;
}

bool ValidText(Host& host, TextRef text) {
  switch (text.kind) {
    case TextKind::kMenuOffset:
      return ValidMenuOffset(host, text.value);
    case TextKind::kActionName:
      if (text.value < kActionNameCount &&
          host.ActionNameAvailable(text.value)) {
        return true;
      }
      host.Refuse("Break Arts action name is unavailable");
      return false;
    case TextKind::kBattleMenuIndex: {
      const std::span<const std::uint16_t> strings = host.BattleMenuStrings();
      if (text.value < strings.size() && strings[text.value] < strings.size()) {
        return true;
      }
      host.Refuse("Break Arts header string index exceeds its halfword table");
      return false;
    }
  }
  host.Refuse("Break Arts text reference has an unknown kind");
  return false;
}

bool MenuOffsetAt(Host& host, std::size_t index, std::uint16_t& offset) {
  const std::span<const std::uint16_t> strings = host.MenuStrings();
  if (index >= strings.size() || strings[index] >= strings.size()) {
    host.Refuse("Break Arts text index or offset exceeds its halfword table");
    return false;
  }
  offset = strings[index];
  return true;
}

bool SetMenuItem(Host& host, const RowPlacement& placement, TextRef text) {
  if (!ValidText(host, text)) {
    return false;
  }
  if (!host.SetMenuItem(placement, text)) {
    host.Refuse("Break Arts menu item could not be created");
    return false;
  }
  return true;
}

bool CreateRows(Host& host, int row_info, std::span<const Row> rows) {
  for (const Row& row : rows) {
    if (!ValidText(host, row.name) || !ValidText(host, row.description)) {
      return false;
    }
  }
  if (!host.CreateRows(row_info, rows)) {
    host.Refuse("Break Arts menu rows could not be created");
    return false;
  }
  return true;
}

bool ReadActionUnlocked(Host& host, int action_id, bool& unlocked) {
  if (action_id < 0 || action_id >= kActionNameCount) {
    host.Refuse("Break Arts action id exceeds the action table");
    return false;
  }
  const std::optional<bool> result = host.ActionUnlocked(action_id);
  if (!result) {
    host.Refuse("Break Arts action table is unavailable");
    return false;
  }
  unlocked = *result;
  return true;
}

bool WeaponTypeIsValid(Host& host, int weapon_type) {
  if (weapon_type >= 1 && weapon_type <= static_cast<int>(kCategoryCount)) {
    return true;
  }
  host.Refuse("Break Arts weapon category is outside 1 through 10");
  return false;
}

std::string_view FormatCost(std::uint8_t cost,
                            std::array<char, kFormatBufferSize>& buffer) {
  std::size_t index = buffer.size() - 1;
  buffer[index] = '\0';
  int remaining = cost;
  do {
    const int packed = VS::Core::Math::SplitDecimalDigit(remaining);
    buffer[--index] = static_cast<char>((packed & kPackedDigitMask) + '0');
    remaining = packed >> kPackedDigitBits;
  } while (remaining != 0);
  buffer[--index] = '#';
  return {buffer.data() + index, buffer.size() - index - 1};
}

std::string_view FormatPoints(int points,
                              std::array<char, kFormatBufferSize>& buffer) {
  const std::size_t suffix_start = buffer.size() - kPointsSuffix.size() - 1;
  std::copy(kPointsSuffix.begin(), kPointsSuffix.end(),
            buffer.begin() + static_cast<std::ptrdiff_t>(suffix_start));
  buffer.back() = '\0';
  std::size_t index = suffix_start - 1;
  do {
    const int packed = VS::Core::Math::SplitDecimalDigit(points);
    buffer[index] = static_cast<char>((packed & kPackedDigitMask) + '0');
    points = packed >> kPackedDigitBits;
    --index;
  } while (points != 0);
  buffer[index] = '#';
  return {buffer.data() + index, kFormatBufferSize - 1 - index};
}

std::uint32_t UnlockPosition(int x_offset, int row) {
  const std::uint32_t x = static_cast<std::uint32_t>(x_offset + 206);
  const std::uint32_t y = static_cast<std::uint32_t>(row * 16 + 50);
  return (x & kPackedCoordinateMask) |
         ((y & kPackedCoordinateMask) << kPackedCoordinateBits);
}

bool BuildArtRows(Context& context, std::array<Row, kMaxArtRows>& rows,
                  std::size_t& row_count) {
  ArtListState& state = context.state.art_list;
  Host& host = context.host;
  const int weapon_type = state.weapon_type;
  if (!WeaponTypeIsValid(host, weapon_type)) {
    return false;
  }
  const std::optional<int> equipped = host.EquippedWeaponCategory();
  if (!equipped) {
    host.Refuse("Break Arts equipped weapon category is unavailable");
    return false;
  }
  const std::span<const std::uint16_t> strings = host.MenuStrings();
  row_count = 0;
  for (std::size_t art = 0; art < kArtCount; ++art) {
    const int action_id = host.DaggerArtOneId() +
                          (weapon_type - 1) * static_cast<int>(kArtCount) +
                          static_cast<int>(art);
    bool unlocked = false;
    if (!ReadActionUnlocked(host, action_id, unlocked)) {
      return false;
    }
    if (!unlocked) {
      continue;
    }
    const std::size_t description_index =
        static_cast<std::size_t>(host.DaggerArtDescriptionIndex()) + art +
        static_cast<std::size_t>(weapon_type - 1) * kArtCount;
    std::uint16_t description_offset = 0;
    if (description_index >= strings.size() ||
        !MenuOffsetAt(host, description_index, description_offset)) {
      return false;
    }
    const std::optional<std::uint32_t> restrictions =
        host.ActionRestrictionFlags(action_id);
    if (!restrictions) {
      host.Refuse("Break Arts action restrictions are unavailable");
      return false;
    }
    std::int32_t row_type = 0;
    if (weapon_type != *equipped || *restrictions != 0) {
      row_type = 1;
    }
    rows[row_count] = {
        .name = {TextKind::kActionName, static_cast<std::uint16_t>(action_id)},
        .description = {TextKind::kMenuOffset, description_offset},
        .type = row_type,
    };
    state.action_ids[row_count] = static_cast<std::uint16_t>(action_id);
    ++row_count;
  }
  return true;
}

int SelectedRowResult(Host& host) {
  const int selected = host.SelectedRow();
  if (selected == std::numeric_limits<int>::max()) {
    host.Refuse("Break Arts selected row cannot be incremented");
    return kMenuSelectionConfirm;
  }
  return selected + 1;
}
}  // namespace

// decomp: MENU1.PRG _setHPCost (src/MENU/MENU1.PRG/30.c:13).
void SetHpCost(int action_id, Host& host) {
  if (action_id < 0 || action_id >= kActionNameCount) {
    host.Refuse("Break Arts action id exceeds the action table");
    return;
  }
  const std::optional<std::uint32_t> flags =
      host.ActionRestrictionFlags(action_id);
  const std::optional<std::uint8_t> cost = host.ActionCost(action_id);
  if (!flags || !cost) {
    host.Refuse("Break Arts action cost or restrictions are unavailable");
    return;
  }
  const int disabled = static_cast<int>((*flags >> 1) & 1U);
  host.SetActionCostText(kCostLabelIndex, "HP", kCostLabelX, disabled);
  std::array<char, kFormatBufferSize> buffer{};
  host.SetActionCostText(kCostValueIndex, FormatCost(*cost, buffer),
                         kCostValueX, disabled);
}

// decomp: MENU1.PRG _setNextArtUnlockPointsRemaining
// (src/MENU/MENU1.PRG/30.c:45).
void DrawNextUnlockPoints(int x_offset, int weapon_category, int row,
                          Host& host) {
  if (weapon_category < 0 ||
      weapon_category > static_cast<int>(kCategoryCount) || row < 0 ||
      row > static_cast<int>(kMaxArtRows)) {
    host.Refuse("Break Arts unlock-progress category or row is invalid");
    return;
  }
  const int category = weapon_category % static_cast<int>(kCategoryCount);
  const std::optional<int> learned = host.ArtsLearned(category);
  if (!learned || *learned < 0 || *learned > static_cast<int>(kArtCount)) {
    host.Refuse("Break Arts learned-art count is unavailable or invalid");
    return;
  }
  if (*learned == static_cast<int>(kArtCount)) {
    return;
  }
  const std::optional<int> kills = host.CategoryKills(category);
  const std::optional<int> requirement =
      host.PointRequirement(category, *learned);
  if (!kills || !requirement) {
    host.Refuse("Break Arts unlock-progress data is unavailable");
    return;
  }
  const int points = *requirement > *kills ? *requirement - *kills : 0;
  const std::uint32_t position = UnlockPosition(x_offset, row);
  std::array<char, kFormatBufferSize> buffer{};
  const std::string_view points_text = FormatPoints(points, buffer);
  host.DrawCompactText("NEXT", position);
  host.DrawCompactText(points_text, position + kNextPointsTextOffset);
}

// decomp: MENU1.PRG _weaponArtsMenu (src/MENU/MENU1.PRG/30.c:104).
namespace {
int SetArtListTitle(int type_cursor_memory, Context& context) {
  ArtListState& state = context.state.art_list;
  Host& host = context.host;
  const int weapon_type = type_cursor_memory & kWeaponTypeMask;
  if (!WeaponTypeIsValid(host, weapon_type)) {
    return 0;
  }
  std::uint16_t title_offset = 0;
  const std::size_t title_index = static_cast<std::size_t>(weapon_type - 1) * 3;
  if (!MenuOffsetAt(host, title_index, title_offset)) {
    return 0;
  }
  state.is_last_row = false;
  state.animation_index = kArtTitleAnimationIndex;
  state.force_cursor_memory = type_cursor_memory >> kPackedDigitBits;
  state.weapon_type = weapon_type;
  if (!SetMenuItem(host,
                   {.row = kTitleRow,
                    .x = kMenuCenterX,
                    .y = kArtTitleY,
                    .width = kArtTitleWidth,
                    .background_width = kMenuItemBackgroundWidth,
                    .icon = weapon_type},
                   {TextKind::kMenuOffset, title_offset})) {
    return 0;
  }
  state.step = ArtListStep::kInit;
  return 0;
}

std::optional<int> InitializeArtListRows(Context& context) {
  ArtListState& state = context.state.art_list;
  Host& host = context.host;
  if (!host.ShortcutInvoked() && !host.RowsIdle()) {
    return std::nullopt;
  }
  if (!WeaponTypeIsValid(host, state.weapon_type)) {
    return 0;
  }
  std::array<Row, kMaxArtRows> rows{};
  std::size_t row_count = 0;
  if (!BuildArtRows(context, rows, row_count)) {
    return 0;
  }
  if ((host.ActionRestrictions() & kActionRestrictionMask) != 0) {
    for (std::size_t row = 0; row < row_count && row < kArtCount; ++row) {
      rows[row].type |= 1;
    }
  }
  if (!host.ShortcutInvoked()) {
    const std::uint16_t name = host.ViewArtsNameOffset();
    const std::uint16_t description = host.ViewArtsDescriptionOffset();
    if (!ValidMenuOffset(host, name) || !ValidMenuOffset(host, description) ||
        row_count >= kMaxArtRows) {
      if (row_count >= kMaxArtRows) {
        host.Refuse("Break Arts row count exceeds its five-row storage");
      }
      return 0;
    }
    rows[row_count] = {
        .name = {TextKind::kMenuOffset, name},
        .description = {TextKind::kMenuOffset, description},
        .type = 2,
    };
    state.action_ids[row_count] = kMenuSelectionBack;
    ++row_count;
  }
  const std::uint8_t cursor_memory = host.CursorMemory();
  if (state.force_cursor_memory != 0) {
    host.SetCursorMemory(1);
    state.force_cursor_memory = 0;
  }
  const bool created = CreateRows(host, (state.weapon_type + 11) | kArtRowInfo,
                                  std::span(rows).first(row_count));
  host.SetCursorMemory(cursor_memory);
  if (!created) {
    return 0;
  }
  state.step = ArtListStep::kWaitForSelection;
  state.row_count = static_cast<std::uint8_t>(row_count);
  state.remaining_rows = static_cast<std::uint8_t>(row_count);
  return std::nullopt;
}

std::optional<int> WaitForArtSelection(Context& context) {
  ArtListState& state = context.state.art_list;
  Host& host = context.host;
  if (state.remaining_rows != 0) {
    --state.remaining_rows;
  }
  state.is_last_row = state.remaining_rows == 0;
  state.selected_row = SelectedRowResult(host);
  host.SetDisplayActionCost(0);
  if (state.selected_row != kMenuSelectionConfirm) {
    state.is_last_row = false;
    if (state.selected_row > 0) {
      const std::size_t selected =
          static_cast<std::size_t>(state.selected_row - 1);
      if (selected >= state.row_count || selected >= state.action_ids.size()) {
        host.Refuse("Break Arts selected row exceeds the created rows");
        return 0;
      }
      state.selected_row = state.action_ids[selected];
    } else if (host.ShortcutInvoked()) {
      state.selected_row = kMenuSelectionQuit;
    }
    if (state.selected_row == kMenuSelectionBack) {
      host.ClearRowsExcept(kHeaderRow);
    } else {
      host.ClearRowsExcept(kNoMenuRow);
      host.DismissInformationBox();
      host.SetCommandLabel(kActionMenuCommandNone);
    }
    state.step = ArtListStep::kReturnSelection;
  } else {
    const int highlighted = host.HighlightedRow();
    if (highlighted < 0 ||
        static_cast<std::size_t>(highlighted) >= state.row_count) {
      host.Refuse("Break Arts highlighted row exceeds the created rows");
      return 0;
    }
    const std::uint16_t action_id =
        state.action_ids[static_cast<std::size_t>(highlighted)];
    if (action_id != kMenuSelectionBack) {
      SetHpCost(action_id, host);
    }
  }
  return std::nullopt;
}

std::optional<int> ReturnArtSelection(Context& context) {
  if (context.host.RowsIdle()) {
    return context.state.art_list.selected_row;
  }
  return std::nullopt;
}

bool AdvanceArtListAnimation(Context& context) {
  ArtListState& state = context.state.art_list;
  Host& host = context.host;
  if (state.is_last_row) {
    if (state.animation_index != 0) {
      --state.animation_index;
    }
  } else {
    const std::span<const std::uint8_t> indices = host.AnimationIndices();
    if (state.animation_index >= indices.size()) {
      host.Refuse("Break Arts animation index exceeds its lookup table");
      return false;
    }
    state.animation_index = indices[state.animation_index];
  }
  const std::span<const std::int8_t> steps = host.RowAnimationSteps();
  if (state.animation_index >= steps.size()) {
    host.Refuse("Break Arts row-animation index exceeds its lookup table");
    return false;
  }
  DrawNextUnlockPoints(steps[state.animation_index], state.weapon_type,
                       state.row_count, host);
  return true;
}
}  // namespace

// decomp: MENU1.PRG _weaponArtsMenu
// (src/MENU/MENU1.PRG/30.c:104).
int UpdateArtList(int type_cursor_memory, Context& context) {
  if (type_cursor_memory != 0) {
    return SetArtListTitle(type_cursor_memory, context);
  }
  const ArtListState& state = context.state.art_list;
  std::optional<int> result;
  switch (state.step) {
    case ArtListStep::kInit:
      result = InitializeArtListRows(context);
      break;
    case ArtListStep::kWaitForSelection:
      result = WaitForArtSelection(context);
      break;
    case ArtListStep::kReturnSelection:
      result = ReturnArtSelection(context);
      break;
    default:
      context.host.Refuse("Break Arts art-list state is invalid");
      return 0;
  }
  if (result.has_value()) {
    return *result;
  }
  AdvanceArtListAnimation(context);
  return 0;
}

namespace {
bool BuildWeaponCategoryRows(Context& context,
                             std::array<Row, kCategoryCount>& rows) {
  Host& host = context.host;
  const std::optional<int> equipped = host.EquippedWeaponCategory();
  if (!equipped) {
    host.Refuse("Break Arts equipped weapon category is unavailable");
    return false;
  }
  const std::span<const std::uint16_t> strings = host.MenuStrings();
  for (std::size_t category = 0; category < kCategoryCount; ++category) {
    const std::size_t text_index = category * 3;
    std::uint16_t name = 0;
    std::uint16_t description = 0;
    if (text_index + 2 >= strings.size() ||
        !MenuOffsetAt(host, text_index, name) ||
        !MenuOffsetAt(host, text_index + 1, description)) {
      return false;
    }
    std::int32_t row_type =
        kCategoryIconStep * static_cast<std::int32_t>(category + 1);
    bool any_unlocked = false;
    for (std::size_t art = 0; art < kArtCount; ++art) {
      const int action_id =
          host.DaggerArtOneId() + static_cast<int>(category * kArtCount + art);
      bool unlocked = false;
      if (!ReadActionUnlocked(host, action_id, unlocked)) {
        return false;
      }
      if (unlocked) {
        any_unlocked = true;
        break;
      }
    }
    if (!any_unlocked) {
      if (!MenuOffsetAt(host, text_index + 2, description)) {
        return false;
      }
      row_type |= kCategoryDisabled;
    } else if (static_cast<int>(category) == *equipped - 1) {
      row_type |= kCategoryEquipped;
    }
    rows[category] = {
        .name = {TextKind::kMenuOffset, name},
        .description = {TextKind::kMenuOffset, description},
        .type = row_type,
    };
  }
  return true;
}

std::optional<int> InitializeWeaponCategories(Context& context) {
  CategoryState& state = context.state.categories;
  Host& host = context.host;
  if (!host.RowsIdle()) {
    return std::nullopt;
  }
  std::array<Row, kCategoryCount> rows{};
  if (!BuildWeaponCategoryRows(context, rows) ||
      !CreateRows(host, kCategoryRowInfo, rows)) {
    return 0;
  }
  state.step = CategoryStep::kWaitForSelection;
  return std::nullopt;
}

std::optional<int> WaitForCategorySelection(Context& context) {
  CategoryState& state = context.state.categories;
  Host& host = context.host;
  state.selected_row = SelectedRowResult(host);
  if (state.selected_row == kMenuSelectionConfirm) {
    return std::nullopt;
  }
  if (state.selected_row == kMenuSelectionQuit) {
    host.ClearRowsExcept(kNoMenuRow);
    host.DismissInformationBox();
    host.SetCommandLabel(kActionMenuCommandNone);
  } else {
    host.ClearRowsExcept(kHeaderRow);
  }
  state.step = CategoryStep::kReturnSelection;
  return std::nullopt;
}
}  // namespace

// decomp: MENU1.PRG _weaponCategoriesMenu
// (src/MENU/MENU1.PRG/30.c:266).
int UpdateWeaponCategories(int initialize, Context& context) {
  CategoryState& state = context.state.categories;
  Host& host = context.host;
  if (initialize != 0) {
    if (SetMenuItem(host,
                    {.row = kTitleRow,
                     .x = kMenuCenterX,
                     .y = kArtTitleY,
                     .width = kArtTitleWidth,
                     .background_width = kMenuItemBackgroundWidth},
                    {TextKind::kMenuOffset, host.ViewArtsNameOffset()})) {
      state.step = CategoryStep::kInit;
    }
    return 0;
  }
  std::optional<int> result;
  switch (state.step) {
    case CategoryStep::kInit:
      result = InitializeWeaponCategories(context);
      break;
    case CategoryStep::kWaitForSelection:
      result = WaitForCategorySelection(context);
      break;
    case CategoryStep::kReturnSelection:
      if (host.RowsIdle()) {
        return state.selected_row;
      }
      break;
    default:
      host.Refuse("Break Arts category state is invalid");
      return 0;
  }
  return result.value_or(0);
}

// decomp: MENU1.PRG _setMenuHeader (src/MENU/MENU1.PRG/30.c:344).
void CreateHeader(Host& host) {
  if (!SetMenuItem(host,
                   {.row = kHeaderRow,
                    .x = kHeaderX,
                    .y = kHeaderY,
                    .width = kHeaderWidth,
                    .background_width = kMenuItemBackgroundWidth},
                   {TextKind::kBattleMenuIndex, host.BreakArtsHeaderIndex()})) {
    return;
  }
  host.SetCommandLabel(kActionMenuCommandMenu);
  host.InitializeInformationBox();
}

// decomp: MENU1.PRG vs_menu1_exec (src/MENU/MENU1.PRG/30.c:358).
namespace {
int InitializeArtsMenu(Context& context, std::uint8_t& step,
                       bool read_equipped_category) {
  State& state = context.state;
  Host& host = context.host;
  if (read_equipped_category) {
    if (!host.RowsIdle()) {
      return 0;
    }
    host.InitializeInformationBox();
    const std::optional<int> equipped = host.EquippedWeaponCategory();
    if (!equipped || !WeaponTypeIsValid(host, *equipped)) {
      if (!equipped) {
        host.Refuse("Break Arts equipped weapon category is unavailable");
      }
      return 0;
    }
    state.update_weapon_type = *equipped;
  }
  UpdateArtList(state.update_weapon_type, context);
  step = static_cast<std::uint8_t>(ScreenStep::kArtsMenu);
  return 0;
}

void InitializeArtsMenuWithTitle(Context& context, std::uint8_t& step) {
  CreateHeader(context.host);
  UpdateArtList(context.state.update_weapon_type | kForceCursorMemoryFlag,
                context);
  step = static_cast<std::uint8_t>(ScreenStep::kArtsMenu);
}

void HandleArtsMenu(Context& context, std::uint8_t& step) {
  Host& host = context.host;
  const int selected = UpdateArtList(0, context);
  if (selected == kMenuSelectionConfirm) {
    return;
  }
  if (selected > 0) {
    if (selected == kMenuSelectionBack) {
      step = static_cast<std::uint8_t>(ScreenStep::kWeaponCategoryMenu);
      UpdateWeaponCategories(1, context);
    } else if (selected < kActionNameCount) {
      host.SetActionSelection(static_cast<std::uint16_t>(selected));
      host.SetExecuteAbilityType(kBreakArtExecuteType);
      step = static_cast<std::uint8_t>(ScreenStep::kExecuteArt);
    } else {
      host.Refuse("Break Arts returned an invalid action selection");
    }
  } else if (selected != kMenuSelectionQuit) {
    step = static_cast<std::uint8_t>(ScreenStep::kExitToMainMenu);
  } else {
    step = static_cast<std::uint8_t>(ScreenStep::kExit);
  }
}

void HandleWeaponCategoryMenu(Context& context, std::uint8_t& step) {
  State& state = context.state;
  Host& host = context.host;
  const int row = UpdateWeaponCategories(0, context);
  if (row == 0) {
    return;
  }
  if (row == kMenuSelectionQuit) {
    step = static_cast<std::uint8_t>(ScreenStep::kExit);
  } else if (row > 0 && row <= static_cast<int>(kCategoryCount)) {
    state.update_weapon_type = row;
    step = static_cast<std::uint8_t>(ScreenStep::kArtsInit);
  } else if (row > static_cast<int>(kCategoryCount)) {
    host.Refuse("Break Arts category selection exceeds 10");
  } else {
    step = static_cast<std::uint8_t>(ScreenStep::kArtsInit);
  }
}

int ExitToMainMenu(Context& context, std::uint8_t& step) {
  Host& host = context.host;
  host.DismissInformationBox();
  host.SetCommandLabel(kActionMenuCommandNone);
  if (host.RowsIdle()) {
    step = static_cast<std::uint8_t>(ScreenStep::kNone);
    return 1;
  }
  return 0;
}

int ExecuteArt(Context& context, std::uint8_t& step) {
  Host& host = context.host;
  if (!host.RowsIdle()) {
    return 0;
  }
  host.SetSelectionKind(kBreakArtSelectionKind);
  host.SetReturnState(host.CurrentMenu());
  host.SetCurrentMenu(kBattleMenu);
  step = static_cast<std::uint8_t>(ScreenStep::kNone);
  return 1;
}

int ExitBreakArts(Context& context, std::uint8_t& step) {
  Host& host = context.host;
  host.DismissInformationBox();
  host.SetCommandLabel(kActionMenuCommandNone);
  if (!host.RowsIdle()) {
    return 0;
  }
  host.SetCurrentMenu(kBattleMenu);
  step = static_cast<std::uint8_t>(ScreenStep::kNone);
  return 1;
}
}  // namespace

int Update(Context& context, std::uint8_t& step) {
  switch (static_cast<ScreenStep>(step)) {
    case ScreenStep::kInit:
      return InitializeArtsMenu(context, step, true);
    case ScreenStep::kArtsInit:
      return InitializeArtsMenu(context, step, false);
    case ScreenStep::kArtsInitWithTitle:
      InitializeArtsMenuWithTitle(context, step);
      break;
    case ScreenStep::kArtsMenu:
      HandleArtsMenu(context, step);
      break;
    case ScreenStep::kWeaponCategoryMenu:
      HandleWeaponCategoryMenu(context, step);
      break;
    case ScreenStep::kExitToMainMenu:
      return ExitToMainMenu(context, step);
    case ScreenStep::kExecuteArt:
      return ExecuteArt(context, step);
    case ScreenStep::kExit:
      return ExitBreakArts(context, step);
    case ScreenStep::kNone:
    default:
      break;
  }
  return 0;
}
}  // namespace VS::Menu::break_arts
