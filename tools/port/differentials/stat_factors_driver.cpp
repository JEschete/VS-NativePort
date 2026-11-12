// Test-only projection of minimal guarded original records; no service stubs.
#include "Battle/StatFactors.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {
namespace battle = VS::Battle;
constexpr std::uint32_t kGlobal = 0x800F19CC, kTable = 0x800F1928,
                        kDefinitions = 0x8004B9DC;
struct Block {
  std::uint32_t address{};
  std::vector<std::uint32_t> words;
};
struct World final : battle::StatFactorWorld {
  std::vector<Block> blocks;
  std::uint32_t action{}, source{}, target{};
  template<class T> T Read(std::uint32_t address) const {
    for (const auto& block : blocks) {
      if (address >= block.address &&
          address + sizeof(T) <= block.address + block.words.size() * 4) {
        T value{};
        std::memcpy(&value, reinterpret_cast<const std::byte*>(block.words.data()) +
                                address - block.address, sizeof(T));
        return value;
      }
    }
    std::abort();
  }
  std::uint32_t State() const { return Read<std::uint32_t>(kGlobal); }
  std::uint32_t Stats(std::uint32_t hit) const {
    return Read<std::uint32_t>(Read<std::uint32_t>(
        kTable + Read<std::uint8_t>(hit) * 4) + 0x3C);
  }
  battle::StatFactorHit Amounts(std::uint32_t address) const {
    return {Read<std::int16_t>(address), Read<std::int16_t>(address + 2)};
  }
  std::optional<std::uint16_t> QueueIndex() override {
    return Read<std::uint16_t>(State());
  }
  std::optional<std::uint8_t> Factor(int hit) override {
    if (!action || hit < 0 || hit > 1) return std::nullopt;
    return (Read<std::uint32_t>(action + 0x14 + hit * 4) >> 22) & 31;
  }
  std::optional<battle::StatFactorHit> PreviousTarget() override {
    return Amounts(State() + 0x58);
  }
  std::optional<battle::StatFactorVital> SourceVital(battle::StatFactorResource resource) override {
    const auto address = Stats(source) + (resource == battle::StatFactorResource::kHp ? 0x18 : 0x1C);
    return battle::StatFactorVital{Read<std::int16_t>(address), Read<std::int16_t>(address + 2)};
  }
  std::optional<std::int16_t> TargetHp() override {
    return Read<std::int16_t>(Stats(target) + 0x18);
  }
  std::optional<std::uint16_t> WeaponPp() override {
    return Read<std::uint16_t>(Stats(source) + 0x14C);
  }
  std::optional<std::uint8_t> SelectedActionType() override {
    return (Read<std::uint32_t>(kDefinitions + Read<std::uint16_t>(State() + 8) * 52) >> 17) & 7;
  }
  std::optional<std::uint8_t> PrimaryHpEffect() override {
    return Read<std::uint32_t>(State() + 0x28) & 3;
  }
  std::optional<battle::StatFactorHit> PrimaryHit() override {
    return Amounts(State() + 0x10);
  }
  std::uint32_t Crc() const {
    std::uint32_t crc = 0xFFFFFFFF;
    for (const auto& block : blocks) {
      for (auto word : block.words) {
        for (int byte = 0; byte < 4; ++byte) {
          crc ^= (word >> (byte * 8)) & 255;
          for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xEDB88320U & (0U - (crc & 1)));
        }
      }
    }
    return crc ^ 0xFFFFFFFF;
  }
};
}
int main() {
  std::string line;
  while (std::getline(std::cin, line)) {
    World world;
    std::istringstream input(line);
    unsigned function{}, hit{}, execute{}, count{};
    input >> function >> world.action >> world.source >> world.target >> hit >> execute >> count;
    for (unsigned i = 0; i < count; ++i) {
      Block block;
      unsigned words{};
      input >> block.address >> words;
      block.words.resize(words);
      for (auto& word : block.words) input >> word;
      world.blocks.push_back(std::move(block));
    }
    const auto before = world.Crc();
    const std::array calculators{battle::ScalePreviousTargetHit, battle::ScaleSourceMissingHp,
        battle::ScalePreviousTargetHit, battle::ScaleTargetCurrentHp,
        +[](int, battle::StatFactorWorld& w) { return battle::WeaponPpWithChainOffset(w); },
        battle::ScaleIncomingSpellDamage, battle::ScaleSourceMissingMp, battle::ScalePreviousTargetHit};
    const auto result = calculators.at(function)(static_cast<int>(hit), world);
    const auto after = world.Crc();
    std::cout << result.has_value() << ' ' << static_cast<std::uint32_t>(result.value_or(0))
              << ' ' << after << ' ' << (before == after) << '\n';
  }
}
