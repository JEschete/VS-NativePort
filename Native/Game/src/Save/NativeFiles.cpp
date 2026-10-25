#include "Save/NativeFiles.hpp"

#include <chrono>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

#include "Core/Log.hpp"
#include "Save/RuntimeSnapshot.hpp"

namespace VS::Game::Save {
namespace {
std::expected<Document, std::string> DecodeFile(std::string_view data) {
  const auto started = std::chrono::steady_clock::now();
  auto document = Decode(data);
  const auto decoded = std::chrono::steady_clock::now();
  if (!document) return std::unexpected(document.error());
  const auto valid = ValidateContent(document->state);
  const auto finished = std::chrono::steady_clock::now();
  VS_LOG_DEBUG(
      "save.perf", "Validate bytes={} decode_us={} content_us={} valid={}",
      data.size(),
      std::chrono::duration_cast<std::chrono::microseconds>(decoded - started)
          .count(),
      std::chrono::duration_cast<std::chrono::microseconds>(finished - decoded)
          .count(),
      valid.has_value());
  if (!valid) return std::unexpected(valid.error());
  return std::move(*document);
}

std::expected<void, std::string> ValidateFile(std::string_view data) {
  const auto document = DecodeFile(data);
  if (!document) return std::unexpected(document.error());
  return {};
}
}  // namespace

Files::Files(VS::Save::SaveStore store) : store_(std::move(store)) {}

std::expected<Files, std::string> Files::Open(std::filesystem::path root) {
  auto store = VS::Save::SaveStore::Open(std::move(root));
  if (!store) return std::unexpected(store.error());
  return Files(std::move(*store));
}

std::expected<LoadedDocument, std::string> Files::Load(int slot) const {
  const auto started = std::chrono::steady_clock::now();
  std::optional<Document> document;
  int decode_attempts = 0;
  auto loaded = store_.Load(
      slot, [&](std::string_view data) -> std::expected<void, std::string> {
        ++decode_attempts;
        auto decoded = DecodeFile(data);
        if (!decoded) return std::unexpected(decoded.error());
        document = std::move(*decoded);
        return {};
      });
  const auto validated = std::chrono::steady_clock::now();
  VS_LOG_DEBUG(
      "save.perf",
      "Load slot={} read_validate_us={} decode_attempts={} valid={}", slot,
      std::chrono::duration_cast<std::chrono::microseconds>(validated - started)
          .count(),
      decode_attempts, loaded.has_value());
  if (!loaded) return std::unexpected(loaded.error());
  if (!document) {
    throw std::logic_error("Save storage accepted a file without validation");
  }
  return LoadedDocument{std::move(*document), loaded->recovered_from_backup,
                        std::move(loaded->recovery_reason)};
}

std::expected<bool, std::string> Files::AnyFiles() const {
  for (int slot = 0; slot < VS::Save::SaveStore::kSlotCount; ++slot) {
    const auto exists = store_.SlotExists(slot);
    if (!exists) return std::unexpected(exists.error());
    if (*exists) return true;
  }
  return false;
}

std::expected<FileInfo, std::string> Files::Inspect(int slot) const {
  const auto exists = store_.SlotExists(slot);
  if (!exists) return std::unexpected(exists.error());
  if (!*exists) return FileInfo{slot, FileStatus::kEmpty, std::nullopt, {}};
  auto loaded = Load(slot);
  if (!loaded) {
    return FileInfo{slot, FileStatus::kInvalid, std::nullopt, loaded.error()};
  }
  return FileInfo{
      slot, loaded->recovered ? FileStatus::kRecovered : FileStatus::kValid,
      std::move(loaded->document), std::move(loaded->recovery_reason)};
}

std::expected<void, std::string> Files::Write(
    int slot, const Document& document, VS::Save::WriteConsent consent) const {
  const auto valid = ValidateContent(document.state);
  if (!valid) return std::unexpected(valid.error());
  const auto encoded = Encode(document);
  if (!encoded) return std::unexpected(encoded.error());
  return store_.Save(slot, *encoded, ValidateFile, consent);
}

}  // namespace VS::Game::Save
