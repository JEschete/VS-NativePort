#include "Menu/Status/LimbControllers.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string_view>

#include "Core/Math/ScalarMath.hpp"
#include "Input/Controller.hpp"
#include "Input/Pad.hpp"

// NOLINTNEXTLINE(readability-identifier-naming): the catalog's component.
namespace VS::Menu::Status {
namespace {
using Core::Math::WrappingAdd;
using Core::Math::WrappingSubtract;
constexpr std::uint32_t kShiftMask = 31;
constexpr std::uint32_t kTileCommand = 0x60000000;
constexpr std::uint32_t kLeaderColor = 0x687018;
constexpr std::uint32_t kLeaderMode = 0xE1000000;
constexpr std::uint32_t kLeaderCommandBase = 33;
constexpr int kLeaderCommandShift = 25;
constexpr std::int32_t kRowPitch = 16;
constexpr std::int32_t kLeaderTop = 42;
constexpr std::int32_t kLabelTop = 34;
constexpr std::int32_t kIndicatorTop = 36;
constexpr std::int32_t kLabelX = 216;
constexpr std::int32_t kConditionX = 224;
constexpr std::uint32_t kNeutralColor = 0x808080;
constexpr std::int32_t kEnteringStep = 12;
constexpr std::int32_t kLeaderStep = 8;
constexpr unsigned kLeavingShift = 5;
constexpr std::uint8_t kDimmingBit = 0x80;
constexpr std::uint8_t kTopElement = 11;
constexpr std::uint8_t kRightElement = 6;
constexpr std::uint8_t kGridColumns = 8;
constexpr std::int32_t kDigitalRotation = 64;
constexpr std::int32_t kMaxPitch = 896;
constexpr std::int32_t kIconTop = 144;
constexpr std::int32_t kCursorTop = 136;
constexpr std::uint32_t kIconPaletteMask = 0x0F0F906A;
constexpr std::uint32_t kIconPalette = 0x37F80000;
constexpr std::uint32_t kAlternateIconPalette = 0x37F90000;
constexpr std::uint32_t kElementPalette = 0x37FF0000;
constexpr std::uint32_t kElementUv = 0x3068;
constexpr std::size_t kAffinityStart = 16;
constexpr std::size_t kTypeStart = 32;
constexpr std::int32_t kCrossResult = 3;
constexpr std::int32_t kQueryPanel = -3;
constexpr std::uint8_t kLeavingPhase = 3;

[[nodiscard]] std::int32_t RowPosition(std::int32_t x, std::int32_t top,
                                       std::size_t limb) {
  // Unlike PackScreenWord, the original does not mask an animated x.
  return std::bit_cast<std::int32_t>(
      static_cast<std::uint32_t>(x) |
      (static_cast<std::uint32_t>(top +
                                  static_cast<std::int32_t>(limb) * kRowPitch)
       << Render::kPackedWordHighShift));
}
[[nodiscard]] LimbControllerResult ServiceResult(bool success) {
  if (!success) {
    return std::unexpected(LimbControllerError::kUnavailableService);
  }
  return {};
}

void RotateModel(StatusEffectNavigationFrame& frame) {
  std::int32_t x = Input::MapStickDeadZone(frame.stick_x);
  std::int32_t y = Input::MapStickDeadZone(frame.stick_y);
  if (x == 0 && y == 0) {
    if ((frame.held & Input::Up) != 0) y = -kDigitalRotation;
    if ((frame.held & Input::Down) != 0) y = kDigitalRotation;
    if ((frame.held & Input::Left) != 0) x = -kDigitalRotation;
    if ((frame.held & Input::Right) != 0) x = kDigitalRotation;
  }
  frame.yaw = WrappingAdd(frame.yaw, x);
  frame.pitch = std::clamp(WrappingSubtract(frame.pitch, y), 0, kMaxPitch);
}

[[nodiscard]] bool RebuildEffects(StatusEffectNavigationState& state,
                                  const StatusEffectNavigationFrame& frame,
                                  const StatusEffectTarget& target,
                                  bool selection_active) {
  if (target.flags == 0) return false;
  state.target_slot = target.actor_slot;
  state.count = 0;
  for (std::size_t effect = 0; effect < kStatusEffectSlots; ++effect) {
    if (((target.flags >> effect) & 1U) != 0) {
      state.active[state.count++] = static_cast<std::uint8_t>(effect);
    }
  }
  if (!selection_active) {
    if (state.selected >= state.count) state.selected = state.count - 1;
  } else if (frame.selected_element == kTopElement) {
    state.selected = 0;
  } else {
    state.selected = std::min<std::uint8_t>(state.count, kGridColumns) - 1;
  }
  return true;
}

[[nodiscard]] bool MoveEffectSelection(StatusEffectNavigationState& state,
                                       StatusEffectNavigationFrame& frame,
                                       StatusEffectNavigationHost& host) {
  int selection = state.selected;
  if ((frame.repeat & Input::Up) != 0) {
    if (selection < kGridColumns) {
      host.PlayChangeSound();
      frame.selected_element = kTopElement;
      return false;
    }
    selection -= kGridColumns;
  }
  if ((frame.repeat & Input::Down) != 0 && state.count >= kGridColumns &&
      selection < kGridColumns) {
    selection = std::min(selection + kGridColumns, state.count - 1);
  }
  if ((frame.repeat & Input::Left) != 0 &&
      (selection & (kGridColumns - 1)) != 0) {
    --selection;
  }
  if ((frame.repeat & Input::Right) != 0) {
    if ((selection & (kGridColumns - 1)) == kGridColumns - 1 ||
        selection == state.count - 1) {
      host.PlayChangeSound();
      frame.selected_element = kRightElement;
      return false;
    }
    selection = std::min(selection + 1, state.count - 1);
  }
  if (selection != state.selected) {
    host.PlayChangeSound();
    state.selected = static_cast<std::uint8_t>(selection);
  }
  return true;
}

struct PanelRow {
  std::size_t limb = 0;
  std::uint8_t name = 0;
  std::int32_t offset = 0;
  std::int32_t dimming = 0;
  std::optional<std::uint32_t> color;
};

[[nodiscard]] LimbControllerResult DrawPanelRow(const LimbPanelState& state,
                                                const PanelRow& row,
                                                LimbPanelHost& host) {
  if (!host.DrawName(
          row.name,
          RowPosition(WrappingAdd(row.offset, kLabelX), kLabelTop, row.limb),
          row.color)) {
    return std::unexpected(LimbControllerError::kUnavailableService);
  }
  const auto condition = host.Condition(state.actor_slot, row.limb);
  if (!condition || *condition >= kLimbConditionCount) {
    return std::unexpected(LimbControllerError::kInvalidCondition);
  }
  const LimbIndicatorRequest indicator{
      .condition = *condition,
      .position = RowPosition(WrappingAdd(row.offset, kLabelX), kIndicatorTop,
                              row.limb),
      .dimming = row.dimming};
  return ServiceResult(
      host.DrawIndicator(indicator) &&
      host.DrawCondition(*condition,
                         RowPosition(WrappingAdd(row.offset, kConditionX),
                                     kLabelTop, row.limb),
                         row.color));
}

[[nodiscard]] LimbControllerResult DrawPanelLeader(const LimbPanelState& state,
                                                   std::size_t limb,
                                                   bool opaque, bool visible,
                                                   LimbPanelHost& host) {
  const auto anchor = host.ProjectAnchor(state.actor_slot, limb);
  if (!anchor) {
    return std::unexpected(LimbControllerError::kUnavailableService);
  }
  if (!visible) return {};
  return ServiceResult(host.DrawLeader({.limb = static_cast<std::int32_t>(limb),
                                        .position = *anchor,
                                        .opacity = opaque ? 1 : 0}));
}

[[nodiscard]] LimbControllerResult EnteringPanel(
    LimbPanelState& state, const LimbPanelFrame& frame,
    std::span<const std::uint8_t> names, LimbPanelHost& host) {
  if (names.empty()) {
    return std::unexpected(LimbControllerError::kEmptyEnteringPanel);
  }
  for (std::size_t limb = 0; limb < names.size(); ++limb) {
    const std::int32_t step = state.animation_steps[limb];
    if (step < 0 ||
        static_cast<std::size_t>(step) >= frame.row_offsets.size()) {
      return std::unexpected(LimbControllerError::kInvalidAnimation);
    }
  }
  if (!host.DrawIcons(state.actor_slot,
                      frame.row_offsets[static_cast<std::size_t>(
                          state.animation_steps.front())])) {
    return std::unexpected(LimbControllerError::kUnavailableService);
  }
  bool last_complete = false;
  for (std::size_t limb = 0; limb < names.size(); ++limb) {
    auto& step = state.animation_steps[limb];
    const auto drawn = DrawPanelRow(
        state,
        {.limb = limb,
         .name = names[limb],
         .offset = frame.row_offsets[static_cast<std::size_t>(step)],
         .color = std::nullopt},
        host);
    if (!drawn) return drawn;
    if (step < kLeaderStep) {
      const auto leader = DrawPanelLeader(state, limb, false, true, host);
      if (!leader) return leader;
    }
    last_complete = step == 0;
    if (!last_complete) --step;
  }
  if (last_complete) state.phase = 2;
  return {};
}

[[nodiscard]] LimbControllerResult ShownPanel(
    const LimbPanelState& state, const LimbPanelFrame& frame,
    std::span<const std::uint8_t> names, LimbPanelHost& host) {
  if (!host.DrawIcons(state.actor_slot, 0)) {
    return std::unexpected(LimbControllerError::kUnavailableService);
  }
  const int selection = frame.selection_and_dimming;
  const int dimming = selection >> 7;
  const int highlighted = selection - kDimmingBit;
  const bool lines = (selection & kDimmingBit) == 0 &&
                     (frame.fade_screen | frame.background_first) == 0;
  for (std::size_t limb = 0; limb < names.size(); ++limb) {
    const int shift = dimming - (static_cast<int>(limb) == highlighted);
    const auto drawn = DrawPanelRow(
        state,
        {.limb = limb,
         .name = names[limb],
         .dimming = shift,
         .color =
             kNeutralColor >> (static_cast<std::uint32_t>(shift) & kShiftMask)},
        host);
    if (!drawn) return drawn;
    const auto leader = DrawPanelLeader(
        state, limb, static_cast<int>(limb) == selection, lines, host);
    if (!leader) return leader;
  }
  return {};
}

[[nodiscard]] LimbControllerResult LeavingPanel(
    LimbPanelState& state, std::span<const std::uint8_t> names,
    LimbPanelHost& host) {
  const auto slide = std::bit_cast<std::int32_t>(
      static_cast<std::uint32_t>(state.animation_steps.front())
      << kLeavingShift);
  if (!host.DrawIcons(state.actor_slot, slide)) {
    return std::unexpected(LimbControllerError::kUnavailableService);
  }
  for (std::size_t limb = 0; limb < names.size(); ++limb) {
    auto& step = state.animation_steps[limb];
    if (step >= kLeaderStep) {
      state.phase = 0;
      continue;
    }
    step = WrappingAdd(step, 1);
    const auto offset = std::bit_cast<std::int32_t>(
        static_cast<std::uint32_t>(step) << kLeavingShift);
    const auto drawn = DrawPanelRow(state,
                                    {.limb = limb,
                                     .name = names[limb],
                                     .offset = offset,
                                     .color = std::nullopt},
                                    host);
    if (!drawn) return drawn;
  }
  return {};
}
}  // namespace

std::string_view LimbControllerErrorText(LimbControllerError error) noexcept {
  switch (error) {
    case LimbControllerError::kMissingActor:
      return "missing actor/stats";
    case LimbControllerError::kInvalidLimb:
      return "limb outside six slots";
    case LimbControllerError::kInvalidCondition:
      return "invalid limb condition";
    case LimbControllerError::kInvalidAnimation:
      return "animation outside table";
    case LimbControllerError::kEmptyEnteringPanel:
      return "unnamed entering actor";
    case LimbControllerError::kInvalidNavigation:
      return "uninitialized status grid";
    case LimbControllerError::kInvalidText:
      return "invalid or oversized text";
    case LimbControllerError::kInsufficientStats:
      return "short stat destination";
    case LimbControllerError::kUnavailableService:
      return "unavailable UI service";
  }
  return "unknown controller error";
}

// decomp: drawStatusIndicator (MENU4.PRG src/MENU/MENU4.PRG/120.c:851; was
// _drawStatusIndicator, 80103E58-80103EF7).
LimbControllerResult DrawLimbConditionIndicator(
    Render::PacketQueue& queue, std::span<const std::uint32_t> palette,
    const LimbIndicatorRequest& request) {
  if (request.condition < 0 ||
      static_cast<std::size_t>(request.condition) >= palette.size()) {
    return std::unexpected(LimbControllerError::kInvalidCondition);
  }
  const std::uint32_t color =
      palette[static_cast<std::size_t>(request.condition)] >>
      (static_cast<std::uint32_t>(request.dimming) & kShiftMask);
  const auto position = static_cast<std::uint32_t>(request.position);
  const std::array packet{
      Render::PackTagWord(6, queue.ChainLink()),
      color | kTileCommand,
      position,
      static_cast<std::uint32_t>(Render::PackScreenWord(7, 7)),
      (color * 2U) | kTileCommand,
      position + static_cast<std::uint32_t>(Render::PackScreenWord(1, 1)),
      static_cast<std::uint32_t>(Render::PackScreenWord(5, 5))};
  return ServiceResult(queue.Emit(packet));
}

// decomp: drawLimbLeaderLine (MENU4.PRG src/MENU/MENU4.PRG/120.c:874; was
// _drawLimbLeaderLine, 80103EF8-80103FEB).
LimbControllerResult DrawLimbLeaderLine(Render::PacketQueue& queue,
                                        const LimbLeaderRequest& request) {
  if (request.limb < 0 ||
      static_cast<std::size_t>(request.limb) >= kStatusLimbSlots) {
    return std::unexpected(LimbControllerError::kInvalidLimb);
  }
  const auto opacity = static_cast<std::uint32_t>(request.opacity);
  const std::uint32_t color =
      (kLeaderColor << (opacity & kShiftMask)) |
      ((kLeaderCommandBase - opacity) << kLeaderCommandShift);
  const int row_y = request.limb * kRowPitch + kLeaderTop;
  const auto x =
      std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(request.position));
  const int delta = (request.position >> Render::kPackedWordHighShift) - row_y;
  const int distance = ((delta < 0 ? -delta : delta) * 3) >> 1;
  const auto elbow = static_cast<std::uint32_t>(
      Render::PackScreenWord(x + (distance * 2) / 3, row_y));
  const std::uint32_t link = queue.ChainLink();
  const std::array packet{
      Render::PackTagWord(8, link),
      kLeaderMode,
      color,
      static_cast<std::uint32_t>(request.position),
      elbow,
      Render::PackTagWord(3, link),
      color,
      elbow,
      static_cast<std::uint32_t>(Render::PackScreenWord(kLabelX, row_y))};
  return ServiceResult(queue.Emit(packet));
}

