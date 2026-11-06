#include "Event/ScriptSources.hpp"

#include <limits>
#include <memory>
#include <utility>
#include <vector>

namespace VS::Event {
namespace {
bool IsRegion(ScriptRegion region) noexcept {
  return region == ScriptRegion::kEventScript ||
         region == ScriptRegion::kDoorEvents ||
         region == ScriptRegion::kSubroutine;
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

}  // namespace VS::Event
