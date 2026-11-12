// Native side of triggered_recovery.py; real owned stat and limb policies.
#include "Battle/BasicActionEffects.hpp"
#include "Battle/DamagePolicies.hpp"
#include "Battle/HitCalculations.hpp"
#include <array>
#include <bit>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {
namespace b = VS::Battle;
using Hit = std::array<std::uint32_t, 25>;
using HalfHit = std::array<std::uint16_t, 50>;
constexpr unsigned kSource = 0x80500000, kTarget = 0x80501000;
constexpr std::array policies{
    b::QueueSpellTriggeredRecovery, b::QueueNonSpellTriggeredRecovery,
    b::QueuePhysicalAffinityRecovery, b::QueueAirAffinityRecovery,
    b::QueueFireAffinityRecovery, b::QueueEarthAffinityRecovery,
    b::QueueWaterAffinityRecovery, b::QueueLightAffinityRecovery};
std::int16_t low(unsigned value) {
  return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}
unsigned widened(std::int16_t value) { return static_cast<unsigned>(static_cast<int>(value)); }
struct Queue final : b::TriggeredRecoveryQueue, b::ActionCalculationWorld {
  unsigned type{}, affinity{}, amount{}, mutate{}, last = 0xAABBCCDD;
  std::array<unsigned, 5> args{};
  Hit source{}, target{}, other{}, changed{};
  std::array<unsigned, 4> before{1,2,3,4};
  b::ActorActionResult hit{};
  std::array<unsigned, 4> after{5,6,7,8};
  std::array<std::array<b::StatFactorVital, 6>, 2> vitals{};
  std::array<std::int16_t, 8> armor{113,113,113,113,113,113,113,113};
  std::vector<std::array<unsigned, 6>> events;
  unsigned actor{};
  void decode() {
    const auto halves = std::bit_cast<HalfHit>(target);
    hit.changes[0] = {low(halves[10]), static_cast<std::uint8_t>(target[11] & 3)};
    for (unsigned i = 0; i < 6; ++i) hit.limbs[i].hp = low(halves[24 + 2*i]);
  }
  void encode() {
    auto halves = std::bit_cast<HalfHit>(target);
    halves[10] = static_cast<std::uint16_t>(hit.changes[0].amount);
    for (unsigned i = 0; i < 6; ++i) halves[24 + 2*i] = static_cast<std::uint16_t>(hit.limbs[i].hp);
    target = std::bit_cast<Hit>(halves);
    target[11] = (target[11] & ~3u) | hit.changes[0].mode;
  }
  std::uint8_t SelectedActionType() override { return static_cast<std::uint8_t>(type); }
  std::uint8_t SelectedAffinity() override { return static_cast<std::uint8_t>(affinity); }
  std::int16_t Calculate() override {
    events.push_back({1, args[0], args[1], args[2], args[3], args[4]});
    return b::CalculateActionStatChange(std::bit_cast<int>(args[3]), std::bit_cast<int>(args[4]), *this).value();
  }
  b::ActionResultChange ReadHp() override { return hit.changes[0]; }
  void WriteHp(b::ActionResultChange change) override { hit.changes[0] = change; encode(); }
  void RecoverLimbs(std::int16_t value) override {
    events.push_back({3, args[0], args[2], widened(value),
                     static_cast<unsigned>(static_cast<std::uint16_t>(hit.changes[0].amount)), target[11]});
    std::array<std::int16_t, 6> limbs{};
    for (unsigned i = 0; i < 6; ++i) limbs[i] = hit.limbs[i].hp;
    b::DistributeLimbRecovery(value, vitals[actor], limbs, armor);
    for (unsigned i = 0; i < 6; ++i) hit.limbs[i].hp = limbs[i];
    encode();
  }
  std::optional<b::CalculationTarget> Target() override {
    return b::CalculationTarget{0, std::bit_cast<int>(target[20]), 0};
  }
  std::optional<b::CalculationTarget> Source() override { return {}; }
  std::optional<b::CalculationActor> Actor(std::uint8_t) override { return b::CalculationActor{255, 0}; }
  std::optional<std::uint8_t> ActionType() override { return {}; }
  std::optional<int> HitCalculator(int, int) override { return {}; }
  std::optional<std::int16_t> StatCalculator(int index, int execute) override {
    events.push_back({2, args[0], args[1], args[2], static_cast<unsigned>(index), static_cast<unsigned>(execute)});
    if (mutate) {
      target = changed; decode(); actor = 1; type ^= 1; affinity = 127;
    }
    return low(amount);
  }
  std::uint32_t Prerequisite() override { return 0; }
  void StoreThreshold(std::uint8_t) override {}
  void StoreLastValue(std::uint32_t value) override { last = value; }
  std::int32_t RandomBounded(int) override { return 0; }
};
}
int main() {
  std::string line;
  while (std::getline(std::cin, line)) {
    std::istringstream in(line);
    Queue queue;
    unsigned function{}, selected{};
    in >> function >> selected >> queue.type >> queue.affinity;
    for (auto& value : queue.args) in >> value;
    in >> queue.amount >> queue.mutate;
    for (auto* block : {&queue.source, &queue.target, &queue.other, &queue.changed})
      for (auto& value : *block) in >> value;
    for (auto& owner : queue.vitals) for (auto& limb : owner) {
      unsigned current{}, maximum{}; in >> current >> maximum;
      limb = {low(current), low(maximum)};
    }
    if (!in || function >= policies.size()) return 2;
    queue.decode();
    policies[function](queue);
    const bool canaries = queue.before == std::array<unsigned,4>{1,2,3,4} &&
                          queue.after == std::array<unsigned,4>{5,6,7,8};
    std::cout << canaries;
    for (const auto* block : {&queue.source, &queue.target, &queue.other})
      for (const auto value : *block) std::cout << ' ' << value;
    for (const auto value : std::bit_cast<std::array<unsigned,4>>(queue.armor)) std::cout << ' ' << value;
    std::cout << ' ' << queue.last << ' ' << queue.events.size();
    for (const auto& event : queue.events) for (const auto value : event) std::cout << ' ' << value;
    std::cout << '\n';
  }
}
