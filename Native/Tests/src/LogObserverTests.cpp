#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

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
  VS_LOG_INFO("combat", "damage={}", 12);
  VS_LOG_INFO("other", "filtered");
  VS_LOG_WARN("other", "important");
  REQUIRE(records.size() == 2);
  CHECK(records[0] == "damage=12");
  CHECK(records[1] == "important");
  Core::SetLogObserver({});
  VS_LOG_INFO("combat", "detached");
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
  VS_LOG_INFO("test", "replacement");
  Core::SetLogObserver(std::move(previous));
  VS_LOG_INFO("test", "restored");
  CHECK(first == 1);
  CHECK(second == 1);
  Core::SetLogObserver({});
  VS_LOG_INFO("test", "detached");
  std::ifstream reader(log_file.path);
  REQUIRE(reader.good());
  const std::string text((std::istreambuf_iterator<char>(reader)),
                         std::istreambuf_iterator<char>());
  CHECK(text.find("test: replacement") != std::string::npos);
  CHECK(text.find("test: restored") != std::string::npos);
  CHECK(text.find("test: detached") != std::string::npos);
}
