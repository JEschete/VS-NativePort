#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "Menu/Status/TerminalControllers.hpp"
#include "Menu/StatusLimbControllersWorld.hpp"

namespace status_terminal_tests {
namespace status = VS::Menu::Status;
namespace fw = VS::Menu::Framework;
enum class Event : std::uint32_t {
  kHud = 1,
  kHoist,
  kInitDetail,
  kBars,
  kStats,
  kPart,
  kDeactivate,
  kFormat,
  kCreate,
  kList,
  kNavigateDetail,
  kDescribe,
  kTabs,
  kExitDetail,
  kComparison,
  kSelectedRow,
  kStoreCursor,
  kReady,
  kSound,
  kCursor,
  kNextActor,
  kSwitch,
  kVitalsBegin,
  kPrimary,
  kPages,
  kCommand,
  kClear,
  kInformation,
  kLoadText,
  kUnloadText,
  kField,
  kPriority,
  kProjection,
  kInitInfo,
  kDismissInfo,
  kCondition,
  kDefense,
  kLimb,
  kEffects,
  kVitals,
  kRelease,
  kBackgroundFade,
  kBuildCamera,
  kPreview,
  kPromptBackground,
  kPromptButton,
  kPromptText
};
inline constexpr std::size_t kViewWords = 21;
inline constexpr std::size_t kCameraWords = 13;
inline constexpr std::size_t kOriginalRowPrefix = 60;
inline constexpr std::size_t kStatusEntries = 256;
inline constexpr std::size_t kHelpEntries = 700;
using Text = std::array<std::uint8_t, 4>;

inline Text MakeText(std::uint32_t seed, std::size_t index) {
  return {static_cast<std::uint8_t>((index + seed) % 220 + 1),
          static_cast<std::uint8_t>(index / 220 + 1), 0xE7, 0xE7};
}

class World final : public status::EquipmentTerminalHost,
                    public status::StatusTerminalHost,
                    public status::TerminalFrameHost {
 public:
  explicit World(std::uint32_t seed = 0) : seed(seed) {
    frame.selected_actor = 1;
    frame.stick_x = frame.stick_y = 128;
    for (auto& actor : actors) {
      actor.limb_names = {1, 2, 3, 4, 5, 6};
      actor.equipment_ids.fill(1);
      actor.accessory_id = 1;
      actor.analyzed = true;
      actor.weapon_gem_slots = actor.shield_gem_slots = 3;
    }
    for (std::size_t index = 0; index < offsets.size(); ++index) {
      offsets[index] = static_cast<std::uint8_t>(index * 7);
    }
    for (std::size_t index = 0; index < animation.size(); ++index) {
      animation[index] = static_cast<std::uint8_t>(index < 10 ? index + 1 : 10);
    }
    for (std::size_t index = 0; index < cursors.size(); ++index) {
      cursors[index] = static_cast<std::int32_t>(0x10000 + index * 0x10007);
    }
    for (std::size_t index = 0; index < status_text.size(); ++index) {
      status_text[index] = MakeText(seed, index);
    }
    for (std::size_t index = 0; index < help_text.size(); ++index) {
      help_text[index] = MakeText(seed + 11, index);
    }
    for (std::size_t index = 0; index < names.size(); ++index) {
      names[index] = MakeText(seed + 21, index);
    }
    for (std::size_t index = 0; index < weapon_names.size(); ++index) {
      weapon_names[index] = MakeText(seed + 31, index);
    }
    for (std::size_t index = 0; index < descriptions.size(); ++index) {
      descriptions[index] = MakeText(seed + 41, index);
    }
    for (std::size_t index = 0; index < component_text.size(); ++index) {
      component_text[index] = MakeText(seed + 51, index);
    }
    for (std::size_t index = 0; index < rows.size(); ++index) {
      auto bytes = std::as_writable_bytes(std::span(&rows[index], 1));
      for (std::size_t byte = 0; byte < kOriginalRowPrefix; ++byte) {
        bytes[byte] =
            static_cast<std::byte>((seed + index * 13 + byte * 7) & 255);
      }
      rows[index].sub_text = nullptr;
    }
    description.fill(0xAA);
  }

  std::uint32_t seed = 0;
  status::TerminalFrame frame{};
  status::TerminalView view{};
  status::EquipmentDetailState detail{};
  status::EquipmentListState list{};
  status::StatusTerminalState screen{};
  std::array<status::TerminalActor, status::kInspectableActorSlots> actors{};
  std::array<fw::MenuRow, fw::kRowCount> rows{};
  std::array<std::uint8_t, 36> offsets{};
  std::array<std::uint8_t, 244> animation{};
  std::array<std::int32_t, 21> cursors{};
  std::array<std::uint8_t, 96> description{};
  std::array<std::uint32_t, 9> row_flags{};
  std::array<Text, kStatusEntries> status_text{};
  std::array<Text, kHelpEntries> help_text{};
  std::array<Text, 9> names{};
  std::array<Text, status::kInspectableActorSlots> weapon_names{};
  std::array<Text, 9> descriptions{};
  std::array<Text, std::size_t{9} * 7> component_text{};
  status_limb_tests::PacketStore packets;
  std::vector<std::vector<std::uint32_t>> events;
  bool services = true;
  bool actors_present = true;
  bool rows_present = true;
  bool texts_present = true;
  bool model_present = true;
  bool ready = true;
  bool item_text_present = true;
  bool switch_changes_actor = false;
  int switch_target = 1;
  int switch_result = 1;
  int selected_row = -1;
  int detail_selection = 9;
  int panel_phase = 2;
  int effects_result = 0;
  int load_result = 1;
  int unload_result = 1;
  std::int32_t cursor_result = 7;
  std::int32_t projection = 256;
  status::TerminalModelCamera model{10, 4};
  bool framebuffer = false;

  void Record(Event kind, std::initializer_list<std::int64_t> arguments = {}) {
    std::vector<std::uint32_t> event{static_cast<std::uint32_t>(kind)};
    for (auto argument : arguments) {
      event.push_back(static_cast<std::uint32_t>(argument));
    }
    events.push_back(std::move(event));
  }
  void TextEvent(std::vector<std::uint32_t>& event,
                 std::span<const std::uint8_t> text) {
    const auto end = std::ranges::find(text, std::uint8_t{0xE7});
    const auto length = end == text.end()
                            ? text.size()
                            : static_cast<std::size_t>(end - text.begin()) + 1;
    event.push_back(static_cast<std::uint32_t>(length));
    event.insert(event.end(), text.begin(),
                 text.begin() + static_cast<std::ptrdiff_t>(length));
  }
  status::TerminalFrame& Frame() override { return frame; }
  status::TerminalCamera& Camera() override { return view.camera; }
  status::TerminalView& View() override { return view; }
  status::TerminalTables Tables() override {
    return {offsets, animation, cursors};
  }
  std::optional<status::TerminalActor> Actor(int slot) override {
    if (!actors_present || slot < 0 || slot >= status::kInspectableActorSlots) {
      return std::nullopt;
    }
    return actors[static_cast<std::size_t>(slot)];
  }
  std::optional<bool> MenuReady() override {
    Record(Event::kReady);
    return services ? std::optional{ready} : std::nullopt;
  }
  bool Sound(status::TerminalSound sound) override {
    Record(Event::kSound, {static_cast<int>(sound)});
    return services;
  }
  std::optional<std::int32_t> Cursor(std::uint32_t step,
                                     std::int32_t position) override {
    Record(Event::kCursor, {step, position});
    return services ? std::optional{cursor_result} : std::nullopt;
  }
  std::optional<int> NextActor(int current, int step) override {
    Record(Event::kNextActor, {current, step});
    return services
               ? std::optional{(current + step + static_cast<int>(seed & 1)) %
                               status::kInspectableActorSlots}
               : std::nullopt;
  }
  std::optional<int> SwitchActor(int request) override {
    Record(Event::kSwitch, {request});
    if (request != 0) {
      frame.selected_actor = request & 15;
    } else if (switch_changes_actor) {
      frame.selected_actor = switch_target;
    }
    return services ? std::optional{switch_result} : std::nullopt;
  }
  bool BeginVitals(int actor) override {
    Record(Event::kVitalsBegin, {actor});
    return services;
  }
  bool PrimaryStats(int command) override {
    Record(Event::kPrimary, {command});
    return services;
  }
  bool StatPages(int command) override {
    Record(Event::kPages, {command});
    return services;
  }
  bool CommandLabel(int command) override {
    Record(Event::kCommand, {command});
    return services;
  }
  bool ClearRows(int row) override {
    Record(Event::kClear, {row});
    return services;
  }
  bool Information(std::span<const std::uint8_t> text) override {
    Record(Event::kInformation);
    TextEvent(events.back(), text);
    return services;
  }
  std::span<const std::uint8_t> StatusText(int index, bool) override {
    return texts_present && index >= 0 &&
                   static_cast<std::size_t>(index) < status_text.size()
               ? std::span<const std::uint8_t>(
                     status_text[static_cast<std::size_t>(index)])
                     .first(3)
               : std::span<const std::uint8_t>{};
  }
  std::span<fw::MenuRow> Rows() override {
    return rows_present ? std::span(rows) : std::span<fw::MenuRow>{};
  }
  bool HideHud() override {
    Record(Event::kHud, {1});
    return services;
  }
  bool HoistRow(int row) override {
    Record(Event::kHoist, {row});
    return services;
  }
  bool InitializeDetail(int row, int pages) override {
    Record(Event::kInitDetail, {row, pages});
    return services;
  }
  bool DpPpBars(int command) override {
    Record(Event::kBars, {command});
    return services;
  }
  bool EquipmentStats(int row) override {
    Record(Event::kStats, {row});
    return services;
  }
  bool ComponentRow(int equipment, int part, bool slide) override {
    Record(Event::kPart, {equipment, part, slide});
    return services;
  }
  bool Deactivate(int row) override {
    Record(Event::kDeactivate, {row});
    return services;
  }
  std::span<std::uint8_t> DescriptionBuffer() override { return description; }
  std::optional<status::TerminalRowText> FormatRow(
      int row, std::span<std::uint8_t> buffer) override {
    Record(Event::kFormat, {row});
    if (!services || buffer.size() < 3) {
      return std::nullopt;
    }
    const auto index = static_cast<std::size_t>(row);
    std::copy_n(descriptions[index].begin(), 3, buffer.begin());
    const auto& name =
        row == 0
            ? weapon_names[static_cast<std::size_t>(frame.selected_actor - 1)]
            : names[index];
    return status::TerminalRowText{std::span(name).first(3), buffer.first(3),
                                   row_flags[index]};
  }
  fw::MenuRow* CreateRow(const fw::RowPlacement& placement,
                         std::span<const std::uint8_t> text) override {
    Record(Event::kCreate, {placement.row, placement.x, placement.y,
                            placement.w, placement.background_width});
    TextEvent(events.back(), text);
    return services && rows_present
               ? &rows[static_cast<std::size_t>(placement.row)]
               : nullptr;
  }
  bool CreateList(std::span<const status::TerminalRowText> text,
                  std::span<const std::int32_t> flags) override {
    Record(Event::kList,
           {static_cast<int>(text.size()), 0x19142, frame.cursor_memory});
    for (std::size_t row = 0; row < text.size(); ++row) {
      events.back().push_back(static_cast<std::uint32_t>(flags[row]));
      TextEvent(events.back(), text[row].name);
      TextEvent(events.back(), text[row].description);
    }
    return services;
  }
  std::optional<int> NavigateDetail(std::uint8_t selected,
                                    std::uint32_t flags) override {
    Record(Event::kNavigateDetail, {selected, flags});
    return services ? std::optional{detail_selection} : std::nullopt;
  }
  std::span<const std::uint8_t> DescribeComponent(int row, int part) override {
    Record(Event::kDescribe, {row, part});
    return std::span(component_text[static_cast<std::size_t>(row) * 7 +
                                    static_cast<std::size_t>(part)])
        .first(3);
  }
  std::span<const std::uint8_t> HelpText(int index) override {
    return texts_present && index >= 0 &&
                   static_cast<std::size_t>(index) < help_text.size()
               ? std::span<const std::uint8_t>(
                     help_text[static_cast<std::size_t>(index)])
                     .first(3)
               : std::span<const std::uint8_t>{};
  }
  bool Tabs() override {
    Record(Event::kTabs, {1});
    return services;
  }
  bool ExitDetail() override {
    Record(Event::kExitDetail, {1});
    return services;
  }
  bool ComparisonStats(int actor) override {
    Record(Event::kComparison, {actor});
    return services;
  }
  std::optional<int> SelectedRow() override {
    Record(Event::kSelectedRow);
    return services ? std::optional{selected_row} : std::nullopt;
  }
  bool StoreCursor() override {
    Record(Event::kStoreCursor);
    return services;
  }
  bool HasItemText() override { return item_text_present; }
  std::optional<int> LoadItemText(bool start) override {
    Record(Event::kLoadText, {start});
    return services ? std::optional{load_result} : std::nullopt;
  }
  std::optional<int> UnloadItemText() override {
    Record(Event::kUnloadText);
    return services ? std::optional{unload_result} : std::nullopt;
  }
  bool FieldRendering(bool enabled) override {
    Record(Event::kField, {enabled});
    return services;
  }
  bool BackgroundPriority(int offset, int brightness) override {
    Record(Event::kPriority, {offset, brightness});
    return services;
  }
  std::int32_t ProjectionDistance() override { return projection; }
  bool SetProjection(std::int32_t value) override {
    Record(Event::kProjection, {value});
    projection = value;
    return services;
  }
  bool InitializeInformation() override {
    Record(Event::kInitInfo);
    return services;
  }
  bool DismissInformation() override {
    Record(Event::kDismissInfo);
    return services;
  }
  bool Condition() override {
    Record(Event::kCondition);
    return services;
  }
  bool Defense() override {
    Record(Event::kDefense);
    return services;
  }
  std::optional<int> LimbPanel(int command) override {
    Record(Event::kLimb, {command});
    return services ? std::optional{panel_phase} : std::nullopt;
  }
  std::optional<int> StatusEffects(std::optional<int> actor,
                                   bool selection) override {
    Record(Event::kEffects, {actor ? *actor : -1, selection});
    return services ? std::optional{effects_result} : std::nullopt;
  }
  std::optional<int> EquipmentList(int request) override {
    const auto result =
        status::UpdateEquipmentList(list, detail, *this, request);
    return result ? std::optional{*result} : std::nullopt;
  }
  bool Vitals() override {
    Record(Event::kVitals);
    return services;
  }
  bool ReleasePreview() override {
    Record(Event::kRelease);
    return services;
  }
  bool BackgroundFade() override {
    Record(Event::kBackgroundFade, {0});
    return services;
  }
  bool DrawFrame() override { return status::DrawFrame(*this).has_value(); }
  bool Prompt(int offset, bool available) override {
    Record(Event::kPromptBackground, {16 - offset, 38, 88, 10});
    Record(Event::kPromptButton, {1, 8 - offset, 36, 0});
    Record(Event::kPromptText, {VS::Render::PackScreenWord(28 - offset, 38),
                                0x404040 << static_cast<int>(available), 0});
    return services;
  }
  std::optional<status::TerminalModelCamera> ModelCamera() override {
    return model_present ? std::optional{model} : std::nullopt;
  }
  bool BuildCamera() override {
    Record(Event::kBuildCamera);
    const auto words =
        std::bit_cast<std::array<std::uint32_t, kCameraWords>>(view.camera);
    events.back().insert(events.back().end(), words.begin(), words.end());
    return services;
  }
  bool DrawPreview() override {
    Record(Event::kPreview);
    return services;
  }
  bool Background() override {
    status_limb_tests::PacketChannel queue(packets,
                                           frame.background_first != 0 ? 0 : 1);
    return status::DrawBackground(queue, framebuffer);
  }
  bool Fade() override {
    status_limb_tests::PacketChannel queue(packets, 0);
    return status::DrawModelSlideFade(queue, frame.fade != 0, framebuffer);
  }
  bool LimbPanel() override { return LimbPanel(0).has_value(); }

  [[nodiscard]] std::size_t Count(Event event) const {
    return static_cast<std::size_t>(
        std::ranges::count_if(events, [event](const auto& words) {
          return !words.empty() &&
                 words[0] == static_cast<std::uint32_t>(event);
        }));
  }
};
}  // namespace status_terminal_tests
