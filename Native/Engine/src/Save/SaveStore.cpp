#include "Save/SaveStore.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <atomic>
#include <cstdint>
#include <format>
#include <fstream>
#include <memory>
#include <string>
#include <system_error>
#include <utility>

#include "SDL3/SDL_log.h"

namespace VS::Save {
namespace {
namespace fs = std::filesystem;

std::string PathText(const fs::path& path) {
  const auto text = path.u8string();
  return {text.begin(), text.end()};
}

std::string FileError(std::string_view operation, const fs::path& path,
                      std::string_view detail) {
  return std::format("{} '{}': {}", operation, PathText(path), detail);
}

std::string WindowsError(std::string_view operation, const fs::path& path,
                         DWORD code) {
  return FileError(operation, path,
                   std::system_category().message(static_cast<int>(code)));
}

struct CloseFile {
  void operator()(HANDLE handle) const {
    if (handle != nullptr && !CloseHandle(handle)) {
      SDL_LogWarn(SDL_LOG_CATEGORY_SYSTEM, "Save handle close failed: %lu",
                  GetLastError());
    }
  }
};
using FileHandle = std::unique_ptr<void, CloseFile>;

fs::path BackupPath(fs::path path) {
  path += L".bak";
  return path;
}

std::expected<bool, std::string> Exists(const fs::path& path) {
  std::error_code error;
  const bool exists = fs::exists(path, error);
  if (error) {
    return std::unexpected(FileError("Inspect", path, error.message()));
  }
  if (exists && !fs::is_regular_file(path, error)) {
    return std::unexpected(FileError(
        "Inspect", path, error ? error.message() : "not a regular file"));
  }
  return exists;
}

std::expected<std::string, std::string> Read(const fs::path& path,
                                             Validator validate) {
  const auto exists = Exists(path);
  if (!exists) return std::unexpected(exists.error());
  if (!*exists) {
    return std::unexpected(FileError("Read", path, "file is missing"));
  }
  std::error_code error;
  const auto size = fs::file_size(path, error);
  if (error) return std::unexpected(FileError("Read", path, error.message()));
  if (size == 0 || size > SaveStore::kMaximumBytes) {
    return std::unexpected(FileError("Read", path, "invalid save byte count"));
  }
  std::ifstream input(path, std::ios::binary);
  if (!input) return std::unexpected(FileError("Read", path, "open failed"));
  std::string data(static_cast<std::size_t>(size), '\0');
  input.read(data.data(), static_cast<std::streamsize>(data.size()));
  if (!input || input.peek() != std::char_traits<char>::eof()) {
    return std::unexpected(
        FileError("Read", path, "incomplete or changed file"));
  }
  const auto valid = validate(data);
  if (!valid) {
    return std::unexpected(FileError("Validate", path, valid.error()));
  }
  return data;
}

std::expected<FileHandle, std::string> Lock(const fs::path& slot) {
  auto path = slot;
  path += L".lock";
  const HANDLE handle = CreateFileW(
      path.c_str(), GENERIC_READ | GENERIC_WRITE | DELETE, 0, nullptr,
      OPEN_ALWAYS, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE,
      nullptr);
  if (handle == INVALID_HANDLE_VALUE) {
    return std::unexpected(WindowsError("Lock", path, GetLastError()));
  }
  return FileHandle(handle);
}

std::string RemoveTemporary(const fs::path& path, std::string message) {
  std::error_code error;
  fs::remove(path, error);
  if (error) {
    message += "; " + FileError("Remove temporary", path, error.message());
  }
  return message;
}

std::expected<void, std::string> WriteBuffer(HANDLE handle,
                                             const fs::path& path,
                                             std::string_view data) {
  DWORD written = 0;
  if (!WriteFile(handle, data.data(), static_cast<DWORD>(data.size()), &written,
                 nullptr)) {
    return std::unexpected(WindowsError("Write", path, GetLastError()));
  }
  if (written != data.size()) {
    return std::unexpected(FileError("Write", path, "incomplete write"));
  }
  if (!FlushFileBuffers(handle)) {
    return std::unexpected(WindowsError("Flush", path, GetLastError()));
  }
  return {};
}

std::expected<void, std::string> WriteAtomic(const fs::path& path,
                                             std::string_view data) {
  static std::atomic<std::uint64_t> sequence{0};
  auto temporary = path;
  temporary += std::format(".{}.{}.{}.tmp", GetCurrentProcessId(),
                           GetTickCount64(), sequence.fetch_add(1));
  const HANDLE raw = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr,
                                 CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (raw == INVALID_HANDLE_VALUE) {
    return std::unexpected(
        WindowsError("Create temporary", temporary, GetLastError()));
  }
  FileHandle file(raw);
  auto result = WriteBuffer(file.get(), temporary, data);
  if (!CloseHandle(file.release())) {
    const auto error =
        WindowsError("Close temporary", temporary, GetLastError());
    result = std::unexpected(result ? error : result.error() + "; " + error);
  }
  if (!result) {
    return std::unexpected(RemoveTemporary(temporary, result.error()));
  }
  if (!MoveFileExW(temporary.c_str(), path.c_str(),
                   MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
    const auto error = WindowsError("Commit", path, GetLastError());
    return std::unexpected(RemoveTemporary(temporary, error));
  }
  return {};
}
}  // namespace

SaveStore::SaveStore(fs::path root) : m_root_(std::move(root)) {}

std::expected<SaveStore, std::string> SaveStore::Open(fs::path root) {
  if (root.empty()) return std::unexpected("Save directory is missing");
  std::error_code error;
  root = fs::absolute(root, error);
  if (error) {
    return std::unexpected(FileError("Resolve", root, error.message()));
  }
  fs::create_directories(root, error);
  if (error) return std::unexpected(FileError("Create", root, error.message()));
  if (!fs::is_directory(root, error)) {
    return std::unexpected(
        FileError("Open", root, error ? error.message() : "not a directory"));
  }
  return SaveStore(std::move(root));
}

std::expected<fs::path, std::string> SaveStore::SlotPath(int slot) const {
  if (slot < 0 || slot >= kSlotCount) {
    return std::unexpected(
        std::format("Save slot {} is outside 0..{}", slot, kSlotCount - 1));
  }
  return m_root_ / std::format("slot{:02}.vssave", slot);
}

std::expected<bool, std::string> SaveStore::SlotExists(int slot) const {
  const auto path = SlotPath(slot);
  if (!path) return std::unexpected(path.error());
  const auto current = Exists(*path);
  if (!current) return std::unexpected(current.error());
  const auto backup = Exists(BackupPath(*path));
  if (!backup) return std::unexpected(backup.error());
  return *current || *backup;
}

std::expected<LoadedSave, std::string> SaveStore::Load(
    int slot, Validator validate) const {
  const auto path = SlotPath(slot);
  if (!path) return std::unexpected(path.error());
  if (validate == nullptr) return std::unexpected("Save validator is missing");
  auto current = Read(*path, validate);
  if (current) return LoadedSave{std::move(*current), false, {}};
  auto backup = Read(BackupPath(*path), validate);
  if (!backup) {
    return std::unexpected(current.error() + "; " + backup.error());
  }
  return LoadedSave{std::move(*backup), true, current.error()};
}

std::expected<void, std::string> SaveStore::Save(int slot,
                                                 std::string_view data,
                                                 Validator validate) const {
  const auto path = SlotPath(slot);
  if (!path) return std::unexpected(path.error());
  if (validate == nullptr) return std::unexpected("Save validator is missing");
  if (data.empty() || data.size() > kMaximumBytes) {
    return std::unexpected("Invalid save byte count");
  }
  const auto valid = validate(data);
  if (!valid) {
    return std::unexpected(FileError("Validate", *path, valid.error()));
  }
  const auto lock = Lock(*path);
  if (!lock) return std::unexpected(lock.error());
  const auto current_exists = Exists(*path);
  if (!current_exists) return std::unexpected(current_exists.error());
  const auto backup_exists = Exists(BackupPath(*path));
  if (!backup_exists) return std::unexpected(backup_exists.error());
  if (*current_exists || *backup_exists) {
    const auto previous = Load(slot, validate);
    if (!previous) return std::unexpected(previous.error());
    const auto backed_up = WriteAtomic(BackupPath(*path), previous->data);
    if (!backed_up) return std::unexpected(backed_up.error());
  }
  return WriteAtomic(*path, data);
}

}  // namespace VS::Save
