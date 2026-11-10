#include <array>
#include <cstdint>
#include <optional>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include "Battle/Ai/ActorSettings.hpp"
#include "Core/Math/ScalarMath.hpp"

namespace {
using VS::Battle::Ai::ActorAiSettings;
using VS::Battle::Ai::ActorAiSettingsSource;
using VS::Battle::Ai::ActorAiSettingsTables;
using VS::Battle::Ai::ApplyActorAiSettings;
using VS::Battle::Ai::ApplySavedAiSettingsFlags;
using VS::Battle::Ai::kSavedAiSettingsMask;
using Waypoints = std::array<std::uint16_t, VS::Battle::Ai::kRoomWaypointCount>;

// Distinct entries so an indexing error selects a visibly wrong value.
ActorAiSettingsTables Tables() {
  ActorAiSettingsTables tables;
  tables.perception_ranges_squared = {0x10000u,  0x90000u,   0x100000u,
                                      0x23FFFFu, 0x240000u,  0x240001u,
                                      0x400000u, 0xFFFFFFFFu};
  tables.view_angles = {60, 90, 120, 359};
  tables.step_heights = {0x10, 0x20, 0x40, 0x80};
  tables.facing_delay_scales = {1, 3, 5, 0xFF};
  for (std::size_t index = 0; index < tables.wander_presets.size(); ++index) {
    tables.wander_presets[index] = {static_cast<std::uint16_t>(100 + index),
                                    static_cast<std::uint16_t>(200 + index),
                                    static_cast<std::uint8_t>(index + 1)};
  }
  tables.terrain_reject_masks = {0x11, 0x22, 0x44, 0x88};
  tables.patrol_rates = {9, 7};
  return tables;
}

ActorAiSettings Decode(std::uint32_t settings) {
  ActorAiSettingsSource source;
  source.settings = settings;
  return ApplyActorAiSettings(source, Tables());
}

// The single-bit settings flags and the decoded field each one sets.
struct SettingsFlag {
  unsigned bit;
  bool ActorAiSettings::* field;
};
}  // namespace

TEST_CASE("AI saved settings flags replace only settings bits 21-31",
          "[ai][actor-settings][saved-settings]") {
  CHECK(kSavedAiSettingsMask == 0xFFE00000u);
  CHECK(ApplySavedAiSettingsFlags(0xFFFFFFFFu, 0x0000u) == 0x001FFFFFu);
  CHECK(ApplySavedAiSettingsFlags(0x00000000u, 0xFFFFu) == 0xFFE00000u);
  CHECK(ApplySavedAiSettingsFlags(0x12345678u, 0x0ACEu) == 0xACF45678u);
}

TEST_CASE("AI saved settings flags move each bit 1-11 to bit 21-31",
          "[ai][actor-settings][saved-settings]") {
  const unsigned bit = GENERATE(range(1u, 12u));
  CHECK(ApplySavedAiSettingsFlags(0u, static_cast<std::uint16_t>(1u << bit)) ==
        1u << (bit + 20));
}

TEST_CASE("AI saved settings ignore the enable bit and flag bits 12-15",
          "[ai][actor-settings][saved-settings]") {
  const std::uint32_t settings =
      GENERATE(0u, 0x001FFFFFu, 0xFFE00000u, 0x80100001u, 0xFFFFFFFFu);
  const std::uint16_t ignored = GENERATE(
      std::uint16_t{0x0001u}, std::uint16_t{0xF000u}, std::uint16_t{0xF001u});
  const auto applied = ApplySavedAiSettingsFlags(settings, 0x0554u);
  CHECK(ApplySavedAiSettingsFlags(settings, static_cast<std::uint16_t>(
                                                0x0554u | ignored)) == applied);
  // Bit 20 stays the template's; saved bit 0 never reaches it.
  CHECK((applied & 0x001FFFFFu) == (settings & 0x001FFFFFu));
  CHECK(applied >> 21 == 0x2AAu);
}

TEST_CASE("AI settings decode indexes the range and view angle tables",
          "[ai][actor-settings][settings-decode]") {
  const ActorAiSettingsTables tables = Tables();
  const unsigned range_index = GENERATE(range(0u, 8u));
  const unsigned choice = GENERATE(range(0u, 4u));
  const ActorAiSettings decoded = Decode((range_index << 18) | (choice << 16));
  CHECK(decoded.far_distance_squared ==
        tables.perception_ranges_squared[range_index]);
  CHECK(decoded.view_angle == tables.view_angles[choice]);
  CHECK(decoded.view_cosine ==
        VS::Core::Math::Cosine((tables.view_angles[choice] / 2) * 4096 / 360));
}

