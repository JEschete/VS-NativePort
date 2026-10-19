#include "Save/SaveStore.hpp"

#include <format>
#include <utility>

namespace VS::Save {

SaveStore::SaveStore(std::filesystem::path root) : m_root_(std::move(root)) {
  std::error_code ec;
  std::filesystem::create_directories(m_root_, ec);
}

std::filesystem::path SaveStore::SlotPath(int slot) const {
  return m_root_ / std::format("slot{:02}.vssave", slot);
}

bool SaveStore::SlotExists(int slot) const {
  std::error_code ec;
  return std::filesystem::exists(SlotPath(slot), ec);
}

}  // namespace VS::Save
