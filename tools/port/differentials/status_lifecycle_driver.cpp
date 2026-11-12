// F00254/F00517/F00520/F00523: native policy driver, with test-only ABI projection.
#include "Battle/StatusLifecycle.hpp"
#include "Items/EquipmentAffinity.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {
namespace battle = VS::Battle;
namespace items = VS::Items;
using State = battle::StatusLifecycleState;
using Image = std::array<std::uint32_t, (0x964 + 32) / 4>;
using Event = std::array<std::uint32_t, 6>;

template <typename T>
T Read(const Image& image, std::size_t offset) {
  T value{};
  std::memcpy(&value, reinterpret_cast<const std::byte*>(image.data()) + 16 + offset, sizeof(value));
  return value;
}
template <typename T>
void Write(Image& image, std::size_t offset, const T& value) {
  std::memcpy(reinterpret_cast<std::byte*>(image.data()) + 16 + offset, &value, sizeof(value));
}

State Decode(const Image& image) {
  State state{};
  state.statuses = Read<std::uint32_t>(image, 0x948);
  state.timers = Read<decltype(state.timers)>(image, 0x94C);
  state.hp = Read<std::int16_t>(image, 0x18);
  state.max_hp = Read<std::int16_t>(image, 0x1A);
  state.strength = Read<battle::StatusAttributes>(image, 0x22);
  state.intelligence = Read<battle::StatusAttributes>(image, 0x26);
  state.agility = Read<battle::StatusAttributes>(image, 0x2A);
  state.base_walk = Read<std::uint8_t>(image, 0x2E);
  state.base_run = Read<std::uint8_t>(image, 0x2F);
  state.walk = Read<std::uint8_t>(image, 0x31);
  state.run = Read<std::uint8_t>(image, 0x33);
  state.mobility_limbs = Read<decltype(state.mobility_limbs)>(image, 0x34);
  state.actor_id = Read<std::uint8_t>(image, 0x957);
  state.analyzed = (Read<std::uint8_t>(image, 0x956) & 2) != 0;
  state.weapon = Read<items::WeaponLoadout>(image, 0x3C);
  state.shield = Read<items::ShieldLoadout>(image, 0x1D4);
  for (std::size_t i = 0; i < state.limbs.size(); ++i) {
    state.limbs[i] = {Read<std::int16_t>(image, 0x398 + i * 0xDC),
                      Read<std::int16_t>(image, 0x39A + i * 0xDC),
                      Read<std::uint8_t>(image, 0x39E + i * 0xDC),
                      Read<items::ArmorLoadout>(image, 0x3B8 + i * 0xDC)};
  }
  return state;
}

Image Encode(const State& state, Image image) {
  Write(image, 0x948, state.statuses);
  Write(image, 0x94C, state.timers);
  Write(image, 0x18, state.hp);
  Write(image, 0x1A, state.max_hp);
  Write(image, 0x22, state.strength);
  Write(image, 0x26, state.intelligence);
  Write(image, 0x2A, state.agility);
  Write(image, 0x2E, state.base_walk);
  Write(image, 0x2F, state.base_run);
  Write(image, 0x31, state.walk);
  Write(image, 0x33, state.run);
  Write(image, 0x34, state.mobility_limbs);
  Write(image, 0x957, state.actor_id);
  Write(image, 0x956, static_cast<std::uint8_t>(
      (Read<std::uint8_t>(image, 0x956) & ~2U) | (state.analyzed ? 2U : 0U)));
  Write(image, 0x3C, state.weapon);
  Write(image, 0x1D4, state.shield);
  for (std::size_t i = 0; i < state.limbs.size(); ++i) {
    Write(image, 0x398 + i * 0xDC, state.limbs[i].hp);
    Write(image, 0x39A + i * 0xDC, state.limbs[i].max_hp);
    Write(image, 0x39E + i * 0xDC, state.limbs[i].name_index);
    Write(image, 0x3B8 + i * 0xDC, state.limbs[i].armor);
  }
  return image;
}

std::uint32_t Crc(const Image& image) {
  std::uint32_t crc = 0xFFFFFFFF;
  const auto* bytes = reinterpret_cast<const unsigned char*>(image.data());
  for (std::size_t i = 0; i < sizeof(image); ++i) {
    crc ^= bytes[i];
    for (int bit = 0; bit < 8; ++bit) {
      crc = (crc >> 1U) ^ (0xEDB88320U & (0U - (crc & 1U)));
    }
  }
  return crc ^ 0xFFFFFFFF;
}

struct World final : battle::StatusLifecycleWorld {
  const Image original;
  const std::array<std::uint32_t, 32> draws;
  const unsigned mutations;
  std::vector<Event> events;
  unsigned next = 0;
  World(Image image, std::array<std::uint32_t, 32> random, unsigned mutate)
      : original(image), draws(random), mutations(mutate) {}

