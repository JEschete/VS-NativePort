#include "Menu/Score/GazetteTerminalLive.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "Actors/ActorRecord.h"
#include "Actors/Movement.h"
#include "Audio/SfxLive.hpp"
#include "Battle/Hud.h"
#include "Battle/HudStateLive.hpp"
#include "Battle/StatusEffects.h"
#include "Content/CdDriveLive.hpp"
#include "Content/StringTables.hpp"
#include "Core/Log.hpp"
#include "Core/Math/MatrixLive.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Core/String/GameText.hpp"
#include "DiscQueue.h"
#include "Field/RoomRenderingLive.hpp"
#include "Input/ControllerLiveState.hpp"
#include "Menu/Framework/ActorPreviewLive.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Framework/FrameworkLiveState.hpp"
#include "Menu/Score/Encyclopedia.hpp"
#include "Menu/Score/EncyclopediaPreview.hpp"
#include "Menu/Score/EncyclopediaPreviewLive.hpp"
#include "Menu/Score/GazetteCameraLive.hpp"
#include "Menu/Score/GazetteRowsLive.hpp"
#include "Menu/Score/GazetteStatisticsLive.hpp"
#include "Menu/Score/GazetteTerminal.hpp"
#include "Menu/Score/GazetteTimes.hpp"
#include "Menu/Score/GazetteTimesLive.hpp"
#include "Menu/Score/Records.hpp"
#include "Menu/Score/ScoreRankLive.hpp"
#include "Menu/Score/ScoreScreen.hpp"
#include "Menu/TextBoxStateLive.hpp"
#include "Render/Display.hpp"
#include "Render/GameFontLive.hpp"
#include "Render/Render.hpp"
#include "Render/RoomDrawLive.hpp"
#include "Render/SpritesLive.hpp"
#include "Render/UiPacketsLive.hpp"
#include "State/Progress.hpp"

namespace menu9_misc_info = VS::Content::text::menu9_prg::misc_info;

