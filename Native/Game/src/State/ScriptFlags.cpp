// Hand-maintained (tools/port/owned.csv).
// The script flag bytes (State/ScriptFlags.hpp).
#include "State/ScriptFlags.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace VS::State {
ScriptFlags::ScriptFlags(std::span<std::byte> bytes,
                         ScriptFlagObserver* observer) noexcept
    : bytes_(bytes), observer_(observer) {}

bool ScriptFlags::Contains(int id) const noexcept {
  return id >= 0 && static_cast<std::size_t>(id) < bytes_.size();
}

std::uint8_t ScriptFlags::Get(int id) const noexcept {
  if (!Contains(id)) return 0;
  return std::to_integer<std::uint8_t>(bytes_[static_cast<std::size_t>(id)]);
}

bool ScriptFlags::Set(int id, std::uint8_t value) noexcept {
  if (!Contains(id)) return false;
  std::byte& flag = bytes_[static_cast<std::size_t>(id)];
  const ScriptFlagWrite write{
      .id = id, .before = std::to_integer<std::uint8_t>(flag), .after = value};
  flag = static_cast<std::byte>(value);
  if (observer_ != nullptr) observer_->FlagWritten(write);
  return true;
}

void ScriptFlags::ResetZone(bool preserve) noexcept {
  if (preserve) return;
  for (int id = kFirstZoneScriptFlag; id < kZoneScriptFlagEnd; ++id) {
    (void)Set(id, 0);
  }
}
}  // namespace VS::State
