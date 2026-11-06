#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <vector>

#include "Event/RoomTriggers.hpp"

namespace VS::Event {

enum class ScriptSourceError : std::uint8_t {
  kRegion,
  kMissingOwner,
  kExtent,
  kGenerationExhausted,
  kNoActiveUse,
  kUseDepth,
  kRetentionAllocation,
};

// Keep this counter outside resettable module data; clearing a mount does not
// make any previously issued generation reusable.
[[nodiscard]] std::expected<std::uint64_t, ScriptSourceError>
NextScriptSourceGeneration(std::uint64_t& serial) noexcept;

// Retains the exact bytes behind a mounted generation, including through an
// opcode callback which replaces the mount. Copies retain the same source;
// every newly prepared mount receives a fresh generation.
class ScriptSource {
 public:
  ScriptSource() = default;
  ScriptSource(const ScriptSource&) = default;
  ScriptSource& operator=(const ScriptSource&) = default;
  ScriptSource(ScriptSource&& other) noexcept;
  ScriptSource& operator=(ScriptSource&& other) noexcept;
  ~ScriptSource() = default;

  // All writes, including deferred disc writes, must finish or be cancelled
  // before a buffer is published through this immutable view.
  [[nodiscard]] static std::expected<ScriptSource, ScriptSourceError>
  FromBuffer(ScriptRegion region,
             std::shared_ptr<const std::vector<std::uint8_t>> buffer,
             std::uint64_t& serial) noexcept;
  [[nodiscard]] static std::expected<ScriptSource, ScriptSourceError> FromRoom(
      ScriptRegion region, RoomEventLease room, std::uint64_t& serial) noexcept;

  [[nodiscard]] ScriptCommand Bytes() const noexcept { return bytes_; }
  [[nodiscard]] std::uint64_t Generation() const noexcept {
    return generation_;
  }
  [[nodiscard]] std::optional<ScriptLocation> At(
      std::uint32_t offset) const noexcept;
  [[nodiscard]] std::optional<ScriptCommand> View(
      ScriptLocation location) const noexcept;

 private:
  ScriptSource(ScriptRegion region, ScriptCommand bytes,
               std::shared_ptr<const std::uint8_t> owner,
               std::uint64_t generation) noexcept;
  ScriptRegion region_ = ScriptRegion::kNone;
  ScriptCommand bytes_{};
  // Aliasing ownership retains the vector or the complete room lease, not an
  // individually allocated byte.
  std::shared_ptr<const std::uint8_t> owner_;
  std::uint64_t generation_ = 0;
};

// Keep this state outside module-image resets. Explicit use boundaries retain
// every exposed generation through nested callbacks without automatic owning
// guards that a non-returning session transition could skip.
class ScriptSourceMounts {
 public:
  ScriptSourceMounts() = default;
  ScriptSourceMounts(const ScriptSourceMounts&) = delete;
  ScriptSourceMounts& operator=(const ScriptSourceMounts&) = delete;
  ScriptSourceMounts(ScriptSourceMounts&&) = delete;
  ScriptSourceMounts& operator=(ScriptSourceMounts&&) = delete;
  ~ScriptSourceMounts() = default;

  [[nodiscard]] std::expected<void, ScriptSourceError> MountBuffer(
      ScriptRegion region,
      std::shared_ptr<const std::vector<std::uint8_t>> buffer) noexcept;
  [[nodiscard]] std::expected<void, ScriptSourceError> MountRoom(
      ScriptRegion region, RoomEventLease room) noexcept;
  // A persistent lease for UI/data consumers, not an automatic callback guard.
  [[nodiscard]] std::expected<ScriptSource, ScriptSourceError> Lease(
      ScriptRegion region) const noexcept;
  [[nodiscard]] std::expected<void, ScriptSourceError> Clear(
      ScriptRegion region) noexcept;
  // Invalidate mounts without revoking bytes already borrowed by active uses.
  // Neither reset nor abandonment reuses a generation.
  void Reset() noexcept;

  [[nodiscard]] std::expected<void, ScriptSourceError> BeginUse() noexcept;
  [[nodiscard]] std::expected<ScriptCommand, ScriptSourceError> Borrow(
      ScriptRegion region) noexcept;
  [[nodiscard]] std::expected<void, ScriptSourceError> EndUse() noexcept;
  // Only after every active user has unwound, or immediately before a
  // non-returning transition which abandons them. Normal callbacks use Reset.
  void AbandonUses() noexcept;

 private:
  static constexpr std::size_t kRegionCount = 3;
  std::array<ScriptSource, kRegionCount> mounted_{};
  std::vector<ScriptSource> retained_;
  std::uint64_t serial_ = 0;
  std::size_t use_depth_ = 0;
};

}  // namespace VS::Event
