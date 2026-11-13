#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

#include "Input/Pad.hpp"
#include "Menu/ContainerDetailControllersWorld.hpp"

namespace {
using namespace container_detail_controllers_tests;
using Number = std::int64_t;
constexpr std::array<std::size_t, 6> kOffsets{0, 5, 9, 14, 19, 23};
constexpr std::array<std::size_t, 6> kLengths{5, 4, 5, 5, 4, 4};
constexpr std::array<Controller, 6> kControllers{
    detail::UpdateWeaponDetails, detail::UpdateBladeDetails,
    detail::UpdateGripDetails,   detail::UpdateShieldDetails,
    detail::UpdateArmorDetails,  detail::UpdateGemDetails};

struct Input {
  int kind = 0, first = 0, second = 0, mode = 0, ready = 0;
  int next_page = 0, token = 0, after_page = 0, after_row = 0;
  std::uint32_t after_pressed = 0;
  int controller_result = 0, category = 0, selected = 0, inventory_category = 0;
  std::array<int, 2> slots{};
  std::uint32_t pressed = 0;
  std::array<std::uint8_t, 28> bytes{};
  std::array<Number, 6> globals{};
  std::array<std::uint8_t, 128> headers{};
  std::array<std::array<std::uint16_t, 192>, 2> order{};
  std::array<int, 2> names{}, descriptions{};
};

template <typename Range> void Read(std::istream &stream, Range &range) {
  for (auto &field : range) {
    Number value = 0;
    stream >> value;
    field = static_cast<std::remove_reference_t<decltype(field)>>(value);
  }
}

Input Read(std::istream &stream) {
  Input item;
  stream >> item.kind >> item.first >> item.second >> item.mode >> item.ready >>
      item.next_page >> item.token >> item.after_page >> item.after_row >>
      item.after_pressed >> item.controller_result >> item.category >>
      item.selected >> item.inventory_category;
  Read(stream, item.slots);
  stream >> item.pressed;
  Read(stream, item.bytes);
  Read(stream, item.globals);
  Read(stream, item.headers);
  for (auto &order : item.order)
    Read(stream, order);
  Read(stream, item.names);
  Read(stream, item.descriptions);
  return item;
}

std::optional<detail::ControllerEntry> Selection(int command) {
  if (command == 0)
    return std::nullopt;
  const auto bits = static_cast<std::uint32_t>(command);
  return detail::ControllerEntry{{static_cast<std::uint8_t>(bits >> 8)},
                                 {static_cast<std::uint8_t>(bits - 1)}};
}

int Command(std::optional<detail::ControllerEntry> selection) {
  if (!selection.has_value())
    return 0;
  const auto entry = selection.value();
  const auto result =
      static_cast<std::uint32_t>(static_cast<std::uint8_t>(entry.row.index))
          << 8 |
      static_cast<std::uint8_t>(static_cast<std::uint32_t>(entry.page.index) +
                                1);
  return static_cast<int>(result == 0 ? 1U << 16 : result);
}

std::uint16_t Half(const std::array<std::uint8_t, 128> &bytes, std::size_t at) {
  return static_cast<std::uint16_t>(bytes.at(at) | bytes.at(at + 1) << 8);
}

void PutHalf(std::array<std::uint8_t, 128> &bytes, std::size_t at,
             std::uint16_t value) {
  bytes.at(at) = static_cast<std::uint8_t>(value);
  bytes.at(at + 1) = static_cast<std::uint8_t>(value >> 8);
}

struct Run {
  explicit Run(Input value) : input(std::move(value)) { Initialize(); }
  Input input;
  World world;
  detail::ControllerState state;
  detail::DispatcherState dispatcher;
  std::array<char, 8> footer{'X', ' ', ' ', ' ', ' ', ' ', '0', '\0'};
  std::vector<std::vector<Number>> trace;
  std::vector<std::uint32_t> textures;

