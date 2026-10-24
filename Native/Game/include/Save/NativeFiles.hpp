#pragma once

#include <expected>
#include <filesystem>
#include <optional>
#include <string>

#include "Save/NativeSnapshot.hpp"
#include "Save/SaveStore.hpp"

namespace VS::Game::Save {

enum class FileStatus { kEmpty, kValid, kRecovered, kInvalid };

struct LoadedDocument {
  Document document;
  bool recovered = false;
  std::string recovery_reason;
};

struct FileInfo {
  int slot = 0;
  FileStatus status = FileStatus::kEmpty;
  std::optional<Document> document;
  std::string message;
};

class Files {
 public:
  static std::expected<Files, std::string> Open(std::filesystem::path root);

  const std::filesystem::path& Root() const { return store_.Root(); }
  std::expected<FileInfo, std::string> Inspect(int slot) const;
  std::expected<bool, std::string> AnyFiles() const;
  std::expected<LoadedDocument, std::string> Load(int slot) const;
  std::expected<void, std::string> Write(
      int slot, const Document& document,
      VS::Save::WriteConsent consent = VS::Save::WriteConsent::kNormal) const;

 private:
  explicit Files(VS::Save::SaveStore store);
  VS::Save::SaveStore store_;
};

}  // namespace VS::Game::Save
