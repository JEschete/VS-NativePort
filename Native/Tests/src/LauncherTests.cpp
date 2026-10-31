#include <Windows.h>
#include <winioctl.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Launcher/Setup.hpp"
#include "Launcher/Updates.hpp"
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
    std::error_code error;
    fs::remove_all(root.parent_path(), error);
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

TEST_CASE("Launcher reads the latest release version and page",
          "[launcher][updates]") {
  const auto release = Launcher::ParseLatestRelease(
      R"({"tag_name":"0.0.9","name":"Alpha","html_url":)"
      R"("https://github.com/JEschete/VS-NativePort/releases/tag/0.0.9"})");
  REQUIRE(release.has_value());
  CHECK(release->version == "0.0.9");
  CHECK(release->url ==
        "https://github.com/JEschete/VS-NativePort/releases/tag/0.0.9");
}

TEST_CASE("Launcher rejects an unusable latest release response",
          "[launcher][updates]") {
  constexpr auto kPage =
      "https://github.com/JEschete/VS-NativePort/releases/tag/0.0.9";
  const auto body = [](const nlohmann::json& tag, const nlohmann::json& url) {
    return nlohmann::json{{"tag_name", tag}, {"html_url", url}}.dump();
  };
  const auto response = GENERATE_COPY(
      std::string("not json"), std::string("{}"), std::string("[]"),
      body(9, kPage), body("", kPage), body("Alpha-Testing", kPage),
      body("0.0.9", nullptr), body("0.0.9", "https://example.com/0.0.9"),
      body("0.0.9", "http://github.com/JEschete/VS-NativePort/releases/x"),
      body("0.0.9",
           "https://github.com/JEschete/VS-NativePort.evil/releases/x"));
  CAPTURE(response);
  CHECK_FALSE(Launcher::ParseLatestRelease(response).has_value());
}

TEST_CASE("Launcher compares release versions numerically",
          "[launcher][updates]") {
  using enum Launcher::UpdateStatus;
  struct Row {
    const char* installed;
    const char* latest;
    Launcher::UpdateStatus status;
  };
  const auto row = GENERATE(
      Row{"0.0.8", "0.0.9", kAvailable}, Row{"0.0.9", "0.0.9", kCurrent},
      Row{"0.0.9", "0.0.10", kAvailable}, Row{"0.0.10", "0.0.9", kCurrent},
      Row{"v0.0.9", "0.0.9", kCurrent}, Row{"1.0", "1.0.0", kCurrent},
      Row{"1.0", "1.0.1", kAvailable}, Row{"", "0.0.9", kUnknownInstalled},
      Row{"alpha-2026-10-30-r4", "0.0.9", kUnknownInstalled},
      Row{"99999999999999999999999.0", "0.0.9", kUnknownInstalled});
  CAPTURE(row.installed, row.latest);
  CHECK(Launcher::CompareRelease(row.installed, row.latest) == row.status);
}

TEST_CASE("Launcher reads its version from the packaged release manifest",
          "[launcher][updates]") {
  const LauncherFixture fixture;
  const auto manifest = fixture.root / "release.json";
  CHECK(Launcher::InstalledVersion(fixture.root).empty());

  // package-release.ps1 writes this file with a UTF-8 BOM.
  std::ofstream(manifest, std::ios::binary)
      << "\xEF\xBB\xBF{\"version\":\"0.0.9\"}";
  CHECK(Launcher::InstalledVersion(fixture.root) == "0.0.9");

  std::ofstream(manifest, std::ios::binary) << "{\"version\":9}";
  CHECK(Launcher::InstalledVersion(fixture.root).empty());

  std::ofstream(manifest, std::ios::binary) << "damaged";
  CHECK(Launcher::InstalledVersion(fixture.root).empty());
}

namespace {
std::string PackageSha256() {
  return "3b9dbdf03a7fe067bffb52b4ce3d3708e2f9fe6f44d4cd523335f5f3f065ab90";
}

std::string PackageUrl() {
  return "https://github.com/JEschete/VS-NativePort/releases/download/0.0.9/"
         "VS-NativePort-0.0.9-win-x64.zip";
}

nlohmann::json PackageAsset() {
  return {{"name", "VS-NativePort-0.0.9-win-x64.zip"},
          {"state", "uploaded"},
          {"size", 6295410},
          {"digest", "sha256:" + PackageSha256()},
          {"browser_download_url", PackageUrl()}};
}

std::string ReleaseWith(const nlohmann::json& assets) {
  return nlohmann::json{
      {"tag_name", "0.0.9"},
      {"html_url",
       "https://github.com/JEschete/VS-NativePort/releases/tag/0.0.9"},
      {"assets", assets}}
      .dump();
}

std::string Contents(const fs::path& path) {
  std::ifstream input(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(input), {}};
}

// A disposable install folder and staged package, shaped like real releases.
struct UpdateFixture {
  fs::path root =
      fs::temp_directory_path() /
      ("vs-update-" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()));
  fs::path install = root / "install";
  fs::path stage = install / Launcher::kUpdateStagingDirectory;
  fs::path backup = install / Launcher::kUpdateBackupDirectory;

  UpdateFixture() { fs::create_directories(install); }
  ~UpdateFixture() {
    std::error_code error;
    fs::remove_all(root, error);
  }
  static std::vector<std::string> Required() {
    return {"VS.Launcher.exe", "VS.Desktop.exe", "VS.Extract.exe", "SDL3.dll",
            "dav1d.dll",       "sfizz.dll",      "webm.dll"};
  }
  // Writes each file as `tag path` and a matching release.json manifest.
  static void Package(const fs::path& directory, const std::string& version,
                      const std::string& tag,
                      const std::vector<std::string>& extra = {}) {
    auto names = Required();
    names.insert(names.end(), extra.begin(), extra.end());
    auto files = nlohmann::json::array();
    for (const auto& name : names) {
      const auto path = directory / name;
      fs::create_directories(path.parent_path());
      std::ofstream(path, std::ios::binary) << tag << ' ' << name;
      files.push_back(
          {{"path", name}, {"sha256", *Launcher::Sha256File(path)}});
    }
    std::ofstream(directory / "release.json", std::ios::binary)
        << nlohmann::json{{"version", version}, {"files", files}}.dump();
  }
  // Rewrites one manifest entry after Package, to build invalid packages.
  void EditManifest(const std::function<void(nlohmann::json&)>& edit) const {
    auto manifest = nlohmann::json::parse(Contents(stage / "release.json"));
    edit(manifest);
    std::ofstream(stage / "release.json", std::ios::binary) << manifest.dump();
  }
};
}  // namespace

