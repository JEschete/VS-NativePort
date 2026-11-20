#include "Save/NativeSnapshot.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <iterator>
#include <limits>
#include <ranges>
#include <set>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "Save/SaveStore.hpp"
#include "nlohmann/json.hpp"

namespace VS::Game::Save {
namespace {
// State fields: _applyLoadedSaveFile/_packageGameSaveData
// (src/TITLE/TITLE.PRG/22C.c:569/616; src/MENU/MENU7.PRG/260.c:681/735).
// Native records stage named fields only; their object bytes are never saved.
using Json = nlohmann::json;
constexpr std::string_view kFormat = "vs-nativeport-save";
constexpr std::string_view kSource = "SLUS-01040";
constexpr std::uint32_t kVersion = 3;
constexpr std::uint32_t kClearVersion = 4;
constexpr int kMaximumDepth = 64;
constexpr unsigned kWordBits = 32;
constexpr std::int32_t kMaximumAngle = 4095;
constexpr std::int32_t kMaximumRandomValue = 32767;
constexpr std::uint64_t kMaximumPlayTicks = 101ull * 60 * 60 * 60 - 1;
constexpr std::uint64_t kMaximumUnixMilliseconds = 253402300799999ull;
constexpr std::uint64_t kHashBasis = 0xCBF29CE484222325ull;
constexpr std::uint64_t kHashPrime = 0x100000001B3ull;
constexpr std::uint32_t kTwoBitMask = 0x3;
constexpr std::uint32_t kThreeBitMask = 0x7;
constexpr std::uint32_t kFourBitMask = 0xF;
constexpr std::uint32_t kFiveBitMask = 0x1F;
constexpr std::uint32_t kSevenBitMask = 0x7F;
constexpr std::uint32_t kTwentyFourBitMask = 0xFFFFFF;
constexpr auto kMaximumLocationId = std::numeric_limits<std::uint8_t>::max();
constexpr std::uint16_t kMaximumSceneId = 1023;

struct Reference {
  std::string_view prefix;
  std::uint32_t maximum;
  std::uint32_t value;
};

struct Bits {
  std::uint32_t value;
  std::uint32_t maximum;
};

struct IdSet {
  std::string_view prefix;
  std::uint32_t count;
  std::vector<std::uint32_t> values;
};

template <class V, class T>
void Id(V& visit, const char* key, std::string_view prefix, T& field) {
  using Unsigned = std::make_unsigned_t<T>;
  Reference reference{prefix, std::numeric_limits<Unsigned>::max(),
                      static_cast<Unsigned>(field)};
  visit(key, reference);
  field = static_cast<T>(reference.value);
}

template <std::size_t N>
IdSet WordIds(std::string_view prefix, const int (&words)[N]) {
  IdSet result{prefix, N * kWordBits, {}};
  for (std::size_t index = 0; index < N * kWordBits; ++index) {
    if ((static_cast<std::uint32_t>(words[index / kWordBits]) >>
         (index % kWordBits)) &
        1u) {
      result.values.push_back(static_cast<std::uint32_t>(index));
    }
  }
  return result;
}

template <std::size_t N>
void ApplyWordIds(const IdSet& ids, int (&words)[N]) {
  std::array<std::uint32_t, N> packed{};
  for (const auto id : ids.values) {
    packed[id / kWordBits] |= 1u << (id % kWordBits);
  }
  for (std::size_t index = 0; index < N; ++index) {
    words[index] = std::bit_cast<std::int32_t>(packed[index]);
  }
}

template <class V>
void Fields(VS::Items::WeaponRange& r, V&& v) {
  v("range", r.range);
  v("second_range", r.second_range);
  v("third_range", r.third_range);
  Bits shape{r.AttackShape(), kThreeBitMask};
  Bits angle{r.AttackShapeAngle(), kFiveBitMask};
  v("shape", shape);
  v("angle", angle);
  r.SetAttackShape(static_cast<std::uint8_t>(shape.value));
  r.SetAttackShapeAngle(static_cast<std::uint8_t>(angle.value));
}

template <class V>
void Fields(VS::Items::InventoryWeapon& r, V&& v) {
  v("index", r.index);
  v("blade_slot", r.blade);
  v("grip_slot", r.grip);
  v("equipped", r.is_equipped);
  v("gem_slots", r.gems);
  v("name_glyphs", r.name);
}

template <class V>
void Fields(VS::Items::InventoryArmor& r, V&& v) {
  Id(v, "definition", "armor", r.id);
  v("sub_id", r.sub_id);
  Id(v, "model", "weapon-model", r.weapon_model_id);
  v("category", r.category);
  v("maximum_dp", r.max_dp);
  v("maximum_pp", r.max_pp);
  v("dp", r.current_dp);
  v("pp", r.current_pp);
  v("gem_capacity", r.gem_slots);
  v("strength", r.strength);
  v("intelligence", r.intelligence);
  v("agility", r.agility);
  v("types", r.types);
  v("classes", r.classes);
  v("affinities", r.affinities);
  v("material", r.material);
  v("limb", r.limb);
  v("index", r.index);
}

template <class V>
void Fields(VS::Items::InventoryShield& r, V&& v) {
  v("index", r.index);
  v("equipped", r.is_equipped);
  v("base", r.base);
  v("gem_slots", r.gems);
}

template <class V>
void Fields(VS::Items::InventoryBlade& r, V&& v) {
  Id(v, "definition", "blade", r.id);
  v("sub_id", r.sub_id);
  Id(v, "model", "weapon-model", r.weapon_model_id);
  v("category", r.category);
  v("maximum_dp", r.max_dp);
  v("maximum_pp", r.max_pp);
  v("dp", r.current_dp);
  v("pp", r.current_pp);
  v("strength", r.strength);
  v("intelligence", r.intelligence);
  v("agility", r.agility);
  v("cost", r.cost);
  Bits damage{static_cast<unsigned>(r.DamageType()) & kTwoBitMask, kTwoBitMask};
  Bits cost{static_cast<unsigned>(r.CostType()) & kThreeBitMask, kThreeBitMask};
  Bits source{static_cast<unsigned>(r.ReservedBits()) & kThreeBitMask,
              kThreeBitMask};
  v("damage_type", damage);
  v("cost_type", cost);
  v("source_flags", source);
  r.SetDamageType(static_cast<std::uint8_t>(damage.value));
  r.SetCostType(static_cast<std::uint8_t>(cost.value));
  r.SetReservedBits(static_cast<std::uint8_t>(source.value));
  v("source_parameter_12", r.recovery_delay);
  v("range", r.range);
  v("classes", r.classes);
  v("affinities", r.affinities);
  v("material", r.material);
  v("assembled_weapon_slot", r.assembled_weapon_index);
  v("index", r.index);
}

template <class V>
void Fields(VS::Items::InventoryGrip& r, V&& v) {
  Id(v, "definition", "grip", r.id);
  v("sub_id", r.sub_id);
  v("category", r.category);
  v("gem_capacity", r.gem_slots);
  v("strength", r.strength);
  v("intelligence", r.intelligence);
  v("agility", r.agility);
  v("types", r.types);
  v("assembled_weapon_slot", r.assembled_weapon_index);
  v("index", r.index);
}

template <class V>
void Fields(VS::Items::InventoryGem& r, V&& v) {
  Id(v, "definition", "gem", r.id);
  v("sub_id", r.sub_id);
  v("effects", r.gem_effects);
  v("strength", r.strength);
  v("intelligence", r.intelligence);
  v("agility", r.agility);
  v("classes", r.classes);
  v("affinities", r.affinities);
  v("equipped_item_slot", r.set_item_index);
  v("index", r.index);
}

template <class V>
void Fields(VS::Items::InventoryMisc& r, V&& v) {
  Id(v, "definition", "item", r.id);
  v("count", r.count);
  v("index", r.index);
}

template <class T, class V>
void InventoryFields(T& r, V&& v) {
  v("weapons", r.weapons);
  v("shields", r.shields);
  v("blades", r.blades);
  v("grips", r.grips);
  v("armor", r.armor);
  v("gems", r.gems);
  v("misc", r.misc);
}

template <class V>
void Fields(VS::Items::InventoryContents& r, V&& v) {
  InventoryFields(r, v);
}

template <class V>
void Fields(VS::Items::ContainerData& r, V&& v) {
  InventoryFields(r, v);
}

template <class V>
void Fields(VS::Items::InventoryIndices& r, V&& v) {
  InventoryFields(r, v);
}

template <class V>
void Fields(VS::State::AshleyLimbSnapshot& r, V&& v) {
  v("hp", r.hp);
  v("maximum_hp", r.max_hp);
  v("types", r.types);
  v("affinities", r.affinities);
}

template <class V>
void Fields(VS::State::AshleyStatsSnapshot& r, V&& v) {
  v("hp", r.current_hp);
  v("maximum_hp", r.max_hp);
  v("mp", r.current_mp);
  v("maximum_mp", r.max_mp);
  v("strength", r.strength);
  v("total_strength", r.total_strength);
  v("intelligence", r.intelligence);
  v("total_intelligence", r.total_intelligence);
  v("agility", r.agility);
  v("total_agility", r.total_agility);
  v("risk", r.risk);
  v("recovery", r.action_recovery_timer);
  v("statuses", r.statuses);
  v("status_timers", r.status_timers);
  v("limbs", r.limbs);
  v("weapon_drawn", r.weapon_drawn);
  v("camera_pitch", r.camera_pitch);
  v("camera_distance", r.camera_distance);
}

template <class V>
void Fields(VS::Input::Settings& r, V&& v) {
  Bits mode{static_cast<unsigned>(r.mode_flags), kFourBitMask};
  Bits container{static_cast<unsigned>(r.container_empty), 1};
  Bits timing{static_cast<unsigned>(r.ability_timing_display), 1};
  Bits weapon{static_cast<unsigned>(r.weapon_stat_change), 1};
  Bits armor{static_cast<unsigned>(r.armor_stat_change), 1};
  v("mode_flags", mode);
  v("container_empty", container);
  v("ability_timing", timing);
  v("weapon_status", weapon);
  v("armor_status", armor);
  r.mode_flags = static_cast<u_char>(mode.value);
  r.container_empty = static_cast<u_char>(container.value);
  r.ability_timing_display = static_cast<u_char>(timing.value);
  r.weapon_stat_change = static_cast<u_char>(weapon.value);
  r.armor_stat_change = static_cast<u_char>(armor.value);
  v("cursor_memory", r.cursor_memory);
  v("menu_flags", r.menu_flags);
  v("campaign_identity", r.slot_state);
  v("information", r.information);
  v("simple_map", r.simple_map);
  v("mono_sound", r.mono_sound);
  v("vibration", r.vibration_on);
  v("chain_bindings", r.mapped_chain_abilities);
  v("defense_bindings", r.mapped_defense_abilities);
  v("source_save_key", r.key);
  v("cumulative_saves", r.cumulative_save_count);
  v("current_game_saves", r.current_game_save_count);
  v("source_save_generation", r.save_file_generation);
}

template <class V>
void Fields(VS::State::RoomVisitFlags& r, V&& v) {
  auto rooms = WordIds("map", r.room_flags);
  auto areas = WordIds("area", r.area_flags);
  v("maps", rooms);
  v("areas", areas);
  ApplyWordIds(rooms, r.room_flags);
  ApplyWordIds(areas, r.area_flags);
}

template <class V>
void Fields(VS::State::ArtsStatus& r, V&& v) {
  v("learned", r.arts_learned);
  v("weapon_kills", r.kills.weapon_category_kills);
  v("abilities_unlocked", r.kills.battle_abilities_unlocked);
  v("total_kills", r.kills.total_kills);
}

template <class V>
void Fields(VS::State::TrialScore& r, V&& v) {
  Bits time{r.time, kTwentyFourBitMask};
  Bits round{r.round, kSevenBitMask};
  Bits difficulty{r.difficulty, 1};
  v("time", time);
  v("round", round);
  v("difficulty", difficulty);
  r.time = time.value;
  r.round = round.value;
  r.difficulty = difficulty.value;
}

template <class V>
void Fields(VS::State::ScoreData& r, V&& v) {
  v("titles", r.titles);
  v("class_kills", r.enemy_kills);
  v("weapon_attacks", r.weapon_attacks);
  v("trials", r.boss_time_trial_scores);
  v("maximum_chain", r.max_chain);
  v("clear_count", r.clear_count);
  v("completion_minutes", r.completion_time_minutes);
  v("map_completion", r.map_completion);
  v("opened_chests", r.opened_chest_count);
  auto kills = WordIds("enemy", r.enemy_kill_flags);
  v("defeated_enemies", kills);
  ApplyWordIds(kills, r.enemy_kill_flags);
  v("streak_score", r.streak_score);
  v("misc_score", r.misc_score);
  v("kill_streak_class", r.kill_streak_enemy_class);
  v("kill_streak_count", r.kill_streak_enemy_count);
  v("weapon_streak_category", r.weapon_kill_streak_blade_category);
  v("weapon_streak_count", r.weapon_kill_streak);
  v("chain_streak", r.chain_streak);
  v("heal_count", r.heal_count);
}

template <class V>
void Fields(VS::Actors::RoomActorRecord& r, V&& v) {
  v("spawn_slot", r.spawn_slot);
  v("hidden", r.hidden);
  v("ai_flags", r.ai_flags);
  v("hp", r.current_hp);
  v("maximum_hp", r.max_hp);
  v("mp", r.current_mp);
  v("maximum_mp", r.max_mp);
  v("strength", r.strength);
  v("intelligence", r.intelligence);
  v("agility", r.agility);
  v("limb_hp", r.limb_hp);
  v("statuses", r.statuses);
  v("tile_x", r.tile_x);
  v("direction", r.direction);
  v("tile_z", r.tile_z);
  v("height", r.height);
}

template <class V>
void Fields(RoomCache& r, V&& v) {
  v("recency", r.recency);
  Id(v, "zone", "zone", r.zone);
  Id(v, "map", "map", r.map);
  v("next_slot", r.next_slot);
  v("actors", r.actors);
}

template <class V>
void Fields(TileAddress& r, V&& v) {
  v("x", r.x);
  v("z", r.z);
  v("layer", r.layer);
}

template <class V>
void Fields(GroundedPlacement& r, V&& v) {
  v("tile", r.tile);
  v("facing", r.facing);
}

template <class V>
void Fields(Location& r, V&& v) {
  Id(v, "zone", "zone", r.zone);
  Id(v, "room", "room", r.room);
  Id(v, "scene", "scene", r.scene);
  v("position", r.position);
  v("facing", r.facing);
  v("camera_yaw", r.camera_yaw);
  v("floor_layer", r.floor_layer);
  v("previous_tile", r.previous_tile);
  v("previous_facing", r.previous_facing);
  v("saved_placement", r.saved_placement);
}

template <class V>
void Fields(OrbitCamera& r, V&& v) {
  v("pitch", r.pitch);
  v("distance", r.distance);
  v("mode", r.mode);
  v("rotation_step", r.rotation_step);
  v("zoom_mode", r.zoom_mode);
  v("zoom_velocity", r.zoom_velocity);
  v("tracking_speed", r.tracking_speed);
  v("look_at", r.look_at);
  v("view_angles", r.view_angles);
}

template <class V>
void Fields(RandomState& r, V&& v) {
  v("bios_state", r.bios_state);
  v("interface_state", r.interface_state);
  v("shuffle", r.shuffle);
  v("shuffle_index", r.shuffle_index);
  v("movement_state", r.movement_state);
}

template <class V>
void Fields(VS::State::SceneTimer& r, V&& v) {
  v("mode", r.mode);
  v("paused_mode", r.paused_mode);
  v("reset_scope", r.reset_scope);
  v("frames", r.time.frames);
  v("seconds", r.time.seconds);
  v("minutes", r.time.minutes);
  v("hours", r.time.hours);
}

template <class V>
void Fields(VS::Field::MpdDoorRecord& r, V&& v) {
#define VS_SAVE_DOOR_BITS(key, field, mask)          \
  {                                                  \
    Bits bits{static_cast<unsigned>(r.field), mask}; \
    v(key, bits);                                    \
    r.field = static_cast<u_short>(bits.value);      \
  }
  v("zone", r.destination_zone_id);
  v("room", r.destination_room_id);
  VS_SAVE_DOOR_BITS("tile_x", tile_location.tile_x, kFiveBitMask);
  VS_SAVE_DOOR_BITS("tile_z", tile_location.tile_z, kFiveBitMask);
  VS_SAVE_DOOR_BITS("floor_layer", tile_location.floor_layer, kThreeBitMask);
  VS_SAVE_DOOR_BITS("entry_direction", tile_location.entry_direction,
                    kThreeBitMask);
  VS_SAVE_DOOR_BITS("required_flag_value", required_flag_value, 255);
  VS_SAVE_DOOR_BITS("entry_tile_x", entry_tile_x, kFiveBitMask);
  VS_SAVE_DOOR_BITS("entry_elevation", entry_elevation, kTwoBitMask);
  VS_SAVE_DOOR_BITS("has_entry_step", has_entry_step, 1);
  VS_SAVE_DOOR_BITS("required_flag_id", required_flag_id, kMaximumSceneId);
  VS_SAVE_DOOR_BITS("entry_tile_z", entry_tile_z, kFiveBitMask);
  VS_SAVE_DOOR_BITS("reserved_bit", reserved06_bit15, 1);
  v("door_id", r.door_id);
  v("reserved_09", r.reserved09);
  v("reserved_0a", r.reserved0a);
  v("reserved_0b", r.reserved0b);
#undef VS_SAVE_DOOR_BITS
}

template <class V>
void Fields(Snapshot& r, V&& v) {
  v("script_flags", r.script_flags);
  IdSet learned{
      "action", static_cast<std::uint32_t>(r.learned_actions.size()), {}};
  for (std::size_t index = 0; index < r.learned_actions.size(); ++index) {
    if (r.learned_actions[index]) {
      learned.values.push_back(static_cast<std::uint32_t>(index));
    }
  }
  v("learned_actions", learned);
  r.learned_actions.fill(false);
  for (const auto id : learned.values) r.learned_actions[id] = true;
  v("settings", r.settings);
  v("ashley", r.ashley);
  v("inventory", r.inventory);
  v("inventory_order", r.inventory_order);
  v("storage", r.storage);
  v("visits", r.visits);
  v("arts", r.arts);
  v("score", r.score);
  v("room_cache", r.room_cache);
  v("room_cache_slot", r.room_cache_slot);
  v("play_ticks", r.play_ticks);
  v("scene_timer", r.scene_timer);
  v("random", r.random);
  v("camera", r.camera);
  v("location", r.location);
}

template <class T>
constexpr bool kArray = std::is_array_v<T>;
template <class T, std::size_t N>
constexpr bool kArray<std::array<T, N>> = true;

template <class T>
Json EncodeValue(const T& value) {
  if constexpr (std::is_same_v<T, Reference>) {
    return std::format("{}/{}", value.prefix, value.value);
  } else if constexpr (std::is_same_v<T, Bits>) {
    if (value.maximum == 1 && value.value <= 1) return value.value != 0;
    return value.value;
  } else if constexpr (std::is_same_v<T, IdSet>) {
    auto array = Json::array();
    for (const auto id : value.values) {
      array.push_back(std::format("{}/{}", value.prefix, id));
    }
    return array;
  } else if constexpr (std::is_same_v<T, char>) {
    return static_cast<unsigned char>(value);
  } else if constexpr (std::is_integral_v<T>) {
    return value;
  } else if constexpr (kArray<T>) {
    auto array = Json::array();
    for (const auto& item : value) array.push_back(EncodeValue(item));
    return array;
  } else {
    T copy = value;
    auto object = Json::object();
    Fields(copy, [&](const char* name, const auto& field) {
      object[name] = EncodeValue(field);
    });
    return object;
  }
}

class Reader {
 public:
  bool Good() const { return error_.empty(); }
  const std::string& Error() const { return error_; }

