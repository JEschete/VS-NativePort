#include "menu2_ability_foundation_driver.hpp"

#include <bit>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>

#include "Menu/AbilityFoundationWorld.hpp"

namespace {
namespace a = VS::Menu::BattleAbilities;
using Words = std::vector<std::int64_t>;
using Calls = std::vector<Words>;
constexpr std::uint32_t kAbilityText = 0x80104690;
constexpr std::uint32_t kMenuText = 0x80104ED8;
constexpr std::uint32_t kActions = 0x8004B9DC;
constexpr std::array<std::uint8_t, 11> kChain{
    24, 25, 34, 37, 35, 30, 26, 27, 32, 29, 31};
constexpr std::array<std::uint8_t, 11> kDefense{
    47, 48, 49, 50, 41, 42, 51, 52, 44, 45, 53};

template <typename Range>
void Append(Words& output, const Range& range) {
  for (const auto value : range) output.push_back(value);
}

void AppendCalls(Words& output, const Calls& calls) {
  output.push_back(calls.size());
  for (const auto& call : calls) {
    output.push_back(call.size());
    Append(output, call);
  }
}

struct Image {
  std::array<std::uint16_t, 1060> abilities{};
  std::array<std::uint16_t, 292> menu{};

  explicit Image(const char* path) {
    std::ifstream file(path, std::ios::binary);
    const std::vector<unsigned char> bytes{
        std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    if (bytes.size() != 0x2920) throw std::runtime_error("wrong MENU2 image");
    const auto read = [&](auto& output, std::size_t offset) {
      for (std::size_t i = 0; i < output.size(); ++i) {
        output[i] = static_cast<std::uint16_t>(
            bytes.at(offset + i * 2) | bytes.at(offset + i * 2 + 1) << 8);
      }
    };
    read(abilities, 0x1E90);
    read(menu, 0x26D8);
  }

  std::uint32_t Description(int action) const {
    const auto text = a::UnlockDescription(action, abilities, menu, 193);
    if (text.empty()) throw std::runtime_error("native refused description");
    const auto address = reinterpret_cast<std::uintptr_t>(text.data());
    const auto menu_start = reinterpret_cast<std::uintptr_t>(menu.data());
    const auto ability_start = reinterpret_cast<std::uintptr_t>(abilities.data());
    if (address >= menu_start && address - menu_start < sizeof(menu)) {
      return kMenuText + static_cast<std::uint32_t>(address - menu_start);
    }
    if (address >= ability_start && address - ability_start < sizeof(abilities)) {
      return kAbilityText + static_cast<std::uint32_t>(address - ability_start);
    }
    throw std::runtime_error("description outside supplied tables");
  }
};

struct World final : VS::Test::ability_foundation::World {
  const Image& image;
  std::array<std::uint32_t, 256> flags{};
  std::array<std::uint8_t, 64> menu_memory{};
  mutable Calls calls;
  std::uint32_t context0 = 0x80310000;
  std::uint32_t context1 = 0x80320000;
  std::uint8_t cursor_enabled = 0;
  int item_state = 83;
  int item_selected = 0;
  int item_target = 1084;

