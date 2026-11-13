#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "Battle/FloatingLabelQueue.hpp"

namespace VS::test_support::floating_labels {
namespace b = Battle;

struct Record {
  std::array<std::int16_t, b::kFloatingLabelAxes> position{};
  std::int16_t waiting_steps = 0;
  std::uint8_t kind = 0;
  std::uint8_t actor = 0;
  std::uint8_t bone = 0;
  std::uint8_t elapsed = 0;
  std::uint16_t amount = 0;
  std::uint8_t style = 0;
  std::uint8_t length = 0;
  std::int16_t placement_x = 0;
  std::int16_t placement_y = 0;
  std::array<std::int8_t, 4> text{};
  bool operator==(const Record&) const = default;
};

class Label final : public b::FloatingLabelView {
 public:
  void Bind(Record& record) { record_ = &record; }
  [[nodiscard]] bool Inactive() const override { return record_->kind == 0; }
  b::FloatingLabelTimingView Timing() override {
    return {record_->waiting_steps, record_->elapsed};
  }
  void SetWorldLocked() override { record_->kind = 3; }
  void SetWorldCoordinate(std::size_t axis, std::int16_t value) override {
    record_->position.at(axis) = value;
  }

 private:
  Record* record_ = nullptr;
};

class Queue final : public b::FloatingLabelQueueView {
 public:
  std::array<Record, b::kFloatingLabelCapacity> records{};
  std::int16_t count = 0;
  std::int16_t visibility = 1;
  std::int16_t cue = 2;
  std::int16_t timer = 3;
  std::int16_t suppression = 4;
  bool owned = true;
  std::size_t copies = 0;
  [[nodiscard]] std::int16_t Count() const override { return count; }
  void SetCount(std::int16_t value) override { count = value; }
  b::FloatingLabelView* Label(b::FloatingLabelSlot slot) override {
    if (!Valid(slot)) return nullptr;
    const auto index = static_cast<std::size_t>(slot.index);
    auto& view = views_.at(index);
    view.Bind(records.at(index));
    return &view;
  }
  bool CopyLabel(b::FloatingLabelSlot destination,
                 b::FloatingLabelSlot source) override {
    if (!Valid(source) || !Valid(destination)) return false;
    records.at(static_cast<std::size_t>(destination.index)) =
        records.at(static_cast<std::size_t>(source.index));
    ++copies;
    return true;
  }
  bool ClearLabel(b::FloatingLabelSlot slot) override {
    if (!Valid(slot)) return false;
    records.at(static_cast<std::size_t>(slot.index)) = {};
    return true;
  }
  bool ClearAllLabels() override {
    if (!owned) return false;
    records.fill({});
    return true;
  }
  b::FloatingLabelResetView ResetView() {
    return {visibility, cue, timer, suppression};
  }
  void Populate() {
    for (std::size_t index = 0; index < records.size(); ++index) {
      records.at(index) = {{11, -22, 33},
                           -3,
                           1,
                           static_cast<std::uint8_t>(index),
                           251,
                           17,
                           static_cast<std::uint16_t>(index + 500),
                           128,
                           3,
                           -19,
                           20,
                           {1, 2, 3, 4}};
    }
  }

 private:
  [[nodiscard]] bool Valid(b::FloatingLabelSlot slot) const {
    return owned && slot.index >= 0 && slot.index < b::kFloatingLabelCapacity;
  }
  std::array<floating_labels::Label, b::kFloatingLabelCapacity> views_{};
};
}  // namespace VS::test_support::floating_labels
