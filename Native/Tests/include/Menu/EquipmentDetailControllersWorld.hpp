#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <stdexcept>
#include <vector>

#include "Menu/Inventory/EquipmentDetailControllers.hpp"
#include "Menu/InventoryDetailHelpersWorld.hpp"

namespace equipment_detail_tests {
namespace inventory = VS::Menu::Inventory;
enum class Event {
  kBegin,
  kLayout,
  kCurrent,
  kStats,
  kBars,
  kComponent,
  kExit,
  kPoll,
  kSelect,
  kFormat,
  kPage,
  kSlots,
  kDraw,
};
struct Call {
  Event event;
  int first = 0;
  int second = 0;
  int third = 0;
  friend bool operator==(Call, Call) = default;
};

class World final : public inventory::EquipmentDetailWorld {
 public:
  inventory_detail_tests::World policies;
  std::array<std::uint8_t, 16> order{1, 2,  3,  4,  5,  6,  7,  8,
                                     9, 10, 11, 12, 13, 14, 15, 16};
  std::array<std::uint8_t, 16> slots{};
  std::array<char, 3> name{'N', '\0', '\0'};
  std::array<char, 3> description{'D', '\0', '\0'};
  std::vector<Call> calls;
  std::function<void(Call)> on_call;
  std::int32_t ready = 1;
  int ready_reads = 0;
  std::uint32_t row_type = 0x58030000;
  std::optional<inventory::DetailPage> forced_page;

  World() { policies.displayed = order; }

  void Record(Call call) {
    calls.push_back(call);
    if (on_call) {
      on_call(call);
    }
  }
  std::int32_t RowsReady() override {
    ++ready_reads;
    return ready;
  }
  std::uint32_t PressedButtons() override { return policies.pressed; }
  void BeginTransition(inventory::DetailRowId row) override {
    Record({Event::kBegin, row.index});
    inventory::BeginDetailsTransition(policies, row);
  }
  void InitializeLayout(inventory::DetailRowId row,
                        inventory::DetailStatPages pages) override {
    Record({Event::kLayout, row.index, static_cast<int>(pages)});
    inventory::InitializeDetailsLayout(policies, row, static_cast<int>(pages));
  }
  inventory::InventorySlot CurrentItem(inventory::DetailPage page) override {
    Record({Event::kCurrent, page.index});
    return inventory::InventorySlot{
        order.at(static_cast<std::size_t>(page.index))};
  }
  void SetStatistics(inventory::InventorySlot slot) override {
    Record({Event::kStats, slot.one_based});
  }
  void OpenDpPpBars() override { Record({Event::kBars, 3}); }
  void CreateComponentRow(std::int32_t row, inventory::InventorySlot slot,
                          bool slide_in) override {
    Record({Event::kComponent, row, slot.one_based, slide_in ? 1 : 0});
  }
  void Exit(bool clear_dp_pp) override {
    Record({Event::kExit, clear_dp_pp ? 1 : 0});
    inventory::ExitDetailsView(policies, clear_dp_pp);
  }
  inventory::DetailPage PollPaging(inventory::DetailPage page) override {
    Record({Event::kPoll, page.index});
    if (forced_page) {
      return forced_page.value();
    }
    return inventory::PollDetailsPaging(policies, page);
  }
  inventory::InventorySlot SelectItem(inventory::DetailPage page) override {
    Record({Event::kSelect, page.index});
    return inventory::SelectDetailsItem(policies, page);
  }
  inventory::DetailRowView FormatItem(inventory::InventorySlot slot) override {
    Record({Event::kFormat, slot.one_based});
    return {.name = name, .description = description, .row_type = row_type};
  }
  void ChangePage(inventory::DetailRowId row,
                  const inventory::DetailRowView& text,
                  inventory::DetailPage page) override {
    if (text.name.data() != name.data() ||
        text.description.data() != description.data() ||
        text.row_type != row_type) {
      throw std::logic_error(
          "controller lost the formatter's borrowed row view");
    }
    Record({Event::kPage, row.index, page.index});
  }
  std::uint8_t GripGemSlots(inventory::InventorySlot slot) override {
    Record({Event::kSlots, slot.one_based});
    return slots.at(static_cast<std::size_t>(slot.one_based - 1));
  }
  void DrawGripGemSlotCount(std::uint8_t count) override {
    Record({Event::kDraw, count});
  }
};
}  // namespace equipment_detail_tests