// decomp: renderStatusIcons (MENU4.PRG src/MENU/MENU4.PRG/120.c:916; was
// _renderStatusIcons, 80103FEC-80104133).
LimbControllerResult DrawStatusEffectIcons(
    StatusIconHost& host, std::uint32_t flags, std::int32_t animation_step,
    std::span<const std::uint16_t, kStatusEffectSlots> texture_offsets) {
  int count = 0;
  for (std::size_t effect = 0; effect < kStatusEffectSlots; ++effect) {
    if (((flags >> effect) & 1U) == 0) continue;
    const int column = count & (kGridColumns - 1);
    const int row = count++ / kGridColumns;
    const auto x =
        WrappingSubtract(column * kRowPitch + kRowPitch, animation_step);
    const int y = row * kRowPitch + kIconTop;
    if (effect >= static_cast<std::size_t>(kRowPitch)) {
      const VitalsSprite element{
          .brightness = 128,
          .xy = Render::PackScreenWord(WrappingAdd(x, 8), y),
          .wh = Render::PackScreenWord(8, 8),
          .uv_clut =
              kElementPalette |
              (kElementUv + (static_cast<std::uint32_t>(effect) & 3U) * 8U)};
      if (!host.QueueStatusSprite(element)) {
        return std::unexpected(LimbControllerError::kUnavailableService);
      }
    }
    const VitalsSprite icon{.brightness = 128,
                            .xy = Render::PackScreenWord(x, y),
                            .wh = Render::PackScreenWord(kRowPitch, kRowPitch),
                            .uv_clut = texture_offsets[effect] |
                                       (((kIconPaletteMask >> effect) & 1U) != 0
                                            ? kAlternateIconPalette
                                            : kIconPalette)};
    if (!host.QueueStatusSprite(icon)) {
      return std::unexpected(LimbControllerError::kUnavailableService);
    }
  }
  return {};
}