// Opt-in: reads the real published 0.0.8 package from GitHub.
TEST_CASE("Launcher downloads a real package and checks GitHub's checksum",
          "[.network]") {
  const UpdateFixture fixture;
  Launcher::LatestRelease release;
  release.version = "0.0.8";
  release.asset_url =
      "https://github.com/JEschete/VS-NativePort/releases/download/0.0.8/"
      "VS-NativePort-alpha-2026-10-30-r4-win-x64.zip";
  release.asset_bytes = 6295410;
  release.asset_sha256 =
      "3B9DBDF03A7FE067BFFB52B4CE3D3708E2F9FE6F44D4CD523335F5F3F065AB90";
  const auto file = fixture.root / "package.zip";
  const auto result = Launcher::DownloadReleasePackage(release, file, {});
  INFO((result ? std::string{} : result.error()));
  CHECK(result.has_value());
  CHECK(fs::file_size(file) == release.asset_bytes);

  release.asset_sha256 = std::string(64, '0');
  CHECK_FALSE(Launcher::DownloadReleasePackage(release, file, {}));
}

// Opt-in: checks a real packaged ZIP named by VS_UPDATE_TEST_ZIP.
TEST_CASE("Launcher can stage a real release package", "[.package]") {
  std::vector<wchar_t> path(MAX_PATH);
  const auto length =
      GetEnvironmentVariableW(L"VS_UPDATE_TEST_ZIP", path.data(), MAX_PATH);
  REQUIRE(length > 0);
  const UpdateFixture fixture;
  const auto extracted =
      Launcher::ExtractRelease(fs::path(path.data()), fixture.stage);
  INFO((extracted ? std::string{} : extracted.error()));
  REQUIRE(extracted.has_value());
  const auto version = Launcher::InstalledVersion(fixture.stage);
  CAPTURE(version);
  const auto verified = Launcher::VerifyStagedRelease(fixture.stage, version);
  INFO((verified ? std::string{} : verified.error()));
  CHECK(verified.has_value());
  const std::vector<std::string> self_check{
      VS::Platform::PathText(fixture.stage / "VS.Launcher.exe"),
      "--self-check"};
  CHECK(VS::Platform::RunProcess(self_check) == 0);
}

