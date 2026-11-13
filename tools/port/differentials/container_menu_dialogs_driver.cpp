#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "Items/ItemRecordsLive.hpp"
#include "Menu/ContainerMenuDialogsWorld.hpp"

extern "C" {
#include "Items/Inventory.h"
}

namespace {
using namespace container_dialog_tests;
using Category = items::ItemCategory;
constexpr std::size_t kFields = 24;
constexpr std::size_t kRowFields = 5;
constexpr std::size_t kMemoryBytes = 242;
constexpr std::size_t kMenuTextOffset = 0x6D2C;
constexpr std::size_t kMenuTextBytes = 1380;
using Values = std::array<std::int64_t, kFields>;

std::vector<std::uint8_t> ReadAll(const std::filesystem::path &path) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream)
    throw std::runtime_error("missing required original input");
  return {std::istreambuf_iterator<char>(stream), {}};
}

std::array<std::size_t, 7> Slots(const Values &input) {
  const auto category = input[4];
  const auto selected = static_cast<std::size_t>(input[5]);
  return {category == 0 ? selected : 0,
          category == 1 ? selected : 63,
          category == 2 ? selected : 63,
          category == 3 ? selected : 0,
          category == 4 ? selected : 63,
          category == 5 ? selected : 191,
          selected};
}

void Populate(DialogWorld &world, const Values &input) {
  const auto slots = Slots(input);
  auto &contents = world.first_owner.items;
  auto &weapon = contents.weapons[slots[0]];
  weapon.index = static_cast<std::uint8_t>(slots[0] + 1U);
  weapon.blade = 64;
  weapon.grip = 64;
  weapon.gems[0] = 192;
  auto &blade = contents.blades[slots[1]];
  blade.id = 17;
  blade.index = static_cast<std::uint8_t>(slots[1] + 1U);
  auto &grip = contents.grips[slots[2]];
  grip.id = 17;
  grip.index = static_cast<std::uint16_t>(slots[2] + 1U);
  auto &shield = contents.shields[slots[3]];
  shield.index = static_cast<std::uint8_t>(slots[3] + 1U);
  shield.base.id = 17;
  shield.gems[0] = 192;
  auto &armor = contents.armor[slots[4]];
  armor.id = 17;
  armor.index = static_cast<std::uint8_t>(slots[4] + 1U);
  auto &gem = contents.gems[slots[5]];
  gem.id = 321;
  gem.index = static_cast<std::int16_t>(slots[5] + 1U);
  auto &misc = contents.misc[slots[6]];
  misc.id = 321;
  misc.count = static_cast<std::uint8_t>(input[20]);
  misc.index = static_cast<std::uint8_t>(slots[6] + 1U);
}

void ReadWorld(std::istream &line, DialogWorld &world, const Values &input) {
  world.direction = static_cast<std::uint8_t>(input[12]);
  world.ready = static_cast<std::int32_t>(input[13]);
  world.information_done = input[14] != 0;
  world.action = static_cast<std::int32_t>(input[15]);
  world.pressed = static_cast<std::uint32_t>(input[16]);
  world.repeated = static_cast<std::uint32_t>(input[17]);
  world.cursor_result = static_cast<std::int32_t>(input[18]);
  world.list_row = static_cast<std::uint8_t>(input[19]);
  world.controls = static_cast<std::uint8_t>(input[21]);
  world.frozen_tabs = static_cast<std::uint8_t>(input[22]);
  for (auto &row : world.rows) {
    std::array<std::int32_t, kRowFields> values{};
    for (auto &value : values)
      line >> value;
    row.state = static_cast<framework::RowState>(values[0]);
    row.x = static_cast<std::int16_t>(values[1]);
    row.target0 = static_cast<std::int16_t>(values[2]);
    row.selected = static_cast<std::uint8_t>(values[3]);
    row.unselectable = static_cast<std::uint8_t>(values[4]);
  }
  world.memory.resize(kMemoryBytes);
  for (auto &byte : world.memory) {
    unsigned value = 0;
    line >> value;
    byte = static_cast<std::uint8_t>(value);
  }
  if (!line)
    throw std::runtime_error("short differential vector");
  Populate(world, input);
}

void Print(std::int64_t value) { std::cout << value << ' '; }

template <typename Raw, typename Record>
void PrintRecord(const Record &record) {
  Raw raw{};
  items::Live::Commit(raw, record);
  for (const auto byte : std::as_bytes(std::span(&raw, 1))) {
    Print(std::to_integer<std::uint8_t>(byte));
  }
}