namespace VS::Menu::Score {
namespace {
namespace memory = Core::Memory;
constexpr std::size_t kEnemyStride = 44;
constexpr std::size_t kDescriptionOffset = 150 * kEnemyStride;
constexpr std::size_t kModelOffset = 0;
constexpr std::size_t kClassOffset = 2;
constexpr std::size_t kFirstFlagOffset = 4;
constexpr std::size_t kFlagCountOffset = 6;
constexpr std::size_t kSelectedOffset = 8;
constexpr std::size_t kHighlightOffset = 10;
constexpr std::size_t kUnlockedOffset = 12;
constexpr std::size_t kPreviousOffset = 13;
constexpr std::size_t kNextOffset = 14;
constexpr std::size_t kNameOffset = 16;
constexpr std::size_t kHalfwordBytes = 2;
constexpr unsigned kByteBits = 8;
constexpr int kControlsSlot = 6;
constexpr int kIdentificationSlot = 7;
constexpr std::size_t kPreviewActor = 1;
constexpr int kSoundGroup = 126;
constexpr std::size_t kOverviewMemoryRow = 2;
constexpr std::size_t kOverviewMemoryPage = 3;

struct EnemyDataOwner {
  std::byte* data = nullptr;
  std::size_t bytes = 0;
  Content_DiscQueueSlot* request = nullptr;
  std::uint64_t generation = 0;
};

EnemyDataOwner enemy_data;
GazetteRecordAnimation enemy_animation;
GazetteScreenState screen_state;
std::int32_t menu_animation = 0;
std::uint64_t refusals = 0;

void Refuse(std::string_view reason) {
  if (std::has_single_bit(++refusals)) {
    VS::Core::LogWarn("menu.score", "gazette terminal: {} ({} refusals)",
                      reason, refusals);
  }
}

std::uint16_t Halfword(std::span<const std::byte> bytes, std::size_t offset) {
  return static_cast<std::uint16_t>(
      std::to_integer<unsigned>(bytes[offset]) |
      (std::to_integer<unsigned>(bytes[offset + 1]) << kByteBits));
}

std::int16_t SignedHalfword(std::span<const std::byte> bytes,
                            std::size_t offset) {
  return std::bit_cast<std::int16_t>(Halfword(bytes, offset));
}

void StoreHalfword(std::span<std::byte> bytes, std::size_t offset,
                   std::int16_t value) {
  const auto bits = static_cast<std::uint16_t>(value);
  bytes[offset] = static_cast<std::byte>(bits);
  bytes[offset + 1] = static_cast<std::byte>(bits >> kByteBits);
}

GazetteText Text(std::span<const std::byte> bytes) {
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
  return {reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size()};
}

GazetteText Label(std::span<const std::byte> bytes, std::size_t index) {
  if (index >= bytes.size() / kHalfwordBytes) return {};
  const auto offset =
      static_cast<std::size_t>(Halfword(bytes, index * kHalfwordBytes)) *
      kHalfwordBytes;
  return offset < bytes.size() ? Text(bytes.subspan(offset)) : GazetteText{};
}

template <std::size_t Size>
GazetteText Label(const u_short (&table)[Size], std::size_t index) {
  return Label(std::as_bytes(std::span(table)), index);
}

std::span<std::byte> EnemyBytes() {
  const auto capacity = memory::LivePayloadCapacity(monBinData);
  const auto file_size = kMonBinFile.size;
  if (!capacity.has_value() || file_size <= 0 ||
      *capacity < static_cast<std::size_t>(file_size) ||
      static_cast<std::size_t>(file_size) <
          kEncyclopediaEnemies * kEnemyStride) {
    return {};
  }
  // Imported file bytes, never the native pointer-bearing row layout.
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
  return {reinterpret_cast<std::byte*>(monBinData),
          static_cast<std::size_t>(file_size)};
}

std::span<const std::byte> Descriptions(std::span<const std::byte> bytes) {
  if (bytes.size() <= kDescriptionOffset) return {};
  const auto table = bytes.subspan(kDescriptionOffset);
  if (static_cast<const void*>(enemyDescriptions) !=
      static_cast<const void*>(table.data())) {
    return {};
  }
  return table;
}

using EnemyRows = std::array<GazetteEnemy, kEncyclopediaEnemies>;

bool ReadEnemies(EnemyRows& rows) {
  const auto bytes = EnemyBytes();
  if (bytes.empty()) return false;
  const auto descriptions = Descriptions(bytes);
  for (std::size_t index = 0; index < rows.size(); ++index) {
    const auto entry = bytes.subspan(index * kEnemyStride, kEnemyStride);
    rows[index] = {
        .model_id = SignedHalfword(entry, kModelOffset),
        .class_id = SignedHalfword(entry, kClassOffset),
        .first_defeat_bit = SignedHalfword(entry, kFirstFlagOffset),
        .defeat_bit_count = SignedHalfword(entry, kFlagCountOffset),
        .selected = SignedHalfword(entry, kSelectedOffset),
        .highlight_step = SignedHalfword(entry, kHighlightOffset),
        .unlocked = std::to_integer<std::uint8_t>(entry[kUnlockedOffset]),
        .previous = std::to_integer<std::uint8_t>(entry[kPreviousOffset]),
        .next = std::to_integer<std::uint8_t>(entry[kNextOffset]),
        .name = Text(entry.subspan(kNameOffset, kEncyclopediaNameBytes)),
        .description = Label(descriptions, index)};
  }
  return true;
}

void StoreEnemies(const EnemyRows& rows, bool links) {
  const auto bytes = EnemyBytes();
  if (bytes.empty()) {
    Refuse("enemy storage changed during an endpoint");
    return;
  }
  for (std::size_t index = 0; index < rows.size(); ++index) {
    const auto entry = bytes.subspan(index * kEnemyStride, kEnemyStride);
    const auto& enemy = rows[index];
    if (links) {
      entry[kUnlockedOffset] = static_cast<std::byte>(enemy.unlocked);
      entry[kPreviousOffset] = static_cast<std::byte>(enemy.previous);
      entry[kNextOffset] = static_cast<std::byte>(enemy.next);
    } else {
      StoreHalfword(entry, kSelectedOffset, enemy.selected);
      StoreHalfword(entry, kHighlightOffset, enemy.highlight_step);
    }
  }
}

bool EmitQuad(const Render::UiTexturedQuad& quad,
              Render::LiveOrderingBase base) {
  auto* ordering = Render::LiveFrameOrderingEntry(base, 0);
  if (ordering == nullptr) return false;
  auto* storage =
      Render::ReserveLivePrimitiveWords(Render::kUiTexturedQuadWords);
  if (storage == nullptr) return false;
  Render::UiTexturedQuadPacket previous{};
  std::ranges::copy(std::span(storage, Render::kUiTexturedQuadWords),
                    previous.begin());
  Render::LivePacketQueue queue(ordering);
  return queue.Emit(
      Render::BuildUiTexturedQuadPacket(quad, previous, queue.ChainLink()));
}

void DrawText(std::string_view text, std::uint32_t position,
              std::uint32_t color, ViewerTextLayer layer) {
  auto* ordering = Render::LiveFrameOrderingEntry(
      layer == ViewerTextLayer::kOverlay ? Render::LiveOrderingBase::kOverlay
                                         : Render::LiveOrderingBase::kScene,
      layer == ViewerTextLayer::kOverlay
          ? 0
          : (layer == ViewerTextLayer::kControls ? kControlsSlot
                                                 : kIdentificationSlot));
  if (ordering == nullptr) {
    Refuse("text has no frame ordering entry");
    return;
  }
  const std::string terminated(text);
  Render::DrawLiveCompactTextColor(
      terminated.c_str(), std::bit_cast<std::int32_t>(position),
      std::bit_cast<std::int32_t>(color), ordering);
}

class LiveEnemyListHost final : public EnemyListHost {
 public:
  void SetInformation(GazetteText text) override {
    Framework::SetLiveInformationMessageText(text);
  }
  Framework::MenuRow& CreateRow(const Framework::RowPlacement& placement,
                                GazetteText text) override {
    return Framework::SetLiveMenuItemText(placement, text);
  }
  Framework::MenuRow& Row() override { return Framework::LiveMenuRow(0); }
  std::int32_t DrawCursor(std::uint8_t step, int x, int y) override {
    return Framework::DrawLiveCursor(
        step, std::bit_cast<std::int32_t>(Framework::PackXy(x, y)));
  }
  void DrawNumber(const EnemyNumber& number) override {
    DrawLiveEnemyNumber(number.x, number.y, number.value,
                        number.dimmed ? 1 : 0);
  }
  void DrawRow(bool scrolling) override {
    auto* row = Framework::LiveMenuItem(0);
    if (scrolling) {
      RenderLiveScrollableRow(row);
    } else {
      Framework::DrawLiveMenuItem(row);
    }
  }
  void Refuse(std::string_view reason) override { Score::Refuse(reason); }
};

class LiveEnemyNumberTarget final : public EnemyQuadTarget {
 public:
  bool DrawQuad(const Render::UiTexturedQuad& quad) override {
    return EmitQuad(quad, Render::LiveOrderingBase::kOverlay);
  }
};

class LiveEnemyNavigationHost final : public EnemyNavigationHost {
 public:
  bool DrawQuad(const Render::UiTexturedQuad& quad) override {
    return EmitQuad(quad, Render::LiveOrderingBase::kScene);
  }
  void DrawText(std::string_view text, std::uint32_t position,
                std::uint32_t color) override {
    Score::DrawText(text, position, color, ViewerTextLayer::kOverlay);
  }
};

class LiveEnemyViewerHost final : public EnemyViewerHost {
 public:
  void DrawEdgeFade() override { DrawLiveViewerEdgeFade(); }
  ProjectionOffset GetProjection() override {
    const auto offset = Render::GetLiveProjectionScreenOffset();
    return {offset.x, offset.y};
  }
  void SetProjection(const ProjectionOffset& offset) override {
    Render::SetLiveProjectionScreenOffset({offset.x, offset.y});
  }
  Core::Math::Transform3 BuildCamera() override {
    MATRIX matrix{};
    BuildLiveViewerCamera(&matrix);
    return Core::Math::TransformOf(matrix);
  }
  void DrawPreview(const Core::Math::Transform3& camera) override {
    MATRIX matrix{};
    Core::Math::StoreTransform(matrix, camera);
    Framework::DrawLiveActorPreview(&matrix);
  }
  Framework::MenuRow& CreateRow(const Framework::RowPlacement& placement,
                                GazetteText text) override {
    return Framework::SetLiveMenuItemText(placement, text);
  }
  void DrawRow() override {
    Framework::DrawLiveMenuItem(Framework::LiveMenuItem(0));
  }
  Framework::MenuRow& Row() override { return Framework::LiveMenuRow(0); }
  void DrawButton(ViewerButton button, int x, int y) override {
    auto* ordering = Render::LiveFrameOrderingEntry(
        Render::LiveOrderingBase::kScene, kControlsSlot);
    if (ordering == nullptr) {
      Refuse("viewer button has no frame ordering entry");
      return;
    }
    Framework::DrawLiveButtonIcon(button == ViewerButton::kRotation
                                      ? Framework::ButtonIcon::kL2
                                      : Framework::ButtonIcon::kR2,
                                  x, y, ordering);
  }
  void DrawText(std::string_view text, std::uint32_t position,
                std::uint32_t color, ViewerTextLayer layer) override {
    Score::DrawText(text, position, color, layer);
  }
  void DrawBanner(const PanelRect& rect, int gradient) override {
    DrawLiveEnemyBanner(rect.x, rect.y, rect.width, rect.height, gradient);
  }
  void DrawPanel(const PanelRect& rect, int gradient) override {
    DrawLiveGradientPanel(rect.x, rect.y, rect.width, rect.height, gradient);
  }
  void Refuse(std::string_view reason) override { Score::Refuse(reason); }
};

GazetteState ReadState() {
  return {.phase = static_cast<GazettePhase>(menuState),
          .next_result = nextState,
          .animation_step = menu_animation,
          .camera = {cameraAngles.vx, cameraAngles.vy, cameraAngles.vz,
                     cameraDistance, cameraHeightOffset},
          .initial_camera_distance = initCameraDistance,
          .selected_enemy = selectedEnemy,
          .enemy_skip_delay = enemySkipDelay,
          .navigation_step = enemyNavigationState,
          .enemy_button_or_slide = buttonHeld,
          .hide_ui = hideUi,
          .rotation_color = static_cast<std::uint32_t>(rotationColor),
          .zoom_color = static_cast<std::uint32_t>(zoomColor)};
}

void WriteState(const GazetteState& state) {
  menuState = static_cast<int>(state.phase);
  nextState = state.next_result;
  menu_animation = state.animation_step;
  cameraAngles.vx = state.camera.pitch;
  cameraAngles.vy = state.camera.yaw;
  cameraAngles.vz = state.camera.roll;
  cameraDistance = state.camera.distance;
  cameraHeightOffset = state.camera.height_offset;
  initCameraDistance = state.initial_camera_distance;
  selectedEnemy = state.selected_enemy;
  enemySkipDelay = state.enemy_skip_delay;
  enemyNavigationState = state.navigation_step;
  buttonHeld = state.enemy_button_or_slide;
  hideUi = state.hide_ui;
  rotationColor = std::bit_cast<std::int32_t>(state.rotation_color);
  zoomColor = std::bit_cast<std::int32_t>(state.zoom_color);
}

GazetteCursorMemory ReadCursor() {
  const auto& cursor = Battle_unusedMenu9CursorMemory;
  return {{cursor.title_page, cursor.title_row},
          {cursor.encyclopaedia_page, cursor.encyclopaedia_row},
          {cursor.record_time_page, cursor.record_time_row},
          cursor.gazette_page,
          cursor.gazette_row};
}

void WriteCursor(const GazetteCursorMemory& cursor) {
  auto& memory = Battle_unusedMenu9CursorMemory;
  memory.title_page = cursor.titles.first_row;
  memory.title_row = cursor.titles.selected_row;
  memory.encyclopaedia_page = cursor.enemies.first_row;
  memory.encyclopaedia_row = cursor.enemies.selected_row;
  memory.record_time_page = cursor.statistics.first_row;
  memory.record_time_row = cursor.statistics.selected_row;
  memory.gazette_page = cursor.time_trial;
  memory.gazette_row = cursor.overview_row;
}

bool BeginRead() {
  if (enemy_data.data != nullptr || enemy_data.request != nullptr ||
      monBinData != nullptr || monBinCdQueueSlot != nullptr) {
    Refuse("a prior enemy-list owner is still active");
    return false;
  }
  if (kMonBinFile.size <= 0) return false;
  const auto size = static_cast<std::size_t>(kMonBinFile.size);
  auto* data = static_cast<std::byte*>(memory::AllocateLiveFromHighEnd(size));
  if (data == nullptr) return false;
  enemy_data.data = data;
  enemy_data.bytes = size;
  // This exported pointer remains a compatibility borrow, not the owner.
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
  monBinData = reinterpret_cast<EnemyEntry*>(data);
  auto* request = Content::AllocateLiveDiscRequest(&kMonBinFile);
  if (request != nullptr) {
    monBinCdQueueSlot = request;
    Content::EnqueueLiveDiscRequest(request, data);
  } else {
    request = Content::CompleteLiveRefusedDiscRequest(&kMonBinFile, data);
    monBinCdQueueSlot = request;
  }
  enemy_data.request = request;
  enemy_data.generation = Content::LiveDiscRequestGeneration(request);
  if (request == nullptr || enemy_data.generation == 0) {
    if (!ReleaseLiveGazetteEnemyData()) {
      Refuse("failed enemy read retained storage awaiting cancellation");
    }
    return false;
  }
  return true;
}

GazetteReadStatus ReadStatus() {
  if (enemy_data.request == nullptr || enemy_data.generation == 0 ||
      Content::LiveDiscRequestGeneration(enemy_data.request) !=
          enemy_data.generation ||
      enemy_data.request != monBinCdQueueSlot ||
      static_cast<void*>(enemy_data.data) != static_cast<void*>(monBinData) ||
      EnemyBytes().empty()) {
    return GazetteReadStatus::kRefused;
  }
  if (enemy_data.request->state == Content_DiscRequestState_Error) {
    return GazetteReadStatus::kRefused;
  }
  return enemy_data.request->state == Content_DiscRequestState_Loaded
             ? GazetteReadStatus::kLoaded
             : GazetteReadStatus::kPending;
}

bool FinishRead() {
  if (ReadStatus() != GazetteReadStatus::kLoaded ||
      enemy_data.bytes <
          kDescriptionOffset + kEncyclopediaEnemies * kHalfwordBytes) {
    return false;
  }
  Content::ReleaseLiveDiscRequest(enemy_data.request);
  enemy_data.request = nullptr;
  enemy_data.generation = 0;
  monBinCdQueueSlot = nullptr;
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
  enemyDescriptions =
      reinterpret_cast<u_short*>(enemy_data.data + kDescriptionOffset);
  return true;
}

class LiveGazetteHost final : public GazetteHost {
 public:
  LiveGazetteHost(GazetteState& state, GazetteCursorMemory& cursor)
      : state_(state), cursor_(cursor) {}

