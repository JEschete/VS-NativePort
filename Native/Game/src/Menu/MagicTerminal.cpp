#include "Menu/MagicTerminal.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace VS::Menu::Magic {
namespace {
enum Step : std::uint8_t {
  kNone,
  kShortcut = 3,
  kInitSubmenu,
  kSubmenu,
  kInitWarlock,
  kWarlock,
  kInitShaman,
  kShaman,
  kInitSorcerer,
  kSorcerer,
  kInitEnchanter,
  kEnchanter,
  kTeleport,
  kExitToMain,
  kExecute,
  kExit
};
constexpr std::array<std::uint16_t, kSchoolCount> kSchoolTitles = {
    80,
    134,
    202,
    283,
};
constexpr std::array<std::uint8_t, kSchoolCount> kSchoolSteps = {
    kWarlock,
    kShaman,
    kSorcerer,
    kEnchanter,
};
constexpr auto kAbilityTypes = std::to_array<std::uint8_t>({6, 8, 10, 12});
constexpr std::size_t kSavePoints = 48;
constexpr std::size_t kStringsPerSchool = 3;
constexpr int kNoRow = 40;
constexpr int kResumedTitleWidth = 126;
constexpr int kShortcutTitleWidth = 140;
constexpr std::int16_t kTeleportSelectionKind = 10;
constexpr std::int16_t kActionSelectionKind = 7;
constexpr std::uint16_t kTeleportTitle = 99;
constexpr std::uint16_t kTeleportDescription = 106;
constexpr std::uint16_t kTeleportDisabled = 144;

std::optional<int> InitializeSubmenu(std::uint8_t& state, MenuHost& host) {
  const auto strings = host.MagicStrings();
  if (strings.size() < kSchoolCount * kStringsPerSchool) {
    host.Refuse("Magic school text index table is truncated");
    return std::nullopt;
  }
  std::array<SpellRow, kSchoolCount + 1> rows{};
  for (std::size_t i = 0; i < kSchoolCount; ++i) {
    const bool unlocked = host.SchoolUnlocked(static_cast<School>(i));
    rows[i] = {
        .name = {TextTable::kMagic, strings[i * kStringsPerSchool]},
        .description = {TextTable::kMagic,
                        strings[i * kStringsPerSchool + (unlocked ? 1U : 2U)]},
        .type = unlocked ? 0 : 1};
  }
  std::size_t count = kSchoolCount;
  if (host.TeleportAvailability() != 0) {
    const auto points = host.SavePointStates();
    if (points.size() < kSavePoints) {
      host.Refuse("Magic save-point state table is truncated");
      return std::nullopt;
    }
    const bool discovered =
        std::ranges::any_of(points.first(kSavePoints),
                            [](std::uint8_t value) { return value != 0; });
    auto& teleport = rows[count++];
    teleport.name = {TextTable::kTeleport, kTeleportTitle};
    teleport.description = {
        TextTable::kTeleport,
        host.AtSavePoint() && host.TeleportAvailability() == 2
            ? kTeleportDisabled
            : kTeleportDescription};
    // Preserve the original repeated live query and its short-circuit order.
    if (host.AtSavePoint() && host.TeleportAvailability() == 2) {
      teleport.description.index = kTeleportDisabled;
    }
    teleport.type =
        !discovered || host.TeleportAvailability() == 2 || !host.AtSavePoint()
            ? 1
            : 0;
  }
  const auto cursor = host.CursorMemory();
  if (state != kShortcut) {
    host.SetCursorMemory(1);
  }
  const bool created = host.CreateRows(0x105, std::span(rows).first(count));
  host.SetCursorMemory(cursor);
  if (!created) {
    return std::nullopt;
  }
  state = kSubmenu;
  return 0;
}

std::optional<int> RunSelection(std::uint8_t& state, MenuHost& host,
                                int selected) {
  if (selected >= 0 && selected < static_cast<int>(kSchoolCount)) {
    const auto school = static_cast<School>(selected);
    state = kSchoolSteps[static_cast<std::size_t>(school)];
    if (!host.RunSchool(school, 1)) {
      return std::nullopt;
    }
  } else if (selected == static_cast<int>(kSchoolCount)) {
    state = kTeleport;
    host.RunTeleport(1);
  } else {
    host.Refuse("Magic selected school is out of range");
    return std::nullopt;
  }
  return 0;
}

std::optional<int> HandleSubmenu(std::uint8_t& state, MenuHost& host) {
  const int row = host.SelectedRow();
  const auto selection =
      std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(row) + 1U);
  if (selection > 0) {
    return RunSelection(state, host, row);
  }
  if (selection < 0) {
    host.ClearRows(kNoRow);
    state = selection == -2 ? kExit : kExitToMain;
  }
  return 0;
}

