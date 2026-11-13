#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Menu/Container/ContainerDetailControllers.hpp"

namespace container_detail_controllers_tests {
namespace detail = VS::Menu::container;
namespace fw = VS::Menu::Framework;
namespace samples {
inline constexpr int kFrames = 10;
inline constexpr int kWaitTicks = kFrames + 1;
inline constexpr int kRow = 20;
inline constexpr int kHeader = 31;
inline constexpr int kWideSlot = 65535;
inline constexpr int kWidePage = 191;
inline constexpr int kByteMask = 255;
inline constexpr int kOldUnused = 91;
inline constexpr int kReadyResult = -7;
inline constexpr int kFirstDetailElement = 9;
inline constexpr int kSelectedX = 48;
inline constexpr int kMergeX = 60;
inline constexpr int kBeforeMergeX = kMergeX + 1;
inline constexpr int kSelectedIcon = 24;
inline constexpr int kIconCount = 8;
inline constexpr int kLastStep = 4;
inline constexpr int kIconPitch = 16;
inline constexpr int kIconY = -16;
inline constexpr std::size_t kMaxOrder = 192;
inline constexpr std::size_t kControllers = 6;
inline constexpr std::size_t kCallArguments = 3;
inline constexpr int kShieldRows = 3;
inline constexpr int kLastInventoryArmor = 16;
inline constexpr int kFirstContainerOnlyArmor = 17;
inline constexpr int kLastContainerArmor = 64;
inline constexpr std::uint32_t kRowType = 0x7C050000;
}  // namespace samples

enum class Event {
  kReady = 1,
  kBegin,
  kLayout,
  kCurrent,
  kStats,
  kBars,
  kDp,
  kComponent,
  kExit,
  kPoll,
  kSelect,
  kFormat,
  kChange,
  kSlots,
  kDrawSlots,
  kArmorGate,
  kReset,
  kHeader,
  kCommand,
  kController,
  kTabs,
  kDetails,
  kIcon,
};
struct Call {
  Event event;
  std::array<std::int32_t, samples::kCallArguments> args{};
  friend bool operator==(const Call&, const Call&) = default;
};

class World final : public detail::ControllerWorld,
                    public detail::CategoryIconWorld,
                    public detail::DispatcherWorld {
 public:
  std::vector<Call> calls;
  std::function<void(const Call&)> on_call;
  std::function<void(bool)> on_triangle;
  std::function<void(bool)> on_cursor;
  std::int32_t ready = 1;
  std::uint32_t pressed = 0;
  std::int32_t next_page = 0;
  std::uint16_t selected_token = samples::kWideSlot;
  std::uint8_t gem_slots = VS::Items::kWeaponGemCount;
  bool armor_shows_dp = true;
  bool triangle = false;
  std::uint8_t category = 1;
  detail::DetailViewMode view = detail::DetailViewMode::kList;
  std::uint8_t icons_visible = 0, controls_visible = 0;
  std::uint8_t selected_element = samples::kFirstDetailElement;
  std::int32_t controller_result = 0;
  bool cursor_bright = false;
  std::size_t owner = 0;
  std::array<std::array<std::uint16_t, samples::kMaxOrder>, 2> order{};
  std::array<fw::MenuRow, 2> headers{};
  std::array<std::array<char, fw::kItemNameBytes>, 2> names{};
  std::array<std::array<char, fw::kStringBufferBytes>, 2> descriptions{};
  detail::DetailRowText borrowed{};
  std::optional<detail::ControllerEntry> received_entry;
  std::vector<detail::CategoryIcon> icons;
  std::array<char, 2> observed_text{};

  World() {
    for (auto& table : order) table.fill(samples::kWideSlot);
    names[0][0] = 'a';
    names[1][0] = 'b';
    descriptions[0][0] = 'c';
    descriptions[1][0] = 'd';
  }
  void Record(Event event, std::int32_t first = 0, std::int32_t second = 0,
              std::int32_t third = 0) {
    calls.push_back({event, {first, second, third}});
    if (on_call) on_call(calls.back());
  }
  std::int32_t RowsReady() override {
    Record(Event::kReady);
    return ready;
  }
  std::uint32_t PressedButtons() override { return pressed; }
  void BeginTransition(detail::DetailRow row) override {
    Record(Event::kBegin, row.index);
  }
  void InitializeLayout(detail::DetailRow row, std::int32_t pages) override {
    Record(Event::kLayout, row.index, pages);
  }
  detail::ContainerSlot CurrentItem(detail::DetailPage page) override {
    Record(Event::kCurrent, page.index);
    return {order.at(owner).at(static_cast<std::size_t>(page.index))};
  }
  void SetStatistics(detail::ContainerSlot item) override {
    Record(Event::kStats, item.one_based);
  }
  void OpenDpPpBars() override { Record(Event::kBars); }
  void OpenDpBar() override { Record(Event::kDp); }
  void CreateComponentRow(std::int32_t row, detail::ContainerSlot item,
                          bool slide_in) override {
    Record(Event::kComponent, row, item.one_based, slide_in ? 1 : 0);
  }
  void Exit(bool hide_dp_pp) override {
    Record(Event::kExit, hide_dp_pp ? 1 : 0);
  }
  void RememberTriangleClose(bool value) override {
    triangle = value;
    if (on_triangle) on_triangle(value);
  }
  detail::DetailPage PollPaging(detail::DetailPage page) override {
    Record(Event::kPoll, page.index);
    return {next_page};
  }
  detail::ContainerSlot SelectItem(detail::DetailPage page) override {
    Record(Event::kSelect, page.index);
    return {selected_token};
  }
  detail::DetailRowText FormatItem(detail::ContainerSlot item) override {
    const detail::DetailRowText text{names.at(owner), descriptions.at(owner),
                                     samples::kRowType};
    borrowed = text;
    Record(Event::kFormat, item.one_based);
    return text;
  }
  void ChangePage(detail::DetailRow row, const detail::DetailRowText& text,
                  detail::DetailPage page) override {
    observed_text = {text.name.front(), text.description.front()};
    Record(Event::kChange, row.index, page.index,
           static_cast<std::int32_t>(text.row_type));
  }
  std::uint8_t GripGemSlots(detail::ContainerSlot item) override {
    Record(Event::kSlots, item.one_based);
    return gem_slots;
  }
  void DrawGripGemSlotCount(std::uint8_t slots) override {
    Record(Event::kDrawSlots, slots);
  }
  bool SelectedArmorShowsDp(detail::ContainerSlot item) override {
    Record(Event::kArmorGate, item.one_based);
    return armor_shows_dp;
  }
  std::uint8_t CurrentCategory() override { return category; }
  void DrawListTabNavigation() override { Record(Event::kTabs, 0); }
  fw::MenuRow& CategoryHeader() override {
    auto& header = headers.at(owner);
    Record(Event::kHeader);
    return header;
  }
  void DrawIcon(detail::CategoryIcon icon) override {
    icons.push_back(icon);
    Record(Event::kIcon, icon.category,
           static_cast<std::int32_t>(fw::PackXy(icon.x, icon.y)),
           static_cast<std::int32_t>(
               fw::PackXy(samples::kIconPitch, samples::kIconPitch)));
  }
  void SetViewMode(detail::DetailViewMode mode) override { view = mode; }
  void ResetDetailCursor() override { Record(Event::kReset); }
  void SetCommand(detail::DetailCommand command) override {
    Record(Event::kCommand, static_cast<int>(command));
  }
  std::int32_t RunController(
      VS::Items::ItemCategory value,
      std::optional<detail::ControllerEntry> selection) override {
    received_entry = selection;
    Record(Event::kController, static_cast<int>(value));
    return controller_result;
  }
  bool ClosedWithTriangle() override { return triangle; }
  void SetIconsVisibility(std::uint8_t value) override {
    icons_visible = value;
  }
  void SetControlsVisibility(std::uint8_t value) override {
    controls_visible = value;
  }
  std::uint8_t SelectedDetailElement() override { return selected_element; }
  void SetCursorBright(bool bright) override {
    cursor_bright = bright;
    if (on_cursor) on_cursor(bright);
  }
  void DrawDetailTabNavigation() override { Record(Event::kTabs, 1); }
  void DrawItemDetails() override { Record(Event::kDetails); }
  [[noreturn]] void Refuse(std::string_view reason) override {
    throw std::out_of_range(std::string(reason));
  }
};

using Controller = std::int32_t (*)(detail::ControllerState&,
                                    detail::ControllerWorld&,
                                    std::optional<detail::ControllerEntry>);
}  // namespace container_detail_controllers_tests
