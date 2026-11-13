#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "Input/Pad.hpp"
#include "Items/ItemRecords.hpp"
#include "Menu/Inventory/EquipmentDetailControllers.hpp"

namespace {
namespace inventory = VS::Menu::Inventory;
namespace fw = VS::Menu::Framework;
using Byte = std::uint8_t;
constexpr std::size_t kRowBytes = 64;
constexpr std::size_t kMemoryBytes = 0xF2;
constexpr std::array<std::size_t, 4> kCapacities{8, 16, 16, 8};
constexpr char kBorrowed[] = "borrowed";
enum Operation {
  kReady = 1,
  kCount,
  kClear,
  kRow,
  kContinue,
  kPages,
  kPrimary,
  kLeave,
  kInvalid,
  kDismiss,
  kBars,
  kStats,
  kComponent,
  kFormat,
  kChange,
  kIcon,
  kText
};

std::int64_t Read() {
  std::int64_t value = 0;
  if (!(std::cin >> value))
    std::exit(2);
  return value;
}

std::uint32_t Word(const std::array<Byte, kRowBytes> &bytes, std::size_t offset,
                   std::size_t size) {
  std::uint32_t value = 0;
  for (std::size_t i = 0; i < size; ++i) {
    value |= std::uint32_t{bytes[offset + i]} << (i * 8);
  }
  return value;
}

void Put(std::array<Byte, kRowBytes> &bytes, std::size_t offset,
         std::uint32_t value, std::size_t size) {
  for (std::size_t i = 0; i < size; ++i) {
    bytes[offset + i] = static_cast<Byte>(value >> (i * 8));
  }
}

struct RowImage {
  std::array<Byte, kRowBytes> original{};
  fw::MenuRow native{};

