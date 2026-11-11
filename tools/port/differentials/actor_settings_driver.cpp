// Native side of tools\port\differentials\actor_settings.py (F01813): one vector per stdin line,
// one line of decoded fields per vector. argv[1] is BATTLE.PRG, read for the module tables.
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#include "Battle/Ai/ActorSettings.hpp"
#include "Battle/Ai/Decisions.hpp"
#include "Core/Math/ScalarMath.hpp"

extern "C" void Core_Errorf(const char*, const char*, ...) { std::abort(); }

namespace {
constexpr std::uint32_t kBase = 0x80068800u;
std::vector<unsigned char> image;

std::uint32_t Read(std::uint32_t address, int size) {
  const std::size_t offset = address - kBase;
  if (offset + size > image.size()) std::abort();
  std::uint32_t value = 0;
  for (int i = 0; i < size; ++i) value |= std::uint32_t{image[offset + i]} << (8 * i);
  return value;
}

// The native table layout the live binding copies (LiveSettingsTables), read
// from the original module bytes.
VS::Battle::Ai::ActorAiSettingsTables Tables() {
  VS::Battle::Ai::ActorAiSettingsTables tables;
  for (std::size_t i = 0; i < tables.perception_ranges_squared.size(); ++i)
    tables.perception_ranges_squared[i] = Read(0x80069C1Cu + 4 * i, 4);
  for (std::size_t i = 0; i < VS::Battle::Ai::kSettingsChoiceCount; ++i) {
    tables.view_angles[i] = static_cast<std::uint16_t>(Read(0x800F17B8u + 4 * i, 2));
    tables.step_heights[i] = static_cast<std::uint16_t>(Read(0x800F17C8u + 4 * i, 2));
    tables.facing_delay_scales[i] = static_cast<std::uint8_t>(Read(0x800F17D8u + i, 1));
    tables.terrain_reject_masks[i] = static_cast<std::uint8_t>(Read(0x80069C18u + i, 1));
  }
  for (std::size_t i = 0; i < tables.wander_presets.size(); ++i) {
    const std::uint32_t entry = 0x80069C3Cu + 6 * i;
    tables.wander_presets[i] = {static_cast<std::uint16_t>(Read(entry, 2)),
                                static_cast<std::uint16_t>(Read(entry + 2, 2)),
                                static_cast<std::uint8_t>(Read(entry + 4, 1))};
  }
  for (std::size_t i = 0; i < tables.patrol_rates.size(); ++i)
    tables.patrol_rates[i] = static_cast<std::uint8_t>(Read(0x80069C08u + i, 1));
  return tables;
}
}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) return 2;
  std::ifstream file(argv[1], std::ios::binary);
  image.assign(std::istreambuf_iterator<char>(file), {});
  const auto tables = Tables();
  std::string line;
  while (std::getline(std::cin, line)) {
    std::istringstream in(line);
    VS::Battle::Ai::ActorAiSettingsSource source;
    unsigned sub_type = 0, radius = 0, movement = 0, has_waypoints = 0;
    std::int32_t original_rcos_argument = 0;
    in >> source.settings >> source.spawn_group >> source.spawn_flags >> sub_type >> radius >> movement >>
        has_waypoints;
    source.sub_type = static_cast<std::uint16_t>(sub_type);
    source.collision_radius = static_cast<std::uint16_t>(radius);
    source.movement_flags = static_cast<std::uint16_t>(movement);
    std::array<std::uint16_t, VS::Battle::Ai::kRoomWaypointCount> waypoints{};
    for (auto& waypoint : waypoints) {
      unsigned value = 0;
      in >> value;
      waypoint = static_cast<std::uint16_t>(value);
    }
    if (has_waypoints) source.room_waypoints = waypoints;
    in >> original_rcos_argument;
    if (!in) std::abort();
    const auto d = VS::Battle::Ai::ApplyActorAiSettings(source, tables);
    const auto slot = VS::Battle::Ai::DecisionBehaviorSlot(source.spawn_group);
    std::printf(
        "%u %u %u %d %d %d %d %u %u %u %u %d %d %u %u %u %d %u %u %u %d %d %d %d %d %d %u %u %u %u %u %d %d %u "
        "%d\n",
        d.far_distance_squared, d.near_distance_squared, d.view_angle, d.view_cosine, d.settings_bit15,
        d.area_lock, d.settings_bit13, d.action_movement_mask, d.tile_step_limit, d.step_height, d.facing_delay,
        d.settings_bit24, d.settings_bit23, d.relation_category, d.settings_bits28, d.settings_bits30,
        d.settings_bit26, d.wander.move_period, d.wander.rest_period, d.wander.rate, d.settings_bit27,
        d.settings_bit25, d.terrain_reject_mask ? int{*d.terrain_reject_mask} : -1, d.settings_bit7,
        d.allow_fallback_action, d.special_mode, d.patrol_rate, d.patrol_row, d.wander_center_radius,
        d.wander_center_tile_x, d.wander_center_tile_z, d.orbiting_boss, slot ? int(*slot) : -1,
        VS::Battle::Ai::DecisionBehaviorIndex(source.spawn_group),
        VS::Core::Math::Cosine(original_rcos_argument));
  }
}