TEST_CASE("AI settings decode indexes the step, facing and wander tables",
          "[ai][actor-settings][settings-decode]") {
  const ActorAiSettingsTables tables = Tables();
  const unsigned choice = GENERATE(range(0u, 4u));
  const unsigned preset = GENERATE(range(0u, 8u));
  const ActorAiSettings decoded =
      Decode((choice << 8) | (choice << 2) | (preset << 4) | (choice << 10));
  CHECK(decoded.step_height == tables.step_heights[choice]);
  CHECK(decoded.tile_step_limit == choice + 1);
  CHECK(decoded.facing_delay ==
        ((128u * tables.facing_delay_scales[choice]) >> 4));
  CHECK(decoded.wander.move_period ==
        tables.wander_presets[preset].move_period);
  CHECK(decoded.wander.rest_period ==
        tables.wander_presets[preset].rest_period);
  CHECK(decoded.wander.rate == tables.wander_presets[preset].rate);
}

TEST_CASE("AI settings decode copies the multi-bit settings fields",
          "[ai][actor-settings][settings-decode]") {
  const unsigned value = GENERATE(range(0u, 4u));
  const ActorAiSettings decoded =
      Decode((value << 21) | (value << 28) | (value << 30));
  CHECK(decoded.relation_category == value);
  CHECK(decoded.settings_bits28 == value);
  CHECK(decoded.settings_bits30 == value);
  CHECK(decoded.terrain_reject_mask == Tables().terrain_reject_masks[value]);
}

TEST_CASE("AI settings decode sets each single-bit flag from its own bit",
          "[ai][actor-settings][settings-decode]") {
  static constexpr std::array kFlags{
      SettingsFlag{7, &ActorAiSettings::settings_bit7},
      SettingsFlag{13, &ActorAiSettings::settings_bit13},
      SettingsFlag{14, &ActorAiSettings::area_lock},
      SettingsFlag{15, &ActorAiSettings::settings_bit15},
      SettingsFlag{23, &ActorAiSettings::settings_bit23},
      SettingsFlag{24, &ActorAiSettings::settings_bit24},
      SettingsFlag{25, &ActorAiSettings::settings_bit25},
      SettingsFlag{26, &ActorAiSettings::settings_bit26},
      SettingsFlag{27, &ActorAiSettings::settings_bit27}};
  const SettingsFlag flag = GENERATE(from_range(kFlags));
  const ActorAiSettings decoded = Decode(1u << flag.bit);
  for (const SettingsFlag& other : kFlags) {
    CAPTURE(flag.bit, other.bit);
    CHECK(decoded.*other.field == (other.bit == flag.bit));
  }
  CHECK(decoded.action_movement_mask == 0);
}

TEST_CASE("AI settings decode caps the near range at 0x240000",
          "[ai][actor-settings][settings-decode]") {
  // Table indices 3-7 hold 0x23FFFF, 0x240000, 0x240001, 0x400000 and
  // 0xFFFFFFFF.
  const auto [index, capped] =
      GENERATE(table<unsigned, std::uint32_t>({{0u, 0x10000u},
                                               {3u, 0x23FFFFu},
                                               {4u, 0x240000u},
                                               {5u, 0x240000u},
                                               {6u, 0x240000u},
                                               {7u, 0x240000u}}));
  const ActorAiSettings decoded = Decode(index << 18);
  CHECK(decoded.near_distance_squared == capped);
  CHECK(decoded.far_distance_squared ==
        Tables().perception_ranges_squared[index]);
}

TEST_CASE("AI settings decode shortens the facing delay above radius 128",
          "[ai][actor-settings][settings-decode]") {
  const auto [radius, step] = GENERATE(table<std::uint16_t, std::uint32_t>(
      {{0, 128u}, {128, 128u}, {129, 32u}, {0xFFFF, 32u}}));
  const unsigned scale = GENERATE(range(0u, 4u));
  ActorAiSettingsSource source;
  source.settings = scale << 2;
  source.collision_radius = radius;
  const ActorAiSettings decoded = ApplyActorAiSettings(source, Tables());
  CHECK(decoded.facing_delay ==
        ((step * Tables().facing_delay_scales[scale]) >> 4));
}

