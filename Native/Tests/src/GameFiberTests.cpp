#include <stdexcept>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "Platform/GameFiber.hpp"

using VS::Platform::FiberCancelled;
using VS::Platform::GameFiber;

namespace {

// Entries are plain function pointers, so each test resets this shared record.
struct FiberRecord {
  int entered = 0;
  int waits = 0;
  int unwound = 0;
  int waits_while_unwinding = 0;
  bool swallowed = false;
  bool in_game = false;
};
FiberRecord record;

struct UnwindProbe {
  UnwindProbe() = default;
  UnwindProbe(const UnwindProbe&) = delete;
  UnwindProbe& operator=(const UnwindProbe&) = delete;
  UnwindProbe(UnwindProbe&&) = delete;
  UnwindProbe& operator=(UnwindProbe&&) = delete;
  ~UnwindProbe() { ++record.unwound; }
};

// A cleanup that waits for a frame, as teardown code in the game may.
struct WaitingCleanup {
  WaitingCleanup() = default;
  WaitingCleanup(const WaitingCleanup&) = delete;
  WaitingCleanup& operator=(const WaitingCleanup&) = delete;
  WaitingCleanup(WaitingCleanup&&) = delete;
  WaitingCleanup& operator=(WaitingCleanup&&) = delete;
  ~WaitingCleanup() {
    GameFiber::YieldToHost();
    GameFiber::YieldToHost();
    ++record.waits_while_unwinding;
    ++record.unwound;
  }
};

void EndlessGame() {
  ++record.entered;
  const UnwindProbe probe;
  for (;;) {
    record.in_game = GameFiber::InGame();
    GameFiber::YieldToHost();
    ++record.waits;
  }
}

void FailingGame() {
  ++record.entered;
  const UnwindProbe probe;
  GameFiber::YieldToHost();
  throw std::runtime_error("game failure");
}

void GameWithWaitingCleanup() {
  ++record.entered;
  const UnwindProbe outer;
  const WaitingCleanup cleanup;
  for (;;) {
    GameFiber::YieldToHost();
    ++record.waits;
  }
}

void GameSwallowingCancellation() {
  ++record.entered;
  const UnwindProbe probe;
  try {
    GameFiber::YieldToHost();
  } catch (const FiberCancelled&) {
    record.swallowed = true;
  }
  GameFiber::YieldToHost();
  ++record.waits;
}

}  // namespace

TEST_CASE("Destroying a waiting game fiber unwinds its frames", "[fiber]") {
  record = {};
  {
    GameFiber fiber(&EndlessGame);
    CHECK(fiber.Resume());
    CHECK(fiber.Resume());
    CHECK(record.in_game);
    CHECK(record.waits == 1);
    CHECK_FALSE(GameFiber::InGame());
  }
  CHECK(record.entered == 1);
  CHECK(record.waits == 1);
  CHECK(record.unwound == 1);
}

TEST_CASE("Cancelling a game fiber finishes it without resuming the game",
          "[fiber]") {
  record = {};
  GameFiber fiber(&EndlessGame);
  CHECK(fiber.Resume());
  fiber.Cancel();
  CHECK(fiber.Finished());
  CHECK(fiber.Cancelled());
  CHECK(record.unwound == 1);
  CHECK(record.waits == 0);
  CHECK_FALSE(fiber.Resume());
  fiber.Cancel();
  CHECK(record.unwound == 1);
}

TEST_CASE("A game fiber cancelled before its first resume never runs",
          "[fiber]") {
  record = {};
  {
    GameFiber fiber(&EndlessGame);
    fiber.Cancel();
    CHECK(fiber.Finished());
    CHECK(fiber.Cancelled());
    CHECK_FALSE(fiber.Resume());
  }
  {
    const GameFiber destroyed_unstarted(&EndlessGame);
  }
  CHECK(record.entered == 0);
  CHECK(record.unwound == 0);
}

TEST_CASE("An exception that ends the game reaches the host once", "[fiber]") {
  record = {};
  GameFiber fiber(&FailingGame);
  CHECK(fiber.Resume());
  std::string message;
  try {
    static_cast<void>(fiber.Resume());
  } catch (const std::runtime_error& error) {
    message = error.what();
  }
  CHECK(message == "game failure");
  CHECK(record.unwound == 1);
  CHECK(fiber.Finished());
  CHECK_FALSE(fiber.Cancelled());
  CHECK_FALSE(fiber.Resume());
}

TEST_CASE("Waits reached while a cancelled game unwinds return at once",
          "[fiber]") {
  record = {};
  {
    GameFiber fiber(&GameWithWaitingCleanup);
    CHECK(fiber.Resume());
  }
  CHECK(record.waits == 0);
  CHECK(record.waits_while_unwinding == 1);
  CHECK(record.unwound == 2);
}

TEST_CASE("A swallowed cancellation is raised again at the next wait",
          "[fiber]") {
  record = {};
  GameFiber fiber(&GameSwallowingCancellation);
  CHECK(fiber.Resume());
  fiber.Cancel();
  CHECK(record.swallowed);
  CHECK(record.waits == 0);
  CHECK(record.unwound == 1);
  CHECK(fiber.Finished());
  CHECK(fiber.Cancelled());
}

TEST_CASE("Waiting outside the game fiber is a no-op", "[fiber]") {
  CHECK_FALSE(GameFiber::InGame());
  GameFiber::YieldToHost();
  CHECK_FALSE(GameFiber::InGame());
}
