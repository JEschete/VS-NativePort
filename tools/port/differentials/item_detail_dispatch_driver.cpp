#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "Core/Math/ScalarMath.hpp"
#include "Input/Pad.hpp"
#include "Items/Inventory.hpp"
#include "Menu/Inventory/ItemDetailDispatch.hpp"
#include "Render/Sprites.hpp"

namespace {
namespace inv = VS::Menu::Inventory;
namespace fw = VS::Menu::Framework;
namespace render = VS::Render;
using Byte = std::uint8_t;
constexpr std::size_t kRowBytes = 64;
constexpr std::size_t kMemoryBytes = 0xF2;
constexpr std::size_t kStateBytes = 28;
constexpr std::array<std::size_t, 6> kCapacities{8, 16, 16, 8, 16, 48};
constexpr std::array<std::size_t, 6> kOffsets{0, 5, 9, 14, 19, 23};
constexpr std::uint32_t kPrimitive = 0x80900000;
constexpr char kBorrowed[] = "borrowed";
enum Operation {
  kReady = 1,
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
  kText,
  kResetCursor,
  kCommand,
  kTabs,
  kDrawDetails
};

std::int64_t Read() {
  std::int64_t value = 0;
  if (!(std::cin >> value)) {
    std::exit(2);
  }
  return value;
}

std::uint32_t Word(const std::array<Byte, kRowBytes> &bytes, std::size_t offset,
                   std::size_t size) {
  std::uint32_t result = 0;
  for (std::size_t index = 0; index < size; ++index) {
    result |= std::uint32_t{bytes.at(offset + index)} << (index * 8);
  }
  return result;
}

void Put(std::array<Byte, kRowBytes> &bytes, std::size_t offset,
         std::uint32_t value, std::size_t size) {
  for (std::size_t index = 0; index < size; ++index) {
    bytes.at(offset + index) = static_cast<Byte>(value >> (index * 8));
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
              .item_page = 0xA5A5,
              .count = 0xA5A5,
              .max = 0xA5A5,
              .x = VS::Core::Math::WrapSigned16(0xA5A5),
              .y = VS::Core::Math::WrapSigned16(0xA5A5),
              .target0 = VS::Core::Math::WrapSigned16(0xA5A5),
              .target1 = VS::Core::Math::WrapSigned16(0xA5A5),
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
    for (std::size_t index = 0; index < head.size(); ++index) {
      bytes[index] = head[index];
    }
    Put(bytes, 14, native.item_page, 2);
    Put(bytes, 16, native.count, 2);
    Put(bytes, 18, native.max, 2);
    Put(bytes, 20, static_cast<std::uint16_t>(native.x), 2);
    Put(bytes, 22, static_cast<std::uint16_t>(native.y), 2);
    Put(bytes, 24, static_cast<std::uint16_t>(native.target0), 2);
    Put(bytes, 26, static_cast<std::uint16_t>(native.target1), 2);
    for (std::size_t index = 0; index < native.text.size(); ++index) {
      bytes[28 + index] = native.text[index];
    }
    Put(bytes, 60, native.sub_text == nullptr ? 0 : Word(original, 60, 4), 4);
    for (const auto value : bytes) {
      std::cout << ' ' << static_cast<unsigned>(value);
    }
  }
};

class World final : public inv::EquipmentDetailWorld,
                    public inv::ItemDetailWorld,
                    public inv::DetailsWorld,
                    public inv::CategoryIconWorld,
                    public inv::ItemDetailDispatcherWorld {
public:
  std::array<inv::EquipmentDetailState, 6> states{};
  inv::ItemDetailDispatcherState dispatcher{};
  int kind = 0, active = 0, mode = 0, bank = 0, buffer_owner = 0;
  std::int32_t command = 0, ready = 0, preview = 0, return_intent = 0;
  std::int32_t animation = 0, highlight = 0, command_value = 0;
  std::uint32_t held = 0, repeat = 0, pressed = 0, after_pressed = 0;
  inv::PanelVisibility panels{};
  Byte header = 0, selected = 0, cursor = 0, glyph = 0;
  Byte initial_name = 0, initial_description = 0;
  std::uint32_t row_type = 0;
  std::array<int, 6> counts{};
  std::array<std::array<Byte, 48>, 6> direct{}, bound{};
  VS::Items::InventoryContents records{};
  std::array<Byte, kMemoryBytes> memory{};
  std::array<std::array<RowImage, fw::kRowCount>, 2> rows{};
  std::array<char, 2> name{}, description{};
  std::uint32_t primitive_cursor = kPrimitive;
  std::array<std::uint32_t, 2> chains{};
  std::vector<render::SpritePacket> sprites;
  std::vector<std::vector<std::int64_t>> calls;

  std::array<Byte, kStateBytes> StateImage() const {
    std::array<Byte, kStateBytes> image{};
    for (std::size_t category = 0; category < states.size(); ++category) {
      const auto &state = states[category];
      const auto offset = kOffsets[category];
      image[offset] = static_cast<Byte>(state.stage);
      image[offset + 1] = state.animation_step;
      image[offset + 2] = state.selected_row;
      image[offset + 3] = state.selected_page;
      if (category == 0 || category == 2 || category == 3) {
        image[offset + 4] = state.unused_entry_byte;
      }
    }
    image[27] = static_cast<Byte>(dispatcher.category);
    return image;
  }
  void AddValues(std::vector<std::int64_t> &values) const {
    for (const auto value : StateImage()) {
      values.push_back(value);
    }
    const std::array<std::int64_t, 9> fields{panels.secondary_title,
                                             panels.category_icons,
                                             panels.status_command,
                                             preview,
                                             header,
                                             return_intent,
                                             selected,
                                             cursor,
                                             command_value};
    values.insert(values.end(), fields.begin(), fields.end());
  }
  void Record(int operation, std::int64_t a = 0, std::int64_t b = 0,
              std::int64_t c = 0, std::int64_t d = 0) {
    std::vector<std::int64_t> values{operation, a, b, c, d};
    AddValues(values);
    calls.push_back(std::move(values));
  }
  inv::EquipmentDetailState &CurrentState() {
    return states.at(static_cast<std::size_t>(active));
  }
  std::size_t Capacity() const {
    return kCapacities.at(static_cast<std::size_t>(active));
  }
  void MoveCurrentPage() {
    auto &page = CurrentState().selected_page;
    page = static_cast<Byte>((page + 1) % counts.at(active));
  }
  void ChangeDirect() {
    auto &reference = direct.at(active).at(CurrentState().selected_page);
    reference = static_cast<Byte>(reference % Capacity() + 1);
  }
  void ReadInput(int input_kind) {
    kind = input_kind;
    active = static_cast<int>(Read());
    command = static_cast<std::int32_t>(Read());
    std::array<Byte, kStateBytes> image{};
    for (auto &byte : image) {
      byte = static_cast<Byte>(Read());
    }
    for (std::size_t category = 0; category < states.size(); ++category) {
      const auto offset = kOffsets[category];
      states[category] = {
          static_cast<inv::DetailStage>(image[offset]), image[offset + 1],
          image[offset + 2], image[offset + 3],
          static_cast<Byte>(category == 0 || category == 2 || category == 3
                                ? image[offset + 4]
                                : 0)};
    }
    dispatcher.category = static_cast<VS::Items::ItemCategory>(image[27]);
    for (auto &count : counts) {
      count = static_cast<int>(Read());
    }
    held = static_cast<std::uint32_t>(Read());
    repeat = static_cast<std::uint32_t>(Read());
    pressed = static_cast<std::uint32_t>(Read());
    after_pressed = static_cast<std::uint32_t>(Read());
    ready = static_cast<std::int32_t>(Read());
    mode = static_cast<int>(Read());
    panels.secondary_title = static_cast<Byte>(Read());
    panels.category_icons = static_cast<Byte>(Read());
    panels.status_command = static_cast<Byte>(Read());
    preview = static_cast<std::int32_t>(Read());
    header = static_cast<Byte>(Read());
    return_intent = static_cast<std::int32_t>(Read());
    for (auto &table : {std::ref(direct), std::ref(bound)}) {
      for (std::size_t category = 0; category < kCapacities.size();
           ++category) {
        for (std::size_t page = 0; page < kCapacities[category]; ++page) {
          table.get()[category][page] = static_cast<Byte>(Read());
        }
      }
    }
    for (auto &armor : records.armor) {
      armor.category = static_cast<Byte>(Read());
    }
    for (auto &grip : records.grips) {
      grip.gem_slots = static_cast<Byte>(Read());
    }
    for (int slot = 0; slot < counts[0]; ++slot) {
      records.weapons.at(slot).blade = 1;
    }
    for (int slot = 0; slot < counts[1]; ++slot) {
      records.blades.at(slot).id = 1;
    }
    for (int slot = 0; slot < counts[2]; ++slot) {
      records.grips.at(slot).id = 1;
    }
    for (int slot = 0; slot < counts[3]; ++slot) {
      records.shields.at(slot).base.id = 1;
    }
    for (int slot = 0; slot < counts[4]; ++slot) {
      records.armor.at(slot).id = 1;
    }
    for (int slot = 0; slot < counts[5]; ++slot) {
      records.gems.at(slot).id = 1;
    }
    row_type = static_cast<std::uint32_t>(Read());
    initial_name = static_cast<Byte>(Read());
    initial_description = static_cast<Byte>(Read());
    name[0] = static_cast<char>(initial_name);
    description[0] = static_cast<char>(initial_description);
    glyph = static_cast<Byte>(Read());
    for (auto &byte : memory) {
      byte = static_cast<Byte>(Read());
    }
    selected = static_cast<Byte>(Read());
    cursor = static_cast<Byte>(Read());
    command_value = static_cast<std::int32_t>(Read());
    animation = static_cast<std::int32_t>(Read());
    highlight = static_cast<std::int32_t>(Read());
    const auto header_x = static_cast<std::int32_t>(Read());
    for (auto &owner : rows) {
      owner[31].native.x = VS::Core::Math::WrapSigned16(header_x);
    }
    chains[0] = static_cast<std::uint32_t>(Read());
    chains[1] = static_cast<std::uint32_t>(Read());
  }
  std::int32_t RowsReady() override {
    Record(kReady);
    if (mode == 1) {
      CurrentState().selected_row = 20;
      MoveCurrentPage();
    }
    return ready;
  }
  std::uint32_t PressedButtons() override { return pressed; }
  std::uint32_t HeldButtons() override { return held; }
  std::uint32_t RepeatButtons() override { return repeat; }
  std::int32_t ItemCount() override {
    return VS::Items::Inventory::CountItems(
        static_cast<VS::Items::ItemCategory>(active), records);
  }
  void BeginTransition(inv::DetailRowId row) override {
    inv::BeginDetailsTransition(*this, row);
  }
  void InitializeLayout(inv::DetailRowId row,
                        inv::DetailStatPages pages) override {
    inv::InitializeDetailsLayout(*this, row, static_cast<int>(pages));
  }
  inv::InventorySlot CurrentItem(inv::DetailPage page) override {
    if (page.index < 0 || static_cast<std::size_t>(page.index) >= Capacity()) {
      Refuse("used direct page outside its actual inventory capacity");
    }
    return {direct.at(active).at(static_cast<std::size_t>(page.index))};
  }
  void SetStatistics(inv::InventorySlot slot) override {
    Record(kStats, slot.one_based, active);
    if (mode == 3) {
      CurrentState().selected_row = 31;
      ChangeDirect();
    }
    if (mode == 4) {
      name[0] = static_cast<char>(initial_name + 2);
      description[0] = static_cast<char>(initial_description + 3);
      buffer_owner = 1;
    }
  }
  bool ArmorShowsDp(inv::InventorySlot slot) override {
    return records.armor.at(slot.one_based - 1).category != 7;
  }
  void OpenDpBar() override { Record(kBars, 1); }
  void OpenDpPpBars() override { Record(kBars, 3); }
  void CreateComponentRow(std::int32_t row, inv::InventorySlot slot,
                          bool slide) override {
    Record(kComponent, row, slot.one_based, slide ? 1 : 0);
  }
  void Exit(bool clear_dp_pp) override {
    inv::ExitDetailsView(*this, clear_dp_pp);
  }
  inv::DetailPage PollPaging(inv::DetailPage page) override {
    return inv::PollDetailsPaging(*this, page);
  }
  inv::InventorySlot SelectItem(inv::DetailPage page) override {
    return inv::SelectDetailsItem(*this, page);
  }
  void RememberPage(inv::CursorMemory memory_of_page) override {
    const auto at = 0x3C + static_cast<std::size_t>(active) * 2;
    memory.at(at) = memory_of_page.row;
    memory.at(at + 1) = memory_of_page.page;
  }
  std::span<const Byte> DisplayOrder() override {
    return {bound.at(active).data(), Capacity()};
  }
  inv::DetailRowView FormatItem(inv::InventorySlot slot) override {
    Record(kFormat, slot.one_based, active, buffer_owner);
    if (mode == 6) {
      bound.at(active).at(CurrentState().selected_page) = 1;
    }
    return {.name = name, .description = description, .row_type = row_type};
  }
  void ChangePage(inv::DetailRowId row, const inv::DetailRowView &text,
                  inv::DetailPage page) override {
    const auto first = static_cast<Byte>(text.name.front());
    const auto second = static_cast<Byte>(text.description.front());
    Record(kChange, row.index, text.row_type, page.index, first << 8 | second);
  }
  std::uint8_t GripGemSlots(inv::InventorySlot slot) override {
    return records.grips.at(slot.one_based - 1).gem_slots;
  }
  void DrawGripGemSlotCount(std::uint8_t slots) override {
    glyph = static_cast<Byte>(slots + 0x30);
    Record(kIcon, 0x116, 256, 32, glyph);
    Record(kText, glyph, fw::PackXy(280, 36));
  }
  void ClearRows() override {
    Record(kClear, 40);
    if (mode == 7) {
      bank = 1;
    }
  }
  fw::MenuRow &Row(inv::DetailRowId row) override {
    Record(kRow, row.index, bank);
    return rows.at(bank).at(static_cast<std::size_t>(row.index)).native;
  }
  void SetPreviewLayout(inv::PreviewLayout layout) override {
    preview = static_cast<int>(layout);
  }
  void SetCategoryVisibility(Byte value) override {
    panels.category_icons = value;
  }
  void SetCommandVisibility(Byte value) override {
    panels.status_command = value;
  }
  void ContinueTransition(inv::DetailRowId row) override {
    Record(kContinue, row.index);
    if (mode == 2) {
      ChangeDirect();
    }
  }
  void ShowStatPages(std::int32_t pages) override { Record(kPages, pages); }
  void MovePrimaryStats(inv::StatPanelMotion motion) override {
    Record(kPrimary, static_cast<int>(motion));
    if (mode == 5) {
      pressed = after_pressed;
      MoveCurrentPage();
    }
  }
  void PlayLeaveSound() override { Record(kLeave); }
  void PlayInvalidSound() override { Record(kInvalid); }
  void DismissCommandRow() override { Record(kDismiss, -1); }
  void SetHeaderPlacement(inv::CategoryHeaderPlacement placement) override {
    header = static_cast<Byte>(placement);
  }
  void SetHeaderPlacement(inv::DetailHeaderPlacement placement) override {
    header = static_cast<Byte>(placement);
  }
  void SetBattleReturnIntent(inv::BattleReturnIntent intent) override {
    return_intent =
        intent == inv::BattleReturnIntent::kBattle ? VS::Input::Triangle : 0;
  }
  void HideDpPpBars() override { Record(kBars, 4); }
  std::uint8_t CurrentCategory() override { return memory[0x3A]; }
  fw::MenuRow &CategoryHeader() override {
    return Row(inv::kCategoryHeaderRow);
  }
  void Tabs(int mode_of_tabs) {
    Record(kTabs, mode_of_tabs);
    if (mode == 7) {
      bank = 1;
      memory[0x3A] = static_cast<Byte>(memory[0x3A] + 3);
    }
  }
  void DrawListTabNavigation() override { Tabs(0); }
  void DrawDetailTabNavigation() override { Tabs(1); }

  class IconQueue final : public render::SpriteQueue {
  public:
    IconQueue(World &world, bool overlay)
        : world_(world), index_(overlay ? 1 : 0) {}
    std::uint32_t ChainLink() const override { return world_.chains[index_]; }
    void Emit(const render::SpritePacket &packet) override {
      world_.sprites.push_back(packet);
      world_.chains[index_] = world_.primitive_cursor & 0xFFFFFF;
      world_.primitive_cursor += 24;
    }

  private:
    World &world_;
    std::size_t index_;
  };
  void DrawIcon(inv::CategoryIcon icon) override {
    IconQueue queue(*this, icon.overlay);
    const auto xy = static_cast<int>(fw::PackXy(icon.x, icon.y));
    const auto size = static_cast<int>(fw::PackXy(16, 16));
    if (icon.overlay) {
      render::QueueDefaultSprite(queue, size, xy);
    } else {
      render::QueueSpriteWithTexturePage(
          queue, render::kDefaultSpriteBrightness, xy, size);
    }
    sprites.back()[render::kSpriteTextureWord] =
        (120 + static_cast<std::uint32_t>(icon.category) * 16) | (128 << 8) |
        (icon.selected_palette ? 0x37FD0000 : 0x37FE0000);
  }
  void ResetDetailCursor() override {
    Record(kResetCursor);
    selected = 9;
  }
  void SetCommand(inv::DetailCommand value) override {
    const int legacy_value = value == inv::DetailCommand::kMenu ? 2 : 0;
    Record(kCommand, legacy_value);
    command_value = legacy_value;
  }
  std::int32_t
  RunController(VS::Items::ItemCategory category,
                std::optional<inv::DetailSelection> selection) override {
    active = static_cast<int>(category);
    auto &state = CurrentState();
    switch (category) {
    case VS::Items::ItemCategory::kWeapon:
      return inv::UpdateWeaponDetails(state, *this, selection);
    case VS::Items::ItemCategory::kBlade:
      return inv::UpdateBladeDetails(state, *this, selection);
    case VS::Items::ItemCategory::kGrip:
      return inv::UpdateGripDetails(state, *this, selection);
    case VS::Items::ItemCategory::kShield:
      return inv::UpdateShieldDetails(state, *this, selection);
    case VS::Items::ItemCategory::kArmor:
      return inv::UpdateArmorDetails(state, *this, selection);
    case VS::Items::ItemCategory::kGem:
      return inv::UpdateGemDetails(state, *this, selection);
    default:
      Refuse("used dispatcher category outside the six controllers");
    }
  }
  bool ReturnsToBattle() override { return return_intent != 0; }
  std::uint8_t SelectedDetailElement() override { return selected; }
  void SetCursorBright(bool bright) override { cursor = bright ? 1 : 0; }
  void DrawItemDetails() override { Record(kDrawDetails); }
  [[noreturn]] void Refuse(std::string_view reason) override {
    std::cerr << reason;
    std::exit(3);
  }
  void Print(std::int32_t result) const {
    std::vector<std::int64_t> values{result};
    AddValues(values);
    values.push_back(glyph);
    bool first = true;
    for (const auto value : values) {
      if (!first) {
        std::cout << ' ';
      }
      first = false;
      std::cout << value;
    }
    for (const auto byte : memory) {
      std::cout << ' ' << static_cast<unsigned>(byte);
    }
    for (const auto &owner : rows) {
      for (const auto &row : owner) {
        row.Print();
      }
    }
    std::cout << ' ' << primitive_cursor << ' ' << chains[0] << ' ' << chains[1]
              << ' ' << sprites.size();
    for (const auto &sprite : sprites) {
      for (const auto word : sprite) {
        std::cout << ' ' << word;
      }
    }
    std::cout << ' ' << calls.size();
    for (const auto &call : calls) {
      for (const auto value : call) {
        std::cout << ' ' << value;
      }
    }
    std::cout << '\n';
  }
};

std::optional<inv::DetailSelection> Selection(std::int32_t command) {
  if (command == 0) {
    return std::nullopt;
  }
  const auto bits = static_cast<std::uint32_t>(command);
  return inv::DetailSelection{inv::DetailRowId{static_cast<Byte>(bits >> 8)},
                              inv::DetailPage{static_cast<Byte>(bits - 1)}};
}

void Execute(World &world) {
  std::int32_t result = 0;
  switch (world.kind) {
  case 0:
    inv::DrawItemCategoryIcons(world, world.animation, world.highlight);
    break;
  case 1:
    result = inv::UpdateArmorDetails(world.CurrentState(), world,
                                     Selection(world.command));
    break;
  case 2:
    result = inv::UpdateGemDetails(world.CurrentState(), world,
                                   Selection(world.command));
    break;
  case 3: {
    std::optional<inv::DetailDispatchEntry> entry;
    if (world.command != 0) {
      entry = inv::DetailDispatchEntry{
          static_cast<VS::Items::ItemCategory>(
              static_cast<std::uint32_t>(world.command) & 0xF),
          Selection(world.command >> 4)};
    }
    result = inv::UpdateItemDetails(world.dispatcher, world, entry);
    break;
  }
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
