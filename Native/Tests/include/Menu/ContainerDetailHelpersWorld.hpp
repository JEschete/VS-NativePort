#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Input/Pad.hpp"
#include "Menu/Container/ContainerDetailHelpers.hpp"
#include "Menu/ContainerDetailHelpersSamples.hpp"

namespace container_detail_helpers_tests {
namespace detail = VS::Menu::container;
namespace fw = VS::Menu::Framework;
enum class Event {
  kCount = 1,
  kInvalid,
  kClear,
  kRow,
  kContinue,
  kPages,
  kPrimary,
  kLeave,
  kDismiss,
  kBars
};
using Call = std::pair<Event, std::int32_t>;

class World final : public detail::DetailsWorld {
 public:
  std::uint32_t held = 0, repeat = 0, pressed = 0;
  std::int32_t count = 0;
  detail::PanelVisibility panels{samples::kTitle, samples::kIcons,
                                 samples::kControls};
  std::uint8_t phase = 0;
  std::int32_t triangle = 0;
  detail::DetailCursor cursor{};
  std::array<std::uint16_t, samples::kMaxOrderRows> order{};
  std::size_t order_size = order.size();
  std::array<std::array<fw::MenuRow, fw::kRowCount>, 2> rows{};
  std::size_t owner = 0;
  std::vector<Call> calls;
  std::function<void(Event, std::int32_t)> on_call;
  std::function<void(detail::DetailCursor)> on_remember;
  std::function<std::span<const std::uint16_t>()> on_order;

  void Record(Event event, std::int32_t value = 0) {
    calls.emplace_back(event, value);
    if (on_call) on_call(event, value);
  }
  std::uint32_t HeldButtons() override { return held; }
  std::uint32_t RepeatButtons() override { return repeat; }
  std::uint32_t PressedButtons() override { return pressed; }
  std::int32_t ItemCount() override {
    Record(Event::kCount);
    return count;
  }
  void RememberPage(detail::DetailCursor memory) override {
    cursor = memory;
    if (on_remember) on_remember(memory);
  }
  std::span<const std::uint16_t> DisplayOrder() override {
    if (on_order) return on_order();
    return std::span(order).first(order_size);
  }
  [[noreturn]] void Refuse(std::string_view reason) override {
    throw std::out_of_range(std::string(reason));
  }
  void ClearRows() override { Record(Event::kClear, fw::kNoRow); }
  fw::MenuRow& Row(detail::DetailRow row) override {
    auto& result = rows.at(owner).at(static_cast<std::size_t>(row.index));
    Record(Event::kRow, row.index);
    return result;
  }
  void SetIconsVisibility(std::uint8_t visibility) override {
    panels.icons = visibility;
  }
  void SetControlsVisibility(std::uint8_t visibility) override {
    panels.controls = visibility;
  }
  void ContinueTransition(detail::DetailRow row) override {
    Record(Event::kContinue, row.index);
  }
  void ShowStatPages(std::int32_t pages) override {
    Record(Event::kPages, pages);
  }
  void MovePrimaryStats(detail::PrimaryStatsMotion motion) override {
    Record(Event::kPrimary, static_cast<int>(motion));
  }
  void PlayLeaveSound() override { Record(Event::kLeave); }
  void PlayInvalidSound() override { Record(Event::kInvalid); }
  void DismissCommandRow() override { Record(Event::kDismiss, -1); }
  void SetPhase(detail::DetailPhase value) override {
    phase = static_cast<std::uint8_t>(value);
  }
  void RememberTriangleClose(bool value) override {
    triangle = value ? VS::Input::Triangle : 0;
  }
  void HideDpPpBars() override { Record(Event::kBars, samples::kBarsOut); }
};

class Records final : public VS::Items::ContainerWorld {
 public:
  VS::Items::ContainerStorage storage;
  std::vector<std::pair<VS::Items::ItemCategory, std::size_t>> reads;
  bool wrong_type = false;
  std::uint16_t Capacity(VS::Items::ItemCategory category) override {
    return storage.Capacity(category);
  }
  Record Read(VS::Items::ItemCategory category, std::size_t index) override {
    reads.emplace_back(category, index);
    if (wrong_type) return VS::Items::InventoryMisc{};
    return storage.Read(category, index);
  }
  void Write(VS::Items::ItemCategory category, std::size_t index,
             const Record& item) override {
    storage.Write(category, index, item);
  }
  [[noreturn]] void Refuse(std::string_view reason) override {
    throw std::out_of_range(std::string(reason));
  }
};
}  // namespace container_detail_helpers_tests
