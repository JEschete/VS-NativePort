#pragma once
// Hand-maintained (tools/port/owned.csv).
// Binding from the room enemy memory, the gameplay HUD, the script flags, the
// reward flow, the menu framework, the event music slot (VS::Event::musicSlot),
// the zone music switch and the scripted room and zone transitions to the
// typed scene handoff opcodes (EventSceneTransition.hpp). The opcode handlers
// keep the original signature and are declared with the other typed opcodes
// in Event/ScriptControlOpcodes.h; they map Event_isMenuTransitionActive and
// Event_roomChangeState to the opcodes' typed state. A missing instruction
// or menu state is refused where the original read through a null pointer.
#include "Event/EventSceneTransition.hpp"

namespace VS::Event {
// The live services as a host, for callers that run the typed opcodes on
// their own state.
[[nodiscard]] SceneTransitionHost& LiveSceneTransitionHost();
}  // namespace VS::Event
