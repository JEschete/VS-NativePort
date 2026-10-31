#include "Launcher/Setup.hpp"

#include <Windows.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "Content/DataStore.hpp"
#include "Content/Disc.hpp"
#include "Platform/App.hpp"
#include "Platform/Paths.hpp"
#include "Save/SaveStore.hpp"
#include "nlohmann/json.hpp"

namespace VS::Launcher {
namespace {
namespace fs = std::filesystem;
using Platform::PathText;
using Platform::Utf8Path;
constexpr int kPipelineVersion = 2;
constexpr auto kReadyFile = "launcher-assets.json";

std::string Upper(std::string text) {
  std::ranges::transform(text, text.begin(), [](unsigned char value) {
    return static_cast<char>(std::toupper(value));
  });
  return text;
}

Result RegularFile(const fs::path& path) {
  std::error_code error;
  if (!fs::is_regular_file(path, error)) {
    return std::unexpected("Missing or unreadable file: " + PathText(path) +
                           (error ? " (" + error.message() + ")" : ""));
  }
  return {};
}

std::expected<nlohmann::json, std::string> ReadJson(const fs::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) return std::unexpected("Cannot read " + PathText(path));
  auto value = nlohmann::json::parse(input, nullptr, false);
  if (value.is_discarded()) {
    return std::unexpected("Invalid JSON in " + PathText(path));
  }
  return value;
}

Result WriteJson(const fs::path& path, const nlohmann::json& value) {
  auto temporary = path;
  temporary += L".tmp";
  std::ofstream output(temporary, std::ios::binary);
  output << value.dump(2) << '\n';
  output.close();
  if (!output) return std::unexpected("Cannot write " + PathText(temporary));
  if (!MoveFileExW(temporary.c_str(), path.c_str(),
                   MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
    return std::unexpected(
        "Cannot commit " + PathText(path) + ": " +
        std::system_category().message(static_cast<int>(GetLastError())));
  }
  return {};
}

nlohmann::json SourceIdentity(const fs::path& image,
                              std::string_view checksum) {
  return {{"image", PathText(fs::absolute(image).lexically_normal())},
          {"bytes", fs::file_size(image)},
          {"modified", fs::last_write_time(image).time_since_epoch().count()},
          {"sha256", checksum}};
}

Result SealAssets(const fs::path& data, const VerifiedDisc& source_disc,
                  const Content::Disc& disc) {
  Content::DataStore::ScopedOverride restore;
  if (auto result = Content::DataStore::Open(data); !result) {
    return result;
  }
  const auto* store = Content::DataStore::Get();
  for (const auto& source : disc.Files()) {
    if (source.directory) continue;
    const auto* entry = store->Find(source.path);
    if (!entry || entry->lba != source.lba || entry->size != source.size) {
      return std::unexpected("Incomplete extraction manifest: " + source.path);
    }
    if (source.interleaved && !source.path.ends_with(".STR")) continue;
    if (entry->form != (source.interleaved ? "mkv" : "raw")) {
      return std::unexpected("Asset conversion did not complete: " +
                             source.path);
    }
    const auto asset = store->FileOf(*entry);
    if (auto result = RegularFile(asset); !result) {
      return result;
    }
    const auto bytes = fs::file_size(asset);
    if ((!source.interleaved && bytes != source.size) ||
        (source.interleaved && bytes == 0)) {
      return std::unexpected("Incomplete asset: " + PathText(asset));
    }
  }
  if (auto result = RegularFile(data / "audio" / "streams" / "index.json");
      !result)
    return result;
  bool bank = false;
  nlohmann::json files = nlohmann::json::array();
  for (const auto& entry : fs::recursive_directory_iterator(data)) {
    if (entry.is_symlink()) {
      return std::unexpected("Unexpected asset link: " +
                             PathText(entry.path()));
    }
    if (!entry.is_regular_file()) continue;
    bank = bank || entry.path().filename() == "bank.json";
    files.push_back({{"path", PathText(fs::relative(entry.path(), data))},
                     {"bytes", entry.file_size()}});
  }
  if (!bank)
    return std::unexpected("Audio bank import did not produce any banks");
  return WriteJson(
      data / kReadyFile,
      {{"version", kPipelineVersion},
       {"source", SourceIdentity(source_disc.image, source_disc.sha256)},
       {"files", files}});
}

Result WritableSaves(const fs::path& path) {
  const auto store = Save::SaveStore::Open(path);
  if (!store) return std::unexpected(store.error());
  const auto probe =
      store->Root() /
      (".launcher-check-" + std::to_string(GetCurrentProcessId()) + "-" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()));
  std::ofstream output(probe, std::ios::binary);
  output << "write check";
  output.close();
  const bool writable = static_cast<bool>(output);
  std::error_code error;
  const bool removed = fs::remove(probe, error);
  if (!writable) {
    return std::unexpected(
        "The save folder is not writable: " + PathText(path) +
        (error ? "; probe cleanup: " + error.message() : ""));
  }
  if (!removed || error) {
    return std::unexpected("Cannot remove save-folder write check: " +
                           PathText(probe) + ": " + error.message());
  }
  return {};
}
}  // namespace