  RowImage() {
    original.fill(0xA5);
    native = {.state = static_cast<fw::RowState>(original[0]),
              .w = original[1],
              .background_width = original[2],
              .animation_step = original[3],
              .gradient_state = original[4],
              .is_scrollable = original[5],
              .selected = original[6],
              .unselectable = original[7],
              .button_id = original[8],
              .row_icon = original[9],
              .font_color = original[10],
              .invert_gradient = original[11],
              .material = original[12],
              .outset_icon = original[13],
              .item_page = static_cast<std::uint16_t>(Word(original, 14, 2)),
              .count = static_cast<std::uint16_t>(Word(original, 16, 2)),
              .max = static_cast<std::uint16_t>(Word(original, 18, 2)),
              .x = std::bit_cast<std::int16_t>(std::uint16_t{0xA5A5}),
              .y = std::bit_cast<std::int16_t>(std::uint16_t{0xA5A5}),
              .target0 = std::bit_cast<std::int16_t>(std::uint16_t{0xA5A5}),
              .target1 = std::bit_cast<std::int16_t>(std::uint16_t{0xA5A5}),
              .sub_text = kBorrowed};
    native.text.fill(0xA5);
  }
  void Print() const {
    auto bytes = original;
    const std::array<Byte, 14> head{static_cast<Byte>(native.state),
                                    native.w,
                                    native.background_width,
                                    native.animation_step,
                                    native.gradient_state,
                                    native.is_scrollable,
                                    native.selected,
                                    native.unselectable,
                                    native.button_id,
                                    native.row_icon,
                                    native.font_color,
                                    native.invert_gradient,
                                    native.material,
                                    native.outset_icon};
    for (std::size_t i = 0; i < head.size(); ++i)
      bytes[i] = head[i];
    Put(bytes, 14, native.item_page, 2);
    Put(bytes, 16, native.count, 2);
    Put(bytes, 18, native.max, 2);
    Put(bytes, 20, static_cast<std::uint16_t>(native.x), 2);
    Put(bytes, 22, static_cast<std::uint16_t>(native.y), 2);
    Put(bytes, 24, static_cast<std::uint16_t>(native.target0), 2);
    Put(bytes, 26, static_cast<std::uint16_t>(native.target1), 2);
    for (std::size_t i = 0; i < native.text.size(); ++i)
      bytes[28 + i] = native.text[i];
    Put(bytes, 60, native.sub_text == nullptr ? 0 : Word(original, 60, 4), 4);
    for (const auto byte : bytes)
      std::cout << ' ' << static_cast<unsigned>(byte);
  }
};

class World final : public inventory::EquipmentDetailWorld,
                    public inventory::DetailsWorld {
public:
  inventory::EquipmentDetailState state{};
  int kind = 0;
  std::int32_t command = 0, count = 0, ready = 0;
  std::uint32_t held = 0, repeat = 0, pressed = 0, after_repeat = 0,
                after_pressed = 0;
  int mode = 0, initial_page = 0, buffer_owner = 0, changed_page = -1;
  inventory::PanelVisibility panels{};
  std::int32_t preview = 0, return_intent = 0;
  Byte header = 0, glyph = 0, initial_name = 0, initial_description = 0;
  std::array<char, 2> name{}, description{};
  std::uint32_t row_type = 0;
  std::array<Byte, 16> order{}, selected_order{};
  std::array<VS::Items::InventoryGrip, 16> grips{};
  std::array<Byte, kMemoryBytes> memory{};
  std::array<RowImage, fw::kRowCount> rows{};
  std::vector<std::array<std::int64_t, 16>> calls;

  void ReadInput(int category) {
    kind = category;
    command = static_cast<std::int32_t>(Read());
    state.stage = static_cast<inventory::DetailStage>(Read());
    state.animation_step = static_cast<Byte>(Read());
    state.selected_row = static_cast<Byte>(Read());
    state.selected_page = static_cast<Byte>(Read());
    state.unused_entry_byte = static_cast<Byte>(Read());
    initial_page = state.selected_page;
    count = static_cast<std::int32_t>(Read());
    held = static_cast<std::uint32_t>(Read());
    repeat = static_cast<std::uint32_t>(Read());
    pressed = static_cast<std::uint32_t>(Read());
    after_repeat = static_cast<std::uint32_t>(Read());
    after_pressed = static_cast<std::uint32_t>(Read());
    ready = static_cast<std::int32_t>(Read());
    mode = static_cast<int>(Read());
    panels.secondary_title = static_cast<Byte>(Read());
    panels.category_icons = static_cast<Byte>(Read());
    panels.status_command = static_cast<Byte>(Read());
    preview = static_cast<std::int32_t>(Read());
    header = static_cast<Byte>(Read());
    return_intent = static_cast<std::int32_t>(Read());
    const auto capacity = kCapacities.at(static_cast<std::size_t>(kind));
    for (std::size_t i = 0; i < capacity; ++i)
      order[i] = static_cast<Byte>(Read());
    for (std::size_t i = 0; i < capacity; ++i)
      selected_order[i] = static_cast<Byte>(Read());
    for (auto &grip : grips)
      grip.gem_slots = static_cast<Byte>(Read());
    row_type = static_cast<std::uint32_t>(Read());
    initial_name = static_cast<Byte>(Read());
    initial_description = static_cast<Byte>(Read());
    name[0] = static_cast<char>(initial_name);
    description[0] = static_cast<char>(initial_description);
    glyph = static_cast<Byte>(Read());
    for (auto &byte : memory)
      byte = static_cast<Byte>(Read());
  }

  void Record(int op, std::int64_t a = 0, std::int64_t b = 0,
              std::int64_t c = 0, std::int64_t d = 0) {
    calls.push_back({op, a, b, c, d, static_cast<Byte>(state.stage),
                     state.animation_step, state.selected_row,
                     state.selected_page, state.unused_entry_byte,
                     panels.secondary_title, panels.category_icons,
                     panels.status_command, preview, header, return_intent});
  }
  std::int32_t RowsReady() override {
    Record(kReady);
    if (mode == 2) {
      state.selected_row = 20;
      state.selected_page = static_cast<Byte>((initial_page + 1) % count);
    }
    return ready;
  }
  std::uint32_t PressedButtons() override { return pressed; }
  void BeginTransition(inventory::DetailRowId row) override {
    inventory::BeginDetailsTransition(*this, row);
  }
  void InitializeLayout(inventory::DetailRowId row,
                        inventory::DetailStatPages pages) override {
    inventory::InitializeDetailsLayout(*this, row, static_cast<int>(pages));
  }
  inventory::InventorySlot CurrentItem(inventory::DetailPage page) override {
    if (page.index < 0 || static_cast<std::size_t>(page.index) >=
                              kCapacities.at(static_cast<std::size_t>(kind))) {
      Refuse("current page outside its typed inventory view");
    }
    return inventory::InventorySlot{
        order.at(static_cast<std::size_t>(page.index))};
  }
  void SetStatistics(inventory::InventorySlot slot) override {
    Record(kStats, slot.one_based);
    if (mode == 4)
      state.selected_row = 31;
    if (mode == 7) {
      name[0] = static_cast<char>(initial_name + 2);
      description[0] = static_cast<char>(initial_description + 3);
      buffer_owner = 1;
    }
  }
  void OpenDpPpBars() override { Record(kBars, 3); }
  void CreateComponentRow(std::int32_t row, inventory::InventorySlot slot,
                          bool slide_in) override {
    Record(kComponent, row, slot.one_based, slide_in ? 1 : 0);
    if (mode == 5) {
      const auto at = static_cast<std::size_t>(changed_page < 0 ? initial_page
                                                                : changed_page);
      order.at(at) = static_cast<Byte>(
          order.at(at) % kCapacities.at(static_cast<std::size_t>(kind)) + 1);
    }
  }
  void Exit(bool clear_dp_pp) override {
    inventory::ExitDetailsView(*this, clear_dp_pp);
  }
  inventory::DetailPage PollPaging(inventory::DetailPage page) override {
    return inventory::PollDetailsPaging(*this, page);
  }
  inventory::InventorySlot SelectItem(inventory::DetailPage page) override {
    return inventory::SelectDetailsItem(*this, page);
  }
  inventory::DetailRowView FormatItem(inventory::InventorySlot slot) override {
    Record(kFormat, slot.one_based, buffer_owner);
    return {.name = name, .description = description, .row_type = row_type};
  }
  void ChangePage(inventory::DetailRowId row,
                  const inventory::DetailRowView &text,
                  inventory::DetailPage page) override {
    const auto first = static_cast<Byte>(text.name.front());
    const auto second = static_cast<Byte>(text.description.front());
    Record(kChange, row.index, text.row_type, page.index, first << 8 | second);
    changed_page = page.index;
  }
  std::uint8_t GripGemSlots(inventory::InventorySlot slot) override {
    return grips.at(static_cast<std::size_t>(slot.one_based - 1)).gem_slots;
  }
  void DrawGripGemSlotCount(std::uint8_t slots) override {
    glyph = static_cast<Byte>(slots + 0x30);
    Record(kIcon, 0x116, 256, 32, glyph);
    Record(kText, glyph, fw::PackXy(280, 36));
  }

  std::uint32_t HeldButtons() override { return held; }
  std::uint32_t RepeatButtons() override { return repeat; }
  std::int32_t ItemCount() override {
    Record(kCount, kind);
    if (mode == 1) {
      repeat = after_repeat;
      pressed = after_pressed;
    }
    return count;
  }
  void RememberPage(inventory::CursorMemory cursor) override {
    const auto at = 0x3C + static_cast<std::size_t>(kind) * 2;
    memory.at(at) = cursor.row;
    memory.at(at + 1) = cursor.page;
  }
  std::span<const Byte> DisplayOrder() override {
    return {selected_order.data(),
            kCapacities.at(static_cast<std::size_t>(kind))};
  }
  [[noreturn]] void Refuse(std::string_view reason) override {
    std::cerr << reason;
    std::exit(3);
  }
  void ClearRows() override { Record(kClear, 40); }
  fw::MenuRow &Row(inventory::DetailRowId row) override {
    Record(kRow, row.index);
    return rows.at(static_cast<std::size_t>(row.index)).native;
  }
  void SetPreviewLayout(inventory::PreviewLayout layout) override {
    preview = static_cast<int>(layout);
  }
  void SetCategoryVisibility(Byte visibility) override {
    panels.category_icons = visibility;
  }
  void SetCommandVisibility(Byte visibility) override {
    panels.status_command = visibility;
  }
  void ContinueTransition(inventory::DetailRowId row) override {
    Record(kContinue, row.index);
    if (mode == 3) {
      auto &current = order.at(state.selected_page);
      current = static_cast<Byte>(
          current % kCapacities.at(static_cast<std::size_t>(kind)) + 1);
    }
  }
  void ShowStatPages(std::int32_t pages) override { Record(kPages, pages); }
  void MovePrimaryStats(inventory::StatPanelMotion motion) override {
    Record(kPrimary, static_cast<int>(motion));
    if (mode == 6) {
      pressed = after_pressed;
      state.selected_page = static_cast<Byte>((initial_page + 1) % count);
    }
  }
  void PlayLeaveSound() override { Record(kLeave); }
  void PlayInvalidSound() override { Record(kInvalid); }
  void DismissCommandRow() override { Record(kDismiss, -1); }
  void
  SetHeaderPlacement(inventory::CategoryHeaderPlacement placement) override {
    header = static_cast<Byte>(placement);
  }
  void SetBattleReturnIntent(inventory::BattleReturnIntent intent) override {
    return_intent = intent == inventory::BattleReturnIntent::kBattle
                        ? VS::Input::Triangle
                        : 0;
  }
  void HideDpPpBars() override { Record(kBars, 4); }

  void Print(std::int32_t result) const {
    std::cout << result << ' ' << static_cast<unsigned>(state.stage) << ' '
              << static_cast<unsigned>(state.animation_step) << ' '
              << static_cast<unsigned>(state.selected_row) << ' '
              << static_cast<unsigned>(state.selected_page) << ' '
              << static_cast<unsigned>(state.unused_entry_byte) << ' '
              << static_cast<unsigned>(panels.secondary_title) << ' '
              << static_cast<unsigned>(panels.category_icons) << ' '
              << static_cast<unsigned>(panels.status_command) << ' ' << preview
              << ' ' << static_cast<unsigned>(header) << ' ' << return_intent
              << ' ' << static_cast<unsigned>(glyph);
    for (const auto byte : memory)
      std::cout << ' ' << static_cast<unsigned>(byte);
    for (const auto &row : rows)
      row.Print();
    std::cout << ' ' << calls.size();
    for (const auto &call : calls)
      for (const auto value : call)
        std::cout << ' ' << value;
    std::cout << '\n';
  }
};

void Execute(World &world) {
  std::optional<inventory::DetailSelection> selection;
  if (world.command != 0) {
    const auto command = static_cast<std::uint32_t>(world.command);
    selection = inventory::DetailSelection{
        inventory::DetailRowId{static_cast<Byte>(command >> 8)},
        inventory::DetailPage{static_cast<Byte>(command - 1)}};
  }
  std::int32_t result = 0;
  switch (world.kind) {
  case 0:
    result = inventory::UpdateWeaponDetails(world.state, world, selection);
    break;
  case 1:
    result = inventory::UpdateBladeDetails(world.state, world, selection);
    break;
  case 2:
    result = inventory::UpdateGripDetails(world.state, world, selection);
    break;
  case 3:
    result = inventory::UpdateShieldDetails(world.state, world, selection);
    break;
  default:
    std::exit(2);
  }
  world.Print(result);
}
} // namespace

int main() {
  std::int64_t kind = 0;
  while (std::cin >> kind) {
    World world;
    world.ReadInput(static_cast<int>(kind));
    Execute(world);
  }
}
