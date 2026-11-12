// Minimal guarded-record projection; executes the owned policy, not a second model.
#include "Battle/DamagePolicies.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {
namespace b = VS::Battle;
constexpr unsigned kTable=0x800F1928, kLast=0x800F1A08, kDp=0x800F1A10;
struct Block { unsigned address{}; std::vector<unsigned> words; };
struct Memory {
  std::vector<Block> blocks;
  template<class T> T Read(unsigned address) const {
    for (const auto& block : blocks) {
      if (address >= block.address && address + sizeof(T) <= block.address + block.words.size()*4) {
        T value{};
        std::memcpy(&value, reinterpret_cast<const std::byte*>(block.words.data()) +
                             address-block.address, sizeof(T));
        return value;
      }
    }
    std::abort();
  }
  void Write(unsigned address, std::int16_t value) {
    for (auto& block : blocks) {
      if (address >= block.address && address+2 <= block.address+block.words.size()*4) {
        std::memcpy(reinterpret_cast<std::byte*>(block.words.data())+address-block.address, &value, 2);
        return;
      }
    }
    std::abort();
  }
  unsigned Stats(unsigned hit) const {
    return Read<unsigned>(Read<unsigned>(kTable+4*Read<std::uint8_t>(hit))+0x3C);
  }
  unsigned Crc() const {
    unsigned crc = 0xFFFFFFFF;
    for (const auto& block : blocks) for (auto word : block.words) {
      for (int byte=0; byte<4; ++byte) {
        crc ^= (word>>(byte*8))&255;
        for (int bit=0; bit<8; ++bit) crc=(crc>>1)^(0xEDB88320U&(0U-(crc&1)));
      }
    }
    return crc^0xFFFFFFFF;
  }
};
std::int16_t Gems(const Memory& m, const std::array<unsigned, 5>& args) {
  std::array<VS::Items::EquipmentPart, 3> source{}, target{};
  for (unsigned i=0; i<3; ++i) {
    source[i].gem_effects=m.Read<std::uint8_t>(args[2]+0xCA+0x30*i);
    target[i].gem_effects=m.Read<std::uint8_t>(args[3]+0x232+0x30*i);
  }
  return b::ApplyDamageGemMultiplier((m.Read<unsigned>(args[0])>>17)&7,
      static_cast<std::int16_t>(args[1]), source, target);
}
void Limbs(Memory& m, unsigned function, const std::array<unsigned, 5>& args) {
  const auto hit=args[1], stats=m.Stats(hit);
  std::array<std::int16_t, 6> hp{};
  std::array<std::int16_t, 8> dp{};
  std::array<b::StatFactorVital, 6> vitals{};
  std::array<std::uint8_t, 6> splash{};
  for (unsigned i=0; i<6; ++i) hp[i]=m.Read<std::int16_t>(hit+0x20+4*i);
  for (unsigned i=0; i<8; ++i) dp[i]=m.Read<std::int16_t>(kDp+2*i);
  const auto amount=static_cast<std::int16_t>(args[2]);
  if (function==2) {
    for (unsigned i=0; i<6; ++i) {
      vitals[i]={m.Read<std::int16_t>(stats+0x398+0xDC*i),
                 m.Read<std::int16_t>(stats+0x39A+0xDC*i)};
    }
    b::DistributeLimbRecovery(amount, vitals, hp, dp);
  } else {
    const auto category=(m.Read<unsigned>(args[0])>>20)&15;
    const auto limb=m.Read<std::int8_t>(hit+1);
    const auto kind=b::DamageDistribution(category);
    std::span<const b::StatFactorVital> used_vitals;
    std::span<const std::uint8_t> used_splash;
    if (kind==b::LimbDistribution::kEven) {
      for (unsigned i=0; i<6; ++i) vitals[i].maximum=m.Read<std::int16_t>(stats+0x39A+0xDC*i);
      used_vitals=vitals;
    } else if (kind==b::LimbDistribution::kSplash) {
      for (unsigned i=0; i<6; ++i) splash[i]=m.Read<std::uint8_t>(stats+0x920+limb*6+i);
      used_splash=splash;
    }
    if (!b::DistributeHitLimbDamage(category, limb, amount, used_vitals, used_splash, hp, dp)) std::abort();
  }
  for (unsigned i=0; i<6; ++i) m.Write(hit+0x20+4*i, hp[i]);
  for (unsigned i=0; i<8; ++i) m.Write(kDp+2*i, dp[i]);
}
}
int main() {
  std::string line;
  while (std::getline(std::cin, line)) {
    Memory memory;
    std::istringstream input(line);
    unsigned function{}, count{};
    std::array<unsigned, 5> args{};
    input>>function;
    for (auto& arg:args) input>>arg;
    input>>count;
    for (unsigned i=0; i<count; ++i) {
      Block block;
      unsigned words{};
      input>>block.address>>words;
      block.words.resize(words);
      for (auto& word:block.words) input>>word;
      memory.blocks.push_back(std::move(block));
    }
    std::int16_t result{};
    switch (function) {
      case 0: result=Gems(memory,args); break;
      case 1: case 2: Limbs(memory,function,args); break;
      case 3: result=b::LastActionStatValue(memory.Read<unsigned>(kLast)); break;
      case 4: result=b::ScaleLastActionStatValue(memory.Read<unsigned>(kLast),
                       (memory.Read<unsigned>(args[0]+0x14+4*args[3])>>22)&31); break;
      case 5: result=b::TargetRisk(memory.Read<std::uint16_t>(memory.Stats(args[2])+0x20)); break;
      default: std::abort();
    }
    std::cout<<static_cast<unsigned>(result)<<' '<<memory.Crc()<<'\n';
  }
}
