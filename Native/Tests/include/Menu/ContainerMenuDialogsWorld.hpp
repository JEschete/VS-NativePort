#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "Menu/Container/ContainerMenuDialogs.hpp"
#include "Menu/Framework/ItemRows.hpp"

namespace container_dialog_tests {
namespace dialogs = VS::Menu::container;
namespace framework = VS::Menu::Framework;
namespace items = VS::Items;
inline constexpr std::uint8_t kListRowsBuilt = 16;
inline constexpr std::size_t kContainerTextBytes = 1380;
inline constexpr std::size_t kHelpEntries = 700;
inline constexpr std::size_t kHelpTextBase = 800;
inline constexpr std::size_t kHelpEntryStride = 4;
inline constexpr std::size_t kContainerEntries = 300;
inline constexpr std::size_t kContainerTextBase = 400;
inline constexpr std::size_t kContainerTextSpread = 200;
inline constexpr std::int32_t kQuantityRow = 34;
inline constexpr std::int32_t kQuantityY = 130;
inline constexpr std::int32_t kQuantityWidth = 126;
inline constexpr std::int32_t kQuantityFont = 5;
inline constexpr std::uint8_t kTransferControls = 5;

enum class Operation : std::int32_t {
  kReady = 1,
  kInitializeInformation,
  kInformationDone,
  kSetInformation,
  kCreateActions,
  kActionText,
  kUpdateActions,
  kActionResult,
  kSound,
  kSort,
  kQuantityRow,
  kTextWord,
  kFont,
  kDigit,
  kCursor,
  kPrepareControls,
  kFlyOut,
  kClearRows,
  kHidePanels,
  kAdvanceRows,
  kRow,
};

struct Event {
  Operation operation;
  std::int32_t first = 0;
  std::int32_t second = 0;
  std::int32_t third = 0;
  friend bool operator==(const Event&, const Event&) = default;
};

struct DialogWorld : dialogs::MenuDialogWorld, dialogs::ItemRowRestoreWorld {
  std::int32_t ready = 1;
  std::int32_t action = -1;
  std::int32_t cursor_result = 0;
  bool information_done = true;
  std::uint32_t pressed = 0;
  std::uint32_t repeated = 0;
  std::uint8_t direction = 0;
  std::uint8_t list_row = kListRowsBuilt;
  std::uint8_t controls = 0;
  std::uint8_t frozen_tabs = 1;
  std::array<framework::MenuRow, framework::kRowCount> rows{};
  std::vector<std::uint8_t> memory =
      std::vector<std::uint8_t>(framework::kMenuMemoryBytes);
  std::vector<std::uint8_t> help = std::vector<std::uint8_t>(
      framework::kItemHelpBytes, VS::Render::kTextTerminator);
  std::vector<std::uint8_t> container = std::vector<std::uint8_t>(
      kContainerTextBytes, VS::Render::kTextTerminator);
  items::ContainerStorage first_owner;
  items::ContainerStorage second_owner;
  items::ContainerWorld* active = &first_owner;
  std::vector<Event> events;
  std::function<void(Operation)> callback;
  std::vector<framework::RowText> actions;

