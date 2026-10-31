#include <Windows.h>
#include <winioctl.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Launcher/Setup.hpp"
#include "Platform/App.hpp"
#include "Platform/Paths.hpp"
#include "nlohmann/json.hpp"

namespace {
namespace fs = std::filesystem;
namespace Launcher = VS::Launcher;
struct LauncherFixture {
  fs::path root =
      fs::temp_directory_path() /
      ("vs-launcher-" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count())) /
      VS::Platform::Utf8Path("paths with spaces \xE6\xB8\xAC\xE8\xA9\xA6");
  fs::path cue = root / "game.cue";
  fs::path image = root / "game image.bin";

  LauncherFixture() {
    fs::create_directories(root);
    std::ofstream(image, std::ios::binary) << "synthetic disc";
    Cue("FILE \"game image.bin\" BINARY\n"
        " TRACK 01 MODE2/2352\n INDEX 01 00:00:00\n");
  }
  ~LauncherFixture() {
    try {
      std::error_code error;
      fs::remove_all(root.parent_path(), error);
    } catch (const std::exception& error) {
      std::fputs(error.what(), stderr);
    }
  }
  void Cue(const std::string& text) const { std::ofstream(cue) << text; }
  void Ready() const {
    fs::create_directories(root / "assets");
    std::ofstream(root / "assets" / "data.bin", std::ios::binary) << "data";
    Json(root / "assets" / "launcher-assets.json",
         {{"version", 2},
          {"source",
           {{"image",
             VS::Platform::PathText(fs::absolute(image).lexically_normal())},
            {"bytes", fs::file_size(image)},
            {"modified", fs::last_write_time(image).time_since_epoch().count()},
            {"sha256", std::string(Launcher::kSupportedDiscSha256)}}},
          {"files", {{{"path", "data.bin"}, {"bytes", std::uintmax_t{4}}}}}});
  }
  static void Json(const fs::path& path, const nlohmann::json& json) {
    std::ofstream(path) << json.dump();
  }
};
}  // namespace

TEST_CASE("Windows processes use UTF-8 for narrow multimedia APIs",
          "[launcher][platform]") {
  CHECK(GetACP() == CP_UTF8);
}

TEST_CASE("Launcher resolves the CUE's actual BIN relative to the CUE",
          "[launcher][content]") {
  LauncherFixture fixture;
  fixture.Cue(
      "REM FILE \"not a file.bin\" BINARY\n"
      "FILE \"game image.bin\" BINARY\n"
      " TRACK 01 MODE2/2352\n INDEX 01 00:00:00\n");
  const auto image = Launcher::ValidateCue(fixture.cue);
  REQUIRE(image.has_value());
  CHECK(*image == fs::absolute(fixture.image));
  CHECK_FALSE(Launcher::ValidateCue(fixture.image).has_value());
}

TEST_CASE("Launcher explains missing BIN files before extraction",
          "[launcher][content]") {
  LauncherFixture fixture;
  fs::remove(fixture.image);
  const auto result = Launcher::ValidateCue(fixture.cue);
  REQUIRE_FALSE(result.has_value());
  CHECK(result.error().find("BIN referenced by the CUE") != std::string::npos);
  CHECK(result.error().find("game image.bin") != std::string::npos);
}

TEST_CASE("Launcher accepts UTF-8 BOMs and preserves CUE backslashes",
          "[launcher][content]") {
  LauncherFixture fixture;
  const auto folder = fixture.root / "sub folder";
  fs::create_directories(folder);
  const auto image = folder / "game image.bin";
  fs::rename(fixture.image, image);
  fixture.Cue(
      "\xEF\xBB\xBF"
      "FILE \"sub folder\\game image.bin\" BINARY\n"
      "TRACK 01 MODE2/2352\nINDEX 01 00:00:00\n");
  const auto result = Launcher::ValidateCue(fixture.cue);
  REQUIRE(result.has_value());
  CHECK(*result == fs::absolute(image));
}

