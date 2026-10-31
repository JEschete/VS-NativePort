#include <Windows.h>
#include <objbase.h>
#include <shellapi.h>

#include <algorithm>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "Launcher/Setup.hpp"
#include "Launcher/Updates.hpp"
#include "Launcher/Window.hpp"
#include "Platform/App.hpp"
#include "Platform/Paths.hpp"

namespace {
namespace fs = std::filesystem;
namespace Launcher = VS::Launcher;
bool g_console_mode = false;
void Report(const std::string& message) {
  if (g_console_mode) {
    std::fprintf(stderr, "launcher: %s\n", message.c_str());
  } else {
    const auto text = VS::Platform::Utf8Path(message).wstring();
    MessageBoxW(nullptr, text.c_str(), L"Vagrant Story Native",
                MB_OK | MB_ICONERROR);
  }
}

// The updating launcher holds the single-instance lock and its old files
// until it exits, moments after starting this one.
void WaitForPreviousLauncher(DWORD process_id) {
  constexpr DWORD kExitWaitMilliseconds = 30000;
  const std::unique_ptr<void, decltype(&CloseHandle)> process(
      OpenProcess(SYNCHRONIZE, FALSE, process_id), CloseHandle);
  if (process) WaitForSingleObject(process.get(), kExitWaitMilliseconds);
}

std::vector<std::wstring> ReadArguments() {
  int count = 0;
  const auto raw = CommandLineToArgvW(GetCommandLineW(), &count);
  if (!raw) throw std::runtime_error("Cannot read command-line arguments.");
  std::vector<std::wstring> arguments(raw, raw + count);
  LocalFree(static_cast<HLOCAL>(raw));
  return arguments;
}

struct Options {
  bool prepare_only = false;
  bool verify_only = false;
  fs::path state;
  Launcher::Settings overrides;
};

void SetOption(Options& options, const std::wstring& option,
               const fs::path& value) {
  if (value.empty()) {
    throw std::runtime_error("Launcher option paths must not be empty.");
  }
  if (option == L"--finish-update") {
    WaitForPreviousLauncher(std::stoul(value.wstring()));
  } else if (option == L"--state-dir") {
    options.state = fs::absolute(value);
  } else if (option == L"--cue") {
    options.overrides.cue = fs::absolute(value);
  } else if (option == L"--save-dir") {
    options.overrides.saves = fs::absolute(value);
  } else if (option == L"--ffmpeg") {
    options.overrides.ffmpeg = fs::absolute(value);
  } else {
    throw std::runtime_error("Unknown launcher option.");
  }
}

Options ParseOptions(const std::vector<std::wstring>& arguments) {
  Options options;
  for (std::size_t index = 1; index < arguments.size(); ++index) {
    const auto& option = arguments[index];
    if (option == L"--prepare-only") {
      options.prepare_only = true;
      continue;
    }
    if (option == L"--verify-only") {
      options.verify_only = true;
      continue;
    }
    if (index + 1 == arguments.size() ||
        arguments[index + 1].starts_with(L"--")) {
      throw std::runtime_error("Missing launcher option value.");
    }
    SetOption(options, option, fs::path(arguments[++index]));
  }
  if (options.prepare_only && options.verify_only) {
    throw std::runtime_error("Choose either --prepare-only or --verify-only.");
  }
  return options;
}

// Empty when the owner declines to reset unreadable settings.
std::optional<Launcher::Settings> LoadOrResetSettings(const fs::path& state) {
  auto loaded = Launcher::LoadSettings(state);
  if (loaded) return *loaded;
  if (g_console_mode) throw std::runtime_error(loaded.error());
  const auto question =
      VS::Platform::Utf8Path(loaded.error() +
                             "\nReset launcher settings? Your assets and "
                             "saves will not be deleted.")
          .wstring();
  if (MessageBoxW(nullptr, question.c_str(), L"Launcher settings",
                  MB_YESNO | MB_ICONWARNING) != IDYES) {
    return std::nullopt;
  }
  fs::rename(state / "launcher.json",
             state / ("launcher-invalid-" + std::to_string(GetTickCount64()) +
                      ".json"));
  loaded = Launcher::LoadSettings(state);
  if (!loaded) throw std::runtime_error(loaded.error());
  return *loaded;
}

int RunConsole(const Options& options, const Launcher::Settings& settings,
               const fs::path& installation) {
  if (options.verify_only) {
    const auto result = Launcher::VerifyDisc(settings.cue);
    if (!result) throw std::runtime_error(result.error());
    std::printf("Verified USA SLUS-01040\nSHA-256: %s\n",
                result->sha256.c_str());
    return 0;
  }
  const auto result =
      Launcher::Prepare(settings, installation, options.state,
                        [](const Launcher::ProgressUpdate& update) {
                          std::printf("%s\n", update.message.c_str());
                        });
  if (!result) throw std::runtime_error(result.error());
  return 0;
}
}  // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int show) try {
  if (GetACP() != CP_UTF8) {
    throw std::runtime_error("Windows 10 version 1903 or later is required.");
  }
  const HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  if (FAILED(initialized)) {
    throw std::runtime_error("Cannot initialize Windows dialogs.");
  }
  struct ComScope {
    ~ComScope() { CoUninitialize(); }
  } com;
  const auto root = VS::Platform::UserDataDir();
  if (!root) throw std::runtime_error(root.error());
  const auto installation = VS::Platform::ExecutableDir();
  const auto arguments = ReadArguments();
  // Reaching this point proves an update's launcher and libraries can load.
  if (std::find(arguments.begin(), arguments.end(), L"--self-check") !=
      arguments.end()) {
    return 0;
  }
  g_console_mode = std::find(arguments.begin(), arguments.end(),
                             L"--prepare-only") != arguments.end() ||
                   std::find(arguments.begin(), arguments.end(),
                             L"--verify-only") != arguments.end();
  auto options = ParseOptions(arguments);
  if (options.state.empty()) options.state = *root;
  const std::unique_ptr<void, decltype(&CloseHandle)> single(
      CreateMutexW(nullptr, FALSE, L"Local\\VSNativePortLauncher"),
      CloseHandle);
  if (!single) throw std::runtime_error("Cannot initialize launcher lock.");
  if (GetLastError() == ERROR_ALREADY_EXISTS) {
    throw std::runtime_error("The launcher is already open.");
  }
  if (const auto recovered = Launcher::RecoverInterruptedUpdate(installation);
      recovered) {
    Launcher::CleanUpdateLeftovers(installation);
  } else {
    Report("An interrupted update could not be undone: " + recovered.error());
  }
  const auto loaded = LoadOrResetSettings(options.state);
  if (!loaded) return 1;
  auto settings = *loaded;
  const auto& overrides = options.overrides;
  if (!overrides.cue.empty()) settings.cue = overrides.cue;
  if (!overrides.saves.empty()) settings.saves = overrides.saves;
  if (!overrides.ffmpeg.empty()) settings.ffmpeg = overrides.ffmpeg;
  if (options.verify_only || options.prepare_only) {
    return RunConsole(options, settings, installation);
  }
  return Launcher::ShowLauncher(settings, installation, options.state, show);
} catch (const std::exception& error) {
  Report(error.what());
  return 1;
}
