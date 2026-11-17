#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "Menu/StatusTerminalWorld.hpp"

namespace st = VS::Menu::Status;
namespace fixture = status_terminal_tests;
namespace {
enum class Mode { kDetails, kList, kFade, kBackground, kFrame, kMain };
template <typename T>
void Read(std::istream& input, T& value) {
  std::int64_t word = 0;
  if (!(input >> word)) {
    throw std::runtime_error("short terminal vector");
  }
  value = static_cast<T>(word);
}
template <typename... T>
void Fields(std::istream& input, T&... values) {
  (Read(input, values), ...);
}
template <typename T, std::size_t N>
void Array(std::istream& input, std::array<T, N>& values) {
  for (auto& value : values) {
    Read(input, value);
  }
}
void Frame(std::istream& input, st::TerminalFrame& f) {
  Fields(input, f.selected_actor, f.previous_actor, f.animation_step, f.yaw,
         f.pitch, f.background_first, f.fade, f.vitals_phase, f.vitals_slide,
         f.initialized_actor, f.model_delay, f.selected_element,
         f.selected_stat, f.cursor_color, f.freeze_tabs, f.stat_page,
         f.enabled_pages, f.selected_equipment, f.cursor_memory, f.hud_hidden,
         f.current_menu, f.pressed, f.held, f.repeat, f.stick_x, f.stick_y);
}
void View(std::istream& input, st::TerminalView& view) {
  std::array<std::uint32_t, fixture::kViewWords> words{};
  Array(input, words);
  view = std::bit_cast<st::TerminalView>(words);
}
void Initialize(std::istream& input, fixture::World& world) {
  Frame(input, world.frame);
  auto& d = world.detail;
  auto& s = world.screen;
  Fields(input, d.cursor_animation, d.phase, d.selected_row, d.row_to_render,
         d.selected_element, world.list.phase, world.list.timer,
         s.animation_wait, s.screen_enabled, s.cursor_animation,
         s.saved_projection, s.prompt_animation);
  View(input, world.view);
  View(input, s.saved_view);
  Fields(input, world.switch_changes_actor, world.switch_target,
         world.switch_result, world.selected_row, world.detail_selection,
         world.panel_phase, world.effects_result, world.load_result,
         world.unload_result, world.cursor_result, world.ready,
         world.item_text_present, world.framebuffer, world.projection,
         world.model.distance, world.model.height);
  for (auto& actor : world.actors) {
    Array(input, actor.limb_names);
    Array(input, actor.equipment_ids);
    Fields(input, actor.accessory_id, actor.weapon_gem_slots,
           actor.shield_gem_slots, actor.analyzed, actor.statuses);
  }
  Array(input, world.row_flags);
  Array(input, world.packets.heads);
  Array(input, world.offsets);
  Array(input, world.animation);
  Array(input, world.cursors);
}
template <typename... T>
void Out(std::vector<std::uint32_t>& values, T... fields) {
  (values.push_back(static_cast<std::uint32_t>(fields)), ...);
}
void Print(fixture::World& w, std::uint8_t menu_state, std::int32_t result) {
  std::vector<std::uint32_t> values;
  const auto& f = w.frame;
  const auto& d = w.detail;
  const auto& s = w.screen;
  Out(values, result, menu_state, f.selected_actor, f.previous_actor,
      f.animation_step, f.yaw, f.pitch, f.background_first, f.fade,
      f.vitals_phase, f.vitals_slide, f.initialized_actor, f.model_delay,
      f.selected_element, f.selected_stat, f.cursor_color, f.freeze_tabs,
      f.stat_page, f.enabled_pages, f.selected_equipment, f.cursor_memory,
      f.hud_hidden, f.current_menu, f.pressed, f.held, f.repeat, f.stick_x,
      f.stick_y, d.cursor_animation, d.phase, d.selected_row, d.row_to_render,
      d.selected_element, w.list.phase, w.list.timer, s.animation_wait,
      s.screen_enabled, s.cursor_animation, s.saved_projection,
      s.prompt_animation);
  for (const auto& view : {w.view, s.saved_view}) {
    for (auto word :
         std::bit_cast<std::array<std::uint32_t, fixture::kViewWords>>(view)) {
      values.push_back(word);
    }
  }
  Out(values, w.projection);
  for (auto byte : w.description) {
    Out(values, byte);
  }
  for (const auto& row : w.rows) {
    for (auto byte :
         std::as_bytes(std::span(&row, 1)).first(fixture::kOriginalRowPrefix)) {
      Out(values, byte);
    }
  }
  Out(values, w.packets.words.size());
  for (auto head : w.packets.heads) {
    values.push_back(head);
  }
  values.insert(values.end(), w.packets.words.begin(), w.packets.words.end());
  Out(values, w.events.size());
  for (const auto& event : w.events) {
    values.insert(values.end(), event.begin(), event.end());
  }
  for (auto value : values) {
    std::cout << value << ' ';
  }
  std::cout << '\n';
}
void Run(std::istream& input) {
  Mode mode = Mode::kDetails;
  int request = 0;
  std::uint32_t seed = 0;
  std::uint8_t menu_state = 0;
  Fields(input, mode, seed, request, menu_state);
  fixture::World world(seed);
  Initialize(input, world);
  st::TerminalResult result{0};
  switch (mode) {
    case Mode::kDetails:
      result = st::UpdateEquipmentDetails(world.detail, world, request);
      break;
    case Mode::kList:
      result =
          st::UpdateEquipmentList(world.list, world.detail, world, request);
      break;
    case Mode::kFade:
      if (!world.Fade()) {
        throw std::runtime_error("fade refused");
      }
      break;
    case Mode::kBackground:
      if (!world.Background()) {
        throw std::runtime_error("background refused");
      }
      break;
    case Mode::kFrame:
      result = st::DrawFrame(world);
      break;
    case Mode::kMain:
      result = st::UpdateStatusTerminal(world.screen, world, menu_state);
      break;
    default:
      throw std::runtime_error("invalid terminal mode");
  }
  if (!result) {
    throw std::runtime_error(
        std::string(st::TerminalErrorText(result.error())) +
        "; mode=" + std::to_string(static_cast<int>(mode)) +
        "; seed=" + std::to_string(seed));
  }
  Print(world, menu_state, *result);
}
}  // namespace
int main() {
  try {
    for (std::string line; std::getline(std::cin, line);) {
      if (line.empty()) {
        continue;
      }
      std::istringstream input(line);
      Run(input);
    }
  } catch (const std::exception& error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 2;
  }
}