TEST_CASE("Launcher selects the release package it can verify",
          "[launcher][updates][install]") {
  auto decoy = PackageAsset();
  decoy["name"] = "VS-NativePort-0.0.9-source.zip";
  const auto release = Launcher::ParseLatestRelease(
      ReleaseWith(nlohmann::json::array({decoy, PackageAsset()})));
  REQUIRE(release.has_value());
  CHECK(release->asset_url == PackageUrl());
  CHECK(release->asset_bytes == 6295410);
  CHECK(release->asset_sha256 ==
        "3B9DBDF03A7FE067BFFB52B4CE3D3708E2F9FE6F44D4CD523335F5F3F065AB90");
}

TEST_CASE("Launcher will not auto-install a package it cannot verify",
          "[launcher][updates][install]") {
  const auto with = [](const char* key, const nlohmann::json& value) {
    auto asset = PackageAsset();
    asset[key] = value;
    return asset;
  };
  auto no_digest = PackageAsset();
  no_digest.erase("digest");
  const auto asset = GENERATE_COPY(
      no_digest, with("name", "VS-NativePort-0.0.8-win-x64.zip"),
      with("state", "starter"), with("size", 0),
      with("size", std::uint64_t{1} << 40), with("digest", "sha256:abc"),
      with("digest", "md5:" + PackageSha256()),
      with("browser_download_url", "https://example.com/package.zip"),
      with("browser_download_url",
           "https://github.com/Other/VS-NativePort/releases/download/0.0.9/"
           "VS-NativePort-0.0.9-win-x64.zip"));
  CAPTURE(asset.dump());
  const auto release =
      Launcher::ParseLatestRelease(ReleaseWith(nlohmann::json::array({asset})));
  REQUIRE(release.has_value());
  CHECK(release->version == "0.0.9");
  CHECK(release->asset_url.empty());
}

TEST_CASE("Launcher accepts a complete staged release",
          "[launcher][updates][install]") {
  const UpdateFixture fixture;
  UpdateFixture::Package(fixture.stage, "0.0.9", "new", {"licenses/a.txt"});
  const auto result = Launcher::VerifyStagedRelease(fixture.stage, "0.0.9");
  INFO((result ? std::string{} : result.error()));
  CHECK(result.has_value());
}

TEST_CASE("Launcher rejects a staged release that does not match its manifest",
          "[launcher][updates][install]") {
  const UpdateFixture fixture;
  UpdateFixture::Package(fixture.stage, "0.0.9", "new");
  const auto bad_path = [&](const char* path) {
    fixture.EditManifest(
        [&](nlohmann::json& manifest) { manifest["files"][0]["path"] = path; });
  };
  const auto defect =
      GENERATE(as<std::string>{}, "wrong version", "changed file",
               "missing file", "unlisted file", "parent path", "absolute path",
               "stream path", "required file not listed", "damaged manifest");
  CAPTURE(defect);
  std::string version = "0.0.9";
  if (defect == "wrong version") {
    version = "0.0.10";
  } else if (defect == "changed file") {
    std::ofstream(fixture.stage / "SDL3.dll", std::ios::binary) << "tampered";
  } else if (defect == "missing file") {
    fs::remove(fixture.stage / "SDL3.dll");
  } else if (defect == "unlisted file") {
    std::ofstream(fixture.stage / "unlisted.dll") << "extra";
  } else if (defect == "parent path") {
    bad_path("../outside.dll");
  } else if (defect == "absolute path") {
    bad_path("C:/Windows/outside.dll");
  } else if (defect == "stream path") {
    bad_path("VS.Launcher.exe:stream");
  } else if (defect == "required file not listed") {
    fixture.EditManifest(
        [](nlohmann::json& manifest) { manifest["files"].erase(0); });
    fs::remove(fixture.stage / "VS.Launcher.exe");
  } else {
    std::ofstream(fixture.stage / "release.json") << "damaged";
  }
  CHECK_FALSE(Launcher::VerifyStagedRelease(fixture.stage, version));
}