// decomp: navigateStatusModifiers (MENU4.PRG src/MENU/MENU4.PRG/120.c:964; was
// _navigateStatusModifiers, 80104134-80104513).
std::expected<std::int32_t, LimbControllerError> NavigateStatusEffects(
    StatusEffectNavigationState& state, StatusEffectNavigationFrame& frame,
    std::optional<StatusEffectTarget> target, bool selection_active,
    StatusEffectNavigationHost& host) {
  if (target) {
    if (target->actor_slot < 0) {
      return std::unexpected(LimbControllerError::kMissingActor);
    }
    if (!RebuildEffects(state, frame, *target, selection_active)) return 1;
  } else {
    selection_active = true;
    if ((frame.pressed & Input::Triangle) != 0) return 2;
    if ((frame.pressed & Input::Cross) != 0) return kCrossResult;
    if ((frame.held & Input::Square) != 0) {
      RotateModel(frame);
      return 0;
    }
    if (state.count == 0 || state.count > kStatusEffectSlots ||
        state.selected >= state.count) {
      return std::unexpected(LimbControllerError::kInvalidNavigation);
    }
    if (!MoveEffectSelection(state, frame, host)) return 1;
  }
  const std::uint8_t effect = state.active[state.selected];
  if (effect >= kStatusEffectSlots) {
    return std::unexpected(LimbControllerError::kInvalidNavigation);
  }
  if (!host.ShowStatusDescription(effect)) {
    return std::unexpected(LimbControllerError::kInvalidText);
  }
  if (selection_active) {
    const auto position = Render::PackScreenWord(
        (state.selected & (kGridColumns - 1)) * kRowPitch + 2,
        (state.selected / kGridColumns) * kRowPitch + kCursorTop);
    state.cursor_animation = static_cast<std::uint8_t>(
        host.DrawCursor(state.cursor_animation, position));
    frame.cursor_color = 1;
  }
  return 0;
}