  World(const Image& data, std::uint32_t seed, std::uint32_t mask = 0)
      : image(data) {
    for (std::size_t i = 0; i < flags.size(); ++i) {
      flags[i] = (seed ^ (static_cast<std::uint32_t>(i) * 0x9E3779B9u)) &
                 ~0x8000u;
    }
    for (std::size_t i = 0; i < kChain.size(); ++i) {
      if ((mask & (1u << i)) != 0) flags[kChain[i]] |= 0x8000;
      if ((mask & (1u << (i + 11))) != 0) flags[kDefense[i]] |= 0x8000;
    }
    for (std::size_t i = 0; i < menu_memory.size(); ++i) {
      menu_memory[i] = static_cast<std::uint8_t>(seed + i * 17);
    }
  }
  bool ActionUnlocked(std::uint8_t action) const override {
    return (flags[action] & 0x8000) != 0;
  }
  void UnlockAction(std::uint8_t action) override { flags[action] |= 0x8000; }
  void Refuse(std::string_view reason) override {
    throw std::runtime_error(std::string(reason));
  }
  void InitializeInformationBox() override { calls.push_back({1, 3}); }
  void SetBreakArtMessage(int weapon, std::uint8_t action) override {
    context0 = kMenuText + image.menu.at(weapon) * 2;
    context1 = kActions + action * 52 + 28;
    calls.push_back({2, kMenuText + 36, context0, context1});
  }
  void ResetUnlockMenuMemory() override {
    std::fill_n(menu_memory.begin() + 0x34, 6, 0);
    calls.push_back({3, 0x800F4F1C, 0, 6});
  }
  int SelectedRow() const override {
    calls.push_back({4});
    return selected;
  }
  bool ShowCategory(const a::AbilityCategoryRow& row) override {
    const bool chain = row.kind == a::AbilityKind::kChain;
    calls.push_back({5, chain ? 0 : 1, row.x, row.y, row.width,
                     row.background_width, chain ? 0x80105004 : 0x80105014});
    item_state = row.state;
    item_selected = row.selected ? 1 : 0;
    item_target = row.target_x;
    return true;
  }
  void ShowBothCategories() override {
    calls.push_back({6, 2, 0x1A, 1, 2, 0x80105004, 0x80104FA6,
                     0x80105014, 0x80104FA6, 0, 0});
  }
  bool ShowAbilityList(a::AbilityKind kind,
                       std::span<const std::uint8_t> actions, int visible,
                       bool force) override {
    Words call{6, visible, kind == a::AbilityKind::kChain ? 0x11B : 0x11C,
               force ? 1 : cursor_enabled, static_cast<int>(actions.size())};
    for (auto action : actions) {
      call.push_back(kActions + action * 52 + 28);
      call.push_back(image.Description(action));
    }
    for (std::size_t i = 0; i < actions.size(); ++i) call.push_back(0);
    calls.push_back(std::move(call));
    return true;
  }
  void ClearMenu() override { calls.push_back({7, 40}); }
  void FlyOutRowsAndHoistSelection(int row) override {
    calls.push_back({8, row, 0});
  }
  bool MenuRowsIdle() const override {
    calls.push_back({9});
    return ready;
  }
  void SetAbilityMessage(bool all, std::uint8_t action) override {
    if (!all) context1 = kActions + action * 52 + 28;
    calls.push_back({2, all ? 0x80105026 : 0x80104FC0, context0, context1});
  }
  void SetActionCostText(int index, std::string_view text, int x,
                         bool disabled) override {
    Words call{10, index, x, disabled ? 1 : 0,
               static_cast<std::int64_t>(text.size())};
    for (const auto byte : text) {
      call.push_back(static_cast<unsigned char>(byte));
    }
    calls.push_back(std::move(call));
  }
  void SetRiskCost(std::uint8_t action) override { calls.push_back({11, action}); }
  std::span<std::uint16_t> AllocateAbilityText(std::size_t words) override {
    calls.push_back({12, static_cast<std::int64_t>(words * 2)});
    return std::span(text).first(words);
  }
};

Words BreakArt(const Image& image, const Words& in) {
  World world(image, static_cast<std::uint32_t>(in.at(8)));
  world.weapon = static_cast<int>(in[1]);
  const auto category = world.weapon % 10;
  world.learned.at(category) = static_cast<int>(in[2]);
  world.kills.at(category) = static_cast<int>(in[3]);
  world.art_requirements.at(category).fill(static_cast<int>(in[4]));
  world.done = in[6] != 0;
  world.confirm = in[7] != 0;
  a::BreakArtUnlockState state{static_cast<std::uint8_t>(in[5])};
  const int result = a::UpdateBreakArtUnlock(state, world, world, in[0] != 0);
  Words output{result, state.timeout};
  Append(output, world.learned);
  Append(output, world.flags);
  output.insert(output.end(), {world.context0, world.context1});
  AppendCalls(output, world.calls);
  return output;
}

Words Description(Image image, const Words& in) {
  const int action = static_cast<int>(in.at(0));
  const auto index = static_cast<std::size_t>(in.at(1));
  if (index != 0) image.abilities.at(index) = static_cast<std::uint16_t>(in.at(2));
  return {image.Description(action), 0};
}

Words AbilityUnlock(const Image& image, const Words& in) {
  World world(image, static_cast<std::uint32_t>(in.at(14)),
               static_cast<std::uint32_t>(in.at(15)));
  world.unlocked = static_cast<int>(in[1]);
  world.total_kills = static_cast<int>(in[2]);
  world.ability_requirements.fill(static_cast<int>(in[3]));
  world.selected = static_cast<int>(in[9]);
  world.ready = in[10] != 0;
  world.done = in[11] != 0;
  world.confirm = in[12] != 0;
  world.cursor_enabled = static_cast<std::uint8_t>(in[13]);
  a::AbilityUnlockState state;
  state.phase = static_cast<a::AbilityUnlockPhase>(in[4]);
  state.timeout = static_cast<std::uint8_t>(in[5]);
  state.first_visit = static_cast<std::uint8_t>(in[6]);
  state.chain_count = static_cast<std::uint8_t>(in[7]);
  state.defense_count = static_cast<std::uint8_t>(in[8]);
  state.remaining_chain.fill(0xCC);
  std::ranges::copy(kChain, state.remaining_chain.begin());
  std::ranges::copy(kDefense, state.remaining_defense.begin());
  const int result = a::UpdateAbilityUnlock(state, world, world, in[0] != 0);
  Words output{result, world.unlocked, state.chain_count, state.defense_count,
               static_cast<int>(state.phase), state.timeout, state.first_visit};
  Append(output, state.remaining_chain);
  Append(output, state.remaining_defense);
  Append(output, world.flags);
  Append(output, world.menu_memory);
  output.insert(output.end(), {world.cursor_enabled, world.context0,
                              world.context1, world.item_state,
                              world.item_selected, world.item_target});
  AppendCalls(output, world.calls);
  return output;
}

struct NotificationHost final : a::UnlockNotificationHost {
  const Words& in;
  Calls calls;
  explicit NotificationHost(const Words& input) : in(input) {}
  void InitializeInformationBox() override { calls.push_back({0}); }
  bool BreakArtPending() const override { return in[1] != 0; }
  bool AbilityPending() const override { return in[2] != 0; }
  int UpdateBreakArt(bool initialize) override {
    calls.push_back({1, initialize ? 1 : 0});
    return static_cast<int>(in[initialize ? 3 : 4]);
  }
  int UpdateAbility(bool initialize) override {
    calls.push_back({2, initialize ? 1 : 0});
    return static_cast<int>(in[initialize ? 5 : 6]);
  }
  int DismissInformationBox() override {
    calls.push_back({3, 7});
    return static_cast<int>(in[7]);
  }
};

Words Notifications(const Words& in) {
  NotificationHost host(in);
  auto state = static_cast<std::uint8_t>(in.at(0));
  const int result = a::UpdateUnlockNotifications(state, host);
  Words output{result, state};
  AppendCalls(output, host.calls);
  return output;
}

Words RiskCost(const Image& image, const Words& in) {
  World world(image, 0);
  a::RiskCostState state;
  for (std::size_t i = 0; i < state.text.size(); ++i) {
    state.text[i] = std::bit_cast<char>(static_cast<std::uint8_t>(in.at(2) + i * 13));
  }
  a::SetRiskCost(state, static_cast<std::uint8_t>(in.at(1)), world);
  Words output;
  for (const char byte : state.text) output.push_back(static_cast<unsigned char>(byte));
  AppendCalls(output, world.calls);
  return output;
}

Words Rows(const Image& image, const Words& in) {
  World world(image, 0);
  const int count = static_cast<int>(in.at(0));
  const auto type = static_cast<a::AbilityMenuType>(in.at(1));
  world.remember_cursor = in.at(2) != 0;
  world.cursor = {static_cast<int>(in.at(3)), static_cast<int>(in.at(4))};
  world.has_text = in.at(5) != 0;
  const auto seed = static_cast<std::uint32_t>(in.at(8));
  for (std::size_t i = 0; i < world.text.size(); ++i) {
    world.text[i] = static_cast<std::uint16_t>(seed + i * 31);
  }
  VS::Test::ability_foundation::Rows rows;
  for (std::size_t row = 0; row < rows.text.size(); ++row) {
    for (std::size_t i = 0; i < rows.names[row].size(); ++i) {
      rows.names[row][i] = static_cast<std::uint16_t>(seed ^ (row * 131 + i * 19));
    }
    for (std::size_t i = 0; i < rows.descriptions[row].size(); ++i) {
      rows.descriptions[row][i] =
          static_cast<std::uint16_t>(seed ^ (row * 313 + i * 23 + 0x1000));
    }
    if ((in[6] & (1ll << row)) != 0) rows.text[row].name = {};
    if ((in[7] & (1ll << row)) != 0) rows.text[row].description = {};
  }
  a::AbilityRowsState state{6, 0x12345678, a::AbilityMenuType::kDefense, {3, 2}};
  const int result = a::CreateAbilityRows(state, world, count, type, rows.text);
  Words output{result, state.count, state.menu_state, static_cast<int>(state.type),
               state.cursor.highlighted, state.cursor.first, world.has_text ? 1 : 0};
  Append(output, world.text);
  AppendCalls(output, world.calls);
  return output;
}
}  // namespace

int RunAbilityFoundationDriver(int argc, char** argv, int function) {
  if (argc != 2) return 2;
  try {
    const Image image(argv[1]);
    std::string line;
    while (std::getline(std::cin, line)) {
      std::istringstream stream(line);
      Words input;
      std::int64_t value = 0;
      while (stream >> value) input.push_back(value);
      Words output;
      switch (function) {
        case 2614: output = BreakArt(image, input); break;
        case 2615: output = Description(image, input); break;
        case 2616: output = AbilityUnlock(image, input); break;
        case 2617: output = Notifications(input); break;
        case 2618: output = RiskCost(image, input); break;
        case 2619: output = Rows(image, input); break;
        default: return 2;
      }
      for (const auto word : output) std::cout << word << ' ';
      std::cout << '\n';
    }
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 3;
  }
  return 0;
}