  void Observe(State& state, unsigned tag, unsigned a = 0, unsigned b = 0,
               unsigned c = 0, unsigned d = 0) {
    events.push_back({tag, a, b, c, d, Crc(Encode(state, original))});
    if ((mutations & 1U) != 0 && (tag == 0 || tag == 1 || tag == 3)) {
      state.hp = static_cast<std::int16_t>(state.hp + 1);
      for (auto& limb : state.limbs) ++limb.name_index;
    }
    if ((mutations & 2U) != 0 && tag == 2) {
      state.intelligence.current ^= 0xAAAA;
      state.weapon.attributes.current_intelligence ^= 0x5555;
    }
    if ((mutations & 4U) != 0 && tag == 5) {
      state.strength.total ^= 0x1234;
      state.statuses ^= 0x40000000;
    }
  }
  std::int32_t RandomBounded(State& state, int bound) override {
    Observe(state, 0, bound);
    return static_cast<int>(draws[next++ % draws.size()] % bound);
  }
  std::int32_t RandomAveraged(State& state, int bound) override {
    Observe(state, 1, bound);
    return static_cast<int>(draws[next++ % draws.size()] % bound);
  }
  void UpdateMovement(State& state) override { Observe(state, 2, state.actor_id, state.walk, state.run); }
  void EquipmentLabel(State& state, int label, int stat, std::uint32_t amount) override {
    Observe(state, 3, label, stat, amount);
  }
  void ValueLabel(State& state, std::uint32_t amount) override { Observe(state, 4, state.actor_id, 0, amount, 0); }
  void StatusLabels(State& state, std::uint32_t statuses) override { Observe(state, 5, state.actor_id, 0, statuses, 1); }
  void RebuildWeapon(State& state) override {
    Observe(state, 6);
    items::RebuildWeaponClassAffinity(state.weapon);
  }
  void RebuildShield(State& state) override {
    Observe(state, 7);
    items::RebuildShieldClassAffinity(state.shield);
  }
  void RebuildArmor(State& state, std::size_t index) override {
    Observe(state, 8, static_cast<unsigned>(index));
    items::RebuildArmorClassAffinity(state.limbs[index].armor);
  }
  void EnchantWeapon(State& state, const battle::ElementEnchantment& change) override {
    Observe(state, 9, change.increased_affinity, change.decreased_affinity, change.amount);
    battle::ApplyWeaponElementEnchantment(state.weapon, change);
  }
  void EnchantArmor(State& state, const battle::ElementEnchantment& change) override {
    Observe(state, 10, change.increased_affinity, change.decreased_affinity, change.amount);
    std::array<items::ArmorLoadout, items::kLimbCount> armor{};
    for (std::size_t i = 0; i < armor.size(); ++i) armor[i] = state.limbs[i].armor;
    battle::ApplyArmorElementEnchantment(state.shield, armor, change);
    for (std::size_t i = 0; i < armor.size(); ++i) state.limbs[i].armor = armor[i];
  }
  void RefreshEquipmentDisplay(State& state) override { Observe(state, 11); }
};
struct Guarded {
  std::array<std::uint32_t, 4> before;
  State state;
  std::array<std::uint32_t, 4> after;
};
}  // namespace

extern "C" void Core_Debugf(const char*, const char*, ...) {}

int main() {
  std::string line;
  while (std::getline(std::cin, line)) {
    std::istringstream in(line);
    unsigned function = 0, requested = 0, mutations = 0;
    std::array<unsigned, 6> durations{};
    std::array<std::uint32_t, 4> flags{}, guards{};
    std::array<std::uint32_t, 32> draws{};
    Image image{};
    in >> function >> requested >> mutations;
    for (auto& v : durations) in >> v;
    for (auto& v : flags) in >> v;
    for (auto& v : guards) in >> v;
    for (auto& v : draws) in >> v;
    for (auto& v : image) in >> v;
    if (!in || function > 3) return 2;
    battle::StatusDurations times{
        static_cast<std::uint8_t>(durations[0]), static_cast<std::uint8_t>(durations[1]),
        static_cast<std::uint8_t>(durations[2]), static_cast<std::uint8_t>(durations[3]),
        static_cast<std::uint8_t>(durations[4]), static_cast<std::uint8_t>(durations[5])};
    Guarded actor{guards, Decode(image), guards};
    World world(image, draws, mutations);
    bool accepted = true;
    if (function == 0) accepted = battle::PreserveAshleyStatuses(actor.state, world);
    if (function == 1) accepted = battle::RemoveActorStatuses(actor.state, requested, world);
    if (function == 2) battle::InflictActorStatuses(actor.state, requested, times, world);
    if (function == 3) accepted = battle::UpdateActorStatusTimers(actor.state, times, flags, world);
    std::cout << !accepted << ' ' << (actor.before == guards && actor.after == guards);
    for (const auto word : Encode(actor.state, image)) std::cout << ' ' << word;
    std::cout << ' ' << world.events.size();
    for (const auto& event : world.events) for (const auto word : event) std::cout << ' ' << word;
    std::cout << '\n';
  }
}