// decomp: renderLimbUi (MENU4.PRG src/MENU/MENU4.PRG/120.c:1149; was
// _renderLimbUi, 801045B8-80104AEB).
std::expected<std::int32_t, LimbControllerError> UpdateLimbPanel(
    LimbPanelState& state, LimbPanelFrame& frame, std::int32_t request,
    LimbPanelHost& host) {
  const auto names = host.Names(state.actor_slot);
  if (!names) return std::unexpected(LimbControllerError::kMissingActor);
  if (request > 0) {
    frame.selection_and_dimming = static_cast<std::uint8_t>(request - 1);
    return 0;
  }
  if (request == kQueryPanel) return state.phase;
  if (request == -2) {
    state.phase = kLeavingPhase;
    return 0;
  }
  if (request < 0) {
    const auto slot = static_cast<std::uint8_t>(
        static_cast<std::uint32_t>(frame.selected_actor) - 1U);
    if (!host.Names(slot) || !host.PopulateComparisonStats(slot)) {
      return std::unexpected(LimbControllerError::kMissingActor);
    }
    state.actor_slot = slot;
    for (std::size_t limb = 0; limb < state.animation_steps.size(); ++limb) {
      state.animation_steps[limb] =
          kEnteringStep + static_cast<std::int32_t>(limb);
    }
    state.phase = 1;
    return 0;
  }
  const auto count = static_cast<std::size_t>(LimbCount(*names));
  const auto visible = std::span(*names).first(count);
  LimbControllerResult result;
  switch (state.phase) {
    case 1:
      result = EnteringPanel(state, frame, visible, host);
      break;
    case 2:
      result = ShownPanel(state, frame, visible, host);
      break;
    case kLeavingPhase:
      result = LeavingPanel(state, visible, host);
      break;
    default:
      break;
  }
  if (!result) return std::unexpected(result.error());
  return 0;
}