  void Publish() {
    WriteState(state_);
    WriteCursor(cursor_);
  }
  void SetBackgroundPriority(int offset, int brightness) override {
    Publish();
    Framework::SetLiveBackgroundPriority(offset, brightness);
  }
  void LoadAshleyPreview() override {
    Publish();
    Framework::LoadLiveActorPreview(0);
  }
  void InitializeAshleyPose() override {
    Publish();
    InitializeLiveAshleyPreviewPose();
  }
  std::optional<GazettePreviewCamera> PreviewCamera() override {
    const auto* actor = Actors_actorRecords[kPreviewActor];
    if (actor == nullptr) return std::nullopt;
    return GazettePreviewCamera{
        actor->menuCameraDistance,
        std::bit_cast<std::int16_t>(actor->collisionHeight)};
  }
  GazetteText RankText() override {
    if (characterRank >= kRiskbreakerRankCount) return {};
    return Label(rankText, characterRank);
  }
  void InitializeRankBox(const GazetteRankBox& placement) override {
    Publish();
    Menu::InitLiveTextBox(0, 0, placement.x, placement.y, placement.columns,
                          placement.lines, placement.center_x,
                          placement.center_y);
  }
  void SetRankText(GazetteText text) override {
    Publish();
    // The rank span borrows the writable module text table.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    Menu::SetLiveTextBoxText(
        0, reinterpret_cast<char*>(const_cast<std::uint8_t*>(text.data())));
  }
  bool MoveRankBox(int dx, int dy, bool required) override {
    Publish();
    auto* box = Menu::LiveTextBox(0);
    if (box == nullptr) return !required;
    box->x = Core::Math::WrapSigned16(static_cast<std::uint32_t>(box->x + dx));
    box->y = Core::Math::WrapSigned16(static_cast<std::uint32_t>(box->y + dy));
    box->x_indent = Core::Math::WrapSigned16(
        static_cast<std::uint32_t>(box->x_indent + dx));
    box->y_indent = Core::Math::WrapSigned16(
        static_cast<std::uint32_t>(box->y_indent + dy));
    return true;
  }
  void DismissRankBox() override {
    Publish();
    Menu::DismissLiveTextBox(0);
  }
  std::int32_t UpdateTopMenu(bool initialize) override {
    Publish();
    return UpdateLiveTopMenu(initialize ? 1 : 0);
  }
  std::uint8_t OverviewCursor() override {
    const auto& memory = VS::Menu::Framework::menuMemory.cursor_memories;
    return static_cast<std::uint8_t>(memory[kOverviewMemoryRow] +
                                     memory[kOverviewMemoryPage]);
  }
  void DrawOverview(std::int32_t step) override {
    Publish();
    RenderLiveOverview(step);
  }
  void ReleaseAshleyPreview() override {
    Publish();
    Framework::ReleaseLiveActorPreview();
  }
  void RestoreCamera() override {
    Publish();
    RestoreLiveCamera();
  }
  void InitializeInformation() override {
    Publish();
    Framework::InitializeLiveInformationBox();
  }
  void DismissInformation() override {
    Publish();
    Framework::DismissLiveInformationBox();
  }
  void BuildPage(GazettePage page) override {
    Publish();
    switch (page) {
      case GazettePage::kTitles:
        CreateLiveTitleRows();
        break;
      case GazettePage::kStatistics:
        CreateLiveStatisticRows();
        break;
      case GazettePage::kTimeTrials:
        CreateLiveTimeTrialRows();
        break;
      case GazettePage::kEnemies:
        InitializeLiveEnemyList();
        break;
    }
  }
  void DrawPage(GazettePage page, std::int32_t step) override {
    Publish();
    switch (page) {
      case GazettePage::kTitles:
        RenderLiveTitles(step);
        break;
      case GazettePage::kStatistics:
        RenderLiveStatistics(step);
        break;
      case GazettePage::kTimeTrials:
        RenderLiveTimeTrialRecords(step);
        break;
      case GazettePage::kEnemies:
        RenderLiveEnemyList(step);
        break;
    }
  }
  void LeaveTimeTrials() override {
    Publish();
    LeaveLiveTimeTrialMenu();
  }
  std::span<const std::uint8_t> TimeTrialDelays() override {
    for (std::size_t index = 0; index < delays_.size(); ++index) {
      delays_[index] =
          static_cast<std::uint8_t>(gazetteRows[index].animation_state);
    }
    return delays_;
  }
  void DeactivateRow(int row) override {
    Publish();
    Framework::DeactivateLiveRow(row);
  }
  void PlaySound(int cue) override {
    Publish();
    Audio::LivePlaySfxCentered(kSoundGroup, cue);
  }
  bool BeginEnemyRead() override {
    Publish();
    return BeginRead();
  }
  GazetteReadStatus EnemyReadStatus() override { return ReadStatus(); }
  bool FinishEnemyRead() override {
    Publish();
    return FinishRead();
  }
  bool FreeEnemyList() override {
    Publish();
    return ReleaseLiveGazetteEnemyData();
  }
  std::span<const GazetteEnemy> Enemies() override {
    return ReadEnemies(enemies_) ? std::span<const GazetteEnemy>(enemies_)
                                 : std::span<const GazetteEnemy>{};
  }
  std::optional<int> LoadEnemyPreview(std::int16_t model) override {
    if (model < 0 || model >= kEnemyPreviewFiles) return std::nullopt;
    Publish();
    return LoadLiveEnemyPreview(model);
  }
  void FreeEnemyPreview() override {
    Publish();
    FreeLiveEnemyPreview();
  }
  void ShowEnemyDescription(std::int32_t selected) override {
    Publish();
    if (selected != selectedEnemy) {
      Refuse("enemy description selection was not published");
      return;
    }
    ShowLiveEnemyDescription();
  }
  bool ShowListDescription(std::int32_t selected) override {
    Publish();
    if (!ReadEnemies(enemies_) || selected < 0 ||
        static_cast<std::size_t>(selected) >= enemies_.size()) {
      return false;
    }
    const auto text = enemies_[static_cast<std::size_t>(selected)].description;
    if (std::ranges::find(text, Core::String::kGameTextTerminator) ==
        text.end()) {
      return false;
    }
    Framework::SetLiveInformationMessageText(text);
    return true;
  }
  void SetMenuVisible(bool visible) override {
    Publish();
    Framework::SetLiveMenuVisible(visible ? 1 : 0);
  }
  void DrawNavigation(int direction) override {
    Publish();
    DrawLiveEnemyNavigation(direction);
  }
  void DrawViewer(std::int32_t step, std::int32_t transition) override {
    Publish();
    RenderLiveEnemyViewer(step, transition);
  }
  void Refuse(std::string_view reason) override { Score::Refuse(reason); }

