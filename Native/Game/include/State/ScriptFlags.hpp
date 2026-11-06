#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <cstdint>
#include <span>

// The script flag bytes the event scripts, battle, menus and save files read
// and write by id (decomp: vs_main_stateFlags, SLUS_010.40 main.h:366, a
// 0x440-byte vs_main_flags_t). Ids 64-127 hold the current zone's flags.
namespace VS::State {
inline constexpr std::size_t kScriptFlagCount = 0x440;
inline constexpr int kFirstZoneScriptFlag = 64;
inline constexpr int kZoneScriptFlagEnd = 128;

struct ScriptFlagWrite {
  int id = 0;
  std::uint8_t before = 0;
  std::uint8_t after = 0;

  friend bool operator==(const ScriptFlagWrite&,
                         const ScriptFlagWrite&) = default;
};

// Told of each stored write, for the scene trace.
class ScriptFlagObserver {
 public:
  ScriptFlagObserver() = default;
  ScriptFlagObserver(const ScriptFlagObserver&) = delete;
  ScriptFlagObserver& operator=(const ScriptFlagObserver&) = delete;
  ScriptFlagObserver(ScriptFlagObserver&&) = delete;
  ScriptFlagObserver& operator=(ScriptFlagObserver&&) = delete;
  virtual ~ScriptFlagObserver() = default;
  virtual void FlagWritten(const ScriptFlagWrite& write) = 0;
};

// A view of the flag bytes; an id is valid when it indexes them.
class ScriptFlags {
 public:
  explicit ScriptFlags(std::span<std::byte> bytes,
                       ScriptFlagObserver* observer = nullptr) noexcept;

  [[nodiscard]] bool Contains(int id) const noexcept;
  // decomp: BATTLE.PRG vs_battle_getStateFlag (4A0A8.c:4275). An id outside
  // the flags reads 0; the original read past the record.
  [[nodiscard]] std::uint8_t Get(int id) const noexcept;
  // decomp: BATTLE.PRG vs_battle_setStateFlag (4A0A8.c:4281). Returns false
  // and stores nothing for an id outside the flags; the original wrote past
  // the record.
  bool Set(int id, std::uint8_t value) noexcept;
  // decomp: BATTLE.PRG func_800BEB9C (4A0A8.c:4263): clears the zone flags,
  // one write each, unless `preserve`.
  void ResetZone(bool preserve) noexcept;

 private:
  std::span<std::byte> bytes_;
  ScriptFlagObserver* observer_;
};
}  // namespace VS::State