// decomp: renderSelectedLimbCondition (src/MENU/MENU4.PRG/120.c:2231; was
// _renderSelectedLimbCondition, 80106308-801063F7).
LimbControllerResult ShowSelectedLimbCondition(
    std::uint8_t limb, const SelectedLimbText& text,
    Render::EncodedTextArguments& arguments, std::span<std::uint8_t> output,
    SelectedLimbHost& host) {
  if (limb >= kStatusLimbSlots) {
    return std::unexpected(LimbControllerError::kInvalidLimb);
  }
  auto selected_arguments = arguments;
  selected_arguments.strings[0] = text.name;
  selected_arguments.strings[1] = text.condition;
  const auto end =
      Render::FormatEncodedText(output, text.format, selected_arguments);
  if (!end) return std::unexpected(LimbControllerError::kInvalidText);
  arguments = selected_arguments;
  return ServiceResult(host.ShowInformation(output.first(*end + 1)) &&
                       host.SelectLimb(limb + 1));
}

// decomp: renderSelectedLimbStats (src/MENU/MENU4.PRG/120.c:2248; was
// _renderSelectedLimbStats, 801063F8-801064D3).
LimbControllerResult PopulateSelectedLimbDefense(
    std::uint8_t limb, const LimbDefenseView& defense,
    std::span<std::int16_t> equipment_stats, SelectedLimbHost& host) {
  if (limb >= kStatusLimbSlots) {
    return std::unexpected(LimbControllerError::kInvalidLimb);
  }
  if (equipment_stats.size() < kTypeStart + kLimbDamageTypeSlots) {
    return std::unexpected(LimbControllerError::kInsufficientStats);
  }
  for (std::size_t index = 0; index < kLimbAffinitySlots * 2; ++index) {
    equipment_stats[kAffinityStart + index] =
        defense.affinities[index & (kLimbAffinitySlots - 1)];
  }
  for (std::size_t index = 0; index < kLimbDamageTypeSlots; ++index) {
    equipment_stats[kTypeStart + index] = defense.damage_types[index];
  }
  return ServiceResult(host.SelectLimb(limb + kDimmingBit + 1));
}
}  // namespace VS::Menu::Status
