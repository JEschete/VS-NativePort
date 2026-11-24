#include "Launcher/Setup.hpp"

#include <Windows.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "Content/DataStore.hpp"
#include "Content/Disc.hpp"
#include "Content/MovieEncoder.hpp"
#include "Platform/App.hpp"
#include "Platform/Paths.hpp"
#include "Save/SaveStore.hpp"
#include "nlohmann/json.hpp"

namespace VS::Launcher {
namespace {
namespace fs = std::filesystem;
using Platform::PathText;
using Platform::Utf8Path;
constexpr auto kReadyFile = "launcher-assets.json";
constexpr std::string_view kUtf8ByteOrderMark = "\xEF\xBB\xBF";
constexpr std::size_t kLongestPathCharacters = 32768;

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
  if (auto result = store->RequireCatalog(); !result) {
    return result;
  }
  if (auto result =
          store->RequireConversions(Content::SourceVerification::kFull);
      !result) {
    return result;
  }
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
      !result) {
    return result;
  }
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
  if (!bank) {
    return std::unexpected("Audio bank import did not produce any banks");
  }
  return WriteJson(
      data / kReadyFile,
      {{"version", kAssetPipelineVersion},
       {"source", SourceIdentity(source_disc.image, source_disc.sha256)},
       {"files", files}});
}

// Renaming a dump leaves the name inside its CUE stale. The BIN's bytes are
// what identify the disc, so a BIN that is unambiguously the CUE's own will do.
std::optional<fs::path> BinBesideCue(const fs::path& cue) {
  std::error_code error;
  auto same_name = cue;
  same_name.replace_extension(".bin");
  if (fs::is_regular_file(same_name, error)) return same_name;
  std::optional<fs::path> only;
  for (const auto& entry : fs::directory_iterator(cue.parent_path(), error)) {
    if (!entry.is_regular_file(error) ||
        Upper(PathText(entry.path().extension())) != ".BIN") {
      continue;
    }
    if (only) return std::nullopt;
    only = entry.path();
  }
  return only;
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

struct CueSheet {
  fs::path image;
  int tracks = 0;
  int indices = 0;

  Result ReadFile(const fs::path& cue, std::istringstream& words) {
    if (!image.empty()) {
      return std::unexpected(
          "Only a single-BIN USA SLUS-01040 dump is supported.");
    }
    std::string name;
    std::string type;
    words >> std::quoted(name, '"', '\0') >> type;
    if (name.empty() || Upper(type) != "BINARY") {
      return std::unexpected("The CUE must contain FILE \"image.bin\" BINARY.");
    }
    image = cue.parent_path() / Utf8Path(name);
    if (Upper(PathText(image.extension())) != ".BIN") {
      return std::unexpected("The CUE must reference a .bin file.");
    }
    if (auto result = RegularFile(image); !result) {
      const auto beside = BinBesideCue(cue);
      if (!beside) {
        return std::unexpected(
            "The BIN referenced by the CUE is missing or unreadable, and the "
            "folder has no single .bin to use instead. Choose the .bin itself."
            "\n" +
            result.error());
      }
      image = *beside;
    }
    return {};
  }

  Result ReadTrack(std::istringstream& words) {
    int number = 0;
    std::string mode;
    if (!(words >> number >> mode) || image.empty() || ++tracks != 1 ||
        number != 1 || Upper(mode) != "MODE2/2352") {
      return std::unexpected(
          "Use a single-track MODE2/2352 USA SLUS-01040 dump.");
    }
    return {};
  }

  Result ReadIndex(std::istringstream& words) {
    int number = 0;
    std::string time;
    if (!(words >> number >> time) || tracks != 1 || ++indices != 1 ||
        number != 1 || time != "00:00:00") {
      return std::unexpected(
          "Unsupported CUE index; expected INDEX 01 00:00:00.");
    }
    return {};
  }

  Result Read(const fs::path& cue, std::string line) {
    if (line.starts_with(kUtf8ByteOrderMark)) {
      line.erase(0, kUtf8ByteOrderMark.size());
    }
    if (line.find('\0') != std::string::npos) {
      return std::unexpected("Save the CUE as UTF-8 text, not UTF-16.");
    }
    std::istringstream words(line);
    std::string command;
    words >> command;
    command = Upper(command);
    if (command.empty() || command == "REM" || command == "TITLE" ||
        command == "PERFORMER" || command == "SONGWRITER" ||
        command == "CATALOG") {
      return {};
    }
    Result result;
    if (command == "FILE") {
      result = ReadFile(cue, words);
    } else if (command == "TRACK") {
      result = ReadTrack(words);
    } else if (command == "INDEX") {
      result = ReadIndex(words);
    } else if (command == "PREGAP" || command == "POSTGAP") {
      return std::unexpected(
          "CUE gaps are not supported by this tester build.");
    } else {
      return std::unexpected(
          "Unsupported CUE directive: " + command +
          ". Use the standard single-track descriptor for the supported dump.");
    }
    if (!result) return result;
    std::string extra;
    if (words >> extra) {
      return std::unexpected("Unexpected data after CUE " + command + ".");
    }
    return {};
  }
};

Result ProbeFfmpeg(const fs::path& ffmpeg, const fs::path& log) {
  if (auto result = RegularFile(ffmpeg); !result) {
    return std::unexpected(
        "FFmpeg is required for first-time setup. Select "
        "ffmpeg.exe, use Install with winget, or Download FFmpeg.");
  }
  const auto probe = Platform::RunProcess(
      std::vector<std::string>{PathText(ffmpeg), "-version"}, log);
  if (!probe || *probe != 0) {
    return std::unexpected("FFmpeg could not run. See " + PathText(log));
  }
  return {};
}

Result UnlinkedTree(const fs::path& path) {
  const auto attributes = GetFileAttributesW(path.c_str());
  if (attributes == INVALID_FILE_ATTRIBUTES) {
    return std::unexpected("Cannot inspect " + PathText(path));
  }
  if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
    return std::unexpected("Refusing linked storage: " + PathText(path));
  }
  if (fs::is_directory(path)) {
    for (const auto& entry : fs::directory_iterator(path)) {
      if (auto result = UnlinkedTree(entry.path()); !result) {
        return result;
      }
    }
  }
  return {};
}