  void Fail(std::string_view path, std::string_view detail) {
    if (Good()) error_ = std::format("{}: {}", path, detail);
  }

  template <class T>
  void Read(const Json& json, T& out, const std::string& path) {
    if (!Good()) return;
    if constexpr (std::is_same_v<T, Reference>) {
      ReadReference(json, out, path);
    } else if constexpr (std::is_same_v<T, Bits>) {
      ReadBits(json, out, path);
    } else if constexpr (std::is_same_v<T, IdSet>) {
      ReadIds(json, out, path);
    } else if constexpr (std::is_same_v<T, bool>) {
      if (!json.is_boolean()) {
        Fail(path, "expected a boolean");
      } else {
        out = json.get<bool>();
      }
    } else if constexpr (std::is_integral_v<T>) {
      ReadInteger(json, out, path);
    } else if constexpr (kArray<T>) {
      ReadArray(json, out, path);
    } else {
      ReadRecord(json, out, path);
    }
  }

 private:
  template <class T>
  void ReadInteger(const Json& json, T& out, const std::string& path) {
    using Number = std::conditional_t<std::is_same_v<T, char>, std::uint8_t, T>;
    if (!json.is_number_integer()) {
      Fail(path, "expected an integer");
      return;
    }
    if (json.is_number_unsigned()) {
      const auto number = json.get<std::uint64_t>();
      if (number >
          static_cast<std::uint64_t>(std::numeric_limits<Number>::max())) {
        Fail(path, "integer exceeds field range");
        return;
      }
      out = static_cast<T>(number);
    } else {
      const auto number = json.get<std::int64_t>();
      const bool negative_unsigned = std::is_unsigned_v<Number> && number < 0;
      const bool below_minimum =
          std::is_signed_v<Number> &&
          number <
              static_cast<std::int64_t>(std::numeric_limits<Number>::min());
      const bool above_maximum =
          number >= 0 &&
          static_cast<std::uint64_t>(number) >
              static_cast<std::uint64_t>(std::numeric_limits<Number>::max());
      if (negative_unsigned || below_minimum || above_maximum) {
        Fail(path, "integer exceeds field range");
        return;
      }
      out = static_cast<T>(number);
    }
  }

