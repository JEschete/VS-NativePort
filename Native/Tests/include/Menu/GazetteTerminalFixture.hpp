#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "Core/Math/ScalarMath.hpp"
#include "Menu/Score/GazetteTerminal.hpp"
#include "Menu/Score/GazetteTimes.hpp"

namespace VS::tests::gazette {
namespace score = Menu::Score;
namespace fw = Menu::Framework;
using Call = std::vector<std::int64_t>;
using Calls = std::vector<Call>;
constexpr std::size_t kObservedMenuBytes = 60;
constexpr std::size_t kObservedBoxBytes = 40;
constexpr std::size_t kBoxIndentX = 16;
constexpr std::size_t kBoxIndentY = 18;
constexpr std::size_t kBoxX = 20;
constexpr std::size_t kBoxY = 22;
constexpr std::size_t kHalfwordBits = 8;

inline std::vector<std::uint8_t> TextBytes(score::GazetteText text) {
  const auto end = std::ranges::find(text, std::uint8_t{0xE7});
  return {text.begin(), end == text.end() ? end : end + 1};
}

inline void AppendText(Call& call, score::GazetteText text) {
  const auto bytes = TextBytes(text);
  call.push_back(static_cast<std::int64_t>(bytes.size()));
  call.insert(call.end(), bytes.begin(), bytes.end());
}

inline void AppendAscii(Call& call, std::string_view text) {
  call.push_back(static_cast<std::int64_t>(text.size() + 1));
  for (const auto letter : text) {
    call.push_back(static_cast<unsigned char>(letter));
  }
  call.push_back(0);
}

inline Call CursorWords(const score::GazetteCursorMemory& cursor) {
  return {cursor.titles.first_row,     cursor.titles.selected_row,
          cursor.enemies.first_row,    cursor.enemies.selected_row,
          cursor.statistics.first_row, cursor.statistics.selected_row,
          cursor.time_trial,           cursor.overview_row};
}

inline void AppendCursor(Call& call, const score::GazetteCursorMemory& cursor) {
  const auto words = CursorWords(cursor);
  call.insert(call.end(), words.begin(), words.end());
}

inline Call MenuBytes(const fw::MenuRow& row, std::int64_t kind) {
  Call result{kind};
  for (const auto byte :
       std::as_bytes(std::span(&row, 1)).first(kObservedMenuBytes)) {
    result.push_back(std::to_integer<std::uint8_t>(byte));
  }
  return result;
}

struct EnemyData {
  std::array<score::GazetteEnemy, score::kOriginalEncyclopediaEnemies + 1>
      enemies{};
  std::array<std::array<std::uint8_t, 4>,
             score::kOriginalEncyclopediaEnemies + 1>
      names{};
  std::array<std::array<std::uint8_t, 3>,
             score::kOriginalEncyclopediaEnemies + 1>
      descriptions{};
  std::array<std::uint32_t, score::kEnemyDefeatFlagWords> flags{};
  std::array<std::uint8_t, 3> locked_name{0xA7, 0xA7, 0xE7};
  std::array<std::uint8_t, 3> locked_description{0x20, 0x21, 0xE7};
  EnemyData() {
    for (std::size_t index = 0; index < enemies.size(); ++index) {
      names[index] = {0xFA, 8, static_cast<std::uint8_t>(index), 0xE7};
      descriptions[index] = {static_cast<std::uint8_t>(index), 0x40, 0xE7};
      enemies[index] = {.model_id = static_cast<std::int16_t>(index),
                        .class_id = static_cast<std::int16_t>(index % 6),
                        .first_defeat_bit = static_cast<std::int16_t>(index),
                        .defeat_bit_count = 1,
                        .name = names[index],
                        .description = descriptions[index]};
    }
  }
};

class EncyclopediaHost final : public score::EnemyListHost,
                               public score::EnemyViewerHost,
                               public score::EnemyNavigationHost {
 public:
  Calls calls;
  fw::MenuRow menu;
  std::array<std::array<std::uint8_t, kObservedMenuBytes>, 8> menu_replies{};
  std::size_t created = 0;
  std::int32_t cursor_reply = 257;
  score::ProjectionOffset projection{97, 103};
  Core::Math::Transform3 camera{};
  std::vector<Render::UiTexturedQuadPacket> storage;
  std::vector<Render::UiTexturedQuadPacket> packets;
  std::size_t quad_capacity = 3;
  std::uint32_t chain = 0;
  std::uint32_t primitive_address = 0;
  int refusals = 0;

  void SetInformation(score::GazetteText text) override {
    Call call{3};
    AppendText(call, text);
    calls.push_back(std::move(call));
  }
  fw::MenuRow& CreateRow(const fw::RowPlacement& placement,
                         score::GazetteText text) override {
    Call call{2,           placement.row, placement.x,
              placement.y, placement.w,   placement.background_width};
    AppendText(call, text);
    calls.push_back(std::move(call));
    menu = {};
    if (created < menu_replies.size()) {
      std::memcpy(&menu, menu_replies[created].data(), kObservedMenuBytes);
    }
    ++created;
    return menu;
  }
  fw::MenuRow& Row() override {
    calls.push_back({1, 0});
    return menu;
  }
  std::int32_t DrawCursor(std::uint8_t step, int x, int y) override {
    calls.push_back({4, step, fw::PackXy(x, y)});
    return cursor_reply;
  }
  void DrawNumber(const score::EnemyNumber& number) override {
    calls.push_back(
        {5, number.x, number.y, number.value, number.dimmed ? 1 : 0});
  }
  void DrawRow(bool scrolling) override {
    calls.push_back(MenuBytes(menu, scrolling ? 7 : 6));
  }
  void DrawRow() override { DrawRow(false); }
  void Refuse(std::string_view) override { ++refusals; }
  bool DrawQuad(const Render::UiTexturedQuad& quad) override {
    if (packets.size() >= quad_capacity) return false;
    Render::UiTexturedQuadPacket previous{};
    if (packets.size() < storage.size()) previous = storage[packets.size()];
    packets.push_back(Render::BuildUiTexturedQuadPacket(quad, previous, chain));
    chain = (chain & 0xFF000000U) |
            ((primitive_address + static_cast<std::uint32_t>(
                                      (packets.size() - 1) *
                                      sizeof(Render::UiTexturedQuadPacket))) &
             0xFFFFFFU);
    return true;
  }
  void DrawEdgeFade() override { calls.push_back({12}); }
  score::ProjectionOffset GetProjection() override {
    calls.push_back({8});
    return projection;
  }
  void SetProjection(const score::ProjectionOffset& offset) override {
    calls.push_back({9, offset.x, offset.y});
    projection = offset;
  }
  Core::Math::Transform3 BuildCamera() override {
    calls.push_back({10});
    return camera;
  }
  void DrawPreview(const Core::Math::Transform3& value) override {
    Call call{11};
    for (const auto& row : value.rotation.m) {
      for (const auto component : row) call.push_back(component);
    }
    call.insert(call.end(), {value.translation.x, value.translation.y,
                             value.translation.z});
    calls.push_back(std::move(call));
  }
  void DrawButton(score::ViewerButton button, int x, int y) override {
    calls.push_back(
        {13, button == score::ViewerButton::kRotation ? 6 : 7, x, y, 6});
  }
  void DrawText(std::string_view text, std::uint32_t position,
                std::uint32_t color, score::ViewerTextLayer layer) override {
    Call call{14, position, color,
              layer == score::ViewerTextLayer::kOverlay
                  ? 0
                  : (layer == score::ViewerTextLayer::kControls ? 6 : 7)};
    AppendAscii(call, text);
    calls.push_back(std::move(call));
  }
  void DrawText(std::string_view text, std::uint32_t position,
                std::uint32_t color) override {
    DrawText(text, position, color, score::ViewerTextLayer::kOverlay);
  }
  void DrawBanner(const score::PanelRect& rect, int gradient) override {
    calls.push_back({15, rect.x, rect.y, rect.width, rect.height, gradient});
  }
  void DrawPanel(const score::PanelRect& rect, int gradient) override {
    calls.push_back({16, rect.x, rect.y, rect.width, rect.height, gradient});
  }
};

class TerminalHost final : public score::GazetteHost {
 public:
  score::GazetteState state;
  score::GazetteCursorMemory cursor;
  score::GazetteInput input;
  EnemyData data;
  Calls calls;
  std::array<std::uint8_t, kObservedBoxBytes> box{};
  std::array<std::uint8_t, score::kTimeTrialCount> delays{};
  std::vector<std::uint8_t> rank{0x10, 0x20, 0xE7};
  std::optional<score::GazettePreviewCamera> preset =
      score::GazettePreviewCamera{320, 107};
  std::optional<int> preview_result = 0;
  score::GazetteReadStatus read_status = score::GazetteReadStatus::kPending;
  int top_result = -1;
  std::uint8_t overview_cursor = 0;
  std::int32_t file_sector = 123;
  std::int32_t file_bytes = 16384;
  bool box_present = true;
  bool enemies_present = true;
  bool read_accepted = true;
  bool finish_accepted = true;
  bool free_accepted = true;
  bool data_started = false;
  bool description_bound = false;
  std::size_t delay_count = score::kTimeTrialCount;
  int refusals = 0;