Result MoveLegacyDirectory(const fs::path& source, const fs::path& target) {
  if (!fs::exists(source)) return {};
  if (auto result = UnlinkedTree(source); !result) {
    return result;
  }
  if (fs::exists(target)) {
    if (auto result = UnlinkedTree(target); !result) {
      return result;
    }
  }
  fs::create_directories(target);
  for (const auto& entry : fs::directory_iterator(source)) {
    const auto destination = target / entry.path().filename();
    if (entry.is_directory()) {
      if (auto result = MoveLegacyDirectory(entry.path(), destination);
          !result) {
        return result;
      }
    } else {
      if (fs::exists(destination)) {
        const auto old_hash = Sha256File(entry.path());
        const auto new_hash = Sha256File(destination);
        if (!old_hash || !new_hash || *old_hash != *new_hash) {
          return std::unexpected("Migration will not overwrite " +
                                 PathText(destination));
        }
        fs::remove(entry.path());
      } else if (!MoveFileExW(entry.path().c_str(), destination.c_str(),
                              MOVEFILE_COPY_ALLOWED | MOVEFILE_WRITE_THROUGH)) {
        return std::unexpected(
            "Cannot move " + PathText(entry.path()) + ": " +
            std::system_category().message(static_cast<int>(GetLastError())));
      }
    }
  }
  fs::remove(source);
  return {};
}

bool GeneratedAssetDirectory(std::string_view name, std::string_view prefix) {
  if (!name.starts_with(prefix)) return false;
  const auto suffix = name.substr(prefix.size());
  return !suffix.empty() &&
         suffix.find_first_not_of("0123456789") == std::string_view::npos;
}

