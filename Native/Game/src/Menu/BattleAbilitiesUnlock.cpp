#include <algorithm>

#include "Menu/BattleAbilities.hpp"

namespace VS::Menu::BattleAbilities {
namespace {
constexpr int kWeaponCategories = 10;
constexpr int kBreakArtTiers = 4;
constexpr int kFirstBreakArt = 184;
constexpr int kBattleAbilityTiers = 22;
constexpr std::uint8_t kMessageTimeout = 120;
constexpr std::uint8_t kChainFirstVisit = 1;
constexpr std::uint8_t kDefenseFirstVisit = 2;
constexpr int kMaximumUnlockRows = 4;
constexpr int kVisibleAbilityRows = 8;
constexpr int kLastVisibleRow = 7;
constexpr int kPreviousVisibleRow = 6;
constexpr int kDecimalBase = 10;
constexpr int kRiskLabelX = 8;
constexpr int kRiskValueX = 72;
constexpr std::uint16_t kNameSpacing = 0x10FA;
constexpr std::uint16_t kDescriptionChunk = 0xF8;
constexpr std::uint16_t kTextEnd = 0xE7E7;
constexpr std::size_t kNameStart = 1;
constexpr std::size_t kNameEnd = 15;
constexpr std::size_t kDescriptionHeader = 16;
constexpr std::size_t kDescriptionStart = 17;
constexpr std::size_t kDescriptionEnd = 63;
constexpr std::uint8_t kCheckBreakArt = 1;
constexpr std::uint8_t kUpdateBreakArt = 2;
constexpr std::uint8_t kCheckAbility = 3;
constexpr std::uint8_t kUpdateAbility = 4;
constexpr std::uint8_t kDismiss = 5;
constexpr auto kChainActions = std::to_array<std::uint8_t>(
    {0x18, 0x19, 0x22, 0x25, 0x23, 0x1E, 0x1A, 0x1B, 0x20, 0x1D, 0x1F});
constexpr auto kDefenseActions = std::to_array<std::uint8_t>(
    {0x2F, 0x30, 0x31, 0x32, 0x29, 0x2A, 0x33, 0x34, 0x2C, 0x2D, 0x35});

std::uint8_t CollectRemaining(std::span<std::uint8_t> remaining,
                              std::span<const std::uint8_t> actions,
                              const UnlockProgress& progress) {
  std::uint8_t count = 0;
  for (const auto action : actions) {
    if (!progress.ActionUnlocked(action)) remaining[count++] = action;
  }
  return count;
}

int InitializeAbilityUnlock(AbilityUnlockState& state, UnlockProgress& progress,
                            AbilityUnlockHost& host) {
  const int unlocked = progress.BattleAbilitiesUnlocked();
  if (unlocked < 0 || unlocked > kBattleAbilityTiers) {
    host.Refuse("ability unlock tier outside requirements");
    return 1;
  }
  if (unlocked == kBattleAbilityTiers ||
      progress.TotalKills() < progress.BattleAbilityRequirement(unlocked)) {
    return 1;
  }
  progress.SetBattleAbilitiesUnlocked(unlocked + 1);
  host.ResetUnlockMenuMemory();
  state.chain_count =
      CollectRemaining(state.remaining_chain, kChainActions, progress);
  state.defense_count =
      CollectRemaining(state.remaining_defense, kDefenseActions, progress);
  state.first_visit = kChainFirstVisit | kDefenseFirstVisit;
  state.timeout = kMessageTimeout;
  state.phase = AbilityUnlockPhase::kInit;
  return 0;
}

void SelectCategories(AbilityUnlockState& state, AbilityUnlockHost& host) {
  constexpr AbilityCategoryRow kCategoryRow{
      AbilityKind::kChain,          320, 18, 140, 8, 180,
      Framework::RowState::kSlideX, true};
  const bool chain = state.chain_count != 0;
  const bool defense = state.defense_count != 0;
  if (!chain && !defense) {
    host.SetAbilityMessage(true, 0);
    state.phase = AbilityUnlockPhase::kShowMessage;
  } else if (chain && defense) {
    host.ShowBothCategories();
    state.phase = AbilityUnlockPhase::kBothRemaining;
  } else {
    auto row = kCategoryRow;
    row.kind = chain ? AbilityKind::kChain : AbilityKind::kDefense;
    if (host.ShowCategory(row)) {
      state.phase = chain ? AbilityUnlockPhase::kChainOnly
                          : AbilityUnlockPhase::kDefenseOnly;
    }
  }
}

void SelectCategory(AbilityUnlockState& state, AbilityUnlockHost& host) {
  const int row = host.SelectedRow();
  if (row == -1) return;
  if (row < -1) {
    host.ClearMenu();
    state.phase = AbilityUnlockPhase::kReinit;
  } else if (row > 1) {
    host.Refuse("ability category selection outside two rows");
  } else {
    host.FlyOutRowsAndHoistSelection(row);
    state.phase = row == 0 ? AbilityUnlockPhase::kChainOnly
                           : AbilityUnlockPhase::kDefenseOnly;
  }
}

void ShowRemaining(AbilityUnlockState& state, AbilityUnlockHost& host,
                   bool chain) {
  if (!host.MenuRowsIdle()) return;
  const auto actions =
      chain ? std::span<const std::uint8_t>(state.remaining_chain)
                  .first(state.chain_count)
            : std::span<const std::uint8_t>(state.remaining_defense)
                  .first(state.defense_count);
  const int count = static_cast<int>(actions.size());
  if (count == 0) {
    host.Refuse("empty ability selection");
    return;
  }
  const int visible = count == 1 ? 1 : std::min(count - 1, kMaximumUnlockRows);
  const auto mask = chain ? kChainFirstVisit : kDefenseFirstVisit;
  const bool first_visit = (state.first_visit & mask) != 0;
  if (host.ShowAbilityList(chain ? AbilityKind::kChain : AbilityKind::kDefense,
                           actions, visible, first_visit)) {
    if (first_visit) state.first_visit -= mask;
    state.phase = chain ? AbilityUnlockPhase::kUnlockChain
                        : AbilityUnlockPhase::kUnlockDefense;
  }
}

void ConfirmAbility(AbilityUnlockState& state, UnlockProgress& progress,
                    AbilityUnlockHost& host, bool chain) {
  const int row = host.SelectedRow();
  if (row == -1) return;
  host.ClearMenu();
  if (row < -1) {
    state.phase = AbilityUnlockPhase::kReinit;
    return;
  }
  const auto actions =
      chain ? std::span<const std::uint8_t>(state.remaining_chain)
                  .first(state.chain_count)
            : std::span<const std::uint8_t>(state.remaining_defense)
                  .first(state.defense_count);
  if (static_cast<std::size_t>(row) >= actions.size()) {
    host.Refuse("ability selection outside remaining rows");
    state.phase = AbilityUnlockPhase::kReinit;
    return;
  }
  const auto action = actions[static_cast<std::size_t>(row)];
  host.InitializeInformationBox();
  host.SetAbilityMessage(false, action);
  progress.UnlockAction(action);
  state.phase = AbilityUnlockPhase::kShowMessage;
}

AbilityCursor RestoreCursor(int count, AbilityCursor cursor) {
  const int overflow = count - kVisibleAbilityRows;
  auto& [highlighted, first] = cursor;
  if (overflow < 0) {
    highlighted += first;
    first = 0;
    if (highlighted >= count) highlighted = 0;
    return cursor;
  }
  if (overflow < first) {
    if (highlighted + first >= count) {
      highlighted = 0;
      first = 0;
    } else {
      highlighted += first - overflow;
      first = overflow;
    }
  }
  if (first > 0 && highlighted == 0) {
    highlighted = 1;
    --first;
  }
  if (first < overflow && highlighted == kLastVisibleRow) {
    highlighted = kPreviousVisibleRow;
    ++first;
  }
  return cursor;
}

bool ValidRows(std::span<const AbilityRowText> rows) {
  return std::ranges::all_of(rows, [](const AbilityRowText& row) {
    return (row.name.empty() || row.name.size() >= kAbilityNameWords) &&
           (row.description.empty() ||
            row.description.size() >= kAbilityDescriptionWords);
  });
}

void CopyRow(std::span<std::uint16_t> destination, const AbilityRowText& row) {
  destination[0] = row.name.empty() ? kTextEnd : kNameSpacing;
  if (!row.name.empty()) {
    std::ranges::copy(row.name.first(kAbilityNameWords),
                      destination.subspan(kNameStart).begin());
    destination[kNameEnd] = kTextEnd;
  }
  destination[kDescriptionHeader] =
      row.description.empty() ? kTextEnd : kDescriptionChunk;
  if (!row.description.empty()) {
    std::ranges::copy(row.description.first(kAbilityDescriptionWords),
                      destination.subspan(kDescriptionStart).begin());
    destination[kDescriptionEnd] = kTextEnd;
  }
}
}  // namespace

// decomp: breakArtUnlocked (src/MENU/MENU2.PRG/143C.c:31; was
// _breakArtUnlocked; MENU2.PRG 0x80103C3C-0x80103DD8, including the return
// delay slot).
int UpdateBreakArtUnlock(BreakArtUnlockState& state, UnlockProgress& progress,
                         BreakArtUnlockHost& host, bool initialize) {
  if (initialize) {
    const int weapon = progress.EquippedWeaponCategory();
    if (weapon < 0 || weapon > kWeaponCategories) {
      host.Refuse("weapon category outside Break Art menu");
      return 1;
    }
    const int category = weapon % kWeaponCategories;
    const int tier = progress.ArtsLearned(category);
    if (tier == kBreakArtTiers) return 1;
    if (tier < 0 || tier >= kBreakArtTiers) {
      host.Refuse("learned Break Art tier outside requirements");
      return 1;
    }
    if (progress.WeaponCategoryKills(category) <
        progress.BreakArtRequirement(category, tier)) {
      return 1;
    }
    const auto action = static_cast<std::uint8_t>(
        kFirstBreakArt + (weapon - 1) * kBreakArtTiers + tier);
    host.InitializeInformationBox();
    progress.SetArtsLearned(category, tier + 1);
    host.SetBreakArtMessage(weapon, action);
    progress.UnlockAction(action);
    state.timeout = kMessageTimeout;
  } else if (host.InformationBoxDone()) {
    if (state.timeout == 0 || host.ConfirmPressed()) return 1;
    --state.timeout;
  }
  return 0;
}

// decomp: getAbilityDescription (src/MENU/MENU2.PRG/143C.c:85; was
// _getAbilityDescription; MENU2.PRG 0x80103DD8-0x80103E20).
std::span<const std::uint16_t> UnlockDescription(
    int ability, std::span<const std::uint16_t> ability_strings,
    std::span<const std::uint16_t> menu_strings, std::size_t fallback_offset) {
  constexpr auto kDescriptionIndexes = std::to_array<std::uint8_t>(
      {0, 0,  0, 0, 0, 0,  0,  0, 0,  0,  0, 0,  0,  0,  0,  0,  0,  0,
       0, 0,  0, 0, 0, 0,  8,  9, 10, 11, 0, 13, 14, 15, 16, 0,  17, 18,
       0, 19, 0, 0, 0, 21, 22, 0, 24, 25, 0, 27, 28, 29, 30, 31, 32, 33});
  const auto index =
      ability >= 0 &&
              static_cast<std::size_t>(ability) < kDescriptionIndexes.size()
          ? kDescriptionIndexes[static_cast<std::size_t>(ability)]
          : 0;
  if (index == 0) {
    return fallback_offset < menu_strings.size()
               ? menu_strings.subspan(fallback_offset)
               : std::span<const std::uint16_t>{};
  }
  if (static_cast<std::size_t>(index) >= ability_strings.size()) return {};
  const auto offset = ability_strings[static_cast<std::size_t>(index)];
  return offset < ability_strings.size() ? ability_strings.subspan(offset)
                                         : std::span<const std::uint16_t>{};
}

// decomp: battleAbilityUnlocked (src/MENU/MENU2.PRG/143C.c:129; was
// _battleAbilityUnlocked; MENU2.PRG 0x80103E20-0x80104578).
int UpdateAbilityUnlock(AbilityUnlockState& state, UnlockProgress& progress,
                        AbilityUnlockHost& host, bool initialize) {
  if (initialize) return InitializeAbilityUnlock(state, progress, host);
  if (state.chain_count > kUnlockableAbilityCount ||
      state.defense_count > kUnlockableAbilityCount) {
    host.Refuse("remaining ability count outside capacity");
    return 0;
  }
  switch (state.phase) {
    case AbilityUnlockPhase::kInit:
      SelectCategories(state, host);
      break;
    case AbilityUnlockPhase::kBothRemaining:
      SelectCategory(state, host);
      break;
    case AbilityUnlockPhase::kChainOnly:
    case AbilityUnlockPhase::kDefenseOnly:
      ShowRemaining(state, host, state.phase == AbilityUnlockPhase::kChainOnly);
      break;
    case AbilityUnlockPhase::kUnlockChain:
    case AbilityUnlockPhase::kUnlockDefense:
      ConfirmAbility(state, progress, host,
                     state.phase == AbilityUnlockPhase::kUnlockChain);
      break;
    case AbilityUnlockPhase::kShowMessage:
      if (host.InformationBoxDone()) {
        if (state.timeout == 0 || host.ConfirmPressed()) return 1;
        --state.timeout;
      }
      break;
    case AbilityUnlockPhase::kReinit:
      if (host.MenuRowsIdle()) state.phase = AbilityUnlockPhase::kInit;
      break;
    default:
      break;
  }
  return 0;
}

// decomp: vs_menu2_actionUnlock (src/MENU/MENU2.PRG/143C.c:426;
// MENU2.PRG 0x80104578-0x80104690).
int UpdateUnlockNotifications(std::uint8_t& state,
                              UnlockNotificationHost& host) {
  switch (state) {
    case 0:
      host.InitializeInformationBox();
      [[fallthrough]];
    case kCheckBreakArt:
      state = kCheckAbility;
      if (host.BreakArtPending() && host.UpdateBreakArt(true) == 0) {
        state = kUpdateBreakArt;
      }
      break;
    case kUpdateBreakArt:
      if (host.UpdateBreakArt(false) != 0) state = kCheckBreakArt;
      break;
    case kCheckAbility:
      state = kDismiss;
      if (host.AbilityPending() && host.UpdateAbility(true) == 0) {
        state = kUpdateAbility;
      }
      break;
    case kUpdateAbility:
      if (host.UpdateAbility(false) != 0) state = kCheckAbility;
      break;
    case kDismiss:
      if (host.DismissInformationBox() == 0) {
        state = 0;
        return 1;
      }
      break;
    default:
      break;
  }
  return 0;
}

// decomp: setRiskCost (src/MENU/MENU2.PRG/64.c:22; was _setRiskCost;
// MENU2.PRG 0x80102864-0x80102928). The original action cost is one unsigned
// byte.
void SetRiskCost(RiskCostState& state, std::uint8_t cost, RiskCostHost& host) {
  host.SetActionCostText(1, "RISK", kRiskLabelX, false);
  state.text.back() = '\0';
  std::size_t index = state.text.size() - 1;
  int remaining = cost;
  do {
    state.text[--index] = static_cast<char>('0' + remaining % kDecimalBase);
    remaining /= kDecimalBase;
  } while (remaining != 0);
  state.text[--index] = '#';
  host.SetActionCostText(0, std::string_view(state.text.data() + index),
                         kRiskValueX, false);
}

// decomp: initAbilityMenu (src/MENU/MENU2.PRG/64.c:73; was _initAbilityMenu;
// MENU2.PRG 0x80102928-0x80102B5C).
int CreateAbilityRows(AbilityRowsState& state, AbilityRowsHost& host, int count,
                      AbilityMenuType type,
                      std::span<const AbilityRowText> rows) {
  if (host.HasAbilityText()) return 0;
  if (count <= 0 || count > static_cast<int>(kAbilityCapacity) ||
      rows.size() < static_cast<std::size_t>(count) ||
      (type != AbilityMenuType::kChain && type != AbilityMenuType::kDefense)) {
    host.Refuse("ability row count or type outside capacity");
    return 0;
  }
  rows = rows.first(static_cast<std::size_t>(count));
  if (!ValidRows(rows)) {
    host.Refuse("truncated ability name or description");
    return 0;
  }
  const auto saved =
      host.CursorMemoryEnabled() ? host.SavedCursor(type) : AbilityCursor{};
  constexpr int kLastSavedByte = 255;
  if (saved.highlighted < 0 || saved.highlighted > kLastSavedByte ||
      saved.first < 0 || saved.first > kLastSavedByte) {
    host.Refuse("ability cursor memory outside byte range");
    return 0;
  }
  const auto cursor = RestoreCursor(count, saved);
  if (cursor.highlighted + cursor.first >= count) {
    host.Refuse("restored ability cursor outside learned rows");
    return 0;
  }
  const auto action = host.ActionAt(type, cursor.highlighted + cursor.first);
  if (!action) {
    host.Refuse("missing learned ability action");
    return 0;
  }
  host.SetRiskCost(*action);
  const std::size_t words = rows.size() * kAbilityRowWords;
  const auto allocation = host.AllocateAbilityText(words);
  if (allocation.size() < words) {
    host.Refuse("ability row allocation failed");
    return 0;
  }
  const auto text = allocation.first(words);
  for (std::size_t row = 0; row < rows.size(); ++row) {
    CopyRow(text.subspan(row * kAbilityRowWords, kAbilityRowWords), rows[row]);
  }
  state = {count, 0, type, cursor};
  host.PublishRows(state, text);
  return 1;
}
}  // namespace VS::Menu::BattleAbilities