 private:
  GazetteState& state_;
  GazetteCursorMemory& cursor_;
  EnemyRows enemies_{};
  std::array<std::uint8_t, kTimeTrialCount> delays_{};
};

GazetteScreenEnvironment ReadScreenEnvironment() {
  return {Battle_hudSlideOffset, VS::Menu::Framework::backgroundFadeStep,
          VS::Menu::Framework::menuState.control.current_menu,
          static_cast<std::uint8_t>(VS::Menu::Framework::hideMenu)};
}

void WriteScreenEnvironment(const GazetteScreenEnvironment& environment) {
  Battle_hudSlideOffset = environment.hud_slide_offset;
  VS::Menu::Framework::menuState.control.current_menu =
      environment.current_menu;
  VS::Menu::Framework::hideMenu = static_cast<char>(environment.hide_menu);
}

class LiveScreenHost final : public GazetteScreenHost {
 public:
  explicit LiveScreenHost(GazetteScreenEnvironment& environment)
      : environment_(environment) {}
  void SetBackgroundFade(bool enabled) override {
    Publish();
    Framework::SetLiveBackgroundFade(enabled ? 1 : 0);
  }
  std::int32_t CalculateProgress() override {
    Publish();
    return CalculateLiveProgress();
  }
  void SetFieldRendering(bool enabled) override {
    Publish();
    Field::LiveSetFieldRenderingEnabled(enabled ? 1 : 0);
  }
  void SetHudHidden(bool hidden) override {
    Publish();
    Battle::SetLiveHudHidden(hidden ? 1 : 0);
  }
  std::int32_t RowsIdle() override {
    Publish();
    return Framework::AreLiveMenuRowsIdle();
  }
  std::int32_t UpdateMenu() override {
    Publish();
    const auto result = UpdateLiveGazette();
    environment_ = ReadScreenEnvironment();
    return result;
  }
  void SetBackgroundPriority(int offset, int brightness) override {
    Publish();
    Framework::SetLiveBackgroundPriority(offset, brightness);
  }
  void ClearCommand() override {
    Publish();
    Framework::SetLiveCommandLabel(Framework::MenuCommand::kNone);
  }
  void ClearRows(int except_row) override {
    Publish();
    Framework::ClearLiveMenuExcept(except_row);
  }

