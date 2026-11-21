#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "Input/Controller.hpp"
#include "Menu/Status/TerminalControllers.hpp"

// NOLINTNEXTLINE(readability-identifier-naming): the catalog's component.
namespace VS::Menu::Status {
namespace {
namespace math = Core::Math;
constexpr std::uint32_t kL2 = 1;
constexpr std::uint32_t kL1 = 4;
constexpr std::uint32_t kR1 = 8;
constexpr std::uint32_t kTriangle = 16;
constexpr std::uint32_t kCircle = 32;
constexpr std::uint32_t kCross = 64;
constexpr std::uint32_t kSquare = 128;
constexpr std::uint32_t kUp = 0x1000;
constexpr std::uint32_t kDown = 0x4000;
constexpr std::uint32_t kLeft = 0x8000;
constexpr std::uint32_t kRight = 0x2000;
constexpr int kAnimationFrames = 10;
constexpr int kExitFrames = 8;
constexpr int kModelSlide = 128;
constexpr int kStatDesc = 6;
constexpr int kEffectsElement = 13;
constexpr int kDefaultYaw = 0xB00;
constexpr int kMaxPitch = 896;
constexpr int kDigitalRotation = 64;
constexpr int kStatusProjection = 512;
constexpr int kFarClip = 4096;
constexpr int kBackgroundPriority = 2046;
constexpr int kBackgroundBrightness = 128;
constexpr int kPhysicalDefense = 50;
constexpr int kBluntDefense = 57;
constexpr int kFirstStatRows = 11;
constexpr int kStatRowsPerPage = 4;
constexpr int kStatRowPitch = 16;
constexpr int kStatRowTop = 50;
constexpr int kForwardActorRequest = 17;
constexpr int kNavigationByte = 8;
constexpr int kNavigationThirdByte = 16;
constexpr int kNavigationLastByte = 24;
constexpr int kHighlightSelectedLimb = 7;
constexpr int kDefensePages = 6;
constexpr int kEffectExitToMenu = 3;
constexpr int kBattleMenu = 5;
constexpr std::array<std::uint32_t, 12> kNavigation{
    0x000A010A, 0x010A0200, 0x020A0301, 0x030A0402, 0x040A0503, 0x050A0604,
    0x060D0705, 0x070D0806, 0x080D0807, 0x0A090B09, 0x0009000A, 0x000B0D09,
};
constexpr std::array<std::int32_t, 13> kCursorPositions{
    0x001400A0, 0x002400A0, 0x003400A0, 0x004400A0, 0x005400A0,
    0x006400A0, 0x00920098, 0x009C0098, 0x00A60098, 0x0004FFFC,
    0x00040042, 0x000CFFFC, 0x000C0042,
};

namespace phase {
constexpr std::uint8_t kNone = 0;
constexpr std::uint8_t kInitialize = 3;
constexpr std::uint8_t kWaitMenu = 4;
constexpr std::uint8_t kInspect = 5;
constexpr std::uint8_t kEffects = 6;
constexpr std::uint8_t kWaitEquipment = 7;
constexpr std::uint8_t kEquipment = 8;
constexpr std::uint8_t kDefense = 9;
constexpr std::uint8_t kLeaveDefense = 10;
constexpr std::uint8_t kWaitQuitMenu = 11;
constexpr std::uint8_t kQuitMenu = 12;
constexpr std::uint8_t kWaitQuitBattle = 13;
constexpr std::uint8_t kQuitBattle = 14;
}  // namespace phase

[[nodiscard]] std::unexpected<TerminalError> Unavailable() {
  return std::unexpected(TerminalError::kUnavailableService);
}
[[nodiscard]] std::optional<TerminalActor> Selected(StatusTerminalHost& host) {
  const int selected = host.Frame().selected_actor;
  return selected > 0 && selected <= kInspectableActorSlots
             ? host.Actor(selected - 1)
             : std::nullopt;
}
[[nodiscard]] bool Description(StatusTerminalHost& host, int element) {
  if (element < kStatDesc) {
    return host.Condition();
  }
  const auto text = host.StatusText(element - kStatDesc);
  return !text.empty() && host.Information(text);
}
[[nodiscard]] TerminalResult Initialize(StatusTerminalState& state,
                                        StatusTerminalHost& host,
                                        std::uint8_t& menu_state) {
  auto& frame = host.Frame();
  state.screen_enabled = 0;
  state.prompt_animation = kAnimationFrames;
  if (!host.HasItemText() && !host.LoadItemText(true)) {
    return Unavailable();
  }
  const auto ready = host.MenuReady();
  if (!ready) {
    return Unavailable();
  }
  if (!*ready) {
    return 0;
  }
  frame.hud_hidden = 1;
  if (!(host.FieldRendering(false) &&
        host.BackgroundPriority(kBackgroundPriority, kBackgroundBrightness))) {
    return Unavailable();
  }
  frame.background_first = 0;
  frame.fade = 0;
  frame.selected_actor = 0;
  frame.previous_actor = 0;
  frame.vitals_phase = 0;
  frame.vitals_slide = 0;
  frame.initialized_actor = 0;
  if (!host.SwitchActor(1)) {
    return Unavailable();
  }
  state.screen_enabled = 1;
  state.animation_wait = 1;
  state.saved_projection = host.ProjectionDistance();
  if (!host.SetProjection(kStatusProjection)) {
    return Unavailable();
  }
  state.saved_view = host.View();
  host.View().camera.far_clip = kFarClip;
  host.View().camera.angles = {};
  frame.yaw = kDefaultYaw;
  frame.pitch = 0;
  if (!host.InitializeInformation()) {
    return Unavailable();
  }
  frame.selected_element = 0;
  frame.enabled_pages = 0;
  if (!host.Condition()) {
    return Unavailable();
  }
  menu_state = phase::kInspect;
  return 0;
}

[[nodiscard]] TerminalResult SwitchInspectionActor(StatusTerminalState& state,
                                                   StatusTerminalHost& host,
                                                   std::uint8_t& menu_state,
                                                   bool effects) {
  auto& frame = host.Frame();
  int slot = frame.selected_actor - 1;
  int direction = 0;
  // Unlike the equipment list, L1 wins when both buttons are pressed.
  for (const auto button : {kR1, kL1}) {
    if ((frame.pressed & button) == 0) {
      continue;
    }
    const auto next = host.NextActor(slot, button == kR1 ? 1 : 14);
    if (!next) {
      return Unavailable();
    }
    slot = *next;
    direction = button == kR1 ? kForwardActorRequest : 1;
  }
  if (slot == frame.selected_actor - 1) {
    return 0;
  }
  frame.previous_actor = frame.selected_actor;
  if (!(host.LimbPanel(-2) && host.PrimaryStats(2) && host.BeginVitals(slot) &&
        host.SwitchActor(slot + direction))) {
    return Unavailable();
  }
  state.animation_wait = 1;
  if (effects) {
    const auto actor = host.Actor(slot);
    if (!actor) {
      return std::unexpected(TerminalError::kMissingActor);
    }
    if (actor->statuses == 0) {
      menu_state = phase::kInspect;
    }
  }
  return 0;
}

void Rotate(TerminalFrame& frame) {
  int horizontal = Input::MapStickDeadZone(frame.stick_x);
  int vertical = Input::MapStickDeadZone(frame.stick_y);
  if (horizontal == 0 && vertical == 0) {
    if ((frame.held & kUp) != 0) {
      vertical = -kDigitalRotation;
    }
    if ((frame.held & kDown) != 0) {
      vertical = kDigitalRotation;
    }
    if ((frame.held & kLeft) != 0) {
      horizontal = -kDigitalRotation;
    }
    if ((frame.held & kRight) != 0) {
      horizontal = kDigitalRotation;
    }
  }
  frame.yaw = math::WrappingAdd(frame.yaw, horizontal);
  frame.pitch =
      std::clamp(math::WrappingSubtract(frame.pitch, vertical), 0, kMaxPitch);
}
[[nodiscard]] TerminalResult NavigateLimbGrid(StatusTerminalState& state,
                                              StatusTerminalHost& host,
                                              std::uint8_t& menu_state,
                                              int limbs) {
  auto& frame = host.Frame();
  int selected = frame.selected_element;
  if (static_cast<std::size_t>(selected) >= kNavigation.size()) {
    return std::unexpected(TerminalError::kInvalidSelection);
  }
  const std::uint32_t links = kNavigation[frame.selected_element];
  if ((frame.repeat & kUp) != 0) {
    selected = static_cast<std::uint8_t>(links);
  } else if ((frame.repeat & kDown) != 0) {
    selected = static_cast<std::uint8_t>(links >> kNavigationByte);
  } else if ((frame.repeat & kLeft) != 0) {
    selected = static_cast<std::uint8_t>(links >> kNavigationThirdByte);
  } else if ((frame.repeat & kRight) != 0) {
    selected = static_cast<std::uint8_t>(links >> kNavigationLastByte);
  }
  if (!host.LimbPanel(kHighlightSelectedLimb)) {
    return Unavailable();
  }
  if (selected == kEffectsElement) {
    const auto effects = host.StatusEffects(frame.selected_actor - 1, true);
    if (!effects) {
      return Unavailable();
    }
    if (*effects == 0) {
      if (!host.Sound(TerminalSound::kChange)) {
        return Unavailable();
      }
      menu_state = phase::kEffects;
      return 1;
    }
    selected = frame.selected_element;
  }
  if (selected < kStatDesc) {
    if (selected < limbs) {
      if (!host.LimbPanel(selected + 1)) {
        return Unavailable();
      }
    } else {
      selected = kStatDesc;
      if ((frame.repeat & kDown) == 0) {
        selected = limbs - 1;
        if ((frame.repeat & kUp) == 0) {
          frame.selected_element = static_cast<std::uint8_t>(selected);
        }
      }
    }
  }
  if (selected < 0 ||
      static_cast<std::size_t>(selected) >= kCursorPositions.size()) {
    return std::unexpected(TerminalError::kInvalidSelection);
  }
  frame.cursor_color = static_cast<std::uint8_t>(selected >= limbs);
  const auto cursor =
      host.Cursor(static_cast<std::uint32_t>(state.cursor_animation),
                  kCursorPositions[static_cast<std::size_t>(selected)]);
  if (!cursor) {
    return Unavailable();
  }
  state.cursor_animation = *cursor;
  if (selected != frame.selected_element) {
    if (!host.Sound(TerminalSound::kChange)) {
      return Unavailable();
    }
    if (selected != kEffectsElement) {
      frame.selected_element = static_cast<std::uint8_t>(selected);
    }
  }
  return Description(host, selected) ? TerminalResult{0}
                                     : TerminalResult{Unavailable()};
}

[[nodiscard]] TerminalResult InspectInput(StatusTerminalState& state,
                                          StatusTerminalHost& host,
                                          std::uint8_t& menu_state,
                                          bool details, bool& available,
                                          int limbs) {
  auto& frame = host.Frame();
  const auto panel = host.LimbPanel(-3);
  if (!panel) {
    return Unavailable();
  }
  if (*panel != 2) {
    return 0;
  }
  if ((frame.pressed & (kTriangle | kCross)) != 0) {
    if (!host.Sound(TerminalSound::kLeave)) {
      return Unavailable();
    }
    menu_state = (frame.pressed & kTriangle) != 0 ? phase::kWaitQuitBattle
                                                  : phase::kWaitQuitMenu;
    return 1;
  }
  if ((frame.pressed & kCircle) != 0 && frame.selected_element < limbs) {
    if (!(host.Sound(TerminalSound::kSelect) && host.PrimaryStats(2) &&
          host.LimbPanel(-2))) {
      return Unavailable();
    }
    state.animation_wait = kExitFrames;
    menu_state = phase::kWaitEquipment;
    return 1;
  }
  available = details && frame.selected_element < limbs;
  if ((frame.pressed & kSquare) != 0) {
    if (available) {
      if (!(host.Sound(TerminalSound::kSelect) && host.Defense() &&
            host.StatPages(kDefensePages))) {
        return Unavailable();
      }
      frame.selected_stat = 0;
      state.animation_wait = kAnimationFrames;
      menu_state = phase::kDefense;
      return 1;
    }
    if (!host.Sound(TerminalSound::kInvalid)) {
      return Unavailable();
    }
  }
  if ((frame.held & kL2) != 0) {
    Rotate(frame);
    return 0;
  }
  return NavigateLimbGrid(state, host, menu_state, limbs);
}

[[nodiscard]] TerminalResult Inspect(StatusTerminalState& state,
                                     StatusTerminalHost& host,
                                     std::uint8_t& menu_state, bool details,
                                     bool& available,
                                     const TerminalActor& actor) {
  auto& frame = host.Frame();
  if (state.animation_wait != 0) {
    const auto switched = host.SwitchActor(0);
    if (!switched) {
      return Unavailable();
    }
    if (*switched != 0) {
      state.animation_wait = 0;
      if (!(host.LimbPanel(-1) && host.PrimaryStats(1))) {
        return Unavailable();
      }
      if (frame.selected_element < kStatDesc) {
        const auto current = Selected(host);
        if (!current) {
          return std::unexpected(TerminalError::kMissingActor);
        }
        const int last = LimbCount(current->limb_names) - 1;
        if (last < 0) {
          return std::unexpected(TerminalError::kInvalidSelection);
        }
        if (last < frame.selected_element) {
          frame.selected_element = static_cast<std::uint8_t>(last);
        }
      }
      if (!Description(host, frame.selected_element)) {
        return Unavailable();
      }
    }
  } else {
    const auto loaded = host.LoadItemText(false);
    if (!loaded) {
      return Unavailable();
    }
    if (*loaded != 0) {
      const auto input = InspectInput(state, host, menu_state, details,
                                      available, LimbCount(actor.limb_names));
      if (!input || *input != 0) {
        return input;
      }
      const auto switched =
          SwitchInspectionActor(state, host, menu_state, false);
      if (!switched) {
        return switched;
      }
    }
  }
  const auto current = Selected(host);
  if (!current) {
    return std::unexpected(TerminalError::kMissingActor);
  }
  frame.cursor_color = static_cast<std::uint8_t>(
      frame.selected_element >= LimbCount(current->limb_names));
  return 0;
}

[[nodiscard]] TerminalResult Effects(StatusTerminalState& state,
                                     StatusTerminalHost& host,
                                     std::uint8_t& menu_state) {
  auto& frame = host.Frame();
  if (state.animation_wait != 0) {
    const auto switched = host.SwitchActor(0);
    if (!switched) {
      return Unavailable();
    }
    if (*switched != 0) {
      if (!host.StatusEffects(frame.selected_actor - 1, false)) {
        return Unavailable();
      }
      state.animation_wait = 0;
      if (!(host.LimbPanel(-1) && host.PrimaryStats(1))) {
        return Unavailable();
      }
    }
    return 0;
  }
  const auto panel = host.LimbPanel(-3);
  if (!panel) {
    return Unavailable();
  }
  if (*panel == 2) {
    const auto input = host.StatusEffects(std::nullopt, true);
    if (!input) {
      return Unavailable();
    }
    if (*input == 1) {
      menu_state = phase::kInspect;
    }
    if (*input == 2 || *input == kEffectExitToMenu) {
      if (!host.Sound(TerminalSound::kLeave)) {
        return Unavailable();
      }
      menu_state = *input == 2 ? phase::kWaitQuitBattle : phase::kWaitQuitMenu;
    }
  }
  return SwitchInspectionActor(state, host, menu_state, true);
}

[[nodiscard]] TerminalResult InspectOrEffects(StatusTerminalState& state,
                                              StatusTerminalHost& host,
                                              std::uint8_t& menu_state,
                                              bool& details, bool& available) {
  const auto actor = Selected(host);
  if (!actor) {
    return std::unexpected(TerminalError::kMissingActor);
  }
  details = host.Frame().selected_actor == 1 || actor->analyzed;
  return menu_state == phase::kInspect
             ? Inspect(state, host, menu_state, details, available, *actor)
             : Effects(state, host, menu_state);
}

[[nodiscard]] TerminalResult SlideDefense(StatusTerminalState& state,
                                          StatusTerminalHost& host,
                                          bool entering) {
  const auto offsets = host.Tables().row_offsets;
  if (state.animation_wait <= 0 ||
      static_cast<std::size_t>(state.animation_wait) > offsets.size()) {
    return std::unexpected(TerminalError::kInvalidAnimation);
  }
  const int offset = offsets[static_cast<std::size_t>(--state.animation_wait)];
  host.Frame().animation_step = entering ? kModelSlide - offset : offset;
  return 0;
}
[[nodiscard]] TerminalResult DefenseInput(StatusTerminalState& state,
                                          StatusTerminalHost& host) {
  auto& frame = host.Frame();
  const auto actor = Selected(host);
  if (!actor) {
    return std::unexpected(TerminalError::kMissingActor);
  }
  const int limbs = LimbCount(actor->limb_names);
  if (limbs == 0 || frame.selected_element >= limbs || frame.stat_page > 2) {
    return std::unexpected(TerminalError::kInvalidSelection);
  }
  if ((frame.held & (kL1 | kR1)) != (kL1 | kR1)) {
    int limb = frame.selected_element;
    if ((frame.repeat & kL1) != 0) {
      limb = limb - 1 + limbs;
    }
    if ((frame.repeat & kR1) != 0) {
      ++limb;
    }
    if (limb >= limbs) {
      limb -= limbs;
    }
    if (limb != frame.selected_element) {
      if (!host.Sound(TerminalSound::kChange)) {
        return Unavailable();
      }
      frame.selected_element = static_cast<std::uint8_t>(limb);
      if (!host.Defense()) {
        return Unavailable();
      }
    }
  }
  const int count = kFirstStatRows - frame.stat_page * kStatRowsPerPage;
  int row = std::min(static_cast<int>(frame.selected_stat), count - 1);
  if ((frame.repeat & kUp) != 0) {
    row = row - 1 + count;
  }
  if ((frame.repeat & kDown) != 0) {
    ++row;
  }
  if (row >= count) {
    row -= count;
  }
  if (row != frame.selected_stat) {
    if (!host.Sound(TerminalSound::kChange)) {
      return Unavailable();
    }
    frame.selected_stat = static_cast<std::uint8_t>(row);
  }
  const auto text =
      host.StatusText(row + (count == 3 ? kBluntDefense : kPhysicalDefense));
  if (text.empty()) {
    return std::unexpected(TerminalError::kMissingText);
  }
  if (!host.Information(text)) {
    return Unavailable();
  }
  const auto cursor =
      host.Cursor(static_cast<std::uint32_t>(state.cursor_animation),
                  Render::PackScreenWord(0, row * kStatRowPitch + kStatRowTop));
  if (!cursor) {
    return Unavailable();
  }
  state.cursor_animation = *cursor;
  return 0;
}

[[nodiscard]] TerminalResult Leave(StatusTerminalState& state,
                                   StatusTerminalHost& host,
                                   std::uint8_t& menu_state) {
  auto& frame = host.Frame();
  if (!(host.PrimaryStats(2) && host.LimbPanel(-2) && host.LimbPanel(0) &&
        host.BeginVitals(0) && host.Vitals() &&
        host.SetProjection(state.saved_projection))) {
    return Unavailable();
  }
  host.View() = state.saved_view;
  if (!(host.FieldRendering(true) &&
        host.CommandLabel(Framework::MenuCommand::kNone) &&
        host.ClearRows(static_cast<int>(Framework::kRowCount)) &&
        host.ReleasePreview() &&
        host.BackgroundPriority(-2, kBackgroundBrightness))) {
    return Unavailable();
  }
  frame.hud_hidden = 0;
  state.animation_wait = kExitFrames;
  state.screen_enabled = 0;
  ++menu_state;
  return 0;
}
[[nodiscard]] TerminalResult FinishLeave(StatusTerminalState& state,
                                         StatusTerminalHost& host,
                                         std::uint8_t& menu_state) {
  if (!(host.LimbPanel(0) && host.Vitals() && host.BackgroundFade() &&
        host.DismissInformation())) {
    return Unavailable();
  }
  if (state.animation_wait <= 0) {
    const auto unloaded = host.UnloadItemText();
    if (!unloaded) {
      return Unavailable();
    }
    if (*unloaded != 0) {
      if (menu_state == phase::kQuitBattle) {
        host.Frame().current_menu = kBattleMenu;
      }
      menu_state = phase::kNone;
      return 1;
    }
  }
  state.animation_wait = math::WrappingSubtract(state.animation_wait, 1);
  return 0;
}
[[nodiscard]] TerminalResult Prompt(StatusTerminalState& state,
                                    StatusTerminalHost& host, bool details,
                                    bool available) {
  const auto tables = host.Tables();
  if (details) {
    if (state.prompt_animation != 0) {
      --state.prompt_animation;
    }
  } else {
    if (state.prompt_animation >= tables.animation_indices.size()) {
      return std::unexpected(TerminalError::kInvalidAnimation);
    }
    state.prompt_animation = tables.animation_indices[state.prompt_animation];
  }
  if (state.prompt_animation < kAnimationFrames) {
    if (state.prompt_animation >= tables.row_offsets.size()) {
      return std::unexpected(TerminalError::kInvalidAnimation);
    }
    if (!host.Prompt(tables.row_offsets[state.prompt_animation], available)) {
      return Unavailable();
    }
  }
  return 0;
}

[[nodiscard]] TerminalResult Step(StatusTerminalState& state,
                                  StatusTerminalHost& host,
                                  std::uint8_t& menu_state, bool& details,
                                  bool& available) {
  switch (menu_state) {
    case phase::kInitialize:
      return Initialize(state, host, menu_state);
    case phase::kWaitMenu:
      if (state.animation_wait != 0) {
        state.animation_wait = math::WrappingSubtract(state.animation_wait, 1);
        if (!(host.LimbPanel(-1) && host.PrimaryStats(1))) {
          return Unavailable();
        }
      } else {
        if (!host.Condition()) {
          return Unavailable();
        }
        menu_state = phase::kInspect;
      }
      break;
    case phase::kInspect:
    case phase::kEffects:
      return InspectOrEffects(state, host, menu_state, details, available);
    case phase::kWaitEquipment:
      if (state.animation_wait != 0) {
        state.animation_wait = math::WrappingSubtract(state.animation_wait, 1);
      } else {
        if (!host.EquipmentList(host.Frame().selected_element + 1)) {
          return Unavailable();
        }
        menu_state = phase::kEquipment;
      }
      break;
    case phase::kEquipment: {
      const auto result = host.EquipmentList(0);
      if (!result) {
        return Unavailable();
      }
      if (*result == -2) {
        menu_state = phase::kWaitQuitBattle;
      } else if (*result != 0) {
        state.animation_wait = kExitFrames;
        menu_state = phase::kWaitMenu;
      }
      break;
    }
    case phase::kDefense:
      if (state.animation_wait != 0) {
        return SlideDefense(state, host, true);
      }
      if ((host.Frame().pressed & (kTriangle | kCross)) == 0) {
        return DefenseInput(state, host);
      }
      if (!(host.Sound(TerminalSound::kLeave) && host.StatPages(-1))) {
        return Unavailable();
      }
      if ((host.Frame().pressed & kTriangle) != 0) {
        menu_state = phase::kWaitQuitBattle;
      } else {
        state.animation_wait = kAnimationFrames;
        menu_state = phase::kLeaveDefense;
      }
      break;
    case phase::kLeaveDefense:
      details = true;
      if (state.animation_wait != 0) {
        return SlideDefense(state, host, false);
      }
      if (!host.LimbPanel(host.Frame().selected_element + 1)) {
        return Unavailable();
      }
      menu_state = phase::kInspect;
      break;
    case phase::kWaitQuitMenu:
    case phase::kWaitQuitBattle:
      return Leave(state, host, menu_state);
    default:
      break;
  }
  return 0;
}
}  // namespace

std::string_view TerminalErrorText(TerminalError error) noexcept {
  switch (error) {
    case TerminalError::kMissingActor:
      return "actor or preview owner unavailable";
    case TerminalError::kInvalidSelection:
      return "selection outside bounded table";
    case TerminalError::kInvalidAnimation:
      return "animation outside bounded table";
    case TerminalError::kMissingRows:
      return "MAINMENU rows unavailable";
    case TerminalError::kMissingText:
      return "bounded text unavailable";
    case TerminalError::kUnavailableService:
      return "required service refused";
  }
  return "unknown terminal refusal";
}

// decomp: vs_menu4_exec (src/MENU/MENU4.PRG/120.c:2272)
TerminalResult UpdateStatusTerminal(StatusTerminalState& state,
                                    StatusTerminalHost& host,
                                    std::uint8_t& menu_state) {
  bool details = false;
  bool available = false;
  if (menu_state == phase::kQuitMenu || menu_state == phase::kQuitBattle) {
    const auto result = FinishLeave(state, host, menu_state);
    if (!result || *result != 0) {
      return result;
    }
  } else {
    const auto result = Step(state, host, menu_state, details, available);
    if (!result) {
      return result;
    }
  }
  if (state.screen_enabled != 0 && !host.DrawFrame()) {
    return Unavailable();
  }
  return Prompt(state, host, details, available);
}
}  // namespace VS::Menu::Status