std::expected<fs::path, std::string> ValidateCue(const fs::path& cue) {
  if (Upper(PathText(cue.extension())) != ".CUE") {
    return std::unexpected("Select a .cue file, not the .bin image.");
  }
  if (auto result = RegularFile(cue); !result) {
    return std::unexpected(result.error());
  }
  std::ifstream input(cue, std::ios::binary);
  std::string line;
  fs::path image;
  int tracks = 0;
  int indices = 0;
  constexpr std::size_t kMaximumCueBytes = 64 * 1024;
  if (fs::file_size(cue) > kMaximumCueBytes) {
    return std::unexpected(
        "The selected CUE is too large to be a supported descriptor.");
  }
  while (std::getline(input, line)) {
    if (line.starts_with("\xEF\xBB\xBF")) line.erase(0, 3);
    if (line.find('\0') != std::string::npos) {
      return std::unexpected("Save the CUE as UTF-8 text, not UTF-16.");
    }
    std::istringstream words(line);
    std::string command;
    words >> command;
    command = Upper(command);
    if (command.empty() || command == "REM") continue;
    if (command == "FILE") {
      if (!image.empty()) {
        return std::unexpected(
            "Only a single-BIN USA SLUS-01040 dump is supported.");
      }
      std::string name;
      std::string type;
      words >> std::quoted(name, '"', '\0') >> type;
      if (name.empty() || Upper(type) != "BINARY") {
        return std::unexpected(
            "The CUE must contain FILE \"image.bin\" BINARY.");
      }
      image = cue.parent_path() / Utf8Path(name);
      if (Upper(PathText(image.extension())) != ".BIN") {
        return std::unexpected("The CUE must reference a .bin file.");
      }
      if (auto result = RegularFile(image); !result) {
        return std::unexpected(
            "The BIN referenced by the CUE is missing or "
            "unreadable. Keep it beside the CUE with its "
            "original name.\n" +
            result.error());
      }
    } else if (command == "TRACK") {
      int number = 0;
      std::string mode;
      if (!(words >> number >> mode) || image.empty() || ++tracks != 1 ||
          number != 1 || Upper(mode) != "MODE2/2352") {
        return std::unexpected(
            "Use a single-track MODE2/2352 USA SLUS-01040 dump.");
      }
    } else if (command == "INDEX") {
      int number = 0;
      std::string time;
      if (!(words >> number >> time) || tracks != 1 || ++indices != 1 ||
          number != 1 || time != "00:00:00") {
        return std::unexpected(
            "Unsupported CUE index; expected INDEX 01 00:00:00.");
      }
    } else if (command == "PREGAP" || command == "POSTGAP") {
      return std::unexpected(
          "CUE gaps are not supported by this tester build.");
    } else if (command == "TITLE" || command == "PERFORMER" ||
               command == "SONGWRITER" || command == "CATALOG") {
      continue;
    } else {
      return std::unexpected(
          "Unsupported CUE directive: " + command +
          ". Use the standard single-track descriptor for the supported dump.");
    }
    std::string extra;
    if (words >> extra) {
      return std::unexpected("Unexpected data after CUE " + command + ".");
    }
  }
  if (!input.eof()) return std::unexpected("Cannot read the CUE file.");
  if (image.empty())
    return std::unexpected("The CUE has no FILE entry for its BIN.");
  if (tracks != 1)
    return std::unexpected("The CUE has no supported TRACK entry.");
  if (indices != 1)
    return std::unexpected("The CUE has no supported INDEX entry.");
  return fs::absolute(image).lexically_normal();
}