TEST_CASE("Launcher update swaps release files and leaves everything else",
          "[launcher][updates][install]") {
  const UpdateFixture fixture;
  UpdateFixture::Package(fixture.install, "0.0.8", "old", {"retired.dll"});
  UpdateFixture::Package(fixture.stage, "0.0.9", "new", {"licenses/a.txt"});
  fs::create_directories(fixture.install / "my saves");
  std::ofstream(fixture.install / "my saves" / "slot.bin") << "player data";

  const auto result =
      Launcher::ApplyStagedRelease(fixture.stage, fixture.install);
  INFO((result ? std::string{} : result.error()));
  REQUIRE(result.has_value());

  CHECK(Launcher::InstalledVersion(fixture.install) == "0.0.9");
  for (const auto& name : UpdateFixture::Required()) {
    CHECK(Contents(fixture.install / name) == "new " + name);
    CHECK(Contents(fixture.backup / name) == "old " + name);
  }
  CHECK(Contents(fixture.install / "licenses" / "a.txt") ==
        "new licenses/a.txt");
  CHECK_FALSE(fs::exists(fixture.install / "retired.dll"));
  CHECK(Contents(fixture.install / "my saves" / "slot.bin") == "player data");
  CHECK_FALSE(fs::exists(fixture.backup / Launcher::kUpdateMarker));
}

TEST_CASE("Launcher update restores every file when a swap fails",
          "[launcher][updates][install]") {
  const UpdateFixture fixture;
  UpdateFixture::Package(fixture.install, "0.0.8", "old", {"retired.dll"});
  UpdateFixture::Package(fixture.stage, "0.0.9", "new", {"licenses/a.txt"});

  // No sharing, so this file cannot be renamed; earlier ones are swapped first.
  const auto locked = fixture.install / "webm.dll";
  const HANDLE lock =
      CreateFileW(locked.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING,
                  FILE_ATTRIBUTE_NORMAL, nullptr);
  REQUIRE(lock != INVALID_HANDLE_VALUE);
  const auto result =
      Launcher::ApplyStagedRelease(fixture.stage, fixture.install);
  CloseHandle(lock);

  CHECK_FALSE(result.has_value());
  CHECK(Launcher::InstalledVersion(fixture.install) == "0.0.8");
  for (const auto& name : UpdateFixture::Required()) {
    CHECK(Contents(fixture.install / name) == "old " + name);
  }
  CHECK(Contents(fixture.install / "retired.dll") == "old retired.dll");
  CHECK_FALSE(fs::exists(fixture.install / "licenses" / "a.txt"));
  CHECK_FALSE(fs::exists(fixture.backup / Launcher::kUpdateMarker));
}

TEST_CASE("Launcher refuses to replace a build that is not a release",
          "[launcher][updates][install]") {
  const UpdateFixture fixture;
  std::ofstream(fixture.install / "VS.Launcher.exe") << "developer build";
  UpdateFixture::Package(fixture.stage, "0.0.9", "new");

  CHECK_FALSE(Launcher::ApplyStagedRelease(fixture.stage, fixture.install));
  CHECK(Contents(fixture.install / "VS.Launcher.exe") == "developer build");
}

