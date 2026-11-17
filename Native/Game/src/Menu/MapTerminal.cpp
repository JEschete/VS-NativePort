#include "Menu/MapTerminal.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Core/Math/ScalarMath.hpp"
#include "Menu/Framework/Framework.hpp"

namespace VS::Menu::map {
namespace {
namespace math = Core::Math;
constexpr std::int32_t kFixedOne = 4096;
constexpr std::int32_t kBackgroundSlot = 2046;
constexpr std::int32_t kDistortionSlot = 8;
constexpr std::int16_t kLeftWidth = 192;
constexpr std::int16_t kRightWidth = 128;
constexpr std::int16_t kScreenHeight = 240;
constexpr std::int32_t kDistortionRows = 239;
constexpr std::uint16_t kSubtractPage = 0x60;
constexpr std::uint16_t kFrontLeftPage = 0x100;
constexpr std::uint16_t kBackLeftPage = 0x105;
constexpr std::uint16_t kFrontRightPage = 0x103;
constexpr std::uint16_t kBackRightPage = 0x108;
constexpr unsigned kDistortionPhaseShift = 6;
constexpr std::int32_t kDistortionDivisor = 128;
constexpr std::int32_t kStickCenter = kMapStickCenter;
constexpr std::int32_t kDeadZone = 64;
constexpr std::int32_t kRotationStep = 64;
constexpr std::int16_t kPitchLimit = 1024;
constexpr std::int32_t kZoomStep = 16;
constexpr std::int32_t kNearDistance = 512;
constexpr std::int32_t kFarDistance = 1024;
constexpr std::int32_t kPanStep = 4;
constexpr std::int32_t kPanWidth = 96;
constexpr std::int32_t kPanHeight = 80;
constexpr std::int32_t kSlideStep = 64;
constexpr std::int32_t kSlideExtent = 384;
constexpr std::int32_t kBrightnessStep = 16;
constexpr std::int32_t kDimBackground = 32;
constexpr std::int32_t kFullBackground = 128;
constexpr std::int32_t kLabelY = 188;
constexpr std::int16_t kHiddenControls = 5;
constexpr std::int16_t kRotationControls = 3;
constexpr std::int32_t kInitialPhase = 0;
constexpr std::int32_t kInputPhase = 1;
constexpr std::int32_t kAreaPhase = 2;
constexpr std::int32_t kQuitPhase = 3;
constexpr std::int32_t kReadPhase = 4;
constexpr std::int32_t kSlidePhase = 5;
constexpr std::int32_t kChooseVolume = 5;
constexpr std::int32_t kCancelVolume = 6;
constexpr std::int32_t kSwitchMenuResult = 2;
constexpr std::int32_t kCancelToMenu = -3;
constexpr std::int32_t kCancelToMap = -2;
constexpr std::int32_t kPendingArea = -1;
constexpr std::uint32_t kShoulders = 12;

bool Down(std::uint32_t buttons, MapButton button) {
  return (buttons & static_cast<std::uint32_t>(button)) != 0;
}

bool Moved(std::uint8_t x, std::uint8_t y) {
  const auto outside = [](std::uint8_t value) {
    const int delta = value - kStickCenter;
    return delta < -kDeadZone || delta > kDeadZone;
  };
  return outside(x) || outside(y);
}

std::int32_t Axis(std::uint8_t value) {
  const int delta = value - kStickCenter;
  return delta < -kDeadZone ? -1 : (delta > kDeadZone ? 1 : 0);
}

std::int16_t AddAngle(std::int16_t value, std::int32_t change) {
  return math::WrapSigned16(static_cast<std::uint32_t>(value + change));
}

void RotateStick(MapAngles& angles, std::uint8_t x, std::uint8_t y) {
  angles.y = AddAngle(angles.y, Axis(x) * kRotationStep);
  angles.x = AddAngle(angles.x, -Axis(y) * kRotationStep);
  angles.x = std::clamp<std::int16_t>(angles.x, 0, kPitchLimit);
}

void RotateButtons(MapAngles& angles, std::uint32_t buttons) {
  if (Down(buttons, MapButton::kLeft)) {
    angles.y = AddAngle(angles.y, -kRotationStep);
  } else if (Down(buttons, MapButton::kRight)) {
    angles.y = AddAngle(angles.y, kRotationStep);
  }
  // The digital path tests before stepping; unlike the stick path it does
  // not clamp a non-multiple-of-64 pitch.
  if (Down(buttons, MapButton::kDown) && angles.x > 0) {
    angles.x = AddAngle(angles.x, -kRotationStep);
  } else if (Down(buttons, MapButton::kUp) && angles.x < kPitchLimit) {
    angles.x = AddAngle(angles.x, kRotationStep);
  }
}

void Zoom(SceneMapView& view, const MapInput& input) {
  if (Moved(input.right_x, input.right_y)) {
    view.camera_distance =
        std::clamp(math::WrappingAdd(view.camera_distance,
                                     Axis(input.right_y) * kZoomStep),
                   kNearDistance, kFarDistance);
  } else if (Down(input.held, MapButton::kUp) &&
             view.camera_distance > kNearDistance) {
    view.camera_distance =
        math::WrappingSubtract(view.camera_distance, kZoomStep);
  } else if (Down(input.held, MapButton::kDown) &&
             view.camera_distance < kFarDistance) {
    view.camera_distance = math::WrappingAdd(view.camera_distance, kZoomStep);
  }
}

void Pan(SceneMapView& view, const MapInput& input) {
  if (Moved(input.right_x, input.right_y)) {
    view.geometry_offset_x =
        std::clamp(math::WrappingAdd(view.geometry_offset_x,
                                     Axis(input.right_x) * kPanStep),
                   -kPanWidth, kPanWidth);
    view.geometry_offset_y =
        std::clamp(math::WrappingAdd(view.geometry_offset_y,
                                     Axis(input.right_y) * kPanStep),
                   -kPanHeight, kPanHeight);
    return;
  }
  if (Down(input.held, MapButton::kUp) &&
      view.geometry_offset_y > -kPanHeight) {
    view.geometry_offset_y -= kPanStep;
  } else if (Down(input.held, MapButton::kDown) &&
             view.geometry_offset_y < kPanHeight) {
    view.geometry_offset_y += kPanStep;
  }
  if (Down(input.held, MapButton::kLeft) &&
      view.geometry_offset_x > -kPanWidth) {
    view.geometry_offset_x -= kPanStep;
  } else if (Down(input.held, MapButton::kRight) &&
             view.geometry_offset_x < kPanWidth) {
    view.geometry_offset_x += kPanStep;
  }
}

bool ProcessViewInput(MapMenuHost& host, MapMenuState& state,
                      const MapInput& input, std::int16_t& commands) {
  auto& view = state.view;
  if (Moved(input.left_x, input.left_y)) {
    commands = kRotationControls;
    RotateStick(view.camera_angles, input.left_x, input.left_y);
  } else if (Down(input.held, MapButton::kL2)) {
    commands = kRotationControls;
    if (Moved(input.right_x, input.right_y)) {
      RotateStick(view.camera_angles, input.right_x, input.right_y);
    } else {
      RotateButtons(view.camera_angles, input.held);
    }
  } else if (Down(input.held, MapButton::kR2)) {
    commands = 2;
    Zoom(view, input);
  } else if (Down(input.held, MapButton::kStart)) {
    view.geometry_offset_x = 0;
    view.geometry_offset_y = 0;
    view.camera_distance = kMapCameraDistance;
    view.camera_angles.x = kMapCameraPitch;
    view.camera_angles.y = math::WrapSigned16(
        static_cast<std::uint32_t>(kMapYawBase - input.field_yaw));
    view.current_room = host.InitialRoom();
    host.CenterOnRoom(view.current_room);
  } else {
    const auto previous_room = view.current_room;
    Pan(view, input);
    if ((input.held & kShoulders) != kShoulders) {
      const auto direction =
          Down(input.repeated, MapButton::kR1)
              ? 1
              : (Down(input.repeated, MapButton::kL1) ? -1 : 0);
      if (direction != 0) {
        view.current_room = host.SelectRoom(direction, view.current_room);
        host.SetCenterTarget(view.current_room);
      }
    }
    if (previous_room != view.current_room &&
        !host.ShowRoomName(view.current_room)) {
      return false;
    }
  }
  return true;
}

bool ProcessInput(MapMenuHost& host, MapMenuState& state, const MapInput& input,
                  bool jammed) {
  auto& view = state.view;
  std::int16_t commands = 0;
  if (view.trigger_hide_ui != view.hide_ui) {
    view.hide_ui = view.trigger_hide_ui;
    host.SetVisible(view.trigger_hide_ui == 0);
  }
  if (Down(input.pressed, MapButton::kCircle)) {
    commands = kHiddenControls;
    host.PlaySound(kChooseVolume);
    view.menu_state = kAreaPhase;
    if (view.hide_ui != 0) {
      view.hide_ui = 0;
      host.SetVisible(true);
    }
    static_cast<void>(host.PollAreas(true));
  } else if (Down(input.pressed, MapButton::kTriangle) ||
             Down(input.pressed, MapButton::kCross)) {
    host.PlaySound(kCancelVolume);
    if (Down(input.pressed, MapButton::kTriangle)) {
      view.quit_state = kSwitchMenuResult;
    }
    view.menu_state = kQuitPhase;
    host.DismissRoomText();
  } else if (jammed) {
    commands = kHiddenControls;
  } else if (!ProcessViewInput(host, state, input, commands)) {
    return false;
  }
  view.menu_commands_state = commands;
  host.DrawFrame();
  return true;
}

bool ValidScene(std::int32_t scene, std::span<const std::uint8_t> paling,
                std::span<const DiscFileRange> files) {
  return scene >= 0 && static_cast<std::size_t>(scene) < paling.size() &&
         static_cast<std::size_t>(scene) < files.size();
}

bool ProcessArea(MapMenuHost& host, MapMenuState& state,
                 std::span<const std::uint8_t> paling,
                 std::span<const std::int32_t> rows,
                 std::span<const DiscFileRange> files) {
  host.DrawFrame();
  auto& view = state.view;
  state.selected_map = host.PollAreas(false);
  switch (state.selected_map) {
    case kPendingArea:
      return true;
    case kCancelToMenu:
      view.menu_state = kQuitPhase;
      view.quit_state = kSwitchMenuResult;
      host.DismissRoomText();
      return true;
    case kCancelToMap:
      view.menu_state = kInputPhase;
      host.ShowAreaName(view.current_scene,
                        view.current_scene == view.current_scene_row);
      return true;
    default:
      break;
  }
  if (state.selected_map < 0 ||
      static_cast<std::size_t>(state.selected_map) >= rows.size()) {
    return false;
  }
  const auto scene = rows[state.selected_map];
  if (!ValidScene(scene, paling, files) || files[scene].bytes <= 0 ||
      files[scene].bytes % sizeof(std::uint32_t) != 0) {
    return false;
  }
  const auto old_scene = view.current_scene;
  const auto old_paling = view.is_paling;
  view.is_paling = paling[old_scene];
  view.current_scene = scene;
  view.menu_state = kReadPhase;
  if (!host.BeginSceneRead(scene, files[scene])) {
    view.current_scene = old_scene;
    view.is_paling = old_paling;
    view.menu_state = kAreaPhase;
    return false;
  }
  return true;
}
}  // namespace

// F02690: _setCamera, MENU5.PRG 0x80104384..0x8010451C.
MapCamera ApplyCamera(MapAngles angles, std::int32_t distance,
                      MapAngles previous_position) {
  const auto product = [](std::int32_t left, std::int32_t right) {
    return math::WrappingMultiply(left, right) / kFixedOne;
  };
  const auto pitch_cosine = math::Cosine(angles.x);
  const auto inverse_distance = math::WrappingNegate(distance);
  previous_position.x = math::WrapSigned16(static_cast<std::uint32_t>(
      product(product(math::Sine(angles.y), distance), pitch_cosine)));
  previous_position.z = math::WrapSigned16(static_cast<std::uint32_t>(product(
      product(math::Cosine(angles.y), inverse_distance), pitch_cosine)));
  previous_position.y = math::WrapSigned16(static_cast<std::uint32_t>(
      product(math::Sine(angles.x), inverse_distance)));
  const auto rotation =
      math::MultiplyMatrixIntoRight(Framework::BuildPitchMatrix(angles.x),
                                    Framework::BuildYawMatrix(angles.y));
  const math::Vec3 negative{
      AddAngle(0, -static_cast<std::int32_t>(previous_position.x)),
      AddAngle(0, -static_cast<std::int32_t>(previous_position.y)),
      AddAngle(0, -static_cast<std::int32_t>(previous_position.z))};
  // ApplyMatrix at 8004177C stores MAC1..3, unlike ApplyRotMatrix (#411).
  return {
      .position = previous_position,
      .transform = {rotation, math::RotateTranslate({rotation, {}}, negative)}};
}

// F02699: _darkenBackground, MENU5.PRG 0x8010625C..0x80106570.
bool DarkenBackground(FramebufferHost& host, bool back_buffer) {
  return host.TexturePage(kBackgroundSlot, kSubtractPage) &&
         host.Strip(kBackgroundSlot, {.width = kLeftWidth,
                                      .height = kScreenHeight,
                                      .raw_texture = true}) &&
         host.TexturePage(kBackgroundSlot,
                          back_buffer ? kBackLeftPage : kFrontLeftPage) &&
         host.Strip(kBackgroundSlot, {.x = kLeftWidth,
                                      .width = kRightWidth,
                                      .height = kScreenHeight,
                                      .raw_texture = true}) &&
         host.TexturePage(kBackgroundSlot,
                          back_buffer ? kBackRightPage : kFrontRightPage);
}

// F02702: _applyPalingScreenEffect, MENU5.PRG 801066E0..80106AF4.
bool DrawPalingDistortion(FramebufferHost& host, bool back_buffer,
                          std::int32_t frame_counter) {
  if (!host.TexturePage(kBackgroundSlot, kSubtractPage)) return false;
  for (std::int32_t row = 0; row < kDistortionRows; ++row) {
    const auto phase = static_cast<std::int32_t>(
        static_cast<std::uint32_t>(math::WrappingAdd(frame_counter, row))
        << kDistortionPhaseShift);
    const auto displacement = math::Sine(phase) / kDistortionDivisor;
    const auto offset = (row & 1) != 0 ? displacement : -displacement;
    if (!host.Strip(kDistortionSlot,
                    {.x = static_cast<std::int16_t>(offset),
                     .y = static_cast<std::int16_t>(row),
                     .width = kLeftWidth,
                     .height = 1,
                     .texture_y = static_cast<std::uint8_t>(row)}) ||
        !host.TexturePage(kDistortionSlot,
                          back_buffer ? kBackLeftPage : kFrontLeftPage) ||
        !host.Strip(kDistortionSlot,
                    {.x = static_cast<std::int16_t>(kLeftWidth + offset),
                     .y = static_cast<std::int16_t>(row),
                     .width = kRightWidth,
                     .height = 1,
                     .texture_y = static_cast<std::uint8_t>(row),
                     .clear_clut = false}) ||
        !host.TexturePage(kDistortionSlot,
                          back_buffer ? kBackRightPage : kFrontRightPage)) {
      return false;
    }
  }
  return true;
}

// F02689: _renderScreen, MENU5.PRG 0x801042B0..0x80104384.
bool DrawFrame(MapFrameHost& host, bool jammed, bool leaving_jammed_scene) {
  host.DrawControls();
  if (!host.EnableMaskWrite()) return false;
  if (jammed) {
    host.DrawDistortion();
    host.SetPalingCaption();
  } else {
    host.ApplyView();
    host.Darken();
    if (!leaving_jammed_scene) host.DrawRooms();
  }
  return true;
}

// F04151: _menuManager, MENU5.PRG 0x80103684..0x801042B0.
std::optional<std::int32_t> UpdateMapMenu(
    MapMenuHost& host, MapMenuState& state, const MapInput& input,
    std::span<const std::uint8_t> paling,
    std::span<const std::int32_t> scene_rows,
    std::span<const DiscFileRange> files) {
  auto& view = state.view;
  if (!ValidScene(view.current_scene, paling, files)) return std::nullopt;
  host.SetFarColorBlack();
  state.label_y = kLabelY;
  if (Down(input.pressed, MapButton::kSelect)) view.trigger_hide_ui ^= 1;
  const bool jammed = paling[view.current_scene] != 0;
  host.SetBackgroundPriority(
      kBackgroundSlot,
      jammed
          ? kFullBackground
          : math::WrappingAdd(view.background_brightness, kFullBackground) / 2);
  if (view.menu_state != kInputPhase) {
    view.menu_commands_state = kHiddenControls;
  }
  switch (view.menu_state) {
    case kInitialPhase:
      if (view.background_brightness > kDimBackground) {
        view.background_brightness -= kBrightnessStep;
        view.geometry_offset_x =
            math::WrappingAdd(view.geometry_offset_x, kSlideStep);
      } else {
        if (!host.InitializeRoomText() ||
            !host.ShowRoomName(view.current_room)) {
          return std::nullopt;
        }
        ++view.menu_state;
      }
      host.DrawFrame();
      break;
    case kInputPhase:
      if (!ProcessInput(host, state, input, jammed)) return std::nullopt;
      break;
    case kAreaPhase:
      if (!ProcessArea(host, state, paling, scene_rows, files)) {
        return std::nullopt;
      }
      break;
    case kQuitPhase:
      view.ui_animation_state = 2;
      if (view.background_brightness >= kFullBackground) {
        if (!host.FreeScene()) return std::nullopt;
        return view.quit_state;
      }
      view.background_brightness =
          math::WrappingAdd(view.background_brightness, kBrightnessStep);
      view.geometry_offset_x =
          math::WrappingAdd(view.geometry_offset_x, kSlideStep);
      host.DrawFrame();
      break;
    case kReadPhase:
      view.geometry_offset_x =
          math::WrappingAdd(view.geometry_offset_x, kSlideStep);
      if (host.SceneReadState() == kDiscReadLoaded &&
          view.geometry_offset_x >= kSlideExtent) {
        view.is_paling = 0;
        if (!host.InstallSceneRead(view.current_scene,
                                   files[view.current_scene])) {
          return std::nullopt;
        }
        view.current_room = host.InitialRoom();
        host.CenterOnRoom(view.current_room);
        if (!host.ShowRoomName(view.current_room)) return std::nullopt;
        view.geometry_offset_x = -kSlideExtent;
        view.geometry_offset_y = 0;
        view.menu_state = kSlidePhase;
      }
      host.DrawFrame();
      break;
    case kSlidePhase:
      view.geometry_offset_x =
          math::WrappingAdd(view.geometry_offset_x, kSlideStep);
      if (view.geometry_offset_x >= 0) view.menu_state = kInputPhase;
      host.DrawFrame();
      break;
    default:
      break;
  }
  view.menu_frames = math::WrappingAdd(view.menu_frames, 1);
  host.AnimateCentering();
  return 0;
}

// F04149: vs_menu5_exec, MENU5.PRG 0x801030F4..0x80103418.
std::optional<std::int32_t> UpdateScreen(MapScreenHost& host,
                                         MapScreenState& state,
                                         std::uint8_t& step,
                                         std::int32_t background_fade_step) {
  constexpr std::int32_t kOpenDelay = 8;
  constexpr std::int32_t kCloseDelay = 10;
  constexpr std::int32_t kHiddenHudSlide = 0x200000;
  constexpr std::uint8_t kAlternateMenu = 6;
  constexpr std::uint8_t kCaptureScreen = 3;
  constexpr std::uint8_t kLoadBackground = 4;
  constexpr std::uint8_t kLoadScene = 5;
  constexpr std::uint8_t kWaitRows = 6;
  constexpr std::uint8_t kManageMap = 7;
  constexpr std::uint8_t kBeginFadeOut = 8;
  constexpr std::uint8_t kWaitFadeOut = 9;
  constexpr std::uint8_t kReleaseScreen = 10;
  switch (step) {
    case kCaptureScreen:
      if (!host.CaptureBackground()) return std::nullopt;
      static_cast<void>(host.LoadBackground(true));
      step = kLoadBackground;
      break;
    case kLoadBackground:
      if (host.LoadBackground(false) == 0) break;
      step = kLoadScene;
      host.FadeBackground(true);
      state.load_delay = kOpenDelay;
      state.scene_loaded = 0;
      [[fallthrough]];
    case kLoadScene:
      if (state.scene_loaded == 0) state.scene_loaded = host.LoadScene();
      if (state.load_delay != 0) {
        state.load_delay = math::WrappingSubtract(state.load_delay, 1);
        break;
      }
      if (state.scene_loaded == 0) break;
      host.SetFieldRendering(false);
      host.SetHudHidden(true);
      host.SetHudSlide(kHiddenHudSlide);
      step = kWaitRows;
      [[fallthrough]];
    case kWaitRows:
      step = static_cast<std::uint8_t>(
          math::WrappingAdd(step, host.MenuRowsIdle()));
      break;
    case kManageMap:
      state.menu_result = host.UpdateMenu();
      if (state.menu_result != 0) {
        host.RestoreMenuPriority();
        host.SetHudHidden(false);
        host.SetHudSlide(0);
        host.SetFieldRendering(true);
        host.ClearCommand();
        host.ClearRows();
        state.load_delay = kCloseDelay;
        step = kBeginFadeOut;
      }
      break;
    case kBeginFadeOut:
      host.FadeBackground(false);
      step = kWaitFadeOut;
      break;
    case kWaitFadeOut:
      if (background_fade_step == 0) {
        if (!host.RestoreBackground()) return std::nullopt;
        step = kReleaseScreen;
      }
      break;
    case kReleaseScreen:
      if (state.menu_result == kSwitchMenuResult) {
        host.SetDestination(kAlternateMenu);
      }
      if (!host.ReleaseBackground()) return std::nullopt;
      host.ShowMenu();
      step = 0;
      return 1;
    default:
      break;
  }
  return 0;
}
}  // namespace VS::Menu::map
