#include "Core/Log.hpp"
#include "Core/Unported.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

struct CountedLogValue {
	int* formats;
};

template <> struct std::formatter<CountedLogValue> : std::formatter<int> {
	auto format(const CountedLogValue& value, std::format_context& context) const
	{
		++*value.formats;
		return std::formatter<int>::format(42, context);
	}
};

using namespace VS;

namespace {
struct LogScope {
	std::filesystem::path path = std::filesystem::temp_directory_path() /
		("vs-log-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".log");

	LogScope()
	{
		Core::SetLogLevel(Core::LogLevel::Info);
		Core::SetLogChannels("");
		Core::SetLogTick(0);
	}

	~LogScope()
	{
		Core::SetLogFile("");
		Core::SetLogLevel(Core::LogLevel::Info);
		Core::SetLogChannels("");
		Core::SetLogTick(0);
		std::error_code error;
		std::filesystem::remove(path, error);
	}
};
} // namespace

TEST_CASE("Logging validates levels and filters without losing the previous filter", "[core][log]")
{
	LogScope scope;
	CHECK(Core::ParseLogLevel("trace") == Core::LogLevel::Trace);
	CHECK(Core::ParseLogLevel("debug") == Core::LogLevel::Debug);
	CHECK(Core::ParseLogLevel("info") == Core::LogLevel::Info);
	CHECK(Core::ParseLogLevel("warn") == Core::LogLevel::Warn);
	CHECK(Core::ParseLogLevel("error") == Core::LogLevel::Error);
	CHECK(Core::ParseLogLevel("off") == Core::LogLevel::Off);
	CHECK_FALSE(Core::ParseLogLevel("verbose").has_value());
	REQUIRE(Core::SetLogChannels("ai,actors"));
	CHECK(Core::IsLogEnabled(Core::LogLevel::Info, "ai"));
	CHECK_FALSE(Core::IsLogEnabled(Core::LogLevel::Info, "actor"));
	CHECK_FALSE(Core::IsLogEnabled(Core::LogLevel::Debug, "actors"));
	for (auto invalid : {"ai,", ",ai", "ai,,actors", "ai, actors", "ai,*"}) {
		CHECK_FALSE(Core::SetLogChannels(invalid));
		CHECK_FALSE(Core::IsLogEnabled(Core::LogLevel::Info, "content"));
		CHECK(Core::IsLogEnabled(Core::LogLevel::Info, "ai"));
	}
	CHECK(Core::IsLogEnabled(Core::LogLevel::Warn, "content"));
	CHECK(Core::IsLogEnabled(Core::LogLevel::Error, "crash"));
	Core::SetLogLevel(Core::LogLevel::Off);
	CHECK_FALSE(Core::IsLogEnabled(Core::LogLevel::Error, "crash"));
	CHECK_FALSE(Core::IsLogEnabled(Core::LogLevel::Off, "ai"));
	Core::SetLogLevel(Core::LogLevel::Trace);
	REQUIRE(Core::SetLogChannels("*"));
	CHECK(Core::IsLogEnabled(Core::LogLevel::Trace, "content"));
	REQUIRE(Core::SetLogChannels(""));
	CHECK(Core::IsLogEnabled(Core::LogLevel::Debug, "actors"));
}

TEST_CASE("Disabled log messages skip formatting", "[core][log]")
{
	LogScope scope;
	int formats = 0;
	VS_LOG_DEBUG("ai", "{}", CountedLogValue{&formats});
	Core::SetLogLevel(Core::LogLevel::Debug);
	REQUIRE(Core::SetLogChannels("actors"));
	VS_LOG_DEBUG("ai", "{}", CountedLogValue{&formats});
	Core::SetLogLevel(Core::LogLevel::Off);
	VS_LOG_ERROR("ai", "{}", CountedLogValue{&formats});
	CHECK(formats == 0);
	Core::SetLogLevel(Core::LogLevel::Debug);
	VS_LOG_DEBUG("actors", "{}", CountedLogValue{&formats});
	CHECK(formats == 1);
}

TEST_CASE("File logging includes tick context and filters C diagnostics", "[core][log]")
{
	LogScope scope;
	REQUIRE(Core::SetLogFile(scope.path.string().c_str()));
	Core::SetLogTick(5301);
	Core::SetLogLevel(Core::LogLevel::Debug);
	REQUIRE(Core::SetLogChannels("ai"));
	Core_Debugf("ai", "actor=%d", 7);
	Core_Debugf("actors", "hidden debug");
	Core_Tracef("hidden trace");
	VS_LOG_WARN("content", "still visible");
	Core::SetLogLevel(Core::LogLevel::Trace);
	REQUIRE(Core::SetLogChannels("trace"));
	Core_Tracef("selected trace %d", 9);
	Core::SetLogLevel(Core::LogLevel::Off);
	Core::LogWrite(Core::LogLevel::Error, "crash", "hidden error");
	REQUIRE(Core::SetLogFile(""));
	std::ifstream file(scope.path);
	const std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	CHECK(text.find("[debug] [tick=5301] ai: actor=7") != std::string::npos);
	CHECK(text.find("[warn] [tick=5301] content: still visible") != std::string::npos);
	CHECK(text.find("[trace] [tick=5301] trace: selected trace 9") != std::string::npos);
	CHECK(text.find("hidden") == std::string::npos);
	file.close();
	REQUIRE(Core::SetLogFile(scope.path.string().c_str()));
	REQUIRE(Core::SetLogFile(nullptr));
	CHECK(std::filesystem::file_size(scope.path) == 0);
}