fs::path LocalSavePath(const fs::path& state, const fs::path& saves) {
  return saves.empty() ? state / "saves"
                       : (saves.is_absolute() ? saves : state / saves);
}
}  // namespace

Result CleanAssetLeftovers(const fs::path& state) {
  if (!fs::exists(state)) return {};
  for (const auto& entry : fs::directory_iterator(state)) {
    const auto name = PathText(entry.path().filename());
    const bool previous = GeneratedAssetDirectory(name, "assets-previous-");
    if ((!previous && !GeneratedAssetDirectory(name, "assets-staging-")) ||
        (previous && !fs::is_directory(state / "assets"))) {
      continue;
    }
    if (auto result = UnlinkedTree(entry.path()); !result) {
      return result;
    }
    fs::remove_all(entry.path());
  }
  return {};
}

Result ActivateAssets(const fs::path& staging, const fs::path& data,
                      const fs::path& previous) {
  const bool replacing = fs::exists(data);
  if (replacing) fs::rename(data, previous);
  std::error_code error;
  fs::rename(staging, data, error);
  if (!error) return CleanAssetLeftovers(data.parent_path());
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
  return std::unexpected("Cannot activate prepared assets: " + error.message());
}

std::expected<fs::path, std::string> ValidateCue(const fs::path& cue) {
  if (Upper(PathText(cue.extension())) != ".CUE") {
    return std::unexpected("Select a .cue, .chd or .bin file.");
  }
  if (auto result = RegularFile(cue); !result) {
    return std::unexpected(result.error());
  }
  constexpr std::size_t kMaximumCueBytes = std::size_t{64} * 1024;
  if (fs::file_size(cue) > kMaximumCueBytes) {
    return std::unexpected(
        "The selected CUE is too large to be a supported descriptor.");
  }
  std::ifstream input(cue, std::ios::binary);
  std::string line;
  CueSheet sheet;
  while (std::getline(input, line)) {
    if (auto result = sheet.Read(cue, line); !result) {
      return std::unexpected(result.error());
    }
  }
  if (!input.eof()) return std::unexpected("Cannot read the CUE file.");
  if (sheet.image.empty()) {
    return std::unexpected("The CUE has no FILE entry for its BIN.");
  }
  if (sheet.tracks != 1) {
    return std::unexpected("The CUE has no supported TRACK entry.");
  }
  if (sheet.indices != 1) {
    return std::unexpected("The CUE has no supported INDEX entry.");
  }
  return fs::absolute(sheet.image).lexically_normal();
}

fs::path FindFfmpeg() {
  std::vector<wchar_t> buffer(kLongestPathCharacters);
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
    if (!PathText(package.path().filename()).starts_with("Gyan.FFmpeg_")) {
      continue;
    }
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
  if (!fs::exists(path)) return Settings{{}, state / "saves", {}};
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
  return Settings{
      Utf8Path(value->at("cue").get<std::string>()),
      LocalSavePath(state, Utf8Path(value->at("saves").get<std::string>())),
      Utf8Path(value->at("ffmpeg").get<std::string>())};
}

Result SaveSettings(const fs::path& state, const Settings& settings) {
  fs::create_directories(state);
  const auto saves =
      settings.saves == state / "saves" ? fs::path("saves") : settings.saves;
  return WriteJson(state / "launcher.json",
                   {{"version", 1},
                    {"cue", PathText(settings.cue)},
                    {"saves", PathText(saves)},
                    {"ffmpeg", PathText(settings.ffmpeg)}});
}