 private:
  void Publish() { WriteScreenEnvironment(environment_); }
  GazetteScreenEnvironment& environment_;
};
}  // namespace

// decomp: _setEnemyDescription (src/MENU/MENU9.PRG/238.c:1414).
void ShowLiveEnemyDescription() {
  EnemyRows enemies;
  LiveEnemyListHost host;
  if (!ReadEnemies(enemies)) {
    Refuse("enemy description without a bounded MON.BIN payload");
    return;
  }
  static_cast<void>(ShowEnemyDescription(
      enemies, selectedEnemy,
      Label(miscInfo, menu9_misc_info::index::kEnemyNotDefeated), host));
}

// decomp: _renderEnemyNo (src/MENU/MENU9.PRG/238.c:1878).
void DrawLiveEnemyNumber(int x, int y, int number, int dimmed) {
  LiveEnemyNumberTarget target;
  if (!DrawEnemyNumber({x, y, number, dimmed != 0}, target)) {
    Refuse("enemy number outside 0..999 or primitive/ordering capacity");
  }
}

// decomp: _initEnemyList (src/MENU/MENU9.PRG/238.c:1916).
void InitializeLiveEnemyList() {
  EnemyRows enemies;
  std::array<std::uint32_t, kEnemyDefeatFlagWords> flags{};
  static_assert(std::size(VS::State::scoreData.enemy_kill_flags) ==
                kEnemyDefeatFlagWords);
  for (std::size_t index = 0; index < flags.size(); ++index) {
    flags[index] = static_cast<std::uint32_t>(
        VS::State::scoreData.enemy_kill_flags[index]);
  }
  if (!ReadEnemies(enemies) || !InitializeEnemyList(enemies, flags)) {
    Refuse("enemy list or defeat-bit range outside its payload");
    return;
  }
  StoreEnemies(enemies, true);
}