  DialogWorld() {
    for (std::size_t i = 0; i < kHelpEntries; ++i) {
      SetOffset(help, i, kHelpTextBase + i * kHelpEntryStride);
    }
    for (std::size_t i = 0; i < kContainerEntries; ++i) {
      SetOffset(container, i, kContainerTextBase + i % kContainerTextSpread);
    }
  }
  static void SetOffset(std::span<std::uint8_t> table, std::size_t index,
                        std::size_t halfword) {
    table[index * 2] = static_cast<std::uint8_t>(halfword);
    table[index * 2 + 1] =
        static_cast<std::uint8_t>(halfword >> framework::kByteShift);
  }
  void Record(Operation operation, std::int32_t first = 0,
              std::int32_t second = 0, std::int32_t third = 0) {
    events.push_back({operation, first, second, third});
    if (callback) callback(operation);
  }
  std::int32_t TextIdentity(std::span<const std::uint8_t> text) const {
    for (std::size_t i = 0; i < help.size(); ++i) {
      if (text.data() == help.data() + i) return static_cast<std::int32_t>(i);
    }
    for (std::size_t i = 0; i < container.size(); ++i) {
      if (text.data() == container.data() + i) {
        return -static_cast<std::int32_t>(i) - 1;
      }
    }
    throw std::out_of_range("dialog text outside the real borrowed inputs");
  }
  std::int32_t RowsReady() override {
    Record(Operation::kReady);
    return ready;
  }
  void InitializeInformationBox() override {
    Record(Operation::kInitializeInformation, 1);
  }
  bool InformationDone() override { return information_done; }
  std::span<const std::uint8_t> HelpText() override { return help; }
  std::span<const std::uint8_t> ContainerText() override { return container; }
  void SetInformation(std::span<const std::uint8_t> text) override {
    Record(Operation::kSetInformation, TextIdentity(text));
  }
  void CreateActions(std::int32_t menu,
                     std::span<const framework::RowText> text) override {
    actions.assign(text.begin(), text.end());
    Record(Operation::kCreateActions, menu,
           static_cast<std::int32_t>(text.size()));
    for (const auto& row : text) {
      Record(Operation::kActionText, TextIdentity(row.name),
             TextIdentity(row.description));
    }
  }
  void UpdateActions() override { Record(Operation::kUpdateActions); }
  std::int32_t ActionResult() override {
    Record(Operation::kActionResult);
    return action;
  }
  void PlaySound(dialogs::DialogSound sound) override {
    Record(Operation::kSound, static_cast<std::int32_t>(sound));
  }
  void Sort(items::ItemCategory category, std::int32_t stat) override {
    Record(Operation::kSort, static_cast<std::int32_t>(category), stat);
  }
  items::ContainerWorld& ActiveRecords() override { return *active; }
  framework::MenuRow& QuantityRow(std::span<const std::uint8_t> text,
                                  std::int32_t x) override {
    Record(Operation::kQuantityRow, x, kQuantityRow,
           static_cast<std::int32_t>(
               framework::PackXy(kQuantityWidth, kQuantityY)));
    for (std::size_t i = 0; i < text.size(); i += sizeof(std::uint32_t)) {
      std::uint32_t word = 0;
      for (std::size_t j = 0; j < sizeof(word) && i + j < text.size(); ++j) {
        word |= static_cast<std::uint32_t>(text[i + j])
                << (j * framework::kByteShift);
      }
      Record(Operation::kTextWord, static_cast<std::int32_t>(word));
    }
    return rows[kQuantityRow];
  }
  std::uint32_t PressedButtons() override { return pressed; }
  std::uint32_t RepeatedButtons() override { return repeated; }
  void SetQuantityFont() override { Record(Operation::kFont, kQuantityFont); }
  void DrawQuantityDigit(std::uint32_t digit, std::int32_t x) override {
    Record(Operation::kDigit, static_cast<std::int32_t>(digit), x, kQuantityY);
  }
  std::int32_t DrawQuantityCursor(std::uint8_t animation,
                                  std::int32_t position) override {
    Record(Operation::kCursor, animation, position);
    return cursor_result;
  }
  std::uint8_t TransferDirection() override { return direction; }
  void PrepareTransferControls() override {
    controls = kTransferControls;
    frozen_tabs = 0;
  }
  void FlyOutAction(std::int32_t row) override {
    Record(Operation::kFlyOut, row);
  }
  void ClearTransferRows() override { Record(Operation::kClearRows); }
  void HideTransferPanels() override { Record(Operation::kHidePanels); }
  std::uint8_t ListAnimationRow() override { return list_row; }
  void AdvanceListRows() override {
    Record(Operation::kAdvanceRows);
    ++list_row;
  }
  framework::MenuRow& Row(std::int32_t index) override {
    Record(Operation::kRow, index);
    return rows.at(static_cast<std::size_t>(index));
  }
  std::span<const std::uint8_t> CursorMemory() override { return memory; }
  [[noreturn]] void Refuse(std::string_view reason) override {
    throw std::out_of_range(std::string(reason));
  }
  std::size_t Count(Operation operation) const {
    return static_cast<std::size_t>(
        std::ranges::count(events, operation, &Event::operation));
  }
};
}  // namespace container_dialog_tests
