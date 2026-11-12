// F00194/F00522/F00525/F00526; test-only legacy image projection.
#include "Battle/ActionResults.hpp"
#include "Battle/ActorVitals.hpp"

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
constexpr std::uint32_t kTable = 0x800F1928, kActors = 0x80500000,
                        kStats = 0x80600000, kHit = 0x80700000,
                        kCounter = 0x800E8490, kTicks = 0x8005E24C;
constexpr std::array<std::uint32_t, 10> kAddresses{
    kTable, kActors, kActors + 0x100, kActors + 0x200,
    kStats, kStats + 0x1000, kStats + 0x2000, kHit, kCounter, kTicks};
constexpr std::array<std::size_t, 10> kSizes{200, 112, 112, 112, 2436, 2436, 2436, 100, 40, 36};
constexpr std::size_t kBytes = 8020;
using Memory = std::array<std::uint32_t, kBytes / 4>;
using Event = std::array<std::uint32_t, 6>;

std::size_t Offset(std::uint32_t address) {
  std::size_t offset = 0;
  for (std::size_t i = 0; i < kAddresses.size(); ++i) {
    const auto start = kAddresses[i] - 16;
    if (address >= start && address < start + kSizes[i]) return offset + address - start;
    offset += kSizes[i];
  }
  std::abort();
}
template <typename T>
T Read(const Memory& memory, std::uint32_t address) {
  T result{};
  std::memcpy(&result, reinterpret_cast<const std::byte*>(memory.data()) + Offset(address), sizeof(T));
  return result;
}
template <typename T>
void Write(Memory& memory, std::uint32_t address, T value) {
  std::memcpy(reinterpret_cast<std::byte*>(memory.data()) + Offset(address), &value, sizeof(T));
}
std::uint32_t Crc(const Memory& memory) {
  std::uint32_t crc = 0xFFFFFFFF;
  const auto* bytes = reinterpret_cast<const unsigned char*>(memory.data());
  for (std::size_t i = 0; i < sizeof(memory); ++i) {
    crc ^= bytes[i];
    for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1U) ^ (0xEDB88320U & (0U - (crc & 1U)));
  }
  return crc ^ 0xFFFFFFFF;
}

struct Stats final : battle::ActorVitalsAccess {
  Memory* memory{};
  std::uint32_t at{};
  battle::ActorVitalsState Read() override {
    battle::ActorVitalsState state;
    auto& status = state.status;
    status.hp = ::Read<std::int16_t>(*memory, at + 0x18);
    status.max_hp = ::Read<std::int16_t>(*memory, at + 0x1A);
    state.mp = ::Read<std::int16_t>(*memory, at + 0x1C);
    state.max_mp = ::Read<std::int16_t>(*memory, at + 0x1E);
    state.risk = ::Read<std::uint16_t>(*memory, at + 0x20);
    state.recovery = ::Read<std::uint16_t>(*memory, at + 0x954);
    status.strength = ::Read<battle::StatusAttributes>(*memory, at + 0x22);
    status.intelligence = ::Read<battle::StatusAttributes>(*memory, at + 0x26);
    status.agility = ::Read<battle::StatusAttributes>(*memory, at + 0x2A);
    status.statuses = ::Read<std::uint32_t>(*memory, at + 0x948);
    status.weapon = ::Read<items::WeaponLoadout>(*memory, at + 0x3C);
    status.shield = ::Read<items::ShieldLoadout>(*memory, at + 0x1D4);
    for (unsigned i = 0; i < 6; ++i) {
      status.limbs[i].hp = ::Read<std::int16_t>(*memory, at + 0x398 + i * 0xDC);
      status.limbs[i].max_hp = ::Read<std::int16_t>(*memory, at + 0x39A + i * 0xDC);
      status.limbs[i].armor = ::Read<items::ArmorLoadout>(*memory, at + 0x3B8 + i * 0xDC);
    }
    return state;
  }
  void Write(const battle::ActorVitalsState& state) override {
    const auto& status = state.status;
    ::Write(*memory, at + 0x18, status.hp);
    ::Write(*memory, at + 0x1A, status.max_hp);
    ::Write(*memory, at + 0x1C, state.mp);
    ::Write(*memory, at + 0x1E, state.max_mp);
    ::Write(*memory, at + 0x20, state.risk);
    ::Write(*memory, at + 0x954, state.recovery);
    ::Write(*memory, at + 0x22, status.strength);
    ::Write(*memory, at + 0x26, status.intelligence);
    ::Write(*memory, at + 0x2A, status.agility);
    ::Write(*memory, at + 0x948, status.statuses);
    ::Write(*memory, at + 0x3C, status.weapon);
    ::Write(*memory, at + 0x1D4, status.shield);
    for (unsigned i = 0; i < 6; ++i) {
      ::Write(*memory, at + 0x398 + i * 0xDC, status.limbs[i].hp);
      ::Write(*memory, at + 0x39A + i * 0xDC, status.limbs[i].max_hp);
      ::Write(*memory, at + 0x3B8 + i * 0xDC, status.limbs[i].armor);
    }
  }
};

