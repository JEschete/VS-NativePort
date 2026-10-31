#include <Windows.h>
#include <objbase.h>
#include <shellapi.h>

#include <algorithm>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "Launcher/Setup.hpp"
#include "Launcher/Window.hpp"
#include "Platform/App.hpp"
#include "Platform/Paths.hpp"

namespace {
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
}  // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int show) try {
  namespace fs = std::filesystem;
  namespace Launcher = VS::Launcher;
  if (GetACP() != CP_UTF8) {
    throw std::runtime_error("Windows 10 version 1903 or later is required.");
  }
  const HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  if (FAILED(initialized))
    throw std::runtime_error("Cannot initialize Windows dialogs.");
  struct ComScope {
    ~ComScope() { CoUninitialize(); }
  } com;
  const auto root = VS::Platform::UserDataDir();
  if (!root) throw std::runtime_error(root.error());
  auto state = *root;
  const auto installation = VS::Platform::ExecutableDir();
  int count = 0;
  const auto raw = CommandLineToArgvW(GetCommandLineW(), &count);
  if (!raw) throw std::runtime_error("Cannot read command-line arguments.");
  const std::vector<std::wstring> arguments(raw, raw + count);
  LocalFree(raw);
  g_console_mode = std::find(arguments.begin(), arguments.end(),
                             L"--prepare-only") != arguments.end() ||
                   std::find(arguments.begin(), arguments.end(),
                             L"--verify-only") != arguments.end();
  bool prepare_only = false;
  bool verify_only = false;
  Launcher::Settings overrides;
  for (std::size_t index = 1; index < arguments.size(); ++index) {
    const auto& option = arguments[index];
    if (option == L"--prepare-only") {
      prepare_only = true;
      continue;
    }
    if (option == L"--verify-only") {
      verify_only = true;
      continue;
    }
    if (index + 1 == arguments.size() ||
        arguments[index + 1].starts_with(L"--")) {
      throw std::runtime_error("Missing launcher option value.");
    }
    const fs::path value(arguments[++index]);
    if (value.empty())
      throw std::runtime_error("Launcher option paths must not be empty.");
    if (option == L"--state-dir")
      state = fs::absolute(value);
    else if (option == L"--cue")
      overrides.cue = fs::absolute(value);
    else if (option == L"--save-dir")
      overrides.saves = fs::absolute(value);
    else if (option == L"--ffmpeg")
      overrides.ffmpeg = fs::absolute(value);
    else
      throw std::runtime_error("Unknown launcher option.");
  }
  if (prepare_only && verify_only) {
    throw std::runtime_error("Choose either --prepare-only or --verify-only.");
  }
  const std::unique_ptr<void, decltype(&CloseHandle)> single(
      CreateMutexW(nullptr, FALSE, L"Local\\VSNativePortLauncher"),
      CloseHandle);
  if (!single) throw std::runtime_error("Cannot initialize launcher lock.");
  if (GetLastError() == ERROR_ALREADY_EXISTS) {
    throw std::runtime_error("The launcher is already open.");
  }
  auto loaded = Launcher::LoadSettings(state);
  if (!loaded) {
    if (g_console_mode) throw std::runtime_error(loaded.error());
    const auto question =
        VS::Platform::Utf8Path(loaded.error() +
                               "\nReset launcher settings? Your assets and "
                               "saves will not be deleted.")
            .wstring();
    if (MessageBoxW(nullptr, question.c_str(), L"Launcher settings",
                    MB_YESNO | MB_ICONWARNING) != IDYES)
      return 1;
    fs::rename(state / "launcher.json",
               state / ("launcher-invalid-" + std::to_string(GetTickCount64()) +
                        ".json"));
    loaded = Launcher::LoadSettings(state);
    if (!loaded) throw std::runtime_error(loaded.error());
  }
  auto settings = *loaded;
  if (!overrides.cue.empty()) settings.cue = overrides.cue;
  if (!overrides.saves.empty()) settings.saves = overrides.saves;
  if (!overrides.ffmpeg.empty()) settings.ffmpeg = overrides.ffmpeg;
  if (verify_only) {
    const auto result = Launcher::VerifyDisc(settings.cue);
    if (!result) throw std::runtime_error(result.error());
    std::printf("Verified USA SLUS-01040\nSHA-256: %s\n",
                result->sha256.c_str());
    return 0;
  }
  if (prepare_only) {
    const auto result =
        Launcher::Prepare(settings, installation, state,
                          [](const Launcher::ProgressUpdate& update) {
                            std::printf("%s\n", update.message.c_str());
                          });
    if (!result) throw std::runtime_error(result.error());
    return 0;
  }
  return Launcher::ShowLauncher(settings, installation, state, show);
} catch (const std::exception& error) {
  Report(error.what());
  return 1;
}