TEST_CASE("AI settings decode keeps the terrain mask under movement flags 0xC0",
          "[ai][actor-settings][settings-decode]") {
  const auto [flags, replaced] =
      GENERATE(table<std::uint16_t, bool>({{0x0000, true},
                                           {0xFF3F, true},
                                           {0x0040, false},
                                           {0x0080, false},
                                           {0x00C0, false}}));
  ActorAiSettingsSource source;
  source.settings = 2u << 30;
  source.movement_flags = flags;
  const ActorAiSettings decoded = ApplyActorAiSettings(source, Tables());
  CHECK(decoded.settings_bits30 == 2);
  CHECK(decoded.terrain_reject_mask.has_value() == replaced);
  if (replaced) {
    CHECK(*decoded.terrain_reject_mask == 0x44);
  }
}

TEST_CASE("AI settings decode keeps bit 12 only with an active room waypoint",
          "[ai][actor-settings][settings-decode]") {
  Waypoints active{};
  active[7] = 0x8000;
  Waypoints inactive{};
  inactive.fill(0x7FFF);
  const auto [waypoints, kept] =
      GENERATE_COPY(table<std::optional<Waypoints>, bool>(
          {{std::nullopt, false}, {inactive, false}, {active, true}}));
  const std::uint32_t bit12 = GENERATE(0u, 1u);
  ActorAiSettingsSource source;
  source.settings = (bit12 << 12) | 0xFFFF0000u;
  source.room_waypoints = waypoints;
  const ActorAiSettings decoded = ApplyActorAiSettings(source, Tables());
  CHECK(decoded.action_movement_mask == (kept ? bit12 : 0u));
}

TEST_CASE("AI settings decode reads the actor's spawn words",
          "[ai][actor-settings][settings-decode]") {
  const std::uint32_t mode = GENERATE(range(0u, 4u));
  const std::uint32_t rate = GENERATE(0u, 1u);
  ActorAiSettingsSource source;
  source.spawn_flags = (mode << 12) | (rate << 14) | 0xFFFF8FFFu;
  // Patrol row 2, wander radius 0x2A, tile x 0x15 and tile z 0x1B, with
  // ones in the unused bits 0-13 and 20-21 around them.
  source.spawn_group =
      (2u << 14) | (0x2Au << 16) | (0x15u << 22) | (0x1Bu << 27) | 0x00003FFFu;
  const ActorAiSettings decoded = ApplyActorAiSettings(source, Tables());
  CHECK(decoded.special_mode == ((mode & 1u) != 0));
  CHECK(decoded.allow_fallback_action == ((mode & 2u) != 0));
  CHECK(decoded.patrol_rate == Tables().patrol_rates[rate]);
  CHECK(decoded.patrol_row == 2);
  CHECK(decoded.wander_center_radius == 0x2Au);
  CHECK(decoded.wander_center_tile_x == 0x15);
  CHECK(decoded.wander_center_tile_z == 0x1B);
}

TEST_CASE("AI settings decode masks every spawn group field",
          "[ai][actor-settings][settings-decode]") {
  ActorAiSettingsSource source;
  source.spawn_group = 0xFFFFFFFFu;
  const ActorAiSettings decoded = ApplyActorAiSettings(source, Tables());
  CHECK(decoded.patrol_row == 3);
  CHECK(decoded.wander_center_radius == 0x3Fu);
  CHECK(decoded.wander_center_tile_x == 0x1F);
  CHECK(decoded.wander_center_tile_z == 0x1F);
  CHECK_FALSE(decoded.special_mode);
  CHECK_FALSE(decoded.allow_fallback_action);
  CHECK(decoded.patrol_rate == 9);
}

TEST_CASE("AI settings decode selects the orbiting boss for 0xAC and 0xAD",
          "[ai][actor-settings][settings-decode]") {
  const auto [sub_type, orbiting] =
      GENERATE(table<std::uint16_t, bool>({{0x0000, false},
                                           {0x00AB, false},
                                           {0x00AC, true},
                                           {0x00AD, true},
                                           {0x00AE, false},
                                           {0xFFFF, false}}));
  ActorAiSettingsSource source;
  source.sub_type = sub_type;
  CHECK(ApplyActorAiSettings(source, Tables()).orbiting_boss == orbiting);
}
