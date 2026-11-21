#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "Menu/Framework/ItemRows.hpp"
#include "Menu/Status/TerminalControllers.hpp"

// NOLINTNEXTLINE(readability-identifier-naming): the catalog's component.
namespace VS::Menu::Status {
namespace {
constexpr std::uint32_t kPreviousActor = 4;
constexpr std::uint32_t kNextActor = 8;
constexpr std::uint32_t kShoulders = kPreviousActor | kNextActor;
constexpr std::uint32_t kTriangle = 16;
constexpr std::uint32_t kCross = 64;
constexpr int kRows = 9;
constexpr int kEquipmentRowBase = 10;
constexpr int kFirstComponent = 21;
constexpr int kComponentEnd = 26;
constexpr int kTitleRow = 32;
constexpr int kNameRow = 4;
constexpr int kAnimationFrames = 10;
constexpr int kModelSlide = 128;
constexpr int kMainElement = 9;
constexpr int kStatElementEnd = 16;
constexpr int kPrimaryElement = 18;
constexpr int kWeaponParts = 6;
constexpr int kShieldParts = 4;
constexpr int kNoneTextOffset = 230;
constexpr int kNothingTextOffset = 233;
constexpr std::uint32_t kWeaponStyle = 0xF000;
constexpr std::uint32_t kShieldStyle = 0xF200;
constexpr std::uint32_t kAccessoryStyle = 0xF400;
constexpr int kLimbLabelBase = 103;
constexpr int kRowLabelShift = 9;
constexpr int kWeaponDetailFlags = 114;
constexpr int kShieldDetailFlags = 96;
constexpr int kArmorDetailFlags = 64;
constexpr int kIconShift = 26;
constexpr int kMaterialShift = 16;
constexpr int kMaterialMask = 7;
constexpr int kArmorTitle = 11;
constexpr int kWeaponIcon = 24;
constexpr int kShieldIcon = 27;
constexpr int kArmorIcon = 28;
constexpr int kDetailX = 155;
constexpr int kDetailY = 18;
constexpr int kDetailWidth = 165;
constexpr int kTitleX = 16;
constexpr int kTitleWidth = 164;
constexpr int kTitleBackground = 8;
constexpr int kNameTarget = 180;
constexpr int kShieldCursorBias = 7;
constexpr int kStaticBars = 8;
constexpr int kComparisonActorBase = 15;
constexpr int kForwardActorRequest = 17;
constexpr int kAllDetailPages = 7;
namespace list_phase {
constexpr std::uint8_t kWaitingSwitch = 3;
constexpr std::uint8_t kReturningRow = 4;
constexpr std::uint8_t kDetails = 5;
}  // namespace list_phase

[[nodiscard]] std::unexpected<TerminalError> Unavailable() {
  return std::unexpected(TerminalError::kUnavailableService);
}
[[nodiscard]] int Count(const TerminalActor& actor) {
  return EquipmentRowCount(actor.limb_names, actor.accessory_id);
}
[[nodiscard]] bool Equipped(const TerminalActor& actor, int row) {
  const int limbs = LimbCount(actor.limb_names);
  return row >= limbs + 2 ||
         actor.equipment_ids[static_cast<std::size_t>(row)] != 0;
}
[[nodiscard]] int Bars(int row, int limbs, bool stationary) {
  const int kind = row < 2 ? 3 : (row - 2 < limbs ? 1 : 0);
  return kind | (stationary ? kStaticBars : 0);
}
[[nodiscard]] bool ShowEquipment(EquipmentTerminalHost& host, int row,
                                 int limbs, bool stationary) {
  return host.DpPpBars(Bars(row, limbs, stationary)) &&
         host.EquipmentStats(row);
}
[[nodiscard]] bool Parts(EquipmentTerminalHost& host, int equipment) {
  const int end = equipment == 0 ? kWeaponParts : kShieldParts;
  for (int part = 1; part < end; ++part) {
    if (!host.ComponentRow(equipment, part, false)) {
      return false;
    }
  }
  return true;
}
[[nodiscard]] bool DetailCursor(EquipmentDetailState& state,
                                EquipmentTerminalHost& host, int element) {
  const auto positions = host.Tables().detail_cursor_positions;
  if (element < 0 || static_cast<std::size_t>(element) >= positions.size()) {
    return false;
  }
  std::int32_t position = positions[static_cast<std::size_t>(element)];
  if (state.selected_row == 1 &&
      static_cast<std::uint32_t>(state.selected_element - kAnimationFrames) <
          2U) {
    position = Core::Math::WrappingAdd(position, kShieldCursorBias);
  }
  host.Frame().cursor_color = state.selected_element != kMainElement;
  const auto cursor = host.Cursor(state.cursor_animation, position);
  if (!cursor) {
    return false;
  }
  state.cursor_animation = static_cast<std::uint8_t>(*cursor);
  return true;
}
[[nodiscard]] bool DetailTitle(EquipmentTerminalHost& host, int row) {
  const auto text = host.StatusText(row < 2 ? row + kMainElement : kArmorTitle);
  if (text.empty()) {
    return false;
  }
  auto* title = host.CreateRow(
      {kTitleRow, kTitleX, kDetailY, kTitleWidth, kTitleBackground}, text);
  if (title == nullptr) {
    return false;
  }
  title->selected = 1;
  title->row_icon = static_cast<std::uint8_t>(
      row == 0 ? kWeaponIcon : (row == 1 ? kShieldIcon : kArmorIcon));
  return true;
}
[[nodiscard]] bool ChangeEquipment(EquipmentDetailState& state,
                                   EquipmentTerminalHost& host, int row,
                                   int limbs) {
  if (!host.Sound(TerminalSound::kChange)) {
    return false;
  }
  for (int item = kEquipmentRowBase; item < kComponentEnd; ++item) {
    if (!host.Deactivate(item)) {
      return false;
    }
  }
  state.selected_row = static_cast<std::uint8_t>(row);
  if (!ShowEquipment(host, row, limbs, true)) {
    return false;
  }
  if (row < 2 && !Parts(host, row)) {
    return false;
  }
  const auto text = host.FormatRow(row, host.DescriptionBuffer());
  if (!text || text->name.empty() || text->description.empty()) {
    return false;
  }
  auto* item = host.CreateRow(
      {row + kEquipmentRowBase, kDetailX, kDetailY, kDetailWidth, 0},
      text->name);
  if (item == nullptr) {
    return false;
  }
  item->selected = 1;
  item->row_icon = static_cast<std::uint8_t>(text->flags >> kIconShift);
  item->material = static_cast<std::uint8_t>((text->flags >> kMaterialShift) &
                                             kMaterialMask);
  if (!(host.Information(text->description) && DetailTitle(host, row))) {
    return false;
  }
  state.selected_element = kMainElement;
  if (!DetailCursor(state, host, kMainElement)) {
    return false;
  }
  host.Frame().freeze_tabs = 1;
  return host.Tabs();
}

[[nodiscard]] std::span<const std::uint8_t> DetailHelp(
    EquipmentTerminalHost& host, int row, int element, int limbs) {
  if (element < 2) {
    return host.HelpText(Framework::item_text::kPhantomPointsDesc - element);
  }
  if (element < kMainElement) {
    constexpr std::array kHelpBases{414, 420, 427};
    const auto page = host.Frame().stat_page;
    return page < kHelpBases.size() ? host.HelpText(element + kHelpBases[page])
                                    : std::span<const std::uint8_t>{};
  }
  if (element < kStatElementEnd) {
    return host.DescribeComponent(row, element - kMainElement);
  }
  int index = element + Framework::item_text::kHumanClassDesc;
  if (element >= kPrimaryElement) {
    if (row == 0) {
      index = element + Framework::item_text::kShortGrip - 1;
    } else if (row - 2 < limbs) {
      // The signed comparison also includes the shield (row 1).
      constexpr int kPolearmGrip = 643;
      index = element + kPolearmGrip;
    }
  }
  return host.HelpText(index);
}

[[nodiscard]] TerminalResult NavigateEquipment(EquipmentDetailState& state,
                                               EquipmentTerminalHost& host,
                                               const TerminalActor& actor) {
  auto& frame = host.Frame();
  const int limbs = LimbCount(actor.limb_names);
  const auto rows = host.Rows();
  if (rows.size() <= kTitleRow) {
    return std::unexpected(TerminalError::kMissingRows);
  }
  for (int row = kFirstComponent; row < kComponentEnd; ++row) {
    rows[static_cast<std::size_t>(row)].selected = 0;
  }
  const int equipment = state.selected_row;
  std::uint32_t flags = equipment - 2 < limbs ? kArmorDetailFlags : 0;
  if (!host.DpPpBars(Bars(equipment, limbs, true))) {
    return Unavailable();
  }
  if (equipment < 2) {
    if (!host.EquipmentStats(equipment)) {
      return Unavailable();
    }
    flags = equipment == 0 ? actor.weapon_gem_slots + kWeaponDetailFlags
                           : actor.shield_gem_slots + kShieldDetailFlags;
  }
  const auto selection = host.NavigateDetail(state.selected_element, flags);
  if (!selection || *selection < 0 ||
      static_cast<std::size_t>(*selection) >=
          host.Tables().detail_cursor_positions.size()) {
    return std::unexpected(TerminalError::kInvalidSelection);
  }
  if (*selection != state.selected_element) {
    if (!host.Sound(TerminalSound::kChange)) {
      return Unavailable();
    }
    state.selected_element = static_cast<std::uint8_t>(*selection);
  }
  const auto text = DetailHelp(host, equipment, *selection, limbs);
  if (text.empty()) {
    return std::unexpected(TerminalError::kMissingText);
  }
  if (!(host.Information(text) && DetailCursor(state, host, *selection))) {
    return Unavailable();
  }
  if ((frame.pressed & kTriangle) != 0) {
    if (!host.ExitDetail()) {
      return Unavailable();
    }
    return -1;
  }
  if ((frame.pressed & kCross) != 0) {
    if (!(host.BeginVitals(frame.selected_actor - 1) && host.ExitDetail() &&
          host.CommandLabel(Framework::MenuCommand::kMenu))) {
      return Unavailable();
    }
    return state.selected_row + 1;
  }
  frame.freeze_tabs = 1;
  return host.Tabs() ? TerminalResult{0} : TerminalResult{Unavailable()};
}

[[nodiscard]] TerminalResult DetailInput(EquipmentDetailState& state,
                                         EquipmentTerminalHost& host,
                                         const TerminalActor& actor) {
  const auto& frame = host.Frame();
  if ((frame.held & kShoulders) != kShoulders) {
    const int count = Count(actor);
    int step = (frame.repeat & kPreviousActor) != 0 ? count - 1 : 0;
    if ((frame.repeat & kNextActor) != 0) {
      step = 1;
    }
    int row = state.selected_row;
    for (int visited = 0; visited < count; ++visited) {
      row += step;
      if (row >= count) {
        row -= count;
      }
      if (row == state.selected_row) {
        break;
      }
      if (Equipped(actor, row)) {
        return ChangeEquipment(state, host, row, LimbCount(actor.limb_names))
                   ? TerminalResult{0}
                   : TerminalResult{Unavailable()};
      }
    }
  }
  return NavigateEquipment(state, host, actor);
}

[[nodiscard]] TerminalResult DetailEntering(EquipmentDetailState& state,
                                            EquipmentTerminalHost& host) {
  auto& frame = host.Frame();
  if (state.row_to_render >= kAnimationFrames) {
    state.phase = 2;
    return 0;
  }
  const auto offsets = host.Tables().row_offsets;
  const auto next = static_cast<std::uint8_t>(state.row_to_render + 1);
  const auto offset = static_cast<std::size_t>(kAnimationFrames - next);
  if (offset >= offsets.size()) {
    return std::unexpected(TerminalError::kInvalidAnimation);
  }
  state.row_to_render = next;
  frame.animation_step = kModelSlide - offsets[offset];
  if (next < kWeaponParts) {
    if (frame.vitals_slide != 0) {
      --frame.vitals_slide;
    }
    frame.vitals_phase = 0;
    if (!host.HideHud()) {
      return Unavailable();
    }
    if (state.selected_row < 2 &&
        !host.ComponentRow(state.selected_row, next, true)) {
      return Unavailable();
    }
  }
  return 0;
}

[[nodiscard]] TerminalResult BuildList(EquipmentListState& state,
                                       EquipmentTerminalHost& host,
                                       const TerminalActor& actor) {
  auto& frame = host.Frame();
  const int limbs = LimbCount(actor.limb_names);
  const int count = Count(actor);
  const auto none = host.StatusText(kNoneTextOffset, false);
  const auto nothing = host.StatusText(kNothingTextOffset, false);
  if (none.empty() || nothing.empty()) {
    return std::unexpected(TerminalError::kMissingText);
  }
  std::array<TerminalRowText, kRows> rows{};
  std::array<std::int32_t, kRows> flags{};
  std::array<std::array<std::uint8_t, Framework::kRowDescriptionBufferBytes>,
             kRows>
      descriptions{};
  const std::uint32_t disabled =
      static_cast<std::uint32_t>(!actor.analyzed && frame.selected_actor != 1);
  for (int index = 0; index < count; ++index) {
    const auto row = static_cast<std::size_t>(index);
    rows[row] = {none, nothing, 1};
    if (Equipped(actor, index)) {
      const auto formatted = host.FormatRow(index, descriptions[row]);
      if (!formatted || formatted->name.empty() ||
          formatted->description.empty()) {
        return std::unexpected(TerminalError::kMissingText);
      }
      rows[row] = *formatted;
    }
    std::uint32_t style = kAccessoryStyle | disabled;
    if (index < 2) {
      style = (index == 0 ? kWeaponStyle : kShieldStyle) | disabled;
    } else if (index - 2 < limbs) {
      style = ((static_cast<std::uint32_t>(actor.limb_names[row - 2]) +
                kLimbLabelBase)
               << kRowLabelShift) +
              disabled;
    }
    flags[row] = static_cast<std::int32_t>(rows[row].flags | style);
  }
  const auto cursor_memory = frame.cursor_memory;
  frame.cursor_memory = 1;
  const bool created =
      host.CreateList(std::span(rows).first(static_cast<std::size_t>(count)),
                      std::span(flags).first(static_cast<std::size_t>(count)));
  frame.cursor_memory = cursor_memory;
  if (!created) {
    return Unavailable();
  }
  state.timer = frame.animation_step != 0 ? kAnimationFrames : 0;
  state.phase = 1;
  return 0;
}

[[nodiscard]] TerminalResult ListInput(EquipmentListState& state,
                                       EquipmentDetailState& detail,
                                       EquipmentTerminalHost& host,
                                       const TerminalActor& actor) {
  auto& frame = host.Frame();
  const auto selected = host.SelectedRow();
  if (!selected) {
    return Unavailable();
  }
  const int result = Core::Math::WrappingAdd(*selected, 1);
  if (result != 0) {
    if (!(host.ComparisonStats(frame.selected_actor + kComparisonActorBase) &&
          host.ClearRows(kNameRow))) {
      return Unavailable();
    }
    if (result < 0) {
      const int limbs = LimbCount(actor.limb_names);
      int limb = std::max(static_cast<int>(frame.selected_equipment) - 2, 0);
      if (limb >= limbs) {
        limb = limbs - 1;
      }
      frame.selected_element = static_cast<std::uint8_t>(limb);
      return result;
    }
    const auto entered = UpdateEquipmentDetails(detail, host, result);
    if (!entered) {
      return entered;
    }
    state.phase = list_phase::kDetails;
    return 0;
  }
  if ((frame.held & kShoulders) == kShoulders) {
    return 0;
  }
  int actor_slot = frame.selected_actor - 1;
  int switch_flags = 0;
  for (const auto button : {kPreviousActor, kNextActor}) {
    if ((frame.pressed & button) == 0) {
      continue;
    }
    const auto next =
        host.NextActor(actor_slot, button == kPreviousActor ? 14 : 1);
    if (!next) {
      return Unavailable();
    }
    actor_slot = *next;
    switch_flags = button == kPreviousActor ? 1 : kForwardActorRequest;
  }
  if (actor_slot == frame.selected_actor - 1) {
    return 0;
  }
  frame.previous_actor = frame.selected_actor;
  if (!(host.BeginVitals(actor_slot) &&
        host.SwitchActor(actor_slot + switch_flags).has_value() &&
        host.StoreCursor() && host.ClearRows(kNameRow))) {
    return Unavailable();
  }
  state.phase = list_phase::kWaitingSwitch;
  return 0;
}
}  // namespace

// decomp: equipmentDetailScreen (src/MENU/MENU4.PRG/120.c:1485; was
// _equipmentDetailScreen)
TerminalResult UpdateEquipmentDetails(EquipmentDetailState& state,
                                      EquipmentTerminalHost& host, int row) {
  auto& frame = host.Frame();
  const auto actor =
      frame.selected_actor > 0 && frame.selected_actor <= kInspectableActorSlots
          ? host.Actor(frame.selected_actor - 1)
          : std::nullopt;
  if (!actor) {
    return std::unexpected(TerminalError::kMissingActor);
  }
  const auto requested =
      static_cast<std::uint8_t>(static_cast<std::uint32_t>(row) - 1U);
  if ((row != 0 && requested >= Count(*actor)) ||
      (row == 0 && state.phase <= 2 && state.selected_row >= Count(*actor))) {
    return std::unexpected(TerminalError::kInvalidSelection);
  }
  if (row != 0) {
    state.selected_row = requested;
    frame.vitals_phase = 0;
    if (!(host.HideHud() && host.HoistRow(state.selected_row) &&
          host.CommandLabel(Framework::MenuCommand::kNone))) {
      return Unavailable();
    }
    state.selected_element = kMainElement;
    state.row_to_render = 0;
    state.phase = 0;
    return 0;
  }
  switch (state.phase) {
    case 0: {
      if (frame.vitals_slide != 0) {
        --frame.vitals_slide;
      }
      const auto ready = host.MenuReady();
      if (!ready) {
        return Unavailable();
      }
      if (!*ready) {
        return 0;
      }
      if (!(host.InitializeDetail(state.selected_row, kAllDetailPages) &&
            ShowEquipment(host, state.selected_row,
                          LimbCount(actor->limb_names), false))) {
        return Unavailable();
      }
      state.phase = 1;
      return 0;
    }
    case 1:
      return DetailEntering(state, host);
    case 2:
      return DetailInput(state, host, *actor);
    default:
      return 0;
  }
}

// decomp: equipmentScreen (src/MENU/MENU4.PRG/120.c:1877; was _equipmentScreen)
TerminalResult UpdateEquipmentList(EquipmentListState& state,
                                   EquipmentDetailState& detail,
                                   EquipmentTerminalHost& host, int element) {
  auto& frame = host.Frame();
  const auto actor =
      frame.selected_actor > 0 && frame.selected_actor <= kInspectableActorSlots
          ? host.Actor(frame.selected_actor - 1)
          : std::nullopt;
  if (!actor) {
    return std::unexpected(TerminalError::kMissingActor);
  }
  if (element != 0) {
    frame.selected_equipment =
        static_cast<std::uint8_t>(static_cast<std::uint32_t>(element) + 1U);
    state.phase = 0;
    return 0;
  }
  switch (state.phase) {
    case 0:
      return BuildList(state, host, *actor);
    case 1:
      if (state.timer == 0) {
        state.phase = 2;
      } else {
        const auto offsets = host.Tables().row_offsets;
        if (state.timer > offsets.size()) {
          return std::unexpected(TerminalError::kInvalidAnimation);
        }
        frame.animation_step = offsets[--state.timer];
      }
      break;
    case 2:
      return ListInput(state, detail, host, *actor);
    case list_phase::kWaitingSwitch: {
      const auto complete = host.SwitchActor(0);
      if (!complete) {
        return Unavailable();
      }
      if (*complete != 0) {
        state.phase = list_phase::kReturningRow;
      }
      break;
    }
    case list_phase::kReturningRow: {
      const auto ready = host.MenuReady();
      if (!ready) {
        return Unavailable();
      }
      if (!*ready) {
        break;
      }
      const auto rows = host.Rows();
      if (rows.size() <= kNameRow) {
        return std::unexpected(TerminalError::kMissingRows);
      }
      auto& name = rows[kNameRow];
      if (name.x > kNameTarget) {
        name.state = Framework::RowState::kSlideX;
        name.target0 = kNameTarget;
        name.selected = 1;
      }
      state.phase = 0;
      break;
    }
    case list_phase::kDetails: {
      const auto result = UpdateEquipmentDetails(detail, host, 0);
      if (!result) {
        return result;
      }
      if (*result < 0) {
        return -2;
      }
      if (*result != 0) {
        frame.selected_equipment = static_cast<std::uint8_t>(*result - 1);
        state.phase = list_phase::kReturningRow;
      }
      break;
    }
    default:
      break;
  }
  return 0;
}
}  // namespace VS::Menu::Status