TEST_CASE("Launcher explains unsupported UTF-16 CUE text", "[launcher]") {
  LauncherFixture fixture;
  fixture.Cue(std::string("F\0I\0L\0E\0", 8));
  const auto result = Launcher::ValidateCue(fixture.cue);
  REQUIRE_FALSE(result.has_value());
  CHECK(result.error().find("UTF-16") != std::string::npos);
}

TEST_CASE("Launcher rejects unsupported or incomplete CUE layouts",
          "[launcher][content]") {
  const auto text =
      GENERATE("REM FILE \"game image.bin\" BINARY\n",
               "FILE \"game image.bin\" BINARY\n TRACK 01 MODE1/2048\n INDEX "
               "01 00:00:00\n",
               "FILE \"game image.bin\" BINARY\n TRACK 01 MODE2/2352\n",
               "FILE \"game image.bin\" BINARY\n TRACK 01 MODE2/2352\n INDEX "
               "01 00:02:00\n",
               "FILE \"game image.bin\" BINARY\n TRACK 01 MODE2/2352\n INDEX "
               "01 00:00:00\n TRACK 02 AUDIO\n",
               "FILE \"game image.bin\" BINARY\nFILE \"another.bin\" BINARY\n",
               "FILE \"game image.bin\" BINARY\n TRACK 01 MODE2/2352\n PREGAP "
               "00:02:00\n",
               "TRACK 01 MODE2/2352\nFILE \"game image.bin\" BINARY\nINDEX 01 "
               "00:00:00\n",
               "FILE \"game image.bin\" BINARY\nINDEX 01 00:00:00\nTRACK 01 "
               "MODE2/2352\n",
               "FILE \"game image.bin\" BINARY\nTRACK 01 MODE2/2352 "
               "garbage\nINDEX 01 00:00:00\n",
               "FILE \"game image.bin\" BINARY\nTRACK 01 MODE2/2352\nINDEX 01 "
               "00:00:00\nGARBAGE\n");
  LauncherFixture fixture;
  fixture.Cue(text);
  CHECK_FALSE(Launcher::ValidateCue(fixture.cue).has_value());
}

namespace {
// Every progress report tries to write the file, which the read lock must deny.
void CheckHashHoldsReadLock(const LauncherFixture& fixture) {
  std::vector<int> progress;
  bool attempted_write = false;
  const auto hash = Launcher::Sha256File(fixture.image, [&](int percent) {
    progress.push_back(percent);
    if (!attempted_write) {
      attempted_write = true;
      const auto writer = CreateFileW(fixture.image.c_str(), GENERIC_WRITE,
                                      FILE_SHARE_READ | FILE_SHARE_WRITE,
                                      nullptr, OPEN_EXISTING, 0, nullptr);
      CHECK(writer == INVALID_HANDLE_VALUE);
      if (writer != INVALID_HANDLE_VALUE) CloseHandle(writer);
    }
  });
  REQUIRE(hash.has_value());
  CHECK(*hash ==
        "BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD");
  REQUIRE_FALSE(progress.empty());
  CHECK(progress.front() == 0);
  CHECK(progress.back() == 100);
  CHECK(std::ranges::is_sorted(progress));
}
}  // namespace

TEST_CASE("Disc SHA-256 streams known inputs with progress and a read lock",
          "[launcher][disc-identity]") {
  LauncherFixture fixture;
  std::ofstream(fixture.image, std::ios::binary) << "abc";
  CheckHashHoldsReadLock(fixture);
  std::ofstream(fixture.image, std::ios::binary).close();
  const auto empty = Launcher::Sha256File(fixture.image);
  REQUIRE(empty.has_value());
  CHECK(*empty ==
        "E3B0C44298FC1C149AFBF4C8996FB92427AE41E4649B934CA495991B7852B855");
}