struct Actor final : battle::VitalsActor {
  Memory* memory{};
  std::array<::Stats, 3>* states{};
  std::uint32_t at{};
  std::uint8_t SpawnIndex() override { return Read<std::uint8_t>(*memory, at + 0x27); }
  std::uint8_t RetainAfterDefeat() override { return Read<std::uint8_t>(*memory, at + 0x26); }
  std::uint32_t WeaponDrawn() override { return Read<std::uint32_t>(*memory, at + 0x20); }
  battle::ActorVitalsAccess* Stats() override {
    const auto pointer = Read<std::uint32_t>(*memory, at + 0x3C);
    return pointer == 0 ? nullptr : &states->at((pointer - kStats) / 0x1000);
  }
};

struct World final : battle::ActorVitalsWorld, battle::ActorActionResultWorld {
  Memory memory{};
  std::array<Stats, 3> states{};
  std::array<Actor, 3> actors{};
  unsigned mutations{}, trigger{}, timer{}, result_owner = kStats, current_slot{};
  std::array<unsigned, 42> flags{}, busy{};
  std::vector<Event> events;
  World() {
    for (unsigned i = 0; i < 3; ++i) {
      states[i].memory = &memory;
      states[i].at = kStats + i * 0x1000;
      actors[i].memory = &memory;
      actors[i].states = &states;
      actors[i].at = kActors + i * 0x100;
    }
  }
  void Observe(unsigned tag, unsigned a = 0, unsigned b = 0, unsigned c = 0, unsigned d = 0) {
    events.push_back({tag, a, b, c, d, Crc(memory)});
    if (tag == 0) {
      if (mutations & 1) {
        Write(memory, kHit, static_cast<std::uint8_t>((Read<std::uint8_t>(memory, kHit) + 1) % 3));
        Write<std::uint8_t>(memory, kHit + 3, 4);
      }
      const auto actor = Read<std::uint32_t>(memory, kTable + Read<std::uint8_t>(memory, kHit) * 4);
      result_owner = Read<std::uint32_t>(memory, actor + 0x3C);
    }
    if (tag == 1 && (mutations & 2)) {
      Write<std::uint16_t>(memory, result_owner + 0x18, Read<std::uint16_t>(memory, result_owner + 0x18) + 1);
      Write<std::uint16_t>(memory, kHit + 4, Read<std::uint16_t>(memory, kHit + 4) + 3);
      Write(memory, kHit + 0x1C, Read<std::uint32_t>(memory, kHit + 0x1C) ^ 3U);
    }
    if ((tag == 1 || tag == 2) && (mutations & 4)) {
      Write<std::uint8_t>(memory, kHit, 2);
      Write(memory, kTable, kActors + 0x100);
      Write(memory, kActors + 0x3C, kStats + 0x2000);
    }
    if (tag == 2 && (mutations & 8)) {
      const auto offset = d ? 0x18U : 0x14U;
      Write(memory, kHit + offset, Read<std::uint32_t>(memory, kHit + offset) ^ c);
      Write<std::uint16_t>(memory, result_owner + 0x18, 0);
    }
    if (tag == 3 || tag == 4) {
      const auto active = Read<std::uint32_t>(memory, b + 0x948);
      Write(memory, b + 0x948, tag == 3 ? active & ~a : active | a);
      if (mutations & 16) {
        Write<std::uint16_t>(memory, b + 0x18, 3);
        Write(memory, kHit + 0x14, Read<std::uint32_t>(memory, kHit + 0x14) | 0x80000000U);
      }
    }
    if (tag == 5 && (mutations & 32)) {
      const auto actor = Read<std::uint32_t>(memory, kTable + a * 4);
      Write(memory, actor + 0x3C, kStats + 0x2000);
      Write(memory, kTable + a * 4, kActors + 0x100);
    }
    if (tag == 7 && (mutations & 64)) Write(memory, kTable + b * 4, kActors + 0x200);
    if (tag == 9) Write<std::uint32_t>(memory, kTable + a * 4, 0);
    if (tag == 14) {
      if (mutations & 128) {
        Write<std::uint16_t>(memory, a + 0x18, mutations & 1 ? 0 : 1);
        Write<std::uint8_t>(memory, kActors + 0x26, mutations & 2 ? 1 : 0);
      }
      if (mutations & 64) {
        Write(memory, kTable + 4, kActors + 0x200);
        Write<std::uint32_t>(memory, kCounter, 31);
        Write<std::uint32_t>(memory, kTicks, 9);
      }
    }
  }
  void CommitEquipmentStats() override { Observe(0); }
  battle::ActorActionResult ReadHit() override {
    battle::ActorActionResult hit;
    hit.target = Read<std::uint8_t>(memory, kHit);
    hit.limb = Read<std::int8_t>(memory, kHit + 1);
    hit.impact = Read<std::uint8_t>(memory, kHit + 3);
    constexpr std::array<unsigned, 12> offsets{4, 6, 8, 10, 12, 14, 16, 18, 0x38, 0x3A, 0x3C, 0x3E};
    constexpr std::array<unsigned, 12> shifts{0, 2, 4, 6, 8, 10, 12, 14, 18, 20, 22, 24};
    const auto modes = Read<std::uint32_t>(memory, kHit + 0x1C);
    for (unsigned i = 0; i < 12; ++i) {
      hit.changes[i] = {Read<std::int16_t>(memory, kHit + offsets[i]),
                         static_cast<std::uint8_t>((modes >> shifts[i]) & 3)};
    }
    hit.inflict_statuses = Read<std::uint32_t>(memory, kHit + 0x18);
    hit.remove_statuses = Read<std::uint32_t>(memory, kHit + 0x14);
    for (unsigned i = 0; i < 6; ++i) hit.limbs[i] = Read<battle::ActionResultLimb>(memory, kHit + 0x20 + i * 4);
    return hit;
  }
  battle::ActorVitalsAccess* TargetStats(std::uint8_t slot) override {
    auto* actor = ActorAt(slot);
    return actor ? actor->Stats() : nullptr;
  }
  void Number(int slot, int limb, std::uint32_t amount, int label) override { Observe(1, slot, limb, amount, label); }
  void StatusLabel(int slot, std::uint32_t mask, bool removing) override { Observe(2, slot, 0, mask, removing); }
  void RemoveStatuses(battle::ActorVitalsAccess& stats, std::uint32_t mask) override {
    Observe(3, mask, static_cast<::Stats&>(stats).at);
  }
  void InflictStatuses(battle::ActorVitalsAccess& stats, std::uint32_t mask) override {
    Observe(4, mask, static_cast<::Stats&>(stats).at);
  }
  battle::VitalsActor* ActorAt(int slot) override {
    const auto actor = Read<std::uint32_t>(memory, kTable + slot * 4);
    return actor ? &actors.at((actor - kActors) / 0x100) : nullptr;
  }
  std::uint32_t StateFlags(int slot) override { current_slot = slot; Observe(5, slot); return flags.at(slot); }
  std::int32_t ActionAnimationBusy(int slot) override { Observe(6, slot); return std::bit_cast<int>(busy.at(slot)); }
  void ResolveEnemyDefeat(int source, int target, int count) override { Observe(7, source, target, count); }
  void RestoreIdle(int slot) override { Observe(8, slot); }
  void RemoveActor(int slot, int reason) override { Observe(9, slot, reason); }
  void SuppressAshleyLabels() override { Observe(10); }
  std::int16_t EventTrigger(int event) override {
    Observe(11, event);
    if (event == 12 && (mutations & 256)) return current_slot == 15 ? 1 : 2;
    return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(trigger));
  }
  void BeginQueuedCutscene() override { Observe(12); }
  void BeginDefeatAnimation(int slot) override { Observe(13, slot); }
  void StatusTimers(battle::ActorVitalsAccess& stats) override { Observe(14, static_cast<::Stats&>(stats).at); }
  std::int32_t Counter() override { return Read<std::int32_t>(memory, kCounter); }
  void SetCounter(std::int32_t value) override { Write(memory, kCounter, value); }
  std::int32_t Ticks() override { return Read<std::int32_t>(memory, kTicks); }
  std::int32_t RegeneratingLimb() override { return Read<std::int32_t>(memory, kCounter + 4); }
  void SetRegeneratingLimb(std::int32_t value) override { Write(memory, kCounter + 4, value); }
  int SceneTimer() override { Observe(15); return std::bit_cast<int>(timer); }
};
}  // namespace

int main() {
  std::string line;
  while (std::getline(std::cin, line)) {
    std::istringstream input(line);
    unsigned function{}, source{}, target{}, count{};
    World world;
    input >> function >> source >> target >> count >> world.mutations >> world.trigger >> world.timer;
    for (auto& value : world.flags) input >> value;
    for (auto& value : world.busy) input >> value;
    for (auto& value : world.memory) input >> value;
    if (!input || function > 3) return 2;
    std::optional<int> result = 0;
    if (function == 0) result = battle::ResolveActorDefeat(std::bit_cast<int>(source), target, std::bit_cast<int>(count), world);
    if (function == 1 && !battle::ApplyActorActionResult(world)) result = std::nullopt;
    if (function == 2) result = battle::UpdateActorVitals(world);
    if (function == 3) result = battle::UpdateRoomActorVitals(world);
    std::cout << result.has_value() << ' ' << static_cast<unsigned>(result.value_or(0));
    for (const auto value : world.memory) std::cout << ' ' << value;
    std::cout << ' ' << world.events.size();
    for (const auto& event : world.events) for (const auto value : event) std::cout << ' ' << value;
    std::cout << '\n';
  }
}
