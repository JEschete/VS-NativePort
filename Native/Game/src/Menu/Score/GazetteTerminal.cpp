#include "Menu/Score/GazetteTerminal.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "Core/Math/ScalarMath.hpp"
#include "Core/String/GameText.hpp"
#include "Menu/Score/GazetteTimes.hpp"

namespace VS::Menu::Score {
namespace {
namespace math = Core::Math;
constexpr std::int32_t kSettled = 8;
constexpr int kBackgroundFront = 2047;
constexpr int kBackgroundBack = -4;
constexpr int kBackgroundBrightness = 128;
constexpr int kOverviewDistance = 336;
constexpr std::int16_t kOverviewHeight = 107;
constexpr std::int16_t kOverviewPitch = 3904;
constexpr std::int16_t kOverviewYaw = 3655;
constexpr int kRankX = 11;
constexpr int kRankY = 248;
constexpr int kRankColumns = 16;
constexpr int kRankLines = 1;
constexpr int kRankVerticalStep = 6;
constexpr int kRankHorizontalStep = 16;
constexpr std::int32_t kTopWaiting = -1;
constexpr std::int32_t kTopCancel = -2;
constexpr std::int32_t kTopExit = -3;
constexpr std::int32_t kEncyclopediaChoice = 3;
constexpr std::int32_t kReturnToOverview = 0;
constexpr std::int32_t kLeaveMenu = 1;
constexpr std::int32_t kOpenViewer = 2;
constexpr std::int32_t kExitScreen = 2;
constexpr int kSelectionSound = 4;
constexpr int kConfirmSound = 5;
constexpr int kCancelSound = 6;
constexpr int kLockedSound = 7;
constexpr int kEnemyChangeSound = 11;
constexpr std::uint32_t kL2 = 1;
constexpr std::uint32_t kR2 = 2;
constexpr std::uint32_t kL1 = 4;
constexpr std::uint32_t kR1 = 8;
constexpr std::uint32_t kTriangle = 0x10;
constexpr std::uint32_t kCircle = 0x20;
constexpr std::uint32_t kCross = 0x40;
constexpr std::uint32_t kSelect = 0x100;
constexpr std::uint32_t kUp = 0x1000;
constexpr std::uint32_t kRight = 0x2000;
constexpr std::uint32_t kDown = 0x4000;
constexpr std::uint32_t kLeft = 0x8000;
constexpr int kStickThreshold = 64;
constexpr int kRotationStep = 64;
constexpr std::int16_t kMaximumPitch = 1024;
constexpr std::int16_t kInitialViewerYaw = 256;
constexpr int kMinimumDistance = 640;
constexpr int kMaximumDistance = 6144;
constexpr int kPreviewDistanceScale = 4;
constexpr int kAnalogZoomStep = 4;
constexpr int kDigitalZoomStep = 64;
constexpr std::uint32_t kBrightControls = 0x808080;
constexpr std::uint32_t kDimControls = 0x202020;
constexpr std::uint32_t kNavigationMask = 15;
constexpr std::uint32_t kSkipDelayMask = 3;
constexpr std::uint8_t kLastVisibleRow = 7;
constexpr std::uint8_t kScrollUpRow = 2;
constexpr std::uint8_t kScrollDownRow = 6;
constexpr std::int32_t kSwitchRight = 1;
constexpr std::int32_t kSwitchLeft = 2;
constexpr std::int32_t kSwitchHidden = 3;
constexpr int kTransitionLeftArrow = 2;
constexpr int kTransitionRightArrow = 3;
constexpr std::uint8_t kScreenInitialize = 3;
constexpr std::uint8_t kScreenPrepare = 4;
constexpr std::uint8_t kScreenReady = 5;
constexpr std::uint8_t kScreenActive = 6;
constexpr std::uint8_t kScreenLeave = 7;
constexpr std::uint8_t kScreenFade = 8;
constexpr std::uint8_t kReturnMenu = 9;
constexpr std::int32_t kHiddenHudOffset = 0x200000;

void Advance(GazetteState& state) {
  state.phase = static_cast<GazettePhase>(
      math::WrappingAdd(static_cast<std::int32_t>(state.phase), 1));
}

void EnterAnimation(GazetteState& state) {
  if (state.animation_step < kSettled) {
    state.animation_step = math::WrappingAdd(state.animation_step, 1);
  }
}

std::optional<std::int32_t> Refuse(GazetteHost& host, std::string_view reason) {
  host.Refuse(reason);
  return std::nullopt;
}

const GazetteEnemy* Enemy(GazetteHost& host, std::int32_t selected) {
  const auto enemies = host.Enemies();
  if (selected < 0 || static_cast<std::size_t>(selected) >=
                          std::min(enemies.size(), kEncyclopediaEnemies)) {
    host.Refuse("enemy selection outside the loaded list");
    return nullptr;
  }
  return &enemies[static_cast<std::size_t>(selected)];
}

int ListSelection(const GazetteCursorMemory& cursor) {
  return cursor.enemies.first_row + cursor.enemies.selected_row;
}

void NavigateRecords(GazetteRecordCursor& cursor, std::uint8_t last_page,
                     const GazetteInput& input, GazetteHost& host) {
  if (cursor.selected_row == 0 && cursor.first_row == 0 &&
      (input.pressed & kUp) != 0) {
    host.PlaySound(kSelectionSound);
    cursor.first_row = last_page;
    cursor.selected_row = kLastVisibleRow;
  } else if (cursor.selected_row == kLastVisibleRow &&
             cursor.first_row == last_page && (input.pressed & kDown) != 0) {
    host.PlaySound(kSelectionSound);
    cursor.first_row = 0;
    cursor.selected_row = 0;
  } else if ((input.repeated & kUp) != 0) {
    if (cursor.selected_row >= kScrollUpRow) {
      host.PlaySound(kSelectionSound);
      --cursor.selected_row;
    } else if (cursor.first_row != 0) {
      --cursor.first_row;
      host.PlaySound(kSelectionSound);
    } else if (cursor.selected_row != 0) {
      --cursor.selected_row;
      host.PlaySound(kSelectionSound);
    }
  } else if ((input.repeated & kDown) != 0) {
    if (cursor.selected_row < kScrollDownRow ||
        (cursor.first_row >= last_page &&
         cursor.selected_row < kLastVisibleRow)) {
      ++cursor.selected_row;
      host.PlaySound(kSelectionSound);
    } else if (cursor.first_row < last_page) {
      ++cursor.first_row;
      host.PlaySound(kSelectionSound);
    }
  }
}

void CancelPage(GazetteState& state, const GazetteInput& input,
                GazetteHost& host, bool time_trials = false) {
  if ((input.pressed & (kTriangle | kCross)) == 0) return;
  host.PlaySound(kCancelSound);
  host.DismissInformation();
  if (time_trials) host.LeaveTimeTrials();
  Advance(state);
  state.next_result =
      (input.pressed & kTriangle) != 0 ? kLeaveMenu : kReturnToOverview;
}

std::int32_t FinishPage(GazetteState& state, GazetteHost& host) {
  if (state.next_result == kReturnToOverview) {
    state.phase = GazettePhase::kPrepareOverview;
    return 0;
  }
  host.RestoreCamera();
  return kExitScreen;
}

std::optional<std::int32_t> PrepareOverview(GazetteState& state,
                                            GazetteHost& host) {
  host.LoadAshleyPreview();
  host.InitializeAshleyPose();
  const auto preset = host.PreviewCamera();
  const auto rank = host.RankText();
  if (!preset.has_value() ||
      std::ranges::find(rank, Core::String::kGameTextTerminator) ==
          rank.end()) {
    return Refuse(host, "overview preview/rank text is missing");
  }
  state.camera = {kOverviewPitch, kOverviewYaw, 0, kOverviewDistance,
                  kOverviewHeight};
  state.initial_camera_distance = preset->distance;
  host.InitializeRankBox(
      {kRankX, kRankY, kRankColumns, kRankLines, kRankX, kRankY});
  host.SetRankText(rank);
  host.UpdateTopMenu(true);
  state.animation_step = 0;
  Advance(state);
  return 0;
}

std::optional<std::int32_t> Overview(GazetteState& state,
                                     GazetteCursorMemory& cursor,
                                     GazetteHost& host) {
  host.DrawOverview(state.animation_step);
  if (state.animation_step < kSettled) {
    if (!host.MoveRankBox(0, -kRankVerticalStep, true)) {
      return Refuse(host, "overview entrance without its rank text box");
    }
    EnterAnimation(state);
  }
  const auto selection = host.UpdateTopMenu(false);
  if (selection == kTopWaiting) return 0;
  if (selection == kTopExit) {
    state.next_result = kExitScreen;
  } else if (selection == kTopCancel) {
    state.next_result = kLeaveMenu;
  } else {
    if (selection == kEncyclopediaChoice && !host.BeginEnemyRead()) {
      return Refuse(host, "could not begin enemy-list read");
    }
    state.next_result = kReturnToOverview;
  }
  cursor.overview_row = host.OverviewCursor();
  host.DismissRankBox();
  Advance(state);
  return 0;
}

std::int32_t LeaveOverview(GazetteState& state,
                           const GazetteCursorMemory& cursor,
                           GazetteHost& host) {
  host.DrawOverview(state.animation_step);
  if (state.animation_step > 0) {
    host.MoveRankBox(0, kRankVerticalStep, false);
    --state.animation_step;
    return 0;
  }
  host.ReleaseAshleyPreview();
  if (state.next_result == 0) {
    state.phase = static_cast<GazettePhase>(
        cursor.overview_row * kEncyclopediaChoice +
        static_cast<int>(GazettePhase::kPrepareTitles));
    return 0;
  }
  host.RestoreCamera();
  return state.next_result;
}

void PreparePage(GazetteState& state, GazettePage page, GazetteHost& host) {
  host.InitializeInformation();
  host.BuildPage(page);
  if (page != GazettePage::kTimeTrials) state.animation_step = 0;
  Advance(state);
}

std::optional<std::int32_t> LeaveTimeTrials(GazetteState& state,
                                            GazetteHost& host) {
  host.DrawPage(GazettePage::kTimeTrials, state.animation_step);
  if (state.animation_step > 0) {
    --state.animation_step;
    return 0;
  }
  const auto delays = host.TimeTrialDelays();
  if (delays.size() < kTimeTrialCount) {
    return Refuse(host, "time-trial cleanup without eight row delays");
  }
  for (state.animation_step = 0;
       state.animation_step < static_cast<int>(kTimeTrialCount);
       ++state.animation_step) {
    if (delays[static_cast<std::size_t>(state.animation_step)] == 0) {
      host.DeactivateRow(state.animation_step);
    }
  }
  state.animation_step = 0;
  return FinishPage(state, host);
}

std::optional<std::int32_t> WaitForEnemies(GazetteState& state,
                                           GazetteHost& host) {
  const auto read = host.EnemyReadStatus();
  if (read == GazetteReadStatus::kRefused) {
    return Refuse(host, "enemy-list read is absent, stale or failed");
  }
  if (read == GazetteReadStatus::kPending) return 0;
  if (!host.FinishEnemyRead()) {
    return Refuse(host, "enemy-list read has no bounded result");
  }
  PreparePage(state, GazettePage::kEnemies, host);
  return 0;
}

std::optional<std::int32_t> EnemyMenu(GazetteState& state,
                                      GazetteCursorMemory& cursor,
                                      const GazetteInput& input,
                                      GazetteHost& host) {
  NavigateRecords(cursor.enemies, kEncyclopediaEnemies - kGazetteVisibleRecords,
                  input, host);
  if ((input.pressed & kCircle) != 0) {
    const auto* enemy = Enemy(host, ListSelection(cursor));
    if (enemy == nullptr) return std::nullopt;
    if (enemy->unlocked != 0) {
      host.PlaySound(kConfirmSound);
      if (!host.LoadEnemyPreview(enemy->model_id).has_value()) {
        return Refuse(host, "enemy preview request refused");
      }
      Advance(state);
      state.next_result = kOpenViewer;
    } else {
      host.PlaySound(kLockedSound);
    }
  } else {
    CancelPage(state, input, host);
  }
  host.DrawPage(GazettePage::kEnemies, state.animation_step);
  EnterAnimation(state);
  return 0;
}

std::optional<std::int32_t> LeaveEnemies(GazetteState& state,
                                         GazetteHost& host) {
  host.DrawPage(GazettePage::kEnemies, state.animation_step);
  if (state.animation_step > 0) {
    --state.animation_step;
    return 0;
  }
  if (state.next_result == kOpenViewer) {
    state.phase = GazettePhase::kWaitForPreview;
    return 0;
  }
  if (!host.FreeEnemyList()) {
    return Refuse(host, "enemy-list owner could not release its read");
  }
  if (state.next_result == kLeaveMenu) {
    host.RestoreCamera();
    return kExitScreen;
  }
  state.phase = GazettePhase::kPrepareOverview;
  return 0;
}

bool SetPreviewCamera(GazetteState& state, GazetteHost& host) {
  const auto preset = host.PreviewCamera();
  if (!preset.has_value()) {
    host.Refuse("loaded enemy preview has no actor camera");
    return false;
  }
  state.initial_camera_distance = preset->distance;
  state.camera.height_offset = preset->height_offset;
  state.camera.distance =
      std::clamp(static_cast<int>(math::WrapSigned16(preset->distance)) *
                     kPreviewDistanceScale,
                 kMinimumDistance, kMaximumDistance);
  return true;
}

std::optional<std::int32_t> WaitForPreview(GazetteState& state,
                                           const GazetteCursorMemory& cursor,
                                           GazetteHost& host) {
  const auto* enemy = Enemy(host, ListSelection(cursor));
  if (enemy == nullptr) return std::nullopt;
  const auto pending = host.LoadEnemyPreview(enemy->model_id);
  if (!pending.has_value()) return Refuse(host, "enemy preview poll refused");
  if (*pending != 0) return 0;
  if (!SetPreviewCamera(state, host)) return std::nullopt;
  state.camera.yaw = kInitialViewerYaw;
  state.camera.pitch = 0;
  state.camera.roll = 0;
  state.enemy_button_or_slide = 0;
  state.selected_enemy = static_cast<std::int16_t>(ListSelection(cursor));
  host.ShowEnemyDescription(state.selected_enemy);
  host.SetMenuVisible(state.hide_ui == 0);
  Advance(state);
  return 0;
}

std::int32_t Stick(std::int32_t value) {
  return math::WrappingSubtract(value, kGazetteStickCenter);
}

bool Deflected(std::int32_t value) {
  const auto distance = value < 0 ? math::WrappingNegate(value) : value;
  return distance > kStickThreshold;
}

std::int16_t AddAngle(std::int16_t angle, int delta) {
  return math::WrapSigned16(static_cast<std::uint32_t>(angle + delta));
}

void Rotate(ViewerCameraSettings& camera, std::int32_t x, std::int32_t y) {
  if (x < -kStickThreshold) {
    camera.yaw = AddAngle(camera.yaw, -kRotationStep);
  } else if (x > kStickThreshold) {
    camera.yaw = AddAngle(camera.yaw, kRotationStep);
  }
  if (y < -kStickThreshold) {
    camera.pitch = AddAngle(camera.pitch, kRotationStep);
  } else if (y > kStickThreshold) {
    camera.pitch = AddAngle(camera.pitch, -kRotationStep);
  }
  camera.pitch = std::clamp(camera.pitch, std::int16_t{0}, kMaximumPitch);
}

void RotateDigital(ViewerCameraSettings& camera, std::uint32_t held) {
  if ((held & kUp) != 0) {
    camera.pitch = AddAngle(camera.pitch, kRotationStep);
  } else if ((held & kDown) != 0) {
    camera.pitch = AddAngle(camera.pitch, -kRotationStep);
  }
  if ((held & kRight) != 0) {
    camera.yaw = AddAngle(camera.yaw, kRotationStep);
  } else if ((held & kLeft) != 0) {
    camera.yaw = AddAngle(camera.yaw, -kRotationStep);
  }
  camera.pitch = std::clamp(camera.pitch, std::int16_t{0}, kMaximumPitch);
}

void Zoom(ViewerCameraSettings& camera, const GazetteInput& input) {
  if (Deflected(Stick(input.right_x)) || Deflected(Stick(input.right_y))) {
    if (Stick(input.right_y) < -kStickThreshold) {
      camera.distance =
          math::WrappingSubtract(camera.distance, kAnalogZoomStep);
    } else if (Stick(input.right_y) > kStickThreshold) {
      camera.distance = math::WrappingAdd(camera.distance, kAnalogZoomStep);
    }
  } else if ((input.held & kUp) != 0) {
    camera.distance = math::WrappingSubtract(camera.distance, kDigitalZoomStep);
  } else if ((input.held & kDown) != 0) {
    camera.distance = math::WrappingAdd(camera.distance, kDigitalZoomStep);
  }
  camera.distance =
      std::clamp(camera.distance, kMinimumDistance, kMaximumDistance);
}

void ViewerControls(GazetteState& state, const GazetteInput& input) {
  state.rotation_color = kBrightControls;
  state.zoom_color = kBrightControls;
  if (Deflected(Stick(input.left_x)) || Deflected(Stick(input.left_y))) {
    state.zoom_color = kDimControls;
    Rotate(state.camera, Stick(input.left_x), Stick(input.left_y));
  } else if ((input.held & kL2) != 0) {
    state.zoom_color = kDimControls;
    if (Deflected(Stick(input.right_x)) || Deflected(Stick(input.right_y))) {
      Rotate(state.camera, Stick(input.right_x), Stick(input.right_y));
    } else {
      RotateDigital(state.camera, input.held);
    }
  } else if ((input.held & kR2) != 0) {
    state.rotation_color = kDimControls;
    Zoom(state.camera, input);
  }
}

bool SkipEnemy(GazetteState& state, bool right, GazetteHost& host) {
  if (state.enemy_skip_delay == 0) {
    const auto* enemy = Enemy(host, state.selected_enemy);
    if (enemy == nullptr) return false;
    const auto next = right ? enemy->next : enemy->previous;
    if (next != state.selected_enemy) {
      if (Enemy(host, next) == nullptr) return false;
      host.PlaySound(kEnemyChangeSound);
      state.selected_enemy = next;
      state.enemy_button_or_slide = static_cast<int>(right ? kR1 : kL1);
      host.ShowEnemyDescription(state.selected_enemy);
    }
  }
  state.enemy_skip_delay = static_cast<std::int16_t>(
      (static_cast<std::uint32_t>(state.enemy_skip_delay) + 1U) &
      kSkipDelayMask);
  return true;
}

bool ViewerSelection(GazetteState& state, const GazetteInput& input,
                     GazetteHost& host) {
  if ((input.pressed & kSelect) != 0) {
    host.SetMenuVisible(state.hide_ui != 0);
    state.hide_ui ^= 1;
  }
  if ((input.held & (kL1 | kR1)) == (kL1 | kR1)) return true;
  if ((input.pressed & (kL1 | kR1)) != 0) state.enemy_skip_delay = 0;
  if ((input.repeated & kR1) != 0 && !SkipEnemy(state, true, host)) {
    return false;
  }
  return (input.repeated & kL1) == 0 || SkipEnemy(state, false, host);
}

void CancelViewer(GazetteState& state, const GazetteInput& input,
                  GazetteHost& host) {
  if ((input.pressed & (kTriangle | kCross)) == 0) return;
  host.PlaySound(kCancelSound);
  if (state.hide_ui != 0) host.SetMenuVisible(true);
  Advance(state);
  if ((input.pressed & kTriangle) != 0) {
    host.DismissInformation();
    state.next_result = kLeaveMenu;
  } else {
    state.next_result = kReturnToOverview;
  }
}

std::optional<std::int32_t> Viewer(GazetteState& state,
                                   const GazetteInput& input,
                                   GazetteHost& host) {
  const auto* enemy = Enemy(host, state.selected_enemy);
  if (enemy == nullptr) return std::nullopt;
  if (enemy->previous != state.selected_enemy) {
    state.navigation_step = static_cast<std::int32_t>(
        (static_cast<std::uint32_t>(state.navigation_step) + 1U) &
        kNavigationMask);
    host.DrawNavigation(0);
    host.DrawNavigation(1);
  }
  CancelViewer(state, input, host);
  ViewerControls(state, input);
  if (!ViewerSelection(state, input, host)) return std::nullopt;
  host.DrawViewer(state.animation_step, 0);
  if (state.animation_step < kSettled) {
    if (!host.MoveRankBox(-kRankHorizontalStep, 0, true)) {
      return Refuse(host, "viewer entrance without its text box");
    }
    EnterAnimation(state);
  }
  if ((input.released &
       static_cast<std::uint32_t>(state.enemy_button_or_slide)) != 0) {
    state.enemy_button_or_slide =
        state.enemy_button_or_slide == static_cast<int>(kR1) ? kSwitchRight
                                                             : kSwitchLeft;
    state.rotation_color = kDimControls;
    state.zoom_color = kDimControls;
    state.phase = GazettePhase::kSwitchLeave;
  }
  return 0;
}

std::optional<std::int32_t> LeaveViewer(GazetteState& state,
                                        const GazetteCursorMemory& cursor,
                                        GazetteHost& host) {
  state.rotation_color = kDimControls;
  state.zoom_color = kDimControls;
  host.DrawViewer(state.animation_step, 0);
  if (state.animation_step > 0) {
    host.MoveRankBox(kRankHorizontalStep, 0, false);
    --state.animation_step;
    return 0;
  }
  host.FreeEnemyPreview();
  if (state.next_result != 0) {
    if (!host.FreeEnemyList()) {
      return Refuse(host, "enemy-list cleanup refused");
    }
    host.RestoreCamera();
    return kExitScreen;
  }
  const int selected = ListSelection(cursor);
  if (Enemy(host, selected) == nullptr) return std::nullopt;
  state.selected_enemy = static_cast<std::int16_t>(selected);
  if (!host.ShowListDescription(selected)) {
    return Refuse(host, "return-to-list description is missing");
  }
  state.phase = GazettePhase::kEnemies;
  return 0;
}

std::optional<std::int32_t> SwitchPreview(GazetteState& state,
                                          const GazetteCursorMemory& cursor,
                                          GazetteHost& host) {
  host.DrawNavigation(kTransitionLeftArrow);
  host.DrawNavigation(kTransitionRightArrow);
  if (state.phase == GazettePhase::kSwitchLeave) {
    host.DrawViewer(state.animation_step, state.enemy_button_or_slide);
    if (state.animation_step > 0) {
      --state.animation_step;
      return 0;
    }
    host.FreeEnemyPreview();
    const auto* enemy = Enemy(host, state.selected_enemy);
    if (enemy == nullptr) return std::nullopt;
    if (!host.LoadEnemyPreview(enemy->model_id).has_value()) {
      return Refuse(host, "replacement preview request refused");
    }
    Advance(state);
  } else if (state.phase == GazettePhase::kSwitchLoad) {
    host.DrawViewer(state.animation_step, kSwitchHidden);
    // The original polls with the remembered list row, not selected_enemy.
    const auto* enemy = Enemy(host, ListSelection(cursor));
    if (enemy == nullptr) return std::nullopt;
    const auto pending = host.LoadEnemyPreview(enemy->model_id);
    if (!pending.has_value()) return Refuse(host, "replacement poll refused");
    if (*pending == 0) {
      if (!SetPreviewCamera(state, host)) return std::nullopt;
      Advance(state);
    }
  } else {
    host.DrawViewer(
        state.animation_step,
        math::WrappingSubtract(kSwitchHidden, state.enemy_button_or_slide));
    if (state.animation_step < kSettled) {
      EnterAnimation(state);
    } else {
      state.enemy_button_or_slide = 0;
      state.phase = GazettePhase::kViewer;
    }
  }
  return 0;
}

void UpdateRecordPage(GazetteState& state, GazetteRecordCursor& cursor,
                      GazettePage page, std::uint8_t last_page,
                      const GazetteInput& input, GazetteHost& host) {
  NavigateRecords(cursor, last_page, input, host);
  CancelPage(state, input, host);
  host.DrawPage(page, state.animation_step);
  EnterAnimation(state);
}

std::int32_t LeaveRecordPage(GazetteState& state, GazettePage page,
                             GazetteHost& host) {
  host.DrawPage(page, state.animation_step);
  if (state.animation_step <= 0) return FinishPage(state, host);
  --state.animation_step;
  return 0;
}

void UpdateTimeTrials(GazetteState& state, GazetteCursorMemory& cursor,
                      const GazetteInput& input, GazetteHost& host) {
  if ((input.repeated & kUp) != 0) {
    host.PlaySound(kSelectionSound);
    cursor.time_trial = static_cast<std::uint8_t>(
        (static_cast<unsigned>(cursor.time_trial) - 1U) &
        (kTimeTrialCount - 1));
  } else if ((input.repeated & kDown) != 0) {
    host.PlaySound(kSelectionSound);
    cursor.time_trial = static_cast<std::uint8_t>(
        (static_cast<unsigned>(cursor.time_trial) + 1U) &
        (kTimeTrialCount - 1));
  }
  CancelPage(state, input, host, true);
  host.DrawPage(GazettePage::kTimeTrials, state.animation_step);
  EnterAnimation(state);
}
}  // namespace

// decomp: _menuInput (src/MENU/MENU9.PRG/238.c:548), 8010337C-80104AF8.
std::optional<std::int32_t> UpdateGazette(GazetteState& state,
                                          GazetteCursorMemory& cursor,
                                          const GazetteInput& input,
                                          GazetteHost& host) {
  switch (state.phase) {
    case GazettePhase::kInitialize:
      host.SetBackgroundPriority(kBackgroundFront, kBackgroundBrightness);
      Advance(state);
      break;
    case GazettePhase::kPrepareOverview:
      return PrepareOverview(state, host);
    case GazettePhase::kOverview:
      return Overview(state, cursor, host);
    case GazettePhase::kLeaveOverview:
      return LeaveOverview(state, cursor, host);
    case GazettePhase::kPrepareTitles:
      PreparePage(state, GazettePage::kTitles, host);
      break;
    case GazettePhase::kTitles:
      UpdateRecordPage(state, cursor.titles, GazettePage::kTitles,
                       kGazetteTitleCount - kGazetteVisibleRecords, input,
                       host);
      break;
    case GazettePhase::kLeaveTitles:
      return LeaveRecordPage(state, GazettePage::kTitles, host);
    case GazettePhase::kPrepareStatistics:
      PreparePage(state, GazettePage::kStatistics, host);
      break;
    case GazettePhase::kStatistics:
      UpdateRecordPage(state, cursor.statistics, GazettePage::kStatistics,
                       kGazetteStatisticCount - kGazetteVisibleRecords, input,
                       host);
      break;
    case GazettePhase::kLeaveStatistics:
      return LeaveRecordPage(state, GazettePage::kStatistics, host);
    case GazettePhase::kPrepareTimeTrials:
      PreparePage(state, GazettePage::kTimeTrials, host);
      break;
    case GazettePhase::kTimeTrials:
      UpdateTimeTrials(state, cursor, input, host);
      break;
    case GazettePhase::kLeaveTimeTrials:
      return LeaveTimeTrials(state, host);
    case GazettePhase::kWaitForEnemies:
      return WaitForEnemies(state, host);
    case GazettePhase::kEnemies:
      return EnemyMenu(state, cursor, input, host);
    case GazettePhase::kLeaveEnemies:
      return LeaveEnemies(state, host);
    case GazettePhase::kWaitForPreview:
      return WaitForPreview(state, cursor, host);
    case GazettePhase::kViewer:
      return Viewer(state, input, host);
    case GazettePhase::kLeaveViewer:
      return LeaveViewer(state, cursor, host);
    case GazettePhase::kSwitchLeave:
    case GazettePhase::kSwitchLoad:
    case GazettePhase::kSwitchEnter:
      return SwitchPreview(state, cursor, host);
    default:
      break;
  }
  return 0;
}

// decomp: vs_menu9_exec (src/MENU/MENU9.PRG/238.c:269), 80102FF8-801031BC.
int UpdateGazetteScreen(std::uint8_t& phase, GazetteScreenState& state,
                        GazetteScreenEnvironment& environment,
                        GazetteScreenHost& host) {
  switch (phase) {
    case kScreenInitialize:
      host.SetBackgroundFade(true);
      state.initialization_delay = kSettled;
      state.progress_complete = 0;
      phase = kScreenPrepare;
      break;
    case kScreenPrepare:
      if (state.progress_complete == 0) {
        state.progress_complete = host.CalculateProgress();
      }
      if (state.initialization_delay != 0) {
        state.initialization_delay =
            math::WrappingSubtract(state.initialization_delay, 1);
        break;
      }
      if (state.progress_complete == 0) break;
      host.SetFieldRendering(false);
      host.SetHudHidden(true);
      environment.hud_slide_offset = kHiddenHudOffset;
      phase = kScreenReady;
      [[fallthrough]];
    case kScreenReady:
      phase = static_cast<std::uint8_t>(
          static_cast<std::uint32_t>(phase) +
          static_cast<std::uint32_t>(host.RowsIdle()));
      break;
    case kScreenActive:
      state.selected_result = host.UpdateMenu();
      if (state.selected_result != 0) {
        host.SetBackgroundPriority(kBackgroundBack, kBackgroundBrightness);
        host.SetHudHidden(false);
        environment.hud_slide_offset = 0;
        host.SetFieldRendering(true);
        host.ClearCommand();
        host.ClearRows(Framework::kNoRow);
        phase = kScreenLeave;
      }
      break;
    case kScreenLeave:
      host.SetBackgroundFade(false);
      phase = kScreenFade;
      break;
    case kScreenFade:
      if (environment.background_fade_step == 0) {
        if (state.selected_result == kExitScreen) {
          environment.current_menu = kReturnMenu;
        }
        environment.hide_menu = 0;
        phase = 0;
        return 1;
      }
      break;
    default:
      break;
  }
  return 0;
}
}  // namespace VS::Menu::Score
