#pragma once

#include <filesystem>

namespace VS::Save {

// Save data lives in plain files under the user data dir, not a memory-card
// image (plan §6.7). Memory-card import/export is a separate, optional
// converter.
class SaveStore {
 public:
  explicit SaveStore(std::filesystem::path root);

  const std::filesystem::path& Root() const { return m_root_; }
  std::filesystem::path SlotPath(int slot) const;
  bool SlotExists(int slot) const;

 private:
  std::filesystem::path m_root_;
};

}  // namespace VS::Save