// decomp: _renderEnemyMenu (src/MENU/MENU9.PRG/238.c:1972).
void RenderLiveEnemyList(int step) {
  EnemyRows enemies;
  if (!ReadEnemies(enemies)) {
    Refuse("enemy menu without a bounded MON.BIN payload");
    return;
  }
  LiveEnemyListHost host;
  if (RenderEnemyList(
          enemies, ReadCursor().enemies, step, enemy_animation,
          Label(miscInfo, menu9_misc_info::index::kEnemyNameNone),
          Label(miscInfo, menu9_misc_info::index::kEnemyNotDefeated), host)) {
    StoreEnemies(enemies, false);
  }
}

// decomp: _renderEnemyDetailScreen (src/MENU/MENU9.PRG/238.c:2069).
void RenderLiveEnemyViewer(int step, int transition) {
  EnemyRows enemies;
  if (hideUi == 0 && !ReadEnemies(enemies)) {
    Refuse("viewer UI without a bounded MON.BIN payload");
    return;
  }
  LiveEnemyViewerHost host;
  static_cast<void>(
      RenderEnemyViewer(enemies,
                        {selectedEnemy, step, transition, hideUi != 0,
                         static_cast<std::uint32_t>(rotationColor),
                         static_cast<std::uint32_t>(zoomColor)},
                        host));
}

