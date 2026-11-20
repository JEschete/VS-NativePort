// Hand-maintained (tools/port/owned.csv).
// The equipment menus' choice list and preview model reload (MENU3.PRG
// 16C.c:39-336 and 68C4.c:12-127), over typed state and hosts.
#include "Menu/Inventory/EquipmentRows.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "Core/Math/ScalarMath.hpp"

namespace VS::Menu::Inventory {
namespace {
using Content::ActorAssetRequest;
using Core::Math::WrappingAdd;
using Core::Math::WrappingMultiply;
using Framework::ItemText;
using Framework::MenuRow;
using Framework::RowPlacement;
using Framework::RowState;

// VS::Core::TextCode::kTerminator twice and VS::Core::TextCode::kChunkSize.
constexpr std::uint8_t kTerminator = 0xE7;
constexpr std::uint8_t kChunkSize = 0xF8;
constexpr std::size_t kDescriptionCopyOffset = 2;

// The pad bits (libetc PADRup, PADRright, PADRdown, PADLup, PADLdown).
constexpr std::uint32_t kTriangle = 1U << 4;
constexpr std::uint32_t kCircle = 1U << 5;
constexpr std::uint32_t kCross = 1U << 6;
constexpr std::uint32_t kUp = 1U << 12;
constexpr std::uint32_t kDown = 1U << 14;
// hasSfx's bit the Triangle path tests (always clear in practice).
constexpr std::int32_t kSilentLeave = 2;
constexpr std::int32_t kFirstChoiceSound = 0x22;
constexpr std::int32_t kOtherChoiceSound = 0x21;

// The first row's and the sliding rows' placement (16C.c:93, 121, 239).
constexpr RowPlacement kTitleRow{kEquipmentFirstRow, 155, 18, 165, 0};
constexpr int kSlideX = 320;
constexpr int kRowX = 169;
constexpr int kRowY = 18;
constexpr int kRowPitch = 16;
constexpr int kRowWidth = 151;
constexpr std::int16_t kRowTarget = 169;
constexpr std::uint8_t kMoreBelow = 2;
constexpr std::uint8_t kMoreAbove = 1;
// The scrolling list's cursor rows: up from 2, down from 4.
constexpr std::int32_t kScrollUpRow = 2;
constexpr std::int32_t kScrollDownRow = 4;
constexpr std::int32_t kLastVisibleRow = kEquipmentVisibleRows - 1;
// The cursor's packed position (16C.c:298).
constexpr std::uint32_t kCursorX = 0x9B;
constexpr std::uint32_t kCursorY = 10;
constexpr std::uint32_t kFirstRowCursorLift = 14;
constexpr int kPositionShift = 16;

// The row flags' fields (16C.c:95-99, 131-139).
constexpr int kIconShift = 26;
constexpr int kOutsetShift = 9;
constexpr std::int32_t kOutsetMask = 0x7F;
constexpr std::int32_t kOutsetBias = 100;
constexpr int kMaterialShift = 16;
constexpr std::int32_t kMaterialMask = 7;
constexpr int kEquippedShift = 19;
constexpr std::int32_t kEquippedMask = 0x7F;

// _populateDataSlot's request (68C4.c:100-127).
constexpr std::uint8_t kEquipmentModelKind = 3;
constexpr std::int32_t kFirstShieldModel = 96;
constexpr std::uint8_t kWeaponTextureSlot = 240;
constexpr std::uint8_t kShieldTextureSlot = 241;
// func_80109320's categories (68C4.c:87): below 3 one-handed, 8 two-handed.
constexpr std::int32_t kFirstTwoHandedCategory = 3;
constexpr std::int32_t kTwoHandedCategory = 8;
// _loadObjectData's weapon models, animations and preview slots.
constexpr int kWeaponModel = 0;
constexpr int kShieldModel = 1;
constexpr int kFirstPreviewWeaponModel = 2;
constexpr int kPreviewWeaponModels = 2;
constexpr std::int32_t kShieldStance = 10;
constexpr int kPreviewIdleAnimation = 1;
constexpr int kSourceActorSlot = 0;
// _loadObjectData's steps (68C4.c:18-84).
constexpr std::uint8_t kReloadQueue = 0;
constexpr std::uint8_t kReloadWait = 1;
constexpr std::uint8_t kReloadPose = 2;
constexpr std::uint8_t kReloadSettle = 3;

// VS::Battle::characterState->equipmentModels' slot a model loads into.
[[nodiscard]] int ModelIndex(std::int32_t model_id) {
  return model_id < kFirstShieldModel ? 0 : 1;
}

[[nodiscard]] std::uint8_t RowIcon(std::int32_t flags) {
  return static_cast<std::uint8_t>(flags >> kIconShift);
}

[[nodiscard]] std::uint8_t Material(std::int32_t flags) {
  return static_cast<std::uint8_t>((flags >> kMaterialShift) & kMaterialMask);
}

[[nodiscard]] std::uint8_t Equipped(std::int32_t flags) {
  return static_cast<std::uint8_t>((flags >> kEquippedShift) & kEquippedMask);
}

// The flags fields a sliding or scrolled row takes (16C.c:129-139, 251-259).
void ApplyRowFlags(MenuRow& row, std::int32_t flags) {
  row.unselectable = static_cast<std::uint8_t>(flags & 1);
  row.row_icon = RowIcon(flags);
  const std::int32_t outset = (flags >> kOutsetShift) & kOutsetMask;
  if (outset != 0) {
    row.outset_icon = static_cast<std::uint8_t>(outset - kOutsetBias);
  }
  row.material = Material(flags);
}

[[nodiscard]] int RowY(std::int32_t row) {
  return WrappingAdd(WrappingMultiply(row, kRowPitch), kRowY);
}

void CopyName(ItemText& record, std::span<const std::uint8_t> name) {
  if (name.empty()) {
    record.name[0] = kTerminator;
    record.name[1] = kTerminator;
    return;
  }
  std::copy_n(name.begin(), kEquipmentNameBytes, record.name.begin());
  record.name[kEquipmentNameBytes] = kTerminator;
  record.name[kEquipmentNameBytes + 1] = kTerminator;
}

void CopyDescription(ItemText& record,
                     std::span<const std::uint8_t> description) {
  if (description.empty()) {
    record.description[0] = kTerminator;
    record.description[1] = kTerminator;
    return;
  }
  record.description[0] = kChunkSize;
  record.description[1] = 0;
  std::copy_n(description.begin(), kEquipmentDescriptionBytes,
              record.description.begin() + kDescriptionCopyOffset);
  constexpr std::size_t kEnd =
      kDescriptionCopyOffset + kEquipmentDescriptionBytes;
  record.description[kEnd] = kTerminator;
  record.description[kEnd + 1] = kTerminator;
}

[[nodiscard]] bool ValidSource(const EquipmentRowSource& source) {
  return (source.name.empty() || source.name.size() >= kEquipmentNameBytes) &&
         (source.description.empty() ||
          source.description.size() >= kEquipmentDescriptionBytes);
}

void CloseList(EquipmentRowsState& state, EquipmentRowsHost& host,
               std::int32_t selected) {
  host.FreeText(state.text);
  state.text = {};
  state.selected_row = selected;
}

// _processEquipSubMenu's slide-in steps 0-5 (16C.c:115-148).
void SlideRowIn(EquipmentRowsState& state, EquipmentRowsHost& host) {
  const std::uint8_t step = state.step;
  const ItemText& record = state.text[step];
  MenuRow& row = host.SetRow(
      {kEquipmentFirstRow + step, kSlideX, RowY(step), kRowWidth, 0},
      record.name);
  row.state = RowState::kSlideX;
  row.target0 = kRowTarget;
  ApplyRowFlags(row, record.flags);
  ++state.step;
  if (state.step == kEquipmentVisibleRows) {
    if (state.row_count > kEquipmentVisibleRows) {
      row.is_scrollable = kMoreBelow;
    }
    state.step = kEquipmentRowsWaiting;
  }
}

// The cursor moves of 16C.c:213-237.
void MoveUp(EquipmentRowsState& state) {
  if (state.row_count <= kEquipmentVisibleRows || state.first_row == 0) {
    if (state.selected_row != 0) {
      state.selected_row = WrappingAdd(state.selected_row, -1);
    }
  } else if (state.selected_row == kScrollUpRow) {
    --state.first_row;
  } else {
    state.selected_row = WrappingAdd(state.selected_row, -1);
  }
}

void MoveDown(EquipmentRowsState& state) {
  const std::int32_t count = state.row_count;
  if (state.row_count <= kEquipmentVisibleRows) {
    if (state.selected_row < count - 1) {
      state.selected_row = WrappingAdd(state.selected_row, 1);
    }
  } else if (state.first_row == count - kEquipmentVisibleRows) {
    if (state.selected_row < kLastVisibleRow) {
      state.selected_row = WrappingAdd(state.selected_row, 1);
    }
  } else if (state.selected_row == kScrollDownRow) {
    ++state.first_row;
  } else {
    state.selected_row = WrappingAdd(state.selected_row, 1);
  }
}

// Redraws rows 1-5 at the new scroll position, keeping each record's
// gradient (16C.c:242-273).
void ScrollRows(EquipmentRowsState& state, EquipmentRowsHost& host,
                std::uint8_t previous_first) {
  std::array<std::uint8_t, kEquipmentRowCapacity> gradients{};
  const int visible = std::min<int>(state.row_count, kEquipmentVisibleRows);
  for (int i = 1; i < visible; ++i) {
    gradients[static_cast<std::size_t>(i) + previous_first] =
        host.Row(kEquipmentFirstRow + i).gradient_state;
  }
  for (int i = 1;;) {
    const std::size_t shown = static_cast<std::size_t>(i) + state.first_row;
    const ItemText& record = state.text[shown];
    MenuRow& row = host.SetRow(
        {kEquipmentFirstRow + i, kRowX, RowY(i), kRowWidth, 0}, record.name);
    row.gradient_state = gradients[shown];
    ApplyRowFlags(row, record.flags);
    ++i;
    if (i == state.row_count) {
      break;
    }
    if (i == kEquipmentVisibleRows) {
      if (state.first_row + kEquipmentVisibleRows < state.row_count) {
        row.is_scrollable = kMoreBelow;
      }
      break;
    }
  }
  if (state.first_row != 0) {
    host.Row(kEquipmentFirstRow + 1).is_scrollable = kMoreAbove;
  }
}

[[nodiscard]] std::int32_t CursorPosition(std::int32_t selected) {
  std::uint32_t position =
      ((static_cast<std::uint32_t>(selected) * kRowPitch + kCursorY)
       << kPositionShift) |
      kCursorX;
  if (selected == 0) {
    position -= kFirstRowCursorLift;
  }
  return static_cast<std::int32_t>(position);
}

// _processEquipSubMenu's input step (16C.c:164-302).
void TakeInput(EquipmentRowsState& state, EquipmentRowsHost& host,
               const EquipmentChoiceInput& input, std::int32_t has_sfx,
               MenuRow& row, std::int32_t selected,
               std::uint8_t previous_first) {
  if ((input.pressed & kTriangle) != 0) {
    if ((has_sfx & kSilentLeave) == 0) {
      host.PlayLeaveSound();
    }
    CloseList(state, host, kEquipmentChoiceBack);
    return;
  }
  if ((input.pressed & kCircle) != 0) {
    if (row.unselectable != 0) {
      host.PlayInvalidSound();
    } else {
      CloseList(state, host, selected);
      if (has_sfx != 0) {
        host.PlayConfirmSound(selected == 0 ? kFirstChoiceSound
                                            : kOtherChoiceSound);
      }
      return;
    }
  }
  row.selected = 0;
  if ((input.pressed & kCross) != 0) {
    host.PlayLeaveSound();
    CloseList(state, host, kEquipmentChoiceCancel);
    return;
  }
  if ((input.repeat & kUp) != 0) {
    MoveUp(state);
  }
  if ((input.repeat & kDown) != 0) {
    MoveDown(state);
  }
  const std::int32_t now = WrappingAdd(state.selected_row, state.first_row);
  if (selected != now) {
    host.PlayChangeSound();
    if (state.first_row != previous_first) {
      ScrollRows(state, host, previous_first);
    }
    state.equipped_item =
        Equipped(state.text[static_cast<std::size_t>(now)].flags);
  }
  host.Row(WrappingAdd(state.selected_row, kEquipmentFirstRow)).selected = 1;
  state.cursor_animation = static_cast<std::uint8_t>(host.DrawCursor(
      state.cursor_animation, CursorPosition(state.selected_row)));
}

// The scroll position and selection a list of row_count rows can hold.
[[nodiscard]] bool ValidPosition(const EquipmentRowsState& state) {
  const int count = state.row_count;
  const int last_first =
      count > kEquipmentVisibleRows ? count - kEquipmentVisibleRows : 0;
  const std::int32_t shown = WrappingAdd(state.selected_row, state.first_row);
  return state.first_row <= last_first && state.selected_row >= 0 &&
         state.selected_row < kEquipmentVisibleRows && shown < count;
}
}  // namespace

// decomp: MENU3.PRG _populateMenuRows (src/MENU/MENU3.PRG/16C.c:39)
bool CreateEquipmentRows(EquipmentRowsState& state, EquipmentRowsHost& host,
                         std::span<const EquipmentRowSource> rows) {
  if (rows.empty() || rows.size() > kEquipmentRowCapacity ||
      !std::ranges::all_of(rows, ValidSource)) {
    return false;
  }
  state.row_count = static_cast<std::uint8_t>(rows.size());
  state.first_row = 0;
  state.selected_row = 0;
  state.step = 1;
  state.text = host.AllocateText(rows.size());
  if (state.text.size() < rows.size()) {
    state.text = {};
    return false;
  }
  for (std::size_t i = 0; i < rows.size(); ++i) {
    ItemText& record = state.text[i];
    record.flags = rows[i].flags;
    CopyName(record, rows[i].name);
    CopyDescription(record, rows[i].description);
  }
  const std::int32_t flags = rows.front().flags;
  MenuRow& row = host.SetRow(kTitleRow, state.text.front().name);
  row.unselectable = static_cast<std::uint8_t>(flags & 1);
  row.row_icon = RowIcon(flags);
  row.material = Material(flags);
  row.selected = 1;
  state.equipped_item = Equipped(flags);
  return true;
}

// decomp: MENU3.PRG _getSelectedItemRow (src/MENU/MENU3.PRG/16C.c:101)
std::int32_t SelectedEquipmentRow(const EquipmentRowsState& state) {
  return state.text.data() != nullptr ? -1 : state.selected_row;
}

// decomp: MENU3.PRG _processEquipSubMenu (src/MENU/MENU3.PRG/16C.c:111)
bool UpdateEquipmentChoices(EquipmentRowsState& state, EquipmentRowsHost& host,
                            const EquipmentChoiceInput& input,
                            std::int32_t has_sfx) {
  if (state.step < kEquipmentVisibleRows) {
    if (state.step == state.row_count) {
      state.step = kEquipmentRowsWaiting;
      return true;
    }
    if (state.step > state.row_count || state.text.size() <= state.step) {
      return false;
    }
    SlideRowIn(state, host);
    return true;
  }
  if (state.text.size() < state.row_count || !ValidPosition(state)) {
    return false;
  }
  const std::uint8_t previous_first = state.first_row;
  const std::int32_t selected = WrappingAdd(state.selected_row, previous_first);
  MenuRow& row = host.Row(WrappingAdd(state.selected_row, kEquipmentFirstRow));
  host.ShowInformation(
      state.text[static_cast<std::size_t>(selected)].description);
  if (state.step == kEquipmentRowsWaiting) {
    if (host.RowsIdle()) {
      state.step = kEquipmentRowsInput;
    }
  } else if (state.step == kEquipmentRowsInput) {
    TakeInput(state, host, input, has_sfx, row, selected, previous_first);
  }
  return true;
}

// decomp: MENU3.PRG _populateUiAccessory (src/MENU/MENU3.PRG/16C.c:311)
bool SetAccessoryPreviewStats(AccessoryPreviewHost& host,
                              std::int32_t accessory) {
  if (accessory < 0 || (accessory != 0 && !host.HasAccessory(accessory - 1))) {
    return false;
  }
  host.ResetStats();
  if (accessory != 0) {
    const AccessoryPreview stats = host.LoadAccessory(accessory - 1);
    AccessoryStatColumns columns;
    for (std::size_t i = 0; i < kAccessoryStatColumn; ++i) {
      const std::size_t source = i & (stats.classes.size() - 1);
      columns.classes[i] = static_cast<std::int16_t>(stats.classes[source]);
      columns.affinities[i] =
          static_cast<std::int16_t>(stats.affinities[source]);
    }
    // The original read types[i & 7] past its four entries, into the classes
    // (16C.c:328). Only the first four type columns belong to the accessory
    // and the type page draws columns 1-3, so the rest stay zero (#372).
    for (std::size_t i = 0; i < stats.types.size(); ++i) {
      columns.types[i] = static_cast<std::int16_t>(stats.types[i]);
    }
    host.StoreStatColumns(columns);
    host.SetPrimaryStats(stats.strength, stats.intelligence, stats.agility,
                         kAccessoryPrimaryStatsKnown);
  }
  host.SetStatusItem(kAccessorySubtype, static_cast<std::uint8_t>(accessory));
  host.SetStatPages(kAccessoryStatPages);
  return true;
}

// decomp: MENU3.PRG _loadObjectData (src/MENU/MENU3.PRG/68C4.c:12)
ModelReload ReloadEquipmentModels(std::uint8_t& step, EquipmentModelHost& host,
                                  const EquipmentModelChoice& choice) {
  if (!host.HasPreviewActor()) {
    return ModelReload::kRefused;
  }
  switch (step) {
    case kReloadQueue:
      if (choice.model_id != 0 &&
          !host.EquipmentModelSlot(ModelIndex(choice.model_id))) {
        return ModelReload::kRefused;
      }
      if (choice.category == 0) {
        host.ReleaseWeaponModel(kShieldModel);
        if (choice.model_id != 0 &&
            IsTwoHandedWeapon(host.PreviewSequenceModel())) {
          host.ReleaseWeaponModel(kWeaponModel);
          host.QueueWeaponAnimations(kShieldStance);
        }
      } else {
        if (choice.category != host.PreviewSequenceModel()) {
          host.QueueWeaponAnimations(choice.category);
        }
        host.ReleaseWeaponModel(kWeaponModel);
        if (host.HasWeaponModel(kFirstPreviewWeaponModel + 1) &&
            IsTwoHandedWeapon(choice.category)) {
          host.ReleaseWeaponModel(kShieldModel);
        }
      }
      static_cast<void>(
          QueueEquipmentModel(host, choice.model_id, choice.material));
      host.SetWeaponDrawn(false);
      ++step;
      break;
    case kReloadWait:
      if (!host.AssetQueueBusy() &&
          host.PreviewAnimation() == kPreviewIdleAnimation) {
        host.ResetEquipmentPose();
        ++step;
      }
      break;
    case kReloadPose:
      host.ReleasePreview();
      host.ClonePreview(kSourceActorSlot);
      host.ResetPreviewPose();
      host.SheathePreviewWeapon();
      host.PlayPreviewAnimation(kPreviewIdleAnimation);
      host.AdvancePreviewSkeleton();
      host.CopyPreviewPoseToBlend();
      for (int i = 0; i < kPreviewWeaponModels; ++i) {
        if (host.HasWeaponModel(kFirstPreviewWeaponModel + i)) {
          host.ClearWeaponModelFlag(kFirstPreviewWeaponModel + i);
        }
      }
      host.SetWeaponDrawn(true);
      ++step;
      break;
    case kReloadSettle:
      if (host.PreviewAnimation() == kPreviewSettledAnimation) {
        step = 0;
        return ModelReload::kDone;
      }
      break;
    default:
      break;
  }
  return ModelReload::kLoading;
}

// decomp: MENU3.PRG func_80109320 (src/MENU/MENU3.PRG/68C4.c:87)
bool IsTwoHandedWeapon(std::int32_t category) {
  if (category < kFirstTwoHandedCategory) {
    return false;
  }
  if (category == kTwoHandedCategory) {
    return true;
  }
  return (category & 1) != 0;
}

// decomp: MENU3.PRG _populateDataSlot (src/MENU/MENU3.PRG/68C4.c:100)
bool QueueEquipmentModel(EquipmentModelHost& host, std::int32_t model_id,
                         std::int32_t material) {
  if (model_id == 0) {
    return true;
  }
  const int index = ModelIndex(model_id);
  const bool shield = index != 0;
  const auto destination = host.EquipmentModelSlot(index);
  if (!destination) {
    return false;
  }
  ActorAssetRequest request{};
  request.kind = kEquipmentModelKind;
  request.target_slot = static_cast<std::int8_t>(index);
  request.model_id = static_cast<std::int16_t>(model_id);
  request.destination = *destination;
  request.sub_kind = 0;
  request.texture_slot = shield ? kShieldTextureSlot : kWeaponTextureSlot;
  request.material = static_cast<std::uint8_t>(material);
  host.QueueAsset(request);
  return true;
}
}  // namespace VS::Menu::Inventory
