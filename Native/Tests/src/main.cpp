#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <optional>

#include <catch2/catch_session.hpp>
#include <catch2/catch_test_case_info.hpp>
#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>

#include "Content/DataStore.hpp"

namespace {
char runningTest[256];

// A production abort() (a missing catalog, for example) ends the process
// without a Catch2 report, so name the test case that was running.
void ReportAbortedTest(int) {
  std::fputs("\nVS.Tests aborted while running: ", stderr);
  std::fputs(runningTest, stderr);
  std::fputs("\n", stderr);
  std::_Exit(134);
}

// Every test case starts without a data store and restores whatever it left
// behind, so a test that needs a catalog must publish its own instead of
// depending on the store an earlier test happened to leave active.
class DataStoreIsolation final : public Catch::EventListenerBase {
 public:
  explicit DataStoreIsolation(const Catch::IConfig* config)
      : Catch::EventListenerBase(config) {
#ifdef _WIN32
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    std::signal(SIGABRT, ReportAbortedTest);
  }
  void testCaseStarting(const Catch::TestCaseInfo& info) override {
    std::snprintf(runningTest, sizeof(runningTest), "%s", info.name.c_str());
    guard_.emplace();
  }
  void testCaseEnded(const Catch::TestCaseStats&) override { guard_.reset(); }

 private:
  std::optional<VS::Content::DataStore::ScopedOverride> guard_;
};
}  // namespace

// NOLINTNEXTLINE(bugprone-throwing-static-initialization): Catch2 registrar.
CATCH_REGISTER_LISTENER(DataStoreIsolation)

int main(int argc, char** argv) { return Catch::Session().run(argc, argv); }