// decomp: _renderEnemyNavigation (src/MENU/MENU9.PRG/238.c:2782).
void DrawLiveEnemyNavigation(int direction) {
  LiveEnemyNavigationHost host;
  if (!DrawEnemyNavigation(direction, enemyNavigationState, hideUi != 0,
                           host)) {
    Refuse("enemy navigation direction or primitive/ordering capacity");
  }
}

// decomp: _menuInput (src/MENU/MENU9.PRG/238.c:548).
int UpdateLiveGazette() {
  auto state = ReadState();
  auto cursor = ReadCursor();
  LiveGazetteHost host(state, cursor);
  const GazetteInput input{
      static_cast<std::uint32_t>(VS::Input::buttonsPressed),
      static_cast<std::uint32_t>(VS::Input::buttonRepeat),
      static_cast<std::uint32_t>(VS::Input::previousButtons),
      static_cast<std::uint32_t>(VS::Input::releasedButtons),
      VS::Input::stickPosBuf.l_stick_x,
      VS::Input::stickPosBuf.l_stick_y,
      VS::Input::stickPosBuf.r_stick_x,
      VS::Input::stickPosBuf.r_stick_y};
  const auto result = UpdateGazette(state, cursor, input, host);
  host.Publish();
  return result.value_or(0);
}

