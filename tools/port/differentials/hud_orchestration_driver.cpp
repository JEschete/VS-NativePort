#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>

#include "../../../Native/Tests/include/Battle/HudOrchestrationFixture.hpp"

namespace {
namespace b = VS::Battle;
namespace f = hud_orchestration_fixture;
constexpr std::size_t kInputWords = 186;
using Words = std::array<std::uint32_t, kInputWords>;

struct Input {
  int operation = 0;
  std::int32_t command = 0;
  std::int32_t slot = 0;
  b::HudFeedbackState feedback{};
  std::array<std::byte, b::kActorMarkerCount> markers{};
  std::uint32_t timer = 0;
  std::uint8_t hundredths = 0;
  std::array<char, 9> text{'0', '0', ':', '0', '0', ':', '0', '0', '\0'};
  std::uint32_t mutations = 0;
  std::int32_t next_scene = 0;
  std::int32_t next_overlay = 0;
};

std::int32_t Signed(std::uint32_t value) {
  return static_cast<std::int32_t>(value);
}

void ReadState(const Words& words, f::World& world, Input& input) {
  input.operation = static_cast<int>(words[0]);
  input.command = Signed(words[1]);
  input.slot = Signed(words[2]);
  input.feedback = {
      .marker_visibility_command = static_cast<std::int16_t>(words[3]),
      .ashley_label_suppression = static_cast<std::int16_t>(words[4])};
  world.state.commands.visibility =
      b::HudVisibilityCommand{static_cast<std::uint8_t>(words[5])};
  world.state.commands.battle_mode =
      b::HudBattleModeCommand{static_cast<std::uint8_t>(words[6])};
  world.state.commands.lower_screen = static_cast<std::uint8_t>(words[7]);
  world.state.commands.shortcut = static_cast<std::uint8_t>(words[8]);
  world.state.shortcut_step = static_cast<std::uint8_t>(words[9]);
  world.state.indicator_step = static_cast<std::uint8_t>(words[10]);
  world.state.indicator_phase = static_cast<std::uint16_t>(words[11]);
  world.state.status_icons_hidden = static_cast<std::uint8_t>(words[12]);
  world.state.slide = Signed(words[13]);
  for (std::size_t index = 0; index < world.state.carousel.size(); ++index) {
    world.state.carousel[index] = static_cast<std::uint8_t>(words[14 + index]);
  }
  world.ticks = Signed(words[18]);
  world.sine = Signed(words[19]);
  world.risk = static_cast<std::uint16_t>(words[20]);
  world.status_flags = words[21];
  world.box_state = static_cast<std::int16_t>(words[22]);
  world.box_duration = static_cast<std::uint8_t>(words[23]);
  world.scene_owner = Signed(words[24]);
  world.overlay_owner = Signed(words[25]);
  world.captured_scene = world.scene_owner;
  input.next_scene = Signed(words[26]);
  input.next_overlay = Signed(words[27]);
  input.mutations = words[28];
  input.timer = words[29];
  input.hundredths = static_cast<std::uint8_t>(words[30]);
  input.text[2] = static_cast<char>(words[31]);
  input.text[5] = static_cast<char>(words[32]);
  input.text[8] = static_cast<char>(words[33]);
  for (std::size_t index = 0; index < world.vitals.size(); ++index) {
    world.vitals[index] = static_cast<std::int16_t>(words[34 + index]);
  }
}

void ReadTables(const Words& words, f::World& world, Input& input) {
  std::size_t cursor = 38;
  const auto next = [&words, &cursor] { return words.at(cursor++); };
  for (auto& marker : input.markers) marker = static_cast<std::byte>(next());
  for (auto& step : world.closing) step = static_cast<std::uint8_t>(next());
  for (auto& step : world.displacement) {
    step = static_cast<std::uint8_t>(next());
  }
  for (auto& tone : world.tones) tone = next();
  for (auto& row : world.markers) {
    for (auto& point : row) point.x = static_cast<std::uint8_t>(next());
    for (auto& point : row) point.y = static_cast<std::uint8_t>(next());
  }
  std::array<std::array<std::uint8_t, 4>, 2> icon_x{};
  for (std::size_t row = 0; row < world.icons.size(); ++row) {
    for (auto& x : icon_x[row]) x = static_cast<std::uint8_t>(next());
    for (std::size_t slot = 0; slot < world.icons[row].size(); ++slot) {
      world.icons[row][slot].position =
          icon_x[row][slot] | (static_cast<std::uint8_t>(next()) << 16);
    }
  }
  for (auto& row : world.icons) {
    for (auto& icon : row) icon.size = Signed(next());
  }
  for (auto& row : world.textures) {
    for (auto& texture : row) texture = static_cast<std::uint16_t>(next());
  }
  for (auto& digit : world.digits) digit = Signed(next());
  for (auto& limb : world.limbs) limb.position = Signed(next());
  for (auto& limb : world.limbs) limb.size = Signed(next());
  for (auto& texture : world.limb_textures) texture = next();
  for (auto& color : world.limb_colors) color = next();
  for (auto& condition : world.conditions) {
    condition = static_cast<b::HudLimbCondition>(next());
  }
}

void Mutate(f::World& world, const f::Event& event, const Input& input) {
  const auto flags = input.mutations;
  if ((flags & 1U) != 0 && event[0] == 4) {
    world.scene_owner = input.next_scene;
    world.overlay_owner = input.next_overlay;
  }
  if ((flags & 2U) != 0 && event[0] == 3) {
    world.state.commands.shortcut = 77;
    world.state.commands.battle_mode = b::HudBattleModeCommand{
        static_cast<std::uint8_t>(world.state.commands.battle_mode.value ^ 1U)};
    for (auto& row : world.textures) {
      for (auto& texture : row) texture ^= 0x101;
    }
    for (auto& condition : world.conditions) {
      condition = static_cast<b::HudLimbCondition>(
          (static_cast<int>(condition) + 1) % 5);
    }
    world.state.slide =
        Signed(static_cast<std::uint32_t>(world.state.slide) + 0x10000U);
  }
  if ((flags & 4U) != 0 && event[0] == 2) {
    for (auto& row : world.icons) {
      for (auto& icon : row) {
        const auto xy = static_cast<std::uint32_t>(icon.position);
        icon.position = static_cast<std::uint8_t>(xy + 7) |
                        (static_cast<std::uint8_t>((xy >> 16) + 1) << 16);
        icon.size ^= 0x10001;
      }
    }
  }
  if ((flags & 8U) != 0 && event[0] == 5) {
    world.sprites.at(world.sprite_count - 1).color ^= 0xFFFF;
    world.state.indicator_phase += 0x1234;
    world.ticks = 4;
  }
  if ((flags & 16U) != 0 && event[0] == 6) {
    world.vitals[2] ^= 0x4000;
    world.risk ^= 0xFFFF;
  }
  if ((flags & 32U) != 0 && event[0] == 10) {
    world.state.carousel[0] ^= 1;
    world.state.carousel[1] ^= 2;
    world.scene_owner = input.next_scene;
    world.state.slide =
        Signed(static_cast<std::uint32_t>(world.state.slide) + 0x10000U);
  }
}

bool Run(f::World& world, Input& input) {
  switch (input.operation) {
    case 0:
      return b::DrawShortcutHud(world);
    case 1:
      return b::DrawBattleHud(world);
    case 2:
      b::FormatPuzzleTimer(
          {.frames = static_cast<std::int8_t>(input.timer),
           .seconds = static_cast<std::int8_t>(input.timer >> 8),
           .minutes = static_cast<std::int8_t>(input.timer >> 16),
           .hours = static_cast<std::int8_t>(input.timer >> 24)},
          input.hundredths, input.text);
      return true;
    case 3:
      b::SetActorMarkersHidden(input.feedback, input.command);
      return true;
    case 4:
      return b::SetActorMarker(input.markers, input.slot, input.command);
    case 5:
      b::SuppressAshleyStatusLabels(input.feedback);
      return true;
    default:
      return false;
  }
}

void Word(std::int64_t value) {
  std::cout << ' ' << static_cast<std::uint32_t>(value);
}

void Print(const f::World& world, const Input& input, bool accepted) {
  std::cout << (accepted ? 0 : 1);
  Word(input.feedback.marker_visibility_command);
  Word(input.feedback.ashley_label_suppression);
  for (const auto marker : input.markers) Word(std::to_integer<int>(marker));
  const auto& state = world.state;
  for (const auto value : std::array<std::int64_t, 13>{
           state.commands.visibility.value, state.commands.battle_mode.value,
           state.commands.lower_screen, state.commands.shortcut,
           state.shortcut_step, state.indicator_step, state.indicator_phase,
           state.status_icons_hidden, state.slide, state.carousel[0],
           state.carousel[1], state.carousel[2], state.carousel[3]}) {
    Word(value);
  }
  Word(world.ticks);
  Word(world.risk);
  Word(world.scene_owner);
  Word(world.overlay_owner);
  Word(world.events.size());
  for (const auto& event : world.events) {
    for (const auto value : event) Word(value);
  }
  Word(world.sprite_count);
  for (std::size_t index = 0; index < world.sprite_count; ++index) {
    const auto& sprite = world.sprites[index];
    Word(sprite.request.draw_mode);
    Word(sprite.color);
    Word(sprite.request.packed_position);
    Word(sprite.texture);
    Word(sprite.request.packed_size);
  }
  for (const unsigned char character : input.text) Word(character);
  std::cout << '\n';
}
}  // namespace

int main() {
  while (true) {
    Words words{};
    for (auto& word : words) {
      if (!(std::cin >> word)) return std::cin.eof() ? 0 : 1;
    }
    f::World world;
    Input input;
    ReadState(words, world, input);
    ReadTables(words, world, input);
    world.callback = [&input](auto& owner, const auto& event) {
      Mutate(owner, event, input);
    };
    const bool accepted = Run(world, input);
    Print(world, input, accepted);
  }
}