  void Sync() {
    if (input.kind >= 1 && input.kind <= 6) {
      const auto category = static_cast<std::size_t>(input.kind - 1);
      const auto at = kOffsets.at(category);
      input.bytes.at(at) = static_cast<std::uint8_t>(state.stage);
      input.bytes.at(at + 1) = state.animation_step;
      input.bytes.at(at + 2) = state.selected_row;
      input.bytes.at(at + 3) = state.selected_page;
      if (kLengths.at(category) == 5)
        input.bytes.at(at + 4) = state.unused_entry_byte;
    } else if (input.kind == 7) {
      input.bytes.back() = static_cast<std::uint8_t>(dispatcher.category);
    }
    input.globals[1] = world.icons_visible;
    input.globals[2] = world.controls_visible;
    input.globals[3] = static_cast<std::uint8_t>(world.view);
  }

  std::vector<Number> Snapshot() {
    Sync();
    std::vector<Number> result(input.bytes.begin(), input.bytes.end());
    result.insert(result.end(), input.globals.begin(), input.globals.end());
    result.push_back(static_cast<Number>(world.owner));
    return result;
  }

  void Record(int event, std::array<Number, 3> args = {},
              std::array<Number, 2> extra = {}) {
    std::vector<Number> row{event,   args[0],  args[1],
                            args[2], extra[0], extra[1]};
    const auto snapshot = Snapshot();
    row.insert(row.end(), snapshot.begin(), snapshot.end());
    trace.push_back(std::move(row));
  }

  void Mutate(Event event) {
    const std::array targets{Event::kIcon,      Event::kReady,  Event::kLayout,
                             Event::kStats,     Event::kSelect, Event::kChange,
                             Event::kPoll,      Event::kBegin,  Event::kExit,
                             Event::kController};
    if (input.mode == 0 ||
        event != targets.at(static_cast<std::size_t>(input.mode)))
      return;
    world.owner = 1;
    world.pressed = input.after_pressed;
    world.selected_element = static_cast<std::uint8_t>(input.selected ^ 9);
    if (input.kind >= 1 && input.kind <= 6) {
      state.selected_row = static_cast<std::uint8_t>(input.after_row);
      state.selected_page = static_cast<std::uint8_t>(
          event == Event::kPoll ? input.next_page : input.after_page);
    }
    if (event == Event::kStats) {
      world.names[0][0] = 's';
      world.descriptions[0][0] = 't';
    }
    if (event == Event::kController) {
      input.globals[4] = 0x10000;
      world.triangle = true;
    }
  }

  void Call(const container_detail_controllers_tests::Call &call) {
    const auto event = call.event;
    if (event == Event::kCurrent)
      return;
    if (event == Event::kSlots) {
      world.gem_slots = static_cast<std::uint8_t>(input.slots.at(world.owner));
      return;
    }
    if (event == Event::kArmorGate) {
      // Controller-service parity supplies the original gate result. Actual
      // Live ownership intentionally differs under #366 and is proved apart.
      world.armor_shows_dp = input.inventory_category != 7;
      return;
    }
    std::array<Number, 3> args{call.args[0], call.args[1], call.args[2]};
    std::array<Number, 2> extra{};
    if (event == Event::kPoll || event == Event::kSelect ||
        event == Event::kFormat)
      args[1] = input.kind - 1;
    if (event == Event::kChange)
      extra = {world.observed_text[0], world.observed_text[1]};
    if (event == Event::kHeader)
      args[0] = 31;
    if (event == Event::kCommand)
      args[0] = call.args[0] == 0 ? 0 : 2;
    if (event == Event::kController)
      args[1] = Command(world.received_entry);
    if (event == Event::kDrawSlots) {
      footer[6] =
          static_cast<char>(static_cast<std::uint8_t>(call.args[0] + 0x30));
      Record(15, {0x116, 256, 32});
      Record(24, {static_cast<std::uint8_t>(footer[6]), 0x240118, 0});
      return;
    }
    if (event == Event::kIcon) {
      const auto icon = world.icons.back();
      extra = {icon.overlay ? 1 : 0, icon.overlay ? 0 : -12};
      textures.push_back((120 + icon.category * 16) | 0x8000 |
                         (icon.selected_palette ? 0x37FD0000U : 0x37FE0000U));
    }
    Record(static_cast<int>(event), args, extra);
    if (event == Event::kExit) {
      input.globals[4] = world.pressed & VS::Input::Triangle;
      world.triangle = input.globals[4] != 0;
    }
    Mutate(event);
  }