void PrintRecords(const DialogWorld &world, const Values &input) {
  const auto slots = Slots(input);
  const auto &records = world.first_owner.items;
  PrintRecord<Items_InventoryWeapon>(records.weapons[slots[0]]);
  PrintRecord<Items_InventoryBlade>(records.blades[slots[1]]);
  PrintRecord<Items_InventoryGrip>(records.grips[slots[2]]);
  PrintRecord<Items_InventoryShield>(records.shields[slots[3]]);
  PrintRecord<Items_InventoryArmor>(records.armor[slots[4]]);
  PrintRecord<Items_InventoryGem>(records.gems[slots[5]]);
  PrintRecord<Items_InventoryMisc>(records.misc[slots[6]]);
}

void Execute(int identifier, const Values &input, DialogWorld &world) {
  dialogs::SortMenuState sort{static_cast<dialogs::ActionDialogStep>(input[1]),
                              static_cast<std::uint8_t>(input[2])};
  dialogs::DiscardMenuState discard{static_cast<dialogs::DiscardStep>(input[3]),
                                    static_cast<Category>(input[4]),
                                    static_cast<std::uint8_t>(input[5]),
                                    static_cast<std::uint8_t>(input[6]),
                                    static_cast<std::uint8_t>(input[7]),
                                    static_cast<std::uint8_t>(input[8]),
                                    static_cast<std::uint8_t>(input[9])};
  dialogs::TransferConfirmationState transfer{
      static_cast<std::int32_t>(input[11]),
      static_cast<dialogs::ActionDialogStep>(input[10])};
  dialogs::DiscardPresentation presentation;
  presentation.cursor_animation = static_cast<std::uint8_t>(input[23]);
  const auto command = static_cast<std::uint32_t>(input[0]);
  std::int32_t result = 0;
  switch (identifier) {
  case 2969: {
    const auto entry =
        command == 0
            ? std::optional<Category>{}
            : static_cast<Category>(static_cast<std::uint8_t>(command - 1U));
    result = dialogs::UpdateSortMenu(sort, world, entry);
    break;
  }
  case 2970: {
    std::optional<dialogs::DiscardEntry> entry;
    if (command != 0) {
      entry = dialogs::DiscardEntry{static_cast<Category>((command >> 12) & 15),
                                    static_cast<std::uint8_t>(command - 1U),
                                    static_cast<std::uint8_t>(command >> 16)};
    }
    result = dialogs::UpdateDiscardMenu(discard, presentation, world, entry);
    break;
  }
  case 2983:
    dialogs::RestoreItemRows(world, static_cast<std::int32_t>(command));
    break;
  case 2984:
    result = dialogs::UpdateTransferConfirmation(transfer, world, command != 0);
    break;
  default:
    throw std::runtime_error("unexpected function ID");
  }
  Print(result);
  Print(static_cast<int>(sort.step));
  Print(sort.category);
  Print(static_cast<int>(discard.step));
  Print(static_cast<int>(discard.category));
  Print(discard.record_index);
  Print(discard.kind);
  Print(discard.place);
  Print(discard.quantity);
  Print(discard.maximum);
  Print(static_cast<int>(transfer.step));
  Print(transfer.result);
  Print(world.controls);
  Print(world.frozen_tabs);
  Print(world.list_row);
  Print(presentation.cursor_animation);
  for (auto byte : presentation.text)
    Print(byte);
  for (const auto &row : world.rows) {
    Print(static_cast<int>(row.state));
    Print(row.x);
    Print(row.target0);
    Print(row.selected);
    Print(row.unselectable);
  }
  PrintRecords(world, input);
  for (const auto &event : world.events) {
    Print(static_cast<int>(event.operation));
    Print(event.first);
    Print(event.second);
    Print(event.third);
  }
  std::cout << '\n';
}
} // namespace

int main(int argc, char **argv) {
  try {
    if (argc != 2)
      throw std::runtime_error("expected original MENUD path");
    const std::filesystem::path path(argv[1]);
    const auto module = ReadAll(path);
    if (module.size() < kMenuTextOffset + kMenuTextBytes) {
      throw std::runtime_error("short original MENUD text data");
    }
    const auto help = ReadAll(path.parent_path() / "ITEMHELP.BIN");
    std::string text;
    while (std::getline(std::cin, text)) {
      std::istringstream line(text);
      int identifier = 0;
      line >> identifier;
      Values input{};
      for (auto &value : input)
        line >> value;
      DialogWorld world;
      world.help = help;
      world.container.assign(module.begin() + kMenuTextOffset,
                             module.begin() + kMenuTextOffset + kMenuTextBytes);
      ReadWorld(line, world, input);
      Execute(identifier, input, world);
    }
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