std::optional<int> HandleSchool(std::uint8_t& state, MenuHost& host) {
  const auto index = static_cast<std::size_t>((state - kInitWarlock) / 2);
  const auto school = static_cast<School>(index);
  if ((state & 1U) == 0) {
    if (!host.CreateHeader() ||
        !host.SetSchoolTitle(school, {TextTable::kMagic, kSchoolTitles[index]},
                             kResumedTitleWidth) ||
        !host.RunSchool(school, 2)) {
      return std::nullopt;
    }
    state = kSchoolSteps[index];
    return 0;
  }
  const auto result = host.RunSchool(school, 0);
  if (!result) {
    return std::nullopt;
  }
  if (*result > 0) {
    host.SetActionSelection(*result);
    host.SetAbilityType(kAbilityTypes[index]);
    state = kExecute;
  } else if (*result < 0) {
    state = *result == -2 ? kExit : kInitSubmenu;
  }
  return 0;
}

std::optional<int> HandleShortcut(std::uint8_t& state, MenuHost& host) {
  if (!host.RowsIdle()) {
    return 0;
  }
  host.InitializeInformation();
  const auto shortcut = host.ShortcutInvoked();
  if (shortcut == 0) {
    return InitializeSubmenu(state, host);
  }
  const auto strings = host.MagicStrings();
  const std::size_t title_index = (shortcut - 1U) * kStringsPerSchool;
  if (shortcut > kSchoolCount || title_index >= strings.size()) {
    host.Refuse("Magic shortcut school is out of range");
    return std::nullopt;
  }
  if (!host.SetSchoolTitle(static_cast<School>(shortcut - 1),
                           {TextTable::kMagic, strings[title_index]},
                           kShortcutTitleWidth)) {
    return std::nullopt;
  }
  return RunSelection(state, host, shortcut - 1);
}
}  // namespace

// decomp: vs_menu0_exec (src/MENU/MENU0.PRG/15AC.c:166).
std::optional<int> Update(std::uint8_t& state, MenuHost& host) {
  if (state >= kInitWarlock && state <= kEnchanter) {
    return HandleSchool(state, host);
  }
  switch (state) {
    case kShortcut:
      return HandleShortcut(state, host);
    case kInitSubmenu:
      return InitializeSubmenu(state, host);
    case kSubmenu:
      return HandleSubmenu(state, host);
    case kTeleport: {
      const int result = host.RunTeleport(0);
      if (result > 0) {
        host.SetSelectionKind(kTeleportSelectionKind);
        host.SetActionSelection(result - 1);
        state = kExit;
      } else if (result < 0) {
        state = result == -2 ? kExit : kInitSubmenu;
      }
      break;
    }
    case kExitToMain:
    case kExit: {
      const bool exit = state == kExit;
      host.DismissInformation();
      host.SetCommand(0);
      if (host.RowsIdle()) {
        if (exit) {
          host.SetCurrentMenu(1);
        }
        state = kNone;
        return 1;
      }
      break;
    }
    case kExecute:
      if (host.RowsIdle()) {
        host.SetSelectionKind(kActionSelectionKind);
        const auto previous = host.CurrentMenu();
        host.SetCurrentMenu(1);
        host.SetReturnMenu(previous);
        state = kNone;
        return 1;
      }
      break;
    default:
      break;
  }
  return 0;
}
}  // namespace VS::Menu::Magic
