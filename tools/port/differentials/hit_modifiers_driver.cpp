// Test-only projection of original guarded storage for six hit modifiers.
#include "Battle/HitModifiers.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>

namespace {
namespace battle = VS::Battle;
constexpr std::uint32_t kTable = 0x800F1928, kActors = 0x80500000, kStats = 0x80600000;
constexpr std::array<std::uint32_t, 10> kAddresses{
    kTable, kActors, kActors + 0x100, kActors + 0x200, kStats, kStats + 0x1000,
    0x80700000, 0x80700100, 0x80700200, 0x800F1A04};
constexpr std::array<std::size_t, 10> kSizes{200, 112, 112, 112, 2436, 2436, 84, 100, 100, 36};
using Memory = std::array<std::uint32_t, 5728 / 4>;

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

struct World final : battle::HitModifierWorld {
  Memory memory{};
  unsigned action{};
  battle::HitModifierStats ReadStats(unsigned at) {
    battle::HitModifierStats stats;
    stats.actor_id = Read<std::uint8_t>(memory, at + 0x957);
    stats.risk = Read<std::uint16_t>(memory, at + 0x20);
    for (unsigned i = 0; i < 3; ++i) {
      stats.weapon_gems[i] = Read<std::uint8_t>(memory, at + 0xCA + 0x30 * i);
      stats.shield_gems[i] = Read<std::uint8_t>(memory, at + 0x232 + 0x30 * i);
    }
    for (unsigned i = 0; i < 6; ++i)
      stats.chain_evasion[i] = Read<std::uint8_t>(memory, at + 0x39F + 0xDC * i);
    return stats;
  }
  std::optional<battle::HitModifierStats> Stats(std::uint8_t slot) override {
    if (slot >= 42) return std::nullopt;
    const auto actor = Read<unsigned>(memory, kTable + slot * 4);
    if (!actor) return std::nullopt;
    const auto stats = Read<unsigned>(memory, actor + 0x3C);
    return stats ? std::optional{ReadStats(stats)} : std::nullopt;
  }
  std::optional<std::uint32_t> WeaponDrawn(std::uint8_t slot) override {
    if (slot >= 42) return std::nullopt;
    const auto actor = Read<unsigned>(memory, kTable + slot * 4);
    return actor ? std::optional{Read<unsigned>(memory, actor + 0x20)} : std::nullopt;
  }
  std::optional<battle::HitModifierAction> Action() override {
    if (!action) return std::nullopt;
    return battle::HitModifierAction{
        Read<std::uint8_t>(memory, action),
        static_cast<std::uint8_t>((Read<unsigned>(memory, action) >> 17) & 7),
        {static_cast<std::uint8_t>(Read<unsigned>(memory, action + 0x14) & 127),
         static_cast<std::uint8_t>(Read<unsigned>(memory, action + 0x18) & 127)}};
  }
  std::optional<battle::HitModifierTarget> Hit(unsigned at) {
    if (!at) return std::nullopt;
    return battle::HitModifierTarget{
        Read<std::uint8_t>(memory, at), Read<std::int8_t>(memory, at + 1),
        Read<std::int32_t>(memory, at + 0x40)};
  }
};

template <typename T>
const T* Pointer(const std::optional<T>& value) { return value ? &value.value() : nullptr; }
}  // namespace

int main() {
  std::string line;
  while (std::getline(std::cin, line)) {
    std::istringstream input(line);
    unsigned function{}, source{}, target{}, stats{}, hit{}, execute{};
    World world;
    input >> function >> world.action >> source >> target >> stats >> hit >> execute;
    for (auto& word : world.memory) input >> word;
    if (!input || function > 5) return 2;
    const auto from = world.Hit(source), to = world.Hit(target);
    const auto state = world.ReadStats(stats);
    const auto index = std::bit_cast<int>(hit);
    std::optional<int> result;
    if (function == 0) result = battle::AttackGemHitBonus(&state, world);
    if (function == 1) result = battle::DefenseGemHitBonus(&state, index, world);
    if (function == 2) result = battle::PreviousHitSuccessThreshold(Read<int>(world.memory, 0x800F1A04));
    if (function == 3) result = battle::GemHitChance(Pointer(from), Pointer(to), index, world);
    if (function == 4) result = battle::RiskHitChance(Pointer(from), world);
    if (function == 5) result = battle::ChainHitChance(Pointer(to), world);
    std::cout << result.has_value() << ' ' << static_cast<unsigned>(result.value_or(0));
    for (const auto word : world.memory) std::cout << ' ' << word;
    std::cout << '\n';
  }
}
