#include "Event/TriggersLive.hpp"

#include <cstdint>
#include <cstdlib>
#include <utility>

#include "Event/ScriptVmLive.hpp"
#include "State/ScriptFlagsLive.hpp"

extern "C" {
#include "Core/Diagnostics.h"
#include "Event/ScriptVm.h"
#include "Event/Triggers.h"
}

namespace VS::Event {
namespace {
constexpr int kActorInteractionTriggerRequest = 4;

// _loadMpdClearedScriptDoorSections / func_800BEB34 (4A0A8.c:4201, 4250).
class LiveRoomSectionHost final : public RoomEventSectionHost {
 public:
  void Refuse(RoomEventLoadError error) override {
    Core_Warnf("event", "Room event import refused: reason=%d bytes=%zu",
               static_cast<int>(error.reason), error.bytes);
  }
  RoomEventCopy CopySections(const RoomEventInput& input) override {
    auto sections = RoomEventSections::Copy(input);
    if (!sections || input.entry_script.empty()) {
      return sections;
    }
    auto source = PrepareLiveRoomEntrySource(*sections);
    if (!source) {
      Core_Warnf("event", "Room entry source preparation refused: reason=%d",
                 static_cast<int>(source.error()));
      return std::unexpected(RoomEventLoadError{
          RoomEventRefusal::kSourcePreparation, input.entry_script.size()});
    }
    prepared_source_ = std::move(*source);
    prepared_room_ = *sections;
    return sections;
  }
  void PublishSections(RoomEventLease sections) override {
    PublishLiveRoomScriptSections(std::move(sections));
  }
  void BindEntryScript(RoomEventLease sections) override {
    if (sections != prepared_room_ ||
        !PublishLiveScriptSource(std::move(prepared_source_))) {
      Core_Errorf("event", "Prepared room entry publication invariant failed");
      std::abort();
    }
    prepared_room_.reset();
  }
  std::int16_t RoomChangeState() override {
    return LiveScriptVm().RoomChangeState();
  }
  void SetRoomChangeState(std::int16_t state) override {
    LiveScriptVm().SetRoomChangeState(state);
  }
  void SetPlaybackPhase(PlaybackPhase phase) override {
    LiveScriptVm().SetPhase(phase);
  }
  void SetStartConfirmed(bool confirmed) override {
    LiveScriptVm().SetStartConfirmed(confirmed);
  }
  void SetRoomEntryPending(bool pending) override {
    Event_hasRoomEntryScript = static_cast<std::int16_t>(pending);
  }
  void SetFlag(int flag, std::int16_t value) override {
    State::SetLiveScriptFlag(flag, value);
  }
  void ReleaseScriptActorControl() override { ReleaseLiveScriptActorControl(); }

 private:
  PreparedScriptSource prepared_source_;
  RoomEventLease prepared_room_;
};
}  // namespace

bool EvaluateLiveActorRoomEventTrigger(int actor_id) {
  const auto context =
      reinterpret_cast<u_char*>(static_cast<std::intptr_t>(actor_id));
  return Event_EvaluateRoomEventTriggers(kActorInteractionTriggerRequest, 0,
                                         context, 0) == 1;
}

RoomEventLoad LoadLiveRoomEventSections(RoomEventInput input) {
  constexpr std::int16_t kWaitingForRoom = 1;
  if (Event_roomChangeState == kWaitingForRoom) {
    input.entry_script = {};
  }
  if (const auto error = ValidateRoomEventInput(input)) {
    Core_Warnf("event", "Room event import refused: section %d, %zu bytes",
               static_cast<int>(error->reason), error->bytes);
    return std::unexpected(*error);
  }
  const int result = Event_LoadRoomEventSections(
      const_cast<std::uint8_t*>(input.triggers.data()),
      static_cast<std::uint16_t>(input.triggers.size()),
      const_cast<std::uint8_t*>(input.entry_script.data()),
      static_cast<std::uint16_t>(input.entry_script.size()),
      const_cast<std::uint8_t*>(input.doors.data()),
      static_cast<std::uint16_t>(input.doors.size()),
      input.first_entry ? 1 : 0);
  return result == 0 ? RoomEventLoadResult::kReady
                     : RoomEventLoadResult::kContinuingEvent;
}

RoomEventLoad ImportLiveRoomScripts(RoomEventInput input) {
  LiveRoomSectionHost host;
  return LoadRoomEventSections(input, host);
}

void FreeLiveRoomScripts() {
  LiveRoomSectionHost host;
  FreeRoomEventSections(host);
}
}  // namespace VS::Event