fs::path FindFfmpeg() {
  std::vector<wchar_t> buffer(32768);
  const DWORD length =
      SearchPathW(nullptr, L"ffmpeg.exe", nullptr,
                  static_cast<DWORD>(buffer.size()), buffer.data(), nullptr);
  if (length != 0 && length < buffer.size()) return fs::path(buffer.data());
  const DWORD count = GetEnvironmentVariableW(
      L"LOCALAPPDATA", buffer.data(), static_cast<DWORD>(buffer.size()));
  if (count == 0 || count >= buffer.size()) return {};
  const auto winget = fs::path(buffer.data()) / "Microsoft" / "WinGet";
  const auto link = winget / "Links" / "ffmpeg.exe";
  if (fs::is_regular_file(link)) return link;
  const auto packages = winget / "Packages";
  if (!fs::is_directory(packages)) return {};
  for (const auto& package : fs::directory_iterator(packages)) {
    if (!PathText(package.path().filename()).starts_with("Gyan.FFmpeg_"))
      continue;
    for (const auto& entry : fs::recursive_directory_iterator(package.path())) {
      if (entry.path().filename() == "ffmpeg.exe" && entry.is_regular_file()) {
        return entry.path();
      }
    }
  }
  return {};
}

std::expected<Settings, std::string> LoadSettings(const fs::path& state) {
  const auto path = state / "launcher.json";
  if (!fs::exists(path)) return Settings{};
  const auto value = ReadJson(path);
  if (!value) return std::unexpected(value.error());
  if (!value->is_object() || !value->contains("version") ||
      !value->at("version").is_number_integer() || value->at("version") != 1) {
    return std::unexpected("Unsupported launcher settings: " + PathText(path));
  }
  for (const auto* key : {"cue", "saves", "ffmpeg"}) {
    if (!value->contains(key) || !value->at(key).is_string()) {
      return std::unexpected("Invalid launcher setting: " + std::string(key));
    }
  }
  return Settings{Utf8Path(value->at("cue").get<std::string>()),
                  Utf8Path(value->at("saves").get<std::string>()),
                  Utf8Path(value->at("ffmpeg").get<std::string>())};
}

Result SaveSettings(const fs::path& state, const Settings& settings) {
  fs::create_directories(state);
  return WriteJson(state / "launcher.json",
                   {{"version", 1},
                    {"cue", PathText(settings.cue)},
                    {"saves", PathText(settings.saves)},
                    {"ffmpeg", PathText(settings.ffmpeg)}});
}

Result CheckInstallation(const fs::path& installation) {
  for (const auto* file : {"VS.Desktop.exe", "VS.Extract.exe", "SDL3.dll",
                           "dav1d.dll", "sfizz.dll", "webm.dll"}) {
    if (auto result = RegularFile(installation / file); !result) {
      return result;
    }
  }
  return {};
}

Result CheckAssets(const fs::path& data, const fs::path& image) {
  const auto ready = ReadJson(data / kReadyFile);
  if (!ready) return std::unexpected(ready.error());
  if (!ready->is_object() || !ready->contains("version") ||
      !ready->at("version").is_number_integer() ||
      ready->at("version") != kPipelineVersion || !ready->contains("source") ||
      ready->at("source") != SourceIdentity(image, kSupportedDiscSha256) ||
      !ready->contains("files") || !ready->at("files").is_array() ||
      ready->at("files").empty()) {
    return std::unexpected(
        "Assets need preparation for this disc/pipeline version.");
  }
  for (const auto& entry : ready->at("files")) {
    if (!entry.is_object() || !entry.contains("path") ||
        !entry.at("path").is_string() || !entry.contains("bytes") ||
        !entry.at("bytes").is_number_unsigned()) {
      return std::unexpected(
          "Invalid asset inventory; preparation is required.");
    }
    const auto relative = Utf8Path(entry.at("path").get<std::string>());
    if (relative.empty() || relative.has_root_path() ||
        std::ranges::find(relative, fs::path("..")) != relative.end()) {
      return std::unexpected("Unsafe path in asset inventory.");
    }
    const auto asset = data / relative;
    if (auto result = RegularFile(asset); !result) {
      return result;
    }
    if (fs::file_size(asset) != entry.at("bytes").get<std::uintmax_t>()) {
      return std::unexpected("Asset size changed: " + PathText(asset));
    }
  }
  return {};
}

