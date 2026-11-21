#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "Menu/Score/GazetteEncyclopedia.hpp"

namespace VS::Menu::Score {  // NOLINT(readability-identifier-naming)
enum class GazettePhase : std::int32_t {
  kInitialize,
  kPrepareOverview,
  kOverview,
  kLeaveOverview,
  kPrepareTitles,
  kTitles,
  kLeaveTitles,
  kPrepareStatistics,
  kStatistics,
  kLeaveStatistics,
  kPrepareTimeTrials,
  kTimeTrials,
  kLeaveTimeTrials,
  kWaitForEnemies,
  kEnemies,
  kLeaveEnemies,
  kWaitForPreview = 19,
  kViewer,
  kLeaveViewer,
  kSwitchLeave = 25,
  kSwitchLoad,
  kSwitchEnter,
};

enum class GazettePage : std::uint8_t {
  kTitles,
  kStatistics,
  kTimeTrials,
  kEnemies,
};

enum class GazetteReadStatus : std::uint8_t { kPending, kLoaded, kRefused };

struct GazetteCursorMemory {
  GazetteRecordCursor titles;
  GazetteRecordCursor enemies;
  GazetteRecordCursor statistics;
  std::uint8_t time_trial = 0;
  std::uint8_t overview_row = 0;
};

struct GazetteState {
  GazettePhase phase = GazettePhase::kInitialize;
  // The original reuses this for overview return values (1/2), page exit
  // (0/1) and enemy-list-to-viewer (2).
  std::int32_t next_result = 0;
  std::int32_t animation_step = 0;
  ViewerCameraSettings camera;
  std::uint16_t initial_camera_distance = 0;
  std::int16_t selected_enemy = 0;
  std::int16_t enemy_skip_delay = 0;
  std::int32_t navigation_step = 0;
  // A held L1/R1 mask becomes slide direction 2/1 when released.
  std::int32_t enemy_button_or_slide = 0;
  std::int32_t hide_ui = 0;
  std::uint32_t rotation_color = 0;
  std::uint32_t zoom_color = 0;
};

inline constexpr std::int32_t kGazetteStickCenter = 128;
// GazetteHost::LoadEnemyPreview's results: 1 while the preview loads, 0 once
// it is posed and (#48) kGazettePreviewFailed once a failed load has released
// what it held. A failed preview returns to the enemy list.
inline constexpr int kGazettePreviewFailed = -1;

struct GazetteInput {
  std::uint32_t pressed = 0;
  std::uint32_t repeated = 0;
  std::uint32_t held = 0;
  std::uint32_t released = 0;
  std::int32_t left_x = kGazetteStickCenter;
  std::int32_t left_y = kGazetteStickCenter;
  std::int32_t right_x = kGazetteStickCenter;
  std::int32_t right_y = kGazetteStickCenter;
};

struct GazettePreviewCamera {
  std::uint16_t distance = 0;
  std::int16_t height_offset = 0;
};

struct GazetteRankBox {
  int x = 0;
  int y = 0;
  int columns = 0;
  int lines = 0;
  int center_x = 0;
  int center_y = 0;
};

class GazetteHost {
 public:
  virtual ~GazetteHost() = default;
  virtual void SetBackgroundPriority(int offset, int brightness) = 0;
  virtual void LoadAshleyPreview() = 0;
  virtual void InitializeAshleyPose() = 0;
  virtual std::optional<GazettePreviewCamera> PreviewCamera() = 0;
  virtual GazetteText RankText() = 0;
  virtual void InitializeRankBox(const GazetteRankBox& placement) = 0;
  virtual void SetRankText(GazetteText text) = 0;
  virtual bool MoveRankBox(int dx, int dy, bool required) = 0;
  virtual void DismissRankBox() = 0;
  virtual std::int32_t UpdateTopMenu(bool initialize) = 0;
  virtual std::uint8_t OverviewCursor() = 0;
  virtual void DrawOverview(std::int32_t step) = 0;
  virtual void ReleaseAshleyPreview() = 0;
  virtual void RestoreCamera() = 0;
  virtual void InitializeInformation() = 0;
  virtual void DismissInformation() = 0;
  virtual void BuildPage(GazettePage page) = 0;
  virtual void DrawPage(GazettePage page, std::int32_t step) = 0;
  virtual void LeaveTimeTrials() = 0;
  virtual std::span<const std::uint8_t> TimeTrialDelays() = 0;
  virtual void DeactivateRow(int row) = 0;
  virtual void PlaySound(int cue) = 0;
  virtual bool BeginEnemyRead() = 0;
  virtual GazetteReadStatus EnemyReadStatus() = 0;
  // Releases the completed request and binds its bounded description table.
  virtual bool FinishEnemyRead() = 0;
  virtual bool FreeEnemyList() = 0;
  virtual std::span<const GazetteEnemy> Enemies() = 0;
  virtual std::optional<int> LoadEnemyPreview(std::int16_t model) = 0;
  virtual void FreeEnemyPreview() = 0;
  virtual void ShowEnemyDescription(std::int32_t selected) = 0;
  // The original return-to-list path uses the entry's description directly.
  virtual bool ShowListDescription(std::int32_t selected) = 0;
  virtual void SetMenuVisible(bool visible) = 0;
  virtual void DrawNavigation(int direction) = 0;
  virtual void DrawViewer(std::int32_t step, std::int32_t transition) = 0;
  virtual void Refuse(std::string_view reason) = 0;
};

// F02803. A null result is a checked native refusal, not a menu exit.
// The host sees state/cursor writes before each endpoint. Rendering follows
// input, including a cancel frame; releasing a viewer bumper can replace
// that frame's exit phase, exactly as in the original.
[[nodiscard]] std::optional<std::int32_t> UpdateGazette(
    GazetteState& state, GazetteCursorMemory& cursor, const GazetteInput& input,
    GazetteHost& host);

struct GazetteScreenState {
  std::int32_t initialization_delay = 0;
  std::int32_t progress_complete = 0;
  std::int32_t selected_result = 0;
};

struct GazetteScreenEnvironment {
  std::int32_t hud_slide_offset = 0;
  std::int32_t background_fade_step = 0;
  std::uint8_t current_menu = 0;
  std::uint8_t hide_menu = 0;
};

class GazetteScreenHost {
 public:
  virtual ~GazetteScreenHost() = default;
  virtual void SetBackgroundFade(bool enabled) = 0;
  virtual std::int32_t CalculateProgress() = 0;
  virtual void SetFieldRendering(bool enabled) = 0;
  virtual void SetHudHidden(bool hidden) = 0;
  virtual std::int32_t RowsIdle() = 0;
  virtual std::int32_t UpdateMenu() = 0;
  virtual void SetBackgroundPriority(int offset, int brightness) = 0;
  virtual void ClearCommand() = 0;
  virtual void ClearRows(int except_row) = 0;
};

// F04156. The external byte phase is distinct from MENU9's three local
// words. Phase 4 falls through to the readiness poll only after its delay
// and progress gates; phase 8 returns one only when the fade has finished.
[[nodiscard]] int UpdateGazetteScreen(std::uint8_t& phase,
                                      GazetteScreenState& state,
                                      GazetteScreenEnvironment& environment,
                                      GazetteScreenHost& host);
}  // namespace VS::Menu::Score