// decomp: vs_menu9_exec (src/MENU/MENU9.PRG/238.c:269).
int UpdateLiveGazetteScreen(std::uint8_t* phase) {
  if (phase == nullptr) {
    Refuse("null score-screen phase");
    return 0;
  }
  auto environment = ReadScreenEnvironment();
  LiveScreenHost host(environment);
  const auto result =
      UpdateGazetteScreen(*phase, screen_state, environment, host);
  WriteScreenEnvironment(environment);
  return result;
}

bool ReleaseLiveGazetteEnemyData() {
  if (enemy_data.data == nullptr &&
      (enemy_data.request != nullptr || monBinData != nullptr ||
       monBinCdQueueSlot != nullptr)) {
    Refuse("enemy-list aliases have no terminal storage owner");
    return false;
  }
  if (enemy_data.data != nullptr) {
    const auto capacity = memory::LivePayloadCapacity(enemy_data.data);
    if (!capacity.has_value() || *capacity < enemy_data.bytes ||
        !Content::CancelLiveDiscWrites({enemy_data.data, enemy_data.bytes})) {
      Refuse("enemy-list storage cannot be safely cancelled/released");
      return false;
    }
  }
  if (enemy_data.request != nullptr &&
      Content::LiveDiscRequestGeneration(enemy_data.request) ==
          enemy_data.generation) {
    Content::ReleaseLiveDiscRequest(enemy_data.request);
  }
  if (enemy_data.data != nullptr) {
    memory::FreeLiveFromHighEnd(enemy_data.data);
  }
  enemy_data = {};
  monBinData = nullptr;
  monBinCdQueueSlot = nullptr;
  enemyDescriptions = nullptr;
  return true;
}

bool ResetLiveGazetteTerminalModuleState() {
  if (!ReleaseLiveGazetteEnemyData()) return false;
  WriteState({});
  cameraAngles.pad = 0;
  screen_state = {};
  enemy_animation = {};
  return true;
}

std::uint64_t LiveGazetteTerminalRefusals() noexcept { return refusals; }
}  // namespace VS::Menu::Score