std::expected<fs::path, std::string> Prepare(const Settings& settings,
                                             const fs::path& installation,
                                             const fs::path& state,
                                             const Progress& progress) {
  if (auto result = CheckInstallation(installation); !result) {
    return std::unexpected(result.error() + "\nExtract the entire tester ZIP.");
  }
  progress({SetupStage::kDisc, "Verifying the supported USA disc...", 0});
  const auto verified = VerifyDisc(settings.cue, [&](int percent) {
    progress({SetupStage::kDisc, "Verifying BIN checksum...", percent});
  });
  if (!verified) return std::unexpected(verified.error());
  if (auto result = WritableSaves(settings.saves); !result) {
    return std::unexpected(result.error());
  }
  fs::create_directories(state);
  progress({SetupStage::kCache, "Disc verified. Checking prepared assets..."});
  auto disc = Content::Disc::Open(verified->image);
  if (!disc) return std::unexpected(disc.error());
  if (!(*disc)->Find("SLUS_010.40")) {
    return std::unexpected(
        "This build requires Vagrant Story USA (SLUS-01040).");
  }
  const auto data = state / "assets";
  const auto ready = CheckAssets(data, verified->image);
  if (ready) {
    progress({SetupStage::kReady, "Assets ready. No extraction needed.", 100});
    return data;
  }
  progress(
      {SetupStage::kCache,
       "Preparing verified game assets. The game will start when complete."});
  if (auto result = RegularFile(settings.ffmpeg); !result) {
    return std::unexpected(
        "FFmpeg is required for first-time setup. Select "
        "ffmpeg.exe, use Install with winget, or Download FFmpeg.");
  }
  const auto probe = Platform::RunProcess(
      std::vector<std::string>{PathText(settings.ffmpeg), "-version"},
      state / "setup.log");
  if (!probe || *probe != 0) {
    return std::unexpected("FFmpeg could not run. See " +
                           PathText(state / "setup.log"));
  }
  const auto suffix = std::to_string(
      std::chrono::steady_clock::now().time_since_epoch().count());
  const auto staging = state / ("assets-staging-" + suffix);
  if (!fs::create_directory(staging)) {
    return std::unexpected("Setup staging already exists: " +
                           PathText(staging));
  }
  const auto run = [&](std::vector<std::string> arguments) -> Result {
    const auto result = Platform::RunProcess(arguments, state / "setup.log");
    if (!result || *result != 0) {
      return std::unexpected(
          "Asset setup failed: " +
          (result ? "exit code " + std::to_string(*result) : result.error()) +
          "\nSee " + PathText(state / "setup.log") +
          "\nIncomplete assets retained at " + PathText(staging));
    }
    return {};
  };
  progress({SetupStage::kExtract,
            "Preparing disc files, intro and streamed audio..."});
  if (auto result = run({PathText(installation / "VS.Extract.exe"), "extract",
                         PathText(verified->image), PathText(staging),
                         "--ffmpeg", PathText(settings.ffmpeg)});
      !result) {
    return std::unexpected(result.error());
  }
  progress({SetupStage::kAudio, "Importing music and sound banks..."});
  if (auto result = run({PathText(installation / "VS.Extract.exe"), "audio",
                         PathText(staging / "disc"), PathText(staging)});
      !result) {
    return std::unexpected(result.error());
  }
  progress({SetupStage::kOutputs, "Verifying all prepared assets..."});
  if (auto result = SealAssets(staging, *verified, **disc); !result) {
    return std::unexpected(result.error() +
                           "\nIncomplete assets: " + PathText(staging));
  }
  const auto previous = state / ("assets-previous-" + suffix);
  const bool replacing = fs::exists(data);
  if (replacing) fs::rename(data, previous);
  std::error_code error;
  fs::rename(staging, data, error);
  if (error) {
    if (replacing) {
      std::error_code rollback;
      fs::rename(previous, data, rollback);
      if (rollback) {
        return std::unexpected(
            "Cannot activate assets: " + error.message() +
            "; cannot restore previous assets: " + rollback.message() +
            ". Previous assets remain at " + PathText(previous));
      }
    }
    return std::unexpected("Cannot activate prepared assets: " +
                           error.message());
  }
  progress({SetupStage::kReady, "Assets ready.", 100});
  return data;
}

std::vector<std::string> GameArguments(const fs::path& installation,
                                       const fs::path& data,
                                       const Settings& settings) {
  return {PathText(installation / "VS.Desktop.exe"), "--data", PathText(data),
          "--save-dir", PathText(settings.saves)};
}
}  // namespace VS::Launcher
