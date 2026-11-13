#include "Menu/Inventory/EquipmentDetailControllers.hpp"

#include "Input/Pad.hpp"

namespace VS::Menu::Inventory {
namespace {
constexpr std::uint8_t kAnimationFrames = 10;
constexpr std::int32_t kWeaponComponentEnd = 6;
constexpr std::int32_t kShieldComponentEnd = 4;
constexpr std::uint32_t kExitButtons = Input::Triangle | Input::Cross;

enum class DetailAnimation { kComponents, kCountdown, kCountup };
struct ControllerRules {
  DetailStatPages pages;
  DetailAnimation animation;
  std::int32_t component_end;
  std::uint8_t first_frame;
  bool clear_unused_byte;
  bool dp_pp;
  bool wait_for_rows;
  bool draw_grip_slots;
};

constexpr ControllerRules kWeaponRules{
    .pages = DetailStatPages::kAll,
    .animation = DetailAnimation::kComponents,
    .component_end = kWeaponComponentEnd,
    .first_frame = 0,
    .clear_unused_byte = true,
    .dp_pp = true,
    .wait_for_rows = false,
    .draw_grip_slots = false,
};
constexpr ControllerRules kBladeRules{
    .pages = DetailStatPages::kClassAndAffinity,
    .animation = DetailAnimation::kCountdown,
    .component_end = 1,
    .first_frame = kAnimationFrames,
    .clear_unused_byte = false,
    .dp_pp = true,
    .wait_for_rows = false,
    .draw_grip_slots = false,
};
constexpr ControllerRules kGripRules{
    .pages = DetailStatPages::kType,
    .animation = DetailAnimation::kCountup,
    .component_end = 1,
    .first_frame = 0,
    .clear_unused_byte = true,
    .dp_pp = false,
    .wait_for_rows = false,
    .draw_grip_slots = true,
};
constexpr ControllerRules kShieldRules{
    .pages = DetailStatPages::kAll,
    .animation = DetailAnimation::kComponents,
    .component_end = kShieldComponentEnd,
    .first_frame = 0,
    .clear_unused_byte = true,
    .dp_pp = true,
    .wait_for_rows = true,
    .draw_grip_slots = false,
};

void Begin(EquipmentDetailState& state, EquipmentDetailWorld& world,
           DetailSelection selection, std::uint8_t first_frame,
           bool clear_unused_byte) {
  state.selected_row = static_cast<std::uint8_t>(selection.row.index);
  state.selected_page = static_cast<std::uint8_t>(selection.page.index);
  world.BeginTransition(DetailRowId{state.selected_row});
  if (clear_unused_byte) {
    state.unused_entry_byte = 0;
  }
  state.animation_step = first_frame;
  state.stage = DetailStage::kInitialize;
}

void Initialize(EquipmentDetailState& state, EquipmentDetailWorld& world,
                DetailStatPages pages, bool show_dp_pp) {
  if (world.RowsReady() == 0) {
    return;
  }
  world.InitializeLayout(DetailRowId{state.selected_row}, pages);
  world.SetStatistics(world.CurrentItem(DetailPage{state.selected_page}));
  if (show_dp_pp) {
    world.OpenDpPpBars();
  }
  state.stage = DetailStage::kAnimate;
}

bool ExitRequested(EquipmentDetailState& state, EquipmentDetailWorld& world,
                   bool clear_dp_pp) {
  if ((world.PressedButtons() & kExitButtons) == 0) {
    return false;
  }
  world.Exit(clear_dp_pp);
  state.stage = DetailStage::kReturn;
  return true;
}

void ChangeItem(EquipmentDetailState& state, EquipmentDetailWorld& world,
                std::int32_t component_end) {
  const DetailPage page = world.PollPaging(DetailPage{state.selected_page});
  if (page.index == state.selected_page) {
    return;
  }
  state.selected_page = static_cast<std::uint8_t>(page.index);
  const InventorySlot slot = world.SelectItem(page);
  const DetailRowView text = world.FormatItem(slot);
  world.SetStatistics(slot);
  world.ChangePage(DetailRowId{state.selected_row}, text, page);
  for (std::int32_t row = 1; row < component_end; ++row) {
    world.CreateComponentRow(row, world.CurrentItem(page), false);
  }
}

void AnimateComponents(EquipmentDetailState& state, EquipmentDetailWorld& world,
                       std::int32_t component_end) {
  ++state.animation_step;
  if (state.animation_step < component_end) {
    world.CreateComponentRow(state.animation_step,
                             world.CurrentItem(DetailPage{state.selected_page}),
                             true);
  }
}
void Animate(EquipmentDetailState& state, EquipmentDetailWorld& world,
             const ControllerRules& rules) {
  if (rules.animation == DetailAnimation::kCountdown) {
    if (state.animation_step != 0) {
      --state.animation_step;
    } else {
      state.stage = DetailStage::kInput;
    }
  } else if (state.animation_step < kAnimationFrames) {
    if (rules.animation == DetailAnimation::kComponents) {
      AnimateComponents(state, world, rules.component_end);
    } else {
      ++state.animation_step;
    }
  } else if (!rules.wait_for_rows || world.RowsReady() != 0) {
    state.stage = DetailStage::kInput;
  }
}

std::int32_t UpdateController(EquipmentDetailState& state,
                              EquipmentDetailWorld& world,
                              std::optional<DetailSelection> selection,
                              const ControllerRules& rules) {
  if (selection) {
    Begin(state, world, selection.value(), rules.first_frame,
          rules.clear_unused_byte);
    return 0;
  }
  switch (state.stage) {
    case DetailStage::kInitialize:
      Initialize(state, world, rules.pages, rules.dp_pp);
      break;
    case DetailStage::kAnimate:
      Animate(state, world, rules);
      break;
    case DetailStage::kInput:
      if (!ExitRequested(state, world, rules.dp_pp)) {
        ChangeItem(state, world, rules.component_end);
      }
      if (rules.draw_grip_slots) {
        world.DrawGripGemSlotCount(world.GripGemSlots(
            world.CurrentItem(DetailPage{state.selected_page})));
      }
      break;
    case DetailStage::kReturn:
      return world.RowsReady();
  }
  return 0;
}
}  // namespace

// decomp: _weaponStatusView (src/MENU/items1.h:98-185).
std::int32_t UpdateWeaponDetails(EquipmentDetailState& state,
                                 EquipmentDetailWorld& world,
                                 std::optional<DetailSelection> selection) {
  return UpdateController(state, world, selection, kWeaponRules);
}

// decomp: _bladeStatusView (src/MENU/items1.h:195-265).
std::int32_t UpdateBladeDetails(EquipmentDetailState& state,
                                EquipmentDetailWorld& world,
                                std::optional<DetailSelection> selection) {
  return UpdateController(state, world, selection, kBladeRules);
}

// decomp: _gripStatusView (src/MENU/items1.h:275-355).
std::int32_t UpdateGripDetails(EquipmentDetailState& state,
                               EquipmentDetailWorld& world,
                               std::optional<DetailSelection> selection) {
  return UpdateController(state, world, selection, kGripRules);
}

// decomp: _shieldStatusView (src/MENU/items1.h:365-454).
std::int32_t UpdateShieldDetails(EquipmentDetailState& state,
                                 EquipmentDetailWorld& world,
                                 std::optional<DetailSelection> selection) {
  return UpdateController(state, world, selection, kShieldRules);
}
}  // namespace VS::Menu::Inventory
