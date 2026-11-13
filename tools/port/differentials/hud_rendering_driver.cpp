#include <cstdint>
#include <iostream>

#include "Battle/HudRenderingFixture.hpp"

namespace {
namespace f = hud_rendering_fixture;
namespace b = VS::Battle;

void Run(f::World& world) {
  const auto& input = world.input;
  bool accepted = false;
  std::uint32_t result = 0;
  switch (input[f::kOperation]) {
    case 0:
      accepted = b::DrawHudStatGauge(
          world, {.kind = static_cast<b::HudGaugeKind>(input[f::kIndex]),
                  .current = f::Signed(input[f::kCurrent]),
                  .maximum = f::Signed(input[f::kMaximum])});
      break;
    case 1:
      accepted = b::DrawSlidingStatusIcon(
          world, {.icon = f::Signed(input[f::kIndex]),
                  .slide = f::Signed(input[f::kCurrent]),
                  .slot = f::Signed(input[f::kSlot])});
      break;
    case 2: {
      const auto tone =
          b::ShortcutIconBrightness(world, f::Signed(input[f::kIndex]));
      accepted = tone.has_value();
      if (accepted) result = static_cast<std::uint32_t>(tone.value());
      break;
    }
    case 3:
      accepted = b::DrawShortcutButtonMarker(
          world, f::Signed(input[f::kMarkerPosition]),
          f::Signed(input[f::kMarkerBrightness]));
      break;
    default:
      break;
  }
  std::cout << (accepted ? 0 : 1) << ' ' << result << ' '
            << world.events.size();
  for (const auto& event : world.events) {
    for (const auto value : event) std::cout << ' ' << value;
  }
  std::cout << '\n';
}
}  // namespace

int main() {
  while (true) {
    f::World world;
    for (auto& value : world.input) {
      if (!(std::cin >> value)) return std::cin.eof() ? 0 : 1;
    }
    Run(world);
  }
}
