#pragma once

#include <cstddef>
#include <expected>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>

namespace VS::Save {

// Game supplies schema validation; storage never interprets gameplay state.
using ValidationResult = std::expected<void, std::string>;
using Validator = std::function<ValidationResult(std::string_view)>;

struct LoadedSave {
  std::string data;
  bool recovered_from_backup = false;
  std::string recovery_reason;
};

enum class WriteConsent { kNormal, kReplaceRecovered };

class SaveStore {
 public:
  static constexpr int kSlotCount = 100;
  static constexpr std::size_t kMaximumBytes = std::size_t{16} * 1024 * 1024;

  static std::expected<SaveStore, std::string> Open(std::filesystem::path root);

  const std::filesystem::path& Root() const { return m_root_; }
  std::expected<std::filesystem::path, std::string> SlotPath(int slot) const;
  std::expected<bool, std::string> SlotExists(int slot) const;
  std::expected<LoadedSave, std::string> Load(int slot,
                                              const Validator& validate) const;
  std::expected<void, std::string> Save(
      int slot, std::string_view data, const Validator& validate,
      WriteConsent consent = WriteConsent::kNormal) const;

 private:
  explicit SaveStore(std::filesystem::path root);

  std::filesystem::path m_root_;
};

}  // namespace VS::Save
