#include "Launcher/Updates.hpp"

#include <Windows.h>
#include <winhttp.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cstdint>
#include <fstream>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "Platform/App.hpp"
#include "Platform/Paths.hpp"
#include "nlohmann/json.hpp"

namespace VS::Launcher {
namespace {
namespace fs = std::filesystem;
using Platform::PathText;
using Platform::Utf8Path;
constexpr auto kPages = "https://github.com/JEschete/VS-NativePort/releases/";
constexpr auto kDigestPrefix = std::string_view("sha256:");
constexpr auto kManifest = "release.json";
constexpr auto kLauncher = "VS.Launcher.exe";
constexpr std::size_t kMaximumResponseBytes = std::size_t{4} * 1024 * 1024;
constexpr std::size_t kDownloadChunkBytes = std::size_t{64} * 1024;
constexpr std::uint64_t kMaximumPackageBytes = 256ULL * 1024 * 1024;
constexpr std::size_t kSha256Digits = 64;
constexpr int kTimeoutMilliseconds = 5000;
constexpr int kPackageTimeoutMilliseconds = 30000;
constexpr int kMoveAttempts = 5;
constexpr DWORD kMoveRetryMilliseconds = 200;
constexpr int kPercent = 100;
constexpr DWORD kStatusOk = 200;
constexpr DWORD kStatusForbidden = 403;
constexpr DWORD kStatusNotFound = 404;
constexpr DWORD kStatusTooManyRequests = 429;

struct CloseHttp {
  void operator()(void* handle) const { WinHttpCloseHandle(handle); }
};
using HttpHandle = std::unique_ptr<void, CloseHttp>;
struct HttpExchange {
  HttpHandle session;
  HttpHandle connection;
  HttpHandle request;
  DWORD status = 0;
};

struct ManifestFile {
  fs::path path;
  std::string sha256;
};
struct Manifest {
  std::string version;
  std::vector<ManifestFile> files;
};

std::string Upper(std::string text) {
  std::ranges::transform(text, text.begin(), [](unsigned char value) {
    return static_cast<char>(std::toupper(value));
  });
  return text;
}

std::string TextField(const nlohmann::json& object, const char* key) {
  if (!object.contains(key) || !object.at(key).is_string()) return {};
  return object.at(key).get<std::string>();
}

std::optional<std::vector<std::uint64_t>> VersionParts(std::string_view text) {
  if (text.starts_with('v')) text.remove_prefix(1);
  std::vector<std::uint64_t> parts;
  const char* cursor = text.data();
  const char* const end = cursor + text.size();
  while (true) {
    std::uint64_t part = 0;
    const auto [next, error] = std::from_chars(cursor, end, part);
    if (error != std::errc{}) return std::nullopt;
    parts.push_back(part);
    if (next == end) return parts;
    if (*next != '.') return std::nullopt;
    cursor = next + 1;
  }
}

std::string NetworkError() {
  return "Cannot reach GitHub (Windows error " +
         std::to_string(GetLastError()) + "). Check your connection.";
}

std::expected<HttpExchange, std::string> SendGet(const wchar_t* host,
                                                 const std::wstring& path,
                                                 const wchar_t* headers,
                                                 int receive_timeout) {
  HttpExchange exchange;
  exchange.session.reset(WinHttpOpen(
      L"VS-NativePort-Launcher", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
      WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
  if (!exchange.session ||
      !WinHttpSetTimeouts(exchange.session.get(), kTimeoutMilliseconds,
                          kTimeoutMilliseconds, kTimeoutMilliseconds,
                          receive_timeout)) {
    return std::unexpected(NetworkError());
  }
  exchange.connection.reset(WinHttpConnect(exchange.session.get(), host,
                                           INTERNET_DEFAULT_HTTPS_PORT, 0));
  if (!exchange.connection) return std::unexpected(NetworkError());
  exchange.request.reset(WinHttpOpenRequest(
      exchange.connection.get(), L"GET", path.c_str(), nullptr,
      WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE));
  if (!exchange.request ||
      !WinHttpSendRequest(exchange.request.get(), headers,
                          headers ? static_cast<DWORD>(-1) : 0,
                          WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
      !WinHttpReceiveResponse(exchange.request.get(), nullptr)) {
    return std::unexpected(NetworkError());
  }
  DWORD size = sizeof exchange.status;
  if (!WinHttpQueryHeaders(
          exchange.request.get(),
          WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
          WINHTTP_HEADER_NAME_BY_INDEX, &exchange.status, &size,
          WINHTTP_NO_HEADER_INDEX)) {
    return std::unexpected(NetworkError());
  }
  return exchange;
}

std::expected<std::string, std::string> ReadResponse(void* request) {
  std::string body;
  while (true) {
    DWORD available = 0;
    if (!WinHttpQueryDataAvailable(request, &available)) {
      return std::unexpected(NetworkError());
    }
    if (available == 0) return body;
    if (body.size() + available > kMaximumResponseBytes) {
      return std::unexpected("GitHub's response was unexpectedly large.");
    }
    const auto offset = body.size();
    body.resize(offset + available);
    DWORD received = 0;
    if (!WinHttpReadData(request, body.data() + offset, available, &received)) {
      return std::unexpected(NetworkError());
    }
    body.resize(offset + received);
  }
}

std::expected<std::string, std::string> RequestLatestRelease() {
  auto exchange = SendGet(
      L"api.github.com", L"/repos/JEschete/VS-NativePort/releases/latest",
      L"Accept: application/vnd.github+json\r\n", kTimeoutMilliseconds);
  if (!exchange) return std::unexpected(exchange.error());
  switch (exchange->status) {
    case kStatusOk:
      return ReadResponse(exchange->request.get());
    case kStatusNotFound:
      return std::unexpected("GitHub lists no published release yet.");
    case kStatusForbidden:
    case kStatusTooManyRequests:
      return std::unexpected("GitHub is limiting requests. Try again later.");
    default:
      return std::unexpected("GitHub returned status " +
                             std::to_string(exchange->status) + ".");
  }
}

// Only this exact package, from this project, is ever installed automatically.
void SelectPackage(const nlohmann::json& release_json, LatestRelease& release) {
  if (!release_json.contains("assets") ||
      !release_json.at("assets").is_array()) {
    return;
  }
  const auto name = "VS-NativePort-" + release.version + "-win-x64.zip";
  const auto url =
      std::string(kPages) + "download/" + release.version + "/" + name;
  for (const auto& asset : release_json.at("assets")) {
    if (!asset.is_object() || TextField(asset, "name") != name ||
        TextField(asset, "state") != "uploaded" ||
        TextField(asset, "browser_download_url") != url ||
        !asset.contains("size") || !asset.at("size").is_number_unsigned()) {
      continue;
    }
    const auto bytes = asset.at("size").get<std::uint64_t>();
    const auto digest = TextField(asset, "digest");
    if (bytes == 0 || bytes > kMaximumPackageBytes ||
        !digest.starts_with(kDigestPrefix) ||
        digest.size() != kDigestPrefix.size() + kSha256Digits ||
        !std::all_of(
            digest.begin() + kDigestPrefix.size(), digest.end(),
            [](unsigned char value) { return std::isxdigit(value); })) {
      continue;
    }
    release.asset_url = url;
    release.asset_bytes = bytes;
    release.asset_sha256 = Upper(digest.substr(kDigestPrefix.size()));
    return;
  }
}

bool SafeReleasePath(const fs::path& path) {
  if (path.empty() || path.has_root_path() ||
      PathText(path).find(':') != std::string::npos) {
    return false;
  }
  if (PathText(*path.begin()).starts_with("update")) return false;
  return std::ranges::none_of(
      path, [](const fs::path& part) { return part == "." || part == ".."; });
}

std::expected<Manifest, std::string> ReadManifest(const fs::path& directory) {
  std::ifstream input(directory / kManifest, std::ios::binary);
  if (!input) return std::unexpected("The release has no release.json.");
  const auto value = nlohmann::json::parse(input, nullptr, false);
  if (!value.is_object() || !value.contains("files") ||
      !value.at("files").is_array() || TextField(value, "version").empty()) {
    return std::unexpected("The release's release.json is damaged.");
  }
  Manifest manifest;
  manifest.version = TextField(value, "version");
  for (const auto& entry : value.at("files")) {
    if (!entry.is_object()) {
      return std::unexpected("The release's release.json is damaged.");
    }
    auto path = Utf8Path(TextField(entry, "path")).make_preferred();
    auto sha256 = TextField(entry, "sha256");
    if (!SafeReleasePath(path) || sha256.empty()) {
      return std::unexpected("The release lists an unsafe file: " +
                             TextField(entry, "path"));
    }
    manifest.files.push_back({std::move(path), Upper(std::move(sha256))});
  }
  return manifest;
}

// Renaming needs no copy, and works on running programs and loaded libraries.
Result MoveFile(const fs::path& source, const fs::path& target) {
  std::error_code error;
  fs::create_directories(target.parent_path(), error);
  for (int attempt = 0; attempt < kMoveAttempts; ++attempt) {
    if (MoveFileExW(source.c_str(), target.c_str(), MOVEFILE_WRITE_THROUGH)) {
      return {};
    }
    if (attempt + 1 < kMoveAttempts) Sleep(kMoveRetryMilliseconds);
  }
  return std::unexpected("Cannot move " + PathText(source) +
                         " (Windows error " + std::to_string(GetLastError()) +
                         ").");
}

fs::path MarkerPath(const fs::path& installation) {
  return installation / kUpdateBackupDirectory / kUpdateMarker;
}

// Safe to rerun: a file is only touched while its backup still exists.
Result RollBack(const fs::path& installation) {
  const auto marker = MarkerPath(installation);
  std::ifstream input(marker, std::ios::binary);
  const auto value = nlohmann::json::parse(input, nullptr, false);
  input.close();
  if (!value.is_object() || !value.contains("files") ||
      !value.at("files").is_array() || !value.contains("added") ||
      !value.at("added").is_array()) {
    return std::unexpected("The interrupted update's record is damaged.");
  }
  const auto backup = installation / kUpdateBackupDirectory;
  const auto discard = installation / kUpdateDiscardDirectory;
  std::set<fs::path> added;
  for (const auto& entry : value.at("added")) {
    added.insert(Utf8Path(entry.get<std::string>()));
  }
  std::size_t discarded = 0;
  const auto set_aside = [&](const fs::path& target) -> Result {
    return MoveFile(target, discard / (std::to_string(GetTickCount64()) + "-" +
                                       std::to_string(++discarded)));
  };
  const auto& files = value.at("files");
  for (auto entry = files.rbegin(); entry != files.rend(); ++entry) {
    const auto path = Utf8Path(entry->get<std::string>());
    if (!SafeReleasePath(path) && path != kManifest) {
      return std::unexpected("The interrupted update's record is damaged.");
    }
    const auto target = installation / path;
    const bool has_backup = fs::exists(backup / path);
    if (has_backup || added.contains(path)) {
      if (fs::exists(target)) {
        if (auto moved = set_aside(target); !moved) {
          return moved;
        }
      }
    }
    if (has_backup) {
      if (auto moved = MoveFile(backup / path, target); !moved) {
        return moved;
      }
    }
  }
  std::error_code error;
  if (!fs::remove(marker, error) || error) {
    return std::unexpected("Cannot finish restoring the previous version.");
  }
  return {};
}

Result WriteMarker(const fs::path& installation,
                   const std::vector<fs::path>& files,
                   const std::vector<fs::path>& added) {
  auto listed = nlohmann::json::array();
  for (const auto& path : files) listed.push_back(PathText(path));
  auto new_files = nlohmann::json::array();
  for (const auto& path : added) new_files.push_back(PathText(path));
  const auto marker = MarkerPath(installation);
  auto temporary = marker;
  temporary += L".tmp";
  {
    std::ofstream output(temporary, std::ios::binary);
    output << nlohmann::json{{"files", listed}, {"added", new_files}}.dump();
    if (!output.flush()) {
      return std::unexpected("Cannot write to the game folder.");
    }
  }
  return MoveFile(temporary, marker);
}

Result Swap(const fs::path& stage, const fs::path& installation,
            const std::vector<fs::path>& files,
            const std::set<fs::path>& incoming) {
  const auto backup = installation / kUpdateBackupDirectory;
  for (const auto& path : files) {
    const auto target = installation / path;
    if (fs::exists(target)) {
      if (auto moved = MoveFile(target, backup / path); !moved) {
        return moved;
      }
    }
    if (incoming.contains(path)) {
      if (auto moved = MoveFile(stage / path, target); !moved) {
        return moved;
      }
    }
  }
  std::error_code error;
  if (!fs::remove(MarkerPath(installation), error) || error) {
    return std::unexpected("Cannot finish the update.");
  }
  return {};
}

Result DownloadPackage(const LatestRelease& release, const fs::path& file,
                       const UpdateProgress& progress) {
  constexpr auto kHost = std::string_view("https://github.com");
  const auto url = std::string_view(release.asset_url).substr(kHost.size());
  auto exchange = SendGet(L"github.com", std::wstring(url.begin(), url.end()),
                          nullptr, kPackageTimeoutMilliseconds);
  if (!exchange) return std::unexpected(exchange.error());
  if (exchange->status != kStatusOk) {
    return std::unexpected("GitHub returned status " +
                           std::to_string(exchange->status) +
                           " for the download.");
  }
  std::ofstream output(file, std::ios::binary);
  if (!output) {
    return std::unexpected(
        "Cannot write to the game folder. Move the game to a folder you can "
        "write to, or download the update from the release page.");
  }
  std::vector<char> buffer(kDownloadChunkBytes);
  std::uint64_t total = 0;
  while (true) {
    DWORD received = 0;
    if (!WinHttpReadData(exchange->request.get(), buffer.data(),
                         static_cast<DWORD>(buffer.size()), &received)) {
      return std::unexpected(NetworkError());
    }
    if (received == 0) break;
    total += received;
    if (total > release.asset_bytes) {
      return std::unexpected("The download is larger than GitHub reported.");
    }
    output.write(buffer.data(), received);
    if (!output) return std::unexpected("Cannot save the download.");
    if (progress) {
      progress("Downloading version " + release.version + "...",
               static_cast<int>(total * kPercent / release.asset_bytes));
    }
  }
  if (total != release.asset_bytes) {
    return std::unexpected("The download ended early. Try again.");
  }
  return {};
}

Result PrepareStagedRelease(const LatestRelease& release,
                            const fs::path& installation,
                            const UpdateProgress& progress) {
  const auto archive = installation / kUpdateArchive;
  const auto stage = installation / kUpdateStagingDirectory;
  if (auto result = DownloadReleasePackage(release, archive, progress);
      !result) {
    return result;
  }
  if (progress) progress("Unpacking the update...", -1);
  if (auto result = ExtractRelease(archive, stage); !result) {
    return result;
  }
  if (progress) progress("Checking every file in the update...", -1);
  if (auto result = VerifyStagedRelease(stage, release.version); !result) {
    return result;
  }
  const std::vector<std::string> self_check{PathText(stage / kLauncher),
                                            "--self-check"};
  const auto started = Platform::RunProcess(self_check);
  if (!started) return std::unexpected(started.error());
  if (*started != 0) {
    return std::unexpected("The new launcher did not start correctly.");
  }
  return {};
}
}  // namespace

std::string InstalledVersion(const fs::path& installation) {
  std::ifstream input(installation / kManifest, std::ios::binary);
  if (!input) return {};
  const auto manifest = nlohmann::json::parse(input, nullptr, false);
  return manifest.is_object() ? TextField(manifest, "version") : std::string{};
}

std::expected<LatestRelease, std::string> ParseLatestRelease(
    std::string_view json) {
  const auto value = nlohmann::json::parse(json, nullptr, false);
  if (!value.is_object() || !value.contains("tag_name") ||
      !value.at("tag_name").is_string() || !value.contains("html_url") ||
      !value.at("html_url").is_string()) {
    return std::unexpected("GitHub's response was not a release.");
  }
  LatestRelease release;
  release.version = value.at("tag_name").get<std::string>();
  release.url = value.at("html_url").get<std::string>();
  if (!VersionParts(release.version)) {
    return std::unexpected("GitHub's latest release has an unknown version: " +
                           release.version);
  }
  // The launcher opens this address, so it must be this project's own page.
  if (!release.url.starts_with(kPages)) {
    return std::unexpected("GitHub's response has an unexpected release page.");
  }
  SelectPackage(value, release);
  return release;
}

UpdateStatus CompareRelease(std::string_view installed,
                            std::string_view latest) {
  const auto current = VersionParts(installed);
  const auto available = VersionParts(latest);
  if (!current || !available) return UpdateStatus::kUnknownInstalled;
  const auto count = std::max(current->size(), available->size());
  for (std::size_t index = 0; index < count; ++index) {
    const auto mine = index < current->size() ? (*current)[index] : 0;
    const auto theirs = index < available->size() ? (*available)[index] : 0;
    if (mine != theirs) {
      return mine < theirs ? UpdateStatus::kAvailable : UpdateStatus::kCurrent;
    }
  }
  return UpdateStatus::kCurrent;
}

std::expected<LatestRelease, std::string> FetchLatestRelease() {
  const auto body = RequestLatestRelease();
  if (!body) return std::unexpected(body.error());
  return ParseLatestRelease(*body);
}

Result DownloadReleasePackage(const LatestRelease& release,
                              const fs::path& file,
                              const UpdateProgress& progress) {
  if (auto result = DownloadPackage(release, file, progress); !result) {
    return result;
  }
  if (progress) progress("Checking the download...", -1);
  const auto sha256 = Sha256File(file);
  if (!sha256) return std::unexpected(sha256.error());
  if (Upper(*sha256) != release.asset_sha256) {
    return std::unexpected("The download does not match GitHub's checksum.");
  }
  return {};
}

Result ExtractRelease(const fs::path& archive, const fs::path& stage) {
  std::array<wchar_t, MAX_PATH> system{};
  if (GetSystemDirectoryW(system.data(), MAX_PATH) == 0) {
    return std::unexpected("Cannot find the Windows system folder.");
  }
  std::error_code error;
  fs::create_directories(stage, error);
  if (error) return std::unexpected("Cannot write to the game folder.");
  // System tar refuses entries that would escape the destination.
  const std::vector<std::string> arguments{
      PathText(fs::path(system.data()) / "tar.exe"),
      "-x",
      "-f",
      PathText(archive),
      "-C",
      PathText(stage)};
  const auto result = Platform::RunProcess(arguments);
  if (!result) return std::unexpected(result.error());
  if (*result != 0) return std::unexpected("Cannot unpack the update.");
  return {};
}

Result VerifyStagedRelease(const fs::path& stage, std::string_view version) {
  const auto manifest = ReadManifest(stage);
  if (!manifest) return std::unexpected(manifest.error());
  if (manifest->version != version) {
    return std::unexpected("The update reports the wrong version: " +
                           manifest->version);
  }
  std::set<fs::path> listed;
  for (const auto& file : manifest->files) {
    const auto sha256 = Sha256File(stage / file.path);
    if (!sha256) return std::unexpected(sha256.error());
    if (Upper(*sha256) != file.sha256) {
      return std::unexpected("A file in the update is damaged: " +
                             PathText(file.path));
    }
    listed.insert(file.path);
  }
  constexpr std::array kRequired{kLauncher,  "VS.Desktop.exe", "VS.Extract.exe",
                                 "SDL3.dll", "dav1d.dll",      "sfizz.dll",
                                 "webm.dll"};
  for (const auto* required : kRequired) {
    if (!listed.contains(required)) {
      return std::unexpected(std::string("The update is missing ") + required);
    }
  }
  std::error_code error;
  for (const auto& entry : fs::recursive_directory_iterator(stage, error)) {
    if (entry.is_directory()) continue;
    const auto path = entry.path().lexically_relative(stage);
    if (path != kManifest && !listed.contains(path)) {
      return std::unexpected("The update has an unexpected file: " +
                             PathText(path));
    }
  }
  if (error) return std::unexpected("Cannot read the unpacked update.");
  return {};
}

Result ApplyStagedRelease(const fs::path& stage, const fs::path& installation) {
  const auto incoming = ReadManifest(stage);
  if (!incoming) return std::unexpected(incoming.error());
  const auto current = ReadManifest(installation);
  if (!current) {
    return std::unexpected(
        "This copy is not a packaged release, so it was not replaced.");
  }
  if (fs::exists(MarkerPath(installation))) {
    return std::unexpected("An earlier update has not been restored yet.");
  }
  std::set<fs::path> ordinary;
  std::set<fs::path> arriving{fs::path(kManifest)};
  for (const auto& file : current->files) ordinary.insert(file.path);
  for (const auto& file : incoming->files) {
    ordinary.insert(file.path);
    arriving.insert(file.path);
  }
  // The launcher and version record go last, keeping the unsafe window small.
  ordinary.erase(kLauncher);
  std::vector<fs::path> files(ordinary.begin(), ordinary.end());
  files.emplace_back(kLauncher);
  files.emplace_back(kManifest);
  std::vector<fs::path> added;
  for (const auto& path : files) {
    if (!fs::exists(installation / path)) added.push_back(path);
  }
  std::error_code error;
  fs::remove_all(installation / kUpdateBackupDirectory, error);
  fs::create_directories(installation / kUpdateBackupDirectory, error);
  if (error) return std::unexpected("Cannot write to the game folder.");
  if (auto marked = WriteMarker(installation, files, added); !marked) {
    return marked;
  }
  auto swapped = Swap(stage, installation, files, arriving);
  if (swapped) return {};
  if (auto restored = RollBack(installation); !restored) {
    return std::unexpected(
        swapped.error() +
        "\nThe previous version could not be fully restored yet. Reopen the "
        "launcher to finish restoring it. " +
        restored.error());
  }
  return std::unexpected(swapped.error() +
                         "\nThe previous version has been restored.");
}

Result RecoverInterruptedUpdate(const fs::path& installation) {
  if (!fs::exists(MarkerPath(installation))) return {};
  return RollBack(installation);
}

void CleanUpdateLeftovers(const fs::path& installation) {
  if (fs::exists(MarkerPath(installation))) return;
  std::error_code error;
  fs::remove_all(installation / kUpdateStagingDirectory, error);
  fs::remove_all(installation / kUpdateBackupDirectory, error);
  fs::remove_all(installation / kUpdateDiscardDirectory, error);
  fs::remove(installation / kUpdateArchive, error);
}

Result InstallRelease(const LatestRelease& release,
                      const fs::path& installation,
                      const UpdateProgress& progress) {
  if (release.asset_url.empty()) {
    return std::unexpected("This release has no package to install.");
  }
  CleanUpdateLeftovers(installation);
  if (auto prepared = PrepareStagedRelease(release, installation, progress);
      !prepared) {
    CleanUpdateLeftovers(installation);
    return std::unexpected(prepared.error() +
                           "\nYour current version was not changed.");
  }
  if (progress) progress("Installing version " + release.version + "...", -1);
  auto applied =
      ApplyStagedRelease(installation / kUpdateStagingDirectory, installation);
  std::error_code error;
  fs::remove(installation / kUpdateArchive, error);
  return applied;
}

}  // namespace VS::Launcher