std::expected<bool, std::string> MigrateLegacyState(const fs::path& legacy,
                                                    const fs::path& state) {
  if (!fs::exists(legacy) || fs::exists(state / "launcher.json")) return false;
  if (auto result = UnlinkedTree(legacy); !result) {
    return std::unexpected(result.error());
  }
  if (fs::exists(state)) {
    if (auto result = UnlinkedTree(state); !result) {
      return std::unexpected(result.error());
    }
  }
  auto settings = LoadSettings(legacy);
  if (!settings) return std::unexpected(settings.error());
  const auto relative = settings->saves.lexically_relative(legacy);
  if (!relative.empty() && !relative.is_absolute() &&
      std::ranges::find(relative, fs::path("..")) == relative.end()) {
    if (relative == ".") {
      return std::unexpected("Legacy saves need a dedicated subdirectory.");
    }
    settings->saves = state / relative;
    if (auto result = MoveLegacyDirectory(legacy / relative, settings->saves);
        !result) {
      return std::unexpected(result.error());
    }
  }
  for (const auto* folder : {"saves", "assets", "combat-lab-saves"}) {
    const auto target = std::string_view(folder) == "combat-lab-saves"
                            ? state / "combat-lab" / "saves"
                            : state / folder;
    if (auto result = MoveLegacyDirectory(legacy / folder, target); !result) {
      return std::unexpected(result.error());
    }
  }
  // Only generated leftovers are discarded; unknown user files stay untouched.
  for (const auto& entry : fs::directory_iterator(legacy)) {
    const auto name = PathText(entry.path().filename());
    if (GeneratedAssetDirectory(name, "assets-previous-") ||
        GeneratedAssetDirectory(name, "assets-staging-") ||
        name == "setup.log" || name == "ffmpeg-encoders.txt") {
      fs::remove_all(entry.path());
    }
  }
  if (auto result = SaveSettings(state, *settings); !result) {
    return std::unexpected(result.error());
  }
  fs::remove(legacy / "launcher.json");
  if (fs::is_empty(legacy)) {
    fs::remove(legacy);
  } else {
    return std::unexpected(
        "Known data migrated; unrecognized files retained in " +
        PathText(legacy));
  }
  return true;
}

Result CheckInstallation(const fs::path& installation) {
  for (const auto* file : {"VS.Desktop.exe", "VS.Extract.exe", "SDL3.dll",
                           "dav1d.dll", "sfizz.dll", "webm.dll", "chdr.dll"}) {
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
      ready->at("version") != kAssetPipelineVersion ||
      !ready->contains("source") ||
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
    progress({SetupStage::kReady, "Assets ready. No extraction needed.",
              kPercentComplete});
    return data;
  }
  progress(
      {SetupStage::kCache,
       "Preparing verified game assets. The game will start when complete."});
  if (auto result = ProbeFfmpeg(settings.ffmpeg, state / "setup.log");
      !result) {
    return std::unexpected(result.error());
  }
  if (const auto encoder = Content::FindAv1Encoder(
          settings.ffmpeg, state / "ffmpeg-encoders.txt");
      !encoder) {
    return std::unexpected(encoder.error());
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
            "Preparing disc files, models, sound banks, intro and streamed "
            "audio..."});
  if (auto result = run({PathText(installation / "VS.Extract.exe"), "extract",
                         PathText(verified->image), PathText(staging),
                         "--ffmpeg", PathText(settings.ffmpeg)});
      !result) {
    return std::unexpected(result.error());
  }
  progress({SetupStage::kOutputs, "Verifying all prepared assets..."});
  if (auto result = SealAssets(staging, *verified, **disc); !result) {
    return std::unexpected(result.error() +
                           "\nIncomplete assets: " + PathText(staging));
  }
  if (auto result =
          ActivateAssets(staging, data, state / ("assets-previous-" + suffix));
      !result) {
    return std::unexpected(result.error());
  }
  progress({SetupStage::kReady, "Assets ready.", kPercentComplete});
  return data;
}

std::vector<std::string> GameArguments(const fs::path& installation,
                                       const fs::path& data,
                                       const Settings& settings) {
  return {PathText(installation / "VS.Desktop.exe"), "--data", PathText(data),
          "--save-dir", PathText(settings.saves)};
}
}  // namespace VS::Launcher
