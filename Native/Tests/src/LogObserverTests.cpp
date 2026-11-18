#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Core/Diagnostics.hpp"
#include "Core/Log.hpp"

namespace {
namespace Core = VS::Core;

struct LogFileScope {
  std::filesystem::path path =
      std::filesystem::temp_directory_path() /
      ("vs-observer-test-" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()) +
       ".log");

  ~LogFileScope() {
    Core::SetLogFile("");
    std::error_code error;
    std::filesystem::remove(path, error);
  }
};

struct ObserverScope {
  Core::LogObserver previous = Core::SetLogObserver({});
  ~ObserverScope() {
    Core::SetLogObserver(std::move(previous));
    Core::SetLogLevel(Core::LogLevel::Info);
    Core::SetLogChannels("*");
    Core::SetLogTick(0);
  }
};

struct BufferedSinkScope {
  LogFileScope primary;
  LogFileScope mirror;
  bool previous_flush = Core::SetLogFlushEachMessage(false);
  ~BufferedSinkScope() {
    Core::SetLogMirrorFile("");
    Core::SetLogFlushEachMessage(previous_flush);
  }
};
}  // namespace

TEST_CASE("Log observers receive filtered records with production tick context",
          "[core][log][observer]") {
  ObserverScope scope;
  std::vector<std::string> records;
  Core::SetLogLevel(Core::LogLevel::Trace);
  REQUIRE(Core::SetLogChannels("combat"));
  Core::SetLogTick(42);
  Core::SetLogObserver([&records](const Core::LogRecord& record) {
    CHECK(record.tick == 42);
    // This takes the logger mutex: observation must happen outside that lock.
    CHECK(Core::IsLogEnabled(record.level, record.channel));
    records.emplace_back(record.message);
  });
  VS::Core::LogInfo("combat", "damage={}", 12);
  VS::Core::LogInfo("other", "filtered");
  VS::Core::LogWarn("other", "important");
  REQUIRE(records.size() == 2);
  CHECK(records[0] == "damage=12");
  CHECK(records[1] == "important");
  Core::SetLogObserver({});
  VS::Core::LogInfo("combat", "detached");
  CHECK(records.size() == 2);
}

TEST_CASE("Log observers can be replaced and restored without affecting sinks",
          "[core][log][observer]") {
  ObserverScope scope;
  LogFileScope log_file;
  REQUIRE(Core::SetLogFile(log_file.path.string().c_str()));
  Core::SetLogLevel(Core::LogLevel::Info);
  Core::SetLogChannels("*");
  int first = 0;
  int second = 0;
  Core::SetLogObserver([&](const Core::LogRecord&) { ++first; });
  auto previous =
      Core::SetLogObserver([&](const Core::LogRecord&) { ++second; });
  VS::Core::LogInfo("test", "replacement");
  Core::SetLogObserver(std::move(previous));
  VS::Core::LogInfo("test", "restored");
  CHECK(first == 1);
  CHECK(second == 1);
  Core::SetLogObserver({});
  VS::Core::LogInfo("test", "detached");
  std::ifstream reader(log_file.path);
  REQUIRE(reader.good());
  const std::string text((std::istreambuf_iterator<char>(reader)),
                         std::istreambuf_iterator<char>());
  CHECK(text.find("test: replacement") != std::string::npos);
  CHECK(text.find("test: restored") != std::string::npos);
  CHECK(text.find("test: detached") != std::string::npos);
}

TEST_CASE("Buffered Core sinks persist warnings without a trace observer",
          "[core][log][warning-persistence]") {
  ObserverScope observer;
  BufferedSinkScope sinks;
  REQUIRE(Core::SetLogFile(sinks.primary.path.string().c_str()));
  REQUIRE(Core::SetLogMirrorFile(sinks.mirror.path.string().c_str()));
  Core::SetLogLevel(Core::LogLevel::Trace);
  Core::SetLogChannels("*");
  VS::Core::LogWarn("effects.texture", "durable warning without observer");
  for (const auto& path : {sinks.primary.path, sinks.mirror.path}) {
    CAPTURE(path.string());
    std::ifstream input(path);
    const std::string contents((std::istreambuf_iterator<char>(input)), {});
    CHECK(contents.find("durable warning without observer") !=
          std::string::npos);
  }
}

TEST_CASE(
    "Native C warnings and errors retain severity filtering and tick context",
    "[core][log][native-diagnostics]") {
  ObserverScope scope;
  std::vector<Core::LogLevel> levels;
  std::vector<std::string> messages;
  std::vector<std::uint64_t> ticks;
  Core::SetLogLevel(Core::LogLevel::Info);
  REQUIRE(Core::SetLogChannels("unrelated"));
  Core::SetLogTick(73);
  Core::SetLogObserver([&](const Core::LogRecord& record) {
    levels.push_back(record.level);
    messages.emplace_back(record.message);
    ticks.push_back(record.tick);
  });
  VS::Core::Warnf("native", "bank=%u", 0U);
  VS::Core::Errorf("native", "slot=%d", -1);
  CHECK((levels == std::vector{Core::LogLevel::Warn, Core::LogLevel::Error}));
  CHECK((messages == std::vector<std::string>{"bank=0", "slot=-1"}));
  CHECK((ticks == std::vector<std::uint64_t>{73, 73}));
}
