#pragma once

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "Battle/FloatingLabelProducers.hpp"
#include "Battle/FloatingLabelQueue.hpp"
#include "TestSupport/FloatingLabelQueueFixture.hpp"

namespace VS::test_support::floating_label_producers {
namespace b = Battle;

// The service a recorded call went to, in the differential's numbering.
enum class Service : std::int32_t {
  kFindBoneTag = 1,
  kBonePosition = 2,
  kTaggedBonePosition = 3,
  kProjectTaggedBone = 4,
  kRandom = 5,
};

struct Call {
  Service service = Service::kFindBoneTag;
  std::array<std::int32_t, 3> arguments{};
  bool operator==(const Call&) const = default;
};

// A scripted callee result: the status it returns and the point it writes.
struct PointResult {
  std::int32_t status = 0;
  b::LabelPoint point{};
};

// The producers over the typed queue fixture (FloatingLabelQueue.cpp's
// reserve, remove and commit) with scripted skeleton and random results.
// A call past its script returns 0 and leaves the point alone.
class World final : public b::FloatingLabelProducerWorld {
 public:
  floating_labels::Queue queue;
  std::int32_t found_bone = 0;
  std::vector<PointResult> bone_positions;
  std::vector<PointResult> tagged_positions;
  std::vector<PointResult> projections;
  std::int32_t random = 0;
  std::vector<Call> calls;
  std::optional<b::FloatingLabelSlot> reserved;

  std::int16_t LabelCount() override { return queue.count; }
  std::optional<b::FloatingLabelRecord> Label(std::int32_t index) override {
    if (index < 0 || index >= b::kFloatingLabelCapacity) return std::nullopt;
    return std::bit_cast<b::FloatingLabelRecord>(
        queue.records.at(static_cast<std::size_t>(index)));
  }
  bool RemoveLabel(std::int32_t index) override {
    return b::RemoveFloatingLabel(queue, {index});
  }
  std::optional<b::FloatingLabelRecord> ReserveLabel() override {
    reserved = b::ReserveFloatingLabel(queue);
    if (!reserved) return std::nullopt;
    return Label(reserved->index);
  }
  bool WriteReservedLabel(const b::FloatingLabelRecord& record) override {
    if (!reserved) return false;
    queue.records.at(static_cast<std::size_t>(reserved->index)) =
        std::bit_cast<floating_labels::Record>(record);
    return true;
  }
  bool WriteReservedTextLength(std::uint8_t text_length) override {
    if (!reserved) return false;
    queue.records.at(static_cast<std::size_t>(reserved->index)).length =
        text_length;
    return true;
  }
  bool CommitReservedLabel() override {
    if (!reserved) return false;
    return b::CommitFloatingLabel(queue, *reserved);
  }
  std::int32_t FindBoneTag(std::int32_t actor, std::int32_t tag,
                           std::int32_t kind) override {
    calls.push_back({Service::kFindBoneTag, {actor, tag, kind}});
    return found_bone;
  }
  std::int32_t BonePosition(std::int32_t actor, std::int32_t bone,
                            std::int32_t endpoint,
                            b::LabelPoint& point) override {
    calls.push_back({Service::kBonePosition, {actor, bone, endpoint}});
    return Next(bone_positions, bone_index_, point);
  }
  std::int32_t TaggedBonePosition(std::int32_t actor, std::int32_t tag,
                                  std::int32_t endpoint,
                                  b::LabelPoint& point) override {
    calls.push_back({Service::kTaggedBonePosition, {actor, tag, endpoint}});
    return Next(tagged_positions, tagged_index_, point);
  }
  std::int32_t ProjectTaggedBone(std::int32_t actor, std::int32_t tag,
                                 std::int32_t endpoint,
                                 b::LabelPoint& point) override {
    calls.push_back({Service::kProjectTaggedBone, {actor, tag, endpoint}});
    return Next(projections, projection_index_, point);
  }
  std::int32_t Random() override {
    calls.push_back({Service::kRandom, {}});
    return random;
  }

  [[nodiscard]] b::FloatingLabelRecord Record(std::size_t index) const {
    return std::bit_cast<b::FloatingLabelRecord>(queue.records.at(index));
  }
  void SetRecord(std::size_t index, const b::FloatingLabelRecord& record) {
    queue.records.at(index) = std::bit_cast<floating_labels::Record>(record);
  }

 private:
  static std::int32_t Next(const std::vector<PointResult>& script,
                           std::size_t& index, b::LabelPoint& point) {
    if (index >= script.size()) return 0;
    const PointResult& result = script.at(index++);
    point = result.point;
    return result.status;
  }
  std::size_t bone_index_ = 0;
  std::size_t tagged_index_ = 0;
  std::size_t projection_index_ = 0;
};
}  // namespace VS::test_support::floating_label_producers
