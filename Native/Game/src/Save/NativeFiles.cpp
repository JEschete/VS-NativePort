#include "Save/NativeFiles.hpp"

#include <string>
#include <utility>

#include "Save/RuntimeSnapshot.hpp"

namespace VS::Game::Save {
namespace {
std::expected<void, std::string> ValidateFile(std::string_view data) {
  const auto document = Decode(data);
  if (!document) return std::unexpected(document.error());
  return ValidateContent(document->state);
}
}  // namespace

Files::Files(VS::Save::SaveStore store) : store_(std::move(store)) {}

std::expected<Files, std::string> Files::Open(std::filesystem::path root) {
  auto store = VS::Save::SaveStore::Open(std::move(root));
  if (!store) return std::unexpected(store.error());
  return Files(std::move(*store));
}

std::expected<LoadedDocument, std::string> Files::Load(int slot) const {
  auto loaded = store_.Load(slot, ValidateFile);
  if (!loaded) return std::unexpected(loaded.error());
  auto document = Decode(loaded->data);
  if (!document) return std::unexpected(document.error());
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
