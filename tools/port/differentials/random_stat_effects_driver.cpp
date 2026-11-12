// Native side of random_stat_effects.py, F00486-F00493 (146C.c:12717-12787).
#include "Battle/BasicActionEffects.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>

namespace {
namespace b = VS::Battle;
using Hit = std::array<std::uint32_t, 25>;
using HalfHit = std::array<std::uint16_t, 50>;
constexpr std::array<unsigned, 12> offsets{4, 6, 8, 10, 12, 14, 16, 18, 56, 58, 60, 62};
constexpr std::array<unsigned, 12> shifts{0, 2, 4, 6, 8, 10, 12, 14, 18, 20, 22, 24};
constexpr std::array policies{
    b::QueueRandomStrengthIncrease, b::QueueRandomIntelligenceIncrease,
    b::QueueRandomAgilityIncrease, b::QueueRandomMaxHpIncrease, b::QueueRandomMaxMpIncrease,
    b::QueueRandomStrengthDecrease, b::QueueRandomIntelligenceDecrease, b::QueueRandomAgilityDecrease};
std::int16_t low(std::uint16_t value) { return std::bit_cast<std::int16_t>(value); }
b::ActorActionResult decode(const Hit& words) {
  const auto halves = std::bit_cast<HalfHit>(words);
  b::ActorActionResult hit{};
  hit.target = static_cast<std::uint8_t>(words[4]);
  hit.limb = std::bit_cast<std::int8_t>(static_cast<std::uint8_t>(words[4] >> 8));
  hit.impact = static_cast<std::uint8_t>(words[4] >> 24);
  for (std::size_t i = 0; i < offsets.size(); ++i)
    hit.changes[i] = {low(halves[8 + offsets[i] / 2]),
                     static_cast<std::uint8_t>((words[11] >> shifts[i]) & 3)};
  for (std::size_t i = 0; i < 6; ++i)
    hit.limbs[i] = {low(halves[24 + i * 2]), low(halves[25 + i * 2])};
  hit.inflict_statuses = words[9];
  hit.remove_statuses = words[10];
  return hit;
}
void encode(Hit& words, const b::ActorActionResult& hit) {
  auto halves = std::bit_cast<HalfHit>(words);
  for (std::size_t i = 0; i < offsets.size(); ++i)
    halves[8 + offsets[i] / 2] = std::bit_cast<std::uint16_t>(hit.changes[i].amount);
  for (std::size_t i = 0; i < 6; ++i) {
    halves[24 + i * 2] = std::bit_cast<std::uint16_t>(hit.limbs[i].hp);
    halves[25 + i * 2] = std::bit_cast<std::uint16_t>(hit.limbs[i].armor_dp);
  }
  words = std::bit_cast<Hit>(halves);
  for (std::size_t i = 0; i < shifts.size(); ++i)
    words[11] = (words[11] & ~(3u << shifts[i])) |
                (static_cast<unsigned>(hit.changes[i].mode) << shifts[i]);
  words[4] = (words[4] & 0x00FF0000u) | hit.target |
      (static_cast<unsigned>(std::bit_cast<std::uint8_t>(hit.limb)) << 8) |
      (static_cast<unsigned>(hit.impact) << 24);
  words[9] = hit.inflict_statuses;
  words[10] = hit.remove_statuses;
}
struct Queue final : b::RandomStatQueue {
  std::array<std::uint32_t, 4> before{1, 2, 3, 4};
  b::ActorActionResult hit{};
  std::array<std::uint32_t, 4> after{5, 6, 7, 8};
  Hit target{}, original{}, changed{};
  unsigned draw{}, count{}, bound{}, reads{}, writes{};
  bool mutate{}, before_draw = true;
  std::int32_t DrawBounded(std::int32_t maximum) override {
    before_draw = before_draw && hit == decode(original) && reads == 0 && writes == 0;
    ++count; bound = maximum;
    if (mutate) { target = changed; hit = decode(target); }
    return static_cast<std::int32_t>(draw);
  }
  b::ActionResultChange ReadChange(b::ActionResultField field) override {
    ++reads;
    return hit.changes.at(static_cast<std::size_t>(field));
  }
  void WriteChange(b::ActionResultField field, b::ActionResultChange change) override {
    ++writes;
    hit.changes.at(static_cast<std::size_t>(field)) = change;
  }
};
}
int main() {
  std::string line;
  while (std::getline(std::cin, line)) {
    std::istringstream in(line);
    unsigned function{}, execute{}, index{}, mutate{};
    Hit source{}, other{};
    Queue queue;
    in >> function >> execute >> index >> queue.draw >> mutate;
    for (auto& v : source) in >> v;
    for (auto& v : queue.target) in >> v;
    for (auto& v : other) in >> v;
    for (auto& v : queue.changed) in >> v;
    if (!in || function >= policies.size()) return 2;
    queue.mutate = mutate != 0;
    queue.original = queue.target;
    queue.hit = decode(queue.target);
    policies[function](queue);
    encode(queue.target, queue.hit);
    const bool canaries = queue.before == std::array<std::uint32_t, 4>{1, 2, 3, 4} &&
        queue.after == std::array<std::uint32_t, 4>{5, 6, 7, 8};
    std::cout << canaries << ' ' << queue.count << ' ' << queue.bound << ' '
              << queue.draw << ' ' << queue.before_draw;
    for (const auto v : source) std::cout << ' ' << v;
    for (const auto v : queue.target) std::cout << ' ' << v;
    for (const auto v : other) std::cout << ' ' << v;
    std::cout << '\n';
  }
}