  template <class T>
  void ReadArray(const Json& json, T& out, const std::string& path) {
    if (!json.is_array() || json.size() != std::size(out)) {
      Fail(path, "incorrect array extent");
      return;
    }
    for (std::size_t index = 0; index < std::size(out); ++index) {
      Read(json[index], out[index], std::format("{}/{}", path, index));
    }
  }

  template <class T>
  void ReadRecord(const Json& json, T& out, const std::string& path) {
    if (!json.is_object()) {
      Fail(path, "expected an object");
      return;
    }
    std::size_t fields = 0;
    Fields(out, [&](const char* name, auto& field) {
      ++fields;
      const auto member = json.find(name);
      const auto child = std::format("{}/{}", path, name);
      if (member == json.end()) {
        Fail(child, "required field is missing");
      } else {
        Read(*member, field, child);
      }
    });
    if (fields != json.size()) Fail(path, "unknown or missing object fields");
  }

  void ReadReference(const Json& json, Reference& out,
                     const std::string& path) {
    if (!json.is_string()) {
      Fail(path, "expected a stable string ID");
      return;
    }
    const auto& text = json.get_ref<const std::string&>();
    const auto prefix = std::format("{}/", out.prefix);
    if (!text.starts_with(prefix) || text.size() == prefix.size()) {
      Fail(path, "incorrect ID kind or empty ID");
      return;
    }
    const std::string_view digits(text.data() + prefix.size(),
                                  text.size() - prefix.size());
    std::uint32_t id = 0;
    const auto result =
        std::from_chars(digits.data(), digits.data() + digits.size(), id);
    if (result.ec != std::errc() ||
        result.ptr != digits.data() + digits.size() || id > out.maximum ||
        (digits.size() > 1 && digits.front() == '0')) {
      Fail(path, "invalid or out-of-range ID");
      return;
    }
    out.value = id;
  }

