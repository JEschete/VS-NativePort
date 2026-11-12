// Checked live callback mutation and refusals for F00486-F00493.
#include <algorithm>
#include <array>
#include <bit>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <type_traits>

#include "Battle/BasicActionEffectsLive.hpp"
#include "Battle/StatusQueueLive.hpp"
#include "Core/RandomLive.hpp"
#include "State/ScriptFlagsLive.hpp"
extern "C" {
#include "Battle/ActionResolution.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Battle/Hit.h"
Battle_Actor* Battle_actors[42]{};
}
namespace {
namespace b = VS::Battle;
using Handler = std::remove_extent_t<decltype(Battle_actionEffectMediators)>;
constexpr std::array<Handler, 8> handlers{
    b::QueueLiveRandomStrengthIncrease, b::QueueLiveRandomIntelligenceIncrease,
    b::QueueLiveRandomAgilityIncrease, b::QueueLiveRandomMaxHpIncrease,
    b::QueueLiveRandomMaxMpIncrease, b::QueueLiveRandomStrengthDecrease,
    b::QueueLiveRandomIntelligenceDecrease, b::QueueLiveRandomAgilityDecrease};
constexpr std::array offsets{10, 12, 14, 16, 18, 10, 12, 14};
constexpr std::array shifts{6, 8, 10, 12, 14, 6, 8, 10};
template<class T> auto bytes(const T& value) {
  return std::bit_cast<std::array<std::byte, sizeof(T)>>(value);
}
struct Guarded {
  std::array<unsigned, 4> before{1, 2, 3, 4};
  Battle_HitResult hit{};
  std::array<unsigned, 4> after{5, 6, 7, 8};
};
Battle_HitResult *cached{}, *selected{}, *alternate{};
Battle_HitResult original{}, during{};
unsigned draws{}, result{};
bool before_draw{};
Battle_HitResult seed(unsigned value) {
  std::array<unsigned, sizeof(Battle_HitResult) / 4> words{};
  words.fill(value);
  return std::bit_cast<Battle_HitResult>(words);
}
void pending(Battle_HitResult& hit, unsigned function, int amount, unsigned mode) {
  auto halves = std::bit_cast<std::array<std::uint16_t, sizeof(Battle_HitResult) / 2>>(hit);
  halves.at(offsets.at(function) / 2) = static_cast<std::uint16_t>(amount);
  auto words = std::bit_cast<std::array<unsigned, sizeof(Battle_HitResult) / 4>>(halves);
  words.at(7) = (words.at(7) & ~(3u << shifts.at(function))) | (mode << shifts.at(function));
  hit = std::bit_cast<Battle_HitResult>(words);
}
}
extern "C" void Core_Errorf(const char*, const char* message, ...) {
  va_list arguments;
  va_start(arguments, message);
  std::vfprintf(stderr, message, arguments);
  va_end(arguments);
  std::fputc('\n', stderr);
  std::_Exit(draws == 0 ? 85 : 89);
}
namespace VS::State {
void SetLiveScriptFlag(int, int) { std::_Exit(88); }
}
namespace VS::Battle {
int QueueLiveStatusRemovalMask(int, Battle_HitResult*) { std::_Exit(88); }
}
namespace VS::Core {
std::int32_t NextLiveRandomBounded(std::int32_t bound) {
  if (bound != 4 || !cached) std::_Exit(88);
  ++draws;
  before_draw = bytes(*cached) == bytes(original);
  *cached = during;
  selected = alternate;
  std::ranges::fill(Battle_actors, nullptr);
  return static_cast<std::int32_t>(result);
}
}
#if defined(PROBE_WRONG_ABI)
int Wrong(Battle_ActionDefinition*, Battle_HitResult*, Battle_HitResult*, int, int);
Handler wrong = &Wrong;
#endif
int main(int argc, char** argv) {
  if (argc > 1) {
    const auto index = static_cast<std::size_t>(std::atoi(argv[1]));
    handlers.at(index)(nullptr, nullptr, nullptr, std::numeric_limits<int>::min(), 0);
    return 86;
  }
  for (unsigned function = 0; function < handlers.size(); ++function) {
    unsigned cases = 0;
    for (result = 0; result < 4; ++result) {
      for (const auto amount : {0, 19, -17, 32767, 32768, 65535}) {
        for (unsigned mode = 0; mode < 4; ++mode) {
          for (const auto unused : {0, 1, -1, std::numeric_limits<int>::min(),
                                    std::numeric_limits<int>::max()}) {
            Guarded target{}, other{}, source{};
            target.hit = seed(0xD6A593EF);
            other.hit = seed(0xFEDCBA98);
            source.hit = seed(0x12345678);
            original = target.hit;
            during = seed(0x7A4CBE91);
            pending(during, function, amount, mode);
            auto expected = target;
            expected.hit = during;
            pending(expected.hit, function, amount + result + 1, function < 5 ? 2 : 1);
            const auto other_before = bytes(other), source_before = bytes(source);
            cached = selected = &target.hit;
            alternate = &other.hit;
            Battle_Actor actor{};
            std::ranges::fill(Battle_actors, &actor);
            draws = 0;
            handlers.at(function)(nullptr, &source.hit, selected, unused, unused);
            if (draws != 1 || !before_draw || selected != alternate ||
                bytes(target) != bytes(expected) || bytes(other) != other_before ||
                bytes(source) != source_before) return 1;
            ++cases;
          }
        }
      }
    }
    std::printf("F%05u: %u callback mutation/cached-target/unused-argument/canary cases passed\n",
                486 + function, cases);
  }
}