  std::optional<std::int32_t> Tick() {
    return score::UpdateGazette(state, cursor, input, *this);
  }
  void SetBackgroundPriority(int offset, int brightness) override {
    calls.push_back({20, offset, brightness});
  }
  void LoadAshleyPreview() override { calls.push_back({21, 0}); }
  void InitializeAshleyPose() override { calls.push_back({22}); }
  std::optional<score::GazettePreviewCamera> PreviewCamera() override {
    return preset;
  }
  score::GazetteText RankText() override { return rank; }
  void InitializeRankBox(const score::GazetteRankBox& placement) override {
    calls.push_back({23, 0, 0, placement.x, placement.y, placement.columns,
                     placement.lines, placement.center_x, placement.center_y});
  }
  void SetRankText(score::GazetteText text) override {
    Call call{24, 0};
    AppendText(call, text);
    calls.push_back(std::move(call));
  }
  bool MoveRankBox(int dx, int dy, bool required) override {
    calls.push_back({25, 0});
    if (!box_present) return !required;
    MoveBoxHalfword(kBoxX, dx);
    MoveBoxHalfword(kBoxIndentX, dx);
    MoveBoxHalfword(kBoxY, dy);
    MoveBoxHalfword(kBoxIndentY, dy);
    return true;
  }
  void DismissRankBox() override { calls.push_back({26, 0}); }
  std::int32_t UpdateTopMenu(bool initialize) override {
    calls.push_back({27, initialize ? 1 : 0});
    return top_result;
  }
  std::uint8_t OverviewCursor() override { return overview_cursor; }
  void DrawOverview(std::int32_t step) override {
    calls.push_back({28, step, state.camera.pitch, state.camera.yaw,
                     state.camera.roll, state.camera.distance,
                     state.camera.height_offset});
  }
  void ReleaseAshleyPreview() override { calls.push_back({29}); }
  void RestoreCamera() override { calls.push_back({30}); }
  void InitializeInformation() override { calls.push_back({31}); }
  void DismissInformation() override { calls.push_back({32}); }
  void BuildPage(score::GazettePage page) override {
    calls.push_back({33, static_cast<int>(page)});
  }
  void DrawPage(score::GazettePage page, std::int32_t step) override {
    Call call{34, static_cast<int>(page), step, static_cast<int>(state.phase),
              state.next_result};
    AppendCursor(call, cursor);
    calls.push_back(std::move(call));
  }
  void LeaveTimeTrials() override { calls.push_back({35}); }
  std::span<const std::uint8_t> TimeTrialDelays() override {
    return std::span(delays).first(std::min(delay_count, delays.size()));
  }
  void DeactivateRow(int row) override {
    calls.push_back({36, row, state.animation_step});
  }
  void PlaySound(int cue) override {
    Call call{37, 126, cue};
    AppendCursor(call, cursor);
    calls.push_back(std::move(call));
  }
  bool BeginEnemyRead() override {
    calls.push_back({38, file_bytes});
    calls.push_back({39, file_sector, file_bytes});
    calls.push_back({40});
    data_started = read_accepted;
    return read_accepted;
  }
  score::GazetteReadStatus EnemyReadStatus() override { return read_status; }
  bool FinishEnemyRead() override {
    calls.push_back({41});
    description_bound = finish_accepted;
    return finish_accepted;
  }
  bool FreeEnemyList() override {
    calls.push_back({42});
    return free_accepted;
  }
  std::span<const score::GazetteEnemy> Enemies() override {
    return enemies_present ? std::span<const score::GazetteEnemy>(data.enemies)
                           : std::span<const score::GazetteEnemy>{};
  }
  std::optional<int> LoadEnemyPreview(std::int16_t model) override {
    calls.push_back({43, model});
    return preview_result;
  }
  void FreeEnemyPreview() override { calls.push_back({44}); }
  void ShowEnemyDescription(std::int32_t selected) override {
    calls.push_back({45, selected});
  }
  bool ShowListDescription(std::int32_t selected) override {
    if (selected < 0 ||
        static_cast<std::size_t>(selected) >= data.enemies.size()) {
      return false;
    }
    Call call{49};
    AppendText(call,
               data.enemies[static_cast<std::size_t>(selected)].description);
    calls.push_back(std::move(call));
    return true;
  }
  void SetMenuVisible(bool visible) override {
    calls.push_back({46, visible ? 1 : 0});
  }
  void DrawNavigation(int direction) override {
    calls.push_back({47, direction, state.navigation_step, state.hide_ui});
  }
  void DrawViewer(std::int32_t step, std::int32_t transition) override {
    calls.push_back({48, step, transition, state.selected_enemy,
                     state.camera.pitch, state.camera.yaw, state.camera.roll,
                     state.camera.distance, state.camera.height_offset,
                     state.hide_ui, state.rotation_color, state.zoom_color});
  }
  void Refuse(std::string_view) override { ++refusals; }

