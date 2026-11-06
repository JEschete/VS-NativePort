#include "Event/ScriptSources.hpp"

#include <algorithm>
#include <limits>
#include <memory>
#include <new>
#include <utility>
#include <vector>

namespace VS::Event {
namespace {
bool IsRegion(ScriptRegion region) noexcept {
  return region == ScriptRegion::kEventScript ||
         region == ScriptRegion::kDoorEvents ||
         region == ScriptRegion::kSubroutine;
}

std::optional<std::size_t> RegionIndex(ScriptRegion region) noexcept {
  switch (region) {
    case ScriptRegion::kEventScript:
      return 0;
    case ScriptRegion::kDoorEvents:
      return 1;
    case ScriptRegion::kSubroutine:
      return 2;
    case ScriptRegion::kNone:
      return std::nullopt;
  }
  return std::nullopt;
}
}  // namespace

std::expected<std::uint64_t, ScriptSourceError> NextScriptSourceGeneration(
    std::uint64_t& serial) noexcept {
  if (serial == std::numeric_limits<std::uint64_t>::max()) {
    return std::unexpected(ScriptSourceError::kGenerationExhausted);
  }
  return ++serial;
}

ScriptSource::ScriptSource(ScriptRegion region, ScriptCommand bytes,
                           std::shared_ptr<const std::uint8_t> owner,
                           std::uint64_t generation) noexcept {
  region_ = region;
  bytes_ = bytes;
  owner_ = std::move(owner);
  generation_ = generation;
}

ScriptSource::ScriptSource(ScriptSource&& other) noexcept {
  *this = std::move(other);
}

ScriptSource& ScriptSource::operator=(ScriptSource&& other) noexcept {
  if (this != &other) {
    region_ = std::exchange(other.region_, ScriptRegion::kNone);
    bytes_ = std::exchange(other.bytes_, {});
    owner_ = std::move(other.owner_);
    generation_ = std::exchange(other.generation_, 0);
  }
  return *this;
}

std::expected<ScriptSource, ScriptSourceError> ScriptSource::FromBuffer(
    ScriptRegion region,
    std::shared_ptr<const std::vector<std::uint8_t>> buffer,
    std::uint64_t& serial) noexcept {
  if (!IsRegion(region)) {
    return std::unexpected(ScriptSourceError::kRegion);
  }
  if (!buffer) {
    return std::unexpected(ScriptSourceError::kMissingOwner);
  }
  if (buffer->size() > std::numeric_limits<std::uint32_t>::max()) {
    return std::unexpected(ScriptSourceError::kExtent);
  }
  const auto generation = NextScriptSourceGeneration(serial);
  if (!generation) {
    return std::unexpected(generation.error());
  }
  const ScriptCommand bytes{*buffer};
  std::shared_ptr<const std::uint8_t> owner(buffer, bytes.data());
  return ScriptSource(region, bytes, std::move(owner), *generation);
}

std::expected<ScriptSource, ScriptSourceError> ScriptSource::FromRoom(
    ScriptRegion region, RoomEventLease room, std::uint64_t& serial) noexcept {
  if (region != ScriptRegion::kEventScript &&
      region != ScriptRegion::kDoorEvents) {
    return std::unexpected(ScriptSourceError::kRegion);
  }
  if (!room) {
    return std::unexpected(ScriptSourceError::kMissingOwner);
  }
  const auto generation = NextScriptSourceGeneration(serial);
  if (!generation) {
    return std::unexpected(generation.error());
  }
  const ScriptCommand bytes = region == ScriptRegion::kEventScript
                                  ? room->EntryScript()
                                  : room->Doors();
  std::shared_ptr<const std::uint8_t> owner(room, bytes.data());
  return ScriptSource(region, bytes, std::move(owner), *generation);
}

std::optional<ScriptLocation> ScriptSource::At(
    std::uint32_t offset) const noexcept {
  if (offset >= bytes_.size()) {
    return std::nullopt;
  }
  return ScriptLocation{
      .region = region_, .offset = offset, .generation = generation_};
}

std::optional<ScriptCommand> ScriptSource::View(
    ScriptLocation location) const noexcept {
  if (location.region != region_ || location.generation != generation_ ||
      location.offset >= bytes_.size()) {
    return std::nullopt;
  }
  return bytes_.subspan(location.offset);
}

std::expected<void, ScriptSourceError> ScriptSourceMounts::MountBuffer(
    ScriptRegion region,
    std::shared_ptr<const std::vector<std::uint8_t>> buffer) noexcept {
  const auto index = RegionIndex(region);
  if (!index) {
    return std::unexpected(ScriptSourceError::kRegion);
  }
  auto source = ScriptSource::FromBuffer(region, std::move(buffer), serial_);
  if (!source) {
    return std::unexpected(source.error());
  }
  mounted_[*index] = std::move(*source);
  return {};
}

std::expected<void, ScriptSourceError> ScriptSourceMounts::MountRoom(
    ScriptRegion region, RoomEventLease room) noexcept {
  const auto index = RegionIndex(region);
  if (!index) {
    return std::unexpected(ScriptSourceError::kRegion);
  }
  auto source = ScriptSource::FromRoom(region, std::move(room), serial_);
  if (!source) {
    return std::unexpected(source.error());
  }
  mounted_[*index] = std::move(*source);
  return {};
}

std::expected<ScriptSource, ScriptSourceError> ScriptSourceMounts::Lease(
    ScriptRegion region) const noexcept {
  const auto index = RegionIndex(region);
  if (!index) {
    return std::unexpected(ScriptSourceError::kRegion);
  }
  if (mounted_[*index].Generation() == 0) {
    return std::unexpected(ScriptSourceError::kMissingOwner);
  }
  return mounted_[*index];
}

std::expected<void, ScriptSourceError> ScriptSourceMounts::Clear(
    ScriptRegion region) noexcept {
  const auto index = RegionIndex(region);
  if (!index) {
    return std::unexpected(ScriptSourceError::kRegion);
  }
  mounted_[*index] = {};
  return {};
}

void ScriptSourceMounts::Reset() noexcept { mounted_.fill({}); }

std::expected<void, ScriptSourceError> ScriptSourceMounts::BeginUse() noexcept {
  if (use_depth_ == std::numeric_limits<std::size_t>::max()) {
    return std::unexpected(ScriptSourceError::kUseDepth);
  }
  ++use_depth_;
  return {};
}

std::expected<ScriptCommand, ScriptSourceError> ScriptSourceMounts::Borrow(
    ScriptRegion region) noexcept {
  if (use_depth_ == 0) {
    return std::unexpected(ScriptSourceError::kNoActiveUse);
  }
  const auto source = Lease(region);
  if (!source) {
    return std::unexpected(source.error());
  }
  if (std::ranges::find(retained_, source->Generation(),
                        &ScriptSource::Generation) == retained_.end()) {
    try {
      retained_.push_back(*source);
    } catch (const std::bad_alloc&) {
      return std::unexpected(ScriptSourceError::kRetentionAllocation);
    }
  }
  return source->Bytes();
}

std::expected<void, ScriptSourceError> ScriptSourceMounts::EndUse() noexcept {
  if (use_depth_ == 0) {
    return std::unexpected(ScriptSourceError::kNoActiveUse);
  }
  --use_depth_;
  if (use_depth_ == 0) {
    retained_.clear();
  }
  return {};
}

void ScriptSourceMounts::AbandonUses() noexcept {
  use_depth_ = 0;
  retained_.clear();
}

}  // namespace VS::Event