  void ReadBits(const Json& json, Bits& out, const std::string& path) {
    if (out.maximum == 1) {
      if (!json.is_boolean()) {
        Fail(path, "expected a boolean");
        return;
      }
      out.value = json.get<bool>() ? 1u : 0u;
      return;
    }
    ReadInteger(json, out.value, path);
    if (out.value > out.maximum) Fail(path, "bit field exceeds source range");
  }

  void ReadIds(const Json& json, IdSet& out, const std::string& path) {
    if (!json.is_array() || json.size() > out.count) {
      Fail(path, "incorrect ID-set extent");
      return;
    }
    std::set<std::uint32_t> seen;
    out.values.clear();
    for (std::size_t index = 0; index < json.size(); ++index) {
      Reference id{out.prefix, out.count - 1, 0};
      ReadReference(json[index], id, std::format("{}/{}", path, index));
      if (!Good()) return;
      if (!seen.insert(id.value).second) {
        Fail(path, "duplicate ID");
        return;
      }
      out.values.push_back(id.value);
    }
  }

  std::string error_;
};

std::string Checksum(const Json& document) {
  std::uint64_t hash = kHashBasis;
  for (const unsigned char byte : document.dump()) {
    hash = (hash ^ byte) * kHashPrime;
  }
  return std::format("{:016X}", hash);
}

std::expected<Json, std::string> Parse(std::string_view data) {
  if (data.empty() || data.size() > VS::Save::SaveStore::kMaximumBytes) {
    return std::unexpected("/: invalid save byte count");
  }
  std::vector<std::set<std::string>> keys(kMaximumDepth + 2);
  std::string error;
  const auto callback = [&](int depth, Json::parse_event_t event, Json& value) {
    if (depth > kMaximumDepth) {
      error = "/: save nesting limit exceeded";
      return false;
    }
    if (event == Json::parse_event_t::object_start) keys[depth + 1].clear();
    if (event == Json::parse_event_t::key &&
        !keys[depth].insert(value.get<std::string>()).second) {
      error = "/: duplicate object key";
      return false;
    }
    return true;
  };
  auto document = Json::parse(data.begin(), data.end(), callback, false);
  if (!error.empty()) return std::unexpected(error);
  if (document.is_discarded()) {
    return std::unexpected("/: malformed JSON");
  }
  return document;
}

std::expected<void, std::string> ValidateModel(const Document& document) {
  const auto& state = document.state;
  if (document.generation == 0) {
    return std::unexpected("/generation: expected a positive generation");
  }
  if (document.saved_unix_ms > kMaximumUnixMilliseconds) {
    return std::unexpected("/saved_unix_ms: timestamp is beyond year 9999");
  }
  if (document.new_game_entry &&
      (document.new_game_entry->destination_zone_id != 1 ||
       document.new_game_entry->destination_room_id != 0)) {
    return std::unexpected("/new_game_entry: expected the original start room");
  }
  if (state.location.zone == 0 || state.location.zone > kMaximumLocationId ||
      state.location.room > kMaximumLocationId ||
      state.location.scene > kMaximumSceneId || state.location.facing < 0 ||
      state.location.facing > kMaximumAngle) {
    return std::unexpected(
        "/state/location: invalid zone, room, scene or facing");
  }
  if (state.location.floor_layer > 1 ||
      state.location.previous_tile.layer > 1 ||
      state.location.saved_placement.tile.layer > 1) {
    return std::unexpected(
        "/state/location: unsupported static collision layer");
  }
  if (state.room_cache_slot >= state.room_cache.size() ||
      state.play_ticks > kMaximumPlayTicks) {
    return std::unexpected("/state: invalid room-cache slot or play time");
  }
  if (state.random.shuffle_index >= state.random.shuffle.size() ||
      std::ranges::any_of(state.random.shuffle, [](auto value) {
        return value < 0 || value > kMaximumRandomValue;
      })) {
    return std::unexpected("/state/random: invalid shuffle state");
  }
  for (const auto& room : state.room_cache) {
    if (room.next_slot >= room.actors.size()) {
      return std::unexpected("/state/room_cache: invalid next slot");
    }
  }
  return {};
}

std::expected<Json, std::string> ReadEnvelope(std::string_view data) {
  auto document = Parse(data);
  if (!document) return std::unexpected(document.error());
  constexpr std::array kKeys{"format",     "version",       "source_game",
                             "generation", "saved_unix_ms", "state",
                             "checksum"};
  if (!document->is_object() || std::ranges::any_of(kKeys, [&](auto key) {
        return !document->contains(key);
      })) {
    return std::unexpected("/: incorrect save envelope fields");
  }
  if (!(*document)["format"].is_string() ||
      (*document)["format"].get_ref<const std::string&>() != kFormat ||
      !(*document)["source_game"].is_string() ||
      (*document)["source_game"].get_ref<const std::string&>() != kSource) {
    return std::unexpected("/: unsupported save format or source game");
  }
  std::uint32_t version = 0;
  Reader reader;
  reader.Read((*document)["version"], version, "/version");
  if (!reader.Good()) return std::unexpected(reader.Error());
  if (version != kVersion && version != kClearVersion) {
    return std::unexpected("/version: unsupported version");
  }
  const bool clear = version == kClearVersion;
  if (document->size() != kKeys.size() + (clear ? 1 : 0) ||
      document->contains("new_game_entry") != clear) {
    return std::unexpected("/: incorrect save envelope fields");
  }
  const auto checksum = document->find("checksum");
  if (!checksum->is_string()) {
    return std::unexpected("/checksum: expected a string");
  }
  const auto expected = checksum->get<std::string>();
  document->erase(checksum);
  if (Checksum(*document) != expected) {
    return std::unexpected("/checksum: save integrity check failed");
  }
  return document;
}
}  // namespace

std::expected<std::string, std::string> Encode(const Document& document) {
  const auto valid = ValidateModel(document);
  if (!valid) return std::unexpected(valid.error());
  Json encoded{{"format", kFormat},
               {"version", document.new_game_entry ? kClearVersion : kVersion},
               {"source_game", kSource},
               {"generation", document.generation},
               {"saved_unix_ms", document.saved_unix_ms},
               {"state", EncodeValue(document.state)}};
  if (document.new_game_entry) {
    encoded["new_game_entry"] = EncodeValue(*document.new_game_entry);
  }
  encoded["checksum"] = Checksum(encoded);
  auto text = encoded.dump(2);
  const auto checked = Decode(text);
  if (!checked) return std::unexpected(checked.error());
  return text;
}

std::expected<Document, std::string> Decode(std::string_view data) {
  const auto encoded = ReadEnvelope(data);
  if (!encoded) return std::unexpected(encoded.error());
  Document document;
  Reader reader;
  reader.Read((*encoded)["generation"], document.generation, "/generation");
  reader.Read((*encoded)["saved_unix_ms"], document.saved_unix_ms,
              "/saved_unix_ms");
  reader.Read((*encoded)["state"], document.state, "/state");
  if ((*encoded)["version"] == kClearVersion) {
    document.new_game_entry.emplace();
    reader.Read((*encoded)["new_game_entry"], *document.new_game_entry,
                "/new_game_entry");
  }
  if (!reader.Good()) return std::unexpected(reader.Error());
  const auto valid = ValidateModel(document);
  if (!valid) return std::unexpected(valid.error());
  return document;
}

std::expected<void, std::string> Validate(std::string_view data) {
  const auto document = Decode(data);
  if (!document) return std::unexpected(document.error());
  return {};
}

std::expected<bool, std::string> SameDocument(const Document& before,
                                              const Document& current) {
  const auto before_valid = ValidateModel(before);
  if (!before_valid) return std::unexpected(before_valid.error());
  const auto current_valid = ValidateModel(current);
  if (!current_valid) return std::unexpected(current_valid.error());
  if (before.generation != current.generation ||
      before.saved_unix_ms != current.saved_unix_ms ||
      before.new_game_entry.has_value() != current.new_game_entry.has_value()) {
    return false;
  }
  if (before.new_game_entry && EncodeValue(*before.new_game_entry) !=
                                   EncodeValue(*current.new_game_entry)) {
    return false;
  }
  return EncodeValue(before.state) == EncodeValue(current.state);
}
}  // namespace VS::Game::Save
