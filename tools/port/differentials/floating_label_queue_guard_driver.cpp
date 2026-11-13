#include <crtdbg.h>

#include <array>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <span>
#include <string_view>
#include <utility>

#include "Battle/FloatingLabelQueueLive.hpp"
#include "Core/Log.hpp"
#include "TestSupport/FloatingLabelQueueLiveFixture.hpp"

namespace {
namespace b = VS::Battle;
namespace f = VS::test_support::live_floating_labels;
std::function<bool()> expected_state;
std::function<void()> during_random;
volatile std::sig_atomic_t refusal_exit = 86;
unsigned diagnostics = 0;

void ResetProbe() {
  Battle_actorMarkersHidden = 9;
  Battle_abilityTimingCueFrames = 9;
  Battle_floatingLabelCount = 9;
  Battle_statusLabelStepTimer = 9;
  Battle_ashleyStatusLabelsSuppressed = 9;
  Battle_floatingLabels = nullptr;
  expected_state = [] {
    return Battle_actorMarkersHidden == 0 &&
           Battle_abilityTimingCueFrames == 0 &&
           Battle_floatingLabelCount == 0 && Battle_statusLabelStepTimer == 0 &&
           Battle_ashleyStatusLabelsSuppressed == 0;
  };
  b::ResetLiveFloatingLabels();
}

void ReserveProbe(std::string_view selected) {
  const bool negative = selected == "reserve-negative";
  const bool overcount = selected == "reserve-overcount";
  Battle_floatingLabelCount = overcount ? 33 : (negative ? -1 : 0);
  if (!negative && !overcount) Battle_floatingLabels = nullptr;
  expected_state = [negative, overcount] {
    return Battle_floatingLabelCount == (overcount ? 33 : (negative ? -1 : 0));
  };
  static_cast<void>(b::ReserveLiveFloatingLabel());
}

bool WorldProbe(std::string_view selected, f::State& state) {
  if (selected == "world-overcount") {
    Battle_floatingLabelCount = 33;
    expected_state = [&state] {
      return Battle_floatingLabelCount == 33 &&
             state.labels[0][0].key.bytes[1] == 0;
    };
    const std::array<std::uint16_t, 3> position{123, 456, 789};
    static_cast<void>(b::QueueLiveWorldLockedLabel(position));
    return false;
  }
  if (selected == "world-empty") {
    expected_state = [&state] {
      return state.labels[0][0].key.bytes[0] == 3 &&
             state.labels[0][0].frame == 0 &&
             state.labels[0][0].position.vx == 11 &&
             Battle_floatingLabelCount == 0;
    };
    static_cast<void>(b::QueueLiveWorldLockedLabelEntry(nullptr));
  }
  if (selected == "world-short") {
    const std::array<std::uint16_t, 2> position{123, 456};
    expected_state = [&state] {
      return state.labels[0][0].position.vx == 123 &&
             state.labels[0][0].position.vy == 456 &&
             state.labels[0][0].position.vz == 33 &&
             state.labels[0][0].key.bytes[0] == 3 &&
             Battle_floatingLabelCount == 0;
    };
    static_cast<void>(b::QueueLiveWorldLockedLabel(position));
  }
  const bool cleared = selected == "world-cleared-alias";
  Battle_floatingLabelCount = 32;
  state.labels[0][5].position = {101, 202, 303, 4};
  auto* const source = reinterpret_cast<std::uint16_t*>(
      &state.labels[0][cleared ? 31 : 4].position);
  expected_state = [&state, cleared] {
    const auto& result = state.labels[0][31];
    return Battle_floatingLabelCount == 32 &&
           result.position.vx == (cleared ? 0 : 101) &&
           result.position.vy == (cleared ? 0 : 202) &&
           result.position.vz == (cleared ? 0 : 303) &&
           result.key.bytes[0] == 3 && result.frame == 0 &&
           state.labels[0][0].key.bytes[1] == 1;
  };
  static_cast<void>(
      b::QueueLiveWorldLockedLabel(std::span<const std::uint16_t>(source, 3)));
  return expected_state();
}

void RemovalProbe(std::string_view selected, f::State& state) {
  if (selected == "remove-overcount") {
    Battle_floatingLabelCount = 33;
    expected_state = [&state] {
      return Battle_floatingLabelCount == 32 &&
             state.labels[0][0].key.bytes[1] == 1 &&
             state.labels[0][30].key.bytes[1] == 31 &&
             state.labels[0][31].key.bytes[1] == 31;
    };
    b::RemoveLiveFloatingLabel(0);
  }
  if (selected == "remove-empty") {
    expected_state = [] { return Battle_floatingLabelCount == -1; };
    b::RemoveLiveFloatingLabel(0);
  }
  if (selected == "remove-negative") {
    Battle_floatingLabelCount = 4;
    expected_state = [&state] {
      return Battle_floatingLabelCount == 3 &&
             state.labels[0][0].key.bytes[1] == 0;
    };
    b::RemoveLiveFloatingLabel(-1);
  }
  Battle_floatingLabels = nullptr;
  Battle_floatingLabelCount = 2;
  expected_state = [] { return Battle_floatingLabelCount == 1; };
  b::RemoveLiveFloatingLabel(0);
}

bool OtherProbe(std::string_view selected, f::State& state) {
  if (selected == "advance-null") {
    expected_state = [] { return Battle_floatingLabelCount == 0; };
    b::AdvanceLiveFloatingLabelQueue(nullptr);
  }
  if (selected == "compact-missing") {
    Battle_floatingLabels = nullptr;
    Battle_floatingLabelCount = 1;
    expected_state = [] { return Battle_floatingLabelCount == 1; };
    b::CompactLiveFloatingLabels(1);
  }
  if (selected == "compact-overcount") {
    Battle_floatingLabelCount = 33;
    expected_state = [] { return Battle_floatingLabelCount == 33; };
    b::CompactLiveFloatingLabels(1);
  }
  if (selected == "producer-saturation") {
    Battle_floatingLabelCount = 32;
    expected_state = [&state] {
      return Battle_floatingLabelCount == 32 &&
             state.labels[0][31].key.bytes[0] == 5 &&
             state.labels[0][31].value == 99 &&
             state.labels[0][0].key.bytes[1] == 1;
    };
    static_cast<void>(Battle_QueueChainCount(99));
    return expected_state();
  }
  if (selected == "producer-overcount" || selected == "consumer-overcount" ||
      selected == "update-overcount") {
    Battle_floatingLabelCount = 33;
    state.canary.key.bytes[0] = 7;
    Battle_abilityTimingCueFrames = 0;
    Battle_statusLabelStepTimer = 0;
    expected_state = [&state] {
      return Battle_floatingLabelCount == 33 &&
             state.labels[0][0].key.bytes[1] == 0 &&
             state.canary.key.bytes[0] == 7;
    };
    if (selected == "producer-overcount") {
      static_cast<void>(Battle_QueueChainCount(99));
    } else if (selected == "consumer-overcount") {
      Battle_CommitEquipmentStatChanges();
    } else {
      Battle_UpdateAndDrawFloatingLabels();
    }
  }
  if (selected == "empty-lazy") {
    Battle_floatingLabels = nullptr;
    b::CompactLiveFloatingLabels(9);
    Battle_floatingLabelCount = 20;
    b::CompactLiveFloatingLabels(0);
    b::CompactLiveFloatingLabels(-1);
    return Battle_floatingLabelCount == 20;
  }
  if (selected == "rebound-owner") {
    Battle_FloatingLabel rebound[1][32]{};
    rebound[0][1].value = 912;
    Battle_floatingLabels = rebound;
    Battle_floatingLabelCount = 2;
    b::RemoveLiveFloatingLabel(0);
    return Battle_floatingLabelCount == 1 && rebound[0][0].value == 912 &&
           state.labels[0][0].value == 500;
  }
  return false;
}

bool RandomCallbackProbe(std::string_view selected, f::State& state) {
  VS::Core::SetLogLevel(VS::Core::LogLevel::Trace);
  static_cast<void>(VS::Core::SetConsoleLogLevel(VS::Core::LogLevel::Warn));
  if (selected == "rng-extended-prefix") {
    during_random = [] {
      const std::array<std::uint16_t, 3> first{101, 202, 303};
      const std::array<std::uint16_t, 3> second{111, 222, 333};
      static_cast<void>(b::QueueLiveWorldLockedLabel(first));
      static_cast<void>(b::QueueLiveWorldLockedLabel(second));
    };
    const int result = Battle_QueueAbilityTimingResult(99);
    return result == 0 && Battle_floatingLabelCount == 2 &&
           state.labels[0][0].key.bytes[0] == 3 &&
           state.labels[0][0].value == 99 &&
           state.labels[0][1].position.vx == 111 &&
           state.labels[0][2].value == 502;
  }
  if (selected == "rng-reset-reservation") {
    Battle_floatingLabelCount = 32;
    expected_state = [&state] {
      return Battle_floatingLabelCount == 0 &&
             state.labels[0][31].key.bytes[0] == 0 &&
             state.labels[0][31].value == 0;
    };
    during_random = [] { b::ResetLiveFloatingLabels(); };
    static_cast<void>(Battle_QueueAbilityTimingResult(99));
    return false;
  }
  Battle_FloatingLabel rebound[1][32]{};
  expected_state = [&state, &rebound] {
    return Battle_floatingLabels == rebound && Battle_floatingLabelCount == 0 &&
           state.labels[0][0].value == 99 && rebound[0][0].key.bytes[0] == 0;
  };
  during_random = [&rebound] {
    Battle_floatingLabels = rebound;
    Battle_floatingLabelCount = 0;
  };
  static_cast<void>(Battle_QueueAbilityTimingResult(99));
  return false;
}

void Observe(const VS::Core::LogRecord& record) {
  if (record.channel == "rng" && during_random) {
    auto callback = std::move(during_random);
    during_random = {};
    callback();
  }
  if (record.channel != "battle.labels") return;
  ++diagnostics;
  const bool expected = expected_state && expected_state();
  refusal_exit = expected && diagnostics == 1 ? 85 : 86;
  std::fprintf(stderr, "partial-mutations=%s\n",
               expected ? "preserved" : "WRONG");
}
}  // namespace

int main(int argc, char** argv) {
  if (argc != 3) return 64;
  VS::Core::SetLogLevel(VS::Core::LogLevel::Debug);
  VS::Core::SetLogObserver(Observe);
  if (!VS::Core::SetLogFile(argv[2])) return 65;
  _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
  _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
  std::signal(SIGABRT, [](int) { std::_Exit(refusal_exit); });
  f::State state;
  state.Populate();
  const std::string_view selected(argv[1]);
  if (selected == "reset-missing") ResetProbe();
  if (selected.starts_with("reserve-")) ReserveProbe(selected);
  if (selected.starts_with("remove-")) RemovalProbe(selected, state);
  const bool passed =
      selected.starts_with("world-")
          ? WorldProbe(selected, state)
          : (selected.starts_with("rng-") ? RandomCallbackProbe(selected, state)
                                          : OtherProbe(selected, state));
  std::printf("%s diagnostics=%u\n", passed ? "PASS" : "FAIL", diagnostics);
  return passed && diagnostics == 0 ? 0 : 1;
}
