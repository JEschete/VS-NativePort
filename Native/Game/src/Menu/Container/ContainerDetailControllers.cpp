#include "Menu/Container/ContainerDetailControllers.hpp"

#include "Core/Math/ScalarMath.hpp"
#include "Input/Pad.hpp"

namespace VS::Menu::container {
namespace {
constexpr std::uint8_t kAnimationFrames = 10;
constexpr std::int32_t kWeaponComponentEnd = 6;
constexpr std::int32_t kShieldComponentEnd = 4;
constexpr std::uint32_t kExitButtons = Input::Triangle | Input::Cross;
constexpr std::int32_t kAllStatPages = 7;
constexpr std::int32_t kClassAffinityPages = 3;
constexpr std::int32_t kTypePage = 4;
constexpr std::uint8_t kFirstDetailElement = 9;
constexpr std::uint8_t kDetailCategoryCount = 6;
constexpr std::int32_t kReturnToBattle = -2;

enum class Animation { kComponents, kCountdown, kCountup };
struct Rules {
  std::int32_t stat_pages;
  Animation animation;
  std::int32_t component_end;
  std::uint8_t first_frame;
  bool clear_unused_byte;
  bool dp_pp;
  bool wait_for_rows;
  bool grip_slots;
  bool armor;
  bool gem;
};
constexpr Rules kWeapon{
    .stat_pages = kAllStatPages,
    .animation = Animation::kComponents,
    .component_end = kWeaponComponentEnd,
    .first_frame = 0,
    .clear_unused_byte = true,
    .dp_pp = true,
    .wait_for_rows = false,
    .grip_slots = false,
    .armor = false,
    .gem = false,
};
constexpr Rules kBlade{
    .stat_pages = kClassAffinityPages,
    .animation = Animation::kCountdown,
    .component_end = 1,
    .first_frame = kAnimationFrames,
    .clear_unused_byte = false,
    .dp_pp = true,
    .wait_for_rows = false,
    .grip_slots = false,
    .armor = false,
    .gem = false,
};
constexpr Rules kGrip{
    .stat_pages = kTypePage,
    .animation = Animation::kCountup,
    .component_end = 1,
    .first_frame = 0,
    .clear_unused_byte = true,
    .dp_pp = false,
    .wait_for_rows = false,
    .grip_slots = true,
    .armor = false,
    .gem = false,
};
constexpr Rules kShield{
    .stat_pages = kAllStatPages,
    .animation = Animation::kComponents,
    .component_end = kShieldComponentEnd,
    .first_frame = 0,
    .clear_unused_byte = true,
    .dp_pp = true,
    .wait_for_rows = true,
    .grip_slots = false,
    .armor = false,
    .gem = false,
};
constexpr Rules kArmor{
    .stat_pages = kAllStatPages,
    .animation = Animation::kCountdown,
    .component_end = 1,
    .first_frame = kAnimationFrames,
    .clear_unused_byte = false,
    .dp_pp = true,
    .wait_for_rows = false,
    .grip_slots = false,
    .armor = true,
    .gem = false,
};
constexpr Rules kGem{
    .stat_pages = kClassAffinityPages,
    .animation = Animation::kCountdown,
    .component_end = 1,
    .first_frame = kAnimationFrames,
    .clear_unused_byte = false,
    .dp_pp = false,
    .wait_for_rows = false,
    .grip_slots = false,
    .armor = false,
    .gem = true,
};

void Begin(ControllerState& state, ControllerWorld& world,
           const ControllerEntry& entry, const Rules& rules) {
  state.selected_row = static_cast<std::uint8_t>(entry.row.index);
  state.selected_page = static_cast<std::uint8_t>(entry.page.index);
  world.BeginTransition({state.selected_row});
  if (rules.clear_unused_byte) {
    state.unused_entry_byte = 0;
  }
  state.animation_step = rules.first_frame;
  state.stage = ControllerStage::kInitialize;
}

void Initialize(ControllerState& state, ControllerWorld& world,
                const Rules& rules) {
  if (world.RowsReady() == 0) {
    return;
  }
  world.InitializeLayout({state.selected_row}, rules.stat_pages);
  world.SetStatistics(world.CurrentItem({state.selected_page}));
  if (rules.armor) {
    if (world.SelectedArmorShowsDp(world.CurrentItem({state.selected_page}))) {
      world.OpenDpBar();
    }
  } else if (rules.dp_pp) {
    world.OpenDpPpBars();
  }
  state.stage = ControllerStage::kAnimate;
}

void Animate(ControllerState& state, ControllerWorld& world,
             const Rules& rules) {
  if (rules.animation == Animation::kCountdown) {
    if (state.animation_step != 0) {
      --state.animation_step;
    } else {
      state.stage = ControllerStage::kInput;
    }
  } else if (state.animation_step < kAnimationFrames) {
    ++state.animation_step;
    if (rules.animation == Animation::kComponents &&
        state.animation_step < rules.component_end) {
      world.CreateComponentRow(state.animation_step,
                               world.CurrentItem({state.selected_page}), true);
    }
  } else if (!rules.wait_for_rows || world.RowsReady() != 0) {
    state.stage = ControllerStage::kInput;
  }
}

void ChangeItem(ControllerState& state, ControllerWorld& world,
                const Rules& rules) {
  const DetailPage page = world.PollPaging({state.selected_page});
  if (page.index == state.selected_page) {
    return;
  }
  state.selected_page = static_cast<std::uint8_t>(page.index);
  const ContainerSlot item = world.SelectItem(page);
  const DetailRowText text = world.FormatItem(item);
  world.SetStatistics(item);
  world.ChangePage({state.selected_row}, text, page);
  for (std::int32_t row = 1; row < rules.component_end; ++row) {
    world.CreateComponentRow(row, world.CurrentItem(page), false);
  }
}

void PollInput(ControllerState& state, ControllerWorld& world,
               const Rules& rules) {
  if ((world.PressedButtons() & kExitButtons) != 0) {
    world.Exit(rules.dp_pp);
    state.stage = ControllerStage::kReturn;
    if (rules.gem) {
      world.RememberTriangleClose((world.PressedButtons() & Input::Triangle) !=
                                  0);
    }
  } else {
    ChangeItem(state, world, rules);
  }
  if (rules.grip_slots) {
    world.DrawGripGemSlotCount(
        world.GripGemSlots(world.CurrentItem({state.selected_page})));
  }
}

std::int32_t UpdateController(ControllerState& state, ControllerWorld& world,
                              std::optional<ControllerEntry> entry,
                              const Rules& rules) {
  if (entry.has_value()) {
    Begin(state, world, entry.value(), rules);
    return 0;
  }
  switch (state.stage) {
    case ControllerStage::kInitialize:
      Initialize(state, world, rules);
      break;
    case ControllerStage::kAnimate:
      Animate(state, world, rules);
      break;
    case ControllerStage::kInput:
      PollInput(state, world, rules);
      break;
    case ControllerStage::kReturn:
      return world.RowsReady();
    case ControllerStage::kUnrecognized:
      break;
  }
  return 0;
}

void EnterDispatcher(DispatcherState& state, DispatcherWorld& world,
                     const DispatchEntry& entry) {
  state.category = entry.category;
  world.SetViewMode(DetailViewMode::kDetails);
  world.RememberTriangleClose(false);
  world.ResetDetailCursor();
  const auto page = entry.selection.has_value()
                        ? static_cast<std::uint8_t>(Core::Math::WrappingAdd(
                              entry.selection.value().page.index, 1))
                        : 0;
  world.CategoryHeader().item_page = page;
  world.SetCommand(DetailCommand::kNone);
}
}  // namespace

// decomp: _weaponNavigation (src/MENU/MENUD.PRG/234.c:829-900).
std::int32_t UpdateWeaponDetails(ControllerState& state, ControllerWorld& world,
                                 std::optional<ControllerEntry> entry) {
  return UpdateController(state, world, entry, kWeapon);
}

// decomp: _bladeNavigation (src/MENU/MENUD.PRG/234.c:902-963).
std::int32_t UpdateBladeDetails(ControllerState& state, ControllerWorld& world,
                                std::optional<ControllerEntry> entry) {
  return UpdateController(state, world, entry, kBlade);
}

// decomp: _gripNavigation (src/MENU/MENUD.PRG/234.c:965-1035).
std::int32_t UpdateGripDetails(ControllerState& state, ControllerWorld& world,
                               std::optional<ControllerEntry> entry) {
  return UpdateController(state, world, entry, kGrip);
}

// decomp: _shieldNavigation (src/MENU/MENUD.PRG/234.c:1037-1108).
std::int32_t UpdateShieldDetails(ControllerState& state, ControllerWorld& world,
                                 std::optional<ControllerEntry> entry) {
  return UpdateController(state, world, entry, kShield);
}

// decomp: _armorNavigation (src/MENU/MENUD.PRG/234.c:1110-1174).
std::int32_t UpdateArmorDetails(ControllerState& state, ControllerWorld& world,
                                std::optional<ControllerEntry> entry) {
  return UpdateController(state, world, entry, kArmor);
}

// decomp: _gemNavigation (src/MENU/MENUD.PRG/234.c:1176-1233).
std::int32_t UpdateGemDetails(ControllerState& state, ControllerWorld& world,
                              std::optional<ControllerEntry> entry) {
  return UpdateController(state, world, entry, kGem);
}

// decomp: func_80105008 (src/MENU/MENUD.PRG/234.c:1235-1271).
std::int32_t UpdateItemDetails(DispatcherState& state, DispatcherWorld& world,
                               std::optional<DispatchEntry> entry) {
  if (entry.has_value()) {
    EnterDispatcher(state, world, entry.value());
  }
  if (static_cast<std::uint8_t>(state.category) >= kDetailCategoryCount) {
    world.Refuse("container detail category outside its six controllers");
  }
  const auto result = world.RunController(
      state.category,
      entry.has_value() ? entry.value().selection : std::nullopt);
  if (result != 0) {
    if (world.ClosedWithTriangle()) {
      return kReturnToBattle;
    }
    world.SetIconsVisibility(1);
    world.SetViewMode(DetailViewMode::kList);
    world.SetControlsVisibility(1);
    world.SetCommand(DetailCommand::kMenu);
  } else if (world.RowsReady() != 0) {
    world.SetCursorBright(world.SelectedDetailElement() != kFirstDetailElement);
    world.DrawDetailTabNavigation();
    world.DrawItemDetails();
  }
  return result;
}
}  // namespace VS::Menu::container
