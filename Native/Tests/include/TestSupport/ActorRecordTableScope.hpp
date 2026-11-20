#pragma once

#include <algorithm>
#include <cstdint>
#include <memory>
#include <vector>

#include "Actors/ActorRecord.hpp"

namespace VS::test_support {
// The actor record table, emptied for one test and then restored.
class ActorRecordTableScope {
 public:
  ActorRecordTableScope() {
    std::ranges::fill(VS::Actors::actorRecords, nullptr);
  }
  ActorRecordTableScope(const ActorRecordTableScope&) = delete;
  ActorRecordTableScope& operator=(const ActorRecordTableScope&) = delete;
  ActorRecordTableScope(ActorRecordTableScope&&) = delete;
  ActorRecordTableScope& operator=(ActorRecordTableScope&&) = delete;
  ~ActorRecordTableScope() { VS::Actors::actorRecords = saved_records_; }

  VS::Actors::ActorRecord& AddActor(int actor_id) {
    records_.push_back(std::make_unique<VS::Actors::ActorRecord>());
    VS::Actors::ActorRecord& actor = *records_.back();
    actor.instance.slot = static_cast<std::uint8_t>(actor_id);
    VS::Actors::actorRecords[actor_id] = &actor;
    return actor;
  }

 private:
  decltype(VS::Actors::actorRecords) saved_records_ = VS::Actors::actorRecords;
  std::vector<std::unique_ptr<VS::Actors::ActorRecord>> records_;
};
}  // namespace VS::test_support