TEST_CASE("Wrong BIN length is rejected before trusting cache or hashing",
          "[launcher][disc-identity]") {
  LauncherFixture fixture;
  fixture.Ready();
  bool hashed = false;
  const auto result =
      Launcher::VerifyDisc(fixture.cue, [&](int) { hashed = true; });
  REQUIRE_FALSE(result.has_value());
  CHECK(result.error().find("Wrong BIN size") != std::string::npos);
  CHECK_FALSE(hashed);
}

TEST_CASE("Prepared-cache metadata cannot bypass source-disc verification",
          "[launcher][disc-identity]") {
  LauncherFixture fixture;
  fixture.Ready();
  REQUIRE(Launcher::CheckAssets(fixture.root / "assets", fixture.image)
              .has_value());
  for (const auto* name : {"VS.Desktop.exe", "VS.Extract.exe", "SDL3.dll",
                           "dav1d.dll", "sfizz.dll", "webm.dll"}) {
    std::ofstream(fixture.root / name) << "synthetic installation";
  }
  const auto saves = fixture.root / "uncreated saves";
  std::vector<Launcher::SetupStage> stages;
  const auto result =
      Launcher::Prepare({fixture.cue, saves, {}}, fixture.root, fixture.root,
                        [&](const Launcher::ProgressUpdate& update) {
                          stages.push_back(update.stage);
                        });
  REQUIRE_FALSE(result.has_value());
  CHECK(result.error().find("Wrong BIN size") != std::string::npos);
  CHECK_FALSE(fs::exists(saves));
  CHECK(std::ranges::all_of(
      stages, [](auto stage) { return stage == Launcher::SetupStage::kDisc; }));
}

TEST_CASE("Correctly sized but wrong BIN bytes fail the checksum gate",
          "[launcher][disc-identity]") {
  LauncherFixture fixture;
  const auto file = CreateFileW(fixture.image.c_str(), GENERIC_WRITE, 0,
                                nullptr, OPEN_EXISTING, 0, nullptr);
  REQUIRE(file != INVALID_HANDLE_VALUE);
  DWORD returned = 0;
  const bool sparse = DeviceIoControl(file, FSCTL_SET_SPARSE, nullptr, 0,
                                      nullptr, 0, &returned, nullptr) != FALSE;
  CloseHandle(file);
  REQUIRE(sparse);
  fs::resize_file(fixture.image, Launcher::kSupportedDiscBytes);
  fixture.Ready();
  const auto result = Launcher::VerifyDisc(fixture.cue);
  REQUIRE_FALSE(result.has_value());
  CHECK(result.error().find("checksum mismatch") != std::string::npos);
  CHECK(result.error().find("Selected SHA-256") != std::string::npos);
  CHECK(result.error().find("No extraction or game launch") !=
        std::string::npos);
}

TEST_CASE("The approved local dump passes exact identity verification",
          "[launcher][disc-identity][disc]") {
  auto dir = VS::Platform::ExecutableDir();
  while (!dir.empty() && dir != dir.parent_path() &&
         !fs::exists(dir / "disks" / "SLUS-01040.cue")) {
    dir = dir.parent_path();
  }
  const auto cue = dir / "disks" / "SLUS-01040.cue";
  if (!fs::exists(cue)) SKIP("Owner-provided supported disc is unavailable");
  const auto result = Launcher::VerifyDisc(cue);
  REQUIRE(result.has_value());
  CHECK(result->sha256 == Launcher::kSupportedDiscSha256);
  CHECK(result->read_lock.get() != nullptr);
}

TEST_CASE("Launcher settings persist Unicode paths and reject damaged settings",
          "[launcher]") {
  LauncherFixture fixture;
  Launcher::Settings settings{fixture.cue, fixture.root / "save folder",
                              fixture.root / "ffmpeg.exe"};
  REQUIRE(Launcher::SaveSettings(fixture.root, settings).has_value());
  const auto loaded = Launcher::LoadSettings(fixture.root);
  REQUIRE(loaded.has_value());
  CHECK(loaded->cue == settings.cue);
  CHECK(loaded->saves == settings.saves);
  CHECK(loaded->ffmpeg == settings.ffmpeg);
  LauncherFixture::Json(fixture.root / "launcher.json", {{"version", "bad"}});
  CHECK_FALSE(Launcher::LoadSettings(fixture.root).has_value());
}

