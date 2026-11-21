#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string_view>

#include "Core/Math/ScalarMath.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Status/ActorSwitching.hpp"
#include "Render/UiPackets.hpp"

// NOLINTNEXTLINE(readability-identifier-naming): the catalog's component.
namespace VS::Menu::Status {
enum class TerminalError : std::uint8_t {
  kMissingActor,
  kInvalidSelection,
  kInvalidAnimation,
  kMissingRows,
  kMissingText,
  kUnavailableService,
};
using TerminalResult = std::expected<std::int32_t, TerminalError>;
[[nodiscard]] std::string_view TerminalErrorText(TerminalError error) noexcept;

enum class TerminalSound : std::uint8_t { kChange, kLeave, kSelect, kInvalid };

struct TerminalActor {
  std::array<std::uint8_t, kStatusLimbSlots> limb_names{};
  std::array<std::uint16_t, kStatusLimbSlots + 2> equipment_ids{};
  std::uint16_t accessory_id = 0;
  std::uint8_t weapon_gem_slots = 0;
  std::uint8_t shield_gem_slots = 0;
  bool analyzed = false;
  std::uint32_t statuses = 0;
};

struct TerminalFrame {
  std::int32_t selected_actor = 0;
  std::int32_t previous_actor = 0;
  std::int32_t animation_step = 0;
  std::int32_t yaw = 0;
  std::int32_t pitch = 0;
  std::int32_t background_first = 0;
  std::int32_t fade = 0;
  std::uint8_t vitals_phase = 0;
  std::uint8_t vitals_slide = 0;
  std::uint8_t initialized_actor = 0;
  std::uint8_t model_delay = 0;
  std::uint8_t selected_element = 0;
  std::uint8_t selected_stat = 0;
  std::uint8_t cursor_color = 0;
  std::uint8_t freeze_tabs = 0;
  std::uint8_t stat_page = 0;
  std::uint8_t enabled_pages = 0;
  std::uint8_t selected_equipment = 0;
  std::uint8_t cursor_memory = 0;
  std::uint8_t hud_hidden = 0;
  std::uint8_t current_menu = 0;
  std::uint32_t pressed = 0;
  std::uint32_t held = 0;
  std::uint32_t repeat = 0;
  std::int32_t stick_x = 0;
  std::int32_t stick_y = 0;
};

struct TerminalTables {
  std::span<const std::uint8_t> row_offsets;
  std::span<const std::uint8_t> animation_indices;
  std::span<const std::int32_t> detail_cursor_positions;
};

// All byte fields are LBU/SB in the original, including declarations spelled
// `char`. No state here owns actors, item text, menu rows or renderer storage.
struct EquipmentDetailState {
  std::uint8_t cursor_animation = 0;
  std::uint8_t phase = 0;
  std::uint8_t selected_row = 0;
  std::uint8_t row_to_render = 0;
  std::uint8_t selected_element = 0;
  friend bool operator==(const EquipmentDetailState&,
                         const EquipmentDetailState&) = default;
};
struct EquipmentListState {
  std::uint8_t phase = 0;
  std::uint8_t timer = 0;
  friend bool operator==(const EquipmentListState&,
                         const EquipmentListState&) = default;
};

struct TerminalRowText {
  std::span<const std::uint8_t> name;
  std::span<const std::uint8_t> description;
  std::uint32_t flags = 0;
};

class TerminalCommonHost {
 public:
  virtual ~TerminalCommonHost() = default;
  // Callbacks must publish this frame before calling another controller and
  // refresh it afterwards. Actor-switch and limb callbacks can change it.
  virtual TerminalFrame& Frame() = 0;
  [[nodiscard]] virtual TerminalTables Tables() = 0;
  [[nodiscard]] virtual std::optional<TerminalActor> Actor(int slot) = 0;
  [[nodiscard]] virtual std::optional<bool> MenuReady() = 0;
  [[nodiscard]] virtual bool Sound(TerminalSound sound) = 0;
  [[nodiscard]] virtual std::optional<std::int32_t> Cursor(
      std::uint32_t animation, std::int32_t position) = 0;
  [[nodiscard]] virtual std::optional<int> NextActor(int current, int step) = 0;
  [[nodiscard]] virtual std::optional<int> SwitchActor(int request) = 0;
  [[nodiscard]] virtual bool BeginVitals(int actor) = 0;
  [[nodiscard]] virtual bool PrimaryStats(int command) = 0;
  [[nodiscard]] virtual bool StatPages(int command) = 0;
  [[nodiscard]] virtual bool CommandLabel(Framework::MenuCommand command) = 0;
  [[nodiscard]] virtual bool ClearRows(int except) = 0;
  [[nodiscard]] virtual bool Information(
      std::span<const std::uint8_t> text) = 0;
  [[nodiscard]] std::span<const std::uint8_t> StatusText(int index) {
    return StatusText(index, true);
  }
  [[nodiscard]] virtual std::span<const std::uint8_t> StatusText(
      int index, bool indexed) = 0;
};

class EquipmentTerminalHost : public TerminalCommonHost {
 public:
  [[nodiscard]] virtual std::span<Framework::MenuRow> Rows() = 0;
  [[nodiscard]] virtual bool HideHud() = 0;
  [[nodiscard]] virtual bool HoistRow(int row) = 0;
  [[nodiscard]] virtual bool InitializeDetail(int row, int pages) = 0;
  [[nodiscard]] virtual bool DpPpBars(int command) = 0;
  [[nodiscard]] virtual bool EquipmentStats(int row) = 0;
  [[nodiscard]] virtual bool ComponentRow(int equipment, int part,
                                          bool slide) = 0;
  [[nodiscard]] virtual bool Deactivate(int row) = 0;
  [[nodiscard]] virtual std::optional<TerminalRowText> FormatRow(
      int row, std::span<std::uint8_t> buffer) = 0;
  [[nodiscard]] virtual std::span<std::uint8_t> DescriptionBuffer() = 0;
  virtual Framework::MenuRow* CreateRow(
      const Framework::RowPlacement& placement,
      std::span<const std::uint8_t> text) = 0;
  [[nodiscard]] virtual bool CreateList(
      std::span<const TerminalRowText> rows,
      std::span<const std::int32_t> flags) = 0;
  [[nodiscard]] virtual std::optional<int> NavigateDetail(
      std::uint8_t selected, std::uint32_t flags) = 0;
  [[nodiscard]] virtual std::span<const std::uint8_t> DescribeComponent(
      int equipment, int component) = 0;
  [[nodiscard]] virtual std::span<const std::uint8_t> HelpText(int index) = 0;
  [[nodiscard]] virtual bool Tabs() = 0;
  [[nodiscard]] virtual bool ExitDetail() = 0;
  [[nodiscard]] virtual bool ComparisonStats(int actor) = 0;
  [[nodiscard]] virtual std::optional<int> SelectedRow() = 0;
  [[nodiscard]] virtual bool StoreCursor() = 0;
};

// decomp: equipmentDetailScreen (MENU4.PRG src/MENU/MENU4.PRG/120.c:1485; was
// _equipmentDetailScreen, 80104F80..80105970).
[[nodiscard]] TerminalResult UpdateEquipmentDetails(EquipmentDetailState& state,
                                                    EquipmentTerminalHost& host,
                                                    int row);
// decomp: equipmentScreen (MENU4.PRG src/MENU/MENU4.PRG/120.c:1877; was
// _equipmentScreen, 80105970..80105E94).
[[nodiscard]] TerminalResult UpdateEquipmentList(EquipmentListState& state,
                                                 EquipmentDetailState& detail,
                                                 EquipmentTerminalHost& host,
                                                 int element);

struct TerminalCamera {
  Core::Math::Vec3 position{};
  std::int32_t position_padding = 0;
  Core::Math::Vec3 look_at{};
  std::int32_t look_at_padding = 0;
  Core::Math::Vec3 angles{};
  std::int32_t angles_padding = 0;
  std::int32_t far_clip = 0;
  friend bool operator==(const TerminalCamera&,
                         const TerminalCamera&) = default;
};
struct TerminalView {
  Core::Math::Mat3 rotation{};
  std::int16_t rotation_padding = 0;
  Core::Math::Vec3 translation{};
  TerminalCamera camera{};
  friend bool operator==(const TerminalView&, const TerminalView&) = default;
};
struct StatusTerminalState {
  std::int32_t animation_wait = 0;
  std::int32_t screen_enabled = 0;
  std::int32_t cursor_animation = 0;
  std::int32_t saved_projection = 0;
  TerminalView saved_view{};
  std::uint8_t prompt_animation = 0;
  friend bool operator==(const StatusTerminalState&,
                         const StatusTerminalState&) = default;
};

// decomp: fadeScreenLeft (MENU4.PRG src/MENU/MENU4.PRG/120.c:2083; was
// _fadeScreenLeft, 80105E94..80105F60).
[[nodiscard]] bool DrawModelSlideFade(Render::PacketQueue& queue, bool enabled,
                                      bool framebuffer);
// decomp: drawMenuBackground (MENU4.PRG src/MENU/MENU4.PRG/120.c:2123; was
// _drawMenuBackground, 80105F60..80106150). The caller chooses scene[-1] when
// background_first, scene[2045] otherwise.
[[nodiscard]] bool DrawBackground(Render::PacketQueue& queue, bool framebuffer);

struct TerminalModelCamera {
  std::uint16_t distance = 0;  // original LHU +0x656
  std::uint16_t height = 0;    // original LHU +0x63e
};
class TerminalFrameHost {
 public:
  virtual ~TerminalFrameHost() = default;
  virtual TerminalFrame& Frame() = 0;
  virtual TerminalCamera& Camera() = 0;
  [[nodiscard]] virtual std::optional<TerminalModelCamera> ModelCamera() = 0;
  [[nodiscard]] virtual bool BuildCamera() = 0;
  [[nodiscard]] virtual bool DrawPreview() = 0;
  [[nodiscard]] virtual bool Background() = 0;
  [[nodiscard]] virtual bool Fade() = 0;
  [[nodiscard]] virtual bool LimbPanel() = 0;
  [[nodiscard]] virtual bool Vitals() = 0;
};
// decomp: drawScreen (MENU4.PRG src/MENU/MENU4.PRG/120.c:2192; was _drawScreen,
// 80106150..80106308).
[[nodiscard]] TerminalResult DrawFrame(TerminalFrameHost& host);

class StatusTerminalHost : public TerminalCommonHost {
 public:
  [[nodiscard]] virtual bool HasItemText() = 0;
  [[nodiscard]] virtual std::optional<int> LoadItemText(bool start) = 0;
  [[nodiscard]] virtual std::optional<int> UnloadItemText() = 0;
  [[nodiscard]] virtual bool FieldRendering(bool enabled) = 0;
  [[nodiscard]] virtual bool BackgroundPriority(int offset, int brightness) = 0;
  [[nodiscard]] virtual std::int32_t ProjectionDistance() = 0;
  [[nodiscard]] virtual bool SetProjection(std::int32_t distance) = 0;
  virtual TerminalView& View() = 0;
  [[nodiscard]] virtual bool InitializeInformation() = 0;
  [[nodiscard]] virtual bool DismissInformation() = 0;
  [[nodiscard]] virtual bool Condition() = 0;
  [[nodiscard]] virtual bool Defense() = 0;
  [[nodiscard]] virtual std::optional<int> LimbPanel(int command) = 0;
  [[nodiscard]] virtual std::optional<int> StatusEffects(
      std::optional<int> actor, bool selection) = 0;
  [[nodiscard]] virtual std::optional<int> EquipmentList(int request) = 0;
  [[nodiscard]] virtual bool Vitals() = 0;
  [[nodiscard]] virtual bool ReleasePreview() = 0;
  [[nodiscard]] virtual bool BackgroundFade() = 0;
  [[nodiscard]] virtual bool DrawFrame() = 0;
  [[nodiscard]] virtual bool Prompt(int offset, bool available) = 0;
};
// decomp: vs_menu4_exec (MENU4.PRG src/MENU/MENU4.PRG/120.c:2272,
// 801064D4..80107448). A byte state and a full signed-word return, not a
// byte-returning callback.
[[nodiscard]] TerminalResult UpdateStatusTerminal(StatusTerminalState& state,
                                                  StatusTerminalHost& host,
                                                  std::uint8_t& menu_state);
}  // namespace VS::Menu::Status
