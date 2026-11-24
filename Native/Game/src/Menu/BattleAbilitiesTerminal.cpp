#include "Menu/BattleAbilitiesTerminal.hpp"

#include <algorithm>

namespace VS::Menu::BattleAbilities {
namespace {
constexpr int kAllAbilitiesUnlocked = 22;
constexpr std::uint32_t kPointsX = 206;
constexpr std::uint32_t kPointsY = 66;
constexpr std::uint32_t kPointValueOffset = 96;
constexpr std::uint32_t kCoordinateShift = 16;
constexpr int kDecimalBase = 10;
constexpr int kSeparatePointsMaximum = 99999;
constexpr std::string_view kCombinedPointsPrefix = "#NEXT ";
constexpr int kChainShortcut = 7;
constexpr int kDefenseShortcut = 8;
constexpr int kShortcutRowOffset = 3;
constexpr int kShortcutX = 320;
constexpr int kShortcutY = 34;
constexpr int kShortcutWidth = 140;
constexpr int kShortcutBackgroundWidth = 8;
constexpr std::uint8_t kPointsAnimationSteps = 10;
constexpr int kPointsExitDistance = 32;
constexpr int kBattleExitSelection = -3;

constexpr std::uint8_t Phase(AbilityScreenPhase phase) {
  return static_cast<std::uint8_t>(phase);
}

int UpdateLearnedAbilities(AbilityKind kind, AbilityListState& state,
                           AbilityListHost& host, bool initialize) {
  if (initialize) {
    host.FlyOutCategory(kind);
    state.phase = AbilityListPhase::kInit;
    return 0;
  }
  switch (state.phase) {
    case AbilityListPhase::kInit: {
      if (!host.ShortcutInvoked() && !host.RowsIdle()) break;
      const auto order = host.ActionOrder(kind);
      if (order.size() < kLearnedAbilitySlots) {
        host.Refuse("learned ability order has fewer than fourteen slots");
        return 0;
      }
      std::array<LearnedAbility, kLearnedAbilitySlots> learned{};
      std::size_t count = 0;
      for (std::size_t slot = 0; slot < kLearnedAbilitySlots; ++slot) {
        const auto action = order[slot];
        if (host.ActionUnlocked(action)) {
          learned[count] = {action, static_cast<std::uint8_t>(slot)};
          host.StoreLearnedAction(kind, static_cast<int>(count), action);
          ++count;
        }
      }
      // Creation is a no-op when rows already exist; the caller still advances.
      host.CreateRows(kind, std::span(learned).first(count));
      state.phase = AbilityListPhase::kAssignments;
      break;
    }
    case AbilityListPhase::kAssignments:
      if (host.UpdateAssignments() != 0) {
        host.ClearAbilityRows();
        return 1;
      }
      break;
  }
  return 0;
}

bool EnterShortcut(std::uint8_t& phase, AbilityScreenHost& host, int shortcut) {
  if (shortcut != kChainShortcut && shortcut != kDefenseShortcut) {
    host.Refuse("ability screen shortcut is neither chain nor defense");
    return false;
  }
  const bool chain = shortcut == kChainShortcut;
  const auto kind = chain ? AbilityKind::kChain : AbilityKind::kDefense;
  if (!host.ShowShortcutCategory(
          {.kind = kind,
           .row = shortcut + kShortcutRowOffset,
           .x = kShortcutX,
           .y = kShortcutY,
           .width = kShortcutWidth,
           .background_width = kShortcutBackgroundWidth})) {
    return false;
  }
  phase =
      Phase(chain ? AbilityScreenPhase::kChain : AbilityScreenPhase::kDefense);
  static_cast<void>(host.UpdateAbilityList(kind, true));
  return true;
}

void SelectCategory(std::uint8_t& phase, AbilityScreenAnimation& animation,
                    AbilityScreenHost& host) {
  animation.menu_animating = true;
  const int selected = host.SelectedCategory();
  if (selected == -1) return;
  animation.menu_animating = false;
  if (selected == 0 || selected == 1) {
    const bool chain = selected == 0;
    phase = Phase(chain ? AbilityScreenPhase::kChain
                        : AbilityScreenPhase::kDefense);
    static_cast<void>(host.UpdateAbilityList(
        chain ? AbilityKind::kChain : AbilityKind::kDefense, true));
    return;
  }
  host.ClearMenu();
  phase = Phase(selected == kBattleExitSelection
                    ? AbilityScreenPhase::kExitToBattle
                    : AbilityScreenPhase::kExitToMainMenu);
}

void UpdateSelectedList(std::uint8_t& phase, AbilityScreenHost& host) {
  const auto kind = phase == Phase(AbilityScreenPhase::kChain)
                        ? AbilityKind::kChain
                        : AbilityKind::kDefense;
  if (host.UpdateAbilityList(kind, false) == 0) return;
  if (host.Shortcut() != 0) {
    host.ClearMenu();
    phase = Phase(AbilityScreenPhase::kExitToBattle);
  } else {
    phase = Phase(AbilityScreenPhase::kSelectTypeInit);
  }
}

void AnimatePoints(AbilityScreenAnimation& animation, AbilityScreenHost& host) {
  if (animation.menu_animating) {
    const auto offset = host.AnimationOffset(animation.points_step);
    if (!offset) {
      host.Refuse("ability points animation step outside its table");
      return;
    }
    host.DrawNextUnlockPoints(*offset);
    if (animation.points_step != 0) --animation.points_step;
  } else if (animation.points_step < kPointsAnimationSteps) {
    ++animation.points_step;
    host.DrawNextUnlockPoints(animation.points_step * kPointsExitDistance);
  }
}
}  // namespace

// decomp: chainAbilityMenu (src/MENU/MENU2.PRG/64.c:501; was _chainAbilityMenu;
// 0x801034FC).
int UpdateChainAbilityMenu(AbilityListState& state, AbilityListHost& host,
                           bool initialize) {
  return UpdateLearnedAbilities(AbilityKind::kChain, state, host, initialize);
}

// decomp: defenseAbilityMenu (src/MENU/MENU2.PRG/64.c:562; was
// _defenseAbilityMenu; 0x80103670).
int UpdateDefenseAbilityMenu(AbilityListState& state, AbilityListHost& host,
                             bool initialize) {
  return UpdateLearnedAbilities(AbilityKind::kDefense, state, host, initialize);
}

// decomp: setNextAbilityUnlockPointsRemaining
// (src/MENU/MENU2.PRG/64.c:621; was _setNextAbilityUnlockPointsRemaining;
// 0x801037E4).
void DrawNextUnlockPoints(int x_offset, UnlockPointsHost& host) {
  const int count = host.UnlockCount();
  if (count == kAllAbilitiesUnlocked) return;
  if (count < 0 || count > kAllAbilitiesUnlocked) {
    host.Refuse("ability unlock count outside the requirement table");
    return;
  }
  const auto requirement = host.Requirement(count);
  if (!requirement) {
    host.Refuse("next ability unlock requirement is missing");
    return;
  }
  int points = std::max(0, static_cast<int>(*requirement) - host.TotalKills());
  const bool combined = points > kSeparatePointsMaximum;
  std::array<char, kUnlockPointsTextCapacity> text{};
  auto position = text.size() - 2;
  text[position--] = 'T';
  text[position--] = 'P';
  do {
    text[position--] = static_cast<char>('0' + points % kDecimalBase);
    points /= kDecimalBase;
  } while (points != 0);
  if (combined) {
    position = position + 1 - kCombinedPointsPrefix.size();
    std::ranges::copy(kCombinedPointsPrefix, text.data() + position);
  } else {
    text[position] = '#';
  }
  const auto screen = (static_cast<std::uint32_t>(x_offset) + kPointsX) |
                      (kPointsY << kCoordinateShift);
  if (!combined) host.DrawCompactText("NEXT", screen);
  host.DrawCompactText({text.data() + position, text.size() - position - 1},
                       screen + kPointValueOffset);
}

// decomp: vs_menu2_exec (src/MENU/MENU2.PRG/64.c:665; 0x801038D4).
int UpdateAbilityScreen(std::uint8_t& phase, AbilityScreenAnimation& animation,
                        AbilityScreenHost& host) {
  switch (static_cast<AbilityScreenPhase>(phase)) {
    case AbilityScreenPhase::kInit: {
      animation.menu_animating = false;
      animation.points_step = kPointsAnimationSteps;
      if (!host.RowsIdle()) break;
      host.InitializeInformationBox();
      const int shortcut = host.Shortcut();
      if (shortcut != 0) {
        EnterShortcut(phase, host, shortcut);
        break;
      }
      [[fallthrough]];
    }
    case AbilityScreenPhase::kSelectTypeInit:
      if (host.RowsIdle()) {
        const bool chain = host.HasLearnedAbility(AbilityKind::kChain);
        const bool defense = host.HasLearnedAbility(AbilityKind::kDefense);
        host.ShowCategories(chain, defense,
                            phase != Phase(AbilityScreenPhase::kInit));
        phase = Phase(AbilityScreenPhase::kSelectType);
      }
      break;
    case AbilityScreenPhase::kSelectType:
      SelectCategory(phase, animation, host);
      break;
    case AbilityScreenPhase::kChain:
    case AbilityScreenPhase::kDefense:
      UpdateSelectedList(phase, host);
      break;
    case AbilityScreenPhase::kExitToMainMenu:
    case AbilityScreenPhase::kExitToBattle:
      host.DismissInformationBox();
      host.ClearCommand();
      if (host.RowsIdle()) {
        if (phase == Phase(AbilityScreenPhase::kExitToBattle)) {
          host.ReturnToBattleMenu();
        }
        phase = Phase(AbilityScreenPhase::kNone);
        return 1;
      }
      break;
    default:
      break;
  }
  AnimatePoints(animation, host);
  return 0;
}
}  // namespace VS::Menu::BattleAbilities