TEST_CASE("First launch has no default CUE save or FFmpeg paths",
          "[launcher][privacy]") {
  LauncherFixture fixture;
  const auto settings = Launcher::LoadSettings(fixture.root / "new profile");
  REQUIRE(settings.has_value());
  CHECK(settings->cue.empty());
  CHECK(settings->saves.empty());
  CHECK(settings->ffmpeg.empty());
  CHECK_FALSE(fs::exists(fixture.root / "new profile"));
}

TEST_CASE("Launcher checks every inventoried asset before reusing extraction",
          "[launcher]") {
  LauncherFixture fixture;
  fixture.Ready();
  REQUIRE(Launcher::CheckAssets(fixture.root / "assets", fixture.image)
              .has_value());
  SECTION("Missing output") {
    fs::remove(fixture.root / "assets" / "data.bin");
  }
  SECTION("Truncated output") {
    std::ofstream(fixture.root / "assets" / "data.bin") << "x";
  }
  SECTION("Changed source disc") {
    std::ofstream(fixture.image, std::ios::app) << "changed";
  }
  CHECK_FALSE(Launcher::CheckAssets(fixture.root / "assets", fixture.image)
                  .has_value());
}

TEST_CASE("Launcher does not accept a missing or invalid completion marker",
          "[launcher]") {
  LauncherFixture fixture;
  fs::create_directories(fixture.root / "assets");
  CHECK_FALSE(Launcher::CheckAssets(fixture.root / "assets", fixture.image)
                  .has_value());
  LauncherFixture::Json(fixture.root / "assets" / "launcher-assets.json",
                        {{"version", "wrong type"}});
  CHECK_FALSE(Launcher::CheckAssets(fixture.root / "assets", fixture.image)
                  .has_value());
}

TEST_CASE(
    "Launcher reports incomplete distributions and passes the chosen save root",
    "[launcher]") {
  LauncherFixture fixture;
  const auto missing = Launcher::CheckInstallation(fixture.root);
  REQUIRE_FALSE(missing.has_value());
  CHECK(missing.error().find("VS.Desktop.exe") != std::string::npos);
  const Launcher::Settings settings{
      fixture.cue, fixture.root / "chosen saves", {}};
  const auto arguments =
      Launcher::GameArguments(fixture.root, fixture.root / "assets", settings);
  REQUIRE(arguments.size() == 5);
  CHECK(arguments[1] == "--data");
  CHECK(arguments[3] == "--save-dir");
  CHECK(VS::Platform::Utf8Path(arguments[4]) == settings.saves);
}

TEST_CASE("Setup subprocesses retain both output streams and their exit code",
          "[launcher][platform]") {
  LauncherFixture fixture;
  const auto output = fixture.root / "setup output.log";
  const std::vector<std::string> arguments{
      "cmd.exe", "/d", "/c",
      "(echo setup stdout) & (echo setup stderr 1>&2) & exit /b 7"};
  const auto result = VS::Platform::RunProcess(arguments, output);
  REQUIRE(result.has_value());
  CHECK(*result == 7);
  std::ifstream input(output);
  const std::string text((std::istreambuf_iterator<char>(input)),
                         std::istreambuf_iterator<char>());
  CHECK(text.find("setup stdout") != std::string::npos);
  CHECK(text.find("setup stderr") != std::string::npos);
  const std::vector<std::string> missing{
      VS::Platform::PathText(fixture.root / "missing.exe")};
  CHECK_FALSE(VS::Platform::RunProcess(missing, output).has_value());
}