 private:
  void MoveBoxHalfword(std::size_t at, int delta) {
    const auto value = static_cast<std::uint16_t>(
        static_cast<unsigned>(box[at]) |
        (static_cast<unsigned>(box[at + 1]) << kHalfwordBits));
    const auto result =
        static_cast<std::uint16_t>(static_cast<int>(value) + delta);
    box[at] = static_cast<std::uint8_t>(result);
    box[at + 1] = static_cast<std::uint8_t>(result >> kHalfwordBits);
  }
};

class ScreenHost final : public score::GazetteScreenHost {
 public:
  score::GazetteScreenState state;
  score::GazetteScreenEnvironment environment;
  std::uint8_t phase = 0;
  std::int32_t progress_reply = 1;
  std::int32_t idle_reply = 1;
  std::int32_t menu_reply = 0;
  Calls calls;

  int Tick() {
    return score::UpdateGazetteScreen(phase, state, environment, *this);
  }
  void SetBackgroundFade(bool enabled) override {
    calls.push_back({60, enabled ? 1 : 0});
  }
  std::int32_t CalculateProgress() override {
    calls.push_back({61});
    return progress_reply;
  }
  void SetFieldRendering(bool enabled) override {
    calls.push_back({62, enabled ? 1 : 0, environment.hud_slide_offset});
  }
  void SetHudHidden(bool hidden) override {
    calls.push_back({63, hidden ? 1 : 0, environment.hud_slide_offset});
  }
  std::int32_t RowsIdle() override {
    calls.push_back({64, phase, environment.hud_slide_offset});
    return idle_reply;
  }
  std::int32_t UpdateMenu() override {
    calls.push_back({65});
    return menu_reply;
  }
  void SetBackgroundPriority(int offset, int brightness) override {
    calls.push_back({20, offset, brightness});
  }
  void ClearCommand() override { calls.push_back({66, 0}); }
  void ClearRows(int except_row) override { calls.push_back({67, except_row}); }
};
}  // namespace VS::tests::gazette