TEST_CASE("Launcher finishes the rollback of an interrupted update",
          "[launcher][updates][install]") {
  const UpdateFixture fixture;
  fs::create_directories(fixture.backup);
  const auto write = [](const fs::path& path, const char* text) {
    std::ofstream(path, std::ios::binary) << text;
  };
  // Swapped, half-swapped, not reached yet, and added by the new version.
  write(fixture.install / "swapped.dll", "new");
  write(fixture.backup / "swapped.dll", "old");
  write(fixture.backup / "half.dll", "old");
  write(fixture.install / "waiting.dll", "old");
  write(fixture.install / "added.dll", "new");
  write(fixture.backup / Launcher::kUpdateMarker,
        R"({"files":["swapped.dll","half.dll","waiting.dll","added.dll"],)"
        R"("added":["added.dll"]})");

  const auto result = Launcher::RecoverInterruptedUpdate(fixture.install);
  INFO((result ? std::string{} : result.error()));
  REQUIRE(result.has_value());

  CHECK(Contents(fixture.install / "swapped.dll") == "old");
  CHECK(Contents(fixture.install / "half.dll") == "old");
  CHECK(Contents(fixture.install / "waiting.dll") == "old");
  CHECK_FALSE(fs::exists(fixture.install / "added.dll"));
  CHECK_FALSE(fs::exists(fixture.backup / Launcher::kUpdateMarker));

  // Recovery has nothing left to do, even if it is interrupted and rerun.
  CHECK(Launcher::RecoverInterruptedUpdate(fixture.install).has_value());
  CHECK(Contents(fixture.install / "waiting.dll") == "old");
}

TEST_CASE("Launcher removes update leftovers only when no swap is pending",
          "[launcher][updates][install]") {
  const UpdateFixture fixture;
  fs::create_directories(fixture.backup);
  fs::create_directories(fixture.stage);
  std::ofstream(fixture.install / Launcher::kUpdateArchive) << "archive";
  std::ofstream(fixture.install / "SDL3.dll") << "keep";
  std::ofstream(fixture.backup / Launcher::kUpdateMarker) << "{}";

  Launcher::CleanUpdateLeftovers(fixture.install);
  CHECK(fs::exists(fixture.backup / Launcher::kUpdateMarker));

  fs::remove(fixture.backup / Launcher::kUpdateMarker);
  Launcher::CleanUpdateLeftovers(fixture.install);
  CHECK_FALSE(fs::exists(fixture.backup));
  CHECK_FALSE(fs::exists(fixture.stage));
  CHECK_FALSE(fs::exists(fixture.install / Launcher::kUpdateArchive));
  CHECK(fs::exists(fixture.install / "SDL3.dll"));
}

TEST_CASE("Launcher unpacks a release package into staging",
          "[launcher][updates][install]") {
  const UpdateFixture fixture;
  const auto source = fixture.root / "source";
  const auto archive = fixture.root / "package.zip";
  UpdateFixture::Package(source, "0.0.9", "new", {"licenses/a.txt"});
  std::vector<wchar_t> system(MAX_PATH);
  GetSystemDirectoryW(system.data(), MAX_PATH);
  const auto tar = fs::path(system.data()) / "tar.exe";
  const std::vector<std::string> create{VS::Platform::PathText(tar),
                                        "-a",
                                        "-c",
                                        "-f",
                                        VS::Platform::PathText(archive),
                                        "-C",
                                        VS::Platform::PathText(source),
                                        "."};
  REQUIRE(VS::Platform::RunProcess(create) == 0);

  const auto result = Launcher::ExtractRelease(archive, fixture.stage);
  INFO((result ? std::string{} : result.error()));
  REQUIRE(result.has_value());
  CHECK(Contents(fixture.stage / "licenses" / "a.txt") == "new licenses/a.txt");
  CHECK(Launcher::VerifyStagedRelease(fixture.stage, "0.0.9").has_value());

  CHECK_FALSE(Launcher::ExtractRelease(fixture.root / "absent.zip",
                                       fixture.root / "absent-stage"));
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

TEST_CASE("Disc SHA-256 streams known inputs with progress and a read lock",
          "[launcher][disc-identity]") {
  LauncherFixture fixture;
  std::ofstream(fixture.image, std::ios::binary) << "abc";
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
