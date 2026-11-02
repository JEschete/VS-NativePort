#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Boot/Session.hpp"

namespace {
using VS::Boot::SessionDestination;
using VS::Boot::SessionRefusal;
using VS::Boot::SessionState;
using VS::Boot::SessionTransitionBarrier;

struct Transferred {
  SessionDestination destination;
};
struct Refused {
  SessionRefusal refusal;
};

// The core has no extern "C" frames, so the test host can leave Transfer and
// Refuse with exceptions instead of the live boundary jump.
class RecordingHost final : public VS::Boot::SessionHost {
 public:
  std::vector<std::string> calls;

  void InvalidateSceneUi() override { calls.emplace_back("ui"); }
  void ResetGameplayInspection() override { calls.emplace_back("inspect"); }
  void ResetNativeSession() override { calls.emplace_back("save"); }
  [[noreturn]] void Transfer(SessionDestination destination) override {
    calls.emplace_back("transfer");
    throw Transferred{destination};
  }
  [[noreturn]] void Refuse(SessionRefusal refusal) override {
    calls.emplace_back("refuse");
    throw Refused{refusal};
  }
};

template <typename Jump>
SessionDestination TransferredTo(Jump jump) {
  try {
    jump();
  } catch (const Transferred& transferred) {
    return transferred.destination;
  }
  FAIL("the jump returned");
  return SessionDestination::kTitle;
}

template <typename Jump>
SessionRefusal RefusedWith(Jump jump) {
  try {
    jump();
  } catch (const Refused& refused) {
    return refused.refusal;
  } catch (const Transferred&) {
    FAIL("the jump transferred");
  }
  FAIL("the jump returned");
  return SessionRefusal::kNoSession;
}

class RecordingTitleHost final : public VS::Boot::TitleSessionHost {
 public:
  std::vector<std::string> calls;
  int entry_mode = -1;

  void InitializeTitleSession() override { calls.emplace_back("init"); }
  void LoadTitleOverlay() override { calls.emplace_back("overlay"); }
  int RunTitle() override {
    calls.emplace_back("title");
    return 3;
  }
  void SetGameEntryMode(int mode) override {
    calls.emplace_back("mode");
    entry_mode = mode;
  }
  void ClearPendingClearSave() override { calls.emplace_back("clear"); }
  void ShowLoadingScreen() override { calls.emplace_back("loading"); }
  void LoadBattleOverlays() override { calls.emplace_back("battle"); }
  int RunLoadedGame() override {
    calls.emplace_back("game");
    return 9;
  }
};

class RecordingGameHost final : public VS::Boot::GameHost {
 public:
  std::vector<std::string> calls;
  void InitializePlatformServices() override { calls.emplace_back("platform"); }
  void RunSessions() override { calls.emplace_back("sessions"); }
};
}  // namespace

TEST_CASE("session begins and ends in the original order", "[session]") {
  SessionState state;
  RecordingHost host;
  REQUIRE(VS::Boot::BeginSession(state, host, true));
  CHECK(state.active);
  CHECK(host.calls == std::vector<std::string>{"ui", "inspect"});

  host.calls.clear();
  VS::Boot::EndSession(state, host);
  CHECK_FALSE(state.active);
  CHECK(host.calls == std::vector<std::string>{"ui", "save", "inspect"});
}

TEST_CASE("session refuses to begin twice or without entries", "[session]") {
  SessionState state;
  RecordingHost host;
  CHECK_FALSE(VS::Boot::BeginSession(state, host, false));
  CHECK_FALSE(state.active);
  REQUIRE(VS::Boot::BeginSession(state, host, true));
  host.calls.clear();
  CHECK_FALSE(VS::Boot::BeginSession(state, host, true));
  CHECK(host.calls.empty());
}

TEST_CASE("session boundary values select the entry", "[session]") {
  CHECK(VS::Boot::EntryFor(0) == SessionDestination::kTitle);
  CHECK(VS::Boot::EntryFor(1) == SessionDestination::kTitle);
  CHECK(VS::Boot::EntryFor(2) == SessionDestination::kGameLoop);
}

TEST_CASE("title transition resets the save session first", "[session]") {
  SessionState state;
  RecordingHost host;
  REQUIRE(VS::Boot::BeginSession(state, host, true));
  host.calls.clear();
  CHECK(TransferredTo([&] { VS::Boot::JumpToTitle(state, host); }) ==
        SessionDestination::kTitle);
  CHECK(host.calls ==
        std::vector<std::string>{"save", "ui", "inspect", "transfer"});
}

TEST_CASE("game-loop transition keeps the save session", "[session]") {
  SessionState state;
  RecordingHost host;
  REQUIRE(VS::Boot::BeginSession(state, host, true));
  host.calls.clear();
  CHECK(TransferredTo([&] { VS::Boot::JumpToGameLoop(state, host); }) ==
        SessionDestination::kGameLoop);
  CHECK(host.calls == std::vector<std::string>{"ui", "inspect", "transfer"});
}

TEST_CASE("transitions without a session are refused untouched", "[session]") {
  SessionState state;
  RecordingHost host;
  CHECK(RefusedWith([&] { VS::Boot::JumpToTitle(state, host); }) ==
        SessionRefusal::kNoSession);
  CHECK(RefusedWith([&] { VS::Boot::JumpToGameLoop(state, host); }) ==
        SessionRefusal::kNoSession);
  CHECK(host.calls == std::vector<std::string>{"refuse", "refuse"});
}

TEST_CASE("transitions inside a barrier are refused untouched", "[session]") {
  SessionState state;
  RecordingHost host;
  REQUIRE(VS::Boot::BeginSession(state, host, true));
  host.calls.clear();
  {
    const SessionTransitionBarrier barrier{state};
    CHECK(RefusedWith([&] { VS::Boot::JumpToTitle(state, host); }) ==
          SessionRefusal::kInsideBarrier);
    CHECK(RefusedWith([&] { VS::Boot::JumpToGameLoop(state, host); }) ==
          SessionRefusal::kInsideBarrier);
  }
  CHECK(host.calls == std::vector<std::string>{"refuse", "refuse"});
  CHECK(state.active);
  CHECK(state.barrier_depth == 0);
  CHECK(TransferredTo([&] { VS::Boot::JumpToGameLoop(state, host); }) ==
        SessionDestination::kGameLoop);
}

TEST_CASE("session barriers nest", "[session]") {
  SessionState state;
  {
    const SessionTransitionBarrier outer{state};
    CHECK(state.barrier_depth == 1);
    {
      const SessionTransitionBarrier inner{state};
      CHECK(state.barrier_depth == 2);
    }
    CHECK(state.barrier_depth == 1);
  }
  CHECK(state.barrier_depth == 0);
}

TEST_CASE("title session runs in the original order", "[session]") {
  RecordingTitleHost host;
  CHECK(VS::Boot::RunTitleAndBattle(host) == 9);
  CHECK(host.entry_mode == 3);
  CHECK(host.calls == std::vector<std::string>{"init", "overlay", "title",
                                               "mode", "clear", "loading",
                                               "battle", "game"});
}

TEST_CASE("game initializes the platform before sessions", "[session]") {
  RecordingGameHost host;
  VS::Boot::RunGame(host);
  CHECK(host.calls == std::vector<std::string>{"platform", "sessions"});
}