  void Initialize() {
    world.ready = input.ready;
    world.next_page = input.next_page;
    world.selected_token = static_cast<std::uint16_t>(input.token);
    world.pressed = input.pressed;
    world.order = input.order;
    world.category = static_cast<std::uint8_t>(input.category);
    world.selected_element = static_cast<std::uint8_t>(input.selected);
    world.controller_result = input.controller_result;
    world.icons_visible = static_cast<std::uint8_t>(input.globals[1]);
    world.controls_visible = static_cast<std::uint8_t>(input.globals[2]);
    world.view = static_cast<detail::DetailViewMode>(input.globals[3]);
    world.triangle = input.globals[4] != 0;
    for (std::size_t n = 0; n < 2; ++n) {
      world.names[n][0] = static_cast<char>(input.names[n]);
      world.descriptions[n][0] = static_cast<char>(input.descriptions[n]);
      world.headers[n].x =
          static_cast<std::int16_t>(Half(input.headers, n * 64 + 20));
      world.headers[n].row_icon = input.headers[n * 64 + 9];
      world.headers[n].item_page = Half(input.headers, n * 64 + 14);
    }
    if (input.kind >= 1 && input.kind <= 6) {
      const auto category = static_cast<std::size_t>(input.kind - 1);
      const auto at = kOffsets.at(category);
      state = {static_cast<detail::ControllerStage>(input.bytes.at(at)),
               input.bytes.at(at + 1), input.bytes.at(at + 2),
               input.bytes.at(at + 3),
               kLengths.at(category) == 5 ? input.bytes.at(at + 4)
                                          : std::uint8_t{0}};
    }
    dispatcher.category =
        static_cast<VS::Items::ItemCategory>(input.bytes.back());
    world.on_call = [this](const auto &call) { Call(call); };
    world.on_triangle = [this](bool triangle) {
      input.globals[4] = triangle ? VS::Input::Triangle : 0;
    };
    world.on_cursor = [this](bool bright) {
      input.globals[5] = bright ? 1 : 0;
    };
  }

  std::vector<Number> Execute() {
    std::int32_t result = 0;
    if (input.kind == 0) {
      detail::DrawCategoryIcons(world, input.first, input.second);
    } else if (input.kind <= 6) {
      result = kControllers.at(static_cast<std::size_t>(input.kind - 1))(
          state, world, Selection(input.first));
    } else {
      std::optional<detail::DispatchEntry> entry;
      if (input.first != 0)
        entry = detail::DispatchEntry{
            static_cast<VS::Items::ItemCategory>(
                static_cast<std::uint32_t>(input.first) & 15),
            Selection(input.first >> 4)};
      result = detail::UpdateItemDetails(dispatcher, world, entry);
    }
    Sync();
    std::vector<Number> values{result};
    values.insert(values.end(), input.bytes.begin(), input.bytes.end());
    values.insert(values.end(), input.globals.begin(), input.globals.end());
    values.insert(values.end(), {static_cast<Number>(world.owner),
                                 world.pressed, world.selected_element});
    for (std::size_t n = 0; n < 2; ++n) {
      input.headers[n * 64 + 9] = world.headers[n].row_icon;
      PutHalf(input.headers, n * 64 + 14, world.headers[n].item_page);
    }
    values.insert(values.end(), input.headers.begin(), input.headers.end());
    for (const char value : footer)
      values.push_back(static_cast<std::uint8_t>(value));
    values.push_back(static_cast<Number>(textures.size()));
    values.insert(values.end(), textures.begin(), textures.end());
    values.push_back(static_cast<Number>(trace.size()));
    for (const auto &row : trace)
      values.insert(values.end(), row.begin(), row.end());
    return values;
  }
};
} // namespace

int main() {
  for (std::string line; std::getline(std::cin, line);) {
    if (line.empty())
      continue;
    std::istringstream stream(line);
    Run run(Read(stream));
    for (const auto value : run.Execute())
      std::cout << value << ' ';
    std::cout << '\n';
  }
}
