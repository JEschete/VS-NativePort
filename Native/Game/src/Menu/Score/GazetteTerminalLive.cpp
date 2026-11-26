#include "Menu/Score/GazetteTerminalLive.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Actors/ActorRecord.hpp"
#include "Audio/SfxLive.hpp"
#include "Battle/Hud.hpp"
#include "Battle/HudStateLive.hpp"
#include "Battle/StatusEffects.hpp"
#include "Content/BestiaryCatalog.hpp"
#include "Content/DataStore.hpp"
#include "Core/Log.hpp"
#include "Core/Math/MatrixLive.hpp"
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
#include "Render/NativeTextDraw.hpp"
#include "Render/Render.hpp"
#include "Render/RoomDrawLive.hpp"
#include "Render/SpritesLive.hpp"
#include "Render/UiPacketsLive.hpp"
#include "State/Progress.hpp"

using VS::Core::Gte::MATRIX;

namespace VS::Menu::Score {
namespace {
static_assert(kGazettePreviewFailed == kEnemyPreviewFailed,
              "the terminal's failed-preview result is the preview's");
constexpr std::size_t kHalfwordBytes = 2;
constexpr unsigned kByteBits = 8;
constexpr int kControlsSlot = 6;
constexpr int kIdentificationSlot = 7;
constexpr std::size_t kPreviewActor = 1;
constexpr int kSoundGroup = 126;
constexpr std::size_t kOverviewMemoryRow = 2;
constexpr std::size_t kOverviewMemoryPage = 3;
constexpr int kListNameIndent = 20;
constexpr int kListBackgroundWidth = 240;
constexpr int kViewerNameShift = 176;
constexpr int kDescriptionX = 8;
constexpr int kDescriptionY = 196;
constexpr double kNameWidth = 220, kNameHeight = 16;
constexpr double kDescriptionWidth = 304, kDescriptionHeight = 28;

using EnemyRows = std::vector<GazetteEnemy>;
// NOLINTNEXTLINE(bugprone-exception-escape): std::map's noexcept move.
struct EnemySession {
  Content::NativeTextAssets text;
  EnemyRows rows;
  std::map<std::string, Render::TextLayout, std::less<>> names;
  std::map<std::string, Render::TextLayout, std::less<>> descriptions;
  std::vector<Render::NativeTextDraw> draws;
  std::string description;
  bool ready = false;
};
std::optional<EnemySession>& EnemiesSession() {
  static std::optional<EnemySession> session;
  return session;
}
GazetteRecordCursor enemyCursor;
bool enemyCursorInitialized = false;
std::uint64_t bestiaryGeneration = 0;
GazetteRecordAnimation enemyAnimation;
GazetteScreenState screenState;
std::int32_t menuAnimation = 0;
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
GazetteText Label(const std::uint16_t (&table)[Size], std::size_t index) {
  return Label(std::as_bytes(std::span(table)), index);
}

bool ReadEnemies(EnemyRows& rows) {
  const auto& session = EnemiesSession();
  if (!session || !session->ready) return false;
  rows = session->rows;
  return true;
}

void StoreEnemies(const EnemyRows& rows, bool) {
  if (auto& session = EnemiesSession(); session) {
    session->rows = rows;
  }
}

void DetachLegacyInformationText() {
  auto box = Menu::ReadLiveTextBox(Menu::kInformationTextBox);
  box.text = nullptr;
  box.speed = 0;
  box.previous_chunks = 0;
  box.flags.done = true;
  Menu::WriteLiveTextBox(Menu::kInformationTextBox, box);
}

void SelectDescription(std::string_view id) {
  auto& session = EnemiesSession();
  if (!session || !session->descriptions.contains(id)) {
    Refuse("native enemy description is not loaded");
    return;
  }
  session->description = id;
  DetachLegacyInformationText();
}

Framework::MenuRow& NativeRow(const Framework::RowPlacement& placement,
                              std::string_view id, bool viewer) {
  constexpr std::array<std::uint8_t, 1> kEmpty{0xE7};
  auto background = placement;
  if (!viewer) background.w = kListBackgroundWidth;
  auto& row = Framework::SetLiveMenuItemText(background, kEmpty);
  auto& session = EnemiesSession();
  if (!session) return row;
  const auto found = session->names.find(id);
  if (found == session->names.end()) {
    Refuse("native enemy name is not loaded");
    return row;
  }
  session->draws.push_back(
      {&found->second,
       viewer ? placement.x - kViewerNameShift : placement.x + kListNameIndent,
       placement.y, true});
  return row;
}

void QueueEnemyText() {
  auto& session = EnemiesSession();
  if (!session) return;
  if (LiveBestiaryInformationVisible()) {
    const auto found = session->descriptions.find(session->description);
    if (found != session->descriptions.end()) {
      session->draws.push_back(
          {&found->second, kDescriptionX, kDescriptionY, true});
    }
  }
  Render::QueueLiveNativeText(session->draws, session->text.atlas.image);
  session->draws.clear();
}

bool AddEnemyLayout(EnemySession& session, std::string_view id, bool name) {
  auto& layouts = name ? session.names : session.descriptions;
  if (layouts.contains(id)) return true;
  auto text = session.text.catalog.Find(id);
  if (!text) {
    Refuse(text.error());
    return false;
  }
  auto layout = Render::LayoutNativeText(
      *text, session.text.font,
      {name ? kNameWidth : kDescriptionWidth,
       name ? kNameHeight : kDescriptionHeight, session.text.catalog.scale});
  if (!layout) {
    Refuse(std::string(id) + ": " + layout.error());
    return false;
  }
  layouts.emplace(id, std::move(*layout));
  return true;
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
  void SetNativeInformation(std::string_view id) override {
    SelectDescription(id);
  }
  Framework::MenuRow& CreateNativeRow(const Framework::RowPlacement& placement,
                                      std::string_view id) override {
    return NativeRow(placement, id, false);
  }
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
  Framework::MenuRow& CreateNativeRow(const Framework::RowPlacement& placement,
                                      std::string_view id) override {
    return NativeRow(placement, id, true);
  }
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
          .animation_step = menuAnimation,
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
  menuAnimation = state.animation_step;
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
  const auto& cursor = VS::Battle::menu9CursorMemory;
  return {{cursor.title_page, cursor.title_row},
          enemyCursorInitialized
              ? enemyCursor
              : GazetteRecordCursor{cursor.encyclopaedia_page,
                                    cursor.encyclopaedia_row},
          {cursor.record_time_page, cursor.record_time_row},
          cursor.gazette_page,
          cursor.gazette_row};
}

void WriteCursor(const GazetteCursorMemory& cursor) {
  auto& memory = VS::Battle::menu9CursorMemory;
  memory.title_page = static_cast<std::uint8_t>(cursor.titles.first_row);
  memory.title_row = cursor.titles.selected_row;
  enemyCursor = cursor.enemies;
  enemyCursorInitialized = true;
  // The original BATTLE cursor is a compatibility bookmark only. The native
  // in-session cursor never narrows to its byte-sized storage.
  memory.encyclopaedia_page = static_cast<std::uint8_t>(
      std::min<std::uint32_t>(cursor.enemies.first_row, UINT8_MAX));
  memory.encyclopaedia_row = cursor.enemies.selected_row;
  memory.record_time_page =
      static_cast<std::uint8_t>(cursor.statistics.first_row);
  memory.record_time_row = cursor.statistics.selected_row;
  memory.gazette_page = cursor.time_trial;
  memory.gazette_row = cursor.overview_row;
}

bool BeginRead() {
  if (EnemiesSession()) {
    Refuse("a prior enemy-list owner is still active");
    return false;
  }
  const auto* store = Content::DataStore::Get();
  if (store == nullptr || store->CatalogTables() == nullptr ||
      store->SaveText() == nullptr) {
    Refuse("missing native bestiary; run VS.Extract tables <datadir>");
    return false;
  }
  auto definitions = Content::LoadBestiary(*store->CatalogTables());
  if (!definitions) {
    Refuse(definitions.error());
    return false;
  }
  if (definitions->size() < kGazetteVisibleRecords) {
    Refuse("native encyclopedia view requires at least eight entries");
    return false;
  }
  EnemySession session{*store->SaveText(), {}, {}, {}, {}, {}, false};
  for (const auto& definition : *definitions) {
    if (!AddEnemyLayout(session, definition.name, true) ||
        !AddEnemyLayout(session, definition.description, false)) {
      return false;
    }
    GazetteEnemy enemy;
    enemy.model_id = static_cast<std::int16_t>(definition.preview_table_index);
    enemy.class_id = static_cast<std::int16_t>(definition.enemy_class);
    enemy.first_defeat_bit =
        static_cast<std::int16_t>(definition.first_defeated_kind);
    enemy.defeat_bit_count =
        static_cast<std::int16_t>(definition.defeated_kind_count);
    enemy.name_id = definition.name;
    enemy.description_id = definition.description;
    enemy.always_unlocked =
        definition.unlock == Content::BestiaryUnlock::kAlways;
    session.rows.push_back(std::move(enemy));
  }
  if (!AddEnemyLayout(session, "text.bestiary.locked.name", true) ||
      !AddEnemyLayout(session, "text.bestiary.locked.description", false)) {
    return false;
  }
  if (enemyCursor.first_row + kGazetteVisibleRecords > session.rows.size()) {
    enemyCursor = {};
    enemyCursorInitialized = false;
  }
  EnemiesSession() = std::move(session);
  ++bestiaryGeneration;
  return true;
}

GazetteReadStatus ReadStatus() {
  return EnemiesSession() ? GazetteReadStatus::kLoaded
                          : GazetteReadStatus::kRefused;
}

bool FinishRead() {
  auto& session = EnemiesSession();
  if (!session || session->ready) return false;
  session->ready = true;
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
    const auto* actor = VS::Actors::actorRecords[kPreviewActor];
    if (actor == nullptr) return std::nullopt;
    return GazettePreviewCamera{
        actor->menu_camera_distance,
        std::bit_cast<std::int16_t>(actor->collision_height)};
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
    if (auto& session = EnemiesSession(); session) {
      session->description.clear();
    }
    Render::ResetQueuedNativeText();
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
    SelectDescription(
        enemies_[static_cast<std::size_t>(selected)].description_id);
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
  return {VS::Battle::hudSlideOffset, VS::Menu::Framework::backgroundFadeStep,
          VS::Menu::Framework::menuState.control.current_menu,
          static_cast<std::uint8_t>(VS::Menu::Framework::hideMenu)};
}

void WriteScreenEnvironment(const GazetteScreenEnvironment& environment) {
  VS::Battle::hudSlideOffset = environment.hud_slide_offset;
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

// decomp: setEnemyDescription (src/MENU/MENU9.PRG/238.c:1419; was
// _setEnemyDescription).
void ShowLiveEnemyDescription() {
  EnemyRows enemies;
  LiveEnemyListHost host;
  if (!ReadEnemies(enemies)) {
    Refuse("enemy description without an active native bestiary");
    return;
  }
  static_cast<void>(ShowEnemyDescription(enemies, selectedEnemy, {}, host));
}

// decomp: renderEnemyNo (src/MENU/MENU9.PRG/238.c:1883; was _renderEnemyNo).
void DrawLiveEnemyNumber(int x, int y, int number, int dimmed) {
  LiveEnemyNumberTarget target;
  if (!DrawEnemyNumber({x, y, number, dimmed != 0}, target)) {
    Refuse("enemy number outside 0..999 or primitive/ordering capacity");
  }
}

// decomp: initEnemyList (src/MENU/MENU9.PRG/238.c:1921; was _initEnemyList).
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
  DetachLegacyInformationText();
}

// decomp: renderEnemyMenu (src/MENU/MENU9.PRG/238.c:1977; was
// _renderEnemyMenu).
void RenderLiveEnemyList(int step) {
  EnemyRows enemies;
  if (!ReadEnemies(enemies)) {
    Refuse("enemy menu without an active native bestiary");
    return;
  }
  LiveEnemyListHost host;
  if (RenderEnemyList(enemies, ReadCursor().enemies, step, enemyAnimation, {},
                      {}, host)) {
    StoreEnemies(enemies, false);
    QueueEnemyText();
  }
}

// decomp: renderEnemyDetailScreen (src/MENU/MENU9.PRG/238.c:2074; was
// _renderEnemyDetailScreen).
void RenderLiveEnemyViewer(int step, int transition) {
  EnemyRows enemies;
  if (hideUi == 0 && !ReadEnemies(enemies)) {
    Refuse("viewer UI without an active native bestiary");
    return;
  }
  LiveEnemyViewerHost host;
  static_cast<void>(
      RenderEnemyViewer(enemies,
                        {selectedEnemy, step, transition, hideUi != 0,
                         static_cast<std::uint32_t>(rotationColor),
                         static_cast<std::uint32_t>(zoomColor)},
                        host));
  QueueEnemyText();
}

// decomp: renderEnemyNavigation (src/MENU/MENU9.PRG/238.c:2785; was
// _renderEnemyNavigation).
void DrawLiveEnemyNavigation(int direction) {
  LiveEnemyNavigationHost host;
  if (!DrawEnemyNavigation(direction, enemyNavigationState, hideUi != 0,
                           host)) {
    Refuse("enemy navigation direction or primitive/ordering capacity");
  }
}

// decomp: menuInput (src/MENU/MENU9.PRG/238.c:553; was _menuInput).
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

// decomp: vs_menu9_exec (src/MENU/MENU9.PRG/238.c:276).
int UpdateLiveGazetteScreen(std::uint8_t* phase) {
  if (phase == nullptr) {
    Refuse("null score-screen phase");
    return 0;
  }
  auto environment = ReadScreenEnvironment();
  LiveScreenHost host(environment);
  const auto result =
      UpdateGazetteScreen(*phase, screenState, environment, host);
  WriteScreenEnvironment(environment);
  return result;
}

bool ReleaseLiveGazetteEnemyData() {
  Render::ResetQueuedNativeText();
  EnemiesSession().reset();
  return true;
}

bool ResetLiveGazetteTerminalModuleState() {
  if (!ReleaseLiveGazetteEnemyData()) return false;
  WriteState({});
  cameraAngles.pad = 0;
  screenState = {};
  enemyAnimation = {};
  enemyCursor = {};
  return true;
}

std::uint64_t LiveGazetteTerminalRefusals() noexcept { return refusals; }
bool BeginLiveBestiary() { return BeginRead(); }
bool FinishLiveBestiary() { return FinishRead(); }
std::uint64_t LiveBestiaryGeneration() noexcept {
  return EnemiesSession() ? bestiaryGeneration : 0;
}
std::span<const GazetteEnemy> LiveBestiaryEnemies() noexcept {
  const auto& session = EnemiesSession();
  return session ? std::span<const GazetteEnemy>(session->rows)
                 : std::span<const GazetteEnemy>{};
}
std::string_view LiveBestiaryDescription() noexcept {
  const auto& session = EnemiesSession();
  return session ? std::string_view(session->description) : std::string_view{};
}
bool LiveBestiaryInformationVisible() {
  const auto& session = EnemiesSession();
  if (!session || session->description.empty() ||
      VS::Input::settings.information == 0) {
    return false;
  }
  const auto box = Menu::ReadLiveTextBox(Menu::kInformationTextBox);
  return box.flags.kind ==
             static_cast<std::uint8_t>(Menu::TextBoxKind::kInformation) &&
         !box.flags.frozen &&
         box.state ==
             static_cast<int>(static_cast<std::uint8_t>(box.open_duration)) + 1;
}
}  // namespace VS::Menu::Score
